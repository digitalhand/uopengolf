# SPDX-License-Identifier: MIT
import inspect
import math
import random
import re
import unittest

import vegetation_wind as w

MPS_PER_MPH = 0.44704
DT = 0.05
# The jacaranda at scale 1 (SM_Jacaranda bounds), Grass Medium and the Nature Essentials tufts.
TREE_HEIGHT_CM, TREE_CROWN_CM = 1960.0, 1474.0
GRASS_HEIGHT_CM, GRASS_CROWN_CM = 36.0, 66.0
UNDERSTORY_HEIGHT_CM, UNDERSTORY_CROWN_CM = 70.0, 24.0
# The jacaranda as the range places it (about 9 m), the size its profile describes.
TREE_SCALE = w.BROADLEAF_TREE.size_cm / TREE_HEIGHT_CM
# Largest instance scales the bounds are tested at: the trees' layout, the presets' default range and the
# understory layout (vegetation_wind_assignments.py keeps the real ones).
TREE_MAX_SCALE, GRASS_MAX_SCALE, UNDERSTORY_MAX_SCALE = 0.62, 1.3, 1.5
# A world position about 280 m from the origin, like the range's trees.
BASE = (-14250.0, 23880.0, 400.0)


def wind(mps, toward_deg=0.0):
    angle = math.radians(toward_deg)
    return (mps * math.cos(angle), mps * math.sin(angle), mps, 10.0)


def point(x, z, base=BASE, scale=1.0):
    """A point x cm out from the axis and z cm up (at scale 1) on an upright plant placed at scale: arm, up,
    world position."""
    arm = (x * scale, 0.0, z * scale)
    return arm, (0.0, 0.0, scale), (base[0] + arm[0], base[1], base[2] + arm[2])


def series(params, at, mps, toward_deg=0.0, seconds=60.0):
    return [w.offset(params, *at, time=i * DT, wind=wind(mps, toward_deg)) for i in range(int(seconds / DT))]


def components(offsets, toward_deg=0.0):
    """Along-wind, crosswind and vertical parts of each offset."""
    angle = math.radians(toward_deg)
    d, s = (math.cos(angle), math.sin(angle)), (-math.sin(angle), math.cos(angle))
    return ([o[0] * d[0] + o[1] * d[1] for o in offsets], [o[0] * s[0] + o[1] * s[1] for o in offsets], [o[2] for o in offsets])


def mean(values):
    return sum(values) / len(values)


def spread(values):
    """Standard deviation: how much something moves about where it leans."""
    centre = mean(values)
    return math.sqrt(sum((v - centre) ** 2 for v in values) / len(values))


def tempo_hz(values):
    """Typical frequency of a motion (Rice): the RMS of its rate over 2 pi times its spread."""
    rates = [(b - a) / DT for a, b in zip(values, values[1:])]
    return math.sqrt(mean([r * r for r in rates])) / (2 * math.pi * spread(values))


def envelope(params, at, mps, seconds=30.0):
    return max(math.dist(o, (0, 0, 0)) for o in series(params, at, mps, seconds=seconds))


def lean_deg(offset, height_cm):
    return math.degrees(math.asin(min(math.hypot(offset[0], offset[1]) / height_cm, 1.0)))


class BeaufortTest(unittest.TestCase):
    def test_reads_the_cpp_scale(self):
        self.assertEqual(len(w.BEAUFORT_MIN_MPS), 13)
        self.assertEqual(w.BEAUFORT_MIN_MPS[3], 3.4)
        self.assertEqual(w.BEAUFORT_MIN_MPS[4], 5.5)
        self.assertEqual(w.BEAUFORT_NAMES[5], "Fresh breeze")
        self.assertEqual(list(w.BEAUFORT_MIN_MPS), sorted(w.BEAUFORT_MIN_MPS))


class Plants(unittest.TestCase):
    def setUp(self):
        self.tree = w.parameters(w.BROADLEAF_TREE, TREE_HEIGHT_CM, TREE_CROWN_CM)
        # Trunk and branch materials: the leaf switch off (vegetation_wind_assets.add_vegetation_wind).
        self.wood = dict(self.tree, WindFlutter=(0.0, 0.0, 1.0, 0.0))
        self.bend_only = dict(self.wood, WindSway=(0.0,) + self.wood["WindSway"][1:])
        self.limbs_only = dict(self.wood, WindBend=(0.0,) + self.wood["WindBend"][1:])
        self.grass = w.parameters(w.TALL_GRASS, GRASS_HEIGHT_CM, GRASS_CROWN_CM)
        self.understory = w.parameters(w.UNDERSTORY_GRASS, UNDERSTORY_HEIGHT_CM, UNDERSTORY_CROWN_CM)
        self.crown_top, self.crown_edge = point(0, 1900, scale=TREE_SCALE), point(1000, 1400, scale=TREE_SCALE)
        self.trunk, self.grass_tip, self.understory_tip = point(0, 800, scale=TREE_SCALE), point(0, 34), point(0, 68)

    def at_force(self, force):
        """A 10 m wind inside a Beaufort force."""
        return w.force_mps(force) + 0.5


