# SPDX-License-Identifier: MIT
import math
import unittest
from PIL import Image, ImageDraw
from turf_rules import SURFACES, blades, mesh_data


class TurfRulesTest(unittest.TestCase):
    def test_roots_stay_in_the_tile_and_tips_overlap_without_flattening(self):
        for family, spec in SURFACES.items():
            size=(.0625,.0625)
            overhanging=0
            for ribbon in blades(family,size,1731):
                self.assertEqual([v[2] for v in ribbon[:2]],[0,0])
                root=[(ribbon[0][i]+ribbon[1][i])/2 for i in range(2)]
                self.assertTrue(all(abs(p)<=s/2 for p,s in zip(root,size)))
                self.assertTrue(.7*spec.height<=ribbon[-1][2]<=1.3*spec.height)
                for x,y,z in ribbon:
                    self.assertTrue(all(math.isfinite(v) for v in (x,y,z)))
                    self.assertLessEqual(abs(x),size[0]/2+spec.height*1.3+spec.width)
                    self.assertLessEqual(abs(y),size[1]/2+spec.height*1.3+spec.width)
                overhanging+=any(abs(v[i])>size[i]/2 for v in ribbon[2:] for i in range(2))
            self.assertGreater(overhanging,0)

    def test_variants_are_repeatable_and_distinct(self):
        first=list(blades('Fairway',(.0625,.0625),1731))
        self.assertEqual(first,list(blades('Fairway',(.0625,.0625),1731)))
        self.assertNotEqual(first,list(blades('Fairway',(.0625,.0625),1732)))

    def test_cut_turf_has_full_projected_coverage(self):
        for family,minimum in (('Fairway',.65),('FirstCut',.80),('Collar',.75)):
            for seed in (1731,1732,1733,1734):
                with self.subTest(family=family,seed=seed):
                    vertices,faces,uvs,colors=mesh_data(family,(.125,.125),seed)
                    image=Image.new('1',(512,512))
                    draw=ImageDraw.Draw(image)
                    for face in faces:
                        draw.polygon([((vertices[i][0]/.125+.5)*512,(vertices[i][1]/.125+.5)*512) for i in face],fill=1)
                    coverage=image.convert('L').histogram()[255]/512**2
                    self.assertGreaterEqual(coverage,minimum)
                    self.assertGreater(max(c[1] for c in colors)-min(c[1] for c in colors),.7)

    def test_triangle_budget_keeps_the_turf_full_but_light(self):
        # Blades this short read as texture beyond a few metres; geometry is spent only where it shows.
        for family,(fewest,most) in (('Fairway',(25000,90000)),('Collar',(25000,90000)),('FirstCut',(20000,140000))):
            with self.subTest(family=family):
                vertices,faces,uvs,colors=mesh_data(family,(.25,.25),1731)
                ribbons=list(blades(family,(.25,.25),1731))
                self.assertGreaterEqual(len(ribbons)/.25**2,fewest)
                self.assertLessEqual(2*len(faces)/.25**2,most)

    def test_blades_are_wide_enough_to_resolve(self):
        for family in ('Fairway','Collar','FirstCut'):
            with self.subTest(family=family):
                widths=[max(math.dist(r[i][:2],r[i+1][:2]) for i in range(0,len(r),2)) for r in blades(family,(.125,.125),1731)]
                self.assertGreaterEqual(sum(widths)/len(widths),.0028)

    def test_short_cuts_use_one_segment_and_first_cut_bends(self):
        for family,count in (('Fairway',4),('Collar',4),('FirstCut',6)):
            with self.subTest(family=family):
                self.assertTrue(all(len(r)==count for r in blades(family,(.0625,.0625),1731)))

if __name__ == '__main__':
    unittest.main()
