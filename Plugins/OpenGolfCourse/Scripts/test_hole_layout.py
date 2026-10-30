# SPDX-License-Identifier: MIT
import math
import unittest

import hole_layout as hole

SQUARE = [(0, 0), (10, 0), (10, 10), (0, 10)]


class HoleLayoutTests(unittest.TestCase):
    def test_outline_moves(self):
        self.assertEqual(hole.center(SQUARE), (5, 5))
        self.assertEqual(hole.translate(SQUARE, 2, -1)[2], (12, 9))
        self.assertEqual(hole.scale_about(SQUARE, 0.5, (5, 5))[0], (2.5, 2.5))
        self.assertTrue(hole.contains(SQUARE, (5, 5)))
        self.assertFalse(hole.contains(SQUARE, (11, 5)))

    def test_feature_mounds_follow_the_feature(self):
        mounds = [(5, 5, 4, 3, 0.5), (5, 5, 2, 1, -1.0), (50, 0, 10, 10, 2.0)]
        self.assertEqual(hole.mounds_at(mounds, (5, 5)), mounds[:2])
        moved = hole.move_mounds(mounds, (5, 5), (20, 20))
        self.assertEqual(moved[0][:2], (20, 20))
        self.assertEqual(moved[2], mounds[2])
        outline, copied = hole.copy_feature(SQUARE, mounds, (5, 5), (100, 0), 0.5)
        self.assertEqual(hole.bounds(outline), (97.5, -2.5, 102.5, 2.5))
        self.assertEqual(copied, [(100, 0, 2.0, 1.5, 0.25), (100, 0, 1.0, 0.5, -0.5)])

    def test_height_matches_the_terrain_sum(self):
        mound = [(0, 0, 10, 5, 2.0)]
        self.assertAlmostEqual(hole.height_at(mound, 0, 0), 2.0)
        self.assertAlmostEqual(hole.height_at(mound, 10, 0), 2.0 * math.exp(-2.0))
        self.assertEqual(hole.height_at([(0, 0, 0, 5, 2.0)], 0, 0), 0.0)

    def test_slope_of_a_gaussian(self):
        # Steepest at one half radius: 2 h / r * exp(-0.5) for a round mound.
        mound = [(0, 0, 10, 10, 1.0)]
        self.assertAlmostEqual(hole.slope_at(mound, 5, 0), 2 / 10 * math.exp(-0.5), places=4)
        self.assertAlmostEqual(hole.slope_at(mound, 0, 0), 0.0, places=6)
        grade, at = hole.max_slope_inside(mound, [(-20, -20), (20, -20), (20, 20), (-20, 20)], 0.5)
        self.assertAlmostEqual(grade, 2 / 10 * math.exp(-0.5), places=3)
        self.assertAlmostEqual(math.hypot(*at), 5.0, delta=0.4)
        self.assertLess(hole.max_slope_near(mound, (0, 0), 1.0), 0.05)

    def test_outline_distance(self):
        self.assertAlmostEqual(hole.outline_distance(SQUARE, hole.translate(SQUARE, 13, 0)), 3.0)
        self.assertEqual(hole.outline_distance(SQUARE, hole.translate(SQUARE, 5, 0)), 0.0)


if __name__ == '__main__':
    unittest.main()
