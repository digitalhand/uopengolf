# SPDX-License-Identifier: MIT
"""Unreal asset and actor names for uopengolf, shared by every generator, importer and check.

Rules (Docs/ContentNaming.md): [Prefix]_[AssetName]_[Descriptor]_[Variant]. Segments are PascalCase letters and
digits, variants are two digits (_01), a texture ends with its role suffix (_D, _N, _R, ...). A placed actor is
labelled like the asset it shows (SM_, BP_) or, for an actor with no asset, by its kind (Landscape_West).

This module imports nothing from Unreal, so it runs in plain Python tests and in the editor alike.
"""
import re

# Asset class name -> prefix. Epic's table, plus common practice where Epic lists none (L_, DA_, MF_, MPC_, PCG_).
CLASS_PREFIXES = {
    "Blueprint": "BP_",
    "BlueprintGeneratedClass": "BP_",
    "CurveTable": "CT_",
    "DataTable": "DT_",
    "Material": "M_",
    "MaterialFunction": "MF_",
    "MaterialInstanceConstant": "MI_",
    "MaterialParameterCollection": "MPC_",
    "NiagaraEmitter": "FXE_",
    "NiagaraSystem": "FXS_",
    "PCGGraph": "PCG_",
    "PhysicalMaterial": "PM_",
    "OpenFairwayPhysicalMaterial": "PM_",
    "PhysicsAsset": "PHYS_",
    "SkeletalMesh": "SK_",
    "Skeleton": "SKEL_",
    "StaticMesh": "SM_",
    "Texture2D": "T_",
    "TextureCube": "HDR_",
    "UserDefinedEnum": "E_",
    "UserDefinedStruct": "F_",
    "WidgetBlueprint": "WBP_",
    "World": "L_",
    # Data assets: any UDataAsset subclass the project defines, and PCG point data.
    "DataAsset": "DA_",
    "PrimaryDataAsset": "DA_",
    "GolfCourseDefinition": "DA_",
    "OpenGolfEnvironmentProfile": "DA_",
    "OpenGolfSurfacePreset": "DA_",
    "OpenGolfTerrainDefinition": "DA_",
    "OpenGolfVegetationPreset": "DA_",
    "PCGDataAsset": "DA_",
}
PREFIXES = tuple(sorted(set(CLASS_PREFIXES.values()), key=len, reverse=True))

# A texture's last segment says what it holds.
TEXTURE_SUFFIXES = ("D", "N", "R", "A", "AO", "M", "E", "H", "ORM", "Mask")
# Source naming schemes (Poly Haven, ambientCG, Nature Essentials, Substance) -> suffix.
_SOURCE_ROLES = {
    "albedo": "D", "basecolor": "D", "basecolour": "D", "col": "D", "color": "D", "colour": "D", "diff": "D",
    "diffuse": "D",
    "nor": "N", "norgl": "N", "nordx": "N", "normal": "N", "normalgl": "N", "normaldx": "N", "normalopengl": "N",
    "nrm": "N",
    "rough": "R", "roughness": "R",
    "alpha": "A", "opacity": "A",
    "ao": "AO", "ambientocclusion": "AO", "occlusion": "AO",
    "metal": "M", "metallic": "M", "metalness": "M",
    "emissive": "E", "emission": "E",
    "disp": "H", "displacement": "H", "height": "H",
    "arm": "ORM", "orm": "ORM",
    "mask": "Mask",
}

SEGMENT = re.compile(r"[A-Z0-9][A-Za-z0-9]*")
# Tile grid coordinates are the one place a segment may be negative.
COORDINATE = re.compile(r"-?\d+")
GUID = re.compile(r"[0-9A-F]{32}")
# Download details that say nothing about the asset: 2K, 8bit, JPG, LOD0.
SOURCE_TOKEN = re.compile(r"\d+[kK]|\d+bit|JPG|PNG|EXR|TGA|LOD\d+", re.IGNORECASE)
FORBIDDEN_WORDS = ("draft", "coastal")

# Generated content of a downloadable course sits under its course folder, apart from the authored assets.
GENERATED = "Generated"
TILE_FOLDER = "Tile"
WATER_FOLDER = "Water"


