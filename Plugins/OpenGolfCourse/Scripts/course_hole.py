# SPDX-License-Identifier: MIT
"""Editor helpers that apply a hole layout (hole_layout.py geometry) to a terrain definition and stand the flag at its pin.

A course describes its hole as a pure function, apply_layout(regions, mounds) -> (regions, mounds), over
{name: [(x, y)]} outlines and (cx, cy, rx, ry, height) mounds in terrain-local metres, and tests it under plain
Python. These helpers move that layout in and out of UOpenGolfTerrainDefinition.
"""
import unreal as ue


def read_layout(definition):
    regions = {str(r.name): [(p.x, p.y) for p in r.outline_m] for r in definition.get_editor_property("regions")}
    mounds = [(m.center_m.x, m.center_m.y, m.radius_m.x, m.radius_m.y, m.height_m) for m in definition.get_editor_property("mounds")]
    return regions, mounds


def write_layout(definition, regions, mounds, templates=None):
    """Writes outlines and mounds back. A region new to the definition copies the surface and spacing of the region
    named by templates[name] and is added after the existing ones, so it draws over them."""
    existing = list(definition.get_editor_property("regions"))
    by_name = {str(r.name): r for r in existing}
    for name in regions:
        if name in by_name:
            continue
        template = by_name.get((templates or {}).get(name))
        if not template:
            raise RuntimeError(f"New region {name} needs a template region")
        region = ue.OpenGolfTerrainRegion()
        region.set_editor_property("name", name)
        region.set_editor_property("surface", template.surface)
        region.set_editor_property("spacing_m", template.spacing_m)
        existing.append(region)
        by_name[name] = region
    for name, outline in regions.items():
        by_name[name].set_editor_property("outline_m", [ue.Vector2D(x, y) for x, y in outline])
    terrain_mounds = []
    for cx, cy, rx, ry, height in mounds:
        mound = ue.OpenGolfTerrainMound()
        mound.set_editor_property("center_m", ue.Vector2D(cx, cy))
        mound.set_editor_property("radius_m", ue.Vector2D(rx, ry))
        mound.set_editor_property("height_m", height)
        terrain_mounds.append(mound)
    definition.set_editor_property("mounds", terrain_mounds)
    definition.set_editor_property("regions", existing)


def lay_out(definition, apply_layout, templates=None):
    """Runs a course's apply_layout over the definition and writes the result back."""
    regions, mounds = apply_layout(*read_layout(definition))
    write_layout(definition, regions, mounds, templates)


def place_flag(range_actor, area, pin):
    """Stands the range's Flagstick layout actor at pin (terrain-local metres) on the ground."""
    flags = [a for a in range_actor.get_editor_property("layout_actors") if a and a.get_actor_label() == "Flagstick"]
    if len(flags) != 1:
        raise RuntimeError("Expected one Flagstick layout actor")
    at = area.get_actor_transform().transform_location(ue.Vector(pin[0] * 100, pin[1] * 100, 0))
    flags[0].modify()
    flags[0].set_actor_location(ue.Vector(at.x, at.y, flags[0].get_actor_location().z), False, False)
    range_actor.align_layout_to_terrain()
