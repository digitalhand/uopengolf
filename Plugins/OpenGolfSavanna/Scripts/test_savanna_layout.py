# SPDX-License-Identifier: MIT
import unittest

from savanna_layout import CANOPY_RATIO, TREE_SETTINGS, ground_mask, mask_shortfall, tree_patches


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

    def test_fresh_mask_has_no_shortfall(self):
        outlines = [[(0, 0), (20, 0), (20, 20), (0, 20)], [(30, 5), (36, 5), (33, 12)]]
        mask = ground_mask(outlines, (-40, -40), (80, 60), 120, 100)
        self.assertEqual(mask_shortfall(mask, outlines, (-40, -40), (80, 60), 120, 100), [])

    def test_stale_mask_reports_play_it_does_not_cover(self):
        # A green moved 40 m past the end of the layout the mask was baked from.
        old = [[(0, 0), (20, 0), (20, 20), (0, 20)]]
        green = [(60, 5), (70, 5), (70, 15), (60, 15)]
        mask = ground_mask(old, (-40, -40), (100, 60), 140, 100)
        self.assertEqual(mask_shortfall(mask, old + [green], (-40, -40), (100, 60), 140, 100), green)

    def test_play_outside_the_mask_bounds_is_a_shortfall(self):
        outline = [(0, 0), (20, 0), (20, 20), (0, 20)]
        mask = ground_mask([outline], (-40, -40), (60, 60), 100, 100)
        beyond = [(61, 0), (0, -41)]
        self.assertEqual(mask_shortfall(mask, [outline, beyond], (-40, -40), (60, 60), 100, 100), beyond)


if __name__ == "__main__":
    unittest.main()
