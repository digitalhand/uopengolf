# SPDX-License-Identifier: MIT
"""Create the savanna course grass presets from the imported Grass Medium tiles. Meshes and materials are not changed.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/BuildSavannaGrassPresets.py"

  DA_RoughGrassDense   tiled 1 m / 0.5 m / 0.25 m squares, edge to edge, over the rough 0-12 m from play
  DA_BoundaryGrass     shorter 1 x 0.25 m edge strips along every play outline

Beyond the rough the ground stays open: scattered tiles there read as spots on bare soil.
Set the first as the terrain area's CourseGrassPresets and the second as its BoundaryGrassPreset, or use the
course kit's ApplyCourseGrass.py. Rebuilding resets the starting settings; copy a preset to tune another course.
"""
from pathlib import Path
import sys
import unreal as ue

PROJECT = Path(ue.Paths.project_dir()).resolve()
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import course_assets as a
import BuildTerrainAssets as terrain
import vegetation_wind_assignments as wind

MESHES = '/OpenGolfSavanna/Meshes/Grass/Medium/SM_GrassMedium_30cm_'
TILES = ((1.0, '1m', 'ABCD'), (0.5, '50cm', 'AB'), (0.25, '25cm', 'AB'))
# Ground cover fades out from 75% of its cull distance onto the matching ground colour.
# The tiles' material sways them (vegetation_wind_assignments.py) until they fade out.
COMMON = {'surface': ue.OpenFairwaySurface.ROUGH, 'kind': ue.OpenGolfVegetationKind.GROUND_COVER,
          'maximum_slope_deg': 35.0, 'seed': 1731, 'wind_motion': True,
          'wind_motion_disable_distance_m': wind.mesh_wind_distance_m(MESHES + '1m_A')}
PRESETS = {
    # Tile footprints keep 0.4 m from play; blades overhang about 0.12 m and the edge strips fill the rest.
    'DA_RoughGrassDense': {'placement_pattern': ue.OpenGolfPlacementPattern.TILED, 'boundary_clearance_m': 0.4,
                           'maximum_distance_from_play_m': 12.0, 'maximum_tile_tilt_deg': 6.0, 'tile_sink_m': 0.02,
                           'cull_distance_m': 90.0},
    'DA_BoundaryGrass': {'boundary_clearance_m': 0.15, 'edge_strip_size_m': ue.Vector2D(1.0, 0.25),
                         'scale_range': ue.Vector2D(0.95, 1.05), 'tile_sink_m': 0.02, 'cull_distance_m': 90.0},
}


def load(name):
    mesh = ue.load_asset(MESHES + name)
    if not mesh:
        raise RuntimeError(f'Import the Grass Medium tiles first: {MESHES}{name} is missing')
    return mesh


def tiles():
    result = []
    for size, label, variants in TILES:
        tile = ue.OpenGolfGrassTile()
        tile.set_editor_property('size_m', size)
        tile.set_editor_property('meshes', [load(f'{label}_{v}') for v in variants])
        result.append(tile)
    return result


def build():
    previous = (a.MOUNT, a.OWNER, a.CONTENT_DIR)
    try:
        a.MOUNT = '/OpenGolfSavanna'
        a.OWNER = 'OpenGolfSavanna.CourseGrass.v1'
        a.CONTENT_DIR = PROJECT / 'Plugins/OpenGolfSavanna/Content'
        meshes = {'DA_RoughGrassDense': ('tiles', tiles()),
                  'DA_BoundaryGrass': ('meshes', [load(f'Edge_{v}') for v in 'AB'])}
        presets = []
        for name, settings in PRESETS.items():
            preset = terrain.create_data(name, ue.OpenGolfVegetationPreset, 'Terrain')
            field, value = meshes[name]
            preset.set_editor_property('meshes', [])
            preset.set_editor_property('tiles', [])
            for key, setting in {**COMMON, **settings, field: value}.items():
                preset.set_editor_property(key, setting)
            presets.append(a.save(preset))
        ue.log(f'OPEN_GOLF_COURSE_GRASS_PRESETS_COMPLETE {[p.get_name() for p in presets]}')
        return presets
    finally:
        a.MOUNT, a.OWNER, a.CONTENT_DIR = previous


if __name__ == '__main__':
    build()
