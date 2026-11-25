# SPDX-License-Identifier: MIT
import unittest

from export_rules import (EXPORT_LIMITS, chromatic_tint, export_size, faces_with_materials, farthest_points, fit_saturation,
                          is_colour, luminance, principled_roles)


class ExportSizeTests(unittest.TestCase):
    def test_colour_and_normal_maps_stop_at_2k(self):
        self.assertEqual(export_size(8192, 4096, EXPORT_LIMITS["albedo"]), (2048, 1024))
        self.assertEqual(export_size(2048, 2048, EXPORT_LIMITS["normal"]), (2048, 2048))

    def test_masks_stop_at_1k(self):
        self.assertEqual(export_size(2048, 2048, EXPORT_LIMITS["roughness"]), (1024, 1024))
        self.assertEqual(export_size(2048, 2048, EXPORT_LIMITS["opacity"]), (1024, 1024))

    def test_small_maps_are_kept(self):
        self.assertEqual(export_size(512, 256, EXPORT_LIMITS["ao"]), (512, 256))



class ColourRoleTests(unittest.TestCase):
    def test_colour_maps_are_written_as_srgb(self):
        for role in ("albedo", "albedo_dry", "opacity_alpha"):
            self.assertTrue(is_colour(role), role)

    def test_data_maps_are_written_raw(self):
        for role in ("normal", "roughness", "ao", "opacity"):
            self.assertFalse(is_colour(role), role)


class PrincipledRoleTests(unittest.TestCase):
    def test_poly_haven_graph(self):
        self.assertEqual(principled_roles(["Color->ShaderNodeBsdfPrincipled.Base Color"]), {"albedo"})
        self.assertEqual(principled_roles(["Color->ShaderNodeNormalMap.Color"]), {"normal"})
        self.assertEqual(principled_roles(["Color->ShaderNodeBsdfPrincipled.Roughness"]), {"roughness"})
        self.assertEqual(principled_roles(["Alpha->ShaderNodeBsdfPrincipled.Alpha"]), {"opacity"})
        self.assertEqual(principled_roles(["Color->ShaderNodeBsdfPrincipled.Alpha"]), {"opacity"})

    def test_poly_haven_leaf_group_sockets(self):
        self.assertEqual(principled_roles(["Color->ShaderNodeGroup.Diffuse"]), {"albedo"})
        self.assertEqual(principled_roles(["Color->ShaderNodeGroup.Rough"]), {"roughness"})
        self.assertEqual(principled_roles(["Color->ShaderNodeGroup.Normal"]), {"normal"})
        self.assertEqual(principled_roles(["Color->ShaderNodeGroup.Alpha"]), {"opacity"})

    def test_unused_links_have_no_role(self):
        self.assertEqual(principled_roles(["Color->ShaderNodeDisplacement.Height"]), set())


class TreeHelperTests(unittest.TestCase):
    def test_collision_keeps_only_wood_faces(self):
        faces = ["trunk", "leaves", "branches", "trunk"]
        self.assertEqual(faces_with_materials(faces, {"trunk"}), [0, 3])
        self.assertEqual(faces_with_materials(faces, {"bark"}), [])

    def test_canopy_points_are_spread_and_deterministic(self):
        grid = [(x, y, z) for x in range(5) for y in range(5) for z in range(5)]
        picked = farthest_points(grid, 8)
        self.assertEqual(picked, farthest_points(list(reversed(grid)), 8))
        self.assertEqual(len(set(picked)), 8)
        self.assertEqual(picked[:2], [(4, 4, 4), (0, 0, 0)])

    def test_few_points_are_kept(self):
        self.assertEqual(farthest_points([(0, 0, 0), (1, 0, 0)], 64), [(0, 0, 0), (1, 0, 0)])


class ChromaticTintTests(unittest.TestCase):
    def tinted(self, colour, tint):
        return [c * k for c, k in zip(colour, tint)]

    def test_same_hue_needs_no_tint(self):
        self.assertEqual(chromatic_tint((0.2, 0.4, 0.1), (0.1, 0.2, 0.05)), (1.0, 1.0, 1.0))

    def test_tint_gives_the_target_hue_at_the_source_brightness(self):
        leaf, target = (0.20, 0.30, 0.10), (0.12, 0.18, 0.09)  # within the clamp
        result = self.tinted(leaf, chromatic_tint(leaf, target))
        self.assertAlmostEqual(luminance(result), luminance(leaf), places=3)
        for got, want in zip(result, target):
            self.assertAlmostEqual(got / luminance(result), want / luminance(target), places=2)

    def test_extreme_shifts_are_clamped_and_keep_the_brightness(self):
        leaf = (0.30, 0.30, 0.01)
        tint = chromatic_tint(leaf, (0.1, 0.1, 0.4), low=0.5, high=1.6)
        self.assertLessEqual(max(tint) / min(tint), 1.6 / 0.5 + 1e-6)
        self.assertAlmostEqual(luminance(self.tinted(leaf, tint)), luminance(leaf), places=3)

    def test_black_or_missing_channels_are_refused(self):
        with self.assertRaises(ValueError):
            chromatic_tint((0.0, 0.0, 0.0), (0.1, 0.2, 0.1))
        with self.assertRaises(ValueError):
            chromatic_tint((0.2, 0.3, 0.0), (0.1, 0.2, 0.1))


class SaturationFitTests(unittest.TestCase):
    def chroma(self, rgb):
        return [c / luminance(rgb) for c in rgb]

    def desaturated(self, colour, saturation):
        luma = luminance(colour)
        return [luma + saturation * (c - luma) for c in colour]

    def test_a_tint_that_reaches_the_target_keeps_full_saturation(self):
        leaf, target = (0.20, 0.30, 0.10), (0.12, 0.18, 0.09)
        self.assertEqual(fit_saturation(leaf, chromatic_tint(leaf, target), target), 1.0)

    def test_a_clamped_tint_is_completed_by_desaturating(self):
        leaf, target = (0.1191, 0.1374, 0.0174), (0.0939, 0.0983, 0.0773)
        tint = chromatic_tint(leaf, target, 0.5, 3.5)
        tinted = [c * k for c, k in zip(leaf, tint)]
        saturation = fit_saturation(leaf, tint, target)
        self.assertTrue(0.2 < saturation < 0.6, saturation)
        for got, want in zip(self.chroma(self.desaturated(tinted, saturation)), self.chroma(target)):
            self.assertAlmostEqual(got, want, delta=0.06)

    def test_a_grey_target_removes_the_colour(self):
        self.assertEqual(fit_saturation((0.2, 0.3, 0.1), (1.0, 1.0, 1.0), (0.2, 0.2, 0.2)), 0.0)


if __name__ == "__main__":
    unittest.main()
