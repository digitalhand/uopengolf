# SPDX-License-Identifier: MIT
"""Install the reusable coastal terrain in the public range, preserving a flat regression map."""
from datetime import datetime, timezone
from pathlib import Path
import math
import shutil

import unreal as ue

MAP = "/Game/UOpenGolf/Maps/UOpenGolf"
FLAT_MAP = "/Game/UOpenGolf/Maps/RangeFlatRegression"
DEFINITION = "/Game/UOpenGolf/Terrain/DA_RangeCoastal"
GENERATED = "/Game/UOpenGolf/Terrain/Generated"
LIB = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def ellipse(cx, cy, rx, ry, count=48, variation=0.0):
    return [ue.Vector2D(cx + rx * (1 + variation * math.sin(3 * a)) * math.cos(a),
                        cy + ry * (1 + variation * math.sin(3 * a)) * math.sin(a))
            for a in (math.tau * i / count for i in range(count))]


def make_definition():
    if LIB.does_asset_exist(DEFINITION):
        return LIB.load_asset(DEFINITION)
    factory = ue.DataAssetFactory()
    factory.set_editor_property("data_asset_class", ue.OpenGolfTerrainDefinition)
    definition = TOOLS.create_asset("DA_RangeCoastal", "/Game/UOpenGolf/Terrain", ue.OpenGolfTerrainDefinition, factory)
    if not definition:
        raise RuntimeError("Could not create the range terrain definition")
    preset = lambda name: ue.load_asset(f"/OpenGolfCourse/Terrain/Presets/DA_Coastal{name}")
    definition.set_editor_property("default_surface", preset("Rough"))
    outlines = [
        ("Fairway", "Fairway", [(-12, -13), (25, -16), (75, -14), (120, -17), (165, -17),
                                (220, -14), (285, -16), (335, -11), (350, 0), (335, 12),
                                (280, 15), (220, 17), (160, 16), (110, 15), (60, 17), (10, 14), (-12, 12)], 2.0),
        ("Fringe", "Fringe", ellipse(137.16, 0, 16, 12, variation=0.07), 0.5),
        ("Green", "Green", ellipse(137.16, 0, 14, 10, variation=0.07), 0.5),
        ("Bunker", "Sand", ellipse(146, 28, 8, 5, variation=0.1), 0.5),
    ]
    regions = []
    for name, surface, outline, spacing in outlines:
        region = ue.OpenGolfTerrainRegion()
        region.set_editor_property("name", name)
        region.set_editor_property("surface", preset(surface))
        region.set_editor_property("outline_m", [ue.Vector2D(*p) if isinstance(p, tuple) else p for p in outline])
        region.set_editor_property("spacing_m", spacing)
        regions.append(region)
    definition.set_editor_property("regions", regions)
    mounds = []
    contours = [(60, -42, 36, 22, 3.0), (130, -53, 45, 30, 4.5), (230, -46, 55, 24, 4.0),
                (310, -65, 55, 34, 5.0), (40, 48, 45, 28, 3.8), (105, 62, 45, 28, 4.0),
                (190, 50, 45, 25, 3.5), (285, 65, 60, 32, 5.0), (385, 5, 48, 65, 5.0),
                (160, -5, 48, 40, 0.45), (126, 5, 22, 18, 0.23),
                (146, 28, 12, 9, 0.55), (146, 28, 7.5, 4.5, -1.1)]
    for x, y, rx, ry, height in contours:
        mound = ue.OpenGolfTerrainMound()
        mound.set_editor_property("center_m", ue.Vector2D(x, y))
        mound.set_editor_property("radius_m", ue.Vector2D(rx, ry))
        mound.set_editor_property("height_m", height)
        mounds.append(mound)
    definition.set_editor_property("mounds", mounds)
    definition.set_editor_property("vegetation", [preset("GrassVegetation"), preset("RoughVegetation")])
    if not LIB.save_loaded_asset(definition):
        raise RuntimeError("Could not save the range terrain definition")
    return definition


