# SPDX-License-Identifier: MIT
"""Read a prepared north-up elevation raster in a course's local metres."""
import math

import numpy as np
from osgeo import gdal


class GroundGrid:
    """Bilinear heights (relative to the course origin) and slopes at local (x east, y north) metres."""

    def __init__(self, samples, first_x, first_y, spacing):
        self.samples = samples  # Rows run north to south, as stored in the raster.
        self.first_x = first_x  # Local position of the north-west sample centre.
        self.first_y = first_y
        self.spacing = spacing

    @classmethod
    def open(cls, path, origin):
        gdal.UseExceptions()
        raster = gdal.Open(str(path))
        west, spacing, _, north, _, step_y = raster.GetGeoTransform()
        if not math.isclose(-step_y, spacing):
            raise ValueError("Expected a north-up raster with square pixels")
        band = raster.GetRasterBand(1)
        samples = band.ReadAsArray().astype(np.float64)
        nodata = band.GetNoDataValue()
        if nodata is not None:
            samples[samples == nodata] = np.nan
        return cls(samples - origin[2], west + spacing / 2 - origin[0], north - spacing / 2 - origin[1], spacing)

    def height(self, x, y):
        """Height in metres, or None outside the grid or over missing samples."""
        rows, cols = self.samples.shape
        gx, gy = (x - self.first_x) / self.spacing, (self.first_y - y) / self.spacing
        if not (0 <= gx <= cols - 1 and 0 <= gy <= rows - 1):
            return None
        c, r = min(int(gx), cols - 2), min(int(gy), rows - 2)
        fx, fy = gx - c, gy - r
        top = self.samples[r, c] * (1 - fx) + self.samples[r, c + 1] * fx
        bottom = self.samples[r + 1, c] * (1 - fx) + self.samples[r + 1, c + 1] * fx
        value = top * (1 - fy) + bottom * fy
        return None if math.isnan(value) else float(value)

    def slope(self, x, y):
        """Rise over run across one sample spacing; infinite where there is no ground."""
        h = self.spacing / 2
        east, west, north, south = self.height(x + h, y), self.height(x - h, y), self.height(x, y + h), self.height(x, y - h)
        if None in (east, west, north, south):
            return float("inf")
        return math.hypot((east - west) / (2 * h), (north - south) / (2 * h))

    def elevation_inside(self, outline, origin, step_m=5.0):
        """Metres above sea level: the median ground inside outline (local metres), sampled every step_m.

        origin is the course origin [east, north, height] the grid was opened with. None when no ground falls inside.
        """
        stride = max(1, round(step_m / self.spacing))
        rows, cols = self.samples.shape
        row_index, col_index = np.mgrid[0:rows:stride, 0:cols:stride]
        x = self.first_x + col_index * self.spacing
        y = self.first_y - row_index * self.spacing
        inside = np.zeros(x.shape, dtype=bool)
        # Even-odd ray casting against every edge of the outline.
        for (x0, y0), (x1, y1) in zip(outline, outline[1:] + outline[:1]):
            if y0 == y1:
                continue
            crosses = (y0 > y) != (y1 > y)
            inside ^= crosses & (x < x0 + (y - y0) * (x1 - x0) / (y1 - y0))
        heights = self.samples[row_index, col_index][inside]
        heights = heights[~np.isnan(heights)]
        return round(float(np.median(heights)) + origin[2], 1) if heights.size else None

