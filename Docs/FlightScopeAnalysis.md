# FlightScope comparison: 2026-10-14

The historical profile reproduces the short carry shown in the supplied screenshot.
Following that investigation, the HUD directions, flight spin decay and calibration
boundaries were corrected, in that order. The agreed scope is defect correction,
not fitting a single FlightScope result. A second low-wood comparison subsequently
identified an excessive inherited lift boost; the targeted gameplay correction
and its remaining discrepancy are documented below.

## Implemented corrections

- **HUD:** negative horizontal launch now displays an absolute angle followed by
  `left`, positive by `right`, and zero by `straight`. Vertical launch uses
  `down`/`up`/`level`. Missing values remain a dash and derived values retain `*`.
  Packet decoding and signed physics inputs were already consistent; no sign was
  inverted. The original FlightScope screenshot's +0.97 input remains a mismatch
  with the game's measured -0.97.
- **Spin:** the default flight profile now uses the speed-dependent Smits/Smith
  decay described below. The sampled decay rate reaches both flight-step overloads,
  including below the aerodynamic force cutoff. The 120 Hz timestep is unchanged.
- **Calibration:** resolve the old override table at neighboring fixed anchors and
  interpolate all scales/offsets linearly in speed, launch and spin. Clamp outside
  the anchors; retain existing key precedence and neutral fallback at each anchor.
  Interpolate landing-response parameters in launch/spin too. Regime/bucket names
  remain descriptive, and logs mark scales as `interpolated`.
- **Contact timing:** regression testing exposed a pre-existing actor/headless
  difference: the actor discarded the unspent part of a landing timestep. It now
  sweeps through the remainder using the post-impact velocity. Without this fix,
  the second bounce of the drive fell on opposite sides of the 4 m/s restitution
  cutoff and total distance differed by 6.61 yd. With it, both report 248.41 yd carry
  and 262.51 yd total. The bounce coefficients and thresholds were not retuned.

Interpolation anchors (interior samples of the original regions, including family
splits at 110 mph and 30 degrees):

| Axis | Anchors |
| --- | --- |
| Speed, mph | 54, 66, 78.5, 95, 107.5, 115, 125 |
| Vertical launch, degrees | 5, 14, 21.5, 27.5, 31.5, 36.5 |
| Total spin, rpm | 1250, 3250, 4750, 6500, 8500 |

`FBallPhysicsProfile::HistoricalReference()` preserves hard lookups and the old
constant 0.2/s decay and original 1.08 low-launch recovery for the unchanged C# golden fixtures. New gameplay defaults
intentionally differ. `ResolveScaleOverride` and `ResolveLandingResponse` return
by value; callers must not assume the result references a table entry.

## Results after correction

At the game's original -0.97 HLA and 75 F:

| Output | Historical | Spin decay only | All corrections | FlightScope screenshot |
| --- | ---: | ---: | ---: | ---: |
| Carry, yd | 82.6934 | 85.1825 | 85.9353 | 87.6 |
| Apex, ft | 31.0994 | 31.2020 | 31.4958 | 35 |
| Hang time, s | 3.1808 | 3.2946 | 3.3201 | 3.5 |
| Landing spin, rpm | 2361.1 | 4135.2 | 4132.5 | Not shown |

Current total is 89.0597 yd and roll is 3.1244 yd, versus historical 90.7420/8.0486
and FlightScope 93.9/6.3. Retaining spin changes landing behavior materially;
improved carry does not establish improved rollout accuracy. The model still needs
independent validation of low-speed lift and landing response across many shots.

Changing only the heading to FlightScope's +0.97 leaves carry unchanged and gives
8.0956 yd offline at rest. Changing only the temperature to 77 F gives 85.7644 yd
carry; pressure and humidity remain unmatched. The 85.94 yd headline is the game's
75 F reproduction, not a claim of fully matched FlightScope conditions.

The previously discontinuous speed comparisons now increase smoothly:

| Speed change, mph | Carry before, yd | Carry after, yd | Change, yd |
| --- | ---: | ---: | ---: |
| 71.99 to 72.00 | 79.2317 | 79.2496 | +0.0180 |
| 84.99 to 85.00 | 106.2782 | 106.3020 | +0.0238 |

