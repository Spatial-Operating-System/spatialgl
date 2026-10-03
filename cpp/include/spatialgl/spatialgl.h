#ifndef SPATIALGL_SPATIALGL_H_
#define SPATIALGL_SPATIALGL_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace spatialgl {
using Vec3 = std::array<double, 3>;
using RGB = Vec3;
struct Point {
  std::string id;
  Vec3 position;
  RGB color{1, 1, 1};
  std::string surface_id;
};
struct Polyline {
  std::string id;
  std::vector<Vec3> vertices;
  RGB color{1, 1, 1};
  std::string surface_id;
};
struct Patch {
  std::string id;
  Vec3 origin, u, v;
  RGB color{1, 1, 1};
  std::string surface_id;
};
using Primitive = std::variant<Point, Polyline, Patch>;
struct SceneFrame {
  std::string id;
  double present_at, expires_at;
  std::vector<Primitive> primitives;
};
struct Surface {
  std::string id;
  Vec3 origin, u, v;
};
struct Occluder {
  std::string id;
  Vec3 min, max;
};
struct World {
  std::vector<Surface> surfaces;
  std::vector<Occluder> occluders;
};
enum class Kind { Point, Polyline, Patch };
struct Sample {
  std::string id, primitive_id;
  Kind kind;
  Vec3 position;
  RGB color;
  std::string surface_id;
};
struct Candidate {
  Sample sample;
  double score;
  Vec3 position;
  std::optional<std::array<int, 2>> pixel;
};
using Feasibility = std::variant<Candidate, std::string>;
struct DeviceDescriptor {
  std::string id, modality;
  double refresh_hz = 30, latency = 0;
  std::size_t capacity = std::numeric_limits<std::size_t>::max();
};
struct RasterCommand {
  int width, height;
  std::vector<Candidate> samples;
};
struct EmitterCommand {
  std::vector<Candidate> targets;
};
// The core preserves custom backend payloads without interpreting their schema.
struct ExtensionCommand {
  std::string schema, payload;
};
using DeviceCommand = std::variant<RasterCommand, EmitterCommand, ExtensionCommand>;
struct DeviceFrame {
  std::string device_id, scene_id;
  double issued_at, apply_at, expires_at;
  DeviceCommand command;
};
struct Observation {
  std::string sample_id;
  Vec3 position;
  RGB color;
};
class Backend {
 public:
  virtual ~Backend() = default;
  virtual DeviceDescriptor descriptor() const = 0;
  virtual Feasibility evaluate(const Sample&, const World&) const = 0;
  virtual DeviceCommand encode(const std::vector<Candidate>&) const = 0;
  virtual void advance(double dt) = 0;
  virtual void apply(const DeviceFrame&) = 0;
  virtual std::vector<Observation> observe(double now) const = 0;
  virtual void reset() = 0;
  // Each runtime owns a fresh backend instance; configuration objects are reusable.
  virtual std::unique_ptr<Backend> clone() const = 0;
};
struct MonitorConfig {
  std::string id, surface_id;
  int width = 640, height = 480;
  double refresh_hz = 30, latency = 0;
};
struct ProjectorConfig {
  std::string id;
  Vec3 position{0, 1, 3}, target{0, 1, 0}, up{0, 1, 0};
  double fov_y = 1.5707963267948966, near = 0.1, far = 10;
  int width = 640, height = 480;
  double refresh_hz = 30, latency = 0;
};
struct DroneConfig {
  std::string id;
  std::size_t count = 3;
  double speed = 1.2;
  Vec3 home{0, 0.2, 1}, min{-3, 0, -3}, max{3, 3, 3};
  double refresh_hz = 30, latency = 0;
};
class RasterBackend : public Backend {
 public:
  void advance(double) override {}
  void apply(const DeviceFrame&) override;
  std::vector<Observation> observe(double now) const override;
  void reset() override { frame_.reset(); }