def pascal(text):
    """PascalCase of free text: "island_tree_02" -> "IslandTree02", "red sand" -> "RedSand"."""
    words = [w for w in re.split(r"[^A-Za-z0-9]+", str(text)) if w]
    return "".join(w[:1].upper() + w[1:] for w in words)


def variant(number):
    """Two-digit variant: 1 -> "01", 12 -> "12"."""
    if number < 0:
        raise ValueError("A variant number is never negative.")
    return f"{number:02d}"


def segment(text):
    """PascalCase segments from free text, keeping underscores: "turf fairway 0.5m" -> "TurfFairway05m",
    "SavannaTree_Near_00" stays. Matches OpenGolfAssetNames::Segment in C++."""
    return "_".join(p for p in (pascal(part) for part in str(text).split("_")) if p)


def without_prefix(name):
    """DA_SavannaTree_Near_00 -> SavannaTree_Near_00; a name without a type prefix is returned as is."""
    match = re.fullmatch(r"([A-Z]{1,4})_(.+)", name)
    return match[2] if match else name


def kind_label(kind, descriptor=""):
    """Label of an actor that shows no single asset: Landscape_West, Colliders_SavannaTree_Near_00."""
    clean = segment(descriptor)
    return f"{kind}_{clean}" if clean else kind


def join(prefix, *parts):
    """prefix + parts joined by underscores; each part must already be a valid segment."""
    if prefix not in PREFIXES:
        raise ValueError(f"Unknown asset prefix {prefix!r}")
    for part in parts:
        if not (SEGMENT.fullmatch(str(part)) or COORDINATE.fullmatch(str(part))):
            raise ValueError(f"{part!r} is not a PascalCase segment")
    return prefix + "_".join(str(p) for p in parts)


def texture_suffix(role):
    """Suffix for a source texture role such as "nor_gl", "NormalDX" or "diff"; None if unknown."""
    return _SOURCE_ROLES.get(re.sub(r"[^a-z]", "", str(role).lower()))


def texture(name, role, *descriptors):
    """T_<Name>_<Descriptors>_<Suffix>, for example texture("Jacaranda", "diff", "Leaves") -> T_Jacaranda_Leaves_D."""
    suffix = role if role in TEXTURE_SUFFIXES else texture_suffix(role)
    if not suffix:
        raise ValueError(f"Unknown texture role {role!r}")
    return join("T_", pascal(name), *[pascal(d) for d in descriptors if d], suffix)


# Source material and file names (Poly Haven, Blender exports) -> asset name and part. The longest match wins, so
# island_tree_02_leaves is the leaves and island_tree_02 alone is the trunk.
SOURCE_ASSETS = {
    "jacaranda_tree_trunk": "Jacaranda_Trunk",
    "jacaranda_tree_branches": "Jacaranda_Branches",
    "jacaranda_tree_leaves": "Jacaranda_Leaves",
    "island_tree_02": "IslandTree02_Trunk",
    "island_tree_02_branches": "IslandTree02_Branches",
    "island_tree_02_leaves": "IslandTree02_Leaves",
    "grass_medium_01": "GrassMedium01",
}
# Mesh parts an exporter names after its tree: SM_JacarandaCanopy -> SM_Jacaranda_Canopy.
MESH_PARTS = ("Canopy", "Collision")


def source_asset(name):
    """(asset name, rest of the words) for a source name: "grass_medium_01_dry" -> ("GrassMedium01", ["dry"])."""
    words = [w for w in re.split(r"[^A-Za-z0-9]+", name) if w]
    for count in range(len(words), 0, -1):
        known = SOURCE_ASSETS.get("_".join(words[:count]).lower())
        if known:
            return known, words[count:]
    return None, words


def material_instance(material):
    """MI_ name for a source material slot: "jacaranda_tree_leaves" -> MI_Jacaranda_Leaves, "rock" -> MI_Rock."""
    known, rest = source_asset(material)
    return "MI_" + "_".join([known] + [pascal(w) for w in rest]) if known else "MI_" + pascal(material)