## Second screenshot: low wood, 23:05:52

`Screenshot 2026-10-14 230552.png` shows **128.1 yd** carry in the game versus
**119.8 yd** in FlightScope: **+8.3 yd, +6.9%**. Both use leftward launch and spin
axis, so this is not the earlier HLA-sign mismatch. Game apex is 16.5 ft versus
14.6 ft, and hang time is 2.93 s versus 2.8 s. Those suggest excess sustained lift
as a candidate; the experiments below isolate model contributions.

Game replay inputs are the decoded Square frame: 51.17 m/s (114.464 mph), VLA
6.95°, HLA −0.63°, 1933 rpm total, axis −1.42°, 1932 rpm backspin and −48 rpm side
spin. FlightScope uses the rounded 114.5 mph, 6.95°, 0.63 Left, 1933 rpm and 1.42
Left, standard ball, Medium surface, no wind, sea level, 77 F, 14.69 psi and 50%
humidity. Game air remains 75 F, sea level and dry. Values below use packet
precision; the earlier fixture's rounded 114.46 mph explains its tiny 0.012 yd
difference from this reproduction. In the following diagnostic table, "pre-change"
means corrected decay/interpolation with the old 1.08 low-launch lift recovery.

| Isolated experiment | Carry, yd | Apex, ft | Hang, s | Landing spin, rpm |
| --- | ---: | ---: | ---: | ---: |
| Historical decay and hard lookup | 123.8252 | 16.0051 | 2.7972 | 1103.9 |
| Corrected decay, hard lookup | 128.7876 | 16.6600 | 2.9464 | 1729.2 |
| Historical decay, interpolation | 123.2473 | 15.9246 | 2.7862 | 1105.8 |
| Pre-change gameplay | 128.1250 | 16.5684 | 2.9332 | 1730.4 |
| Pre-change, regime overrides removed | 128.7876 | 16.6600 | 2.9464 | 1729.2 |
| Low-launch lift recovery disabled (**new gameplay**) | 121.5451 | 15.0337 | 2.7406 | 1740.3 |
| Pre-change, mid-spin lift boost disabled | 128.1250 | 16.5684 | 2.9332 | 1730.4 |
| Pre-change, 10% less lift | 118.4049 | 14.4353 | 2.6516 | 1744.7 |
| Pre-change, 10% more drag | 122.6721 | 16.0480 | 2.8478 | 1738.4 |
| Pre-change, 960 Hz | 128.4503 | 16.6306 | 2.9392 | 1730.1 |
| Pre-change, rounded FS inputs and 77 F dry air | 127.9769 | 16.5253 | 2.9262 | 1730.6 |
| FlightScope screenshot | 119.8 | 14.6 | 2.8 | Not shown |

The corrected decay adds 4.96 yd with hard lookups. Interpolation then removes
0.66 yd. Removing the low-launch lift recovery removes 6.58 yd from the pre-change
result, while the mid-spin boost contributes nothing here. Numerical resolution
and rounded inputs/temperature are much smaller effects. A flat carry correction
would hide the disagreements in height and flight time; 10% less lift, for
example, makes carry close but hang time too short.

### Applied low-launch correction

Gameplay now sets `FFlightProfile::LowLaunchLiftRecoveryMax` to **1.0**, removing
the up-to-8% empirical lift recovery. Ordinary Magnus lift still applies. The
legacy boost faded out between 6.5° and 9.5° launch and also depended on Reynolds
number and spin ratio. Disabling it changes low-launch trajectories without
changing the earlier 20.78° iron, standard driver, or other launches at/above
9.5°. `HistoricalReference()` explicitly restores 1.08 to preserve C# parity.
Regime rows, interpolation anchors, global lift/drag, and spin decay are unchanged
by this adjustment. The low wood remains **1.75 yd long**, and its hang time is
about **0.06 s short** of FlightScope. This removes an empirically identified
overcorrection rather than fitting all outputs to a single reference.

