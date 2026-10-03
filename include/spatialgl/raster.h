#ifndef SPATIALGL_RASTER_H_
#define SPATIALGL_RASTER_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "spatialgl/optics.h"

namespace spatialgl::raster {

using Homography = std::array<double, 9>;
enum class TransferFunction { kLinear, kSrgb };
struct Calibration {
  // Maps normalized top-left image coordinates [0,1]^2 to output coordinates.
  Homography homography{1, 0, 0, 0, 1, 0, 0, 0, 1};
  // Nonnegative square splat radius in pixels; radius 0 emits a single pixel.
  int point_radius = 1;
  TransferFunction transfer = TransferFunction::kSrgb;
  double lease_seconds = 0.1;
};
struct RasterFrame {
  optics::Id device_id;
  double time = 0, expires_at = 0;
  std::uint64_t world_revision = 0, calibration_revision = 0;
  int width = 0, height = 0;
  std::vector<std::uint8_t> rgb;
  std::vector<optics::Diagnostic> diagnostics;
};

// Maps source image coordinates to destination coordinates. Throws for degenerate,
// ill-conditioned or pole-crossing correspondences.
Homography fit_homography(const std::array<optics::Vec2, 4>& source,
                          const std::array<optics::Vec2, 4>& destination);
// RasterFrame is a sampled rendering of simulator contributions; it does not
// establish measured physical output or continuous geometric coverage.
RasterFrame compile(const optics::Snapshot& snapshot, const optics::DeviceProfile& device,
                    const Calibration& calibration = {});
void write_ppm(const RasterFrame& frame, const std::string& path);

}  // namespace spatialgl::raster
#endif  // SPATIALGL_RASTER_H_
