# SPDX-License-Identifier: MIT
import math
import struct
import unittest

import savanna_hole as sh
from outline_smoothing import crossing_outlines
from savanna_hole import hole
from savanna_layout import tree_patches


def float32(value):
    """As UOpenGolfTerrainDefinition stores it."""
    return struct.unpack("f", struct.pack("f", value))[0]


def ellipse(cx, cy, rx, ry, count=64):
    return [(cx + rx * math.cos(math.tau * i / count), cy + ry * math.sin(math.tau * i / count)) for i in range(count)]


# The range before the hole: the authored fairway, the 150-yard green and fringe, and the approach bunker with its lip
# and hollow, as saved in DA_SavannaRange on 2026-10-29.
FAIRWAY = [(-12, -13), (25, -16), (75, -14), (120, -17), (165, -17), (220, -14), (285, -16), (335, -11), (350, 0),
           (335, 12), (280, 15), (220, 17), (160, 16), (110, 15), (60, 17), (10, 14), (-12, 12)]
RANGE = {
    "Fairway": FAIRWAY,
    "Fringe": ellipse(137.16, -0.84, 16.3, 12.0),
    "Green": ellipse(137.16, -0.7, 14.25, 10.0),
    "Bunker": ellipse(108.0, 0.0, 8.3, 5.0),
}
MOUNDS = [(310.0, -65.0, 55.0, 34.0, 5.0), (285.0, 65.0, 60.0, 32.0, 5.0), (385.0, 5.0, 48.0, 65.0, 5.0),
          (160.0, -5.0, 48.0, 40.0, 0.45), (126.0, 5.0, 22.0, 18.0, 0.23), (108.0, 0.0, 12.0, 9.0, 0.55), (108.0, 0.0, 7.5, 4.5, -1.1)]
# UOpenGolfTerrainDefinition::DetailMaximumM: the detailed ground ends here.
DETAIL_MAX = (504.0, 126.0)


class SavannaHoleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.regions, cls.mounds = sh.apply_layout(RANGE, MOUNDS)

    def test_green_is_400_yards_out_with_the_pin_on_it(self):
        self.assertAlmostEqual(sh.PIN[0] * 1.0936133, 400.0, places=2)
        self.assertEqual(hole.center(self.regions["Green"]), sh.GREEN_CENTER)
        self.assertTrue(hole.contains(self.regions["Green"], sh.PIN))
        self.assertTrue(all(hole.contains(self.regions["Fringe"], p) for p in self.regions["Green"][::8]))
        x0, y0, x1, y1 = hole.bounds(self.regions["Fringe"])
        self.assertLess(x1, DETAIL_MAX[0] - 20)

    def test_green_contour_is_gentle(self):
        grade, at = hole.max_slope_inside(self.mounds, self.regions["Green"], 0.5)
        self.assertLessEqual(grade, sh.MAX_GREEN_GRADE, at)
        self.assertGreater(grade, 0.01, "The green should have readable break")
        self.assertLessEqual(hole.max_slope_near(self.mounds, sh.PIN, sh.PIN_FLAT_RADIUS_M), sh.MAX_PIN_GRADE)

    def test_fairway_bunker_sits_left_of_the_landing_area(self):
        bunker = self.regions["Bunker"]
        x0, y0, x1, y1 = hole.bounds(bunker)
        self.assertLess(y1, 0, "left of the centre line")
        gap = hole.outline_distance(bunker, self.regions["Fairway"])
        self.assertTrue(1.0 < gap < 4.0, f"just off the fairway's left edge ({gap:.1f} m)")
        self.assertTrue(200 < x0 and x1 < 280, "in the drive landing area")
        self.assertEqual(hole.mounds_at(self.mounds, sh.FAIRWAY_BUNKER), [(240.0, -22.0, 12.0, 9.0, 0.55), (240.0, -22.0, 7.5, 4.5, -1.1)])

    def test_drives_down_the_middle_roll_on_level_ground(self):
        # Range tests compare drives with flat-ground predictions; the bunker's lip stays out of the middle 18 m.
        features = hole.mounds_at(self.mounds, sh.FAIRWAY_BUNKER)
        self.assertLess(max(hole.height_at(features, x, y) for x in range(200, 300, 2) for y in range(-9, 10)), 0.01)

    def test_outlines_nest_or_stay_apart(self):
        # The terrain builder refuses crossing region edges.
        self.assertEqual(crossing_outlines(list(self.regions.values())), [])
        x0 = hole.bounds(self.regions["Fringe"])[0]
        self.assertAlmostEqual(hole.bounds(self.regions["Fairway"])[2], x0 - sh.FAIRWAY_GAP_M)

    def test_greenside_bunker_is_right_of_the_green(self):
        bunker = self.regions[sh.GREENSIDE_BUNKER_NAME]
        x0, y0, x1, y1 = hole.bounds(bunker)
        gx0, gy0, gx1, gy1 = hole.bounds(self.regions["Green"])
        self.assertGreater(y0, gy1, "right of the green")
        self.assertTrue(gx0 < (x0 + x1) / 2 < gx1, "beside it, not short or long")
        self.assertGreaterEqual(hole.outline_distance(bunker, self.regions["Fringe"]), sh.MIN_BUNKER_CLEARANCE_M)
        self.assertEqual(len(hole.mounds_at(self.mounds, sh.GREENSIDE_BUNKER)), 2)

    def test_backdrop_hill_moved_clear_of_the_green(self):
        self.assertFalse(hole.mounds_at(self.mounds, sh.BACKDROP_HILL[0]))
        self.assertEqual(len(hole.mounds_at(self.mounds, sh.BACKDROP_HILL[1])), 1)
        # The detailed ground meets the unchanged Landscape at the box edge; the hill must not lift it there.
        hill = hole.mounds_at(self.mounds, sh.BACKDROP_HILL[1])
        self.assertLess(max(hole.height_at(hill, DETAIL_MAX[0], y) for y in range(-126, 127, 6)), 0.01)

    def test_layout_reapplies_unchanged(self):
        regions, mounds = sh.apply_layout(self.regions, self.mounds)
        for name, outline in regions.items():
            for a, b in zip(outline, self.regions[name]):
                self.assertAlmostEqual(math.dist(a, b), 0.0, places=9)
        self.assertEqual(sorted(mounds), sorted(self.mounds))

    def test_layout_reapplies_after_float32_storage(self):
        stored = ({name: [(float32(x), float32(y)) for x, y in outline] for name, outline in self.regions.items()},
                  [tuple(float32(v) for v in m) for m in self.mounds])
        regions, mounds = sh.apply_layout(*stored)
        self.assertEqual(len(mounds), len(self.mounds), "no mound is added twice")
        for name, outline in regions.items():
            for a, b in zip(outline, self.regions[name]):
                self.assertLess(math.dist(a, b), 1e-3)

    def test_trees_stay_off_the_green(self):
        # Trees keep 8 m from every play outline (TREE_SETTINGS); no patch reaches the green complex at all.
        for name, kind, outline, _ in tree_patches():
            if kind == "Near":
                for feature in (self.regions["Fringe"], self.regions[sh.GREENSIDE_BUNKER_NAME]):
                    self.assertGreater(hole.outline_distance(outline, feature), 0.0, name)


if __name__ == "__main__":
    unittest.main()
