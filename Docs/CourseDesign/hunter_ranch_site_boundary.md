# Hunter Ranch: locate the course and define the study area

[Series](hunter_ranch_workflow.md) | Previous: [Setup](hunter_ranch_setup.md) | Next: [Data acquisition](hunter_ranch_elevation_imagery.md)

**Inputs:** official references, QGIS and a reference map. **Outputs:** study boundary,
context buffer and a recorded horizontal working CRS. The boundary is for modelling, not a
surveyed parcel boundary. Exact Hunter Ranch bounds have not been measured for these guides.

## Table of contents

- [Locate the correct property](#locate-the-correct-property)
- [Set the working coordinate system](#set-the-working-coordinate-system)
- [Create the boundary layer](#create-the-boundary-layer)
- [Add surrounding terrain](#add-surrounding-terrain)
- [Plan the first-hole patch](#plan-the-first-hole-patch)
- [Check and revise the boundary](#check-and-revise-the-boundary)

## Locate the correct property

1. Open [LidarExplorer](https://www.usgs.gov/tools/lidarexplorer) or
   [The National Map](https://apps.nationalmap.gov/downloader/).
2. Search for `Hunter Ranch Golf Course, Paso Robles, California`, then the street address
   `4041 Highway 46 East` if necessary. Search controls change; use the map around Paso Robles
   and Highway 46 East if address lookup is unavailable.
3. Compare the result with the [official site](https://www.hunterranchgolf.com/). Confirm the
   road, entrance/clubhouse context and extent of golf-shaped features before selecting data.
4. Record the map position as a discovery aid. Do not use an entrance pin as the entire
   course's centre or boundary. Do not treat unverified search coordinates as surveyed control.

You can begin with a generous catalog selection and refine the exact polygon after acquiring
georeferenced imagery. The workflow therefore permits a second pass through this guide.

## Set the working coordinate system

Open `gis/hunter_ranch.qgz`. Click the CRS indicator in the status bar, search for **26910**,
and choose **NAD83 / UTM zone 10N**. This is the series' proposed local metre-based horizontal
working system. Save the project.

Every loaded layer must retain its actual source CRS. A layer in longitude/latitude can be
displayed alongside a projected layer because QGIS transforms it for display. Changing the
project CRS does not rewrite that layer's coordinates. Use a reprojection/export tool when
you need stored coordinates in the working system.

Height datum and units require separate metadata. EPSG:26910 does not by itself say what
vertical reference a DEM uses. Record that in the source register before combining elevations.

## Create the boundary layer

1. After loading georeferenced imagery, choose **Layer > Create Layer > New GeoPackage Layer**.
2. Set database to `gis/hunter_ranch.gpkg`, table name to `aoi`, geometry to Polygon and CRS
   to EPSG:26910. Add text fields `name`, `source_id`, and `notes`.
3. Select `aoi`, toggle editing, and choose **Add Polygon Feature**. Click around the playable
   course, including teeing areas, all numbered greens and the connecting land between holes.
4. Include practice and clubhouse context if they will be visible in the reconstruction.
   Avoid tight tracing along every tree canopy; this polygon defines data coverage.
5. Right-click to finish the polygon. Enter `Hunter Ranch study area`, identify the imagery
   source, and note that the boundary is inferred from references.
6. Save edits and exit editing mode. Give the layer a transparent fill and a visible outline
   through **Layer Properties > Symbology** so it does not obscure the imagery.

These operations use QGIS's documented [vector editing tools](https://docs.qgis.org/3.44/en/docs/user_manual/working_with_vector/editing_geometry_attributes.html).

## Add surrounding terrain

Open the Processing Toolbox and search **Buffer**. Choose `aoi`, enter **100 metres** as a
starting design margin, enable dissolve if multiple study polygons should form one boundary,
and save the result as a new layer `aoi_context` in the GeoPackage.

The 100 m distance is a workflow default, not a measured Hunter Ranch requirement. Inspect the
tee and approach sightlines against the drone references. Expand the context where nearby
ridges, vegetation or structures must be visible. Far-background scenery may be modelled at
lower detail than the playable terrain, with that simplification recorded.

## Plan the first-hole patch

Once Hole 1 is identified, create a rectangular polygon layer `export_patches` with `patch_id`
and `notes`. The first rectangle should contain the tee, intended route, green and sufficient
surrounding ground that its edges are outside normal shots and views.

Use east/north-aligned edges for a simpler first export. This is a rectangular raster crop,
not a rotated rectangle tightly aligned with the hole. Record bounds in the working CRS.
Later, align the crop to the raster grid in the terrain guide so sample spacing remains consistent.

## Check and revise the boundary

Zoom to each outer hole and verify no tee or green is clipped. Check the driving range and
practice areas separately; they should not be accidentally counted as numbered holes.
Run **Check validity** on the study polygon before using it as a clipping mask. Repair any
self-intersection by editing the original vertices and inspecting the result.

Save a map screenshot for human reference, but keep the GeoPackage as the authoritative
boundary. Proceed to [data acquisition](hunter_ranch_elevation_imagery.md), then return here
if the downloaded imagery reveals missing course context.
