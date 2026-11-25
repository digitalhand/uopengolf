# SPDX-License-Identifier: MIT
import unittest

try:
    from osgeo import ogr
    import PIL  # noqa: F401
except ImportError:  # The review needs GDAL's Python bindings and Pillow.
    ogr = None

if ogr:
    from review_course_layout import hole_checks


def box(x0, y0, x1, y1):
    return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]]


class FlatGround:
    @staticmethod
    def slope(_x, _y):
        return 0.0


@unittest.skipUnless(ogr, "GDAL Python bindings and Pillow are not installed")
class HoleCheckTests(unittest.TestCase):
    LAYOUT = {"boundary_m": box(-100, -100, 100, 400), "water": [], "regions": [
        {"id": "way/1", "surface": "fairway", "outline_m": box(-20, -10, 20, 300)},
        {"id": "tee/1/Blue", "surface": "tee", "outline_m": box(-3, -5, 3, 5)},
        {"id": "way/2", "surface": "green", "outline_m": box(-15, 270, 15, 300)}]}

    def hole(self, tees):
        return {"number": 1, "cup_m": [0, 285], "tees_m": tees, "published_yards": {"Blue": 312, "Red": 280},
                "routing_m": [[0, 0], [0, 285]], "review": []}

    def test_a_hole_with_every_tee_on_a_pad_passes(self):
        self.assertEqual(hole_checks(self.LAYOUT, self.hole({"Blue": [0, 0]}), FlatGround), [])

    def test_a_tee_off_its_pad_is_reported(self):
        problems = hole_checks(self.LAYOUT, self.hole({"Blue": [0, 0], "Red": [0, 40]}), FlatGround)
        self.assertEqual(problems, ["Red tee is on fairway, not a tee pad"])


if __name__ == "__main__":
    unittest.main()
