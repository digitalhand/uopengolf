# SPDX-License-Identifier: MIT
"""Convert mapped golf features to an editable layout in local metres.

Outlines from public maps overlap and cross freely, but the terrain builder needs outlines that are
either apart or nested, so every surface edge becomes one clean constrained edge. clean_regions settles
each crossing the way a course is mown: a fairway grows a collar around a green or tee that crosses it
and steps back from a bunker; a bunker or tee gives way to a green. Cups are chosen on flat ground away
from the green edge, and every published tee set gets a tee pad along the route. A tee the author moved in
the corrections file gets its pad where the author placed it, while the layout keeps the map estimate.
"""
import math

from osgeo import ogr, osr

import course_corrections
from outline_smoothing import smooth_outline

ogr.UseExceptions()

DRAW_ORDER = ("fairway", "tee", "green", "bunker")  # Later surfaces win where outlines nest.
WATER_KINDS = ("water_hazard", "lateral_water_hazard", "water")
YARD_M = 0.9144
GAP_M = 0.5          # Clearance between two surface edges after cleanup.
PEER_GAP_M = 1.1     # A bunker or tee stays this far from a green, so a fairway edge fits between them.
MIN_AREA_M2 = 2.0
CUP_EDGE_M = 4.0
CUP_FALLBACK_EDGE_M = 2.0
CUP_MAX_SLOPE = 0.03
TEE_PAD_WIDTH_M = 5.0
TEE_PAD_LENGTH_M = 10.0
TEE_LEVEL_SLOPE = 0.04
TEE_SEARCH_M = 25
PLAY_AREA_MARGIN_M = 25.0    # Rough this far outside the course boundary is built as finely as the rough inside it.
PLAY_AREA_CLEARANCE_M = 3.0  # The play area's edge keeps this far from every surface edge.


def join_route(parts):
    """Join directed sections at common nodes. Ambiguous branches need a designer."""
    if not parts:
        return []
    starts = [p for p in parts if not any(other[-1] == p[0] for other in parts)]
    if len(starts) != 1:
        return []
    route = list(starts[0])
    remaining = [part for part in parts if part is not starts[0]]
    while remaining:
        next_parts = [part for part in remaining if part[0] == route[-1]]
        if len(next_parts) != 1:
            return []
        route.extend(next_parts[0][1:])
        remaining.remove(next_parts[0])
    return route


def route_length_m(route):
    return sum(math.dist(a, b) for a, b in zip(route, route[1:]))


def along(route, distance):
    """The point distance metres along route, clamped to its ends."""
    for start, end in zip(route, route[1:]):
        length = math.dist(start, end)
        if distance <= length and length:
            return [a + (b - a) * distance / length for a, b in zip(start, end)]
        distance -= length
    return list(route[-1])


def rings_from_ways(ways):
    """Closed rings from the outer member ways of a multipolygon, joined end to end in either direction."""
    pending = [list(map(list, w)) for w in ways if len(w) >= 2]
    rings = []
    while pending:
        ring = pending.pop(0)
        while ring[0] != ring[-1]:
            match = next((w for w in pending if w[0] == ring[-1] or w[-1] == ring[-1]), None)
            if match is None:
                break
            pending.remove(match)
            ring.extend((match if match[0] == ring[-1] else match[::-1])[1:])
        if ring[0] == ring[-1] and len(ring) >= 4:
            rings.append(ring[:-1])
    return rings


# Geometry helpers over OGR (GEOS). Outlines are lists of [x, y] without a repeated closing point.

def _polygon(points):
    ring = ogr.Geometry(ogr.wkbLinearRing)
    for x, y in [*points, points[0]]:
        ring.AddPoint_2D(float(x), float(y))
    shape = ogr.Geometry(ogr.wkbPolygon)
    shape.AddGeometry(ring)
    return shape.MakeValid() if not shape.IsValid() else shape


def _parts(shape):
    """Polygons inside shape, without their holes; the terrain fills a hole with whatever nests inside it."""
    if shape is None or shape.IsEmpty():
        return []
    kind = ogr.GT_Flatten(shape.GetGeometryType())
    if kind == ogr.wkbPolygon:
        outer = ogr.Geometry(ogr.wkbPolygon)
        outer.AddGeometry(shape.GetGeometryRef(0).Clone())
        return [outer]
    if kind in (ogr.wkbMultiPolygon, ogr.wkbGeometryCollection):
        return [p for i in range(shape.GetGeometryCount()) for p in _parts(shape.GetGeometryRef(i))]
    return []


def _outline(shape):
    ring = shape.GetGeometryRef(0)
    points = [[round(ring.GetX(i), 3), round(ring.GetY(i), 3)] for i in range(ring.GetPointCount() - 1)]
    return [p for i, p in enumerate(points) if p != points[i - 1]]


