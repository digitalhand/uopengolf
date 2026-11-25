# SPDX-License-Identifier: MIT
import io
from pathlib import Path
import unittest
from urllib.parse import parse_qs, urlparse

try:
    from osgeo import gdal
except ImportError:
    gdal = None

if gdal:
    from prepare_course_site import aerial_request, feature_kind, layout, recipe_record

OSM = """<osm>
  <node id="1" lon="0" lat="0"/><node id="2" lon="1" lat="0"/><node id="3" lon="1" lat="1"/><node id="4" lon="0" lat="1"/>
  <node id="5" lon="0.2" lat="0.2"/><node id="6" lon="0.4" lat="0.2"/><node id="7" lon="0.4" lat="0.4"/>
  <way id="10"><nd ref="1"/><nd ref="2"/><nd ref="3"/></way>
  <way id="11"><nd ref="1"/><nd ref="4"/><nd ref="3"/></way>
  <way id="12"><nd ref="5"/><nd ref="6"/><nd ref="7"/><nd ref="5"/><tag k="golf" v="green"/></way>
  <way id="13"><nd ref="5"/><nd ref="6"/><nd ref="7"/><nd ref="5"/><tag k="natural" v="water"/><tag k="water" v="pond"/></way>
  <way id="14"><nd ref="1"/><nd ref="5"/><tag k="golf" v="hole"/><tag k="ref" v="1"/></way>
  <way id="15"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/><tag k="leisure" v="golf_course"/></way>
  <relation id="20"><member type="way" ref="10" role="outer"/><member type="way" ref="11" role="outer"/>
    <member type="way" ref="12" role="inner"/><tag k="type" v="multipolygon"/><tag k="golf" v="fairway"/></relation>
  <relation id="21"><member type="way" ref="99" role="outer"/><tag k="type" v="route"/></relation>
</osm>"""
CONFIG = {"boundary_way": "15", "hole_count": 2}


@unittest.skipUnless(gdal, "GDAL Python bindings are not installed")
class LayoutTests(unittest.TestCase):
    def setUp(self):
        self.collection, self.review = layout(io.StringIO(OSM), CONFIG)
        self.by_id = {f["id"]: f for f in self.collection["features"]}

    def test_multipolygon_fairways_come_from_their_outer_ways(self):
        fairway = self.by_id["relation/20"]
        self.assertEqual(fairway["properties"]["kind"], "fairway")
        ring = fairway["geometry"]["coordinates"][0]
        self.assertEqual(ring[0], ring[-1])
        self.assertEqual(len(ring), 5)

    def test_ponds_are_water_and_the_boundary_is_marked(self):
        self.assertEqual(self.by_id["way/13"]["properties"]["kind"], "water")
        self.assertEqual(self.by_id["way/15"]["properties"]["kind"], "course_boundary")
        self.assertEqual(self.by_id["way/14"]["geometry"]["type"], "LineString")

    def test_untagged_member_ways_and_other_relations_are_not_features(self):
        self.assertNotIn("way/10", self.by_id)
        self.assertNotIn("relation/21", self.by_id)

    def test_holes_without_exactly_one_route_are_listed(self):
        self.assertEqual(self.review, {"2": []})

    def test_feature_kinds(self):
        self.assertEqual(feature_kind({"golf": "bunker", "natural": "sand"}), "bunker")
        self.assertEqual(feature_kind({"golf": "water_hazard"}), "water")
        self.assertEqual(feature_kind({"landuse": "grass"}), None)


@unittest.skipUnless(gdal, "GDAL Python bindings are not installed")
class AerialTests(unittest.TestCase):
    CONFIG = {"sample_bounds_m": [1000, 2000, 3400, 4400], "spacing_m": 1, "crs": "EPSG:26910",
              "aerial": {"service": "https://example.org/ImageServer/exportImage", "pixel_m": 0.6,
                         "source": "NAIP", "license": "public domain"}}

    def test_the_request_covers_the_sample_cells_at_native_resolution(self):
        url, georeference = aerial_request(self.CONFIG)
        query = parse_qs(urlparse(url).query)
        self.assertEqual(query["bbox"], ["999.5,1999.5,3400.5,4400.5"])
        self.assertEqual(query["size"], ["4000,4000"])
        self.assertEqual(query["bboxSR"], ["26910"])
        self.assertEqual(georeference["bounds_m"], [999.5, 1999.5, 3400.5, 4400.5])

    def test_the_recipe_record_resolves_corrections_next_to_the_recipe(self):
        record = recipe_record(Path("/data/sites/Course.json"), {"corrections": "Course/corrections.json"})
        self.assertEqual(record["corrections"], "/data/sites/Course/corrections.json")


if __name__ == "__main__":
    unittest.main()
