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


if __name__ == '__main__':
    unittest.main()
