#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

#include "spatialgl/spatialgl.h"
namespace spatialgl {
namespace {
bool finite(Vec3 p) {
  return std::all_of(p.begin(), p.end(), [](double x) { return std::isfinite(x); });
}
void require(bool c, const char* s) {
  if (!c) throw std::invalid_argument(s);
}
bool valid_basis(Vec3 u, Vec3 v) {
  return finite(u) && finite(v) && dot(u, u) > 0 && dot(v, v) > 0 && std::abs(dot(u, v)) <= 1e-6;
}
}  // namespace
Vec3 add(Vec3 a, Vec3 b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 scale(Vec3 a, double n) { return {a[0] * n, a[1] * n, a[2] * n}; }
double dot(Vec3 a, Vec3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double distance(Vec3 a, Vec3 b) {
  const auto d = sub(a, b);
  return std::sqrt(dot(d, d));
}
std::optional<std::array<double, 2>> surface_uv(Vec3 p, const Surface& s) {
  const auto d = sub(p, s.origin);
  const double u = dot(d, s.u) / dot(s.u, s.u), v = dot(d, s.v) / dot(s.v, s.v);
  if (distance(p, add(s.origin, add(scale(s.u, u), scale(s.v, v)))) <= 1e-6 && u >= -1e-8 &&
      u <= 1 + 1e-8 && v >= -1e-8 && v <= 1 + 1e-8)
    return std::array<double, 2>{u, v};
  return std::nullopt;
}
bool blocked(Vec3 a, Vec3 b, const std::vector<Occluder>& boxes) {
  const auto d = sub(b, a);
  return std::any_of(boxes.begin(), boxes.end(), [&](const auto& box) {
    double near = 1e-7, far = 1 - 1e-7;
    for (int k = 0; k < 3; ++k) {
      if (std::abs(d[k]) < 1e-12) {
        if (a[k] < box.min[k] || a[k] > box.max[k]) return false;
      } else {
        const double x = (box.min[k] - a[k]) / d[k], y = (box.max[k] - a[k]) / d[k];
        near = std::max(near, std::min(x, y));
        far = std::min(far, std::max(x, y));
        if (near > far) return false;
      }
    }
    return true;
  });
}
void validate_world(const World& world) {
  std::set<std::string> ids;
  for (const auto& s : world.surfaces)
    require(!s.id.empty() && ids.insert(s.id).second && finite(s.origin) && valid_basis(s.u, s.v),
            "Invalid or duplicate surface");
  for (const auto& b : world.occluders) {
    require(finite(b.min) && finite(b.max), "Invalid occluder");
    for (int i = 0; i < 3; ++i) require(b.min[i] <= b.max[i], "Inverted occluder bounds");
  }
}
void validate_frame(const SceneFrame& frame) {
  require(!frame.id.empty() && std::isfinite(frame.present_at) && std::isfinite(frame.expires_at) &&
              frame.present_at >= 0 && frame.expires_at > frame.present_at,
          "Invalid scene interval");
  std::set<std::string> ids;
  for (const auto& primitive : frame.primitives)
    std::visit(
        [&](const auto& p) {
          require(!p.id.empty() && ids.insert(p.id).second, "Primitive IDs must be unique");
          require(finite(p.color) && std::all_of(p.color.begin(), p.color.end(),
                                                 [](double c) { return c >= 0 && c <= 1; }),
                  "RGB must be in [0,1]");
          using T = std::decay_t<decltype(p)>;
          if constexpr (std::is_same_v<T, Point>)
            require(finite(p.position), "Non-finite point");
          else if constexpr (std::is_same_v<T, Polyline>)
            require(
                p.vertices.size() >= 2 && std::all_of(p.vertices.begin(), p.vertices.end(), finite),
                "Invalid polyline");
          else
            require(finite(p.origin) && valid_basis(p.u, p.v),
                    "Patch requires perpendicular nonzero edges");
        },
        primitive);
}
std::vector<Sample> sample_scene(const std::vector<Primitive>& primitives, double spacing) {
  require(std::isfinite(spacing) && spacing > 0, "Invalid sample spacing");
  std::vector<Sample> result;
  for (const auto& primitive : primitives)
    std::visit(
        [&](const auto& p) {
          std::size_t index = 0;
          using T = std::decay_t<decltype(p)>;
          constexpr auto kind = std::is_same_v<T, Point>      ? Kind::Point
                                : std::is_same_v<T, Polyline> ? Kind::Polyline
                                                              : Kind::Patch;
          const auto emit = [&](Vec3 position) {
            result.push_back({p.id + ":" + std::to_string(index++), p.id, kind, position, p.color,
                              p.surface_id});
          };
          const auto divisions = [&](double len) {
            const double n = std::max(1.0, std::ceil(len / spacing));
            require(std::isfinite(n) && n <= 100000, "Sampling budget exceeded");
            return static_cast<std::size_t>(n);
          };
          if constexpr (std::is_same_v<T, Point>)
            emit(p.position);
          else if constexpr (std::is_same_v<T, Polyline>) {
            for (std::size_t i = 1; i < p.vertices.size(); ++i) {
              const auto a = p.vertices[i - 1], b = p.vertices[i];
              const auto n = divisions(distance(a, b));
              require(result.size() + n + 1 <= 100000, "Sampling budget exceeded");
              for (std::size_t j = i == 1 ? 0 : 1; j <= n; ++j) {
                const double t = static_cast<double>(j) / n;
                emit(add(scale(a, 1 - t), scale(b, t)));
              }
            }
          } else {
            const auto nu = divisions(std::sqrt(dot(p.u, p.u))),
                       nv = divisions(std::sqrt(dot(p.v, p.v)));
            require((nu + 1) * (nv + 1) + result.size() <= 100000, "Sampling budget exceeded");
            for (std::size_t i = 0; i <= nu; ++i)
              for (std::size_t j = 0; j <= nv; ++j)
                emit(add(p.origin, add(scale(p.u, static_cast<double>(i) / nu),
                                       scale(p.v, static_cast<double>(j) / nv))));
          }
          require(result.size() <= 100000, "Sampling budget exceeded");
        },
        primitive);
  return result;
}
}  // namespace spatialgl
