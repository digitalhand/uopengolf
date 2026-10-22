# SPDX-License-Identifier: MIT
import unittest

from savanna_layout import CANOPY_RATIO, TREE_SETTINGS, ground_mask, relocate_bunker, tree_patches


class LayoutTests(unittest.TestCase):
    def test_patches_repeat_and_have_gaps(self):
        patches = tree_patches()
        self.assertEqual(patches, tree_patches())
        self.assertEqual(len(patches), 36)
        self.assertEqual(len({p[0] for p in patches}), len(patches))
        near = [p for p in patches if p[1] == "Near"]
        self.assertEqual(len(near), 12)
        self.assertTrue(all(15 < abs(y) < 55 for p in near for x, y in p[2]))

    def test_prefix_only_renames_patches(self):
        public, private = tree_patches(), tree_patches("DesertOak")
        self.assertEqual([p[1:] for p in public], [p[1:] for p in private])
        self.assertTrue(all(p[0].startswith("DesertOak") for p in private))

    def test_tree_scales_match_the_private_baseline(self):
        self.assertAlmostEqual(TREE_SETTINGS["Near"]["scale_range"][0], 0.29524871668, places=6)
        self.assertAlmostEqual(TREE_SETTINGS["Backdrop"]["scale_range"][1], 0.61733822579, places=6)

    def test_mask_matches_ground_transition(self):
        outline = [(0, 0), (20, 0), (20, 20), (0, 20)]
        mask = ground_mask([outline], (-40, -40), (60, 60), 100, 100)
        self.assertEqual(mask[50 * 100 + 62], 255)
        # Solid rough under the whole 12 m grass band.
        self.assertEqual(mask[50 * 100 + 71], 255)
        self.assertEqual(mask[50 * 100 + 94], 0)
        self.assertTrue(70 < mask[50 * 100 + 74] < 180)  # middle of the 12-16 m fade
        self.assertEqual(mask, ground_mask([list(reversed(outline))], (-40, -40), (60, 60), 100, 100))

    def test_empty_mask_is_native_ground(self):
        self.assertEqual(ground_mask([], (-10, -10), (10, 10), 8, 8), bytes(64))

    def test_bunker_moves_before_green_with_its_contours(self):
        outline = [(138, 23), (154, 23), (154, 33), (138, 33)]
        moved, centers = relocate_bunker(outline, [(146, 28), (146, 28), (126, 5)])
        self.assertEqual(moved, [(100, -5), (116, -5), (116, 5), (100, 5)])
        self.assertTrue(all(x < 122.9 for x, y in moved))
        self.assertEqual(centers, [(108, 0), (108, 0), (126, 5)])

    def test_bunker_relocation_is_repeatable(self):
        outline = [(100, -5), (116, -5), (116, 5), (100, 5)]
        self.assertEqual(relocate_bunker(outline, [(108, 0), (126, 5)]), (outline, [(108, 0), (126, 5)]))


if __name__ == "__main__":
    unittest.main()
