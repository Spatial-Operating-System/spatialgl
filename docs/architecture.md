# SpatialGL compatibility runtime contract · v0.2

This document defines the retained `SceneFrame / Backend / RealizationState`
API. The new surface optics contract, policies, and limits are defined in the
[core API](core-api.md); the C11 boundary is in the [C API](c-api.md), and
dependencies are in [modules](modules.md).

## Research boundary

SpatialGL consumes graphics with coordinates already assigned. Finding an
object and its location from a request such as `highlight(robot)` belongs to
an upstream system. The research question is: under which conditions can the
same spatial graphic be realized through different physical output techniques?

The common contract is **spatial targets, realization constraints, time, and
observations**. It is not a universal pixel format. Forcing every device into
one command format would conflate a drone trajectory with a projector image.

## Primitives

| Type | World-space representation | Compatibility realization |
| --- | --- | --- |
| Point | Position and RGB | Surface raster sample or point-emitting drone |
| Polyline | At least two vertices and RGB | Sampled segments sent to raster backends |
| Patch | Origin, orthogonal u/v edges, and RGB | Sampled finite rectangle sent to raster backends |

Lengths are metres, time is seconds, and coordinates are right-handed with
+Y up. Compatibility RGB is a display color in [0,1], not radiance or physical
source energy. An explicit `surface_id` constrains output to that surface.
Otherwise, a backend may find a surface containing the exact requested point.
It must not project or silently correct the requested position.

Default sample spacing is 0.15 m and is configurable at runtime construction.
Sample IDs combine primitive ID and generation order. Correspondence persists
only while topology and sample count stay the same. Editing a Polyline/Patch
does not preserve permanent surface-parameter IDs. Visibility and coverage
between samples are not guaranteed.

## Three frame/state contracts

`SceneFrame` is a copied, immutable submission replacing the complete desired
scene. It becomes valid at `present_at` and expires at `expires_at`. Future
scenes may be queued. Duplicate scene IDs, past presentation times, and invalid
geometry are rejected. The newest submitted scene eligible at the current time
wins; later submission wins a tie. Expiry never resurrects an older scene.

`DeviceFrame` contains one device's commands. `issued_at` is its refresh tick;
`apply_at` is delivery after fixed latency. Frames preserve `scene_id` and
`expires_at`. Raster commands contain resolution, pixels, RGB, and world-space
samples; emitter commands contain target positions. A new command replaces the
device's whole previous command. Empty scenes also send clear commands.
Additional techniques may use a versioned `ExtensionCommand` schema/payload.

`RealizationState` reports realized/tracking/pending/unrealizable status for
each requested sample, actual modeled position, position error, source scene,
command age, and failure reasons. `outputs` also includes samples no longer
requested but still emitting. Prior-scene output is marked `stale`.
`device_frames` records the last applied commands, even if expired; consult
`outputs` to determine what currently emits.

The `realized` predicate tests positional distance only; default tolerance is
2 cm. It does not guarantee color, deadlines, or perceptual equivalence. An
older scene's nearby sample can pass the position predicate, so provenance
and stale flags must also be checked.

## Execution time

`advance(t)` processes device ticks and delivery events in time order, then
integrates remaining movement. Time must be monotonic. Devices can have
different refresh rates and latencies; browser redraw rate is not a device
clock. Constant-speed drone movement is integrated between command arrivals.
Small and large advance steps agree up to floating-point rounding.

The simulator gates emission at scene expiry. This does not assert that real
hardware implements expiry. Drones continue moving toward their last command
while their light is off. A hardware integration must separately define
watchdog, cancellation, acknowledgement, movement stop, and safety behavior.

Scene replacement is not globally atomic. Different refresh rates and
latencies can produce simultaneous old, new, and empty output. The
compatibility model assumes fixed latency, FIFO delivery, and no command loss.

## Backend extension boundary

A `Backend` implements:

