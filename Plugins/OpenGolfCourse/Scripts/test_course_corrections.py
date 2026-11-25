# SPDX-License-Identifier: MIT
import copy
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from course_corrections import apply, capture, read

LAYOUT = {
    "version": 1,
    "holes": [
        {"number": 1, "par": 4, "routing_m": [[0, 0], [100, 20], [300, 0]], "cup_m": [300, 0],
         "tees_m": {"Blue": [0, 0], "Red": [40, 8]}, "review": []},
        {"number": 2, "par": 3, "routing_m": [[400, 0], [550, 0]], "cup_m": [550, 0],
         "tees_m": {"Blue": [400, 0]}, "review": ["cup slope 4.0 %"]},
    ],
    "regions": [
        {"id": "way/1", "surface": "fairway", "outline_m": [[0, -10], [300, -10], [300, 10], [0, 10]]},
        {"id": "way/2", "surface": "green", "outline_m": [[290, -8], [310, -8], [310, 8], [290, 8]]},
    ],
    "water": [],
}


class ApplyTests(unittest.TestCase):
    def test_no_corrections_returns_an_equal_copy(self):
        result = apply(LAYOUT, None)
        self.assertEqual(result, LAYOUT)
        self.assertIsNot(result, LAYOUT)

    def test_the_source_layout_is_never_changed(self):
        before = copy.deepcopy(LAYOUT)
        apply(LAYOUT, {"holes": {"1": {"cup_m": [295, 2], "tees_m": {"Red": [50, 9]}}},
                       "regions": {"way/2": None}})
        self.assertEqual(LAYOUT, before)

    def test_hole_values_override_and_tee_sets_merge(self):
        result = apply(LAYOUT, {"holes": {"1": {"cup_m": [295, 2], "tees_m": {"Red": [50, 9]}}}})
        hole = result["holes"][0]
        self.assertEqual(hole["cup_m"], [295, 2])
        self.assertEqual(hole["tees_m"], {"Blue": [0, 0], "Red": [50, 9]})
        self.assertIn("corrected by author", hole["review"])

    def test_aim_points_replace_the_middle_of_the_route(self):
        result = apply(LAYOUT, {"holes": {"1": {"aim_m": [[120, 30], [200, 25]]}}})
        self.assertEqual(result["holes"][0]["routing_m"], [[0, 0], [120, 30], [200, 25], [300, 0]])

    def test_a_reviewed_hole_clears_its_review_notes(self):
        result = apply(LAYOUT, {"holes": {"2": {"reviewed": "Cup slope accepted from aerial"}}})
        self.assertEqual(result["holes"][1]["review"], ["corrected by author"])

    def test_regions_can_be_replaced_removed_or_added(self):
        outline = [[280, -9], [312, -9], [312, 9], [280, 9]]
        result = apply(LAYOUT, {"regions": {"way/2": {"surface": "green", "outline_m": outline}, "way/1": None,
                                            "author/1": {"surface": "bunker", "outline_m": [[50, 12], [60, 12], [55, 20]]}}})
        ids = [r["id"] for r in result["regions"]]
        self.assertEqual(ids, ["way/2", "author/1"])
        self.assertEqual(result["regions"][0]["outline_m"], outline)

    def test_unknown_holes_and_bad_values_fail_clearly(self):
        with self.assertRaisesRegex(ValueError, "hole 19"):
            apply(LAYOUT, {"holes": {"19": {"cup_m": [0, 0]}}})
        with self.assertRaisesRegex(ValueError, "cup_m"):
            apply(LAYOUT, {"holes": {"1": {"cup_m": [0]}}})
        with self.assertRaisesRegex(ValueError, "surface"):
            apply(LAYOUT, {"regions": {"author/1": {"surface": "lava", "outline_m": [[0, 0], [1, 0], [0, 1]]}}})


class ReadTests(unittest.TestCase):
    def test_the_file_named_by_the_recipe_is_read(self):
        with TemporaryDirectory() as folder:
            path = Path(folder) / "corrections.json"
            path.write_text(json.dumps({"holes": {"2": {"cup_m": [1, 2]}}}), encoding="utf-8")
            self.assertEqual(read({"corrections": str(path)}), {"holes": {"2": {"cup_m": [1, 2]}}})

    def test_no_corrections_until_the_recipe_names_a_written_file(self):
        with TemporaryDirectory() as folder:
            self.assertIsNone(read({}))
            self.assertIsNone(read({"corrections": str(Path(folder) / "corrections.json")}))


class CaptureTests(unittest.TestCase):
    def test_only_changes_beyond_five_centimetres_are_kept(self):
        holes = [{"number": 1, "cup_m": [300.01, 0], "tees_m": {"Blue": [0, 0], "Red": [45, 8]}, "aim_m": [[100, 20]]},
                 {"number": 2, "cup_m": [552, 1], "tees_m": {"Blue": [400, 0]}, "aim_m": []}]
        self.assertEqual(capture(LAYOUT, holes, LAYOUT["regions"]),
                         {"holes": {"1": {"tees_m": {"Red": [45, 8]}}, "2": {"cup_m": [552, 1]}}, "regions": {}})

    def test_changed_aim_and_region_outlines_are_kept(self):
        moved = [dict(LAYOUT["regions"][0]), dict(LAYOUT["regions"][1], outline_m=[[289, -8], [310, -8], [310, 8], [290, 8]])]
        holes = [{"number": 1, "cup_m": [300, 0], "tees_m": {"Blue": [0, 0], "Red": [40, 8]}, "aim_m": [[110, 25]]},
                 {"number": 2, "cup_m": [550, 0], "tees_m": {"Blue": [400, 0]}, "aim_m": []}]
        result = capture(LAYOUT, holes, moved)
        self.assertEqual(result["holes"], {"1": {"aim_m": [[110, 25]]}})
        self.assertEqual(list(result["regions"]), ["way/2"])

    def test_existing_corrections_are_kept_when_capturing_again(self):
        holes = [{"number": 1, "cup_m": [300, 0], "tees_m": {"Blue": [0, 0], "Red": [40, 8]}, "aim_m": [[100, 20]]},
                 {"number": 2, "cup_m": [550, 0], "tees_m": {"Blue": [400, 0]}, "aim_m": []}]
        previous = {"holes": {"2": {"reviewed": "Checked on aerial"}}, "attribution": "x"}
        result = capture(LAYOUT, holes, LAYOUT["regions"], previous)
        self.assertEqual(result["holes"], {"2": {"reviewed": "Checked on aerial"}})
        self.assertEqual(result["attribution"], "x")


if __name__ == "__main__":
    unittest.main()
