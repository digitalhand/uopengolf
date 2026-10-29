# SPDX-License-Identifier: MIT
"""Give the savanna vegetation its wind (editor only), as vegetation_wind_assignments.py lists it.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/ApplyVegetationWind.py" [--keep-map-unsaved | --materials-only]

Scoped and repeatable:
  - adds the course kit's wind hook to the savanna vegetation parents without rebuilding the rest of
    their graphs (their builders add it too, so a rebuild keeps it)
  - sets each listed material instance's profile, sized to its tallest mesh, with its offset bound
    sized for its largest instance
  - lets the open map's placed meshes and the vegetation presets run that motion
Placement, textures and tuning are untouched. Every existing asset it saves, and the map, is copied to
Saved/VegetationWindBackups/<time>/ first; only what it changed is saved. Run with the public map open,
saved, and Play stopped, after rebuilding DebugGame (UOpenGolfWindSubsystem, bWindMotion).
--keep-map-unsaved accepts a map with unsaved changes and leaves saving it to you: the components
change in the open map, and your edits and these save together.
--materials-only rebuilds MF_VegetationWind and sets the listed instances' profiles, leaving the parents,
presets and map alone: enough after a change to vegetation_wind.py or the assignments' sizes and scales.
The importers call apply_materials() so a re-import keeps the motion.
"""
from datetime import datetime
from pathlib import Path
import importlib
import shutil
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import savanna_assets as s
import vegetation_wind_assignments as assign

# savanna_assets is reloaded only when this runs as a script: reloading it reloads course_assets, which
# would drop the mount an importer's savanna_helpers() set while it calls apply_materials().
assign = importlib.reload(assign)
a, wind = s.a, s.wind
LIB = ue.EditorAssetLibrary
MAP = "/Game/UOpenGolf/Maps/UOpenGolf"
PARENTS = tuple(f"/OpenGolfSavanna/Materials/{name}"
                for name in ("M_SavannaCard", "M_SavannaSolid", "M_SavannaGrass", "M_SavannaUnderstoryGrass"))
PRESET_CLASS = ue.TopLevelAssetPath("/Script/OpenGolfCourse", "OpenGolfVegetationPreset")
MOUNTS = {"/OpenGolfSavanna/": s.PROJECT / "Plugins/OpenGolfSavanna/Content",
          "/OpenGolfCourse/": s.PROJECT / "Plugins/OpenGolfCourse/Content", "/Game/": s.PROJECT / "Content"}


def require_ready(keep_map_unsaved=False):
    # Looked up by path: Python wraps only Blueprint-exposed classes, and the subsystem is not one.
    if not ue.load_class(None, "/Script/OpenGolfCourse.OpenGolfWindSubsystem"):
        raise RuntimeError("Rebuild DebugGame and restart the editor to load UOpenGolfWindSubsystem")
    try:
        ue.get_default_object(ue.OpenGolfVegetationPreset).get_editor_property("wind_motion")
    except Exception as error:
        raise RuntimeError("Rebuild DebugGame and restart the editor to load the presets' bWindMotion") from error
    editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError("Stop Play first")
    world = editor.get_editor_world()
    if not world.get_path_name().startswith(MAP):
        raise RuntimeError(f"Open {MAP} first (open: {world.get_path_name()})")
    if not keep_map_unsaved and ue.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError("Save or discard the open map's changes first, or pass --keep-map-unsaved")
    return world


def disk_file(package):
    mount = next(m for m in MOUNTS if package.startswith(m))
    for suffix in (".uasset", ".umap"):
        candidate = MOUNTS[mount] / (package[len(mount):] + suffix)
        if candidate.exists():
            return candidate
    return None


