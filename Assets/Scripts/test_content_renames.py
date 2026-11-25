# SPDX-License-Identifier: MIT
import unittest

from Assets.Scripts import check_content_names as check
from Assets.Scripts.content_renames import DELETE, actor_label, move, nature_essentials_texture, number_labels, plan, skipped
from Assets.Scripts.content_renames import names


class RuleTests(unittest.TestCase):
    def test_examples(self):
        cases = {
            "/Game/UOpenGolf/Maps/UOpenGolf": "/Game/UOpenGolf/Maps/L_SavannaRange",
            "/OpenGolfCourse/Terrain/Presets/DA_CoastalRough": "/OpenGolfCourse/Terrain/Presets/DA_TerrainSurface_Rough",
            "/OpenGolfCourse/Terrain/Materials/MI_CoastalTee": "/OpenGolfCourse/Terrain/Materials/MI_TerrainSurface_Tee",
            "/OpenGolfCourse/Surfaces/T_Grass_NormalDX": "/OpenGolfCourse/Surfaces/T_Grass_N",
            "/OpenGolfSavanna/Textures/CutTurf/Collar/T_TurfCollar_Color": "/OpenGolfSavanna/CutTurf/Textures/Collar/T_TurfCollar_D",
            "/OpenGolfSavanna/Meshes/CutTurf/Rough/SM_TurfRough_1m_A": "/OpenGolfSavanna/CutTurf/Meshes/Rough/SM_TurfRough_1m_A",
            "/OpenGolfSavanna/Textures/Ground/Rock029/T_Rock029_2K_JPG_NormalDX": "/OpenGolfSavanna/Ground/Textures/Rock029/T_Rock029_N",
            "/OpenGolfSavanna/Trees/Jacaranda/Meshes/SM_JacarandaCanopy": "/OpenGolfSavanna/Trees/Jacaranda/Meshes/SM_Jacaranda_Canopy",
            "/OpenGolfSavanna/Trees/Jacaranda/Meshes/SM_Jacaranda": "/OpenGolfSavanna/Trees/Jacaranda/Meshes/SM_Jacaranda",
            "/OpenGolfSavanna/Trees/Jacaranda/Materials/MI_JacarandaTreeLeaves": "/OpenGolfSavanna/Trees/Jacaranda/Materials/MI_Jacaranda_Leaves",
            "/HunterRanch/Vegetation/Trees/IslandTree02/Materials/MI_IslandTree02":
                "/HunterRanch/Vegetation/Trees/IslandTree02/Materials/MI_IslandTree02_Trunk",
            "/Game/UOpenGolf/Terrain/Savanna/Trees/DA_SavannaTreeOuter12": "/Game/UOpenGolf/Terrain/Savanna/Trees/DA_SavannaTree_Outer_00",
            "/Game/UOpenGolf/Terrain/Savanna/Trees/DA_SavannaTreeBackdrop35": "/Game/UOpenGolf/Terrain/Savanna/Trees/DA_SavannaTree_Backdrop_07",
        }
        for old, new in cases.items():
            self.assertEqual(move(old), new, old)

    def test_nature_essentials_textures_take_their_material_name(self):
        cases = {"T_NE_Branches_49a21_NormalOpenGL_16bit_4K": "T_NE_Branch_N", "T_NE_rock_1_loop_2500_LOD0_u0_v0": "T_NE_RockLight_A_D",
                 "T_NE_Rock_4_H_AO": "T_NE_RockDark_A_AO", "T_NE_Rock2HNormal": "T_NE_RockLight_B_N",
                 "T_NE_CreepingBentgrass_COLOR": "T_NE_Grass_D", "T_NE_rock_4H_rough": "T_NE_RockDark_A_R"}
        for old, new in cases.items():
            self.assertEqual(nature_essentials_texture(old), new, old)

    def test_two_assets_on_one_path_are_refused(self):
        with self.assertRaises(ValueError):
            plan(["/OpenGolfCourse/Surfaces/T_Grass_Color", "/OpenGolfCourse/Surfaces/T_Grass_D"])

    def test_the_rebuilt_course_and_vegetation_runs_are_left_alone(self):
        self.assertTrue(skipped("/HunterRanch/Course/Course"))
        self.assertTrue(skipped("/Game/UOpenGolf/Terrain/Savanna/CutTurf/Vegetation_01A1FCD896297B9BA3EA4F397FA1F142/Points_7"))
        self.assertEqual(plan(["/HunterRanch/Course/DA_Ground"]), {})


