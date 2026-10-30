# SPDX-License-Identifier: MIT
"""Turn a range map into the open-licence savanna course, or rebuild it.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/BuildSavannaRange.py" [--map PATH] [--course FOLDER] [--generated FOLDER]

Defaults convert the public range, /Game/UOpenGolf/Maps/UOpenGolf, with course assets in
/Game/UOpenGolf/Terrain/Savanna. Build the savanna assets first (ImportSavannaAssets.py for the
grass, BuildSurfaceTextures.py, BuildSavannaGrassPresets.py, BuildSavannaGround.py,
ImportNatureEssentials.py). The script:

1. backs up the map and its terrain definition;
2. copies the map's definition to <course>/DA_SavannaRange the first time and switches the map to it;
3. lays out the par-4 hole (savanna_hole.py: the green 400 yards out with its contour and flag, the fairway bunker on
   the left of the landing area, a greenside bunker), sets the Grass008/Ground093C surfaces, the savanna ground
   (course mask, rough and outer-ground instances) and 36 seeded jacaranda patches, and drops
   scattered grass presets;
4. rebuilds the terrain, then the trees, wood collision and understory, saves the map and deletes the
   generated folders it no longer uses.

Then place the course grass with the course kit's ApplyCourseGrass.py (logged below). Completion is
logged as OPEN_GOLF_SAVANNA_RANGE_COMPLETE. Running it again rebuilds the same course.
"""
from datetime import datetime, timezone
from pathlib import Path
import argparse
import importlib
import json
import shutil
import sys

import unreal as ue

PROJECT = Path(ue.Paths.project_dir()).resolve()
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / "Scripts"))
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
import BuildSavannaGround as ground
import course_hole
import RebuildRangeVegetation as vegetation
import savanna_assets as s
import savanna_hole as hole_spec
import savanna_layout as layout
import savanna_trees as trees
import understory

ground, course_hole, vegetation, s, hole_spec, layout, trees, understory = [importlib.reload(m) for m in (ground, course_hole, vegetation, s, hole_spec, layout, trees, understory)]
LIB = ue.EditorAssetLibrary
OWNER = "OpenGolfSavanna.Course.v1"
SURFACES = {
    ue.OpenFairwaySurface.FAIRWAY: "/OpenGolfSavanna/Terrain/DA_FairwayGrass008",
    ue.OpenFairwaySurface.GREEN: "/OpenGolfSavanna/Terrain/DA_GreenGrass008",
    ue.OpenFairwaySurface.SAND: "/OpenGolfSavanna/Terrain/DA_BunkerGround093C",
}
ROUGH_SOURCE = "/OpenGolfCourse/Terrain/Presets/DA_CoastalRough"
COURSE_GRASS = ["/OpenGolfSavanna/Terrain/DA_RoughGrassDense"]
BOUNDARY_GRASS = "/OpenGolfSavanna/Terrain/DA_BoundaryGrass"
TREE_PREFIX = "SavannaTree"


def content_dir(folder):
    """The Content directory and mount of a /Game or plugin content folder."""
    mount = "/" + folder.strip("/").split("/")[0]
    if mount == "/Game":
        return mount, PROJECT / "Content"
    descriptors = list((PROJECT / "Plugins").rglob(f"{mount[1:]}.uplugin"))
    if len(descriptors) != 1:
        raise RuntimeError(f"Cannot locate the plugin that mounts {folder}")
    return mount, descriptors[0].parent / "Content"


def relative(folder, mount):
    return folder[len(mount) + 1:]


def definition_copy(source, path):
    if LIB.does_asset_exist(path):
        return s.required(path)
    copy = LIB.duplicate_asset(source.get_path_name().split(".")[0], path)
    if not copy:
        raise RuntimeError(f"Could not copy the terrain definition to {path}")
    return copy


def preset_copy(source, path):
    if LIB.does_asset_exist(path):
        return s.required(path)
    copy = LIB.duplicate_asset(source, path)
    if not copy:
        raise RuntimeError(f"Could not copy {source} to {path}")
    return copy


