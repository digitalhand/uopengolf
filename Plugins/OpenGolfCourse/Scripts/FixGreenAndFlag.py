# SPDX-License-Identifier: MIT
"""Repair green stripes and the flag without rebuilding other course materials or textures."""
from pathlib import Path
import importlib
import sys
import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
import course_assets
import course_props
for module in (course_assets, course_props):
    importlib.reload(module)

material_tools = ue.MaterialEditingLibrary
green = ue.load_asset("/OpenGolfCourse/Surfaces/MI_Green")
if not green:
    raise RuntimeError("The existing green material is missing")
material_tools.set_material_instance_scalar_parameter_value(green, "StripeContrast", 0.0)
material_tools.update_material_instance(green)
course_assets.save(green)

pole = course_assets.import_asset("SM_Flagstick.fbx", "Flagstick", "SM_Flagstick", mesh=True)
bounds = pole.get_bounding_box()
if bounds.max.x - bounds.min.x > 1.3 or bounds.max.y - bounds.min.y > 1.3:
    raise RuntimeError("Export the pole-only Blender source before importing the flagstick")
pole.set_editor_property("static_materials", list(pole.get_editor_property("static_materials"))[:1])
course_assets.save(pole)
course_props.build_flag()
ue.log("OPEN_GOLF_GREEN_AND_FLAG_FIXED")