class CalmAndBeaufortTest(Plants):
    """The Beaufort land descriptions are the calibration."""

    def test_calm_is_exactly_still(self):
        rng = random.Random(3)
        for params in (self.tree, self.grass, self.understory):
            for _ in range(200):
                at = point(rng.uniform(-1400, 1400), rng.uniform(0, 1960))
                self.assertEqual(w.offset(params, *at, time=rng.uniform(0, 1000), wind=wind(0.0)), (0.0, 0.0, 0.0))

    def test_light_air_barely_moves_anything(self):
        # Force 1: smoke drifts, vanes are still.
        mps = 1.2
        for params, at in ((self.tree, self.crown_edge), (self.wood, self.crown_top)):
            self.assertLess(envelope(params, at, mps), 1.0)
        for params, at, height in ((self.grass, self.grass_tip, 34.0), (self.understory, self.understory_tip, 68.0)):
            # A 3 degree lean of a 34 cm blade moves its tip under 2 cm: still, from the tee.
            self.assertLess(max(lean_deg(o, height) for o in series(params, at, mps, seconds=30)), 3.0)

    def test_force_2_rustles_leaves_not_trunks(self):
        mps = self.at_force(2)
        leaves = components(series(self.tree, self.crown_edge, mps))[0]
        trunk = components(series(self.wood, self.trunk, mps))[0]
        self.assertGreater(spread(leaves), 0.2)
        self.assertLess(spread(trunk), 0.1)

    def test_small_branches_move_from_force_4(self):
        def limb_motion(force):
            return spread(components(series(self.limbs_only, self.crown_edge, self.at_force(force)))[0])
        self.assertLess(limb_motion(2), 0.3)
        self.assertGreater(limb_motion(4), 0.8)

    def test_trees_sway_at_force_5_and_move_whole_at_force_7(self):
        crown = components(series(self.wood, self.crown_top, self.at_force(5)))[0]
        self.assertGreater(spread(crown), 3.0, "small trees sway")
        self.assertGreater(mean(crown), 10.0)
        whole = self.at_force(7)
        self.assertGreater(mean(components(series(self.wood, self.crown_top, whole))[0]), 25.0)
        self.assertGreater(mean(components(series(self.wood, self.trunk, whole))[0]), 5.0, "the whole tree leans")

    def test_grass_leans_from_force_3_without_lying_flat(self):
        def mean_lean(mps):
            return mean([lean_deg(o, 34.0) for o in series(self.grass, self.grass_tip, mps)])
        self.assertGreater(mean_lean(self.at_force(3)), 10.0)
        strong = mean_lean(30 * MPS_PER_MPH)
        self.assertGreater(strong, 30.0)
        self.assertLess(strong, 60.0)


class DownwindTest(Plants):
    """Wind pushes plants over; it does not swing them to and fro."""

    def cases(self):
        return (("crown top", self.wood, self.crown_top), ("crown edge", self.wood, self.crown_edge),
                ("leaves", self.tree, self.crown_edge), ("tall grass", self.grass, self.grass_tip),
                ("understory", self.understory, self.understory_tip))

    def test_never_moves_upwind(self):
        for force in range(3, 9):
            for toward in (0.0, 135.0):
                for name, params, at in self.cases():
                    along = components(series(params, at, self.at_force(force), toward, seconds=30), toward)[0]
                    self.assertGreaterEqual(min(along), 0.0, f"{name} at force {force}, toward {toward}")

    def test_motion_is_mostly_along_the_wind(self):
        for mph in (10, 20, 30):
            for name, params, at in self.cases():
                along, side, vertical = components(series(params, at, mph * MPS_PER_MPH))
                self.assertGreater(mean(along), 3 * spread(side), f"{name} at {mph} mph")
                self.assertGreater(spread(along), spread(vertical), f"{name} at {mph} mph")

    def test_lean_grows_with_the_wind(self):
        for name, params, at in self.cases():
            previous = 0.0
            for mph in (5, 10, 15, 20, 25, 30, 40):
                current = mean(components(series(params, at, mph * MPS_PER_MPH, seconds=30))[0])
                self.assertGreater(current, previous, f"{name} at {mph} mph")
                previous = current

    def test_surges_shrink_as_grass_lies_over(self):
        def surge(mph):
            along = components(series(self.grass, self.grass_tip, mph * MPS_PER_MPH))[0]
            return spread(along) / mean(along)
        self.assertLess(surge(40), surge(10))

    def test_weight_sets_the_tempo(self):
        # Heavy trunks follow only the long gusts; limbs and light grass follow faster ones.
        mps = 25 * MPS_PER_MPH
        def tempo(params, at):
            return tempo_hz(components(series(params, at, mps))[0])
        trunk = tempo(self.bend_only, self.crown_top)
        limbs = tempo(self.limbs_only, self.crown_edge)
        grass, understory = tempo(self.grass, self.grass_tip), tempo(self.understory, self.understory_tip)
        self.assertLess(trunk, limbs)
        self.assertLess(trunk, understory)
        self.assertLess(understory, grass, "seed heads make the understory grass slower")

    def test_gusts_roll_downwind_across_grass(self):
        mps, gap_cm = 20 * MPS_PER_MPH, 500.0
        upwind = components(series(self.grass, self.grass_tip, mps))[0]
        downwind = components(series(self.grass, point(0, 34, (BASE[0] + gap_cm, BASE[1], BASE[2])), mps))[0]
        def correlation(lag):
            a, b = upwind[:len(upwind) - lag], downwind[lag:]
            ma, mb = mean(a), mean(b)
            return sum((x - ma) * (y - mb) for x, y in zip(a, b))
        best = max(range(0, 60), key=correlation)
        self.assertAlmostEqual(best * DT, gap_cm / 100 / mps, delta=2 * DT)


