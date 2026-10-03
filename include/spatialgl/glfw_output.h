#ifndef SPATIALGL_GLFW_OUTPUT_H_
#define SPATIALGL_GLFW_OUTPUT_H_

#include <memory>
#include <string>
#include <vector>

#include "spatialgl/raster.h"

namespace spatialgl::raster {
struct DisplayInfo {
  int index = 0;
  std::string name;
  int width = 0, height = 0;
};

// Optional runtime GLFW/OpenGL presentation using an OpenGL 2.1 compatibility
// context and RGB pixel upload. On macOS, initialize and use this API on the
// process main thread; elsewhere, keep all calls on the construction thread.
// now values must be finite and monotonic, using the
// same clock as RasterFrame timestamps. Window ownership does not guarantee a
// hardware cutoff if the host stops polling.
class GlfwOutput {
 public:
  GlfwOutput(int width, int height, const std::string& title = "SpatialGL", int display_index = -1,
             const std::string& library_path = "");
  ~GlfwOutput();
  GlfwOutput(GlfwOutput&&) noexcept;
  GlfwOutput& operator=(GlfwOutput&&) noexcept;
  GlfwOutput(const GlfwOutput&) = delete;
  GlfwOutput& operator=(const GlfwOutput&) = delete;

  static std::vector<DisplayInfo> displays(const std::string& library_path = "");
  // A frame whose time is in the future is cleared and rejected. Expiry is
  // enforced when present or poll runs; it is not an external watchdog.
  bool present(const RasterFrame& frame, double now);
  bool poll(double now);
  void blackout();
  void close();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace spatialgl::raster
#endif  // SPATIALGL_GLFW_OUTPUT_H_
