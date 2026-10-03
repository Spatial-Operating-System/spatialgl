"""Run the six core-driven surface optics acceptance scenarios.

Run with: bazel run //python:optics_scenarios
"""

from spatialgl.optics import (
    Availability, Composition, DeviceKind, EventKind, GeometryKind, OpticalRuntime, OutputMix,
    PipelineState, PlanarMotion, PlanStatus, Presentation, PresentationState,
    Resource, RuntimeOptions, TraversalState, draw, display, fixed_raster, galvo,
    occluder, plane, rig, steerable_raster, world,
)


def make_runtime(devices, *, source_world=None, resources=(), output_mix=OutputMix.NORMALIZED,
                 presentation=None):
    options = RuntimeOptions()
    options.pipeline.output_mix = output_mix
    options.pipeline.sample_spacing = 0.25
    options.presentation = presentation or PresentationState()
    return OpticalRuntime(source_world or world([plane("wall")]), rig(devices, resources), options)


def f01_overlap_occlusion():
    high_gain = fixed_raster("high-gain", position=(-1, 1, 3), target=(0, 1, 0), latency=0.01)
    high_gain.calibrated_gain = (2, 1, 1)
    ordinary = fixed_raster("ordinary", position=(1, 1, 3), target=(0, 1, 0), latency=0.01)
    shared = make_runtime([high_gain, ordinary])
    assert not shared.submit(display("shared-overlap", [draw("dot", "wall")]))
    normalized = shared.advance(0.1)
    assert {x.device_id for x in normalized.contributions} == {"high-gain", "ordinary"}
    assert abs(normalized.targets[0].aggregate_linear_rgb[0] - 1.0) < 1e-8

    large = make_runtime([fixed_raster("clock", latency=0.03)])
    small = make_runtime([fixed_raster("clock", latency=0.03)])
    timed = display("timed", [draw("line", "wall", geometry=GeometryKind.POLYLINE,
                                    vertices=[(-0.25, 1, 0), (0.25, 1, 0)])])
    assert not large.submit(timed) and not small.submit(timed)
    large_trace = large.advance(0.4)
    for tick in (0.05, 0.1, 0.2, 0.3, 0.4):
        small.advance(tick)
    small_trace = small.snapshot()
    signature = lambda snapshot: [
        (p.device_id, [(e.kind, e.time, e.position, e.linear_rgb, e.dwell, e.draw_id)
                       for e in p.events])
        for p in snapshot.programs
    ]
    assert signature(large_trace) == signature(small_trace)

    first = fixed_raster("left", position=(-1, 1, 3), target=(0, 1, 0), latency=0.01)
    second = fixed_raster("right", position=(1, 1, 3), target=(0, 1, 0), latency=0.01)
    blocked_world = world([plane("wall")], occluders=[occluder("post", (-0.7, 0.8, 1), (-0.3, 1.2, 2))])
    runtime = make_runtime([first, second], source_world=blocked_world)
    assert not runtime.submit(display("overlap", [draw("dot", "wall")]))
    before_latency = runtime.advance(0.005)
    assert not before_latency.contributions
    state = runtime.advance(0.1)
    assert {x.device_id for x in state.contributions} == {"right"}
    light = state.targets[0]
    assert light.aggregate_linear_rgb[0] <= 1.0 + 1e-9
    assert all(x.provenance.name in ("PREDICTED", "SIMULATED") for x in state.targets)
    # The core's target records remain attached to their source revision.
    assert state.world_revision == 1
    # A second advance after the original submission preserves absolute program
    # event times instead of re-timestamping an already scheduled program.
    later = runtime.advance(0.2)
    assert [(p.device_id, [e.time for e in p.events]) for p in state.programs] == [
        (p.device_id, [e.time for e in p.events]) for p in later.programs
    ]
    return "F01 normalized overlap"


