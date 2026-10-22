# SPDX-License-Identifier: MIT
import math
import unittest

from understory_layout import CANOPY_RADIUS_M, CATEGORIES, PLAY_CLEARANCE_M, understory_points


def by_category(points):
    result = {}
    for point in points:
        result.setdefault(point[0], []).append(point)
    return result


class UnderstoryTests(unittest.TestCase):
    def test_same_trees_give_the_same_cover(self):
        trees = [(10.0, 40.0, 0.7), (-30.0, 120.0, 1.0)]
        self.assertEqual(understory_points(trees, []), understory_points(trees, []))

    def test_one_tree_does_not_reshuffle_another(self):
        alone = understory_points([(0.0, 0.0, 1.0)], [])
        together = understory_points([(0.0, 0.0, 1.0), (500.0, 500.0, 1.0)], [])
        self.assertEqual(alone, [p for p in together if math.hypot(p[2], p[3]) < 100])

    def test_each_category_stays_in_its_ring(self):
        radius = CANOPY_RADIUS_M
        for name, points in by_category(understory_points([(0.0, 0.0, 1.0)], [])).items():
            inner, outer = CATEGORIES[name]["ring"]
            # Tuft members may sit up to the cluster radius beyond their ring.
            slack = CATEGORIES[name].get("cluster", (1, 0.0))[1]
            for _, variant, x, y, yaw, scale, lift in points:
                distance = math.hypot(x, y)
                self.assertGreaterEqual(distance, inner * radius - slack - 1e-9, name)
                self.assertLessEqual(distance, outer * radius + slack + 1e-9, name)
                low, high = CATEGORIES[name]["scale"]
                self.assertTrue(low <= scale <= high, name)
                self.assertTrue(0 <= variant < len(CATEGORIES[name]["meshes"]), name)
                self.assertTrue(0 <= yaw < 360)
                self.assertTrue(0 <= lift <= 1.0)

    def test_tufts_group_their_members(self):
        points = by_category(understory_points([(0.0, 0.0, 1.0)], []))
        for name, settings in CATEGORIES.items():
            size, spread = settings.get("cluster", (1, 0.0))
            if size == 1:
                continue
            members = points[name]
            self.assertEqual(len(members) % size, 0, name)
            for start in range(0, len(members), size):
                group = members[start:start + size]
                cx, cy = group[0][2], group[0][3]
                self.assertTrue(all(math.hypot(p[2] - cx, p[3] - cy) <= spread + 1e-9 for p in group), name)

    def test_full_size_tree_gets_every_category_at_its_count(self):
        counts = {name: len(points) for name, points in by_category(understory_points([(0.0, 0.0, 1.0)], [])).items()}
        for name, settings in CATEGORIES.items():
            if float(settings["count"]).is_integer():
                self.assertEqual(counts.get(name, 0), settings["count"], name)

    def test_smaller_trees_get_less_cover(self):
        big = len(understory_points([(0.0, 0.0, 1.0)], []))
        small = len(understory_points([(0.0, 0.0, 0.5)], []))
        self.assertLess(small, big * 0.4)
        self.assertGreater(small, 0)

    def test_cover_keeps_off_play_surfaces(self):
        fairway = [(4.0, -50.0), (60.0, -50.0), (60.0, 50.0), (4.0, 50.0)]
        points = understory_points([(0.0, 0.0, 1.0)], [fairway])
        unrestricted = understory_points([(0.0, 0.0, 1.0)], [])
        self.assertLess(len(points), len(unrestricted))
        self.assertTrue(all(p[2] <= 4.0 - PLAY_CLEARANCE_M + 1e-9 for p in points))
        self.assertTrue(points)

    def test_litter_is_denser_near_the_trunk(self):
        points = [p for t in range(40) for p in understory_points([(t * 100.0, 0.0, 1.0)], [])
                  if p[0] == "litter"]
        inner, outer = CATEGORIES["litter"]["ring"]
        middle = CANOPY_RADIUS_M * (inner + outer) / 2
        near = sum(1 for p in points if math.hypot(p[2] - round(p[2] / 100) * 100, p[3]) < middle)
        # An even spread over the disc would put a quarter of the litter inside half the radius.
        self.assertGreater(near / len(points), 0.4)


if __name__ == "__main__":
    unittest.main()
