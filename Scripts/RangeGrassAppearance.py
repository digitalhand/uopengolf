# SPDX-License-Identifier: MIT
"""Set the range grass palette using explicit sRGB hex colors in Unreal Python.

Run with -run=pythonscript -script=<absolute path to this file>. The importer also
calls apply_grass_colors() when generating assets. Existing palette edits are kept
unless reset=True is explicitly requested (as it is for direct execution).
"""
import unreal as ue

DARK_HEX = "406B1F"
LIGHT_HEX = "4D7E20"
PALETTE_VERSION = "UOpenGolf.GrassPalette.v2"


def linear_color(hex_rgb):
    """Hex colors are sRGB; material vector parameters store linear RGB."""
    def channel(offset):
        value = int(hex_rgb[offset:offset + 2], 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return ue.LinearColor(*(channel(offset) for offset in (0, 2, 4)), 1.0)


def apply_grass_colors(reset=False):
    library = ue.EditorAssetLibrary
    editing = ue.MaterialEditingLibrary
    for name in ("M_Fairway", "M_GrassBlades"):
        material = library.load_asset(f"/Game/UOpenGolf/Art/{name}")
        if not material or library.get_metadata_tag(material, "Generator") != "UOpenGolf.ImportRangeAssets.v1":
            raise RuntimeError(f"Missing or unrelated range material: {name}")
        if library.get_metadata_tag(material, "GrassPalette") == PALETTE_VERSION and not reset:
            continue

        expressions = list(editing.get_material_expressions(material))

        def parameter(parameter_name, hex_rgb):
            names = (parameter_name, "GrassTint") if parameter_name == "GrassDarkColor" else (parameter_name,)
            expr = next((expr for expr in expressions
                         if isinstance(expr, ue.MaterialExpressionVectorParameter)
                         and str(expr.get_editor_property("parameter_name")) in names), None)
            if not expr:
                expr = editing.create_material_expression(material, ue.MaterialExpressionVectorParameter)
            expr.set_editor_property("parameter_name", parameter_name)
            expr.set_editor_property("group", "Grass appearance")
            expr.set_editor_property("default_value", linear_color(hex_rgb))
            return expr

        dark = parameter("GrassDarkColor", DARK_HEX)
        light = parameter("GrassLightColor", LIGHT_HEX)
        is_fairway = name == "M_Fairway"
        palette = next((expr for expr in expressions
                        if isinstance(expr, ue.MaterialExpressionCustom)
                        and (expr.get_editor_property("desc") == PALETTE_VERSION
                             or (is_fairway and "float stripe =" in expr.get_editor_property("code"))
                             or (not is_fairway and "return lerp(" in expr.get_editor_property("code")))), None)
        if not palette:
            raise RuntimeError(f"Missing grass color expression: {name}")

        inputs = list(palette.get_editor_property("inputs"))
        for input_name in ("DarkGreen", "LightGreen") + (() if is_fairway else ("P",)):
            if not any(str(pin.get_editor_property("input_name")) == input_name for pin in inputs):
                pin = ue.CustomInput()
                pin.set_editor_property("input_name", input_name)
                inputs.append(pin)
        palette.set_editor_property("inputs", inputs)
        palette.set_editor_property("desc", PALETTE_VERSION)

        # Retain mowing and small surface variations, bounded by the requested
        # colors. The original texture now supplies detail rather than hue.
        detail = ("float detail = (saturate(dot(C, float3(0.2126,0.7152,0.0722))*2.0)-0.5)*0.12;"
                  if is_fairway else "float detail = (R-0.5)*0.08;")
        palette.set_editor_property("code",
            "float stripe = smoothstep(-0.12, 0.12, sin(P.x * 0.00392699));\n"
            + detail + "\nreturn lerp(DarkGreen, LightGreen, saturate(stripe + detail));")
        if not is_fairway:
            position = next((expr for expr in expressions if isinstance(expr, ue.MaterialExpressionWorldPosition)), None)
            position = position or editing.create_material_expression(material, ue.MaterialExpressionWorldPosition)
            if not editing.connect_material_expressions(position, "", palette, "P"):
                raise RuntimeError("Could not connect blade world position")
        if not (editing.connect_material_expressions(dark, "RGB", palette, "DarkGreen")
                and editing.connect_material_expressions(light, "RGB", palette, "LightGreen")
                and editing.connect_material_property(palette, "", ue.MaterialProperty.MP_BASE_COLOR)):
            raise RuntimeError(f"Could not connect grass palette: {name}")
        editing.recompile_material(material)
        library.set_metadata_tag(material, "GrassPalette", PALETTE_VERSION)
        if not library.save_loaded_asset(material, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save grass palette: {name}")
        for parameter_name, hex_rgb in (("GrassDarkColor", DARK_HEX), ("GrassLightColor", LIGHT_HEX)):
            actual = editing.get_material_default_vector_parameter_value(material, parameter_name)
            expected = linear_color(hex_rgb)
            if any(abs(getattr(actual, channel) - getattr(expected, channel)) > 1e-6 for channel in ("r", "g", "b")):
                raise RuntimeError(f"Palette verification failed: {name}.{parameter_name}")
        ue.log(f"Verified {name}: dark #{DARK_HEX}, light #{LIGHT_HEX} (sRGB, stored as linear RGB)")


if __name__ == "__main__":
    apply_grass_colors(reset=True)
