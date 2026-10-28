# SPDX-License-Identifier: MIT
import struct
import tempfile
import unittest
from pathlib import Path

import numpy as np
from turf_mask import CHANNELS, decode, field_bounds, field_channels, polygon_fields, write_field

SPEC = {'default_surface': 'ROUGH', 'regions': [
    {'surface': 'FAIRWAY', 'outline': [(0, 0), (10, 0), (10, 4), (0, 4)]},
    {'surface': 'FRINGE', 'outline': [(6, 1), (9, 1), (9, 3), (6, 3)]},
    {'surface': 'GREEN', 'outline': [(6.5, 1.5), (8.5, 1.5), (8.5, 2.5), (6.5, 2.5)]},
    {'surface': 'SAND', 'outline': [(2, 6), (4, 6), (4, 8), (2, 8)]}]}


def sample(spec, data, bounds, point):
    """Nearest texel's decoded distances at a point in metres."""
    (x0, y0), (x1, y1) = bounds
    column = int((point[0] - x0) / (x1 - x0) * data.shape[1])
    row = int((point[1] - y0) / (y1 - y0) * data.shape[0])
    return decode(data[row, column])


class PolygonFieldTest(unittest.TestCase):
    def test_concave_polygon_and_edges(self):
        polygon = [(0, 0), (3, 0), (3, 1), (1, 1), (1, 3), (0, 3)]
        inside, distance = polygon_fields(polygon, np.array([0, .5, 2]), np.array([1, 2, 2]))
        self.assertEqual(inside.tolist(), [True, True, False])
        self.assertAlmostEqual(distance[-1], 1)


class CourseFieldTest(unittest.TestCase):
    def setUp(self):
        self.bounds = field_bounds(SPEC, margin=2)
        self.data = field_channels(SPEC, self.bounds, 400, 240)

    def test_bounds_cover_every_play_region_including_sand(self):
        (x0, y0), (x1, y1) = self.bounds
        self.assertEqual((x0, y0, x1, y1), (-2, -2, 12, 10))

    def test_signed_distances_decode_inside_and_out(self):
        expected = {(5, 2): {'FAIRWAY': -2}, (5, 5): {'FAIRWAY': 1}, (7.5, 2): {'GREEN': -.5, 'FRINGE': -1},
                    (3, 7): {'SAND': -1}, (3, 5.5): {'SAND': .5}}
        for point, distances in expected.items():
            decoded = sample(SPEC, self.data, self.bounds, point)
            for surface, distance in distances.items():
                # Nearest-texel lookup and 8-bit steps: within a texel plus one step.
                self.assertAlmostEqual(decoded[surface], distance, delta=.06, msg=(point, surface))

    def test_far_points_clamp_to_each_channel_range(self):
        decoded = sample(SPEC, self.data, self.bounds, (11.9, 9.9))
        for surface, limit in CHANNELS:
            self.assertAlmostEqual(decoded[surface], limit, delta=.02)

    def test_edges_cross_zero_between_neighbouring_texels(self):
        row = self.data[80, :, 0].astype(int)  # y = 2.025 m, across the fairway
        signs = np.sign(row - 127.5)
        self.assertEqual(int(np.count_nonzero(np.diff(signs))), 2)

    def test_repeatable(self):
        self.assertTrue(np.array_equal(self.data, field_channels(SPEC, self.bounds, 400, 240)))

    def test_rejects_regions_that_break_layer_order(self):
        reordered = {'default_surface': 'ROUGH', 'regions': [SPEC['regions'][2], SPEC['regions'][0]]}
        with self.assertRaises(ValueError):
            field_channels(reordered, field_bounds(reordered), 64, 64)

    def test_writes_a_top_left_bgra_targa_and_its_decode_sidecar(self):
        with tempfile.TemporaryDirectory() as folder:
            target = Path(folder) / 'field.tga'
            info = write_field(SPEC, target, 64, 32)
            header = target.read_bytes()[:18]
            self.assertEqual(struct.unpack('<HHB', header[12:17]), (64, 32, 32))
            self.assertEqual(header[17], 0x28)
            self.assertEqual(len(target.read_bytes()), 18 + 64 * 32 * 4)
            self.assertEqual([c[0] for c in info['channels']], [c[0] for c in CHANNELS])
            self.assertEqual(info, __import__('json').loads(target.with_suffix('.json').read_text()))


if __name__ == '__main__':
    unittest.main()
