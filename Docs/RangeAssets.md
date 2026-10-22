# Range assets

## Playing and performance

The public range includes fairway and rough materials, optional short 3D grass, a dimpled
and marked golf ball, a putting green, and a red target flagstick whose cloth flag flies in the wind. Open the project, press Play, and press
**H** for a replay shot. The imported `.uasset` files are included; players need
neither a network connection nor Blender to use them.

Use the Unreal console (**~**) during play:

| Command | Effect |
| --- | --- |
| `uopengolf.Grass3D 0` | Clears all decorative grass instances |
| `uopengolf.Grass3D 1` | Recreates the same deterministic grass population (default) |
| `uopengolf.WindMph 10` | Sets 10 mph wind for the ball and flag; default is 0 (calm) |
| `uopengolf.WindFromDeg 90` | Wind from the right; 0 is headwind, 180 tailwind, 270 from the left |
| `uopengolf.GreenStimp 8`–`15` | Green speed as a stimp reading in feet (default 10), from the next shot |

Grass covers a 20 m radius around the tee, clipped to the 34 m fairway strip and the
range's back edge and excluding the 5 m tee paint. Public clumps contain 16 small triangles, use no collision
or shadows, and use draw-distance bounds of 7 and 10 m. The toggle is checked every 0.25 s.
The fairway texture, mowing bands, and physical ground remain when grass is off.
Grass world-position displacement is disabled.

