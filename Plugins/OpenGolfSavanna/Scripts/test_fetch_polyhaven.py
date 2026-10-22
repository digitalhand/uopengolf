# SPDX-License-Identifier: MIT
import hashlib
import tempfile
import unittest
from pathlib import Path

from FetchPolyHaven import ASSETS, matches, sha256sums


class PinTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.file = Path(self.folder.name) / "leaf.png"
        self.file.write_bytes(b"jacaranda")
        self.md5 = hashlib.md5(b"jacaranda").hexdigest()

    def tearDown(self):
        self.folder.cleanup()

    def test_a_file_matches_only_its_pinned_size_and_md5(self):
        self.assertTrue(matches(self.file, self.md5, 9))
        self.assertFalse(matches(self.file, self.md5, 10))
        self.assertFalse(matches(self.file, "0" * 32, 9))
        self.assertFalse(matches(Path(self.folder.name) / "missing.png", self.md5, 9))

    def test_sums_list_relative_paths(self):
        expected = hashlib.sha256(b"jacaranda").hexdigest()
        self.assertEqual(sha256sums(Path(self.folder.name), ["leaf.png"]), f"{expected}  leaf.png\n")

    def test_every_pin_is_complete(self):
        for name, files in ASSETS.items():
            self.assertTrue(files, name)
            for path, url, md5, size in files:
                self.assertTrue(url.startswith("https://dl.polyhaven.org/"), path)
                self.assertEqual(len(md5), 32, path)
                self.assertGreater(size, 0, path)
                self.assertFalse(Path(path).is_absolute(), path)

    def test_each_asset_lists_a_path_once(self):
        for name, files in ASSETS.items():
            paths = [path for path, *_ in files]
            self.assertEqual(len(paths), len(set(paths)), name)

    def test_grass_project_brings_its_textures_and_export_maps(self):
        name = "grass_medium_01"
        paths = {path for path, *_ in ASSETS[name]}
        self.assertIn(f"{name}_2k.blend", paths)
        for role in ("alpha_2k.png", "diff_2k.jpg", "dry_diff_2k.png", "nor_gl_2k.exr", "rough_2k.exr"):
            self.assertIn(f"textures/{name}_{role}", paths)
        for role in ("diff_2k.png", "nor_gl_2k.png", "rough_1k.png", "ao_1k.png"):
            self.assertIn(f"maps/{name}_{role}", paths)


if __name__ == "__main__":
    unittest.main()
