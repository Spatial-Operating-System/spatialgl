#include <inttypes.h>
#include <stdio.h>

#include "spatialgl/capi/spatialgl.h"

#define REQUIRE_OK(context, expression)                                                            \
  do {                                                                                             \
    spgl_status status_ = (expression);                                                            \
    if (status_ != SPGL_STATUS_OK) {                                                               \
      const char* detail_ = spgl_context_last_error(context);                                      \
      fprintf(stderr, "%s failed with status %d%s%s\n", #expression, status_, detail_ ? ": " : "", \
              detail_ ? detail_ : "");                                                             \
      goto cleanup;                                                                                \
    }                                                                                              \
  } while (0)

int main(void) {
  int exit_code = 1;
  spgl_context* context = NULL;
  spgl_frame* frame = NULL;
  spgl_snapshot* snapshot = NULL;
  uint64_t ticket = 0, aggregate_count = 0;
  spgl_context_desc context_desc;
  spgl_surface_desc surface;
  spgl_device_desc device;
  spgl_frame_desc frame_desc;
  spgl_draw_desc draw;
  spgl_aggregate aggregate;

  REQUIRE_OK(NULL, spgl_context_desc_init(&context_desc));
  REQUIRE_OK(NULL, spgl_context_create(&context_desc, &context));

  REQUIRE_OK(context, spgl_surface_desc_init(&surface));
  surface.id = "wall";
  surface.origin = (spgl_vec3){-1, -1, 0};
  surface.u = (spgl_vec3){2, 0, 0};
  surface.v = (spgl_vec3){0, 2, 0};
  REQUIRE_OK(context, spgl_context_set_surface(context, &surface));

  REQUIRE_OK(context, spgl_device_desc_init(&device));
  device.id = "projector";
  device.profile = "fixed-projector";
  device.position = (spgl_vec3){0, 0, 2};
  device.target = (spgl_vec3){0, 0, 0};
  device.latency_s = 0.05;
  REQUIRE_OK(context, spgl_context_set_device(context, &device));

  REQUIRE_OK(context, spgl_frame_desc_init(&frame_desc));
  frame_desc.id = "hello-frame";
  frame_desc.app_id = "hello-c";
  frame_desc.present_at_s = 0;
  frame_desc.expires_at_s = 1;
  REQUIRE_OK(context, spgl_frame_create(context, &frame_desc, &frame));

  REQUIRE_OK(context, spgl_draw_desc_init(&draw));
  draw.draw_id = "hello-point";
  draw.surface_id = "wall";
  draw.color_linear = (spgl_rgb){0.2, 0.5, 0.9};
  REQUIRE_OK(context, spgl_frame_draw_point(frame, &draw, (spgl_vec3){0, 0, 0}));
  REQUIRE_OK(context, spgl_context_submit(context, frame, &ticket));
  spgl_frame_destroy(frame);
  frame = NULL;

  REQUIRE_OK(context, spgl_context_advance(context, 0.1));
  REQUIRE_OK(context, spgl_context_snapshot(context, &snapshot));
  REQUIRE_OK(context, spgl_snapshot_aggregate_count(snapshot, &aggregate_count));
  if (aggregate_count != 1) {
    fprintf(stderr, "expected one aggregate target, got %" PRIu64 "\n", aggregate_count);
    goto cleanup;
  }
  REQUIRE_OK(context, spgl_aggregate_init(&aggregate));
  REQUIRE_OK(context, spgl_snapshot_aggregate_at(snapshot, 0, &aggregate));
  printf("ticket=%" PRIu64 " light=(%.3f, %.3f, %.3f) provenance=%u\n", ticket,
         aggregate.light_linear.r, aggregate.light_linear.g, aggregate.light_linear.b,
         aggregate.provenance);
  exit_code = 0;

cleanup:
  spgl_snapshot_destroy(snapshot);
  spgl_frame_destroy(frame);
  spgl_context_destroy(context);
  return exit_code;
}