The flag is simulated cloth: gravity drops it, and the wind flies and flaps it (see the
[kit README](../Plugins/OpenGolfCourse/README.md#flagstick)). Its pole is 213.36 cm above ground,
1.27 cm in diameter, with a 35.56 by 50.8 cm flag. These are conventional flag dimensions, not prescribed
PGA dimensions. The ball is rendered at 42.67 mm diameter, matching the
[USGA minimum](https://www.usga.org/equipment-standards/equipment-rules-2019/equipment-rules/part-4-rule-4.html).
The pole dimensions meet the [flagstick dimensional constraints](https://www.usga.org/content/usga/home-page/equipment-standards/equipment-rules-2019/equipment-rules/part-8-rule-2.html).
The ball carries a dark alignment line and a blue dot; it turns as it rolls along the
ground and re-tees with the line toward the target. It shows no spin in flight.

The fairway is 34 m wide (`AUOpenGolfRange::FairwayHalfWidthM` = 17 m), with rough strips out to
70 m on both sides and surrounding playable rough to 1,100 m from the tee. Fairway, fringe and the former green use `MI_Fairway` and play as
Fairway; the side strips and the backdrop use `MI_Rough`, and the strips play as Rough.
A putting green 20 ft (6.1 m) in radius surrounds the flag (`PuttingGreen`, the kit's
`SM_GreenDisc` with `MI_Green`) and plays as Green. Its top sits 1 mm above the fairway
surround, which has an open center. Its collision hull bevels down to the fairway over its outer 30 cm, so a
rolling ball meets a 0.2 degree ramp rather than a step. Fairway surrounds it, so calibration
shots that finish off the green are unchanged. The green has no mowing stripes; it is a lighter,
turf material with mowing contrast fixed at zero. It plays at stimp 10;
`uopengolf.GreenStimp` changes the speed from the next shot.

Select **Range settings** in the Outliner and open **Details → UOpenGolf → Wind** to edit
**Wind Speed (mph)** and **Wind From (degrees)**. These settings save with the map and
drive the ball and flag during Play. **Preview Wind in Editor** enables cloth animation
before Play when the viewport has Realtime enabled. Default wind is 0 mph, so a hanging
flag at rest is expected. See [Validate wind and flag movement](#validate-wind-and-flag-movement).
The pole mesh contains no flag faces, and the cloth material has no vertex deformation.

Mowing bands run down the target line: nine 3.78 m bands, one centered on the line and
the fairway edges on band boundaries, at half contrast between sRGB **#406B1F** and
**#4D7E20**. The rough is a deeper, coarser green with no bands. The tee uses the turf
material with both palette colors set to #406B1F, so it keeps the turf texture. These are
explicit base colors converted to linear RGB, rather than multipliers on the source
texture; lighting and tone mapping still affect the final on-screen pixels. The
procedural bands get no mip filtering: their edges widen with the on-screen pixel footprint
and their contrast fades to the mean as a band narrows toward the horizon. Without this they
aliased into moire that changed every frame under temporal AA. The bands are world-aligned
and assume the range sits at the world origin with no yaw. The supplied golf photographs
were used as visual references only and are not distributed as game assets.

Both sides of each public blade use upward shading normals, matching the turf's
lighting direction. This reduces the dark blade edges that flicker as Unreal
combines successive frames. The blade material shares the turf's mowing bands and palette,
keeps 0.9 roughness, and explicitly enables instanced static mesh support.

## Validate wind and flag movement

1. Select **Range settings** in the Outliner. In Details, expand **UOpenGolf → Wind**
   or search for `Wind`.
2. Set **Wind Speed (mph)** to **10** and **Wind From (degrees)** to **90** (from the right).
   Keep **Preview Wind in Editor** enabled and turn on viewport **Realtime** with **Ctrl+R**
   if it is off. Select `Flagstick` and press **F** to frame the flag.
3. Wait about 5–10 seconds. One flag should lift downwind and flap, without starting Play.
   Disable the preview checkbox to freeze editor animation; this does not disable wind
   during Play.
4. Set speed to **20** to check that stronger wind holds it farther out. Set direction to
   **270** to check that it turns the other way. Allow time to settle after each change.
   **0** is headwind and **180** is tailwind.
5. Set the wind you want and save the map before Play. Play uses those authored settings
   even if console values remain from an earlier session. Setting speed to **0** restores
   calm air; the cloth falls beside the pole and settles.

During Play, edit the same controls on the Play-world copy of **Range settings**. Those
runtime edits do not save back to the source map. The spawned ball also exposes
**OpenFairway → Air → Wind Speed Mph** and **Wind From Deg** for direct runtime inspection.
A shot already in motion keeps the wind captured at launch, and the flag shows that same
wind until the shot finishes. New wind then affects the flag and subsequent shots.

The console commands remain available during Play: enter `uopengolf.WindFromDeg 90` and
`uopengolf.WindMph 10` separately in Output Log → Cmd. A console change updates the
Play-world range controls and the ball; editing the range controls can change them again.
Console values can remain between Play sessions, but starting Play reapplies the map's wind.

An Unreal `WindDirectionalSource` actor is not connected to this flag. Editing a material's
old droop parameters does not drive the cloth either. If nothing moves, check that wind
speed is above zero, editor preview and Realtime are enabled (or Play is running unpaused),
and the editor has rebuilt the current game modules. Rebuild and restart DebugGame if that
is your editor configuration; a Development build does not update its modules. The expected
scene is one pole and one cloth surface.

## Sources and regeneration

The flagstick, ball, turf, grass and their materials belong to the
[OpenGolfCourse plugin](../Plugins/OpenGolfCourse/README.md), which documents the CC0
sources, conversions, checksums, the editable Blender sources and the rebuild steps. See
[third-party notices](../THIRD_PARTY_NOTICES.md) for asset licenses and authors.

The game keeps only the tracer material. Run `Scripts/UpdateRangePresentation.py` through
Unreal Python (`-ExecutePythonScript`, or `py` in the editor) to regenerate `M_ShotTrail`.
The game-owned ribbon uses a two-sided unlit translucent material with feathered
edges and a `Color` parameter (white by default) that tints the history tracers
orange. Defaults are 3 cm width (at least 0.004 × camera distance, about
2 px at 1080p, so the far arc doesn't thin below a pixel), 0.35 opacity (0.55 for history tracers) and
rollout fading below 5 m/s; these are editable game-ball presentation properties.
Because the game ball loads `M_ShotTrail` at startup, the script can't delete its
graph nodes (`!IsRooted` assertion). On an existing material it rewires emissive to
`Color` and leaves the old white constant disconnected. The ribbon reuses CPU
buffers, recreates topology only when sample count changes, and updates existing
vertices for camera motion. It skips unchanged geometry, opacity and visibility.
These are game-side presentation changes; physics still runs at 120 Hz.

## Validation

Build and run Smoke, `UOpenGolf.Physics`, `UOpenGolf.Course` and `UOpenGolf.Range` with the
project scripts. `UOpenGolf.Range.Presentation` checks the imported ball's source diameter,
non-colliding visuals, the course-kit flagstick, its hanging cloth flag, the wind console
variables reaching the ball and the flag, the green-stimp console variable reaching the ball, the putting green around the flag, each piece's
surface material, the odd mowing-band count across the fairway, and grass disable/re-enable
behavior. `UOpenGolf.Range.RolloutSurfaces` checks that the fairway, rough and green lies
resolve through their physical materials. It keeps the putting green out of its reference
rollouts, so they stay flat single-surface runs. `UOpenGolf.Course.FlagCloth` checks how the
cloth hangs, flies and flaps across wind speeds.
`UOpenGolf.Range.WindSettings` checks saved editor controls, editor cloth preview,
Play-world wind, active-shot wind retention and console overrides.

For rendered inspection and a same-camera grass comparison:

```powershell
& "$env:UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$PWD/UOpenGolf.uproject" `
  -unattended -nosound -RenderOffscreen -ResX=1280 -ResY=720 -DDC=UOpenGolfSmoke `
  '-ExecCmds=Automation RunTests UOpenGolf.Visual.RangeAssets' `
  '-TestExit=Automation Test Queue Empty' `
  "-ReportExportPath=$PWD/Saved/Automation/RangeAssets"
```

Captures are written under `Saved/Screenshots/RangeAssets/`. They include the flag in calm
air (`Flag`) and a 10 mph crosswind (`FlagWind10mph`), alongside the tee, ball and grass views.
The test records mean
editor frame intervals with grass on and off after a settling period. These
include editor and automation overhead and are not standalone-game benchmarks.
Run only one editor at a time, and check again before measuring. Another editor shares
GPU time and shader-working files, so its presence invalidates a controlled FPS comparison.
The first rendered run using the project-local cache may spend several minutes
compiling shaders.
Static tee, grass-off, closeup and coverage-edge views additionally capture 20
frames at half-second intervals to inspect flicker. The tee views match the startup
camera's 70-degree field of view and unrestricted aspect ratio. The report records
actual viewport dimensions and anti-aliasing settings; editor captures may differ
from the requested command-line resolution. Compare equal-sized sequences with
`python3 Scripts/grass_stability.py Saved/Screenshots/RangeAssets --sequence <id>`.
This measures changes over the lower 40% of the image, averaging consecutive pairs;
it counts a pixel as changed when any RGB channel differs by more than 3/255. Each view
also reports its worst 5%-high horizontal band, which catches localized flicker such as
distant moire near the horizon that the lower-40% figure misses.

`UOpenGolf.Range.ShotTrail`
checks path creation, monotonic rollout fade, disappearance at rest and reset for
driver, iron and slow shots. Use `check_range_variant.py --map <map> --render
--shot-trail` for matching moving captures under `Saved/Screenshots/ShotTrail/`.
For a CPU comparison without screenshot encoding, set `UE_ROOT`, build the editor,
and run these sequentially on an otherwise idle machine:

```powershell
python Scripts/check_range_variant.py --map /Game/UOpenGolf/Maps/UOpenGolf --render --shot-trail --trail-timing --legacy-trail
python Scripts/check_range_variant.py --map /Game/UOpenGolf/Maps/UOpenGolf --render --shot-trail --trail-timing
```

`--legacy-trail` selects the previous geometry implementation in development automation
builds; it requires `--trail-timing`. Timing mode sets and checks a 1280x720 render
target, disables VSync, caps at 120 FPS and waits ten seconds after shaders finish.
The report's `Editor.log` includes mean/max tracer and ball CPU times, mean HUD CPU
time, and whole-frame intervals for each shot. These means cover actor/widget ticks,
not individual 120 Hz physics substeps. The game also exposes CPU trace scopes
`UOpenGolf_BallPhysics`, `UOpenGolf_ShotTrail` and `UOpenGolf_HudRefresh`.

Measured tracer CPU work decreased, but whole-frame intervals did not establish an
FPS gain. Detailed measurements, a non-reproducing slow run, and remaining platform
gaps are recorded in [phase status](PhasesStatus.md#reference-measurements).

## Targeted green and flag repair

Export the existing Blender sources with `PrepareCourseAssets.py`, then run
`Plugins/OpenGolfCourse/Scripts/FixGreenAndFlag.py` in the editor. This changes only the
green's stripe contrast, the pole mesh, and the flag material. It does not rebuild rough,
fairway, grass, ball, or source textures. The full `BuildCourseAssets.py` generator remains a
separate maintainer operation and reapplies defaults throughout the kit.

The saved map exposes the layout in the Outliner before Play. Use **Build Default Layout**
only to populate an empty controller, and **Refresh Presentation** after changing global
range presentation settings. Daylight sky capture is refreshed during setup rather than
every frame.
