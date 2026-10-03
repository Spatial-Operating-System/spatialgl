#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "spatialgl/capi/spatialgl.h"
#include "spatialgl/optics.h"

using namespace spatialgl::optics;

#define REQUIRE(x)                                                                            \
  do {                                                                                        \
    if (!(x)) {                                                                               \
      std::fprintf(stderr, "parity assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
      return 1;                                                                               \
    }                                                                                         \
  } while (0)

bool near(double a, double b) { return std::abs(a - b) < 1e-8; }
uint64_t hash_id(const std::string& value) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (unsigned char byte : value) {
    hash ^= byte;
    hash *= UINT64_C(1099511628211);
  }
  return hash ? hash : 1;
}

int main() {
  WorldSnapshot world;
  world.planes.push_back({"screen", {-1, -1, 0}, {2, 0, 0}, {0, 2, 0}});
  world.source_time = 0;
  world.valid_until = 3;
  world.world_revision = 11;
  world.calibration_revision = 4;

  Rig rig;
  DeviceProfile device;
  device.id = "projector";
  device.position = {0, 0, 2};
  device.target = {0, 0, 0};
  device.latency = 0.1;
  device.clock_offset = 0.02;
  device.clock_drift = 0.001;
  device.clock_uncertainty = 0.003;
  device.calibrated_gain = {2, 0.5, 1.5};
  rig.devices.push_back(device);

  RuntimeOptions options;
  options.pipeline.output_mix = OutputMix::kNormalized;
  Runtime native(world, rig, options);
  for (const auto& spec : std::vector<std::pair<std::string, Vec3>>{{"list-a", {-0.35, 0.1, 0}},
                                                                    {"list-b", {0.35, -0.1, 0}}}) {
    DisplayList list;
    list.id = spec.first;
    list.app_id = spec.first == "list-a" ? "app-a" : "app-b";
    list.present_at = 0;
    list.expires_at = 2;
    list.composition = Composition::kOver;
    DrawCall draw;
    draw.draw_id = "cursor";
    draw.target.surface_id = "screen";
    draw.target.geometry = GeometryKind::kPoint;
    draw.target.origin = spec.second;
    draw.linear_rgb = {0.2, 0.4, 0.8};
    draw.opacity = 0.5;
    draw.intensity = 2.0;
    list.draws.push_back(draw);
    native.submit(list);
  }
  Snapshot native_early = native.advance(0.05);
  REQUIRE(native_early.contributions.empty());
  REQUIRE(native_early.world_revision == 11);
  REQUIRE(native_early.calibration_revision == 4);
  Snapshot expected = native.advance(0.5);

  spgl_context_desc context_desc;
  REQUIRE(spgl_context_desc_init(&context_desc) == SPGL_STATUS_OK);
  spgl_context* context = nullptr;
  REQUIRE(spgl_context_create(&context_desc, &context) == SPGL_STATUS_OK);
  spgl_surface_desc surface;
  REQUIRE(spgl_surface_desc_init(&surface) == SPGL_STATUS_OK);
  surface.id = "screen";
  surface.origin = {-1, -1, 0};
  surface.u = {2, 0, 0};
  surface.v = {0, 2, 0};
  REQUIRE(spgl_context_set_surface(context, &surface) == SPGL_STATUS_OK);
  spgl_device_desc c_device;
  REQUIRE(spgl_device_desc_init(&c_device) == SPGL_STATUS_OK);
  c_device.id = "projector";
  c_device.profile = "fixed-projector";
  c_device.position = {0, 0, 2};
  c_device.target = {0, 0, 0};
  c_device.latency_s = 0.1;
  c_device.clock_offset_s = 0.02;
  c_device.clock_drift = 0.001;
  c_device.clock_uncertainty_s = 0.003;
  c_device.calibrated_gain = {2, 0.5, 1.5};
  REQUIRE(spgl_context_set_device(context, &c_device) == SPGL_STATUS_OK);
  spgl_world_metadata_desc metadata;
  REQUIRE(spgl_world_metadata_desc_init(&metadata) == SPGL_STATUS_OK);
  metadata.source_time_s = 0;
  metadata.valid_until_s = 3;
  metadata.world_revision = 11;
  metadata.calibration_revision = 4;
  REQUIRE(spgl_context_update_world_metadata(context, &metadata) == SPGL_STATUS_OK);

  spgl_draw_desc c_draw;
  REQUIRE(spgl_draw_desc_init(&c_draw) == SPGL_STATUS_OK);
  c_draw.draw_id = "cursor";
  c_draw.surface_id = "screen";
  c_draw.color_linear = {0.2, 0.4, 0.8};
  c_draw.opacity = 0.5;
  c_draw.intensity = 2.0;
  uint64_t tickets[2]{};
  const char* list_ids[] = {"list-a", "list-b"};
  const char* app_ids[] = {"app-a", "app-b"};
  const spgl_vec3 positions[] = {{-0.35, 0.1, 0}, {0.35, -0.1, 0}};
  for (int i = 0; i < 2; ++i) {
    spgl_frame_desc frame_desc;
    REQUIRE(spgl_frame_desc_init(&frame_desc) == SPGL_STATUS_OK);
    frame_desc.id = list_ids[i];
    frame_desc.app_id = app_ids[i];
    frame_desc.expires_at_s = 2;
    spgl_frame* frame = nullptr;
    REQUIRE(spgl_frame_create(context, &frame_desc, &frame) == SPGL_STATUS_OK);
    REQUIRE(spgl_frame_draw_point(frame, &c_draw, positions[i]) == SPGL_STATUS_OK);
    REQUIRE(spgl_context_submit(context, frame, &tickets[i]) == SPGL_STATUS_OK);
    spgl_frame_destroy(frame);
  }
  REQUIRE(spgl_context_advance(context, 0.05) == SPGL_STATUS_OK);
  spgl_snapshot* early = nullptr;
  REQUIRE(spgl_context_snapshot(context, &early) == SPGL_STATUS_OK);
  uint64_t count = 0;
  REQUIRE(spgl_snapshot_output_count(early, &count) == SPGL_STATUS_OK && count == 0);
  uint64_t revision = 0;
  REQUIRE(spgl_snapshot_world_revision(early, &revision) == SPGL_STATUS_OK && revision == 11);
  REQUIRE(spgl_snapshot_calibration_revision(early, &revision) == SPGL_STATUS_OK && revision == 4);
  spgl_snapshot_destroy(early);

  REQUIRE(spgl_context_advance(context, 0.5) == SPGL_STATUS_OK);
  spgl_snapshot* actual = nullptr;
  REQUIRE(spgl_context_snapshot(context, &actual) == SPGL_STATUS_OK);
  REQUIRE(spgl_snapshot_world_revision(actual, &revision) == SPGL_STATUS_OK && revision == 11);
  REQUIRE(spgl_snapshot_calibration_revision(actual, &revision) == SPGL_STATUS_OK && revision == 4);
  REQUIRE(spgl_snapshot_output_count(actual, &count) == SPGL_STATUS_OK);
  REQUIRE(count == expected.contributions.size());
  for (uint64_t i = 0; i < count; ++i) {
    spgl_output output;
    REQUIRE(spgl_output_init(&output) == SPGL_STATUS_OK);
    REQUIRE(spgl_snapshot_output_at(actual, i, &output) == SPGL_STATUS_OK);
    const Contribution& want = expected.contributions[i];
    REQUIRE(output.draw_id_hash == hash_id(want.draw_id));
    REQUIRE(output.source_frame_id_hash == hash_id(want.display_list_id));
    REQUIRE(output.device_id_hash == hash_id(want.device_id));
    for (int channel = 0; channel < 3; ++channel) {
      REQUIRE(near((&output.position_m.x)[channel], want.position[channel]));
      REQUIRE(near((&output.contribution_linear.r)[channel],
                   want.linear_rgb[channel] * want.intensity));
    }
    REQUIRE(output.world_revision == 11 && output.calibration_revision == 4);
  }
  uint64_t aggregate_count = 0;
  REQUIRE(spgl_snapshot_aggregate_count(actual, &aggregate_count) == SPGL_STATUS_OK);
  REQUIRE(aggregate_count == expected.targets.size());
  RGB expected_total{}, actual_total{};
  for (const auto& target : expected.targets)
    for (int channel = 0; channel < 3; ++channel)
      expected_total[channel] += target.aggregate_linear_rgb[channel];
  for (uint64_t i = 0; i < aggregate_count; ++i) {
    spgl_aggregate aggregate;
    REQUIRE(spgl_aggregate_init(&aggregate) == SPGL_STATUS_OK);
    REQUIRE(spgl_snapshot_aggregate_at(actual, i, &aggregate) == SPGL_STATUS_OK);
    for (int channel = 0; channel < 3; ++channel)
      actual_total[channel] += (&aggregate.light_linear.r)[channel];
  }
  for (int channel = 0; channel < 3; ++channel)
    REQUIRE(near(actual_total[channel], expected_total[channel]));

  uint64_t event_count = 0;
  REQUIRE(spgl_snapshot_program_event_count(actual, &event_count) == SPGL_STATUS_OK);
  uint64_t expected_events = 0;
  for (const auto& program : expected.programs) expected_events += program.events.size();
  REQUIRE(event_count == expected_events && event_count > 0);
  uint64_t event_index = 0;
  for (const auto& program : expected.programs) {
    for (const auto& want : program.events) {
      spgl_program_event event;
      REQUIRE(spgl_program_event_init(&event) == SPGL_STATUS_OK);
      REQUIRE(spgl_snapshot_program_event_at(actual, event_index++, &event) == SPGL_STATUS_OK);
      REQUIRE(event.kind == static_cast<uint32_t>(want.kind) + 1);
      REQUIRE(event.source_list_id_hash == hash_id(want.display_list_id));
      REQUIRE(event.draw_id_hash == hash_id(want.draw_id));
      REQUIRE(event.world_revision == 11 && event.calibration_revision == 4);
      REQUIRE(near(event.time_s, want.time));
      REQUIRE(near(event.dwell_s, want.dwell));
      for (int channel = 0; channel < 3; ++channel) {
        REQUIRE(near((&event.position_m.x)[channel], want.position[channel]));
        REQUIRE(near((&event.color_linear.r)[channel], want.linear_rgb[channel]));
      }
    }
  }
  spgl_snapshot_destroy(actual);
  spgl_context_destroy(context);
  std::puts("SpatialGL C/C++ parity test passed");
  return 0;
}
