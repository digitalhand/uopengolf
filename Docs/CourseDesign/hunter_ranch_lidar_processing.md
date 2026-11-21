# Hunter Ranch: optional LiDAR processing

[Series](hunter_ranch_workflow.md) | Previous: [Data acquisition](hunter_ranch_elevation_imagery.md) | Next: [QGIS terrain](hunter_ranch_qgis_terrain.md)

**Inputs:** classified LAS/LAZ files with metadata. **Output:** a ground-elevation GeoTIFF
and a record of processing choices. Skip this chapter when a supplied DEM meets your needs.
The pipeline below is a template, not a tested Hunter Ranch data conversion.

## Table of contents

- [Decide whether processing is justified](#decide-whether-processing-is-justified)
- [Inspect the cloud](#inspect-the-cloud)
- [Prepare a small trial](#prepare-a-small-trial)
- [Run the optional PDAL pipeline](#run-the-optional-pdal-pipeline)
- [Validate the derived ground](#validate-the-derived-ground)
- [Handle multiple tiles](#handle-multiple-tiles)

## Decide whether processing is justified

Write down the defect in the supplied DEM: missing coverage, overly coarse samples, a clearly
documented processing problem, or a newer survey available as point data. Raw LiDAR does not
automatically solve poor ground sampling beneath vegetation or provide survey-quality greens.
Use the acquisition metadata to compare date, classification, density and vertical accuracy.

If the cloud is unclassified, stop before applying a ground-only filter. Automated ground
classification requires parameter selection and visual checks of slopes, bunkers and trees.
It is a separate processing experiment; this chapter assumes the provider already classified
ground points. A class number is meaningful only after checking the dataset's classification scheme.

## Inspect the cloud

1. Add a representative LAS/LAZ file through QGIS's Browser or Data Source Manager. Allow any
   point-cloud index generation to finish. Display support depends on the installed build.
2. Open layer information. Record CRS, extent, point count, classification attributes and
   height range. Compare the footprint with the study boundary.
3. Colour points by classification, then by elevation. Inspect ground near a green, a bunker,
   a tree group and a clear fairway. Use a 3D view if supported to distinguish canopy from ground.
4. Check the source's vertical units and datum. Reprojecting horizontal coordinates does not
   justify assuming that Z is already metres or has been converted to another vertical datum.
5. Keep the original cloud unchanged. Work on a small area with surrounding points first.

Read [PDAL's information command](https://pdal.io/en/stable/apps/info.html) for an optional
command-line inspection. In a configured PDAL shell, a starting command is:

```powershell
pdal info --metadata "source/lidar/REPLACE_WITH_TILE.laz"
```

Replace the filename with a downloaded file; `REPLACE_WITH_TILE` is not a catalog identifier.

## Prepare a small trial

Choose a tile or crop containing part of Hole 1 plus neighbouring samples. For a first trial,
use one cloud with established CRS and Z in metres. Verify that class 2 means ground in this
dataset. If it does not, change the selection based on its metadata rather than forcing class 2.

The pipeline below uses inverse-distance interpolation at 1 m spacing. That spacing and the
1.5 m search radius are trial settings, not claims about local point density. Sparse areas
will require inspection; increasing the radius can bridge gaps but also smooth real features.

## Run the optional PDAL pipeline

Save the following as `processed/h01/ground_trial.json` in the external authoring workspace.
Run from the `HunterRanch/` folder. Replace the source path. Keep the output name unique so
reruns do not overwrite an accepted terrain master.

```json
{
  "pipeline": [
    {"type": "readers.las", "filename": "source/lidar/REPLACE_WITH_TILE.laz"},
    {"type": "filters.expression", "expression": "Classification == 2"},
    {"type": "filters.reprojection", "out_srs": "EPSG:26910"},
    {
      "type": "writers.gdal",
      "filename": "processed/h01/ground_trial.tif",
      "resolution": 1.0,
      "radius": 1.5,
      "output_type": "idw",
      "data_type": "float32",
      "nodata": -9999
    }
  ]
}
```

```powershell
pdal pipeline "processed/h01/ground_trial.json"
```

The expression selects the intended points; the writer grids their Z values. The reprojection
stage relies on valid input CRS metadata and does not establish the vertical conversion needed
for an arbitrary source. If Z is not metres, add an independently verified unit/datum conversion
before gridding. Consult [expression filtering](https://pdal.io/en/stable/stages/filters.expression.html),
[reprojection](https://pdal.io/en/stable/stages/filters.reprojection.html), and
[GDAL raster writing](https://pdal.io/en/stable/stages/writers.gdal.html) for the installed version.

## Validate the derived ground

Open the output in QGIS. Confirm one elevation band, the intended spacing and plausible extent.
Render hillshade and inspect the same four areas used in the point-cloud inspection. Check that
tree crowns and roofs disappeared without removing real ground features.

Compare selected elevations with ground points and any supplied DEM. Record differences with
their locations; do not declare the new raster better merely because it has more visible detail.
Examine holes beneath trees and around water. Water elevations and submerged terrain may be
poorly represented, and interpolation across them is an explicit modelling decision.

Change one parameter at a time when experimenting. Retain the JSON and results for each trial.
Accept the product only after checking voids, slopes and units, then continue to the terrain guide.

## Handle multiple tiles

Independent tile interpolation can create seams because each edge lacks neighbours. For a
larger patch, read the relevant compatible tiles, merge their point views, and grid with overlap
beyond the final crop. Crop the accepted output to the shared raster grid afterward. Plan memory
use before merging a whole acquisition; begin with the first-hole neighbourhood.

Do not average surveys with different vertical references or dates without an explicit reason.
Record which tiles contributed to the result. If you cannot establish those references, retain
the supplied consistent DEM and document the limitation instead of disguising a seam with smoothing.