def main():
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    if not levels.load_level(MAP):
        raise RuntimeError("Could not open the public range")
    ranges = [a for a in actors.get_all_level_actors() if isinstance(a, ue.UOpenGolfRange)]
    if len(ranges) != 1:
        raise RuntimeError("Expected exactly one range controller")
    range_actor = ranges[0]
    if range_actor.get_editor_property("terrain"):
        ue.log("Range already has authored terrain; preserving it. Rebuild the selected area explicitly.")
        return
    root = Path(ue.Paths.project_dir()).resolve()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    backup = root / "Saved/TerrainBackups" / stamp
    backup.mkdir(parents=True, exist_ok=False)
    shutil.copy2(root / "Content/UOpenGolf/Maps/UOpenGolf.umap", backup / "UOpenGolf.umap")
    if not LIB.does_asset_exist(FLAT_MAP):
        if not LIB.duplicate_asset(MAP, FLAT_MAP) or not LIB.save_asset(FLAT_MAP):
            raise RuntimeError("Could not preserve the flat regression map")
    definition = make_definition()
    area = actors.spawn_actor_from_class(ue.OpenGolfTerrainArea, range_actor.get_actor_location(), range_actor.get_actor_rotation())
    area.set_actor_label("Coastal range terrain")
    area.set_folder_path("Terrain")
    area.set_editor_property("definition", definition)
    error = ue.OpenGolfTerrainBuilder.build_terrain(area, GENERATED, False)
    if error is None:
        raise RuntimeError("Terrain operation failed; inspect the Unreal log")
    # Remove only the ground belonging to this controller, after generation succeeded.
    ground = []
    for actor in actors.get_all_level_actors():
        if actor.get_owner() != range_actor:
            continue
        if str(actor.get_folder_path()) == "Range/Ground" or actor.get_actor_label() == "Backdrop":
            ground.append(actor)
    if len(ground) < 10:
        raise RuntimeError("The expected legacy ground was not found; leaving the map unsaved for review")
    for actor in ground:
        if not actors.destroy_actor(actor):
            raise RuntimeError(f"Could not remove legacy ground actor {actor.get_actor_label()}")
    range_actor.set_editor_property("terrain", area)
    tee_material = ue.load_asset("/OpenGolfCourse/Terrain/Materials/MI_CoastalTee")
    if tee_material:
        for actor in actors.get_all_level_actors():
            if actor.get_owner() != range_actor:
                continue
            for component in actor.get_components_by_class(ue.StaticMeshComponent):
                if component.get_name() == "TeeBox":
                    component.set_material(0, tee_material)
    range_actor.align_layout_to_terrain()
    range_actor.refresh_presentation()
    error = ue.OpenGolfTerrainBuilder.start_vegetation(area, GENERATED)
    if error is None:
        raise RuntimeError("Terrain operation failed; inspect the Unreal log")
    started = datetime.now(timezone.utc)

    def finish(_delta):
        error = ue.OpenGolfTerrainBuilder.bake_vegetation(area)
        if error is None and (datetime.now(timezone.utc) - started).total_seconds() < 180:
            return
        ue.unregister_slate_post_tick_callback(handle)
        if error is None:
            ue.log_error(f"Terrain vegetation failed; map not saved: {error}")
            return
        if not levels.save_current_level():
            ue.log_error("Could not save the upgraded range")
            return
        (backup / "completed.txt").write_text(f"Flat map: {FLAT_MAP}\nTerrain definition: {DEFINITION}\n", encoding="utf-8")
        ue.log("OPEN_GOLF_RANGE_TERRAIN_COMPLETE")

    handle = ue.register_slate_post_tick_callback(finish)
    ue.log(f"Terrain built; waiting for vegetation. Original map backup: {backup}")


if __name__ == "__main__":
    main()
