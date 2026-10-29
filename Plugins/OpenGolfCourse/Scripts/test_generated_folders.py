# SPDX-License-Identifier: MIT
import unittest

from generated_folders import is_run_folder, loaded_in_memory, referenced_from_outside, remove_unreferenced, run_folders

ROOT = "/Game/Range/Generated"
BUILD = f"{ROOT}/Build_01A180BF3EDD7F0386ED0462654019FB"
OLD_BUILD = f"{ROOT}/Build_01A1567C6ECD7869B99A610A61184ED9"
VEGETATION = f"{ROOT}/Vegetation_01A18050C47175D29FEEC0F95A2CCAA7"
MAP = "/Game/Range/Maps/Range"


class FakeLibrary:
    """The EditorAssetLibrary calls the module makes, over an in-memory registry."""

    def __init__(self, assets, referencers, undeletable=(), loaded=()):
        self.assets = list(assets)
        self.referencers = referencers
        self.undeletable = set(undeletable)
        self.loaded = set(loaded)
        self.deleted = []

    def list_assets(self, folder, recursive=True, include_folder=False):
        return [a for a in self.assets if a.startswith(folder + "/")]

    def find_package_referencers_for_asset(self, package, load_assets_to_confirm=False):
        # Confirming would load the referenced asset, which would then look loaded in this session.
        if load_assets_to_confirm:
            self.loaded.update(a for a in self.assets if a.split(".")[0] == package)
        return self.referencers.get(package, [])

    def find_asset_data(self, path):
        return FakeAssetData(path in self.loaded)

    def delete_directory(self, folder):
        if folder in self.undeletable:
            return False
        self.deleted.append(folder)
        self.assets = [a for a in self.assets if not a.startswith(folder + "/")]
        return True


class FakeAssetData:
    def __init__(self, loaded):
        self.loaded = loaded

    def is_asset_loaded(self):
        return self.loaded


def asset(folder, name):
    return f"{folder}/{name}.{name}"


class RunFolderNameTests(unittest.TestCase):
    def test_builder_folders_are_recognised(self):
        self.assertTrue(is_run_folder("Build_01A180BF3EDD7F0386ED0462654019FB"))
        self.assertTrue(is_run_folder("Vegetation_01A18050C47175D29FEEC0F95A2CCAA7"))

    def test_other_names_are_never_run_folders(self):
        for name in ("Build_", "Build_01A180BF", "Build_01A180BF3EDD7F0386ED0462654019FBX", "build_01A180BF3EDD7F0386ED0462654019FB",
                     "Vegetation_01a18050c47175d29feec0f95a2ccaa7", "Materials", "Trees", "Vegetation_ABC/Points_0", ""):
            self.assertFalse(is_run_folder(name), name)

    def test_prefixes_can_be_narrowed(self):
        self.assertFalse(is_run_folder("Build_01A180BF3EDD7F0386ED0462654019FB", prefixes=("Vegetation_",)))


class RunFoldersTests(unittest.TestCase):
    def test_lists_only_run_folders_directly_under_the_root(self):
        library = FakeLibrary([asset(BUILD, "SM_Surface_0"), asset(VEGETATION, "Points_0"), asset(VEGETATION, "PCG_Vegetation_0"),
                               asset(f"{ROOT}/Materials", "MI_Ground"), asset(ROOT, "DA_Range"),
                               asset(f"{ROOT}/Nested/Build_01A1567C6ECD7869B99A610A61184ED9", "SM_Surface_0")], {})
        self.assertEqual(run_folders(library, ROOT), {BUILD, VEGETATION})


class ReferenceTests(unittest.TestCase):
    def test_references_inside_the_folder_do_not_count(self):
        graph, points = asset(VEGETATION, "PCG_Vegetation_0"), asset(VEGETATION, "Points_0")
        library = FakeLibrary([graph, points], {points.split(".")[0]: [graph.split(".")[0]]})
        self.assertFalse(referenced_from_outside(library, VEGETATION))

    def test_a_map_reference_keeps_the_folder(self):
        mesh = asset(BUILD, "SM_Surface_0")
        library = FakeLibrary([mesh], {mesh.split(".")[0]: [MAP]})
        self.assertTrue(referenced_from_outside(library, BUILD))

    def test_a_sibling_with_a_longer_name_is_outside(self):
        mesh = asset(BUILD, "SM_Surface_0")
        library = FakeLibrary([mesh], {mesh.split(".")[0]: [BUILD + "0/SM_Other"]})
        self.assertTrue(referenced_from_outside(library, BUILD))


