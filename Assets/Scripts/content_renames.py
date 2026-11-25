# SPDX-License-Identifier: MIT
"""The November 2026 content rename: every committed asset whose name or folder broke Docs/ContentNaming.md.

plan(packages) maps each existing package path that moves to its new path; rename_content.py applies it in the
editor. The rules are data here so tests can check that every new name follows asset_naming.problems and that no two
assets land on one path. Generators were changed in the same commit to create the new names.

Hunter Ranch's course folder is not renamed: the course is rebuilt with the new builder names.
"""
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Plugins/OpenGolfCourse/Scripts"))
import asset_naming as names  # noqa: E402

SAVANNA = "/OpenGolfSavanna"
COURSE_KIT = "/OpenGolfCourse"

# Whole packages whose new path is not a rule. Old package path -> new package path.
EXPLICIT = {
    # The startup map and its lighting data.
    "/Game/UOpenGolf/Maps/UOpenGolf": "/Game/UOpenGolf/Maps/L_SavannaRange",
    "/Game/UOpenGolf/Maps/UOpenGolf_BuiltData": "/Game/UOpenGolf/Maps/L_SavannaRange_BuiltData",
    # "Art" said nothing; the ball, gimme ring and training rings share these overlay materials.
    "/Game/UOpenGolf/Art/M_ShotTrail": "/Game/UOpenGolf/Effects/M_ShotTrail",
    "/Game/UOpenGolf/Art/M_TargetRingLine": "/Game/UOpenGolf/Effects/M_TargetRingLine",
    # Two assets were both called DA_SavannaRange; the course definition keeps the name, which is its ID.
    "/Game/UOpenGolf/Terrain/Savanna/DA_SavannaRange": "/Game/UOpenGolf/Terrain/Savanna/DA_SavannaRange_Terrain",
    # Course kit: "Coastal" was the first range; these are the kit's general terrain surfaces.
    f"{COURSE_KIT}/Terrain/Materials/M_CoastalSurface": f"{COURSE_KIT}/Terrain/Materials/M_TerrainSurface",
    f"{COURSE_KIT}/Terrain/Materials/M_CoastalGrass": f"{COURSE_KIT}/Terrain/Materials/M_TerrainGrass",
    f"{COURSE_KIT}/Terrain/Presets/DA_CoastalGrassVegetation": f"{COURSE_KIT}/Terrain/Presets/DA_TerrainVegetation_Grass",
    f"{COURSE_KIT}/Terrain/Presets/DA_CoastalRoughVegetation": f"{COURSE_KIT}/Terrain/Presets/DA_TerrainVegetation_Rough",
    f"{COURSE_KIT}/Terrain/Vegetation/SM_CoastalGrass": f"{COURSE_KIT}/Terrain/Vegetation/SM_TerrainGrass_Tall",
    f"{COURSE_KIT}/Terrain/Vegetation/SM_CoastalRough": f"{COURSE_KIT}/Terrain/Vegetation/SM_TerrainGrass_Short",
    f"{COURSE_KIT}/Materials/M_Water": f"{COURSE_KIT}/Water/M_Water",
    f"{COURSE_KIT}/Flagstick/M_Pole": f"{COURSE_KIT}/Flagstick/M_FlagstickPole",
    # Shared savanna parents.
    f"{SAVANNA}/Materials/M_SavannaCard": f"{SAVANNA}/Shared/Materials/M_SavannaCard",
    f"{SAVANNA}/Materials/M_SavannaSolid": f"{SAVANNA}/Shared/Materials/M_SavannaSolid",
    f"{SAVANNA}/Materials/T_WhiteMask": f"{SAVANNA}/Shared/Textures/T_WhiteMask",
    f"{SAVANNA}/Materials/M_SavannaGround": f"{SAVANNA}/Ground/Materials/M_SavannaGround",
    f"{SAVANNA}/Materials/M_SavannaSurface": f"{SAVANNA}/Surfaces/Materials/M_SavannaSurface",
    f"{SAVANNA}/Materials/M_SavannaGrass": f"{SAVANNA}/Grass/Materials/M_SavannaGrass",
    f"{SAVANNA}/Materials/M_SavannaUnderstoryGrass": f"{SAVANNA}/NatureEssentials/Materials/M_SavannaUnderstoryGrass",
    f"{SAVANNA}/Materials/M_CutTurfSurface": f"{SAVANNA}/CutTurf/Materials/M_CutTurfSurface",
    f"{SAVANNA}/Terrain/DA_BoundaryGrass": f"{SAVANNA}/Grass/Presets/DA_BoundaryGrass",
    f"{SAVANNA}/Terrain/DA_RoughGrassDense": f"{SAVANNA}/Grass/Presets/DA_RoughGrassDense",
    f"{SAVANNA}/Terrain/DA_BunkerGround093C": f"{SAVANNA}/Surfaces/Presets/DA_BunkerGround093C",
    f"{SAVANNA}/Terrain/DA_FairwayGrass008": f"{SAVANNA}/Surfaces/Presets/DA_FairwayGrass008",
    f"{SAVANNA}/Terrain/DA_GreenGrass008": f"{SAVANNA}/Surfaces/Presets/DA_GreenGrass008",
}
# Folder moves for the savanna art: type-first folders become one folder per feature. Longest prefix first.
FOLDERS = (
    (f"{SAVANNA}/Materials/CutTurf", f"{SAVANNA}/CutTurf/Materials"),
    (f"{SAVANNA}/Materials/Bunker", f"{SAVANNA}/Surfaces/Materials"),
    (f"{SAVANNA}/Materials/Fairway", f"{SAVANNA}/Surfaces/Materials"),
    (f"{SAVANNA}/Materials/Green", f"{SAVANNA}/Surfaces/Materials"),
    (f"{SAVANNA}/Materials/Grass/Medium", f"{SAVANNA}/Grass/Medium/Materials"),
    (f"{SAVANNA}/Materials/NatureEssentials", f"{SAVANNA}/NatureEssentials/Materials"),
    (f"{SAVANNA}/Meshes/CutTurf", f"{SAVANNA}/CutTurf/Meshes"),
    (f"{SAVANNA}/Meshes/Grass/Medium", f"{SAVANNA}/Grass/Medium/Meshes"),
    (f"{SAVANNA}/Meshes/NatureEssentials", f"{SAVANNA}/NatureEssentials/Meshes"),
    (f"{SAVANNA}/Terrain/CutTurf", f"{SAVANNA}/CutTurf/Presets"),
    (f"{SAVANNA}/Textures/CutTurf", f"{SAVANNA}/CutTurf/Textures"),
    (f"{SAVANNA}/Textures/Grass/Medium", f"{SAVANNA}/Grass/Medium/Textures"),
    (f"{SAVANNA}/Textures/Ground", f"{SAVANNA}/Ground/Textures"),
    (f"{SAVANNA}/Textures/NatureEssentials", f"{SAVANNA}/NatureEssentials/Textures"),
    (f"{SAVANNA}/Textures/Surfaces", f"{SAVANNA}/Surfaces/Textures"),
)
# Nature Essentials textures carry the pack's file names; each is used by one material, so it is named after it.
NATURE_ESSENTIALS = {
    "Branches_49a21": "NE_Branch", "CreepingBentgrass": "NE_Grass", "Debris_owqi2": "NE_Stick", "Moss_1": "NE_MossDead",
    "QuercusRobur_8s3fd": "NE_OakSeedling", "QuercusRobur_fg96e": "NE_OakLeaf",
    "Rock_1": "NE_RockLight_A", "rock1H": "NE_RockLight_A", "rock_1_loop": "NE_RockLight_A",
    "Rock2H": "NE_RockLight_B", "Rock_2": "NE_RockLight_B",
    "Rock3H": "NE_RockDark_B", "Rock_3": "NE_RockDark_B",
    "Rock4H": "NE_RockDark_A", "rock_4H": "NE_RockDark_A", "Rock_4": "NE_RockDark_A",
}
# Pack role words, including the rocks' run-together ones (Rock2HNormal, Rock3H_LOD0_u0_v0 is the colour).
NE_ROLES = (("AmbientOcclusion", "AO"), ("NormalOpenGL", "N"), ("Normal", "N"), ("NRM", "N"), ("Albedo", "D"),
            ("COLOR", "D"), ("LOD0", "D"), ("loop", "D"), ("Opacity", "A"), ("Roughness", "R"), ("rough", "R"),
            ("H_AO", "AO"))
