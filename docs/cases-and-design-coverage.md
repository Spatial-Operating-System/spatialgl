# Multi-device cases and SpatialGL design coverage

Reviewed on 2026-10-03. **This is a historical design audit of commit `19baf72`
and the v0.2 architecture.** The subsequent surface optics implementation is
documented in the [core API](core-api.md) and [validation record](validation.md).
Its acceptance tests do not establish complete reproduction of the referenced
research systems.

## Findings and scope

At the time of this audit, the implementation contained fixed projectors and
monitors, point-emitting drones, static planes/AABBs, sparse samples, and
independent device refresh/latency/expiry. SurfaceTarget, DisplayList, and
sampling/traversal/presentation state were proposals.

Those proposals alone did not cover every case. Missing common contracts were
**joint optical composition, shared physical resources, timestamped world
state, observers/channels, and staged execution evidence**. Independent
`evaluate(sample)` calls and single-device assignment cannot express all of
these interactions.

R01–R12 are cases grounded in papers, researcher pages, official implementations,
or standards. A standard's expressive capabilities do not guarantee hardware
performance. T01–T08 are constructed counterexamples, not deployed systems.
Coverage judgments and proposed changes are design inferences about SpatialGL.
This audit compares publications and project descriptions; it is not a report
of reproducing their systems or executing all the proposed simulator fixtures.

## Reference cases

