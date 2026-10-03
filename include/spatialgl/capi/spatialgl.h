#ifndef SPATIALGL_CAPI_SPATIALGL_H_
#define SPATIALGL_CAPI_SPATIALGL_H_

/* SpatialGL C ABI v1. This header is valid C11 and C++. */
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(SPATIALGL_CAPI_BUILD)
#define SPATIALGL_API __declspec(dllexport)
#else
#define SPATIALGL_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define SPATIALGL_API __attribute__((visibility("default")))
#else
#define SPATIALGL_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SPATIALGL_ABI_VERSION_MAJOR 1u
#define SPATIALGL_ABI_VERSION_MINOR 0u
#define SPATIALGL_ABI_VERSION ((SPATIALGL_ABI_VERSION_MAJOR << 16) | SPATIALGL_ABI_VERSION_MINOR)

typedef struct spgl_context spgl_context;
typedef struct spgl_frame spgl_frame;
typedef struct spgl_snapshot spgl_snapshot;

typedef int32_t spgl_status;
enum {
  SPGL_STATUS_OK = 0,
  SPGL_STATUS_INVALID_ARGUMENT = 1,
  SPGL_STATUS_INVALID_STATE = 2,
  SPGL_STATUS_OUT_OF_RANGE = 3,
  SPGL_STATUS_UNSUPPORTED = 4,
  SPGL_STATUS_NO_MEMORY = 5,
  SPGL_STATUS_INTERNAL = 6
};

typedef int32_t spgl_geometry_kind;
enum { SPGL_GEOMETRY_POINT = 1, SPGL_GEOMETRY_POLYLINE = 2, SPGL_GEOMETRY_PATCH = 3 };

typedef int32_t spgl_coordinate_space;
enum { SPGL_COORDINATES_WORLD = 1, SPGL_COORDINATES_SURFACE_LOCAL = 2 };

typedef int32_t spgl_composition_mode;
enum { SPGL_COMPOSITION_OVER = 1, SPGL_COMPOSITION_REPLACE = 2, SPGL_COMPOSITION_ADD = 3 };

typedef int32_t spgl_output_mix_mode;
enum { SPGL_OUTPUT_EXCLUSIVE = 1, SPGL_OUTPUT_NORMALIZED = 2, SPGL_OUTPUT_ADDITIVE = 3 };

typedef int32_t spgl_presentation_state;
enum {
  SPGL_PRESENTATION_IMMEDIATE = 1,
  SPGL_PRESENTATION_TIMED = 2,
  SPGL_PRESENTATION_SYNCHRONIZED_GROUP = 3
};

typedef int32_t spgl_traversal_state;
enum { SPGL_TRAVERSAL_STABLE = 1, SPGL_TRAVERSAL_PRIORITY = 2, SPGL_TRAVERSAL_SHORTEST_PATH = 3 };

typedef int32_t spgl_output_kind;
enum { SPGL_OUTPUT_RASTER = 1, SPGL_OUTPUT_SCAN = 2, SPGL_OUTPUT_STEERING = 3 };

typedef int32_t spgl_provenance;
enum { SPGL_PROVENANCE_SIMULATED = 1, SPGL_PROVENANCE_PREDICTED = 2, SPGL_PROVENANCE_MEASURED = 3 };

typedef int32_t spgl_diagnostic_code;
enum {
  SPGL_DIAGNOSTIC_NONE = 0,
  SPGL_DIAGNOSTIC_UNSUPPORTED = 1,
  SPGL_DIAGNOSTIC_NO_PLAN_FOUND = 2,
  SPGL_DIAGNOSTIC_PROVED_INFEASIBLE = 3,
  SPGL_DIAGNOSTIC_INVALIDATED = 4,
  SPGL_DIAGNOSTIC_STALE_WORLD = 5,
  SPGL_DIAGNOSTIC_RESOURCE_CONFLICT = 6
};

typedef int32_t spgl_event_kind;
enum {
  SPGL_EVENT_BLANK = 1,
  SPGL_EVENT_EMIT = 2,
  SPGL_EVENT_DWELL = 3,
  SPGL_EVENT_POSE = 4,
  SPGL_EVENT_TRIGGER = 5
};

typedef int32_t spgl_availability;
enum {
  SPGL_AVAILABILITY_AVAILABLE = 0,
  SPGL_AVAILABILITY_UNAVAILABLE = 1,
  SPGL_AVAILABILITY_FAILED = 2,
  SPGL_AVAILABILITY_HANDING_OFF = 3
};

