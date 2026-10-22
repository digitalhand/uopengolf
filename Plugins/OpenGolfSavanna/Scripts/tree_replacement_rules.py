# SPDX-License-Identifier: MIT
"""Checks shared by tree migration and collision validation."""
import math


def replacement_scale(current: str, original: str, replacement: str, ratio: float) -> float:
    if not math.isfinite(ratio) or ratio <= 0:
        raise ValueError('Canopy scale ratio must be finite and positive')
    if current == replacement:
        return 1.0
    if current == original:
        return ratio
    raise ValueError(f'Unexpected tree mesh: {current}')


def validate_pairs(tree_keys, collider_keys) -> None:
    trees, colliders = list(tree_keys), list(collider_keys)
    if (not trees or len(set(trees)) != len(trees) or len(set(colliders)) != len(colliders)
            or set(trees) != set(colliders)):
        raise ValueError('Each visible tree must have exactly one matching collider')
