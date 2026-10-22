# SPDX-License-Identifier: MIT
import math
import unittest

from ne_rules import ASSETS, EXPORT_LIMITS, LITTER, export_size, image_roles, litter_layout, texture_filename


class TextureNameTests(unittest.TestCase):
    def test_blender_copy_suffixes_are_removed(self):
        self.assertEqual(texture_filename("CreepingBentgrass_COLOR.png.001", "PNG"), "CreepingBentgrass_COLOR.png")
        self.assertEqual(texture_filename("Rock3LNormal.001", "PNG"), "Rock3LNormal.png")
        self.assertEqual(texture_filename("Rock_3_L_AO.jpg.001", "JPEG"), "Rock_3_L_AO.jpg")
        self.assertEqual(texture_filename("Rock 1 Normal.001", "PNG"), "Rock_1_Normal.png")


class ExportSizeTests(unittest.TestCase):
    def test_large_textures_shrink_to_the_limit_keeping_aspect(self):
        self.assertEqual(export_size(8192, 8192), (2048, 2048))
        self.assertEqual(export_size(1024, 2048), (1024, 2048))
        self.assertEqual(export_size(4096, 2048), (2048, 1024))

    def test_mask_maps_get_a_smaller_limit(self):
        self.assertEqual(export_size(4096, 4096, EXPORT_LIMITS["roughness"]), (1024, 1024))
        self.assertEqual(EXPORT_LIMITS["albedo"], 2048)
        self.assertEqual(EXPORT_LIMITS["opacity_alpha"], EXPORT_LIMITS["albedo"])

    def test_small_textures_are_kept(self):
        self.assertEqual(export_size(512, 512), (512, 512))
        self.assertEqual(export_size(2048, 2048), (2048, 2048))


class RoleTests(unittest.TestCase):
    def test_principled_inputs(self):
        self.assertEqual(image_roles(["Color->ShaderNodeBsdfPrincipled:Principled BSDF.Roughness"]), {"roughness"})
        self.assertEqual(image_roles(["Color->ShaderNodeNormalMap:Normal Map.Color"]), {"normal"})
        self.assertEqual(image_roles(["Color->ShaderNodeBsdfPrincipled:Principled BSDF.Base Color"]), {"albedo"})

    def test_ao_multiply_and_ramp(self):
        self.assertEqual(image_roles(["Color->ShaderNodeMix:ao_strength.A"]), {"albedo"})
        self.assertEqual(image_roles(["Color->ShaderNodeMix:ao_strength.B"]), {"ao"})
        self.assertEqual(image_roles(["Color->ShaderNodeMix:Mix.B"]), {"ao"})
        self.assertEqual(image_roles(["Color->ShaderNodeValToRGB:ColorRamp.001.Factor"]), {"roughness"})

    def test_grass_colour_carries_opacity_in_alpha(self):
        roles = image_roles(["Color->ShaderNodeMix:Mix.A", "Color->ShaderNodeHueSaturation:Hue Saturation Value.Color",
                             "Alpha->ShaderNodeMixShader:Mix Shader.Factor"])
        self.assertEqual(roles, {"albedo", "opacity_alpha"})

    def test_separate_opacity_and_unused_maps(self):
        self.assertEqual(image_roles(["Color->ShaderNodeMixShader:Mix Shader.Factor"]), {"opacity"})
        self.assertEqual(image_roles(["Color->ShaderNodeBsdfTranslucent:Translucent BSDF.Color"]), set())
        self.assertEqual(image_roles(["Color->ShaderNodeDisplacement:displacement.001.Height"]), set())


class AssetTableTests(unittest.TestCase):
    def test_names_are_unique_and_prefixed(self):
        names = [asset["name"] for asset in ASSETS] + [f"SM_NE_Litter_{suffix}" for suffix in LITTER["variants"]]
        self.assertEqual(len(names), len(set(names)))
        self.assertTrue(all(name.startswith("SM_NE_") for name in names))

    def test_every_asset_has_a_source_and_material(self):
        for asset in ASSETS:
            self.assertTrue(asset["collection"].endswith((" High", " Low")), asset)
            self.assertTrue(asset["material"].startswith("NE_"), asset)
            self.assertIn(asset["kind"], {"grass", "card", "rock"})
            self.assertGreaterEqual(asset["max_tris"], 200)

    def test_triangle_budget_ratio(self):
        from ne_rules import decimate_ratio
        self.assertEqual(decimate_ratio(1311, 4000), 1.0)
        self.assertAlmostEqual(decimate_ratio(16304, 4000), 4000 / 16304)
        self.assertEqual(decimate_ratio(0, 4000), 1.0)


class LitterTests(unittest.TestCase):
    def test_layout_repeats_for_a_seed(self):
        self.assertEqual(litter_layout(4), litter_layout(4))
        self.assertNotEqual(litter_layout(4), litter_layout(5))

    def test_pieces_stay_inside_the_patch_and_lie_flat(self):
        pieces = litter_layout(11)
        radius = LITTER["radius_m"]
        counts = {}
        for kind, variant, x, y, z, yaw, tilt in pieces:
            counts[kind] = counts.get(kind, 0) + 1
            self.assertLessEqual(math.hypot(x, y), radius + 1e-9)
            self.assertGreaterEqual(z, 0)
            self.assertLess(z, 0.03)
            self.assertLessEqual(abs(tilt), math.radians(LITTER["max_tilt_deg"]) + 1e-9)
            self.assertGreaterEqual(variant, 0)
        self.assertEqual(counts, {kind: LITTER["counts"][kind] for kind in LITTER["counts"]})

    def test_patch_is_denser_at_its_centre(self):
        pieces = litter_layout(3)
        radius = LITTER["radius_m"]
        inner = sum(1 for p in pieces if math.hypot(p[2], p[3]) < radius / 2)
        # A uniform disc puts a quarter of the pieces inside half the radius.
        self.assertGreater(inner / len(pieces), 0.35)


if __name__ == "__main__":
    unittest.main()