class ShapeTest(Plants):
    def test_tree_moves_as_one_despite_rounding(self):
        # The base is rebuilt at each vertex in float32; 280 m out that is good to about 0.005 cm.
        for x, z in ((1000, 1400), (-600, 1700), (0, 1900)):
            arm, up, position = point(x, z)
            nudged = (position[0] + 0.005, position[1] - 0.004, position[2])
            for i in range(0, 400, 7):
                exact = w.offset(self.wood, arm, up, position, time=i * DT, wind=wind(25 * MPS_PER_MPH))
                rounded = w.offset(self.wood, arm, up, nudged, time=i * DT, wind=wind(25 * MPS_PER_MPH))
                self.assertLess(math.dist(exact, rounded), 0.05)

    def test_offsets_stay_inside_the_material_bounds(self):
        rng = random.Random(11)
        for params, height, crown, max_scale in ((self.tree, TREE_HEIGHT_CM, TREE_CROWN_CM, TREE_MAX_SCALE),
                                                 (self.grass, GRASS_HEIGHT_CM, GRASS_CROWN_CM, GRASS_MAX_SCALE),
                                                 (self.understory, UNDERSTORY_HEIGHT_CM, UNDERSTORY_CROWN_CM, UNDERSTORY_MAX_SCALE)):
            bound = w.max_displacement_cm(params, max_scale)
            for _ in range(1500):
                at = point(rng.uniform(-1.2, 1.2) * crown, rng.uniform(0, 1.1) * height, scale=rng.uniform(0.3, max_scale))
                result = w.offset(params, *at, time=rng.uniform(0, 600), wind=wind(rng.uniform(0, w.DESIGN_MPS), rng.uniform(0, 360)))
                self.assertLessEqual(max(abs(v) for v in result), bound)

    def test_leaning_keeps_length(self):
        # A blade tip leaning by its full bend drops, rather than stretching away from its base.
        arm, up, position = self.grass_tip
        for i in range(200):
            offset = w.offset(self.grass, arm, up, position, time=i * 0.1, wind=wind(w.DESIGN_MPS))
            moved = tuple(a + o for a, o in zip(arm, offset))
            self.assertLess(math.dist(moved, (0, 0, 0)), 34 + w.TALL_GRASS.leaves.flutter_cm * 1.2)

    def test_grass_tiles_meet_without_seams(self):
        # The same blade position in two neighbouring tiles moves the same: grass varies by world position.
        position = (150.0, 40.0, 28.0)
        left = w.offset(self.grass, (50.0, 40.0, 28.0), (0, 0, 1), position, time=3.0, wind=wind(8.0))
        right = w.offset(self.grass, (-50.0, 40.0, 28.0), (0, 0, 1), position, time=3.0, wind=wind(8.0))
        for a, b in zip(left, right):
            self.assertAlmostEqual(a, b, places=9)

    def test_tree_parts_stay_joined(self):
        # Trunk and leaf materials at one point differ only by the leaves' own motion.
        leaves = w.BROADLEAF_TREE.leaves
        for i in range(100):
            leaf = w.offset(self.tree, *self.crown_edge, time=i * 0.1, wind=wind(12.0))
            wood = w.offset(self.wood, *self.crown_edge, time=i * 0.1, wind=wind(12.0))
            self.assertLessEqual(math.dist(leaf, wood), (leaves.flutter_cm + leaves.stream_cm) * 1.1)

    def test_instances_lean_the_same_at_any_scale(self):
        # A plant placed larger is the same plant, bigger: it leans the same angle, so its tip moves farther.
        mps = 20 * MPS_PER_MPH
        for name, params, z in (("tall grass", self.grass, 34.0), ("understory", self.understory, 68.0)):
            def mean_lean(scale):
                return mean([lean_deg(o, z * scale) for o in series(params, point(0, z, scale=scale), mps)])
            for scale in (0.7, 1.5):
                self.assertAlmostEqual(mean_lean(scale), mean_lean(1.0), delta=0.03 * mean_lean(1.0), msg=f"{name} at {scale}")

        # Only the gusts differ, being a little weaker higher up.
        def crown(scale):
            return mean(components(series(self.bend_only, point(0, 1900, scale=scale), mps))[0])
        self.assertAlmostEqual(crown(1.0), 2 * crown(0.5), delta=0.1 * crown(1.0), msg="a tree twice the size bends twice as far")

    def test_parameters_carry_physical_values(self):
        # The tree's centimetres are for a 9 m tree; the material gets them for the 19.6 m mesh at scale 1.
        size = TREE_HEIGHT_CM / 900.0
        for name, expected in (("WindBend", (150.0 * size, 25.0, 0.35, 0.15)), ("WindSway", (40.0 * size, 12.0, 1.2, 0.15)),
                               ("WindFlutter", (0.8 * size, 4.0, 2.5, 3.0 * size))):
            for value, wanted in zip(self.tree[name], expected):
                self.assertAlmostEqual(value, wanted, msg=name)
        self.assertEqual(self.tree["WindShape"], (TREE_HEIGHT_CM, TREE_CROWN_CM, 2.0, 0.0))
        self.assertEqual(self.tree["WindDetail"], (2.0, 0.0, 0.0, 0.0))
        self.assertAlmostEqual(self.grass["WindBend"][0], GRASS_HEIGHT_CM * math.sin(math.radians(60.0)))
        self.assertEqual(self.grass["WindSway"][0], 0.0)
        self.assertEqual(self.grass["WindDetail"][1], 1.0)
        self.assertEqual(set(w.PROFILES), {"TallGrass", "UnderstoryGrass", "BroadleafTree"})


