# SPDX-License-Identifier: MIT
"""Build an editable course map from a prepared layout inside Unreal Editor.

Run headless with OPENGOLF_DESIGN_SOURCE (the folder prepare_course_site.py wrote) and OPENGOLF_DESIGN_DEST
(an empty content folder, such as /HunterRanch/Draft) set, or from the Course Designer's Build button.
The author corrections named in the source's recipe.json are applied over the prepared layout.
"""
import json
import math
import os
from pathlib import Path
import re
import sys

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
import course_corrections  # noqa: E402
import course_water  # noqa: E402

PRESETS = {"rough": "Rough", "fairway": "Fairway", "green": "Green", "bunker": "Sand", "tee": "Fairway"}
# Sample spacing in metres. The elevation grid is 1 m, so finer spacing on greens and bunkers keeps their edges
# and contours smooth rather than adding new height detail.
SPACING_M = {"rough": 2.0, "fairway": 1.0, "tee": 1.0, "green": 0.5, "bunker": 0.5}
OUTSIDE_SPACING_M = 4.0  # Rough beyond the play area, which is out of play and seen from afar.
PLAY_AREA_NAME = "PlayArea"  # No separator, so SaveCourseCorrections.py skips it: it is built, not map data.
TILE_M = 64
MARGIN_TILES = 2
REGION_NAME_SEPARATOR = "|"  # SaveCourseCorrections.py splits "<surface>|<source id>".


def asset(folder, name, cls):
    factory = ue.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    result = ue.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, factory)
    if not result:
        raise RuntimeError("Could not create " + name)
    return result


def require(result, action):
    if result is None or result is False or (isinstance(result, tuple) and not result[0]):
        raise RuntimeError(f"{action} failed: {result}")


def position(point):
    return ue.Vector(point[0] * 100, point[1] * 100, 0)


def load_layout(source):
    recipe = json.loads((source / "recipe.json").read_text())
    layout = json.loads((source / "source_layout.json").read_text())
    return course_corrections.apply(layout, course_corrections.read(recipe))


def terrain_definition(root, layout, presets, source):
    terrain = asset(root, "DA_Ground", ue.OpenGolfTerrainDefinition)
    terrain.set_editor_property("default_surface", presets["rough"])
    outline = layout["boundary_m"]
    min_tile = [math.floor(min(p[i] for p in outline) / TILE_M) - MARGIN_TILES for i in range(2)]
    max_tile = [math.ceil(max(p[i] for p in outline) / TILE_M) + MARGIN_TILES for i in range(2)]
    # The shared terrain validator still expects a 63-quad Landscape border around its legacy detail rectangle.
    low = [math.floor(v * TILE_M / 63) * 63 for v in min_tile]
    high = [math.ceil(v * TILE_M / 63) * 63 for v in max_tile]
    for key, values in {"minimum_m": low, "maximum_m": high, "detail_minimum_m": [v + 63 for v in low],
                        "detail_maximum_m": [v - 63 for v in high]}.items():
        terrain.set_editor_property(key, ue.Vector2D(*values))
    terrain.set_editor_property("landscape_spacing_m", 1)
    play_area = layout.get("play_area_m")
    terrain.set_editor_property("detail_spacing_m", OUTSIDE_SPACING_M if play_area else SPACING_M["rough"])
    require(ue.OpenGolfTerrainBuilder.import_elevation(terrain, str(source / "ground.oge")), "Elevation import")
    regions = []
    for r in ([{"id": None, "surface": "rough", "outline_m": play_area}] if play_area else []) + layout["regions"]:
        region = ue.OpenGolfTerrainRegion()
        region.name = PLAY_AREA_NAME if r["id"] is None else f"{r['surface']}{REGION_NAME_SEPARATOR}{r['id']}"
        region.surface = presets[r["surface"]]
        region.spacing_m = SPACING_M[r["surface"]]
        region.outline_m = [ue.Vector2D(*p) for p in r["outline_m"]]
        regions.append(region)
    terrain.set_editor_property("regions", regions)
    return terrain, min_tile, max_tile