TREE_KINDS = {"Near": 0, "Outer": 12, "Backdrop": 28}  # First number of each kind in the old single sequence.
# Deleted rather than renamed: unused. The PNGs beside them stay (Build.cs stages them).
DELETE = tuple(f"/Game/UI/Clubs/{club}_club_face" for club in ("driver", "iron", "putter", "wood"))


def nature_essentials_texture(name):
    """T_NE_Branches_49a21_Albedo_8bit_4K -> T_NE_Branch_D."""
    stem = name[len("T_NE_"):]
    source = max((s for s in NATURE_ESSENTIALS if stem.startswith(s)), key=len, default=None)
    if not source:
        raise ValueError(f"Unknown Nature Essentials texture {name}")
    rest = stem[len(source):]
    suffix = next((s for word, s in NE_ROLES if word.lower() in rest.lower()), None)
    if not suffix:
        raise ValueError(f"No role in {name}")
    return f"T_{NATURE_ESSENTIALS[source]}_{suffix}"


def renamed(name, folder):
    """New asset name by rule, or name unchanged."""
    if name.startswith("T_NE_"):
        return nature_essentials_texture(name) if names.problems(name) else name
    if name.startswith("T_") and names.problems(name):
        return names.source_texture(name[2:])
    match = re.fullmatch(r"(M|MI|DA)_Coastal(\w+)", name)
    if match:
        kind = {"M": "M_Terrain", "MI": "MI_TerrainSurface_", "DA": "DA_TerrainSurface_"}[match[1]]
        return kind + match[2]
    match = re.fullmatch(r"DA_SavannaTree(Near|Outer|Backdrop)(\d\d)", name)
    if match:
        return f"DA_SavannaTree_{match[1]}_{int(match[2]) - TREE_KINDS[match[1]]:02d}"
    match = re.fullmatch(r"SM_Surface_(\d)", name)
    if match:
        return f"SM_Surface_{int(match[1]):02d}"
    match = re.fullmatch(r"MI_JacarandaTree(Trunk|Branches|Leaves)", name)
    if match:
        return f"MI_Jacaranda_{match[1]}"
    match = re.fullmatch(r"MI_IslandTree02(Branches|Leaves)?", name)
    if match:
        return f"MI_IslandTree02_{match[1] or 'Trunk'}"
    if name.startswith("SM_"):
        return names.mesh(name)
    return name


