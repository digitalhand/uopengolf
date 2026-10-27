# SPDX-License-Identifier: MIT
import unittest

from export_rules import EXPORT_LIMITS, export_size, faces_with_materials, farthest_points, is_colour, principled_roles


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


if __name__ == "__main__":
    unittest.main()
