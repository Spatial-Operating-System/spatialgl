# Hardware demo candidates

Research date: **2026-10-03**. Upstream SDKs, headers, protocols, and fabrication
references were inspected. H01 now has an experimental sampled raster compiler
and optional GLFW presentation path; see [fixed raster output](raster-output.md).
The remaining entries are integration candidates. No projector, actuator or
laser has been connected or optically measured. SDK capability does not
establish achievable optical accuracy, latency, or mechanical payload.

## Recommended starting rig

Start with **one HDMI projector, a USB camera, and two matte planar targets**.
Reuse the same projector on a **bearing-supported, printed pan/tilt cradle with
two DYNAMIXEL servos** for the second experiment. This tests surface placement,
movement cost, settling, and expiry without first building laser electronics.

For the first scanning experiment, use **a Helios USB DAC and an existing ILDA
graphics laser in a controlled lab setup**. Evaluate the MIT-licensed C++
**Libera** transport layer alongside the manufacturer's smaller Helios SDK.
Both provide an actual integration path; neither supplies SpatialGL's world
calibration or a complete physical rendering model.

Keep hardware dependencies and device tests opt-in. The simulator and ordinary
`bazel test //...` must remain usable without USB devices, serial ports, a
display, vendor SDK installations, or hardware purchases.

## Shortlist

"Fit" describes the current simulator vocabulary. Only H01 has an output
prototype; it has not been validated on the listed projector.
Effort is relative engineering judgment: calibration and fabricated mechanics
can dominate a small SDK adapter.

