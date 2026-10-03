from typing import Sequence
from .optics import DeviceProfile, Snapshot, Diagnostic

class TransferFunction:
    LINEAR: TransferFunction
    SRGB: TransferFunction
class Calibration:
    homography: tuple[float, ...]
    point_radius: int
    transfer: TransferFunction
    lease_seconds: float
class RasterFrame:
    device_id: str
    time: float
    expires_at: float
    world_revision: int
    calibration_revision: int
    width: int
    height: int
    rgb: bytes
    diagnostics: list[Diagnostic]
def calibration_from_corners(source: Sequence[Sequence[float]], destination: Sequence[Sequence[float]], *, point_radius: int = ..., transfer: TransferFunction = ..., lease_seconds: float = ...) -> Calibration: ...
def fit_homography(source: Sequence[Sequence[float]], destination: Sequence[Sequence[float]]) -> tuple[float, ...]: ...
def compile(snapshot: Snapshot, device: DeviceProfile, calibration: Calibration = ...) -> RasterFrame: ...
def write_ppm(frame: RasterFrame, path: str) -> None: ...

class DisplayInfo:
    index: int
    name: str
    width: int
    height: int
class GlfwOutput:
    def __init__(self, width: int, height: int, title: str = ..., display_index: int = ..., library_path: str = ...) -> None: ...
    @staticmethod
    def displays(library_path: str = ...) -> list[DisplayInfo]: ...
    def present(self, frame: RasterFrame, now: float) -> bool: ...
    def poll(self, now: float) -> bool: ...
    def blackout(self) -> None: ...
    def close(self) -> None: ...
