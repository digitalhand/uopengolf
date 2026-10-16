# SPDX-License-Identifier: MIT
"""Reimport only the public grass clump after PrepareRangeAssets.py has run in Blender.

Run with UnrealEditor <project> -run=pythonscript -script=<absolute path to this file>.
ImportRangeAssets.py refuses to rebuild existing materials, so this replaces just
SM_GrassClump and keeps its M_GrassBlades material.
"""
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
DEST = '/Game/UOpenGolf/Art'
OWNER = 'UOpenGolf.ImportRangeAssets.v1'
LIB = u.EditorAssetLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()

clump = LIB.load_asset(DEST + '/SM_GrassClump')
assert clump and LIB.get_metadata_tag(clump, 'Generator') == OWNER, 'Refusing to replace unrelated SM_GrassClump'
task = u.AssetImportTask()
task.filename = str(ROOT / 'Saved/AssetImport/Prepared/SM_GrassClump.fbx')
task.destination_path = DEST
task.destination_name = 'SM_GrassClump'
task.automated = True
task.replace_existing = True
options = u.FbxImportUI()
options.import_mesh = True
options.import_materials = False
options.import_textures = False
options.import_as_skeletal = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type = False
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.auto_generate_collision = False
options.static_mesh_import_data.generate_lightmap_u_vs = False
task.options = options
TOOLS.import_asset_tasks([task])
clump = LIB.load_asset(DEST + '/SM_GrassClump')
clump.set_material(0, LIB.load_asset(DEST + '/M_GrassBlades'))
assert clump.get_material(0)
LIB.set_metadata_tag(clump, 'Generator', OWNER)
assert LIB.save_loaded_asset(clump, only_if_is_dirty=False)
u.log_warning('GRASS_CLUMP_COMPLETE bounds=%s' % clump.get_bounding_box())
