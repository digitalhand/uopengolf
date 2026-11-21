# Hunter Ranch: prepare terrain in QGIS

[Series](hunter_ranch_workflow.md) | Previous: [Data acquisition](hunter_ranch_elevation_imagery.md) | Next: [Layout tracing](hunter_ranch_qgis_layout.md)

**Inputs:** elevation tiles, imagery, metadata and study polygons. **Outputs:** a Float32
geographic terrain master, inspection layers and a rectangular Hole 1 patch. Work in
`gis/hunter_ranch.qgz`; write derived files to `processed/`, never over original downloads.

## Table of contents

- [Inspect every input](#inspect-every-input)
- [Assemble compatible tiles](#assemble-compatible-tiles)
- [Reproject and establish units](#reproject-and-establish-units)
- [Crop the course master](#crop-the-course-master)
- [Inspect terrain quality](#inspect-terrain-quality)
- [Prepare a rectangular first-hole patch](#prepare-a-rectangular-first-hole-patch)
- [Save a processing record](#save-a-processing-record)

## Inspect every input

1. Drag each DEM into QGIS. Rename the display layers meaningfully without renaming originals.
2. In **Properties > Information**, record CRS, band count, pixel size, NoData, extent and
   data type. Use the source metadata for vertical datum and height units.
3. Choose **Singleband gray** or **Singleband pseudocolor** under Symbology. A renderer changes
   the display, not the numerical elevation values. A contrast-stretched image is not a heightmap export.
4. Use Identify at several positions. Confirm that the reported numbers are elevations, not
   RGB intensities. Look for valid low points accidentally confused with NoData.
5. Compare imagery and terrain footprints. Resolve missing/incorrect CRS labels from metadata.
   Never assign EPSG:26910 merely to make an unknown raster appear near the course.

See [Raster Properties](https://docs.qgis.org/3.44/en/docs/user_manual/working_with_raster/raster_properties.html)
for the relevant inspection controls.

## Assemble compatible tiles

Open **Processing Toolbox**, search **Build virtual raster**, and choose compatible elevation
tiles. Keep each file as part of one elevation band rather than placing input tiles into separate
bands. Save as `processed/terrain_mosaic.vrt`. A VRT references its input files, so moving those
files afterward can break it.

Inspect seams before proceeding. Use a consistent acquisition and vertical reference. If pixel
sizes differ, choose a justified target spacing in the reprojection step rather than assuming
the finest available sample spacing means equal accuracy everywhere.

Use [QGIS raster miscellaneous tools](https://docs.qgis.org/3.44/en/docs/user_manual/processing_algs/gdal/rastermiscellaneous.html)
for virtual mosaics and raster information. If you need an independent deliverable, write a
GeoTIFF; the small VRT itself is not a backup of the elevation data.

## Reproject and establish units

1. Search **Warp (reproject)**. Choose the mosaic as input and EPSG:26910 as target CRS.
2. Choose bilinear resampling for continuous elevations. Retain justified resolution, such as
   1 m for a suitable 1 m source, and Float32 output to preserve fractional heights.
3. Preserve a defined NoData value. Save to `processed/terrain_projected.tif`.
4. Inspect the result's spacing and extent rather than relying on the dialog settings alone.
5. Establish Z units separately. If heights are international feet, multiply by 0.3048; if
   US survey feet, the factor is 1200/3937. Use the factor actually specified in metadata.
   Use Raster Calculator to produce a new metre-valued raster, preserving NoData behaviour.
6. Record the vertical reference. Do not relabel ellipsoidal heights as orthometric heights.
   A required datum conversion needs a documented transformation, not just a unit factor.

These are project-specific choices applied through [QGIS Warp](https://docs.qgis.org/3.44/en/docs/user_manual/processing_algs/gdal/rasterprojections.html).
Check sample elevations after conversion. If a 300-foot input becomes approximately 91.44 m,
the arithmetic is plausible; it still does not prove the source's height datum.

## Crop the course master

Use **Clip raster by mask layer**, select the working DEM and `aoi_context`, and preserve the
target spacing and CRS. Save `processed/hunter_ranch_dem_m.tif`. Keep NoData outside the mask.
Verify that the tool did not introduce a new grid origin or spacing; record the actual output
geotransform for all downstream crops. See [QGIS extraction tools](https://docs.qgis.org/3.44/en/docs/user_manual/processing_algs/gdal/rasterextraction.html).

This masked master is useful for GIS inspection. It is not automatically suitable for a
rectangular Blender height image because its outer corners may contain NoData. For that image,
crop from a rectangular source with valid coverage or make a documented context-fill decision.

## Inspect terrain quality

Generate hillshade using the Processing Toolbox. If horizontal and vertical units are both
metres, avoid an unintended vertical exaggeration. Use a second light direction to distinguish
real ridges from lighting illusions. Overlay imagery with adjustable opacity.

Create 1 m contours for broad inspection if useful. A contour interval finer than the raster
accuracy is a visualization choice, not new measurement detail. At Hole 1, inspect the tee,
landing area, green, bunker complexes and nearby tree groups.

For a suspected seam, sample points immediately on either side and compare with source tiles.
For a NoData hole, first check missing downloads. Filling a small isolated raster void is
different from inventing an entire green beneath tree canopy. Record interpolated areas in
a polygon layer so later modelling can distinguish them from measured terrain.

Do not smooth the whole raster because a hillshade looks noisy. Determine whether noise is
measurement error, interpolation, actual terrain or display contrast before editing elevations.

## Prepare a rectangular first-hole patch

1. Use `export_patches` to identify the first-hole rectangle. Fit its edges to the master grid.
2. Run **Clip raster by extent**, preserving the original resolution and alignment. Save
   `processed/h01/h01_dem_m.tif`.
3. Record west/east/south/north outer edges, columns, rows, pixel spacing and rotation. These
   guides assume a north-up raster with no rotation; reproject rotated sources to that form.
4. Verify all cells needed by the mesh have valid heights. Retain the rectangular imagery
   reference even if it has a different pixel resolution, but give it identical geographic bounds.
5. Compute valid minimum and maximum heights over this patch. Store them for height encoding.
   Exclude NoData from statistics; -9999 is not a mountain valley.

**Illustrative grid:** 501 columns at 1 m spacing occupy 501 m between outer pixel edges,
but the first-to-last sample-centre span is 500 m. Preserve that distinction for Blender UVs.
The [export guide](hunter_ranch_export_coordinates.md) defines the shared coordinate record.

## Save a processing record

Record input filenames, tool names, CRS transformation, unit factor, output resolution,
resampling, NoData policy, crop extent and output names. QGIS Processing history can help recover
parameters, but also keep a human-readable summary beside the outputs.

Reopen the project and verify it still finds the files. The handoff is ready when the master
and Hole 1 patch have correct units, complete coverage and a documented grid. Continue to
[layout tracing](hunter_ranch_qgis_layout.md); encode the height image when preparing the Blender handoff.
