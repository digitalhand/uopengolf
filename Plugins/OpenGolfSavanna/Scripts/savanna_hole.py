# SPDX-License-Identifier: MIT
"""The savanna range as a par-4 hole: the green 400 yards out with a gently contoured surface, the fairway bunker on the
left of the landing area and a greenside bunker right of the green.

Terrain-local metres (X downrange, Y right). apply_layout is pure, so the layout is tested under plain Python;
BuildSavannaRange.py applies it to DA_SavannaRange. Running it again leaves an applied layout unchanged.
"""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "OpenGolfCourse" / "Scripts"))
import hole_layout as hole  # noqa: E402

PAR = 4
# Tee to pin, 400 yards; AUOpenGolfRange::HoleDistanceM places the flag here.
PIN = (365.76, 0.0)
# The pin sits back left of the green's centre, on its flattest ground.
GREEN_CENTER = (363.0, 1.5)
# In the left (-Y) rough beside the drive landing area, 2.5 m off the fairway (terrain regions nest or stay apart; their
# edges never cross). Its lip just reaches the fairway's edge, so drives down the middle roll on level ground.
FAIRWAY_BUNKER = (240.0, -22.0)
# Right of the green, just off the fringe, three quarters the size of the fairway bunker.
GREENSIDE_BUNKER = (360.0, 19.0)
GREENSIDE_BUNKER_SCALE = 0.75
GREENSIDE_BUNKER_NAME = "GreensideBunker"
# New regions take their surface and spacing from these existing ones.
REGION_TEMPLATES = {GREENSIDE_BUNKER_NAME: "Bunker"}
# The backdrop hill that closed the range behind the old green would tilt the new one; it moves back and narrows so
# it still fades out (under 1 cm) before the detailed ground meets the Landscape at x = 504 m.
BACKDROP_HILL = ((385.0, 5.0), (440.0, 5.0))
BACKDROP_HILL_RADIUS_X = 36.0
# (cx, cy, rx, ry, height): a broad rise behind the green tilting it gently toward the fairway, a swell back right and a
# softer one front left. Grades stay under 2.5 % on the green and 1 % within 1.5 m of the pin.
GREEN_CONTOUR = [
    (GREEN_CENTER[0] + 30.0, GREEN_CENTER[1] - 4.0, 34.0, 30.0, 0.25),
    (GREEN_CENTER[0] + 5.0, GREEN_CENTER[1] + 8.0, 9.0, 8.0, 0.10),
    (GREEN_CENTER[0] - 8.0, GREEN_CENTER[1] - 6.0, 10.0, 9.0, 0.12),
]
MAX_GREEN_GRADE = 0.025
MAX_PIN_GRADE = 0.01
PIN_FLAT_RADIUS_M = 1.5
# Green-to-bunker and fringe-to-bunker clearances.
MIN_BUNKER_CLEARANCE_M = 1.5
# The fairway runs up to a metre of first cut short of the fringe; it is pulled back from FAIRWAY_TRIM_START_X on.
FAIRWAY_GAP_M = 1.0
FAIRWAY_TRIM_START_X = 300.0


def _centroid(outline):
    """Mean of the outline's points: where a bunker's lip and hollow mounds are centred."""
    return sum(p[0] for p in outline) / len(outline), sum(p[1] for p in outline) / len(outline)


def apply_layout(regions, mounds):
    """regions: {name: outline}; mounds: [(cx, cy, rx, ry, height)]. Returns the hole's (regions, mounds)."""
    regions = dict(regions)
    green_offset = (GREEN_CENTER[0] - hole.center(regions["Green"])[0], GREEN_CENTER[1] - hole.center(regions["Green"])[1])
    for name in ("Green", "Fringe"):
        regions[name] = hole.translate(regions[name], *green_offset)
    regions["Fairway"] = hole.trim_end(regions["Fairway"], FAIRWAY_TRIM_START_X, hole.bounds(regions["Fringe"])[0] - FAIRWAY_GAP_M)

    bunker = regions["Bunker"]
    origin = _centroid(bunker)
    regions["Bunker"] = hole.translate(bunker, FAIRWAY_BUNKER[0] - origin[0], FAIRWAY_BUNKER[1] - origin[1])
    mounds = hole.move_mounds(mounds, origin, FAIRWAY_BUNKER)

    # The greenside bunker copies the fairway bunker's shape and its lip and hollow. Mounds an earlier run added are
    # found by centre, since the definition stores them as float32.
    added = [GREENSIDE_BUNKER] + [m[:2] for m in GREEN_CONTOUR]
    mounds = [m for m in mounds if not any(hole.mounds_at([m], centre) for centre in added)]
    regions[GREENSIDE_BUNKER_NAME], greenside = hole.copy_feature(regions["Bunker"], mounds, FAIRWAY_BUNKER, GREENSIDE_BUNKER, GREENSIDE_BUNKER_SCALE)
    hills = hole.mounds_at(mounds, BACKDROP_HILL[0]) + hole.mounds_at(mounds, BACKDROP_HILL[1])
    mounds = [(*BACKDROP_HILL[1], BACKDROP_HILL_RADIUS_X, m[3], m[4]) if m in hills else m for m in mounds]
    mounds += greenside + [tuple(m) for m in GREEN_CONTOUR]
    return regions, mounds
