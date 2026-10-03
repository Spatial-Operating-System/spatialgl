#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "spatialgl/capi/spatialgl.h"
#include "spatialgl/optics.h"

using namespace spatialgl::optics;

struct spgl_context;
struct spgl_frame {
  spgl_context* owner = nullptr;
  DisplayList value;
};
struct spgl_snapshot {
  Snapshot value;
};
struct spgl_context {
  std::string error;
  RuntimeOptions options;
  WorldSnapshot world;
  Rig rig;
  std::map<Id, PlanarMotion> motion_by_id;
  std::map<Id, bool> configured_devices;
  std::map<Id, Availability> availability;
  std::unique_ptr<Runtime> runtime;
  std::map<Id, uint64_t> tickets;
  uint64_t next_ticket = 1;
  double now = 0;
};

namespace {
constexpr uint32_t kAbi = SPATIALGL_ABI_VERSION;
class StatusError final : public std::exception {
 public:
  StatusError(spgl_status status, std::string message)
      : status_(status), message_(std::move(message)) {}
  spgl_status status() const noexcept { return status_; }
  const char* what() const noexcept override { return message_.c_str(); }

 private:
  spgl_status status_;
  std::string message_;
};
template <typename T>
bool valid_record(const T* p) {
  return p && p->abi_version == kAbi && p->size >= sizeof(T);
}
bool finite(spgl_vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
bool finite(spgl_rgb p) { return std::isfinite(p.r) && std::isfinite(p.g) && std::isfinite(p.b); }
bool valid_color(spgl_rgb p) { return finite(p) && p.r >= 0 && p.g >= 0 && p.b >= 0; }
Vec3 vec(spgl_vec3 p) { return {p.x, p.y, p.z}; }
RGB rgb(spgl_rgb p) { return {p.r, p.g, p.b}; }
spgl_vec3 cvec(Vec3 p) { return {p[0], p[1], p[2]}; }
spgl_rgb crgb(RGB p) { return {p[0], p[1], p[2]}; }
const char* str(const char* p, bool allow_empty = false) {
  if (!p || (!allow_empty && !*p)) throw std::invalid_argument("Required string is null or empty");
  return p;
}
bool valid_utf8(const char* s) {
  const auto* p = reinterpret_cast<const unsigned char*>(s);
  while (*p) {
    if (*p < 0x80) {
      ++p;
      continue;
    }
    unsigned n = (*p >= 0xC2 && *p <= 0xDF)   ? 1
                 : (*p >= 0xE0 && *p <= 0xEF) ? 2
                 : (*p >= 0xF0 && *p <= 0xF4) ? 3
                                              : 99;
    if (n == 99) return false;
    const unsigned char lead = *p++;
    for (unsigned i = 0; i < n; ++i)
      if ((p[i] & 0xC0) != 0x80) return false;
    if ((lead == 0xE0 && p[0] < 0xA0) || (lead == 0xED && p[0] >= 0xA0) ||
        (lead == 0xF0 && p[0] < 0x90) || (lead == 0xF4 && p[0] >= 0x90))
      return false;
    p += n;
  }
  return true;
}
Id id(const char* p, bool allow_empty = false) {
  str(p, allow_empty);
  if (!valid_utf8(p)) throw std::invalid_argument("String is not valid UTF-8");
  return p;
}
uint64_t hash_id(const Id& s) {
  if (s.empty()) return 0;
  uint64_t h = UINT64_C(14695981039346656037);
  for (unsigned char c : s) {
    h ^= c;
    h *= UINT64_C(1099511628211);
  }
  return h ? h : 1;
}
spgl_status code(const std::exception& e) {
  if (const auto* status_error = dynamic_cast<const StatusError*>(&e))
    return status_error->status();
  if (dynamic_cast<const std::bad_alloc*>(&e)) return SPGL_STATUS_NO_MEMORY;
  if (dynamic_cast<const std::out_of_range*>(&e)) return SPGL_STATUS_OUT_OF_RANGE;
  if (dynamic_cast<const std::invalid_argument*>(&e)) return SPGL_STATUS_INVALID_ARGUMENT;
  if (dynamic_cast<const std::logic_error*>(&e)) return SPGL_STATUS_INVALID_STATE;
  return SPGL_STATUS_INTERNAL;
}
template <typename F>
spgl_status call(spgl_context* ctx, F&& fn) {
  try {
    fn();
    if (ctx) ctx->error.clear();
    return SPGL_STATUS_OK;
  } catch (const std::exception& e) {
    if (ctx) try {
        ctx->error = e.what();
      } catch (...) {
      }
    return code(e);
  } catch (...) {
    if (ctx) try {
        ctx->error = "Unknown C++ exception";
      } catch (...) {
      }
    return SPGL_STATUS_INTERNAL;
  }
}
void ensure_context(spgl_context* c) {
  if (!c) throw std::invalid_argument("Context is null");
}
void ensure_runtime(spgl_context* c) {
  ensure_context(c);
  if (!c->runtime) {
    c->world.motion.clear();
    for (const auto& p : c->world.planes) {
      auto i = c->motion_by_id.find(p.id);
      c->world.motion.push_back(i == c->motion_by_id.end() ? PlanarMotion{} : i->second);
    }
    c->runtime = std::make_unique<Runtime>(c->world, c->rig, c->options);
    for (const auto& [device, state] : c->availability)
      if (state != Availability::kAvailable) c->runtime->set_availability(device, state);
  }
}
void update_world(spgl_context* c, WorldSnapshot world, std::map<Id, PlanarMotion> motions) {
  world.motion.clear();
  for (const auto& p : world.planes) {
    auto i = motions.find(p.id);
    world.motion.push_back(i == motions.end() ? PlanarMotion{} : i->second);
  }
  world.world_revision = std::max(world.world_revision, c->world.world_revision + 1);
  if (c->runtime) c->runtime->update_world(world);
  c->world = std::move(world);
  c->motion_by_id = std::move(motions);
}
void check_mutable_config(spgl_context* c) {
  ensure_context(c);
  if (c->runtime)
    throw std::logic_error("Device and resource configuration is frozen after runtime creation");
}
void check_desc(const spgl_context_desc* d) {
  if (!valid_record(d))
    throw std::invalid_argument("Invalid context descriptor size or ABI version");
}
void check_surface(const spgl_surface_desc* d) {
  if (!valid_record(d))
    throw std::invalid_argument("Invalid surface descriptor size or ABI version");
}
void check_occluder(const spgl_occluder_desc* d) {
  if (!valid_record(d))
    throw std::invalid_argument("Invalid occluder descriptor size or ABI version");
}
void check_device(const spgl_device_desc* d) {
  if (!valid_record(d))
    throw std::invalid_argument("Invalid device descriptor size or ABI version");
}
void check_resource(const spgl_resource_desc* d) {
  if (!valid_record(d))
    throw std::invalid_argument("Invalid resource descriptor size or ABI version");
}
void check_motion(const spgl_world_motion_desc* d) {
  if (!valid_record(d))
    throw std::invalid_argument("Invalid motion descriptor size or ABI version");
}
void check_world_metadata(const spgl_world_metadata_desc* d) {
  if (!valid_record(d))
    throw std::invalid_argument("Invalid world metadata descriptor size or ABI version");
}
void check_frame(const spgl_frame_desc* d) {
  if (!valid_record(d)) throw std::invalid_argument("Invalid frame descriptor size or ABI version");
}
void check_draw(const spgl_draw_desc* d) {
  if (!valid_record(d)) throw std::invalid_argument("Invalid draw descriptor size or ABI version");
}
Composition composition(spgl_composition_mode x) {
  switch (x) {
    case SPGL_COMPOSITION_OVER:
      return Composition::kOver;
    case SPGL_COMPOSITION_REPLACE:
      return Composition::kReplace;
    case SPGL_COMPOSITION_ADD:
      return Composition::kAdd;
    default:
      throw std::invalid_argument("Invalid composition enum");
  }
}
OutputMix output_mix(spgl_output_mix_mode x) {
  switch (x) {
    case SPGL_OUTPUT_EXCLUSIVE:
      return OutputMix::kExclusive;
    case SPGL_OUTPUT_NORMALIZED:
      return OutputMix::kNormalized;
    case SPGL_OUTPUT_ADDITIVE:
      return OutputMix::kAdditive;
    default:
      throw std::invalid_argument("Invalid output mix enum");
  }
}
void require_output_record(uint32_t size, uint32_t version, size_t required) {
  if (version != kAbi || size < required)
    throw std::invalid_argument("Output record size or ABI version is invalid");
}
void copy_str(char* dst, size_t cap, const std::string& s) {
  if (!cap) return;
  const size_t n = std::min(cap - 1, s.size());
  std::memcpy(dst, s.data(), n);
  dst[n] = '\0';
}
void fill_diag(spgl_diagnostic& o, const Diagnostic& d) {
  o.code = d.code == "stale-world"                ? SPGL_DIAGNOSTIC_STALE_WORLD
           : d.status == PlanStatus::kUnsupported ? SPGL_DIAGNOSTIC_UNSUPPORTED
           : d.status == PlanStatus::kNoPlanFound ? SPGL_DIAGNOSTIC_NO_PLAN_FOUND
           : d.status == PlanStatus::kInfeasible  ? SPGL_DIAGNOSTIC_PROVED_INFEASIBLE
           : d.status == PlanStatus::kInvalidated ? SPGL_DIAGNOSTIC_INVALIDATED
                                                  : SPGL_DIAGNOSTIC_NONE;
  o.severity = d.status == PlanStatus::kAccepted ? 0u : 1u;
  o.draw_id_hash = hash_id(d.object_id);
  o.device_id_hash = 0;
  o.resource_id_hash = 0;
  std::memset(o.message, 0, sizeof(o.message));
  copy_str(o.message, sizeof(o.message), d.code + ": " + d.message);
}
void init(spgl_output* p) {
  if (!p) throw std::invalid_argument("Output is null");
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  std::memset(reinterpret_cast<char*>(p) + 8, 0, sizeof(*p) - 8);
}
void init(spgl_aggregate* p) {
  if (!p) throw std::invalid_argument("Output is null");
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  std::memset(reinterpret_cast<char*>(p) + 8, 0, sizeof(*p) - 8);
}
void init(spgl_diagnostic* p) {
  if (!p) throw std::invalid_argument("Output is null");
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  std::memset(reinterpret_cast<char*>(p) + 8, 0, sizeof(*p) - 8);
}
void init(spgl_program_event* p) {
  if (!p) throw std::invalid_argument("Output is null");
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  std::memset(reinterpret_cast<char*>(p) + 8, 0, sizeof(*p) - 8);
}
}  // namespace

extern "C" {
uint32_t spgl_abi_version(void) { return SPATIALGL_ABI_VERSION; }
#define INIT_FN(name, type)                        \
  spgl_status name(type* p) {                      \
    try {                                          \
      if (!p) return SPGL_STATUS_INVALID_ARGUMENT; \
      std::memset(p, 0, sizeof(*p));               \
      p->size = sizeof(*p);                        \
      p->abi_version = kAbi;                       \
      return SPGL_STATUS_OK;                       \
    } catch (...) {                                \
      return SPGL_STATUS_INTERNAL;                 \
    }                                              \
  }
INIT_FN(spgl_surface_desc_init, spgl_surface_desc)
INIT_FN(spgl_occluder_desc_init, spgl_occluder_desc)
INIT_FN(spgl_world_motion_desc_init, spgl_world_motion_desc)
INIT_FN(spgl_world_metadata_desc_init, spgl_world_metadata_desc)
#undef INIT_FN
spgl_status spgl_context_desc_init(spgl_context_desc* p) {
  if (!p) return SPGL_STATUS_INVALID_ARGUMENT;
  std::memset(p, 0, sizeof(*p));
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  p->sample_spacing_m = 0.05;
  p->position_tolerance_m = 0;
  p->max_samples = 100000;
  p->output_mix = SPGL_OUTPUT_NORMALIZED;
  p->traversal = SPGL_TRAVERSAL_STABLE;
  p->presentation = SPGL_PRESENTATION_IMMEDIATE;
  p->planner_candidate_budget = 4096;
  return SPGL_STATUS_OK;
}
spgl_status spgl_device_desc_init(spgl_device_desc* p) {
  if (!p) return SPGL_STATUS_INVALID_ARGUMENT;
  std::memset(p, 0, sizeof(*p));
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  p->position = {0, 0, 0};
  p->target = {0, 0, -1};
  p->up = {0, 1, 0};
  p->fov_y_radians = 1.5707963267948966;
  p->near_m = 0.01;
  p->far_m = 100;
  p->refresh_hz = 30000;
  p->max_sample_dwell_s = 0.01;
  p->calibrated_gain = {1, 1, 1};
  p->sample_budget = 100000;
  p->raster_width = 640;
  p->raster_height = 480;
  return SPGL_STATUS_OK;
}
spgl_status spgl_resource_desc_init(spgl_resource_desc* p) {
  if (!p) return SPGL_STATUS_INVALID_ARGUMENT;
  std::memset(p, 0, sizeof(*p));
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  p->capacity = 1;
  return SPGL_STATUS_OK;
}
spgl_status spgl_frame_desc_init(spgl_frame_desc* p) {
  if (!p) return SPGL_STATUS_INVALID_ARGUMENT;
  std::memset(p, 0, sizeof(*p));
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  p->composition = SPGL_COMPOSITION_OVER;
  return SPGL_STATUS_OK;
}
spgl_status spgl_draw_desc_init(spgl_draw_desc* p) {
  if (!p) return SPGL_STATUS_INVALID_ARGUMENT;
  std::memset(p, 0, sizeof(*p));
  p->size = sizeof(*p);
  p->abi_version = kAbi;
  p->geometry = SPGL_GEOMETRY_POINT;
  p->coordinates = SPGL_COORDINATES_WORLD;
  p->color_linear = {1, 1, 1};
  p->opacity = 1;
  p->intensity = 1;
  return SPGL_STATUS_OK;
}
spgl_status spgl_output_init(spgl_output* p) {
  try {
    init(p);
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
spgl_status spgl_aggregate_init(spgl_aggregate* p) {
  try {
    init(p);
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
spgl_status spgl_diagnostic_init(spgl_diagnostic* p) {
  try {
    init(p);
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
spgl_status spgl_program_event_init(spgl_program_event* p) {
  try {
    init(p);
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
spgl_status spgl_receipt_init(spgl_receipt* p) {
  try {
    if (!p) return SPGL_STATUS_INVALID_ARGUMENT;
    p->size = sizeof(*p);
    p->abi_version = kAbi;
    std::memset(reinterpret_cast<char*>(p) + 8, 0, sizeof(*p) - 8);
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}

spgl_status spgl_context_create(const spgl_context_desc* d, spgl_context** out) {
  if (!out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = nullptr;
  spgl_context temp;
  auto s = call(&temp, [&] {
    check_desc(d);
    if (!std::isfinite(d->sample_spacing_m) || d->sample_spacing_m <= 0 ||
        !std::isfinite(d->position_tolerance_m) || d->position_tolerance_m < 0 ||
        d->max_samples == 0 || !std::isfinite(d->max_dark_gap_s) || d->max_dark_gap_s < 0 ||
        !std::isfinite(d->minimum_duty) || d->minimum_duty < 0 || d->minimum_duty > 1 ||
        !std::isfinite(d->requested_group_skew_s) || d->requested_group_skew_s < 0 ||
        d->planner_candidate_budget == 0 || d->require_sync > 1 ||
        d->allow_laser_fill_approximation > 1)
      throw std::invalid_argument("Invalid context options");
    if (d->position_tolerance_m > 0)
      throw StatusError(SPGL_STATUS_UNSUPPORTED, "Certified position-error bounds are not modeled");
    temp.options.pipeline.sample_spacing = d->sample_spacing_m;
    temp.options.pipeline.max_position_error = d->position_tolerance_m;
    temp.options.pipeline.max_samples = d->max_samples;
    temp.options.pipeline.output_mix = output_mix(d->output_mix);
    temp.options.pipeline.allow_laser_fill_approximation = d->allow_laser_fill_approximation;
    temp.options.traversal.max_dark_gap = d->max_dark_gap_s;
    temp.options.traversal.minimum_duty = d->minimum_duty;
    switch (d->traversal) {
      case SPGL_TRAVERSAL_STABLE:
        temp.options.traversal.order = Traversal::kStable;
        break;
      case SPGL_TRAVERSAL_PRIORITY:
        temp.options.traversal.order = Traversal::kPriority;
        break;
      case SPGL_TRAVERSAL_SHORTEST_PATH:
        temp.options.traversal.order = Traversal::kShortestPath;
        break;
      default:
        throw std::invalid_argument("Invalid traversal enum");
    }
    switch (d->presentation) {
      case SPGL_PRESENTATION_IMMEDIATE:
        temp.options.presentation.mode = Presentation::kImmediate;
        break;
      case SPGL_PRESENTATION_TIMED:
        temp.options.presentation.mode = Presentation::kTimed;
        break;
      case SPGL_PRESENTATION_SYNCHRONIZED_GROUP:
        temp.options.presentation.mode = Presentation::kSynchronizedGroup;
        break;
      default:
        throw std::invalid_argument("Invalid presentation enum");
    }
    temp.options.presentation.requested_group_skew = d->requested_group_skew_s;
    temp.options.presentation.require_sync = d->require_sync;
    temp.options.planner_candidate_budget = d->planner_candidate_budget;
    temp.world.source_time = 0;
    temp.world.valid_until = 1e300;
  });
  if (s != SPGL_STATUS_OK) return s;
  try {
    *out = new spgl_context(std::move(temp));
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_NO_MEMORY;
  }
}
void spgl_context_destroy(spgl_context* c) { delete c; }
const char* spgl_context_last_error(const spgl_context* c) {
  return c ? c->error.c_str() : nullptr;
}
spgl_status spgl_context_set_surface(spgl_context* c, const spgl_surface_desc* d) {
  return call(c, [&] {
    ensure_context(c);
    check_surface(d);
    Id name = id(d->id);
    if (!finite(d->origin) || !finite(d->u) || !finite(d->v))
      throw std::invalid_argument("Non-finite surface");
    const Vec3 u = vec(d->u), v = vec(d->v);
    const double uu = u[0] * u[0] + u[1] * u[1] + u[2] * u[2],
                 vv = v[0] * v[0] + v[1] * v[1] + v[2] * v[2],
                 uv = u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
    if (uu <= 0 || vv <= 0 || std::abs(uv) > 1e-6)
      throw std::invalid_argument("Surface axes must be nonzero and perpendicular");
    WorldSnapshot proposed = c->world;
    auto it = std::find_if(proposed.planes.begin(), proposed.planes.end(),
                           [&](auto& p) { return p.id == name; });
    Plane p{name, vec(d->origin), u, v};
    if (it == proposed.planes.end())
      proposed.planes.push_back(p);
    else
      *it = p;
    update_world(c, std::move(proposed), c->motion_by_id);
  });
}
spgl_status spgl_context_remove_surface(spgl_context* c, const char* name) {
  return call(c, [&] {
    ensure_context(c);
    Id n = id(name);
    WorldSnapshot proposed = c->world;
    auto& v = proposed.planes;
    auto i = std::find_if(v.begin(), v.end(), [&](auto& p) { return p.id == n; });
    if (i == v.end()) throw std::out_of_range("Unknown surface");
    v.erase(i);
    auto motions = c->motion_by_id;
    motions.erase(n);
    update_world(c, std::move(proposed), std::move(motions));
  });
}
spgl_status spgl_context_set_occluder(spgl_context* c, const spgl_occluder_desc* d) {
  return call(c, [&] {
    ensure_context(c);
    check_occluder(d);
    Id n = id(d->id);
    if (!finite(d->min) || !finite(d->max))
      throw std::invalid_argument("Non-finite occluder bounds");
    Vec3 mn = vec(d->min), mx = vec(d->max);
    for (int k = 0; k < 3; k++)
      if (mn[k] > mx[k]) throw std::invalid_argument("Inverted occluder bounds");
    Occluder x{n, mn, mx};
    WorldSnapshot proposed = c->world;
    auto i = std::find_if(proposed.occluders.begin(), proposed.occluders.end(),
                          [&](auto& o) { return o.id == n; });
    if (i == proposed.occluders.end())
      proposed.occluders.push_back(x);
    else
      *i = x;
    update_world(c, std::move(proposed), c->motion_by_id);
  });
}
spgl_status spgl_context_remove_occluder(spgl_context* c, const char* name) {
  return call(c, [&] {
    ensure_context(c);
    Id n = id(name);
    WorldSnapshot proposed = c->world;
    auto& v = proposed.occluders;
    auto i = std::find_if(v.begin(), v.end(), [&](auto& o) { return o.id == n; });
    if (i == v.end()) throw std::out_of_range("Unknown occluder");
    v.erase(i);
    update_world(c, std::move(proposed), c->motion_by_id);
  });
}
spgl_status spgl_context_set_resource(spgl_context* c, const spgl_resource_desc* d) {
  return call(c, [&] {
    check_mutable_config(c);
    check_resource(d);
    Id name = id(d->id);
    if (!std::isfinite(d->capacity) || d->capacity <= 0)
      throw std::invalid_argument("Invalid resource capacity");
    if (std::abs(d->capacity - 1.0) > 1e-9)
      throw StatusError(SPGL_STATUS_UNSUPPORTED,
                        "Only unit resource capacity is supported by this simulator");
    auto i = std::find_if(c->rig.resources.begin(), c->rig.resources.end(),
                          [&](auto& r) { return r.id == name; });
    if (i == c->rig.resources.end())
      c->rig.resources.push_back({name, d->capacity});
    else
      i->capacity = d->capacity;
  });
}
spgl_status spgl_context_remove_resource(spgl_context* c, const char* name) {
  return call(c, [&] {
    check_mutable_config(c);
    Id n = id(name);
    if (std::any_of(c->rig.devices.begin(), c->rig.devices.end(), [&](auto& d) {
          return std::find(d.resource_ids.begin(), d.resource_ids.end(), n) != d.resource_ids.end();
        }))
      throw std::logic_error("Resource is still referenced by a device");
    auto& v = c->rig.resources;
    auto i = std::find_if(v.begin(), v.end(), [&](auto& r) { return r.id == n; });
    if (i == v.end()) throw std::out_of_range("Unknown resource");
    v.erase(i);
  });
}
spgl_status spgl_context_set_device(spgl_context* c, const spgl_device_desc* d) {
  return call(c, [&] {
    check_mutable_config(c);
    check_device(d);
    DeviceProfile x;
    x.id = id(d->id);
    const std::string profile = id(d->profile);
    if (profile == "fixed-projector")
      x.kind = DeviceKind::kFixedRaster;
    else if (profile == "steerable-projector")
      x.kind = DeviceKind::kSteerableRaster;
    else if (profile == "galvo")
      x.kind = DeviceKind::kGalvo;
    else
      throw std::invalid_argument("Unknown device profile");
    if (!finite(d->position) || !finite(d->target) || !finite(d->up) ||
        !finite(d->calibrated_gain) || !std::isfinite(d->fov_y_radians) || d->fov_y_radians <= 0 ||
        d->fov_y_radians >= 3.14159265358979323846 || !std::isfinite(d->near_m) || d->near_m <= 0 ||
        !std::isfinite(d->far_m) || d->far_m <= d->near_m || !std::isfinite(d->refresh_hz) ||
        d->refresh_hz <= 0 || !std::isfinite(d->latency_s) || d->latency_s < 0 ||
        !std::isfinite(d->max_angular_speed_rad_s) || d->max_angular_speed_rad_s < 0 ||
        !std::isfinite(d->settling_s) || d->settling_s < 0 || !std::isfinite(d->clock_offset_s) ||
        !std::isfinite(d->clock_drift) || !std::isfinite(d->clock_uncertainty_s) ||
        d->clock_uncertainty_s < 0 || !std::isfinite(d->max_sample_dwell_s) ||
        d->max_sample_dwell_s <= 0 || d->sample_budget == 0 || d->sample_budget > 1000000 ||
        d->raster_width < 2 || d->raster_height < 2)
      throw std::invalid_argument("Invalid or non-finite device field");
    for (double gain : rgb(d->calibrated_gain))
      if (gain < 0) throw std::invalid_argument("Calibrated gain must be nonnegative");
    if (profile == "steerable-projector" && d->max_angular_speed_rad_s <= 0)
      throw std::invalid_argument("Steerable projector requires positive angular speed");
    x.position = vec(d->position);
    x.target = vec(d->target);
    x.up = vec(d->up);
    x.fov_y = d->fov_y_radians;
    x.near_plane = d->near_m;
    x.far_plane = d->far_m;
    x.width = d->raster_width;
    x.height = d->raster_height;
    x.sample_rate_hz = d->refresh_hz;
    x.latency = d->latency_s;
    x.max_angular_speed = d->max_angular_speed_rad_s;
    x.settling_time = d->settling_s;
    x.clock_offset = d->clock_offset_s;
    x.clock_drift = d->clock_drift;
    x.clock_uncertainty = d->clock_uncertainty_s;
    x.max_sample_dwell = d->max_sample_dwell_s;
    x.calibrated_gain = rgb(d->calibrated_gain);
    x.sample_budget = d->sample_budget;
    if (d->resource_count > 4096 || (d->resource_count && !d->resource_ids))
      throw std::invalid_argument("Resource ID array is invalid");
    for (uint32_t i = 0; i < d->resource_count; i++) {
      Id rid = id(d->resource_ids[i]);
      if (std::find(x.resource_ids.begin(), x.resource_ids.end(), rid) != x.resource_ids.end())
        throw std::invalid_argument("Duplicate device resource ID");
      if (std::none_of(c->rig.resources.begin(), c->rig.resources.end(),
                       [&](auto& q) { return q.id == rid; }))
        throw std::invalid_argument("Device references an undeclared resource");
      x.resource_ids.push_back(std::move(rid));
    }
    auto it = std::find_if(c->rig.devices.begin(), c->rig.devices.end(),
                           [&](auto& q) { return q.id == x.id; });
    if (it == c->rig.devices.end())
      c->rig.devices.push_back(x);
    else
      *it = x;
    c->configured_devices[x.id] = true;
    c->availability[x.id] = Availability::kAvailable;
  });
}
spgl_status spgl_context_remove_device(spgl_context* c, const char* name) {
  return call(c, [&] {
    check_mutable_config(c);
    Id n = id(name);
    auto& v = c->rig.devices;
    auto i = std::find_if(v.begin(), v.end(), [&](auto& d) { return d.id == n; });
    if (i == v.end()) throw std::out_of_range("Unknown device");
    v.erase(i);
    c->availability.erase(n);
  });
}
spgl_status spgl_context_set_device_availability(spgl_context* c, const char* name,
                                                 spgl_availability a) {
  return call(c, [&] {
    ensure_context(c);
    if (a < SPGL_AVAILABILITY_AVAILABLE || a > SPGL_AVAILABILITY_HANDING_OFF)
      throw std::invalid_argument("Invalid availability enum");
    Id n = id(name);
    Availability v = static_cast<Availability>(a);
    if (std::none_of(c->rig.devices.begin(), c->rig.devices.end(),
                     [&](auto& d) { return d.id == n; }))
      throw std::out_of_range("Unknown device");
    c->availability[n] = v;
    if (c->runtime) c->runtime->set_availability(n, v);
  });
}
spgl_status spgl_context_set_surface_motion(spgl_context* c, const spgl_world_motion_desc* d) {
  return call(c, [&] {
    ensure_context(c);
    check_motion(d);
    Id n = id(d->surface_id);
    if (!finite(d->linear_velocity_m_s) || !finite(d->angular_velocity_rad_s) ||
        !std::isfinite(d->source_time_s) || !std::isfinite(d->valid_until_s) ||
        d->valid_until_s < d->source_time_s)
      throw std::invalid_argument("Invalid motion");
    auto i = std::find_if(c->world.planes.begin(), c->world.planes.end(),
                          [&](auto& p) { return p.id == n; });
    if (i == c->world.planes.end()) throw std::out_of_range("Unknown surface");
    auto motions = c->motion_by_id;
    motions[n] = {vec(d->linear_velocity_m_s), vec(d->angular_velocity_rad_s)};
    WorldSnapshot proposed = c->world;
    proposed.source_time = d->source_time_s;
    proposed.valid_until = d->valid_until_s;
    proposed.world_revision = std::max(proposed.world_revision + 1, d->revision);
    proposed.calibration_revision =
        std::max(proposed.calibration_revision, d->calibration_revision);
    update_world(c, std::move(proposed), std::move(motions));
  });
}
spgl_status spgl_context_update_world_metadata(spgl_context* c, const spgl_world_metadata_desc* d) {
  return call(c, [&] {
    ensure_context(c);
    check_world_metadata(d);
    if (!std::isfinite(d->source_time_s) || !std::isfinite(d->valid_until_s) ||
        d->valid_until_s < d->source_time_s)
      throw std::invalid_argument("Invalid world metadata validity interval");
    if (c->world.world_revision == UINT64_MAX)
      throw StatusError(SPGL_STATUS_OUT_OF_RANGE, "World revision counter is exhausted");
    WorldSnapshot proposed = c->world;
    const uint64_t next_revision = c->world.world_revision + 1;
    if (d->world_revision != 0 && d->world_revision < next_revision)
      throw std::invalid_argument("World revision must strictly advance");
    if (d->calibration_revision != 0 && d->calibration_revision < c->world.calibration_revision)
      throw std::invalid_argument("Calibration revision cannot decrease");
    proposed.source_time = d->source_time_s;
    proposed.valid_until = d->valid_until_s;
    proposed.world_revision = d->world_revision == 0 ? next_revision : d->world_revision;
    proposed.calibration_revision =
        d->calibration_revision == 0 ? c->world.calibration_revision : d->calibration_revision;
    update_world(c, std::move(proposed), c->motion_by_id);
  });
}
spgl_status spgl_context_world_revision(const spgl_context* c, uint64_t* out) {
  if (!c || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = c->world.world_revision;
  return SPGL_STATUS_OK;
}
spgl_status spgl_context_calibration_revision(const spgl_context* c, uint64_t* out) {
  if (!c || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = c->world.calibration_revision;
  return SPGL_STATUS_OK;
}

spgl_status spgl_frame_create(spgl_context* c, const spgl_frame_desc* d, spgl_frame** out) {
  if (!out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = nullptr;
  return call(c, [&] {
    ensure_context(c);
    check_frame(d);
    auto f = std::make_unique<spgl_frame>();
    f->owner = c;
    f->value.id = id(d->id);
    f->value.app_id = d->app_id ? id(d->app_id, true) : Id{};
    f->value.present_at = d->present_at_s;
    f->value.expires_at = d->expires_at_s;
    f->value.world_revision = d->world_revision;
    f->value.calibration_revision = d->calibration_revision;
    f->value.composition = composition(d->composition);
    if (!std::isfinite(d->present_at_s) || !std::isfinite(d->expires_at_s) || d->present_at_s < 0 ||
        d->expires_at_s <= d->present_at_s)
      throw std::invalid_argument("Invalid frame interval");
    *out = f.release();
  });
}
void spgl_frame_destroy(spgl_frame* f) { delete f; }
spgl_status spgl_frame_draw_point(spgl_frame* f, const spgl_draw_desc* d, spgl_vec3 p) {
  return call(f ? f->owner : nullptr, [&] {
    if (!f) throw std::invalid_argument("Frame is null");
    check_draw(d);
    if (!finite(p) || !valid_color(d->color_linear) || !std::isfinite(d->opacity) ||
        d->opacity < 0 || d->opacity > 1 || !std::isfinite(d->intensity) || d->intensity < 0 ||
        d->closed > 1)
      throw std::invalid_argument("Invalid draw value");
    DrawCall x;
    x.draw_id = id(d->draw_id);
    x.target.surface_id = id(d->surface_id);
    x.target.coordinates = d->coordinates == SPGL_COORDINATES_WORLD ? CoordinateSpace::kWorld
                           : d->coordinates == SPGL_COORDINATES_SURFACE_LOCAL
                               ? CoordinateSpace::kSurfaceLocal
                               : throw std::invalid_argument("Invalid coordinate enum");
    if (d->geometry != SPGL_GEOMETRY_POINT)
      throw std::invalid_argument("Draw descriptor geometry does not match operation");
    x.target.geometry = GeometryKind::kPoint;
    x.target.origin = vec(p);
    x.linear_rgb = rgb(d->color_linear);
    x.opacity = d->opacity;
    x.intensity = d->intensity;
    x.priority = d->priority;
    x.closed = d->closed;
    f->value.draws.push_back(std::move(x));
  });
}
spgl_status spgl_frame_draw_polyline(spgl_frame* f, const spgl_draw_desc* d, const spgl_vec3* v,
                                     uint64_t n) {
  return call(f ? f->owner : nullptr, [&] {
    if (!f) throw std::invalid_argument("Frame is null");
    check_draw(d);
    if (d->geometry != SPGL_GEOMETRY_POLYLINE || !v || n < 2 ||
        n > f->owner->options.pipeline.max_samples || !valid_color(d->color_linear) ||
        !std::isfinite(d->opacity) || d->opacity < 0 || d->opacity > 1 ||
        !std::isfinite(d->intensity) || d->intensity < 0 || d->closed > 1)
      throw std::invalid_argument("Invalid polyline");
    DrawCall x;
    x.draw_id = id(d->draw_id);
    x.target.surface_id = id(d->surface_id);
    x.target.coordinates = d->coordinates == SPGL_COORDINATES_WORLD ? CoordinateSpace::kWorld
                           : d->coordinates == SPGL_COORDINATES_SURFACE_LOCAL
                               ? CoordinateSpace::kSurfaceLocal
                               : throw std::invalid_argument("Invalid coordinate enum");
    x.target.geometry = GeometryKind::kPolyline;
    x.linear_rgb = rgb(d->color_linear);
    x.opacity = d->opacity;
    x.intensity = d->intensity;
    x.priority = d->priority;
    x.closed = d->closed;
    for (uint64_t i = 0; i < n; i++) {
      if (!finite(v[i])) throw std::invalid_argument("Non-finite vertex");
      x.target.vertices.push_back(vec(v[i]));
    }
    f->value.draws.push_back(std::move(x));
  });
}
spgl_status spgl_frame_draw_patch(spgl_frame* f, const spgl_draw_desc* d, spgl_vec3 o, spgl_vec3 u,
                                  spgl_vec3 v) {
  return call(f ? f->owner : nullptr, [&] {
    if (!f) throw std::invalid_argument("Frame is null");
    check_draw(d);
    if (d->geometry != SPGL_GEOMETRY_PATCH || !finite(o) || !finite(u) || !finite(v) ||
        !valid_color(d->color_linear) || !std::isfinite(d->opacity) || d->opacity < 0 ||
        d->opacity > 1 || !std::isfinite(d->intensity) || d->intensity < 0 || d->closed > 1)
      throw std::invalid_argument("Invalid patch");
    DrawCall x;
    x.draw_id = id(d->draw_id);
    x.target.surface_id = id(d->surface_id);
    x.target.coordinates = d->coordinates == SPGL_COORDINATES_WORLD ? CoordinateSpace::kWorld
                           : d->coordinates == SPGL_COORDINATES_SURFACE_LOCAL
                               ? CoordinateSpace::kSurfaceLocal
                               : throw std::invalid_argument("Invalid coordinate enum");
    x.target.geometry = GeometryKind::kPatch;
    x.target.origin = vec(o);
    x.target.u = vec(u);
    x.target.v = vec(v);
    x.linear_rgb = rgb(d->color_linear);
    x.opacity = d->opacity;
    x.intensity = d->intensity;
    x.priority = d->priority;
    x.closed = d->closed;
    f->value.draws.push_back(std::move(x));
  });
}
spgl_status spgl_context_submit(spgl_context* c, const spgl_frame* f, uint64_t* ticket) {
  return call(c, [&] {
    ensure_context(c);
    if (!f || f->owner != c || !ticket)
      throw std::invalid_argument("Frame/context mismatch or null ticket");
    ensure_runtime(c);
    auto errors = c->runtime->submit(f->value);
    if (!errors.empty()) {
      const auto& e = errors.front();
      const spgl_status status = e.status == PlanStatus::kUnsupported
                                     ? SPGL_STATUS_UNSUPPORTED
                                     : SPGL_STATUS_INVALID_ARGUMENT;
      throw StatusError(status, e.code + ": " + e.message);
    }
    *ticket = c->next_ticket++;
    c->tickets[f->value.id] = *ticket;
  });
}
spgl_status spgl_context_cancel(spgl_context* c, uint64_t ticket) {
  return call(c, [&] {
    ensure_context(c);
    auto it = std::find_if(c->tickets.begin(), c->tickets.end(),
                           [&](auto& p) { return p.second == ticket; });
    if (it == c->tickets.end()) throw std::out_of_range("Unknown ticket");
    ensure_runtime(c);
    c->runtime->cancel(it->first);
  });
}
spgl_status spgl_context_advance(spgl_context* c, double t) {
  return call(c, [&] {
    ensure_context(c);
    if (!std::isfinite(t) || t < c->now)
      throw std::invalid_argument("Advance time must be finite and monotonic");
    ensure_runtime(c);
    c->runtime->advance(t);
    c->now = t;
  });
}
spgl_status spgl_context_snapshot(spgl_context* c, spgl_snapshot** out) {
  if (!out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = nullptr;
  return call(c, [&] {
    ensure_context(c);
    ensure_runtime(c);
    auto s = std::make_unique<spgl_snapshot>();
    s->value = c->runtime->snapshot();
    *out = s.release();
  });
}
void spgl_snapshot_destroy(spgl_snapshot* s) { delete s; }
spgl_status spgl_snapshot_time_s(const spgl_snapshot* s, double* out) {
  if (!s || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = s->value.time;
  return SPGL_STATUS_OK;
}
spgl_status spgl_snapshot_world_revision(const spgl_snapshot* s, uint64_t* out) {
  if (!s || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = s->value.world_revision;
  return SPGL_STATUS_OK;
}
spgl_status spgl_snapshot_calibration_revision(const spgl_snapshot* s, uint64_t* out) {
  if (!s || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = s->value.calibration_revision;
  return SPGL_STATUS_OK;
}
spgl_status spgl_snapshot_output_count(const spgl_snapshot* s, uint64_t* out) {
  if (!s || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = s->value.contributions.size();
  return SPGL_STATUS_OK;
}
spgl_status spgl_snapshot_output_at(const spgl_snapshot* s, uint64_t i, spgl_output* o) {
  if (!s || !o) return SPGL_STATUS_INVALID_ARGUMENT;
  try {
    require_output_record(o->size, o->abi_version, sizeof(*o));
    if (i >= s->value.contributions.size()) return SPGL_STATUS_OUT_OF_RANGE;
    o->size = sizeof(*o);
    o->abi_version = kAbi;
    std::memset(reinterpret_cast<char*>(o) + 8, 0, sizeof(*o) - 8);
    const auto& x = s->value.contributions[i];
    o->kind = SPGL_OUTPUT_RASTER;
    o->provenance = static_cast<uint32_t>(x.provenance) + 1;
    o->output_id = i + 1;
    o->draw_id_hash = hash_id(x.draw_id);
    o->device_id_hash = hash_id(x.device_id);
    o->source_frame_id_hash = hash_id(x.display_list_id);
    o->world_revision = s->value.world_revision;
    o->calibration_revision = s->value.calibration_revision;
    o->duty_fraction = x.duty_fraction.value_or(std::numeric_limits<double>::quiet_NaN());
    o->time_s = s->value.time;
    o->age_s = 0;
    o->position_m = cvec(x.position);
    o->contribution_linear = crgb({x.linear_rgb[0] * x.intensity, x.linear_rgb[1] * x.intensity,
                                   x.linear_rgb[2] * x.intensity});
    o->flags = x.duty_fraction ? 0 : SPGL_OUTPUT_DUTY_UNAVAILABLE;
    for (const auto& p : s->value.programs)
      if (p.device_id == x.device_id && p.display_list_id == x.display_list_id) {
        o->kind = p.device_kind == DeviceKind::kGalvo             ? SPGL_OUTPUT_SCAN
                  : p.device_kind == DeviceKind::kSteerableRaster ? SPGL_OUTPUT_STEERING
                                                                  : SPGL_OUTPUT_RASTER;
        o->source_frame_id_hash = hash_id(p.display_list_id);
        o->world_revision = p.world_revision;
        o->calibration_revision = p.calibration_revision;
        o->time_s = p.starts_at;
        o->age_s = std::max(0.0, s->value.time - p.issued_at);
        break;
      }
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
spgl_status spgl_snapshot_aggregate_count(const spgl_snapshot* s, uint64_t* out) {
  if (!s || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = s->value.targets.size();
  return SPGL_STATUS_OK;
}
spgl_status spgl_snapshot_aggregate_at(const spgl_snapshot* s, uint64_t i, spgl_aggregate* o) {
  if (!s || !o) return SPGL_STATUS_INVALID_ARGUMENT;
  try {
    require_output_record(o->size, o->abi_version, sizeof(*o));
    if (i >= s->value.targets.size()) return SPGL_STATUS_OUT_OF_RANGE;
    init(o);
    const auto& x = s->value.targets[i];
    o->draw_id_hash = hash_id(x.draw_id);
    o->light_linear = crgb(x.aggregate_linear_rgb);
    o->position_error_m = x.position_error.value_or(std::numeric_limits<double>::quiet_NaN());
    o->age_s = x.age;
    o->duty_fraction = x.duty.value_or(std::numeric_limits<double>::quiet_NaN());
    o->max_dark_gap_s = x.max_dark_gap.value_or(std::numeric_limits<double>::quiet_NaN());
    o->provenance = static_cast<uint32_t>(x.provenance) + 1;
    o->flags = (x.position_error ? 0 : SPGL_AGGREGATE_POSITION_ERROR_UNAVAILABLE) |
               (x.duty ? 0 : SPGL_AGGREGATE_DUTY_UNAVAILABLE) |
               (x.max_dark_gap ? 0 : SPGL_AGGREGATE_DARK_GAP_UNAVAILABLE);
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
spgl_status spgl_snapshot_diagnostic_count(const spgl_snapshot* s, uint64_t* out) {
  if (!s || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = s->value.diagnostics.size();
  return SPGL_STATUS_OK;
}
spgl_status spgl_snapshot_diagnostic_at(const spgl_snapshot* s, uint64_t i, spgl_diagnostic* o) {
  if (!s || !o) return SPGL_STATUS_INVALID_ARGUMENT;
  try {
    require_output_record(o->size, o->abi_version, sizeof(*o));
    if (i >= s->value.diagnostics.size()) return SPGL_STATUS_OUT_OF_RANGE;
    init(o);
    fill_diag(*o, s->value.diagnostics[i]);
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
spgl_status spgl_snapshot_program_event_count(const spgl_snapshot* s, uint64_t* out) {
  if (!s || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  uint64_t n = 0;
  for (const auto& p : s->value.programs) n += p.events.size();
  *out = n;
  return SPGL_STATUS_OK;
}
spgl_status spgl_snapshot_program_event_at(const spgl_snapshot* s, uint64_t i,
                                           spgl_program_event* o) {
  if (!s || !o) return SPGL_STATUS_INVALID_ARGUMENT;
  try {
    require_output_record(o->size, o->abi_version, sizeof(*o));
    init(o);
    for (const auto& p : s->value.programs)
      for (const auto& e : p.events) {
        if (i-- == 0) {
          o->kind = static_cast<uint32_t>(e.kind) + 1;
          o->provenance = SPGL_PROVENANCE_PREDICTED;
          o->generation = p.generation;
          o->device_id_hash = hash_id(p.device_id);
          o->draw_id_hash = hash_id(e.draw_id);
          o->source_list_id_hash = hash_id(e.display_list_id);
          o->world_revision = p.world_revision;
          o->calibration_revision = p.calibration_revision;
          o->time_s = e.time;
          o->position_m = cvec(e.position);
          o->color_linear = crgb(e.linear_rgb);
          o->dwell_s = e.dwell;
          return SPGL_STATUS_OK;
        }
      }
    return SPGL_STATUS_OUT_OF_RANGE;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
spgl_status spgl_snapshot_receipt_count(const spgl_snapshot* s, uint64_t* out) {
  if (!s || !out) return SPGL_STATUS_INVALID_ARGUMENT;
  *out = s->value.receipts.size();
  return SPGL_STATUS_OK;
}
spgl_status spgl_snapshot_receipt_at(const spgl_snapshot* s, uint64_t i, spgl_receipt* o) {
  if (!s || !o) return SPGL_STATUS_INVALID_ARGUMENT;
  try {
    require_output_record(o->size, o->abi_version, sizeof(*o));
    if (i >= s->value.receipts.size()) return SPGL_STATUS_OUT_OF_RANGE;
    o->size = sizeof(*o);
    o->abi_version = kAbi;
    std::memset(reinterpret_cast<char*>(o) + 8, 0, sizeof(*o) - 8);
    const auto& r = s->value.receipts[i];
    o->device_id_hash = hash_id(r.device_id);
    o->generation = r.generation;
    o->availability = static_cast<uint32_t>(r.availability);
    o->provenance = static_cast<uint32_t>(r.provenance) + 1;
    o->accepted_at_s = r.accepted_at;
    o->timing_uncertainty_s = r.timing_uncertainty;
    return SPGL_STATUS_OK;
  } catch (...) {
    return SPGL_STATUS_INVALID_ARGUMENT;
  }
}
}
