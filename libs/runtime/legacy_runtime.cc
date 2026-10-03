#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

#include "spatialgl/spatialgl.h"
namespace spatialgl {
namespace {
void require(bool c, const char* s) {
  if (!c) throw std::invalid_argument(s);
}
void validate_device(const DeviceDescriptor& d) {
  require(!d.id.empty() && std::isfinite(d.refresh_hz) && d.refresh_hz > 0 &&
              std::isfinite(d.latency) && d.latency >= 0,
          "Invalid device clock");
}
std::optional<std::string> optional_id(const std::string& id) {
  return id.empty() ? std::nullopt : std::optional{id};
}
constexpr double kInfinity = std::numeric_limits<double>::infinity();
}  // namespace
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
