# Hunter Ranch: coordinates, height encoding and exports

[Series](hunter_ranch_workflow.md) | Previous: [Blender](hunter_ranch_blender.md) | Next: [Unreal](hunter_ranch_unreal.md)

**Inputs:** geographic masters and the Blender working model. **Outputs:** a coordinate record,
height image, FBX export, marker table and calibration results. This chapter is the authority
for the series' coordinate convention; all examples below are illustrative, not surveyed values.

## Table of contents

- [Create the coordinate record](#create-the-coordinate-record)
- [Convert geographic positions](#convert-geographic-positions)
- [Understand raster edges and samples](#understand-raster-edges-and-samples)
- [Encode a height image](#encode-a-height-image)
- [Export a mesh without losing placement](#export-a-mesh-without-losing-placement)
- [Export markers](#export-markers)
- [Calibrate and reimport](#calibrate-and-reimport)

## Create the coordinate record

Create `exports/coordinates.md` before exporting the first hole:

```text
Working horizontal CRS: EPSG:26910
Source vertical datum:
Source Z units and conversion:
Course origin E0, N0, H0 (metres):
Blender axes: +X east, +Y north, +Z up
Target Unreal axes: +X east, +Y north, +Z up
Patch ID and revision:
Outer raster bounds west/east/south/north:
Columns C, rows R, spacing sx/sy:
Raster rotation: 0 for this workflow
First/last sample-centre coordinates:
Valid Hmin/Hmax and NoData policy:
Mesh origin and actor placement policy:
Blender FBX settings / Unreal import settings:
Calibration results:
```

Choose a course origin near the site and keep it fixed across holes. A round projected
coordinate is convenient, but its actual value must come from the study area. `H0` is an
offset for local scene coordinates, not a change of vertical datum.

## Convert geographic positions

```text
Blender x_m = E - E0
Blender y_m = N - N0
Blender z_m = H - H0

Unreal X_cm = 100 * x_m
Unreal Y_cm = 100 * y_m
Unreal Z_cm = 100 * z_m
```

These equations define the intended map axes. FBX exporters/importers have their own axis
conversions, so verify the result before relying on it. A correct length alone cannot detect
a mirrored course.

**Illustrative arithmetic:** a point 125 m east, 80 m north and 12.5 m above the chosen origin
must end up at `(12500,8000,1250)` cm in the target convention. Put asymmetric labelled markers
at those relative directions to detect an unexpected rotation or reflection.

## Understand raster edges and samples

For a north-up raster with west outer edge `W`, north outer edge `N`, and positive spacings:

```text
sample_easting(column) = W + (column + 0.5) * sx
sample_northing(row)   = N - (row + 0.5) * sy
```

Column and row are zero-based. The full image extent is `C*sx` by `R*sy`; the span between
outer sample centres is `(C-1)*sx` by `(R-1)*sy`. This is why the Blender guide uses an inset UV
rectangle instead of stretching the first and last samples to the geographic outer edges.
See [GDAL geotransforms](https://gdal.org/en/stable/tutorials/geotransforms_tut.html).

When exporting adjacent patches, deliberately share an edge row/column of samples so both
meshes can use identical boundary positions. Simply clipping two non-overlapping pixel blocks
leaves their outer sample centres one spacing apart.

## Encode a height image

First resolve NoData inside the rectangular mesh footprint. Determine valid `Hmin` and `Hmax`
using full raster statistics, not only a colour renderer's approximate display range. Preserve
the Float32 elevation file. If all heights are identical, create a flat mesh instead of scaling
by a zero height range.

Use this unsigned 16-bit encoding:

```text
code = round(65535 * (H - Hmin) / (Hmax - Hmin))
H = Hmin + code * (Hmax - Hmin) / 65535
```

In QGIS **Translate (convert format)**, choose the patch GeoTIFF, UInt16 output, PNG format,
and explicit source/destination scaling through the advanced parameters where available.
The optional equivalent below uses **example bounds 240–280 m**, which must be replaced:

```powershell
gdal_translate -of PNG -ot UInt16 -scale 240 280 0 65535 "processed/h01/h01_dem_m.tif" "processed/h01/h01_height_u16.png"
```

Run from the authoring workspace in a configured GDAL shell. This command converts values;
it does not repair NoData, establish a datum or verify that the chosen bounds contain all heights.
Inspect the output with `gdalinfo` and preserve any georeferencing sidecars. See
[GDAL Translate](https://gdal.org/en/stable/programs/gdal_translate.html).

For the example's 40 m range, one code step is about 0.00061036 m. That is encoding precision,
not terrain accuracy. After rounding, pure quantisation error is at most half a step for values
within range; interpolation, source error and modelling changes are additional effects.

## Export a mesh without losing placement

1. Save the Blender master and create an export copy with applied geometry modifiers and stable
   material slots. Keep normal/triangulation changes predictable between revisions.
2. For the first patch, use a **shared-origin mesh**: make its object origin the course local
   origin and apply location on the export copy so its vertices retain course-local positions
   while object location/rotation are zero and scale is one. Do not do this blindly to a parented
   object; first inspect the complete transform and use an unparented export copy.
3. Export selected mesh objects through **File > Export > FBX**. Include meshes, exclude cameras,
   lights and reference objects, and preserve material slots. Use scale 1 and record unit/axis
   conversion settings. A common starting axis preset is -Y Forward / Z Up, but calibration
   determines whether it works with the selected Unreal importer and version.
4. Export calibration geometry using the same settings. In Unreal use import scale 1 initially.
   Verify one consistent unit conversion; do not multiply by 100 in both Blender and Unreal.
5. With course-local positions baked in the vertices, place the imported terrain actor at
   `(0,0,0)` with no rotation and unit scale. Verify markers and pivot behaviour.

Later, patch-local pivots can improve editing convenience, but require a per-patch translation
in the placement record. Do not mix that strategy with shared-origin meshes without documenting
which offsets are baked into vertices and which belong to actors. Review Epic's
[FBX static mesh pipeline](https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-static-mesh-pipeline-in-unreal-engine)
for importer-specific handling.

## Export markers

Create `exports/h01/h01_markers.csv` with:

```csv
marker_id,hole_id,kind,tee_name,easting_m,northing_m,height_m,x_cm,y_cm,z_cm,source_id,notes
```

Export geographic X/Y from QGIS, then calculate local coordinates with the recorded origin.
Distinguish DEM height from final model height in `notes`. A pin moved onto a sculpted green
must use the final surface height and normal in Unreal; do not register its old DEM Z blindly.
The CSV is a manual handoff until a game importer is implemented.

## Calibrate and reimport

Import the 10 m object first and confirm 1,000 cm. Check east and north separately. Compare
three non-collinear landmark positions and at least two heights, then record the import preset.
If all distances are correct but all positions are shifted, fix origin/placement rather than
individually moving markers.

On reimport, keep slot names/order stable and check collision and material assignments again.
Changing the crop's `Hmin/Hmax` requires updating displacement settings even if the PNG filename
stays the same. Version the height image and coordinate record together to prevent stale settings.
