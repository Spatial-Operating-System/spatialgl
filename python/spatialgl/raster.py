"""Python helpers for native fixed-raster compilation and presentation."""
from __future__ import annotations

import math
from typing import Sequence

from bindings.python.spatialgl import _optics_native as _optics_native
from bindings.python.spatialgl._raster_native import (
    Calibration,
    RasterFrame,
    DisplayInfo,
    GlfwOutput,
    TransferFunction,
    compile,
    fit_homography,
    write_ppm,
)


def calibration_from_corners(source: Sequence[Sequence[float]],
                             destination: Sequence[Sequence[float]], *,
                             point_radius: int = 1,
                             transfer: TransferFunction = TransferFunction.SRGB,
                             lease_seconds: float = 0.1) -> Calibration:
    """Build calibration from four normalized top-left-origin image corners.

    The native fitter computes the homography. Inputs must each contain four
    finite (x, y) points in the unit square.
    """
    def corners(name: str, values: Sequence[Sequence[float]]) -> list[tuple[float, float]]:
        if len(values) != 4:
            raise ValueError(f"{name} must contain exactly four points")
        result = []
        for point in values:
            if len(point) != 2:
                raise ValueError(f"each {name} point must contain x and y")
            x, y = float(point[0]), float(point[1])
            if not math.isfinite(x) or not math.isfinite(y) or not (0 <= x <= 1 and 0 <= y <= 1):
                raise ValueError(f"{name} coordinates must be finite and within [0, 1]")
            result.append((x, y))
        return result

    src, dst = corners("source", source), corners("destination", destination)
    if isinstance(point_radius, bool) or not isinstance(point_radius, int) or point_radius < 0:
        raise ValueError("point_radius must be a non-negative integer")
    if not math.isfinite(lease_seconds) or lease_seconds <= 0:
        raise ValueError("lease_seconds must be finite and positive")
    result = Calibration()
    result.homography = fit_homography(src, dst)
    result.point_radius = point_radius
    result.transfer = transfer
    result.lease_seconds = lease_seconds
    return result


__all__ = ["Calibration", "DisplayInfo", "GlfwOutput", "RasterFrame", "TransferFunction", "calibration_from_corners",
           "compile", "fit_homography", "write_ppm"]
