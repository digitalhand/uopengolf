# SPDX-License-Identifier: MIT
"""Generate the training target's line material, M_TargetRingLine.

The 3 ft and 6 ft ring edges are 5 cm bands lying flat on the ground. From the address camera
(2.2 m up, 70 degree FOV) their far arcs are thinner than a pixel, and as translucency TSR does
not average them, so a still camera shows them breaking into flickering dashes. This material
grows each edge in the vertex shader until it is at least MinPixels tall on screen, and the
pixel shader draws it as a smooth profile across that width, so camera jitter cannot break it up.

Ground bands (Source/UOpenGolf/Private/Training/GroundBands.cpp) supply, per vertex:
  UV1 = (t, band width in cm): t is 0 on the band's inner row and 1 on its outer row
  UV2 = the radial unit direction in XY, away from the target's centre

Run with UnrealEditor-Cmd -ExecutePythonScript, or py "<path>" in the editor, before a drill has
loaded the material in that editor session: rebuilding deletes its expressions.
"""
import unreal as u

DEST = '/Game/UOpenGolf/Art'
NAME = 'M_TargetRingLine'
PATH = f'{DEST}/{NAME}'
OWNER = 'UOpenGolf.TrainingRings.v1'
GROUP = 'Ring'
LIB = u.EditorAssetLibrary
MAT = u.MaterialEditingLibrary

# World Position Offset (xyz) and the share of the drawn width the band really covers (w).
# P is the vertex relative to the camera; N its ground normal.
# The band's screen width is measured across the drawn line: along G, perpendicular to both the view ray and the
# ring's tangent. The radial direction's whole screen projection is no measure of it: away from the middle of the far
# arc that projection runs mostly along the nearly level ellipse, so it overstates the width and moving along it only
# slides vertices along the line.
GROW_HLSL = '''
float D = max(length(P), 1.0);
float3 V = P / D;
float3 R = float3(Radial, 0.0);
R -= dot(R, N) * N;
R /= max(length(R), 1e-4);
float3 G = cross(V, cross(N, R));
float GLength = length(G);
G = GLength > 1e-4 ? G / GLength : float3(0.0, 0.0, 0.0);
// Across the line and away from the ground.
G *= dot(G, N) < 0.0 ? -1.0 : 1.0;
// One pixel at this distance, in cm.
float Px = D * 2.0 * TanHalfFov.x / max(ViewSize.x, 1.0);
// Positive where the outer row lies further along G (the far side), negative on the near side.
float Spread = dot(R, G);
float TruePx = Band.y * abs(Spread) / Px;
float Extra = max(MinPixels - TruePx, 0.0) * Px;
// The row further along G moves on along it, so nothing sinks into the ground and the inner row stays on the
// scoring radius on the far side.
float Grow = Spread >= 0.0 ? Band.x : 1.0 - Band.x;
return float4(Grow * Extra * G, TruePx / max(TruePx, MinPixels));
'''

# A profile across the drawn band that fades to zero at both rows, at least a pixel each side.
OPACITY_HLSL = '''
float W = max(fwidth(Band.x), 1e-4);
float Profile = saturate(Band.x / W) * saturate((1.0 - Band.x) / W);
return Opacity * lerp(MinOpacityScale, 1.0, saturate(Coverage)) * Profile;
'''


def enum(owner, name):
    """An enum value by its Python name, failing with the valid names if this engine lacks it."""
    try:
        return getattr(owner, name)
    except AttributeError:
        raise RuntimeError(f'{owner.__name__}.{name} not found; valid: {[n for n in dir(owner) if n.isupper()]}') from None


def node(material, cls, **properties):
    expression = MAT.create_material_expression(material, cls)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def scalar(material, name, default, sort):
    return node(material, u.MaterialExpressionScalarParameter, parameter_name=name, default_value=default, group=GROUP,
                sort_priority=sort)


def custom(material, description, code, inputs, output_type):
    """A Custom HLSL node; inputs maps pin name to the source expression (first output)."""
    pins = []
    for name in inputs:
        pin = u.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    expression = node(material, u.MaterialExpressionCustom, code=code, output_type=output_type, inputs=pins,
                      description=description)
    for name, source in inputs.items():
        assert MAT.connect_material_expressions(source, '', expression, name), f'{description}.{name}'
    return expression


def mask(material, source, r=False, g=False, b=False, a=False):
    expression = node(material, u.MaterialExpressionComponentMask, r=r, g=g, b=b, a=a)
    assert MAT.connect_material_expressions(source, '', expression, '')
    return expression


def view_property(material, name):
    return node(material, u.MaterialExpressionViewProperty, property=enum(u.MaterialExposedViewProperty, name))


def build_material(material):
    MAT.delete_all_material_expressions(material)
    material.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    # Grown rows tilt the band toward the camera; two-sided keeps any tilt from being culled.
    material.set_editor_property('two_sided', True)
    material.set_editor_property('translucency_pass', enum(u.MaterialTranslucencyPass, 'MTP_AFTER_DOF'))

    color = node(material, u.MaterialExpressionVectorParameter, parameter_name='Color',
                 default_value=u.LinearColor(1, 1, 1, 1), group=GROUP, sort_priority=0)
    assert MAT.connect_material_property(color, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = scalar(material, 'Opacity', 0.9, 1)
    min_pixels = scalar(material, 'MinPixels', 2.0, 2)
    min_scale = scalar(material, 'MinOpacityScale', 0.6, 3)

    band = node(material, u.MaterialExpressionTextureCoordinate, coordinate_index=1)
    radial = node(material, u.MaterialExpressionTextureCoordinate, coordinate_index=2)
    position = node(material, u.MaterialExpressionWorldPosition,
                    world_position_shader_offset=enum(u.WorldPositionIncludedOffsets, 'WPT_CAMERA_RELATIVE_NO_OFFSETS'))
    normal = node(material, u.MaterialExpressionVertexNormalWS)
    grow = custom(material, 'RingLineGrow', GROW_HLSL,
                  {'P': position, 'N': normal, 'Band': band, 'Radial': radial,
                   'TanHalfFov': view_property(material, 'MEVP_TAN_HALF_FIELD_OF_VIEW'),
                   'ViewSize': view_property(material, 'MEVP_VIEW_SIZE'), 'MinPixels': min_pixels},
                  u.CustomMaterialOutputType.CMOT_FLOAT4)
    assert MAT.connect_material_property(mask(material, grow, r=True, g=True, b=True), '',
                                         u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    coverage = node(material, u.MaterialExpressionVertexInterpolator)
    assert MAT.connect_material_expressions(mask(material, grow, a=True), '', coverage, '')
    alpha = custom(material, 'RingLineOpacity', OPACITY_HLSL,
                   {'Band': band, 'Coverage': coverage, 'Opacity': opacity, 'MinOpacityScale': min_scale},
                   u.CustomMaterialOutputType.CMOT_FLOAT1)
    assert MAT.connect_material_property(alpha, '', u.MaterialProperty.MP_OPACITY)
    MAT.layout_material_expressions(material)


if LIB.does_asset_exist(PATH):
    material = LIB.load_asset(PATH)
    assert LIB.get_metadata_tag(material, 'Generator') == OWNER, f'{PATH} was not made by this script'
else:
    material = u.AssetToolsHelpers.get_asset_tools().create_asset(NAME, DEST, u.Material, u.MaterialFactoryNew())
build_material(material)
LIB.set_metadata_tag(material, 'Generator', OWNER)
MAT.recompile_material(material)
assert LIB.save_loaded_asset(material, only_if_is_dirty=False)

u.log(f'TRAINING_RING_ASSETS_COMPLETE {NAME}')
