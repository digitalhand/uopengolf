# SPDX-License-Identifier: MIT
"""Check a prepared course layout and draw each hole over the aerial image for review.

    python3 review_course_layout.py <source folder> [--holes 1 12]

Reads the source folder written by prepare_course_site.py, applies the author corrections named in its
recipe.json, and writes review/hole_NN.png and review/checks.json. A hole passes when every tee is on a tee
pad, in bounds, dry and nearly level, the cup sits on its green away from the edge, and the mapped route is
within five percent of the published back-tee yardage.
"""
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import course_corrections
from course_layout_draft import CUP_FALLBACK_EDGE_M, CUP_MAX_SLOPE, YARD_M, _point, _polygon, route_length_m
from ground_grid import GroundGrid

TEE_MAX_SLOPE = 0.05
ROUTE_TOLERANCE = 0.05
MARGIN_M = 45
MAX_IMAGE_PX = 1100
COLOURS = {"fairway": (120, 230, 120), "tee": (80, 220, 255), "green": (200, 255, 60), "bunker": (255, 225, 150),
           "water": (60, 140, 255), "boundary": (255, 255, 255), "route": (255, 230, 0), "cup": (255, 40, 40)}


def load(folder):
    recipe = json.loads((folder / "recipe.json").read_text())
    layout = json.loads((folder / "source_layout.json").read_text())
    return recipe, course_corrections.apply(layout, course_corrections.read(recipe))


def _inside(outline, point):
    return bool(outline) and _polygon(outline).Contains(_point(*point))


def surface_at(layout, point):
    """The surface the terrain draws at point: a later region is drawn over an earlier one."""
    return next((r["surface"] for r in reversed(layout["regions"]) if _inside(r["outline_m"], point)), "rough")


def hole_checks(layout, hole, ground):
    """Problems with one hole, in plain words; empty when it passes."""
    problems = list(n for n in hole.get("review", []) if n != course_corrections.AUTHOR_NOTE)
    if "cup_m" not in hole:
        return problems or ["hole has no cup"]
    water = [w["outline_m"] for w in layout["water"]]
    for name, tee in hole["tees_m"].items():
        if (surface := surface_at(layout, tee)) != "tee":
            problems.append(f"{name} tee is on {surface}, not a tee pad")
        if layout["boundary_m"] and not _inside(layout["boundary_m"], tee):
            problems.append(f"{name} tee is out of bounds")
        if any(_inside(w, tee) for w in water):
            problems.append(f"{name} tee is in water")
        slope = ground.slope(*tee)
        if slope >= TEE_MAX_SLOPE:
            problems.append(f"{name} tee slope {slope * 100:.1f} %")
    greens = [r["outline_m"] for r in layout["regions"] if r["surface"] == "green" and _inside(r["outline_m"], hole["cup_m"])]
    if not greens:
        problems.append("cup is not on a green")
    else:
        edge = _polygon(greens[0]).Boundary().Distance(_point(*hole["cup_m"]))
        slope = ground.slope(*hole["cup_m"])
        if edge < CUP_FALLBACK_EDGE_M or slope >= CUP_MAX_SLOPE * 1.5:
            problems.append(f"cup {edge:.1f} m from the green edge on {slope * 100:.1f} % slope")
    back = max(hole["published_yards"].values())
    measured = route_length_m(hole["routing_m"]) / YARD_M
    if abs(measured - back) > back * ROUTE_TOLERANCE:
        problems.append(f"mapped route {measured:.0f} yd against {back} yd published")
    return problems


