# Fixed raster output

The first output path turns a fixed projector's **predicted samples** into an
image. It can save that image without a display or present it in a GLFW window
on a connected monitor/projector. This is the H01 prototype from the
[hardware guide](hardware-demos.md); it does not add actuator or laser control.

## Run the demo

```sh
bazel run //examples/hardware:raster_demo -- --output .artifacts/raster-demo
```

The output directory contains a PPM image and JSON metadata. The scene has a
sampled grid and a marker. The metadata identifies the output as predicted and
optical feedback as unavailable. Headless export does not load GLFW.

With a separately installed GLFW runtime:

```sh
bazel run //examples/hardware:raster_demo -- --window --duration 10
```

The default is a window. To select an HDMI output deliberately, enumerate
displays and then choose an index for fullscreen presentation:

```sh
bazel run //examples/hardware:raster_demo -- --list-displays
bazel run //examples/hardware:raster_demo -- --window --display 1 --duration 10
```

Display indices describe the current enumeration and can change after a
connection changes. Do not persist an index as a hardware identity. Check the
display list before a fullscreen demo.

Pass `--glfw-library /absolute/path/to/libglfw.3.dylib` when the library is
outside the platform loader's search path. Linux typically uses `libglfw.so.3`;
Windows uses `glfw3.dll`. Only the macOS configuration has been validated in
this project. The driver uses compatibility OpenGL, so a compatible desktop
context is required; OpenGL ES and core-only contexts are not this path.

### Build an optional runtime from source

GLFW is not linked into the simulator. SpatialGL vendors its unmodified 3.4
public header and license, then loads a shared library only when window output
or display discovery is requested. One way to provide it is:

```sh
git clone --branch 3.4 --depth 1 https://github.com/glfw/glfw.git /tmp/spatialgl-glfw
cmake -S /tmp/spatialgl-glfw -B /tmp/spatialgl-glfw-build \
  -DBUILD_SHARED_LIBS=ON -DGLFW_BUILD_EXAMPLES=OFF \
  -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF -DGLFW_INSTALL=OFF
cmake --build /tmp/spatialgl-glfw-build
```

Use the resulting shared library with `--glfw-library`. Consult
[GLFW's build instructions](https://www.glfw.org/docs/3.4/compile_guide.html)
for platform development dependencies. The ordinary project test suite does
not require this build, a display server, or a connected projector.

## Use an app's observations

```python
from spatialgl import optics, raster

device = optics.fixed_raster("projector", width=640, height=480)
runtime = optics.OpticalRuntime(
    optics.world([optics.plane("wall")]), optics.rig([device]),
)
runtime.submit(optics.display(
    "marker", [optics.draw("dot", "wall", color=(0, 1, 0))],
    expires_at=1,
))
frame = raster.compile(runtime.advance(0.05), device)
raster.write_ppm(frame, "/tmp/spatialgl-frame.ppm")
```

Compilation consumes an existing optics snapshot. The optics runtime still
owns scene composition, device allocation, visibility, command delay, and
lifecycle. The output layer selects one fixed raster device and projects its
eligible contributions into pixels; it does not independently schedule draw
calls or report physical measurements.

`RasterFrame.rgb` is an immutable byte copy in top-to-bottom RGB order. The
frame carries its device ID, dimensions, time, expiry, world/calibration
revisions, and diagnostics. Empty, expired, cancelled, or unavailable output
compiles to black. Malformed or inconsistent input must not resurrect a
previous image.

## Manual planar calibration

The output correction is a homography on normalized image coordinates, with
`(0, 0)` at the top left and `(1, 1)` at the bottom right. Provide four
correspondences between predicted source image coordinates and desired output
coordinates, or provide the matrix directly:

```json
{
  "source_corners": [[0, 0], [1, 0], [1, 1], [0, 1]],
  "destination_corners": [[0.08, 0.05], [0.93, 0.09], [0.90, 0.94], [0.06, 0.90]],
  "point_radius": 2,
  "transfer": "srgb",
  "lease_seconds": 0.1
}
```

```sh
bazel run //examples/hardware:raster_demo -- \
  --calibration calibration.json --output .artifacts/calibrated
```

The four-point fitter runs in the native module. Degenerate and singular
transforms, nonfinite values, and transforms crossing a projective pole are
rejected. Corners alone are not an accuracy measurement: check registration
on separate target points with the projector's focus and keystone fixed.
Automatic camera acquisition, lens-distortion fitting, nonplanar warp, and
measured photometric calibration remain future work.

## Sampling and light interpretation

Each contribution becomes a square sample footprint with configurable pixel
radius. Ordered lines and patches retain the simulator's finite sampling;
the output layer does not infer continuous line or filled-patch coverage.
Sampling spacing and footprint size therefore affect appearance.

Coincident world samples sum their already-composed emitted colors. Distinct
samples that land on the same pixel use a componentwise maximum so increasing
sampling density does not add brightness repeatedly. This is a specified
sample-visualization rule, not a general framebuffer blending model. Output
values saturate to the display's byte range.

The default transfer encodes linear values as sRGB. The `linear` option writes
linear values into bytes instead. Neither option measures or compensates a
projector's actual transfer function, black level, reflectance, or color gamut.

## Presentation and expiry

The window driver and display enumeration belong on the application's main
thread. Keep construction, event processing, presentation, and destruction on
that thread. `present(frame, now)` and `poll(now)` use the same monotonically
advancing scene clock as the optics runtime. The demo bridges it to elapsed
host monotonic time and samples it again after image compilation.

A frame has a short application lease, conservatively bounded by relevant
program expiry. Continue polling to clear an expired frame. New observations
after cancellation, availability changes, or revision invalidation produce
black output. The caller must replace the displayed frame with that result;
a previously copied image does not learn about later runtime mutations.

Closing the driver clears its output. A host swap is a software presentation
operation; it is not a receipt for photons at the target. Event-loop stalls,
process crashes, HDMI buffering, and projector persistence are outside the
lease guarantee. This implementation has no independent hardware watchdog or
measured blanking deadline.

## Modules and verification

The native interfaces are [raster.h](../include/spatialgl/raster.h) and
[glfw_output.h](../include/spatialgl/glfw_output.h), implemented under
`libs/raster` and `libs/drivers/glfw`. Python exposes `spatialgl.raster` through
a separate binding. Existing optics contracts and the C ABI are unchanged.

The public output surface consists of `Calibration`, `RasterFrame`,
`fit_homography(source, destination)`, `compile(snapshot, device, calibration)`
and `write_ppm(frame, path)`. `GlfwOutput.displays()` returns current display
indices and dimensions. A `GlfwOutput` owns one window; `present(frame, now)`
and `poll(now)` return false after closure, while `blackout()` and `close()`
explicitly clear output. Invalid inputs raise exceptions. Supply a runtime
library path to the constructor or display enumeration when needed.

Headless tests exercise expected pixel positions and colors, calibration,
clipping, composition, leases, cancellation, availability, revisions, input
validation, and image export. Window output is a separately invoked smoke
check. See the [validation record](validation.md) for executed checks and
platform limits.
