# Hunter Ranch: acquire elevation and aerial imagery

[Series](hunter_ranch_workflow.md) | Previous: [Site boundary](hunter_ranch_site_boundary.md) | Next: [QGIS terrain](hunter_ranch_qgis_terrain.md)

**Inputs:** approximate study extent and source register. **Outputs:** original elevation and
orthophoto files, associated metadata and a coverage decision. This guide does not certify an
exact Hunter Ranch tile, acquisition project or imagery year.

## Table of contents

- [Choose the elevation product](#choose-the-elevation-product)
- [Select and download elevation](#select-and-download-elevation)
- [Select aerial imagery](#select-aerial-imagery)
- [Inspect downloads in QGIS](#inspect-downloads-in-qgis)
- [Record the acquisition decision](#record-the-acquisition-decision)
- [Handle incomplete or conflicting coverage](#handle-incomplete-or-conflicting-coverage)

## Choose the elevation product

Start with a supplied ground DEM. It already represents elevations on a regular grid and
avoids a separate point-cloud processing task. Prefer approximately 1 m spacing where a
suitable product exists, but check acquisition date and accuracy rather than choosing solely
by pixel size. A high-resolution surface model can contain trees and buildings that are
unsuitable as the golf course's ground.

Raw LiDAR is useful when its classified ground returns can improve a deficient DEM. It adds
classification, gridding and void-handling decisions. Use the [LiDAR guide](hunter_ranch_lidar_processing.md)
only after establishing why the supplied raster is insufficient. USGS explains available
product families in [3DEP products and services](https://www.usgs.gov/3d-elevation-program/about-3dep-products-services).

## Select and download elevation

1. Launch [LidarExplorer](https://www.usgs.gov/tools/lidarexplorer). Navigate to the verified
   Hunter Ranch study area, not merely the centre of Paso Robles.
2. Inspect the available project/product footprints. Open metadata for candidates intersecting
   the course and surrounding margin. Catalog controls vary; use the provider's download and
   metadata links associated with each result.
3. Record the project name, acquisition dates, resolution, horizontal CRS, vertical datum,
   height units and published accuracy. A filename with `1m` is not enough metadata.
4. Prefer one consistent acquisition covering the first-hole patch and ideally the course.
   If multiple tiles belong to that acquisition, download all that intersect the study area.
5. Store original files under `source/elevation/`, preserving filenames. Store XML, text or
   PDF metadata beside them. Extract archives into a subfolder rather than mixing their contents
   with unrelated acquisitions.
6. Keep the download URLs and identifiers in the source register. Confirm file sizes and
   checksums when the provider supplies them. An interrupted file may exist but still be invalid.

**Checkpoint:** tile footprints cover the buffered study area with no unexplained gaps.
Do not assume a coastal San Luis Obispo County survey extends inland to Hunter Ranch.

## Select aerial imagery

Use [The National Map data-download guidance](https://www.usgs.gov/the-national-map-data-delivery/gis-data-download)
and its NAIP imagery index. The [USGS NAIP archive description](https://www.usgs.gov/centers/eros/science/usgs-eros-archive-aerial-photography-national-agriculture-imagery-program-naip)
is useful for understanding the product and acquisition information.

1. Search the same study area. Inspect acquisition dates and resolution for actual intersecting
   images, not just the service's general description.
2. Prefer a coherent acquisition where fairway, green and bunker edges are visible. Compare
   current official references with older imagery to identify changes.
3. Download georeferenced images using the catalog's actual download links. A screenshot of
   a browser map lacks reliable geographic placement and is unsuitable as the tracing master.
4. Store files and metadata under `source/imagery/`. Keep native bands initially. If the image
   includes near-infrared, inspect the product's band order before choosing visible RGB bands.
5. Record the image's reuse and attribution terms. A display service and a downloadable source
   product may have different delivery capabilities; do not assume either is an export licence.

## Inspect downloads in QGIS

Drag the imagery and elevation files into QGIS. For each, open **Properties > Information**
and record dimensions, extent, pixel size, CRS, bands and NoData information. For a DEM,
use a single-band grayscale or pseudocolour display. For a multiband orthophoto, use the
documented red, green and blue bands in **Symbology**.

Toggle layers individually. If everything appears blank, zoom to the layer and check whether
the extent is plausible. If two layers appear on different continents, fix incorrect CRS
assignment from metadata before trying to drag one into place. QGIS documents these controls
in [Raster Properties](https://docs.qgis.org/3.44/en/docs/user_manual/working_with_raster/raster_properties.html).

## Record the acquisition decision

Create one row per downloaded product in `references/data_inventory.csv`:

```csv
source_id,provider,product,tile_id,filename,acquired_date,pixel_m,horizontal_crs,vertical_datum,z_units,url,coverage_notes
```

Add a short decision note: why this product was chosen, where coverage is weak, and whether
the imagery and elevation dates are compatible. Record the distinction between source accuracy
and raster spacing. A 1 m pixel size does not establish centimetre-accurate putting contours.

## Handle incomplete or conflicting coverage

If Hole 1 crosses a missing tile, resolve the download before generating a height image.
If only coarse elevation exists, complete a scale/routing preview and label detailed green
shape as provisional. If acquisitions differ in vertical reference, obtain a documented
conversion or choose a consistent source; do not blend a seam away with sculpting.

If imagery dates disagree, use an explicit feature-by-feature decision for changed bunkers or
tees. Keep the older source in the register so later readers understand the reconstruction's
chosen date. Proceed to [QGIS terrain](hunter_ranch_qgis_terrain.md), or the optional LiDAR branch.
