# SPDX-License-Identifier: MIT
"""Hidden ball collision that follows a tree's solid wood.

A separately exported wood-only mesh (the trunk and main limbs) collides with triangle collision;
branches and leaves stay passable to blocking traces (a canopy volume may slow the ball instead). The terrain's hidden tree-collider components
(tagged OpenGolf.Terrain.TreeColliders and OpenFairway.Obstacle) swap their mesh for the proxy
at each tree's exact transform. The visible tree meshes are not changed.
"""
import json
from pathlib import Path

import unreal as ue
from tree_replacement_rules import validate_pairs

TREES_TAG = "OpenGolf.Terrain.Trees"
COLLIDERS_TAG = "OpenGolf.Terrain.TreeColliders"
OBSTACLE_TAG = "OpenFairway.Obstacle"
BALL_RADIUS_CM = 2.135


def _key(position):
    return round(position.x, 1), round(position.y, 1)


def tree_data(actors, tree_mesh):
    """Tree transforms by trunk position, and the hidden collider components."""
    trees, colliders = {}, []
    for actor in actors:
        for component in actor.get_components_by_class(ue.InstancedStaticMeshComponent):
            if actor.actor_has_tag(TREES_TAG):
                if component.get_editor_property("static_mesh").get_path_name().split(".")[0] != tree_mesh:
                    raise RuntimeError(f"Expected only {tree_mesh} in the baked trees")
                for index in range(component.get_instance_count()):
                    transform = component.get_instance_transform(index, world_space=True)
                    key = _key(transform.translation)
                    if key in trees:
                        raise RuntimeError("Two trees have the same trunk position")
                    trees[key] = transform
            elif actor.actor_has_tag(COLLIDERS_TAG):
                colliders.append(component)
    return trees, colliders


def _same_transform(a, b, location_cm=0.01, scale=0.0001, rotation=0.0001):
    """Python's Vector/Quat equals() takes no tolerance, so compare components directly."""
    p, q = a.rotation, b.rotation
    dot = p.x * q.x + p.y * q.y + p.z * q.z + p.w * q.w  # |dot| is 1 for the same rotation
    return ((a.translation - b.translation).length() <= location_cm
            and (a.scale3d - b.scale3d).length() <= scale
            and 1 - abs(dot) <= rotation)


def _hit_component(result):
    """The hit component; Python exposes BreakHitResult only as HitResult.to_tuple()."""
    return next((value for value in result.to_tuple() if isinstance(value, ue.PrimitiveComponent)), None)


def _section_slots(mesh):
    subsystem = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    slots = mesh.get_editor_property("static_materials")
    for lod in range(mesh.get_num_lods()):
        for section in range(mesh.get_num_sections(lod)):
            yield lod, section, str(slots[subsystem.get_lod_material_slot(mesh, lod, section)].material_slot_name)


def _geometry(mesh):
    """Triangle count and bounds; Nanite meshes report their full-detail triangles."""
    if mesh.get_editor_property("nanite_settings").enabled:
        triangles = mesh.get_num_nanite_triangles()
    else:
        triangles = sum(mesh.get_num_triangles(lod) for lod in range(mesh.get_num_lods()))
    bounds = mesh.get_bounds()
    return triangles, tuple(round(v, 2) for v in (bounds.origin.x, bounds.origin.y, bounds.origin.z,
                                                  bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z))


def _box(mesh):
    bounds = mesh.get_bounds()
    return bounds.origin - bounds.box_extent, bounds.origin + bounds.box_extent


