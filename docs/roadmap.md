# SpatialGL roadmap

## 0.1 · Completed: simulation baseline

Implemented world-coordinate Point/Polyline/Patch, finite support planes,
fixed projector/monitor/point-emitter backends, capacity and pixel conflicts,
AABB occlusion, refresh/latency/expiry, output provenance, headless examples,
and an interactive 3D comparison viewer.

## 0.2 · Completed: C++ core, Python API, and Bazel

Moved physical realization into C++20. Added pybind11 bindings, matched Bazel
C++/Python toolchains, native/API/server tests, user scripts and a REPL, a live
Python lab, and standalone observation views. The initial TypeScript
implementation remains in Git history.

## 0.3 · Completed: modular surface optics and C ABI

Added surface-bound display lists, rigs and shared resources, fixed/steerable
raster and ordered galvo models, virtual composition and physical light
mixing, world/calibration revisions, cancellation, and fault states. Split
the C++ implementation into focused Bazel libraries and connected the C11 ABI
and Python optics API. The [core contract](core-api.md), [C API](c-api.md), and
[module layout](modules.md) define supported behavior and approximations.
F01–F06 validate minimal constructed simulator scenarios; they do not reproduce
the complete systems discussed in the literature.

## Next: reproducible experiments

- Python wheel distribution for separate environments and notebooks.
- Versioned JSON inputs for scenes, worlds, and device profiles; saved trace replay.
- Coverage error measurement through sampling-density convergence and analytic fixtures.
- Whole-primitive allocation, quality/error/deadline requests, and explicit degradation policies.
- Device profile controls, variable delay, dropped commands, and clear acknowledgements.

Acceptance: identical inputs and seeds produce identical command/observation
traces, with independent reporting of unmet requests, timing violations, and
modeled position error.

## Next: richer physical rendering

Use the [coverage audit](cases-and-design-coverage.md) and F01–F06 to test the
structure of optical simulation. Extend accuracy and hardware support on top
of joint contributions, shared resources, timed programs, and timestamped
world/clock/calibration provenance. Observer/eye/focus, sensing passes, and
volume/audio/haptic support require separate milestones.

- Steerable projectors: actuator acceleration, inertia, dynamic warp, and measured profiles.
- Moving monitors: updated support transforms and movement cost.
- Drone/robot emitters: collision-free trajectories, minimum separation, acceleration, and persistent identity.
- Multiple projectors: continuous raster framebuffers, footprints/reflectance, and measured photometric calibration.
- Galvo: path travel and corner dynamics, fill approximation, and real DAC/stream queues.
- Rigs: trigger synchronization, loss/underrun, and hardware cancellation acknowledgements.
- Nonplanar surfaces: mesh support, visibility clipping, and surface coordinate mapping.

Acceptance: the same scene reports feasible content, timing cost, and spatial
error comparably across techniques. Free-space patches must not be silently
replaced by surface projection.

## Later: the first hardware backend

Start with one fixed monitor or projector. Implement world/device calibration,
driver transport, measured feedback, watchdog/expiry, and command
acknowledgements. Compare predicted and measured output. Drone hardware follows
only after a separately validated control and safety layer exists.
