from typing import Any, Literal, Sequence, TypeAlias

Vec3: TypeAlias = Sequence[float]
RGB: TypeAlias = Sequence[float]

class Point:
    id: str
    position: list[float]
    color: list[float]
    surface_id: str
    def __init__(self, id: str, position: Vec3, color: RGB = ..., surface_id: str = ...) -> None: ...
class Polyline:
    id: str
    vertices: list[list[float]]
    color: list[float]
    surface_id: str
    def __init__(self, id: str, vertices: Sequence[Vec3], color: RGB = ..., surface_id: str = ...) -> None: ...
class Patch:
    id: str
    origin: list[float]
    u: list[float]
    v: list[float]
    color: list[float]
    surface_id: str
    def __init__(self, id: str, origin: Vec3, u: Vec3, v: Vec3, color: RGB = ..., surface_id: str = ...) -> None: ...
class Surface:
    id: str
    origin: list[float]
    u: list[float]
    v: list[float]
    def __init__(self, id: str, origin: Vec3, u: Vec3, v: Vec3) -> None: ...
class Occluder:
    id: str
    min: list[float]
    max: list[float]
    def __init__(self, id: str, min: Vec3, max: Vec3) -> None: ...
class World:
    surfaces: list[Surface]
    occluders: list[Occluder]
    def __init__(self, surfaces: Sequence[Surface] = ..., occluders: Sequence[Occluder] = ...) -> None: ...
Primitive: TypeAlias = Point | Polyline | Patch
class SceneFrame:
    id: str
    present_at: float
    expires_at: float
    primitives: list[Primitive]
    def __init__(self, id: str, present_at: float, expires_at: float, primitives: Sequence[Primitive]) -> None: ...
class DeviceDescriptor:
    id: str
    modality: str
    refresh_hz: float
    latency: float
    capacity: int
class Backend:
    @property
    def descriptor(self) -> DeviceDescriptor: ...
def Projector(id: str, *, position: Vec3 = ..., target: Vec3 = ..., up: Vec3 = ..., fov_y: float = ..., near: float = ..., far: float = ..., width: int = ..., height: int = ..., refresh_hz: float = ..., latency: float = ...) -> Backend: ...
def Monitor(id: str, *, surface_id: str, width: int = ..., height: int = ..., refresh_hz: float = ..., latency: float = ...) -> Backend: ...
def Drones(id: str, *, count: int = ..., speed: float = ..., home: Vec3 = ..., min: Vec3 = ..., max: Vec3 = ..., refresh_hz: float = ..., latency: float = ...) -> Backend: ...
class Sample:
    id: str
    primitive_id: str
    kind: Literal['point', 'polyline', 'patch']
    position: list[float]
    color: list[float]
    surface_id: str
class RealizedSample:
    sample: Sample
    device_id: str | None
    actual: list[float] | None
    error: float | None
    status: Literal['realized', 'tracking', 'pending', 'unrealizable']
    source_scene_id: str | None
    age: float | None
    reasons: list[str]
class DeviceFrame:
    device_id: str
    scene_id: str
    issued_at: float
    apply_at: float
    expires_at: float
    def to_dict(self) -> dict[str, Any]: ...
class ObservedOutput:
    sample_id: str
    position: list[float]
    color: list[float]
    device_id: str
    source_scene_id: str | None
    age: float
    stale: bool
class RealizationState:
    time: float
    scene_id: str | None
    samples: list[RealizedSample]
    device_frames: list[DeviceFrame]
    outputs: list[ObservedOutput]
    def to_dict(self) -> dict[str, Any]: ...
class Runtime:
    def __init__(self, world: World, backends: Sequence[Backend], spacing: float = ..., tolerance: float = ...) -> None: ...
    def submit(self, frame: SceneFrame) -> None: ...
    def advance(self, to: float) -> RealizationState: ...
    def snapshot(self) -> RealizationState: ...
def sample_scene(primitives: Sequence[Primitive], spacing: float = ...) -> list[Sample]: ...
__version__: str
