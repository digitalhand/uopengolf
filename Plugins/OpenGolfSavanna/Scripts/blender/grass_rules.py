# SPDX-License-Identifier: MIT
"""Rules for the Poly Haven rough grass tiles. No bpy here, so they can be unit-tested."""
from pathlib import Path
import random
import re

MEDIUM = "grass_medium_01"
UNDERLAY = "leafy_grass"  # dense turf texture for the ground under the tiles
TILE_M = 1.0
# Tufts sit on a 1/16 m grid, one per cell, so every footprint below has the same density
# (256 tufts per m^2) and an exact grid.
CELL_M = 1.0 / 16
# Tall tufts carry the 30 cm height; bushier mid, small and large tufts close the cover between them.
WEIGHTS = {"Tall": 4, "Mid": 4, "Small": 4, "Large": 2, "Tiny": 1}
# The edge strip faces the first cut: no wide Large tufts, fewer Tall ones, and scaled down.
EDGE_WEIGHTS = {"Tall": 2, "Mid": 4, "Small": 4, "Tiny": 2}
# The graded strip between the first cut and the tall rough: Blender's -Y faces play. The FBX export mirrors Y, so
# in Unreal that is local +Y, which ScatterBoundary turns towards play. Tufts grow from `low` to `high`
# of full size across its depth, and up to `front_gaps` of the front cells stay empty, so the cut line is ragged.
EDGE_RAMP = {"low": 0.2, "high": 1.0, "front_gaps": 0.5}
# Dense tiles laid edge to edge give full cover, like a commercial grass pack's
# SM_Grass_<height>_Dense_* meshes. Placement fills the interior with the largest square that fits
# and subdivides near play edges and on curved ground (1 m -> 0.5 m -> 0.25 m); edge strips run along
# the outlines. Blades may overhang a tile's edge; the next tile covers the same strip, so no seam
# shows. Each variant has its own seed; Unreal also turns tiles by 90 degrees.
# Settings: `size` (x, y) in metres, `lod` of Grass Medium's tufts, `scale` (full-size tufts reach
# about 32 cm) and optional `weights` (default WEIGHTS). Lower LODs keep dense tiles affordable.
_SQUARE = {"lod": 1, "scale": 1.0}
TILES = (
    ("SM_GrassMedium_30cm_1m_A", MEDIUM, 11, {**_SQUARE, "size": (1.0, 1.0)}),
    ("SM_GrassMedium_30cm_1m_B", MEDIUM, 23, {**_SQUARE, "size": (1.0, 1.0)}),
    ("SM_GrassMedium_30cm_1m_C", MEDIUM, 37, {**_SQUARE, "size": (1.0, 1.0)}),
    ("SM_GrassMedium_30cm_1m_D", MEDIUM, 41, {**_SQUARE, "size": (1.0, 1.0)}),
    ("SM_GrassMedium_30cm_50cm_A", MEDIUM, 53, {**_SQUARE, "size": (0.5, 0.5)}),
    ("SM_GrassMedium_30cm_50cm_B", MEDIUM, 59, {**_SQUARE, "size": (0.5, 0.5)}),
    ("SM_GrassMedium_30cm_25cm_A", MEDIUM, 61, {**_SQUARE, "size": (0.25, 0.25)}),
    ("SM_GrassMedium_30cm_25cm_B", MEDIUM, 67, {**_SQUARE, "size": (0.25, 0.25)}),
    # 1 m long along the outline (X) and 0.25 m deep into the rough (Y).
    ("SM_GrassMedium_30cm_Edge_A", MEDIUM, 71, {"lod": 1, "scale": 0.7, "size": (1.0, 0.25), "weights": EDGE_WEIGHTS}),
    ("SM_GrassMedium_30cm_Edge_B", MEDIUM, 73, {"lod": 1, "scale": 0.7, "size": (1.0, 0.25), "weights": EDGE_WEIGHTS}),
    # 1 m along the outline and 0.5 m deep, rising from the first cut (-Y here, +Y in Unreal) to the tall rough.
    ("SM_GrassMedium_30cm_EdgeRamp_A", MEDIUM, 79, {**_SQUARE, "size": (1.0, 0.5), "ramp": EDGE_RAMP}),
    ("SM_GrassMedium_30cm_EdgeRamp_B", MEDIUM, 83, {**_SQUARE, "size": (1.0, 0.5), "ramp": EDGE_RAMP}),
)
_TUFT = re.compile(r"^grass_medium_01_([a-z]+)_([a-z])_LOD([0-2])$")