def f02_hybrid_and_unsupported_fill():
    runtime = make_runtime([fixed_raster("raster"), galvo("laser", sample_rate_hz=20000)])
    background = draw("background", "wall", (-0.5, 0.5, 0),
                      geometry=GeometryKind.PATCH, color=(0.1, 0.2, 0.8))
    background.target.u, background.target.v = (1, 0, 0), (0, 0.5, 0)
    list_ = display("hybrid", [background, draw(
        "cursor", "wall", geometry=GeometryKind.POLYLINE,
        vertices=[(-0.2, 1, 0), (0.2, 1, 0)], color=(0, 1, 0))])
    assert not runtime.submit(list_)
    state = runtime.advance(0.1)
    laser = next(p for p in state.programs if p.device_id == "laser")
    assert any(e.kind == EventKind.BLANK for e in laser.events)
    background_programs = [p for p in state.programs if any(e.draw_id == "background" for e in p.events)]
    assert background_programs and all(p.device_id == "raster" for p in background_programs)

    def lower_patch_contribution(mode):
        options = RuntimeOptions()
        options.pipeline.sample_spacing = 0.25
        runtime = OpticalRuntime(world([plane("wall")]), rig([fixed_raster("raster")]), options)
        patch = draw("same-bin", "wall", (-0.5, 0.5, 0),
                     geometry=GeometryKind.PATCH, color=(0.2, 0, 0))
        patch.target.u, patch.target.v = (1, 0, 0), (0, 0.5, 0)
        cursor = draw("same-bin", "wall", geometry=GeometryKind.POLYLINE,
                      vertices=[(-0.5, 1, 0), (0.5, 1, 0)],
                      color=(0, 0.3, 0), opacity=1.0)
        assert not runtime.submit(display("patch-app", [patch], app_id="patch-app"))
        assert not runtime.submit(display("cursor-app", [cursor], app_id="cursor-app",
                                          composition=mode))
        return runtime.advance(0.1).contributions

    def center_patch(rows):
        return next(row for row in rows if row.display_list_id == "patch-app" and
                    all(abs(a - b) < 1e-8 for a, b in zip(row.position, (0, 1, 0))))

    over_rows = lower_patch_contribution(Composition.OVER)
    add_rows = lower_patch_contribution(Composition.ADD)
    assert center_patch(over_rows).intensity == 0
    assert center_patch(add_rows).intensity > 0

    identities = OpticalRuntime(world([plane("wall")]), rig([fixed_raster("p")]))
    assert not identities.submit(display("left-source", [draw("shared-id", "wall", (-0.5, 1, 0))],
                                          app_id="app-left"))
    assert not identities.submit(display("right-source", [draw("shared-id", "wall", (0.5, 1, 0))],
                                          app_id="app-right"))
    identified = identities.advance(0.1)
    assert {(c.display_list_id, tuple(c.position)) for c in identified.contributions} == {
        ("left-source", (-0.5, 1.0, 0.0)), ("right-source", (0.5, 1.0, 0.0))
    }
    assert {t.display_list_id for t in identified.targets} == {"left-source", "right-source"}
    fill = draw("patch-fill", "wall", (-0.5, 0.5, 0), geometry=GeometryKind.PATCH)
    fill.target.u, fill.target.v = (1, 0, 0), (0, 1, 0)
    rejected = make_runtime([galvo("fill-laser")])
    result = rejected.submit(display("laser-fill", [fill]))
    assert any(d.status == PlanStatus.UNSUPPORTED for d in result)
    unsupported = display("fill", [draw("volume", "wall", geometry=GeometryKind.VOLUME)])
    diagnostics = runtime.submit(unsupported)
    assert any(d.status == PlanStatus.UNSUPPORTED for d in diagnostics)
    return "F02 raster and blanked galvo; unsupported fill rejected"


