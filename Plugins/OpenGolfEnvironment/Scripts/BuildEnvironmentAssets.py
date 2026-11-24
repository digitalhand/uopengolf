# SPDX-License-Identifier: MIT
"""Create the plugin's original procedural cloud material and default profile in Unreal."""
from pathlib import Path
import shutil
import sys
from tempfile import mkdtemp

import unreal

ROOT = '/OpenGolfEnvironment'
lib = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary

def build(*, refresh_clouds=False):
    path = ROOT + '/Materials/M_Clouds'
    material = assets.load_asset(path)
    new_material = not material
    if new_material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Clouds', ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    if new_material or refresh_clouds:
        volumes = {}
        for name, asset in [('Shape', 'T_VolumeNoiseShape128'), ('Erosion', 'T_VolumeNoiseErosion32')]:
            texture = assets.load_asset('/Engine/EngineSky/VolumetricClouds/' + asset)
            if not isinstance(texture, unreal.VolumeTexture):
                raise RuntimeError(f'Missing engine cloud volume {asset}.')
            volumes[name] = texture
        if not new_material:
            project = Path(unreal.Paths.project_dir())
            backup_root = project / 'Saved/AssetBackups/Environment'
            backup_root.mkdir(parents=True, exist_ok=True)
            backup = Path(mkdtemp(prefix='CloudMaterial-', dir=backup_root))
            source = project / 'Plugins/OpenGolfEnvironment/Content/Materials/M_Clouds.uasset'
            shutil.copy2(source, backup / source.name)
            lib.delete_all_material_expressions(material)
        material.set_editor_property('material_domain', unreal.MaterialDomain.MD_VOLUME)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
        def node(kind):
            return lib.create_material_expression(material, kind)
        position = node(unreal.MaterialExpressionWorldPosition)
        layer = node(unreal.MaterialExpressionCloudSampleAttribute)
        coverage = node(unreal.MaterialExpressionScalarParameter)
        coverage.set_editor_property('parameter_name', 'Coverage')
        coverage.set_editor_property('default_value', .35)
        offset = node(unreal.MaterialExpressionScalarParameter)
        offset.set_editor_property('parameter_name', 'CloudOffset')
        density = node(unreal.MaterialExpressionCustom)
        density.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        inputs = []
        for name in ['Position', 'Coverage', 'Offset', 'LayerHeight', 'LayerAltitudeCm', *volumes]:
            item = unreal.CustomInput()
            item.set_editor_property('input_name', name)
            inputs.append(item)
        density.set_editor_property('inputs', inputs)
        density.set_editor_property('code', Path(__file__).with_name('cumulus_density.hlsl').read_text())
        for name, texture in volumes.items():
            source = node(unreal.MaterialExpressionTextureObject)
            source.set_editor_property('texture', texture)
            source.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            if not lib.connect_material_expressions(source, '', density, name):
                raise RuntimeError(f'Could not connect the {name} volume.')
        lib.connect_material_expressions(position, '', density, 'Position')
        lib.connect_material_expressions(coverage, '', density, 'Coverage')
        lib.connect_material_expressions(offset, '', density, 'Offset')
        if not lib.connect_material_expressions(layer, 'NormAltitudeInLayer', density, 'LayerHeight'):
            raise RuntimeError('Could not connect the cloud layer height.')
        if not lib.connect_material_expressions(layer, 'AltitudeInLayer', density, 'LayerAltitudeCm'):
            raise RuntimeError('Could not connect the cloud altitude.')
        lib.connect_material_property(density, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        color = node(unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant', unreal.LinearColor(.95, .95, .95, 1))
        lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        scattering = node(unreal.MaterialExpressionVolumetricAdvancedMaterialOutput)
        scattering.set_editor_property('gray_scale_material', True)
        scattering.set_editor_property('multi_scattering_approximation_octave_count', 1)
        scattering.set_editor_property('const_phase_g', -.26)
        scattering.set_editor_property('const_phase_g2', .15)
        scattering.set_editor_property('const_phase_blend', .30)
        scattering.set_editor_property('const_multi_scattering_contribution', .70)
        scattering.set_editor_property('const_multi_scattering_occlusion', .38)
        lib.layout_material_expressions(material)
    usage = unreal.MaterialUsage.MATUSAGE_VOLUMETRIC_CLOUD
    usage_changed = not lib.has_material_usage(material, usage)
    if usage_changed:
        lib.set_material_usage(material, usage)
        if not lib.has_material_usage(material, usage):
            raise RuntimeError('Could not enable volumetric cloud usage.')
    if new_material or refresh_clouds or usage_changed:
        lib.recompile_material(material)
        if not assets.save_loaded_asset(material):
            raise RuntimeError('Could not save the cloud material.')
    profile_path = ROOT + '/Profiles/DA_DefaultDaylight'
    profile = assets.load_asset(profile_path)
    if not profile:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.OpenGolfEnvironmentProfile)
        profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_DefaultDaylight', ROOT + '/Profiles', unreal.OpenGolfEnvironmentProfile, factory)
        profile.set_editor_property('cloud_material', material)
        assets.save_loaded_asset(profile)
    unreal.log('OPEN_GOLF_ENVIRONMENT_ASSETS_COMPLETE')
    return profile

if __name__ == '__main__':
    build(refresh_clouds='--refresh-clouds' in sys.argv)
