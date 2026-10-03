# First HDMI raster demo

Run `bazel run //examples/hardware:raster_demo` for a headless PPM image and `metadata.json` under `.artifacts/raster-demo` (or pass `--output DIR` to choose another directory). Add `--window` for live GLFW output; `--list-displays` lists available displays, and `--display INDEX` selects an explicit fullscreen display. `--glfw-library PATH` points to an optional GLFW shared library. The live window updates for three seconds by default; use `--duration SECONDS` to change that.

The optional `--calibration FILE.json` accepts either a 9-number homography or four normalized corner correspondences, plus optional `point_radius`, `transfer` (`srgb` or `linear`), and positive `lease_seconds`. Example corner schema:

```json
{
  "source_corners": [[0, 0], [1, 0], [1, 1], [0, 1]],
  "destination_corners": [[0.02, 0.03], [0.98, 0.01], [0.99, 0.97], [0.01, 0.99]],
  "point_radius": 2,
  "transfer": "srgb",
  "lease_seconds": 0.1
}
```

The scene is a sparse sampled polyline grid and animated point splat compiled by SpatialGL's native optics and raster code. It does not claim continuous fill or measured optical feedback. A real monitor needs a compatible GLFW runtime; frame output and metadata report simulated/predicted software state, not optical measurements.
