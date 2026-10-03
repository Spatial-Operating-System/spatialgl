# C++ module layout

The root `//:spatialgl` and `//:optics` labels are public aggregate libraries. The
legacy API remains available from `spatialgl/spatialgl.h`; its implementation
is split into `//libs/core:core`, `//libs/backends:backends`, and
`//libs/runtime:runtime`. The typed API is declared by
`spatialgl/optics.h`; validation and sampling policies live in
`//libs/optics:optics`, while event planning, device allocation and predicted
observations live in `//libs/simulation:simulation`.

| Target | Responsibility | Depends on |
| --- | --- | --- |
| `//libs/core:core` | Legacy value types, math, and configuration | — |
| `//libs/backends:backends` | Legacy backend and device models | `//libs/core:core` |
| `//libs/runtime:runtime` | Legacy runtime orchestration | core, backends |
| `//libs/optics:optics` | Typed world, target, rig, policy contracts and validation | `//:optics_headers` |
| `//libs/simulation:simulation` | Deterministic geometry sampling, resource allocation, timing, composition, observations | optics |
| `//:optics` | Public typed API aggregate | optics, simulation |

The public C ABI is under `include/spatialgl/capi/`; its adapter and the Python
binding are separate targets. They copy typed values and call the C++ core.
The simulator owns geometry, sampling, allocation, device timing and predicted
observations.

The directory layouts are structural references, not dependencies or copied
code: [bgfx](https://github.com/bkaradzic/bgfx) illustrates a standalone C99
header alongside a C++ implementation; [Filament](https://github.com/google/filament)
uses focused feature modules under `libs`; [GLFW](https://github.com/glfw/glfw)
shows a standalone public C header and opaque handles.

Unsupported sensing reconstruction, stereo/eye/focus, volume, coherent or
scattering optics, audio and haptics remain explicit future capabilities.