class HlslTest(unittest.TestCase):
    def test_hlsl_uses_every_input_and_returns_the_offset(self):
        for name in ("A", "Up", "P", "T", "Wind", "Air", "Bend", "Sway", "Flutter", "Shape", "Detail"):
            self.assertRegex(w.WIND_HLSL, rf"\b{name}\b")
        self.assertTrue(w.WIND_HLSL.rstrip().endswith("return O;"))
        self.assertNotRegex(w.WIND_HLSL, r"\bfloat\s+cross\b", "cross is an HLSL intrinsic")
        self.assertNotRegex(w.WIND_HLSL, r"frac\s*\(\s*sin", "a hash of a rebuilt position differs per vertex")
        self.assertNotRegex(w.WIND_HLSL, r"\{\{|\}\}|\{count\}|\{gusts\}", "the template was filled in")

    def test_hlsl_scales_deflection_with_the_instance(self):
        # The Python mirror's behaviour is tested above; this ties the HLSL to it.
        for term in (r"Bend\.x \* scale", r"Sway\.x \* scale", r"Flutter\.x \* scale", r"Flutter\.w \* scale"):
            self.assertRegex(w.WIND_HLSL, term)

    def test_gust_table_is_filled_in(self):
        for wavelength, amplitude, phase, side_phase in w.GUSTS:
            self.assertIn(f"float4({wavelength:g}, {amplitude:g}, {phase:g}, {side_phase:g})", w.WIND_HLSL)
        self.assertAlmostEqual(sum(gust[1] ** 2 for gust in w.GUSTS), 1.0, delta=0.002)
        self.assertEqual([gust[0] for gust in w.GUSTS], sorted((gust[0] for gust in w.GUSTS), reverse=True))

    def test_mirror_uses_the_same_constants(self):
        # Every number in the HLSL appears in the Python mirror, so editing one side shows up here.
        numbers = lambda text: set(re.findall(r"(?<![\w.])\d+\.?\d*(?:e-?\d+)?", text))
        mirror = "".join(inspect.getsource(f) for f in (w.offset, w._gusts, w._share))
        self.assertEqual(numbers(w._HLSL) - numbers(mirror), set())


if __name__ == "__main__":
    unittest.main()
