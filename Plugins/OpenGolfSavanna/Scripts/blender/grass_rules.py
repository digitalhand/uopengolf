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


def tile_points(seed, pieces, size=(TILE_M, TILE_M), scale=1.0, weights=None, cell=CELL_M):
    """Seeded (x, y, yaw degrees, scale, piece) placements, one per grid cell of the footprint.

    pieces maps piece name -> kind (Tall, Mid, ...). Each point stays inside its own cell, so cover
    is even, every footprint has the same density and the result is repeatable.
    """
    cols, rows = grid(size, cell)
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
            result.append((x, y, rng.uniform(0, 360), scale * rng.uniform(0.9, 1.15), rng.choices(names, chances)[0]))
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