def source_texture(stem, prefix="T_"):
    """Texture name for a source file stem: "jacaranda_tree_leaves_nor_gl_2k" -> T_Jacaranda_Leaves_N,
    "Rock029_2K-JPG_NormalDX" -> T_Rock029_N, "grass_medium_01_dry_diff_2k" -> T_GrassMedium01_Dry_D."""
    words = [w for w in re.split(r"[^A-Za-z0-9]+", stem) if w and not SOURCE_TOKEN.fullmatch(w)]
    suffix = None
    for count in (2, 1):
        tail = words[-count:]
        if len(words) > count and all(w.isalpha() for w in tail) and "".join(tail).lower() in _SOURCE_ROLES:
            suffix = _SOURCE_ROLES["".join(tail).lower()]
            words = words[:-count]
            break
    if not suffix:
        raise ValueError(f"No texture role in {stem!r}")
    known, rest = source_asset("_".join(words))
    # An unknown source keeps its words together as one name: leafy_grass -> LeafyGrass.
    parts = [known] + [pascal(w) for w in rest] if known else [pascal("_".join(words))]
    return prefix + "_".join(parts + [suffix])


def mesh(name):
    """A mesh name with its part separated: SM_IslandTree02Canopy -> SM_IslandTree02_Canopy."""
    for part in MESH_PARTS:
        if name.endswith(part) and not name.endswith("_" + part) and len(name) > len("SM_" + part):
            return name[: -len(part)] + "_" + part
    return name


# Downloadable course content. root is the course's content folder, such as /HunterRanch/Course.
def course_map(course):
    return join("L_", pascal(course))


def course_definition(course):
    """DA_<Course>; also the course's primary asset ID and ?Course= key, so keep it stable once published."""
    return join("DA_", pascal(course))


def course_terrain(course):
    return join("DA_", pascal(course), "Terrain")


def generated_tiles(root):
    return f"{root}/{GENERATED}/Tiles"


def generated_water(root):
    return f"{root}/{GENERATED}/Water"


def tile_tag(x, y):
    """Actor tag that keys a ground square's rebuild."""
    return f"{TILE_FOLDER}_{x}_{y}"


def tile_mesh(surface, x, y):
    """SM_<Surface>_Tile_<x>_<y>; the actor showing it carries the same label."""
    return join("SM_", pascal(surface), TILE_FOLDER, x, y)


def lake(number):
    """Name of the n-th lake (1-based): Lake_01. Its mesh and actor are SM_Lake_01."""
    return f"Lake_{variant(number)}"


def problems(name, prefixes=PREFIXES):
    """Reasons name breaks the rules (empty when it follows them). For an asset or actor label."""
    found = []
    if " " in name:
        found.append("contains a space")
    lower = name.lower()
    for word in FORBIDDEN_WORDS:
        if word in lower:
            found.append(f"contains '{word}'")
    prefix = next((p for p in prefixes if name.startswith(p)), None)
    if prefix is None:
        found.append("has no known type prefix")
        return found
    parts = name[len(prefix):].split("_")
    if not parts[0]:
        found.append("has no name after its prefix")
        return found
    for index, part in enumerate(parts):
        # The two segments after "Tile" are grid coordinates: any width, and negative west or south of the origin.
        coordinate = TILE_FOLDER in parts[max(0, index - 2):index]
        if GUID.fullmatch(part) or (coordinate and COORDINATE.fullmatch(part)):
            continue
        if not SEGMENT.fullmatch(part):
            found.append(f"segment '{part}' is not PascalCase")
        elif part.isdigit() and len(part) == 1:
            found.append(f"variant '{part}' is not two digits")
        if SOURCE_TOKEN.fullmatch(part):
            found.append(f"segment '{part}' is a source file detail (resolution, bit depth, format or LOD)")
    if prefix == "T_" and source_role(parts[-1]):
        found.append(f"texture role '{parts[-1]}' should be the suffix _{source_role(parts[-1])}")
    return found


def source_role(segment):
    """The standard suffix when segment names a texture role in a source's own words ("Color", "Rock2HNormal")."""
    if segment in TEXTURE_SUFFIXES:
        return None
    exact = texture_suffix(segment)
    if exact and exact != "Mask":
        return exact
    lower = segment.lower()
    return next((suffix for word, suffix in _SOURCE_ROLES.items()
                 if len(word) >= 5 and suffix != "Mask" and lower.endswith(word) and lower != word), None)
