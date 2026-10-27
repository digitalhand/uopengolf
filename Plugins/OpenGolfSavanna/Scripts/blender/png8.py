# SPDX-License-Identifier: MIT
"""Write 8-bit PNG texture copies from Blender.

Poly Haven ships 16-bit PNGs, and Image.save() keeps 16 bits. Unreal imports those as 16-bit
sources, and colour maps came out washed out (pale grass and jacaranda leaves), so every export
writes 8 bits per channel instead. Colour maps keep their sRGB encoding and data maps (normal,
roughness, AO, opacity) are written raw; both match the source within one 8-bit step.

Load each texture with load(), which sets its colour space before anything else touches it:
assigning Image.colorspace_settings later, even to the same value, reloads the file and silently
drops any scale() or pixel edits (such as premultiply) made since.
"""
import bpy
import numpy


def colorspace(color):
    return "sRGB" if color else "Non-Color"


def load(path, color):
    """Load a texture for export, with its colour space set before it is scaled or edited."""
    image = bpy.data.images.load(str(path), check_existing=False)
    image.colorspace_settings.name = colorspace(color)
    return image


def premultiply(image):
    """Multiply colour by the image's own alpha.

    Poly Haven's grass PNGs store un-premultiplied colour, which blows out to near white at the
    soft edges of thin blades; their Blender materials use the premultiplied JPG instead. This
    turns the PNG into that JPG, at full quality.
    """
    if image.channels != 4:
        return
    pixels = numpy.empty(len(image.pixels), dtype=numpy.float32)
    image.pixels.foreach_get(pixels)
    rgba = pixels.reshape(-1, 4)
    rgba[:, :3] *= rgba[:, 3:4]
    image.pixels.foreach_set(pixels)
    image.update()


def save(image, target, color):
    """Save image, loaded with load() and then scaled or edited, to target as an 8-bit PNG."""
    if image.colorspace_settings.name != colorspace(color):
        # Setting it here would reload the file and lose the scale and edits; see the module notes.
        raise RuntimeError(f"{image.name}: load it with png8.load(path, {color}) before scaling or editing it")
    scene = bpy.context.scene
    settings = scene.render.image_settings
    settings.file_format = "PNG"
    settings.color_depth = "8"
    settings.color_mode = "RGBA" if image.channels == 4 else "RGB"
    settings.color_management = "OVERRIDE"
    view = settings.view_settings
    view.view_transform = "Standard" if color else "Raw"
    view.look = "None"
    view.exposure = 0.0
    view.gamma = 1.0
    image.save_render(str(target), scene=scene)


def mean_srgb(path):
    """Mean colour of a saved 8-bit PNG as an sRGB hex string, over its opaque pixels when it has alpha.

    Byte images keep their stored sRGB values in Image.pixels, so the result matches what an
    artist would pick from the file. The ground under grass uses it to match the blades.
    """
    image = bpy.data.images.load(str(path), check_existing=False)
    try:
        pixels = numpy.empty(len(image.pixels), dtype=numpy.float32)
        image.pixels.foreach_get(pixels)
        rgba = pixels.reshape(-1, image.channels)
        if image.channels == 4:
            rgba = rgba[rgba[:, 3] >= 0.9]
        if not len(rgba):
            raise RuntimeError(f"{path}: no opaque pixels to average")
        return "".join(f"{round(float(v) * 255):02X}" for v in rgba[:, :3].mean(axis=0))
    finally:
        bpy.data.images.remove(image)