class Picture:
    """The aerial image cropped around one hole, drawing in local metres."""

    def __init__(self, aerial, georeference, origin, points):
        self.aerial, self.origin = aerial, origin
        west, _south, east, north = georeference["bounds_m"]
        self.pixel_m = (east - west) / georeference["size"][0]
        self.west, self.north = west, north
        xs, ys = [p[0] for p in points], [p[1] for p in points]
        left, top = self.pixel(min(xs) - MARGIN_M, max(ys) + MARGIN_M)
        right, bottom = self.pixel(max(xs) + MARGIN_M, min(ys) - MARGIN_M)
        self.box = (int(left), int(top), int(right), int(bottom))
        self.scale = min(1.0, MAX_IMAGE_PX / max(right - left, bottom - top))
        crop = aerial.crop(self.box)
        self.image = crop.resize((int(crop.width * self.scale), int(crop.height * self.scale)))
        self.draw = ImageDraw.Draw(self.image)
        self.font = ImageFont.load_default(15)

    def pixel(self, x, y):
        return ((x + self.origin[0] - self.west) / self.pixel_m, (self.north - (y + self.origin[1])) / self.pixel_m)

    def at(self, point):
        px, py = self.pixel(*point)
        return ((px - self.box[0]) * self.scale, (py - self.box[1]) * self.scale)

    def outline(self, points, colour, width=2):
        self.draw.line([self.at(p) for p in [*points, points[0]]], fill=colour, width=width)

    def mark(self, point, colour, label, radius=5):
        x, y = self.at(point)
        self.draw.ellipse((x - radius, y - radius, x + radius, y + radius), outline=(0, 0, 0), fill=colour, width=1)
        self.draw.text((x + radius + 2, y - 8), label, fill=colour, font=self.font, stroke_width=2, stroke_fill=(0, 0, 0))


def draw_hole(aerial, georeference, layout, hole, problems, path):
    route = hole["routing_m"]
    points = route + list(hole.get("tees_m", {}).values()) + ([hole["cup_m"]] if "cup_m" in hole else [])
    picture = Picture(aerial, georeference, layout["origin_m"], points)
    if layout["boundary_m"]:
        picture.outline(layout["boundary_m"], COLOURS["boundary"], 1)
    for region in layout["regions"]:
        picture.outline(region["outline_m"], COLOURS[region["surface"]])
    for water in layout["water"]:
        picture.outline(water["outline_m"], COLOURS["water"])
    picture.draw.line([picture.at(p) for p in route], fill=COLOURS["route"], width=2)
    for name, tee in hole.get("tees_m", {}).items():
        picture.mark(tee, COLOURS["tee"], name[0])
    if "cup_m" in hole:
        picture.mark(hole["cup_m"], COLOURS["cup"], "cup", 4)
    title = f"Hole {hole['number']} · par {hole['par']} · {max(hole['published_yards'].values())} yd"
    lines = [title] + (problems or ["no problems found"])
    for i, line in enumerate(lines):
        picture.draw.text((8, 8 + i * 19), line, fill=(255, 255, 255), font=picture.font, stroke_width=2, stroke_fill=(0, 0, 0))
    picture.image.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path)
    parser.add_argument("--holes", type=int, nargs="*")
    args = parser.parse_args()
    recipe, layout = load(args.source)
    ground = GroundGrid.open(args.source / "ground.tif", recipe["origin_m"])
    aerial = Image.open(args.source / "aerial.jpg").convert("RGB")
    georeference = json.loads((args.source / "aerial.json").read_text())
    out = args.source / "review"
    out.mkdir(exist_ok=True)
    report = {}
    for hole in layout["holes"]:
        if args.holes and hole["number"] not in args.holes:
            continue
        problems = hole_checks(layout, hole, ground)
        report[str(hole["number"])] = problems
        if len(hole["routing_m"]) >= 2:
            draw_hole(aerial, georeference, layout, hole, problems, out / f"hole_{hole['number']:02d}.png")
    for set_name in layout["holes"][0]["published_yards"]:
        total = sum(h["published_yards"][set_name] for h in layout["holes"])
        report.setdefault("_totals", {})[set_name] = total
    (out / "checks.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    failing = [n for n, p in report.items() if not n.startswith("_") and p]
    print(f"Reviewed {len(report) - 1} holes; holes with problems: {failing or 'none'}. Images in {out}")


if __name__ == "__main__":
    main()
