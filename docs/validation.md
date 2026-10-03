# Validation · C++/Python migration · 2026-10-03

## Bazel checks

`bazel test //... --test_output=errors` passed all three test targets:

- `//:core_test`: 25 native C++ checks.
- `//python:api_test`: 6 Python/native binding tests.
- `//python:demo_test`: 6 experiment/view/server tests.

The 37 checks cover support constraints, projector pixel quantization, frustum and occlusion, projector fallback, monitor preference, pixel conflicts, drone capacity and motion, scheduling and command latency, expiry and supersession, pending clears and stale outputs, independent clocks, custom C++ backend commands, invalid inputs, backend/input lifetime isolation, sample budgets, JSON conversion, Python-generated scenes, standalone view escaping, local HTTP trace export, invalid reset preservation, and an occluder that actually intersects the default projection paths.

`clang-format --dry-run --Werror` passed for the C++ header, implementation, native tests and Python bindings. `git diff --check` passed.

## Python execution

- `bazel run //python:simulate`: calls the real compiled C++ extension. A light is initially pending, starts moving after 100 ms command latency, reaches its target at 1.1 s, and has no visible output at the 3 s expiry.
- `bazel run //python:python -- examples/hello.py`: executes a user Python script in the ABI-matched Bazel runtime, prints the native tracking/error observation and writes `.artifacts/hello.html`.
- `bazel run //python:demo -- --output .artifacts/python-simulator.html`: writes a standalone HTML observation of the mixed scene. Output paths resolve against the user's original working directory.
- `bazel run //python:demo -- --port 5188`: serves a Python-owned live experiment on loopback. The HTTPServer serializes C++ runtime access.

Bazel 9.2.0 and the macOS arm64 C++ toolchain were used. rules_python supplies Python 3.13 for both the extension and the Python launchers. System Python 3.14 is not used to import the Python 3.13 extension. Other platforms were not tested.

## Browser verification

The actual Python lab was opened in the Codex in-app browser. Its room, display surfaces, projector origins, desired samples, actual outputs, error lines and diagnostic table were visually inspected.

- Free-space Patch: 40 unrealizable samples with per-backend support/primitive reasons.
- Three free-space Points and one drone: two capacity failures.
- Mixed scene with occlusion: 40 wall-patch samples could not be realized; both projectors reported occlusion while the monitor and airborne emitters continued displaying their supported content.
- Removing the occluder restored the mixed scene without those support failures.
- Pause, 100 ms step, scene selection, drone count, occlusion controls and persisted settings after page reload were exercised.

The updated viewer uses Canvas only for camera projection and drawing. No physical simulation runs in browser JavaScript. A final screenshot is stored locally at `.artifacts/python-simulator.jpg` (generated, gitignored).

Static view generation was verified by execution and HTML-content tests; its separate browser view was not inspected. Narrow-screen CSS is present but was not separately inspected. Trace-download behavior is covered by the HTTP integration test; the new browser download button was not separately exercised.

## Workspace integration

The existing SpatialGL registration now names the Bazel-built C++ contract and Python API. Cluster execution rules now require `bazel test //...`. The mandatory workspace `scripts/test-workspace-tools` completed successfully in the isolated test environment prepared for the initial project creation.

## Model limits

These checks validate the geometry/time model and language boundary, not optical radiometry, flight dynamics or real hardware. The standalone native extension is tied to the configured Python ABI; pip/notebook wheels remain future work. Same-instance native calls require external serialization when used from multiple threads.
