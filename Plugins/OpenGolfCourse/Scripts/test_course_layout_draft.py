# SPDX-License-Identifier: MIT
import math
import unittest

try:
    from osgeo import ogr, osr
except ImportError:  # The layout tools need GDAL's Python bindings; Unreal's Python does not have them.
    ogr = None

if ogr:
    from course_layout_draft import (PLAY_AREA_CLEARANCE_M, PLAY_AREA_MARGIN_M, choose_cup, clean_regions, draft,
                                     join_route, level_tees, place_tees, rings_from_ways, route_length_m, tee_pads)


def box(x0, y0, x1, y1):
    return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]]


def polygon(points):
    ring = ogr.Geometry(ogr.wkbLinearRing)
    for x, y in [*points, points[0]]:
        ring.AddPoint_2D(x, y)
    shape = ogr.Geometry(ogr.wkbPolygon)
    shape.AddGeometry(ring)
    return shape


def boundary_gap(a, b):
    return polygon(a["outline_m"]).Boundary().Distance(polygon(b["outline_m"]).Boundary())


def flat(_x, _y):
    return 0.0


@unittest.skipUnless(ogr, "GDAL Python bindings are not installed")
class RouteTests(unittest.TestCase):
    def test_sections_join_at_shared_nodes(self):
        self.assertEqual(join_route([[[2, 0], [3, 0]], [[0, 0], [1, 0], [2, 0]]]), [[0, 0], [1, 0], [2, 0], [3, 0]])

    def test_branches_need_a_designer(self):
        self.assertEqual(join_route([[[0, 0], [1, 0]], [[1, 0], [2, 0]], [[1, 0], [1, 1]]]), [])

    def test_length_follows_the_route(self):
        self.assertAlmostEqual(route_length_m([[0, 0], [3, 4], [3, 10]]), 11.0)


@unittest.skipUnless(ogr, "GDAL Python bindings are not installed")
class RingTests(unittest.TestCase):
    def test_outer_ways_join_into_closed_rings_in_either_direction(self):
        rings = rings_from_ways([[[0, 0], [10, 0], [10, 10]], [[0, 0], [0, 10], [10, 10]]])
        self.assertEqual(len(rings), 1)
        self.assertEqual(sorted(map(tuple, rings[0])), [(0, 0), (0, 10), (10, 0), (10, 10)])

    def test_a_single_closed_way_is_one_ring(self):
        self.assertEqual(rings_from_ways([[[0, 0], [1, 0], [0, 1], [0, 0]]]), [[[0, 0], [1, 0], [0, 1]]])

    def test_an_open_ring_is_dropped(self):
        self.assertEqual(rings_from_ways([[[0, 0], [1, 0], [2, 2]]]), [])


