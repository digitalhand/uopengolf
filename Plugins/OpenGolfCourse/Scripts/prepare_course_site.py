# SPDX-License-Identifier: MIT
"""Build an offline terrain grid, aerial reference and geographic layout from public sources.

    python3 prepare_course_site.py <site recipe.json> <source folder>

Downloads are cached in the source folder, so a second run works offline. The recipe names the
elevation tile, the OpenStreetMap extract, the course boundary way, the scorecard and optionally an
aerial image service and an author corrections file (relative to the recipe).
"""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import shutil
from tempfile import TemporaryDirectory
from urllib.parse import urlencode
from urllib.request import urlopen
import xml.etree.ElementTree as ET

from osgeo import gdal

import course_corrections
from prepare_course_elevation import prepare
from course_layout_draft import WATER_KINDS, draft, rings_from_ways
from ground_grid import GroundGrid

WATER_TAGS = {"natural": ("water",), "water": ("pond", "lake", "reservoir"), "golf": WATER_KINDS}
AERIAL_MAX_PIXELS = 4000


def cache(url, path):
    if path.is_file():
        return
    with TemporaryDirectory(dir=path.parent) as scratch:
        temp = Path(scratch) / "download"
        with urlopen(url, timeout=180) as source, temp.open("wb") as target:
            shutil.copyfileobj(source, target)
        temp.replace(path)


def feature_kind(tags):
    """The layout kind for a map feature's tags, or None to ignore it."""
    if tags.get("golf") and tags["golf"] not in WATER_KINDS:
        return tags["golf"]
    if any(tags.get(key) in values for key, values in WATER_TAGS.items()):
        return "water"
    return None


def _feature(fid, kind, tags, coords, closed):
    return {"type": "Feature", "id": fid, "properties": {"kind": kind, **tags},
            "geometry": {"type": "Polygon" if closed else "LineString", "coordinates": [coords + coords[:1]] if closed else coords}}


def layout(xml, config):
    """GeoJSON features for golf ways, golf multipolygons and water, plus hole numbers needing route review."""
    root = ET.parse(xml)
    nodes = {n.get("id"): [float(n.get("lon")), float(n.get("lat"))] for n in root.iter("node")}
    ways = {w.get("id"): w for w in root.iter("way")}
    features = []
    routes = defaultdict(list)
    for way in ways.values():
        tags = {t.get("k"): t.get("v") for t in way.findall("tag")}
        kind = "course_boundary" if way.get("id") == config["boundary_way"] else feature_kind(tags)
        if not kind:
            continue
        coords = [nodes[n.get("ref")] for n in way.findall("nd")]
        closed = len(coords) >= 4 and coords[0] == coords[-1] and kind not in ("hole", "cartpath")
        features.append(_feature("way/" + way.get("id"), kind, tags, coords[:-1] if closed else coords, closed))
        if kind == "hole":
            routes[tags.get("ref")].append("way/" + way.get("id"))
    for relation in root.iter("relation"):
        tags = {t.get("k"): t.get("v") for t in relation.findall("tag")}
        kind = feature_kind(tags)
        if tags.get("type") != "multipolygon" or not kind:
            continue
        outer = [[nodes[n.get("ref")] for n in ways[m.get("ref")].findall("nd") if n.get("ref") in nodes]
                 for m in relation.findall("member")
                 if m.get("type") == "way" and m.get("role") == "outer" and m.get("ref") in ways]
        for index, ring in enumerate(rings_from_ways(outer)):
            suffix = "" if index == 0 else f"#{index + 1}"
            features.append(_feature(f"relation/{relation.get('id')}{suffix}", kind, tags, ring, True))
    review = {str(i): routes[str(i)] for i in range(1, config["hole_count"] + 1) if len(routes[str(i)]) != 1}
    return {"type": "FeatureCollection", "attribution": "© OpenStreetMap contributors, ODbL 1.0",
            "license": "https://www.openstreetmap.org/copyright", "features": features}, review


def aerial_request(config):
    """The aerial image URL and its georeference: north-up, covering the site's sample bounds."""
    aerial = config["aerial"]
    west, south, east, north = config["sample_bounds_m"]
    half = config["spacing_m"] / 2
    bounds = [west - half, south - half, east + half, north + half]
    pixels = min(AERIAL_MAX_PIXELS, math.ceil((bounds[2] - bounds[0]) / aerial["pixel_m"]))
    epsg = config["crs"].split(":")[-1]
    query = urlencode({"bbox": ",".join(f"{v:.1f}" for v in bounds), "bboxSR": epsg, "imageSR": epsg,
                       "size": f"{pixels},{pixels}", "format": "jpg", "f": "image"})
    return f"{aerial['service']}?{query}", {"bounds_m": bounds, "size": [pixels, pixels], "crs": config["crs"],
                                           "source": aerial["source"], "license": aerial["license"]}


def recipe_record(site_path, config):
    """The recipe as used, with the corrections file resolved, for tools that only see the source folder."""
    record = dict(config)
    if config.get("corrections"):
        record["corrections"] = str((site_path.parent / config["corrections"]).resolve())
    return record


def build(site_path, config, folder):
    folder.mkdir(parents=True, exist_ok=True)
    dem = folder / config["elevation_url"].rsplit("/", 1)[-1]
    xml = folder / "source.osm"
    cache(config["elevation_url"], dem)
    cache(config["osm_url"], xml)
    if config.get("aerial"):
        url, georeference = aerial_request(config)
        sidecar = folder / "aerial.json"
        # A cached image taken for other bounds or size would be placed wrongly; fetch it again.
        if not sidecar.is_file() or json.loads(sidecar.read_text()) != georeference:
            (folder / "aerial.jpg").unlink(missing_ok=True)
        cache(url, folder / "aerial.jpg")
        (folder / "aerial.json").write_text(json.dumps(georeference, indent=2), encoding="utf-8")
    features, review = layout(xml, config)
    with TemporaryDirectory(dir=folder) as workspace:
        stage = Path(workspace)
        west, south, east, north = config["sample_bounds_m"]
        spacing = config["spacing_m"]
        half = spacing / 2
        raster = gdal.Warp(str(stage / "ground.tif"), str(dem), dstSRS=config["crs"],
                           outputBounds=(west-half, south-half, east+half, north+half),
                           xRes=spacing, yRes=spacing, resampleAlg="bilinear", dstNodata=-999999)
        raster.FlushCache()
        raster = None
        prepare(stage / "ground.tif", stage / "ground.oge", config["origin_m"])
        ground = GroundGrid.open(stage / "ground.tif", config["origin_m"])
        recipe = recipe_record(site_path, config)
        result = draft(config, features, ground.slope, course_corrections.read(recipe))
        (stage / "source.geojson").write_text(json.dumps(features), encoding="utf-8")
        (stage / "source_layout.json").write_text(json.dumps(result), encoding="utf-8")
        (stage / "route_review.json").write_text(json.dumps(review, indent=2), encoding="utf-8")
        (stage / "recipe.json").write_text(json.dumps(recipe, indent=2), encoding="utf-8")
        for path in stage.iterdir():
            path.replace(folder / path.name)
    flagged = [h["number"] for h in result["holes"] if h["review"]]
    print(f'{len(features["features"])} features, {len(result["regions"])} surfaces, {len(result["water"])} lakes. '
          f'Holes with review notes: {flagged}. Joined split routes: {list(review)}')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("site", type=Path)
    parser.add_argument("cache", type=Path, help="Dedicated source folder for this site")
    args = parser.parse_args()
    gdal.UseExceptions()
    build(args.site.resolve(), json.loads(args.site.read_text()), args.cache)
