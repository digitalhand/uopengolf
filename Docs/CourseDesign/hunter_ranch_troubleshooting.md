# Hunter Ranch: diagnose and recover from workflow problems

[Series](hunter_ranch_workflow.md) | Previous: [Validation](hunter_ranch_validation.md) | Next: [Resources](hunter_ranch_resources.md)

**Inputs:** a failed check and the revision/settings that produced it. **Output:** a corrected
authoritative source and a repeated check. Diagnose the earliest incorrect handoff rather than
compensating for every error in the final Unreal map.

## Table of contents

- [Start with a reproducible case](#start-with-a-reproducible-case)
- [GIS and data problems](#gis-and-data-problems)
- [Blender problems](#blender-problems)
- [Export and placement problems](#export-and-placement-problems)
- [Unreal and gameplay problems](#unreal-and-gameplay-problems)
- [Performance and appearance problems](#performance-and-appearance-problems)
- [Close the issue](#close-the-issue)

## Start with a reproducible case

Save the failing file and record one exact location or action that demonstrates the issue.
Note source, processing, model and import revisions. Reproduce it without decorative scenery
where possible. Compare the same point in QGIS, Blender and Unreal to locate the first stage
where the value or shape becomes wrong.

Use the [validation record](hunter_ranch_validation.md#create-a-validation-record). Change one
setting at a time and keep a known-good revision. Randomly changing scale, material and collision
settings together can create an apparently fixed view with an unexplained underlying error.

## GIS and data problems

| Symptom | Diagnose | Recovery and check |
| --- | --- | --- |
| Layer appears far from Paso Robles | Inspect stored extent and actual CRS metadata | Restore the correct source CRS; reproject a new output; compare known landmarks |
| Imagery and terrain are offset | Compare CRS, datum transformations, crop bounds and acquisition dates | Correct preparation; verify three separated points instead of dragging one layer visually |
| Raster has a sharp seam | Inspect contributing tiles, vertical units/datums and interpolation | Use compatible sources or a documented conversion; sample both sides again |
| Large spike/deep pit | Identify the cell value and NoData handling | Recover coverage or repair a documented small void; recheck statistics before height encoding |
| Trees appear as hills | Inspect whether data is surface elevation or filtered ground | Use a bare-earth product or verified ground classification; inspect hillshade beneath canopy |
| Course edge is missing | Compare study boundary with tile footprints | Expand downloads and reprocess; do not stretch the existing raster to fill missing land |
| Green contours look too smooth | Compare raster spacing, accuracy and resampling | Seek better evidence or mark approximation; finer resampling is not new detail |

For height-unit errors, compare one original source value with its expected metre conversion.
If every elevation differs by the same multiplier, investigate units. If all differ by an offset,
investigate vertical reference or origin. A difference that varies geographically may require
a proper datum transformation rather than one constant correction.

## Blender problems

**No displacement:** check that the grid contains enough vertices, the modifier is enabled,
the image texture is loaded, UV coordinates point to the intended map and Strength is nonzero.
A four-vertex plane only samples four heights, regardless of image resolution.

**Wrong relief but correct endpoints:** inspect colour-space conversion. Height data must be
linear; a gamma transform changes middle values. Compare a numerical midrange sample against
the formula in [the coordinate guide](hunter_ranch_export_coordinates.md#encode-a-height-image).

**Terrain is shifted by about half a cell:** compare sample-centre coordinates, plane dimensions
and UV inset. Do not fix a systematic raster/UV mapping error by moving every marker separately.

**Surface outline looks right from above but breaks at ground level:** inspect triangulation,
long thin faces and duplicate surfaces. Make a wireframe view and compare both sides of the
boundary. Shade Smooth only changes visible normals; it does not repair geometry.

**Bunker becomes a vertical wall:** inspect the floor and lip transition in side view. Add a
controlled transition supported by references, then retest ball contact. Large sculpt brush
changes can accidentally affect the surrounding fairway or shared patch border.

**Missing image after reopening:** inspect external image paths. Reconnect the original file
and use relative paths or deliberate packing. Do not replace a height image with a compressed
preview just because the preview opens successfully.

## Export and placement problems

First import only the 10 m calibration bar and asymmetric direction markers. If they fail,
the terrain's complexity is irrelevant to the immediate problem.

- **100x size error:** count metre/centimetre conversions. Inspect Blender scale, FBX unit
  conversion, Unreal import scale and actor scale; correct the duplicated or missing conversion.
- **Correct size, wrong orientation:** compare east and north separately. A reflection cannot
  always be repaired by rotating the actor. Correct axis conversion and inspect normals.
- **Correct shape, uniform offset:** check whether course or patch translation is baked into
  vertices and also applied to the actor. Apply the chosen offset exactly once.
- **Only one patch shifted:** compare that patch's crop bounds and pivot strategy against the
  accepted shared-origin convention. Do not redefine the origin for the entire course.
- **Height changes after updating PNG:** compare recorded Hmin/Hmax and displacement settings.
  A new crop can change the encoding even when the filename is unchanged.

After correction, repeat all three control points and at least two heights. A single point
can appear correct under several incorrect transforms.

## Unreal and gameplay problems

**Ball falls through terrain:** inspect whether collision is enabled and blocks the configured
ball trace channel. Then inspect Collision Complexity. The current ground probe uses a simple
query; a render-only mesh or inappropriate simple hull may not provide the desired surface.

**Sand plays like fairway:** inspect the hit's physical material, component surface tag and
actor surface tag. Check for an import-generated material or a whole-component override. Do
not compensate by globally changing the ball's default surface.

**Ball jumps at a seam:** disable one suspected overlapping collider temporarily in a test
copy, then inspect gaps, heights and triangulation. Correct the source boundary or ownership;
do not tune global bounce to conceal a local mesh defect.

**Cup appears correct but does not capture:** check registration on the active ball, final
surface position/normal and whether a later call replaced the active cup list. Ensure the test
is not expecting scored-hole completion while using practice placement.

**Pressing Play recreates the range:** the inspected game mode is range-oriented. A new visual
map does not supply a course adapter. Return to [Unreal integration](hunter_ranch_unreal.md#connect-tee-and-round-flow)
and record the missing integration rather than deleting shared range assets.

**Water has no golf consequence:** a visual material does not define penalty areas or drops.
List the missing rule/adapter explicitly; do not assume an engine water feature supplies golf rules.

## Performance and appearance problems

Compare fixed views with scenery categories toggled. If grass causes a large increase in GPU
time, inspect density, culling and shadow settings before simplifying ground collision. If
movement hitches but stationary frame time is good, inspect loading/streaming and shader work.

For dark/flickering seams, inspect overlapping faces and normal orientation before lighting.
For a colour ring around the camera, compare grass fading and ground material colour sources;
the maintained [turf workflow](../TurfWorkflow.md) documents similar integration concerns.

## Close the issue

Record the cause, changed source, new revision and repeated check. If the cause remains unknown,
mark the fix provisional. Update downstream exports and documentation so the next hole does
not repeat the same mistake. Consult [resources](hunter_ranch_resources.md) for official tool
manuals matched to your installed versions.