@unittest.skipUnless(ogr, "GDAL Python bindings are not installed")
class CleanRegionTests(unittest.TestCase):
    def regions(self, *items):
        return [{"id": f"way/{i}", "surface": surface, "outline_m": outline} for i, (surface, outline) in enumerate(items)]

    def assert_separate(self, cleaned):
        for i, a in enumerate(cleaned):
            for b in cleaned[i + 1:]:
                self.assertFalse(polygon(a["outline_m"]).Boundary().Intersects(polygon(b["outline_m"]).Boundary()), (a["id"], b["id"]))

    def test_a_fairway_grows_a_collar_around_a_green_that_crosses_it(self):
        cleaned = clean_regions(self.regions(("fairway", box(0, -15, 100, 15)), ("green", box(90, -10, 115, 10))), smooth_m=None)
        fairway, green = cleaned
        self.assertEqual((fairway["surface"], green["surface"]), ("fairway", "green"))
        self.assertTrue(polygon(fairway["outline_m"]).Contains(polygon(green["outline_m"])))
        self.assertGreater(boundary_gap(fairway, green), 0.4)
        self.assert_separate(cleaned)

    def test_a_bunker_on_the_fairway_edge_cuts_the_fairway(self):
        cleaned = clean_regions(self.regions(("fairway", box(0, -15, 100, 15)), ("bunker", box(40, 10, 50, 20))), smooth_m=None)
        fairway, bunker = cleaned
        self.assertAlmostEqual(polygon(bunker["outline_m"]).GetArea(), 100, delta=0.5)
        self.assertFalse(polygon(fairway["outline_m"]).Intersects(polygon(bunker["outline_m"])))
        self.assert_separate(cleaned)

    def test_a_bunker_that_overlaps_a_green_gives_way(self):
        cleaned = clean_regions(self.regions(("green", box(0, 0, 30, 30)), ("bunker", box(25, 10, 40, 20))), smooth_m=None)
        green, bunker = cleaned
        self.assertEqual(green["outline_m"], box(0, 0, 30, 30))
        self.assertGreater(boundary_gap(green, bunker), 1.0)

    def test_overlapping_outlines_of_one_surface_merge(self):
        cleaned = clean_regions(self.regions(("fairway", box(0, 0, 50, 20)), ("fairway", box(40, 0, 90, 20))), smooth_m=None)
        self.assertEqual(len(cleaned), 1)
        self.assertAlmostEqual(polygon(cleaned[0]["outline_m"]).GetArea(), 1800, delta=1)

    def test_growing_around_one_tee_that_reaches_the_next_is_settled(self):
        cleaned = clean_regions(self.regions(("fairway", box(0, -10, 100, 10)), ("tee", box(-3, -3, 1, 3)),
                                             ("tee", box(-7.4, -3, -3.6, 3))), smooth_m=None)
        self.assertEqual([r["surface"] for r in cleaned], ["fairway", "tee", "tee"])
        self.assert_separate(cleaned)

    def test_slivers_are_dropped(self):
        cleaned = clean_regions(self.regions(("green", box(0, 0, 30, 30)), ("bunker", box(29, 0, 31.2, 1))), smooth_m=None)
        self.assertEqual([r["surface"] for r in cleaned], ["green"])

    def test_regions_come_out_in_drawing_order_with_stable_ids(self):
        cleaned = clean_regions(self.regions(("bunker", box(200, 0, 210, 10)), ("green", box(100, 0, 130, 30)),
                                             ("tee", box(0, 0, 5, 10)), ("fairway", box(20, 0, 80, 30))), smooth_m=None)
        self.assertEqual([r["surface"] for r in cleaned], ["fairway", "tee", "green", "bunker"])
        self.assertEqual([r["id"] for r in cleaned], ["way/3", "way/2", "way/1", "way/0"])

    def test_smoothing_keeps_every_outline_separate(self):
        cleaned = clean_regions(self.regions(("fairway", box(0, -15, 100, 15)), ("green", box(90, -10, 115, 10)),
                                             ("bunker", box(60, 12, 70, 22))), smooth_m=1.0)
        self.assertEqual(len(cleaned), 3)
        self.assert_separate(cleaned)


@unittest.skipUnless(ogr, "GDAL Python bindings are not installed")
class CupTests(unittest.TestCase):
    GREEN = box(0, 0, 30, 30)

    def test_the_cup_stays_clear_of_the_edge_and_slopes(self):
        def slope(x, _y):
            return 0.08 if x < 15 else 0.01
        cup, note = choose_cup(self.GREEN, [10, 15], slope)
        self.assertIsNone(note)
        self.assertGreaterEqual(cup[0], 15)
        self.assertGreaterEqual(polygon(self.GREEN).Boundary().Distance(ogr.CreateGeometryFromWkt(f"POINT ({cup[0]} {cup[1]})")), 4)

    def test_a_steep_green_falls_back_and_asks_for_review(self):
        cup, note = choose_cup(self.GREEN, [15, 15], lambda _x, _y: 0.06)
        self.assertEqual(cup, [15.0, 15.0])
        self.assertIn("slope", note)

    def test_without_a_green_the_route_end_is_used(self):
        cup, note = choose_cup(None, [5, 5], flat)
        self.assertEqual(cup, [5, 5])
        self.assertIn("green", note)


@unittest.skipUnless(ogr, "GDAL Python bindings are not installed")
class TeeTests(unittest.TestCase):
    ROUTE = [[0, 0], [0, 200], [100, 200]]

    def test_forward_tees_move_along_the_route_by_their_yardage(self):
        tees = place_tees(self.ROUTE, {"Blue": 300, "Red": 250})
        self.assertEqual(tees["Blue"], [0, 0])
        self.assertAlmostEqual(tees["Red"][1], 50 * 0.9144, places=3)

    def test_a_tee_on_a_bank_moves_to_the_nearest_level_ground(self):
        def slope(x, y):  # Level terraces at y 0-12 and y 40-55 beside the route; banks elsewhere.
            return 0.01 if (-4 <= x <= 4 and (0 <= y <= 12 or 40 <= y <= 55)) else 0.2
        tees, notes = level_tees(self.ROUTE, {"Blue": 300, "Red": 270}, {"Blue": [0, 2], "Red": [0, 27.4]}, slope)
        self.assertEqual(tees["Blue"], [0, 2])
        self.assertEqual(notes, [])
        self.assertTrue(40 <= tees["Red"][1] <= 43 or 9 <= tees["Red"][1] <= 12, tees["Red"])

    def test_without_level_ground_nearby_the_tee_stays_and_is_noted(self):
        tees, notes = level_tees(self.ROUTE, {"Blue": 300}, {"Blue": [0, 0]}, lambda _x, _y: 0.3)
        self.assertEqual(tees["Blue"], [0, 0])
        self.assertIn("Blue tee", notes[0])

    def test_tee_pads_line_up_with_the_route_and_merge_when_they_overlap(self):
        pads = tee_pads(7, self.ROUTE, {"Blue": [0, 0], "White": [0, 4], "Red": [0, 60]})
        self.assertEqual(len(pads), 2)
        self.assertTrue(all(p["surface"] == "tee" and p["id"].startswith("tee/7/") for p in pads))
        long_pad = polygon(pads[0]["outline_m"]).GetEnvelope()
        self.assertGreater(long_pad[3] - long_pad[2], long_pad[1] - long_pad[0])


