# C++ optical core API

The simulation-first typed API is declared in `spatialgl/optics.h` under
`spatialgl::optics` and built by Bazel target `//:optics`. `Runtime` copies its
world snapshot and rig at construction and copies submitted `DisplayList`
values. Display IDs are unique for the life of a runtime. The newest presented
display per app replaces that app's earlier list; expiry and cancellation do
not resurrect earlier content. `advance` is monotonic and `snapshot` returns an
independent value.

Targets are points, ordered polylines and rectangular patches attached to a
declared finite plane. World coordinates stay in world space. Surface-local
coordinates use `origin + u*x + v*y`. Plane translation and constant-axis
angular motion are evaluated relative to `WorldSnapshot.source_time`; the
snapshot's `valid_until` bounds its use. Display and program values retain
world and calibration revisions, and a world update invalidates stale output.

Fixed raster profiles use a calibrated pinhole frustum and direct ray
occlusion. Steerable raster profiles can point outside the fixed frustum; the
simulator accounts for angular speed and settling before predicted output.
Galvo output preserves sampled curve order (including the closing edge when
`DrawCall.closed` is true), inserts explicit blank events,
limits dwell and sample count, and schedules events in the device-local clock.
For clock drift `d`, local event time is `(1+d)*global_time + offset`; event
dwell is in local-clock seconds, while target dark-gap bounds are global
seconds. Snapshot contributions for a galvo are cycle-average predictions,
not instantaneous beams. Their `duty_fraction` and target `duty` are modeled
fractions of the scan cycle. Metrics the model cannot calculate, such as
position error, remain unavailable (`nullopt`). `max_position_error=0` requests no
bound; a positive requested bound is explicitly unsupported because the model
cannot certify it. Traversal minimum-duty and maximum-dark-gap requests are
checked against the predicted cycle metrics and produce `no-plan-found`
diagnostics when unmet. Device `sample_budget` bounds the number of samples in
any programmed target for every device kind.

`OVER`, `REPLACE` and `ADD` compose exact coincident sampled bins on the same
surface across points, polylines and patches. This finite-bin model does not
compute continuous coverage intersections between samples. `OVER` uses the
foreground opacity to attenuate a lower sample; `REPLACE` masks it; `ADD`
retains both. `EXCLUSIVE`, `NORMALIZED` and `ADDITIVE` are separate physical
output policies. `NORMALIZED` shares calibrated linear-light contribution per
channel and never boosts a device past calibrated gain; insufficient gain is
reported as a residual. The model does not cover material reflectance, BRDF or
coherent wave interference.

Rig resources are globally identified and exclusively reserved (capacity
one). Shared mirror/DAC conflicts are reported and the deterministic scheduler
does not claim global optimality. Unsupported, infeasible, no-plan and
invalidated outcomes are distinct. Tight synchronized groups are rejected
because no trigger-capable device model exists; clock uncertainty is retained
in receipts. `Provenance::kSimulated` and `kPredicted` never mean hardware
measurement.

Cancellation advances the generation and permanently retires that display ID.
Device availability can transition among available, unavailable, failed and
handoff states. Profiles and rig resources are configured at runtime
construction; use a new runtime to change the rig. Runtime calls on one
instance must be serialized by the caller.

Volume and free-space ray targets, camera reconstruction, stereo/eye/focus,
coherent/scattering optics, audio and haptics are unsupported or outside this
API. Enabling laser-fill approximation returns unsupported because no fill
approximation has been implemented.
