# Hunter Ranch: validation procedures and completion gates

[Series](hunter_ranch_workflow.md) | Previous: [Full course](hunter_ranch_full_course.md) | Next: [Troubleshooting](hunter_ranch_troubleshooting.md)

**Inputs:** source records and the relevant prototype stage. **Outputs:** repeatable evidence
of what is correct, approximate or still untested. No Hunter Ranch gameplay checks were run
while writing this series; use this chapter when the course assets and integration exist.

## Table of contents

- [Create a validation record](#create-a-validation-record)
- [Check sources and geography](#check-sources-and-geography)
- [Check mesh conversion](#check-mesh-conversion)
- [Check surfaces and contact](#check-surfaces-and-contact)
- [Check putting and cup capture](#check-putting-and-cup-capture)
- [Measure performance](#measure-performance)
- [Review the full course](#review-the-full-course)
- [Completion checklist](#completion-checklist)

## Create a validation record

Create `validation/h01_revision_001.md` for the first export. Include tool versions, source and
model revisions, test-map name, date, hardware, graphics settings and tester notes. Use this
table for each check:

| Check ID | Setup and input | Expected result | Observed result | Evidence | Status |
| --- | --- | --- | --- | --- | --- |
| SCALE-01 | 10 m exported bar | 1,000 cm in Unreal | Fill after measuring | Screenshot/measurement | Pending |
| ALIGN-01 | Three non-collinear points | Match coordinate record | Fill after measuring | Coordinate table | Pending |
| SURFACE-01 | Green/fringe boundary probes | Intended surface on each side | Fill after probing | Hit results | Pending |

Use `pending`, `pass`, `fail`, or `blocked`, with a reason for blocked checks. Never turn
`pending` into `pass` because the geometry looks plausible.

## Check sources and geography

Verify full source coverage, recorded vertical reference, actual imagery dates and the selected
course revision. Review the scorecard sums and hole identities independently of your mesh.

For at least three separated first-hole control points, record geographic E/N/H and intended
local X/Y/Z. Include points near opposite ends and a lateral point so a mirrored or rotated
import cannot pass using just one baseline. Record source confidence: points traced from a
1 m image do not justify millimetre claims about real-world location.

Check NoData, seams and suspicious elevations in QGIS before comparing game geometry.
Distinguish source uncertainty from conversion error. The terrain may transfer perfectly while
still being too inaccurate to reproduce a real green's putting contours.

## Check mesh conversion

1. Measure the calibration object in Unreal and verify 1,000 cm. Investigate any difference
   before importing more holes.
2. Compare all control positions numerically. For unedited points, conversion residuals should
   be explainable by encoding, interpolation and floating-point effects, not metre-scale shifts.
3. Reconstruct selected height codes using the recorded formula. Include a low, middle and
   high value to detect unintended gamma correction or stale displacement settings.
4. Compare source and mesh boundaries in plan view, then inspect normals and triangulation
   from low camera angles. Smooth shading alone cannot prove contact geometry is smooth.
5. Record intentional sculpt differences separately. A planned 0.1 m change is not an import
   error, but it must not be misreported as a measured geographic elevation.

Use a tolerance based on the specific check. For an exact 10 m authored object, an initial
conversion target of at most 0.1 cm discrepancy is reasonable to investigate; it is not a
real-world survey tolerance. For imagery-traced boundaries, choose a tolerance consistent with
the source resolution and record it before comparing.

## Check surfaces and contact

With debug probes or a future small test helper, sample the terrain at the centre of every
material region and on each side of boundaries. Record hit component, point, normal and physical
material. Compare against the [physics resolver rules](../../Plugins/OpenFairwayPhysics/Docs/Reference.md#surfaces).

Roll the ball slowly across green/fringe, fairway/rough, sand/grass and terrain-patch joins in
both directions. Include diagonal crossings and corners. Watch for artificial jumps, sinking,
stopping on invisible edges, or changing surfaces before reaching the visible boundary.

If the ball uses the wrong surface, inspect tags and returned materials before tuning friction.
If it jumps, inspect duplicate collision and geometry before changing the physics profile.
Keep wind and launch conditions fixed during comparisons.

## Check putting and cup capture

Start with a gentle straight putt toward the registered cup, then repeat from different
directions and distances. Confirm the visual cup and physics descriptor share location and
normal. Test a shot that stops short and one that passes beside the cup so a false capture
does not appear as success.

Record green speed/settings and final model revision. Terrain shape, physical surface and
green-speed parameters all affect roll. Do not claim real Hunter Ranch putting fidelity without
appropriate measurement/reference evidence. A candidate pin that works in the simulation remains
an authored position unless independently verified.

Check scored play separately from practice placement. The current hole-round rules treat
manual placement as practice; a test using placement may intentionally not finish a scored hole.
See [the local play rules](../../Plugins/OpenGolfCourse/Docs/Reference.md#scoring-and-play-rules).

## Measure performance

Use fixed tee, landing-area and green camera views. Record resolution, scalability, VSync/frame
cap, hardware and whether running in editor or a packaged build. Let shader compilation and
initial loading settle before comparing steady-state performance.

In the Unreal console, `stat unit` and `stat fps` provide initial timing observations; use
`stat gpu` when supported for a more detailed GPU view. Consult
[Epic's stat commands](https://dev.epicgames.com/documentation/en-us/unreal-engine/stat-commands-in-unreal-engine).
Compare terrain-only, trees-added and grass-added passes under identical conditions.

A target frame rate is a product decision. Record one explicitly for your hardware instead
of declaring the course fast from a single screenshot. Capture hitches during movement as well
as stationary averages. Do not optimise by degrading putting collision without retesting roll.

## Review the full course

Play the official hole order once full-round integration exists. Change tee sets, restart a hole,
move between holes and finish Hole 18. Check score progression, active cup selection and spawn
positions. Inspect paths and scenery from adjacent holes to catch clipping outside the first view.

Repeat relevant checks after reimport, not every unrelated test in the project. A material-slot
change requires surface checks; a coordinate change requires alignment checks across affected
holes; a foliage-only change primarily requires visual/performance review unless collision changed.

## Completion checklist

- [ ] Source inventory, metadata and usage records complete.
- [ ] Hole identities and scorecard transcription checked.
- [ ] CRS, vertical reference, units and shared origin documented.
- [ ] No unexplained NoData, seams or canopy-derived ground.
- [ ] Scale, axes and control-point checks pass.
- [ ] Material classification matches visible boundaries.
- [ ] Terrain joins and slow rolls pass in both directions.
- [ ] Cup placement and capture behaviour verified.
- [ ] Tee selection, practice/scored play and full-round flow verified where implemented.
- [ ] Performance recorded on the intended machine/settings.
- [ ] Artistic approximations and unresolved integration work listed.
- [ ] Geographic masters, Blender files and export settings retained.
