# Phase status

## Open-ended Distance Practice, lie-only minimap header, two-line hole heading · 2026-11-23

Distance Practice has no shot limit. The distance can change whenever the ball is at rest, and each
change starts a new hit count. Only the Distance Ladder offers 3 or 5 tries. The minimap header no
longer says In flight or Rolling: it keeps the lie the shot was hit from while the ball is in the air,
then names the surface. In a course round the top-right heading is two tiles tall, with "Hole 1" and
"Par 4 · 400 yd" on separate Bold 18 lines.

On Linux with UE 5.8.4, the Development editor build passed, including a full `-DisableAdaptiveUnity`
build. In one guarded editor with no other editor running:
- Six headless tests passed: `Training.Scoring` (3), `Range.ShotDisplay`, `Minimap.Labels` and
  `Range.TrainingGames`. See `Saved/Automation/20261123-223005-Headless-AD82Fl`.
- Four offscreen Vulkan tests at 1792 × 1087 passed: `Visual.RangeHudControls`,
  `Minimap.RenderedClick`, `Visual.DistanceLadderReplay` and `Visual.TrainingTarget`. Two were marked
  with warnings, which came only from the editor's blocked telemetry upload. See
  `Saved/Automation/20261123-223049-Rendered-2MskVx` and
  `Saved/Screenshots/RangeHud/20261123-223246-course-heading.png`.

Still unverified:
- The DebugGame build.
- Windows.
- Packaged builds.
- The owner's look review.
- A rendered shot from the tee box. The rendered in-flight check started from a rough lie.

## Training game picker and winning radius · 2026-11-23

The Training picker shows both games side by side, with a 3 ft, 6 ft, or 10 ft radius and a
choice of 3 or 5 shots or tries. Both games use the same line-touch rule and show a single ring.

On Linux with Unreal Engine 5.8.4, the Development and DebugGame editor builds passed.
Six Development tests passed in offscreen Play at 1045 × 801, including the three scoring tests,
training gameplay, the HUD controls, and the 35 yd replay. The replay stopped 91.5782 cm from
the cup and advanced to 45 yd after the HUD called it on the line. See
`Saved/Automation/TrainingPicker-Development-5ymrBw/index.json` and the captures
`Saved/Screenshots/RangeHud/training-game-picker.png` and
`Saved/Screenshots/RangeHud/20261123-215608-distance-ladder-10ft.png`.

The DebugGame target test passed separately in Vulkan SM6 at 1792 × 1087 offscreen with one
editor and six stationary frames per view with the ring shown and hidden. Compared with the
earlier two-ring Development capture, the 6 ft far-arc continuity stayed at 97.7%, 95.3%, and
100% at 15, 50, and 150 yd, with zero toggles. Ring-box sparkle went from 0.001% to 0.004% at
15 yd and stayed at 0.000% at 50 and 150 yd. A close view and a 3 cm camera move show one
labelled ring. Continuous camera motion was not measured. See
`Saved/Automation/TrainingPicker-DebugGame-gbKpeK/ring-stability.txt` and the frames in
`Saved/Screenshots/TrainingGames`.

## Distance Ladder line touches · 2026-11-23

The ladder now counts a stopped ball that touches the white 3 ft stripe. On Linux with UE 5.8.4,
both Development and DebugGame editor builds passed. A standalone check of the compiled scoring
function passed at 35 and 150 yd, including exact contact and a 1 mm gap.
All five tests passed in one DebugGame editor with Vulkan rendering. The 45 mph replay at 35 yd
stopped 91.578205834662398 cm from the cup, 1.38 mm past the old cutoff. The HUD showed “on the 3 ft
line” and advanced to 45 yd. The report is in `Saved/Automation/LadderLine-whH67k/index.json` and the
1606 × 906 Play capture is in `Saved/Screenshots/TrainingGames/ladder-35yd-rest.png`.
The original session logged distances rounded to tenths of a yard.

## Vulkan Nanite base-pass assertion · 2026-11-23

