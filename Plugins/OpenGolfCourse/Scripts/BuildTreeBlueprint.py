# SPDX-License-Identifier: MIT
"""Build or update a placeable tree Blueprint: the visible tree with its hidden wood and canopy (editor only).

    py "<project>/Plugins/OpenGolfCourse/Scripts/BuildTreeBlueprint.py" --tree /<Course>/.../SM_<Species>
        --wood /<Course>/.../SM_<Species>_Collision --canopy /<Course>/.../SM_<Species>_Canopy
        --blueprint /<Course>/Vegetation/Trees/BP_Tree_<Species> [--wind-distance-cm 15000]

The parts and their collision, tags and visibility are in tree_blueprint_rules.py. The wood mesh must collide on its
triangles and the canopy must have a convex hull (ImportSavannaAssets.py sets both). The Blueprint is created when
missing; an existing one is updated only when this script made it, so placed copies keep working. Completion is
logged as OPEN_GOLF_TREE_BLUEPRINT_COMPLETE. Building the same actor by hand is described in
Docs/CourseDesign/course_vegetation.md (step 3c).
"""
from pathlib import Path
import importlib
import sys

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tree_blueprint_rules as rules

rules = importlib.reload(rules)
OWNER = "OpenGolfCourse.TreeBlueprint.v1"
LIB = ue.EditorAssetLibrary
SUBOBJECTS = ue.SubobjectDataBlueprintFunctionLibrary


def mesh(path, collision):
    asset = LIB.load_asset(path)
    if not isinstance(asset, ue.StaticMesh):
        raise RuntimeError(f"Not a static mesh: {path}")
    body = asset.get_editor_property("body_setup")
    if collision == "complex" and body.get_editor_property("collision_trace_flag") != ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
        raise RuntimeError(f"{path} must collide on its triangles (complex as simple); import it with ImportSavannaAssets.py")
    if collision == "convex" and ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem).get_convex_collision_count(asset) < 1:
        raise RuntimeError(f"{path} needs a convex hull: an overlap only sees inside simple collision")
    return asset


def blueprint(path):
    if LIB.does_asset_exist(path):
        asset = LIB.load_asset(path)
        if LIB.get_metadata_tag(asset, "Generator") != OWNER:
            raise RuntimeError(f"Refusing to change {path}: this script did not make it")
        return asset
    folder, name = path.rsplit("/", 1)
    factory = ue.BlueprintFactory()
    factory.set_editor_property("parent_class", ue.Actor)
    asset = ue.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, ue.Blueprint, factory)
    if not asset:
        raise RuntimeError(f"Could not create {path}")
    LIB.set_metadata_tag(asset, "Generator", OWNER)
    return asset


def templates(subsystem, bp):
    """(handles, {variable name: component template}) of the Blueprint's components."""
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    found = {}
    for handle in handles:
        data = SUBOBJECTS.get_data(handle)
        template = SUBOBJECTS.get_object_for_blueprint(data, bp)
        if isinstance(template, ue.ActorComponent):
            found[str(SUBOBJECTS.get_variable_name(data))] = template
    return handles, found


def component(subsystem, bp, name):
    """The named static mesh component, added under the root when missing."""
    handles, found = templates(subsystem, bp)
    if name in found:
        if not isinstance(found[name], ue.StaticMeshComponent):
            raise RuntimeError(f"{bp.get_path_name()}: {name} is not a static mesh component")
        return found[name]
    params = ue.AddNewSubobjectParams(parent_handle=handles[0], new_class=ue.StaticMeshComponent, blueprint_context=bp)
    handle, reason = subsystem.add_new_subobject(params)
    if not subsystem.rename_subobject(handle, ue.Text(name)):
        raise RuntimeError(f"Could not name the {name} component: {reason}")
    template = SUBOBJECTS.get_object_for_blueprint(SUBOBJECTS.get_data(handle), bp)
    if not isinstance(template, ue.StaticMeshComponent):
        raise RuntimeError(f"Could not add {name}: {reason}")
    return template


def configure(template, part, asset):
    template.set_editor_property("static_mesh", asset)
    template.set_collision_profile_name(part["profile"])
    for prop, key in (("visible", "visible"), ("hidden_in_game", "hidden_in_game"), ("cast_shadow", "cast_shadow"),
                      ("generate_overlap_events", "overlap_events"), ("can_ever_affect_navigation", "affects_navigation"),
                      ("evaluate_world_position_offset", "evaluate_wpo"), ("world_position_offset_disable_distance", "wpo_disable_distance")):
        template.set_editor_property(prop, part[key])
    template.set_editor_property("mobility", getattr(ue.ComponentMobility, part["mobility"]))
    template.set_editor_property("component_tags", list(part["tags"]))


def main(argv):
    args = rules.parse_args(argv)
    meshes = {"Tree": mesh(args.tree, None), "Wood": mesh(args.wood, "complex"), "Canopy": mesh(args.canopy, "convex")}
    bp = blueprint(args.blueprint)
    subsystem = ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
    # Static parts need a static root, or the engine warns and moves them every frame.
    for template in templates(subsystem, bp)[1].values():
        if isinstance(template, ue.SceneComponent) and not isinstance(template, ue.StaticMeshComponent):
            template.set_editor_property("mobility", ue.ComponentMobility.STATIC)
    for part in rules.components(args.tree, args.wood, args.canopy, args.wind_distance_cm):
        configure(component(subsystem, bp, part["name"]), part, meshes[part["name"]])
    ue.BlueprintEditorLibrary.compile_blueprint(bp)
    LIB.set_metadata_tag(bp, "Generator", OWNER)
    if not LIB.save_loaded_asset(bp, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {args.blueprint}")
    ue.log(f"OPEN_GOLF_TREE_BLUEPRINT_COMPLETE {args.blueprint} parts={sorted(templates(subsystem, bp)[1])}")


if __name__ == "__main__":
    main(sys.argv[1:])
