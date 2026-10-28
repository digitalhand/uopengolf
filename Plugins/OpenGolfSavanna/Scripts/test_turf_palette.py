# SPDX-License-Identifier: MIT
import colorsys
import re
import unittest

from turf_palette import (BRIGHT_HLSL, FAMILY_COLOR_HLSL, GRASS_COLOR_HLSL, HUE_CELLS_CM, HUE_HLSL, MACRO_STRENGTH, PALETTE,
                          ROUGH_GRASS, hls, linear)


class PaletteTest(unittest.TestCase):
    def test_every_family_has_three_tones(self):
        self.assertEqual(set(PALETTE), {'Fairway', 'FirstCut', 'Collar', 'Green', 'Tee'})
        for family, tones in PALETTE.items():
            self.assertEqual(set(tones), {'Mid', 'Dry', 'Lush'}, family)

    def test_tones_are_muted_olive_greens(self):
        # Maintained turf in the owner's samples measures about 74 degrees hue and 0.37 saturation. The
        # range's sun renders authored tones about 5 degrees yellower, so mid tones sit a little above that.
        for family, tones in PALETTE.items():
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

    def test_green_varies_less_than_fairway(self):
        def spread(family):
            return hls(PALETTE[family]['Dry'])[1] - hls(PALETTE[family]['Lush'])[1]
        self.assertLess(spread('Green'), spread('Fairway'))

    def test_mowing_heights_order_lightness(self):
        def light(family):
            return hls(PALETTE[family]['Mid'])[1]
        self.assertLess(light('FirstCut'), light('Fairway'))
        self.assertLessEqual(light('Collar'), light('Tee'))

    def test_fairway_and_first_cut_match_the_darker_mottled_sample(self):
        # The 2026-10-27 fairway sample renders about 10% darker than the range, with luma spread 11-14
        # against our 3-7: darker tones and stronger brightness patches.
        self.assertLessEqual(hls(PALETTE['Fairway']['Mid'])[1], .32)
        self.assertLessEqual(hls(PALETTE['FirstCut']['Mid'])[1], .29)
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