def _distinct(points, minimum=0.05):
    kept = []
    for p in points:
        if not kept or math.dist(p, kept[-1]) >= minimum:
            kept.append(p)
    while len(kept) > 1 and math.dist(kept[0], kept[-1]) < minimum:
        kept.pop()
    return kept


def _smoothed(points, spacing):
    points = _distinct(points)
    if spacing is None or len(points) < 3:
        return points
    return smooth_outline(points, spacing)


def _conflict(a, b):
    return a.Boundary().Intersects(b.Boundary()) or a.Boundary().Distance(b.Boundary()) < GAP_M * 0.2


def _union(shapes):
    result = None
    for shape in shapes:
        result = shape.Clone() if result is None else result.Union(shape)
    return result


def _merge_surface(items):
    """Overlapping or touching outlines of one surface become one; the first ID of each group is kept."""
    groups = []
    for rid, shape in items:
        touching = [g for g in groups if g[1].Intersects(shape)]
        ids = [i for g in touching for i in g[0]] + [rid]
        merged = _union([g[1] for g in touching] + [shape])
        groups = [g for g in groups if g not in touching] + [(ids, merged)]
    return [(ids, part) for ids, shape in groups for part in _parts(shape)]


def _give_way(shape, others, gap):
    """shape without the parts of others it conflicts with, kept gap away from them."""
    near = [o for o in others if _conflict(shape, o)]
    return shape.Difference(_union([o.Buffer(gap, 4) for o in near])) if near else shape


def _grow_around(shape, others, gap, rounds=6):
    """shape grown to hold the others it conflicts with; growing can reach the next one, so repeat."""
    for _ in range(rounds):
        near = [o for o in others if _conflict(shape, o)]
        if not near:
            break
        shape = shape.Union(_union([o.Buffer(gap, 4) for o in near]))
    return shape


def _settle(by_surface):
    greens = [s for _, s in by_surface["green"]]
    by_surface["bunker"] = [(i, b) for i, s in by_surface["bunker"] for b in _parts(_give_way(s, greens, PEER_GAP_M))]
    bunkers = [s for _, s in by_surface["bunker"]]
    by_surface["tee"] = [(i, t) for i, s in by_surface["tee"] for t in _parts(_give_way(s, greens + bunkers, PEER_GAP_M))]
    tees = [s for _, s in by_surface["tee"]]
    fairways = [(i, _give_way(_grow_around(s, greens + tees, GAP_M), bunkers, GAP_M)) for i, s in by_surface["fairway"]]
    by_surface["fairway"] = _merge_surface([(i, p) for ids, s in fairways for i in ids[:1] for p in _parts(s)])
    return by_surface


def _named(by_surface):
    result = []
    for surface in DRAW_ORDER:
        used = {}
        for ids, shape in sorted(by_surface[surface], key=lambda item: item[0]):
            if shape.GetArea() < MIN_AREA_M2:
                continue
            rid = ids[0]
            used[rid] = used.get(rid, 0) + 1
            result.append({"id": rid if used[rid] == 1 else f"{rid}#{used[rid]}", "surface": surface,
                           "outline_m": _outline(shape.SimplifyPreserveTopology(0.02))})
    return result


def _check_separate(regions):
    shapes = [_polygon(r["outline_m"]) for r in regions]
    crossing = [(a["id"], b["id"]) for i, a in enumerate(regions) for j, b in enumerate(regions[i + 1:], i + 1)
                if a["surface"] != b["surface"] and shapes[i].Boundary().Intersects(shapes[j].Boundary())]
    if crossing:
        raise ValueError(f"Outlines still cross after cleanup; correct them by hand: {crossing[:8]}")


def clean_regions(regions, smooth_m=1.0):
    """Regions in drawing order whose outlines never cross or touch, each keeping a source ID."""
    by_surface = {surface: [] for surface in DRAW_ORDER}
    for region in regions:
        if region["surface"] in by_surface:
            points = region["outline_m"] if region["surface"] == "tee" else _smoothed(region["outline_m"], smooth_m)
            by_surface[region["surface"]].extend((region["id"], p) for p in _parts(_polygon(points)))
    by_surface = {s: _merge_surface(items) for s, items in by_surface.items()}
    cleaned = _named(_settle(by_surface))
    _check_separate(cleaned)
    return cleaned


def _point(x, y):
    p = ogr.Geometry(ogr.wkbPoint)
    p.AddPoint_2D(float(x), float(y))
    return p