def f03_shared_steering_contention():
    resource = Resource()
    resource.id, resource.capacity = "mirror", 1
    left = steerable_raster("app-a", resource_ids=["mirror"])
    right = steerable_raster("app-b", resource_ids=["mirror"])
    runtime = make_runtime([left, right], resources=[resource])
    first = display("app-a-list", [draw("a", "wall", (-0.5, 1, 0))], app_id="app-a")
    second = display("app-b-list", [draw("b", "wall", (0.5, 1, 0))], app_id="app-b")
    assert not runtime.submit(first)
    assert not runtime.submit(second)
    state = runtime.advance(0.1)
    assert state.programs
    assert any(d.status in (PlanStatus.NO_PLAN_FOUND, PlanStatus.INFEASIBLE)
               for d in state.diagnostics)
    assert len({p.device_id for p in state.programs}) == 1
    ordered = sorted(state.programs, key=lambda p: p.starts_at)
    for previous, following in zip(ordered, ordered[1:]):
        previous_end = max(event.time + event.dwell for event in previous.events)
        assert following.starts_at >= previous_end
    return "F03 shared resource contention reported"


def f04_delayed_moving_plane():
    motion = PlanarMotion()
    motion.linear_velocity = (0.1, 0, 0)
    moving = world([plane("wall")], valid_until=2.0,
                   motion=[motion])
    device = steerable_raster("moving", latency=0.2)
    device.settling_time = 0.1
    runtime = make_runtime([device], source_world=moving)
    local = draw("local", "wall", (0.5, 0.5, 0), geometry=GeometryKind.POINT)
    from spatialgl.optics import CoordinateSpace
    local.target.coordinates = CoordinateSpace.SURFACE_LOCAL
    assert not runtime.submit(display("motion", [local]))
    assert not runtime.advance(0.1).contributions
    assert not runtime.advance(0.25).contributions
    current = runtime.advance(0.5)
    assert current.programs and abs(current.programs[0].starts_at - 0.2) < 1e-8
    emitted_events = [event.time for event in current.programs[0].events if event.kind == EventKind.EMIT]
    assert emitted_events and emitted_events[0] >= 0.3
    assert abs(current.contributions[0].position[0] - 0.05) < 1e-8
    emitted = runtime.advance(0.75)
    assert abs(emitted.contributions[0].position[0] - 0.075) < 1e-8
    expired_motion = PlanarMotion()
    expired_motion.linear_velocity = (0.1, 0, 0)
    expired = world([plane("wall")], source_time=0, valid_until=0.2,
                    world_revision=2, calibration_revision=1, motion=[expired_motion])
    runtime.update_world(expired)
    state = runtime.advance(0.8)
    assert any(d.code in ("stale-world", "revision-changed") for d in state.diagnostics)
    return "F04 motion latency and expired-world invalidation"