def set_surfaces(definition):
    regions = list(definition.get_editor_property("regions"))
    for region in regions:
        path = SURFACES.get(region.surface.surface if region.surface else None)
        if path:
            region.set_editor_property("surface", s.required(path))
    definition.set_editor_property("regions", regions)


def set_ground(definition, course, mount):
    folder = relative(course, mount)
    mask, minimum, maximum = ground.course_mask(definition, f"{folder}/Materials", "T_SavannaRoughMask")
    instances = []
    for name, native in (("MI_SavannaRough", 0), ("MI_SavannaNative", 1)):
        instance = s.a.material_instance(f"{folder}/Materials", name, s.required("/OpenGolfSavanna/Materials/M_SavannaGround"))
        ground.configure_ground(instance, native, mask, minimum, maximum)
        instance.set_editor_property("override_phys_material", True)
        instance.set_editor_property("phys_material", s.required("/OpenGolfCourse/Surfaces/PM_Rough"))
        ue.MaterialEditingLibrary.update_material_instance(instance)
        instances.append(s.a.save(instance))
    rough = preset_copy(ROUGH_SOURCE, f"{course}/DA_SavannaRough")
    rough.set_editor_property("material", instances[0])
    s.a.save(rough)
    definition.set_editor_property("default_surface", rough)
    return instances


def set_trees(definition, course, mount):
    folder = f"{relative(course, mount)}/Trees"
    factory = ue.DataAssetFactory()
    factory.set_editor_property("data_asset_class", ue.OpenGolfVegetationPreset)
    areas = [area for area in definition.vegetation_areas
             if not (area.preset and area.preset.kind == ue.OpenGolfVegetationKind.TREE)]
    for name, kind, outline, seed in layout.tree_patches(TREE_PREFIX):
        preset = s.a.load_or_create(folder, f"DA_{name}", ue.OpenGolfVegetationPreset, factory)
        s.a.save(trees.configure(preset, kind, seed))
        area = ue.OpenGolfVegetationArea()
        area.name = name
        area.outline_m = [ue.Vector2D(x, y) for x, y in outline]
        area.preset = preset
        areas.append(area)
    definition.set_editor_property("vegetation_areas", areas)
    # Grass comes from the course grass layer; scattered ground-cover presets are dropped.
    definition.set_editor_property("vegetation", [])


def apply_ground(area, rough, native):
    changed = 0
    for actor in area.generated_actors:
        if not actor:
            continue
        if isinstance(actor, ue.LandscapeProxy):
            actor.set_editor_property("landscape_material", native)
            changed += 1
            continue
        for component in actor.get_components_by_class(ue.StaticMeshComponent):
            if component.get_material(0) == rough:
                changed += 1
    if not changed:
        raise RuntimeError("Found neither the rough mesh nor a Landscape to apply the savanna ground to")
    return changed


def finish_trees(terrain, backup, extra=None):
    """After a vegetation rebuild: wood collision and understory under the baked trees. Returns the counts."""
    report = trees.apply(terrain, PROJECT / "Saved/Automation/SavannaRange/collision.json")
    understory.apply(terrain)
    result = counts(terrain)
    if not result["trees"] or result["trees"] != result["colliders"]:
        raise RuntimeError(f"Tree/collider mismatch: {result}")
    (backup / "counts.json").write_text(json.dumps({"counts": result, **(extra or {}), "collision": str(report)[:200]}, indent=2))
    return result


