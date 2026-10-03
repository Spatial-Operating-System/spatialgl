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
Vec3 cross(Vec3 a, Vec3 b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(Vec3 v) { return scale(v, 1 / std::sqrt(dot(v, v))); }
void require(bool c, const char* s) {
  if (!c) throw std::invalid_argument(s);
}
void validate_device(const DeviceDescriptor& d) {
  require(!d.id.empty() && std::isfinite(d.refresh_hz) && d.refresh_hz > 0 &&
              std::isfinite(d.latency) && d.latency >= 0,
          "Invalid device clock");
}
void validate_raster(int w, int h) { require(w >= 2 && h >= 2, "Invalid raster dimensions"); }
}  // namespace
void RasterBackend::apply(const DeviceFrame& frame) {
  require(std::holds_alternative<RasterCommand>(frame.command), "Expected raster command");
  frame_ = frame;
}
std::vector<Observation> RasterBackend::observe(double now) const {
  std::vector<Observation> outputs;
  if (frame_ && now < frame_->expires_at)
    for (const auto& c : std::get<RasterCommand>(frame_->command).samples)
      outputs.push_back({c.sample.id, c.position, c.sample.color});
  return outputs;
}
MonitorBackend::MonitorBackend(MonitorConfig config) : config_(std::move(config)) {
  validate_device(descriptor());
  validate_raster(config_.width, config_.height);
  require(!config_.surface_id.empty(), "Monitor requires a support ID");
}
DeviceDescriptor MonitorBackend::descriptor() const {
  return {config_.id, "screen", config_.refresh_hz, config_.latency};
}
DeviceCommand MonitorBackend::encode(const std::vector<Candidate>& samples) const {
  return RasterCommand{config_.width, config_.height, samples};
}
std::unique_ptr<Backend> MonitorBackend::clone() const {
  return std::make_unique<MonitorBackend>(config_);
}
Feasibility MonitorBackend::evaluate(const Sample& sample, const World& world) const {
  auto it = std::find_if(world.surfaces.begin(), world.surfaces.end(),
                         [&](const auto& s) { return s.id == config_.surface_id; });
  if (it == world.surfaces.end() || (!sample.surface_id.empty() && sample.surface_id != it->id))
    return std::string{"no-support-surface"};
  auto uv = surface_uv(sample.position, *it);
  if (!uv) return std::string{"outside-support"};
  const std::array<int, 2> pixel{static_cast<int>(std::lround((*uv)[0] * (config_.width - 1))),
                                 static_cast<int>(std::lround((*uv)[1] * (config_.height - 1)))};
  const auto position =
      add(it->origin, add(scale(it->u, static_cast<double>(pixel[0]) / (config_.width - 1)),
                          scale(it->v, static_cast<double>(pixel[1]) / (config_.height - 1))));
  return Candidate{sample, 100, position, pixel};
}
ProjectorBackend::ProjectorBackend(ProjectorConfig config) : config_(std::move(config)) {
  validate_device(descriptor());
  validate_raster(config_.width, config_.height);
  const auto forward = sub(config_.target, config_.position), right = cross(forward, config_.up);
  require(finite(config_.position) && finite(config_.target) && finite(config_.up) &&
              std::isfinite(config_.fov_y) && config_.fov_y > 0 && config_.fov_y < std::acos(-1) &&
              std::isfinite(config_.near) && std::isfinite(config_.far) && config_.near > 0 &&
              config_.far > config_.near && dot(forward, forward) > 0 && dot(right, right) > 0,
          "Invalid projector calibration");
}
DeviceDescriptor ProjectorBackend::descriptor() const {
  return {config_.id, "projection", config_.refresh_hz, config_.latency};
}
DeviceCommand ProjectorBackend::encode(const std::vector<Candidate>& samples) const {
  return RasterCommand{config_.width, config_.height, samples};
}
std::unique_ptr<Backend> ProjectorBackend::clone() const {
  return std::make_unique<ProjectorBackend>(config_);
}
Feasibility ProjectorBackend::evaluate(const Sample& sample, const World& world) const {
  const auto& p = config_;
  auto it = std::find_if(world.surfaces.begin(), world.surfaces.end(), [&](const auto& s) {
    return (sample.surface_id.empty() || sample.surface_id == s.id) &&
           surface_uv(sample.position, s).has_value();
  });
  if (it == world.surfaces.end()) return std::string{"no-support-surface"};
  const auto forward = unit(sub(p.target, p.position)), right = unit(cross(forward, p.up)),
             up = cross(right, forward), d = sub(sample.position, p.position);
  const double z = dot(d, forward), h = std::tan(p.fov_y / 2),
               aspect = static_cast<double>(p.width) / p.height;
  if (z < p.near || z > p.far) return std::string{"outside-frustum"};
  const double x = dot(d, right) / (z * h * aspect), y = dot(d, up) / (z * h);
  if (std::abs(x) > 1 || std::abs(y) > 1) return std::string{"outside-frustum"};
  if (blocked(p.position, sample.position, world.occluders)) return std::string{"occluded"};
  const std::array<int, 2> pixel{static_cast<int>(std::lround((x + 1) * 0.5 * (p.width - 1))),
                                 static_cast<int>(std::lround((1 - y) * 0.5 * (p.height - 1)))};
  const auto ray =
      add(forward,
          add(scale(right, (static_cast<double>(pixel[0]) / (p.width - 1) * 2 - 1) * h * aspect),
              scale(up, (1 - static_cast<double>(pixel[1]) / (p.height - 1) * 2) * h)));
  const auto normal = cross(it->u, it->v);
  const double denominator = dot(ray, normal);
  if (std::abs(denominator) < 1e-12) return std::string{"outside-support"};
  const auto position =
      add(p.position, scale(ray, dot(sub(it->origin, p.position), normal) / denominator));
  if (!surface_uv(position, *it)) return std::string{"outside-support"};
  if (blocked(p.position, position, world.occluders)) return std::string{"occluded"};
  return Candidate{sample, 50 - z, position, pixel};
}
DroneBackend::DroneBackend(DroneConfig config) : config_(std::move(config)) {
  validate_device(descriptor());
  require(config_.count > 0 && config_.count <= 100000 && std::isfinite(config_.speed) &&
              config_.speed > 0 && finite(config_.home) && finite(config_.min) &&
              finite(config_.max),
          "Invalid emitter configuration");
  for (int i = 0; i < 3; ++i)
    require(config_.min[i] <= config_.max[i] && config_.home[i] >= config_.min[i] &&
                config_.home[i] <= config_.max[i],
            "Invalid emitter bounds");
  reset();
}
DeviceDescriptor DroneBackend::descriptor() const {
  return {config_.id, "emitter", config_.refresh_hz, config_.latency, config_.count};
}
std::unique_ptr<Backend> DroneBackend::clone() const {
  return std::make_unique<DroneBackend>(config_);
}
void DroneBackend::reset() {
  frame_.reset();
  positions_.assign(config_.count, config_.home);
  bindings_.clear();
}
Feasibility DroneBackend::evaluate(const Sample& sample, const World&) const {
  if (sample.kind != Kind::Point || !sample.surface_id.empty())
    return std::string{"unsupported-primitive"};
  for (int i = 0; i < 3; ++i)
    if (sample.position[i] < config_.min[i] || sample.position[i] > config_.max[i])
      return std::string{"outside-support"};
  return Candidate{sample, 10, sample.position, std::nullopt};
}
DeviceCommand DroneBackend::encode(const std::vector<Candidate>& samples) const {
  return EmitterCommand{samples};
}
void DroneBackend::advance(double dt) {
  if (!frame_) return;
  for (const auto& c : std::get<EmitterCommand>(frame_->command).targets) {
    auto& p = positions_[bindings_.at(c.sample.id)];
    const double d = distance(p, c.position);
    if (d > 0) p = add(p, scale(sub(c.position, p), std::min(1.0, config_.speed * dt / d)));
  }
}
void DroneBackend::apply(const DeviceFrame& frame) {
  require(std::holds_alternative<EmitterCommand>(frame.command), "Expected emitter targets");
  const auto& targets = std::get<EmitterCommand>(frame.command).targets;
  require(targets.size() <= config_.count, "Emitter capacity exceeded");
  std::map<std::string, std::size_t> next;
  std::set<std::size_t> used;
  for (const auto& c : targets)
    if (bindings_.contains(c.sample.id)) {
      const auto slot = bindings_.at(c.sample.id);
      next[c.sample.id] = slot;
      used.insert(slot);
    }
  for (const auto& c : targets)
    if (!next.contains(c.sample.id)) {
      std::size_t slot = 0;
      while (used.contains(slot)) ++slot;
      next[c.sample.id] = slot;
      used.insert(slot);
    }
  bindings_ = std::move(next);
  frame_ = frame;
}
std::vector<Observation> DroneBackend::observe(double now) const {
  std::vector<Observation> outputs;
  if (frame_ && now < frame_->expires_at)
    for (const auto& c : std::get<EmitterCommand>(frame_->command).targets)
      outputs.push_back({c.sample.id, positions_[bindings_.at(c.sample.id)], c.sample.color});
  return outputs;
}
}  // namespace spatialgl