def choose_cup(green_outline, target, slope_at, step=1.0):
    """A cup position on flat green away from its edge, nearest target; a note says why one needs review."""
    if not green_outline:
        return list(target), "no mapped green at the end of the route"
    green = _polygon(green_outline)
    edge = green.Boundary()
    x0, x1, y0, y1 = green.GetEnvelope()
    candidates = []
    for i in range(int((x1 - x0) / step) + 1):
        for j in range(int((y1 - y0) / step) + 1):
            x, y = x0 + i * step, y0 + j * step
            p = _point(x, y)
            if green.Contains(p):
                candidates.append((math.dist((x, y), target), edge.Distance(p), slope_at(x, y), [float(x), float(y)]))
    candidates.sort(key=lambda c: c[0])
    for distance, clearance, slope, point in candidates:
        if clearance >= CUP_EDGE_M and slope < CUP_MAX_SLOPE:
            return point, None
    usable = [c for c in candidates if c[1] >= CUP_FALLBACK_EDGE_M] or candidates
    if not usable:
        return list(target), "green too small for a cup"
    best = min(usable, key=lambda c: (round(c[2], 3), c[0]))
    return best[3], f"cup slope {best[2] * 100:.1f} % with {best[1]:.1f} m to the edge"


def place_tees(route, published_yards):
    """The longest set at the route start; shorter sets move forward along the route by their yardage."""
    back = max(published_yards.values())
    return {name: [round(v, 3) for v in along(route, (back - yards) * YARD_M)] for name, yards in published_yards.items()}


def _remaining_m(route, point):
    """Route length still to play from where point sits along the route."""
    total, walked, best = route_length_m(route), 0.0, None
    for a, b in zip(route, route[1:]):
        length = math.dist(a, b)
        t = 0.0 if not length else max(0.0, min(1.0, ((point[0] - a[0]) * (b[0] - a[0]) + (point[1] - a[1]) * (b[1] - a[1])) / length ** 2))
        foot = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
        candidate = (math.dist(point, foot), total - walked - t * length)
        best = candidate if best is None or candidate[0] < best[0] else best
        walked += length
    return best[1]


def _level(slope_at, x, y):
    return all(slope_at(x + dx, y + dy) < TEE_LEVEL_SLOPE for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)))


def level_tees(route, published_yards, tees, slope_at):
    """Each tee moved onto the nearest level ground that keeps its yardage; notes name tees left on a slope."""
    back, total = max(published_yards.values()), route_length_m(route)
    result, notes = {}, []
    for name, start in tees.items():
        if _level(slope_at, *start):
            result[name] = list(start)
            continue
        target = total - (back - published_yards[name]) * YARD_M
        best = None
        for i in range(-TEE_SEARCH_M, TEE_SEARCH_M + 1):
            for j in range(-TEE_SEARCH_M, TEE_SEARCH_M + 1):
                x, y = start[0] + i, start[1] + j
                offset = math.hypot(i, j)
                if offset > TEE_SEARCH_M or (best and offset * 0.5 >= best[0]) or not _level(slope_at, x, y):
                    continue
                score = abs(_remaining_m(route, (x, y)) - target) + 0.5 * offset
                if best is None or score < best[0]:
                    best = (score, [round(x, 3), round(y, 3)])
        if best is None:
            notes.append(f"{name} tee has no level ground within {TEE_SEARCH_M} m")
        result[name] = best[1] if best else list(start)
    return result, notes


def _heading(route, point):
    """Direction of play at the route section nearest point."""
    best = min(zip(route, route[1:]), key=lambda s: _point(*point).Distance(_line(s)))
    dx, dy = best[1][0] - best[0][0], best[1][1] - best[0][1]
    length = math.hypot(dx, dy) or 1.0
    return dx / length, dy / length


def _line(points):
    line = ogr.Geometry(ogr.wkbLineString)
    for x, y in points:
        line.AddPoint_2D(float(x), float(y))
    return line


