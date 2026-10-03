#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "spatialgl/glfw_output.h"
#include "spatialgl/raster.h"

namespace py = pybind11;
namespace sr = spatialgl::raster;

PYBIND11_MODULE(_raster_native, m) {
  m.doc() = "SpatialGL native raster compilation and display output";
  // Import first so Snapshot, DeviceProfile and Diagnostic use the same
  // pybind11 registrations as the optics extension.
  py::module_::import("bindings.python.spatialgl._optics_native");

  py::enum_<sr::TransferFunction>(m, "TransferFunction")
      .value("LINEAR", sr::TransferFunction::kLinear)
      .value("SRGB", sr::TransferFunction::kSrgb);
  py::class_<sr::Calibration>(m, "Calibration")
      .def(py::init<>())
      .def_readwrite("homography", &sr::Calibration::homography)
      .def_readwrite("point_radius", &sr::Calibration::point_radius)
      .def_readwrite("transfer", &sr::Calibration::transfer)
      .def_readwrite("lease_seconds", &sr::Calibration::lease_seconds);
  py::class_<sr::RasterFrame>(m, "RasterFrame")
      .def_readonly("device_id", &sr::RasterFrame::device_id)
      .def_readonly("time", &sr::RasterFrame::time)
      .def_readonly("expires_at", &sr::RasterFrame::expires_at)
      .def_readonly("world_revision", &sr::RasterFrame::world_revision)
      .def_readonly("calibration_revision", &sr::RasterFrame::calibration_revision)
      .def_readonly("width", &sr::RasterFrame::width)
      .def_readonly("height", &sr::RasterFrame::height)
      .def_property_readonly("rgb",
                             [](const sr::RasterFrame& frame) {
                               return py::bytes(reinterpret_cast<const char*>(frame.rgb.data()),
                                                frame.rgb.size());
                             })
      .def_readonly("diagnostics", &sr::RasterFrame::diagnostics);
  m.def("fit_homography", &sr::fit_homography, py::arg("source"), py::arg("destination"));
  m.def("compile", &sr::compile, py::arg("snapshot"), py::arg("device"),
        py::arg("calibration") = sr::Calibration{});
  m.def("write_ppm", &sr::write_ppm, py::arg("frame"), py::arg("path"));
  py::class_<sr::DisplayInfo>(m, "DisplayInfo")
      .def_readonly("index", &sr::DisplayInfo::index)
      .def_readonly("name", &sr::DisplayInfo::name)
      .def_readonly("width", &sr::DisplayInfo::width)
      .def_readonly("height", &sr::DisplayInfo::height);
  py::class_<sr::GlfwOutput>(m, "GlfwOutput")
      .def(py::init<int, int, std::string, int, std::string>(), py::arg("width"), py::arg("height"),
           py::arg("title") = "SpatialGL", py::arg("display_index") = -1,
           py::arg("library_path") = "")
      .def_static("displays", &sr::GlfwOutput::displays, py::arg("library_path") = "")
      .def("present", &sr::GlfwOutput::present, py::arg("frame"), py::arg("now"))
      .def("poll", &sr::GlfwOutput::poll, py::arg("now"))
      .def("blackout", &sr::GlfwOutput::blackout)
      .def("close", &sr::GlfwOutput::close);
}