def move(path):
    """New package path for an existing one, or the same path."""
    if path in EXPLICIT:
        return EXPLICIT[path]
    folder, name = path.rsplit("/", 1)
    for old, new in FOLDERS:
        if folder == old or folder.startswith(old + "/"):
            folder = new + folder[len(old):]
            break
    return f"{folder}/{renamed(name, folder)}"


def skipped(path):
    """Packages this rename leaves alone: Hunter Ranch's rebuilt course and generated vegetation runs (cleaned instead)."""
    return path.startswith("/HunterRanch/Course/") or "/Vegetation_" in path


def plan(packages):
    """{old package: new package} for every package that moves. Raises if two would land on one path."""
    result = {}
    for path in sorted(packages):
        if skipped(path) or path in DELETE:
            continue
        target = move(path)
        if target != path:
            result[path] = target
    targets = {}
    unmoved = set(packages) - set(result) - set(DELETE)
    for old, new in result.items():
        clash = targets.get(new) or (new if new in unmoved else None)
        if clash:
            raise ValueError(f"{old} and {clash} would both become {new}")
        targets[new] = old
    return result


# Actor labels in saved maps. Generated actors already get these labels from the builders; this relabels the ones
# saved before (Docs/ContentNaming.md#actors).
CLASS_LABELS = {"UOpenGolfRange": "RangeSettings", "OpenGolfEnvironment": "Environment_Daylight"}
# Labels many actors share; the relabel numbers them _01, _02, ... in a stable order, after any already numbered.
NUMBERED = ("Trees", "Vegetation", "CourseGrass")


def number_labels(labels, existing):
    """Numbers each shared label (NUMBERED) in order, continuing after the numbers existing labels already use."""
    counts = {}
    for label in labels:
        counts[label] = counts.get(label, 0) + 1
    next_number = {}
    for label in NUMBERED:
        used = [int(m[1]) for e in existing if (m := re.fullmatch(re.escape(label) + r"_(\d+)", e))]
        next_number[label] = max(used, default=0) + 1
    result = []
    for label in labels:
        if label in NUMBERED and (counts[label] > 1 or next_number[label] > 1):
            result.append(f"{label}_{next_number[label]:02d}")
            next_number[label] += 1
        else:
            result.append(label)
    return result


def actor_label(label, kind, surface=None, course=None):
    """New label for an actor with label, class name kind, the OpenFairway surface its ground shows (if any) and the
    course the map belongs to. Returns the label unchanged when it already follows the rules."""
    match = re.fullmatch(r"Coastal ground (west|east|south|north)", label)
    if match:
        return names.kind_label("Landscape", match[1].capitalize())
    if re.fullmatch(r"Terrain DA_\w+", label) and surface:
        return f"SM_{surface}_Ground"
    match = re.fullmatch(r"Tree colliders (\w+)", label)
    if match:
        return names.kind_label("Colliders", names.without_prefix(renamed("DA_" + match[1], "")))
    match = re.fullmatch(r"Tree canopy (SM_\w+)", label)
    if match:
        return names.kind_label("Canopy", names.without_prefix(match[1]))
    match = re.fullmatch(r"PCG (.+)", label)
    if match:
        return names.kind_label("PCG", names.without_prefix(renamed(match[1], "")))
    # The editor numbers duplicate labels itself ("Coastal trees2"); an earlier relabel may have kept that form.
    for pattern, shared in ((r"Coastal[ _]trees\d*", "Trees"), (r"Coastal[ _]vegetation\d*", "Vegetation"),
                            (r"Course[ _]grass\d*", "CourseGrass")):
        if re.fullmatch(pattern, label, re.IGNORECASE):
            return shared
    fixed = {"Savanna understory": "Vegetation_SavannaUnderstory", "Course Environment": "Environment_Daylight"}
    if label in fixed:
        return fixed[label]
    if kind == "OpenGolfTerrainArea" and course:
        return names.kind_label("TerrainArea", course)
    if kind in CLASS_LABELS and (label == kind or " " in label):
        return CLASS_LABELS[kind]
    if " " in label:
        words = label.split()
        return names.kind_label(names.pascal(words[0]), " ".join(words[1:]))
    return label