class ActorLabelTests(unittest.TestCase):
    def test_saved_labels(self):
        cases = {
            ("Coastal ground west", "Landscape"): "Landscape_West",
            ("Terrain DA_FairwayGrass008", "StaticMeshActor", "Fairway"): "SM_Fairway_Ground",
            ("Tree colliders SavannaTreeOuter12", "Actor"): "Colliders_SavannaTree_Outer_00",
            ("Tree canopy SM_Jacaranda", "Actor"): "Canopy_Jacaranda",
            ("PCG DA_TurfFairwayGrass 0.5m", "Actor"): "PCG_TurfFairwayGrass05m",
            ("Coastal trees", "Actor"): "Trees",
            ("Course grass", "Actor"): "CourseGrass",
            ("Course Environment", "OpenGolfEnvironment"): "Environment_Daylight",
            ("Coastal range terrain", "OpenGolfTerrainArea", None, "SavannaRange"): "TerrainArea_SavannaRange",
            ("UOpenGolfRange", "UOpenGolfRange"): "RangeSettings",
            ("Range settings", "UOpenGolfRange"): "RangeSettings",
            ("Savanna understory", "Actor"): "Vegetation_SavannaUnderstory",
        }
        for args, label in cases.items():
            self.assertEqual(actor_label(*args), label, args)
            self.assertFalse(" " in label)

    def test_editor_numbered_duplicates(self):
        for label in ("Coastal trees2", "Coastal_Trees3", "Course grass4"):
            self.assertIn(actor_label(label, "Actor"), ("Trees", "CourseGrass"), label)

    def test_numbering_continues_after_existing_labels(self):
        self.assertEqual(number_labels(["Trees", "Trees", "Landscape_West"], {"Trees_36", "Trees_01"}),
                         ["Trees_37", "Trees_38", "Landscape_West"])
        self.assertEqual(number_labels(["Trees", "Trees"], set()), ["Trees_01", "Trees_02"])
        self.assertEqual(number_labels(["CourseGrass"], set()), ["CourseGrass"])

    def test_conforming_labels_stay(self):
        for label in ("RangeView", "Flagstick", "DirectionalLight", "SM_Rough_Tile_0_-10", "BP_Tree_IslandTree02_Hole01"):
            self.assertEqual(actor_label(label, "Actor"), label)


class ProjectTests(unittest.TestCase):
    """The plan over the content on disk, before or after the rename has been applied."""

    def setUp(self):
        self.packages = []
        for mount, root in check.content_roots(check.PROJECT):
            for path in root.rglob("*"):
                if path.suffix in check.PACKAGE_SUFFIXES:
                    self.packages.append(f"{mount}/{path.relative_to(root).with_suffix('').as_posix()}")

    def test_every_renamed_asset_follows_the_rules(self):
        for old, new in plan(self.packages).items():
            self.assertEqual(check.asset_problems(new.rsplit("/", 1)[1]), [], f"{old} -> {new}")
            for folder in new.split("/")[2:-1]:
                self.assertEqual(check.folder_problems(folder), [], f"{old} -> {new}")

    def test_after_the_rename_only_skipped_content_breaks_the_rules(self):
        moves = plan(self.packages)
        after = [moves.get(p, p) for p in self.packages if p not in DELETE and not skipped(p)]
        problems = {p: check.asset_problems(p.rsplit("/", 1)[1]) for p in after}
        self.assertEqual({p: found for p, found in problems.items() if found}, {})


if __name__ == "__main__":
    unittest.main()
