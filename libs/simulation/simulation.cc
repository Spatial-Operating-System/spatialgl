#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

#include "spatialgl/optics.h"
namespace spatialgl::optics {
namespace {
std::optional<std::size_t> divisions(double length, double spacing, std::size_t limit) {
  const double n = std::ceil(length / spacing);
  if (!std::isfinite(n) || n < 0 || n > double(limit) ||
      n > double(std::numeric_limits<int>::max()))
    return std::nullopt;
  return std::max<std::size_t>(1, std::size_t(n));
}
double dot(Vec3 a, Vec3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 add(Vec3 a, Vec3 b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(Vec3 a, double b) { return {a[0] * b, a[1] * b, a[2] * b}; }
double norm(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 cross(Vec3 a, Vec3 b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(Vec3 a) {
  const double n = norm(a);
  return n > 0 ? mul(a, 1 / n) : Vec3{};
}
bool inside(const Plane& p, Vec3 x, Vec2* uv = nullptr) {
  const Vec3 d = sub(x, p.origin);
  const double uu = dot(p.u, p.u), vv = dot(p.v, p.v);
  if (uu <= 0 || vv <= 0 || std::abs(dot(p.u, p.v)) > 1e-6) return false;
  const double a = dot(d, p.u) / uu, b = dot(d, p.v) / vv;
  const Vec3 q = add(p.origin, add(mul(p.u, a), mul(p.v, b)));
  if (uv) *uv = {a, b};
  return norm(sub(q, x)) < 1e-5 && a >= -1e-8 && a <= 1 + 1e-8 && b >= -1e-8 && b <= 1 + 1e-8;
}
bool segment_hit(Vec3 a, Vec3 b, const Plane& p) {
  Vec3 n = cross(p.u, p.v), d = sub(b, a);
  double den = dot(d, n);
  if (std::abs(den) < 1e-12) return false;
  double t = dot(sub(p.origin, a), n) / den;
  return t > 1e-7 && t < 1 - 1e-7;
}
bool occluded(Vec3 a, Vec3 b, const std::vector<Occluder>& boxes) {
  const Vec3 d = sub(b, a);
  for (const auto& box : boxes) {
    double lo = 1e-7, hi = 1 - 1e-7;
    bool hit = true;
    for (int k = 0; k < 3; k++) {
      if (std::abs(d[k]) < 1e-12) {
        if (a[k] < box.min[k] || a[k] > box.max[k]) hit = false;
      } else {
        const double x = (box.min[k] - a[k]) / d[k], y = (box.max[k] - a[k]) / d[k];
        lo = std::max(lo, std::min(x, y));
        hi = std::min(hi, std::max(x, y));
        if (lo > hi) hit = false;
      }
    }
    if (hit) return true;
  }
  return false;
}
Vec3 at_time(const Plane& p, const PlanarMotion& m, double dt) {
  return add(p.origin, mul(m.linear_velocity, dt));
}
Vec3 rotate_axis(Vec3 v, Vec3 omega, double dt) {
  const double speed = norm(omega);
  if (speed < 1e-12 || dt == 0) return v;
  const Vec3 axis = mul(omega, 1.0 / speed);
  const double angle = speed * dt;
  return add(add(mul(v, std::cos(angle)), mul(cross(axis, v), std::sin(angle))),
             mul(axis, dot(axis, v) * (1 - std::cos(angle))));
}
double steering_delay(const DeviceProfile& d, Vec3 p) {
  if (d.kind != DeviceKind::kSteerableRaster) return 0;
  const Vec3 from = unit(sub(d.target, d.position)), to = unit(sub(p, d.position));
  const double angle = std::acos(std::clamp(dot(from, to), -1.0, 1.0));
  return d.settling_time + angle / d.max_angular_speed;
}
}  // namespace
struct Runtime::Impl {
  WorldSnapshot world;
  Rig rig;
  RuntimeOptions options;
  double now = 0;
  std::uint64_t gen = 1;
  std::vector<DisplayList> displays;
  std::map<Id, Availability> availability;
  std::map<Id, std::uint64_t> canceled;
  std::set<Id> canceled_ids;
  std::set<Id> seen_display_ids;
  Snapshot state;
  Impl(WorldSnapshot w, Rig r, RuntimeOptions o)
      : world(std::move(w)), rig(std::move(r)), options(o) {
    auto e = validate(world, rig, options);
    if (!e.empty()) throw std::invalid_argument(e.front().message);
    state.world_revision = world.world_revision;
    state.calibration_revision = world.calibration_revision;
    for (const auto& d : rig.devices) availability[d.id] = Availability::kAvailable;
  }
  Snapshot build(double t) {
    Snapshot s;
    s.time = t;
    s.world_revision = world.world_revision;
    s.calibration_revision = world.calibration_revision;
    if (options.traversal.order != Traversal::kStable) {
      s.diagnostics.push_back({PlanStatus::kUnsupported, "traversal", "unsupported-traversal",
                               "Only stable ordered traversal is currently modeled"});
      state = s;
      return s;
    }
    if (options.presentation.require_sync ||
        options.presentation.mode == Presentation::kSynchronizedGroup) {
      s.diagnostics.push_back(
          {PlanStatus::kUnsupported, "presentation", "sync-not-modeled",
           "Group synchronization is unavailable without a trigger-capable profile"});
      state = s;
      return s;
    }
    for (const auto& d : rig.devices)
      s.receipts.push_back(
          {d.id, gen, availability[d.id], t, d.clock_uncertainty, Provenance::kSimulated});
    std::map<Id, const DisplayList*> latest_by_app;
    for (const auto& x : displays)
      if (x.present_at <= t) {
        const Id app = x.app_id.empty() ? "default" : x.app_id;
        auto old = latest_by_app.find(app);
        if (old == latest_by_app.end() || old->second->present_at <= x.present_at)
          latest_by_app[app] = &x;
      }
    std::vector<const DisplayList*> active;
    for (const auto& x : displays) {
      const Id app = x.app_id.empty() ? "default" : x.app_id;
      auto latest = latest_by_app.find(app);
      if (latest != latest_by_app.end() && latest->second == &x && t < x.expires_at &&
          !canceled_ids.contains(x.id))
        active.push_back(&x);
    }
    std::stable_sort(active.begin(), active.end(),
                     [](const auto* a, const auto* b) { return a->present_at < b->present_at; });
    if (active.empty()) {
      state = s;
      return s;
    }
    struct Layer {
      const DisplayList* frame;
      DrawCall draw;
    };
    std::vector<Layer> draws;
    for (const auto* f : active) {
      auto diagnostics = validate(*f);
      s.diagnostics.insert(s.diagnostics.end(), diagnostics.begin(), diagnostics.end());
      for (const auto& c : f->draws) {
        draws.push_back({f, c});
      }
    }
    std::stable_sort(draws.begin(), draws.end(), [](const Layer& a, const Layer& b) {
      return a.draw.priority > b.draw.priority;
    });
    std::map<Id, Id> reserved;
    std::map<Id, double> device_next_free;
    std::size_t candidate_budget = options.planner_candidate_budget;
    for (const auto& layer : draws) {
      const auto& f = *layer.frame;
      const auto& c = layer.draw;
      if (f.world_revision != world.world_revision ||
          f.calibration_revision != world.calibration_revision) {
        s.diagnostics.push_back({PlanStatus::kInvalidated, c.draw_id, "revision-changed",
                                 "Submitted output uses an older world or calibration revision"});
        continue;
      }
      if (c.target.geometry == GeometryKind::kVolume || c.target.geometry == GeometryKind::kRayPath)
        continue;
      auto pi = std::find_if(world.planes.begin(), world.planes.end(),
                             [&](const Plane& p) { return p.id == c.target.surface_id; });
      if (pi == world.planes.end()) {
        s.diagnostics.push_back(
            {PlanStatus::kUnsupported, c.draw_id, "missing-surface", "Target surface is absent"});
        continue;
      }
      const std::size_t ix = std::distance(world.planes.begin(), pi);
      PlanarMotion m{};
      if (ix < world.motion.size()) m = world.motion[ix];
      if (t > world.valid_until) {
        s.diagnostics.push_back({PlanStatus::kInvalidated, c.draw_id, "stale-world",
                                 "World snapshot expired before output time"});
        continue;
      }
      Plane p = *pi;
      p.origin = at_time(p, m, t - world.source_time);
      p.u = rotate_axis(p.u, m.angular_velocity, t - world.source_time);
      p.v = rotate_axis(p.v, m.angular_velocity, t - world.source_time);
      std::vector<Vec3> points;
      if (c.target.geometry == GeometryKind::kPoint)
        points.push_back(
            c.target.coordinates == CoordinateSpace::kWorld
                ? c.target.origin
                : add(p.origin, add(mul(p.u, c.target.origin[0]), mul(p.v, c.target.origin[1]))));
      else if (c.target.geometry == GeometryKind::kPolyline) {
        const auto& verts = c.target.vertices;
        auto world_point = [&](Vec3 q) {
          return c.target.coordinates == CoordinateSpace::kWorld
                     ? q
                     : add(p.origin, add(mul(p.u, q[0]), mul(p.v, q[1])));
        };
        const std::size_t segment_count = verts.size() - 1 + (c.closed ? 1 : 0);
        for (std::size_t segment = 0; segment < segment_count; ++segment) {
          const std::size_t a_index = segment;
          const std::size_t b_index = (segment + 1) % verts.size();
          const Vec3 a = world_point(verts[a_index]), b = world_point(verts[b_index]);
          const auto n = divisions(norm(sub(b, a)), options.pipeline.sample_spacing,
                                   options.pipeline.max_samples);
          const std::size_t additional = n ? *n + (segment == 0 ? 1 : 0) : 0;
          if (!n || additional > options.pipeline.max_samples -
                                     std::min(points.size(), options.pipeline.max_samples)) {
            points.clear();
            s.diagnostics.push_back({PlanStatus::kInfeasible, c.draw_id, "sample-budget",
                                     "Polyline exceeds sample budget"});
            break;
          }
          for (std::size_t j = (segment == 0 ? 0 : 1); j <= *n; j++)
            points.push_back(add(mul(a, 1 - double(j) / *n), mul(b, double(j) / *n)));
        }
      } else if (c.target.geometry == GeometryKind::kPatch) {
        const double spacing = options.pipeline.sample_spacing;
        const auto nu = divisions(norm(c.target.u), spacing, options.pipeline.max_samples),
                   nv = divisions(norm(c.target.v), spacing, options.pipeline.max_samples);
        if (!nu || !nv || (*nu + 1) > options.pipeline.max_samples / (*nv + 1)) {
          s.diagnostics.push_back(
              {PlanStatus::kInfeasible, c.draw_id, "sample-budget", "Patch exceeds sample budget"});
          continue;
        }
        for (std::size_t i = 0; i <= *nu; i++)
          for (std::size_t j = 0; j <= *nv; j++) {
            Vec3 q = add(c.target.origin,
                         add(mul(c.target.u, double(i) / *nu), mul(c.target.v, double(j) / *nv)));
            points.push_back(c.target.coordinates == CoordinateSpace::kWorld
                                 ? q
                                 : add(p.origin, add(mul(p.u, q[0]), mul(p.v, q[1]))));
          }
      }
      if (c.target.geometry == GeometryKind::kPolyline &&
          points.size() > options.pipeline.max_samples) {
        s.diagnostics.push_back({PlanStatus::kInfeasible, c.draw_id, "sample-budget",
                                 "Polyline exceeds sample budget"});
        continue;
      }
      std::vector<Contribution> contributions;
      for (const Vec3 x : points) {
        if (!inside(p, x)) {
          s.diagnostics.push_back({PlanStatus::kUnsupported, c.draw_id, "outside-surface",
                                   "Target point is outside declared surface"});
          continue;
        }
        struct Candidate {
          const DeviceProfile* d;
          double score;
        };
        std::vector<Candidate> candidates;
        bool resource_blocked = false;
        for (const auto& device : rig.devices) {
          if (candidate_budget == 0) {
            s.diagnostics.push_back(
                {PlanStatus::kNoPlanFound, c.draw_id, "planner-budget",
                 "Deterministic candidate budget exhausted; no optimality claim is made"});
            break;
          }
          --candidate_budget;
          if (availability[device.id] != Availability::kAvailable) continue;
          if (points.size() > device.sample_budget) {
            s.diagnostics.push_back({PlanStatus::kInfeasible, c.draw_id, "device-sample-budget",
                                     "Target sample count exceeds device program budget"});
            continue;
          }
          bool resource_conflict = false;
          for (const auto& rid : device.resource_ids)
            if (reserved.contains(rid) && reserved.at(rid) != device.id) resource_conflict = true;
          if (resource_conflict) {
            resource_blocked = true;
            continue;
          }
          if (c.target.geometry == GeometryKind::kPatch && device.kind == DeviceKind::kGalvo) {
            s.diagnostics.push_back({PlanStatus::kUnsupported, c.draw_id,
                                     options.pipeline.allow_laser_fill_approximation
                                         ? "laser-fill-approximation-unimplemented"
                                         : "unsupported-laser-fill",
                                     options.pipeline.allow_laser_fill_approximation
                                         ? "Requested laser fill approximation is not implemented"
                                         : "Galvo fill is unsupported"});
            continue;
          }
          if (device.kind == DeviceKind::kFixedRaster ||
              device.kind == DeviceKind::kSteerableRaster || device.kind == DeviceKind::kGalvo) {
            const Vec3 fwd = unit(sub(device.target, device.position));
            const Vec3 right = unit(cross(fwd, device.up));
            const Vec3 up = cross(right, fwd);
            const Vec3 delta = sub(x, device.position);
            const double z = dot(delta, fwd), h = std::tan(device.fov_y / 2),
                         aspect = double(device.width) / device.height;
            if (z < device.near_plane || z > device.far_plane) continue;
            if (device.kind != DeviceKind::kSteerableRaster &&
                (std::abs(dot(delta, right) / (z * h * aspect)) > 1 ||
                 std::abs(dot(delta, up) / (z * h)) > 1))
              continue;
            if (segment_hit(device.position, x, p) || occluded(device.position, x, world.occluders))
              continue;
            candidates.push_back({&device, 1.0});
          }
        }
        if (candidates.empty()) {
          s.diagnostics.push_back({PlanStatus::kNoPlanFound, c.draw_id, "no-plan-found",
                                   "No available device can illuminate this target sample"});
          continue;
        }
        if (options.pipeline.output_mix == OutputMix::kExclusive) candidates.resize(1);
        {
          std::vector<Candidate> compatible;
          for (const auto& cand : candidates) {
            bool clash = false;
            for (const auto& rid : cand.d->resource_ids)
              if (std::any_of(compatible.begin(), compatible.end(), [&](const auto& x) {
                    return x.d->id != cand.d->id &&
                           std::find(x.d->resource_ids.begin(), x.d->resource_ids.end(), rid) !=
                               x.d->resource_ids.end();
                  }))
                clash = true;
            if (!clash)
              compatible.push_back(cand);
            else
              resource_blocked = true;
          }
          candidates = std::move(compatible);
        }
        if (resource_blocked)
          s.diagnostics.push_back({PlanStatus::kNoPlanFound, c.draw_id, "resource-conflict",
                                   "A shared physical resource is already reserved"});
        for (const auto& cand : candidates)
          for (const auto& rid : cand.d->resource_ids) reserved.emplace(rid, cand.d->id);
        RGB gain_sum{};
        for (const auto& cand : candidates)
          for (int k = 0; k < 3; k++) gain_sum[k] += cand.d->calibrated_gain[k];
        for (const auto& cand : candidates) {
          Contribution q;
          q.draw_id = c.draw_id;
          q.display_list_id = f.id;
          q.surface_id = c.target.surface_id;
          q.device_id = cand.d->id;
          q.position = x;
          q.intensity = c.intensity * c.opacity;
          if (cand.d->kind == DeviceKind::kGalvo)
            q.duty_fraction = 1.0 / (std::max<std::size_t>(1, points.size()) * 2.0);
          else
            q.duty_fraction = 1.0;
          for (int k = 0; k < 3; k++)
            q.linear_rgb[k] = c.linear_rgb[k] * cand.d->calibrated_gain[k];
          if (options.pipeline.output_mix == OutputMix::kNormalized)
            for (int k = 0; k < 3; k++) {
              if (gain_sum[k] >= 1.0)
                q.linear_rgb[k] /= gain_sum[k];
              else if (gain_sum[k] > 0 && c.linear_rgb[k] > 0)
                s.diagnostics.push_back({PlanStatus::kNoPlanFound, c.draw_id,
                                         "normalization-residual",
                                         "Available calibrated gain cannot meet the normalized "
                                         "target without exceeding device output"});
              else if (c.linear_rgb[k] > 0)
                s.diagnostics.push_back(
                    {PlanStatus::kNoPlanFound, c.draw_id, "unattainable-channel",
                     "No selected device emits a requested linear-light channel"});
            }
          contributions.push_back(q);
        }
      }
      RGB aggregate{};
      std::vector<Vec3> represented;
      for (const auto& q : contributions) {
        auto dev = std::find_if(rig.devices.begin(), rig.devices.end(),
                                [&](const auto& x) { return x.id == q.device_id; });
        if (dev != rig.devices.end() &&
            t >= std::max(f.present_at + dev->latency, device_next_free[dev->id]) +
                     steering_delay(*dev, q.position)) {
          auto same_bin = [&](const Contribution& old) {
            return old.surface_id == q.surface_id && norm(sub(old.position, q.position)) < 1e-8;
          };
          if (f.composition != Composition::kAdd) {
            for (auto& old : s.contributions) {
              if (!same_bin(old) || old.display_list_id == f.id) continue;
              if (f.composition == Composition::kReplace)
                old.intensity = 0;
              else
                old.intensity *= (1.0 - c.opacity);
            }
            for (auto& prior : s.programs)
              for (auto& event : prior.events)
                if (event.kind == EventKind::kEmit && event.display_list_id != f.id &&
                    event.surface_id == q.surface_id &&
                    norm(sub(event.position, q.position)) < 1e-8) {
                  if (f.composition == Composition::kReplace)
                    event.linear_rgb = {};
                  else
                    for (double& channel : event.linear_rgb) channel *= (1.0 - c.opacity);
                }
          }
          s.contributions.push_back(q);
          if (std::none_of(represented.begin(), represented.end(),
                           [&](Vec3 x) { return norm(sub(x, q.position)) < 1e-9; }))
            represented.push_back(q.position);
          for (int k = 0; k < 3; k++)
            aggregate[k] += q.linear_rgb[k] * q.intensity * q.duty_fraction.value_or(1.0);
        }
      }
      if (!represented.empty())
        for (double& channel : aggregate) channel /= represented.size();
      TargetLight light;
      light.draw_id = c.draw_id;
      light.display_list_id = f.id;
      light.aggregate_linear_rgb = aggregate;
      light.age = t - f.present_at;
      light.provenance = Provenance::kPredicted;
      if (!contributions.empty()) {
        std::vector<std::pair<Vec3, double>> duty_by_position;
        bool raster = false;
        bool scan = false;
        double scan_interval = 0;
        for (const auto& q : contributions) {
          auto duty_bin = std::find_if(
              duty_by_position.begin(), duty_by_position.end(),
              [&](const auto& bin) { return norm(sub(bin.first, q.position)) < 1e-8; });
          if (duty_bin == duty_by_position.end())
            duty_by_position.push_back({q.position, q.duty_fraction.value_or(0)});
          else
            duty_bin->second += q.duty_fraction.value_or(0);
          auto dev = std::find_if(rig.devices.begin(), rig.devices.end(),
                                  [&](const auto& d) { return d.id == q.device_id; });
          if (dev != rig.devices.end()) {
            if (dev->kind == DeviceKind::kGalvo) {
              scan = true;
              scan_interval =
                  std::min(1 / dev->sample_rate_hz, dev->max_sample_dwell) / (1 + dev->clock_drift);
            } else
              raster = true;
          }
        }
        double duty = 0;
        for (const auto& bin : duty_by_position) duty += std::min(1.0, bin.second);
        if (!duty_by_position.empty()) light.duty = duty / duty_by_position.size();
        if (raster)
          light.max_dark_gap = 0.0;
        else if (scan && !duty_by_position.empty())
          light.max_dark_gap = std::max(0.0, (2.0 * duty_by_position.size() - 1) * scan_interval);
      }
      if (options.traversal.minimum_duty > 0 &&
          (!light.duty || *light.duty + 1e-12 < options.traversal.minimum_duty))
        s.diagnostics.push_back({PlanStatus::kNoPlanFound, c.draw_id, "minimum-duty-unmet",
                                 "Predicted target duty is below the requested minimum"});
      if (options.traversal.max_dark_gap > 0 &&
          (!light.max_dark_gap || *light.max_dark_gap > options.traversal.max_dark_gap + 1e-12))
        s.diagnostics.push_back({PlanStatus::kNoPlanFound, c.draw_id, "max-dark-gap-exceeded",
                                 "Predicted dark gap exceeds the requested maximum"});
      s.targets.push_back(light);
      for (const auto& device : rig.devices) {
        std::vector<Contribution> owned;
        for (const auto& q : contributions)
          if (q.device_id == device.id) owned.push_back(q);
        if (owned.empty()) continue;
        TimedProgram program;
        program.device_id = device.id;
        program.device_kind = device.kind;
        program.display_list_id = f.id;
        program.generation = gen;
        program.world_revision = f.world_revision;
        program.calibration_revision = f.calibration_revision;
        program.issued_at = f.present_at;
        program.starts_at = std::max(f.present_at + device.latency, device_next_free[device.id]);
        program.expires_at = f.expires_at;
        const double interval = std::min(1 / device.sample_rate_hz, device.max_sample_dwell);
        double when = program.starts_at;
        auto emit = [&](EventKind kind, Vec3 pos, RGB rgb, double dwell) {
          const double local = when * (1 + device.clock_drift) + device.clock_offset;
          program.events.push_back(
              {kind, local, pos, rgb, dwell, c.draw_id, f.id, c.target.surface_id});
          when += dwell / (1 + device.clock_drift);
        };
        auto position_at = [&](Vec3 sampled, double global_time) {
          if (c.target.coordinates != CoordinateSpace::kSurfaceLocal) return sampled;
          const Vec3 delta = sub(sampled, p.origin);
          const Vec3 local{dot(delta, p.u) / dot(p.u, p.u), dot(delta, p.v) / dot(p.v, p.v), 0};
          const double dt = global_time - world.source_time;
          Plane moved = *pi;
          moved.origin = at_time(moved, m, dt);
          moved.u = rotate_axis(moved.u, m.angular_velocity, dt);
          moved.v = rotate_axis(moved.v, m.angular_velocity, dt);
          return add(moved.origin, add(mul(moved.u, local[0]), mul(moved.v, local[1])));
        };
        Vec3 previous = device.target;
        for (const auto& q : owned) {
          if (device.kind == DeviceKind::kSteerableRaster) {
            Vec3 pose = position_at(q.position, when);
            const Vec3 a = unit(sub(previous, device.position)),
                       b = unit(sub(pose, device.position));
            const double angle = std::acos(std::clamp(dot(a, b), -1.0, 1.0));
            when += angle / device.max_angular_speed + device.settling_time;
            pose = position_at(q.position, when);
            emit(EventKind::kPose, pose, {}, 0);
            previous = pose;
          }
          if (device.kind == DeviceKind::kGalvo) {
            emit(EventKind::kBlank, position_at(q.position, when), {}, interval);
          }
          RGB command_rgb = q.linear_rgb;
          for (double& channel : command_rgb) channel *= q.intensity;
          emit(EventKind::kEmit, position_at(q.position, when), command_rgb, interval);
        }
        if (device.kind == DeviceKind::kSteerableRaster || device.kind == DeviceKind::kGalvo)
          device_next_free[device.id] = when;
        auto duplicate = std::find_if(s.programs.begin(), s.programs.end(), [&](const auto& q) {
          return q.device_id == program.device_id && q.display_list_id == program.display_list_id &&
                 q.events.front().draw_id == c.draw_id;
        });
        if (duplicate == s.programs.end()) s.programs.push_back(std::move(program));
      }
    }
    for (auto& light : s.targets) {
      RGB sum{};
      std::vector<Vec3> positions;
      for (const auto& q : s.contributions) {
        if (q.display_list_id != light.display_list_id || q.draw_id != light.draw_id) continue;
        if (std::none_of(positions.begin(), positions.end(),
                         [&](Vec3 p) { return norm(sub(p, q.position)) < 1e-8; }))
          positions.push_back(q.position);
        for (int k = 0; k < 3; ++k)
          sum[k] += q.linear_rgb[k] * q.intensity * q.duty_fraction.value_or(1.0);
      }
      if (positions.empty()) {
        light.aggregate_linear_rgb = {};
      } else {
        for (double& channel : sum) channel /= positions.size();
        light.aggregate_linear_rgb = sum;
      }
    }
    // Only contributions whose latency has elapsed are observably emitted.
    state = s;
    return s;
  }
};

Runtime::Runtime(WorldSnapshot w, Rig r, RuntimeOptions o)
    : impl_(std::make_unique<Impl>(std::move(w), std::move(r), o)) {}
Runtime::~Runtime() = default;
Runtime::Runtime(Runtime&&) noexcept = default;
Runtime& Runtime::operator=(Runtime&&) noexcept = default;
std::vector<Diagnostic> Runtime::submit(DisplayList d) {
  auto e = validate(d);
  if (!e.empty()) return e;
  if (impl_->seen_display_ids.contains(d.id))
    return {{PlanStatus::kInfeasible, d.id, "duplicate-display-id",
             "Display list IDs are unique for the lifetime of a runtime"}};
  if (d.present_at < impl_->now)
    return {
        {PlanStatus::kInfeasible, d.id, "past-submission", "Cannot submit display in the past"}};
  const bool has_raster =
      std::any_of(impl_->rig.devices.begin(), impl_->rig.devices.end(), [](const auto& x) {
        return x.kind == DeviceKind::kFixedRaster || x.kind == DeviceKind::kSteerableRaster;
      });
  if (!has_raster)
    for (const auto& c : d.draws)
      if (c.target.geometry == GeometryKind::kPatch)
        return {{PlanStatus::kUnsupported, c.draw_id, "unsupported-laser-fill",
                 "Galvo fill is unsupported without a supported approximation"}};
  if (d.world_revision == 0) d.world_revision = impl_->world.world_revision;
  if (d.calibration_revision == 0) d.calibration_revision = impl_->world.calibration_revision;
  impl_->seen_display_ids.insert(d.id);
  impl_->displays.push_back(std::move(d));
  return {};
}
Snapshot Runtime::advance(double t) {
  if (!std::isfinite(t) || t < impl_->now)
    throw std::invalid_argument("Clock must advance monotonically");
  impl_->now = t;
  return impl_->build(t);
}
Snapshot Runtime::snapshot() const { return impl_->state; }
std::uint64_t Runtime::cancel(const Id& id) {
  const auto g = ++impl_->gen;
  impl_->canceled[id] = g;
  impl_->canceled_ids.insert(id);
  return g;
}
void Runtime::set_availability(const Id& id, Availability a) {
  if (!impl_->availability.contains(id)) throw std::invalid_argument("Unknown device");
  if (a != Availability::kAvailable && a != Availability::kUnavailable &&
      a != Availability::kFailed && a != Availability::kHandingOff)
    throw std::invalid_argument("Unknown availability state");
  impl_->availability[id] = a;
  ++impl_->gen;
}
void Runtime::update_world(WorldSnapshot w) {
  auto e = validate(w, impl_->rig, impl_->options);
  if (!e.empty()) throw std::invalid_argument(e.front().message);
  if (w.world_revision <= impl_->world.world_revision ||
      w.calibration_revision < impl_->world.calibration_revision)
    throw std::invalid_argument("World revisions must advance");
  impl_->world = std::move(w);
  ++impl_->gen;
  impl_->state = impl_->build(impl_->now);
}
}  // namespace spatialgl::optics
