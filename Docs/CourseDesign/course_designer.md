# Course Designer: build a real course in Unreal

This is the current way to recreate a real course. It replaces the manual QGIS and Blender
[series](hunter_ranch_workflow.md) for new courses; that series stays as background and as an
alternative for hand-shaped terrain. Hunter Ranch (Paso Robles, California) is the first course built this way.

## Table of contents

- [What you need](#what-you-need)
- [The files of a course](#the-files-of-a-course)
- [1. Write the site recipe](#1-write-the-site-recipe)
- [2. Prepare the site](#2-prepare-the-site)
- [3. Review the layout on the aerial](#3-review-the-layout-on-the-aerial)
- [4. Build the course map](#4-build-the-course-map)
- [5. Edit and play in the Course Designer](#5-edit-and-play-in-the-course-designer)
- [6. Test](#6-test)
- [7. Package and publish](#7-package-and-publish)
- [How the ground is built](#how-the-ground-is-built)
- [Checklist for the next course](#checklist-for-the-next-course)
- [Hunter Ranch as built](#hunter-ranch-as-built)
- [Limits](#limits)

## What you need

- Unreal Editor for this project (one editor at a time).
- Python 3 outside Unreal with the GDAL bindings (`osgeo`), NumPy and Pillow. The Course Designer runs the
  preparation with `python3`, or with the interpreter named by `OPENGOLF_GIS_PYTHON`.
- Network access the first time a site is prepared. Every download is cached, so later runs work offline.

## The files of a course

| File | Purpose | In Git |
| --- | --- | --- |
| `Assets/Data/CourseDesign/<Course>.json` | Site recipe: sources, origin, scorecard | Yes |
| `Assets/Data/CourseDesign/<Course>/corrections.json` | Author fixes to tees, cups, aim points and outlines | Yes, under ODbL (see its `LICENSE.md`) |
| `Saved/CourseDesign/<Course>/` | Downloads, prepared ground, layout and review images | No |
| `Plugins/<Course>/` | Content plugin (category **Golf courses**) holding the course map and assets | Yes; packaged as a separate download |

## 1. Write the site recipe

Copy `Assets/Data/CourseDesign/HunterRanch.json` and change:

- `crs`, `origin_m` and `sample_bounds_m`: a projected coordinate system in metres, the course origin (east, north,
  height) and the square to sample. Keep the square within 4,000 × 4,000 samples.
- `elevation_url`: a bare-earth elevation tile that covers the whole square. Check its actual footprint.
- `osm_url` and `boundary_way`: an OpenStreetMap extract and the ID of the course's `leisure=golf_course` way.
- `aerial`: an image service for review (optional).
- `hole_count`, `par` and `tee_yards`: from the club's scorecard, one list per tee set, longest set first.
- `corrections`: the corrections file, relative to the recipe.
- `location` and `designer`: where the course is and who designed it, for the round setup's Course information.

## 2. Prepare the site

```bash
python3 Plugins/OpenGolfCourse/Scripts/prepare_course_site.py Assets/Data/CourseDesign/HunterRanch.json Saved/CourseDesign/HunterRanch
```

The Course Designer's **Prepare site data…** button runs the same command. It:

- clips and resamples the elevation to a 1 m grid and writes the Unreal copy to `ground_east_south.oge`
- reads fairways, greens, bunkers, tees, hole routes and lakes from the map data, including multipolygon fairways
- cleans the outlines so none cross. A fairway grows a collar around a green or tee that crosses it and steps back
  from a bunker; a bunker or tee gives way to a green.
- places one cup per green on flat ground at least 4 m from the edge
- places each tee set along the route by its published yardage, then moves it onto nearby level ground
- adds a tee pad for every tee set. A tee you moved in `corrections.json` gets its pad where you put it.
- outlines the play area: the course boundary grown by 25 m and around any surface that reaches past it
- writes `source_layout.json`, plus `route_review.json` for holes whose route was split in the map data

`source_layout.json` keeps the tee and cup positions estimated from the map data. **Save corrections** compares the
course against these estimates, so your corrections stay in `corrections.json` and are never folded into the map data.

For an older prepared site, run **Prepare site data…** once before rebuilding. The map builder
requires the east/south elevation grid. Source outlines and saved corrections use east/north.
See [Course import coordinates](README.md#course-import-coordinates) for the tee-view landmark check.

## 3. Review the layout on the aerial

```bash
python3 Plugins/OpenGolfCourse/Scripts/review_course_layout.py Saved/CourseDesign/HunterRanch
```

This writes one image per hole to `review/` and the problems it finds to `review/checks.json`:

- tees off a tee pad, out of bounds, in water or on slopes of 5 % or more
- cups off the green or near its edge
- routes more than 5 % from the published back-tee yardage

Fix each problem in `corrections.json` (keyed by hole number and map feature ID), add a `reviewed` note that says
what you checked. After correcting a tee, prepare the site again (step 2) so its tee pad moves with it, then run the
review again. Repeat until it is clean. For Hunter Ranch, holes 2 and 10 needed tee corrections on 2026-11-24.

## 4. Build the course map

The Course Designer's **Build course** button builds into an empty content folder, such as `/HunterRanch/Course`.
Without an editor open, run the same script headless:

```bash
OPENGOLF_DESIGN_SOURCE=$PWD/Saved/CourseDesign/HunterRanch OPENGOLF_DESIGN_DEST=/HunterRanch/Course \
  <engine>/Engine/Binaries/Linux/UnrealEditor $PWD/UOpenGolf.uproject -nullrhi -unattended -nosound \
  -DisablePlugins=ModelContextProtocol -ExecutePythonScript=$PWD/Plugins/OpenGolfCourse/Scripts/build_course.py
```

It creates, named by [Content naming](../ContentNaming.md):

- the course map `L_<Course>` (`/HunterRanch/Course/L_HunterRanch`);
- the terrain definition `DA_<Course>_Terrain`;
- the ground squares, `Generated/Tiles/Tile_<x>_<y>_<GUID>/SM_<Surface>_Tile_<x>_<y>`, each actor labelled like its
  mesh and filed under `Terrain/<Surface>` in the Outliner;
- a water surface for each lake, `Generated/Water/Water_<GUID>/SM_Lake_<NN>`, filed under `Water`;
- the range controller (`RangeSettings`) with daylight (`Environment_Daylight`) and flag;
- the course definition `DA_<Course>` with every hole, tee set, cup, boundary and lake. Its name is the course's ID
  in menus, saved setups and downloads, so keep it once the course is published. The definition also gets the round setup's Course information: the site recipe's `location` and `designer`,
and the elevation, which preparing the site measures as the median ground height inside the course boundary. Move an
existing build out of the way first; the builder refuses a folder that already has assets. Record hand-placed actors
before you do, with `course_actors.py --record <file> --map <map>`, and put them back after the build with
`course_actors.py --restore <file>`.

## 5. Edit and play in the Course Designer

Open **Tools → Golf → Course Designer**, enter the course definition (for example
`/HunterRanch/Course/DA_HunterRanch`) and choose **Open course**.

- Pick a hole and tee set. Drag a yellow marker to move the tee, an aim point or the cup, or drag an outline vertex
  to reshape a surface. Moving an outline rebuilds only the ground squares it touches. **Ctrl+Z** undoes an edit.
- Dragging a tee moves only the tee, not its tee pad. Either drag the pad's outline too, or save corrections, prepare
  the site again and build again.
- **Play hole** starts Play on the selected hole and tee set.
- **Save** saves the map, course and terrain, then removes ground squares and lakes that the saved map no longer uses.
- **Save corrections** writes your changes into `corrections.json`, so the next build and the next import keep them.

## 6. Test

```bash
bash Assets/Scripts/ue.sh test --engine-root <engine> --filter UOpenGolf.Course.HunterRanch
```

`HunterRanch.Round` checks every hole and plays a scored round from 1 to 18. `HunterRanch.Water` chips a ball into
the lake and checks the penalty stroke and the replay.

## 7. Package and publish

```bash
python3 Assets/Scripts/package_course.py --engine <engine> --platform Linux --version 1.3 --host Saved/CourseHosting
python3 Assets/Scripts/check_course_install.py Saved/CoursePackaging/Base/Linux/UOpenGolf.sh Saved/CourseHosting Saved/Automation/CourseInstall
```

`package_course.py` cooks the base game and the course as a separate pack. It writes the pack, its SHA-256 hashes
and the derived layout (under ODbL) to the host folder, and updates `catalog.json`. Never reuse a version: a published
version is not overwritten. `check_course_install.py` serves that folder locally and runs the packaged game to check
that the course:

- is absent before install
- downloads, installs and plays without a restart
- plays offline after a restart
- can be uninstalled and is gone at the next start

Players see the catalog in **Courses** when `[CourseDownloads] BaseUrl` points at the host.

## How the ground is built

The ground is built in 64 m squares. Each square is triangulated with every surface outline as a fixed edge, so
surface borders follow the outlines exactly. Neighbouring squares share identical vertices along their common edge,
so the ground and its collision have no gaps.

| Surface | Spacing |
| --- | --- |
| Green, bunker | 0.5 m |
| Tee, fairway | 1 m |
| Rough in the play area | 2 m |
| Rough beyond the play area | 4 m |
| Square edges | 0.5 m |

Heights come from the 1 m elevation grid, so the finer spacing smooths edges and contours rather than adding detail.
Every vertex takes its normal from the same grid, so lighting matches across square and surface edges. The ground is
lit dynamically, so its meshes have no lightmap UVs.

The play area is a rough region named `PlayArea`. It is built, not map data, so **Save corrections** ignores it.
Rough farther out is out of play and mostly seen from a distance, so the coarser spacing there keeps the course
download and its memory smaller.

Each surface of each square is one mesh with its own collision, so a square has one to five meshes. A rebuilt
square or lake goes to a new folder (`Generated/Tiles/Tile_<x>_<y>_<GUID>` or `Generated/Water/Water_<GUID>`), and
**Save** removes the folders the map no longer uses. The `Tile_<x>_<y>` actor tag, not the label, tells a rebuild which
actors to replace.

## Checklist for the next course

Hunter Ranch went through these steps. Follow them in order for the next course; each step names what to check
before moving on.

1. **Pick the data.** Find a bare-earth elevation tile (1 m or finer) that covers the whole course, and check its
   footprint. Check that the map data has the course boundary, a hole route for every hole (`golf=hole` with `ref`),
   and greens, fairways, tees and bunkers. Multipolygon fairways are fine.
2. **Write the recipe** from the club's scorecard. List the tee sets longest first. Check that each set's total
   matches the scorecard.
3. **Prepare the site** and read its summary line. A hole listed under "review notes" or "Joined split routes" needs a
   look in the next step.
4. **Review every hole image**, not only the ones `checks.json` flags. Compare each tee and cup with the aerial.
   Yardage estimates put tees on banks, paths and wooded slopes; a tee box that is not visible on the aerial usually
   shares the next longer set's box. Record each fix in `corrections.json` with a `reviewed` note.
5. **Prepare again after corrections**, so tee pads follow the corrected tees, then review until `checks.json` is clean.
6. **Back up and build.** Back up an existing course folder to `Saved/CourseDesign/Backups/`, empty it, and
   build. Close other editors first: two editors on one project break each other's shader compiles.
7. **Test.** Run `UOpenGolf.Course.HunterRanch` (or a copy of those tests for the new course). They check that every
   tee is on its tee pad, dry and in bounds, that every cup is on its green, and that a full round and a lake
   penalty work.
8. **Look at it.** Capture still views and a slow pan of a few holes (`UOpenGolf.Visual.HunterRanch` does this for
   holes 1, 9 and 12 and saves the frames in `Saved/Automation/HunterReview/`), then walk the holes in Play. Check
   tee pads, green edges, bunkers, water and the horizon.
9. **Package with a new version** and run the install check. Never reuse a published version.
10. **Record it.** Add a dated entry with results and open items to [Phase status](../PhasesStatus.md), and the data
    licences to the notices.

## Hunter Ranch as built

Recorded 2026-11-24 from `Saved/CourseDesign/HunterRanch` and `/HunterRanch/Course`.

| Item | Value |
| --- | --- |
| Holes, tee sets | 18; Blue, White, Combo, Silver, Red |
| Tee-set totals (yards) | 6,751 / 6,292 / 6,035 / 5,781 / 5,424 |
| Map features | 224, giving 228 cleaned surfaces, including 60 tee pads |
| Lakes | 1 on the course; 9 other ponds lie outside the boundary and are left out |
| Author corrections | Hole 2 White tee; hole 10 Silver and Red tees |
| Ground squares | 64 m; 460 squares within two squares of the boundary's bounding box |
| Ground meshes | 722 (one per surface per square); 60 MB, of which rough is 38 MB |
| Course content | 84 MB, mostly the ground squares and the 23 MB elevation grid in `DA_HunterRanch_Terrain` |
| Course information | Paso Robles, California; designed by Ken Hunter Jr. and Mike McGinnis; 263 m (863 ft), the median ground inside the boundary |

Mapped route lengths are within about 4 % of the published yardages. Hole 2's White tee measures about 30 yd longer
than published, because no separate White tee box is visible on the aerial.

## Limits

- Tee and cup positions are estimates from map data, elevation and aerial review, not surveys.
- Green contours come from 1 m lidar. No green is shaped by hand yet.
- The course uses the course kit's coastal surfaces and has no regional rough yet. Its tree is Island Tree 02, a
  stand-in for the native blue oak, with one sample near hole 1. To add trees and grass by hand, see
  [Course vegetation](course_vegetation.md).
- Only Linux packs have been built and tested. Windows packs and hosting on S3 and CloudFront are still to come.
