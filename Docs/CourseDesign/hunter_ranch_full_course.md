# Hunter Ranch: expand from Hole 1 to all 18 holes

[Series](hunter_ranch_workflow.md) | Previous: [Environment](hunter_ranch_environment.md) | Next: [Validation](hunter_ranch_validation.md)

**Inputs:** a validated first-hole workflow, course-wide source data and verified routing.
**Outputs:** consistent hole assets, full-course assembly and an integration/validation ledger.
This chapter does not claim the repository currently implements a complete 18-hole round manager.

## Table of contents

- [Freeze the first-hole conventions](#freeze-the-first-hole-conventions)
- [Create the hole inventory](#create-the-hole-inventory)
- [Work in neighbouring groups](#work-in-neighbouring-groups)
- [Manage shared boundaries](#manage-shared-boundaries)
- [Add tee sets and pin candidates](#add-tee-sets-and-pin-candidates)
- [Connect full-round behaviour](#connect-full-round-behaviour)
- [Prepare a reviewable course revision](#prepare-a-reviewable-course-revision)

## Freeze the first-hole conventions

Before copying the process, confirm the coordinate origin, height units, Blender scale, FBX
settings, Unreal placement policy and material mapping. Save a known-good first-hole revision
and its validation results. If those conventions are still changing, multiplying them across
18 holes makes errors harder to diagnose.

Keep source data geographic and engine assets local. Continue to use the same course origin;
do not reset each hole to `(0,0,0)` unless its local offset is explicitly represented in placement.
Use `h01` through `h18` consistently in filenames, markers and validation notes.

## Create the hole inventory

Make `references/hole_inventory.csv`:

```csv
hole_id,identity_verified,par,tee_sets,reference_ids,terrain_revision,layout_revision,mesh_revision,gameplay_status,open_issues
```

Transcribe official routing and scorecard details carefully. Keep practice facilities in a
separate list; do not give them unused numbered-hole IDs. For each hole, identify the terrain
patches it intersects and any shared features such as paths, water or neighbouring rough.

Use explicit statuses such as `reference_pending`, `geometry_ready`, `collision_checked`,
`playable_checked` and `art_reviewed`. These describe your authoring ledger, not an implemented
engine enumeration. A completed mesh should not be marked gameplay-ready without a play check.

## Work in neighbouring groups

1. Choose the next small group based on geographic adjacency and shared terrain, not simply
   the next filename. Keep official hole ordering in metadata regardless of work order.
2. Confirm imagery/elevation coverage, then trace and review its layouts in QGIS.
3. Export matching patches and reuse the accepted modelling process. Preserve per-patch height
   ranges; never apply Hole 1's displacement range to another patch without checking.
4. Bring the group into the full-course test map with the same placement rules.
5. Run dimensional, surface and collision checks before adding dense scenery.
6. Record changes affecting already completed neighbours and repeat their boundary tests.

This grouping reduces duplicate modelling in shared rough and avoids isolated holes whose
surrounding terrain conflicts when assembled.

## Manage shared boundaries

Use a terrain ownership map. Every playable position should have one intended ground collider.
Shared export edges should use identical sampled heights and vertices, or a deliberately built
transition with matching endpoints. Adjacent raster crops need a shared sample row/column;
see the [coordinate guide](hunter_ranch_export_coordinates.md#understand-raster-edges-and-samples).

Avoid smoothing each patch border independently. If a border needs correction, update the
common source or edit both sides from the same boundary data. Inspect joins in wireframe and
with slow ball rolls, including diagonal crossings and patch corners.

Do not assign duplicate colliders to a water edge, a path or rough that belongs to two hole
groups. Visual object separation for organisation does not require duplicate physical ground.

## Add tee sets and pin candidates

Verify each tee set against the scorecard and imagery. Use a marker on the appropriate teeing
area, and preserve the distinction between a published tee yardage and a chosen daily position.
Evaluate tee-to-green sightlines from the actual gameplay camera.

For pins, start with one playable candidate per green. Record its model revision and final
surface height/normal. Test short putts from multiple directions before adding more candidates.
Name pins as authored alternatives unless you possess dated evidence of real cup positions.

A pin table might contain `hole_id`, `pin_id`, `x_cm`, `y_cm`, `z_cm`, `model_revision`, and
`validation_notes`. It remains working documentation until an adapter reads it.

## Connect full-round behaviour

The future course adapter must select a hole, tee set and active pin; position/reset the ball;
register the current cup; start scoring with that hole's par; and move to the next hole only
after the appropriate completion event. It must distinguish scored rounds from practice placement.

Test changes of hole and tee explicitly, including returning to a previous hole, restarting,
and finishing Hole 18. Ensure inactive cups cannot accidentally capture shots. Implement water,
out-of-bounds and drop decisions deliberately rather than assuming visual polygons define rules.
Use the [Unreal integration chapter](hunter_ranch_unreal.md) to identify existing code references.

## Prepare a reviewable course revision

For a review, supply the course revision, tool versions, source ledger, unresolved approximations,
known gameplay limitations, fixed-view screenshots and performance records. State whether the
review covers visuals, geometry, ball behaviour or the complete round.

Preserve editable QGIS and Blender masters, export records and asset provenance. If preparing
game assets for distribution later, use the project's asset packaging conventions and check
referenced materials/textures are included. A local editor preview is not a packaged-build test.
Use the [validation checklist](hunter_ranch_validation.md) as the completion gate.