typedef struct spgl_vec3 {
  double x, y, z;
} spgl_vec3;
typedef struct spgl_rgb {
  double r, g, b;
} spgl_rgb;

/* Every extensible input/output record starts with size and abi_version.
 * Initialize with its _init function before setting fields. */
typedef struct spgl_context_desc {
  uint32_t size;
  uint32_t abi_version;
  double sample_spacing_m;
  double position_tolerance_m; /* must be 0; positive certified bounds are unsupported */
  uint32_t max_samples;
  uint32_t output_mix;
  uint32_t traversal;
  uint32_t presentation;
  double max_dark_gap_s;
  double minimum_duty;
  double requested_group_skew_s;
  uint32_t require_sync;
  uint32_t allow_laser_fill_approximation;
  uint32_t planner_candidate_budget;
  uint32_t reserved;
} spgl_context_desc;

typedef struct spgl_surface_desc {
  uint32_t size;
  uint32_t abi_version;
  const char* id;
  spgl_vec3 origin;
  spgl_vec3 u;
  spgl_vec3 v;
} spgl_surface_desc;

typedef struct spgl_occluder_desc {
  uint32_t size;
  uint32_t abi_version;
  const char* id;
  spgl_vec3 min;
  spgl_vec3 max;
} spgl_occluder_desc;

typedef struct spgl_device_desc {
  uint32_t size;
  uint32_t abi_version;
  const char* id;
  const char* profile; /* "fixed-projector", "steerable-projector", or "galvo" */
  spgl_vec3 position;
  spgl_vec3 target;
  spgl_vec3 up;
  double fov_y_radians;
  double near_m;
  double far_m;
  double refresh_hz;
  double latency_s;
  double max_angular_speed_rad_s;
  double settling_s;
  double clock_offset_s;
  double clock_drift;
  double clock_uncertainty_s;
  double max_sample_dwell_s;
  spgl_rgb calibrated_gain;
  uint32_t sample_budget;
  uint32_t raster_width;
  uint32_t raster_height;
  uint32_t resource_count;
  const char* const* resource_ids;
} spgl_device_desc;

typedef struct spgl_resource_desc {
  uint32_t size;
  uint32_t abi_version;
  const char* id;
  double capacity;
} spgl_resource_desc;

typedef struct spgl_world_motion_desc {
  uint32_t size;
  uint32_t abi_version;
  const char* surface_id;
  spgl_vec3 linear_velocity_m_s;
  double source_time_s;
  double valid_until_s;
  uint64_t revision;
  uint64_t calibration_revision;
  spgl_vec3 angular_velocity_rad_s;
} spgl_world_motion_desc;

typedef struct spgl_world_metadata_desc {
  uint32_t size;
  uint32_t abi_version;
  double source_time_s;
  double valid_until_s;
  uint64_t world_revision;
  uint64_t calibration_revision;
} spgl_world_metadata_desc;

typedef struct spgl_frame_desc {
  uint32_t size;
  uint32_t abi_version;
  const char* id;
  const char* app_id; /* optional; null means no app ID */
  double present_at_s;
  double expires_at_s;
  uint64_t world_revision;
  uint64_t calibration_revision;
  spgl_composition_mode composition;
} spgl_frame_desc;

typedef struct spgl_draw_desc {
  uint32_t size;
  uint32_t abi_version;
  const char* draw_id;
  const char* surface_id; /* required for all v1 surface targets */
  spgl_geometry_kind geometry;
  spgl_coordinate_space coordinates;
  spgl_rgb color_linear;
  double opacity;
  double intensity;
  int32_t priority;
  uint8_t closed;
  uint8_t reserved[3];
} spgl_draw_desc;

typedef struct spgl_output {
  uint32_t size;
  uint32_t abi_version;
  uint32_t kind;
  uint32_t provenance;
  uint64_t output_id;
  uint64_t draw_id_hash;
  uint64_t device_id_hash;
  uint64_t source_frame_id_hash;
  uint64_t world_revision;
  uint64_t calibration_revision;
  double duty_fraction;
  double time_s;
  double age_s;
  spgl_vec3 position_m;
  spgl_rgb contribution_linear;
  uint32_t flags;
  uint32_t reserved;
} spgl_output;
enum { SPGL_OUTPUT_DUTY_UNAVAILABLE = 1u << 0 };

