# Hunter Ranch: tools, folders and version records

[Series](hunter_ranch_workflow.md) | Previous: [References](hunter_ranch_references.md) | Next: [Site boundary](hunter_ranch_site_boundary.md)

**Inputs:** Windows workstation and repository checkout. **Outputs:** an organised authoring
workspace and recorded tool versions. These are instructions for a future setup; creating this
documentation did not install tools or alter the game project.

## Table of contents

- [Install the graphical tools](#install-the-graphical-tools)
- [Create the working folders](#create-the-working-folders)
- [Record versions and settings](#record-versions-and-settings)
- [Optional command-line tools](#optional-command-line-tools)
- [Protect the source data](#protect-the-source-data)
- [Verify the setup](#verify-the-setup)

## Install the graphical tools

1. Obtain QGIS from [qgis.org/download](https://qgis.org/download/). Choose a supported release
   appropriate for your system and record its exact version. The menu examples in these guides
   refer to QGIS 3.44 documentation; use the Processing Toolbox search when menu placement differs.
2. Open QGIS. Enable **View > Panels > Browser**, **Layers**, and **Processing Toolbox** where
   available. Save a blank project once the workspace below exists.
3. Obtain Blender from [blender.org/download](https://www.blender.org/download/). Start with
   its default keymap so shortcuts in the Blender guide have their normal meanings. Record whether
   you use a different keymap or emulate the numpad.
4. Follow the repository's [root README](../../README.md) for Unreal and C++ prerequisites.
   At inspection, it documents Unreal 5.8.4, and `UOpenGolf.uproject` associates with 5.8.
   Open the project using its intended engine rather than accepting an unplanned upgrade.
5. Establish that the existing game opens before importing Hunter Ranch. Any existing startup
   or build failure is a separate problem; keep it out of the course-data diagnosis.

## Create the working folders

Choose a data location with enough space for original rasters, derived copies and exports.
Storage requirements depend on selected coverage; inspect download sizes before committing to
an entire acquisition project. Use this structure outside the documentation folder:

```text
HunterRanch/
  references/
  source/
    elevation/
    imagery/
    lidar/
  gis/
  processed/
    h01/
  blender/
  exports/
    h01/
  validation/
```

Save `gis/hunter_ranch.qgz` as the QGIS project and `gis/hunter_ranch.gpkg` as the vector
database when you create the first layer. QGIS projects reference external raster files;
saving the project does not embed all elevation and imagery data. Prefer relative paths
in **Project > Properties > General** where the installed version exposes that setting.

Keep `source/` unchanged after acquisition. Write all processing outputs under `processed/`.
Use `h01`, not mixed names such as `hole1`, `Hole01` and `first`, throughout the workflow.

## Record versions and settings

Create `references/tool_versions.md`:

```text
QGIS version:
GDAL version reported by QGIS:
Blender version and keymap:
Unreal version:
Repository revision:
Optional PDAL version:
Working CRS: EPSG:26910
Height units: metres after documented conversion
Vertical reference: to be read from source metadata
```

Use QGIS's **Help > About** for bundled library information. Keep screenshots or notes of
Blender FBX and Unreal import settings after calibration succeeds; defaults are not a durable
record. Use the same tested versions through the first-hole milestone.

## Optional command-line tools

The graphical workflow does not require a separately installed GDAL if QGIS supplies its
processing provider. For the optional command examples, use the shell environment supplied by
your GIS distribution so it can locate the correct binaries. A normal PowerShell window may
not have those programs on `PATH`.

Check availability without modifying files:

```powershell
gdalinfo --version
gdal_translate --help
pdal --version
```

If PDAL is absent, consult its [download instructions](https://pdal.io/en/stable/download.html)
only when the LiDAR branch is needed. QGIS's point-cloud display support does not prove a
standalone `pdal` command is installed. Do not install arbitrary Python packages expecting
them to provide every native GIS executable.

The commands in this series assume you run them from the `HunterRanch/` workspace. Paths
inside JSON examples use forward slashes, which avoids Windows backslash escaping mistakes.
Replace explicitly named placeholders before execution.

## Protect the source data

Keep a second copy of geographic masters, GeoPackages, `.blend` files and metadata on separate
storage. Close QGIS before copying an actively edited GeoPackage. Save Blender revisions before
applying modifiers, deleting geometry or restructuring surface boundaries.

Avoid placing multi-gigabyte source downloads into ordinary Git commits. If course assets are
eventually added to the game repository, follow its existing asset and Git LFS practices. The
documentation folder is for instructions and small diagrams, not the full authoring dataset.

## Verify the setup

Open and save the QGIS project, close it, and reopen it. Confirm the chosen folder is writable.
Open Blender and save a test file in `blender/`; confirm your scene units can be set to Metric.
Open the existing Unreal project without importing anything. Record failures at this stage so
they are not later mistaken for terrain-data problems. Continue to [site boundary](hunter_ranch_site_boundary.md).
