#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "spatialgl/spatialgl.h"

namespace py = pybind11;
using namespace spatialgl;
namespace {
const char* kind_name(Kind k) {
  return k == Kind::Point ? "point" : k == Kind::Polyline ? "polyline" : "patch";
}
py::dict sample_dict(const Sample& s) {
  py::dict d;
  d["id"] = s.id;
  d["primitive_id"] = s.primitive_id;
  d["kind"] = kind_name(s.kind);
  d["position"] = s.position;
  d["color"] = s.color;
  d["surface_id"] = s.surface_id.empty() ? py::none() : py::cast(s.surface_id);
  return d;
}
py::dict frame_dict(const DeviceFrame& f) {
  py::dict d, c;
  d["device_id"] = f.device_id;
  d["scene_id"] = f.scene_id.empty() ? py::none() : py::cast(f.scene_id);
  d["issued_at"] = f.issued_at;
  d["apply_at"] = f.apply_at;
  d["expires_at"] = f.expires_at;
  std::visit(
      [&](const auto& cmd) {
        using T = std::decay_t<decltype(cmd)>;
        if constexpr (std::is_same_v<T, ExtensionCommand>) {
          c["kind"] = "extension";
          c["schema"] = cmd.schema;
          c["payload"] = cmd.payload;
        } else {
          py::list entries;
          const auto& candidates = [&]() -> const std::vector<Candidate>& {
            if constexpr (std::is_same_v<T, RasterCommand>)
              return cmd.samples;
            else
              return cmd.targets;
          }();
          for (const auto& candidate : candidates) {
            py::dict entry;
            entry["sample_id"] = candidate.sample.id;
            entry["position"] = candidate.position;
            entry["color"] = candidate.sample.color;
            if (candidate.pixel) entry["pixel"] = *candidate.pixel;
            entries.append(entry);
          }
          if constexpr (std::is_same_v<T, RasterCommand>) {
            c["kind"] = "raster-samples";
            c["width"] = cmd.width;
            c["height"] = cmd.height;
            c["samples"] = entries;
          } else {
            c["kind"] = "emitter-targets";
            c["targets"] = entries;
          }
        }
      },
      f.command);
  d["command"] = c;
  return d;
}
py::dict state_dict(const RealizationState& state) {
  py::dict d;
  d["time"] = state.time;
  d["scene_id"] = state.scene_id;
  py::list samples, outputs, frames;
  for (const auto& s : state.samples) {
    py::dict entry;
    entry["sample"] = sample_dict(s.sample);
    entry["device_id"] = s.device_id;
    entry["actual"] = s.actual;
    entry["error"] = s.error;
    entry["status"] = s.status;
    entry["source_scene_id"] = s.source_scene_id;
    entry["age"] = s.age;
    entry["reasons"] = s.reasons;
    samples.append(entry);
  }
  for (const auto& o : state.outputs) {
    py::dict entry;
    entry["sample_id"] = o.sample_id;
    entry["position"] = o.position;
    entry["color"] = o.color;
    entry["device_id"] = o.device_id;
    entry["source_scene_id"] = o.source_scene_id;
    entry["age"] = o.age;
    entry["stale"] = o.stale;
    outputs.append(entry);
  }
  for (const auto& f : state.device_frames) frames.append(frame_dict(f));
  d["samples"] = samples;
  d["outputs"] = outputs;
  d["device_frames"] = frames;
  return d;
}
}  // namespace
#define READONLY(T, field) def_readonly(#field, &T::field)
#define READWRITE(T, field) def_readwrite(#field, &T::field)
PYBIND11_MODULE(_native, m) {
  m.doc() = "SpatialGL C++20 simulation core";
  m.attr("__version__") = "0.2.0";
  py::class_<Point>(m, "Point")
      .def(py::init([](std::string id, Vec3 position, RGB color, std::string surface_id) {
             return Point{std::move(id), position, color, std::move(surface_id)};
           }),
           py::arg("id"), py::arg("position"), py::arg("color") = RGB{1, 1, 1},
           py::arg("surface_id") = "")
      .READWRITE(Point, id)
      .READWRITE(Point, position)
      .READWRITE(Point, color)
      .READWRITE(Point, surface_id);
  py::class_<Polyline>(m, "Polyline")
      .def(py::init(
               [](std::string id, std::vector<Vec3> vertices, RGB color, std::string surface_id) {
                 return Polyline{std::move(id), std::move(vertices), color, std::move(surface_id)};
               }),
           py::arg("id"), py::arg("vertices"), py::arg("color") = RGB{1, 1, 1},
           py::arg("surface_id") = "")
      .READWRITE(Polyline, id)
      .READWRITE(Polyline, vertices)
      .READWRITE(Polyline, color)
      .READWRITE(Polyline, surface_id);
  py::class_<Patch>(m, "Patch")
      .def(py::init(
               [](std::string id, Vec3 origin, Vec3 u, Vec3 v, RGB color, std::string surface_id) {
                 return Patch{std::move(id), origin, u, v, color, std::move(surface_id)};
               }),
           py::arg("id"), py::arg("origin"), py::arg("u"), py::arg("v"),
           py::arg("color") = RGB{1, 1, 1}, py::arg("surface_id") = "")
      .READWRITE(Patch, id)
      .READWRITE(Patch, origin)
      .READWRITE(Patch, u)
      .READWRITE(Patch, v)
      .READWRITE(Patch, color)
      .READWRITE(Patch, surface_id);
  py::class_<Surface>(m, "Surface")
      .def(py::init([](std::string id, Vec3 origin, Vec3 u, Vec3 v) {
             return Surface{std::move(id), origin, u, v};
           }),
           py::arg("id"), py::arg("origin"), py::arg("u"), py::arg("v"))
      .READWRITE(Surface, id)
      .READWRITE(Surface, origin)
      .READWRITE(Surface, u)
      .READWRITE(Surface, v);
  py::class_<Occluder>(m, "Occluder")
      .def(py::init([](std::string id, Vec3 min, Vec3 max) {
             return Occluder{std::move(id), min, max};
           }),
           py::arg("id"), py::arg("min"), py::arg("max"))
      .READWRITE(Occluder, id)
      .READWRITE(Occluder, min)
      .READWRITE(Occluder, max);
  py::class_<World>(m, "World")
      .def(py::init([](std::vector<Surface> surfaces, std::vector<Occluder> occluders) {
             return World{std::move(surfaces), std::move(occluders)};
           }),
           py::arg("surfaces") = std::vector<Surface>{},
           py::arg("occluders") = std::vector<Occluder>{})
      .READWRITE(World, surfaces)
      .READWRITE(World, occluders);
  py::class_<SceneFrame>(m, "SceneFrame")
      .def(py::init([](std::string id, double present_at, double expires_at,
                       std::vector<Primitive> primitives) {
             return SceneFrame{std::move(id), present_at, expires_at, std::move(primitives)};
           }),
           py::arg("id"), py::arg("present_at"), py::arg("expires_at"), py::arg("primitives"))
      .READWRITE(SceneFrame, id)
      .READWRITE(SceneFrame, present_at)
      .READWRITE(SceneFrame, expires_at)
      .READWRITE(SceneFrame, primitives);
  py::class_<MonitorConfig>(m, "MonitorConfig")
      .def(py::init<>())
      .READWRITE(MonitorConfig, id)
      .READWRITE(MonitorConfig, surface_id)
      .READWRITE(MonitorConfig, width)
      .READWRITE(MonitorConfig, height)
      .READWRITE(MonitorConfig, refresh_hz)
      .READWRITE(MonitorConfig, latency);
  py::class_<ProjectorConfig>(m, "ProjectorConfig")
      .def(py::init<>())
      .READWRITE(ProjectorConfig, id)
      .READWRITE(ProjectorConfig, position)
      .READWRITE(ProjectorConfig, target)
      .READWRITE(ProjectorConfig, up)
      .READWRITE(ProjectorConfig, fov_y)
      .READWRITE(ProjectorConfig, near)
      .READWRITE(ProjectorConfig, far)
      .READWRITE(ProjectorConfig, width)
      .READWRITE(ProjectorConfig, height)
      .READWRITE(ProjectorConfig, refresh_hz)
      .READWRITE(ProjectorConfig, latency);
  py::class_<DroneConfig>(m, "DroneConfig")
      .def(py::init<>())
      .READWRITE(DroneConfig, id)
      .READWRITE(DroneConfig, count)
      .READWRITE(DroneConfig, speed)
      .READWRITE(DroneConfig, home)
      .READWRITE(DroneConfig, min)
      .READWRITE(DroneConfig, max)
      .READWRITE(DroneConfig, refresh_hz)
      .READWRITE(DroneConfig, latency);
  py::class_<DeviceDescriptor>(m, "DeviceDescriptor")
      .READONLY(DeviceDescriptor, id)
      .READONLY(DeviceDescriptor, modality)
      .READONLY(DeviceDescriptor, refresh_hz)
      .READONLY(DeviceDescriptor, latency)
      .READONLY(DeviceDescriptor, capacity);
  py::class_<Backend, std::shared_ptr<Backend>>(m, "Backend")
      .def_property_readonly("descriptor", &Backend::descriptor);
  py::class_<MonitorBackend, Backend, std::shared_ptr<MonitorBackend>>(m, "MonitorBackend")
      .def(py::init<MonitorConfig>());
  py::class_<ProjectorBackend, Backend, std::shared_ptr<ProjectorBackend>>(m, "ProjectorBackend")
      .def(py::init<ProjectorConfig>());
  py::class_<DroneBackend, Backend, std::shared_ptr<DroneBackend>>(m, "DroneBackend")
      .def(py::init<DroneConfig>());
  py::class_<Sample>(m, "Sample")
      .READONLY(Sample, id)
      .READONLY(Sample, primitive_id)
      .READONLY(Sample, position)
      .READONLY(Sample, color)
      .READONLY(Sample, surface_id)
      .def_property_readonly("kind", [](const Sample& s) { return kind_name(s.kind); });
  py::class_<RealizedSample>(m, "RealizedSample")
      .READONLY(RealizedSample, sample)
      .READONLY(RealizedSample, device_id)
      .READONLY(RealizedSample, actual)
      .READONLY(RealizedSample, error)
      .READONLY(RealizedSample, status)
      .READONLY(RealizedSample, source_scene_id)
      .READONLY(RealizedSample, age)
      .READONLY(RealizedSample, reasons);
  py::class_<Observation>(m, "Observation")
      .READONLY(Observation, sample_id)
      .READONLY(Observation, position)
      .READONLY(Observation, color);
  py::class_<ObservedOutput, Observation>(m, "ObservedOutput")
      .READONLY(ObservedOutput, device_id)
      .READONLY(ObservedOutput, source_scene_id)
      .READONLY(ObservedOutput, age)
      .READONLY(ObservedOutput, stale);
  py::class_<DeviceFrame>(m, "DeviceFrame")
      .READONLY(DeviceFrame, device_id)
      .READONLY(DeviceFrame, scene_id)
      .READONLY(DeviceFrame, issued_at)
      .READONLY(DeviceFrame, apply_at)
      .READONLY(DeviceFrame, expires_at)
      .def("to_dict", frame_dict);
  py::class_<RealizationState>(m, "RealizationState")
      .READONLY(RealizationState, time)
      .READONLY(RealizationState, scene_id)
      .READONLY(RealizationState, samples)
      .READONLY(RealizationState, device_frames)
      .READONLY(RealizationState, outputs)
      .def("to_dict", state_dict);
  py::class_<Runtime>(m, "Runtime")
      .def(py::init<World, const std::vector<std::shared_ptr<Backend>>&, double, double>(),
           py::arg("world"), py::arg("backends"), py::arg("spacing") = 0.15,
           py::arg("tolerance") = 0.02)
      .def("submit", &Runtime::submit, py::arg("frame"))
      .def("advance", &Runtime::advance, py::arg("to"), py::call_guard<py::gil_scoped_release>())
      .def("snapshot", &Runtime::snapshot, py::call_guard<py::gil_scoped_release>());
  m.def("sample_scene", sample_scene, py::arg("primitives"), py::arg("spacing") = 0.15);
}
