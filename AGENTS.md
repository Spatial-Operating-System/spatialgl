# SpatialGL

Independent repository in the dostos workspace. Keep implementation and code-adjacent contracts here; register ownership in the workspace index only.

- C++20 is the sole implementation of geometry, sampling, routing, device dynamics and simulation clocks. Build with Bazel/Bzlmod.
- User-facing authoring, experiments, scripts and views use Python. pybind11 connects typed values and native calls; do not duplicate physical simulation in Python or browser JavaScript.
- Python ABI and extension must use the same Bazel toolchain. `bazel run //python:python -- script.py` runs user scripts in that environment.
- Metres, seconds, right-handed coordinates with +Y up. Never silently snap world geometry onto another support surface.
- Preserve SceneFrame / DeviceFrame / RealizationState separation; physical presentation is not globally atomic.
- Extend C++ Backend with capability evaluation, native command encoding, observations, reset and fresh-instance clone semantics. Report unsupported geometry and observed error.
- Runtime copies submitted values and owns cloned backends. Serialize calls on each runtime instance; no thread safety is promised.
- Run `bazel test //...` for changes. Check the browser for viewer changes. Use clang-format with the checked-in style for C++.
- Distinguish simulator assumptions from measured hardware performance. Real-world device control and its safety contracts are future work.