def _pad(point, heading):
    ux, uy = heading
    vx, vy = -uy, ux
    half_l, half_w = TEE_PAD_LENGTH_M / 2 - 1, TEE_PAD_WIDTH_M / 2 - 1
    corners = [[point[0] + ux * a * half_l + vx * b * half_w, point[1] + uy * a * half_l + vy * b * half_w]
               for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    return _polygon(corners).Buffer(1.0, 4)  # Rounded corners.


def tee_pads(number, route, tees):
    """Rounded tee pads, one per tee set, merged where they overlap."""
    shapes = [(f"tee/{number}/{name}", _pad(point, _heading(route, point))) for name, point in tees.items()]
    return [{"id": ids[0], "surface": "tee", "outline_m": _outline(shape)} for ids, shape in _merge_surface(shapes)]


def _containing(outlines, point, reach=30.0):
    p = _point(*point)
    shapes = [(o, _polygon(o)) for o in outlines]
    inside = [o for o, s in shapes if s.Contains(p)]
    if inside:
        return inside[0]
    near = sorted(((s.Distance(p), o) for o, s in shapes), key=lambda item: item[0])
    return near[0][1] if near and near[0][0] <= reach else None


def play_area(boundary, regions):
    """The course boundary grown by PLAY_AREA_MARGIN_M and around every surface that reaches past it.

    The terrain builds rough inside it more finely than the rough farther out. Every surface outline is either
    inside it or apart from it, never crossing it, so its edge is one more clean constrained edge.
    """
    shape = _polygon(boundary).Buffer(PLAY_AREA_MARGIN_M, 4)
    for region in regions:
        shape = shape.Union(_polygon(region["outline_m"]).Buffer(PLAY_AREA_CLEARANCE_M, 4))
    largest = max(_parts(shape), key=lambda part: part.GetArea())
    return _outline(largest.SimplifyPreserveTopology(0.5))


def _local_transform(config):
    source = osr.SpatialReference()
    source.ImportFromEPSG(4326)
    source.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    target = osr.SpatialReference()
    target.SetFromUserInput(config["crs"])
    target.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    transform = osr.CoordinateTransformation(source, target)
    east, north, _ = config["origin_m"]

    def local(coords):
        return [[round(x - east, 3), round(y - north, 3)] for x, y, _ in transform.TransformPoints(coords)]
    return local


def _hole(config, number, features, local, greens, slope_at):
    segments = [f for f in features if f["properties"]["kind"] == "hole" and f["properties"].get("ref") == str(number)]
    route = join_route([f["geometry"]["coordinates"] for f in segments])
    route_m = local(route) if route else []
    yards = {k: v[number - 1] for k, v in config["tee_yards"].items()}
    record = {"number": number, "par": config["par"][number - 1], "published_yards": yards,
              "ways": [f["id"] for f in segments], "routing_m": route_m, "review": []}
    if len(route_m) < 2:
        record["review"].append("no connected route in the map data")
        return record
    record["mapped_length_yards"] = round(route_length_m(route_m) / YARD_M, 1)
    green = _containing(greens, route_m[-1])
    record["cup_m"], note = choose_cup(green, route_m[-1], slope_at)
    if note:
        record["review"].append(note)
    record["tees_m"], notes = level_tees(route_m, yards, place_tees(route_m, yards), slope_at)
    record["review"].extend(notes)
    return record


def _pads(holes, corrections):
    """Tee pads at each tee as the author corrected it, so a moved tee keeps its tee turf."""
    placed = course_corrections.apply({"holes": holes, "regions": []}, {"holes": (corrections or {}).get("holes", {})})
    return [pad for hole in placed["holes"] if hole.get("tees_m") and len(hole["routing_m"]) >= 2
            for pad in tee_pads(hole["number"], hole["routing_m"], hole["tees_m"])]


def make_layout(config, collection, slope_at=None, corrections=None):
    """The editable layout for config's holes; slope_at(x, y) reads the prepared ground in local metres.

    corrections is the author's corrections file; only its tees are used here, to place the tee pads.
    """
    slope_at = slope_at or (lambda _x, _y: 0.0)
    local = _local_transform(config)
    features = collection["features"]
    raw, water, boundary = [], [], []
    for feature in features:
        shape = feature["geometry"]
        if shape["type"] != "Polygon":
            continue
        kind, outline = feature["properties"]["kind"], local(shape["coordinates"][0][:-1])
        if kind == "course_boundary":
            boundary = outline
        elif kind in WATER_KINDS:
            water.append({"id": feature["id"], "outline_m": outline})
        else:
            raw.append({"id": feature["id"], "surface": {"bunker": "bunker"}.get(kind, kind), "outline_m": outline})
    greens = [r["outline_m"] for r in raw if r["surface"] == "green"]
    holes = [_hole(config, number, features, local, greens, slope_at) for number in range(1, config["hole_count"] + 1)]
    pads = _pads(holes, corrections)
    if boundary:
        area = _polygon(boundary)
        water = [w for w in water if _polygon(w["outline_m"]).Intersects(area)]
    water = [{"id": w["id"], "outline_m": _outline(_polygon(_smoothed(w["outline_m"], 1.0)))} for w in water]
    regions = clean_regions(raw + pads)
    return {"version": 2, "name": config["name"], "crs": config["crs"], "origin_m": config["origin_m"],
            "attribution": collection["attribution"], "license": collection["license"], "boundary_m": boundary,
            "holes": holes, "regions": regions, "water": water,
            "play_area_m": play_area(boundary, regions) if boundary else []}
