#include "spatialgl/raster.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

namespace spatialgl::raster {
namespace {
using V3 = optics::Vec3;
bool finite(V3 v) {
  return std::all_of(v.begin(), v.end(), [](double x) { return std::isfinite(x); });
}
bool finite2(optics::Vec2 v) {
  return std::all_of(v.begin(), v.end(), [](double x) { return std::isfinite(x); });
}
double dot(V3 a, V3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V3 sub(V3 a, V3 b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V3 cross(V3 a, V3 b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double norm(V3 a) { return std::sqrt(dot(a, a)); }
V3 unit(V3 a) {
  const double n = norm(a);
  if (!std::isfinite(n) || n < 1e-12) throw std::invalid_argument("invalid camera basis");
  return {a[0] / n, a[1] / n, a[2] / n};
}
void solve8(double a[8][9], double out[8]) {
  for (int c = 0; c < 8; c++) {
    int pivot = c;
    for (int r = c + 1; r < 8; r++)
      if (std::abs(a[r][c]) > std::abs(a[pivot][c])) pivot = r;
    if (!std::isfinite(a[pivot][c]) || std::abs(a[pivot][c]) < 1e-12)
      throw std::invalid_argument("degenerate homography correspondence");
    for (int j = c; j < 9; j++) std::swap(a[c][j], a[pivot][j]);
    const double d = a[c][c];
    for (int j = c; j < 9; j++) a[c][j] /= d;
    for (int r = 0; r < 8; r++)
      if (r != c) {
        const double f = a[r][c];
        for (int j = c; j < 9; j++) a[r][j] -= f * a[c][j];
      }
  }
  for (int i = 0; i < 8; i++) out[i] = a[i][8];
}
optics::Vec2 map(const Homography& h, optics::Vec2 p) {
  double scale = 0;
  for (double x : h) scale = std::max(scale, std::abs(x));
  if (!std::isfinite(scale) || scale == 0) throw std::invalid_argument("invalid homography");
  Homography normalized;
  for (int i = 0; i < 9; ++i) normalized[i] = h[i] / scale;
  const double d = normalized[6] * p[0] + normalized[7] * p[1] + normalized[8];
  if (!std::isfinite(d) || std::abs(d) < 1e-12) throw std::invalid_argument("homography pole");
  return {(normalized[0] * p[0] + normalized[1] * p[1] + normalized[2]) / d,
          (normalized[3] * p[0] + normalized[4] * p[1] + normalized[5]) / d};
}
bool valid_h(const Homography& h) {
  if (!std::all_of(h.begin(), h.end(), [](double x) { return std::isfinite(x); })) return false;
  double scale = 0;
  for (double x : h) scale = std::max(scale, std::abs(x));
  if (!std::isfinite(scale) || scale == 0) return false;
  double a[9];
  for (int i = 0; i < 9; i++) a[i] = h[i] / scale;
  const double det = a[0] * (a[4] * a[8] - a[5] * a[7]) - a[1] * (a[3] * a[8] - a[5] * a[6]) +
                     a[2] * (a[3] * a[7] - a[4] * a[6]);
  const double adj[9] = {
      a[4] * a[8] - a[5] * a[7], a[2] * a[7] - a[1] * a[8], a[1] * a[5] - a[2] * a[4],
      a[5] * a[6] - a[3] * a[8], a[0] * a[8] - a[2] * a[6], a[2] * a[3] - a[0] * a[5],
      a[3] * a[7] - a[4] * a[6], a[1] * a[6] - a[0] * a[7], a[0] * a[4] - a[1] * a[3]};
  double anorm = 0, adj_norm = 0;
  for (int r = 0; r < 3; r++) {
    double row = 0, arow = 0;
    for (int c = 0; c < 3; c++) {
      row += std::abs(a[r * 3 + c]);
      arow += std::abs(adj[r * 3 + c]);
    }
    anorm = std::max(anorm, row);
    adj_norm = std::max(adj_norm, arow);
  }
  if (!std::isfinite(det) || std::abs(det) < 1e-12 ||
      !std::isfinite(anorm * adj_norm / std::abs(det)) || anorm * adj_norm / std::abs(det) > 1e10)
    return false;
  const double d[4] = {a[8], a[6] + a[8], a[6] + a[7] + a[8], a[7] + a[8]};
  return std::all_of(std::begin(d), std::end(d),
                     [](double x) { return std::isfinite(x) && std::abs(x) > 1e-10; }) &&
         std::all_of(std::begin(d) + 1, std::end(d),
                     [&](double x) { return std::signbit(x) == std::signbit(d[0]); });
}
std::uint8_t encode(double x, TransferFunction tf) {
  x = std::clamp(x, 0.0, 1.0);
  if (tf == TransferFunction::kSrgb)
    x = x <= 0.0031308 ? 12.92 * x : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
  return static_cast<std::uint8_t>(std::lround(std::clamp(x, 0.0, 1.0) * 255));
}
struct Pixel {
  int x, y;
};
}  // namespace

Homography fit_homography(const std::array<optics::Vec2, 4>& s,
                          const std::array<optics::Vec2, 4>& d) {
  for (int i = 0; i < 4; i++)
    if (!finite2(s[i]) || !finite2(d[i]))
      throw std::invalid_argument("homography points must be finite");
  auto noncollinear = [](const auto& p) {
    double max_area = 0;
    for (int i = 0; i < 4; i++)
      for (int j = i + 1; j < 4; j++)
        for (int k = j + 1; k < 4; k++)
          max_area = std::max(max_area, std::abs((p[j][0] - p[i][0]) * (p[k][1] - p[i][1]) -
                                                 (p[j][1] - p[i][1]) * (p[k][0] - p[i][0])));
    return max_area > 1e-10;
  };
  if (!noncollinear(s) || !noncollinear(d))
    throw std::invalid_argument("homography points are collinear or duplicate");
  double a[8][9]{};
  for (int i = 0; i < 4; i++) {
    const double x = s[i][0], y = s[i][1], u = d[i][0], v = d[i][1];
    const int r = i * 2;
    a[r][0] = x;
    a[r][1] = y;
    a[r][2] = 1;
    a[r][6] = -u * x;
    a[r][7] = -u * y;
    a[r][8] = u;
    a[r + 1][3] = x;
    a[r + 1][4] = y;
    a[r + 1][5] = 1;
    a[r + 1][6] = -v * x;
    a[r + 1][7] = -v * y;
    a[r + 1][8] = v;
  }
  double x[8];
  solve8(a, x);
  Homography h{x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], 1};
  if (!valid_h(h)) throw std::invalid_argument("homography crosses a pole or is unstable");
  for (int i = 0; i < 4; i++) {
    auto q = map(h, s[i]);
    if (std::hypot(q[0] - d[i][0], q[1] - d[i][1]) > 1e-7 * (1 + std::hypot(d[i][0], d[i][1])))
      throw std::invalid_argument("unstable homography fit");
  }
  return h;
}

RasterFrame compile(const optics::Snapshot& s, const optics::DeviceProfile& d,
                    const Calibration& c) {
  if (d.width < 1 || d.height < 1 || d.width > 16384 || d.height > 16384 ||
      static_cast<std::uint64_t>(d.width) * d.height > 64000000)
    throw std::invalid_argument("raster dimensions exceed supported bounds");
  if (c.point_radius < 0 || c.point_radius > 128 || !std::isfinite(c.lease_seconds) ||
      c.lease_seconds <= 0 || c.lease_seconds > 60 ||
      (c.transfer != TransferFunction::kLinear && c.transfer != TransferFunction::kSrgb) ||
      !valid_h(c.homography) || !std::isfinite(s.time))
    throw std::invalid_argument("invalid raster calibration or timestamp");
  RasterFrame f;
  f.device_id = d.id;
  f.time = s.time;
  f.expires_at = s.time + c.lease_seconds;
  if (!std::isfinite(f.expires_at)) throw std::invalid_argument("raster lease timestamp overflow");
  f.world_revision = s.world_revision;
  f.calibration_revision = s.calibration_revision;
  f.width = d.width;
  f.height = d.height;
  f.rgb.assign(static_cast<std::size_t>(d.width) * d.height * 3, 0);
  auto fail = [&](const char* code, const char* message) {
    f.diagnostics.push_back({optics::PlanStatus::kInfeasible, d.id, code, message});
  };
  const optics::Receipt* receipt = nullptr;
  for (const auto& r : s.receipts)
    if (r.device_id == d.id) receipt = &r;
  if (!receipt || receipt->availability != optics::Availability::kAvailable) {
    fail("raster-no-receipt", "No available device receipt; output is black");
    return f;
  }
  using ProgramKey = std::pair<optics::Id, optics::Id>;
  std::set<ProgramKey> eligible_draws;
  std::map<ProgramKey, double> program_expiries;
  bool found_program = false;
  for (const auto& p : s.programs)
    if (p.device_id == d.id) {
      found_program = true;
      if (p.device_kind != optics::DeviceKind::kFixedRaster ||
          p.world_revision != s.world_revision ||
          p.calibration_revision != s.calibration_revision) {
        fail("raster-program-revision",
             "Device program is unsupported or revision-inconsistent; output is black");
        return f;
      }
      if (p.generation != receipt->generation) {
        fail("raster-program-generation",
             "Device program does not match its receipt; output is black");
        return f;
      }
      if (!std::isfinite(p.expires_at) || !std::isfinite(p.starts_at) ||
          p.expires_at <= p.starts_at) {
        fail("raster-invalid-program", "Device program timing is malformed; output is black");
        return f;
      }
      if (p.starts_at > s.time || p.expires_at <= s.time) continue;
      for (const auto& e : p.events)
        if (e.kind == optics::EventKind::kEmit) {
          if (e.display_list_id != p.display_list_id || e.draw_id.empty()) {
            fail("raster-invalid-program",
                 "Device program event identity is malformed; output is black");
            return f;
          }
          eligible_draws.insert({p.display_list_id, e.draw_id});
          auto key = ProgramKey{p.display_list_id, e.draw_id};
          auto expiry = program_expiries.find(key);
          if (expiry == program_expiries.end())
            program_expiries.emplace(key, p.expires_at);
          else
            expiry->second = std::min(expiry->second, p.expires_at);
        }
    }
  if (!found_program) {
    fail("raster-no-program", "No device program; output is black");
    return f;
  }
  if (d.kind != optics::DeviceKind::kFixedRaster) {
    fail("raster-unsupported-device", "Only fixed raster output is supported; output is black");
    return f;
  }
  try {
    const V3 forward = unit(sub(d.target, d.position));
    const V3 right = unit(cross(forward, d.up));
    const V3 up = cross(right, forward);
    const double tan_half = std::tan(d.fov_y / 2), aspect = double(d.width) / d.height;
    if (!std::isfinite(d.fov_y) || d.fov_y <= 0 || d.fov_y >= std::acos(-1.0) ||
        !std::isfinite(tan_half) || tan_half <= 0 || !finite(d.position) || !finite(d.target) ||
        !finite(d.up) || !std::isfinite(d.near_plane) || !std::isfinite(d.far_plane) ||
        d.near_plane <= 0 || d.far_plane <= d.near_plane)
      throw std::invalid_argument("invalid device projection");
    // First sum emissions at an exactly coincident world sample, then use max
    // for separate world samples that collide in the raster/splat.
    using Key = std::tuple<double, double, double>;
    std::map<Key, std::array<double, 3>> world_bins;
    bool found_sample = false;
    for (const auto& q : s.contributions)
      if (q.device_id == d.id && eligible_draws.contains({q.display_list_id, q.draw_id})) {
        if (!finite(q.position) || !std::isfinite(q.intensity) || q.intensity < 0 ||
            !std::all_of(
                q.linear_rgb.begin(), q.linear_rgb.end(),
                [](double x) { return std::isfinite(x) && x >= 0; }))
          throw std::invalid_argument("malformed matching contribution");
        found_sample = true;
        auto expiry = program_expiries.find({q.display_list_id, q.draw_id});
        if (expiry != program_expiries.end()) f.expires_at = std::min(f.expires_at, expiry->second);
        auto& color = world_bins[{q.position[0], q.position[1], q.position[2]}];
        for (int k = 0; k < 3; k++) {
          const double emitted = q.linear_rgb[k] * q.intensity;
          if (!std::isfinite(emitted) || !std::isfinite(color[k] + emitted))
            throw std::invalid_argument("contribution color overflow");
          color[k] += emitted;
        }
      }
    if (!found_sample) {
      fail("raster-no-active-samples", "No active sampled emissions; output is black");
      return f;
    }
    if (s.time >= f.expires_at) {
      fail("raster-expired-program", "Program lease has expired; output is black");
      return f;
    }
    std::map<std::size_t, std::array<double, 3>> pixels;
    for (const auto& [pos, color] : world_bins) {
      V3 x{std::get<0>(pos), std::get<1>(pos), std::get<2>(pos)}, delta = sub(x, d.position);
      const double z = dot(delta, forward);
      if (z < d.near_plane || z > d.far_plane) continue;
      const double nx = dot(delta, right) / (z * tan_half * aspect),
                   ny = dot(delta, up) / (z * tan_half);
      if (!std::isfinite(nx) || !std::isfinite(ny) || std::abs(nx) > 1 || std::abs(ny) > 1)
        continue;
      // Device normalized coordinates use bottom-left +Y-up; raster coordinates are top-left.
      auto uv = map(c.homography, {(nx + 1) * 0.5, (1 - ny) * 0.5});
      if (!finite2(uv) || uv[0] < 0 || uv[0] > 1 || uv[1] < 0 || uv[1] > 1) continue;
      int cx = std::clamp(static_cast<int>(std::floor(uv[0] * d.width)), 0, d.width - 1);
      int cy = std::clamp(static_cast<int>(std::floor(uv[1] * d.height)), 0, d.height - 1);
      for (int yy = cy - c.point_radius; yy <= cy + c.point_radius; yy++)
        for (int xx = cx - c.point_radius; xx <= cx + c.point_radius; xx++) {
          if (xx < 0 || yy < 0 || xx >= d.width || yy >= d.height) continue;
          const auto index = (static_cast<std::size_t>(yy) * d.width + xx) * 3;
          auto& dst = pixels[index];
          for (int k = 0; k < 3; k++) dst[k] = std::max(dst[k], color[k]);
        }
    }
    for (const auto& [index, color] : pixels)
      for (int k = 0; k < 3; k++) f.rgb[index + k] = encode(color[k], c.transfer);
  } catch (const std::invalid_argument& e) {
    std::fill(f.rgb.begin(), f.rgb.end(), 0);
    fail("raster-invalid-input", e.what());
  }
  return f;
}

void write_ppm(const RasterFrame& f, const std::string& path) {
  if (f.width <= 0 || f.height <= 0 || static_cast<std::uint64_t>(f.width) * f.height > 64000000 ||
      f.rgb.size() != static_cast<std::size_t>(f.width) * f.height * 3)
    throw std::invalid_argument("malformed raster frame");
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot open PPM output: " + path);
  out << "P6\n" << f.width << " " << f.height << "\n255\n";
  out.write(reinterpret_cast<const char*>(f.rgb.data()),
            static_cast<std::streamsize>(f.rgb.size()));
  if (!out) throw std::runtime_error("failed writing PPM output: " + path);
}
}  // namespace spatialgl::raster
