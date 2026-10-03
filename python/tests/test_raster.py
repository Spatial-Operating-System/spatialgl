import json
import io
import contextlib
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from spatialgl.optics import OpticalRuntime, fixed_raster, plane, rig, world, draw, display
from spatialgl.raster import (
    Calibration, GlfwOutput, RasterFrame, TransferFunction, calibration_from_corners, compile,
)
from examples.hardware import raster_demo


class RasterTests(unittest.TestCase):
    def make_runtime(self):
        device = fixed_raster("test", position=(0, 1, 3), target=(0, 1, 0), width=96, height=64)
        runtime = OpticalRuntime(world([plane("wall")], valid_until=10), rig([device]))
        return runtime, device

    def test_native_types_and_corner_calibration(self):
        self.assertTrue(callable(GlfwOutput.displays))
        calibration = calibration_from_corners(
            [(0, 0), (1, 0), (1, 1), (0, 1)],
            [(0, 0), (1, 0), (1, 1), (0, 1)],
        )
        self.assertIsInstance(calibration, Calibration)
        self.assertEqual(calibration.transfer, TransferFunction.SRGB)
        with self.assertRaises(ValueError):
            calibration_from_corners([(0, 0)], [(0, 0)])

    def test_frame_bytes_are_immutable_and_expiry_blanks(self):
        runtime, device = self.make_runtime()
        runtime.submit(display("short", [draw("dot", "wall")], expires_at=0.1))
        live = compile(runtime.advance(0.05), device)
        self.assertIsInstance(live, RasterFrame)
        frozen = live.rgb
        self.assertIsInstance(frozen, bytes)
        self.assertEqual(len(frozen), 96 * 64 * 3)
        self.assertTrue(any(frozen), "accepted live optics snapshot must render nonzero pixels")
        expired = compile(runtime.advance(0.2), device)
        self.assertEqual(frozen, live.rgb)
        self.assertEqual(set(expired.rgb), {0})
        submitted = runtime.submit(display("cancelled", [draw("another-dot", "wall")],
                                            present_at=0.2, expires_at=1.0))
        self.assertFalse(submitted, "cancellation fixture must be accepted by the runtime")
        before_cancel = compile(runtime.advance(0.25), device)
        self.assertTrue(any(before_cancel.rgb), "accepted pre-cancel snapshot must render nonzero pixels")
        runtime.cancel("cancelled")
        cancelled = compile(runtime.advance(0.3), device)
        self.assertEqual(set(cancelled.rgb), {0})

    def test_headless_cli_exports_ppm_and_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(raster_demo.main(["--output", directory, "--width", "96", "--height", "64"]), 0)
            image = (Path(directory) / "frame.ppm").read_bytes()
            self.assertTrue(image.startswith(b"P6\n96 64\n255\n"))
            payload = image.split(b"\n", 3)[3]
            self.assertTrue(any(payload), "headless PPM payload must contain rendered pixels")
            metadata = json.loads((Path(directory) / "metadata.json").read_text())
            self.assertEqual(metadata["provenance"], "PREDICTED")
            self.assertEqual(metadata["optical_feedback"], "UNAVAILABLE")
            self.assertEqual(metadata["rgb_bytes"], 96 * 64 * 3)
            self.assertEqual(metadata["diagnostic_codes"], [])

    def test_corner_calibration_changes_rendered_pixel_coordinates(self):
        def nonzero_bounds(payload, width, height):
            coordinates = []
            for y in range(height):
                for x in range(width):
                    offset = (y * width + x) * 3
                    if any(payload[offset:offset + 3]):
                        coordinates.append((x, y))
            self.assertTrue(coordinates)
            return (min(x for x, _ in coordinates), min(y for _, y in coordinates),
                    max(x for x, _ in coordinates), max(y for _, y in coordinates))

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            identity_dir, scaled_dir = root / "identity", root / "scaled"
            self.assertEqual(raster_demo.main(["--output", str(identity_dir), "--width", "96", "--height", "64"]), 0)
            calibration_path = root / "calibration.json"
            calibration_path.write_text(json.dumps({
                "source_corners": [[0, 0], [1, 0], [1, 1], [0, 1]],
                "destination_corners": [[0.25, 0.25], [0.75, 0.25], [0.75, 0.75], [0.25, 0.75]],
            }))
            self.assertEqual(raster_demo.main([
                "--output", str(scaled_dir), "--width", "96", "--height", "64",
                "--calibration", str(calibration_path),
            ]), 0)
            identity = (identity_dir / "frame.ppm").read_bytes().split(b"\n", 3)[3]
            scaled = (scaled_dir / "frame.ppm").read_bytes().split(b"\n", 3)[3]
            self.assertNotEqual(identity, scaled)
            identity_bounds = nonzero_bounds(identity, 96, 64)
            scaled_bounds = nonzero_bounds(scaled, 96, 64)
            self.assertLess(scaled_bounds[2] - scaled_bounds[0], identity_bounds[2] - identity_bounds[0])
            self.assertLess(scaled_bounds[3] - scaled_bounds[1], identity_bounds[3] - identity_bounds[1])

    def test_window_presents_with_fresh_non_decreasing_scene_time(self):
        class FakeOutput:
            def __init__(self):
                self.times = []
                self.closed = False
            def present(self, frame, now):
                self.times.append((frame.time, now))
                return True
            def poll(self, now):
                return True
            def blackout(self):
                pass
            def close(self):
                self.closed = True

        clock_values = iter([10.0, 10.1, 10.12])
        output = FakeOutput()
        with patch.object(raster_demo, "_frame", return_value=SimpleNamespace(time=0.1)):
            raster_demo._run_window(output, None, None, None, 0.11,
                                    clock=lambda: next(clock_values), sleep=lambda _: None)
        self.assertEqual(len(output.times), 1)
        self.assertAlmostEqual(output.times[0][0], 0.1)
        self.assertAlmostEqual(output.times[0][1], 0.12)
        self.assertGreaterEqual(output.times[0][1], output.times[0][0])
        self.assertTrue(output.closed)

    def test_window_closes_if_blackout_raises(self):
        class FailingOutput:
            closed = False
            def present(self, frame, now):
                return False
            def poll(self, now):
                return True
            def blackout(self):
                raise RuntimeError("blackout failed")
            def close(self):
                self.closed = True

        clock_values = iter([20.0, 20.1, 20.12])
        output = FailingOutput()
        with patch.object(raster_demo, "_frame", return_value=SimpleNamespace(time=0.1)):
            with self.assertRaisesRegex(RuntimeError, "blackout failed"):
                raster_demo._run_window(output, None, None, None, 1.0,
                                        clock=lambda: next(clock_values), sleep=lambda _: None)
        self.assertTrue(output.closed)

    def test_invalid_cli_and_calibration_fail_clearly(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(raster_demo.main(["--width", "10"]), 2)
            malformed = [
                {"homography": [1, 2]},
                {"homography": [True, 0, 0, 0, 1, 0, 0, 0, 1]},
                {"homography": ["1", 0, 0, 0, 1, 0, 0, 0, 1]},
                {"destination_corners": [[0, 0], [1, 0], [1, 1], [0, 1]]},
                {"source_corners": [[0, 0], [1, 0], [1, 1], [0, 1]]},
                {"source_corners": [[0, 0]], "destination_corners": [[0, 0]]},
                {"source_corners": [[True, 0]] * 4, "destination_corners": [[0, 0]] * 4},
                {"source_corners": [["0", 0]] * 4, "destination_corners": [[0, 0]] * 4},
                {"source_corners": [[float("nan"), 0]] * 4, "destination_corners": [[0, 0]] * 4},
                {"homography": [1, 0, 0, 0, 1, 0, 0, 0, 1],
                 "source_corners": [[0, 0]] * 4, "destination_corners": [[0, 0]] * 4},
            ]
            for index, data in enumerate(malformed):
                path = Path(directory) / f"bad-{index}.json"
                path.write_text(json.dumps(data))
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(raster_demo.main(["--calibration", str(path)]), 2, repr(data))


if __name__ == "__main__":
    unittest.main()
