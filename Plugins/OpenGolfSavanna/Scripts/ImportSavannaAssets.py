# SPDX-License-Identifier: MIT
"""Import a savanna export manifest into /OpenGolfSavanna.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/ImportSavannaAssets.py" <export.json> [--materials-only] [--meshes=A,B]

The manifest comes from a Blender export script (for example blender/ExportJacaranda.py). It names
a content folder, the FBX meshes and, per material, its kind (card, solid or grass), texture layers and
an optional vertex-colour blend. A material with a `category` (for example Grass/Bermuda) uses
type-first folders: Meshes/<category>, Materials/<category> and Textures/<category>. Older manifests
name one `folder` holding Meshes, Materials and Textures subfolders. --materials-only keeps the
imported textures and meshes and rebuilds only the materials. --meshes=A,B imports only those meshes and
reuses the existing material instances, for example the jacaranda's collision helpers without the tree.

A mesh entry with `nanite: false` and a `collision` mode is a hidden ball-contact helper: "complex"
collides on its triangles (a tree's wood) and "convex" keeps the FBX's single UCX_ hull (a canopy
volume that overlap queries can see inside).
"""
from pathlib import Path
import importlib
import json
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import savanna_assets as s

s = importlib.reload(s)
a = s.a
# Tree canopies keep their leaf normals much further out than ground cover does.
CARD_SETTINGS = {"NormalStrength": 1.0, "NormalFadeStartCm": 3000.0, "NormalFadeEndCm": 8000.0, "AOStrength": 0.0,
                 "RoughnessMin": 0.6}
SOLID_SETTINGS = {"RoughnessMin": 0.5, "AOStrength": 0.0}
# Starting look for the rough; tune in the MI after review.
# Grass Medium's light, low-saturation blades read near white under the range's sun with upward
# normals and no shadows, so they are darkened and saturated here.
GRASS_SETTINGS = {"grass_medium_01": {"Brightness": 0.55, "Saturation": 1.6, "Tint": "B4D27E"}}


def folders(entry, folder):
    """Content folders for a material's meshes, instance and textures."""
    if "category" in entry:
        return {kind: f"{kind}/{entry['category']}" for kind in ("Meshes", "Materials", "Textures")}
    return {kind: f"{folder}/{kind}" for kind in ("Meshes", "Materials", "Textures")}


def instance_name(material):
    return "MI_" + "".join(part.capitalize() for part in material.replace("-", "_").split("_") if part)


def layer_textures(layer, folder, suffix, reuse):
    textures = {}
    normal = s.import_texture(layer["normal"], folder, "normal", reuse=reuse) if "normal" in layer else None
    textures["Albedo" + suffix] = s.import_texture(layer["albedo"], folder, "albedo", reuse=reuse)
    if "albedo_dry" in layer:
        textures["AlbedoDry"] = s.import_texture(layer["albedo_dry"], folder, "albedo_dry", reuse=reuse)
    if normal:
        textures["Normal" + suffix] = normal
    if "roughness" in layer:
        textures["Roughness" + suffix] = s.import_texture(layer["roughness"], folder, "roughness", normal, reuse=reuse)
    if not suffix:
        for role, parameter in (("ao", "AmbientOcclusion"), ("opacity", "Opacity")):
            if role in layer:
                textures[parameter] = s.import_texture(layer[role], folder, role, reuse=reuse)
    return textures


def build_materials(manifest, folder, reuse):
    s.white_mask(s.PROJECT / "Saved/AssetImport/OpenGolfSavanna/defaults")
    kinds = {entry["kind"] for entry in manifest["materials"].values()}
    parents = {"card": s.card_material(), "solid": s.solid_material()}
    if "grass" in kinds:
        import savanna_grass
        parents["grass"] = importlib.reload(savanna_grass).grass_material()
    result = {}
    for name, entry in manifest["materials"].items():
        layers = entry["layers"]
        if entry["kind"] in ("card", "grass") and len(layers) != 1:
            raise RuntimeError(f"{name}: {entry['kind']} materials take one texture layer")
        textures = {}
        target = folders(entry, folder)
        for index, layer in enumerate(layers[:2]):
            textures.update(layer_textures(layer, target["Textures"], "" if index == 0 else "B", reuse))
        if entry["kind"] == "textures":
            continue  # textures for another material, such as the ground under the grass tiles
        if entry["kind"] == "grass":
            textures.setdefault("AlbedoDry", textures["Albedo"])
            settings = {key: a.linear_color(value) if isinstance(value, str) else value
                        for key, value in GRASS_SETTINGS.get(name, {}).items()}
            result[name] = s.instance(target["Materials"], instance_name(name), parents["grass"], textures, settings)
            continue
        settings = dict(CARD_SETTINGS if entry["kind"] == "card" else SOLID_SETTINGS)
        if entry.get("blend") == "vertex_red":
            settings["VertexBlend"] = 1.0
        if entry["kind"] == "card":
            settings["OpacityFromAlbedoAlpha"] = 1.0 if "opacity_alpha" in layers[0] else 0.0
        result[name] = s.instance(target["Materials"], instance_name(name), parents[entry["kind"]], textures, settings)
    return result


def existing_materials(manifest, folder):
    """The material instances an earlier import made, for importing meshes on their own."""
    result = {}
    for name, entry in manifest["materials"].items():
        if entry["kind"] == "textures":
            continue
        path = f"/OpenGolfSavanna/{folders(entry, folder)['Materials']}/{instance_name(name)}"
        material = ue.EditorAssetLibrary.load_asset(path)
        if not material:
            raise RuntimeError(f"Import the materials first: {path} is missing")
        result[name] = material
    return result


