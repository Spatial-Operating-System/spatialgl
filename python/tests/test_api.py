import json
import unittest
from spatialgl import Drones, Monitor, Patch, Point, Polyline, Projector, Runtime, SceneFrame, Surface, World


class ApiTest(unittest.TestCase):
    def test_cpp_module_and_python_keywords(self):
        import spatialgl._native as native
        self.assertTrue(native.__file__.endswith(('.so', '.pyd')))
        runtime = Runtime(World(), [Drones('drone', count=1, speed=1, home=(0, 1, 0), latency=0.1)])
        runtime.submit(SceneFrame('scene', present_at=0, expires_at=3,
                                 primitives=[Point('dot', position=(1, 1, 0))]))
        state = runtime.advance(0.5)
        self.assertEqual(state.samples[0].status, 'tracking')
        self.assertAlmostEqual(state.samples[0].actual[0], 0.4)
        self.assertAlmostEqual(state.samples[0].error, 0.6)
        self.assertEqual(runtime.advance(1.1).samples[0].status, 'realized')

    def test_python_snapshot_and_json(self):
        runtime = Runtime(World(), [Drones('drone', count=1, home=(0, 1, 0))])
        runtime.submit(SceneFrame('scene', 0, 1, [Point('dot', (0, 1, 0))]))
        old = runtime.advance(0)
        serialized = json.loads(json.dumps(old.to_dict(), allow_nan=False))
        self.assertEqual(serialized['samples'][0]['sample']['kind'], 'point')
        self.assertEqual(serialized['device_frames'][0]['command']['kind'], 'emitter-targets')
        self.assertEqual(runtime.advance(1).outputs, [])
        self.assertEqual(len(old.outputs), 1)

    def test_all_primitives_and_raster_backends(self):
        world = World([Surface('wall', (-2, 0, 0), (4, 0, 0), (0, 3, 0))])
        runtime = Runtime(world, [Projector('projector'), Monitor('monitor', surface_id='wall')])
        runtime.submit(SceneFrame('scene', 0, 1, [
            Polyline('line', [(-1, 1, 0), (1, 1, 0)], surface_id='wall'),
            Patch('patch', (-1, 1.5, 0), (1, 0, 0), (0, 0.5, 0), surface_id='wall'),
        ]))
        state = runtime.advance(0)
        self.assertTrue(all(s.device_id == 'monitor' for s in state.samples))
        frame = next(f for f in state.device_frames if f.device_id == 'monitor')
        self.assertEqual(frame.to_dict()['command']['kind'], 'raster-samples')

    def test_copy_input_and_backend_lifetime(self):
        p = Point('dot', (0, 1, 0))
        f = SceneFrame('scene', 0, 1, [p])
        b = Drones('drone', count=1, home=(0, 1, 0))
        runtime = Runtime(World(), [b])
        runtime.submit(f)
        f.expires_at = 0.01
        p.position = (2, 2, 2)
        del b, f, p
        self.assertEqual(runtime.advance(0.5).samples[0].status, 'realized')

    def test_errors_are_python_value_errors(self):
        with self.assertRaises(ValueError):
            Projector('bad', fov_y=float('nan'))
        with self.assertRaises(ValueError):
            Drones('bad', speed=0)
        r = Runtime(World(), [])
        with self.assertRaises(ValueError):
            r.advance(-1)

    def test_capacity_and_support_failures(self):
        runtime = Runtime(World(), [Drones('drone', count=1)])
        runtime.submit(SceneFrame('scene', 0, 1, [Point('a', (0, 1, 0)), Point('b', (1, 1, 0))]))
        self.assertEqual(runtime.advance(0).samples[1].reasons, ['drone: capacity'])
        r = Runtime(World(), [Projector('projector')])
        r.submit(SceneFrame('air', 0, 1, [Point('dot', (0, 1, 1))]))
        self.assertEqual(r.advance(0).samples[0].reasons, ['projector: no-support-surface'])

    def test_optics_submit_copies_values_and_snapshots_are_independent(self):
        from spatialgl.optics import OpticalRuntime, draw, display, fixed_raster, plane, rig, world

        frame = display('copied', [draw('mark', 'wall')])
        runtime = OpticalRuntime(world([plane('wall')]), rig([fixed_raster('p')]))
        self.assertEqual(runtime.submit(frame), [])
        frame.draws[0].draw_id = 'mutated-after-submit'
        first = runtime.advance(0.1)
        self.assertEqual(first.targets[0].draw_id, 'mark')
        runtime.cancel('copied')
        runtime.advance(0.2)
        self.assertEqual(first.targets[0].draw_id, 'mark')
        self.assertEqual(first.world_revision, 1)

    def test_optics_validation_rejects_bad_intervals_and_colors(self):
        from spatialgl.optics import DisplayList, GeometryKind, PlanStatus, draw, validate_display

        invalid_interval = DisplayList()
        invalid_interval.id, invalid_interval.present_at, invalid_interval.expires_at = 'bad', 2, 1
        self.assertTrue(any(d.status == PlanStatus.INFEASIBLE for d in validate_display(invalid_interval)))
        bad_color = draw('bad-color', 'wall', color=(-1, 0, 0))
        invalid = DisplayList()
        invalid.id, invalid.present_at, invalid.expires_at, invalid.draws = 'bad-color', 0, 1, [bad_color]
        self.assertTrue(any(d.code == 'invalid-color' for d in validate_display(invalid)))
        unsupported = draw('volume', 'wall', geometry=GeometryKind.VOLUME)
        invalid.draws = [unsupported]
        self.assertTrue(any(d.status == PlanStatus.UNSUPPORTED for d in validate_display(invalid)))

    def test_optics_observation_values_are_readonly(self):
        from spatialgl.optics import OpticalRuntime, draw, display, fixed_raster, plane, rig, world

        runtime = OpticalRuntime(world([plane('wall')]), rig([fixed_raster('p')]))
        runtime.submit(display('immutable', [draw('mark', 'wall')]))
        observed = runtime.advance(0.1)
        with self.assertRaises(AttributeError):
            observed.time = 9
        with self.assertRaises(AttributeError):
            observed.targets[0].draw_id = 'changed'
        with self.assertRaises(AttributeError):
            observed.programs[0].events[0].time = 9
        self.assertEqual(observed.programs[0].events[0].display_list_id, 'immutable')
        self.assertEqual(observed.programs[0].events[0].surface_id, 'wall')
        self.assertEqual(observed.contributions[0].display_list_id, 'immutable')
        self.assertEqual(observed.targets[0].display_list_id, 'immutable')
        with self.assertRaises(AttributeError):
            observed.contributions[0].surface_id = 'other'
        retained = runtime.snapshot()
        self.assertEqual(retained.programs[0].events[0].time, observed.programs[0].events[0].time)

    def test_optics_invalid_profiles_geometry_policies_and_budgets(self):
        from spatialgl.optics import (
            DeviceProfile, GeometryKind, PlanStatus, RuntimeOptions, draw, fixed_raster,
            plane, rig, validate_display, validate_world, world,
        )

        scene = world([plane('wall')])
        invalid_profiles = []
        for field, value in (
            ('fov_y', float('nan')), ('fov_y', 0.0), ('fov_y', 4.0),
            ('fov_y', float('inf')),
        ):
            profile = fixed_raster('camera')
            setattr(profile, field, value)
            invalid_profiles.append(profile)
        wrong_clip_order = fixed_raster('camera')
        wrong_clip_order.near_plane, wrong_clip_order.far_plane = 2.0, 1.0
        invalid_profiles.append(wrong_clip_order)
        coincident = fixed_raster('camera')
        coincident.target = coincident.position
        invalid_profiles.append(coincident)
        parallel_up = fixed_raster('camera')
        parallel_up.up = (0, 0, -1)
        invalid_profiles.append(parallel_up)
        for profile in invalid_profiles:
            problems = validate_world(scene, rig([profile]))
            self.assertTrue(any(d.status == PlanStatus.INFEASIBLE for d in problems),
                            [(d.code, d.message) for d in problems])

        for axes in (((float('nan'), 0, 0), (0, 1, 0)), ((0, 0, 0), (0, 1, 0))):
            patch_draw = draw('patch', 'wall', geometry=GeometryKind.PATCH)
            patch_draw.target.u, patch_draw.target.v = axes
            from spatialgl.optics import display
            self.assertTrue(any(d.status == PlanStatus.INFEASIBLE for d in
                                validate_display(display('bad-patch', [patch_draw]))))

        for mutation in (
            lambda options: setattr(options.pipeline, 'max_position_error', float('nan')),
            lambda options: setattr(options.pipeline, 'max_position_error', -1),
            lambda options: setattr(options.traversal, 'max_dark_gap', float('inf')),
            lambda options: setattr(options.traversal, 'max_dark_gap', -1),
            lambda options: setattr(options.traversal, 'minimum_duty', float('nan')),
            lambda options: setattr(options.traversal, 'minimum_duty', 1.1),
            lambda options: setattr(options.pipeline, 'max_samples', 1_000_001),
            lambda options: setattr(options, 'planner_candidate_budget', 0),
        ):
            options = RuntimeOptions()
            mutation(options)
            self.assertTrue(any(d.status == PlanStatus.INFEASIBLE for d in
                                validate_world(scene, rig([fixed_raster('p')]), options)))

        enormous_device = fixed_raster('p')
        enormous_device.sample_budget = 1_000_001
        self.assertTrue(any(d.status == PlanStatus.INFEASIBLE for d in
                            validate_world(scene, rig([enormous_device]))))
        duplicate_planes = world([plane('wall'), plane('wall')])
        self.assertTrue(any(d.status == PlanStatus.INFEASIBLE for d in
                            validate_world(duplicate_planes, rig([]))))
        duplicate_devices = rig([fixed_raster('same'), fixed_raster('same')])
        self.assertTrue(any(d.status == PlanStatus.INFEASIBLE for d in
                            validate_world(scene, duplicate_devices)))

    def test_optics_angular_plane_motion_is_modeled_or_rejected(self):
        from spatialgl.optics import (
            PlanarMotion, PlanStatus, OpticalRuntime, CoordinateSpace, draw, display,
            fixed_raster, plane, rig, world,
        )

        motion = PlanarMotion()
        motion.angular_velocity = (0, 0, 0.5)
        try:
            runtime = OpticalRuntime(world([plane('wall')], valid_until=5, motion=[motion]),
                                     rig([fixed_raster('p')]))
        except ValueError as error:
            self.assertIn('angular', str(error).lower())
            return
        target = draw('local', 'wall', (0.5, 0.5, 0))
        target.target.coordinates = CoordinateSpace.SURFACE_LOCAL
        runtime.submit(display('rotation', [target]))
        initial = runtime.advance(0)
        rotated = runtime.advance(1)
        if any(d.status == PlanStatus.UNSUPPORTED and 'angular' in (d.code + d.message).lower()
               for d in rotated.diagnostics):
            self.assertFalse(rotated.contributions)
        else:
            self.assertTrue(rotated.contributions, [(d.code, d.message) for d in rotated.diagnostics])
            self.assertNotEqual(rotated.contributions[0].position, initial.contributions[0].position)

    def test_optics_same_app_cancellation_and_expiry_do_not_resurrect_old_list(self):
        from spatialgl.optics import OpticalRuntime, draw, display, fixed_raster, plane, rig, world

        runtime = OpticalRuntime(world([plane('wall')]), rig([fixed_raster('p')]))
        old = display('old', [draw('old-draw', 'wall')], present_at=0,
                      expires_at=10, app_id='one-app')
        latest = display('latest', [draw('latest-draw', 'wall')], present_at=1,
                         expires_at=1.5, app_id='one-app')
        self.assertEqual(runtime.submit(old), [])
        self.assertEqual(runtime.submit(latest), [])
        self.assertEqual([c.draw_id for c in runtime.advance(0.5).contributions], ['old-draw'])
        self.assertEqual([c.draw_id for c in runtime.advance(1.2).contributions], ['latest-draw'])
        runtime.cancel('latest')
        self.assertFalse(runtime.advance(1.3).contributions)
        self.assertFalse(runtime.advance(1.6).contributions)

    def test_optics_duplicate_display_ids_have_explicit_policy(self):
        from spatialgl.optics import PlanStatus, OpticalRuntime, draw, display, fixed_raster, plane, rig, world

        runtime = OpticalRuntime(world([plane('wall')]), rig([fixed_raster('p')]))
        original = display('same-id', [draw('first', 'wall')], app_id='a')
        duplicate = display('same-id', [draw('second', 'wall')], app_id='b')
        self.assertEqual(runtime.submit(original), [])
        result = runtime.submit(duplicate)
        self.assertTrue(any(d.status == PlanStatus.INFEASIBLE and 'duplicate' in d.code
                            for d in result), [(d.code, d.message) for d in result])
        self.assertEqual([c.draw_id for c in runtime.advance(0.1).contributions], ['first'])

    def test_optics_steerable_programs_serialize_and_gate_emission(self):
        from spatialgl.optics import (
            EventKind, OpticalRuntime, CoordinateSpace, draw, display, plane, rig,
            steerable_raster, world,
        )

        device = steerable_raster('mirror', max_angular_speed=1.0, settling_time=0.1)
        device.clock_offset, device.clock_drift = 0.02, 0.0001
        def make():
            runtime = OpticalRuntime(world([plane('wall')]), rig([device]))
            left = draw('left', 'wall', (-0.5, 1, 0))
            right = draw('right', 'wall', (0.5, 1, 0))
            runtime.submit(display('left-list', [left], app_id='left-app'))
            runtime.submit(display('right-list', [right], app_id='right-app'))
            return runtime

        probe = make().advance(3.0)
        by_id = {program.display_list_id: program for program in probe.programs}
        self.assertEqual(set(by_id), {'left-list', 'right-list'})
        def first_global_emit(program):
            event = next(e for e in program.events if e.kind == EventKind.EMIT)
            return (event.time - device.clock_offset) / (1 + device.clock_drift)
        ordered = sorted(((key, first_global_emit(by_id[key]))
                          for key in ('left-list', 'right-list')), key=lambda pair: pair[1])
        (first_id, first_time), (second_id, second_time) = ordered
        self.assertLess(first_time, second_time)
        global_time = lambda local: (local - device.clock_offset) / (1 + device.clock_drift)
        first_end = global_time(max(e.time + e.dwell for e in by_id[first_id].events))
        second_start = global_time(by_id[second_id].events[0].time)
        self.assertGreaterEqual(second_start, first_end)

        before_first = make().advance(first_time - 1e-5)
        self.assertFalse(before_first.contributions)
        between = make().advance((first_time + second_time) / 2)
        self.assertEqual({c.draw_id for c in between.contributions},
                         {first_id.removesuffix('-list')})
        before_second = make().advance(second_time - 1e-5)
        self.assertNotIn(second_id.removesuffix('-list'), {c.draw_id for c in before_second.contributions})
        after_second = make().advance(second_time + 1e-4)
        self.assertEqual({c.draw_id for c in after_second.contributions}, {'left', 'right'})

    def test_optics_galvo_aggregate_light_applies_scan_duty(self):
        from spatialgl.optics import (
            GeometryKind, OpticalRuntime, OutputMix, RuntimeOptions, draw, display,
            galvo, plane, rig, world,
        )

        options = RuntimeOptions()
        options.pipeline.output_mix = OutputMix.EXCLUSIVE
        options.pipeline.sample_spacing = 0.5
        runtime = OpticalRuntime(world([plane('wall')]), rig([galvo('laser')]), options)
        line = draw('scan', 'wall', geometry=GeometryKind.POLYLINE,
                    vertices=[(-0.5, 1, 0), (0.5, 1, 0)], color=(0.6, 0.2, 0.1))
        runtime.submit(display('scan-list', [line]))
        snapshot = runtime.advance(0.1)
        light = snapshot.targets[0]
        self.assertIsNotNone(light.duty)
        self.assertLess(light.duty, 1.0)
        self.assertAlmostEqual(light.aggregate_linear_rgb[0], 0.6 * light.duty, places=8)


if __name__ == '__main__':
    unittest.main()
