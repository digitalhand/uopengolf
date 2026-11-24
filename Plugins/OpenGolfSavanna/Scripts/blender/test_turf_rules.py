# SPDX-License-Identifier: MIT
import hashlib
import json
import math
import statistics
import unittest
from PIL import Image, ImageDraw
from turf_rules import FOOTPRINTS, SURFACES, blades, mesh_data

# Fingerprints of the maintained families, recorded from the generator before the Rough family was added
# (2026-11-20). A change here means existing meshes, bakes and placements would drift on the next rebuild.
MAINTAINED_FINGERPRINTS = {
    'Fairway': '8ea42a20179b0da9a2aef9efb3c0f4238126f0c27c8bb93dce03fca3f8caa075',
    'FirstCut': '06f82f80157e5d6e09531d93b7e13f7b6f191e967861ee906010c3b1a99ed807',
    'Collar': 'a0e990f7eace3f268a35ca4542171f265a188618a34eb87a264d49fcafcdc2b9',
    'Green': '6d67a60c0ab59a3a1994a3a0650ebf53c5d14aab7f55c729464977c51f6f36b3',
}


def fingerprint(family):
    # Rounded to a micrometre so platform floating-point differences cannot flip the hash.
    data = []
    for size in ((.0625, .0625), (.5, .05)):
        for seed in (1731, 1732):
            vertices, faces, uvs, colors = mesh_data(family, size, seed)
            data.append([[round(x, 6) for p in vertices for x in p], faces,
                         [round(x, 6) for p in uvs for x in p], [round(x, 6) for p in colors for x in p]])
    return hashlib.sha256(json.dumps(data, separators=(',', ':')).encode()).hexdigest()


def centreline(ribbon):
    return [tuple((ribbon[i][k] + ribbon[i + 1][k]) / 2 for k in range(3)) for i in range(0, len(ribbon), 2)]


class TurfRulesTest(unittest.TestCase):
    def test_roots_stay_in_the_tile_and_tips_overlap_without_flattening(self):
        for family, spec in SURFACES.items():
            size=(.0625,.0625)
            overhanging=0
            for ribbon in blades(family,size,1731):
                self.assertEqual([v[2] for v in ribbon[:2]],[0,0])
                root=[(ribbon[0][i]+ribbon[1][i])/2 for i in range(2)]
                self.assertTrue(all(abs(p)<=s/2 for p,s in zip(root,size)))
                peak=max(v[2] for v in ribbon)
                self.assertTrue(.7*spec.height<=peak<=1.3*spec.height)
                self.assertGreaterEqual(ribbon[-1][2],.6*spec.height)
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

    def test_maintained_families_are_unchanged(self):
        for family, expected in MAINTAINED_FINGERPRINTS.items():
            with self.subTest(family=family):
                self.assertEqual(fingerprint(family), expected)

    def test_cut_turf_has_full_projected_coverage(self):
        # Rough blades stand nearly upright like the reference lawn, so less of the tile is covered from straight
        # above; the gaps show the rough ground, which the turf field gives the same colour, and grazing views fill in.
        for family,minimum in (('Fairway',.65),('FirstCut',.80),('Collar',.75),('Rough',.58)):
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
        for family,(fewest,most) in (('Fairway',(25000,90000)),('Collar',(25000,90000)),('FirstCut',(20000,140000)),
                                     ('Rough',(15000,140000))):
            with self.subTest(family=family):
                vertices,faces,uvs,colors=mesh_data(family,(.25,.25),1731)
                ribbons=list(blades(family,(.25,.25),1731))
                self.assertGreaterEqual(len(ribbons)/.25**2,fewest)
                self.assertLessEqual(2*len(faces)/.25**2,most)

    def test_blades_are_wide_enough_to_resolve(self):
        for family in ('Fairway','Collar','FirstCut','Rough'):
            with self.subTest(family=family):
                widths=[max(math.dist(r[i][:2],r[i+1][:2]) for i in range(0,len(r),2)) for r in blades(family,(.125,.125),1731)]
                self.assertGreaterEqual(sum(widths)/len(widths),.0028)

    def test_short_cuts_use_one_segment_and_first_cut_bends(self):
        for family,count in (('Fairway',4),('Collar',4),('FirstCut',6),('Rough',8)):
            with self.subTest(family=family):
                self.assertTrue(all(len(r)==count for r in blades(family,(.0625,.0625),1731)))

    def test_rough_is_even_and_leans_over_near_the_tip(self):
        # Like a tutorial hair-particle lawn: even height, nearly upright roots, tips pushed over, never folded flat.
        spec=SURFACES['Rough']
        ribbons=list(blades('Rough',(.25,.25),1731))
        peaks=[max(v[2] for v in r) for r in ribbons]
        self.assertLess(statistics.pstdev(peaks)/statistics.mean(peaks),.10)
        self.assertTrue(all(.7*spec.height<=p<=1.3*spec.height for p in peaks))
        bends,reaches,tips=[],[],[]
        for ribbon,peak in zip(ribbons,peaks):
            line=centreline(ribbon)
            def tilt(a,b):
                return math.degrees(math.atan2(math.dist(a[:2],b[:2]),b[2]-a[2]))
            bends.append(tilt(line[-2],line[-1])-tilt(line[0],line[1]))
            reaches.append(math.dist(line[0][:2],line[-1][:2])/peak)
            tips.append(line[-1][2]/peak)
        self.assertGreaterEqual(statistics.median(bends),20)
        self.assertTrue(.25<=statistics.mean(reaches)<=.7,f'reach {statistics.mean(reaches):.2f}')
        self.assertGreaterEqual(sorted(tips)[len(tips)//10],.85)

    def test_footprints_halve_for_tiled_placement(self):
        self.assertTrue(set(FOOTPRINTS)<=set(SURFACES))
        self.assertNotIn('Green',FOOTPRINTS)
        for family,footprints in FOOTPRINTS.items():
            squares=[size[0] for size,label in footprints if size[0]==size[1]]
            with self.subTest(family=family):
                self.assertTrue(all(math.isclose(b,a/2) for a,b in zip(squares,squares[1:])))
        self.assertEqual([size[0] for size,_ in FOOTPRINTS['Rough']],[1,.5,.25,.125])
        self.assertNotIn('Edge',[label for _,label in FOOTPRINTS['Rough']])
        for family in ('Fairway','FirstCut','Collar'):
            self.assertEqual([label for _,label in FOOTPRINTS[family]],['50cm','25cm','125mm','625mm','Edge'])

if __name__ == '__main__':
    unittest.main()
