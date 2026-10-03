#include "spatialgl/raster.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "spatialgl/glfw_output.h"

using namespace spatialgl;
namespace {
void check(bool ok, int line) {
  if (!ok) throw std::runtime_error("check failed at line " + std::to_string(line));
}
#define CHECK(x) check((x), __LINE__)
optics::Snapshot render_snapshot(std::vector<optics::Vec3> points,
                                 std::vector<optics::RGB> colors = {}, double latency = 0) {
  optics::WorldSnapshot w;
  w.planes.push_back({"wall", {-2, -2, -2}, {4, 0, 0}, {0, 4, 0}});
  w.source_time = 0;
  w.valid_until = 4;
  w.world_revision = 7;
  w.calibration_revision = 9;
  optics::DeviceProfile d;
  d.id = "projector";
  d.position = {0, 0, 0};
  d.target = {0, 0, -1};
  d.up = {0, 1, 0};
  d.width = 4;
  d.height = 4;
  d.fov_y = std::acos(-1.0) / 2;
  d.near_plane = .1;
  d.far_plane = 10;
  d.latency = latency;
  optics::Rig rig;
  rig.devices = {d};
  optics::Runtime runtime(w, rig);
  optics::DisplayList display;
  display.id = "display";
  display.expires_at = 2;
  for (size_t i = 0; i < points.size(); ++i) {
    optics::DrawCall call;
    call.draw_id = "draw" + std::to_string(i);
    call.target.surface_id = "wall";
    call.target.origin = points[i];
    call.target.geometry = optics::GeometryKind::kPoint;
    if (i < colors.size()) call.linear_rgb = colors[i];
    display.draws.push_back(call);
  }
  CHECK(runtime.submit(display).empty());
  return runtime.advance(.5);
}
std::uint8_t at(const raster::RasterFrame& f, int x, int y, int channel = 0) {
  return f.rgb[(static_cast<size_t>(y) * f.width + x) * 3 + channel];
}
}  // namespace
int main() {
  try {
    optics::DeviceProfile device;
    device.id = "projector";
    device.position = {0, 0, 0};
    device.target = {0, 0, -1};
    device.up = {0, 1, 0};
    device.width = 4;
    device.height = 4;
    device.fov_y = std::acos(-1.0) / 2;
    device.near_plane = .1;
    device.far_plane = 10;
    // Independent simulator fixture: center, up and down samples map to top-left raster rows.
    auto s = render_snapshot({{0, 0, -2}, {0, .5, -2}, {0, -.5, -2}});
    raster::Calibration c;
    c.point_radius = 0;
    auto frame = raster::compile(s, device, c);
    CHECK(frame.width == 4 && frame.height == 4 && frame.world_revision == 7 &&
          frame.calibration_revision == 9);
    CHECK(at(frame, 2, 2) > 250);
    CHECK(at(frame, 2, 1) > 250);
    CHECK(at(frame, 0, 0) == 0);
    auto wide_device = device;
    wide_device.width = 8;
    auto aspect_frame = raster::compile(render_snapshot({{1, 0, -2}}), wide_device, c);
    CHECK(at(aspect_frame, 5, 2) > 250);
    CHECK(at(aspect_frame, 6, 2) == 0);
    // Clipping and aspect are from the same pinhole projection used by simulation.
    auto clipped = render_snapshot({{0, 0, -.05}, {10, 0, -2}, {0, 0, -11}});
    auto black = raster::compile(clipped, device, c);
    for (auto b : black.rgb) CHECK(b == 0);
    // Exact coincident world samples add; distinct world samples colliding in one pixel max.
    auto coincident = render_snapshot({{0, 0, -2}, {0, 0, -2}}, {{.2, 0, 0}, {.2, 0, 0}});
    auto together = raster::compile(coincident, device, c);
    auto distinct = render_snapshot({{-.05, 0, -2}, {.05, 0, -2}}, {{.2, 0, 0}, {.2, 0, 0}});
    auto collided = raster::compile(distinct, device, c);
    CHECK(at(together, 2, 2) == 170);
    CHECK(at(collided, 2, 2) == 124);
    auto linear = c;
    linear.transfer = raster::TransferFunction::kLinear;
    auto linear_frame =
        raster::compile(render_snapshot({{0, 0, -2}}, {{.2, 0, 0}}), device, linear);
    CHECK(at(linear_frame, 2, 2) == 51);
    // Homography maps a corner fixture exactly; reject collinear/duplicate correspondences.
    std::array<optics::Vec2, 4> src{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    std::array<optics::Vec2, 4> dst{{{.1, .2}, {.9, .1}, {.8, .9}, {.2, .8}}};
    const auto h = raster::fit_homography(src, dst);
    for (int i = 0; i < 4; i++) {
      double z = h[6] * src[i][0] + h[7] * src[i][1] + h[8];
      CHECK(std::hypot((h[0] * src[i][0] + h[1] * src[i][1] + h[2]) / z - dst[i][0],
                       (h[3] * src[i][0] + h[4] * src[i][1] + h[5]) / z - dst[i][1]) < 1e-7);
    }
    bool degenerate = false;
    try {
      auto bad = src;
      bad[3] = {2, 0};
      (void)raster::fit_homography(bad, dst);
    } catch (const std::invalid_argument&) {
      degenerate = true;
    }
    CHECK(degenerate);
    bool singular = false;
    try {
      auto bad = c;
      bad.homography = {0, 0, .5, 0, 0, .5, 0, 0, 1};
      (void)raster::compile(s, device, bad);
    } catch (const std::invalid_argument&) {
      singular = true;
    }
    CHECK(singular);
    auto calibrated = c;
    calibrated.homography = {1, 0, .25, 0, 1, 0, 0, 0, 1};
    auto shifted = raster::compile(render_snapshot({{0, 0, -2}}), device, calibrated);
    CHECK(at(shifted, 3, 2) > 250);
    const auto scale_fixture = render_snapshot({{0, 0, -2}});
    const auto expected_scaled = raster::compile(scale_fixture, device, c);
    for (double scale : {1e-15, 1e200, -1e-100}) {
      auto equivalent = c;
      for (double& coefficient : equivalent.homography) coefficient *= scale;
      CHECK(raster::compile(scale_fixture, device, equivalent).rgb == expected_scaled.rgb);
    }
    // Receipt missing/unavailable, inconsistent revisions and malformed matching input stay black.
    auto unavailable = s;
    unavailable.receipts[0].availability = optics::Availability::kUnavailable;
    auto off = raster::compile(unavailable, device, c);
    for (auto b : off.rgb) CHECK(b == 0);
    auto no_receipt = s;
    no_receipt.receipts.clear();
    auto missing = raster::compile(no_receipt, device, c);
    for (auto b : missing.rgb) CHECK(b == 0);
    auto stale = s;
    stale.programs[0].world_revision++;
    auto stale_frame = raster::compile(stale, device, c);
    for (auto b : stale_frame.rgb) CHECK(b == 0);
    auto malformed = s;
    malformed.contributions[0].position[0] = std::numeric_limits<double>::quiet_NaN();
    auto bad_frame = raster::compile(malformed, device, c);
    for (auto b : bad_frame.rgb) CHECK(b == 0);
    CHECK(!bad_frame.diagnostics.empty());
    // Expired and latency-pending programs produce black output.
    auto expired = s;
    expired.time = 2.1;
    auto expired_frame = raster::compile(expired, device, c);
    for (auto b : expired_frame.rgb) CHECK(b == 0);
    auto pending = render_snapshot({{0, 0, -2}}, {}, 1.0);
    auto pending_frame = raster::compile(pending, device, c);
    for (auto b : pending_frame.rgb) CHECK(b == 0);
    // A pending second application cannot erase the currently active application.
    {
      optics::WorldSnapshot w;
      w.planes.push_back({"wall", {-2, -2, -2}, {4, 0, 0}, {0, 4, 0}});
      w.valid_until = 4;
      w.world_revision = 7;
      w.calibration_revision = 9;
      auto d = device;
      d.latency = .5;
      optics::Rig rig;
      rig.devices = {d};
      optics::Runtime runtime(w, rig);
      auto make_display = [](std::string id, double at) {
        optics::DrawCall q;
        q.draw_id = "dot";
        q.target.surface_id = "wall";
        q.target.origin = {0, 0, -2};
        optics::DisplayList f;
        f.id = id;
        f.app_id = id;
        f.present_at = at;
        f.expires_at = 3;
        f.draws = {q};
        return f;
      };
      CHECK(runtime.submit(make_display("active", 0)).empty());
      auto first = runtime.advance(.6);
      CHECK(runtime.submit(make_display("pending", .6)).empty());
      auto both = runtime.advance(.7);
      auto active_frame = raster::compile(both, d, c);
      CHECK(at(active_frame, 2, 2) > 250);
      runtime.set_availability("projector", optics::Availability::kUnavailable);
      auto offline = runtime.advance(.8);
      auto offline_frame = raster::compile(offline, d, c);
      for (auto b : offline_frame.rgb) CHECK(b == 0);
    }
    // A canceled program produces no renderable output.
    {
      optics::WorldSnapshot w;
      w.planes.push_back({"wall", {-2, -2, -2}, {4, 0, 0}, {0, 4, 0}});
      w.valid_until = 4;
      w.world_revision = 7;
      w.calibration_revision = 9;
      optics::Rig rig;
      rig.devices = {device};
      optics::Runtime runtime(w, rig);
      optics::DrawCall q;
      q.draw_id = "dot";
      q.target.surface_id = "wall";
      q.target.origin = {0, 0, -2};
      optics::DisplayList f;
      f.id = "canceled";
      f.expires_at = 3;
      f.draws = {q};
      CHECK(runtime.submit(f).empty());
      runtime.cancel("canceled");
      auto canceled = runtime.advance(.5);
      auto canceled_frame = raster::compile(canceled, device, c);
      for (auto b : canceled_frame.rgb) CHECK(b == 0);
    }
    // PPM bytes and malformed frame errors are observable.
    const std::string path = "/tmp/spatialgl-raster-test.ppm";
    raster::write_ppm(frame, path);
    std::ifstream in(path, std::ios::binary);
    std::string magic;
    in >> magic;
    CHECK(magic == "P6");
    in.close();
    std::remove(path.c_str());
    bool io_error = false;
    try {
      raster::write_ppm(frame, "/definitely/missing/directory/out.ppm");
    } catch (const std::runtime_error&) {
      io_error = true;
    }
    CHECK(io_error);
    bool glfw_load_error = false;
    try {
      raster::GlfwOutput out(4, 4, "headless", -1, "/definitely/missing/libglfw.dylib");
    } catch (const std::runtime_error&) {
      glfw_load_error = true;
    }
    CHECK(glfw_load_error);
    glfw_load_error = false;
    try {
      (void)raster::GlfwOutput::displays("/definitely/missing/libglfw.dylib");
    } catch (const std::runtime_error&) {
      glfw_load_error = true;
    }
    CHECK(glfw_load_error);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
