import json
from http.server import HTTPServer
from pathlib import Path
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from demo import make_handler
from spatialgl import Point, Runtime, SceneFrame, World
from spatialgl.scenario import Experiment, Settings
from spatialgl.view import write_view


class ExperimentTest(unittest.TestCase):
    def test_capacity_scenario_and_settings(self):
        e = Experiment(Settings(scenario='air', count=1))
        self.assertEqual(sum(s.status == 'unrealizable' for s in e.step(0.2).samples), 2)
        with self.assertRaises(ValueError):
            Settings(latency=float('nan'))
        with self.assertRaises(ValueError):
            Settings(scenario='unknown')
        with self.assertRaises(ValueError):
            e.step(-1)

    def test_occluder_intersects_the_default_projection_paths(self):
        e = Experiment(Settings(scenario='wall', occlusion=True))
        blocked = [s for s in e.step(0.2).samples if s.status == 'unrealizable']
        self.assertGreater(len(blocked), 0)
        self.assertTrue(any('occluded' in reason for s in blocked for reason in s.reasons))

    def test_sampled_scene_timing_is_step_independent(self):
        a, b = Experiment(), Experiment()
        a.step(0.5)
        for _ in range(10):
            b.step(0.05)
        self.assertAlmostEqual(a.state.time, b.state.time)
        # At a frame boundary, accumulated IEEE rounding can affect which scene
        # is submitted; use an interior observation to compare geometry.
        a.step(0.03)
        b.step(0.03)
        self.assertEqual(a.state.scene_id, b.state.scene_id)
        for x, y in zip(a.state.samples, b.state.samples):
            if x.actual is not None:
                for left, right in zip(x.actual, y.actual):
                    self.assertAlmostEqual(left, right, places=8)

    def test_standalone_view_escapes_embedded_script_content(self):
        world = World()
        r = Runtime(world, [])
        r.submit(SceneFrame('</script><script>BAD</script>', 0, 1, [Point('dot', (0, 1, 0))]))
        with tempfile.TemporaryDirectory() as directory:
            path = write_view(Path(directory) / 'view.html', r.advance(0), world)
            html = path.read_text()
            self.assertNotIn('/*__SPATIALGL_DATA__*/null', html)
            self.assertNotIn('</script><script>BAD', html)
            self.assertIn('\\u003c/script>', html)


class ServerTest(unittest.TestCase):
    def setUp(self):
        self.server = HTTPServer(('127.0.0.1', 0), make_handler())
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f'http://127.0.0.1:{self.server.server_port}'

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def call(self, path, body=None):
        request = Request(self.base + path, data=None if body is None else json.dumps(body).encode(),
                          headers={} if body is None else {'Content-Type': 'application/json'})
        with urlopen(request, timeout=2) as response:
            return json.loads(response.read())

    def test_live_cpp_observations_and_trace_download(self):
        self.assertEqual(self.call('/api/state')['state']['time'], 0)
        self.assertEqual(self.call('/api/step', {'dt': 0.1})['state']['time'], 0.1)
        with urlopen(self.base + '/api/trace', timeout=2) as response:
            self.assertIn('attachment', response.headers['Content-Disposition'])
            data = json.loads(response.read())
            self.assertEqual(len(data['trace']), 2)
            self.assertEqual(data['schema_version'], 2)

    def test_failed_reset_preserves_the_previous_experiment(self):
        self.call('/api/step', {'dt': 0.3})
        with self.assertRaises(HTTPError) as error:
            self.call('/api/reset', {'scenario': 'invalid'})
        self.assertEqual(error.exception.code, 400)
        self.assertEqual(self.call('/api/state')['state']['time'], 0.3)


if __name__ == '__main__':
    unittest.main()
