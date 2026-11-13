# Hunter Ranch: trace the course layout in QGIS

[Series](hunter_ranch_workflow.md) | Previous: [Terrain](hunter_ranch_qgis_terrain.md) | Next: [Blender](hunter_ranch_blender.md)

**Inputs:** aligned imagery, terrain, official references and confirmed Hole 1 identity.
**Outputs:** editable surface polygons, route lines, reference markers and a labelled overlay.
These layers are authoring data; the game does not automatically consume this proposed schema.

## Table of contents

- [Create layers and fields](#create-layers-and-fields)
- [Set up tracing](#set-up-tracing)
- [Trace golf surfaces](#trace-golf-surfaces)
- [Add routing and markers](#add-routing-and-markers)
- [Check geometry and distances](#check-geometry-and-distances)
- [Export a reference overlay](#export-a-reference-overlay)

## Create layers and fields

Use **Layer > Create Layer > New GeoPackage Layer**. Select the existing
`gis/hunter_ranch.gpkg` database and create a new table for each geometry type. Do not overwrite
the database containing `aoi`. Set each new layer's CRS to EPSG:26910.

| Table | Geometry | Fields to add |
| --- | --- | --- |
| `surfaces` | Polygon | `hole_id` integer; `surface`, `source_id`, `confidence`, `notes` text |
| `routes` | LineString | `hole_id`, `par` integer; `tee_name`, `notes` text |
| `markers` | Point | `hole_id` integer; `kind`, `tee_name`, `source_id`, `notes` text |
| `context_areas` | Polygon | `feature`, `source_id`, `notes` text |
| `context_lines` | LineString | `feature`, `source_id`, `notes` text |

Use the GeoPackage's generated feature identifier as a stable row ID. Suggested surface values
are `green`, `fringe`, `fairway`, `rough`, `sand`, `tee`, and `water`; these are authoring labels,
not a promise that each has a corresponding implemented gameplay type.

## Set up tracing

1. Put imagery beneath the vector layers. Style surface polygons with transparent fills or
   partially transparent category colours so boundaries remain visible.
2. Enable the snapping toolbar. Limit snapping to layers whose edges should actually meet;
   snapping to unrelated context features can pull vertices off the intended boundary.
3. Start with a small screen-pixel tolerance, such as 8 px, and test at your working zoom.
   It is an interaction aid, not a claim about ground accuracy.
4. Use topological editing where shared boundaries need to move together. Inspect edits rather
   than assuming the setting automatically repairs all gaps and overlaps.
5. Save edits frequently and keep a GeoPackage backup before large geometry operations.

The official [editing reference](https://docs.qgis.org/3.44/en/docs/user_manual/working_with_vector/editing_geometry_attributes.html)
explains snapping and vertex tools. The suggested layer design and values here are specific to
this authoring workflow.

## Trace golf surfaces

Select `surfaces`, toggle editing and use **Add Polygon Feature**. Start with the Hole 1 green:
place vertices at meaningful changes of direction, finish the polygon, and enter `hole_id=1`
and `surface=green`. Record your imagery source and whether the edge is clear or partly obscured.

Trace the bunker footprint, fairway and teeing areas next. Add a fringe only where references
support its boundary; a plausible strip you create artistically should be labelled as inferred.
Keep shadows, mowing stripes and wet patches separate from actual surface transitions.

Choose a consistent area model: final playable polygons should partition the ground, with
interior holes cut out of containing regions as needed. A green inside a fairway outline is
not two collision surfaces stacked over each other. You may keep broad reference outlines
during tracing, but resolve overlaps before mesh construction or rasterising classification.

For unknown edges under trees, use conservative geometry and an explicit note. Do not add
dozens of vertices around compression artifacts. Match the level of detail to image quality.

## Add routing and markers

1. Select `routes` and draw from the chosen tee through the intended landing route to the green.
   Use the official hole identity and par. Add a distinct route when comparing another tee set.
2. In `markers`, place `tee_reference` at a representative tee position. Record the tee name;
   a large teeing area can support many daily placements.
3. Place `green_center` for geographical reference. Place a separate `pin_candidate` only when
   you intentionally propose a cup position. Do not silently convert the centre into a real pin.
4. Add three separated `control` markers near recognisable locations across the first-hole patch.
   They will verify geographic-to-local alignment; source-derived control is not survey control.
5. Keep water, cart paths and tree groups in context layers until their visual and gameplay
   roles are decided. A water polygon alone does not implement penalty/drop rules.

## Check geometry and distances

Run **Check validity** on surfaces. Inspect self-intersections, tiny slivers and unexpected
multipart shapes. Fix the traced source rather than repeatedly applying automatic repairs
without reviewing changes. Check neighbouring polygon edges at green/fringe and sand/grass joins.

Measure the route in metres and compare against the matching scorecard tee. For a reproducible
projected line length, use a field-calculator expression such as `length($geometry)` on the
metre-based route. Preserve the published yardage in the reference table. Explain discrepancies
before changing geometry: wrong hole, wrong tee, dogleg convention and dated imagery are candidates.

For marker coordinate export, add decimal fields using `x($geometry)` and `y($geometry)`.
Sample DEM elevations for reference with a suitable raster-sampling tool. Re-sample or manually
place gameplay Z after Blender edits; geographic terrain heights may no longer match the model.

## Export a reference overlay

Style surfaces with labelled outlines and show the three control markers. Export a map image
using the first-hole rectangle's exact extent and aspect ratio, without surrounding title or
legend margins. A print layout can help, but its map item's extent—not the paper size—defines
the mapping. Record width, height and pixel dimensions.

Use this image in Blender for visual alignment. For a reproducible geometry handoff, retain
the GeoPackage and a coordinate table as the authoritative sources. The overlay is not a mesh,
surface-physics mask or automatically georeferenced Blender object. Follow the next guide to
set its placement and UV mapping deliberately.
