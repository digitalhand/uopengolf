# Carry audit (2026-11-17 to 2026-11-18)

## Driver calculator refinement (2026-11-18)

The gameplay table now prioritizes fs for driver/wood launches. The earlier results below
describe the original seven-knot fit. The new table adds W = .08, .12 and .16, adjusts only
W < .16, and retains the original high-spin curves and drag-crisis term. Club labels do not
affect simulation. Drag and lift remain monotonic; the original low-spin measured-lift
anchor is relaxed in favor of the calculator trajectories.

The fresh baseline reproduced the reported live driver: 243.92 yd carry, 17.7 yd roll.
Headless carry was 243.99 yd in default air (75 F, dry, sea level), versus 244.69 yd
in fs air (77 F, 50% humidity, 14.69 psi). The fs comparison for its left spin axis is
247.7 yd, not the zero-axis 249.7 yd row.

The refined table gives about 248.7 yd carry in fs air (+1.0 yd), with apex 84.7 ft
against 83.8 ft. Across all 13 fs rows, carry MAE improves from 3.81 to 0.71 yd;
the largest absolute error is 1.34 yd. Mean error is +0.27 yd. Tour-average carry
MAE changes from 2.34 to 3.33 yd, primarily on drivers and woods where the sources disagree.
The tour iron/wedge MAE remains 1.67 yd, and lm iron MAE remains 2.65 yd.

All recorded iron/wedge carry changes are below 0.45 yd. This includes low, mishit
trajectories; ordinary higher-spin shots are unchanged. Roll is not calibrated here:
the driver lands faster and shallower, increasing flat-fairway roll from 17.93 to 19.63 yd
in fs air (fs reference 13.1 yd). The largest recorded iron/wedge roll movement is about
1.50 yd, despite carry staying protected. Surface parameters are unchanged.

Reproduction tools:

- `Assets/Scripts/flight_driver_fit.py` uses NumPy/SciPy, the original table as baseline,
  and the local reference library plus recorded shots. It freezes W >= .16, penalizes
  protected carry movement and reference-error increases, fits at 60 Hz, and verifies at
  120 Hz. Shot names select audit cohorts only. The script prints a candidate, never edits C++.
- `Assets/Scripts/flight_comparison.py --before Saved/CarryAudit/fs_before.csv --after
  Saved/CarryAudit/fs_after.csv --output Saved/CarryAudit/flightscope_comparison.md`
  produces all fs/tm/lm before/after reference tables plus every replayed shot's carry,
  apex, landing-angle and roll changes. These are Unreal results, not fitting predictions.
- Keep a fresh Unreal baseline before refitting and replay the candidate with
  `python Assets/Scripts/shot_compare.py --build`. Require fs carry errors <= 2 yd and
  protected iron/wedge carry movement <= 0.5 yd. Check reference MAE to numerical precision,
  and disclose roll changes separately. The existing sand-contact parity failure must
  not be confused with a flight calibration regression.

