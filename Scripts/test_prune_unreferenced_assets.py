# SPDX-License-Identifier: MIT
# Fixture paths are made up: the tool skips this file when it scans sources, but real paths here would
# still mislead a reader searching for an asset's users.
import unittest
from pathlib import Path

from prune_unreferenced_assets import decide, expand, named_in_sources, package_file

ART = "/Game/Fixture/Art"
MAP = "/Game/Fixture/Maps/Range"


class FakeLibrary:
    def __init__(self, assets):
        self.assets = assets

    def list_assets(self, folder, recursive=True, include_folder=False):
        return [a for a in self.assets if a.startswith(folder.rstrip("/") + "/")]


class ExpandTests(unittest.TestCase):
    def test_folders_expand_and_object_paths_become_packages(self):
        library = FakeLibrary([f"{ART}/M_Fairway.M_Fairway", f"{ART}/T_Grass_Color.T_Grass_Color", "/Game/Fixture/Other/X.X"])
        self.assertEqual(expand(library, [f"{ART}/", "/Kit/Flagstick/M_Flag.M_Flag"]),
                         {f"{ART}/M_Fairway", f"{ART}/T_Grass_Color", "/Kit/Flagstick/M_Flag"})


class NamedInSourcesTests(unittest.TestCase):
    def test_a_full_path_names_the_asset(self):
        sources = {"Kit.h": 'TEXT("/Kit/Grass/SM_Clump.SM_Clump")'}
        self.assertEqual(named_in_sources({"/Kit/Grass/SM_Clump"}, sources), {"/Kit/Grass/SM_Clump": "Kit.h"})

    def test_folder_and_name_in_one_file_name_the_asset(self):
        sources = {"Presentation.py": f"DEST = '{ART}'\npath = DEST + '/M_ShotTrail'",
                   "surfaces.py": "texture('T_Grass_Color')"}
        named = named_in_sources({f"{ART}/M_ShotTrail", f"{ART}/T_Grass_Color"}, sources)
        self.assertEqual(named, {f"{ART}/M_ShotTrail": "Presentation.py"})

    def test_names_match_whole_words_only(self):
        sources = {"flag.py": "FOLDER = '/Kit/Flagstick'\nmaterial('M_FlagCloth')"}
        self.assertEqual(named_in_sources({"/Kit/Flagstick/M_Flag"}, sources), {})


class DecideTests(unittest.TestCase):
    def test_outside_references_and_sources_keep_assets(self):
        packages = {f"{ART}/M_Fairway", f"{ART}/T_Grass_Color", f"{ART}/M_ShotTrail", f"{ART}/SM_GolfBall"}
        referencers = {f"{ART}/T_Grass_Color": [f"{ART}/M_Fairway"], f"{ART}/SM_GolfBall": [MAP]}
        deletable, kept = decide(packages, referencers, {f"{ART}/M_ShotTrail": "Ball.cpp"})
        self.assertEqual(deletable, [f"{ART}/M_Fairway", f"{ART}/T_Grass_Color"])
        self.assertIn("Ball.cpp", kept[f"{ART}/M_ShotTrail"])
        self.assertIn(MAP, kept[f"{ART}/SM_GolfBall"])

    def test_assets_used_by_a_kept_asset_are_kept(self):
        packages = {f"{ART}/M_Fairway", f"{ART}/T_Grass_Color", f"{ART}/T_Grass_NormalDX"}
        referencers = {f"{ART}/T_Grass_Color": [f"{ART}/M_Fairway"], f"{ART}/T_Grass_NormalDX": [f"{ART}/T_Grass_Color"]}
        deletable, kept = decide(packages, referencers, {f"{ART}/M_Fairway": "x.py"})
        self.assertEqual(deletable, [])
        self.assertIn(f"{ART}/M_Fairway", kept[f"{ART}/T_Grass_Color"])
        self.assertIn(f"{ART}/T_Grass_Color", kept[f"{ART}/T_Grass_NormalDX"])

    def test_self_references_do_not_keep_an_asset(self):
        deletable, kept = decide({f"{ART}/M_Fairway"}, {f"{ART}/M_Fairway": [f"{ART}/M_Fairway"]}, {})
        self.assertEqual((deletable, kept), ([f"{ART}/M_Fairway"], {}))


class PackageFileTests(unittest.TestCase):
    def test_game_and_plugin_mounts(self):
        mounts = {"Game": Path("/p/Content"), "Kit": Path("/p/Plugins/Kit/Content")}
        self.assertEqual(package_file(f"{ART}/M_Fairway", mounts), Path("/p/Content/Fixture/Art/M_Fairway"))
        self.assertEqual(package_file("/Kit/Flagstick/M_Flag", mounts), Path("/p/Plugins/Kit/Content/Flagstick/M_Flag"))

    def test_unknown_mount_is_an_error(self):
        with self.assertRaises(ValueError):
            package_file("/Missing/Thing", {"Game": Path("/p/Content")})


if __name__ == "__main__":
    unittest.main()
