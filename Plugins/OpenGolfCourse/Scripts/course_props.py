# SPDX-License-Identifier: MIT
"""Prop assets: the flagstick, its flag material and the marked golf ball."""
import unreal as ue

from course_assets import (connect, custom, delete_if_unreferenced, finish_function, finish_material, function_call,
                           function_input, function_output, import_asset, material, material_function, material_instance,
                           node, output, save, scalar, vector, MAT)

FLAG_FOLDER = "Flagstick"
BALL_FOLDER = "Ball"
FLAG_RED = (0.65, 0.025, 0.018, 1.0)


def build_flag():
    # The flag is cloth simulated by UOpenGolfFlagstickComponent, so its material only shades it.
    flag = material(FLAG_FOLDER, "M_FlagCloth")
    flag.set_editor_property("two_sided", True)
    output(flag, vector(flag, "FlagColor", FLAG_RED, "Flag", 0), ue.MaterialProperty.MP_BASE_COLOR, "RGB")
    output(flag, scalar(flag, "FlagRoughness", 0.8, "Flag", 1), ue.MaterialProperty.MP_ROUGHNESS)
    finish_material(flag)
    red = material_instance(FLAG_FOLDER, "MI_Flag_Red", flag)
    MAT.clear_all_material_instance_parameters(red)
    MAT.set_material_instance_vector_parameter_value(red, "FlagColor", ue.LinearColor(*FLAG_RED))
    MAT.update_material_instance(red)
    save(red)
    # Old wave assets can stay unused; loaded editor graphs must not be deleted during migration.
    return red


def build_pole():
    pole = material(FLAG_FOLDER, "M_Pole")
    output(pole, vector(pole, "PoleColor", (0.8, 0.82, 0.84, 1.0), "Pole"), ue.MaterialProperty.MP_BASE_COLOR, "RGB")
    output(pole, scalar(pole, "PoleRoughness", 0.35, "Pole"), ue.MaterialProperty.MP_ROUGHNESS)
    return finish_material(pole)


def build_ball_marking():
    function = material_function(BALL_FOLDER, "MF_BallMarking",
                                 "Alignment line (the local XZ great circle) and a dot on it at local -X, "
                                 "anti-aliased by pixel footprint. Outputs 0-1 masks.")
    line = function_input(function, "LineHalfWidth", ue.FunctionInputType.FUNCTION_INPUT_SCALAR, (0.03, 0, 0, 0), 0,
                          "Half width of the line as a fraction of the radius.")
    radius = function_input(function, "DotRadius", ue.FunctionInputType.FUNCTION_INPUT_SCALAR, (0.14, 0, 0, 0), 1,
                            "Dot radius as a fraction of the ball radius.")
    position = node(function, ue.MaterialExpressionLocalPosition, "LocalPosition")
    masks = custom(function, "BallMarking",
                   "float3 n = normalize(P);\n"
                   "float lineDist = abs(n.y);\n"
                   "float lineEdge = max(fwidth(lineDist), 1e-4);\n"
                   "float lineMask = 1.0 - smoothstep(LineHalfWidth - lineEdge, LineHalfWidth + lineEdge, lineDist);\n"
                   "float dotDist = n.x < 0.0 ? length(n.yz) : 2.0;\n"
                   "float dotEdge = max(fwidth(dotDist), 1e-4);\n"
                   "float dotMask = 1.0 - smoothstep(DotRadius - dotEdge, DotRadius + dotEdge, dotDist);\n"
                   "return float2(lineMask, dotMask);",
                   {"P": (position, ""), "LineHalfWidth": (line, ""), "DotRadius": (radius, "")},
                   ue.CustomMaterialOutputType.CMOT_FLOAT2)
    for sort, (name, red) in enumerate((("Line", True), ("Dot", False))):
        mask = node(function, ue.MaterialExpressionComponentMask, f"{name}Mask", r=red, g=not red, b=False, a=False)
        connect(masks, mask, "")
        function_output(function, name, mask, sort)
    return finish_function(function)


def build_ball(marking):
    ball = material(BALL_FOLDER, "M_GolfBall")
    call = function_call(ball, marking, "BallMarking")
    connect(scalar(ball, "LineHalfWidth", 0.03, "Marking", 3), call, "LineHalfWidth")
    connect(scalar(ball, "DotRadius", 0.14, "Marking", 4), call, "DotRadius")
    color = custom(ball, "BallColor", "return lerp(lerp(Ball, Line, LineMask), Dot, DotMask);",
                   {"Ball": (vector(ball, "BallColor", (0.92, 0.92, 0.88, 1.0), "Marking", 0), "RGB"),
                    "Line": (vector(ball, "LineColor", (0.02, 0.02, 0.025, 1.0), "Marking", 1), "RGB"),
                    "Dot": (vector(ball, "DotColor", (0.02, 0.08, 0.45, 1.0), "Marking", 2), "RGB"),
                    "LineMask": (call, "Line"), "DotMask": (call, "Dot")},
                   ue.CustomMaterialOutputType.CMOT_FLOAT3)
    output(ball, color, ue.MaterialProperty.MP_BASE_COLOR)
    output(ball, scalar(ball, "BallRoughness", 0.3, "Marking", 5), ue.MaterialProperty.MP_ROUGHNESS)
    return finish_material(ball)


def build_props():
    flagstick = import_asset("SM_Flagstick.fbx", FLAG_FOLDER, "SM_Flagstick", mesh=True)
    bounds = flagstick.get_bounding_box()
    if bounds.max.y - bounds.min.y > 1.3 or bounds.max.x - bounds.min.x > 1.3:
        raise RuntimeError("Flagstick must contain only the pole; export the updated Blender source first")
    # Reimport can retain unused slots from the old flag-bearing mesh.
    flagstick.set_editor_property("static_materials", list(flagstick.get_editor_property("static_materials"))[:1])
    flagstick.set_material(0, build_pole())
    save(flagstick)
    build_flag()

    golf_ball = import_asset("SM_GolfBall.fbx", BALL_FOLDER, "SM_GolfBall", mesh=True)
    golf_ball.set_material(0, build_ball(build_ball_marking()))
    # The source is detailed; built-in reduction keeps close detail and drops it with distance.
    options = ue.EditorScriptingMeshReductionOptions()
    options.auto_compute_lod_screen_size = False
    options.reduction_settings = [ue.EditorScriptingMeshReductionSettings(percent_triangles=p, screen_size=s)
                                  for p, s in ((1.0, 1.0), (0.25, 0.1), (0.05, 0.025))]
    editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    if not editor:
        raise RuntimeError("Run inside the editor (or -ExecutePythonScript): LODs need editor subsystems.")
    editor.set_lods(golf_ball, options)
    save(golf_ball)
    ue.log("Props built: SM_Flagstick with M_Pole, MI_Flag_Red on M_FlagCloth, SM_GolfBall with M_GolfBall")
