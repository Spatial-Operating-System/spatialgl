#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "spatialgl/capi/spatialgl.h"

#define CHECK(expr)                                                              \
  do {                                                                           \
    if (!(expr)) {                                                               \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
      return 1;                                                                  \
    }                                                                            \
  } while (0)

int main(void) {
  _Static_assert(sizeof(spgl_status) == 4, "status must remain fixed width");
  CHECK(spgl_abi_version() == SPATIALGL_ABI_VERSION);

  spgl_context_desc context_desc;
  CHECK(spgl_context_desc_init(&context_desc) == SPGL_STATUS_OK);
  CHECK(context_desc.position_tolerance_m == 0);
  spgl_context_desc unsupported_tolerance = context_desc;
  unsupported_tolerance.position_tolerance_m = 0.01;
  spgl_context* rejected_context = NULL;
  CHECK(spgl_context_create(&unsupported_tolerance, &rejected_context) == SPGL_STATUS_UNSUPPORTED);
  CHECK(rejected_context == NULL);
  spgl_context_desc bad_context_desc = context_desc;
  bad_context_desc.abi_version = SPATIALGL_ABI_VERSION + 1;
  CHECK(spgl_context_create(&bad_context_desc, &rejected_context) == SPGL_STATUS_INVALID_ARGUMENT);
  CHECK(rejected_context == NULL);
  spgl_context_desc bad_policy_desc = context_desc;
  bad_policy_desc.output_mix = 99;
  CHECK(spgl_context_create(&bad_policy_desc, &rejected_context) == SPGL_STATUS_INVALID_ARGUMENT);
  CHECK(rejected_context == NULL);
  spgl_context* context = NULL;
  CHECK(spgl_context_create(&context_desc, &context) == SPGL_STATUS_OK);
  CHECK(context != NULL);

  spgl_surface_desc surface;
  CHECK(spgl_surface_desc_init(&surface) == SPGL_STATUS_OK);
  spgl_surface_desc undersized_surface = surface;
  undersized_surface.size = 4;
  CHECK(spgl_context_set_surface(context, &undersized_surface) == SPGL_STATUS_INVALID_ARGUMENT);
  surface.id = "screen";
  surface.u = (spgl_vec3){0, 0, 0};
  CHECK(spgl_context_set_surface(context, &surface) == SPGL_STATUS_INVALID_ARGUMENT);
  surface.origin = (spgl_vec3){-1, -1, 0};
  surface.u = (spgl_vec3){2, 0, 0};
  surface.v = (spgl_vec3){0, 2, 0};
  CHECK(spgl_context_set_surface(context, &surface) == SPGL_STATUS_OK);

  spgl_device_desc device;
  CHECK(spgl_device_desc_init(&device) == SPGL_STATUS_OK);
  spgl_resource_desc resource;
  CHECK(spgl_resource_desc_init(&resource) == SPGL_STATUS_OK);
  resource.id = "steering-dac";
  resource.capacity = 2;
  CHECK(spgl_context_set_resource(context, &resource) == SPGL_STATUS_UNSUPPORTED);
  resource.capacity = 1;
  CHECK(spgl_context_set_resource(context, &resource) == SPGL_STATUS_OK);
  device.id = "projector";
  device.profile = "fixed-projector";
  device.position = (spgl_vec3){0, 0, 2};
  device.target = (spgl_vec3){0, 0, 0};
  device.latency_s = 0.1;
  const char* resource_ids[] = {"steering-dac"};
  device.resource_count = 1;
  device.resource_ids = resource_ids;
  CHECK(spgl_context_set_device(context, &device) == SPGL_STATUS_OK);
  device.id = "projector-2";
  device.calibrated_gain = (spgl_rgb){3, 3, 3};
  device.resource_count = 0;
  device.resource_ids = NULL;
  CHECK(spgl_context_set_device(context, &device) == SPGL_STATUS_OK);

  spgl_draw_desc draw;
  CHECK(spgl_draw_desc_init(&draw) == SPGL_STATUS_OK);
  draw.draw_id = "center-dot";
  draw.surface_id = "screen";

  spgl_frame_desc frame_desc;
  CHECK(spgl_frame_desc_init(&frame_desc) == SPGL_STATUS_OK);
  frame_desc.id = "frame-1";
  frame_desc.app_id = "smoke";
  frame_desc.present_at_s = 0;
  frame_desc.expires_at_s = 2;
  spgl_frame_desc invalid_frame_desc = frame_desc;
  invalid_frame_desc.expires_at_s = invalid_frame_desc.present_at_s;
  spgl_frame* rejected_frame = NULL;
  CHECK(spgl_frame_create(context, &invalid_frame_desc, &rejected_frame) ==
        SPGL_STATUS_INVALID_ARGUMENT);
  CHECK(rejected_frame == NULL);
  spgl_frame* frame = NULL;
  CHECK(spgl_frame_create(context, &frame_desc, &frame) == SPGL_STATUS_OK);
  const spgl_vec3 nonfinite_point = {NAN, 0, 0};
  CHECK(spgl_frame_draw_point(frame, &draw, nonfinite_point) == SPGL_STATUS_INVALID_ARGUMENT);
  CHECK(spgl_frame_draw_point(frame, &draw, (spgl_vec3){0, 0, 0}) == SPGL_STATUS_OK);

  uint64_t ticket = 0;
  CHECK(spgl_context_submit(context, frame, &ticket) == SPGL_STATUS_OK);
  CHECK(ticket != 0);
  spgl_frame_destroy(frame); /* submission owns a copy */
  CHECK(spgl_context_advance(context, 0.05) == SPGL_STATUS_OK);

  spgl_snapshot* snapshot = NULL;
  CHECK(spgl_context_snapshot(context, &snapshot) == SPGL_STATUS_OK);
  CHECK(snapshot != NULL);
  double snapshot_time = 0;
  uint64_t count = 0;
  CHECK(spgl_snapshot_output_count(NULL, &count) == SPGL_STATUS_INVALID_ARGUMENT);
  CHECK(spgl_snapshot_time_s(snapshot, &snapshot_time) == SPGL_STATUS_OK);
  CHECK(snapshot_time == 0.05);
  CHECK(spgl_snapshot_output_count(snapshot, &count) == SPGL_STATUS_OK && count == 0);
  /* device latency has not elapsed */
  CHECK(spgl_snapshot_program_event_count(snapshot, &count) == SPGL_STATUS_OK && count > 0);
  /* scheduled program is visible */
  spgl_snapshot_destroy(snapshot);
  CHECK(spgl_context_advance(context, 0.15) == SPGL_STATUS_OK);
  CHECK(spgl_context_snapshot(context, &snapshot) == SPGL_STATUS_OK);
  CHECK(spgl_snapshot_time_s(snapshot, &snapshot_time) == SPGL_STATUS_OK);
  CHECK(snapshot_time == 0.15);
  uint64_t snapshot_calibration = UINT64_MAX;
  CHECK(spgl_snapshot_calibration_revision(snapshot, &snapshot_calibration) == SPGL_STATUS_OK);
  CHECK(snapshot_calibration == 0);
  CHECK(spgl_snapshot_output_count(snapshot, &count) == SPGL_STATUS_OK && count == 2);
  CHECK(spgl_snapshot_aggregate_count(snapshot, &count) == SPGL_STATUS_OK && count == 1);
  CHECK(spgl_snapshot_receipt_count(snapshot, &count) == SPGL_STATUS_OK && count == 2);

  spgl_output output;
  CHECK(spgl_output_init(&output) == SPGL_STATUS_OK);
  output.reserved = UINT32_MAX;
  spgl_output undersized_output = output;
  undersized_output.size = 4;
  CHECK(spgl_snapshot_output_at(snapshot, 0, &undersized_output) == SPGL_STATUS_INVALID_ARGUMENT);
  CHECK(spgl_snapshot_output_at(snapshot, 0, &output) == SPGL_STATUS_OK);
  CHECK(output.reserved == 0);
  CHECK(output.draw_id_hash != 0 && output.device_id_hash != 0);
  CHECK(output.source_frame_id_hash != 0 && output.world_revision != 0);
  const double first_share = output.contribution_linear.r;
  CHECK(spgl_snapshot_output_at(snapshot, 1, &output) == SPGL_STATUS_OK);
  CHECK(fabs(first_share - 0.25) < 1e-9);
  CHECK(fabs(output.contribution_linear.r - 0.75) < 1e-9);
  CHECK(spgl_snapshot_output_at(snapshot, UINT64_MAX, &output) == SPGL_STATUS_OUT_OF_RANGE);

  spgl_program_event event;
  CHECK(spgl_program_event_init(&event) == SPGL_STATUS_OK);
  CHECK(spgl_snapshot_program_event_count(snapshot, &count) == SPGL_STATUS_OK && count > 0);
  CHECK(spgl_snapshot_program_event_at(snapshot, 0, &event) == SPGL_STATUS_OK);
  CHECK(event.source_list_id_hash != 0 && event.draw_id_hash != 0);
  const uint64_t old_generation = event.generation;

  spgl_receipt receipt;
  CHECK(spgl_receipt_init(&receipt) == SPGL_STATUS_OK);
  CHECK(spgl_snapshot_receipt_at(snapshot, 0, &receipt) == SPGL_STATUS_OK);
  CHECK(receipt.device_id_hash != 0);

  spgl_aggregate aggregate;
  CHECK(spgl_aggregate_init(&aggregate) == SPGL_STATUS_OK);
  CHECK(spgl_snapshot_aggregate_at(snapshot, 0, &aggregate) == SPGL_STATUS_OK);
  CHECK(fabs(aggregate.light_linear.r - 1.0) < 1e-9);
  CHECK(fabs(aggregate.light_linear.g - 1.0) < 1e-9);
  CHECK(fabs(aggregate.light_linear.b - 1.0) < 1e-9);

  spgl_context* other_context = NULL;
  CHECK(spgl_context_create(&context_desc, &other_context) == SPGL_STATUS_OK);
  spgl_frame* foreign_frame = NULL;
  CHECK(spgl_frame_create(other_context, &frame_desc, &foreign_frame) == SPGL_STATUS_OK);
  CHECK(spgl_context_submit(context, foreign_frame, &ticket) == SPGL_STATUS_INVALID_ARGUMENT);
  spgl_frame_destroy(foreign_frame);
  spgl_context_destroy(other_context);

  spgl_world_metadata_desc metadata;
  CHECK(spgl_world_metadata_desc_init(&metadata) == SPGL_STATUS_OK);
  metadata.source_time_s = 0.15;
  metadata.valid_until_s = 2.0;
  metadata.calibration_revision = 1;
  CHECK(spgl_context_update_world_metadata(context, &metadata) == SPGL_STATUS_OK);
  uint64_t current_revision = 0;
  CHECK(spgl_context_world_revision(context, &current_revision) == SPGL_STATUS_OK);
  const uint64_t calibration_world_revision = current_revision;
  CHECK(spgl_context_calibration_revision(context, &current_revision) == SPGL_STATUS_OK);
  CHECK(current_revision == 1);
  CHECK(spgl_context_advance(context, 0.16) == SPGL_STATUS_OK);
  spgl_snapshot* invalidated = NULL;
  CHECK(spgl_context_snapshot(context, &invalidated) == SPGL_STATUS_OK);
  CHECK(spgl_snapshot_output_count(invalidated, &count) == SPGL_STATUS_OK && count == 0);
  CHECK(spgl_snapshot_diagnostic_count(invalidated, &count) == SPGL_STATUS_OK && count > 0);
  spgl_diagnostic diagnostic;
  CHECK(spgl_diagnostic_init(&diagnostic) == SPGL_STATUS_OK);
  CHECK(spgl_snapshot_diagnostic_at(invalidated, 0, &diagnostic) == SPGL_STATUS_OK);
  CHECK(diagnostic.code == SPGL_DIAGNOSTIC_INVALIDATED);
  CHECK(spgl_snapshot_calibration_revision(invalidated, &snapshot_calibration) == SPGL_STATUS_OK);
  CHECK(snapshot_calibration == 1);
  CHECK(spgl_snapshot_calibration_revision(snapshot, &snapshot_calibration) == SPGL_STATUS_OK);
  CHECK(snapshot_calibration == 0);
  spgl_snapshot_destroy(invalidated);
  CHECK(spgl_context_cancel(context, ticket) == SPGL_STATUS_OK);

  /* Invalid revision changes are transactional and leave the world intact. */
  metadata.world_revision = calibration_world_revision;
  metadata.calibration_revision = 0;
  CHECK(spgl_context_update_world_metadata(context, &metadata) == SPGL_STATUS_INVALID_ARGUMENT);
  CHECK(spgl_context_world_revision(context, &current_revision) == SPGL_STATUS_OK);
  CHECK(current_revision == calibration_world_revision);
  CHECK(spgl_context_calibration_revision(context, &current_revision) == SPGL_STATUS_OK);
  CHECK(current_revision == 1);
  metadata.world_revision = 0;
  metadata.valid_until_s = 0.2;
  CHECK(spgl_context_update_world_metadata(context, &metadata) == SPGL_STATUS_OK);
  spgl_frame_desc fresh_desc = frame_desc;
  fresh_desc.id = "fresh-frame";
  fresh_desc.present_at_s = 0.16;
  spgl_frame* fresh_frame = NULL;
  CHECK(spgl_frame_create(context, &fresh_desc, &fresh_frame) == SPGL_STATUS_OK);
  CHECK(spgl_frame_draw_point(fresh_frame, &draw, (spgl_vec3){0, 0, 0}) == SPGL_STATUS_OK);
  uint64_t fresh_ticket = 0;
  CHECK(spgl_context_submit(context, fresh_frame, &fresh_ticket) == SPGL_STATUS_OK);
  spgl_frame_destroy(fresh_frame);
  CHECK(spgl_context_advance(context, 0.3) == SPGL_STATUS_OK);
  spgl_snapshot* expired = NULL;
  CHECK(spgl_context_snapshot(context, &expired) == SPGL_STATUS_OK);
  CHECK(spgl_snapshot_diagnostic_count(expired, &count) == SPGL_STATUS_OK && count > 0);
  int found_stale_world = 0;
  for (uint64_t i = 0; i < count; ++i) {
    CHECK(spgl_snapshot_diagnostic_at(expired, i, &diagnostic) == SPGL_STATUS_OK);
    if (diagnostic.code == SPGL_DIAGNOSTIC_STALE_WORLD) found_stale_world = 1;
  }
  CHECK(found_stale_world);
  spgl_snapshot_destroy(expired);

  CHECK(spgl_context_cancel(context, fresh_ticket) == SPGL_STATUS_OK);
  CHECK(spgl_context_set_device_availability(context, "projector", SPGL_AVAILABILITY_UNAVAILABLE) ==
        SPGL_STATUS_OK);
  spgl_world_motion_desc motion;
  CHECK(spgl_world_motion_desc_init(&motion) == SPGL_STATUS_OK);
  motion.surface_id = "screen";
  motion.source_time_s = 0.15;
  motion.valid_until_s = 2.0;
  motion.calibration_revision = 7;
  motion.linear_velocity_m_s = (spgl_vec3){0, 0, 0};
  motion.angular_velocity_rad_s = (spgl_vec3){0, 0, 0};
  CHECK(spgl_context_set_surface_motion(context, &motion) == SPGL_STATUS_OK);
  current_revision = 0;
  CHECK(spgl_context_world_revision(context, &current_revision) == SPGL_STATUS_OK);
  CHECK(current_revision > 0);
  CHECK(spgl_context_calibration_revision(context, &current_revision) == SPGL_STATUS_OK);
  CHECK(current_revision == 7);
  CHECK(spgl_context_advance(context, 0.35) == SPGL_STATUS_OK);
  spgl_snapshot* after_cancel = NULL;
  CHECK(spgl_context_snapshot(context, &after_cancel) == SPGL_STATUS_OK);
  CHECK(spgl_snapshot_output_count(after_cancel, &count) == SPGL_STATUS_OK && count == 0);
  CHECK(spgl_snapshot_receipt_at(after_cancel, 0, &receipt) == SPGL_STATUS_OK);
  CHECK(receipt.availability == SPGL_AVAILABILITY_UNAVAILABLE);
  CHECK(receipt.generation > old_generation);
  spgl_snapshot_destroy(after_cancel);

  /* Snapshot storage outlives the context and remains queryable. */
  spgl_context_destroy(context);
  CHECK(spgl_snapshot_output_at(snapshot, 0, &output) == SPGL_STATUS_OK);
  spgl_snapshot_destroy(snapshot);

  CHECK(spgl_context_create(NULL, &context) == SPGL_STATUS_INVALID_ARGUMENT);
  CHECK(context == NULL);
  puts("SpatialGL C ABI v1 smoke test passed");
  return 0;
}
