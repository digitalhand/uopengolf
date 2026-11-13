# Hunter Ranch: documentation, glossary and learning order

[Series](hunter_ranch_workflow.md) | Previous: [Troubleshooting](hunter_ranch_troubleshooting.md) | [Overview](course_hunter_ranch.md)

Research date: **13 November 2026**. Use primary manuals for tool behaviour and official course
references for course facts. The companion guides propose an authoring workflow; they do not
establish a tested version combination, exact data coverage or surveyed green contours.

## Table of contents

- [Learn only what the next step needs](#learn-only-what-the-next-step-needs)
- [Course and geographic data](#course-and-geographic-data)
- [QGIS and raster processing](#qgis-and-raster-processing)
- [Blender](#blender)
- [Unreal and local integration](#unreal-and-local-integration)
- [Version and evidence notes](#version-and-evidence-notes)
- [Glossary](#glossary)

## Learn only what the next step needs

1. Learn QGIS layers, CRS display and saving a project. Load one elevation tile and one image.
2. Learn digitising and attribute editing. Trace one green, one bunker and two markers.
3. Learn raster reprojection/cropping and inspect one small rectangular patch.
4. Learn Blender navigation, units, Edit/Object modes, UVs and displacement on that patch.
5. Learn material-slot assignment and mesh export before detailed sculpting.
6. Learn Unreal static-mesh import, collision and physical materials. Prove the 10 m calibration.
7. Study the project's cup/round interfaces when connecting gameplay. A general Unreal tutorial
   cannot describe custom repository integration automatically.
8. Add scenery, performance tuning and optional tools after first-hole correctness is established.

Keep a short learning log with the task attempted, manual page, result and unresolved question.
This turns tutorial viewing into evidence about the actual Hunter Ranch workflow.

## Course and geographic data

| Source | Use |
| --- | --- |
| [Hunter Ranch official site](https://www.hunterranchgolf.com/) | Identity, address and official navigation |
| [Course information](https://www.hunterranchgolf.com/golf-course/information) | Visual character and embedded drone reference |
| [Official scorecard](https://www.hunterranchgolf.com/golf-course/scorecard) | Visually verify tee names, pars and yardages |
| [USGS LidarExplorer](https://www.usgs.gov/tools/lidarexplorer) | Discover actual elevation footprints and metadata |
| [3DEP product descriptions](https://www.usgs.gov/3d-elevation-program/about-3dep-products-services) | Distinguish terrain products and intended use |
| [The National Map downloads](https://www.usgs.gov/the-national-map-data-delivery/gis-data-download) | Data acquisition entry points |
| [USGS NAIP archive](https://www.usgs.gov/centers/eros/science/usgs-eros-archive-aerial-photography-national-agriculture-imagery-program-naip) | Orthophoto background and delivery information |

## QGIS and raster processing

| Reference | Read when |
| --- | --- |
| [QGIS training manual](https://docs.qgis.org/latest/en/docs/training_manual/) | Learning basic project, vector and raster operations |
| [Raster properties](https://docs.qgis.org/3.44/en/docs/user_manual/working_with_raster/raster_properties.html) | Inspecting metadata, bands and display |
| [Raster projections](https://docs.qgis.org/3.44/en/docs/user_manual/processing_algs/gdal/rasterprojections.html) | Reprojecting actual coordinates rather than assigning labels |
| [Raster miscellaneous tools](https://docs.qgis.org/3.44/en/docs/user_manual/processing_algs/gdal/rastermiscellaneous.html) | Building VRT mosaics and inspecting rasters |
| [Raster extraction](https://docs.qgis.org/3.44/en/docs/user_manual/processing_algs/gdal/rasterextraction.html) | Clipping and contour inspection |
| [Vector editing](https://docs.qgis.org/3.44/en/docs/user_manual/working_with_vector/editing_geometry_attributes.html) | Digitising, snapping and boundary repair |
| [GDAL Translate](https://gdal.org/en/stable/programs/gdal_translate.html) | Scaling values into 16-bit exports |
| [GDAL geotransforms](https://gdal.org/en/stable/tutorials/geotransforms_tut.html) | Understanding pixel edges and sample centres |
| [PDAL documentation](https://pdal.io/en/stable/) | Taking the optional point-cloud branch |
| [PDAL expression filter](https://pdal.io/en/stable/stages/filters.expression.html) | Selecting verified ground classifications |
| [PDAL raster writer](https://pdal.io/en/stable/stages/writers.gdal.html) | Gridding selected points and controlling interpolation |

## Blender

The expanded [Hunter Ranch Blender walkthrough](hunter_ranch_blender.md) includes a practice
patch, detailed modelling checkpoints, a [task-based YouTube learning path](hunter_ranch_blender.md#task-based-youtube-learning-path),
and [official documentation by task](hunter_ranch_blender.md#blender-documentation-by-task).

Start with the [Blender manual](https://docs.blender.org/manual/en/latest/) chapters on interface
navigation, meshes, UV editing, modifiers and materials. Match the version selector to the
installed application before following shortcuts or screenshots.

The [Displace modifier reference](https://docs.blender.org/manual/en/5.0/modeling/modifiers/deform/displace.html)
explains the height-image controls. [Knife Project](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/mesh/knife_project.html)
is an optional boundary-construction technique; manual Knife cuts are adequate for a small first pass.
The Blender chapter supplies project-specific coordinate and modelling decisions rather than
assuming a terrain add-on already performs geographic import.

Full Blender manual pages intermittently failed retrieval during this research. Official indexed
results were available for Displace and Knife Project, but UI steps have not been exercised on
the author's machine for this course. Verify controls in the installed version.

## Unreal and local integration

| Reference | Use |
| --- | --- |
| [Root project README](../../README.md) | Engine/toolchain setup and repository expectations |
| [Course kit](../../Plugins/OpenGolfCourse/Docs/Reference.md) | Current materials, cup and play rules |
| [Physics reference](../../Plugins/OpenFairwayPhysics/Docs/Reference.md) | Surface resolution and ball/cup behaviour |
| [Turf workflow](../TurfWorkflow.md) | Existing maintained grass sources and handoff |
| [FBX pipeline](https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-static-mesh-pipeline-in-unreal-engine) | Static mesh and material-slot import |
| [Collision complexity](https://dev.epicgames.com/documentation/en-us/unreal-engine/simple-versus-complex-collision-in-unreal-engine) | Query geometry choices |
| [Landscape technical guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine) | Supported grids and height representation |
| [Landscape materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine) | Layer blending and material setup |
| [Foliage Mode](https://dev.epicgames.com/documentation/en-us/unreal-engine/foliage-mode-in-unreal-engine) | Repeated vegetation placement |
| [Stat commands](https://dev.epicgames.com/documentation/en-us/unreal-engine/stat-commands-in-unreal-engine) | Initial performance measurements |
| [PlantUML sequence syntax](https://plantuml.com/sequence-diagram) | Maintaining the overview diagram |

## Version and evidence notes

QGIS examples refer to 3.44 documentation; Blender references may show 5.0; the local project
README specifies Unreal 5.8.4. This is a documentation reference set, not a validated software
bundle. Epic's unversioned pages can show 5.9, so select the project version where available.

Exact Hunter Ranch elevations, source tile IDs, imagery dates, tee coordinates and pin positions
have not been measured in this documentation task. Read those from acquired metadata and
verified references. The series intentionally leaves them as recorded inputs rather than
inventing plausible-looking location data.

## Glossary

| Term | Meaning in this workflow |
| --- | --- |
| AOI | Area of interest selected for data acquisition/modelling |
| CRS | Coordinate reference system defining horizontal coordinates |
| Vertical datum | Reference surface used for elevations; separate from display units |
| DEM | Gridded elevation data; verify whether it represents ground |
| DSM | Surface model that may include vegetation/buildings |
| LiDAR | Sampled range-derived points; classification and density affect ground reconstruction |
| Orthophoto | Geometrically corrected aerial image suitable for geographic alignment |
| NoData | Missing/invalid raster cell value, not a real terrain elevation |
| GeoTIFF | Raster format carrying geographic metadata |
| GeoPackage | File database for editable geographic layers |
| VRT | Virtual raster referencing other files without embedding their pixels |
| UV | Mesh coordinates used to sample a texture image |
| Height encoding | Mapping physical elevation into integer image values |
| Physical material | Engine material data used to identify ball-contact surface behaviour |
| Collision mesh | Geometry queried by physics, which can differ from rendered geometry |
| Control point | A reference used to check transforms; not necessarily a surveyed point |
| Pin candidate | Authored cup location pending validation, not necessarily a real daily pin |
| Source master | Editable authoritative data retained for repeatable exports |
