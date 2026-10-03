#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

#include "spatialgl/spatialgl.h"

using namespace spatialgl;
namespace {
void check(bool condition) {
  if (!condition) throw std::runtime_error("Expectation failed");
}
void close(double a, double b) { check(std::abs(a - b) < 1e-9); }
void throws(const std::function<void()>& f) {
  try {
    f();
  } catch (const std::invalid_argument&) {
    return;
  }
  throw std::runtime_error("Expected invalid_argument");
}
World wall() { return {{{"wall", {-2, 0, 0}, {4, 0, 0}, {0, 3, 0}}}, {}}; }
SceneFrame point(std::string id = "p", Vec3 position = {0, 1, 0}) {
  return {id, 0, 5, {Point{"dot", position, {1, 0, 0}, {}}}};
}
std::shared_ptr<ProjectorBackend> projector(std::string id = "projector", double latency = 0) {
  ProjectorConfig p;
  p.id = id;
  p.refresh_hz = 10;
  p.latency = latency;
  return std::make_shared<ProjectorBackend>(p);
}
std::shared_ptr<DroneBackend> drone(std::size_t count = 1) {
  DroneConfig d;
  d.id = "drone";
  d.count = count;
  d.speed = 1;
  d.home = {0, 1, 1};
  d.refresh_hz = 10;
  return std::make_shared<DroneBackend>(d);
}
class CustomEmitter : public Backend {
 public:
  DeviceDescriptor descriptor() const override { return {"custom", "emitter", 10, 0, 1}; }
  Feasibility evaluate(const Sample& s, const World&) const override {
    return Candidate{s, 1, s.position, {}};
  }
  DeviceCommand encode(const std::vector<Candidate>& candidates) const override {
    return ExtensionCommand{"custom/v1", candidates.empty() ? "" : "dot:0"};
  }
  void advance(double) override {}
  void apply(const DeviceFrame& f) override { frame_ = f; }
  std::vector<Observation> observe(double now) const override {
    if (!frame_ || now >= frame_->expires_at ||
        std::get<ExtensionCommand>(frame_->command).payload.empty())
      return {};
    return {{"dot:0", {0, 1, 0}, {1, 0, 0}}};
  }
  void reset() override { frame_.reset(); }
  std::unique_ptr<Backend> clone() const override { return std::make_unique<CustomEmitter>(); }

