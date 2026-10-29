# SPDX-License-Identifier: MIT
"""Savanna range layout: seeded tree patches, the approach bunker and the rough-to-desert ground mask.

Terrain-local metres; no unreal import, so the rules run under plain Python tests.
"""
import math
import random

# The rough is solid out to ROUGH_SOLID_M from play, where the 12 m band of 3D grass tiles ends,
# then fades to desert over ROUGH_FADE_M, so no wide strip of bare rough shows past the tiles.
ROUGH_SOLID_M = 12.0
ROUGH_FADE_M = 4.0


def relocate_bunker(outline, mound_centers, target=(108.0, 0.0)):
    center = tuple(sum(p[axis] for p in outline) / len(outline) for axis in (0, 1))
    delta = target[0] - center[0], target[1] - center[1]
    moved = [(x + delta[0], y + delta[1]) for x, y in outline]
    centers = [(x + delta[0], y + delta[1]) if math.hypot(x - center[0], y - center[1]) < 0.1 else (x, y)
               for x, y in mound_centers]
    return moved, centers


# Jacaranda canopy scale: each patch kind's scale range times the canopy ratio measured on
# 2026-10-20.
CANOPY_RATIO = 0.5368158485
TREE_SETTINGS = {
    "Near": dict(spacing_m=12.0, minimum_separation_m=9.0, scale_range=(0.55 * CANOPY_RATIO, 0.78 * CANOPY_RATIO),
                 minimum_distance_from_play_m=8.0, maximum_distance_from_play_m=25.0),
    "Outer": dict(spacing_m=27.0, minimum_separation_m=16.0, scale_range=(0.6 * CANOPY_RATIO, 1.1 * CANOPY_RATIO),
                  minimum_distance_from_play_m=25.0, maximum_distance_from_play_m=0.0),
    "Backdrop": dict(spacing_m=42.0, minimum_separation_m=24.0, scale_range=(0.7 * CANOPY_RATIO, 1.15 * CANOPY_RATIO),
                     minimum_distance_from_play_m=45.0, maximum_distance_from_play_m=0.0),
}


def tree_patches(prefix="SavannaTree"):
    """(name, kind, outline, seed) for 36 seeded tree areas: near the fairway, outer and backdrop."""
    result = []
    rng = random.Random(61019)

    def patch(kind, x, y, rx, ry):
        name = f"{prefix}{kind}{len(result):02d}"
        phase = rng.random() * math.tau
        points = []
        for i in range(12):
            angle = math.tau * i / 12
            radius = rng.uniform(0.88, 1.08)
            dx, dy = rx * radius * math.cos(angle), ry * radius * math.sin(angle)
            points.append((x + dx * math.cos(phase) - dy * math.sin(phase),
                           y + dx * math.sin(phase) + dy * math.cos(phase)))
        result.append((name, kind, points, 6100 + len(result)))

    for sign, xs in ((1, (30, 85, 145, 210, 275, 330)), (-1, (50, 112, 175, 242, 300, 340))):
        for x in xs:
            patch("Near", x, sign * rng.uniform(32, 37), 16, 13)
    for sign in (-1, 1):
        for x in (-15, 100, 220, 350):
            for y in (160, 310):
                patch("Outer", x + rng.uniform(-8, 8), sign * (y + rng.uniform(-8, 8)), 45, 60)
    for x in (520, 750):
        for y in (-310, -100, 130, 340):
            patch("Backdrop", x + rng.uniform(-12, 12), y + rng.uniform(-10, 10), 70, 68)
    return result


def ground_mask(outlines, minimum, maximum, width=1024, height=512):
    """White near play, black beyond ROUGH_SOLID_M + ROUGH_FADE_M. Texture row zero corresponds to minimum Y."""
    reach = ROUGH_SOLID_M + ROUGH_FADE_M
    pixels = bytearray(width * height)
    sx = (maximum[0] - minimum[0]) / width
    sy = (maximum[1] - minimum[1]) / height
    for outline in outlines:
        for a, b in zip(outline, outline[1:] + outline[:1]):
            dx, dy = b[0] - a[0], b[1] - a[1]
            length2 = dx * dx + dy * dy
            x0 = max(0, int(math.floor((min(a[0], b[0]) - reach - minimum[0]) / sx)))
            x1 = min(width, int(math.ceil((max(a[0], b[0]) + reach - minimum[0]) / sx)))
            y0 = max(0, int(math.floor((min(a[1], b[1]) - reach - minimum[1]) / sy)))
            y1 = min(height, int(math.ceil((max(a[1], b[1]) + reach - minimum[1]) / sy)))
            for row in range(y0, y1):
                py = minimum[1] + (row + 0.5) * sy - a[1]
                for col in range(x0, x1):
                    px = minimum[0] + (col + 0.5) * sx - a[0]
                    t = max(0, min(1, (px * dx + py * dy) / length2)) if length2 else 0
                    distance = math.hypot(px - t * dx, py - t * dy)
                    fade = max(0, min(1, (distance - ROUGH_SOLID_M) / ROUGH_FADE_M))
                    value = round(255 * (1 - fade * fade * (3 - 2 * fade)))
                    index = row * width + col
                    pixels[index] = max(pixels[index], value)
    return bytes(pixels)
