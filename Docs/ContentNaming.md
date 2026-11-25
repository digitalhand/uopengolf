# Content naming and layout

Every asset, folder and placed actor in the game follows these rules, including content that scripts generate.
They extend Epic's
[recommended asset naming conventions](https://dev.epicgames.com/documentation/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects).

Two checks enforce them:

- `python3 Assets/Scripts/check_content_names.py` checks names and folders on disk. It runs without the editor,
  with the other script tests.
- `UOpenGolf.Content.Naming` runs in the editor. It checks each asset's prefix against its class, makes sure no
  redirector is left behind, and checks the label of every actor in the project's maps.

Generators get their names from shared helpers, so new content follows the rules too:

- `Plugins/OpenGolfCourse/Scripts/asset_naming.py` for Python.
- `OpenGolfAssetNames.h` for the C++ builders.

When you add a generator, use these helpers rather than writing names by hand.

## Asset names

Asset names use the pattern `Prefix_AssetName_Descriptor_Variant`:

- Each part is a PascalCase segment of letters and digits, separated by underscores.
- Variants have two digits: `SM_Lake_01`, `DA_SavannaTree_Near_00`.
- Leave download details out of names: resolution (`2K`), bit depth (`8bit`), file format (`JPG`) and LOD (`LOD0`).
- Never use the words "Draft" or "Coastal". Coastal is the retired first range.
- Never put spaces in a name.

| Asset | Prefix | Asset | Prefix |
| --- | --- | --- | --- |
| Blueprint | `BP_` | Widget Blueprint | `WBP_` |
| Static mesh | `SM_` | Skeletal mesh | `SK_` |
| Material | `M_` | Material instance | `MI_` |
| Material function* | `MF_` | Material parameter collection* | `MPC_` |
| Texture | `T_` | Physical material | `PM_` |
| Level (map)* | `L_` | Data asset, including presets and course definitions* | `DA_` |
| PCG graph* | `PCG_` | Data table | `DT_` |
| Niagara system / emitter | `FXS_` / `FXE_` | Enum / structure | `E_` / `F_` |

\* Epic's table has no entry for these. The prefix is common practice, as in Epic's Lyra sample.

A texture that a material samples as a surface ends in its role:

| Suffix | Role | Suffix | Role |
| --- | --- | --- | --- |
| `_D` | Colour (albedo, diffuse) | `_N` | Normal |
| `_R` | Roughness | `_A` | Opacity (alpha) |
| `_AO` | Ambient occlusion | `_ORM` | Packed occlusion, roughness, metallic |
| `_M` | Metallic | `_H` | Height |
| `_E` | Emissive | `_Mask` | A mask |

UI images and data textures, such as `T_Avatar_Eagle`, `T_SavannaRange_Preview` and `T_CourseSurfaceField`, end in a
descriptive word instead.

Imported third-party files are renamed on import:

- Poly Haven `jacaranda_tree_leaves_nor_gl_2k` becomes `T_Jacaranda_Leaves_N`.
- ambientCG `Rock029_2K-JPG_Color` becomes `T_Rock029_D`.
- Nature Essentials textures are named after the material that uses them: `T_NE_Branch_N`.

Map a new source to its asset name in `SOURCE_ASSETS` in `asset_naming.py`.

### Names that code depends on

Some names are read by code, so renaming them breaks something. Change the code in the same commit, or leave the
name as it is:

| Name | Who reads it |
| --- | --- |
| `PM_<Surface>`, from the `EOpenFairwaySurface` names | `OpenGolfCourse.h` builds the path; the terrain builders and five Python builders use it |
| `DA_<Course>`, for example `DA_SavannaRange` or `DA_HunterRanch` | It is the course's primary asset ID and its `?Course=` travel key. Saved setups and downloads use it |
| `/HunterRanch`, the plugin, content mount and pack ID | `CourseDownloads.cpp` and `package_course.py` |
| `SM_Surface_<NN>` in a terrain build | `UOpenGolfTerrainBuilder::SurfaceMeshIndex` reads the number back |
| `OpenGolf.Terrain.TreeCanopyFor.<tree mesh path>` canopy tag | Canopy rebuilds match on the tree mesh path. Rename a tree mesh and the old canopy actor stays behind |
| `OpenGolf.WoodProxy` metadata on a tree mesh | Holds the path of its collision mesh. `rename_content.py` remaps it |
| `Flagstick` component and actor label, `WindIndicator` inside `WBP_WindIndicator` | `course_hole.py`, `RangePresentationTest`, `UOpenGolfHUD.cpp` |
| Map paths | `TestMaps.h`, `Config/DefaultEngine.ini`, `Config/DefaultGame.ini` and the packaging checks |

## Folders

- **One folder per feature.** Organise content by feature, such as `Trees/Jacaranda`, `CutTurf` or `Grass/Medium`,
  not by asset type. Inside a feature, put each type in its own subfolder: `Materials/`, `Meshes/`, `Textures/` and
  `Presets/`. Shared parents go in `Shared/`.
- **Folder names** are PascalCase, with no spaces.
- **Game content** lives under `/Game/UOpenGolf`:
  - `Maps/` holds the maps.
  - `Courses/` holds the course definitions.
  - `Effects/` holds the overlay materials.
  - `UI/` holds the widgets.
  - `Terrain/` holds the range's terrain.
- **Shared kits** are plugins:
  - `/OpenGolfCourse` holds the course kit.
  - `/OpenGolfSavanna` holds the open-licence savanna art.
  - `/OpenGolfEnvironment` holds the sky and daylight.
- **Downloadable courses** each have their own plugin, mounted under the course's name. See
  [Course vegetation](CourseDesign/course_vegetation.md#where-things-live).
- **Generated content** lives in a `Generated/` folder under the feature that owns it, one folder per build. That way
  nobody edits it by hand, and `generated_folders.py` can remove builds that nothing uses any more.
  - Range ground: `Build_<GUID>/SM_Surface_<NN>`.
  - Vegetation runs: `Vegetation_<GUID>`.
  - Course ground: `<course>/Generated/Tiles/Tile_<x>_<y>_<GUID>/SM_<Surface>_Tile_<x>_<y>`.
  - Lakes: `<course>/Generated/Water/Water_<GUID>/SM_Lake_<NN>`.

  The GUID gives every build a new package. Undo then keeps working, and the build never overwrites a mesh that the
  map has loaded.

Layout of a downloadable course plugin:

```text
Plugins/HunterRanch/Content/
  Course/
    L_HunterRanch                  the course map
    DA_HunterRanch                 course definition (ID, holes, tees, water)
    DA_HunterRanch_Terrain         terrain definition (elevation, regions)
    Generated/Tiles/Tile_<x>_<y>_<GUID>/SM_<Surface>_Tile_<x>_<y>
    Generated/Water/Water_<GUID>/SM_Lake_<NN>
  Vegetation/Trees/BP_Tree_IslandTree02, IslandTree02/{Materials,Meshes,Textures}
```

## Actors

These rules are for a placed actor's label, the name shown in the Outliner.

- **An actor that shows one asset** carries that asset's name or prefix:
  - Ground squares: `SM_Rough_Tile_0_-10`.
  - Lakes: `SM_Lake_01`.
  - The range ground: `SM_Fairway_Ground`.
  - A placed tree: `BP_Tree_IslandTree02_Hole01`.
- **Any other actor** is labelled by kind: `<Kind>_<Descriptor>`, or `<Kind>` alone when there is only one. Examples:
  - `Landscape_West`
  - `TerrainArea_HunterRanch`
  - `RangeSettings`
  - `Environment_Daylight`
  - `PCG_SavannaTree_Near_00`
  - `Trees_SavannaTree_Near_00`
  - `Colliders_SavannaTree_Near_00`
  - `Canopy_Jacaranda`
  - `CourseGrass_TurfFairwayGrass1m`
- **No spaces and no retired words** in any label.
- **Outliner folders group actors by kind:**
  - `Terrain/Rough`, `Terrain/Fairway`, `Terrain/Green`, `Terrain/Sand`
  - `Terrain/Landscape`
  - `Terrain/Trees`
  - `Terrain/Vegetation`
  - `Water`
  - `Range/Lighting`
  - `Range/Targets`

Labels are names for people. Code finds actors by tag or class, and course ground squares by their
`Tile_<x>_<y>` tag. The `Flagstick` label is the one exception.

## Renaming content

Rename assets in the editor or with a script, never in the file system: references then follow and redirectors can
be fixed up. Rename in this order:

1. Update every generator and every path written in code (C++, Python, INI files, tests) in the same change. Search
   with `grep -rn` across `Source`, `Plugins/*/Source`, `Plugins/*/Scripts`, `Assets/Scripts` and `Config`.
2. Rename with `Assets/Scripts/rename_content.py`. It takes a plan from `content_renames.py`, backs up the content,
   renames, fixes up and deletes the redirectors, remaps the `OpenGolf.WoodProxy` metadata, and relabels the range
   map's actors. Run it without `--apply` first to see the plan.
3. Run `check_content_names.py`, the script tests, `UOpenGolf.Content.Naming` and the tests of the features you
   touched.

To rebuild a downloadable course from scratch with `build_course.py`, the course folder must be empty. Before
building:

- Record its hand-placed actors with `course_actors.py --record`. Put them back afterwards with `--restore`.
- Back up the folder to `Saved/CourseDesign/Backups/`.

Author corrections survive the rebuild, because they live in the site's `corrections.json`.

### Running a rename headless

`rename_content.py` runs in a headless editor too. With no other editor open:

```bash
<engine>/Engine/Binaries/Linux/UnrealEditor "$PWD/UOpenGolf.uproject" -nullrhi -unattended -nosound -nosplash -nopause \
  -DisablePlugins=ModelContextProtocol "-ExecutePythonScript=$PWD/Assets/Scripts/rename_content.py --apply --quit"
```

Things the 2026-11-25 run ran into:

- **Map renames can be refused by a confirmation dialog.** A renamed map that some class defaults still refer to
  brings up a prompt, and an unattended editor answers Cancel. On this machine the culprit was the Level Editor's
  remembered cameras, `EditorViews` in `Saved/Config/<Platform>Editor/EditorPerProjectUserSettings.ini`. Point those
  entries at the new map name, keeping a copy of the file, and run again.
- **The engine's redirector fix-up crashes a headless editor**, because it ends with a report dialog. The runner
  uses `UOpenGolfContentTools::FixUpRedirectors` (`unreal.OpenGolfContentTools.fix_up_redirectors`). It does the
  same steps without the dialog: it loads the referencers, rewrites their soft paths, saves them and deletes the
  redirectors.
- **The asset registry may still be scanning** when a startup script runs. Call `wait_for_completion()` before
  planning, or stale entries look like assets that still need renaming.
- **Some deletes don't reach the disk.** The scripted editor on Linux can segfault on exit (see `CLAUDE.local.md`).
  Assets deleted in that session can stay on disk although the log says they were deleted. Check the files
  afterwards; after a registry check that nothing uses them, remove them with the editor closed.
- **Class defaults keep assets loaded.** `SM_Flagstick` is loaded by the flagstick component's class defaults, so its
  old material file survived the rename unchanged. Check every old path once the runner finishes:
  `test_content_renames.py` and `check_content_names.py` both list leftovers.
- **Reruns are safe.** A rerun skips redirectors and moves only what remains. It backs up only when there is
  something to move.

## Risks after the 2026-11-25 rename

| Risk | What could go wrong | What to do |
| --- | --- | --- |
| Git LFS storage | The renamed and resaved assets are new LFS objects, while the old ones stay in history: roughly the size of `Plugins/OpenGolfSavanna/Content`, plus Hunter Ranch's 84 MB, plus the resaved maps. This counts against the LFS quota | Check the repository's LFS storage and quota before pushing (`git lfs status` lists what will upload). Push from one machine, then re-clone or `git lfs pull` on the other |
| Stale DebugGame editor | A DebugGame build from before the change still names course tiles `CourseTile_<x>_<y>` under `Tiles/`, so the Course Designer would mix old and new names. It also loads `M_ShotTrail` from the old `Art/` folder, so the ball trail and training rings lose their material | Rebuild DebugGame before opening the editor or using the Course Designer |
| Course pack and base game out of step | The Hunter Ranch pack refers to base-game assets such as `/OpenGolfCourse/Terrain/Presets/DA_TerrainSurface_Rough`. A pack cooked before the rename, installed on a new base game, loses its ground materials, and the reverse also breaks | Cook and publish the Hunter Ranch pack together with the base game release that contains the rename. Never mix them |
| Paths outside this repository | Your own scripts, the Windows machine's `Saved/Config`, and bookmarks or Most Recently Used lists in either editor still name `/Game/UOpenGolf/Maps/UOpenGolf`, `/HunterRanch/Course/Course` or old asset paths | Update them by hand. An editor that can't find its last map opens an empty one; nothing is lost |
| Saved player data | Course IDs (`DA_SavannaRange`, `DA_HunterRanch`) did not change, so saved round setups should still match. Replays and recordings were not checked for stored map paths | Load an older replay after upgrading |
| Course Designer on an old map | The builder no longer removes `CourseTile_` actors. If a Hunter Ranch map from before the rebuild comes back, from Git history or a backup, editing an outline there would leave duplicate ground | Rebuild a restored older course with `build_course.py` before editing it |
| Tests that failed after the change | `UOpenGolf.Range.WindControls`, `UOpenGolf.Menu.RoundSetup`, `UOpenGolf.Profile.Avatars` and `UOpenGolf.LaunchMonitor.OpenConnect.ReconnectIsolation` failed. None of them uses renamed content, and there was no earlier report to compare against | Investigate them as their own work ([Phase status](PhasesStatus.md)) |
| Generated names in old docs | Dated entries in `PhasesStatus.md` and `TurfWorkflow.md` keep the names used at the time, such as `CoastalReview` and `UOpenGolf.Visual.CoastalTerrain` | Read them as history. The current names are in this guide |