 private:
  std::optional<DeviceFrame> frame_;
};
}  // namespace
int main() {
  int failed = 0, passed = 0;
  auto test = [&](const char* name, const std::function<void()>& run) {
    try {
      run();
      ++passed;
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception& e) {
      ++failed;
      std::cerr << "FAIL " << name << ": " << e.what() << '\n';
    }
  };
  test("projector support", [] {
    check(std::get<std::string>(
              projector()->evaluate(sample_scene(point("air", {0, 1, 1}).primitives)[0], wall())) ==
          "no-support-surface");
  });
  test("pinhole pixels", [] {
    Runtime r(wall(), {projector()});
    r.submit(point());
    auto s = r.advance(0);
    check(s.samples[0].status == "realized");
    check(*s.samples[0].error > 0);
    check(std::get<RasterCommand>(s.device_frames[0].command).samples[0].pixel ==
          std::array<int, 2>{320, 240});
  });
  test("frustum", [] {
    ProjectorConfig p;
    p.id = "narrow";
    p.fov_y = 0.1;
    ProjectorBackend b(p);
    check(std::get<std::string>(b.evaluate(sample_scene(point("p", {1, 1, 0}).primitives)[0],
                                           wall())) == "outside-frustum");
  });
  test("occlusion segment", [] {
    auto w = wall();
    w.occluders.push_back({"box", {-0.5, 0.5, 1}, {0.5, 1.5, 2}});
    check(std::get<std::string>(projector()->evaluate(sample_scene(point().primitives)[0], w)) ==
          "occluded");
    check(!blocked({0, 1, 3}, {0, 1, 0}, {{"behind", {-1, 0, -3}, {1, 2, -1}}}));
  });
  test("projector fallback", [] {
    auto w = wall();
    w.occluders.push_back({"box", {-0.2, 0.5, 1.4}, {0.2, 1.5, 1.6}});
    ProjectorConfig p;
    p.id = "other";
    p.position = {2, 1, 3};
    Runtime r(w, {projector(), std::make_shared<ProjectorBackend>(p)});
    r.submit(point());
    check(r.advance(0).samples[0].device_id == "other");
  });
  test("monitor preference", [] {
    MonitorConfig m;
    m.id = "monitor";
    m.surface_id = "wall";
    Runtime r(wall(), {projector(), std::make_shared<MonitorBackend>(m)});
    r.submit(point());
    check(r.advance(0).samples[0].device_id == "monitor");
  });
  test("pixel conflict", [] {
    auto f = point();
    f.primitives.push_back(Point{"other", {0, 1, 0}, {1, 0, 0}, {}});
    Runtime r(wall(), {projector()});
    r.submit(f);
    auto s = r.advance(0);
    check(s.samples[1].status == "unrealizable");
    check(s.samples[1].reasons[0] == "projector: pixel-conflict");
  });
  test("drone capacity", [] {
    auto f = point();
    f.primitives.push_back(Point{"other", {0, 1, 1}, {1, 1, 0}, {}});
    Runtime r({}, {drone()});
    r.submit(f);
    check(r.advance(0).samples[1].reasons[0] == "drone: capacity");
  });
  test("unsupported patch", [] {
    Runtime r({}, {drone()});
    r.submit({"patch", 0, 5, {Patch{"p", {0, 1, 1}, {1, 0, 0}, {0, 1, 0}, {1, 1, 1}, {}}}});
    check(r.advance(0).samples[0].reasons[0] == "drone: unsupported-primitive");
  });
  test("present and latency", [] {
    Runtime r(wall(), {projector("p", 0.15)});
    auto f = point();
    f.present_at = 0.2;
    r.submit(f);
    check(!r.advance(0.19).scene_id);
    check(r.advance(0.2).samples[0].status == "pending");
    check(r.advance(0.351).samples[0].status == "realized");
  });
  test("drone speed", [] {
    Runtime r({}, {drone()});
    r.submit(point("p", {0, 1, 2}));
    close((*r.advance(0.5).samples[0].actual)[2], 1.5);
    check(r.advance(1).samples[0].status == "realized");
  });
  test("step invariance", [] {
    Runtime a({}, {drone()}), b({}, {drone()});
    a.submit(point("p", {0, 1, 2}));
    b.submit(point("p", {0, 1, 2}));
    for (int i = 0; i <= 53; ++i) b.advance(i / 100.0);
    close((*a.advance(0.53).samples[0].actual)[2], (*b.snapshot().samples[0].actual)[2]);
  });
  test("stale provenance", [] {
    Runtime r(wall(), {projector("p", 0.2)});
    r.submit(point("first"));
    r.advance(0.3);
    auto f = point("second", {0.5, 1, 0});
    f.present_at = 0.3;
    r.submit(f);
    auto s = r.advance(0.35);
    check(s.samples[0].source_scene_id == "first");
    check(*s.samples[0].error > 0.4);
    check(s.outputs[0].stale);
  });
  test("late expired output", [] {
    Runtime r(wall(), {projector("p", 0.3)});
    auto f = point();
    f.expires_at = 0.15;
    r.submit(f);
    auto s = r.advance(0.31);
    check(s.samples.empty() && s.outputs.empty());
  });
  test("replacement clear", [] {
    Runtime r(wall(), {projector()});
    r.submit(point());
    r.advance(0.2);
    r.submit({"clear", 0.2, 3, {}});
    check(r.advance(0.31).outputs.empty());
  });
  test("no resurrection", [] {
    Runtime r(wall(), {projector()});
    r.submit(point());
    r.advance(0.2);
    auto f = point("new");
    f.present_at = 0.2;
    f.expires_at = 0.4;
    r.submit(f);
    check(!r.advance(0.5).scene_id);
  });
  test("removed output", [] {
    Runtime r(wall(), {projector("p", 0.2)});
    r.submit(point());
    r.advance(0.3);
    r.submit({"clear", 0.3, 3, {}});
    auto s = r.advance(0.35);
    check(s.samples.empty() && s.outputs.size() == 1 && s.outputs[0].stale);
    check(r.advance(0.61).outputs.empty());
  });
  test("independent clocks", [] {
    MonitorConfig m;
    m.id = "m";
    m.surface_id = "wall";
    m.refresh_hz = 2;
    Runtime r(wall(), {std::make_shared<MonitorBackend>(m), projector()});
    r.submit(point());
    for (const auto& f : r.advance(0.35).device_frames)
      close(f.issued_at, f.device_id == "m" ? 0 : 0.3);
  });
  test("custom schema", [] {
    Runtime r(wall(), {std::make_shared<CustomEmitter>()});
    r.submit(point());
    auto s = r.advance(0);
    check(s.samples[0].status == "realized");
    check(std::get<ExtensionCommand>(s.device_frames[0].command).schema == "custom/v1");
  });
  test("invalid calibration", [] {
    ProjectorConfig p;
    p.id = "bad";
    p.fov_y = std::nan("");
    throws([&] { ProjectorBackend b(p); });
    DroneConfig d;
    d.id = "bad";
    d.home = {10, 0, 0};
    throws([&] { DroneBackend b(d); });
  });
  test("invalid submissions", [] {
    Runtime r(wall(), {projector()});
    r.submit(point());
    throws([&] { r.submit(point()); });
    r.advance(0.3);
    throws([&] { r.advance(0.2); });
    throws([&] { r.submit(point("late")); });
  });
  test("invalid scene and world", [] {
    auto f = point();
    f.primitives.push_back(std::get<Point>(f.primitives[0]));
    throws([&] { validate_frame(f); });
    auto w = wall();
    w.surfaces[0].u = {0, 0, 0};
    throws([&] { validate_world(w); });
  });
  test("copy isolation", [] {
    auto b = drone();
    Runtime a({}, {b}), c({}, {b});
    a.submit(point("a", {0, 1, 2}));
    c.submit(point("c", {0, 1, 0}));
    a.advance(0.5);
    close((*c.advance(0.5).samples[0].actual)[2], 0.5);
  });
  test("sampling budget", [] {
    Runtime r({}, {});
    throws([&] {
      r.submit({"huge", 0, 1, {Patch{"p", {0, 0, 0}, {1000, 0, 0}, {0, 1000, 0}, {1, 1, 1}, {}}}});
    });
    check(!r.snapshot().scene_id);
  });
  test("no backend", [] {
    Runtime r({}, {});
    r.submit(point());
    check(r.advance(0).samples[0].reasons[0] == "none: no-device");
  });
  std::cout << passed << " passed, " << failed << " failed\n";
  return failed ? 1 : 0;
}
