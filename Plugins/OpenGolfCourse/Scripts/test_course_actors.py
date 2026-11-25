# SPDX-License-Identifier: MIT
import json
import unittest

from course_actors import normalize_label, record_entry


class LabelTests(unittest.TestCase):
    def test_numbers_get_two_digits(self):
        self.assertEqual(normalize_label("BP_Tree_IslandTree02_Hole1"), "BP_Tree_IslandTree02_Hole01")
        self.assertEqual(normalize_label("BP_Tree_Jacaranda_3"), "BP_Tree_Jacaranda_03")
        self.assertEqual(normalize_label("BP_Tree_IslandTree02_Hole12"), "BP_Tree_IslandTree02_Hole12")

    def test_spaces_become_separators(self):
        self.assertEqual(normalize_label("BP_Tree Hole 4"), "BP_Tree_Hole_04")


class EntryTests(unittest.TestCase):
    def test_entries_round_trip_through_json(self):
        entry = record_entry("/HunterRanch/Vegetation/Trees/BP_Tree_IslandTree02.BP_Tree_IslandTree02_C",
                             (1.0, 2.0, 3.0), (0.0, 0.0, 90.0), (1.1, 1.1, 1.1), "BP_Tree_IslandTree02_Hole1", "Vegetation")
        self.assertEqual(json.loads(json.dumps(entry)), entry)
        self.assertEqual(entry["label"], "BP_Tree_IslandTree02_Hole01")

    def test_folders_get_two_digit_numbers_and_none_means_no_folder(self):
        entry = record_entry("/T/BP_Tree.BP_Tree_C", (0, 0, 0), (0, 0, 0), (1, 1, 1), "BP_Tree", "Course/Hole_1")
        self.assertEqual(entry["folder"], "Course/Hole_01")
        self.assertEqual(record_entry("/T/BP_Tree.BP_Tree_C", (0, 0, 0), (0, 0, 0), (1, 1, 1), "BP_Tree", "None")["folder"], "")


if __name__ == "__main__":
    unittest.main()
