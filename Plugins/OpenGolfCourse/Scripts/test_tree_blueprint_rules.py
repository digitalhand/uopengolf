# SPDX-License-Identifier: MIT
import unittest

from tree_blueprint_rules import CANOPY_TAG, OBSTACLE_TAG, components, parse_args


class ComponentTests(unittest.TestCase):
    def setUp(self):
        self.parts = {part["name"]: part for part in components("/T/SM_Tree", "/T/SM_TreeWood", "/T/SM_TreeCanopy", 15000)}

    def test_the_visible_tree_never_touches_the_ball(self):
        tree = self.parts["Tree"]
        self.assertEqual((tree["mesh"], tree["profile"], tree["visible"], tree["cast_shadow"]), ("/T/SM_Tree", "NoCollision", True, True))
        self.assertEqual(tree["tags"], [])
        self.assertTrue(tree["evaluate_wpo"])
        self.assertEqual(tree["wpo_disable_distance"], 15000)

    def test_the_wood_blocks_as_a_hidden_obstacle(self):
        wood = self.parts["Wood"]
        self.assertEqual((wood["mesh"], wood["profile"], wood["visible"], wood["hidden_in_game"]), ("/T/SM_TreeWood", "BlockAll", False, True))
        self.assertEqual(wood["tags"], [OBSTACLE_TAG])
        self.assertFalse(wood["cast_shadow"])

    def test_the_canopy_only_overlaps_as_hidden_foliage(self):
        canopy = self.parts["Canopy"]
        self.assertEqual((canopy["mesh"], canopy["profile"], canopy["visible"]), ("/T/SM_TreeCanopy", "OverlapAll", False))
        self.assertEqual(canopy["tags"], [CANOPY_TAG])
        self.assertFalse(canopy["overlap_events"])

    def test_tags_are_the_ball_physics_names(self):
        self.assertEqual((OBSTACLE_TAG, CANOPY_TAG), ("OpenFairway.Obstacle", "OpenFairway.Canopy"))

    def test_every_part_is_static_and_off_the_navigation(self):
        for part in self.parts.values():
            self.assertEqual(part["mobility"], "STATIC", part["name"])
            self.assertFalse(part["affects_navigation"], part["name"])


class ArgumentTests(unittest.TestCase):
    def test_paths_and_distance(self):
        args = parse_args(["--tree", "/A/SM_T", "--wood", "/A/SM_W", "--canopy", "/A/SM_C", "--blueprint", "/A/BP_T"])
        self.assertEqual((args.tree, args.wood, args.canopy, args.blueprint, args.wind_distance_cm),
                         ("/A/SM_T", "/A/SM_W", "/A/SM_C", "/A/BP_T", 15000))

    def test_paths_must_be_content_paths(self):
        with self.assertRaises(SystemExit):
            parse_args(["--tree", "SM_T", "--wood", "/A/SM_W", "--canopy", "/A/SM_C", "--blueprint", "/A/BP_T"])


if __name__ == "__main__":
    unittest.main()