One of about 11 Linux Play in Standalone Game launches (UE 5.8.4, DebugGame, Vulkan SM6 with
`VK_EXT_descriptor_buffer` bindless, RTX 3060) failed `ByteOffset + NumBytes <= PackedUniformBuffers.Num()`
(`VulkanResources.h:1105`) on frame 3, in the main view's `Nanite::BasePass`. The run just before it, with
the same map, content and config, played 741 frames. The project has no shader code on this path.

The engine source points to a race. With the default `r.RDG.ParallelExecute=2`, the Nanite shading pass
records in `FRDGAsyncTask` tasks that are not awaited when the graph returns. In the first frames, PSO
precaching triggers static mesh updates that unregister and re-register Nanite shading bins. That mutates
the shared pipeline-group sets those tasks read, without `FRDGBuilder::WaitForAsyncExecuteTask()`. This is
inferred from the source; the interleaving itself was not captured. `Config/Linux/LinuxEngine.ini` sets
`r.RDG.ParallelExecute=1`, which keeps passes parallel but awaits them. It replaces an untested
`r.Nanite.ParallelBasePassBuild=0`, which still leaves the pass as an un-awaited async task.

After the change, 30 of 30 offscreen DebugGame Standalone launches of the public map ran 25 s past their
first frame with no assertion. Each log showed `r.RDG.ParallelExecute = "1"`, bindless Vulkan SM6, the same
first-frame PSO creation (100 by frame 3) and a hole-map capture. At the earlier rate, 30 clean runs would
occur by chance about 6% of the time. That rate rests on one failure, and no offscreen baseline was run.
No other editor ran during the batch. Windowed runs, packaged builds, Windows and the CPU cost of awaiting
the tasks are unmeasured.

## Training games · 2026-11-23

Linux UE 5.8.4 Development and DebugGame editor builds passed. The first rendered run passed 18 tests, including Distance Practice, range HUD controls, automatic tee-up, cup placement, and the minimap. The 26 yd tee capture shows both colored zones, the moved flag and matching minimap distance. The run is in `Saved/Automation/TrainingGamesReview/index.json` and the capture is in `Saved/Screenshots/RangeHud/20261123-175631-distance-practice.png`. The owner’s editor opened during the run, so it provides no controlled timing measurement. The expanded dialog, reset and close-target checks were built after this run. Their results are pending.

## Training ring shimmer, game prompt, HUD tooltips and 5-yard steps · 2026-11-23

The owner saw the 6 ft ring shimmer with the camera still, not while shots were played. From the tee
camera (2.2 m up, 70° FOV) the 5 cm edges were 0.3–0.8 px tall, and as `M_ShotTrail` translucency TSR's
thin-geometry and anti-flicker handling skip them, so each jittered frame drew a different dash pattern.
The edges now use `M_TargetRingLine` ([Range assets](RangeAssets.md)), which keeps them at least 2 px
wide on screen, and `UTargetRingMesh` pads the mesh bounds so the grown edges are not occlusion-culled.

Measured with `Assets/Scripts/ring_stability.py` on `UOpenGolf.Visual.TrainingTarget` (Linux, UE 5.8.4
Development editor, Vulkan SM6, 1792 × 1087 offscreen, one editor; six stationary frames per view, rings
shown and hidden). Toggles are the share of far-arc columns that switch between frames; sparkle counts
ring-box pixels changing by more than 16/255.

| Tee view | Far-arc continuity before → after | Toggles before → after | Ring-box sparkle before → after (rings hidden) |
|---|---|---|---|
| Chip (about 10 yd) | 19.4% → 97.7% | 25.1% → 0.0% | 2.595% → 0.008% (0.025%) |
| 15 yd | 38.7% → 97.7% | 36.2% → 0.0% | 2.613% → 0.001% (0.000%) |
| 50 yd | 24.9% → 95.3% | 29.2% → 0.0% | 2.559% → 0.000% (0.000%) |
| 150 yd | 0.0% → 100.0% | 0.0% → 0.0% | 0.144% → 0.000% (0.012%) |

Before the bounds fix, the grown 150 yd target blinked as a whole (60% toggles, 4.27% sparkle). Frames are
in `Saved/Automation/TrainingRings/{Before,After}-frames`. By eye the near rings are continuous white
ellipses, the 150 yd target is a steady 2 px bar, and the overhead view looks as before. In the test's 3 cm
pan step the lower-middle box changes as much before as after (18.05% and 18.13% of pixels), but that box
holds only the ring's near edge, so it is no ring measurement.

