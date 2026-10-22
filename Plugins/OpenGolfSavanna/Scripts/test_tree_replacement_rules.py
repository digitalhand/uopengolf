# SPDX-License-Identifier: MIT
import unittest

from tree_replacement_rules import replacement_scale, validate_pairs


class TreeReplacementTests(unittest.TestCase):
    def test_scale_changes_only_for_original_mesh(self):
        self.assertEqual(replacement_scale('old', 'old', 'new', 0.5), 0.5)
        self.assertEqual(replacement_scale('new', 'old', 'new', 0.5), 1.0)
        with self.assertRaises(ValueError):
            replacement_scale('other', 'old', 'new', 0.5)

    def test_rejects_bad_ratio(self):
        for ratio in (0, -1, float('nan'), float('inf')):
            with self.assertRaises(ValueError):
                replacement_scale('old', 'old', 'new', ratio)

    def test_pairs_require_one_collider_per_tree(self):
        validate_pairs([(1, 2), (3, 4)], [(3, 4), (1, 2)])
        for colliders in ([], [(1, 2)], [(1, 2), (1, 2)], [(1, 2), (5, 6)]):
            with self.assertRaises(ValueError):
                validate_pairs([(1, 2), (3, 4)], colliders)
        with self.assertRaises(ValueError):
            validate_pairs([], [])


if __name__ == '__main__':
    unittest.main()
