#ifndef SPATIALGL_OPTICS_H_
#define SPATIALGL_OPTICS_H_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace spatialgl::optics {

using Id = std::string;
using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;
using RGB = std::array<double, 3>;

enum class GeometryKind { kPoint, kPolyline, kPatch, kVolume, kRayPath };
enum class CoordinateSpace { kWorld, kSurfaceLocal };
enum class Composition { kOver, kReplace, kAdd };
enum class OutputMix { kExclusive, kNormalized, kAdditive };
enum class Traversal { kStable, kPriority, kShortestPath };
enum class Presentation { kImmediate, kTimed, kSynchronizedGroup };
enum class DeviceKind { kFixedRaster, kSteerableRaster, kGalvo, kUnsupported };
enum class PlanStatus { kAccepted, kUnsupported, kInfeasible, kNoPlanFound, kInvalidated };
enum class Provenance { kSimulated, kPredicted, kMeasured, kUnavailable };
enum class Availability { kAvailable, kUnavailable, kFailed, kHandingOff };
enum class EventKind { kBlank, kEmit, kDwell, kPose, kTrigger };

struct Plane {
  Id id;
  Vec3 origin{};
  Vec3 u{1, 0, 0};
  Vec3 v{0, 0, 1};
};
struct PlanarMotion {
  Vec3 linear_velocity{};
  Vec3 angular_velocity{};  // radians/second around the plane origin axes
};
struct Occluder {
  Id id;
  Vec3 min{};
  Vec3 max{};
};
struct WorldSnapshot {
  std::vector<Plane> planes;
  std::vector<Occluder> occluders;
  double source_time = 0;
  double valid_until = 0;
  std::uint64_t world_revision = 0;
  std::uint64_t calibration_revision = 0;
  std::vector<PlanarMotion> motion;  // aligned with planes
};
struct SurfaceTarget {
  Id surface_id;
  CoordinateSpace coordinates = CoordinateSpace::kWorld;
  Vec3 origin{};
  std::vector<Vec3> vertices;  // ordered for polyline; patch uses origin + u/v below
  Vec3 u{}, v{};
  GeometryKind geometry = GeometryKind::kPoint;
  double captured_at = 0;
  Id stable_path_id;
};
struct DrawCall {
  Id draw_id;
  SurfaceTarget target;
  RGB linear_rgb{1, 1, 1};
  double opacity = 1;
  double intensity = 1;
  int priority = 0;
  bool closed = false;
};
struct DisplayList {
  Id id;
  Id app_id;
  double present_at = 0;
  double expires_at = 0;
  std::uint64_t world_revision = 0;
  std::uint64_t calibration_revision = 0;
  Composition composition = Composition::kOver;
  std::vector<DrawCall> draws;
};
struct PipelineState {
  double sample_spacing = 0.05;
  // Zero means no requested bound. Positive certified bounds are unsupported.
  double max_position_error = 0;
  std::size_t max_samples = 100000;
  OutputMix output_mix = OutputMix::kNormalized;
  bool allow_laser_fill_approximation = false;
};
struct TraversalState {
  Traversal order = Traversal::kStable;
  double max_dark_gap = 0;
  double minimum_duty = 0;
};
struct PresentationState {
  Presentation mode = Presentation::kImmediate;
  double requested_group_skew = 0;
  bool require_sync = false;
};
struct DeviceProfile {
  Id id;
  DeviceKind kind = DeviceKind::kFixedRaster;
  Vec3 position{};
  Vec3 target{0, 0, -1};
  Vec3 up{0, 1, 0};
  double fov_y = 1.5707963267948966;
  double near_plane = 0.01;
  double far_plane = 100;
  int width = 640;
  int height = 480;
  double sample_rate_hz = 30000;
  double latency = 0;
  double clock_offset = 0;
  double clock_drift = 0;
  double clock_uncertainty = 0;
  double max_angular_speed = 0;
  double settling_time = 0;
  double max_sample_dwell = 0.01;
  std::vector<Id> resource_ids;
  RGB calibrated_gain{1, 1, 1};
  std::size_t sample_budget = 100000;
};
struct Resource {
  Id id;
  double capacity = 1;
};
struct Rig {
  std::vector<DeviceProfile> devices;
  std::vector<Resource> resources;
};
struct ProgramEvent {
  EventKind kind = EventKind::kBlank;
  double time = 0;
  Vec3 position{};
  RGB linear_rgb{};
  double dwell = 0;
  Id draw_id;
  Id display_list_id;
  Id surface_id;
};
struct TimedProgram {
  Id device_id;
  DeviceKind device_kind = DeviceKind::kFixedRaster;
  Id display_list_id;
  std::uint64_t generation = 0;
  std::uint64_t world_revision = 0;
  std::uint64_t calibration_revision = 0;
  double issued_at = 0;
  double starts_at = 0;
  double expires_at = 0;
  std::vector<ProgramEvent> events;
};
struct Diagnostic {
  PlanStatus status = PlanStatus::kAccepted;
  Id object_id;
  std::string code;
  std::string message;
};
struct Contribution {
  Id draw_id;
  Id display_list_id;
  Id surface_id;
  Id device_id;
  Vec3 position{};
  RGB linear_rgb{};
  double intensity = 0;
  double occlusion_fraction = 0;
  std::optional<double> duty_fraction;
  Provenance provenance = Provenance::kPredicted;
};
struct TargetLight {
  Id draw_id;
  Id display_list_id;
  RGB aggregate_linear_rgb{};
  std::optional<double> position_error;
  double age = 0;
  std::optional<double> duty;
  std::optional<double> max_dark_gap;
  Provenance provenance = Provenance::kUnavailable;
};
struct Receipt {
  Id device_id;
  std::uint64_t generation = 0;
  Availability availability = Availability::kAvailable;
  double accepted_at = 0;
  double timing_uncertainty = 0;
  Provenance provenance = Provenance::kSimulated;
};
struct Snapshot {
  double time = 0;
  std::uint64_t world_revision = 0;
  std::uint64_t calibration_revision = 0;
  std::vector<Contribution> contributions;
  std::vector<TargetLight> targets;
  std::vector<TimedProgram> programs;
  std::vector<Receipt> receipts;
  std::vector<Diagnostic> diagnostics;
};
struct RuntimeOptions {
  PipelineState pipeline;
  TraversalState traversal;
  PresentationState presentation;
  std::size_t planner_candidate_budget = 4096;
};

class Runtime {
 public:
  Runtime(WorldSnapshot world, Rig rig, RuntimeOptions options = {});
  ~Runtime();
  Runtime(Runtime&&) noexcept;
  Runtime& operator=(Runtime&&) noexcept;
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  std::vector<Diagnostic> submit(DisplayList display);
  Snapshot advance(double to_time);
  Snapshot snapshot() const;
  std::uint64_t cancel(const Id& display_list_id);
  void set_availability(const Id& device_id, Availability availability);
  void update_world(WorldSnapshot world);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Validation and deterministic geometry/sampling helpers are public for authoring adapters.
std::vector<Diagnostic> validate(const WorldSnapshot& world, const Rig& rig,
                                 const RuntimeOptions& options);
std::vector<Diagnostic> validate(const DisplayList& display);
}  // namespace spatialgl::optics

#endif  // SPATIALGL_OPTICS_H_
