# SPDX-License-Identifier: MIT
import colorsys
import re
import unittest

from turf_palette import (BRIGHT_HLSL, FAMILY_COLOR_HLSL, GRASS_COLOR_HLSL, HUE_AMOUNT, HUE_CELLS_CM, HUE_HLSL, MACRO_STRENGTH,
                          PALETTE, ROUGH_BLADE, ROUGH_GRASS, ROUGH_UNDERLAY, hls, linear, rough_blade_tints)

MAINTAINED = ('Fairway', 'FirstCut', 'Collar', 'Green', 'Tee')


def luma(rgb):
    return .2126 * rgb[0] + .7152 * rgb[1] + .0722 * rgb[2]


class PaletteTest(unittest.TestCase):
    def test_every_family_has_three_tones(self):
        self.assertEqual(set(PALETTE), {'Fairway', 'FirstCut', 'Collar', 'Green', 'Tee', 'Rough'})
        for family, tones in PALETTE.items():
            self.assertEqual(set(tones), {'Mid', 'Dry', 'Lush'}, family)

    def test_tones_are_muted_olive_greens(self):
        # The owner's reference screenshots (2026-11-20) render maintained turf at 72-75 degrees hue and 0.35-0.41
        # saturation. The range's sun renders authored tones a few degrees yellower and more saturated, so the
        # authored tones sit a little above that in hue and below it in saturation.
        for family in MAINTAINED:
            tones = PALETTE[family]
            for tone, color in tones.items():
                hue, _, saturation = hls(color)
                self.assertTrue(60 <= hue <= 92, f'{family} {tone} hue {hue:.0f}')
                self.assertTrue(.25 <= saturation <= .45, f'{family} {tone} saturation {saturation:.2f}')
            self.assertTrue(72 <= hls(tones['Mid'])[0] <= 85, family)

    def test_dry_is_lighter_and_yellower_lush_darker_and_bluer(self):
        for family, tones in PALETTE.items():
            mid, dry, lush = (hls(tones[t]) for t in ('Mid', 'Dry', 'Lush'))
            self.assertLess(dry[0], mid[0], family)
            self.assertGreater(dry[1], mid[1], family)
            self.assertGreater(lush[0], mid[0], family)
            self.assertLess(lush[1], mid[1], family)

    def test_maintained_palettes_match_the_reference_calibration(self):
        # Calibrated against Play captures and the owner's reference screenshots (2026-11-20). A change here moves
        # every cut-turf surface on the range; recalibrate against captures first.
        self.assertEqual({f: PALETTE[f] for f in MAINTAINED}, {
            'Fairway': {'Mid': '627240', 'Dry': '727B40', 'Lush': '4D6336'},
            'FirstCut': {'Mid': '5F6F37', 'Dry': '717837', 'Lush': '4D612E'},
            'Collar': {'Mid': '5F6F37', 'Dry': '717837', 'Lush': '4D612E'},
            'Green': {'Mid': '607140', 'Dry': '69753F', 'Lush': '556A39'},
            'Tee': {'Mid': '667843', 'Dry': '707C42', 'Lush': '576E3A'}})
        self.assertEqual({f: HUE_AMOUNT[f] for f in MAINTAINED},
                         {'Fairway': .22, 'FirstCut': .25, 'Collar': .25, 'Green': .12, 'Tee': .15})
        self.assertEqual({f: MACRO_STRENGTH[f] for f in MAINTAINED},
                         {'Fairway': .20, 'FirstCut': .20, 'Collar': .20, 'Green': .035, 'Tee': .05})

    def test_green_fringe_takes_the_former_first_cut_colour(self):
        # The owner (2026-11-23) asked for the fringe around the green to look like the fairway's first cut, which was
        # then removed. The collar keeps its own blades and texture grain; only its colour follows the first cut.
        self.assertEqual(PALETTE['Collar'], PALETTE['FirstCut'])
        self.assertEqual(HUE_AMOUNT['Collar'], HUE_AMOUNT['FirstCut'])
        self.assertEqual(MACRO_STRENGTH['Collar'], MACRO_STRENGTH['FirstCut'])

    def test_every_family_has_patch_settings(self):
        self.assertEqual(set(HUE_AMOUNT), set(PALETTE))
        self.assertEqual(set(MACRO_STRENGTH), set(PALETTE))

    def test_rough_is_dark_olive_like_the_reference(self):
        # The reference rough renders at about 73 degrees hue, 0.15 lightness and 0.53 saturation, far darker than
        # its first cut, with near-black gaps between olive blades.
        hue, light, saturation = hls(PALETTE['Rough']['Mid'])
        first_cut = hls(PALETTE['FirstCut']['Mid'])
        self.assertTrue(74 <= hue <= 86, f'hue {hue:.0f}')
        self.assertTrue(.30 <= saturation <= .45, f'saturation {saturation:.2f}')
        # Authored tones: the tone curve renders dark tones darker still, so the rendered gap (about 0.2) is larger.
        self.assertLessEqual(light, first_cut[1] - .08)
        tip, thatch = hls(ROUGH_BLADE['Tip']), hls(ROUGH_BLADE['Thatch'])
        self.assertTrue(60 <= tip[0] <= 80, f'tip hue {tip[0]:.0f}')
        self.assertGreater(tip[1], light)
        self.assertLess(thatch[1], hls(PALETTE['Rough']['Lush'])[1])
        self.assertTrue(0 < ROUGH_BLADE['BaseEnd'] < ROUGH_BLADE['TipStart'] < 1)
        self.assertTrue(0 < ROUGH_BLADE['TipAmount'] <= 1)

    def test_rough_tips_change_hue_not_brightness(self):
        # From above a blade shows mostly its tip; a brighter tip would make the blades lighter than the ground under them.
        thatch, tip = rough_blade_tints()
        mid = linear(PALETTE['Rough']['Mid'])
        self.assertAlmostEqual(luma([t * m for t, m in zip(tip, mid)]), luma(mid), places=6)
        self.assertGreater(tip[0], tip[2])
        self.assertTrue(all(0 < v < 1 for v in thatch))

    def test_rough_underlay_follows_the_rough_palette(self):
        self.assertEqual(ROUGH_UNDERLAY['RoughLow'], linear(PALETTE['Rough']['Lush']))
        self.assertEqual(ROUGH_UNDERLAY['RoughHigh'], linear(PALETTE['Rough']['Dry']))

    def test_green_varies_less_than_fairway(self):
        def spread(family):
            return hls(PALETTE[family]['Dry'])[1] - hls(PALETTE[family]['Lush'])[1]
        self.assertLess(spread('Green'), spread('Fairway'))

    def test_mowing_heights_order_lightness(self):
        def light(family):
            return hls(PALETTE[family]['Mid'])[1]
        self.assertLess(light('FirstCut'), light('Fairway'))
        self.assertLessEqual(light('Rough'), light('FirstCut') + .01)
        self.assertLessEqual(light('Collar'), light('Tee'))

    def test_tones_follow_the_reference_screenshots(self):
        # In the owner's reference screenshots (2026-11-20) the first cut is about as light as the fairway and the
        # green a little lighter; mottled brightness patches stay strong on the fairway and first cut.
        def light(family):
            return hls(PALETTE[family]['Mid'])[1]
        self.assertTrue(.34 <= light('Fairway') <= .40, f"fairway {light('Fairway'):.3f}")
        self.assertTrue(light('Fairway') - .03 <= light('FirstCut') <= light('Fairway'))
        # The green is a flat, bladeless surface and renders lighter than the fairway at the same authored tone, so
        # its calibrated tone sits close to the fairway's.
        self.assertLessEqual(abs(light('Green') - light('Fairway')), .05)
        for family in ('Fairway', 'FirstCut'):
            self.assertGreaterEqual(MACRO_STRENGTH[family], .18, family)
        self.assertLess(MACRO_STRENGTH['Green'], MACRO_STRENGTH['Fairway'])

    def test_rough_grass_is_muted_olive(self):
        self.assertLessEqual(ROUGH_GRASS['Saturation'], 1.0)
        hue, _, _ = hls(ROUGH_GRASS['Tint'])
        self.assertTrue(60 <= hue <= 90, f'tint hue {hue:.0f}')
        self.assertTrue(0 < ROUGH_GRASS['DryJitter'] <= 1)

    def test_linear_conversion(self):
        self.assertEqual(linear('000000'), (0.0, 0.0, 0.0))
        self.assertAlmostEqual(linear('FFFFFF')[1], 1.0)
        self.assertAlmostEqual(linear('808080')[0], .2158605, places=5)
        r, g, b = (int('6B7C39'[i:i + 2], 16) / 255 for i in (0, 2, 4))
        self.assertAlmostEqual(hls('6B7C39')[0], colorsys.rgb_to_hls(r, g, b)[0] * 360)


class ShaderTextTest(unittest.TestCase):
    def test_rough_grass_colour_has_no_per_instance_term(self):
        # A per-instance value is constant over each tiled square of tufts, so it shows the tile grid.
        pins = set(re.findall(r'\b[A-Z][A-Za-z]*\b', GRASS_COLOR_HLSL))
        self.assertIn('Noise', pins)
        self.assertNotIn('Rnd', pins)

    def test_family_colour_mixes_dry_and_lush_tones(self):
        for pin in ('Dry', 'Lush', 'Hue', 'Bright', 'HueAmount'):
            self.assertIn(pin, FAMILY_COLOR_HLSL)

    def test_patch_weights_sum_to_one(self):
        # Weights summing to one keep each patch weight in 0..1 around a 0.5 mean.
        self.assertEqual(len(HUE_CELLS_CM), 3)
        for code in (HUE_HLSL, BRIGHT_HLSL):
            weights = [float(w) for w in re.findall(r'(\d\.\d+)\*', code)]
            self.assertAlmostEqual(sum(weights), 1.0, msg=code)


if __name__ == '__main__':
    unittest.main()
