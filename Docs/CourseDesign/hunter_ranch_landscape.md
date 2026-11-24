# Hunter Ranch: optional Unreal Landscape route

[Series](hunter_ranch_workflow.md) | Prerequisite: [Coordinates](hunter_ranch_export_coordinates.md) | Continue: [Unreal validation](hunter_ranch_validation.md)

**Inputs:** geographic DEM, aligned masks and coordinate record. **Outputs:** a calibrated
Landscape prototype. Use this as an alternative broad-terrain representation; it is not required
for the Blender mesh workflow. Landscape and detailed mesh joins need independent validation.

## Table of contents

- [Decide what Landscape owns](#decide-what-landscape-owns)
- [Choose a supported grid](#choose-a-supported-grid)
- [Calculate scale and elevation offset](#calculate-scale-and-elevation-offset)
- [Import and check orientation](#import-and-check-orientation)
- [Add surface layers](#add-surface-layers)
- [Join detailed meshes](#join-detailed-meshes)

## Decide what Landscape owns

A heightfield stores one elevation per horizontal sample; it works well for broad landforms.
Fine bunker lips, tight transitions and custom mesh topology can be easier to control in Blender.
For the first Landscape experiment, import a small area and compare its shape and ball contact
with the accepted mesh before committing the entire course.

Write down which surface owns collision in each region. A detail mesh over a still-collidable
Landscape can produce competing contacts. A Landscape hidden only visually can still be hit
by the ball. Keep a clean boundary plan from the beginning.

## Choose a supported grid

Follow the component/section combinations in [Epic's Landscape technical guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine).
For example, 505-by-505 vertices can use 4-by-4 components with 2-by-2 sections of 63 quads
per section. At 1 m between vertices, this represents a 504-by-504 m sample-centre span.
This is an illustrative supported configuration, not Hunter Ranch's measured extent.

In QGIS/GDAL, prepare an aligned raster whose samples match the intended vertex positions.
Expand/crop the geographic area to a suitable grid where practical. If resampling is necessary,
record target spacing and interpolation. Never resize the image in a graphics editor and
retain the old metre spacing without checking the resulting course dimensions.

## Calculate scale and elevation offset

Landscape height codes are centred around 32768. The underlying local height relationship is
`(code - 32768) / 128`, multiplied by actor Z scale in centimetres. For this series' encoding
where 0 maps to `Hmin` and 65535 to `Hmax`, an endpoint-consistent derivation is:

```text
range_m = Hmax - Hmin
Z_scale = 100 * range_m * 128 / 65535
actor_Z_cm = 100 * (Hmin - H0) + 256 * Z_scale
```

This formula follows from the documented signed Landscape range and the series' explicit
65535 encoding. Epic's common `range_cm / 512` approximation differs slightly at the top code.
Do not mix two encodings and then add an arbitrary visual offset to disguise the difference.

**Illustrative example:** `Hmin=240`, `Hmax=280`, `H0=200` gives Z scale about 7.812619 and
actor Z about 6000.0305 cm. Code 0 reconstructs 4000 cm above the local origin and code 65535
reconstructs 8000 cm. Check the actual imported endpoints and intermediate samples.

For 1 m spacing, X and Y scale start at 100 cm per grid interval. Actor location must also
account for the Landscape component layout/pivot and the chosen first-sample location; do not
assume its origin is the geographic southwest corner without checking the imported bounds.

## Import and check orientation

1. In a separate test map, enter Landscape mode and choose **Import from File**.
2. Select the 16-bit grayscale heightmap. Confirm that the proposed resolution/component
   arrangement matches your prepared grid before creating the actor.
3. Enter the calculated X/Y/Z scales and the intended placement. Record the settings.
4. Compare the three control points with the geographic record. North-up image rows increase
   southward, while your target +Y increases northward: use the importer's flip option if present
   or a deliberately prepared flipped export. Apply the same orientation to masks.
5. Check dimensions and multiple elevations, then save the accepted import settings.

See [Epic's heightmap import instructions](https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-and-exporting-landscape-heightmaps-in-unreal-engine).
Select documentation matching the repository's engine version; unversioned links can show newer releases.

## Add surface layers

Create a Landscape material with explicit target layers for the golf surfaces you need, then
create the corresponding Layer Info assets. Decide how weights blend at boundaries. For a first
validation pass, hard, obvious colours make classification errors easier to see than detailed grass.

Rasterise the geographic polygons to aligned layer masks if using imported masks. Keep the
same origin, sample positions, dimensions and orientation as the heightmap. Use a deliberate
class/coverage rule for narrow fringes and boundary pixels rather than interpolating class IDs
as continuous elevations. Consult [Landscape materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine)
for layer behaviour.

Set physical-material information using the Landscape mechanism supported by your engine
configuration, then probe it with the actual ball queries. A layer named `Sand` is not enough
to assign the repository's `UOpenFairwayPhysicalMaterial`. Compare the visible and returned
surface on both sides of every transition.

## Join detailed meshes

Keep boundary vertices at common world heights. Decide whether a Landscape hole removes the
underlying region or whether another tested collision arrangement gives the detail mesh sole
ownership. Verify the join with slow rolls in both directions and through corners.

Do not assume changing collision mip level or enabling Nanite preserves putting behaviour.
Record collision settings and compare them against the source mesh. If this branch adds more
handoff risk than it solves for the first hole, retain the mesh workflow and evaluate Landscape
later for distant context only.