| ID | Concrete configuration | Reusable control software | Fit and demo | Effort / main gap |
| --- | --- | --- | --- | --- |
| H01 | Existing HDMI projector or monitor; compact reference: ViewSonic M1 mini Plus | [GLFW](https://www.glfw.org/docs/latest/monitor_guide.html), [OpenCV](https://docs.opencv.org/4.x/d9/dab/tutorial_homography.html) | `FixedRaster`: experimental sampled marker/grid output and manual planar warp | Low; camera calibration and physical validation remain |
| H02 | Two independently connected HDMI projectors + camera | Same display/calibration stack | Two `FixedRaster` devices: overlap, ownership, handoff, light mixing | Medium; physical overlap and photometry |
| H03 | Small HDMI projector + 2× XL430-W250-T + U2D2 + printed cradle | [DYNAMIXEL SDK](https://github.com/ROBOTIS-GIT/DynamixelSDK) plus GLFW | `SteerableRaster`: alternate labels between two panels | Medium; kinematics, pose feedback, settle gate |
| H04 | Stationary HDMI projector + front-surface mirror + two servo axes + printed mirror holder | Same DYNAMIXEL SDK | Steerable raster with a folded optical path; lighter moving assembly | Higher; reflected-ray calibration, clipping, focus |
| H05 | Printed stepper pan/tilt derived from isaac879's mount; Arduino Nano, NEMA17, TMC2208, home sensors | [Pan-Tilt-Mount firmware/CAD reference](https://github.com/isaac879/Pan-Tilt-Mount), [AccelStepper](https://www.airspayce.com/mikem/arduino/AccelStepper/) | `SteerableRaster`: move, settle, then project | Medium; firmware acknowledgements, lost steps, backlash |
| H06 | Helios USB DAC + ILDA cable + RGB galvo graphics laser | [Helios C++ SDK](https://github.com/Grix/helios_dac), [Libera](https://github.com/sebleedelisle/libera-laser) | `Galvo`: outline, vector glyphs, blank transitions | Medium; calibrated sample compiler and scanner dynamics |
| H07 | Ether Dream 4 + Ethernet + an ILDA graphics laser | Libera; [libetherdream](https://github.com/j4cbo/j4cDAC/tree/master/driver/libetherdream) alternative | `Galvo`: FIFO backpressure, disconnect and cancellation experiments | Medium; stream lead and buffered tail |
| H08 | HeliosPRO, or Helios USB + OpenIDN adapter, + an ILDA graphics laser | Helios C++ SDK's IDN path or Libera | `Galvo`: network discovery and a two-controller rig | Medium; device identity, transport semantics, clock uncertainty |
| H09 | Existing LaserDock / compatible LaserCube **USB** unit | [laserdocklib](https://github.com/Wickedlasers/laserdocklib) or Libera | `Galvo`: integrated scanner/color source | Medium; exact model/protocol compatibility |
| H10 | ADJ Pocket Pro LED moving head + OLA-compatible DMX interface | [Open Lighting Architecture](https://github.com/OpenLightingProject/ola) | Spotlight/gobo scheduling; needs a new optical capability | Medium; fixture channel profile and beam footprint |
| H11 | Raspberry Pi + Pimoroni Pan-Tilt HAT + tiny mirror or LED + printed holder | [pantilthat-python](https://github.com/pimoroni/pantilthat-python) | Cheap aiming prototype; LED needs an emitter capability | Low mechanically; no measured axis position, Python-only reference |

### H01–H02: raster output over HDMI

An ordinary projector appears as a display: the application does not need a
projector manufacturer's graphics SDK. GLFW provides display discovery and
window/context management; the driver must still render pixels and present
them. OpenCV provides planar homography tools for the calibration stage.
[GLFW monitor guide](https://www.glfw.org/docs/latest/monitor_guide.html),
[OpenCV homography tutorial](https://docs.opencv.org/4.x/d9/dab/tutorial_homography.html).

The [M1 mini Plus specifications](https://manuals.viewsonic.com/M1_mini_Plus_Specifications)
provide a compact mechanical reference: HDMI input, 854×480 native resolution,
110×104×27 mm body, and 0.3 kg mass. This is a sizing reference, not a stock
or mounting compatibility guarantee. An already-owned projector is preferable
for H01. Lock focus and keystone settings during calibration.

SpatialGL's first raster compiler maps predicted samples into an RGB image
with clipping, a manual homography, and linear/sRGB encoding. It can export an
image or present it through GLFW. Sparse sample footprints are insufficient
for continuous filled patches, and encoding is not measured transfer-function
correction. Camera calibration remains a separate integration step. A buffer
swap confirms a host presentation operation; a camera or optical timing sensor
is needed to measure when light reaches the target. Black pixels also leave a
projector-dependent residual light level.

With two projectors, begin on a shared flat board. Compare exclusive ownership
with normalized blending, then interrupt one output. HDMI outputs should be
treated as independently presented until synchronization is measured. A summed
software RGB value is not a photometric calibration result.

### H03–H04: steerable projection with printed mechanics

The [DYNAMIXEL SDK](https://github.com/ROBOTIS-GIT/DynamixelSDK) provides C++ and
Python control on Linux, macOS, and Windows. Use the C++ side for SpatialGL's
driver. Protocol 2.0 group writes and group reads can command two axes and
retrieve their state. The [XL430 control table](https://emanual.robotis.com/docs/en/dxl/x/xl430-w250/)
includes goal/present position, present velocity, movement state, and motion
profile registers. U2D2 is the USB-to-actuator interface; the servos require
their own model-appropriate supply. See the
[ROBOTIS selection guide](https://github.com/ROBOTIS-GIT/emanual/blob/master/docs/en/faq/dxl-selection-guide.md).

For H03, print a base, pan carrier, tilt yoke, projector cradle, cable guides,
and a calibration-target holder. Balance the projector around the tilt axis
and support the assembly with bearings. Select motors using payload, lever arm,
acceleration, duty cycle, and thermal margin; stall torque is not a supported
payload rating. This is a proposed custom design; SpatialGL does not yet ship
CAD or an assembly validated for the M1 mini Plus.

For H04, print a mirror holder and two-axis gimbal while leaving the projector
stationary. Use a purchased front-surface mirror rather than a printed optical
surface. Smaller moving mass helps, but the full projected cone must fit the
mirror. The mirror angle is not the output ray angle: fit a reflected-ray
model, including changing virtual projector pose, image orientation, travel
limits, vignetting, and target focus. This is an inferred engineering design,
not an existing complete open-hardware projector kit.

The first driver should display black, move both axes, read positions until a
configured settle condition holds, then show the warped image. Encoder state
measures actuator pose, not target registration or brightness. Begin with
discrete target switching; add projection during motion only after measuring
the kinematic and optical errors. A mirror and a projector sharing one gimbal
must expose that gimbal as a shared resource.

### H05: an existing printable stepper design

[isaac879/Pan-Tilt-Mount](https://github.com/isaac879/Pan-Tilt-Mount) supplies
firmware, parts references, and STEP material; the README links to printable
[STL/STEP files](https://www.thingiverse.com/thing:4547074). Its default branch
is `TMC2208-Drivers` and describes a three-axis camera slider, so isolate the
pan/tilt assembly and adapt the payload mount rather than assuming it is a
drop-in projector gimbal. The design uses home sensors and USB serial control.

The inspected firmware exposes a 57600-baud interface with separate pan and
tilt instructions, homing, and debug status. Add framed commands, sequence IDs,
completion replies, and a timeout before using it as an asynchronous device.
The original parser's timing assumptions should not become SpatialGL's
transport contract. Home sensors and computed step counts do not reveal missed
steps during a move.
[Firmware header](https://github.com/isaac879/Pan-Tilt-Mount/blob/1f541e62e78d1afa249663efd2236ad38d011ea5/pan_tilt_mount_nano_code_tmc2208/panTiltMount.h).

Use the published mechanics as a starting point for stop-and-project demos;
measure backlash and settle time with the camera. The repository's MIT license
does not erase the firmware's AccelStepper dependency: the latter offers GPLv3
or commercial licensing. Check the separate model page's terms before
redistributing its CAD. [AccelStepper licensing](https://www.airspayce.com/mikem/arduino/AccelStepper/).

### H06–H08: ILDA scanning through a purchased controller

Helios is a DAC, **not a laser or a galvo assembly**. The manufacturer's page
lists the classic USB model at US$114 and HeliosPRO at US$219 as of the research
date, excluding the laser, cabling, and local charges. The classic device has
12-bit XY, 8-bit RGB/intensity, a maximum 4095-point frame, and a 65.5 kpps DAC
ceiling. The scanner's usable speed and scan angle can be much lower.
[Helios product and SDK guide](https://bitlasers.com/helios-laser-dac/).

The SDK provides discovery, frame readiness, high-resolution point input,
output stopping, and USB/IDN selection. Its high-resolution API converts for
older devices; it does not upgrade their physical resolution. Implement output
clipping, black travel samples, corner/blanking compensation, and sample-rate
conversion before transport. Use finite playback and an expiry policy rather
than allowing stale artwork to loop indefinitely.
[Inspected Helios header](https://github.com/Grix/helios_dac/blob/f772cbbfb3e0e5e7e61afc48868c88a85e3d5741/sdk/cpp/HeliosDac.h).

[Ether Dream 4](https://www.ether-dream.com/) uses Ethernet and remains protocol
compatible with earlier models. Its [protocol](https://www.ether-dream.com/protocol.html)
provides FIFO fullness, playback state, point rate/count, underflow indication,
ACK/NAK, and stop commands. These help measure transport behavior, but do not
measure beam position or emitted optical power. The legacy `libetherdream`
stop wrapper waits for the current frame to finish, whereas the wire protocol
has an immediate playback stop: the adapter must expose the distinction.
[Inspected library header](https://github.com/j4cbo/j4cDAC/blob/12ecbe565fcaa88f771b1bfdfb1852baca38fbe6/driver/libetherdream/etherdream.h).

[HeliosPRO](https://bitlasers.com/heliospro-laser-dac/) and the
[OpenIDN adapter](https://bitlasers.com/helios-laser-dac/) provide a network
route. Prefer wired Ethernet for timing experiments. Supporting USB Helios and
IDN with one SDK does not make their buffering, shutter control, and frame
repeat behavior identical; probe these capabilities per device.

A concrete compatible optical endpoint is the
[Laserworld DS-1000RGB MK5](https://www.laserworld.com/en/laserworld-ds/laserworld-ds-1000rgb-mk5):
the vendor documents external ILDA input, analog color modulation, and a
30 kpps scanner specification at 8 degrees. It is a **Class 4** laser, suitable
only for an appropriately controlled laser bench, not the default desktop demo.
Start with transport tests with output disabled; optical trials require a
contained beam path, matte termination, and a working physical interlock/stop.
Software expiry is an additional control, not a replacement for that hardware.
Its built-in LAN/DMX modes do not automatically expose an open arbitrary-point
stream compatible with Ether Dream or IDN; use the verified external ILDA path.

Print controller trays, cable strain relief, and calibration fixtures if useful.
Use purchased galvos, drivers, optics, and an appropriate enclosure; a printed
holder does not supply scanner calibration or a laser interlock.

### H09–H11: useful alternatives with different boundaries

For an existing LaserDock or compatible LaserCube USB unit,
[laserdocklib](https://github.com/Wickedlasers/laserdocklib) exposes sample
submission, DAC-rate queries, output enable/disable, and ring-buffer controls.
Its inspected source is older than the current Helios/Libera code. Confirm the
exact device generation before choosing it. A LaserCube network model uses a
different transport; neither a USB connector nor the product family name proves
compatibility. Libera provides separate USB and network backends.
[USB device header](https://github.com/Wickedlasers/laserdocklib/blob/51d13bcaff5f6fca3876ac6ca78864272dfb57db/lib/include/laserdocklib/LaserdockDevice.h).

The [ADJ Pocket Pro](https://www.adj.com/products/pocket-pro) is a purchased LED
moving head with pan/tilt, dimming, and discrete color/gobo controls. OLA can
deliver fixture channel values through DMX transports, including Art-Net/sACN
paths with suitable interfaces. This gives an effective “move a spotlight
between named targets” demo, but cannot render arbitrary raster images or
polylines. It needs a beam-footprint/discrete-pattern capability and a fixture
channel map, not a false `SteerableRaster` profile. Read the fixture manual for
the chosen channel mode. [OLA C++ client examples](https://docs.openlighting.org/ola/doc/latest/client_tutorial.html).

The [Pimoroni HAT library](https://github.com/pimoroni/pantilthat-python) is MIT
licensed and controls a tiny two-servo assembly over Raspberry Pi I²C. Its
Python transport is useful for a fast LED/mirror prototype, but supplies no
measured joint pose and is not a C++ driver. Keep calibration, allocation, and
planning in C++; a Python transport bridge would be an explicit prototype
exception. Do not mount a whole projector on the camera assembly without a
separate mechanical design. Verify support for the exact Pi/OS combination.

## Which libraries should SpatialGL reuse?

| Library | Role | License evidence / integration decision |
| --- | --- | --- |
| GLFW + OpenCV | Display/window output and camera/planar calibration | Zlib / Apache-2.0 for current main code; use bounded C++ components, keep GPU rendering explicit |
| DYNAMIXEL SDK | Serial actuator transport and feedback | Apache-2.0; strongest initial motion-driver candidate |
| Helios SDK | Small USB/IDN laser transport adapter | SDK MIT; libusb has its own LGPL terms; hardware/firmware have different, noncommercial terms |
| **Libera (`libera-laser`)** | C++ discovery, controller connection, point/frame queues, multiple DAC protocols | Main project MIT; preferred broad laser-transport candidate; retain upstream/submodule notices and validate the pinned backends |
| libetherdream | C-compatible Ether Dream transport | Source offers GPLv2/GPLv3/LGPLv3 choices; older implementation, useful narrow alternative |
| laserdocklib | USB LaserDock transport | LGPLv3; conditional on device generation |
| OLA | DMX network/interface routing | Inspected C++ client files LGPL-2.1-or-later; separate daemon/plugin packaging from SpatialGL core |
| OpenLase | Laser graphics/path-generation reference | Whole project GPLv2/v3; individual portions offer LGPLv2/v3; inspect each imported file |
| ofxLaser | Calibration, scan profiles, zones, graphics reference | Current main is noncommercial share-alike; do not treat it as a permissive core dependency |
| Modulaser `laser-dac-rs` | Alternative multi-DAC implementation and comparison | MIT declaration; Rust would add a separate toolchain/FFI boundary, so secondary to C++ here |

These observations come from upstream
[Helios SDK license](https://github.com/Grix/helios_dac/blob/f772cbbfb3e0e5e7e61afc48868c88a85e3d5741/sdk/cpp/LICENSE.md),
[Libera license](https://github.com/sebleedelisle/libera-laser/blob/c99959fbaa3270aadb0b5cad8622e008b3372446/LICENSE),
[Ether Dream source notice](https://github.com/j4cbo/j4cDAC/blob/12ecbe565fcaa88f771b1bfdfb1852baca38fbe6/driver/libetherdream/etherdream.c),
[OpenLase license](https://github.com/marcan/openlase/blob/e6c80165479a1fa85774673257e938f4066c939e/LICENSE.txt),
[ofxLaser's current terms](https://github.com/sebleedelisle/ofxLaser#licence), and
[laser-dac-rs](https://github.com/ModulaserApp/laser-dac-rs).
Pin versions and carry their actual notices when adding code; this document
does not add any of these dependencies.

Libera is particularly relevant: it already separates controller transport
from graphics. Its C++ interface exposes discovery/connection,
`isReadyForNewFrame`, `sendFrame`, point callbacks, and queue/buffer state.
The upstream frame format has a steady-clock presentation timestamp. Retain
SpatialGL's world-space composition and resource allocation above it; translate
the clock and sample coordinates at the adapter boundary. Decide whether Libera
frame scheduling or SpatialGL's executor owns pacing, rather than stacking two
uncoordinated queues.
[Libera integration guide](https://github.com/sebleedelisle/libera-laser/blob/c99959fbaa3270aadb0b5cad8622e008b3372446/docs/integration.md),
[controller header](https://github.com/sebleedelisle/libera-laser/blob/c99959fbaa3270aadb0b5cad8622e008b3372446/include/libera/core/LaserController.hpp).

Libera's documented stale-frame blanking is a useful host-side safeguard.
It does not establish SpatialGL's device expiry guarantee or remove samples
already in a hardware FIFO. Its CMake build also brings transport/platform
dependencies: a Bazel integration needs an explicit source/dependency map or
an isolated prebuilt-library boundary, with unused plugins/audio/GUI disabled.

## Concrete driver mapping

The following names are real upstream entry points. The mapping describes
implementation work still required in SpatialGL, not a runnable adapter.

| SpatialGL operation | Upstream entry points | Adapter responsibility |
| --- | --- | --- |
| Select a raster output | `glfwGetMonitors`, `glfwCreateWindow`, `glfwSwapBuffers` | Stable selection, render target, calibrated image, host presentation receipt |
| Apply a pan/tilt pose | DYNAMIXEL `GroupSyncWrite::addParam` / `txPacket` | Calibrated joint targets, packing, range and motion profiles; transmit success is not arrival |
| Observe pan/tilt | `GroupSyncRead::txRxPacket` / `getData` | Timestamped joint feedback, communication/device errors, settle decision |
| Submit a classic Helios frame | `OpenDevicesOnlyUsb`, `GetStatus`, `WriteFrameHighResolution`, `Stop` | Device identity, calibrated samples, readiness, playback flags and stop latency |
| Submit through Libera | `System::discoverControllers` / `connectController`, controller `sendFrame` | Explicit device choice, arming, point conversion, clock bridge and queue policy |
| Stream to Ether Dream | `etherdream_connect`, `etherdream_is_ready`, `etherdream_write`, `etherdream_stop` | Bounded queued duration, reconnect policy, actual stop semantics |
| Stream to LaserDock USB | `set_dac_rate`, `send_samples`, `disable_output`, `clear_ringbuffer` | Rate limits, buffer accounting and disabled-output recovery |
| Send a DMX fixture state | OLA client `SendDMX` | Universe/fixture ownership, channel encoding, blackout and device-specific latency |

## Changes needed before calling a driver “supported”

The current `DeviceProfile` and `TimedProgram` are simulation contracts. They
lack a production transport interface, raster payloads, actuator joint-space
commands, device queue acknowledgements, and measured output records. Keep the
existing simulator/C ABI intact while defining a versioned hardware boundary.

```text
Python DisplayList
        |
C++ composition / allocation / calibrated device compiler
        |
RasterFrame + optional JointTrajectory  OR  LaserSampleBlock
        |
C++ executor: generation, deadline, queue budget, device identity
        |
GLFW / DYNAMIXEL SDK / Helios SDK or Libera / OLA
        |
Transport status + actuator telemetry + separately calibrated optical capture
```

Proposed responsibilities, not new exported API names:

1. **Probe and open:** report protocol, geometry, rates, queue depth, repeat/stop
   semantics, identity, and firmware. Select a configured device explicitly.
2. **Compile:** carry world/calibration revisions and convert to pixels, joint
   trajectories, or uniformly sampled scanner coordinates. Reject unsupported
   geometry; galvo fill remains unsupported until a fill compiler is validated.
3. **Submit:** copy/own payloads and report queue acceptance separately from
   predicted presentation. Bound queued duration so cancellation is useful.
4. **Cancel and expire:** invalidate generations, discard unsent work, request
   device stop, and report any already-buffered tail. Reconnection requires a
   fresh submission; it must not replay expired content.
5. **Observe:** record source and host timestamps with uncertainty. Separate
   host submission, controller playback, measured joint position, and measured
   target light; an SDK success code must not become `Measured` optical output.

Use a composite steerable device over separate raster and actuator drivers.
The planner sees one useful output capability plus shared resources; SDKs stay
in focused transport modules. Proposed future locations are
`libs/drivers/{raster,dynamixel,laser,dmx}` and `examples/hardware/`, with explicit
optional Bazel targets. These directories are not implemented by this document.

## Demo sequence and evidence to collect

| Stage | App/demo | What it establishes | Record before claiming support |
| --- | --- | --- | --- |
| D1 · H01 | Anchored grid and a moving surface-local marker | Raster calibration and Python-to-native output | Camera registration error, latency distribution, expiry behavior |
| D2 · H03 or H05 | One label alternates between two boards | Reachability, movement cost, settle-before-display | Joint/camera traces; dark interval, backlash, missed deadlines |
| D3 · H06 | Ordered glyph/outline plus blank travel | Sample compiler and scanner transport | Bounded point count, rate/angle settings, optical registration; disconnect/stop trace |
| D4 · H01 + H06 | Filled raster panel with laser outline | Technique-specific geometry and mixed output ownership | Separate photometry, per-device timing and predicted/measured provenance |
| D5 · H02 or two H07/H08 | Two outputs share a target; one fails | Conflict policy, handoff and independent clocks | Buffered-tail duration, overlap changes, availability and no stale replay |
| D6 · Two apps sharing one H03/H04 gimbal | Competing apps request different targets | Explicit shared mechanical resource | Ownership trace and blanking during contention; do not claim simultaneous aim |

A camera view can also demonstrate an occluder-triggered reroute after a new
world snapshot; camera reconstruction is additional work. Report the sensing
delay and revision used, rather than implying the present AABB simulator
already observes obstacles.

Measure calibration error on held-out target points, move/settle time over
different distances, and timing over repeated trials. Publish raw traces and
the configuration with the demo. Do not invent accuracy or frame-rate targets
before selecting the rig. Hardware acceptance remains a separately invoked
suite, while transport fakes and encoder/queue tests can run without devices.

## Inspected upstream revisions

These are research pins, not installed dependencies or a compatibility matrix.

| Upstream | Commit inspected | Commit date |
| --- | --- | --- |
| Helios SDK | `f772cbbfb3e0e5e7e61afc48868c88a85e3d5741` | 2026-09-07 |
| DYNAMIXEL SDK | `f838bc90f72fcf5b9c279432d6e12fc24969daa8` | 2026-09-14 |
| Libera | `c99959fbaa3270aadb0b5cad8622e008b3372446` | 2026-09-18 |
| Pan-Tilt-Mount | `1f541e62e78d1afa249663efd2236ad38d011ea5` | 2021-10-29 |
| libetherdream / j4cDAC | `12ecbe565fcaa88f771b1bfdfb1852baca38fbe6` | 2020-07-28 |
| laserdocklib | `51d13bcaff5f6fca3876ac6ca78864272dfb57db` | 2022-10-11 |
| OLA | `8cf04267302de88e9ff2e93dcc05c0c848ca7be3` | 2026-09-21 |
| OpenLase | `e6c80165479a1fa85774673257e938f4066c939e` | 2020-09-01 |
| laser-dac-rs | `92e632ad8d5bc303e8b3b370595a1c3967db29cb` | 2026-09-30 |

Related contracts: [core API](core-api.md), [module layout](modules.md),
[coverage audit](cases-and-design-coverage.md), and [roadmap](roadmap.md).