The actual range replay now lands at **121.5477 yd**, agreeing with the headless
121.5451 yd within 0.003 yd. The corrected one-surface fairway result is 37.6923 yd
roll and 159.2375 yd total; the range still has multiple surfaces. Across the 21
fixtures, ordinary launches at/above 9.5° keep identical carry and total. Other
affected examples: low bump 37.2783 → 36.9756 yd, topped 56.3856 → 53.7136 yd,
and 9.4° driver 160.7460 → 160.7257 yd. These are change-impact checks, not new
external accuracy claims. The 75.7 mph screenshot iron remains 85.9353 yd.

### Regime and boundary assessment

This shot decodes as `D-S3-V0-P0`: driver/wood family (>110 mph and <18°), speed
105–120 mph, launch <10°, spin <2500 rpm. It has no exact or fallback table row
under hard lookup, so historical regime scales are neutral. Current interpolation
samples speed 107.5/115, launch 5/14 and spin 1250/3250. The `D-S3-V1` override
(drag 1.04, lift 0.99) contributes about 20.1%; the remaining weight is neutral.
Resolved regime scales are drag **1.008047**, lift **0.997988**, with net drag
**1.018128** after the global 1.01 multiplier.

There is no hard regime switch at this shot under current defaults, and blending
is already reducing the overshoot. However, the anchors spread calibration
between neighboring bins: a `V0` label does not imply that only `V0` calibration
affects it. The inherited table and launch-angle lift recovery were tuned with
the older decay. They need joint revalidation over low-launch shots and their
neighbors, not restoration of the old discontinuities. Existing boundary tests
cover family/bin thresholds and interpolation anchors; they establish continuity,
not universally monotonic carry or FlightScope accuracy. The physics README now
contains the [full regime decoder and landing-bucket mapping](../Plugins/OpenFairwayPhysics/README.md#decoding-a-regime-key).

### Rollout and next comparison set

The screenshot game roll/total are 28.8/156.9 yd, versus FlightScope 38.7/158.5 yd.
The game lands on fairway and finishes on the range's green. The same pre-change
inputs on one flat fairway produce 35.0836 yd roll and 163.2086 yd total. The
surface transition explains why the range total differs from that headless
result; neither is directly equivalent to FlightScope's Medium surface. Compare
first-contact carry separately from rollout and record the landing surface and
final lie.

The game now provides nine selectable replay presets, including both screenshot
shots. Press **P**, choose a shot, then **Hit selected shot**; **R** repeats it.
The earlier 75.7 mph preset retains its original leftward HLA; its reference note
calls out the screenshot's rightward FlightScope input. Other synthesized presets
have no FlightScope targets yet. For further tuning, collect matched carry,
apex and hang time for multiple low-launch speeds/spins and neighboring iron
shots. The existing two references still miss in opposite directions. The only
production coefficient change in this second analysis is disabling the legacy
low-launch recovery; see the correction above.

Scientific and implementation references are collected in the
[physics README](../Plugins/OpenFairwayPhysics/README.md#references).

## Rollout surface isolation

**Current scene:** following this diagnosis, the user requested fairway test shots.
Every range ground mesh is now assigned the Fairway tag and appearance, including
the former green/fringe and side strips. Contact-surface resolution remains
unchanged. The default scene now reproduces the all-fairway row below; the test
reconstructs the historical mixed layout explicitly for comparison.

Assignment verification: Windows build, all 14 project tests, separate smoke and
rendered check pass. Reports: `Saved/Automation/20261014-234851-cf978ea3`,
`Saved/Automation/20261014-235107-48ad60d0`, and `Saved/Automation/FairwayScene`.
The default actor gives 121.5477 yd carry, 37.6921 yd roll and 159.2398 yd total;
the rendered HUD's Fairway lie was inspected. Linux remains untested.

The follow-up user log reports 121.5 yd carry, 31.9 yd roll, 153.5 yd total,
landing on Fairway and resting on Green. `UOpenGolf.Range.RolloutSurfaces`
reproduces the shot three times in PIE, retaining identical geometry, ball actor,
launch and coefficients. Only component surface tags change between runs:

| Contact configuration | Carry, yd | Roll, yd | Total, yd |
| --- | ---: | ---: | ---: |
| Original range: fairway → fringe → green | 121.5477 | 31.9233 | 153.4711 |
| Same geometry, all fairway tags | 121.5477 | 37.6921 | 159.2398 |
| Original fringe retained, green tagged fairway | 121.5477 | 36.7137 | 158.2615 |
| Headless, single fairway | 121.5451 | 37.6923 | 159.2375 |
| FlightScope screenshot, Medium | 119.8 | 38.7 | 158.5 |

The original path enters Fringe at 131.473 yd / 3.700 s and Green at 133.650 yd /
4.033 s. Rest occurs at 11.792 s, reproducing the user's 11.8 s log. With the
green replaced by fairway, retaining the fringe costs 0.9784 yd versus all
fairway. Restoring the green then costs another 4.7904 yd on that path. These
are sequential controlled differences, not universal additive surface penalties.
The all-fairway actor/headless roll difference is less than 0.001 yd, excluding
tile geometry or the actor wrapper as the cause of the reported gap in this case.

The resolved regime contact multipliers are neutral: kinetic friction **1**, rolling
friction **1**, grass viscosity **1**, critical-angle offset **0**, spin-back angle
boost multiplier **1**. The interpolated Run response adds **zero landing bite**.
`override interpolated` labels blended launch calibration; it does not impose a
distance cap. Aerodynamic scales still affect the landing state indirectly.

| Resolved surface | Kinetic friction | Rolling friction | Grass viscosity | Rolling spin scale |
| --- | ---: | ---: | ---: | ---: |
| Fairway | 0.50000 | 0.05000 | 0.00170 | 1 |
| Fringe | 0.56000 | 0.07250 | 0.00245 | 1 |
| Green, stimp 10 | 0.58000 | 0.05179 | 0.01100 | 0 |

The green's grass-viscosity coefficient is about 6.47 times the fairway's; it
applies speed-dependent translational drag and spin damping. Its rolling
resistance separately follows stimp and does not receive the fairway's spin
multiplier. The combined green response explains the additional stopping here;
the experiment does not attribute all of it to viscosity alone. This is the
implemented model, not evidence that a real green should always stop this shot
sooner. Stimp constrains low-speed roll, so validating high-speed contact and drag
remains a separate calibration task.

The useful FlightScope comparison is the uniform-fairway result: roll is about
1.01 yd shorter, while carry is 1.75 yd longer and total is 0.74 yd longer. Medium
and Fairway are still different models. Increasing regime rollout to compensate
for the mixed range would obscure the surface mismatch. This investigation adds
a regression test and documentation; no runtime physics or surface settings change.

Validation: Windows editor build and all 14 current project tests pass, including
the new surface-isolation test. Report:
`Saved/Automation/20261014-234231-61fb0e80`. Separate startup smoke passes:
`Saved/Automation/20261014-234420-030290be`. No presentation changes or Linux run.

## Historical investigation

The remaining sections preserve the **historical investigation** and its measured
experiments. References there to the current model describe the pre-fix baseline.

## Reference and reproduction

Source: user-provided `Screenshot 2026-10-14 220535.png`, showing the
[FlightScope Trajectory Optimizer](https://trajectory.flightscope.com/) beside the
running game. This is a comparison with another trajectory model, not a measured
outdoor landing. The reported GSPro advantage of 1–2 yards has no supplied result
or settings to reproduce independently.

Inputs: 75.7 mph, 20.78 degrees vertical launch, 4463 rpm total spin,
20.62 degrees right spin axis. The game shows **-0.97 degrees** horizontal launch;
FlightScope is set to **+0.97 degrees (Right)**. FlightScope uses its standard ball,
medium surface, no wind, 77 F, 14.69 psi and 50% humidity. The game defaults to
75 F, sea level, dry air and fairway. Pressure and humidity are not exposed by
the game's air model.

| Output | Game screenshot | Native headless reproduction | FlightScope screenshot |
| --- | ---: | ---: | ---: |
| Carry, yd | 82.7 | 82.6934 | 87.6 |
| Roll, yd | 8.0 | 8.0486 | 6.3 |
| Total, yd | 90.7 | 90.7420 | 93.9 |
| Apex, ft | 31.0 | 31.0994 | 35 |
| Hang time, s | 3.18 | 3.1808 | 3.5 |
| Offline, yd right | 4.5 | 4.4692 | 9.6 |

Carry is 4.91 yd (5.6%) short. Lower apex and hang time make insufficient sustained
lift a candidate alongside drag. Surface friction and landing response cannot
explain first-contact carry; they influence the separate rollout discrepancy.
The screenshot values are rounded, and the two surface models are not equivalent.

## Findings

### 1. Flight spin decay is unusually fast

`GolfPhysics::StepFlight` multiplies spin by `exp(-dt / 5)`. This removes 18.1%
per second and leaves only about 2361 rpm at landing from 4463 rpm at launch.
It is independent of airspeed. It also changes impact spin and therefore rollout.

For a physics-based comparison, the diagnostic uses the Smits/Smith form
`d(omega)/dt = -2e-5 * (speed / radius) * omega`, with speed and radius in SI units.
The golf-ball measurements and units are discussed in section II and footnote 5
of [Nathan's spin-down analysis](https://baseball.physics.illinois.edu/spindown.pdf).
At this launch speed that corresponds to an initial decay time constant around
31.5 s, lengthening as the ball slows. It is a candidate model, not a known
implementation detail of FlightScope; its cited wind-tunnel Reynolds range also
does not cover the entire low-speed portion of this trajectory.

Changing only the diagnostic spin decay gives **85.18 yd carry**, 31.20 ft apex,
3.295 s hang time and about 4135 rpm landing spin. That recovers roughly half the
carry gap but does not recover the reference apex. A spin-decay change alone is
therefore insufficient. Existing lift/drag and landing calibration were fitted
around the old decay, so a global switch needs a broader external comparison set.

### 2. Discrete calibration bins create nonphysical distance drops

`FBallPhysicsProfile::ResolveScaleOverride` selects one fixed override from hard
speed/launch/spin bins. At the screenshot's launch and spin, the selected
`I-S1b-V2-P2` override is 0.94 drag and 1.03 lift (drag also has a global 1.01 factor).
Immediately below 72 mph, the override is 0.82 drag and 1.13 lift. At 85 mph this
shot falls back to neutral regime scales.

| Ball speed, mph | Carry, yd |
| ---: | ---: |
| 71.99 | 79.6612 |
| 72.00 | 75.0252 |
| 84.99 | 105.4300 |
| 85.00 | 102.2045 |

A 0.01 mph increase must not abruptly remove 4.64 or 3.23 yd under otherwise
identical conditions. These jumps are confirmed model defects, separate from
the particular 75.7 mph shortfall. A continuous calibration surface, or smooth
blending between calibrated regions, needs regression coverage at the boundaries
for speed, launch and spin. Adding one more special-case multiplier perpetuates
this problem.

### 3. Low-speed lift is highly sensitive to the coefficient curves

`FlightAerodynamics::GetCl` interpolates independent polynomial fits at Reynolds
numbers 50k, 60k, 65k, 70k and 75k. At a fixed spin ratio of 0.25, its base lift
coefficient changes from 0.2144 at 75k to 0.1000 at 65k, 0.02356 at 60k and
0.00526 at 50k. The mid-spin boost is applied afterward. The shot starts near
Re 94k and decelerates into this sensitive region.

This explains why small atmosphere and spin changes can have unintuitive results.
It is a priority for validation against measured ball-specific aerodynamic data;
the screenshot alone does not establish that the polynomial fits are wrong.
Other calibration corrections depend on the initial launch angle, so this is an
empirical shot model rather than aerodynamics determined solely by instantaneous
velocity, spin, air and ball properties. `FFlightProfile::CdMin` is also declared
but never used; changing it currently has no effect.

### 4. Input comparison and numerical effects

- Changing HLA from -0.97 to +0.97 preserves carry on flat ground but changes
  final offline from 4.4692 to 7.5416 yd. Much of the lateral discrepancy is an
  input mismatch; the remainder still needs investigation with matching settings
  and definitions. The HUD's wording of a negative value followed by "right" is
  easy to misread. No device sign error was established here.
- Increasing the integration rate from 120 to 960 Hz changes carry from 82.6934
  to 82.9194 yd: only +0.226 yd. Numerical error is far too small to explain the gap.
- Matching 77 F alone gives 82.4659 yd. Temperature alone does not resolve it;
  humidity and pressure remain unmatched. Lower density also changes Reynolds
  number and lift, so it cannot be treated as a simple distance multiplier here.
- The native headless result agrees with the displayed game result to rounding.
  Unit conversion, terrain collision and the Unreal wrapper are not implicated
  by this reproduction. Carry is a projection onto initial horizontal shot
  direction; FlightScope's exact carry/offline definitions should be confirmed
  before comparing strongly curving shots.

## Controlled sensitivity experiments

All rows use game inputs and 75 F unless explicitly noted. Each change is isolated;
these are diagnostic results, not proposed fitted settings.

| Experiment | Carry, yd | Apex, ft | Hang time, s |
| --- | ---: | ---: | ---: |
| Production baseline | 82.6934 | 31.0994 | 3.1808 |
| 960 Hz only | 82.9194 | 31.2409 | 3.1883 |
| Speed-dependent spin decay only | 85.1825 | 31.2020 | 3.2946 |
| 77 F only, dry air | 82.4659 | 31.0515 | 3.1734 |
| 10% less drag only | 85.8047 | 31.8133 | 3.2346 |
| 10% more lift only | 84.1017 | 32.4517 | 3.2773 |

Prioritize smooth calibration boundaries, validated spin decay and low-Re lift,
then assess carry, apex, hang time and landing together over a range of shots.
Do not infer a universal yardage adjustment from one screenshot or assume GSPro's
reported longer result is ground truth. Shortening this shot further would
increase its disagreement with the supplied FlightScope reference.

## Reproduction and validation

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Build
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Test -Filter UOpenGolf.Physics
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Smoke
```

`UOpenGolf.Physics.FlightScopeAnalysis` logs historical, decay-only and corrected
comparisons and experiments. Its
assertions verify that the diagnostic flight loop agrees with the production
simulator, that heading rotation preserves carry, and that the shot completes.
A passing result does **not** claim agreement with FlightScope. The separate
`UOpenGolf.Physics.FlightCorrections` test verifies analytical spin decay,
mirrored launch directions, anchor interpolation and boundary continuity. The original
21 C# golden fixtures establish port parity, not independent physical accuracy.

Historical investigation validation: Windows / Unreal 5.8.3, 2026-10-14: editor build, all three physics tests and the
startup smoke test pass. Reports: `Saved/Automation/20261014-221126-03bc11be` (physics)
and `Saved/Automation/20261014-221212-6ace7a82` (smoke).
Linux and rendered presentation were not tested during the historical investigation.

Earlier correction validation (Windows / Unreal 5.8.3, 2026-10-14): editor build,
all 11 headless project tests, a separate smoke test and rendered HUD capture test
pass. Reports: `Saved/Automation/20261014-230126-b2e8eb64`,
`Saved/Automation/20261014-230219-11ff00ec`, and
`Saved/Automation/LaunchDirectionsCached`. The images
`Saved/Screenshots/LaunchDirections/20261014-230405-left-up.png` and
`20261014-230408-right-down.png` were inspected: both launch labels are correct.
These are targeted HUD checks, not full ball/camera visual validation. Linux and
new hardware checks remain untested. A first rendered attempt using the empty
smoke cache was stopped during shader compilation; the successful run used the
normal cache. An intermediate range run passed its assertions but stalled on
process exit; the final complete suite and smoke commands exited successfully.

Second-shot and selector validation (Windows / Unreal 5.8.3, 2026-10-14): final
editor build and all 12 headless project tests pass, including replay selection
and the ball actor's corrected low-wood carry. Headless report:
`Saved/Automation/20261014-233128-032069a4`. Separate startup smoke also passes:
`Saved/Automation/20261014-233523-38e15ca2`. Rendered report:
`Saved/Automation/ReplaySelectorFinal`. Captures
`Saved/Screenshots/ReplaySelector/20261014-233453-1.png`,
`20261014-233455-3.png`, and `20261014-233459-5.png` were inspected: the preview
and expanded list are readable at 700×330, and the game HUD reports 121.5 yd carry.
An earlier visual run exposed small text and a transparent background; those
were corrected before this final render. Linux and hardware remain untested in
this follow-up.
