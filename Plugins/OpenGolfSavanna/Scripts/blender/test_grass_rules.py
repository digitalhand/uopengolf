# SPDX-License-Identifier: MIT
import unittest

from grass_rules import (CELL_M, EDGE_WEIGHTS, MEDIUM, TILE_M, TILES, content_folders, grass_maps, grid,
                         medium_tuft, medium_tuft_kind, tile_points, underlay_maps)

PIECES = {"GrassMedium_TallA_LOD0": "Tall", "GrassMedium_MidA_LOD0": "Mid", "GrassMedium_LargeA_LOD0": "Large",
          "GrassMedium_TinyA_LOD0": "Tiny"}


class GrassRuleTests(unittest.TestCase):
    def test_static_tufts_become_pieces(self):
        self.assertEqual(medium_tuft("grass_medium_01_tall_a_LOD0"), "GrassMedium_TallA_LOD0")
        self.assertEqual(medium_tuft("grass_medium_01_mid_c_LOD2"), "GrassMedium_MidC_LOD2")
        self.assertIsNone(medium_tuft("grass_medium_01_tall_a_LOD3"))
        self.assertIsNone(medium_tuft("grass_medium_01_geonodes_tall_a_LOD0"))
        self.assertEqual(medium_tuft_kind("GrassMedium_TallA_LOD1"), "Tall")
        with self.assertRaises(ValueError):
            medium_tuft_kind("SM_GrassMedium_TallA")

    def test_tiles_have_unique_names_and_seeds(self):
        self.assertEqual(len({name for name, *_ in TILES}), len(TILES))
        self.assertEqual(len({seed for _, _, seed, _ in TILES}), len(TILES))
        for name, asset, seed, settings in TILES:
            self.assertEqual(asset, MEDIUM, name)
            self.assertTrue({"size", "lod", "scale"} <= set(settings), name)
            grid(settings["size"])  # every footprint is a whole number of cells

    def test_the_family_has_halving_squares_and_an_edge_strip(self):
        sizes = {settings["size"] for *_, settings in TILES}
        self.assertEqual(sizes, {(1.0, 1.0), (0.5, 0.5), (0.25, 0.25), (1.0, 0.25)})
        for name, _, _, settings in TILES:
            square = settings["size"][0] == settings["size"][1]
            self.assertEqual("Edge" not in name, square, name)

    def test_every_footprint_has_the_same_density(self):
        for name, _, seed, settings in TILES:
            points = tile_points(seed, PIECES, settings["size"], weights=settings.get("weights"))
            area = settings["size"][0] * settings["size"][1]
            self.assertAlmostEqual(len(points) / area, 1 / CELL_M ** 2, msg=name)

    def test_points_are_repeatable_inside_the_tile_and_evenly_spread(self):
        points = tile_points(11, PIECES)
        self.assertEqual(points, tile_points(11, PIECES))
        self.assertNotEqual(points, tile_points(23, PIECES))
        self.assertEqual(len(points), 256)
        half = TILE_M / 2
        self.assertTrue(all(-half < x < half and -half < y < half for x, y, *_ in points))
        quadrants = [sum(1 for x, y, *_ in points if (x < 0) == left and (y < 0) == low)
                     for left in (True, False) for low in (True, False)]
        self.assertEqual(quadrants, [64] * 4)
        self.assertTrue(all(0.9 <= scale <= 1.15 and name in PIECES for _, _, _, scale, name in points))
        half_size = tile_points(11, PIECES, scale=0.5)
        self.assertTrue(all(0.45 <= scale <= 0.575 for _, _, _, scale, _ in half_size))

    def test_strips_stay_inside_their_rectangle_without_large_tufts(self):
        points = tile_points(71, PIECES, (1.0, 0.25), weights=EDGE_WEIGHTS)
        self.assertEqual(len(points), 64)
        self.assertTrue(all(-0.5 < x < 0.5 and -0.125 < y < 0.125 for x, y, *_ in points))
        self.assertNotIn("GrassMedium_LargeA_LOD0", {name for *_, name in points})

    def test_footprints_off_the_cell_grid_are_rejected(self):
        with self.assertRaises(ValueError):
            grid((0.3, 0.3))

    def test_unweighted_pieces_are_rejected(self):
        with self.assertRaises(ValueError):
            tile_points(1, {"Unknown": "Weed"})

    def test_folders_are_type_first(self):
        self.assertEqual(content_folders("Grass/Medium"),
                         {"Meshes": "Meshes/Grass/Medium", "Materials": "Materials/Grass/Medium",
                          "Textures": "Textures/Grass/Medium"})

    def test_grass_maps_include_the_dry_colour_and_alpha(self):
        medium = grass_maps(MEDIUM, "/art/PolyHaven/grass_medium_01/maps")
        self.assertEqual(medium["albedo_dry"], "/art/PolyHaven/grass_medium_01/textures/grass_medium_01_dry_diff_2k.png")
        self.assertEqual(medium["opacity"], "/art/PolyHaven/grass_medium_01/textures/grass_medium_01_alpha_2k.png")
        self.assertEqual(set(medium), {"albedo", "albedo_dry", "normal", "roughness", "ao", "opacity"})


    def test_underlay_has_colour_normal_and_roughness(self):
        maps = underlay_maps("/art/PolyHaven/leafy_grass/maps")
        self.assertEqual(maps["albedo"], "/art/PolyHaven/leafy_grass/maps/leafy_grass_diff_2k.png")
        self.assertEqual(set(maps), {"albedo", "normal", "roughness"})


if __name__ == "__main__":
    unittest.main()
