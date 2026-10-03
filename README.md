<p align="center">
  <img src="docs/assets/spatialgl-logo.png" alt="SpatialGL" width="560">
</p>

# SpatialGL

**Spatial graphics for projectors and scanning lasers. C++20 core, Python apps, C11 API.**

SpatialGL lets an application describe what to display, where it belongs in physical space, and when it should appear. The runtime turns surface-bound graphics into timed device programs and reports predicted light, resource conflicts, timing, and unsupported requests.

The current implementation is a simulation-first foundation for fixed projectors, steerable projectors, and galvo laser scanners. It brings familiar graphics concepts—draw calls, display lists, composition, and presentation state—to devices with different optical and mechanical constraints.

## Quick start

Install [Bazelisk](https://github.com/bazelbuild/bazelisk) or Bazel 9.2.0 and a C++20 toolchain. On macOS, install the Command Line Tools. Bazel downloads the locked dependencies and a matching Python 3.13 runtime.

```sh
git clone https://github.com/Spatial-Operating-System/spatialgl.git
cd spatialgl
bazel test //...
bazel run //examples/optics:scenarios
bazel run //examples/c:hello
```

Validation currently covers macOS arm64. See [validation](docs/validation.md) for the tested toolchain and platform limits.

## Write an app in Python

```python
from spatialgl import optics as sgl

world = sgl.world([sgl.plane("wall")])
devices = sgl.rig([
    sgl.fixed_raster("left", position=(-1, 1, 3), target=(0, 1, 0)),
    sgl.fixed_raster("right", position=(1, 1, 3), target=(0, 1, 0)),
])
sim = sgl.OpticalRuntime(world, devices)
diagnostics = sim.submit(sgl.display(
    "frame-0", [sgl.draw("marker", "wall", color=(0, 1, 0))], expires_at=2,
))
state = sim.advance(0.1)
for target in state.targets:
    print(target.draw_id, target.aggregate_linear_rgb, target.provenance)
for program in state.programs:
    print(program.device_id, program.generation)
    for event in program.events:
        print(event.kind, event.time, event.position, event.linear_rgb)
```

Save the example as `app.py`, then run it with the ABI-matched Python launcher:

```sh
bazel run //python:python -- app.py
bazel run //python:python  # Interactive Python with spatialgl imported as sgl
```

The two projectors share the requested green light under the default `NORMALIZED` output policy. The example inspects both aggregate target light and device programs. Python authors the scene; C++ owns geometry, allocation, timing, and simulation. Inputs are copied, and returned observations are independent snapshots.

The native extension uses Bazel's Python 3.13 ABI. Standalone pip wheels and notebook installation are future work. Use the Bazel launcher rather than importing its extension into a different Python version.

## Core contract

```text
WorldSnapshot + Rig + RuntimeOptions
                 ↓
             DisplayList
                 ↓
        Timed device programs
                 ↓
     Snapshot: contributions, target light,
     receipts, diagnostics and provenance
```

| Area | Current behavior |
| --- | --- |
| Graphics | Points, ordered polylines, and rectangular patches on finite planes; world or surface-local coordinates |
| Devices | Fixed raster, speed/settling-limited steerable raster, and ordered galvo scanning with blank travel and dwell |
| Composition | `OVER`, `REPLACE`, and `ADD` on exact coincident sampled bins |
| Physical output | `EXCLUSIVE`, `NORMALIZED`, and `ADDITIVE`, with calibrated linear-light gains |
| Resources | Explicit shared resource IDs, exclusive reservations, priorities, and deterministic greedy allocation |
| Time and motion | Timestamped planar motion, command latency, device-local clock offset/drift, and predicted duty/dark gap |
| Lifecycle | Copied submissions, per-app replacement, expiry, cancellation generations, availability, and world/calibration invalidation |
| Observations | Predicted contributions, aggregate target light, ordered commands, diagnostics, revisions, and provenance |

Virtual layer composition and physical light mixing are separate policies. A draw call cannot remove a device's physical constraints. Unsupported, infeasible, no-plan-found, and invalidated outcomes are distinct; the allocator does not claim global optimality.

The contract and model assumptions are defined in the [C++ core API](docs/core-api.md) and [Python API](docs/python-api.md). The [multi-device design audit](docs/cases-and-design-coverage.md) compares 12 reference cases and eight constructed counterexamples.

## Use C or C++

The C++ optics API is declared in [`include/spatialgl/optics.h`](include/spatialgl/optics.h), in namespace `spatialgl::optics`, and built by `//:optics`.

The C11 boundary is [`include/spatialgl/capi/spatialgl.h`](include/spatialgl/capi/spatialgl.h). ABI v1 provides initialized POD descriptors, opaque context/frame/snapshot handles, copied inputs, explicit statuses, submission/cancellation, and count/index queries. Snapshots remain valid after their source context is destroyed. C++ exceptions and STL objects do not cross the C boundary.

```sh
bazel build //:optics //:spatialgl
bazel build //bindings/c:spatialgl_c
bazel run //examples/c:hello
```

See the [C API contract](docs/c-api.md) and [C11 example](examples/c/hello.c) for ownership, errors, versioning, and usage. Calls on one runtime/context must be externally serialized.

## Explore the compatibility viewer

```sh
bazel run //python:demo -- --port 5188
```

Open [http://127.0.0.1:5188/](http://127.0.0.1:5188/). The Python-owned lab compares two fixed projectors, a monitor, and point-emitting drones. Change scene/device refresh rates, latency, occlusion, drone count, and speed; pause, step, reset, or export observation traces.

```sh
bazel run //python:simulate
bazel run //python:python -- examples/hello.py
bazel run //python:demo -- --output .artifacts/view.html
```

The viewer uses the retained `SceneFrame / DeviceFrame / RealizationState` compatibility runtime. The new optics API is exercised by the optics scenario runner and native snapshots. Browser JavaScript only draws observations and handles interaction; it does not implement physical simulation. Node/npm is not required.

## Repository layout

```text
include/spatialgl/          Public C++ headers and the C ABI
libs/core/                 Compatibility values and geometry
libs/backends/             Compatibility device models
libs/runtime/              Compatibility runtime
libs/optics/               Optical types and validation
libs/simulation/           Optical planning and simulation
bindings/c/                C ABI adapter and shared library
bindings/python/           Native Python bindings
python/spatialgl/          Python authoring, experiments, and viewer
examples/                  Runnable C and Python applications
tests/                     Optical scenarios and C ABI consumers
docs/                      Contracts, design audit, and validation
```

Each module has its own Bazel target. The structure draws on [bgfx](https://github.com/bkaradzic/bgfx), [Filament](https://github.com/google/filament), and [GLFW](https://github.com/glfw/glfw) as layout references; they are not implementation dependencies. See [module responsibilities](docs/modules.md).

## Model limits

SpatialGL currently predicts output; it does not control hardware or report measured photons. Composition uses finite samples rather than a continuous framebuffer. The optical transfer model uses calibrated gains and direct visibility, without material reflectance, BRDF, or coherent interference. Steering and scanning use idealized timing rather than actuator acceleration or analog corner dynamics.

Galvo fills, required hardware synchronization, nonstable traversal, and certified position-error bounds are explicitly unsupported. Camera reconstruction, observer/eye/focus channels, volumetric rendering, audio, haptics, hardware transport, and hardware acknowledgements remain future capabilities. Simulator expiry and availability states do not establish hardware guarantees.

## Development

Run `bazel test //...` for changes. The seven test targets cover the compatibility runtime, optics, Python API/viewer, constructed multi-device scenarios, an actual C11 caller, and C/C++ parity. Format C++ with the checked-in `.clang-format` and Bazel BUILD files with Buildifier. See [contributor instructions](AGENTS.md).

Further reading: [compatibility architecture](docs/architecture.md), [roadmap](docs/roadmap.md), [validation record](docs/validation.md), and [logo provenance](docs/assets/README.md).
