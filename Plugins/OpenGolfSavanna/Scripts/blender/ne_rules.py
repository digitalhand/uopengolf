# SPDX-License-Identifier: MIT
"""Which Nature Essentials pieces the savanna understory uses, and how they are prepared. No bpy here."""
import math
import random
import re

# One Unreal mesh each. `collection` names the source pack's LOD collection, `pick` the object
# (sorted by name) inside it. `material` is the clean slot name shared by every mesh that uses it.
ASSETS = [
    dict(name="SM_NE_GrassClump", collection="Grass Low", pick=0, material="NE_Grass", kind="grass", max_tris=2000),
    dict(name="SM_NE_GrassSeedy", collection="Grass Seedy Low", pick=0, material="NE_Grass", kind="grass", max_tris=4000),
    dict(name="SM_NE_GrassSmall", collection="Grass Small Low", pick=0, material="NE_Grass", kind="grass", max_tris=2500),
    *[dict(name=f"SM_NE_MossDead_{s}", collection="Moss Dead Low", pick=i, material="NE_MossDead", kind="card", max_tris=2500)
      for i, s in enumerate("ABCD")],
    dict(name="SM_NE_RockDark_A", collection="Dark Rocks High", pick=0, material="NE_RockDark_A", kind="rock", max_tris=3000),
    dict(name="SM_NE_RockDark_B", collection="Dark Rocks High", pick=1, material="NE_RockDark_B", kind="rock", max_tris=3000),
    dict(name="SM_NE_RockLight_A", collection="Light Rocks High", pick=0, material="NE_RockLight_A", kind="rock", max_tris=3000),
    dict(name="SM_NE_RockLight_B", collection="Light Rocks High", pick=1, material="NE_RockLight_B", kind="rock", max_tris=3000),
    *[dict(name=f"SM_NE_Seedling_{s}", collection="Oak Small High", pick=i, material="NE_OakSeedling", kind="card", max_tris=1000)
      for i, s in enumerate("ABC")],
]

# Leaf litter patches built from single leaves, twigs and small branches.
LITTER = dict(
    variants="ABCD",
    radius_m=0.6,
    max_tilt_deg=10.0,
    counts={"leaf": 70, "stick": 12, "branch": 4},
    sources={"leaf": ("Oak Leaves High", "NE_OakLeaf"), "stick": ("Sticks High", "NE_Stick"),
             "branch": ("Branches Low", "NE_Branch")},
)

# Kept in the project for the turf tile renders; not exported as meshes.
TURF_SOURCES = ["Grass Cut High", "Grass Cut Low"]

_EXTENSIONS = {"PNG": ".png", "JPEG": ".jpg", "TARGA": ".tga", "OPEN_EXR": ".exr", "TIFF": ".tif"}


def texture_filename(image_name, file_format):
    """A stable file name for a packed image: Blender's .001 copy suffixes dropped, spaces replaced."""
    base = re.sub(r"(\.\d{3})+$", "", image_name)
    extension = _EXTENSIONS.get(file_format, ".png")
    stem = base[: -len(extension)] if base.lower().endswith(extension) else re.sub(r"\.(png|jpe?g|tga|exr|tif)$", "", base, flags=re.I)
    return stem.replace(" ", "_") + extension


def image_roles(links):
    """Texture roles from an image node's outgoing links, written as 'Output->NodeType:Node.Socket'."""
    roles = set()
    for link in links:
        output, target = link.split("->", 1)
        node_type, rest = target.split(":", 1)
        socket = rest.rsplit(".", 1)[1]
        if output == "Alpha" and node_type == "ShaderNodeMixShader":
            roles.add("opacity_alpha")
        elif node_type == "ShaderNodeMixShader":
            roles.add("opacity")
        elif node_type == "ShaderNodeNormalMap":
            roles.add("normal")
        elif node_type == "ShaderNodeValToRGB" or (node_type == "ShaderNodeBsdfPrincipled" and socket == "Roughness"):
            roles.add("roughness")
        elif node_type == "ShaderNodeBsdfPrincipled" and socket == "Base Color":
            roles.add("albedo")
        elif node_type == "ShaderNodeMix" and socket == "A":
            roles.add("albedo")
        elif node_type == "ShaderNodeMix" and socket == "B":
            roles.add("ao")
    return roles


# Committed texture sources stay at these sizes; an imported texture asset keeps its whole source.
EXPORT_TEXTURE_LIMIT = 2048
EXPORT_LIMITS = {"albedo": 2048, "opacity_alpha": 2048, "normal": 2048, "roughness": 1024, "ao": 1024, "opacity": 1024}


def export_size(width, height, limit=EXPORT_TEXTURE_LIMIT):
    """Texture size for Unreal: the longest side at most `limit`, keeping the aspect ratio."""
    scale = min(1.0, limit / max(width, height))
    return max(1, round(width * scale)), max(1, round(height * scale))


def decimate_ratio(triangles, budget):
    return 1.0 if triangles <= budget else budget / triangles


def litter_layout(seed, pools=None):
    """Pieces of one litter patch as (kind, variant, x, y, z, yaw, tilt), denser toward the centre."""
    rng = random.Random(seed)
    pools = pools or {}
    radius = LITTER["radius_m"]
    tilt_limit = math.radians(LITTER["max_tilt_deg"])
    pieces = []
    for kind, count in LITTER["counts"].items():
        for _ in range(count):
            distance = radius * rng.random() ** 0.75
            angle = rng.random() * math.tau
            z = 0.002 + rng.random() * 0.012 if kind == "leaf" else 0.004
            pieces.append((kind, rng.randrange(pools.get(kind, 1000)), distance * math.cos(angle),
                           distance * math.sin(angle), z, rng.random() * math.tau, rng.uniform(-tilt_limit, tilt_limit)))
    return pieces