typedef struct spgl_aggregate {
  uint32_t size;
  uint32_t abi_version;
  uint64_t draw_id_hash;
  spgl_rgb light_linear;
  double position_error_m;
  double age_s;
  double duty_fraction;
  double max_dark_gap_s;
  uint32_t provenance;
  uint32_t flags;
} spgl_aggregate;
enum {
  SPGL_AGGREGATE_POSITION_ERROR_UNAVAILABLE = 1u << 0,
  SPGL_AGGREGATE_DUTY_UNAVAILABLE = 1u << 1,
  SPGL_AGGREGATE_DARK_GAP_UNAVAILABLE = 1u << 2
};

typedef struct spgl_diagnostic {
  uint32_t size;
  uint32_t abi_version;
  uint32_t code;
  uint32_t severity;
  uint64_t draw_id_hash;
  uint64_t device_id_hash;
  uint64_t resource_id_hash;
  char message[192];
} spgl_diagnostic;

typedef struct spgl_program_event {
  uint32_t size;
  uint32_t abi_version;
  uint32_t kind;
  uint32_t provenance;
  uint64_t generation;
  uint64_t device_id_hash;
  uint64_t draw_id_hash;
  uint64_t source_list_id_hash;
  uint64_t world_revision;
  uint64_t calibration_revision;
  double time_s;
  spgl_vec3 position_m;
  spgl_rgb color_linear;
  double dwell_s;
} spgl_program_event;

typedef struct spgl_receipt {
  uint32_t size;
  uint32_t abi_version;
  uint64_t device_id_hash;
  uint64_t generation;
  uint32_t availability;
  uint32_t provenance;
  double accepted_at_s;
  double timing_uncertainty_s;
} spgl_receipt;

SPATIALGL_API uint32_t spgl_abi_version(void);
SPATIALGL_API spgl_status spgl_context_desc_init(spgl_context_desc* desc);
SPATIALGL_API spgl_status spgl_surface_desc_init(spgl_surface_desc* desc);
SPATIALGL_API spgl_status spgl_occluder_desc_init(spgl_occluder_desc* desc);
SPATIALGL_API spgl_status spgl_device_desc_init(spgl_device_desc* desc);
SPATIALGL_API spgl_status spgl_resource_desc_init(spgl_resource_desc* desc);
SPATIALGL_API spgl_status spgl_world_motion_desc_init(spgl_world_motion_desc* desc);
SPATIALGL_API spgl_status spgl_world_metadata_desc_init(spgl_world_metadata_desc* desc);
SPATIALGL_API spgl_status spgl_frame_desc_init(spgl_frame_desc* desc);
SPATIALGL_API spgl_status spgl_draw_desc_init(spgl_draw_desc* desc);
SPATIALGL_API spgl_status spgl_output_init(spgl_output* value);
SPATIALGL_API spgl_status spgl_aggregate_init(spgl_aggregate* value);
SPATIALGL_API spgl_status spgl_diagnostic_init(spgl_diagnostic* value);
SPATIALGL_API spgl_status spgl_program_event_init(spgl_program_event* value);
SPATIALGL_API spgl_status spgl_receipt_init(spgl_receipt* value);

SPATIALGL_API spgl_status spgl_context_create(const spgl_context_desc* desc,
                                              spgl_context** out_context);
SPATIALGL_API void spgl_context_destroy(spgl_context* context);
SPATIALGL_API const char* spgl_context_last_error(const spgl_context* context);
SPATIALGL_API spgl_status spgl_context_set_surface(spgl_context* context,
                                                   const spgl_surface_desc* surface);
SPATIALGL_API spgl_status spgl_context_remove_surface(spgl_context* context, const char* id);
SPATIALGL_API spgl_status spgl_context_set_occluder(spgl_context* context,
                                                    const spgl_occluder_desc* occluder);
SPATIALGL_API spgl_status spgl_context_remove_occluder(spgl_context* context, const char* id);
SPATIALGL_API spgl_status spgl_context_set_device(spgl_context* context,
                                                  const spgl_device_desc* device);
SPATIALGL_API spgl_status spgl_context_set_resource(spgl_context* context,
                                                    const spgl_resource_desc* resource);