def counts(area):
    result = {"trees": 0, "colliders": 0, "ground_cover": 0}
    for actor in area.generated_actors:
        if not actor:
            continue
        key = ("trees" if actor.actor_has_tag("OpenGolf.Terrain.Trees") else
               "colliders" if actor.actor_has_tag("OpenGolf.Terrain.TreeColliders") else
               "ground_cover" if actor.actor_has_tag("OpenGolf.Terrain.Vegetation") else None)
        if key:
            result[key] += sum(c.get_instance_count() for c in actor.get_components_by_class(ue.InstancedStaticMeshComponent))
    return result


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--map", default=vegetation.MAP)
    parser.add_argument("--course", default="/Game/UOpenGolf/Terrain/Savanna")
    parser.add_argument("--generated", default=vegetation.GENERATED)
    args = parser.parse_args(argv)
    editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError("Stop Play before building the savanna range")
    trees.require_native_support()
    if not hasattr(ue.OpenGolfTerrainBuilder, "add_vegetation_host"):
        raise RuntimeError("Rebuild DebugGame and restart the editor to load AddVegetationHost")
    world = editor.get_editor_world()
    if not world or world.get_path_name().split(".")[0] != args.map:
        raise RuntimeError(f"Open {args.map} first (and save or discard other edits)")
    ranges = [a for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors() if isinstance(a, ue.UOpenGolfRange)]
    if len(ranges) != 1 or not ranges[0].get_editor_property("terrain"):
        raise RuntimeError("Expected one range controller with terrain")
    range_actor = ranges[0]
    area = range_actor.get_editor_property("terrain")
    mount, content = content_dir(args.course)

    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    backup = PROJECT / "Saved/TerrainBackups" / f"{stamp}-savanna-range"
    backup.mkdir(parents=True, exist_ok=False)
    source_map = vegetation.map_file(args.map)
    shutil.copy2(source_map, backup / source_map.name)
    current = area.get_editor_property("definition")
    definition_path = f"{args.course}/DA_SavannaRange"
    _, definition_content = content_dir(current.get_path_name())
    definition_file = definition_content / (current.get_path_name().split(".")[0].split("/", 2)[2] + ".uasset")
    if definition_file.exists():
        shutil.copy2(definition_file, backup / definition_file.name)

    tee_before = range_actor.get_tee_location()
    with s.course_helpers(mount, content, OWNER):
        definition = definition_copy(current, definition_path)
        definition.modify()
        course_hole.lay_out(definition, hole_spec.apply_layout, hole_spec.REGION_TEMPLATES)
        set_surfaces(definition)
        rough, native = set_ground(definition, args.course, mount)
        set_trees(definition, args.course, mount)
        if not LIB.save_loaded_asset(definition, only_if_is_dirty=False):
            raise RuntimeError("Could not save the savanna terrain definition")
    area.modify()
    area.set_editor_property("definition", definition)
    area.set_editor_property("course_grass_presets", [s.required(path) for path in COURSE_GRASS])
    area.set_editor_property("boundary_grass_preset", s.required(BOUNDARY_GRASS))
    if ue.OpenGolfTerrainBuilder.build_terrain(area, args.generated, True) is None:
        raise RuntimeError(f"Terrain rebuild failed; map not saved. Backup: {backup}")
    applied = apply_ground(area, rough, native)
    course_hole.place_flag(range_actor, area, hole_spec.PIN)
    range_actor.refresh_presentation()
    tee_after = range_actor.get_tee_location()
    if abs(tee_after.x - tee_before.x) > 0.01 or abs(tee_after.y - tee_before.y) > 0.01:
        raise RuntimeError(f"The tee moved horizontally; map not saved. Backup: {backup}")

    def finished(_range, terrain):
        result = finish_trees(terrain, backup, {"ground_targets": applied})
        grass = " ".join(f"--grass-preset {p}" for p in COURSE_GRASS)
        ue.log(f"OPEN_GOLF_SAVANNA_RANGE_COMPLETE {result}; backup={backup}. Next: ApplyCourseGrass.py --map {args.map} "
               f"--generated {args.generated} {grass} --boundary-preset {BOUNDARY_GRASS}")

    # After the save, the replaced ground build and the vegetation runs' PCG folders are deleted.
    vegetation.rebuild(args.map, args.generated, finished=finished)


if __name__ == "__main__":
    main(sys.argv[1:])