Picking the other training game while one runs opens the drill dialog with a caption ("Exit Drill to start
Distance Ladder, 3 tries."): Exit Drill ends the running game and starts the picked one; Restart Drill or Esc keeps
the running game. The Distance Practice slider moves in 5-yard steps. HUD toolbar: the training games button has no
tooltip, the club button's reads "Change Club" and the record button's "Record Shots" (the recording status stays in
its accessible name; failed saves are logged and turn the icon amber). While placing a ball the pointer is the HUD's
ball icon (a software pointer for the crosshair cursor). In the Development editor (one editor, 1792 × 1087) these
passed: `UOpenGolf.Range.TrainingGames` (prompt, pending choice, ring material, UV rows, padded bounds),
`UOpenGolf.Visual.RangeHudControls` (prompt from the HUD, tooltip texts and absence, ball pointer, 5-yard step),
`UOpenGolf.Training.Scoring`, `UOpenGolf.Visual.Menu` and the five shot-recording tests (`UOpenGolf.Menu.GimmieScoring`
passed earlier with the ring change), and the full `-DisableAdaptiveUnity` build passed. The tests were written before each fix
but could only run after it, so there is no recorded red run. The ball pointer is checked by its mapping, not in a
capture. Pending: the owner's DebugGame rebuild and
look review, a measured slow pan from the tee, packaged rendering and Windows. The flat labels remain
sub-pixel from the tee.

## Training target rings, try counter and Distance Ladder · 2026-11-23

The first Distance Practice zones measured 3 yd and 10 yd, not feet, and sat 0.6 cm above the ball-contact ground.
Cut-turf blades are 15 mm (fairway), 20 mm (collar) and up to 55 mm (rough). Within the grass draw distance they hid
the near half of the target, which showed only as specks between blades (owner screenshot of the 25 yd target from the
tee). The target is now white 3 ft and 6 ft rings with flat "3 ft" and "6 ft" labels, 6 cm above the ground. Each ring
edge widens with distance from the tee (5 cm, or 0.15% of the distance, whichever is larger).

The player picks 3 or 5 tries with the game. Distance Practice plays one set at the chosen yardage. Distance Ladder runs
15, 25 … 145, 150 yd: one ball inside 3 ft moves on, and missing every try moves back one target (not below 15 yd).
Balls are scored where they stop. While a game runs, launch-monitor shots wait for the ball to stop and are refused once
the game is finished.

Linux UE 5.8.4: the Development editor build passed, including a full unity build with `-DisableAdaptiveUnity`.
`UOpenGolf.Training.Scoring` (rings, target set, ladder; 3 tests) passed headless with no other editor running
(`Saved/Automation/20261123-191411-6pOHzy`). The DebugGame build, `UOpenGolf.Range.TrainingGames`, the range HUD controls test and
`UOpenGolf.Visual.TrainingTarget` (now with 15 yd and 150 yd tee views) have not run yet. Before-change captures from
the same test were not taken; the owner screenshot is the only before evidence. Rendered ring and label checks
(visibility over the grass, label orientation, stationary flicker, pan), packaged rendering of the engine text material
and Windows are pending.

## Rough mask behind the green · 2026-11-23

Brown zig-zag steps along the back of the green's fringe came from a stale savanna rough mask. It was baked before the
par-4 hole and left desert under the rough beside the back of the green and the greenside bunker. `RefreshRoughMask.py`
re-baked it from the current outlines in the owner's DebugGame editor (Linux, UE 5.8.4, Vulkan SM6, one editor), and
`BuildParFourHole.py` now re-bakes it too. Details are in
[Turf workflow](TurfWorkflow.md#rough-mask-follows-the-par-four-hole--2026-11-23).

Editor viewport captures show the steps gone. In a slow pan at the back of the green, lower-40% pixels whose second
difference exceeds 16/255 fell from 35.5% to 25.9%. The stationary comparison is inconclusive: editor captures are not a
controlled measurement. A rendered Play run, frame timing and owner acceptance are pending. The 70 OpenGolfSavanna
script tests pass.

## Fringe only around the green · 2026-11-23

The fairway's first-cut band was removed. The fairway now meets the 50 mm rough at its mowing line, and only the green
keeps a fringe, coloured like the old first cut. Details and placement counts are in
[Turf workflow](TurfWorkflow.md#fringe-only-around-the-green--2026-11-23). Terrain, physics and outlines are unchanged.

Rendered run: Linux, UE 5.8.4 DebugGame editor (the owner's, in Play), Vulkan SM6, one editor,
`UOpenGolf.Visual.CoastalTerrain` passed, 1792 × 1087 (`Saved/Automation/CoastalReview/NoFirstCut-20261123-215012`).
The before run is `Cloud-FullAfter-20261121-175057`. It came from a separate offscreen Development editor at
1787 × 1081 and has the same turf apart from the first cut and the collar colour. `UOpenGolf.Range.CoastalTerrain`
passed.

| Stationary view | Lower 40% changed, before → after | Worst 5% band changed |
| --- | --- | --- |
| Tee, grass on | 0.002% → 0.001% | 5.137% → 5.707% |
| Tee, grass off | 0.001% → 0.001% | 4.969% → 5.383% |
| Grass edge, on | 0.003% → 0.001% | 5.090% → 5.502% |
| Grass edge, off | 0.002% → 0.001% | 5.017% → 5.315% |
| Close grass, on | 0.024% → 0.037% | 4.767% → 4.644% |
| Close grass, off | 0.002% → 0.008% | 3.876% → 4.107% |

View 9 pan, lower 40%:
- Pixels whose second difference exceeds 16/255 rose from 3.6% to 9.7%, and the mean second difference from 5.9 to
  7.7/255 (`Saved/Automation/CutTurf/NoFirstCut/pan_flicker.py`).
- The rise is in the rough. The rough now fills the old first-cut band in the view's foreground, so it covers 32% of
  the lower frame, up from 15%.
- The fairway-only third went from 0.9% to 0.7%.

Limits:
- No Play view shows the putting green, so the fringe was checked only in editor-viewport captures, and its
  stationary shimmer is unverified.
- The before and after runs used different editor processes.
- Frame timing was not measured.
- Owner acceptance is pending.

## Course weather · 2026-11-21

The saved range uses the reusable OpenGolfEnvironment actor. The plugin material draws a continuous cloud field from Unreal’s shape and erosion volumes. Coverage zero clears the layer. Speed zero freezes cloud drift. The morning and sunset sun angles and brightness were adjusted for the range’s fixed exposure.

Rendered checks use the saved UOpenGolf map in Play on Linux with Unreal Engine 5.8.4, Vulkan SM6 and an RTX 3060. Each stationary view has 20 frames at 1792 × 1087. The previous material and its replacement use the same tee, grass-edge and close-grass cameras, grass switches and anti-aliasing settings. Both runs use one editor.

The capture folders under `Saved/Automation/CoastalReview` are `Environment-CumulusDay-20261121-081006` (previous clouds) and `Environment-FairWeatherDay-20261121-082413` (revised clouds). The latter contains `Cumulus-comparison.jpg`. Measure the captures with `Assets/Scripts/grass_stability.py <folder> --profile coastal`. Timing between screenshot requests includes capture overhead.

| Stationary view | Lower 40% changed, previous → revised | Worst 5% band changed, previous → revised |
| --- | --- | --- |
| Tee, grass on | 0.001% → 0.002% | 6.338% → 7.136% |
| Tee, grass off | 0.001% → 0.001% | 5.917% → 7.012% |
| Grass edge, on | 0.002% → 0.002% | 6.522% → 6.693% |
| Grass edge, off | 0.002% → 0.002% | 6.452% → 6.489% |
| Close grass, on | 0.027% → 0.027% | 6.009% → 7.353% |
| Close grass, off | 0.003% → 0.003% | 5.421% → 6.337% |

Values are mean percentages of pixels changing by more than 3/255 between successive captures. The fixed canopy boxes include visible sky. Pixels changing by more than 16/255 increased from 0.109% to 0.151% at the tee, 0.082% to 0.085% at the green, 0.108% to 0.184% at the edge and 0.151% to 0.194% at the bunker. These measurements include both intended cloud motion and temporal noise. The slow-moving camera sequence completed and its start, middle and end frames showed continuous cloud banks.

Median elapsed time per editor frame between screenshot requests went from 27.07 to 26.57 ms at the tee, 28.69 to 28.54 ms at the grass edge and 30.83 to 31.08 ms close to the grass, with grass on. Standalone packaged frame rate remains unmeasured.

## Cloud shimmer · 2026-11-21

The clouds above shimmered even with cloud speed zero. `AOpenGolfEnvironment` left the cloud component at Unreal's default of 96 view samples per 15 km, so each ray-march step was about 156 m. In `M_Clouds`, one such step is nearly opaque. Unreal jitters each frame's first sample with blue noise, so a pixel alternated between lit edge, shadowed interior and sky. The actor now sets **View Sample Count Scale** to 4 (about 39 m per step). The material and map are unchanged. `UOpenGolf.Environment.Actor` checks the setting and failed before the change.

Rendered runs: Linux, UE 5.8.4 Development editor, Vulkan SM6, RTX 3060, one offscreen editor at a time (`-RenderOffscreen -ResX=1792 -ResY=1087`, 1787 × 1081 captures), same saved map and anti-aliasing settings. Cloud GPU time comes from `-CoastalCsvProfile -csvGpuStats`. It is the median `GPU/VolumetricCloud` over each view's capture frames. Reports are under `Saved/Automation/20261121-09*-cloud-*`, and captures are in `Saved/Automation/CoastalReview/Cloud-*-20261121-17*`.

Cloud review (`-EnvironmentCloudReview`, speed 0, so any change is noise). Columns give the share of pixels in the top 40% that change by more than 3/255 between captures. Clear sky measured 0.034–0.035% in every run.

| View | Scale 1 (before) | Scale 4 | Scale 8 | Cloud GPU ms, 1 → 4 → 8 |
| --- | --- | --- | --- | --- |
| Tee, day | 0.924% | 0.073% | 0.076% | 0.28 → 0.97 → 1.88 |
| Across range | 1.293% | 0.000% | 0.000% | 0.33 → 1.14 → 2.19 |
| Overhead | 1.439% | 0.002% | 0.000% | 0.09 → 0.25 → 0.45 |
| Morning | 1.131% | 0.121% | 0.128% | 0.35 → 1.21 → 2.33 |
| Sunset | 1.004% | 0.123% | 0.114% | 0.42 → 1.46 → 2.82 |

Scale 8 doubles the cost without steadier clouds. At scale 4, all remaining change in the tee, morning and sunset views is in tree canopy at the bottom of the band. A repeat of scale 1 matched its first run to within 0.2 percentage points. A run with the shipped default (no override) reported scale 4.00 and matched the scale 4 row.

Full `UOpenGolf.Visual.CoastalTerrain` (default cloud drift), scale 1 → 4:

| Stationary view | Lower 40% changed | Worst 5% band changed |
| --- | --- | --- |
| Tee, grass on | 0.002% → 0.002% | 6.988% → 5.137% |
| Tee, grass off | 0.001% → 0.001% | 7.056% → 4.969% |
| Grass edge, on | 0.002% → 0.003% | 5.776% → 5.090% |
| Grass edge, off | 0.002% → 0.002% | 5.833% → 5.017% |
| Close grass, on | 0.023% → 0.024% | 6.806% → 4.767% |
| Close grass, off | 0.002% → 0.002% | 5.963% → 3.876% |

Canopy-box sparkle (> 16/255, boxes include sky) fell from 0.153% to 0.050% at the tee, 0.077% to 0.059% at the green, 0.120% to 0.057% at the edge and 0.184% to 0.095% at the bunker. The tee's top 40% went from 1.089% to 0.080%. The cloud pass went from 0.22–0.28 ms to 0.76–0.98 ms in these views. The slow-moving camera (a 5 cm slide per frame) showed continuous cloud banks with no smearing in the start, middle and end frames. Camera rotation, the DebugGame editor, packaged rendering and Windows remain unchecked.

At scale 1, part of the clouds' fine grain was this sampling noise. At scale 4 the cloud bodies look softer; the shapes are unchanged. Sharper billows would need a material change, which needs its own shimmer check.
