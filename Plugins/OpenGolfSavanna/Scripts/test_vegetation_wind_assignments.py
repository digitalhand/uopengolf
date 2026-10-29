# SPDX-License-Identifier: MIT
from pathlib import Path
import sys
import unittest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "OpenGolfCourse/Scripts"))
import savanna_layout
import understory_layout
import vegetation_wind
import vegetation_wind_assignments as assign

PLUGINS = HERE.parents[1]


def asset_file(path):
    """The .uasset for a /OpenGolfSavanna or /OpenGolfCourse package path."""
    mount, rest = path.lstrip("/").split("/", 1)
    return PLUGINS / mount / "Content" / (rest + ".uasset")


def package_path(file):
    """The /Mount/... package path of a plugin .uasset."""
    content = next(parent for parent in file.parents if parent.name == "Content")
    return f"/{content.parent.name}/{file.relative_to(content).with_suffix('').as_posix()}"


class AssignmentsTest(unittest.TestCase):
    def test_every_material_names_a_known_profile(self):
        for path, entry in assign.MATERIALS.items():
            self.assertIn(entry.profile, vegetation_wind.PROFILES, path)

    def test_materials_and_their_size_meshes_exist(self):
        for path, entry in assign.MATERIALS.items():
            self.assertTrue(asset_file(path).exists(), path)
            for mesh in entry.size_meshes:
                self.assertTrue(asset_file(mesh).exists(), mesh)

    def test_meshes_that_set_a_size_are_placed_moving(self):
        for path, entry in assign.MATERIALS.items():
            for mesh in entry.size_meshes:
                self.assertIsNotNone(assign.mesh_wind_distance_m(mesh), f"{mesh} ({path}) never runs its wind")

    def test_every_mesh_pattern_matches_an_asset(self):
        for pattern in assign.MESHES:
            folder = asset_file(pattern).parent
            matches = [package for package in map(package_path, folder.glob("*.uasset")) if assign.mesh_wind_distance_m(package)]
            self.assertTrue(matches, pattern)

    def test_still_vegetation_stays_still(self):
        for mesh in ("SM_NE_Litter_A", "SM_NE_MossDead_A", "SM_NE_RockDark_A", "SM_NE_Seedling_A"):
            self.assertIsNone(assign.mesh_wind_distance_m(f"{assign.NE}/{mesh}"))
        self.assertIsNone(assign.mesh_wind_distance_m("/OpenGolfSavanna/Meshes/CutTurf/Fairway/SM_CutTurf_Fairway_00"))
        self.assertIsNone(assign.mesh_wind_distance_m(f"{assign.JACARANDA}/Meshes/SM_JacarandaCanopy"))

    def test_bounds_cover_every_placed_scale(self):
        # The material's offset bound is sized for its largest instance; a larger one would be clamped.
        trees = max(settings["scale_range"][1] for settings in savanna_layout.TREE_SETTINGS.values())
        understory = max(category["scale"][1] for category in understory_layout.CATEGORIES.values()
                         if any("Grass" in mesh for mesh in category["meshes"]))
        for path, entry in assign.MATERIALS.items():
            placed = trees if assign.JACARANDA in path else understory if "NatureEssentials" in path else assign.PRESET_MAX_SCALE
            self.assertGreaterEqual(entry.max_scale, placed, path)

    def test_object_paths_match_their_package(self):
        self.assertEqual(assign.mesh_wind_distance_m(f"{assign.JACARANDA}/Meshes/SM_Jacaranda.SM_Jacaranda"), 150.0)
        self.assertEqual(assign.mesh_wind_distance_m(f"{assign.GRASS_MEDIUM}EdgeRamp_A.SM_GrassMedium_30cm_EdgeRamp_A"), 90.0)


if __name__ == "__main__":
    unittest.main()
