#include "spatialgl/spatialgl.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace spatialgl {
namespace {
constexpr double kInfinity = std::numeric_limits<double>::infinity();
bool finite(Vec3 p) {
  return std::all_of(p.begin(), p.end(), [](double x) { return std::isfinite(x); });
}
Vec3 cross(Vec3 a, Vec3 b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(Vec3 v) { return scale(v, 1 / std::sqrt(dot(v, v))); }
void require(bool condition, const char* reason) {
  if (!condition) throw std::invalid_argument(reason);
}
bool valid_basis(Vec3 u, Vec3 v) {
  return finite(u) && finite(v) && dot(u, u) > 0 && dot(v, v) > 0 && std::abs(dot(u, v)) <= 1e-6;
}
void validate_device(const DeviceDescriptor& d) {
  require(!d.id.empty() && std::isfinite(d.refresh_hz) && d.refresh_hz > 0 &&
              std::isfinite(d.latency) && d.latency >= 0,
          "Invalid device clock");
}
void validate_raster(int width, int height) {
  require(width >= 2 && height >= 2, "Invalid raster dimensions");
}
std::optional<std::string> optional_id(const std::string& id) {
  return id.empty() ? std::nullopt : std::optional{id};
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
Plan plan(const std::vector<Sample>& samples, const std::vector<std::unique_ptr<Backend>>& backends,
          const World& world) {
  Plan result;
  for (const auto& b : backends) result.assignments[b->descriptor().id] = {};
  for (const auto& sample : samples) {
    Unmet unmet{sample, {}};
    std::vector<std::pair<const Backend*, Candidate>> candidates;
    for (const auto& b : backends) {
      auto feasible = b->evaluate(sample, world);
      if (auto* c = std::get_if<Candidate>(&feasible))
        candidates.emplace_back(b.get(), *c);
      else
        unmet.attempts.emplace_back(b->descriptor().id, std::get<std::string>(feasible));
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
      return a.second.score != b.second.score ? a.second.score > b.second.score
                                              : a.first->descriptor().id < b.first->descriptor().id;
    });
    bool assigned = false;
    for (const auto& [backend, c] : candidates) {
      const auto d = backend->descriptor();
      auto& list = result.assignments[d.id];
      if (list.size() >= d.capacity) {
        unmet.attempts.emplace_back(d.id, "capacity");
        continue;
      }
      if (c.pixel && std::any_of(list.begin(), list.end(),
                                 [&](const auto& other) { return other.pixel == c.pixel; })) {
        unmet.attempts.emplace_back(d.id, "pixel-conflict");
        continue;
      }
      list.push_back(c);
      assigned = true;
      break;
    }
    if (!assigned) {
      if (unmet.attempts.empty()) unmet.attempts.emplace_back("none", "no-device");
      result.unmet.push_back(std::move(unmet));
    }
  }
  return result;
}
Runtime::Runtime(World world, const std::vector<std::shared_ptr<Backend>>& backends, double spacing,
                 double tolerance)
    : world_(std::move(world)), spacing_(spacing), tolerance_(tolerance) {
  validate_world(world_);
  require(std::isfinite(spacing) && spacing > 0 && std::isfinite(tolerance) && tolerance >= 0,
          "Invalid sampling or tolerance");
  std::set<std::string> ids;
  for (const auto& b : backends) {
    require(b != nullptr, "Null backend");
    const auto d = b->descriptor();
    validate_device(d);
    require(ids.insert(d.id).second, "Duplicate device ID");
    auto copy = b->clone();
    require(copy != nullptr && copy->descriptor().id == d.id, "Invalid backend clone");
    copy->reset();
    backends_.push_back(std::move(copy));
    refresh_counts_.push_back(0);
  }
}
void Runtime::submit(SceneFrame frame) {
  validate_frame(frame);
  require(frame.present_at >= time_, "Cannot submit a scene in the past");
  require(
      std::none_of(frames_.begin(), frames_.end(), [&](const auto& f) { return f.id == frame.id; }),
      "Duplicate scene ID");
  // Validate the resource budget before modifying the submitted scene history.
  sample_scene(frame.primitives, spacing_);
  frames_.push_back(std::move(frame));
}
const SceneFrame* Runtime::scene_at(double time) const {
  const SceneFrame* latest = nullptr;
  for (const auto& frame : frames_)
    if (frame.present_at <= time && (!latest || frame.present_at >= latest->present_at))
      latest = &frame;
  return latest && time < latest->expires_at ? latest : nullptr;
}
RealizationState Runtime::advance(double to) {
  require(std::isfinite(to) && to >= time_, "Clock must advance monotonically");
  while (true) {
    double next = kInfinity;
    for (std::size_t i = 0; i < backends_.size(); ++i)
      next = std::min(next, refresh_counts_[i] / backends_[i]->descriptor().refresh_hz);
    for (const auto& f : pending_) next = std::min(next, f.apply_at);
    if (!std::isfinite(next) || next > to) break;
    for (auto& b : backends_) b->advance(next - time_);
    time_ = next;
    const auto* scene = scene_at(next);
    const auto routed =
        plan(scene ? sample_scene(scene->primitives, spacing_) : std::vector<Sample>{}, backends_,
             world_);
    for (std::size_t i = 0; i < backends_.size(); ++i) {
      auto& b = backends_[i];
      const auto d = b->descriptor();
      if (std::abs(refresh_counts_[i] / d.refresh_hz - next) < 1e-9) {
        pending_.push_back({d.id, scene ? scene->id : "", next, next + d.latency,
                            scene ? scene->expires_at : next,
                            b->encode(routed.assignments.at(d.id))});
        ++refresh_counts_[i];
      }
    }
    // Stable delivery order preserves FIFO for each device, including zero latency.
    for (auto it = pending_.begin(); it != pending_.end();) {
      if (it->apply_at <= next) {
        auto b = std::find_if(backends_.begin(), backends_.end(),
                              [&](const auto& b) { return b->descriptor().id == it->device_id; });
        (*b)->apply(*it);
        applied_.insert_or_assign(it->device_id, *it);
        it = pending_.erase(it);
      } else
        ++it;
    }
  }
  for (auto& b : backends_) b->advance(to - time_);
  time_ = to;
  return snapshot();
}
RealizationState Runtime::snapshot() const {
  const auto* scene = scene_at(time_);
  const auto samples = scene ? sample_scene(scene->primitives, spacing_) : std::vector<Sample>{};
  const auto routed = plan(samples, backends_, world_);
  RealizationState state{time_, scene ? std::optional{scene->id} : std::nullopt, {}, {}, {}};
  for (const auto& [_, frame] : applied_) state.device_frames.push_back(frame);
  std::map<std::string, std::vector<Observation>> observations;
  for (const auto& b : backends_) {
    const auto id = b->descriptor().id;
    auto observed = b->observe(time_);
    auto f = applied_.find(id);
    const auto source = f == applied_.end() ? std::nullopt : optional_id(f->second.scene_id);
    const double age = f == applied_.end() ? 0 : time_ - f->second.issued_at;
    for (const auto& o : observed)
      state.outputs.push_back({o, id, source, age, source != state.scene_id});
    observations[id] = std::move(observed);
  }
  for (const auto& sample : samples) {
    RealizedSample r{sample,    std::nullopt, std::nullopt, std::nullopt,
                     "pending", std::nullopt, std::nullopt, {}};
    for (const auto& unmet : routed.unmet)
      if (unmet.sample.id == sample.id) {
        r.status = "unrealizable";
        for (const auto& [id, reason] : unmet.attempts) r.reasons.push_back(id + ": " + reason);
      }
    for (const auto& [id, list] : routed.assignments)
      if (std::any_of(list.begin(), list.end(),
                      [&](const auto& c) { return c.sample.id == sample.id; })) {
        r.device_id = id;
        auto frame = applied_.find(id);
        if (frame != applied_.end()) {
          r.source_scene_id = optional_id(frame->second.scene_id);
          r.age = time_ - frame->second.issued_at;
        }
        for (const auto& o : observations.at(id))
          if (o.sample_id == sample.id) {
            r.actual = o.position;
            r.error = distance(sample.position, o.position);
            r.status = *r.error <= tolerance_ ? "realized" : "tracking";
            break;
          }
        break;
      }
    state.samples.push_back(std::move(r));
  }
  return state;
}
}  // namespace spatialgl
