# SPDX-License-Identifier: MIT
"""Seeded dry-savanna cover around each tree: litter and moss near the trunk, shaded grass under the
canopy and taller seedy grass at the drip line. Terrain-local meters; no unreal import."""
import math
import random

# Canopy radius at canopy scale 1; a jacaranda's canopy scale is its instance scale divided by
# savanna_layout.CANOPY_RATIO.
CANOPY_RADIUS_M = 5.8
PLAY_CLEARANCE_M = 2.0
NE = "/OpenGolfSavanna/Meshes/NatureEssentials"

# ring: fraction of the scaled canopy radius. count: pieces for a canopy-scale-1 tree, scaled by canopy area.
# center: True packs pieces toward the inner edge (linear in radius) instead of evenly over the area.
# cluster: (pieces, radius_m) grows each placed point into a tuft; count is then in pieces.
CATEGORIES = {
    "litter": dict(meshes=[f"{NE}/SM_NE_Litter_{s}" for s in "ABCD"], ring=(0.0, 0.85), count=32, center=True,
                   scale=(1.2, 2.0), cull_m=45),
    "moss": dict(meshes=[f"{NE}/SM_NE_MossDead_{s}" for s in "ABCD"], ring=(0.04, 0.35), count=6, center=True,
                 scale=(1.5, 3.0), cull_m=40),
    "seedlings": dict(meshes=[f"{NE}/SM_NE_Seedling_{s}" for s in "ABC"], ring=(0.3, 1.0), count=2, center=False,
                      scale=(1.5, 3.0), cull_m=30),
    "rocks": dict(meshes=[f"{NE}/SM_NE_Rock{s}" for s in ("Dark_A", "Dark_B", "Light_A", "Light_B")], ring=(0.15, 1.1),
                  count=1.2, center=False, scale=(0.1, 0.25), cull_m=250),
    "shade_grass": dict(meshes=[f"{NE}/SM_NE_GrassSmall", f"{NE}/SM_NE_GrassClump"], ring=(0.35, 0.85), count=8,
                        center=False, scale=(0.8, 1.2), cull_m=120, cluster=(2, 0.25)),
    "drip_grass": dict(meshes=[f"{NE}/SM_NE_GrassClump", f"{NE}/SM_NE_GrassClump", f"{NE}/SM_NE_GrassSeedy"],
                       ring=(0.8, 1.35), count=36, center=False, scale=(1.0, 1.5), cull_m=120, cluster=(3, 0.35)),
}


def _tree_rng(x, y, category):
    # Seeded from the tree's own position so adding or moving one tree leaves the others unchanged.
    return random.Random(f"{round(x * 10)}:{round(y * 10)}:{category}")


def _segment_distance(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    length2 = dx * dx + dy * dy
    t = 0.0 if not length2 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length2))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def _inside(outline, p):
    result = False
    for a, b in zip(outline, outline[1:] + outline[:1]):
        if (a[1] > p[1]) != (b[1] > p[1]) and p[0] < a[0] + (p[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1]):
            result = not result
    return result


def _near_play(p, outlines):
    for outline, (x0, y0, x1, y1) in outlines:
        if p[0] < x0 - PLAY_CLEARANCE_M or p[0] > x1 + PLAY_CLEARANCE_M or p[1] < y0 - PLAY_CLEARANCE_M or p[1] > y1 + PLAY_CLEARANCE_M:
            continue
        if _inside(outline, p) or min(_segment_distance(p, a, b) for a, b in zip(outline, outline[1:] + outline[:1])) < PLAY_CLEARANCE_M:
            return True
    return False


def understory_points(trees, outlines):
    """trees: (x, y, canopy scale) of each tree. outlines: closed play outlines (fairway, green, fringe, sand, tee).
    Returns (category, variant, x, y, yaw_deg, scale, lift_cm) for every piece."""
    boxes = [(list(o), (min(p[0] for p in o), min(p[1] for p in o), max(p[0] for p in o), max(p[1] for p in o)))
             for o in outlines]
    result = []
    for tx, ty, tree_scale in trees:
        radius = CANOPY_RADIUS_M * tree_scale
        for name, settings in CATEGORIES.items():
            rng = _tree_rng(tx, ty, name)
            size, spread = settings.get("cluster", (1, 0.0))
            expected = settings["count"] * tree_scale * tree_scale / size
            count = int(expected) + (1 if rng.random() < expected - int(expected) else 0)
            inner, outer = settings["ring"]
            for _ in range(count):
                u, angle = rng.random(), rng.random() * math.tau
                if settings["center"]:
                    distance = radius * (inner + (outer - inner) * u)
                else:
                    distance = radius * math.sqrt(inner * inner + u * (outer * outer - inner * inner))
                cx, cy = tx + distance * math.cos(angle), ty + distance * math.sin(angle)
                for member in range(size):
                    # The first piece marks the tuft centre; the rest spread evenly over its disc.
                    offset = spread * math.sqrt(rng.random()) if member else 0.0
                    heading = rng.random() * math.tau
                    x, y = cx + offset * math.cos(heading), cy + offset * math.sin(heading)
                    variant = rng.randrange(len(settings["meshes"]))
                    yaw = rng.random() * 360.0
                    scale = rng.uniform(*settings["scale"])
                    lift = rng.random()
                    if _near_play((x, y), boxes):
                        continue
                    result.append((name, variant, x, y, yaw, scale, lift))
    return result