def hole_records(layout):
    holes = []
    for record in layout["holes"]:
        route = record["routing_m"]
        hole = ue.GolfCourseHole()
        hole.set_editor_property("par", record["par"])
        hole.set_editor_property("cup_cm", position(record["cup_m"]))
        hole.set_editor_property("routing_cm", [position(p) for p in route[1:-1]])
        tees = []
        for name, yards in record["published_yards"].items():
            tee = ue.GolfCourseTee()
            tee.set_editor_property("set_name", name)
            tee.set_editor_property("published_yards", yards)
            tee.set_editor_property("position_cm", position(record["tees_m"][name]))
            tees.append(tee)
        hole.set_editor_property("tees", tees)
        holes.append(hole)
    return holes


def build(source, root):
    if not re.fullmatch(r"/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)*", root):
        raise ValueError("Select a mounted Unreal content folder.")
    layout = load_layout(source)
    if not layout["holes"] or any(len(h["routing_m"]) < 2 or "cup_m" not in h for h in layout["holes"]):
        raise ValueError("Resolve missing or disconnected hole routes in the corrections file.")
    if not layout.get("boundary_m"):
        raise ValueError("Add a closed course boundary.")
    level = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    library = ue.EditorAssetLibrary
    map_path = root + "/Course"
    if library.list_assets(root, recursive=True):
        raise ValueError("Choose an empty destination to preserve existing course edits.")
    if ue.EditorLoadingAndSavingUtils.get_dirty_map_packages() or ue.EditorLoadingAndSavingUtils.get_dirty_content_packages():
        raise ValueError("Save your editor changes before building a new course.")
    if not level.new_level(map_path):
        raise RuntimeError("Could not create " + map_path)
    presets = {k: ue.load_asset("/OpenGolfCourse/Terrain/Presets/DA_Coastal" + v) for k, v in PRESETS.items()}
    terrain, min_tile, max_tile = terrain_definition(root, layout, presets, source)
    area = actors.spawn_actor_from_class(ue.OpenGolfTerrainArea, ue.Vector())
    area.set_editor_property("definition", terrain)
    tiles = [(col, row) for row in range(min_tile[1], max_tile[1]) for col in range(min_tile[0], max_tile[0])]
    with ue.ScopedSlowTask(len(tiles), "Building course ground") as task:
        for col, row in tiles:
            task.enter_progress_frame(1)
            require(ue.OpenGolfTerrainBuilder.build_course_tile(area, ue.IntPoint(col, row), root + "/Tiles"), f"Tile {col},{row}")
    water_material = course_water.material()
    for index, lake in enumerate(layout["water"], start=1):
        require(ue.OpenGolfTerrainBuilder.build_water_surface(area, [ue.Vector2D(*p) for p in lake["outline_m"]], water_material,
                                                               root + "/Water", f"Lake{index}"), f"Water {lake['id']}")
    controller = actors.spawn_actor_from_class(ue.UOpenGolfRange, position(layout["holes"][0]["tees_m"][next(iter(layout["holes"][0]["tees_m"]))]))
    controller.set_editor_property("terrain", area)
    controller.build_default_layout()
    course = asset(root, "DA_" + re.sub(r"[^A-Za-z0-9_]", "", layout["name"]), ue.GolfCourseDefinition)
    course.set_editor_property("display_name", layout["name"] + " (draft)")
    course.set_editor_property("layout", hole_records(layout))
    course.set_editor_property("holes", len(layout["holes"]))
    course.set_editor_property("par", sum(h["par"] for h in layout["holes"]))
    course.set_editor_property("length_m", sum(max(h["published_yards"].values()) for h in layout["holes"]) * 0.9144)
    course.set_editor_property("map", ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world())
    course.set_editor_property("InBoundsCm", [ue.Vector2D(p[0] * 100, p[1] * 100) for p in layout["boundary_m"]])
    water = []
    for lake in layout["water"]:
        area_record = ue.GolfWaterArea()
        area_record.set_editor_property("outline_cm", [ue.Vector2D(p[0] * 100, p[1] * 100) for p in lake["outline_m"]])
        water.append(area_record)
    course.set_editor_property("water", water)
    require(library.save_loaded_asset(terrain), "Saving terrain")
    require(library.save_loaded_asset(course), "Saving course")
    require(level.save_current_level(), "Saving map")
    ue.log(f"COURSE_DRAFT_COMPLETE {map_path} tiles={len(tiles)} regions={len(layout['regions'])} lakes={len(layout['water'])}")


if __name__ == "__main__":
    build(Path(os.environ["OPENGOLF_DESIGN_SOURCE"]).resolve(), os.environ["OPENGOLF_DESIGN_DEST"])
