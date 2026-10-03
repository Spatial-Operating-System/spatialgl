# SpatialGL C API v1

`include/spatialgl/capi/spatialgl.h` is the C11 boundary for the C++20 optics
simulator. It exposes copied, typed values and opaque ownership handles; the
implementation and scheduling remain in `spatialgl::optics`. This is an ABI
version 1 contract for the shipped library. It does not promise permanent
binary compatibility across platforms, compilers, or future major versions.

## Values and calls

Call each `_init` function before filling a descriptor or query result. Init
sets the exact v1 `size`, ABI version, and documented defaults. Inputs must
have the current ABI version and at least the current struct size. Output
records accept a current or larger size and are populated only through the
known v1 prefix. Unknown enum values, null required pointers, invalid UTF-8
IDs, non-finite numbers, invalid time intervals, and invalid geometry return an
error status. All arrays and strings are read and copied synchronously by the
call; their storage can be reused as soon as the call returns. The caller must
still supply valid readable memory for the full duration of the call.
The ABI version function and each record's `abi_version` use the same packed
major/minor value (`SPATIALGL_ABI_VERSION`, currently `0x00010000`).

`spgl_context_desc` carries sampling, output-mix, traversal and presentation
policy for the context. Its `position_tolerance_m` default is zero, meaning no
certified position-error bound is requested. Any positive value is rejected as
`SPGL_STATUS_UNSUPPORTED`; the simulator does not model a certified position
error estimate. Frame descriptors carry list identity, app identity,
composition, validity interval, and optional world/calibration revisions (zero
asks the runtime to capture its current revisions). Configure devices and
their physical resource IDs before the first submit/advance operation creates
the runtime. Once created, surface and occluder changes update the world
revision, surface motion updates its timestamped planar prediction and world
revision, and an optional calibration revision can be supplied with that
update. `spgl_context_world_revision` and
`spgl_context_calibration_revision` expose the current counters. Device
availability changes are forwarded to the runtime. The device/resource graph
is fixed for that runtime's lifetime; adding or removing a device or resource
afterward returns `SPGL_STATUS_INVALID_STATE`. An empty resource ID list means
the device declares no shared resources.

`spgl_context_update_world_metadata` changes source time, validity end, and
revisions independently of surface motion. A zero `world_revision` advances
the current world revision by one; a nonzero value must strictly advance it.
A zero `calibration_revision` retains the current calibration revision; a
nonzero value may retain or increase it, but cannot decrease it. Metadata
updates are validated and applied transactionally. Every successful update
advances the world revision, including calibration-only changes, invalidating
queued display programs from an earlier world or calibration. Resource
capacities other than one are rejected as unsupported because the current
simulator models only exclusive unit-capacity resources.

Frames are context-associated mutable builders. Draw calls accept point,
ordered polyline, or patch geometry, with world or surface-local coordinates.
Every v1 draw names a configured surface. Color is nonnegative linear light;
opacity and intensity are separate draw values.
Submission copies the whole frame; later frame mutation or destruction cannot
change queued work. A frame from another context is rejected. `advance` accepts
finite monotonic seconds and returns an error for backward time. Snapshot
queries expose copied contribution, aggregate target light, diagnostic,
program-event, and device receipt records through count/index calls. Program
events retain the core's blank/emit/dwell/pose/trigger kind, local-device time,
position, color, dwell, generation, source list, and captured revisions.
Optional modeled metrics use NaN with an unavailable flag; numeric zero remains
a simulated or predicted value according to provenance, never a measurement by
itself. Every `_at` call checks the
index and the output record size/version.
Count, time, and revision accessors take output pointers and return statuses, so
a null handle or output pointer is distinguishable from a valid empty result.
`spgl_snapshot_calibration_revision` reports the calibration revision captured
by that snapshot; an existing snapshot keeps its original revision after later
world updates.

Snapshots own their result storage and remain valid after the source context
has been destroyed. Frames must be destroyed before their context; destroying
the context invalidates any frames still associated with it. Destroy functions
accept null. A context is single-threaded: calls that access or mutate one
context, its frames, or its last-error storage must be externally serialized.
Independent snapshots can be queried concurrently if each output buffer is
owned by its calling thread. Handles must not be used after destruction; the
library cannot safely detect arbitrary dangling pointers.

`spgl_context_last_error` returns a borrowed, NUL-terminated message owned by
the context. It remains valid until the next API call on that context, including
another error query, or context destruction. A null context has no error
storage; use the returned status as the error in that case. Error text is for
humans and must not be parsed as a machine protocol.

Fixed-width 64-bit IDs in query records are stable FNV-1a hashes of the
corresponding UTF-8 object IDs, with zero reserved for an absent ID. Hashes
make records fixed-size C PODs; callers needing collision-free names should
retain the IDs they supplied. Hashes are identifiers, not security tokens.

## Modeled behavior and limits

The API reports simulator/predicted provenance according to the C++ core. It
does not claim a value is measured unless a future backend supplies a measured
observation. Program events describe modeled ordered output, not photons
confirmed by hardware. Device profiles select only modes implemented by the
core; unsupported combinations return explicit errors/diagnostics. Availability
states are simulation inputs and do not certify hardware fail-safe behavior.
The implementation does not provide a hardware transport or a global atomic
presentation guarantee.

Laser fills are rejected by default. The context option exists for a future
explicit approximation, but the current core has no implemented fill
approximation, so enabling it does not make a galvo fill supported. The ABI
does not silently convert unsupported geometry into a different shape. The
supported C descriptor fields and defaults are listed in the public header and
are fixed for ABI v1.

## Shared library and C client

The Bazel target `//bindings/c:spatialgl_c` builds the shared C ABI library.
`//tests/capi:capi_smoke_test` compiles and links an actual C11 caller and
checks two-projector normalized light with calibrated gains, pre-latency
output, scheduled events, descriptor validation, submission, output queries,
and independent snapshot lifetime. `//examples/c:hello` is a small C11 usage
example.
