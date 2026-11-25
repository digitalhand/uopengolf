# SPDX-License-Identifier: MIT
"""Where an export manifest imports, and the material instance settings it asks for. Pure Python, so it can be
tested without Unreal.

A manifest without a destination imports into this plugin. A course's own art names its plugin, so the course
pack carries it: {"mount": "/HunterRanch", "content_dir": "Plugins/HunterRanch/Content", "owner": "HunterRanch.v1"}.
"""
import math
import re
from pathlib import Path

SAVANNA = {"mount": "/OpenGolfSavanna", "content_dir": "Plugins/OpenGolfSavanna/Content", "owner": "OpenGolfSavanna.v1"}
FIELDS = ("mount", "content_dir", "owner")


def destination(manifest, project):
    """(mount, content folder, generator owner) for a manifest."""
    given = {key: manifest[key] for key in FIELDS if key in manifest}
    if given and set(given) != set(FIELDS):
        raise ValueError(f"A manifest destination needs all of {', '.join(FIELDS)}")
    chosen = {**SAVANNA, **given}
    match = re.fullmatch(r"/([A-Za-z0-9_]+)", chosen["mount"])
    if not match:
        raise ValueError(f"Not a plugin mount: {chosen['mount']!r}")
    if chosen["content_dir"] != f"Plugins/{match.group(1)}/Content":
        raise ValueError(f"{chosen['mount']} imports into Plugins/{match.group(1)}/Content, not {chosen['content_dir']!r}")
    if not re.fullmatch(r"[A-Za-z0-9_.]+", chosen["owner"] or ""):
        raise ValueError(f"Not a generator owner: {chosen['owner']!r}")
    return chosen["mount"], Path(project) / chosen["content_dir"], chosen["owner"]


def is_savanna(mount):
    return mount == SAVANNA["mount"]


def _value(name, value):
    if isinstance(value, (list, tuple)):
        if len(value) not in (3, 4) or not all(isinstance(v, (int, float)) and math.isfinite(v) and v >= 0 for v in value):
            raise ValueError(f"{name} must be 3 or 4 non-negative numbers, not {value!r}")
        return tuple(float(v) for v in value) + ((1.0,) if len(value) == 3 else ())
    if isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value):
        return float(value)
    raise ValueError(f"{name} must be a number or a colour, not {value!r}")


def instance_settings(defaults, manifest_settings):
    """The defaults with the manifest's settings over them; colours become (r, g, b, a) tuples."""
    return {**defaults, **{name: _value(name, value) for name, value in (manifest_settings or {}).items()}}


def material_for_slot(slot, names):
    """The material a mesh slot takes: its own name, else the longest name it extends with "_" (FBX exports can
    suffix slots). Longest first, so a slot island_tree_02_leaves never takes island_tree_02."""
    if slot in names:
        return slot
    extended = [name for name in names if slot.startswith(name + "_")]
    return max(extended, key=len) if extended else None