def medium_tuft(object_name):
    """The piece name for one of Poly Haven's static tufts (LOD0-2), or None for anything else."""
    match = _TUFT.match(object_name)
    return (f"GrassMedium_{match.group(1).capitalize()}{match.group(2).upper()}_LOD{match.group(3)}"
            if match else None)


def medium_tuft_kind(piece_name):
    """The size kind of a piece named by medium_tuft, e.g. GrassMedium_TallA_LOD1 -> Tall."""
    match = re.match(r"^GrassMedium_([A-Z][a-z]+)[A-Z]_LOD[0-2]$", piece_name)
    if not match:
        raise ValueError(f"Not a Grass Medium piece: {piece_name}")
    return match.group(1)


def grid(size, cell=CELL_M):
    """Columns and rows of tuft cells for a footprint (x, y) in metres; it must be a whole number of cells."""
    dims = []
    for length in size:
        count = round(length / cell)
        if count < 1 or abs(count * cell - length) > 1e-6:
            raise ValueError(f"Footprint {size} is not a whole number of {cell} m cells")
        dims.append(count)
    return tuple(dims)


def _ramp(ramp, t):
    """Size factor and chance of keeping a cell at depth t (0 at the front, 1 at the back) of a graded strip."""
    if not ramp:
        return 1.0, 1.0
    rise = t * t * (3 - 2 * t)
    return ramp["low"] + (ramp["high"] - ramp["low"]) * rise, 1 - ramp["front_gaps"] * (1 - t) ** 2


def tile_points(seed, pieces, size=(TILE_M, TILE_M), scale=1.0, weights=None, cell=CELL_M, ramp=None):
    """Seeded (x, y, yaw degrees, scale, piece) placements, one per grid cell of the footprint.

    pieces maps piece name -> kind (Tall, Mid, ...). Each point stays inside its own cell, so cover
    is even, every footprint has the same density and the result is repeatable. A ramp (see EDGE_RAMP)
    grades size and cover from the front row (-Y) to the back row instead.
    """
    cols, rows = grid(size, cell)
    if ramp and not (0 < ramp["low"] <= ramp["high"] and 0 <= ramp["front_gaps"] < 1):
        raise ValueError(f"A ramp rises from low to high and leaves fewer than all front cells empty: {ramp}")
    rng = random.Random(seed)
    names = sorted(pieces)
    chances = [(weights or WEIGHTS).get(pieces[name], 0) for name in names]
    if not any(chances):
        raise ValueError("No weighted pieces to place")
    result = []
    for row in range(rows):
        for col in range(cols):
            x = -size[0] / 2 + (col + rng.uniform(0.1, 0.9)) * cell
            y = -size[1] / 2 + (row + rng.uniform(0.1, 0.9)) * cell
            yaw, jitter, piece = rng.uniform(0, 360), rng.uniform(0.9, 1.15), rng.choices(names, chances)[0]
            # Only ramps draw this, so the other tiles keep the random sequence they were built from.
            keep = rng.random() if ramp else 0.0
            grow, cover = _ramp(ramp, (y + size[1] / 2) / size[1])
            if keep < cover:
                result.append((x, y, yaw, scale * grow * jitter, piece))
    return result


def content_folders(category):
    """Type-first content folders, e.g. Grass/Medium -> Meshes/Grass/Medium."""
    return {kind: f"{kind}/{category}" for kind in ("Meshes", "Materials", "Textures")}


def grass_maps(asset, maps):
    """Texture role -> source PNG for an asset, from the pinned FetchPolyHaven files."""
    maps = Path(maps)
    textures = maps.parent / "textures"
    roles = {"albedo": maps / f"{asset}_diff_2k.png", "normal": maps / f"{asset}_nor_gl_2k.png",
             "roughness": maps / f"{asset}_rough_1k.png", "ao": maps / f"{asset}_ao_1k.png",
             "opacity": textures / f"{asset}_alpha_2k.png", "albedo_dry": textures / f"{asset}_dry_diff_2k.png"}
    return {role: str(path) for role, path in roles.items()}


def underlay_maps(maps):
    """Texture role -> source PNG for the ground texture under the tiles."""
    maps = Path(maps)
    return {"albedo": str(maps / f"{UNDERLAY}_diff_2k.png"), "normal": str(maps / f"{UNDERLAY}_nor_gl_2k.png"),
            "roughness": str(maps / f"{UNDERLAY}_rough_1k.png")}
