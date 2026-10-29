# SPDX-License-Identifier: MIT
"""Which savanna vegetation moves in the wind, and how. Pure Python, so it can be tested without Unreal.

The motion itself is the course kit's (vegetation_wind.py profiles, vegetation_wind_assets.py wiring).
To give more vegetation wind:
  1. pick a profile in vegetation_wind.PROFILES, or add one there (data only)
  2. add its material instances to MATERIALS, with the meshes whose size the motion scales to and the
     largest scale they are placed at
  3. add its placed meshes to MESHES, with the distance beyond which they hold still
  4. run ApplyVegetationWind.py, then the shimmer checks in AGENTS.md
Its parent material must call add_vegetation_wind; the savanna parents (card, solid and both grasses) do.
Cut turf, litter, moss, rocks and seedlings stay still.
"""
from dataclasses import dataclass
from fnmatch import fnmatchcase

import savanna_layout
import understory_layout

JACARANDA = "/OpenGolfSavanna/Trees/Jacaranda"
NE = "/OpenGolfSavanna/Meshes/NatureEssentials"
GRASS_MEDIUM = "/OpenGolfSavanna/Meshes/Grass/Medium/SM_GrassMedium_30cm_"
# Largest scale of a vegetation preset's default ScaleRange (OpenGolfTerrain.h).
PRESET_MAX_SCALE = 1.3
TREE_MAX_SCALE = max(settings["scale_range"][1] for settings in savanna_layout.TREE_SETTINGS.values())
UNDERSTORY_MAX_SCALE = max(PRESET_MAX_SCALE, *(category["scale"][1] for category in understory_layout.CATEGORIES.values()
                                               if any("Grass" in mesh for mesh in category["meshes"])))


@dataclass(frozen=True)
class MaterialWind:
    profile: str
    # The tallest of these sets the height (and crown) the motion scales to.
    size_meshes: tuple
    # Leaves and blades flutter; trunks and branches leave it to them.
    flutter: bool = True
    # Largest scale its meshes are placed at: motion grows with the instance, and this sizes its bound.
    max_scale: float = PRESET_MAX_SCALE


TREE = (f"{JACARANDA}/Meshes/SM_Jacaranda",)
MATERIALS = {
    f"{JACARANDA}/Materials/MI_JacarandaTreeLeaves": MaterialWind("BroadleafTree", TREE, max_scale=TREE_MAX_SCALE),
    f"{JACARANDA}/Materials/MI_JacarandaTreeBranches": MaterialWind("BroadleafTree", TREE, flutter=False, max_scale=TREE_MAX_SCALE),
    f"{JACARANDA}/Materials/MI_JacarandaTreeTrunk": MaterialWind("BroadleafTree", TREE, flutter=False, max_scale=TREE_MAX_SCALE),
    "/OpenGolfSavanna/Materials/Grass/Medium/MI_GrassMedium01": MaterialWind("TallGrass", (f"{GRASS_MEDIUM}1m_A",)),
    "/OpenGolfSavanna/Materials/NatureEssentials/MI_NE_Grass": MaterialWind(
        "UnderstoryGrass", tuple(f"{NE}/SM_NE_Grass{name}" for name in ("Clump", "Seedy", "Small")), max_scale=UNDERSTORY_MAX_SCALE),
}

# Placed meshes (patterns allowed) that run their material's wind, and the distance in m beyond which they
# hold still. Trees stop at 150 m so their distant shadows stay cached; grass stops where it fades out.
MESHES = {
    f"{JACARANDA}/Meshes/SM_Jacaranda": 150.0,
    f"{GRASS_MEDIUM}*": 90.0,
    f"{NE}/SM_NE_Grass*": 60.0,
}


def package_path(path):
    """/A/B/SM_X from /A/B/SM_X.SM_X or /A/B/SM_X."""
    return path.split(".", 1)[0]


def mesh_wind_distance_m(mesh_path):
    """Hold-still distance for a placed mesh, or None when it does not move in the wind."""
    path = package_path(mesh_path)
    return next((distance for pattern, distance in MESHES.items() if fnmatchcase(path, pattern)), None)
