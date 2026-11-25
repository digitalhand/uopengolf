# SPDX-License-Identifier: MIT
"""The course kit's lake surface material, M_Water: procedural, with no textures.

A still pond reads mostly as reflected sky, which the renderer supplies from the low roughness; the base
colour is the dark green-brown of an inland irrigation lake seen straight down. Rebuilding keeps the node
graph and reapplies these defaults, like the other course kit builders.
"""
import unreal as ue

import course_assets as assets

FOLDER = "Materials"
NAME = "M_Water"
DEEP_COLOUR = "26392F"
EDGE_COLOUR = "4A5A3E"


def material():
    """M_Water, created or updated, compiled and saved."""
    water = assets.material(FOLDER, NAME)
    deep = assets.vector(water, "DeepColour", assets.linear_color(DEEP_COLOUR), "Water", 0)
    edge = assets.vector(water, "GrazingColour", assets.linear_color(EDGE_COLOUR), "Water", 1)
    fresnel = assets.node(water, ue.MaterialExpressionFresnel, "Grazing", exponent=4.0)
    colour = assets.node(water, ue.MaterialExpressionLinearInterpolate, "Colour")
    assets.connect(deep, colour, "A")
    assets.connect(edge, colour, "B")
    assets.connect(fresnel, colour, "Alpha")
    assets.output(water, colour, ue.MaterialProperty.MP_BASE_COLOR)
    assets.output(water, assets.scalar(water, "Roughness", 0.05, "Water", 2), ue.MaterialProperty.MP_ROUGHNESS)
    assets.output(water, assets.scalar(water, "Specular", 0.5, "Water", 3), ue.MaterialProperty.MP_SPECULAR)
    return assets.finish_material(water)
