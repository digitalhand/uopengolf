# Coastal terrain delivery

## Trees and trunk colliders — 2026-10-19

The kit can now place trees as well as ground cover. The range map is unchanged:
it has no tree presets or tree areas yet, so nothing new appears in Play.

- **Presets.** `UOpenGolfVegetationPreset::Kind` is `GroundCover` (the default; tilted
  to the slope, no shadows) or `Tree` (upright, casts shadows). Trees set
  `TrunkColliderRadiusM` and `TrunkColliderHeightM` at instance scale 1.
  `MinimumDistanceFromPlayM` and `MaximumDistanceFromPlayM` keep any preset within a
  band measured from the fairway, fringe, green and sand outlines.
- **Areas.** `UOpenGolfTerrainDefinition::VegetationAreas` scatter a preset inside a
  named outline. Unlike the `Vegetation` list, which covers the detailed area, an area
  may extend onto the surrounding Landscape. Its jittered grid starts half a step inside the
  outline's box, so a strip narrower than three spacing steps still gets a row; an area that
  places nothing gets no actors. Placement is the deterministic
  `UOpenGolfTerrainDefinition::Scatter`; the editor builder only turns its points into
  PCG data.
- **Baked actors.** Ground cover is baked as before (`OpenGolf.Terrain.Vegetation`);
  `uopengolf.Grass3D` hides only these. Trees bake into "Coastal trees" actors tagged
  `OpenGolf.Terrain.Trees`, without collision, and are never culled. An area may
  place up to 5,000 trees. Each tree also gets a hidden trunk
  collider: an instance of the engine cylinder in a "Tree colliders" actor tagged
  `OpenGolf.Terrain.TreeColliders` and `OpenFairway.Obstacle`. It blocks Visibility,
  the ball's contact channel, and reaches 0.5 m below the ground so sloped bases leave
  no gap. Colliders come from the placement points, not the visual mesh, so replacing
  the tree art cannot change play.
