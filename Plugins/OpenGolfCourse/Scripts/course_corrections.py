# SPDX-License-Identifier: MIT
"""Author corrections over a course layout prepared from public map data.

The prepared layout (source_layout.json) is rebuilt whenever the map data is fetched again. What an
author fixes by hand lives in a separate corrections file, keyed by hole number and map feature ID, so
a fresh import keeps every fix. Lengths are local metres. This module has no GIS dependency, so the
Unreal editor can use it too.

    {
      "holes": {"12": {"cup_m": [x, y], "tees_m": {"Red": [x, y]}, "aim_m": [[x, y]], "reviewed": "why"}},
      "regions": {"way/123": {"surface": "green", "outline_m": [[x, y], ...]}, "way/456": null}
    }

aim_m replaces the points between the tee and the green. A region set to null is removed; an unknown
ID adds a region. reviewed records that the remaining review notes were checked and accepted.
"""
import copy
import json
import math
from pathlib import Path

SURFACES = ("fairway", "tee", "green", "bunker")
HOLE_POINT_KEYS = ("cup_m", "tees_m", "aim_m")
TOLERANCE_M = 0.05
AUTHOR_NOTE = "corrected by author"


def _point(value, name):
    if not (isinstance(value, (list, tuple)) and len(value) == 2 and all(isinstance(v, (int, float)) and math.isfinite(v) for v in value)):
        raise ValueError(f"{name} must be an [x, y] point in metres, not {value!r}")
    return [float(value[0]), float(value[1])]


def _points(values, name, minimum=0):
    if not isinstance(values, (list, tuple)) or len(values) < minimum:
        raise ValueError(f"{name} needs at least {minimum} points")
    return [_point(v, name) for v in values]


def _region(region_id, value):
    if not isinstance(value, dict) or value.get("surface") not in SURFACES:
        raise ValueError(f"Region {region_id} needs a surface from {SURFACES}")
    return {"id": region_id, "surface": value["surface"], "outline_m": _points(value.get("outline_m"), f"{region_id} outline_m", 3)}


def _corrected_hole(hole, fix):
    result = copy.deepcopy(hole)
    if "cup_m" in fix:
        result["cup_m"] = _point(fix["cup_m"], f"hole {hole['number']} cup_m")
    if "tees_m" in fix:
        if not isinstance(fix["tees_m"], dict):
            raise ValueError(f"hole {hole['number']} tees_m must map tee names to points")
        result["tees_m"] = {**result.get("tees_m", {}),
                            **{name: _point(p, f"hole {hole['number']} tees_m {name}") for name, p in fix["tees_m"].items()}}
    if "aim_m" in fix:
        route = result["routing_m"]
        result["routing_m"] = [route[0], *_points(fix["aim_m"], f"hole {hole['number']} aim_m"), route[-1]]
    review = [] if fix.get("reviewed") else [note for note in result.get("review", []) if note != AUTHOR_NOTE]
    result["review"] = review + [AUTHOR_NOTE]
    return result


def read(recipe):
    """The corrections file the recipe names, or None when it names none or the file is not written yet."""
    path = Path(recipe["corrections"]) if recipe.get("corrections") else None
    return json.loads(path.read_text(encoding="utf-8")) if path and path.is_file() else None


def apply(layout, corrections):
    """A new layout with the corrections applied; the given layout is not changed."""
    result = copy.deepcopy(layout)
    if not corrections:
        return result
    numbers = {str(h["number"]) for h in result["holes"]}
    fixes = corrections.get("holes", {})
    unknown = sorted(set(fixes) - numbers, key=lambda n: (len(n), n))
    if unknown:
        raise ValueError(f"Corrections name hole {', '.join(unknown)}, which the layout does not have")
    result["holes"] = [_corrected_hole(h, fixes[str(h["number"])]) if str(h["number"]) in fixes else h for h in result["holes"]]
    region_fixes = corrections.get("regions", {})
    existing = {r["id"] for r in result["regions"]}
    kept = [r if r["id"] not in region_fixes else _region(r["id"], region_fixes[r["id"]])
            for r in result["regions"] if not (r["id"] in region_fixes and region_fixes[r["id"]] is None)]
    added = [_region(rid, value) for rid, value in region_fixes.items() if rid not in existing and value is not None]
    result["regions"] = kept + added
    return result


def _moved(a, b):
    return math.dist(a, b) > TOLERANCE_M


def _hole_changes(source, placed):
    changes = {}
    if _moved(placed["cup_m"], source["cup_m"]):
        changes["cup_m"] = placed["cup_m"]
    tees = {name: p for name, p in placed["tees_m"].items() if name not in source["tees_m"] or _moved(p, source["tees_m"][name])}
    if tees:
        changes["tees_m"] = tees
    aim = source["routing_m"][1:-1]
    if len(aim) != len(placed["aim_m"]) or any(_moved(a, b) for a, b in zip(aim, placed["aim_m"])):
        changes["aim_m"] = placed["aim_m"]
    return changes


def _outline_changed(a, b):
    return len(a) != len(b) or any(_moved(p, q) for p, q in zip(a, b))


def capture(source_layout, placed_holes, placed_regions, previous=None):
    """Corrections that turn source_layout into what the author placed in the editor.

    placed_holes lists {"number", "cup_m", "tees_m", "aim_m"} as saved in the course; placed_regions lists
    the terrain's {"id", "surface", "outline_m"}. Point corrections are recomputed from what was placed;
    other notes in previous corrections, such as reviewed, are kept.
    """
    result = copy.deepcopy(previous) if previous else {}
    holes = {number: {k: v for k, v in fix.items() if k not in HOLE_POINT_KEYS}
             for number, fix in result.get("holes", {}).items()}
    by_number = {h["number"]: h for h in source_layout["holes"]}
    for placed in placed_holes:
        changes = _hole_changes(by_number[placed["number"]], placed)
        if changes:
            holes[str(placed["number"])] = {**holes.get(str(placed["number"]), {}), **changes}
    result["holes"] = {number: fix for number, fix in holes.items() if fix}
    source_regions = {r["id"]: r for r in source_layout["regions"]}
    placed_by_id = {r["id"]: r for r in placed_regions}
    regions = {rid: None for rid in source_regions if rid not in placed_by_id}
    for rid, region in placed_by_id.items():
        source = source_regions.get(rid)
        if source is None or source["surface"] != region["surface"] or _outline_changed(source["outline_m"], region["outline_m"]):
            regions[rid] = {"surface": region["surface"], "outline_m": region["outline_m"]}
    result["regions"] = regions
    return result
