# SPDX-License-Identifier: MIT
"""Prepare a projected, north-up DEM with metre pixels and heights for Unreal."""
import argparse
import os
import struct
import tempfile
from pathlib import Path

import numpy as np
from osgeo import gdal


def prepare(source, destination, origin):
    gdal.UseExceptions()
    raster = gdal.Open(str(source))
    crs = raster.GetSpatialRef()
    if not crs or not crs.IsProjected() or not np.isclose(crs.GetLinearUnits(), 1):
        raise ValueError("Reproject the source to a local projected CRS in metres first.")
    west, spacing, shear_x, north, shear_y, step_y = raster.GetGeoTransform()
    if shear_x or shear_y or spacing <= 0 or not np.isclose(-step_y, spacing):
        raise ValueError("Prepare a north-up raster with square metre-based pixels.")
    width, height = raster.RasterXSize, raster.RasterYSize
    if min(width, height) < 2 or width * height > 16000000:
        raise ValueError("Crop the input to a grid with 2 or more rows and columns and at most 16 million samples.")
    band = raster.GetRasterBand(1)
    if band.GetUnitType().lower() not in ("", "m", "meter", "metre", "meters", "metres"):
        raise ValueError("Convert elevation units to metres first.")
    samples = band.ReadAsArray().astype(np.float64)
    if not band.GetMaskBand().ReadAsArray().all() or not np.isfinite(samples).all():
        raise ValueError("The course extent contains missing elevation data.")
    scale = band.GetScale()
    samples = samples * (1 if scale is None else scale) + (band.GetOffset() or 0) - origin[2]
    if samples.min() < -256 or samples.max() > 255.99:
        raise ValueError("Choose a vertical origin that fits the Landscape height range.")
    first_x = west + spacing / 2 - origin[0]
    first_y = north - (height - 0.5) * spacing - origin[1]
    with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as stream:
        temp = Path(stream.name)
        try:
            stream.write(struct.pack("<4siiddd", b"OGE1", width, height, first_x, first_y, spacing))
            stream.write(samples[::-1].astype("<f4").tobytes())
            stream.close()
            os.replace(temp, destination)
        finally:
            temp.unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--origin", type=float, nargs=3, required=True, metavar=("EAST", "NORTH", "HEIGHT"))
    args = parser.parse_args()
    prepare(args.source, args.destination, args.origin)