def proxy_mesh(source, proxy, wood_slots):
    """Check and configure the imported wood-only collision mesh (blender/ExportJacaranda.py) for source."""
    library = ue.EditorAssetLibrary
    source_mesh = library.load_asset(source)
    mesh = library.load_asset(proxy)
    if not source_mesh or not mesh or source == proxy:
        raise RuntimeError(f"Import {source} and its collision mesh {proxy} first (ImportSavannaAssets.py)")
    slots = {slot for _, _, slot in _section_slots(mesh)}
    if slots != set(wood_slots):
        raise RuntimeError(f"{proxy} has sections {sorted(slots)}; expected only the wood {sorted(wood_slots)}")
    low, high = _box(mesh)
    source_low, source_high = _box(source_mesh)
    tolerance = 5.0  # cm; both come from the same Blender object and FBX settings
    if any(getattr(low, axis) < getattr(source_low, axis) - tolerance or getattr(high, axis) > getattr(source_high, axis) + tolerance
           for axis in "xyz"):
        raise RuntimeError(f"{proxy} does not line up with {source}; re-export both from the same project")
    subsystem = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    nanite = mesh.get_editor_property("nanite_settings")
    if nanite.enabled:
        nanite.set_editor_property("enabled", False)
        subsystem.set_nanite_settings(mesh, nanite, True)
    for lod, section, _ in _section_slots(mesh):
        if not subsystem.is_section_collision_enabled(mesh, lod, section):
            subsystem.enable_section_collision(mesh, True, lod, section)
    body = mesh.get_editor_property("body_setup")
    if body.get_editor_property("collision_trace_flag") != ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
        body.set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if not library.save_loaded_asset(mesh, only_if_is_dirty=True):
        raise RuntimeError(f"Could not save {proxy}")
    # Tag the tree so reimporting it refreshes this proxy. Only when missing: a metadata write dirties
    # the 800k-triangle tree package. The wood slots come from the export manifest.
    if library.get_metadata_tag(source_mesh, "OpenGolf.WoodProxy") != proxy:
        library.set_metadata_tag(source_mesh, "OpenGolf.WoodProxy", proxy)
    return mesh


def apply(terrain, tree_mesh, proxy, wood_slots):
    """Give every hidden tree collider instance the wood proxy at its tree's transform."""
    if ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError("Stop Play before applying tree collision")
    trees, colliders = tree_data([actor for actor in terrain.generated_actors if actor], tree_mesh)
    if not trees or sum(c.get_instance_count() for c in colliders) != len(trees):
        raise RuntimeError("Every tree must have one collision instance before replacement")
    replacements = []
    keys = []
    for component in colliders:
        transforms = []
        for index in range(component.get_instance_count()):
            key = _key(component.get_instance_transform(index, world_space=True).translation)
            keys.append(key)
            transform = trees.get(key)
            if not transform:
                raise RuntimeError("A tree collision instance has no matching visible tree")
            transforms.append(transform)
        replacements.append((component, transforms))
    validate_pairs(trees, keys)
    mesh = proxy_mesh(tree_mesh, proxy, wood_slots)
    for component, transforms in replacements:
        component.modify()
        component.clear_instances()
        component.set_static_mesh(mesh)
        component.set_collision_profile_name("BlockAll")
        component.set_visibility(False)
        component.set_hidden_in_game(True)
        component.set_cast_shadow(False)
        component.set_editor_property("component_tags", list(set(str(t) for t in component.component_tags) | {OBSTACLE_TAG}))
        component.add_instances(transforms, False, True)
    ue.log(f"TREE_WOOD_COLLISION_APPLIED {len(trees)} trees; wood sections {sorted(wood_slots)}")
    return len(trees)


def probe_faces(wood_mesh, wood_slots, passable_mesh, passable_slots):
    """One outward-facing triangle per wood section (from the collision proxy) and one outer triangle
    per passable section (from the visible tree), in mesh space.

    Wood faces come from the non-Nanite proxy: for a Nanite mesh Python only sees the coarse
    fallback, whose faces can lie centimetres off the geometry the proxy collides with. Passable
    faces only need to lie clear of all wood, outside the proxy's bounds by more than a probe sweep,
    so the fallback is enough.
    """
    wood_low, wood_high = _box(wood_mesh)
    clearance = 30 + 2 * BALL_RADIUS_CM + 10  # the sweep length either side of the face, plus margin
    faces = []
    for mesh, slots, expect_hit in ((wood_mesh, wood_slots, True), (passable_mesh, passable_slots, False)):
        height = _box(mesh)[1].z
        for wanted in sorted(slots):
            section = next((s for lod, s, slot in _section_slots(mesh) if lod == 0 and slot == wanted), None)
            if section is None:
                raise RuntimeError(f"{mesh.get_name()} has no section {wanted}")
            vertices, triangles, *_ = ue.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)
            best = None
            for offset in range(0, len(triangles), 3):
                a, b, c = (vertices[triangles[offset + i]] for i in range(3))
                center = (a + b + c) / 3
                if expect_hit and not 0.2 * height < center.z < 0.6 * height:  # cheap test before the vector math
                    continue
                if not expect_hit and all(getattr(wood_low, axis) - clearance < getattr(center, axis) < getattr(wood_high, axis) + clearance
                                          for axis in "xyz"):
                    continue
                normal = ue.MathLibrary.cross_vector_vector(b - a, c - a)
                length = normal.length()
                if length < 1:
                    continue
                normal /= length
                # Wood: a mid-height, sideways-facing face. Passable: the outermost face clear of the wood.
                if expect_hit and abs(normal.z) < 0.6:
                    score = length
                elif not expect_hit:
                    score = (center.x ** 2 + center.y ** 2) ** 0.5
                else:
                    continue
                if best is None or score > best[0]:
                    best = (score, center, normal)
            if not best:
                raise RuntimeError(f"No probe face found in section {wanted}")
            faces.append((wanted, best[1], best[2], expect_hit))
    return faces


