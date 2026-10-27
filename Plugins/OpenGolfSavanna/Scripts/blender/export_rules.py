# SPDX-License-Identifier: MIT
"""Rules shared by the Blender export scripts. No bpy here, so they can be unit-tested."""

# Texture sources committed to the repository stay at these sizes; an imported texture asset
# keeps its whole source image, so Unreal-side caps would not shrink the repository.
EXPORT_LIMITS = {"albedo": 2048, "albedo_dry": 2048, "opacity_alpha": 2048, "normal": 2048, "roughness": 1024, "ao": 1024, "opacity": 1024}


def is_colour(role):
    """True for a colour map, written as sRGB; normal, roughness, AO and opacity maps are data, written raw."""
    return role.startswith("albedo") or role == "opacity_alpha"


def export_size(width, height, limit):
    """The longest side at most `limit`, keeping the aspect ratio."""
    scale = min(1.0, limit / max(width, height))
    return max(1, round(width * scale)), max(1, round(height * scale))


_PRINCIPLED = {"Base Color": "albedo", "Roughness": "roughness", "Alpha": "opacity"}
# Poly Haven's leaf material wraps its BSDF in a node group with these inputs.
_GROUP = {"Diffuse": "albedo", "Rough": "roughness", "Normal": "normal", "Alpha": "opacity"}


def principled_roles(links):
    """Texture roles in a Principled BSDF (or Poly Haven group) graph, from 'Output->NodeType.Socket' links."""
    roles = set()
    for link in links:
        target = link.split("->", 1)[1]
        node_type, socket = target.split(".", 1)
        if node_type == "ShaderNodeNormalMap":
            roles.add("normal")
        elif node_type in ("ShaderNodeBsdfPrincipled", "ShaderNodeGroup"):
            role = (_PRINCIPLED if node_type == "ShaderNodeBsdfPrincipled" else _GROUP).get(socket)
            if role:
                roles.add(role)
    return roles


def faces_with_materials(face_materials, wanted):
    """Indices of the faces whose material name is in `wanted`, in face order."""
    return [index for index, name in enumerate(face_materials) if name in wanted]


def farthest_points(points, count):
    """Up to `count` of `points` spread as far apart as possible, starting from the highest one.

    Deterministic for the same input, so repeated exports give the same canopy hull."""
    points = [tuple(p) for p in points]
    if len(points) <= count:
        return points
    start = max(range(len(points)), key=lambda i: (points[i][2], points[i]))
    chosen = [points[start]]
    nearest = [sum((a - b) ** 2 for a, b in zip(p, chosen[0])) for p in points]
    while len(chosen) < count:
        index = max(range(len(points)), key=lambda i: (nearest[i], points[i]))
        chosen.append(points[index])
        nearest = [min(d, sum((a - b) ** 2 for a, b in zip(p, points[index]))) for d, p in zip(nearest, points)]
    return chosen
