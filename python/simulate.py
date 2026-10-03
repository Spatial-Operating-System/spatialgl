"""Run the headless Python example: bazel run //python:simulate."""
import json
from spatialgl import Drones, Point, Runtime, SceneFrame, World


def main():
    runtime = Runtime(World(), [Drones("drone", count=1, speed=1,
                      home=(0, 1, 0), refresh_hz=10, latency=0.1)])
    runtime.submit(SceneFrame("hello-space", present_at=0, expires_at=3,
                             primitives=[Point("light", (1, 1, 0), (1, 0.7, 0.2))]))
    for t in [0, 0.1, 0.5, 1, 1.1, 3]:
        state = runtime.advance(t)
        print(json.dumps(state.to_dict()))


if __name__ == "__main__":
    main()
