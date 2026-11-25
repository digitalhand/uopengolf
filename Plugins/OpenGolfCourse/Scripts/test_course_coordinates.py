# SPDX-License-Identifier: MIT
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest

from course_coordinates import GRID_FILE, source_xy, unreal_xy
from test_ground_grid import gdal, write_plane


class CourseCoordinateTests(unittest.TestCase):
    def test_looking_east_north_bunker_is_left_and_south_creek_is_right(self):
        tee = unreal_xy((10, 20))
        cup = unreal_xy((110, 20))
        self.assertGreater(cup[0], tee[0])
        self.assertEqual(cup[1], tee[1])
        self.assertLess(unreal_xy((62, 32))[1], tee[1])
        self.assertGreater(unreal_xy((83, 7))[1], tee[1])

    def test_edited_tee_and_outline_return_to_source_metres(self):
        for point, units in [((12.5, -73.123), 100), ((31.75, 9.125), 1)]:
            x, y = unreal_xy(point, units)
            edited = SimpleNamespace(x=x + 2 * units, y=y - 3 * units)
            self.assertEqual(source_xy(edited, units, True), [point[0] + 2, round(point[1] + 3, 3)])
        old_tee = SimpleNamespace(x=450, y=-690)
        self.assertEqual(source_xy(old_tee, 100, False), [4.5, -6.9])

    @unittest.skipUnless(gdal, "GDAL Python bindings are not installed")
    def test_grid_starts_at_northwest_sample_and_advances_south(self):
        from prepare_course_elevation import prepare

        with TemporaryDirectory() as folder:
            raster = Path(folder) / "ground.tif"
            output = Path(folder) / GRID_FILE
            write_plane(raster, 100, 220, 3, 2, lambda e, n: 50 + e - 100 + 2 * (n - 200))
            prepare(raster, output, (104, 210, 50))
            data = output.read_bytes()
        header = struct.Struct("<4siiddd")
        self.assertEqual(header.unpack_from(data), (b"OGE1", 3, 3, -3, -9, 2))
        self.assertEqual(struct.unpack_from("<9f", data, header.size), (39, 41, 43, 35, 37, 39, 31, 33, 35))
