"""Typed Python authoring API for the C++ surface optics runtime."""

from bindings.python.spatialgl._optics_native import (
    Availability, Composition, Contribution, CoordinateSpace, DeviceKind, DeviceProfile,
    Diagnostic, DisplayList, DrawCall, EventKind, GeometryKind, OpticalRuntime, OutputMix,
    PipelineState, PlanarMotion, PlanStatus, Plane, Presentation, PresentationState,
    Occluder, ProgramEvent, Provenance, Receipt, Resource, Rig, RuntimeOptions, Snapshot,
    SurfaceTarget, TargetLight, TimedProgram, Traversal, TraversalState, WorldSnapshot,
    validate_display, validate_world,
)


def fixed_raster(device_id, *, position=(0, 1, 3), target=(0, 1, 0),
                 fov_y=1.5707963267948966, width=640, height=480, latency=0.0,
                 sample_rate_hz=60.0, resource_ids=()):
    """Create a fixed pinhole raster profile."""
    device = DeviceProfile()
    device.id, device.kind = device_id, DeviceKind.FIXED_RASTER
    device.position, device.target = position, target
    device.fov_y, device.width, device.height = fov_y, width, height
    device.latency, device.sample_rate_hz = latency, sample_rate_hz
    device.resource_ids = list(resource_ids)
    return device


def steerable_raster(device_id, **kwargs):
    angular_speed = kwargs.pop("max_angular_speed", 1.0)
    settling_time = kwargs.pop("settling_time", 0.0)
    device = fixed_raster(device_id, **kwargs)
    device.kind = DeviceKind.STEERABLE_RASTER
    device.max_angular_speed, device.settling_time = angular_speed, settling_time
    return device


def galvo(device_id, **kwargs):
    kwargs.setdefault("sample_rate_hz", 30000.0)
    device = fixed_raster(device_id, **kwargs)
    device.kind = DeviceKind.GALVO
    return device


def world(planes, *, source_time=0.0, valid_until=100.0, world_revision=1,
          calibration_revision=1, motion=(), occluders=()):
    result = WorldSnapshot()
    result.planes = list(planes)
    result.occluders = list(occluders)
    result.source_time, result.valid_until = source_time, valid_until
    result.world_revision, result.calibration_revision = world_revision, calibration_revision
    result.motion = list(motion)
    return result


def rig(devices, resources=()):
    result = Rig()
    result.devices, result.resources = list(devices), list(resources)
    return result


def plane(surface_id, origin=(-2, 0, 0), u=(4, 0, 0), v=(0, 3, 0)):
    result = Plane()
    result.id, result.origin, result.u, result.v = surface_id, origin, u, v
    return result


def occluder(occluder_id, minimum, maximum):
    result = Occluder()
    result.id, result.min, result.max = occluder_id, minimum, maximum
    return result


def draw(draw_id, surface_id, position=(0, 1, 0), *, geometry=GeometryKind.POINT,
         vertices=(), color=(1, 1, 1), intensity=1.0, priority=0, opacity=1.0):
    result = DrawCall()
    result.draw_id, result.linear_rgb, result.intensity, result.priority = draw_id, color, intensity, priority
    result.opacity = opacity
    result.target.surface_id = surface_id
    result.target.geometry, result.target.origin = geometry, position
    result.target.vertices = list(vertices)
    return result


def display(display_id, draws, *, present_at=0.0, expires_at=10.0,
            app_id="python", composition=Composition.OVER):
    result = DisplayList()
    result.id, result.app_id = display_id, app_id
    result.present_at, result.expires_at = present_at, expires_at
    result.composition, result.draws = composition, list(draws)
    return result


__all__ = [name for name in globals() if not name.startswith("_")]