def configure_helper(mesh, item):
    """Collision settings for a hidden ball-contact helper; it is never drawn, so Nanite stays off."""
    subsystem = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    nanite = mesh.get_editor_property("nanite_settings")
    if nanite.enabled:
        nanite.set_editor_property("enabled", False)
        subsystem.set_nanite_settings(mesh, nanite, True)
    body = mesh.get_editor_property("body_setup")
    if item["collision"] == "complex":
        for lod in range(mesh.get_num_lods()):
            for section in range(mesh.get_num_sections(lod)):
                if not subsystem.is_section_collision_enabled(mesh, lod, section):
                    subsystem.enable_section_collision(mesh, True, lod, section)
        body.set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    elif item["collision"] == "convex":
        hulls = subsystem.get_convex_collision_count(mesh)
        if hulls != 1:
            raise RuntimeError(f"{item['name']}: expected one UCX_ hull, found {hulls}")
        body.set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    else:
        raise RuntimeError(f"{item['name']}: unknown collision mode {item['collision']}")


def adopt_wood_proxy(manifest, folder, item):
    """Older wood proxies were duplicated from the tree in Unreal, so they lack the importer's owner tag.
    Adopt one only when its tree names it as the wood proxy."""
    path = f"/OpenGolfSavanna/{folders(manifest['materials'][item['materials'][0]], folder)['Meshes']}/{item['name']}"
    library = ue.EditorAssetLibrary
    if item["kind"] != "tree_complex" or not library.does_asset_exist(path):
        return
    proxy = library.load_asset(path)
    if library.get_metadata_tag(proxy, "Generator") == a.OWNER:
        return
    for tree in (entry for entry in manifest["meshes"] if not entry.get("collision")):
        tree_path = path.rsplit("/", 1)[0] + "/" + tree["name"]
        if library.does_asset_exist(tree_path) and library.get_metadata_tag(library.load_asset(tree_path), "OpenGolf.WoodProxy") == path:
            library.set_metadata_tag(proxy, "Generator", a.OWNER)
            ue.log(f"SAVANNA_ADOPTED_WOOD_PROXY {path}")
            return


def import_meshes(manifest, folder, materials, only=None):
    meshes = []
    helpers = {item["kind"]: item for item in manifest["meshes"] if item.get("collision")}
    for item in manifest["meshes"]:
        if only and item["name"] not in only:
            continue
        vertex_colors = any(manifest["materials"][m].get("blend") == "vertex_red" for m in item["materials"])
        target = folders(manifest["materials"][item["materials"][0]], folder)
        adopt_wood_proxy(manifest, folder, item)
        mesh = a.import_asset(item["fbx"], target["Meshes"], item["name"], mesh=True,
                              collision=item.get("collision") == "convex", vertex_colors=vertex_colors)
        slots = mesh.get_editor_property("static_materials")
        for index, slot in enumerate(slots):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            key = next((name for name in materials if slot_name == name or slot_name.startswith(name + "_")), None)
            if not key:
                raise RuntimeError(f"{item['name']}: no material for slot {slot_name}")
            mesh.set_material(index, materials[key])
        if item.get("collision"):
            configure_helper(mesh, item)
        else:
            nanite = mesh.get_editor_property("nanite_settings")
            if not nanite.get_editor_property("enabled"):
                nanite.set_editor_property("enabled", True)
                mesh.set_editor_property("nanite_settings", nanite)
        meshes.append(a.save(mesh))
        proxy = ue.EditorAssetLibrary.get_metadata_tag(mesh, "OpenGolf.WoodProxy")
        if proxy and not item.get("collision"):
            import tree_wood_collision
            wood = importlib.reload(tree_wood_collision)
            wood_item = helpers.get("tree_complex")
            if not wood_item:
                raise RuntimeError(f"{item['name']} has a wood proxy, but the manifest has no collision mesh; "
                                   "re-export the tree")
            wood.proxy_mesh(mesh.get_path_name().split(".")[0], proxy, set(wood_item["slots"]))
            a.save(mesh)
        ue.log(f"SAVANNA_MESH {item['name']} slots={[str(x.get_editor_property('material_slot_name')) for x in slots]} "
               f"vertex_colors={vertex_colors}")
    return meshes


def main(argv):
    paths = [arg for arg in argv if not arg.startswith("--")]
    if len(paths) != 1:
        raise RuntimeError("Pass one export.json")
    manifest = json.loads(Path(paths[0]).read_text())
    reuse = "--materials-only" in argv
    only = next((set(arg.split("=", 1)[1].split(",")) for arg in argv if arg.startswith("--meshes=")), None)
    if only and not only <= {item["name"] for item in manifest["meshes"]}:
        raise RuntimeError(f"--meshes names a mesh the manifest does not have: {sorted(only)}")
    folder = manifest.get("folder", manifest.get("category"))
    with s.savanna_helpers():
        materials = existing_materials(manifest, folder) if only else build_materials(manifest, folder, reuse)
        meshes = [] if reuse else import_meshes(manifest, folder, materials, only)
    ue.log(f"SAVANNA_IMPORT_COMPLETE folder={folder} meshes={len(meshes)} materials={len(materials)} materials_only={reuse}")


if __name__ == "__main__":
    main(sys.argv[1:])
