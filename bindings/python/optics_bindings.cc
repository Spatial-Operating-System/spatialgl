#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "spatialgl/optics.h"

namespace py = pybind11;
namespace so = spatialgl::optics;

PYBIND11_MODULE(_optics_native, m) {
  m.doc() = "SpatialGL surface optics authoring and simulation core";

#define ENUM(T, ...) py::enum_<so::T>(m, #T).value(__VA_ARGS__)
  ENUM(GeometryKind, "POINT", so::GeometryKind::kPoint)
      .value("POLYLINE", so::GeometryKind::kPolyline)
      .value("PATCH", so::GeometryKind::kPatch)
      .value("VOLUME", so::GeometryKind::kVolume)
      .value("RAY_PATH", so::GeometryKind::kRayPath);
  ENUM(CoordinateSpace, "WORLD", so::CoordinateSpace::kWorld)
      .value("SURFACE_LOCAL", so::CoordinateSpace::kSurfaceLocal);
  ENUM(Composition, "OVER", so::Composition::kOver)
      .value("REPLACE", so::Composition::kReplace)
      .value("ADD", so::Composition::kAdd);
  ENUM(OutputMix, "EXCLUSIVE", so::OutputMix::kExclusive)
      .value("NORMALIZED", so::OutputMix::kNormalized)
      .value("ADDITIVE", so::OutputMix::kAdditive);
  ENUM(Traversal, "STABLE", so::Traversal::kStable)
      .value("PRIORITY", so::Traversal::kPriority)
      .value("SHORTEST_PATH", so::Traversal::kShortestPath);
  ENUM(Presentation, "IMMEDIATE", so::Presentation::kImmediate)
      .value("TIMED", so::Presentation::kTimed)
      .value("SYNCHRONIZED_GROUP", so::Presentation::kSynchronizedGroup);
  ENUM(DeviceKind, "FIXED_RASTER", so::DeviceKind::kFixedRaster)
      .value("STEERABLE_RASTER", so::DeviceKind::kSteerableRaster)
      .value("GALVO", so::DeviceKind::kGalvo)
      .value("UNSUPPORTED", so::DeviceKind::kUnsupported);
  ENUM(PlanStatus, "ACCEPTED", so::PlanStatus::kAccepted)
      .value("UNSUPPORTED", so::PlanStatus::kUnsupported)
      .value("INFEASIBLE", so::PlanStatus::kInfeasible)
      .value("NO_PLAN_FOUND", so::PlanStatus::kNoPlanFound)
      .value("INVALIDATED", so::PlanStatus::kInvalidated);
  ENUM(Provenance, "SIMULATED", so::Provenance::kSimulated)
      .value("PREDICTED", so::Provenance::kPredicted)
      .value("MEASURED", so::Provenance::kMeasured)
      .value("UNAVAILABLE", so::Provenance::kUnavailable);
  ENUM(Availability, "AVAILABLE", so::Availability::kAvailable)
      .value("UNAVAILABLE", so::Availability::kUnavailable)
      .value("FAILED", so::Availability::kFailed)
      .value("HANDING_OFF", so::Availability::kHandingOff);
  ENUM(EventKind, "BLANK", so::EventKind::kBlank)
      .value("EMIT", so::EventKind::kEmit)
      .value("DWELL", so::EventKind::kDwell)
      .value("POSE", so::EventKind::kPose)
      .value("TRIGGER", so::EventKind::kTrigger);
#undef ENUM

#define FIELD(T, f) .def_readwrite(#f, &so::T::f)
#define OBS_FIELD(T, f) .def_readonly(#f, &so::T::f)
  py::class_<so::Plane>(m, "Plane")
      .def(py::init<>()) FIELD(Plane, id) FIELD(Plane, origin) FIELD(Plane, u) FIELD(Plane, v);
  py::class_<so::PlanarMotion>(m, "PlanarMotion")
      .def(py::init<>()) FIELD(PlanarMotion, linear_velocity) FIELD(PlanarMotion, angular_velocity);
  py::class_<so::Occluder>(m, "Occluder")
      .def(py::init<>()) FIELD(Occluder, id) FIELD(Occluder, min) FIELD(Occluder, max);
  py::class_<so::WorldSnapshot>(m, "WorldSnapshot")
      .def(py::init<>()) FIELD(WorldSnapshot, planes) FIELD(WorldSnapshot, occluders)
          FIELD(WorldSnapshot, source_time) FIELD(WorldSnapshot, valid_until)
              FIELD(WorldSnapshot, world_revision) FIELD(WorldSnapshot, calibration_revision)
                  FIELD(WorldSnapshot, motion);
  py::class_<so::SurfaceTarget>(m, "SurfaceTarget")
      .def(py::init<>()) FIELD(SurfaceTarget, surface_id) FIELD(SurfaceTarget, coordinates)
          FIELD(SurfaceTarget, origin) FIELD(SurfaceTarget, vertices) FIELD(SurfaceTarget, u)
              FIELD(SurfaceTarget, v) FIELD(SurfaceTarget, geometry)
                  FIELD(SurfaceTarget, captured_at) FIELD(SurfaceTarget, stable_path_id);
  py::class_<so::DrawCall>(m, "DrawCall")
      .def(py::init<>()) FIELD(DrawCall, draw_id) FIELD(DrawCall, target)
          FIELD(DrawCall, linear_rgb) FIELD(DrawCall, intensity) FIELD(DrawCall, priority)
              FIELD(DrawCall, closed) FIELD(DrawCall, opacity);
  py::class_<so::DisplayList>(m, "DisplayList")
      .def(py::init<>()) FIELD(DisplayList, id) FIELD(DisplayList, app_id)
          FIELD(DisplayList, present_at) FIELD(DisplayList, expires_at)
              FIELD(DisplayList, world_revision) FIELD(DisplayList, calibration_revision)
                  FIELD(DisplayList, composition) FIELD(DisplayList, draws);
  py::class_<so::PipelineState>(m, "PipelineState")
      .def(py::init<>()) FIELD(PipelineState, sample_spacing)
          FIELD(PipelineState, max_position_error) FIELD(PipelineState, max_samples)
              FIELD(PipelineState, output_mix) FIELD(PipelineState, allow_laser_fill_approximation);
  py::class_<so::TraversalState>(m, "TraversalState")
      .def(py::init<>()) FIELD(TraversalState, order) FIELD(TraversalState, max_dark_gap)
          FIELD(TraversalState, minimum_duty);
  py::class_<so::PresentationState>(m, "PresentationState")
      .def(py::init<>()) FIELD(PresentationState, mode)
          FIELD(PresentationState, requested_group_skew) FIELD(PresentationState, require_sync);
  py::class_<so::DeviceProfile>(m, "DeviceProfile")
      .def(py::init<>()) FIELD(DeviceProfile, id) FIELD(DeviceProfile, kind)
          FIELD(DeviceProfile, position) FIELD(DeviceProfile, target) FIELD(DeviceProfile, up)
              FIELD(DeviceProfile, fov_y) FIELD(DeviceProfile, near_plane) FIELD(
                  DeviceProfile, far_plane) FIELD(DeviceProfile, width) FIELD(DeviceProfile, height)
                  FIELD(DeviceProfile, sample_rate_hz) FIELD(DeviceProfile, latency)
                      FIELD(DeviceProfile, clock_offset) FIELD(DeviceProfile, clock_drift) FIELD(
                          DeviceProfile, clock_uncertainty) FIELD(DeviceProfile, max_angular_speed)
                          FIELD(DeviceProfile, settling_time) FIELD(DeviceProfile, max_sample_dwell)
                              FIELD(DeviceProfile, resource_ids)
                                  FIELD(DeviceProfile, calibrated_gain)
                                      FIELD(DeviceProfile, sample_budget);
  py::class_<so::Resource>(m, "Resource")
      .def(py::init<>()) FIELD(Resource, id) FIELD(Resource, capacity);
  py::class_<so::Rig>(m, "Rig").def(py::init<>()) FIELD(Rig, devices) FIELD(Rig, resources);
  py::class_<so::ProgramEvent>(m, "ProgramEvent") OBS_FIELD(ProgramEvent, kind)
      OBS_FIELD(ProgramEvent, time) OBS_FIELD(ProgramEvent, position)
          OBS_FIELD(ProgramEvent, linear_rgb) OBS_FIELD(ProgramEvent, dwell)
              OBS_FIELD(ProgramEvent, draw_id) OBS_FIELD(ProgramEvent, display_list_id)
                  OBS_FIELD(ProgramEvent, surface_id);
  py::class_<so::TimedProgram>(m, "TimedProgram") OBS_FIELD(TimedProgram, device_id)
      OBS_FIELD(TimedProgram, device_kind) OBS_FIELD(TimedProgram, display_list_id)
          OBS_FIELD(TimedProgram, generation) OBS_FIELD(TimedProgram, world_revision)
              OBS_FIELD(TimedProgram, calibration_revision) OBS_FIELD(TimedProgram, issued_at)
                  OBS_FIELD(TimedProgram, starts_at) OBS_FIELD(TimedProgram, expires_at)
                      OBS_FIELD(TimedProgram, events);
  py::class_<so::Diagnostic>(m, "Diagnostic") OBS_FIELD(Diagnostic, status)
      OBS_FIELD(Diagnostic, object_id) OBS_FIELD(Diagnostic, code) OBS_FIELD(Diagnostic, message);
  py::class_<so::Contribution>(m, "Contribution") OBS_FIELD(Contribution, draw_id)
      OBS_FIELD(Contribution, display_list_id) OBS_FIELD(Contribution, surface_id)
          OBS_FIELD(Contribution, device_id) OBS_FIELD(Contribution, position)
              OBS_FIELD(Contribution, linear_rgb) OBS_FIELD(Contribution, intensity)
                  OBS_FIELD(Contribution, occlusion_fraction) OBS_FIELD(Contribution, duty_fraction)
                      OBS_FIELD(Contribution, provenance);
  py::class_<so::TargetLight>(m, "TargetLight") OBS_FIELD(TargetLight, draw_id)
      OBS_FIELD(TargetLight, display_list_id) OBS_FIELD(TargetLight, aggregate_linear_rgb)
          OBS_FIELD(TargetLight, position_error) OBS_FIELD(TargetLight, age)
              OBS_FIELD(TargetLight, duty) OBS_FIELD(TargetLight, max_dark_gap)
                  OBS_FIELD(TargetLight, provenance);
  py::class_<so::Receipt>(m, "Receipt") OBS_FIELD(Receipt, device_id) OBS_FIELD(Receipt, generation)
      OBS_FIELD(Receipt, availability) OBS_FIELD(Receipt, accepted_at)
          OBS_FIELD(Receipt, timing_uncertainty) OBS_FIELD(Receipt, provenance);
  py::class_<so::Snapshot>(m, "Snapshot") OBS_FIELD(Snapshot, time)
      OBS_FIELD(Snapshot, world_revision) OBS_FIELD(Snapshot, calibration_revision)
          OBS_FIELD(Snapshot, contributions) OBS_FIELD(Snapshot, targets)
              OBS_FIELD(Snapshot, programs) OBS_FIELD(Snapshot, receipts)
                  OBS_FIELD(Snapshot, diagnostics);
  py::class_<so::RuntimeOptions>(m, "RuntimeOptions")
      .def(py::init<>()) FIELD(RuntimeOptions, pipeline) FIELD(RuntimeOptions, traversal)
          FIELD(RuntimeOptions, presentation) FIELD(RuntimeOptions, planner_candidate_budget);
#undef FIELD
#undef OBS_FIELD
  py::class_<so::Runtime>(m, "OpticalRuntime")
      .def(py::init<so::WorldSnapshot, so::Rig, so::RuntimeOptions>(), py::arg("world"),
           py::arg("rig"), py::arg("options") = so::RuntimeOptions{})
      .def("submit", &so::Runtime::submit, py::arg("display"))
      .def("advance", &so::Runtime::advance, py::arg("to_time"),
           py::call_guard<py::gil_scoped_release>())
      .def("snapshot", &so::Runtime::snapshot)
      .def("cancel", &so::Runtime::cancel, py::arg("display_list_id"))
      .def("set_availability", &so::Runtime::set_availability, py::arg("device_id"),
           py::arg("availability"))
      .def("update_world", &so::Runtime::update_world, py::arg("world"));
  m.def(
      "validate_world",
      [](const so::WorldSnapshot& world, const so::Rig& rig, const so::RuntimeOptions& options) {
        return so::validate(world, rig, options);
      },
      py::arg("world"), py::arg("rig"), py::arg("options") = so::RuntimeOptions{});
  m.def(
      "validate_display", [](const so::DisplayList& display) { return so::validate(display); },
      py::arg("display"));
}
