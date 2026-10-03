# Python optics API

`spatialgl.optics` is the typed authoring surface for the C++ surface optics runtime. It is built as a second pybind module using the same Bazel Python toolchain as the legacy `spatialgl` API. No geometry, allocation, event scheduling, or device simulation is implemented in Python.

Construct a `WorldSnapshot` from calibrated `Plane` values and a `Rig` from `DeviceProfile` and `Resource` values. `fixed_raster`, `steerable_raster`, and `galvo` provide small profile helpers; use the full profile value when setting clock, calibration, or motion constraints. A `DisplayList` contains stable-ID `DrawCall` values. `SurfaceTarget` distinguishes world and surface-local coordinates, and `GeometryKind` identifies points, ordered polylines, and patches.

`OpticalRuntime.submit` copies a display list and returns core diagnostics. `advance` returns a value snapshot containing emitted contributions, aggregate target light, timed programs, receipts, and diagnostics. Observation objects and their fields are read-only; their vectors are copied into independent Python values. `snapshot` returns an independent value. `cancel`, `set_availability`, and `update_world` delegate directly to the native runtime. Serialize calls made on one runtime instance.

Composition (`Composition`) controls display-list layering. Physical output mix (`OutputMix`) separately controls how devices contribute. Laser fills are unsupported, including when the future approximation option is enabled; unsupported target geometry is surfaced by validation. Required synchronized-group presentation currently returns an explicit unsupported diagnostic because no trigger-capable profile is modeled. Program timestamps and all light values are simulator predictions, not hardware measurements.

`PipelineState.max_position_error` defaults to zero, meaning no certified bound is requested. A positive value is unsupported because the simulator cannot certify target-position error. `TraversalState.minimum_duty` and `max_dark_gap` are checked against predicted cycle metrics and produce a no-plan diagnostic when unmet. `DrawCall.closed` closes a polyline, and each device's `sample_budget` limits the target sample count it can program.

Run all six executable scenario checks with `bazel run //python:optics_scenarios`; `bazel test //python:optics_scenario_test` runs the same acceptance assertions. The existing authoring API, headless demo, and live viewer remain available with `bazel run //python:python -- script.py` and `bazel run //python:demo -- --port 5188`.

`spatialgl.raster` adds a separate output surface: `compile(snapshot, device,
calibration)` returns an immutable RGB frame, `write_ppm` exports it, and
`GlfwOutput` optionally presents it. `Calibration` and `calibration_from_corners`
provide a manual planar warp. Native snapshot contributions remain the source
of rendering decisions. See [fixed raster output](raster-output.md) for the
demo, main-thread presentation, leases and finite-sample limitations.