 protected:
  std::optional<DeviceFrame> frame_;
};
class MonitorBackend final : public RasterBackend {
 public:
  explicit MonitorBackend(MonitorConfig config);
  DeviceDescriptor descriptor() const override;
  Feasibility evaluate(const Sample&, const World&) const override;
  DeviceCommand encode(const std::vector<Candidate>&) const override;
  std::unique_ptr<Backend> clone() const override;

 private:
  MonitorConfig config_;
};
class ProjectorBackend final : public RasterBackend {
 public:
  explicit ProjectorBackend(ProjectorConfig config);
  DeviceDescriptor descriptor() const override;
  Feasibility evaluate(const Sample&, const World&) const override;
  DeviceCommand encode(const std::vector<Candidate>&) const override;
  std::unique_ptr<Backend> clone() const override;

 private:
  ProjectorConfig config_;
};
class DroneBackend final : public Backend {
 public:
  explicit DroneBackend(DroneConfig config);
  DeviceDescriptor descriptor() const override;
  Feasibility evaluate(const Sample&, const World&) const override;
  DeviceCommand encode(const std::vector<Candidate>&) const override;
  void advance(double dt) override;
  void apply(const DeviceFrame&) override;
  std::vector<Observation> observe(double now) const override;
  void reset() override;
  std::unique_ptr<Backend> clone() const override;

 private:
  DroneConfig config_;
  std::optional<DeviceFrame> frame_;
  std::vector<Vec3> positions_;
  std::map<std::string, std::size_t> bindings_;
};
struct Unmet {
  Sample sample;
  std::vector<std::pair<std::string, std::string>> attempts;
};
struct Plan {
  std::map<std::string, std::vector<Candidate>> assignments;
  std::vector<Unmet> unmet;
};
struct RealizedSample {
  Sample sample;
  std::optional<std::string> device_id;
  std::optional<Vec3> actual;
  std::optional<double> error;
  std::string status;
  std::optional<std::string> source_scene_id;
  std::optional<double> age;
  std::vector<std::string> reasons;
};
struct ObservedOutput : Observation {
  std::string device_id;
  std::optional<std::string> source_scene_id;
  double age;
  bool stale;
};
struct RealizationState {
  double time;
  std::optional<std::string> scene_id;
  std::vector<RealizedSample> samples;
  std::vector<DeviceFrame> device_frames;
  std::vector<ObservedOutput> outputs;
};
Vec3 add(Vec3 a, Vec3 b);
Vec3 sub(Vec3 a, Vec3 b);
Vec3 scale(Vec3 a, double factor);
double dot(Vec3 a, Vec3 b);
double distance(Vec3 a, Vec3 b);
std::optional<std::array<double, 2>> surface_uv(Vec3 p, const Surface& surface);
bool blocked(Vec3 a, Vec3 b, const std::vector<Occluder>& occluders);
void validate_world(const World& world);
void validate_frame(const SceneFrame& frame);
std::vector<Sample> sample_scene(const std::vector<Primitive>& primitives, double spacing = 0.15);
Plan plan(const std::vector<Sample>& samples, const std::vector<std::unique_ptr<Backend>>& backends,
          const World& world);
class Runtime {
 public:
  Runtime(World world, const std::vector<std::shared_ptr<Backend>>& backends, double spacing = 0.15,
          double tolerance = 0.02);
  void submit(SceneFrame frame);
  RealizationState advance(double to);
  RealizationState snapshot() const;
  const World& world() const { return world_; }

 private:
  const SceneFrame* scene_at(double time) const;
  World world_;
  std::vector<std::unique_ptr<Backend>> backends_;
  double spacing_, tolerance_, time_ = 0;
  std::vector<SceneFrame> frames_;
  std::vector<std::uint64_t> refresh_counts_;
  std::vector<DeviceFrame> pending_;
  std::map<std::string, DeviceFrame> applied_;
};
}  // namespace spatialgl
#endif  // SPATIALGL_SPATIALGL_H_
