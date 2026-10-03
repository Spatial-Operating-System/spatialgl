"""Run with: bazel run //python:python -- examples/hello.py"""
from pathlib import Path
import spatialgl as sgl
from spatialgl.view import write_view

world = sgl.World()
runtime = sgl.Runtime(world, [sgl.Drones('lights', count=1, speed=1, home=(0, 1, 0))])
runtime.submit(sgl.SceneFrame(
    id='hello-space', present_at=0, expires_at=3,
    primitives=[sgl.Point('light', position=(1, 1, 0), color=(1, 0.65, 0.2))],
))
state = runtime.advance(0.5)
for sample in state.samples:
    print(f'{sample.sample.id}: {sample.status}, actual={sample.actual}, error={sample.error:.3f} m')
print(write_view(Path('.artifacts/hello.html'), state, world))
