# SPDX-License-Identifier: MIT
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from Assets.Scripts import check_content_names as check


def touch(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"")


class NameTests(unittest.TestCase):
    def test_assets(self):
        self.assertEqual(check.asset_problems("SM_Rough_Tile_0_-10"), [])
        self.assertEqual(check.asset_problems("L_SavannaRange_BuiltData"), [])
        self.assertTrue(check.asset_problems("UOpenGolf_BuiltData"))
        self.assertTrue(check.asset_problems("driver_club_face"))

    def test_folders(self):
        for name in ("CutTurf", "Generated", "Tile_-1_-10_01A213E8D4DD7013B322BC3AE7CF7245",
                     "Build_F1319D95477D0A86D393EBB11B847BC8", "Water_01A211295E90702DBBA6FF667E4D34D1", "Developers"):
            self.assertEqual(check.folder_problems(name), [], name)
        for name in ("Draft", "materials", "Coastal", "Tile_1_01A213E8D4DD7013B322BC3AE7CF7245", "My Folder"):
            self.assertTrue(check.folder_problems(name), name)


class ScanTests(unittest.TestCase):
    def test_scan_reports_assets_and_folders_and_skips_engine_folders(self):
        with TemporaryDirectory() as directory:
            project = Path(directory)
            touch(project / "Content/UOpenGolf/Maps/L_Range.umap")
            touch(project / "Content/UOpenGolf/art/M_Trail.uasset")
            touch(project / "Content/Developers/someone/whatever.uasset")
            touch(project / "Plugins/Kit/Content/Meshes/rock.uasset")
            found = check.scan(project)
            self.assertEqual(set(found), {"/Game/UOpenGolf/art", "/Kit/Meshes/rock"})

    def test_exceptions_match_patterns_and_unused_ones_are_reported(self):
        found = {"/Kit/Meshes/rock": ["has no known type prefix"]}
        self.assertEqual(check.unexcepted(found, ["/Kit/Meshes/*"]), {})
        self.assertEqual(check.unused_exceptions(found, ["/Kit/Meshes/*", "/Old/*"]), ["/Old/*"])


class ProjectTests(unittest.TestCase):
    def test_project_content_follows_the_rules(self):
        remaining = check.unexcepted(check.scan(check.PROJECT), check.read_exceptions())
        self.assertEqual(remaining, {}, "Run Assets/Scripts/check_content_names.py and see Docs/ContentNaming.md")


if __name__ == "__main__":
    unittest.main()
