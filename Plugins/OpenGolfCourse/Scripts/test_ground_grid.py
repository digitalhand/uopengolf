# SPDX-License-Identifier: MIT
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

try:
    import numpy as np
    from osgeo import gdal, osr
except ImportError:
    gdal = None

if gdal:
    from ground_grid import GroundGrid


def write_plane(path, west, north, size, spacing, height):
    """A north-up UTM raster whose sample centres hold height(east, north)."""
    raster = gdal.GetDriverByName("GTiff").Create(str(path), size, size, 1, gdal.GDT_Float32)
    raster.SetGeoTransform((west, spacing, 0, north, 0, -spacing))
    crs = osr.SpatialReference()
    crs.ImportFromEPSG(26910)
    raster.SetProjection(crs.ExportToWkt())
    rows = [[height(west + (c + 0.5) * spacing, north - (r + 0.5) * spacing) for c in range(size)] for r in range(size)]
    raster.GetRasterBand(1).WriteArray(np.array(rows, dtype=np.float32))
    raster.FlushCache()


@unittest.skipUnless(gdal, "GDAL Python bindings are not installed")
class GroundGridTests(unittest.TestCase):
    def test_heights_and_slopes_use_local_metres(self):
        with TemporaryDirectory() as folder:
            path = Path(folder) / "ground.tif"
            write_plane(path, 1000, 2100, 100, 1.0, lambda e, n: 200 + 0.1 * (e - 1000) + 0.02 * (n - 2000))
            ground = GroundGrid.open(path, [1050, 2050, 200])
            self.assertAlmostEqual(ground.height(0, 0), 5 + 1, places=3)
            self.assertAlmostEqual(ground.height(10.25, -3.5), 5 + 1.025 + 1 - 0.07, places=3)
            self.assertAlmostEqual(ground.slope(0, 0), (0.1 ** 2 + 0.02 ** 2) ** 0.5, places=4)

    def test_points_outside_the_grid_have_no_ground(self):
        with TemporaryDirectory() as folder:
            path = Path(folder) / "ground.tif"
            write_plane(path, 0, 10, 10, 1.0, lambda e, n: 0)
            ground = GroundGrid.open(path, [0, 0, 0])
            self.assertIsNone(ground.height(50, 50))
            self.assertEqual(ground.slope(50, 50), float("inf"))

    def test_course_elevation_is_the_median_ground_inside_the_boundary(self):
        with TemporaryDirectory() as folder:
            path = Path(folder) / "ground.tif"
            # A slope rising 1 m per metre east, with the origin 250 m up at the grid's south-west corner.
            write_plane(path, 0, 100, 100, 1.0, lambda e, n: 250 + e)
            ground = GroundGrid.open(path, [0, 0, 250])
            # Local x 20-40 inside the boundary: heights 20-40 above the origin, so 280 m above sea level at the middle.
            boundary = [[20, 10], [40, 10], [40, 90], [20, 90]]
            self.assertAlmostEqual(ground.elevation_inside(boundary, [0, 0, 250], step_m=1.0), 280.0, delta=0.6)
            # Ground beyond the boundary does not count.
            self.assertAlmostEqual(ground.elevation_inside([[0, 0], [5, 0], [5, 5], [0, 5]], [0, 0, 250], step_m=1.0), 252.5, delta=0.6)

    def test_a_boundary_off_the_grid_has_no_elevation(self):
        with TemporaryDirectory() as folder:
            path = Path(folder) / "ground.tif"
            write_plane(path, 0, 10, 10, 1.0, lambda e, n: 0)
            ground = GroundGrid.open(path, [0, 0, 0])
            self.assertIsNone(ground.elevation_inside([[500, 500], [600, 500], [600, 600]], [0, 0, 0]))


if __name__ == "__main__":
    unittest.main()
