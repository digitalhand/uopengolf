# SPDX-License-Identifier: MIT
"""Dry-savanna cover (Nature Essentials) around every baked jacaranda of a terrain area.

The pieces are instanced-mesh components added to the terrain's baked ground-cover actor, so the
range's 3D grass switch and the terrain checks treat them as vegetation. A vegetation rebuild
replaces that actor, so apply() runs again after each rebuild (BuildSavannaRange.py does).

Standalone, with the course map open:
    py "<project>/Plugins/OpenGolfSavanna/Scripts/understory.py"
backs up the map, reapplies the cover and saves the map.
"""
from datetime import datetime, timezone
from pathlib import Path
import importlib
import math
import shutil
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import savanna_layout
import savanna_trees as trees
import understory_layout as layout

savanna_layout, trees, layout = [importlib.reload(module) for module in (savanna_layout, trees, layout)]
TAG = "OpenGolfSavanna.Understory"
# Components added before the understory moved to this plugin carry the private range's tag.
REPLACED_TAGS = {TAG, "UOpenGolfPrivate.Understory"}
VEGETATION_TAG = "OpenGolf.Terrain.Vegetation"
MAX_SLOPE_DEG = 30.0
# Rocks sit partly buried; litter pieces get a small random lift so overlapping patches don't flicker.
SINK_CM = {"rocks": -12.0}
LIFT_CM = {"litter": 0.4, "moss": 0.2}


def terrain_frame(terrain):
    origin = terrain.get_actor_location()
    forward, right = terrain.get_actor_forward_vector(), terrain.get_actor_right_vector()
    return origin, (forward.x, forward.y), (right.x, right.y)


def to_local(frame, point):
    origin, forward, right = frame
    qx, qy = point.x - origin.x, point.y - origin.y
    return (qx * forward[0] + qy * forward[1]) / 100.0, (qx * right[0] + qy * right[1]) / 100.0


def to_world(frame, x, y):
    origin, forward, right = frame
    return ue.Vector(origin.x + (x * forward[0] + y * right[0]) * 100.0,
                     origin.y + (x * forward[1] + y * right[1]) * 100.0, origin.z)


def play_outlines(terrain):
    definition = terrain.get_editor_property("definition")
    return [[(p.x, p.y) for p in region.outline_m] for region in definition.regions
            if region.surface and region.surface.surface != ue.OpenFairwaySurface.ROUGH]


def ground(terrain, position):
    hit = terrain.ground_at(position)
    if hit is None or hit is False:
        return None
    if len(hit) == 3:
        found, point, normal = hit
        return (point, normal) if found else None
    return hit


def host_actor(terrain):
    """The baked ground-cover actor, or a new generated one when the course has no scattered cover.

    Course grass is excluded: replacing that layer would delete the understory with it.
    """
    for actor in terrain.generated_actors:
        if actor and actor.actor_has_tag(VEGETATION_TAG) and not actor.actor_has_tag("OpenGolf.Terrain.CourseGrass"):
            return actor
    if not hasattr(ue.OpenGolfTerrainBuilder, "add_vegetation_host"):
        raise RuntimeError("Rebuild DebugGame and restart the editor to load AddVegetationHost")
    actor = ue.OpenGolfTerrainBuilder.add_vegetation_host(terrain, "Savanna understory")
    if not actor:
        raise RuntimeError("Could not create the understory host actor (see LogTemp)")
    return actor


def handles(actor):
    subsystem = ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
    return subsystem, subsystem.k2_gather_subobject_data_for_instance(actor)


def component_of(handle):
    data = ue.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    return ue.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)


def remove_understory(terrain):
    removed = 0
    for actor in terrain.generated_actors:
        if not actor:
            continue
        subsystem, found = handles(actor)
        for handle in found[1:]:
            component = component_of(handle)
            if isinstance(component, ue.InstancedStaticMeshComponent) and REPLACED_TAGS & {str(t) for t in component.component_tags}:
                actor.modify()
                removed += subsystem.delete_subobject(found[0], handle, None)
    return removed


