# Hunter Ranch: build Hole 1 in Blender

[Series](hunter_ranch_workflow.md) | Previous: [QGIS layout](hunter_ranch_qgis_layout.md) | Next: [Export and coordinates](hunter_ranch_export_coordinates.md)

**Inputs:** `h01_dem_m.tif`, a matching layout overlay, marker coordinates and the crop record.
**Outputs:** an editable `.blend` master, connected golf surfaces and calibration geometry.
This chapter is a proposed manual workflow. It has not been executed on Hunter Ranch data.

**How to use this chapter:** work through one small practice patch first, then repeat with
Hunter Ranch's verified Hole 1 data. Each stage has a concrete checkpoint. Do not attempt to
learn every Blender feature before starting, and do not spend time rendering a finished scene
before checking terrain scale and height reconstruction.

References reviewed: **13 November 2026**. Keyboard instructions assume Blender's default
keymap, left-click selection and a mouse with a middle button. Documentation links use a
named release where possible; choose your installed version in the manual. Videos demonstrate
individual skills, not an independently verified Hunter Ranch pipeline.

## Table of contents

- [Blender concepts you need first](#blender-concepts-you-need-first)
- [Navigation and selection quick reference](#navigation-and-selection-quick-reference)
- [Practice on a small patch](#practice-on-a-small-patch)
- [Suggested work sessions](#suggested-work-sessions)
- [Prepare the height image](#prepare-the-height-image)
- [Set up the scene](#set-up-the-scene)
- [Create a correctly sized grid](#create-a-correctly-sized-grid)
- [Map pixel centres to UVs](#map-pixel-centres-to-uvs)
- [Displace and calibrate the ground](#displace-and-calibrate-the-ground)
- [Apply the layout reference](#apply-the-layout-reference)
- [Create surface boundaries](#create-surface-boundaries)
- [Refine the green and bunkers](#refine-the-green-and-bunkers)
- [Prepare the mesh for export](#prepare-the-mesh-for-export)
- [Save and check the milestone](#save-and-check-the-milestone)
- [Task-based YouTube learning path](#task-based-youtube-learning-path)
- [Blender documentation by task](#blender-documentation-by-task)
- [Common Blender problems and recovery](#common-blender-problems-and-recovery)

## Blender concepts you need first

Blender's main window contains several **editors**. The 3D Viewport displays geometry; the
Outliner lists objects and collections; Properties contains settings for the selected object,
scene, modifiers and materials. A workspace such as UV Editing or Shading rearranges those
editors for a task. Switching workspaces does not create another copy of your course.

Keyboard commands usually act on the editor beneath the mouse pointer. If `A` selects UVs
instead of terrain vertices, the pointer is over the UV Editor. If an operation seems unavailable,
check the selected object, active editor and mode before searching for another tool.

| Concept | What it means for Hunter Ranch |
| --- | --- |
| Object | The terrain container with its own location, rotation and scale |
| Mesh | The vertices, edges and faces that form the ground inside that object |
| Object Mode | Select or transform the whole terrain, manage modifiers and exports |
| Edit Mode | Select and change terrain vertices, cut boundaries and assign faces |
| Vertex / edge / face | A position / connection / surface polygon; the ball ultimately encounters triangles |
| Modifier | A saved operation, such as height displacement, evaluated on the mesh |
| Applying a modifier | Makes its result part of the editable mesh; preserve a source copy first |
| UV map | Coordinates telling a mesh where to sample the height or reference image |
| Material slot | A named material assignment for selected faces, useful for grass/sand regions |
| Origin | Object pivot; separate from the mesh's visible centre and from the 3D cursor |
| Collection | An organisation group; it does not automatically merge its objects |

Object scale and terrain elevation are different controls. Changing the object's Z scale can
exaggerate every height; changing Displace Strength changes the decoded elevation range. Neither
should be used to make the ground look more dramatic before checking actual source values.

Start with the official [Blender Fundamentals training](https://studio.blender.org/training/blender-2-8-fundamentals/)
for interface vocabulary. Its 2.8 videos use an older interface. Blender Studio also describes
[updated fundamentals training](https://studio.blender.org/blog/updated-blender-fundamentals/);
check the version and access label of a lesson before following it.

## Navigation and selection quick reference

Use these with the pointer over the 3D Viewport. Menu alternatives matter when using a laptop
or a different keymap. Practise them on a disposable scene before editing course data.

| Action | Default input | Hunter Ranch use |
| --- | --- | --- |
| Orbit / pan / zoom | Middle-mouse drag / Shift + middle-mouse drag / mouse wheel | Inspect green shape from several heights |
| Frame the selected object | Numpad decimal, or View > Frame Selected | Recover when the terrain seems lost |
| Top / front / right view | Numpad 7 / 1 / 3, or View > Viewpoint | Compare plan boundaries and vertical profiles |
| Orthographic/perspective | Numpad 5 | Use orthographic top view for geographic projection |
| Switch Object/Edit Mode | Tab, or mode selector | Move the object versus edit its ground geometry |
| Vertex / edge / face selection | Main keyboard 1 / 2 / 3 in mesh Edit Mode | Choose the appropriate selection type |
| Move / rotate / scale | G / R / S; then X, Y or Z to constrain | Make a controlled height edit with G then Z |
| Confirm / cancel an active operation | Enter or left-click / Esc or right-click | Cancel a mistaken move before it becomes an edit |
| Search operators | F3 | Find a tool when its menu location differs |
| Undo / save | Ctrl+Z / Ctrl+S | Recover an edit and retain a known-good revision |
| Open viewport sidebar | N | Inspect Item transforms and View clipping |

The numpad and main keyboard numbers have different jobs. Main-keyboard `3` in Edit Mode
selects faces; Numpad `3` changes the view. In Object Mode, selection shortcuts can have other
meanings. Use the header's selection icons if uncertain.

If you lack a middle mouse button or numpad, start with the viewport navigation gizmo and View
menu. Configure emulation in Preferences only deliberately, then record that keymap choice.
Watch the official [Viewport Navigation video](https://www.youtube.com/watch?v=ILqOWe3zAbk)
and practise framing, orbiting and returning to top view before moving on.

## Practice on a small patch

Create `blender/practice_ground.blend`, separate from the actual course. This exercise is
invented geometry, not Hunter Ranch elevation data.

1. Add a Grid, set its resolution to roughly 21-by-21 vertices and dimensions to 20-by-20 m.
   Apply Scale in Object Mode. Verify the object stays 20 m wide while scale becomes one.
2. Enter Edit Mode. Deselect everything, choose Vertex Select and select a vertex near the centre.
3. Turn on proportional editing with `O`. Press `G`, then `Z`; move the mouse slightly and
   scroll to change the influence radius. Cancel once to see how recovery works.
4. Repeat the move and raise the centre by an illustrative 0.5 m. Inspect in side view, then
   undo. Notice the difference between moving one vertex and a smooth region.
5. Turn proportional editing off. In top view, use the Knife tool to draw a closed boundary
   crossing existing faces. Confirm the cut and inspect the new edges.
6. Select faces inside the boundary and assign a new material named `PracticeGreen`. Assign
   the remaining faces `PracticeRough`; switch to Material Preview to see the colours.
7. Save, close and reopen the file. Confirm geometry and material assignments survived.

**Checkpoint:** you can select an object, edit its faces, cancel an operation and save a
recoverable revision. If this is difficult, use the navigation, Knife and materials videos in
the learning table before opening a large geographic mesh.

## Suggested work sessions

| Session | Work to complete | Save point |
| --- | --- | --- |
| Basics | Practice scene, selection, controlled moves and materials | `practice_ground.blend` |
| Geographic base | Real crop, units, grid, UVs and calibrated displacement | `hunter_ranch_h01_001.blend` |
| Layout | Reference overlay, control markers and surface cuts | `hunter_ranch_h01_002.blend` |
| Golf detail | Green/bunker refinements with an edit log | `hunter_ranch_h01_003.blend` |
| Handoff | Export copy, triangulation, calibration object and FBX trial | `hunter_ranch_h01_004.blend` |

These are milestones rather than time estimates. Stop at each checkpoint and reopen the file
before proceeding if you are still learning external-image paths and modifier behaviour.

## Prepare the height image

The [coordinate guide](hunter_ranch_export_coordinates.md#encode-a-height-image) owns the
height encoding. Read that section before continuing. Use a north-up raster with valid cells,
known minimum/maximum heights and no unexplained NoData. Produce a 16-bit grayscale PNG, not
a screenshot, and keep the Float32 GeoTIFF master.

Before opening Blender, write a one-page input sheet beside the height image:

| Input | Where to obtain it | Why Blender needs it |
| --- | --- | --- |
| Columns, rows and spacing | QGIS raster information | Vertex count and physical dimensions |
| West/east/south/north bounds | Geographic export record | Patch centre and reference overlay placement |
| Valid minimum/maximum height | Full raster statistics excluding NoData | Displacement strength and base elevation |
| Course origin E0/N0/H0 | Shared coordinate record | Position within the entire course |
| Three control points | QGIS marker layer | Detect shifts, mirroring and wrong heights |
| Overlay extent | Map export settings | Confirm the reference image covers exactly the same area |

Open the PNG only to check that it is present and readable. A viewer's contrast or apparent
brightness does not prove it contains correct heights. Do not resave it through a photo editor
that may convert it to 8-bit, apply contrast, or discard supporting files.

For this chapter, `C` is the number of columns, `R` the rows, and `s` the square pixel spacing
in metres. Obtain all three from the actual raster. `Hmin` and `Hmax` are its valid height range.
Use the same course origin `(E0,N0,H0)` for every hole. Do not copy illustrative heights below
as Hunter Ranch measurements.

## Set up the scene

1. Open Blender, choose a General scene and save as `blender/hunter_ranch_h01_001.blend`.
2. In **Scene Properties > Units**, choose Metric, Unit Scale 1 and metres for length display.
   One Blender unit now represents one metre under this workflow.
3. Remove the default cube from the new authoring scene. Create collections named `SOURCE`,
   `WORKING`, `REFERENCE`, and `EXPORT`. Use the Outliner to keep them distinguishable.
4. Keep +X as east, +Y as north and +Z as up within Blender. Use top orthographic view
   with Numpad 7, or **View > Viewpoint > Top** when no numpad is available.
5. In the viewport sidebar's View settings, increase clipping distance if needed to see the
   entire patch. Vanishing geometry at a distant view is not necessarily missing geometry.

### Make collections and save a clean scene

In the Outliner, right-click to create a collection and rename it. Repeat for the four named
collections. Select an object in the viewport, press `M`, and choose its destination collection.
Moving an object into `SOURCE` organises it; it does not automatically protect its geometry.
Use the Outliner filter/restriction controls to expose selection and render visibility controls
where needed, and disable accidental selection of the retained source after creating a working copy.

Keep the original source visible until the working copy is ready, then hide it from both the
viewport and renders. Two coincident terrain copies can make inspection flicker. Restore the
source alone when you need to compare edits; do not select both copies and edit them together.

**Checkpoint:** the scene is saved, Metric units are selected, the axes are understood, and
you can identify the selected object by name in the Outliner.

## Create a correctly sized grid

1. Use **Add > Mesh > Grid**. Open **Adjust Last Operation** immediately, before another action
   replaces the panel. Set the X/Y grid resolution to match the desired sample counts.
2. Check the resulting vertex counts in Edit Mode; grid resolution labels can vary by version.
   For exact sample matching, you need `C` vertices across X and `R` across Y.
3. In Object Mode, set the grid's X dimension to `(C-1)*s` and Y dimension to `(R-1)*s` in
   the Item sidebar. Use **Object > Apply > Scale**. Scale should now be `(1,1,1)`.
4. Put its centre at the geographic patch centre relative to `(E0,N0)`. Set Z initially to
   `Hmin-H0`. Keep rotation zero so displacement along Z is unambiguous.
5. Rename it `HR_H01_Terrain_Source` and place it in `SOURCE`.

**Illustrative example:** a 501-by-301 raster at 1 m spacing needs a 500-by-300 m grid between
outer sample centres. It has 150,801 vertices. This is a manageable concept for a patch, but
whole-course grids at very fine spacing grow quickly. Begin coarser for preview if necessary,
then document that the preview does not sample every source pixel.

### Enter dimensions and transforms without guessing

With the grid selected in Object Mode, press `N` and open Item. Enter X and Y under Dimensions;
do not type these lengths into the Scale fields. Blender may accept expressions such as
`(501-1)*1` in a numeric field. Replace those illustrative counts with your actual raster data.

Use **Object > Apply > Scale** or `Ctrl+A > Scale`. Inspect the result: dimensions remain
unchanged and scale becomes one. This does not set the course origin or apply a modifier.
Read [Applying transforms](https://docs.blender.org/manual/en/5.0/scene_layout/object/editing/apply.html)
for the distinction before using All Transforms on an export copy.

For the patch's centre, compute `(west+east)/2-E0` and `(south+north)/2-N0` and enter those
in Location X/Y. Enter `Hmin-H0` in Location Z. Save these calculations in the coordinate
record; they must not depend on where you happened to click in the viewport.

Enable viewport statistics through the Overlays menu if you need to inspect vertex/face totals.
A grid with many more vertices than expected can result from repeated subdivision or an
unintended duplicate. Inspect it before adding another resolution-increasing operation.

**Checkpoint:** the source grid has the intended dimensions, unit scale, zero rotation and
documented location. From above it is rectangular; from the side it is still flat.

If the GUI limits the chosen count, build the grid in smaller matching patches or use a reviewed
Blender Python helper later. Do not silently change dimensions to fit a convenient square grid.

## Map pixel centres to UVs

A texture coordinate `(0,0)` is an image edge, whereas the first height sample is half a pixel
inside. Use a UV map that maps the west/east grid edges to `0.5/C` and `1-0.5/C`, and the
south/north edges to `0.5/R` and `1-0.5/R`.

1. In Edit Mode select the grid. Use the UV Editing workspace and create a planar top-view UV
   projection. Before displacement, return to top orthographic view and use **UV > Project
   from View (Bounds)**, or search for that operation with F3. Inspect the operator's Scale
   to Bounds and image-aspect options. Normalize its rectangular island to the full unit square
   first: the geometry's proportions are already represented by its physical X/Y dimensions.
2. In the UV Editor, verify the initial corners are `(0,0)`, `(1,0)`, `(1,1)` and `(0,1)`.
   Select corner UVs and inspect their coordinates if uncertain; projection options may preserve
   aspect ratio rather than automatically filling the square.
3. Set the UV pivot to the 2D cursor and place that cursor at `(0.5,0.5)`.
4. Select all UVs, scale X by `(C-1)/C`, then Y by `(R-1)/R`. This puts the island on sample centres.
5. Name the UV map `TerrainUV`. Confirm that increasing U goes east and increasing V goes north.
   Image loading usually presents the top image row at high V, but verify with landmarks.

For the illustrative 501-by-301 raster, the west UV is approximately 0.000998004 and the
south UV approximately 0.001661130. The [GDAL geotransform explanation](https://gdal.org/en/stable/tutorials/geotransforms_tut.html)
documents the underlying raster edge/centre distinction. The UV construction above is the
recommended application of that distinction in this workflow.

### Operate the UV Editor explicitly

In Object Data Properties, find UV Maps and rename the active map `TerrainUV`. In the UV
Editing workspace, keep all mesh faces selected in the 3D Viewport; with the pointer over the
UV Editor press `A` to select their UVs. If no UVs appear, check that you are in Edit Mode and
that faces, rather than an unrelated object, are selected.

The UV Editor's own sidebar contains the 2D cursor coordinates; it is separate from the 3D
cursor in the viewport. Set the cursor to `(0.5,0.5)` and choose 2D Cursor as pivot in the UV
Editor's header. Perform the X and Y scaling there, not in the 3D Viewport. For the illustrative
501-by-301 image, enter `S`, `X`, `500/501`, Enter; then `S`, `Y`, `300/301`, Enter.

Inspect corner UV values afterward. If the island was not initially the unit square, the inset
operation will preserve that earlier error. Return to the projection step rather than adding
another arbitrary scale. Avoid Smart UV Project or Pack Islands for this geographic map:
rearranging islands would destroy the connection between image positions and terrain locations.

The official [UV operators reference](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/uv.html)
explains view projection. Watch [UV Unwrapping for Beginners](https://www.youtube.com/watch?v=qa_1LjeWsJg)
for UV Editor familiarity, then use this chapter's geographic mapping rather than adopting
arbitrary seam placement from a general object-unwrapping exercise.

**Checkpoint:** west/east and south/north map to the documented UV limits. Nothing has changed
the grid's physical dimensions while editing UVs.

## Displace and calibrate the ground

1. Add a **Displace** modifier to the grid. Create a texture datablock and load the height PNG
   as an image texture through Texture Properties.
2. Set coordinates to UV and select `TerrainUV`. Choose direction Z in local space.
3. Set Midlevel to **0**, Strength to **Hmax-Hmin**, and ensure image intensity is treated as
   linear height data. Use Non-Color where available; disable colour-space conversion in the
   legacy texture controls when that path is exposed. Avoid brightness/contrast adjustments.
4. Use a non-repeating image extension for edge behaviour. Sampling beyond the patch must not
   wrap to the opposite side of the course.
5. Check the modifier in side view. The flat grid already sits at `Hmin-H0`, so its maximum
   displaced elevation should be `Hmax-H0` relative to the shared origin.
6. Compare at least three source sample positions with QGIS. Record differences before sculpting.
   If midrange heights are wrong while extremes agree, investigate colour conversion.

### Find the correct texture controls

Select the terrain in Object Mode. In the wrench tab, add Displace and press New beside its
texture field. Open Texture Properties for that texture datablock, choose an image-based
texture type if needed, and load the height image. Return to the modifier to set UV coordinates,
`TerrainUV`, Z direction, strength and midlevel.

This texture belongs to a geometry modifier. An Image Texture node in the material's Shader
Editor is a separate setup. Connecting height data to a shader's Bump input can change lighting
without moving a single exported vertex. The first-hole ground must contain real displaced
geometry because the game needs a contact surface.

Initially inspect the modifier with a plain material in Solid view. In a side view, toggle
the modifier's viewport visibility to compare flat and displaced states. Do not apply it yet.
If nothing moves, check texture assignment, sufficient vertices, strength, and whether the
modifier is enabled. If everything becomes a tall spike, investigate NoData encoding first.

### Measure before making artistic changes

Make a temporary duplicate, apply Displace on that duplicate and inspect a known sample vertex
in Edit Mode. The Item sidebar reports vertex coordinates in the object's local space; with
zero rotation and unit scale, add object location to obtain scene coordinates. To compare
with absolute source height, also add `H0`. Remove or clearly label this inspection duplicate
so it is never exported alongside the terrain.

Use low, middle and high sample values. If exact sample vertices are unavailable because you
chose a coarser preview grid, document the interpolation difference and repeat calibration on
the final grid. The purpose is to identify errors before they are hidden by sculpting.

**Illustrative heights:** with `Hmin=240 m`, `Hmax=280 m` and `H0=200 m`, the grid starts at
Z=40 m and uses Strength=40 m. An intensity of 0.5 gives approximately Z=60 m. These are
arithmetic examples, not measured Hunter Ranch elevations.

The [Blender Displace reference](https://docs.blender.org/manual/en/5.0/modeling/modifiers/deform/displace.html)
describes the texture, direction, strength and midlevel controls. Full manual retrieval was
limited during research, although the official indexed description confirmed the displacement
relationship; check the UI against the manual for your installed version.

## Apply the layout reference

Create a temporary material on the terrain. In the Shader Editor, load the aligned layout
overlay into an Image Texture node and connect it to Base Color. Use `TerrainUV` when the
overlay has the same outer geographic bounds as the height raster; it may have a different
pixel count while representing the same area. Use Material Preview or an unlit inspection
setup to see the outlines clearly.

Place labelled empties or small reference objects at the tee, green centre and three control
points using local coordinates. Keep them in `REFERENCE`. Verify the overlay intersects these
points correctly before cutting mesh boundaries. A visually attractive but shifted overlay
will transfer its errors into every surface boundary.

### Build the temporary image material

1. Select the terrain and open Material Properties. Create a material named `HR_LayoutReference`.
2. In the Shading workspace, identify its Principled BSDF and Material Output nodes. Ensure
   you are editing this terrain material rather than the World shader.
3. Add an Image Texture node, open the overlay, and connect its Color output to Base Color.
   Add a UV Map node, choose `TerrainUV`, and connect its UV output to the image's Vector input.
4. Use Material Preview to inspect it. A reference colour image normally uses its ordinary
   colour interpretation; do not copy height-data settings blindly to every image.
5. If the terrain looks solid grey, check the selected material, node connection, viewport mode
   and image path. If the image repeats, inspect UVs and image extension settings.

Add reference markers through **Add > Empty > Plain Axes** and enter their local coordinates
in the Item panel. Name them `HR_H01_TeeReference`, `HR_H01_GreenCenter` and `HR_Control_A/B/C`.
Adjust their display size for readability. Empties mark positions but do not render a golf tee,
cup or collision shape. They will not become gameplay actors automatically.

**Checkpoint:** all three control markers line up with the same features on the overlay. The
tee and green should appear in the correct relative direction, not just the correct distance apart.

## Create surface boundaries

1. Duplicate the source terrain into `WORKING`; hide the source from viewport/render instead
   of deleting it. Name the working copy `HR_H01_Terrain_Working`.
2. Save a revision, then apply Displace on the working copy in Object Mode. Its elevations are
   now editable mesh geometry. Keep the original modifier stack in the hidden source collection.
3. In top view, cut the green boundary with the Knife tool, following the reference outline.
   Inspect the resulting faces from an oblique view. Cuts interpolate the existing mesh surface;
   they do not reconstruct unseen contours.
4. Repeat for fringe, fairway, tee and bunker edges. For complex outlines, curves and
   [Knife Project](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/mesh/knife_project.html)
   are an optional alternative; confirm the current selection procedure for your Blender version.
5. Keep one connected surface initially. Create material slots `Green`, `Fringe`, `Fairway`,
   `Rough`, and `Sand` in Material Properties. In Face Select mode select each region, select
   its slot, and press **Assign**. Use simple distinctive colours for inspection.
6. Check narrow transition faces and long triangles around cuts. Add supporting edges where
   required; a smooth outline can still contain poor triangulation underneath.

Do not create a second green plane just above the ground as the default solution. It can
produce overlapping collision and visible flicker. If later separating surfaces into objects,
make their boundary positions match and ensure only one surface owns collision at each location.

### Cut one green boundary as a complete exercise

Use `Shift+D` for an independent working duplicate, then right-click to cancel its initial
movement while retaining the duplicate. Avoid `Alt+D` linked duplication when you want to
edit the mesh independently. Move the duplicate to `WORKING`, rename it and hide the source.
Apply Displace from the working object's modifier menu only after saving a revision.

1. Enter Edit Mode and go to top orthographic view. Keep the reference material visible.
2. Press `K` for Knife. Start on an existing edge near the intended green perimeter and click
   a short sequence following the outline. Finish on an existing edge and press Enter.
3. Work in short, reviewable segments. Continue until the green perimeter forms a closed chain.
   A painted line or unfinished Knife preview is not an actual mesh boundary.
4. Inspect in Wireframe or with edges visible. Check that neighbouring cuts meet at vertices,
   rather than ending a small distance apart. Do not select all faces and move them to test a cut.
5. Use Face Select and select the faces inside the boundary. Orbit to confirm no unintended
   faces were selected beyond the green. X-Ray selection can include hidden geometry.
6. Save before cutting another feature. If a cut creates a long sliver or unexpected diagonal,
   undo immediately or repair that local topology while the cause is still clear.

See [Knife topology tool](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/mesh/knife_topology_tool.html)
and [the CG Essentials Knife video](https://www.youtube.com/watch?v=fb3qod9-9DM). A connected
mesh remains connected after a cut: hovering and selecting linked geometry can select the
entire terrain rather than only the green. Material assignments are per-face, not per loose part.

### Assign and verify each golf material

In Material Properties, use the slot-list `+` button to add a slot, then create or select its
material. Give it a stable descriptive name. While the green faces remain selected in Edit
Mode, activate the `Green` slot and click **Assign**. Deselect all faces, then use that slot's
**Select** button to verify exactly which faces received it.

Repeat for Fringe, Fairway, Rough and Sand. Temporarily use bright distinct colours to expose
unassigned strips. Keep one reference-enabled material version or a clearly separated overlay
reference object for checking outlines, but prevent that object from reaching the final export.
If you use a raised reference plane, keep it non-exported and recognise that its Z is cosmetic.

The [material assignment manual](https://docs.blender.org/manual/en/5.0/render/materials/assignment.html)
and [face-material video](https://www.youtube.com/watch?v=My5Zt2V8fG0) show the slot workflow.
Blender names and colours do not create Unreal physical materials; the slot names make that
later assignment easier to audit.

**Checkpoint:** selecting each material slot highlights only its intended region; no duplicate
ground lies beneath it, and adjacent surfaces share continuous geometry.

## Refine the green and bunkers

For the green, preserve broad slopes that are supported by the DEM. Add local subdivisions
where a contour change needs more geometry. With proportional editing, move a small selected
area along Z and control the influence radius; inspect changes from both plan and ball height.
If using sculpt brushes, start with low strength and keep a before/after version.

Use reference photographs to guide transition character, but label unmeasured detail as artistic.
Do not flatten a green solely because its source appears noisy, or add slopes merely to make
putting challenging. Candidate pin areas should be evaluated in the actual game physics.

For each bunker, distinguish the outer grass lip from the sand boundary and floor. Add enough
edge loops to form a transition, lower the floor where supported by references, and keep the
surrounding rim continuous with grass. Check that the ball will encounter a plausible surface
instead of a razor-thin wall. Avoid vertical or overhanging detail until its collision behaviour
has been tested.

In the current course kit, the cup is represented visually and captured by a gameplay rule.
Do not cut a physical hole in the green mesh for the first implementation. The
[Unreal guide](hunter_ranch_unreal.md#place-and-register-the-cup) explains why.

### Make a controlled green adjustment

1. Save a new revision and note the intended change and its evidence. Work on the editable copy.
2. In Vertex Select, choose the centre of the region needing adjustment. Enable proportional
   editing and choose a smooth falloff. Use Connected Only if nearby disconnected geometry
   should be excluded, while remembering that the entire terrain itself is connected.
3. Press `G`, then `Z`. Start with a small numerical move, such as an **illustrative** 0.05 m,
   and adjust the influence radius before confirming. This is a practice magnitude, not a
   prescription for Hunter Ranch's real contours.
4. Inspect from the approach, both sides and putting height. Check that the falloff did not
   move a bunker lip, course boundary or shared patch edge unintentionally.
5. Turn proportional editing off when finished. Otherwise an ordinary later vertex move can
   change a much larger area than intended.

The [proportional-editing manual](https://docs.blender.org/manual/en/5.0/editors/3dview/controls/proportional_editing.html)
and [Ryan King Art demonstration](https://www.youtube.com/watch?v=F7VggbBaCsg) explain the
interaction. Prefer controlled edits while learning. Sculpting is optional; begin on a duplicate
and avoid Dyntopo or voxel remeshing on the accepted terrain until you understand their effects
on topology, UVs, material assignments and patch boundaries.

### Shape a bunker without losing its boundary

Select interior floor vertices while leaving the outer grass transition fixed. Lower or reshape
the interior in small increments only where supported by references, with enough geometry to
form a gradual transition. Add intermediate edges before attempting a sharp lip in a coarse grid.
Avoid selecting just the entire sand region and translating it downward: that can stretch its
boundary into steep walls without a plausible transition.

Inspect the sand/grass join from low angles and in wireframe. Confirm the material boundary still
matches the intended sand edge after editing. Keep any proposed steep face for later collision
evaluation; a screenshot cannot establish how the ball will contact it.

**Checkpoint:** the edit log distinguishes source-derived terrain from inferred refinements,
and all retained source/control geometry remains available for comparison.

## Prepare the mesh for export

Make an export copy after modelling. Inspect face orientation, recalculate normals outward
where needed, and check for duplicates or non-manifold accidents. A terrain patch's outer
border is intentionally open, so distinguish it from an unintended interior crack.

Triangulate the export copy or add a final Triangulate modifier, then inspect changed diagonals
near steep transitions. Smooth shading changes visible normals; it does not smooth the collision
triangles. Evaluate roll on the actual triangulated surface, especially across green boundaries.

Add a separate 10 m calibration bar and asymmetric east/north markers to `REFERENCE`. Export
them separately during calibration. Keep the mesh origin and placement conventions documented;
the export guide explains how to avoid losing course offsets during static-mesh import.

### Inspect normals, triangles and calibration geometry

Enable **Face Orientation** in viewport overlays and inspect the top of the ground. If faces
point down, correct their winding using the normal tools. Recalculate Outside can help on
shaped surfaces, but an open flat grid does not have an unambiguous enclosed outside; inspect
the actual result and use Flip on the appropriate selection when needed. See
[Editing normals](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/mesh/normals.html).

Add a final [Triangulate modifier](https://docs.blender.org/manual/en/5.0/modeling/modifiers/generate/triangulate.html)
to the export copy and inspect its result. Compare the same green with flat and smooth shading:
this helps reveal large planar facets hidden by interpolated lighting. Do not run a large
Merge by Distance threshold over the whole course; it can collapse intentionally close vertices
and narrow surface transitions. Investigate duplicate geometry locally first.

Create the calibration bar with **Add > Mesh > Cube**, enter dimensions X=10 m, Y=0.2 m,
Z=0.2 m and apply Scale. Name it `HR_Calibration_10m`. Add separate labelled east/north markers
at unequal distances so a reflected or rotated import is obvious. Keep these in REFERENCE,
export them in a separate calibration FBX and exclude them from the finished terrain mesh.

Export a trial before adding foliage. Follow the [coordinate and FBX guide](hunter_ranch_export_coordinates.md#export-a-mesh-without-losing-placement)
for the shared-origin policy. Do not change object origins, apply location and add actor offsets
without recording which step owns placement. Watch the FBX video below for the export-dialog
concepts, then use the project's own calibrated unit/axis settings.

**Checkpoint:** the export copy contains the intended terrain only, with stable slots, inspected
triangles, correct upward-facing ground and a separate unit-calibration file.

## Save and check the milestone

Save a new numbered `.blend` revision with source, working, reference and export collections
clearly named. Use **File > External Data** to manage image paths or pack resources where
appropriate; remember packing imagery increases file size and does not change its reuse terms.

Confirm scale, three control points, source-height reconstruction, material assignments and
boundary continuity. Log inferred contour edits. Only then continue to
[export and coordinates](hunter_ranch_export_coordinates.md). Visual modelling completion is
separate from successful ball/cup integration.

Before closing Blender, save a short handoff note with the `.blend` revision, height image,
overlay filename, UV map, height range, object origin, material-slot list and intentional terrain
edits. Use **File > External Data > Report Missing Files** where available to inspect dependencies,
and reopen the file after saving. Keep source downloads even if images are packed into the blend.

| Final check | Evidence to retain |
| --- | --- |
| Input dimensions and height encoding | Coordinate record and raster metadata |
| Three geographic points align | Labelled top-view capture and numerical comparison |
| Low/middle/high heights reconstruct | Sample-value comparison before sculpting |
| Surface selections are correct | Material-colour plan view |
| Green and bunker transitions are continuous | Oblique and wireframe captures |
| Export scale and orientation | 10 m bar and asymmetric direction-marker import |
| Artistic changes are identified | Brief before/after edit log |

## Task-based YouTube learning path

These are **direct video links**, selected for the named skills. Titles and topics were checked
against indexed YouTube pages/descriptions; video playback and the complete instructions were
not independently reviewed end to end. Some direct page fetches were restricted. No invented
chapter timestamps are supplied. Older interfaces remain useful for concepts, but current
manuals and the calibration checks above take precedence over copying old settings.

| Learn this task | YouTube reference | Apply it to Hunter Ranch | Boundary of the recommendation |
| --- | --- | --- | --- |
| Navigate and recover a lost view | [Viewport Navigation — Blender 2.80 Fundamentals](https://www.youtube.com/watch?v=ILqOWe3zAbk), official Blender channel | Frame the terrain, orbit the green, return to top view | Older 2.80 interface; learn controls rather than matching every icon |
| Understand heightmap displacement | [Blender 2.8: height or topographical map with the displacement modifier](https://www.youtube.com/watch?v=5NeYSH1iJK0) | See the relationship between mesh density, image values and terrain | Historical tutorial; use the guide's measured dimensions, linear data and height range |
| Understand UV editing | [UV Unwrapping for Beginners](https://www.youtube.com/watch?v=qa_1LjeWsJg), Ryan King Art | Become comfortable with UV selection, islands and image mapping | General unwrapping; preserve the planar geographic mapping rather than repacking islands |
| Cut mesh boundaries | [How to Use the KNIFE Tool in Blender!](https://www.youtube.com/watch?v=fb3qod9-9DM), The CG Essentials | Cut a green, bunker or fairway boundary into the connected mesh | Cutting changes topology; it does not determine real course contours |
| Assign materials to selected faces | [How to Assign Multiple Materials to Faces in Blender](https://www.youtube.com/watch?v=My5Zt2V8fG0), Eric Polley | Separate Green, Fringe, Fairway, Rough and Sand assignments | Visual materials only; Unreal physics assignment is a later step |
| Adjust terrain with a falloff | [How to Use Proportional Editing in Blender](https://www.youtube.com/watch?v=F7VggbBaCsg), Ryan King Art | Make small controlled green or bunker-transition adjustments | Record inferred changes; a pleasing shape is not survey evidence |
| Understand FBX export/import dialogs | [Export/Import FBX files Blender to Unreal Engine Tutorial — Fix Common Errors](https://www.youtube.com/watch?v=Lq04nFIZy5M) | Learn the handoff and common mesh/UV issues before calibration | Engine/importer versions differ; do not copy arbitrary scale, lighting or collision settings |

**Recommended order:** navigation → small practice patch → height displacement → UV concepts
→ Knife → material assignment → proportional editing → export. Pause each video after one
relevant operation and reproduce that operation in the practice file. Then repeat it on the
saved Hunter Ranch working copy. Watching a whole series without producing a checked intermediate
file is less useful than learning the next required operation.

You do not need character rigging, animation, particle simulation or advanced procedural
materials for the first-hole milestone. For optional sculpting, first study the installed
version's sculpt documentation and practise on a disposable duplicate. The geometry can be
completed using controlled mesh editing before adopting a sculpt-heavy workflow.

## Blender documentation by task

Use these pages alongside the workflow, especially when a video's interface differs. Direct
retrieval of some manual pages was restricted during research; indexed official content was
available for several controls. The links are references to consult, not evidence that this
entire course workflow was executed in those Blender versions.

| Task | Official reference | What to look up |
| --- | --- | --- |
| Learn the interface and basic tools | [Blender Fundamentals](https://studio.blender.org/training/blender-2-8-fundamentals/) and [updated training announcement](https://studio.blender.org/blog/updated-blender-fundamentals/) | Editor vocabulary, selection and modelling basics; check lesson version/access |
| Understand modifier evaluation | [Modifier introduction](https://docs.blender.org/manual/en/3.0/modeling/modifiers/introduction.html) | Evaluated geometry versus applying a modifier; older conceptual reference |
| Set transforms deliberately | [Apply transforms](https://docs.blender.org/manual/en/5.0/scene_layout/object/editing/apply.html) | Applying scale versus location/rotation |
| Project a top-down UV map | [UV operators](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/uv.html) | Project from View and Bounds options |
| Decode the height image | [Displace modifier](https://docs.blender.org/manual/en/5.0/modeling/modifiers/deform/displace.html) | UV coordinates, direction, strength and midlevel |
| Cut boundaries manually | [Knife topology tool](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/mesh/knife_topology_tool.html) | Completing cuts and tool-specific options |
| Project an existing cutter outline | [Knife Project](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/mesh/knife_project.html) | Optional cutter-object workflow and selection requirements |
| Assign golf regions | [Material assignment](https://docs.blender.org/manual/en/5.0/render/materials/assignment.html) | Slots, Assign, Select and shared materials |
| Make controlled contour adjustments | [Proportional editing](https://docs.blender.org/manual/en/5.0/editors/3dview/controls/proportional_editing.html) | Influence radius, falloff and connectivity |
| Correct face direction | [Editing normals](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/mesh/normals.html) | Recalculate versus Flip and open-surface limitations |
| Stabilise exported triangles | [Triangulate modifier](https://docs.blender.org/manual/en/5.0/modeling/modifiers/generate/triangulate.html) | Triangle generation and inspection |
| Learn additional operations | [Blender manual index](https://docs.blender.org/manual/en/latest/) | Find version-matched sculpting, image management and export chapters |

For the receiving engine, use [Epic's static-mesh FBX documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-static-mesh-pipeline-in-unreal-engine)
and the local [Unreal guide](hunter_ranch_unreal.md). Blender material nodes do not necessarily
transfer as equivalent Unreal shaders, and a static mesh does not include the course's tee,
cup registration or scoring logic.

## Common Blender problems and recovery

| Problem | Check first | Recovery |
| --- | --- | --- |
| A shortcut affects the wrong thing | Mouse pointer's editor and current mode | Move into the intended editor; confirm mode in its header |
| Terrain disappears while orbiting | Frame target, viewport clipping, hidden collection | Select it in Outliner, Frame Selected, then inspect clipping |
| Editing changes the source too | Linked duplicate or shared mesh datablock | Restore a source revision; create an independent working copy |
| Displace does nothing | Vertex count, texture assignment, strength and modifier visibility | Test on a small grid; distinguish material bump from real geometry |
| Middle elevations are incorrect | Linear versus colour-transformed height values | Compare a numerical midrange sample before sculpting |
| UVs are absent | Edit Mode, face selection and active UV map | Select faces in the viewport, then select UVs in the UV Editor |
| Image is stretched or shifted | Projection bounds, pixel-centre inset and geographic extent | Return to the recorded mapping; avoid eyeballed compensation |
| Whole terrain moves during a green edit | Object Mode or oversized proportional radius | Undo; enter Edit Mode, inspect selection and reduce influence |
| Green material spreads outside the green | Face selection and active slot | Use the slot's Select button to audit assignment |
| Colours do not show | Solid versus Material Preview, material node setup | Switch view mode and inspect Base Color connections |
| Black/red-facing patches or bad shading | Face orientation, duplicate faces and normals | Inspect and correct local winding; do not assume smooth shading fixes geometry |
| File reopens without images | External paths or moved source files | Relink originals, record paths and save again |
| Export has two terrain layers | SOURCE/WORKING copies included in selection | Export the intended copy only and recheck collision in Unreal |

If a fix changes geographic dimensions or height encoding, repeat the source checks. If it
changes surface topology, repeat boundary and ball-contact checks after import. Continue to
the broader [troubleshooting guide](hunter_ranch_troubleshooting.md) for problems crossing
QGIS, Blender and Unreal.
