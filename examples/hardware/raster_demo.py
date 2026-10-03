"""First fixed-raster HDMI demo, with native optics and pixel compilation."""
import argparse
import json
import math
import os
from pathlib import Path
import sys
import time

from spatialgl.optics import (
    GeometryKind, OpticalRuntime, OutputMix, RuntimeOptions, draw, display,
    fixed_raster, plane, rig, world,
)
from spatialgl.raster import Calibration, GlfwOutput, TransferFunction, calibration_from_corners, compile, write_ppm


def _calibration(path):
    if path is None:
        return Calibration()
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read calibration JSON: {error}") from error
    if not isinstance(data, dict):
        raise ValueError("calibration JSON must be an object")
    allowed = {"homography", "source_corners", "destination_corners", "point_radius", "transfer", "lease_seconds"}
    if set(data) - allowed:
        raise ValueError("calibration JSON contains unknown fields")
    has_homography = "homography" in data
    has_source, has_destination = "source_corners" in data, "destination_corners" in data
    if has_homography and (has_source or has_destination):
        raise ValueError("provide homography or corner correspondences, not both")
    if not has_homography and (has_source != has_destination):
        raise ValueError("both source_corners and destination_corners are required")
    if not has_homography and not has_source:
        raise ValueError("provide homography or both source_corners and destination_corners")
    radius = data.get("point_radius", 1)
    lease = data.get("lease_seconds", 0.1)
    transfer_name = data.get("transfer", "srgb")
    if transfer_name not in ("srgb", "linear"):
        raise ValueError("transfer must be 'srgb' or 'linear'")
    if isinstance(radius, bool) or not isinstance(radius, int) or radius < 0:
        raise ValueError("point_radius must be a non-negative integer")
    if isinstance(lease, bool) or not isinstance(lease, (int, float)) or not math.isfinite(lease) or lease <= 0:
        raise ValueError("lease_seconds must be finite and positive")
    transfer = TransferFunction.SRGB if transfer_name == "srgb" else TransferFunction.LINEAR
    if "homography" in data:
        values = data["homography"]
        if not isinstance(values, list) or len(values) != 9:
            raise ValueError("homography must be an array of nine numbers")
        if any(isinstance(value, bool) or not isinstance(value, (int, float)) for value in values):
            raise ValueError("homography entries must be JSON numbers")
        try:
            values = [float(value) for value in values]
        except OverflowError as error:
            raise ValueError("homography values must be finite") from error
        if not all(math.isfinite(value) for value in values):
            raise ValueError("homography values must be finite")
        result = Calibration()
        result.homography = values
        result.point_radius, result.lease_seconds, result.transfer = radius, lease, transfer
        return result
    for name in ("source_corners", "destination_corners"):
        points = data[name]
        if not isinstance(points, list) or len(points) != 4:
            raise ValueError(f"{name} must contain exactly four x/y points")
        for point in points:
            if not isinstance(point, list) or len(point) != 2:
                raise ValueError(f"each {name} point must be a two-number array")
            if any(isinstance(value, bool) or not isinstance(value, (int, float)) for value in point):
                raise ValueError(f"{name} entries must be JSON numbers")
            try:
                normalized = [float(value) for value in point]
            except OverflowError as error:
                raise ValueError(f"{name} coordinates must be finite and within [0, 1]") from error
            if any(not math.isfinite(value) or not 0 <= value <= 1 for value in normalized):
                raise ValueError(f"{name} coordinates must be finite and within [0, 1]")
    try:
        return calibration_from_corners(data["source_corners"], data["destination_corners"],
                                        point_radius=radius, transfer=transfer, lease_seconds=lease)
    except (TypeError, IndexError, KeyError) as error:
        raise ValueError("corner correspondences must be arrays of x/y points") from error


def _scene(width, height):
    device = fixed_raster("hdmi", position=(0, 1.5, 4), target=(0, 1.5, 0),
                          width=width, height=height, sample_rate_hz=60)
    device.sample_budget = 20000
    options = RuntimeOptions()
    options.pipeline.output_mix = OutputMix.NORMALIZED
    options.pipeline.sample_spacing = 0.16
    options.pipeline.max_samples = 20000
    options.planner_candidate_budget = 20000
    runtime = OpticalRuntime(world([plane("screen", origin=(-2, 0, 0), u=(4, 0, 0), v=(0, 3, 0))],
                                    valid_until=1e9), rig([device]), options)
    return runtime, device


