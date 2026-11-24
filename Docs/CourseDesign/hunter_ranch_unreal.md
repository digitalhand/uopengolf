# Hunter Ranch: import terrain and connect gameplay in Unreal

[Series](hunter_ranch_workflow.md) | Previous: [Export](hunter_ranch_export_coordinates.md) | Next: [Environment](hunter_ranch_environment.md)

**Inputs:** calibrated FBX, coordinate record, marker table and a working repository checkout.
**Outputs:** a visual first-hole map, verified collision/surfaces, and an explicit gameplay
integration record. The GUI can import the environment; a complete playable course also requires
game-specific wiring. This document does not provide or claim an implemented Hunter Ranch importer.

## Table of contents

- [Understand the existing integration](#understand-the-existing-integration)
- [Create an isolated visual test map](#create-an-isolated-visual-test-map)
- [Import and calibrate the terrain](#import-and-calibrate-the-terrain)
- [Assign visual and physical surfaces](#assign-visual-and-physical-surfaces)
- [Configure collision for the ball](#configure-collision-for-the-ball)
- [Place and register the cup](#place-and-register-the-cup)
- [Connect tee and round flow](#connect-tee-and-round-flow)
- [Validate and reimport](#validate-and-reimport)

## Understand the existing integration

Local inspection on 13 November 2026 found these relevant pieces:

| Existing piece | What it supplies | What it does not establish |
| --- | --- | --- |
| [OpenGolfCourse reference](../../Plugins/OpenGolfCourse/Docs/Reference.md) | Materials, cup/flag components and hole-play rules | An automatic import of this guide's GeoPackage/CSV |
| [Layout helper](../../Plugins/OpenGolfCourse/Scripts/course_hole.py) | Writes outlines/mounds into a procedural terrain definition | Arbitrary GeoTIFF or FBX terrain ingestion |
| [Game mode](../../Source/UOpenGolf/Private/UOpenGolfGameMode.cpp) | Existing range setup, ball creation and cup registration | A general 18-hole course selector |
| [Ball implementation](../../Plugins/OpenFairwayPhysics/Source/OpenFairwayPhysics/Private/OpenFairwayBall.cpp) | Ground/contact queries and physical-surface detection | Automatic recognition of an image's grass/sand colours |

The inspected game mode locates or spawns a range controller and calls its default-layout
setup, then spawns the ball from the range tee. Therefore, simply adding imported terrain and
pressing Play can still start the existing range flow. Treat the visual import and the gameplay
adapter as separate milestones. Do not delete or overwrite the existing range to conceal this.

## Create an isolated visual test map

1. Open the project in its intended Unreal version. Resolve existing build or missing-LFS-asset
   problems before starting the terrain import. Follow the [root README](../../README.md).
2. Create a new test level and save it under a clearly named course folder, for example
   `/Game/Courses/HunterRanch/Maps/L_HunterRanch_H01_Test`. This is a suggested new asset path.
3. Add simple daylight illumination or start from an appropriate empty/default level template.
   Keep lighting comparable between revisions; postpone production art tuning.
4. Use editor viewport inspection initially. If testing Play before game integration, explicitly
   select a neutral test GameMode through World Settings so range auto-creation is not mistaken
   for Hunter Ranch setup. A neutral mode is for visual inspection and provides no golf gameplay.
5. Enable plugin content visibility in the Content Browser so course materials can be located.

## Import and calibrate the terrain

Import the separate calibration object first using the settings recorded in the export guide.
Confirm a 10 m bar measures 1,000 cm and the east/north markers preserve handedness. Capture
the import settings and do not compensate for failed calibration by eye.

Import the terrain FBX as a static mesh. For the first pass, avoid automatic generation of
unrelated materials/textures when the project already has suitable course materials. Preserve
the intended material slots. Keep automatic simplification/Nanite choices out of the first
collision comparison unless you explicitly verify which geometry the ball queries.

Place a shared-origin export at actor location zero, rotation zero and scale one. Inspect the
three source control points and two heights. If you used a patch-local pivot instead, apply its
recorded translation exactly once. Review [Epic's FBX pipeline](https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-static-mesh-pipeline-in-unreal-engine)
for the importer in your engine version; newer engines can expose different import dialogs.

## Assign visual and physical surfaces

Open the static mesh and inspect its material slots. Assign the existing `MI_Green`,
`MI_Fairway` and `MI_Rough` where appropriate. For a different appearance, create course-specific
instances rather than editing shared assets used by the range. Set each material's physical
material to the corresponding course surface, following the kit's reference.

| Authoring label | Starting physical mapping | Additional decision |
| --- | --- | --- |
| Green | Green | Select and record a trial green-speed setting |
| Fringe/collar | Fringe | Keep its physical boundary aligned with the visible edge |
| Fairway | Fairway | Use other variants only with a deliberate surface decision |
| Rough | Rough | Separate appearance from physics parameters |
| Sand | Sand | Verify the returned material on bunker floor and slopes |
| Tee | Explicit chosen turf preset | No dedicated tee preset is assumed by this guide |
| Water | Separate gameplay decision | A water shader is not a penalty/drop implementation |

Avoid a whole-component surface tag when one component contains several golf surfaces; it
can override per-material results. The inspected resolver prioritises a component surface tag,
then physical material, then actor tag, then default surface. See the
[physics surface reference](../../Plugins/OpenFairwayPhysics/Docs/Reference.md#surfaces).

## Configure collision for the ball

In the Static Mesh Editor, inspect **Collision Complexity**. A useful first static-terrain
trial is **Use Complex Collision As Simple** so simple queries can see the triangle surface.
Keep the terrain static, with physics simulation disabled. A single convex hull cannot represent
the hole's hills and depressions. Epic documents the tradeoff in
[Simple versus Complex Collision](https://dev.epicgames.com/documentation/en-us/unreal-engine/simple-versus-complex-collision-in-unreal-engine).

The inspected `ProbeGround` uses a simple query and requests the physical material. Merely
having a detailed render mesh does not ensure that query hits it. Ensure the terrain blocks
the configured ball trace channel; the ball header defaults that channel to Visibility.
Inspect other query/overlap settings as well when adding obstacles or water.

At minimum, check hits in the middle of each material region and immediately either side of
its edge. Inspect the hit position, normal and returned physical material using a small debug
probe, existing debugging facilities, or a future test helper. A colourful mesh is not evidence
that physical surfaces resolve correctly. Such diagnostic tooling is not added by this document.

## Place and register the cup

1. Convert the selected `pin_candidate` X/Y using the shared origin. Trace onto the **final**
   playable surface to obtain its ground position and normal.
2. Use an actor/component setup containing the existing cup component and optionally a flag.
   Use the same sampled surface descriptor for both the visual cup and ball physics.
3. Register the cup after placement. The current interfaces support this relationship:

```cpp
// Integration sketch: Cup and Ball must already be valid objects.
// GroundPositionCm and GroundNormal must come from the final terrain query.
FOpenFairwayCup Surface{GroundPositionCm, GroundNormal};
Cup->PlaceOnSurface(Surface, OptionalFlagstick);
Ball->SetCups({Cup->GetCupDescriptor()});
```

This is an API illustration, not a standalone compilable class or a new editor command.
See [the cup documentation](../../Plugins/OpenGolfCourse/Docs/Reference.md#cup) for lifecycle
details. Registering a new list replaces the previous list; update it when active pins change.

The cup is a visual opening over intact terrain, with capture handled by physics rules.
Do not excavate a tiny mesh hole or add a blocking cylinder for the first setup. Test short
putts from several directions and ensure the registered height/normal matches what is visible.

## Connect tee and round flow

For an actual Hunter Ranch gameplay mode, the adapter must supply the current hole's tee,
pin, par and terrain references to the systems currently driven by the range. It must create
or position the ball, register cups, choose Hole play mode, and advance or reset the round
without rebuilding a procedural range over the imported mesh.

Use the existing game mode and `FOpenGolfHoleRound` as implementation references. The adapter
must record each completed stroke once, distinguish practice placement from scored play, and
handle a holed ball. Later it must select the next hole and tee set. This integration requires
code/Blueprint work beyond GUI asset import and has not been implemented here.

Before that adapter exists, keep the map labelled as a visual/collision prototype. You can
validate terrain and materials without presenting it as a finished playable course.

## Validate and reimport

Use [the validation chapter](hunter_ranch_validation.md) for control points, surface probes,
slow rolls, cups and performance. Save evidence of the first successful configuration. On
reimport, verify material-slot assignments, collision complexity, normals and actor placement.
Do not let an importer-generated material silently replace a physics-aware course material.

Only proceed to detailed [environment work](hunter_ranch_environment.md) after the first-hole
checks pass, or clearly record which gameplay checks remain blocked by missing integration.
