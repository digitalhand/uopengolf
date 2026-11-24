# SPDX-License-Identifier: MIT
import hashlib
import re
import struct
import tempfile
import unittest
from pathlib import Path

from FetchFluentEmoji import AVATARS, COMMIT, LICENSE, matches, png_size, url

CATALOGUE = Path(__file__).resolve().parents[3] / "Source/UOpenGolf/Private/Profile/AvatarCatalog.cpp"


class PinTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.root = Path(self.folder.name)

    def tearDown(self):
        self.folder.cleanup()

    def test_at_least_twenty_unique_avatars(self):
        ids = [avatar for avatar, *_ in AVATARS]
        self.assertGreaterEqual(len(ids), 20)
        self.assertEqual(len(ids), len(set(ids)))
        for avatar in ids:
            self.assertRegex(avatar, r"^[A-Z][A-Za-z]+$", "ids name Unreal assets T_Avatar_<Id>")

    def test_every_pin_is_complete(self):
        self.assertRegex(COMMIT, r"^[0-9a-f]{40}$")
        for avatar, path, digest, size in AVATARS:
            self.assertRegex(digest, r"^[0-9a-f]{64}$", avatar)
            self.assertGreater(size, 0, avatar)
            self.assertTrue(path.endswith("_3d.png") or path.endswith("_3d_default.png"), avatar)
            self.assertIn("/3D/", path, avatar)
        self.assertRegex(LICENSE[1], r"^[0-9a-f]{64}$")

    def test_urls_pin_the_commit_and_encode_spaces(self):
        source = url("assets/Flag in hole/3D/flag_in_hole_3d.png")
        self.assertTrue(source.startswith(f"https://raw.githubusercontent.com/microsoft/fluentui-emoji/{COMMIT}/"))
        self.assertIn("Flag%20in%20hole", source)

    def test_a_file_matches_only_its_pinned_size_and_hash(self):
        file = self.root / "a.png"
        file.write_bytes(b"emoji")
        digest = hashlib.sha256(b"emoji").hexdigest()
        self.assertTrue(matches(file, digest, 5))
        self.assertTrue(matches(file, digest))
        self.assertFalse(matches(file, digest, 6))
        self.assertFalse(matches(file, "0" * 64, 5))
        self.assertFalse(matches(self.root / "missing.png", digest, 5))

    def test_png_size_reads_the_header(self):
        png = self.root / "square.png"
        png.write_bytes(b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 256, 256) + b"\x08\x06\x00\x00\x00")
        self.assertEqual(png_size(png), (256, 256))
        text = self.root / "page.html"
        text.write_bytes(b"<html>not an image</html>")
        self.assertIsNone(png_size(text))

    def test_ids_match_the_game_catalogue(self):
        catalogue = re.findall(r'\{TEXT\("([A-Za-z]+)"\), TEXT\("', CATALOGUE.read_text())
        self.assertEqual(catalogue, [avatar for avatar, *_ in AVATARS])


if __name__ == "__main__":
    unittest.main()