| ID / case | Physical arrangement and documented behavior | Historical v0.2 coverage | Missing contract / trade-off |
| --- | --- | --- | --- |
| R01 RoomAlive | Calibrated projectors and Kinect depth data for room projection mapping; the toolkit example uses six projectors and six Kinects. [Official code](https://github.com/microsoft/RoomAliveToolkit) | Approximates only part of independent fixed-plane output; no depth mesh, images, or composition. | Shared world coordinates, calibration revisions, dynamic meshes. Coverage vs. alignment/composition cost. |
| R02 LightSpace | Calibrated depth cameras/projectors move projected objects across tables, walls, and hands. [Researcher description](https://www.microsoft.com/en-us/research/project/lightspace/) | Static surfaces only; no hand motion or anchor changes between surfaces. | Time-dependent target binding, handoff, stale tracking. Latency vs. estimation stability. |
| R03 Shader Lamps | Multiple projectors alter physical object appearance and compose overlap. [Author paper](https://web.media.mit.edu/~raskar/Shaderlamps/ShaderLamps_2000.pdf) | Sparse RGB is not a physical light/appearance model. | Multiple contributions to one target, material/photometric response, gamut. Brightness/alignment vs. appearance accuracy. |
| R04 Beamatron | Motorized pan/tilt projection with depth sensing, stabilization while steering, and geometry-aware graphics. [Author paper](https://www.microsoft.com/en-us/research/wp-content/uploads/2012/10/WilsonUIST2012.pdf) | Fixed pose; steering absent. | Pose trajectories, feedback, timed image warp, sensor/actuator dependencies. Reach vs. movement/settling time. |
| R05 Lumipen | High-speed camera and coaxial Saccade Mirror project patterns onto moving targets. [Lab description](https://ishikawa-vision.org/mvf/Lumipen/index-e.html) | Static world and fixed projector cannot represent it. | Object poses with measurement time, prediction, validity, and output-time optical transforms. Freshness vs. prediction error. |
| R06 Synthetic-aperture shadowless projection | Overlapping projectors reduce occlusion shadows and compensate blur from misalignment; 2026 preprint. [Paper abstract](https://arxiv.org/abs/2603.11551) | Occlusion fallback only; no simultaneous overlap. | Joint contributions, partial visibility, blur/energy composition. Shadow robustness vs. blur/cost. |
| R07 Heterogeneous projectors as room lights | Different projectors provide selective environmental illumination beyond a projection target. [Author paper](https://arxiv.org/abs/2403.02547) | No ambient/environmental lighting objective. | Shared graphical/illumination passes; controllable vs. uncontrolled ambient light. Target contrast vs. room lighting quality. |
| R08 REFLCT | Tracked head-mounted projectors and retroreflective surfaces provide user-specific perspective-correct images. [Author research record](https://vgl.ict.usc.edu/bibtexbrowser.php?bib=ICT.bib&key=krum_head_2016) | One RGB/position target, without an observer. | Observer-indexed targets, directional surface response, cross-talk. Personalization vs. optical separation/calibration. |
| R09 Multifocal stereoscopic projection mapping | High-speed projection, active shutter glasses, and electrically tunable lenses synchronize eye-specific imagery and focus. [Author paper](https://arxiv.org/abs/2110.07726) | Per-device apply times, without eye/phase/focus dependencies. | Coupled rigs, eye/focus channels, phase windows, triggers/clocks. Depth planes and eye-specific output vs. duty/brightness. |
| R10 Multi-projector structured-light vision | Overlapping projector patterns make source-pattern identification difficult in camera observations. [Author paper](https://arxiv.org/abs/1508.07859) | Emission only; no capture task. | Measurement/display passes, camera exposure intervals, pattern IDs, interference scheduling. Display continuity vs. measurement quality. |
| R11 Multi-head laser / IDN | Official material describes timestamped streams, multiple channels, and sample-synchronized multi-head output; this verifies standard expressiveness. [ILDA technical material](https://www.ilda.com/technical.htm) | No laser scan commands or clock/stream queues. | Timed scan samples, clock domains, phase relations, buffering/underrun/cancel. Timing alignment vs. buffering delay. |
| R12 MATD | Acoustic trapping moves a particle illuminated by RGB while supplying audio/tactile content. [Original research](https://www.nature.com/articles/s41586-019-1739-5) | Point drones resemble only part of the geometric objective; no relevant physics or multimodal output. | Volume targets, shared trapping/illumination rigs, audio/haptic channels. Boundary case outside surface optics v1. |

RoomAlive and similar systems do not imply that SpatialGL must own object
recognition. The inferred renderer contract consumes timestamped estimates
and uncertainty produced by upstream sensing/tracking.

## Constructed counterexamples and acceptance conditions

These conditions were fixture specifications at the time of the audit, not
executed results. Current implemented checks are recorded separately.

| ID | Devices / situation | Failure of the baseline or proposal | Required acceptance condition |
| --- | --- | --- | --- |
| T01 | Raster background and laser cursor that should mask it | A laser command alone cannot remove background light. | Joint composition generates raster masks and laser paths; unsupported synchronization explicitly yields independent output or rejection. |
| T02 | One of two projectors disconnects; old commands arrive late | Retired output may be relit. | Command generations/epochs, bounded cancellation capabilities, stale-packet rejection, failover provenance; distinguish software rejection from confirmed darkness. |
| T03 | Two apps/logical backends share one steering mirror | Locally feasible plans request different poses simultaneously. | Shared physical-resource reservations cannot overlap; execution checks leases/epochs. Simulator cloning is not hardware ownership. |
| T04 | Polyline edits change sample count; device handoff follows | Generation-order IDs no longer identify the same point. | Stable draw IDs and surface/path correspondence; explicitly reassign when correspondence fails rather than preserving incorrect tracking. |
| T05 | One steerable projector must revisit two walls with minimum brightness | Mean refresh hides long dark intervals. | Per-location dark-gap/revisit, duty, and age; distinguish limited search failure from proved physical infeasibility. |
| T06 | Request a free-space line/patch from a surface projector or laser spot | Projecting onto a wall changes the graphic. | Explicit surface/volume/ray-path target kinds; reject unsupported requests. Fog/scattering requires a separate model. |
| T07 | Device drift/jitter with a fence requiring all outputs visible | Command receipt/apply time is mistaken for simultaneous emission. | Clock uncertainty and phase/skew bounds, staged receipts, unavailable metrics; reject or negotiate unsupported synchronization. |
| T08 | Previous output persists after new-scene expiry and calibration changes | Old output is interpreted as realized in new geometry. | Preserve source scene/world/calibration revisions and age; retain expiry/stale/no-resurrection semantics while distinguishing prediction from observation. |

## Common contract extensions

### 1. Separate targets from device assignment: joint contributions

`sample -> one device` cannot cover R03/R06/T01. Multiple devices may realize
one target, and one emission may influence multiple targets. A first optical
model can assume calibrated incoherent light and direct transfer:

```text
E_target(x, t) ≈ E_uncontrolled(x, t) + Σ_i A_i(q_i(t), world(t)) u_i(t)
0 ≤ u_i(t) ≤ device_limit_i
```

`A_i` represents pose, visibility, footprint, and calibrated surface-light
transfer. Appearance needs reflectance and observer response. Do not directly
sum sRGB in this expression. Begin with known diffuse surfaces and coarse
spatial bins; this does not cover coherent interference, arbitrary BRDFs, or
volumetric scattering.

Virtual `OVER` and physical `EXCLUSIVE / NORMALIZED / ADDITIVE` are separate
stages. Equal average light can still have different maximum dark gaps.
Policies cannot generate negative light; unattainable appearance needs a
photometric residual.

### 2. Extend device lists into rigs with resource graphs

R09/T03 can share mirrors, laser DACs, DLP engines, cameras, and trigger lines.
A rig needs physical resource IDs, actuator coupling, exclusive modes, clock
domains, trigger support, and buffer/cancel/ack capabilities. Plans reserve
time intervals. Stereo/focus/image tasks become one coupled schedule. A fresh
simulator clone must not be interpreted as a cloned physical device.

A deterministic small candidate search is a valid first planner. Distinguish
unsupported, proved-infeasible, no-plan-found, accepted, and invalidated;
exhausting a search budget is not a proof of physical impossibility.

### 3. Place timed programs inside command envelopes

Laser paths, steering, DLP exposures, and shutters have intervals/phases rather
than instantaneous application. Preserve SceneFrame/DeviceFrame/RealizationState
separation and extend the command envelope:

```text
DeviceFrame envelope:
  generation, source_scene, world_revision, calibration_revision
  issued_at, valid_interval, clock_domain, timing_uncertainty
  update_mode: replace | append | cancel
  TimedProgram: actuator trajectory + raster exposures | scan samples | trigger events
```

Scan samples preserve ordering, emission/blanking, dwell, and output time.
Display lists must retain raster/curve semantics before sampling loses that
information. Pipeline/traversal/presentation state guides planning; it does not
change physical dynamics. Stroke-level replacement boundaries do not imply
globally atomic visibility. Synchronization needs hardware capabilities and
clock uncertainty; legacy commands can remain single hold intervals.

### 4. Consume timestamped world snapshots

For R02/R04/R05, geometry's observation time matters as much as its shape.
World snapshots need source time, reference frame, transform/mesh revisions,
validity horizon, uncertainty, and optional prediction. Targets use local
coordinates and surface/object anchors.

The renderer consumes tracking instead of reimplementing it. Tracking or
calibration changes revalidate affected plans. Old observations must not be
reinterpreted in new world coordinates. Observers also have timestamped poses;
emitted-ray visibility and observer visibility are different quantities.

### 5. Make target and pass kinds explicit

SurfaceTarget alone cannot express R08/R09/R10/R12/T06. Start with surface
optics and define later boundaries:

- SurfaceTarget: a surface-bound graphic or calibrated appearance.
- ViewTarget: observer/eye/channel-specific graphics requiring optical separation.
- MeasurementPass: pattern emission and sensor exposure scheduling; reconstruction stays upstream.
- VolumeTarget and audio/haptic channels: future contracts, explicitly unsupported in v1.

Graphics, illumination, and measurement passes can share physical resources.
Policy scopes include draws, composition groups, and rig schedules. One app's
draw state cannot relax another app's resource limits. Keep AppId/LayerId
identity and perform physical allocation after composition.

### 6. Extend feedback beyond position error

R06/R09/T05/T07 can differ in output quality at the same position. Report
frame age, revisit/dark gap, duty, photometric residual, group skew/phase,
missing coverage, and budget use in addition to geometric error.

Receipts distinguish accepted, queued, latched, actuator-settled,
emission-predicted, and emission-measured where supported. A GPU fence or
driver acknowledgement is not a photon measurement. Predictions retain model,
world, and calibration revisions; observations retain sensor time and
uncertainty. Unavailable values are not numeric zero or success.

## State concepts borrowed from OpenGL

| Concept | Relevant cases | What state alone cannot solve |
| --- | --- | --- |
| Sampling/filtering/LOD | R01/R03/R06, laser detail | Physical footprint and time budgets |
| Virtual depth/masks/composition | R01/R03/T01 | Emitter visibility, observer transfer, inability to subtract light |
| Traversal/batching | R04/R05/R11 | Shared actuators and trajectory cost |
| Buffering/presentation | R05/R09/R11/T07 | Hardware phases and clock uncertainty |
| Render passes/FBO | R03/R07/R10 | Joint optical transfer and sensor capture scheduling |

These concepts are useful; adding state does not automatically implement every
physical model.

## Implementation priorities and simulator fixtures

### v1: focus on surface optics

1. DisplayList, stable DrawId, and SurfaceTarget; retain raster/ordered curves.
2. Rig/resource IDs and timed raster/scan/steering programs with scoped states.
3. Joint direct-light contributions on coarse bins and timestamped planes.
4. Generation-aware queues/cancel, clock uncertainty, and prediction/measurement provenance.
5. Compare policy trade-offs through the following traces.

| Fixture | Arrangement | Desired observations |
| --- | --- | --- |
| F01 overlap | Two fixed projectors, one wall, partial occlusion | Joint target contributions, light sum, ownership change, dark gaps |
| F02 hybrid | Raster background and galvo cursor | Virtual OVER vs. physical additive, shared mask, version skew |
| F03 shared steering | Two apps/logical outputs, one mirror, two surfaces | Resource conflicts, revisit/duty/age, no-plan-found vs. infeasible |
| F04 moving target | Moving plane, sensor delay, steerable projector | Capture/prediction/output-time alignment, tracking loss, revalidation |
| F05 async laser pair | Different rates and latencies | Order, blanking, phase, drift/jitter/underrun, supported synchronization bounds |
| F06 fault/handoff | Disconnection, late old commands, calibration updates | Generation rejection, prior output provenance, cancellation evidence |

Passing six minimal fixtures does not reproduce the research systems.
Convergence should also be checked by varying bin size and integration time.
Physical truth remains limited by simulator assumptions. The implemented
subset and its unsupported cases are recorded in [validation](validation.md).

### Later scope

R08/R09 directional/eye/focus targets, R10 measurement scheduling, and R07
environmental illumination need separate milestones. R12, fog/volume,
coherent optics, audio, and haptic physics are outside the first optical
implementation. The API must explicitly reject unavailable targets.

## Audit verdict

Historical v0.2 was a baseline, not complete multi-device coverage. SurfaceTarget
and small OpenGL-like state objects also need joint contributions, shared
resources, time, and observer contracts. Testing those common contracts is a
stronger foundation than adding more device kinds without shared semantics.
