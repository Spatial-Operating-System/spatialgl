# Validation · modular optics and C ABI · 2026-10-03

## Build prerequisites

Install [Bazelisk](https://github.com/bazelbuild/bazelisk) or Bazel 9.2.0 and a
C++20 toolchain. On macOS, install the Command Line Tools. Bazel downloads the
locked dependencies and a matching Python 3.13 runtime. Run app scripts through
`bazel run //python:python -- script.py` so the interpreter matches the native
extension. Standalone pip wheels and notebook installation remain future work.

## Public repository presentation

The README, roadmap, compatibility architecture, design audit, and live viewer
were translated into English for public publication. A generated transparent
logo and its prompts are stored in `docs/assets/`. All seven Bazel test targets
passed after the viewer translation. The English live viewer was checked in
the in-app browser; labels, controls, scene, and observation table rendered
without visible layout overlap. A text scan found no remaining Korean in the
current tracked documentation or application sources. Historical commits are
preserved and may contain the original Korean text.

## Current implementation

`bazel test //... --test_output=errors` passed all seven test targets:

- `//:core_test`: retained C++ compatibility runtime behavior.
- `//:optics_test`: typed optical geometry, composition, allocation, timing,
  availability, cancellation and revision contracts.
- `//python:api_test`: retained API plus Python bindings to the optical core.
- `//python:demo_test`: existing experiment, HTML view and HTTP server behavior.
- `//python:optics_scenario_test`: F01–F06 and sampled-bin composition.
- `//tests/capi:capi_smoke_test`: an actual C11 caller, descriptor validation,
  copied submission, queries and snapshot ownership.
- `//tests/capi:capi_parity_test`: equivalent C and C++ scenes, including
  contributions, commands, source IDs and revisions.

The new scenario runner exercised normalized projector overlap and occlusion,
raster/galvo content, shared-resource conflicts, moving surfaces, asynchronous
laser clocks with explicit rejection of unsupported group synchronization,
and cancellation/failure/calibration invalidation. These are constructed
acceptance scenarios; they are not reproductions of the papers in the
[coverage audit](cases-and-design-coverage.md).

The native and binding checks additionally cover closed curves, opacity and
composition across primitive kinds, calibrated per-channel gain limits,
device sample budgets, clock drift, modeled duty/dark-gap bounds, enum and
finite-value validation, context mismatch, independent snapshots and world
metadata updates. Position-error certification and laser-fill approximation
requests are rejected explicitly.

## Executed clients and build boundaries

- `bazel build //bindings/c:spatialgl_c //:spatialgl //:optics` passed. The macOS
  shared library exports 54 `spgl_` functions.
- A separately compiled C11 smoke program linked and ran against
  `libspatialgl_c.dylib`, in addition to the Bazel C11 test target.
- `bazel run //examples/c:hello` submitted a point and queried its predicted
  light `(0.200, 0.500, 0.900)` through the C boundary.
- `bazel run //python:optics_scenarios` passed all seven scenarios.
- The optics Python example in README executed through `//python:python` and
  produced normalized green light from two projector contributions.
- An independent moving-surface probe compared the same scheduled steerable
  command after two different advance times; its event time and position
  remained equal.
- `bazel run //python:demo -- --output .artifacts/modular-simulator.html`
  generated the existing viewer successfully. New optical scenarios currently
  expose observations through Python/C/C++; they are not integrated into that
  compatibility viewer.

All C/C++ sources passed `clang-format --dry-run --Werror`; all Bazel BUILD
files passed `buildifier -mode=check`. The extracted legacy implementation
units also passed an independent C++20 compile check with
`-Wall -Wextra -Werror`.

Validation used macOS arm64, Bazel 9.2.0 and the configured Python 3.13
runtime. Other platforms were not tested. The shared C library was linked by
a client with the same macOS deployment target. Python wheels and system
Python 3.14 imports were not validated.

## Scope of the evidence

These checks establish simulated geometry, scheduling, ownership and language
boundary behavior. The optical model composes exact coincident sampled bins;
it does not compute continuous raster coverage between samples. Steering uses
angular speed and settling, and galvo programs use ordered samples, blank
travel and dwell; neither models actuator acceleration or analog corner
dynamics. Position error is unavailable. Required group triggers, nonstable
traversal and galvo fill remain explicitly unsupported. The allocator is a
deterministic greedy scheduler, not an optimal solver. There is no hardware
transport, measured optical output or certified hardware timing guarantee.
See [core API](core-api.md) and [C ABI](c-api.md) for the executable contract.

The mandatory workspace suite passed after project registration and the
portable implementation role changes. Blacksmith-dependent checks are now
optional and were separately verified; SpatialGL's Bazel checks do not depend
on Blacksmith.

## Historical v0.2 migration evidence

The following record describes the earlier compatibility runtime and viewer
verification. Its three-target/37-check count predates the optics and C ABI
implementation above.

### Bazel checks

`bazel test //... --test_output=errors` passed all three test targets:

- `//:core_test`: 25 native C++ checks.
- `//python:api_test`: 6 Python/native binding tests.
- `//python:demo_test`: 6 experiment/view/server tests.

The 37 checks cover support constraints, projector pixel quantization, frustum and occlusion, projector fallback, monitor preference, pixel conflicts, drone capacity and motion, scheduling and command latency, expiry and supersession, pending clears and stale outputs, independent clocks, custom C++ backend commands, invalid inputs, backend/input lifetime isolation, sample budgets, JSON conversion, Python-generated scenes, standalone view escaping, local HTTP trace export, invalid reset preservation, and an occluder that actually intersects the default projection paths.

`clang-format --dry-run --Werror` passed for the C++ header, implementation, native tests and Python bindings. `git diff --check` passed.

### Python execution

- `bazel run //python:simulate`: calls the real compiled C++ extension. A light is initially pending, starts moving after 100 ms command latency, reaches its target at 1.1 s, and has no visible output at the 3 s expiry.
- `bazel run //python:python -- examples/hello.py`: executes a user Python script in the ABI-matched Bazel runtime, prints the native tracking/error observation and writes `.artifacts/hello.html`.
- `bazel run //python:demo -- --output .artifacts/python-simulator.html`: writes a standalone HTML observation of the mixed scene. Output paths resolve against the user's original working directory.
- `bazel run //python:demo -- --port 5188`: serves a Python-owned live experiment on loopback. The HTTPServer serializes C++ runtime access.

Bazel 9.2.0 and the macOS arm64 C++ toolchain were used. rules_python supplies Python 3.13 for both the extension and the Python launchers. System Python 3.14 is not used to import the Python 3.13 extension. Other platforms were not tested.

### Browser verification

The actual Python lab was opened in the Codex in-app browser. Its room, display surfaces, projector origins, desired samples, actual outputs, error lines and diagnostic table were visually inspected.

- Free-space Patch: 40 unrealizable samples with per-backend support/primitive reasons.
- Three free-space Points and one drone: two capacity failures.
- Mixed scene with occlusion: 40 wall-patch samples could not be realized; both projectors reported occlusion while the monitor and airborne emitters continued displaying their supported content.
- Removing the occluder restored the mixed scene without those support failures.
- Pause, 100 ms step, scene selection, drone count, occlusion controls and persisted settings after page reload were exercised.

The updated viewer uses Canvas only for camera projection and drawing. No physical simulation runs in browser JavaScript. A final screenshot is stored locally at `.artifacts/python-simulator.jpg` (generated, gitignored).

Static view generation was verified by execution and HTML-content tests; its separate browser view was not inspected. Narrow-screen CSS is present but was not separately inspected. Trace-download behavior is covered by the HTTP integration test; the new browser download button was not separately exercised.

### Workspace integration

The existing SpatialGL registration now names the Bazel-built C++ contract and Python API. Cluster execution rules now require `bazel test //...`. The mandatory workspace `scripts/test-workspace-tools` completed successfully in the isolated test environment prepared for the initial project creation.

### Model limits

These checks validate the geometry/time model and language boundary, not optical radiometry, flight dynamics or real hardware. The standalone native extension is tied to the configured Python ABI; pip/notebook wheels remain future work. Same-instance native calls require external serialization when used from multiple threads.