def backup(packages):
    folder = s.PROJECT / "Saved/VegetationWindBackups" / datetime.now().strftime("%Y%m%d-%H%M%S")
    for package in packages:
        source = disk_file(package)
        if source:
            target = folder / source.relative_to(s.PROJECT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    ue.log(f"VEGETATION_WIND_BACKUP {folder}")
    return folder


def wind_presets():
    """Vegetation presets placing any moving mesh, with the farthest hold-still distance among them."""
    result = []
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_class(PRESET_CLASS, True):
        preset = data.get_asset()
        meshes = list(preset.get_editor_property("meshes"))
        meshes += [mesh for tile in preset.get_editor_property("tiles") for mesh in tile.get_editor_property("meshes")]
        distances = [d for d in (assign.mesh_wind_distance_m(mesh.get_path_name()) for mesh in meshes if mesh) if d]
        if distances:
            result.append((preset, max(distances)))
    return result


def wind_components(world):
    """Placed instanced meshes on the map that move in the wind, with their hold-still distance."""
    result = []
    for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
        for component in actor.get_components_by_class(ue.InstancedStaticMeshComponent):
            mesh = component.get_editor_property("static_mesh")
            distance = assign.mesh_wind_distance_m(mesh.get_path_name()) if mesh else None
            if distance:
                result.append((component, distance))
    return result


def add_hooks():
    for path in PARENTS:
        material = s.required(path)
        wind.add_vegetation_wind(material)
        a.finish_material(material)


def apply_materials(only=None):
    """Set the listed material instances' profiles (all, or those among the paths in only); returns their paths."""
    done = []
    for path, entry in assign.MATERIALS.items():
        if only is not None and path not in {assign.package_path(item) for item in only}:
            continue
        sizes = [wind.mesh_size_cm(s.required(mesh)) for mesh in entry.size_meshes]
        height, crown = max(size[0] for size in sizes), max(size[1] for size in sizes)
        wind.apply_profile(s.required(path), wind.w.PROFILES[entry.profile], height, crown, entry.flutter, entry.max_scale)
        ue.log(f"VEGETATION_WIND_MATERIAL {path} {entry.profile} height={height:.0f}cm crown={crown:.0f}cm flutter={entry.flutter} "
               f"max_scale={entry.max_scale:.2f}")
        done.append(path)
    return done


def apply_profiles_only():
    """Rebuild the wind function and set the listed instances' profiles; nothing placed changes."""
    backup([wind.MPC, wind.FUNCTION] + list(assign.MATERIALS))
    with s.savanna_helpers():
        wind.wind_function()
        materials = apply_materials()
    ue.log(f"OPEN_GOLF_VEGETATION_WIND_APPLIED materials={len(materials)} materials_only=True")


def main(keep_map_unsaved=False, materials_only=False):
    world = require_ready(keep_map_unsaved or materials_only)
    if materials_only:
        apply_profiles_only()
        return
    presets, components = wind_presets(), wind_components(world)
    backup(list(PARENTS) + list(assign.MATERIALS) + [p.get_outermost().get_name() for p, _ in presets] + [MAP])
    with s.savanna_helpers():
        wind.wind_function()
        add_hooks()
        materials = apply_materials()
    for preset, distance in presets:
        preset.modify()
        preset.set_editor_property("wind_motion", True)
        preset.set_editor_property("wind_motion_disable_distance_m", distance)
        if not LIB.save_loaded_asset(preset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {preset.get_path_name()}")
    counts = {}
    for component, distance in components:
        wind.enable_wind_motion(component, distance)
        mesh = component.get_editor_property("static_mesh").get_name()
        counts[mesh] = counts.get(mesh, 0) + component.get_instance_count()
    if keep_map_unsaved:
        ue.log_warning(f"{MAP} changed and was left unsaved, as asked; save it to keep the wind")
    elif components and not ue.EditorLoadingAndSavingUtils.save_packages([world.get_outermost()], only_dirty=False):
        raise RuntimeError(f"Could not save {MAP}")
    ue.log(f"OPEN_GOLF_VEGETATION_WIND_APPLIED materials={len(materials)} presets={len(presets)} "
           f"components={len(components)} instances={counts}")


if __name__ == "__main__":
    s = importlib.reload(s)
    a, wind = s.a, s.wind
    main(keep_map_unsaved="--keep-map-unsaved" in sys.argv[1:], materials_only="--materials-only" in sys.argv[1:])