SPATIALGL_API spgl_status spgl_context_remove_resource(spgl_context* context, const char* id);
SPATIALGL_API spgl_status spgl_context_remove_device(spgl_context* context, const char* id);
SPATIALGL_API spgl_status spgl_context_set_device_availability(spgl_context* context,
                                                               const char* id,
                                                               spgl_availability availability);
SPATIALGL_API spgl_status spgl_context_set_surface_motion(spgl_context* context,
                                                          const spgl_world_motion_desc* motion);
SPATIALGL_API spgl_status
spgl_context_update_world_metadata(spgl_context* context, const spgl_world_metadata_desc* metadata);
SPATIALGL_API spgl_status spgl_context_world_revision(const spgl_context* context,
                                                      uint64_t* out_revision);
SPATIALGL_API spgl_status spgl_context_calibration_revision(const spgl_context* context,
                                                            uint64_t* out_revision);

SPATIALGL_API spgl_status spgl_frame_create(spgl_context* context, const spgl_frame_desc* desc,
                                            spgl_frame** out_frame);
SPATIALGL_API void spgl_frame_destroy(spgl_frame* frame);
SPATIALGL_API spgl_status spgl_frame_draw_point(spgl_frame* frame, const spgl_draw_desc* draw,
                                                spgl_vec3 position);
SPATIALGL_API spgl_status spgl_frame_draw_polyline(spgl_frame* frame, const spgl_draw_desc* draw,
                                                   const spgl_vec3* vertices, uint64_t count);
SPATIALGL_API spgl_status spgl_frame_draw_patch(spgl_frame* frame, const spgl_draw_desc* draw,
                                                spgl_vec3 origin, spgl_vec3 u, spgl_vec3 v);
SPATIALGL_API spgl_status spgl_context_submit(spgl_context* context, const spgl_frame* frame,
                                              uint64_t* out_ticket_id);
SPATIALGL_API spgl_status spgl_context_cancel(spgl_context* context, uint64_t ticket_id);
SPATIALGL_API spgl_status spgl_context_advance(spgl_context* context, double to_time_s);
SPATIALGL_API spgl_status spgl_context_snapshot(spgl_context* context,
                                                spgl_snapshot** out_snapshot);
SPATIALGL_API void spgl_snapshot_destroy(spgl_snapshot* snapshot);
SPATIALGL_API spgl_status spgl_snapshot_time_s(const spgl_snapshot* snapshot, double* out_time_s);
SPATIALGL_API spgl_status spgl_snapshot_world_revision(const spgl_snapshot* snapshot,
                                                       uint64_t* out_revision);
SPATIALGL_API spgl_status spgl_snapshot_calibration_revision(const spgl_snapshot* snapshot,
                                                             uint64_t* out_revision);
SPATIALGL_API spgl_status spgl_snapshot_output_count(const spgl_snapshot* snapshot,
                                                     uint64_t* out_count);
SPATIALGL_API spgl_status spgl_snapshot_output_at(const spgl_snapshot* snapshot, uint64_t index,
                                                  spgl_output* out_value);
SPATIALGL_API spgl_status spgl_snapshot_aggregate_count(const spgl_snapshot* snapshot,
                                                        uint64_t* out_count);
SPATIALGL_API spgl_status spgl_snapshot_aggregate_at(const spgl_snapshot* snapshot, uint64_t index,
                                                     spgl_aggregate* out_value);
SPATIALGL_API spgl_status spgl_snapshot_diagnostic_count(const spgl_snapshot* snapshot,
                                                         uint64_t* out_count);
SPATIALGL_API spgl_status spgl_snapshot_diagnostic_at(const spgl_snapshot* snapshot, uint64_t index,
                                                      spgl_diagnostic* out_value);
SPATIALGL_API spgl_status spgl_snapshot_program_event_count(const spgl_snapshot* snapshot,
                                                            uint64_t* out_count);
SPATIALGL_API spgl_status spgl_snapshot_program_event_at(const spgl_snapshot* snapshot,
                                                         uint64_t index,
                                                         spgl_program_event* out_value);
SPATIALGL_API spgl_status spgl_snapshot_receipt_count(const spgl_snapshot* snapshot,
                                                      uint64_t* out_count);
SPATIALGL_API spgl_status spgl_snapshot_receipt_at(const spgl_snapshot* snapshot, uint64_t index,
                                                   spgl_receipt* out_value);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* SPATIALGL_CAPI_SPATIALGL_H_ */
