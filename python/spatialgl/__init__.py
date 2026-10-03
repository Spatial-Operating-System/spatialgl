"""SpatialGL: Python scene authoring over the Bazel-built C++ simulation core."""
from math import pi
from ._native import (
    Backend, DeviceDescriptor, DeviceFrame, DroneBackend, DroneConfig, MonitorBackend, MonitorConfig,
    ObservedOutput, Occluder, Patch, Point, Polyline, ProjectorBackend, ProjectorConfig,
    RealizationState, RealizedSample, Runtime, Sample, SceneFrame, Surface, World, __version__, sample_scene,
)


def Projector(id: str, *, position=(0, 1, 3), target=(0, 1, 0), up=(0, 1, 0),
              fov_y=pi / 2, near=0.1, far=10, width=640, height=480,
              refresh_hz=30, latency=0) -> Backend:
    """Configure a fixed pinhole projector. Angles are radians."""
    config = ProjectorConfig()
    config.id, config.position, config.target, config.up = id, position, target, up
    config.fov_y, config.near, config.far = fov_y, near, far
    config.width, config.height = width, height
    config.refresh_hz, config.latency = refresh_hz, latency
    return ProjectorBackend(config)


def Monitor(id: str, *, surface_id: str, width=640, height=480,
            refresh_hz=30, latency=0) -> Backend:
    """Configure a raster display on a calibrated support surface."""
    config = MonitorConfig()
    config.id, config.surface_id = id, surface_id
    config.width, config.height = width, height
    config.refresh_hz, config.latency = refresh_hz, latency
    return MonitorBackend(config)


def Drones(id: str, *, count=3, speed=1.2, home=(0, 0.2, 1),
           min=(-3, 0, -3), max=(3, 3, 3), refresh_hz=30, latency=0) -> Backend:
    """Configure point emitters with speed-limited movement in metres/second."""
    config = DroneConfig()
    config.id, config.count, config.speed = id, count, speed
    config.home, config.min, config.max = home, min, max
    config.refresh_hz, config.latency = refresh_hz, latency
    return DroneBackend(config)


__all__ = [
    "Backend", "DeviceDescriptor", "DeviceFrame", "Drones", "Monitor", "Occluder", "Patch", "Point",
    "Polyline", "Projector", "RealizationState", "RealizedSample", "ObservedOutput", "Runtime", "Sample", "SceneFrame",
    "Surface", "World", "sample_scene",
]
