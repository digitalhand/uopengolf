# SPDX-License-Identifier: MIT
import unittest

import asset_naming as n


class BuildNameTests(unittest.TestCase):
    def test_free_text_becomes_pascal_case(self):
        self.assertEqual(n.pascal("island_tree_02"), "IslandTree02")
        self.assertEqual(n.pascal("red sand"), "RedSand")
        self.assertEqual(n.pascal("HunterRanch"), "HunterRanch")

    def test_variants_have_two_digits(self):
        self.assertEqual(n.variant(1), "01")
        self.assertEqual(n.variant(12), "12")
        with self.assertRaises(ValueError):
            n.variant(-1)

    def test_join_rejects_unknown_prefixes_and_bad_segments(self):
        self.assertEqual(n.join("SM_", "Lake", "01"), "SM_Lake_01")
        with self.assertRaises(ValueError):
            n.join("XX_", "Lake")
        with self.assertRaises(ValueError):
            n.join("SM_", "lake")

    def test_texture_roles_from_source_schemes(self):
        for role, suffix in {"diff": "D", "Color": "D", "Albedo": "D", "nor_gl": "N", "NormalDX": "N", "NRM": "N",
                             "rough": "R", "Roughness": "R", "alpha": "A", "Opacity": "A", "ao": "AO",
                             "AmbientOcclusion": "AO", "arm": "ORM"}.items():
            self.assertEqual(n.texture_suffix(role), suffix, role)
        self.assertIsNone(n.texture_suffix("preview"))

    def test_texture_names(self):
        self.assertEqual(n.texture("jacaranda_tree", "diff", "leaves"), "T_JacarandaTree_Leaves_D")
        self.assertEqual(n.texture("Rock029", "N"), "T_Rock029_N")
        with self.assertRaises(ValueError):
            n.texture("Rock029", "preview")


class SourceNameTests(unittest.TestCase):
    def test_source_textures(self):
        cases = {
            "jacaranda_tree_leaves_nor_gl_2k": "T_Jacaranda_Leaves_N",
            "jacaranda_tree_trunk_diff_2k": "T_Jacaranda_Trunk_D",
            "island_tree_02_diff_2k": "T_IslandTree02_Trunk_D",
            "island_tree_02_leaves_alpha_2k": "T_IslandTree02_Leaves_A",
            "grass_medium_01_dry_diff_2k": "T_GrassMedium01_Dry_D",
            "grass_medium_01_ao_1k": "T_GrassMedium01_AO",
            "leafy_grass_rough_1k": "T_LeafyGrass_R",
            "red_sand_nor_gl_2k": "T_RedSand_N",
            "Rock029_2K-JPG_NormalDX": "T_Rock029_N",
            "Rock029_2K-JPG_Color": "T_Rock029_D",
        }
        for stem, name in cases.items():
            self.assertEqual(n.source_texture(stem), name, stem)
            self.assertEqual(n.problems(name), [], name)
        with self.assertRaises(ValueError):
            n.source_texture("preview")

    def test_material_instances(self):
        self.assertEqual(n.material_instance("jacaranda_tree_leaves"), "MI_Jacaranda_Leaves")
        self.assertEqual(n.material_instance("island_tree_02"), "MI_IslandTree02_Trunk")
        self.assertEqual(n.material_instance("island_tree_02_branches"), "MI_IslandTree02_Branches")
        self.assertEqual(n.material_instance("grass_medium_01"), "MI_GrassMedium01")
        self.assertEqual(n.material_instance("rock_dark"), "MI_RockDark")

    def test_mesh_parts(self):
        self.assertEqual(n.mesh("SM_JacarandaCanopy"), "SM_Jacaranda_Canopy")
        self.assertEqual(n.mesh("SM_IslandTree02Collision"), "SM_IslandTree02_Collision")
        self.assertEqual(n.mesh("SM_Jacaranda_Canopy"), "SM_Jacaranda_Canopy")
        self.assertEqual(n.mesh("SM_Jacaranda"), "SM_Jacaranda")


class CourseNameTests(unittest.TestCase):
    def test_course_assets(self):
        self.assertEqual(n.course_map("HunterRanch"), "L_HunterRanch")
        self.assertEqual(n.course_definition("Hunter Ranch"), "DA_HunterRanch")
        self.assertEqual(n.course_terrain("HunterRanch"), "DA_HunterRanch_Terrain")

    def test_generated_folders_sit_under_the_course(self):
        self.assertEqual(n.generated_tiles("/HunterRanch/Course"), "/HunterRanch/Course/Generated/Tiles")
        self.assertEqual(n.generated_water("/HunterRanch/Course"), "/HunterRanch/Course/Generated/Water")

    def test_tiles_name_their_surface_and_square(self):
        self.assertEqual(n.tile_tag(0, -10), "Tile_0_-10")
        self.assertEqual(n.tile_mesh("Rough", 0, -10), "SM_Rough_Tile_0_-10")
        self.assertEqual(n.tile_mesh("fairway", 12, 3), "SM_Fairway_Tile_12_3")
        self.assertEqual(n.problems(n.tile_mesh("Green", -1, 4)), [])

    def test_lakes_are_numbered_with_two_digits(self):
        self.assertEqual(n.lake(1), "Lake_01")
        self.assertEqual(n.problems("SM_" + n.lake(1)), [])


class ProblemTests(unittest.TestCase):
    def test_conforming_names(self):
        for name in ("SM_Rough_Tile_0_-10", "SM_Lake_01", "DA_HunterRanch", "DA_HunterRanch_Terrain", "L_SavannaRange",
                     "MI_TerrainSurface_Fairway", "T_Jacaranda_Leaves_D", "T_NE_Rock01_N", "BP_Tree_IslandTree02",
                     "PM_FairwaySoft", "MPC_Wind", "WBP_WindIndicator", "SM_Surface_00", "T_Avatar_Bear_D"):
            self.assertEqual(n.problems(name), [], name)

    def test_each_rule(self):
        cases = {
            "driver_club_face": "has no known type prefix",
            "SM_CoastalRough": "contains 'coastal'",
            "DA_DraftCourse": "contains 'draft'",
            "SM_Lake_01": None,
            "SM_Surface_0": "variant '0' is not two digits",
            "T_island_tree_02_diff_2k": "segment 'island' is not PascalCase",
            "T_Rock029_Color": "should be the suffix _D",
            "T_NE_Rock2HNormal": "should be the suffix _N",
            "T_Rock029_2K_JPG_D": "segment '2K' is a source file detail",
            "T_NE_Rock3H_LOD0_D": "segment 'LOD0' is a source file detail",
            "Terrain DA_X": "contains a space",
            "SM_": "has no name after its prefix",
        }
        for name, expected in cases.items():
            found = n.problems(name)
            if expected is None:
                self.assertEqual(found, [], name)
            else:
                self.assertTrue(any(expected in p for p in found), f"{name}: {found}")

    def test_ui_and_data_textures_need_no_role_suffix(self):
        for name in ("T_Avatar_Eagle", "T_SavannaRange_Preview", "T_WhiteMask", "T_CourseSurfaceField"):
            self.assertEqual(n.problems(name), [], name)

    def test_coordinates_only_follow_tile(self):
        self.assertTrue(n.problems("SM_Rough_-1"))
        self.assertEqual(n.problems("SM_Rough_Tile_-1_-1"), [])

    def test_generated_guid_segments_are_allowed(self):
        self.assertEqual(n.problems("SM_Rough_Tile_1_1_01A20F3A24AE785298BF8625BD59E809"), [])


if __name__ == "__main__":
    unittest.main()
