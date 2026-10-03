<p align="center">
  <img src="docs/assets/spatialgl-logo.png" alt="SpatialGL" width="560">
</p>

# SpatialGL

**Spatial graphics for projectors and scanning lasers.**

SpatialGL lets an application describe what to display, where it belongs in physical space, and when it should appear. The runtime accounts for device reach, movement, timing, and shared resources, then reports the predicted output.

Start in simulation with fixed projectors, steerable projectors, and galvo laser scanners. Use familiar graphics concepts—draw calls, layers, and presentation—to explore how different devices can realize the same scene.

## Quick start

Install [Bazelisk](https://github.com/bazelbuild/bazelisk) and the platform build tools listed in the [setup and validation guide](docs/validation.md).

```sh
git clone https://github.com/Spatial-Operating-System/spatialgl.git
cd spatialgl
bazel test //...
bazel run //examples/optics:scenarios
```

Validation currently covers macOS arm64.

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

Save the example as `app.py`, then run it with the project launcher:

```sh
bazel run //python:python -- app.py
bazel run //python:python  # Interactive Python with spatialgl imported as sgl
```

The two projectors share the requested green light under the default `NORMALIZED` output policy. The example inspects the combined target light and each device's planned output. See the [app API guide](docs/python-api.md) for more examples and usage details.

## What you can explore

| Area | Current behavior |
| --- | --- |
| Graphics | Points, ordered lines, and rectangular patches on planar surfaces |
| Devices | Fixed and steerable projection, plus ordered laser scanning |
| Composition | Layering, replacement, and additive graphics |
| Physical output | Exclusive ownership, shared light output, and additive illumination |
| Resources | Competing apps and devices sharing one physical resource |
| Time and motion | Moving surfaces, command delay, steering/settling time, and independent device clocks |
| Lifecycle | Replacement, expiry, cancellation, device availability, and recalibration |
| Observations | Predicted light, device plans, conflicts, timing, and unsupported requests |

Layer composition and physical light mixing are separate policies. The runtime reports when a request cannot be realized within a device's constraints.

The [multi-device design audit](docs/cases-and-design-coverage.md) examines reference systems and constructed cases that test these trade-offs.

## Explore the viewer

```sh
bazel run //python:demo -- --port 5188
```

Open [http://127.0.0.1:5188/](http://127.0.0.1:5188/). The lab compares two fixed projectors, a monitor, and simulated point-emitting drones. Change refresh rates, latency, occlusion, drone count, and speed; pause, step, reset, or export observation traces.

```sh
bazel run //python:simulate
bazel run //python:python -- examples/hello.py
bazel run //python:demo -- --output .artifacts/view.html
```

The viewer explores the original device models. Run `//examples/optics:scenarios` for the newer surface-optics experiments.

## Try display output

Save a sampled grid and marker as an image:

```sh
bazel run //examples/hardware:raster_demo -- --output .artifacts/raster-demo
```

With an optional GLFW runtime, add `--window` to show an animated demo on a monitor or HDMI projector. See the [display output guide](docs/raster-output.md) for display selection and manual calibration.

## Model limits

SpatialGL predicts optical output and can present those samples on a fixed raster display. Actuator/laser drivers and measured optical feedback are planned. Composition uses finite samples, and steering/scanning use simplified dynamics. Filled laser shapes, guaranteed hardware synchronization, and certified position-error bounds are unsupported. See the [model assumptions and limits](docs/core-api.md) for the full contract.

## Build a physical demo

The [hardware demo guide](docs/hardware-demos.md) compares 11 configurations, including HDMI projectors, printed pan/tilt mounts with DYNAMIXEL or stepper control, Helios/Ether Dream/IDN laser controllers, and DMX moving lights. It links inspected upstream SDKs and fabrication references, maps concrete control calls to future drivers, and defines six experiments with measurable outcomes.

The recommended sequence is one HDMI projector, a printed steerable mount, then an ILDA scanning setup. The fixed display prototype is available; the remaining device drivers are integration candidates. Hardware dependencies and device acceptance tests remain opt-in.

## Development

Run `bazel test //...` for changes. See the [contributor instructions](AGENTS.md) and [module guide](docs/modules.md) for implementation and repository structure.

## Documentation

- [App API](docs/python-api.md)
- [Display output](docs/raster-output.md)
- [Core API and model assumptions](docs/core-api.md)
- [Native integration](docs/c-api.md)
- [Architecture](docs/architecture.md)
- [Roadmap](docs/roadmap.md)
- [Setup and validation](docs/validation.md)
- [Logo provenance](docs/assets/README.md)