@unittest.skipUnless(ogr, "GDAL Python bindings are not installed")
class DraftTests(unittest.TestCase):
    """One mapped hole, 290 m from the back tee to the green, with a course boundary around it."""
    YARDS = {"Blue": [300], "Red": [270]}

    def setUp(self):
        wgs, utm = osr.SpatialReference(), osr.SpatialReference()
        wgs.ImportFromEPSG(4326)
        utm.ImportFromEPSG(32610)
        for reference in (wgs, utm):
            reference.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
        east, north, _ = osr.CoordinateTransformation(wgs, utm).TransformPoint(-120.7, 35.6)
        back = osr.CoordinateTransformation(utm, wgs)

        def lonlat(points):
            return [list(back.TransformPoint(east + x, north + y)[:2]) for x, y in points]

        def area(fid, kind, points):
            return {"id": fid, "properties": {"kind": kind}, "geometry": {"type": "Polygon", "coordinates": [lonlat([*points, points[0]])]}}
        self.area = area
        self.config = {"name": "Test", "crs": "EPSG:32610", "origin_m": [east, north, 0], "hole_count": 1, "par": [4],
                       "tee_yards": self.YARDS}
        self.collection = {"attribution": "test", "license": "test", "features": [
            area("way/1", "course_boundary", box(-100, -50, 100, 400)),
            area("way/2", "green", box(-15, 270, 15, 300)),
            {"id": "way/3", "properties": {"kind": "hole", "ref": "1"},
             "geometry": {"type": "LineString", "coordinates": lonlat([[0, 0], [0, 290]])}}]}

    def tee_regions(self, layout, point):
        return [r for r in layout["regions"] if r["surface"] == "tee" and polygon(r["outline_m"]).Contains(point_at(point))]

    def test_every_tee_set_gets_a_pad(self):
        layout = draft(self.config, self.collection)
        for name, tee in layout["holes"][0]["tees_m"].items():
            self.assertTrue(self.tee_regions(layout, tee), name)

    def test_a_corrected_tee_gets_its_pad_where_the_author_placed_it(self):
        mapped = draft(self.config, self.collection)["holes"][0]["tees_m"]["Red"]
        layout = draft(self.config, self.collection, corrections={"holes": {"1": {"tees_m": {"Red": [20, 60]}}}})
        self.assertEqual(layout["holes"][0]["tees_m"]["Red"], mapped, "the layout keeps the map estimate for Save corrections")
        self.assertTrue(self.tee_regions(layout, [20, 60]))
        self.assertFalse(self.tee_regions(layout, mapped))

    def test_the_play_area_holds_the_boundary_with_a_margin_and_no_surface_edge_crosses_it(self):
        self.collection["features"] += [self.area("way/4", "fairway", box(-20, 380, 20, 450)),  # Reaches past the boundary.
                                        self.area("way/5", "fairway", box(600, 0, 640, 40))]    # Far outside it.
        layout = draft(self.config, self.collection)
        play = polygon(layout["play_area_m"])
        self.assertTrue(play.Contains(polygon(box(-100, -50, 100, 400)).Buffer(PLAY_AREA_MARGIN_M - 1)))
        for region in layout["regions"]:
            gap = play.Boundary().Distance(polygon(region["outline_m"]))
            self.assertGreater(gap, PLAY_AREA_CLEARANCE_M - 1, region["id"])
        self.assertTrue(play.Contains(polygon(box(-20, 380, 20, 450))))
        self.assertFalse(play.Intersects(polygon(box(600, 0, 640, 40))))

    def test_corrections_for_a_hole_the_course_lacks_fail(self):
        with self.assertRaises(ValueError):
            draft(self.config, self.collection, corrections={"holes": {"7": {"tees_m": {"Red": [20, 60]}}}})


def point_at(point):
    shape = ogr.Geometry(ogr.wkbPoint)
    shape.AddPoint_2D(*point)
    return shape


if __name__ == "__main__":
    unittest.main()