Why the gameplay flight changed, what was tried, and how well the result matches each reference.
Sources are named only by acronym; their data stays outside the repository, in the local reference
data folder (see [`Assets/Data/README.md`](../../../Assets/Data/README.md#where-the-data-lives)).

| Acronym | Reference | Rows | Speeds | Gives |
| --- | --- | ---: | --- | --- |
| lm | Launch-monitor app export, one range session (2026-11-13) | 124 | 36–121 mph | carry, total, roll, apex, landing angle |
| tm | Published tour averages (men's and women's tours), one row per club | 23 | 86–167 mph | carry, apex (max height), landing angle |
| fs | Online trajectory calculator, standard ball, 77 °F, 50% humidity, 14.69 psi | 13 | 109–175 mph | carry, apex |

All three give a model's or an average's answer, not one measured flight: lm's results are the
app's computed flight for the measured launch, tm's rows are averages over many tracked shots (the
carry of the average launch is close to, not equal to, the average carry), and fs is a calculator.
Club labels are never read: every comparison uses the launch values only.

## The complaint

Full shots carried 5–8 yd farther than lm. The example launch, 109.3 mph / 16.7° / 5072 rpm,
carried 163.1 yd against lm's 157.0 (fs 154.7). Against tm the legacy flight was 10 yd long on
average, and 7–19 yd long for every club the tour sheet lists.

## What was tried

1. **RK4 integration alone** (same aerodynamics): carry moved by under 0.5 yd. The step size was not
   the cause.
2. **Measured coefficients of one modern ball** (`ProV1Reference()`: drag and lift digitized from
   Lyu et al. 2018 and 2020): carry error against lm grew from 4.1 to 13.3 yd, and the example
   carried 190.8 yd. That drag was measured near 2250 rpm, and nothing in the papers measures how a
   modern ball's drag rises with spin, so fast, high-spin shots met far too little drag.
3. **Published model combinations** (Smits and Smith 1994 drag and lift; modern drag with the 1994
   spin-drag slope; 1994 drag with modern lift): the best matched lm to 3.1 yd but left the men's
   tour-average driver 46 yd short (229 against 275 yd). lm has no shot above 121 mph, so the fault
   only showed against tm and fs. The 1994 ball's drag at driver speeds (0.31) is far above a modern
   ball's (about 0.21).

## What the references require

For every reference shot, the constant drag and lift that reproduce both its carry and its apex
(solved in a replica of the engine's flight, validated to 0.01 yd against Unreal) fall on one
smooth curve against the launch spin ratio W, whichever source the shot comes from:

| Launch W | tm Cd / Cl | fs Cd / Cl | lm Cd / Cl |
| --- | --- | --- | --- |
| ~0.08 (driver) | 0.218 / 0.158 | 0.212–0.220 / 0.163–0.173 | |
| ~0.10–0.15 | 0.267–0.288 / 0.216–0.248 | 0.233–0.280 / 0.195–0.252 | 0.238 / 0.238 |
| ~0.20–0.25 | 0.314–0.334 / 0.300–0.316 | 0.334 / 0.314 | 0.316 / 0.309 |
| ~0.30–0.40 | 0.347–0.403 / 0.343–0.372 | | 0.349 / 0.339 |
| ~0.45–0.50 | 0.374–0.421 / 0.323–0.407 | | 0.407 / 0.399 |

Drag rises steeply with spin (spin-induced drag) from a modern ball's low-spin value; lift follows
a saturating curve close to the measured modern-ball lift. Parametric laws (a power-law or
saturating lift with lift-induced drag Cd0 + K·Cl²) fitted lm alone to 0.6–0.8 yd and predicted the
drivers within 6 yd without seeing them, but could not also follow tm's trajectory shape: tm's
balls fly lower and land steeper than any spin-ratio-only law that matches their carry. Adding the
measured drag rise of a modern ball below Re 100k (the drag crisis, felt on the descent) closes
most of that gap.

## The model (`EFlightModel::SpinRatioV1`)

Drag and lift are piecewise-linear curves in W (`Private/OpenFairway/SpinRatioData.h`), plus a share
of the measured drag crisis, fitted by `Assets/Scripts/flight_fit.py`:

- **Data and weights:** tm 2, lm 1, fs 0.5 (each source's total weight); carry (±3 yd), apex
  (±5 ft) and landing angle (±1.5°) residuals.
- **Physical constraints:** both curves never fall with spin; drag never drops below 0.17, the
  measured minimum of a modern ball; lift is zero without spin, and below the launch data
  (W < 0.08) it follows the measured lift of a modern ball (0.115 at W 0.06); smoothness on both
  curves.
- **Result:** drag 0.187 → 0.361 from W 0.06 to 0.5; lift 0.113 → 0.353; the drag-crisis share is
  0.68 (the fit chose it). The lift also matches the measured modern-ball lift at W 0.18 and 0.32,
  which the fit did not use.
- **Checks during the analysis:** cross-validation within lm (test error 0.86–0.90 yd for a
  four-constant law), half of fs predicting the other half (0.95–1.69 yd), and the women's tour
  rows predicted from the men's and lm.

## Results (Unreal, 2026-11-18)

`python Assets/Scripts/carry_audit.py`; engine minus reference, bias / mean absolute error:

| Reference | Profile | Carry, yd | Apex, ft | Landing, ° |
| --- | --- | --- | --- | --- |
| tm | SpinRatioV1 (gameplay) | +0.3 / 2.3 | +3.6 / 4.7 | −1.3 / 1.7 |
| tm | legacy | +10.1 / 10.1 | −2.6 / 6.7 | −4.0 / 4.1 |
| lm | SpinRatioV1 | −2.2 / 2.2 | −0.1 / 0.6 | +1.3 / 1.4 |
| lm | legacy | −1.0 / 4.1 | −1.8 / 2.7 | −0.2 / 1.3 |
| fs | SpinRatioV1 | −3.8 / 3.8 | −0.4 / 1.2 | |
| fs | legacy | +2.9 / 3.4 | −4.1 / 4.7 | |

| Launch | tm | fs | lm | Legacy | SpinRatioV1 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 167 mph / 10.9° / 2686 rpm (men's driver) | 275 | 283.1 | | 282.2 | 276.4 |
| 140 mph / 13.2° / 2611 rpm (women's driver) | 218 | 228.1 | | 229.2 | 223.1 |
| 150 mph / 12.5° / 2335 rpm, 9.9° left axis, fs's air | | 247.7 | | 248.9 | 244.7 (246.8 with no axis) |
| 109.3 mph / 16.7° / 5072 rpm (the example) | | 154.7 | 157.0 | 163.1 | 153.7 |

The references disagree by several yards for the same launch (fs 8–10 yd longer than tm; lm 2–3 yd
longer than tm), so no model matches all three; this one leans to tm. `UOpenGolf` passes 112 of
113, including live/headless carry parity within 0.25 yd at 30–144 fps and 120/960 Hz carry
convergence within 0.1 yd for every library shot; the one failure is a live/headless contact case
on sand (see Open items).

## Open items

- **Roll.** The turf was fitted to lm with the legacy flight. The new flight lands about 1.3°
  steeper and slower than lm's model, so roll is 1.7 yd short of lm on average and one accuracy
  gate fails (mid launches above 4000 rpm: roll error 1.1 → 2.4 yd, against the 0.5 yd allowance).
  `UOpenGolf.Calibration.Rollout` can refit the turf to the new landings; not done yet.
- **Driver apex.** The men's driver peaks at 107 ft against tm's 96 (fs gives 106.5 for the same
  launch).
- **Live/headless contact on sand.** A grazing second touchdown on sand (113.9 mph / 0.6°) ends
  0.31 yd farther live than headless, failing `UOpenGolf.Range.RecordedContactParity` (0.25 yd
  allowed). Flight and both impacts agree; the live sweep finds the contact a step late, then moves
  a whole step before its first ground step. Fixing it means a ground step for the rest of an impact
  step in both loops, which moves every total slightly; best done with the turf refit.
- **Headwinds** have not been compared with the published wind sets for the new flight (see the
  physics reference, Headwinds).
- **Coverage.** Outside 36–175 mph (Re 35k–215k, W 0.06–0.8) the curves are held at their ends;
  the results report time spent outside.

## Reproducing

1. Put the reference data in the local folder (`UOPENGOLF_DATA`, default `~/data/uopengolf`).
2. `powershell -File Assets/Scripts/ue.ps1 Build`, then
   `python Assets/Scripts/shot_compare.py` (replays every session and writes the results) and
   `python Assets/Scripts/carry_audit.py --sample-session <session>`.
3. To refit: `python Assets/Scripts/flight_fit.py` (needs NumPy) prints a new table for
   `SpinRatioData.h`; rebuild and rerun step 2 and `UOpenGolf.Physics`.