def check(world, actors, tree_mesh, wood_slots, passable_slots, report_path=None, proxy=None):
    """Ball-sized sweeps through each tree's wood faces (must hit) and its branch and leaf faces (must miss)."""
    trees, colliders = tree_data(actors, tree_mesh)
    faces = probe_faces(ue.load_asset(proxy or tree_mesh), wood_slots, ue.load_asset(tree_mesh), passable_slots)
    bounds = ue.load_asset(tree_mesh).get_bounds()
    faces.append(("open gap", ue.Vector(bounds.origin.x + bounds.box_extent.x + 200, 0,
                                        bounds.origin.z), ue.Vector(1, 0, 0), False))
    failures, hits, clear = [], 0, 0
    keys = []
    for component in colliders:
        if component.is_visible():
            failures.append("A collision mesh is visible")
        if not component.component_has_tag(OBSTACLE_TAG):
            failures.append("Missing ball obstacle tag")
        if proxy and component.static_mesh.get_path_name().split(".")[0] != proxy:
            failures.append("Unexpected collision mesh")
        ignored = [actor for actor in actors if actor != component.get_owner()]
        for index in range(component.get_instance_count()):
            actual = component.get_instance_transform(index, world_space=True)
            key = _key(actual.translation)
            keys.append(key)
            transform = trees.get(key)
            if not transform:
                failures.append(f"Collision has no tree at {key}")
                continue
            if not _same_transform(actual, transform):
                failures.append(f"Collider transform differs from its tree at {key}")
            for name, point, normal, expected in faces:
                # Sweep through the face both ways: imported winding may point the normal inward, and a
                # sweep that starts inside a thick trunk passes its wall from behind without a hit.
                hit = False
                for side in (1, -1):
                    start = ue.MathLibrary.transform_location(transform, point + normal * (30 * side))
                    end = ue.MathLibrary.transform_location(transform, point - normal * (30 * side))
                    result = ue.SystemLibrary.sphere_trace_single(world, start, end, BALL_RADIUS_CM, ue.TraceTypeQuery.ECC_VISIBILITY,
                                                               False, ignored, ue.DrawDebugTrace.NONE, False)
                    if result is not None:
                        hit = True
                        if _hit_component(result) != component:
                            failures.append(f"Sweep hit a different component at {key}")
                if hit != expected:
                    failures.append(f"{name}: expected hit={expected}, got {hit} at {key}")
                elif hit:
                    hits += 1
                else:
                    clear += 1
    try:
        validate_pairs(trees, keys)
    except ValueError as error:
        failures.append(str(error))
    report = {"trees": len(trees), "colliders": sum(c.get_instance_count() for c in colliders), "wood_hits": hits,
              "expected_wood_hits": len(trees) * len(wood_slots), "clear_leaf_sweeps": clear,
              "expected_clear": len(trees) * (len(passable_slots) + 1), "failures": failures[:50], "failure_count": len(failures)}
    if report_path:
        Path(report_path).parent.mkdir(parents=True, exist_ok=True)
        Path(report_path).write_text(json.dumps(report, indent=2))
    ue.log(f"TREE_WOOD_COLLISION_CHECK {len(trees)} trees; {hits} wood hits; {clear} clear; {len(failures)} failures")
    if failures:
        raise RuntimeError(f"Tree collision validation failed: {failures[:5]}")
    return report
