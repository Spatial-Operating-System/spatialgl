#include "spatialgl/optics.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>

using namespace spatialgl::optics;
namespace {
void check(bool v, std::source_location loc = std::source_location::current()) {
  if (!v) throw std::runtime_error("expectation failed at line " + std::to_string(loc.line()));
}
WorldSnapshot world() {
  WorldSnapshot w;
  w.planes.push_back({"wall", {-1, -1, -2}, {2, 0, 0}, {0, 2, 0}});
  w.source_time = 0;
  w.valid_until = 10;
  w.world_revision = 1;
  w.calibration_revision = 1;
  return w;
}
DeviceProfile device(Id id, double x = 0) {
  DeviceProfile d;
  d.id = std::move(id);
  d.position = {x, 0, 0};
  d.target = {x, 0, -1};
  d.width = 32;
  d.height = 32;
  d.fov_y = 1.5;
  d.sample_rate_hz = 1000;
  d.latency = .01;
  return d;
}
DisplayList point(Id id = "frame") {
  DrawCall c;
  c.draw_id = "dot";
  c.target.surface_id = "wall";
  c.target.origin = {0, 0, -2};
  c.target.geometry = GeometryKind::kPoint;
  DisplayList f;
  f.id = std::move(id);
  f.present_at = 0;
  f.expires_at = 2;
  f.draws.push_back(c);
  return f;
}
}  // namespace
int main() {
  try {
    // F01: both projectors contribute while NORMALIZED bounds aggregate light.
    {
      auto r = device("r");
      Rig rig;
      rig.devices = {r, device("s", .2)};
      RuntimeOptions o;
      o.pipeline.output_mix = OutputMix::kNormalized;
      Runtime sim(world(), rig, o);
      auto f = point();
      f.draws[0].intensity = .2;
      check(sim.submit(f).empty());
      auto pending = sim.advance(.005);
      check(pending.contributions.empty());
      check(pending.programs.size() == 2);
      check(pending.programs[0].starts_at == .01);
      auto s = sim.advance(.1);
      check(s.contributions.size() == 2);
      check(std::abs(s.targets[0].aggregate_linear_rgb[0] - .2) < 1e-8);
      check(std::abs(s.programs[0].events[0].linear_rgb[0] - .1) < 1e-8);
    }
    // Sample-bin OVER masks lower patch samples; ADD retains them. Refining
    // sample spacing does not increase the target-space average.
    {
      auto run = [](Composition mode, double spacing) {
        Rig rig;
        rig.devices = {device("r")};
        RuntimeOptions options;
        options.pipeline.sample_spacing = spacing;
        Runtime sim(world(), rig, options);
        DisplayList background;
        background.id = "background";
        background.app_id = "raster";
        background.expires_at = 2;
        DrawCall patch;
        patch.draw_id = "shared";
        patch.target.surface_id = "wall";
        patch.target.coordinates = CoordinateSpace::kWorld;
        patch.target.geometry = GeometryKind::kPatch;
        patch.target.origin = {-.5, -.5, -2};
        patch.target.u = {1, 0, 0};
        patch.target.v = {0, 1, 0};
        patch.linear_rgb = {1, 0, 0};
        background.draws = {patch};
        check(sim.submit(background).empty());

        DisplayList cursor;
        cursor.id = "cursor";
        cursor.app_id = "laser";
        cursor.present_at = .01;
        cursor.expires_at = 2;
        cursor.composition = mode;
        DrawCall point_draw;
        point_draw.draw_id = "shared";  // IDs are not spatial identity.
        point_draw.target.surface_id = "wall";
        point_draw.target.geometry = GeometryKind::kPoint;
        point_draw.target.origin = {0, 0, -2};
        point_draw.linear_rgb = {0, 1, 0};
        cursor.draws = {point_draw};
        check(sim.submit(cursor).empty());
        return sim.advance(.2);
      };
      const auto over = run(Composition::kOver, .5);
      const auto add = run(Composition::kAdd, .5);
      const auto refined = run(Composition::kOver, .25);
      auto red_at_cursor = [](const Snapshot& s) {
        for (const auto& q : s.contributions)
          if (q.display_list_id == "background" && std::abs(q.position[0]) < 1e-8 &&
              std::abs(q.position[1]) < 1e-8)
            return q.intensity * q.linear_rgb[0];
        return -1.0;
      };
      check(red_at_cursor(over) == 0);
      check(red_at_cursor(add) > .99);
      auto aggregate = [](const Snapshot& s, const Id& display) {
        for (const auto& t : s.targets)
          if (t.display_list_id == display) return t.aggregate_linear_rgb[0];
        return -1.0;
      };
      check(std::abs(aggregate(over, "background") - 8.0 / 9.0) < 1e-8);
      check(std::abs(aggregate(refined, "background") - 24.0 / 25.0) < 1e-8);
      check(aggregate(add, "background") > .99);
    }
    // F02: ordered laser paths retain point order and insert blank travel events.
    {
      Rig rig;
      auto g = device("g");
      g.kind = DeviceKind::kGalvo;
      g.clock_drift = .25;
      rig.devices = {g};
      Runtime sim(world(), rig);
      auto f = point();
      f.draws[0].target.geometry = GeometryKind::kPolyline;
      f.draws[0].target.vertices = {{-.5, 0, -2}, {0, 0, -2}, {.5, 0, -2}};
      check(sim.submit(f).empty());
      auto s = sim.advance(.1);
      check(s.programs.size() == 1);
      check(s.programs[0].events.size() > 6);
      check(s.programs[0].events[0].kind == EventKind::kBlank);
      check(s.programs[0].events[1].position[0] < s.programs[0].events[3].position[0]);
      check(std::abs(s.programs[0].events[0].dwell -
                     (s.programs[0].events[1].time - s.programs[0].events[0].time)) < 1e-12);
      check(std::abs(s.programs[0].events[0].dwell / 1.25 - .0008) < 1e-12);
    }
    // Closed paths include a sampled closing edge and cycle constraints report
    // infeasible requests instead of silently accepting them.
    {
      Rig rig;
      auto g = device("g");
      g.kind = DeviceKind::kGalvo;
      g.sample_rate_hz = 100;
      rig.devices = {g};
      RuntimeOptions options;
      options.pipeline.sample_spacing = .5;
      options.traversal.minimum_duty = .5;
      options.traversal.max_dark_gap = .001;
      Runtime sim(world(), rig, options);
      auto f = point("closed");
      f.draws[0].target.geometry = GeometryKind::kPolyline;
      f.draws[0].target.vertices = {{-.5, 0, -2}, {.5, 0, -2}};
      f.draws[0].closed = true;
      check(sim.submit(f).empty());
      const auto s = sim.advance(.5);
      check(s.programs.size() == 1);
      check(s.programs[0].events.back().position[0] == -.5);
      bool duty_rejected = false, gap_rejected = false;
      for (const auto& d : s.diagnostics) {
        duty_rejected |= d.code == "minimum-duty-unmet";
        gap_rejected |= d.code == "max-dark-gap-exceeded";
      }
      check(duty_rejected && gap_rejected);
    }
    // Every device's advertised sample budget bounds its programmed target.
    {
      auto raster = device("small");
      raster.sample_budget = 1;
      Rig rig;
      rig.devices = {raster};
      Runtime sim(world(), rig);
      auto f = point("budget");
      f.draws[0].target.geometry = GeometryKind::kPolyline;
      f.draws[0].target.vertices = {{-.5, 0, -2}, {0, 0, -2}, {.5, 0, -2}};
      check(sim.submit(f).empty());
      const auto s = sim.advance(.5);
      check(s.contributions.empty());
      check(std::any_of(s.diagnostics.begin(), s.diagnostics.end(),
                        [](const Diagnostic& d) { return d.code == "device-sample-budget"; }));
    }
    // Geometry/profile metadata that the model depends on is validated.
    {
      auto w = world();
      w.planes[0].v = {1, 1, 0};
      Rig rig;
      rig.devices = {device("a")};
      check(!validate(w, rig, {}).empty());
      auto f = point("metadata");
      f.draws[0].target.captured_at = std::numeric_limits<double>::infinity();
      check(!validate(f).empty());
      RuntimeOptions bounded;
      bounded.pipeline.max_position_error = .01;
      const auto diagnostics = validate(world(), rig, bounded);
      check(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& d) {
        return d.code == "unsupported-position-error-bound" && d.status == PlanStatus::kUnsupported;
      }));
      RuntimeOptions unknown_option;
      unknown_option.pipeline.output_mix = static_cast<OutputMix>(999);
      check(!validate(world(), rig, unknown_option).empty());
      auto unknown_geometry = point("unknown-geometry");
      unknown_geometry.draws[0].target.geometry = static_cast<GeometryKind>(999);
      check(!validate(unknown_geometry).empty());
    }
    // F03: logical devices sharing one mirror cannot own the same interval.
    {
      Rig rig;
      rig.resources.push_back({"mirror", 1});
      auto a = device("a"), b = device("b");
      a.resource_ids = {"mirror"};
      b.resource_ids = {"mirror"};
      rig.devices = {a, b};
      Runtime sim(world(), rig);
      sim.submit(point());
      auto s = sim.advance(.1);
      check(s.contributions.size() == 1);
      check(!s.diagnostics.empty());
    }
    // F04: source-time planar motion is applied at output time and expiry is enforced.
    {
      auto w = world();
      w.motion.push_back({{.5, 0, 0}, {}});
      Rig rig;
      rig.devices = {device("steer")};
      rig.devices[0].kind = DeviceKind::kSteerableRaster;
      rig.devices[0].max_angular_speed = 2;
      Runtime sim(w, rig);
      auto f = point();
      f.draws[0].target.origin = {.5, 0, -2};
      sim.submit(f);
      auto s = sim.advance(.5);
      check(!s.contributions.empty());
      check(std::abs(s.contributions[0].position[0] - .5) < 1e-8);

      auto moving = world();
      moving.motion.push_back({{.5, 0, 0}, {}});
      Rig fixed;
      fixed.devices = {device("fixed")};
      Runtime local_sim(moving, fixed);
      auto local = point("local-frame");
      local.draws[0].target.coordinates = CoordinateSpace::kSurfaceLocal;
      local.draws[0].target.origin = {.5, .5, 0};
      local_sim.submit(local);
      const auto early = local_sim.advance(.5);
      const auto later = local_sim.advance(1.0);
      check(!early.programs.empty() && !later.programs.empty());
      check(std::abs(early.programs[0].events[0].time - later.programs[0].events[0].time) < 1e-12);
      check(std::abs(early.programs[0].events[0].position[0] -
                     later.programs[0].events[0].position[0]) < 1e-12);
    }
    // F05: exact timed trace is independent of host advance step size.
    {
      Rig rig;
      auto a = device("a"), b = device("b");
      a.sample_rate_hz = 100;
      b.sample_rate_hz = 150;
      b.latency = .02;
      rig.devices = {a, b};
      Runtime x(world(), rig), y(world(), rig);
      x.submit(point());
      y.submit(point());
      auto sx = x.advance(.5);
      for (int i = 1; i <= 50; i++) y.advance(i * .01);
      auto sy = y.snapshot();
      check(sx.programs.size() == sy.programs.size());
      for (std::size_t i = 0; i < sx.programs.size(); i++) {
        check(sx.programs[i].events.size() == sy.programs[i].events.size());
        for (std::size_t j = 0; j < sx.programs[i].events.size(); j++)
          check(std::abs(sx.programs[i].events[j].time - sy.programs[i].events[j].time) < 1e-12);
      }
    }
    // F06: canceled generation cannot be restored and snapshots are independent values.
    {
      Rig rig;
      rig.devices = {device("a")};
      Runtime sim(world(), rig);
      sim.submit(point());
      auto before = sim.advance(.1);
      const auto generation = sim.cancel("frame");
      auto after = sim.advance(.2);
      check(!after.contributions.size());
      check(!before.contributions.empty());
      check(generation > before.programs[0].generation);
    }
    // Unsupported volume is explicit; no success-shaped output is made.
    {
      Rig rig;
      rig.devices = {device("a")};
      Runtime sim(world(), rig);
      auto f = point();
      f.draws[0].target.geometry = GeometryKind::kVolume;
      check(sim.submit(f).size() == 1);
      check(sim.advance(.1).contributions.empty());
    }
    std::cout << "PASS optics scenarios\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL optics scenarios: " << e.what() << '\n';
    return 1;
  }
}
