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


# Rec. 709 weights, for linear RGB.
LUMA = (0.2126, 0.7152, 0.0722)


def luminance(rgb):
    return sum(weight * channel for weight, channel in zip(LUMA, rgb))


def chromatic_tint(source_rgb, target_rgb, low=0.5, high=1.6):
    """Per-channel multiplier (linear RGB) that gives source the target's hue and saturation at the source's own
    brightness, for a material Tint. Each channel stays within [low, high] before the brightness is restored, so a
    far-off target only moves the colour part of the way. A photo's brightness depends on its light, a texture's
    does not, so only the colour is taken from the target."""
    if any(channel <= 0 for channel in source_rgb) or luminance(target_rgb) <= 0:
        raise ValueError("Both colours need light in every source channel")
    source_luma, target_luma = luminance(source_rgb), luminance(target_rgb)
    tint = [min(high, max(low, (t / target_luma) / (s / source_luma))) for s, t in zip(source_rgb, target_rgb)]
    scale = source_luma / luminance([s * k for s, k in zip(source_rgb, tint)])
    return tuple(round(k * scale, 4) for k in tint)


def fit_saturation(source_rgb, tint, target_rgb):
    """Saturation (0-1) that takes source x tint closest to the target's colour, for a material that blends a colour
    toward its own grey: c' = luma + s (c - luma). It completes a clamped tint, and turns a bright texture into a
    dull foliage colour, without multiplying a weak channel further."""
    tinted = [s * k for s, k in zip(source_rgb, tint)]
    tinted_luma, target_luma = luminance(tinted), luminance(target_rgb)
    if tinted_luma <= 0 or target_luma <= 0:
        raise ValueError("Both colours need light")
    have = [c / tinted_luma - 1 for c in tinted]
    want = [c / target_luma - 1 for c in target_rgb]
    spread = sum(h * h for h in have)
    if spread < 1e-12:
        return 1.0
    return round(min(1.0, max(0.0, sum(h * w for h, w in zip(have, want)) / spread)), 4)
