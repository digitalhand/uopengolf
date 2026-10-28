# SPDX-License-Identifier: MIT
"""Give the turf its mixed greens and remove the squares from the tall rough. Materials only.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/FixGrassPalette.py"

Pass --maintained-only to retain the tall rough's material and tuning.

Open /Game/UOpenGolf/Maps/UOpenGolf with Play stopped, after FixTurfTransitions.py has run once.
- M_SavannaGrass colours the tall rough by world-space patch noise instead of per instance, so the
  tiled squares of tufts no longer show; MI_GrassMedium01 takes the muted rough settings.
- The turf field materials are rebuilt with the dry/lush palette (turf_palette.py) and every turf
  instance is configured again (FixTurfTransitions.fix).
Terrain, outlines, grass placement and physics are not touched. Assets are backed up to Saved/TurfAssetBackups.
"""
from datetime import datetime, timezone
from pathlib import Path
import importlib
import shutil
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import FixTurfTransitions as transitions
import savanna_assets as s
import turf_palette as palette

GRASS = ['/OpenGolfSavanna/Materials/M_SavannaGrass', '/OpenGolfSavanna/Materials/Grass/Medium/MI_GrassMedium01']


def backup():
    folder = transitions.PROJECT / 'Saved/TurfAssetBackups' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-before-palette')
    for path in GRASS:
        source = transitions.course.package_file(path, '.uasset')
        if not source.exists():
            raise RuntimeError(f'Missing {path}; import the savanna grass first')
        target = folder / (path.strip('/') + '.uasset')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    return folder


def rough_grass():
    import savanna_grass
    with s.savanna_helpers():
        importlib.reload(savanna_grass).grass_material()
    instance = s.required(GRASS[1])
    for name, value in palette.ROUGH_GRASS.items():
        if isinstance(value, str):
            ue.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
                instance, name, ue.LinearColor(*s.a.linear_color(value)))
        else:
            ue.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance, name, value)
    ue.MaterialEditingLibrary.update_material_instance(instance)
    if not ue.EditorAssetLibrary.save_loaded_asset(instance):
        raise RuntimeError('Could not save ' + GRASS[1])


def fix(maintained_only=False):
    global palette, transitions
    palette = importlib.reload(palette)
    transitions = importlib.reload(transitions)
    folder = None
    if not maintained_only:
        folder = backup()
        rough_grass()
    # Rebuilds the field materials with the palette, configures every turf instance and saves the map.
    transitions.fix()
    ue.log('OPEN_GOLF_GRASS_PALETTE_COMPLETE ' + str(folder))


if __name__ == '__main__':
    fix(maintained_only='--maintained-only' in sys.argv)