1. `descriptor`: technique, refresh rate, latency, and sample capacity.
2. `evaluate(sample, world) const`: side-effect-free support, score, actual
   spatial position, and device coordinates.
3. `encode(candidates) const`: selected samples to device-specific commands.
4. `advance(dt)` and `apply(frame)`: internal dynamics and command execution.
5. `observe(now)` and `reset()`: output observations and experiment reset.
6. `clone()`: a fresh execution instance with the same configuration.

Each runtime owns mutable backend state. Construction clones and resets fresh
adapters so an input configuration can be reused across runtimes. Custom
backends must implement this ownership contract. v0.2 assumes commands are
received and executed normally; measured sensing, driver state, error returns,
and stream queue contracts require further APIs.

## Routing and physical realizability

Every backend evaluates each sample. Feasible candidates are sorted by score
and device ID; the first with remaining resources wins. Scores are currently
monitor=100, projector=50-depth, and drone=10. A monitor already supplying a
physical surface is preferred. Capacity or raster pixel conflicts cause
fallback; if every candidate fails, the runtime reports reasons.

Compatibility projectors have fixed poses. A point emitter occupies one drone
continuously; one drone does not rapidly traverse a polyline to simulate
persistence of vision. Removing the monitor backend does not remove its
physical surface from the world, so a projector can still target that surface.

This is a deterministic greedy baseline. It does not solve whole-primitive
atomic allocation, observer quality, device switching cost, optimal placement,
blending, or optical interference. Samples sharing a raster pixel produce a
pixel-conflict diagnostic instead of compositing.

## Further abstraction decisions

A technique supporting only Point can become a compatibility backend. Audio
or haptics needs a content/channel contract instead of reusing RGB primitives.
Moving monitors need updated support geometry and device pose. Steerable
projection couples capability evaluation to motion planning. Keep the boundary
between desired spatial graphics and native commands explicit.

## Implementation and language boundaries

The C++20 primitive contract is `std::variant<Point, Polyline, Patch>`.
`DeviceCommand` distinguishes RasterCommand, EmitterCommand, and
ExtensionCommand. The core does not depend on Python, DOM, or graphics UI
libraries. Techniques matching independent sample evaluation and per-device
command replacement can extend `Backend`. Joint optical composition, shared
actuators, and interval/phase-aware scan programs require the newer compiler
and runtime contract. A string payload alone does not implement those semantics.
Compatibility Python exposes built-in backends, without subclass callbacks.

Python authors Point, Polyline, Patch, Surface, World, and SceneFrame, then calls
`Runtime.submit()`, `advance()`, or `snapshot()`. pybind11 runs the actual C++
implementation. Observations are read-only, independent snapshots and can be
serialized with `to_dict()`. Copied inputs let a runtime outlive input Python
objects.

Bazel 9.2.0, Bzlmod locks, rules_cc, rules_python, and pybind11_bazel provide
the build. The Python 3.13 toolchain matches the native extension and Python
launchers. Use `//python:python` for scripts/REPL and `//python:demo` for the
lab. Do not import the extension into a different Python ABI. Notebook/pip
wheels are future distribution work.

The single-threaded loopback HTTPServer generates scenes in Python and
serializes native calls. Browser JavaScript draws computed targets/observations.
`write_view()` embeds one observation in standalone HTML, with camera rotation
and zoom but no live simulation.

Runtime instances are not thread-safe. Native advance/snapshot bindings release
the GIL, so shared-instance Python calls require an external lock. Separate
instances do not share backend state. `sample_scene` enforces a 100,000-sample
scene budget and rejects excessive requests during submission.

## Multi-device design audit

The [audit](cases-and-design-coverage.md) compares 12 reference cases and eight
constructed counterexamples against the historical v0.2 baseline. Joint
contributions, rigs/resources, timed programs, timestamped worlds/calibration,
observer/pass/channel contracts, and richer evidence are required for broader
coverage. The current optics API implements a subset of those extensions;
refer to its own contract and validation rather than interpreting this
compatibility document as a claim of full coverage.
