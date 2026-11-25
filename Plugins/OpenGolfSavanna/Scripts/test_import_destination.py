# SPDX-License-Identifier: MIT
import unittest
from pathlib import Path

from import_destination import SAVANNA, destination, instance_settings, is_savanna, material_for_slot

PROJECT = Path("/project")
COURSE = {"mount": "/HunterRanch", "content_dir": "Plugins/HunterRanch/Content", "owner": "HunterRanch.v1"}


class DestinationTests(unittest.TestCase):
    def test_manifests_without_a_destination_import_into_the_savanna_plugin(self):
        self.assertEqual(destination({"folder": "Trees/Jacaranda"}, PROJECT),
                         ("/OpenGolfSavanna", PROJECT / "Plugins/OpenGolfSavanna/Content", "OpenGolfSavanna.v1"))
        self.assertTrue(is_savanna(SAVANNA["mount"]))

    def test_a_course_manifest_imports_into_its_own_plugin(self):
        self.assertEqual(destination({"folder": "Vegetation/Trees/IslandTree02", **COURSE}, PROJECT),
                         ("/HunterRanch", PROJECT / "Plugins/HunterRanch/Content", "HunterRanch.v1"))
        self.assertFalse(is_savanna("/HunterRanch"))

    def test_a_destination_needs_all_three_fields(self):
        with self.assertRaises(ValueError):
            destination({"mount": "/HunterRanch"}, PROJECT)

    def test_the_mount_must_be_its_plugins_content(self):
        for wrong in ({**COURSE, "content_dir": "Plugins/OpenGolfSavanna/Content"},
                      {**COURSE, "content_dir": "../Elsewhere/Content"},
                      {**COURSE, "mount": "/Hunter Ranch"},
                      {**COURSE, "owner": ""}):
            with self.assertRaises(ValueError, msg=wrong):
                destination(wrong, PROJECT)


class InstanceSettingsTests(unittest.TestCase):
    def test_manifest_settings_override_the_defaults_without_changing_them(self):
        defaults = {"RoughnessMin": 0.6, "AOStrength": 0.0}
        merged = instance_settings(defaults, {"Tint": [1.05, 0.95, 3.58, 1.0], "RoughnessMin": 0.7})
        self.assertEqual(merged, {"RoughnessMin": 0.7, "AOStrength": 0.0, "Tint": (1.05, 0.95, 3.58, 1.0)})
        self.assertEqual(defaults, {"RoughnessMin": 0.6, "AOStrength": 0.0})

    def test_colours_become_four_channel_tuples(self):
        self.assertEqual(instance_settings({}, {"Tint": [1, 0.5, 2]})["Tint"], (1.0, 0.5, 2.0, 1.0))

    def test_bad_values_are_refused(self):
        for bad in ({"Tint": [1, 2]}, {"Tint": "blue"}, {"RoughnessMin": float("nan")}, {"Tint": [1, 1, -1, 1]}):
            with self.assertRaises(ValueError, msg=bad):
                instance_settings({}, bad)


class SlotTests(unittest.TestCase):
    NAMES = ["island_tree_02", "island_tree_02_branches", "island_tree_02_leaves"]

    def test_a_slot_takes_its_own_material_even_when_another_name_prefixes_it(self):
        self.assertEqual(material_for_slot("island_tree_02_leaves", self.NAMES), "island_tree_02_leaves")
        self.assertEqual(material_for_slot("island_tree_02_branches", self.NAMES), "island_tree_02_branches")
        self.assertEqual(material_for_slot("island_tree_02", self.NAMES), "island_tree_02")

    def test_a_suffixed_slot_takes_the_longest_material_it_extends(self):
        self.assertEqual(material_for_slot("island_tree_02_leaves_001", self.NAMES), "island_tree_02_leaves")
        self.assertEqual(material_for_slot("jacaranda_tree_trunk_skin", ["jacaranda_tree_trunk"]), "jacaranda_tree_trunk")

    def test_an_unknown_slot_has_no_material(self):
        self.assertIsNone(material_for_slot("bark", self.NAMES))
        self.assertIsNone(material_for_slot("island_tree_0", self.NAMES))


if __name__ == "__main__":
    unittest.main()
