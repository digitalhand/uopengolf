# Designing realistic golf courses: research and starting points

Research checked: **13 November 2026**. This guide helps choose an authoring workflow for
real-world recreations and realistic fictional courses. Recommendations are research judgments;
the external tools and import paths described here have not been tested together in this project.

## Table of contents

- [Recommended starting point](#recommended-starting-point)
- [Understand the four jobs](#understand-the-four-jobs)
- [Compare the options](#compare-the-options)
- [Golf-specific tools and examples](#golf-specific-tools-and-examples)
- [What your Godot designer already provides](#what-your-godot-designer-already-provides)
- [Workflow for a real course](#workflow-for-a-real-course)
- [Workflow for a fictional course](#workflow-for-a-fictional-course)
- [What makes the result realistic](#what-makes-the-result-realistic)
- [Keep the design portable](#keep-the-design-portable)
- [Fit with Unreal Open Golf](#fit-with-unreal-open-golf)
- [What Infinite Tees simplifies](#what-infinite-tees-simplifies)
- [A small research exercise](#a-small-research-exercise)
- [Reading list and references](#reading-list-and-references)
- [Glossary](#glossary)

## Recommended starting point

For a concrete real-course walkthrough, see [Recreating Hunter Ranch in Paso Robles](course_hunter_ranch.md), including a step-by-step workflow and PlantUML sequence diagram.

For detailed tool instructions, follow the [17-part Hunter Ranch companion series](hunter_ranch_workflow.md#read-and-work-in-order), covering data acquisition, QGIS, Blender, Unreal, environment work and validation.

**Start by learning the QGIS terrain/layout workflow and reviewing OPCD's Blender course-design
approach.** QGIS is a sensible home for geographic source data and traced course outlines;
Blender provides precise mesh editing and shaping without making a game engine the only place
where the design exists. Use Unreal to evaluate the eventual visual and playable result.

This is a small toolchain to learn, rather than a single golf-specific application. Its advantage
is that you can retain editable sources and standard exports. Its cost is manual handoff and
learning some mapping and modelling concepts. QGIS has a structured [training manual](https://docs.qgis.org/latest/en/docs/training_manual/),
and Blender has an extensive [manual](https://docs.blender.org/manual/en/latest/).

Choose the first branch according to the course:

- **Real recreation:** begin with an existing elevation raster in QGIS. Add raw LiDAR processing
  only if the available raster lacks the coverage or detail you need.
- **Fictional course:** begin with a small Blender terrain and a top-down hole sketch. Add GIS
  tools when you want a real terrain base or geographic references.
- **Fastest preview inside this game:** investigate Unreal Landscape editing directly. It reduces
  handoffs but retains editor dependence; Unreal's documented heightmap import is only the terrain
  part of a course workflow. See [Epic's import guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-and-exporting-landscape-heightmaps-in-unreal-engine).

For a golf-specific workflow, **OPCD is the most directly relevant learning lead found in this
research**. Read its introduction before investing in a new editor. Its public instructions
need compatibility and licence follow-up, described below.

## Understand the four jobs

| Job | What you create | Sensible starting tool |
| --- | --- | --- |
| Prepare terrain | Ground elevations, extent, scale and source records | QGIS; PDAL/GDAL when necessary |
| Design the golf | Fairways, greens, bunkers, water, routing, tees and pin positions | QGIS for tracing; Blender for shapes and contours |
| Build the environment | Materials, vegetation, water appearance and landmarks | Blender and the target engine |
| Make it playable | Collision, surface physics, cup placement, hole metadata and packaging | The target game's tooling |

A visually complete mesh does not automatically contain playable golf information. These jobs
can share source data, but the receiving game must understand what each surface and marker means.

## Compare the options

Learning burden below is a relative assessment for someone who wants to design courses rather
than learn general game development. Licence labels describe the tools; downloaded assets and
geographic datasets have their own terms.

| Option | Best use | Learning burden and documentation | Portability and remaining work |
| --- | --- | --- | --- |
| **QGIS** — GPL-2.0-or-later | Inspect terrain, trace layouts, manage coordinates | Moderate GIS learning; structured exercises | GeoTIFF and vector data travel well; requires modelling/rendering and game integration |
| **Blender** — GPL | Shape greens and bunkers, edit terrain meshes, make props | Moderate modelling learning; extensive manual | Mesh exports are portable; shaders, collision and golf metadata need target-specific setup |
| **GDAL + PDAL** — permissive licences | Repeatable raster and point-cloud preparation | Technical; reference documentation and pipelines | Useful processing foundation; supplies neither a golf layout UI nor a finished environment |
| **Unreal Landscape** — Epic licence | Sculpt, paint and preview in the playback engine | Moderate editor learning; extensive official guides | Heightmaps can be exchanged; materials and game setup remain Unreal-specific |
| **Godot + Terrain3D** — MIT for Terrain3D | Continue the existing editor-based prototype | Familiar if you know Godot; repository links documentation | Height data can be exchanged, while resources/scenes/editor UI remain tied to Godot |
| **OPCD** — licence not established from public overview | Study a golf-specific Blender mesh workflow | Community support; public introduction uses old software versions | Advertises FBX output; Unreal gameplay integration still needs work |
| **Infinite Tees** — proprietary platform | Benchmark an integrated course-building experience | Vendor describes guided map/spline editing | Exports target its Unreal plugin; compatibility with this game is unverified |

Sources: [QGIS repository/licence](https://github.com/qgis/QGIS),
[Blender licence](https://www.blender.org/about/license/),
[GDAL licence](https://gdal.org/en/stable/license.html), [PDAL about/licence](https://pdal.io/en/stable/about.html),
[Terrain3D repository](https://github.com/TokisanGames/Terrain3D), and the golf-specific links below.
QGIS, Blender, GDAL and PDAL have maintained documentation sites suitable as long-term learning
references. Match examples to the installed version; no particular release combination was validated here.

## Golf-specific tools and examples

### Open Platform Course Designer (OPCD)

Read the [OPCD overview](https://zerosandonesgcd.com/open-platform-course-designer/) and
[course-creation introduction](https://zerosandonesgcd.com/opcd-course-creation/).
The authors describe a Blender add-on, an Inkscape/Unity workflow, and FBX output intended to be
usable across engines. Their public getting-started version list dates to March 2021.

**Why study it:** golf surface construction and a complete course-art workflow are closer to
your problem than a general terrain generator. **Before adopting it:** establish the current
download, licence, supported Blender version, and which tutorial steps depend on Unity or GSPro.
The public site directs readers toward its community for help. The word “Open” in its name
does not establish an open-source licence. Treat it as a workflow lead until those details are resolved.

### TGC Designer Tools

[Chad Rockey's TGC Designer Tools](https://github.com/chadrockey/TGC-Designer-Tools)
is an Apache-2.0 project explicitly targeting terrain/course preparation for The Golf Club 2019.
It is worth reading as an example of a golf-specific LiDAR pipeline. It is not an Unreal course
exporter. The repository has source, issues and release history, but this research did not establish
current maintenance or a working installation on this machine. Evaluate its dependencies before
considering reuse; do not assume compatibility with newer golf games.

### GolfForge

[GolfForge](https://github.com/GolfForge/golfforge) is a public AGPL-3.0 golf simulator with
Unreal and course-pipeline directories. Its README describes an early, pre-1.0 project using
LiDAR and OpenStreetMap. This makes it a relevant implementation reference, especially for
studying separation of source data and engine output. Its feature claims were not independently
tested here. Inspect its pipeline documentation and licence before considering code reuse;
public source does not imply compatibility with this repository's MIT code.

### A documented Blender-to-golf example

The [crogine/VGA Golf course-creation guide](https://github.com/fallahn/crogine/wiki/Creating-a-course)
walks through geometry, collision masks, materials, tee/pin data and export. Its scale and art
direction differ from realistic simulation, but it is a useful concrete explanation of what a
playable course needs beyond a terrain mesh. The page was last edited in 2022; its exact Blender
instructions should be treated as historical.

### Another integrated workflow to compare

[Muni Golf's course-building guide](https://munigolfsim.com/docs) describes a desktop layout
studio followed by Unreal sculpting and finishing. Read it alongside Infinite Tees to compare
how integrated tools reduce handoffs. Its public guide alone does not establish an open-source
licence or an export path into this game; it is a product/workflow reference here.

## What your Godot designer already provides

Local inspection of `../open-golf-designer/addons/` found these pieces:

| Existing piece | Research implication |
| --- | --- |
| `GolfCourseProject.cs`, hole and tee definitions | Course metadata and marker concepts already exist |
| `TerrainImportProfile.cs` | Source heightmap, point cloud, boundaries, overlays, GeoJSON, CRS, resolution and scale settings are already represented |
| `GolfCourseDesignDock.cs` | The UI exposes GDAL/PDAL tools and hole/bunker inputs inside the Godot editor |
| `GeoJsonCourseLayoutImporter.cs` | Existing geographic-layout import logic is worth reviewing before defining another layout workflow |
| `CourseExportService.cs` | Writes `course.json`, `course.gd` and `course.tscn`, referencing a shared Godot base scene |

The useful distinction for research is between **the design information** and **the editor/export
integration**. Terrain sources, geographic outlines and hole definitions are concepts you can
retain elsewhere. Godot `Resource` types, `res://` paths, generated scripts and scene inheritance
make the current implementation engine-specific. Even its JSON currently names a Godot scene.

The inspected sibling directory contained an `addons` tree, rather than a complete runnable
project at its root. These observations come from source inspection, not an editor run or an
end-to-end terrain import. No files in that project were changed.

## Workflow for a real course

Use one hole as the research unit. Include nearby terrain so the hole has context.

1. **Choose the source.** For US courses, inspect [USGS LidarExplorer](https://www.usgs.gov/tools/lidarexplorer).
   Look for a suitable ground-elevation DEM before downloading an entire raw point cloud.
   Save acquisition date, resolution, coordinate reference system and vertical units. USGS
   explains its product choices in [About 3DEP Products & Services](https://www.usgs.gov/3d-elevation-program/about-3dep-products-services).
2. **Inspect in QGIS.** Load the elevation raster and permitted reference imagery. Use hillshade,
   contours and measured distances to check the terrain. Pick a local projected CRS in metres;
   document the vertical datum separately. For another country, begin with its national or regional
   mapping agency; [OpenTopography](https://opentopography.org/) is another discovery resource.
3. **Prepare the ground.** Crop with a surrounding margin and resolve missing data. If processing
   raw LiDAR, inspect classification and use ground points for the terrain. PDAL handles point
   processing; GDAL handles raster reprojection/resampling. Avoid smoothing away green contours.
4. **Trace the layout.** Use separate editable layers for surface polygons, hole routing lines,
   and tee/pin points. Distinguish green, collar, fairway, rough, sand and water. These are proposed
   authoring categories, not an implemented interchange contract.
5. **Refine golf features.** Use Blender or the target engine to inspect greens, bunker floors
   and lips at close range. Record inferred contours separately from measured geometry. Reference
   imagery tells you where a green is, but does not prove its putting contours.
6. **Prepare the handoff.** Keep the geographic master and export a heightmap or terrain mesh,
   surface information and markers using the same extent and origin. Follow the receiving tool's
   import requirements; see [portability](#keep-the-design-portable).
7. **Evaluate in game.** Check dimensions, visual alignment, physical surfaces and putting before
   decorating the rest of the course. Save source data so corrections can be repeated.

Expected research outputs: a QGIS project, elevation master, editable layout, source notes and
one candidate engine export. The manual work is feature tracing, terrain repair, detailed shaping
and game setup. Aerial data will not eliminate those steps.

### Reference imagery and map data

Use imagery with terms that permit the intended tracing and redistribution. A publicly viewable
basemap is not automatically a redistributable texture. [OpenStreetMap's copyright page](https://www.openstreetmap.org/copyright)
explains its attribution and ODbL terms; its golf features can help with initial layout where
coverage exists, but should be checked against current references. Keep elevation, imagery,
map data and vegetation licences in the source notes as separate entries.

## Workflow for a fictional course

1. Sketch a single hole from above, with intended tee-to-green distance, landing area and shot choices.
2. Start with a simple terrain mesh in Blender, or a modest real terrain base. Establish metre
   scale and a local origin before shaping it.
3. Shape broad landforms first, then the green complex and hazards. Keep course boundaries
   editable as curves or other source geometry while evaluating shapes.
4. Inspect from the tee, approach landing area and putting height. Adjust sightlines and playable
   slopes before detailed materials and vegetation.
5. Export a small test using the intended heightmap or mesh route. Recheck scale and surface
   alignment after import, then add tees, a pin and physical surface assignments.

Expected outputs: an editable Blender file, a layout sketch, export settings and one playable
test hole. The main learning cost is mesh editing and sculpting. A mountain/erosion generator
can supply background scenery, but golf-specific contours still need deliberate design.

## What makes the result realistic

The following are evaluation recommendations for this project:

- **Ground shape:** elevation data captures broad landforms; greens, collars and bunker edges
  need close inspection. A 1 m raster gives samples spaced roughly a metre apart; resampling it
  to a smaller spacing adds samples without recovering missing survey detail.
- **Putting geometry:** small contour errors affect roll. Inspect slope transitions and possible
  pin locations using the game's actual physics. A fine mesh cannot repair inaccurate source elevations.
- **Surface agreement:** a visible sand or green boundary should agree with collision and the
  surface physics used by the ball. A colour overlay alone does not establish this agreement.
- **Continuity:** adjoining surfaces need matching edges. Overlapping terrain and detail meshes
  can introduce gaps, flicker or conflicting collision.
- **Vegetation and materials:** establish consistent colour and mowing transitions first, then
  add dense detail near the player. Evaluate foliage cost from gameplay cameras.
- **Reference fidelity:** compare terrain silhouettes, hazard positions and distinctive landmarks
  before spending time on tiny props. Record what is surveyed and what is an artistic approximation.

## Keep the design portable

Keep the editable masters alongside reproducible exports. There is no universal golf-course
format established by the tools in this guide.

| Information | Suitable source/exchange choice | What must accompany it |
| --- | --- | --- |
| Geographic ground elevation | GeoTIFF master | CRS, vertical units/datum, extent and NoData policy |
| Engine heightfield | 16-bit heightmap where supported | Elevation range/offset, spacing, orientation and dimensions |
| Geographic layout | GeoPackage master; GeoJSON where appropriate | Feature meaning, hole IDs and coordinate convention |
| Detailed geometry | Blender source; FBX or glTF export | Units, axes, normals, materials and collision intent |
| Surface masks | Raster images or explicit polygons | Same origin/extent as terrain, category definitions |
| Gameplay data | Documented markers and metadata | Tees, pins, hole order, par and intended surface mapping |

Standard GeoJSON uses longitude/latitude coordinates; local metre coordinates require an
explicitly documented conversion rather than silently treating them as standard geographic
GeoJSON. See [RFC 7946](https://www.rfc-editor.org/rfc/rfc7946).

For a future handoff, record the geographic origin, mapping into local coordinates, horizontal
and vertical units, rotation, image orientation and elevation encoding. Unreal normally uses
centimetres, while the inspected Godot profile defaults to one unit per metre. Verify the
conversion with measured landmarks. Materials and golf semantics generally need rebuilding
even when the mesh imports correctly.

## Fit with Unreal Open Golf

This repository already has a [course kit](../../Plugins/OpenGolfCourse/README.md) with terrain,
physical surfaces, cups and hole-play support. Its [reference](../../Plugins/OpenGolfCourse/Docs/Reference.md)
describes material-based surface assignments and scripted hole layout. The
[turf workflow](../TurfWorkflow.md) documents the existing Blender/Unreal grass handoff.

That is useful receiving infrastructure, but it does not establish a ready-made importer for
the Godot designer's output, OPCD exports, or Infinite Tees packages. An integration would need
to align terrain, surface classification, tee/pin placement and course metadata with this game's
systems. Landscape physical-material and collision behavior would need validation if adopting
a new Landscape-based course route.

There are two terrain routes worth evaluating later: **Landscape heightfields** for broad
terrain/sculpting, and **mesh surfaces** for detailed golf geometry. A combination may be useful,
provided collision and boundaries agree. This guide makes no decision to replace the current
terrain implementation.

When reading Epic documentation, select **Unreal 5.8** to match this repository's documented
5.8.4 baseline; unversioned documentation links can default to newer engine versions.

## What Infinite Tees simplifies

The vendor's [course-building page](https://itgolf.io/course-building) describes course search,
USGS data acquisition in the US, uploaded LiDAR elsewhere, boundary selection, spline layout,
optional sculpting and an Unreal package/plugin handoff. It also describes automatic vegetation
and surface setup. These are vendor descriptions, not independently measured results.

The useful lesson is the integration: one layout feeds terrain, surfaces, markers and decoration.
A QGIS/Blender workflow needs explicit handoffs between those jobs. Its “hours” headline should
not be treated as an estimate for reproducing a highly detailed course with general-purpose tools.
The page does not establish an open-source licence or compatibility with Unreal Open Golf.

## A small research exercise

After reading, use these optional exercises to decide what is worth pursuing. They are evaluation
ideas, not work already performed.

| Exercise | Question it answers | Observable result |
| --- | --- | --- |
| Load one real hole in QGIS and trace its green, fairway and a bunker | Is map-based authoring comfortable? | Aligned terrain and editable features with a known distance |
| Shape a fictional green and bunker in Blender | Is mesh authoring manageable? | Editable contours inspected at ground level |
| Review OPCD's current distribution and one beginner lesson | Does it remove enough golf-specific manual work? | Known licence/version requirements and a clear export path |
| Import a small terrain sample into Unreal later | Does the proposed handoff preserve the design? | Correct scale/orientation, markers and surface behavior |

Record time spent on learning versus actual design, documentation gaps, source ownership and
which steps must be repeated after an edit. Those observations will make a later tool or editor
decision more useful than feature lists alone.

## Reading list and references

Read the first three groups in order; use the technical references when a specific issue arises.

### First: see a golf workflow

- [OPCD overview](https://zerosandonesgcd.com/open-platform-course-designer/) — decide whether
  the Blender/community approach matches how you want to work.
- [OPCD course creation](https://zerosandonesgcd.com/opcd-course-creation/) — understand mesh
  export and engine finishing; note the dated version list.
- [crogine course guide](https://github.com/fallahn/crogine/wiki/Creating-a-course) — a concrete
  explanation of geometry, surface masks and gameplay markers.
- [Infinite Tees process](https://itgolf.io/course-building) and [Muni Golf guide](https://munigolfsim.com/docs)
  — compare integrated product workflows with manual handoffs.

### Second: learn geographic preparation

- [QGIS Training Manual](https://docs.qgis.org/latest/en/docs/training_manual/) — begin with
  Modules 2, 5, 6 and 7: layers, vector editing, reprojection and terrain analysis. Its forestry
  section also includes a DEM-from-LiDAR lesson; dependencies may differ by version.
- [USGS LidarExplorer](https://www.usgs.gov/tools/lidarexplorer) — discover US elevation coverage.
- [3DEP product information](https://www.usgs.gov/3d-elevation-program/about-3dep-products-services)
  — understand available source products before downloading.
- [OpenTopography](https://opentopography.org/) — discover additional topographic datasets;
  availability and access terms depend on the dataset.

### Third: learn shaping and engine handoff

- [Blender Manual](https://docs.blender.org/manual/en/latest/) — focus on mesh editing, curves,
  sculpting, materials and import/export. The site did not render through the research browser;
  exact current subsection links were therefore not verified.
- [Epic heightmap import/export](https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-and-exporting-landscape-heightmaps-in-unreal-engine)
  — learn supported inputs and import scale.
- [Epic Landscape Technical Guide](https://dev.epicgames.com/documentation/unreal-engine/landscape-technical-guide-in-unreal-engine)
  — check component sizes, valid resolutions and height precision before exporting.
- [Terrain3D repository](https://github.com/TokisanGames/Terrain3D) — follow its documentation
  and release links when comparing continued Godot authoring with other options.

### Technical references and source examples

- [GDAL gdalwarp](https://gdal.org/en/stable/programs/gdalwarp.html) — raster reprojection,
  cropping and resampling; [GDAL licence](https://gdal.org/en/stable/license.html).
- [PDAL documentation](https://pdal.io/en/stable/) — point-cloud readers, filters and pipelines;
  [about/licence](https://pdal.io/en/stable/about.html).
- [TGC Designer Tools](https://github.com/chadrockey/TGC-Designer-Tools) — golf-specific terrain
  processing reference, tied to TGC 2019.
- [GolfForge](https://github.com/GolfForge/golfforge) — early Unreal course-pipeline reference;
  inspect documentation, releases and AGPL terms before reuse.
- [GeoJSON standard](https://www.rfc-editor.org/rfc/rfc7946) — coordinate semantics for layout exchange.
- [OpenStreetMap terms](https://www.openstreetmap.org/copyright) and
  [Blender licence/output explanation](https://www.blender.org/about/license/) — distinguish
  software terms from source-data and artwork rights.

## Glossary

| Term | Meaning here |
| --- | --- |
| LiDAR / point cloud | Measured 3D samples, often stored as LAS/LAZ |
| DEM / DTM | Gridded elevation / ground terrain model; inspect the product definition |
| DSM | Surface model that can include trees and buildings |
| CRS | Coordinate reference system locating horizontal coordinates |
| Vertical datum | Reference used for elevation values |
| GeoTIFF | Raster format that can retain geographic positioning |
| Heightmap | Image/grid encoding terrain height; needs scale and offset information |
| Surface mask | Labels identifying ground types; rendering and physics must interpret them |
| Spline | Editable curve used to define boundaries or paths |
| NoData | Raster cells without valid measurements |
| Collision | Geometry the ball actually contacts, which may differ from visible detail |