def _draws(t):
    items = []
    # Sparse grid polylines keep the requested sample count bounded.
    for i in range(1, 8):
        x = -1.75 + i * 0.5
        items.append(draw(f"grid-v-{i}", "screen", geometry=GeometryKind.POLYLINE,
                          vertices=[(x, 0.15, 0), (x, 2.85, 0)], color=(0.07, 0.13, 0.18), intensity=0.8))
    for i in range(1, 6):
        y = i * 0.5
        items.append(draw(f"grid-h-{i}", "screen", geometry=GeometryKind.POLYLINE,
                          vertices=[(-1.9, y, 0), (1.9, y, 0)], color=(0.07, 0.13, 0.18), intensity=0.8))
    x = 0.9 * math.sin(t * 1.8)
    y = 1.5 + 0.65 * math.cos(t * 1.2)
    items.append(draw("marker", "screen", (x, y, 0), color=(0.1, 0.75, 1.0), intensity=1.0))
    return items


def _metadata(frame):
    return {
        "device_id": frame.device_id,
        "time": frame.time,
        "expires_at": frame.expires_at,
        "world_revision": frame.world_revision,
        "calibration_revision": frame.calibration_revision,
        "width": frame.width,
        "height": frame.height,
        "rgb_bytes": len(frame.rgb),
        "diagnostics": [d.message for d in frame.diagnostics],
        "diagnostic_codes": [d.code for d in frame.diagnostics],
        "provenance": "PREDICTED",
        "optical_feedback": "UNAVAILABLE",
        "rendered_geometry": ["sampled polylines", "points"],
        "coverage": "sparse; no continuous fill",
    }


def _frame(runtime, device, calibration, scene_time):
    dl = display(f"demo-{scene_time:.6f}", _draws(scene_time), present_at=scene_time,
                 expires_at=scene_time + calibration.lease_seconds, app_id="raster-demo")
    diagnostics = runtime.submit(dl)
    if diagnostics:
        raise RuntimeError("optics planner rejected demo frame: " + "; ".join(d.message for d in diagnostics))
    snapshot = runtime.advance(scene_time)
    return compile(snapshot, device, calibration)


def _run_window(output, runtime, device, calibration, duration, clock=time.monotonic, sleep=time.sleep):
    start = clock()
    try:
        while True:
            scene_time = clock() - start
            frame = _frame(runtime, device, calibration, scene_time)
            # Compilation takes time, so presentation observes a fresh monotonic
            # timestamp in the same scene-clock epoch.
            present_time = clock() - start
            if not output.present(frame, present_time) or not output.poll(present_time):
                break
            if present_time >= duration:
                break
            sleep(1 / 30)
    finally:
        try:
            output.blackout()
        finally:
            output.close()


def _workspace_path(path):
    path = Path(path)
    if not path.is_absolute():
        path = Path(os.environ.get("BUILD_WORKING_DIRECTORY", os.getcwd())) / path
    return path


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default=".artifacts/raster-demo",
                        help="headless output directory (default: .artifacts/raster-demo)")
    parser.add_argument("--window", action="store_true", help="present a live GLFW window")
    parser.add_argument("--list-displays", action="store_true", help="list displays using GLFW")
    parser.add_argument("--display", type=int, help="explicit fullscreen display index")
    parser.add_argument("--glfw-library", default="", help="optional GLFW shared-library path")
    parser.add_argument("--duration", type=float, default=3.0, help="live demo duration in seconds")
    parser.add_argument("--calibration", help="calibration JSON file")
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    args = parser.parse_args(argv)
    try:
        if args.width < 64 or args.width > 4096 or args.height < 64 or args.height > 4096:
            raise ValueError("width and height must be between 64 and 4096")
        if args.duration <= 0 or not math.isfinite(args.duration):
            raise ValueError("duration must be finite and positive")
        if args.display is not None and args.display < 0:
            raise ValueError("display index must be non-negative")
        if args.list_displays:
            for item in GlfwOutput.displays(str(_workspace_path(args.glfw_library)) if args.glfw_library else ""):
                print(json.dumps({"index": item.index, "name": item.name,
                                  "width": item.width, "height": item.height}))
            return 0
        if args.display is not None and not args.window:
            raise ValueError("--display requires --window")
        calibration_path = _workspace_path(args.calibration) if args.calibration else None
        calibration = _calibration(calibration_path)
        runtime, device = _scene(args.width, args.height)
        if not args.window:
            frame = _frame(runtime, device, calibration, 0.0)
            output = Path(args.output)
            if not output.is_absolute():
                output = Path(os.environ.get("BUILD_WORKING_DIRECTORY", os.getcwd())) / output
            output.mkdir(parents=True, exist_ok=True)
            write_ppm(frame, str(output / "frame.ppm"))
            (output / "metadata.json").write_text(json.dumps(_metadata(frame), indent=2) + "\n", encoding="utf-8")
            print(str(output))
            return 0

        library_path = str(_workspace_path(args.glfw_library)) if args.glfw_library else ""
        output = GlfwOutput(args.width, args.height, "SpatialGL raster demo",
                            -1 if args.display is None else args.display, library_path)
        _run_window(output, runtime, device, calibration, args.duration)
        return 0
    except (ValueError, RuntimeError, OSError) as error:
        print(f"raster_demo: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
