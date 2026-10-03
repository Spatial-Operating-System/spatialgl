#include "spatialgl/optics.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace spatialgl::optics {
namespace {
bool finite(Vec3 p) {
  return std::all_of(p.begin(), p.end(), [](double x) { return std::isfinite(x); });
}
double dot(Vec3 a, Vec3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 sub(Vec3 a, Vec3 b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double norm(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 cross(Vec3 a, Vec3 b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
bool valid(GeometryKind v) {
  return v == GeometryKind::kPoint || v == GeometryKind::kPolyline || v == GeometryKind::kPatch ||
         v == GeometryKind::kVolume || v == GeometryKind::kRayPath;
}
bool valid(CoordinateSpace v) {
  return v == CoordinateSpace::kWorld || v == CoordinateSpace::kSurfaceLocal;
}
bool valid(DeviceKind v) {
  return v == DeviceKind::kFixedRaster || v == DeviceKind::kSteerableRaster ||
         v == DeviceKind::kGalvo || v == DeviceKind::kUnsupported;
}
bool valid(OutputMix v) {
  return v == OutputMix::kExclusive || v == OutputMix::kNormalized || v == OutputMix::kAdditive;
}
bool valid(Traversal v) {
  return v == Traversal::kStable || v == Traversal::kPriority || v == Traversal::kShortestPath;
}
bool valid(Presentation v) {
  return v == Presentation::kImmediate || v == Presentation::kTimed ||
         v == Presentation::kSynchronizedGroup;
}
bool valid(Composition v) {
  return v == Composition::kOver || v == Composition::kReplace || v == Composition::kAdd;
}
}  // namespace

std::vector<Diagnostic> validate(const WorldSnapshot& w, const Rig& r, const RuntimeOptions& o) {
  std::vector<Diagnostic> d;
  if (!std::isfinite(w.source_time) || !std::isfinite(w.valid_until) ||
      w.valid_until < w.source_time)
    d.push_back({PlanStatus::kInfeasible, "world", "invalid-world-interval",
                 "World validity interval is invalid"});
  if (!w.motion.empty() && w.motion.size() != w.planes.size())
    d.push_back(
        {PlanStatus::kInfeasible, "world", "motion-count", "Motion must align with planes"});
  for (const auto& m : w.motion)
    if (!finite(m.linear_velocity) || !finite(m.angular_velocity))
      d.push_back(
          {PlanStatus::kInfeasible, "world", "invalid-motion", "Planar motion must be finite"});
  for (const auto& b : w.occluders)
    if (b.id.empty() || !finite(b.min) || !finite(b.max) || b.min[0] > b.max[0] ||
        b.min[1] > b.max[1] || b.min[2] > b.max[2])
      d.push_back(
          {PlanStatus::kInfeasible, b.id, "invalid-occluder", "Occluder bounds are invalid"});
  std::set<Id> ids, resources;
  for (const auto& p : w.planes)
    if (p.id.empty() || !ids.insert(p.id).second || !finite(p.origin) || !finite(p.u) ||
        !finite(p.v) || norm(cross(p.u, p.v)) < 1e-9 ||
        std::abs(dot(p.u, p.v)) > 1e-6 * norm(p.u) * norm(p.v))
      d.push_back({PlanStatus::kInfeasible, p.id, "invalid-plane",
                   "Plane must be finite, nondegenerate, rectangular and uniquely named"});
  ids.clear();
  for (const auto& q : r.resources)
    if (q.id.empty() || !resources.insert(q.id).second || !std::isfinite(q.capacity) ||
        q.capacity <= 0)
      d.push_back(
          {PlanStatus::kInfeasible, q.id, "invalid-resource", "Resource is invalid or duplicated"});
    else if (std::abs(q.capacity - 1.0) > 1e-9)
      d.push_back({PlanStatus::kUnsupported, q.id, "unsupported-resource-capacity",
                   "Only exclusive unit-capacity resources are modeled"});
  for (const auto& x : r.devices) {
    if (x.id.empty() || !ids.insert(x.id).second || x.width < 2 || x.height < 2 ||
        !std::isfinite(x.sample_rate_hz) || x.sample_rate_hz <= 0 || !std::isfinite(x.latency) ||
        x.latency < 0 || !std::isfinite(x.clock_offset) || !std::isfinite(x.clock_drift) ||
        !std::isfinite(x.clock_uncertainty) || x.clock_uncertainty < 0 || x.clock_drift <= -1 ||
        !std::isfinite(x.max_sample_dwell) || x.max_sample_dwell <= 0 || x.sample_budget == 0 ||
        x.sample_budget > 1000000 || !finite(x.position) || !finite(x.target) || !finite(x.up) ||
        !std::isfinite(x.fov_y) || x.fov_y <= 0 || x.fov_y >= std::acos(-1.0) ||
        !std::isfinite(x.near_plane) || !std::isfinite(x.far_plane) || x.near_plane <= 0 ||
        x.far_plane <= x.near_plane || norm(cross(sub(x.target, x.position), x.up)) < 1e-9 ||
        !finite(x.calibrated_gain) ||
        std::any_of(x.calibrated_gain.begin(), x.calibrated_gain.end(),
                    [](double g) { return g < 0; }))
      d.push_back(
          {PlanStatus::kInfeasible, x.id, "invalid-device", "Device profile or clock is invalid"});
    if (!valid(x.kind))
      d.push_back({PlanStatus::kInfeasible, x.id, "invalid-device-kind",
                   "Device kind has an unknown enum value"});
    else if (x.kind == DeviceKind::kUnsupported)
      d.push_back(
          {PlanStatus::kUnsupported, x.id, "unsupported-device", "Device model is unsupported"});
    if (x.kind == DeviceKind::kSteerableRaster &&
        (!std::isfinite(x.max_angular_speed) || x.max_angular_speed <= 0 ||
         !std::isfinite(x.settling_time) || x.settling_time < 0))
      d.push_back(
          {PlanStatus::kInfeasible, x.id, "invalid-steering",
           "Steerable profiles require positive angular speed and nonnegative settling time"});
    for (const auto& id : x.resource_ids)
      if (!resources.contains(id))
        d.push_back({PlanStatus::kInfeasible, x.id, "unknown-resource",
                     "Device names an undeclared resource"});
  }
  if (!valid(o.pipeline.output_mix) || !valid(o.traversal.order) || !valid(o.presentation.mode) ||
      o.pipeline.sample_spacing <= 0 || !std::isfinite(o.pipeline.sample_spacing) ||
      o.pipeline.max_samples == 0 || o.pipeline.max_samples > 1000000 ||
      !std::isfinite(o.pipeline.max_position_error) || o.pipeline.max_position_error < 0 ||
      !std::isfinite(o.traversal.max_dark_gap) || o.traversal.max_dark_gap < 0 ||
      !std::isfinite(o.traversal.minimum_duty) || o.traversal.minimum_duty < 0 ||
      o.traversal.minimum_duty > 1 || o.planner_candidate_budget == 0 ||
      !std::isfinite(o.presentation.requested_group_skew) ||
      o.presentation.requested_group_skew < 0)
    d.push_back({PlanStatus::kInfeasible, "pipeline", "invalid-sampling",
                 "Sampling parameters are invalid"});
  if (std::isfinite(o.pipeline.max_position_error) && o.pipeline.max_position_error > 0)
    d.push_back({PlanStatus::kUnsupported, "pipeline", "unsupported-position-error-bound",
                 "The simulator cannot certify a positive target position-error bound"});
  return d;
}
std::vector<Diagnostic> validate(const DisplayList& f) {
  std::vector<Diagnostic> d;
  if (f.id.empty() || !std::isfinite(f.present_at) || !std::isfinite(f.expires_at) ||
      f.present_at < 0 || f.expires_at <= f.present_at || !valid(f.composition))
    d.push_back(
        {PlanStatus::kInfeasible, f.id, "invalid-display-interval", "Display interval is invalid"});
  std::set<Id> ids;
  for (const auto& c : f.draws) {
    if (c.draw_id.empty() || !ids.insert(c.draw_id).second || c.target.surface_id.empty() ||
        !finite(c.target.origin) || !std::isfinite(c.target.captured_at) ||
        !std::isfinite(c.intensity) || c.intensity < 0 || !valid(c.target.geometry) ||
        !valid(c.target.coordinates))
      d.push_back({PlanStatus::kInfeasible, c.draw_id, "invalid-draw",
                   "Draw id, target or intensity is invalid"});
    if (c.target.geometry == GeometryKind::kVolume || c.target.geometry == GeometryKind::kRayPath)
      d.push_back({PlanStatus::kUnsupported, c.draw_id, "unsupported-geometry",
                   "Volume and free-space ray targets are not supported"});
    if (!std::isfinite(c.opacity) || c.opacity < 0 || c.opacity > 1)
      d.push_back(
          {PlanStatus::kInfeasible, c.draw_id, "invalid-opacity", "Opacity must be in [0,1]"});
    if (c.target.geometry == GeometryKind::kPolyline &&
        (c.target.vertices.size() < 2 ||
         std::any_of(c.target.vertices.begin(), c.target.vertices.end(),
                     [](Vec3 p) { return !finite(p); })))
      d.push_back({PlanStatus::kInfeasible, c.draw_id, "invalid-polyline",
                   "Polyline requires at least two finite vertices"});
    if (c.target.geometry == GeometryKind::kPatch &&
        (!finite(c.target.u) || !finite(c.target.v) || norm(c.target.u) < 1e-9 ||
         norm(c.target.v) < 1e-9 || std::abs(dot(c.target.u, c.target.v)) > 1e-6))
      d.push_back({PlanStatus::kInfeasible, c.draw_id, "invalid-patch",
                   "Patch axes must be finite, nonzero, and perpendicular"});
    for (double x : c.linear_rgb)
      if (!std::isfinite(x) || x < 0)
        d.push_back({PlanStatus::kInfeasible, c.draw_id, "invalid-color",
                     "Color must be finite nonnegative linear light"});
  }
  return d;
}

}  // namespace spatialgl::optics