class LoadedTests(unittest.TestCase):
    def test_a_folder_with_any_loaded_asset_is_loaded(self):
        graph, points = asset(VEGETATION, "PCG_Vegetation_0"), asset(VEGETATION, "Points_0")
        self.assertTrue(loaded_in_memory(FakeLibrary([graph, points], {}, loaded=[points]), VEGETATION))
        self.assertFalse(loaded_in_memory(FakeLibrary([graph, points], {}), VEGETATION))


class RemoveTests(unittest.TestCase):
    def library(self, undeletable=(), loaded=()):
        current, old = asset(BUILD, "SM_Surface_0"), asset(OLD_BUILD, "SM_Surface_0")
        graph, points = asset(VEGETATION, "PCG_Vegetation_0"), asset(VEGETATION, "Points_0")
        return FakeLibrary([current, old, graph, points, asset(f"{ROOT}/Materials", "MI_Ground")],
                           {current.split(".")[0]: [MAP], points.split(".")[0]: [graph.split(".")[0]]}, undeletable, loaded)

    def test_unreferenced_runs_go_and_the_map_keeps_its_build(self):
        library, lines = self.library(), []
        removed, kept = remove_unreferenced(library, [ROOT], log=lines.append)
        self.assertEqual(sorted(removed), sorted([OLD_BUILD, VEGETATION]))
        self.assertEqual(kept, [BUILD])
        self.assertEqual(sorted(library.deleted), sorted([OLD_BUILD, VEGETATION]))
        self.assertTrue(any(BUILD in line for line in lines))

    def test_a_dry_run_deletes_nothing(self):
        library = self.library()
        removed, kept = remove_unreferenced(library, [ROOT], apply=False, log=lambda _line: None)
        self.assertEqual(sorted(removed), sorted([OLD_BUILD, VEGETATION]))
        self.assertEqual(library.deleted, [])

    def test_kept_folders_are_skipped(self):
        library = self.library()
        removed, _kept = remove_unreferenced(library, [ROOT], keep=[VEGETATION], log=lambda _line: None)
        self.assertEqual(removed, [OLD_BUILD])

    def test_a_failed_delete_is_reported_as_kept(self):
        library = self.library(undeletable=[OLD_BUILD])
        removed, kept = remove_unreferenced(library, [ROOT], log=lambda _line: None)
        self.assertEqual(removed, [VEGETATION])
        self.assertEqual(sorted(kept), sorted([BUILD, OLD_BUILD]))

    def test_a_run_still_loaded_in_this_session_is_kept_not_deleted(self):
        # The editor can still hold a run it just baked; deleting it then fails part-way and corrupts it.
        library, lines = self.library(loaded=[asset(VEGETATION, "Points_0")]), []
        removed, kept = remove_unreferenced(library, [ROOT], log=lines.append)
        self.assertEqual(removed, [OLD_BUILD])
        self.assertEqual(sorted(kept), sorted([BUILD, VEGETATION]))
        self.assertNotIn(VEGETATION, library.deleted)
        self.assertTrue(any(VEGETATION in line and "loaded" in line for line in lines))

    def test_a_referenced_build_is_reported_as_referenced_even_when_loaded(self):
        library, lines = self.library(loaded=[asset(BUILD, "SM_Surface_0")]), []
        remove_unreferenced(library, [ROOT], log=lines.append)
        self.assertTrue(any(BUILD in line and "referenced" in line for line in lines))

    def test_a_dry_run_leaves_runs_unloaded_so_apply_can_remove_them(self):
        library = self.library()
        remove_unreferenced(library, [ROOT], apply=False, log=lambda _line: None)
        removed, _kept = remove_unreferenced(library, [ROOT], log=lambda _line: None)
        self.assertEqual(sorted(removed), sorted([OLD_BUILD, VEGETATION]))

    def test_other_folders_are_never_touched(self):
        library = self.library()
        remove_unreferenced(library, [ROOT], log=lambda _line: None)
        self.assertTrue(any("/Materials/" in a for a in library.assets))


if __name__ == "__main__":
    unittest.main()
