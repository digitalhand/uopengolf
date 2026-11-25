# SPDX-License-Identifier: MIT
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from Assets.Scripts.package_course import layout_files, publish


def entry(version="draft3", platform="Linux"):
    return {"id": "HunterRanch", "name": "Hunter Ranch", "definition": "/HunterRanch/Draft/DA_HunterRanch.DA_HunterRanch",
            "version": version, "platform": platform, "game_version": "0.2.0", "files": []}


class PublishTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.host = self.root / "host"
        self.paks = []
        for suffix, body in ((".pak", b"pak"), (".utoc", b"toc"), (".ucas", b"cas" * 10)):
            path = self.root / f"HunterRanchLinux{suffix}"
            path.write_bytes(body)
            self.paks.append(path)
        self.layout = []
        for name, body in (("source_layout.json", "{}"), ("source.geojson", "{}"), ("corrections.json", "{}"), ("LICENSE.md", "ODbL")):
            path = self.root / name
            path.write_text(body)
            self.layout.append(path)

    def tearDown(self):
        self.temp.cleanup()

    def test_pack_files_are_renamed_hashed_and_listed(self):
        publish(self.paks, self.host, entry(), self.layout)
        catalog = json.loads((self.host / "catalog.json").read_text())
        files = catalog["courses"][0]["files"]
        self.assertEqual([f["name"] for f in files], ["Course.pak", "Course.utoc", "Course.ucas"])
        cas = self.host / "HunterRanch/draft3/Linux/Course.ucas"
        self.assertEqual(files[2]["size"], 30)
        self.assertEqual(files[2]["sha256"], hashlib.sha256(cas.read_bytes()).hexdigest())

    def test_the_derived_layout_is_published_beside_the_pack(self):
        publish(self.paks, self.host, entry(), self.layout)
        course = json.loads((self.host / "catalog.json").read_text())["courses"][0]
        self.assertEqual(course["layout"], "HunterRanch/draft3/layout/")
        self.assertIn("OpenStreetMap", course["attribution"])
        published = sorted(p.name for p in (self.host / "HunterRanch/draft3/layout").iterdir())
        self.assertEqual(published, ["LICENSE.md", "corrections.json", "source.geojson", "source_layout.json"])

    def test_a_second_platform_shares_the_layout_and_both_stay_listed(self):
        publish(self.paks, self.host, entry(), self.layout)
        publish(self.paks, self.host, entry(platform="Windows"), self.layout)
        courses = json.loads((self.host / "catalog.json").read_text())["courses"]
        self.assertEqual(sorted(c["platform"] for c in courses), ["Linux", "Windows"])

    def test_a_published_version_is_never_overwritten(self):
        publish(self.paks, self.host, entry(), self.layout)
        with self.assertRaisesRegex(ValueError, "already exists"):
            publish(self.paks, self.host, entry(), self.layout)

    def test_a_newer_version_replaces_the_catalog_entry_but_keeps_old_files(self):
        publish(self.paks, self.host, entry("draft3"), self.layout)
        publish(self.paks, self.host, entry("draft4"), self.layout)
        courses = json.loads((self.host / "catalog.json").read_text())["courses"]
        self.assertEqual([c["version"] for c in courses], ["draft4"])
        self.assertTrue((self.host / "HunterRanch/draft3/Linux/Course.pak").is_file())


class LayoutFileTests(unittest.TestCase):
    def test_layout_files_come_from_the_source_folder_and_the_recipe(self):
        with tempfile.TemporaryDirectory() as temp:
            source, data = Path(temp) / "source", Path(temp) / "data"
            source.mkdir()
            data.mkdir()
            (data / "corrections.json").write_text("{}")
            (data / "LICENSE.md").write_text("ODbL")
            (source / "recipe.json").write_text(json.dumps({"corrections": str(data / "corrections.json")}))
            for name in ("source_layout.json", "source.geojson"):
                (source / name).write_text("{}")
            self.assertEqual([p.name for p in layout_files(source)],
                             ["source_layout.json", "source.geojson", "corrections.json", "LICENSE.md"])

    def test_a_missing_layout_file_stops_publishing(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            (source / "recipe.json").write_text("{}")
            with self.assertRaisesRegex(FileNotFoundError, "source_layout.json"):
                layout_files(source)


if __name__ == "__main__":
    unittest.main()