- **Ball.** A component or actor tagged `OpenFairway.Obstacle` deflects the ball
  sideways at 30% speed; the ball never lands or rests on it. A ball dropping onto a
  flat top leaves outward at 1 m/s or more. Ground probes and terrain ground queries
  look through obstacles. See
  [obstacles](../Plugins/OpenFairwayPhysics/README.md#how-it-uses-unreal-physics).
- **Rebuilding.** `UOpenGolfTerrainBuilder::RebuildVegetation` places everything
  first, then replaces only an area's vegetation, trees and colliders; a placement
  failure leaves the existing vegetation in place. Terrain, tee, targets and the course
  grass layer are kept.
  `Scripts/RebuildRangeVegetation.py` wraps it for the public map: it backs the map up
  to `Saved/TerrainBackups`, bakes and saves. It then deletes generated `Build_*` and
  `Vegetation_*` folders that nothing on disk references (`--keep-generated` keeps them); see
  `Plugins/OpenGolfCourse/Scripts/generated_folders.py`.

Rebuilding vegetation draws the same ground-cover placement as before: the jittered
grid and random sequence are unchanged for presets without a distance band.

## Delivery — 2026-10-19

The existing `/Game/UOpenGolf/Maps/UOpenGolf` range now contains generated coastal
terrain, green, fringe, shallow bunker and 69,364 baked vegetation instances.
The original range was kept at `/Game/UOpenGolf/Maps/RangeFlatRegression` until 2026-10-28,
when the public map became the project's only map; a disk backup stays under `Saved/TerrainBackups`. Shared assets and presets
live in OpenGolfCourse; the range definition and generated terrain live in the game.

Unreal 5.8.3 on Linux passed the definition, saved-map contact and rendered coastal
capture tests. Report: `Saved/Automation/CoastalAfter/index.json`. The contact test
checks ball-sized sphere sweeps, physical surfaces and both sides of the four outer
seams. Height tolerances are 3 cm on detailed playing surfaces and 10 cm on the
coarser rough. This is not a full gameplay or Windows regression result.

Review frames are in `Saved/Automation/CoastalReview/After`: view 0 tee, 1 tee without
vegetation, 2 overview, 3 green, 4 close grass, 5 grass edge and 6 bunker.
These are actual Play captures. Their measured size is 1745 × 971; the requested
window size did not establish a 1920 × 1080 Play viewport or an FPS baseline.

`FixCoastalStability.py` applies stable coastal blade shading and a quieter tee
material without regenerating terrain. The generators retain those changes.
The stationary comparison shows reduced flicker, with measurements in Phase status.
Slow camera motion still needs review. AGENTS.md and CLAUDE.md now require shimmer
checks after asset edits; these two instruction files are locally ignored by Git.

### Imported art

Use the existing reusable definitions to evaluate imported grass meshes. Detailed playing
surfaces are static meshes, so a Landscape-only material is not a direct replacement for
every surface. Imported art must be redistributable (see the
[asset licence roadmap](AssetLicenseRoadmap.md)). The current original assets are a working
fallback, not an approved AAA art baseline.

The following section records the earlier framework-only checkpoint; it is superseded
by this delivery.

## Phase 1 checkpoint — 2026-10-19

Paused for review at the user's request. The reusable terrain framework compiles;
the existing range map has not been migrated. There is no new rendered scene to
review yet. The work is a foundation for the requested visual quality baseline.

The OpenGolfCourse plugin now contains terrain, surface and vegetation definitions,
height and surface queries, and an editor module for generating detailed surface
meshes, surrounding Landscape sections and vegetation. Shared kit assets belong in
the plugin; the range's layout definition belongs in the game. The range can use a
terrain actor to place the tee and align the tee box and flag.

Two original grass meshes have been authored as reusable Blender sources in
`OpenGolfCourse/Coastal` under the Blender art root (outside the repository; see
`Plugins/OpenGolfCourse/Scripts/art_root.py`) and exported to FBX under
`Saved/AssetImport/Prepared`. They have not been imported or visually reviewed in
Unreal. These are initial assets, not an approved final art baseline.

Prepared scripts:

- `Plugins/OpenGolfCourse/Scripts/PrepareCoastalGrass.py`: creates and exports the
  original grass sources. Executed successfully.
- `Plugins/OpenGolfCourse/Scripts/BuildTerrainAssets.py`: creates separate coastal
  materials and reusable presets. Not executed.
- `Scripts/UpgradeRangeTerrain.py` (removed 2026-10-28): backed up the range, kept a flat
  regression map, generated the coastal terrain and baked vegetation.

## Evidence and limits

- Unreal Engine 5.8.3, Linux Development editor build succeeded using
  `Scripts/ue.sh build`. Log: `Saved/Logs/Build-editor.log`.
- `UOpenGolf.Course.Terrain.Definition` passed (one test). Report:
  `Saved/Automation/20261019-160853-oOEBzC/index.json`.
- All three new Python scripts passed syntax compilation. Tracked changes passed
  `git diff --check`.
- Terrain generation, collision seams, physical surface selection, vegetation
  baking and map migration have not been exercised in Unreal yet.
- No Windows build, coverage measurement, rendered comparison, packaged check or
  FPS measurement was performed for this terrain work.
- The Unreal build validated the changed `.Build.cs` file. The requested C# solution
  formatting checks are unavailable because this checkout has no `OpenFairway.sln`.

## Next delivery phases

1. **Integrate and review the range:** first exercise generation and ball contact
   in a disposable test scene. Check seams, slopes and surface selection before
   migrating the existing range. Import the assets, finish the surface art, bake
   vegetation and capture tee, green and bunker views. Preserve tee and target
   distances. Update flat-layout tests to use the retained regression map and add
   tests for the contoured range. Review regeneration and failure recovery before
   relying on the migration script.
2. **Establish the quality baseline:** review the rendered range, finish art and
   lighting, validate gameplay and measure 1080p performance with one editor.
   Record Windows/Linux and packaged results separately. The 60 FPS goal remains
   unverified.
3. **Build the practice facility:** reuse the approved kit for additional practice
   areas, then extend toward a showcase hole.

Stop after each delivery for review. Resume with phase 1 integration above only
when the user asks to continue.
