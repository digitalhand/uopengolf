# SPDX-License-Identifier: MIT
"""Update an open course's grass layer and/or surface materials without rebuilding its terrain.

    py "<project>/Plugins/OpenGolfCourse/Scripts/ApplyCourseGrass.py" --map /YourCourse/Maps/Course
        --generated /YourCourse/Generated [--grass-preset PRESET ...] [--boundary-preset PRESET]
        [--surface-preset PRESET ...]

--grass-preset (repeatable) and --boundary-preset set the terrain area's course grass layer, which is placed
through PCG, baked, and replaces only the previous layer. --surface-preset (repeatable) swaps the material of
every generated surface of that preset's type (fairway, green, sand, ...) and updates the matching region
presets, so a later terrain rebuild keeps it. Physical materials come with the preset's material.

The map must be open with Play stopped. Its saved map and terrain definition are backed up first, so save
wanted edits before running. Completion is logged as OPEN_GOLF_COURSE_GRASS_COMPLETE. After the map is saved,
generated folders under --generated that nothing references any more are deleted (generated_folders.py).
"""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import shutil
import sys
import time
import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generated_folders

LIB = ue.EditorAssetLibrary
COURSE_GRASS_TAG = 'OpenGolf.Terrain.CourseGrass'
BAKE_TIMEOUT_S = 300


def package_file(package, extension):
    root = Path(ue.Paths.project_dir()).resolve()
    mount, relative = package.split('.')[0].strip('/').split('/', 1)
    if mount == 'Game':
        return root / 'Content' / (relative + extension)
    plugins = list((root / 'Plugins').rglob(mount + '.uplugin'))
    if len(plugins) != 1:
        raise RuntimeError(f'Cannot locate package mount: {mount}')
    return plugins[0].parent / 'Content' / (relative + extension)


def load_presets(paths, preset_class, what):
    presets = []
    for path in paths:
        preset = ue.load_asset(path)
        if not isinstance(preset, preset_class):
            raise RuntimeError(f'{what} {path} is missing or has the wrong type')
        presets.append(preset)
    return presets


def terrain_area(map_path):
    editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if editor.get_game_world() or not world or world.get_path_name().split('.')[0] != map_path:
        raise RuntimeError('Open the requested map and stop Play before applying changes')
    areas = [actor for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
             if isinstance(actor, ue.OpenGolfTerrainArea)]
    if len(areas) != 1:
        raise RuntimeError('Expected one terrain area in this map')
    return areas[0]


def surface_targets(area, surfaces):
    """(component, slot, new material) for every generated surface whose region type a preset replaces."""
    regions = list(area.definition.regions)
    targets = []
    for preset in surfaces:
        if not preset.material:
            raise RuntimeError(f'{preset.get_path_name()} has no material')
        old = {r.surface.material for r in regions if r.surface and r.surface.surface == preset.surface} - {None}
        found = [(component, slot, preset.material) for actor in area.generated_actors if actor
                 for component in actor.get_components_by_class(ue.StaticMeshComponent)
                 if not isinstance(component, ue.InstancedStaticMeshComponent)
                 for slot in range(component.get_num_materials()) if component.get_material(slot) in old]
        if not found:
            raise RuntimeError(f'No generated {preset.surface} surface matched; nothing changed')
        targets.extend(found)
    return targets


def backup(map_path, definition):
    folder = (Path(ue.Paths.project_saved_dir()) / 'TerrainBackups' /
              (datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f') + '-course-grass'))
    folder.mkdir(parents=True)
    for package, extension in ((map_path, '.umap'), (definition.get_path_name(), '.uasset')):
        source = package_file(package, extension)
        shutil.copy2(source, folder / source.name)
    ue.log(f'Course grass backup: {folder}')
    return folder


def apply(map_path, generated, grass_presets=(), boundary_preset=None, surface_presets=(), finished=None, cleanup=None):
    """cleanup lists the folders whose unreferenced generated runs are deleted after saving; default [generated]."""
    area = terrain_area(map_path)
    definition = area.definition
    grass = load_presets(grass_presets, ue.OpenGolfVegetationPreset, 'Grass preset')
    boundary = load_presets([boundary_preset] if boundary_preset else [], ue.OpenGolfVegetationPreset, 'Boundary preset')
    surfaces = load_presets(surface_presets, ue.OpenGolfSurfacePreset, 'Surface preset')
    if not grass and not boundary and not surfaces:
        raise RuntimeError('Choose grass, boundary and/or surface presets')
    targets = surface_targets(area, surfaces)
    folder = backup(map_path, definition)

    def save():
        if surfaces:
            definition.modify()
            regions = list(definition.regions)
            for region in regions:
                replacement = next((p for p in surfaces if region.surface and region.surface.surface == p.surface), None)
                if replacement:
                    region.set_editor_property('surface', replacement)
            definition.set_editor_property('regions', regions)
            for component, slot, material in targets:
                component.modify()
                component.set_material(slot, material)
            if not LIB.save_loaded_asset(definition):
                raise RuntimeError('Could not save the terrain definition')
        if not ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level():
            raise RuntimeError('Could not save the map')
        layer = [c for a in area.generated_actors if a and a.actor_has_tag(COURSE_GRASS_TAG)
                 for c in a.get_components_by_class(ue.InstancedStaticMeshComponent)]
        # Presets can share meshes, so sum every component that uses one.
        counts = {}
        for c in layer:
            if c.static_mesh:
                counts[c.static_mesh.get_name()] = counts.get(c.static_mesh.get_name(), 0) + c.get_instance_count()
        summary = f'course_grass={sum(counts.values())} surfaces={len(targets)} meshes={counts}'
        (folder / 'completed.txt').write_text(summary + '\n', encoding='utf-8')
        ue.log(f'OPEN_GOLF_COURSE_GRASS_COMPLETE {summary}')
        if finished:
            finished()
        # Every save is done, so the registry now matches the map: earlier runs' folders are unused.
        generated_folders.remove_unreferenced(LIB, [generated] if cleanup is None else cleanup, log=ue.log)

    if not grass and not boundary:
        save()
        return
    area.modify()
    area.set_editor_property('course_grass_presets', grass)
    area.set_editor_property('boundary_grass_preset', boundary[0] if boundary else None)
    if ue.OpenGolfTerrainBuilder.rebuild_course_grass(area, generated) is None:
        raise RuntimeError(f'Course grass placement failed (see LogTemp); the map was not saved. Backup: {folder}')
    started = time.monotonic()
    busy = []

    # PCG generates over the next frames; bake and save once it finishes. Baking, importing and
    # saving can pump Slate, which calls this again from inside itself, so nested calls return.
    def tick(_delta):
        if busy:
            return
        busy.append(True)
        try:
            if ue.OpenGolfTerrainBuilder.bake_vegetation(area) is None:
                if time.monotonic() - started < BAKE_TIMEOUT_S:
                    busy.clear()
                    return
                raise RuntimeError('Course grass bake timed out; map not saved')
            ue.unregister_slate_post_tick_callback(handle)
            save()
        except Exception as error:
            ue.unregister_slate_post_tick_callback(handle)
            ue.log_error(f'{error}; backup: {folder}')

    handle = ue.register_slate_post_tick_callback(tick)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--map', required=True)
    parser.add_argument('--generated', required=True)
    parser.add_argument('--grass-preset', action='append', default=[])
    parser.add_argument('--boundary-preset')
    parser.add_argument('--surface-preset', action='append', default=[])
    args = parser.parse_args()
    apply(args.map, args.generated, args.grass_preset, args.boundary_preset, args.surface_preset)