def f05_async_scan_pair():
    fast = galvo("fast", sample_rate_hz=30000, latency=0.01)
    slow = galvo("slow", position=(0.2, 1, 3), target=(0.2, 1, 0),
                 sample_rate_hz=10000, latency=0.08)
    fast.clock_offset, fast.clock_drift = 0.003, 0.0002
    slow.clock_offset, slow.clock_drift = -0.002, -0.0001
    fast.clock_uncertainty = slow.clock_uncertainty = 0.002
    presentation = PresentationState()
    runtime = make_runtime([fast, slow], output_mix=OutputMix.ADDITIVE, presentation=presentation)
    assert not runtime.submit(display("pair", [draw("one", "wall", (-0.2, 1, 0)),
                                                draw("two", "wall", (0.2, 1, 0))]))
    state = runtime.advance(0.1)
    assert {p.device_id for p in state.programs} == {"fast", "slow"}
    assert min(p.starts_at for p in state.programs) < max(p.starts_at for p in state.programs)
    assert not state.diagnostics
    for program in state.programs:
        blank = next(event for event in program.events if event.kind == EventKind.BLANK)
        emitted = next(event for event in program.events
                       if event.kind == EventKind.EMIT and event.draw_id == blank.draw_id)
        device = next(item for item in (fast, slow) if item.id == program.device_id)
        local_delta = emitted.time - blank.time
        global_delta = ((emitted.time - device.clock_offset) -
                        (blank.time - device.clock_offset)) / (1 + device.clock_drift)
        assert abs(local_delta - blank.dwell) < 1e-8
        assert abs(global_delta - blank.dwell / (1 + device.clock_drift)) < 1e-8
        assert abs(local_delta - global_delta) > 1e-10
        assert emitted.display_list_id == program.display_list_id
        assert emitted.surface_id == "wall"
    tight = PresentationState()
    tight.mode, tight.require_sync = Presentation.SYNCHRONIZED_GROUP, True
    tight.requested_group_skew = 0.001
    impossible = make_runtime([fast, slow], output_mix=OutputMix.ADDITIVE, presentation=tight)
    assert not impossible.submit(display("tight-pair", [draw("one", "wall", (-0.2, 1, 0)),
                                                         draw("two", "wall", (0.2, 1, 0))]))
    rejected = impossible.advance(0.1)
    assert not rejected.programs and not rejected.contributions
    assert any(d.status in (PlanStatus.UNSUPPORTED, PlanStatus.INFEASIBLE, PlanStatus.NO_PLAN_FOUND)
               for d in rejected.diagnostics)
    return "F05 asynchronous programs expose distinct start timing"


def f06_cancel_fault_and_revision():
    runtime = make_runtime([fixed_raster("projector")])
    assert not runtime.submit(display("cancel-me", [draw("dot", "wall")]))
    original = runtime.advance(0.1)
    old_revision = original.world_revision
    generation = runtime.cancel("cancel-me")
    runtime.set_availability("projector", Availability.FAILED)
    next_state = runtime.advance(0.2)
    assert not next_state.programs
    assert next_state.receipts[0].generation >= generation
    updated = world([plane("wall")], valid_until=100, world_revision=2,
                    calibration_revision=2)
    runtime.update_world(updated)
    retained = original
    assert retained.world_revision == old_revision == 1
    assert retained.programs  # A returned snapshot owns its data.
    return "F06 cancellation, fault state, and snapshot provenance"


def composition_outcomes():
    def aggregate(mode):
        options = RuntimeOptions()
        options.pipeline.output_mix = OutputMix.ADDITIVE
        runtime = OpticalRuntime(world([plane("wall")]), rig([fixed_raster("p")]), options)
        lower = display("lower", [draw("same", "wall", color=(0.25, 0, 0))],
                        app_id="lower", composition=Composition.OVER)
        upper = display("upper", [draw("same", "wall", color=(0, 0.25, 0), opacity=0.5)],
                        app_id="upper", composition=mode)
        assert not runtime.submit(lower)
        assert not runtime.submit(upper)
        lights = runtime.advance(0.1).targets
        return tuple(sum(x.aggregate_linear_rgb[i] for x in lights) for i in range(3))

    over, replace, add = (aggregate(mode) for mode in (
        Composition.OVER, Composition.REPLACE, Composition.ADD,
    ))
    assert all(abs(a - b) < 1e-8 for a, b in zip(over, (0.125, 0.125, 0)))
    assert all(abs(a - b) < 1e-8 for a, b in zip(replace, (0, 0.125, 0)))
    assert all(abs(a - b) < 1e-8 for a, b in zip(add, (0.25, 0.125, 0)))
    return "composition modes produce distinct target light"


def main():
    scenarios = [f01_overlap_occlusion, f02_hybrid_and_unsupported_fill,
                 f03_shared_steering_contention, f04_delayed_moving_plane,
                 f05_async_scan_pair, f06_cancel_fault_and_revision, composition_outcomes]
    for scenario in scenarios:
        print(scenario())


if __name__ == "__main__":
    main()