def add_component(actor, name, mesh, cull_m):
    subsystem, found = handles(actor)
    params = ue.AddNewSubobjectParams(parent_handle=found[0], new_class=ue.InstancedStaticMeshComponent,
                                      blueprint_context=None)
    handle, reason = subsystem.add_new_subobject(params)
    component = component_of(handle)
    if not isinstance(component, ue.InstancedStaticMeshComponent):
        raise RuntimeError(f"Could not add {name} to {actor.get_actor_label()}: {reason}")
    subsystem.rename_subobject(handle, ue.Text(name))
    component.set_static_mesh(mesh)
    component.set_collision_profile_name("NoCollision")
    component.set_cast_shadow(False)
    component.set_editor_property("evaluate_world_position_offset", False)
    component.set_editor_property("component_tags", [TAG])
    component.set_cull_distances(cull_m * 75.0, cull_m * 100.0)
    return component


def transform(point, normal, category, yaw, scale, lift):
    forward = ue.Vector(math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0)
    rotation = ue.MathLibrary.make_rot_from_zx(normal, forward)
    offset = SINK_CM.get(category, 0.0) * scale + LIFT_CM.get(category, 0.0) * lift
    location = point + normal * offset
    return ue.Transform(location, rotation, ue.Vector(scale, scale, scale))


def apply(terrain):
    if ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError("Stop Play before applying the understory")
    frame = terrain_frame(terrain)
    baked, _ = trees.wood.tree_data([actor for actor in terrain.generated_actors if actor], trees.SOURCE)
    if not baked:
        raise RuntimeError("No baked jacarandas found")
    canopies = []
    for tree in baked.values():
        x, y = to_local(frame, tree.translation)
        canopies.append((x, y, tree.scale3d.x / savanna_layout.CANOPY_RATIO))
    points = layout.understory_points(canopies, play_outlines(terrain))
    minimum_up = math.cos(math.radians(MAX_SLOPE_DEG))
    grouped, skipped = {}, 0
    for category, variant, x, y, yaw, scale, lift in points:
        hit = ground(terrain, to_world(frame, x, y))
        if not hit or hit[1].z < minimum_up:
            skipped += 1
            continue
        grouped.setdefault((category, variant), []).append(transform(hit[0], hit[1], category, yaw, scale, lift))
    removed = remove_understory(terrain)
    host = host_actor(terrain)
    host.modify()
    counts = {}
    for (category, variant), transforms in sorted(grouped.items()):
        settings = layout.CATEGORIES[category]
        mesh = ue.EditorAssetLibrary.load_asset(settings["meshes"][variant])
        if not mesh:
            raise RuntimeError(f"Missing understory mesh {settings['meshes'][variant]}; run ImportNatureEssentials.py")
        component = add_component(host, f"Understory_{category}_{variant}", mesh, settings["cull_m"])
        component.add_instances(transforms, False, True)
        counts[category] = counts.get(category, 0) + len(transforms)
    total = sum(counts.values())
    ue.log(f"OPEN_GOLF_UNDERSTORY_APPLIED trees={len(canopies)} total={total} {counts} skipped={skipped} "
           f"removed_components={removed}")
    return counts


def main():
    editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError("Stop Play before saving the understory")
    world = editor.get_editor_world()
    areas = [a for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
             if isinstance(a, ue.OpenGolfTerrainArea)]
    if not world or len(areas) != 1:
        raise RuntimeError("Open a course map with one terrain area first")
    sys.path.insert(0, str(Path(ue.Paths.project_dir()).resolve() / "Scripts"))
    import RebuildRangeVegetation as vegetation
    source = vegetation.map_file(world.get_path_name().split(".")[0])
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    backup = Path(ue.Paths.project_dir()).resolve() / "Saved/TerrainBackups" / f"{stamp}-understory"
    backup.mkdir(parents=True, exist_ok=False)
    shutil.copy2(source, backup / source.name)
    apply(areas[0])
    if not ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Could not save the understory map")
    ue.log(f"OPEN_GOLF_UNDERSTORY_SAVED backup={backup}")


if __name__ == "__main__":
    main()
