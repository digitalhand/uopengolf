# Accuracy against reference flights

How closely the gameplay physics matches reference shots, how the flight table was fitted, and how to
reproduce both. This is the plugin's comparison document: sources are named only by acronym, and their
data stays outside the repository, in the local reference data folder (see
[`Assets/Data/README.md`](../../../Assets/Data/README.md#where-the-data-lives)). How the physics works is in
the [reference](Reference.md).

| Acronym | Reference | Rows | Speeds | Gives |
| --- | --- | ---: | --- | --- |
| lm | Launch-monitor app export, one range session | 124 | 36–121 mph | carry, total, roll, apex, landing angle |
| tm | Published tour averages (men's and women's tours), one row per club | 23 | 86–167 mph | carry, apex (max height), landing angle |
| fs | Online trajectory calculator, standard ball, 77 °F, 50% humidity, 14.69 psi | 13 | 109–175 mph | carry, apex |

All three give a model's or an average's answer, not one measured flight: lm's results are the app's
computed flight for the measured launch, tm's rows are averages over many tracked shots (the carry of the
average launch is close to, not equal to, the average carry), and fs is a calculator. Club labels are never
read: every comparison uses the launch values only.

## Flight

Engine minus reference, bias / mean absolute error. Every shot is replayed in Unreal
(`UOpenGolf.Physics.ShotLibrary`) on flat fairway, in its session's reference air, with no wind:

| Reference | Shots | Carry, yd | Apex, ft | Landing, ° |
| --- | ---: | --- | --- | --- |
| tm | 23 | +1.3 / 3.3 | +3.9 / 5.0 | −1.3 / 1.7 |
| lm | 124 | −2.1 / 2.1 | −0.1 / 0.6 | +1.3 / 1.4 |
| fs | 13 | +0.3 / 0.7 | +1.2 / 1.2 | |

lm's six topped shots (launched below 0°) count with a carry and apex of 0. Some launches, each in its
reference's air:

| Launch | Reference | Carry, yd | Engine carry | Apex, ft | Engine apex |
| --- | --- | ---: | ---: | ---: | ---: |
| 175 mph / 10.0° / 2200 rpm | fs | 299.7 | 300.3 | 96.8 | 96.7 |
| 167 mph / 10.9° / 2686 rpm (men's driver) | tm | 275 | 281.8 | 96 | 108.2 |
| 167 mph / 10.9° / 2686 rpm | fs | 283.1 | 282.9 | 106.5 | 107.6 |
| 150 mph / 12.5° / 2335 rpm | fs | 249.7 | 251.0 | 85.5 | 86.3 |
| 150 mph / 12.5° / 2335 rpm, 9.9° left spin axis | fs | 247.7 | 248.7 | 83.8 | 84.7 |
| 140 mph / 13.2° / 2611 rpm (women's driver) | tm | 218 | 228.0 | 75 | 84.4 |
| 132 mph / 12.1° / 5361 rpm (men's 5 iron) | tm | 194 | 192.5 | 93 | 92.3 |
| 109.3 mph / 16.7° / 5072 rpm | fs | 154.7 | 154.1 | 71.6 | 74.2 |
| 109.3 mph / 16.7° / 5072 rpm | lm | 157.0 | 153.7 | 73.3 | 74.6 |

The references disagree with each other by several yards for the same launch: fs carries 8–10 yd farther
than tm at tm's driver and wood launches, and lm's model 2–3 yd farther than tm's at iron launches. The
table follows fs for drivers and woods, so it carries long of tm there (+2.8 yd above 120 mph; +8.7 yd below
3000 rpm, all of it the men's and women's drivers and the women's 3 wood) and its driver apex is high of
tm's (108 ft against 96; fs gives 106.5 ft for the same launch). Higher-spin launches sit between tm and lm:
tm's launches at 5000 rpm and above carry within 1.8 yd on average (1.0 yd short), and lm's shots carry 2.1 yd
short.

## Roll

Against lm, the only reference with roll (`shot_compare.py`), bias / mean absolute error in yards:

| Launch class | Shots | lm roll | Engine roll | Error |
| --- | ---: | ---: | ---: | --- |
| Ground, below 2.5° | 9 | 86.6 | 86.1 | −0.5 / 1.1 |
| Low, 2.5–9° | 11 | 35.5 | 35.6 | +0.1 / 0.7 |
| Mid 9–20°, below 4000 rpm | 33 | 22.7 | 20.4 | −2.3 / 2.3 |
| Mid 9–20°, 4000 rpm and up | 48 | 10.3 | 8.0 | −2.3 / 2.3 |
| High, 20° and up | 23 | 3.6 | 3.1 | −0.5 / 0.7 |
| All | 124 | 20.1 | 18.5 | −1.6 / 1.8 |

Total distance is −3.7 / 3.8 yd, carry and roll together. The turf was fitted to lm's roll from lm's own
landings; the flight lands about 1.3° steeper than lm's model, so mid launches roll about 2 yd short. Of the baseline gates (`shot_compare.py --check-baseline`), mid launches at 4000 rpm and above fail:
their roll error is 2.3 yd against 1.1 yd in the baseline (0.5 yd allowed). The six topped shots finish
within 0.9 yd of lm on average (worst 2.5 yd); see [Topped shots](Reference.md#topped-shots).

## The flight table

For every reference shot, the constant drag and lift that reproduce both its carry and its apex fall on one
smooth curve against the launch spin ratio W (surface speed over air speed), whichever source the shot comes
from:

| Launch W | tm Cd / Cl | fs Cd / Cl | lm Cd / Cl |
| --- | --- | --- | --- |
| ~0.08 (driver) | 0.218 / 0.158 | 0.212–0.220 / 0.163–0.173 | |
| ~0.10–0.15 | 0.267–0.288 / 0.216–0.248 | 0.233–0.280 / 0.195–0.252 | 0.238 / 0.238 |
| ~0.20–0.25 | 0.314–0.334 / 0.300–0.316 | 0.334 / 0.314 | 0.316 / 0.309 |
| ~0.30–0.40 | 0.347–0.403 / 0.343–0.372 | | 0.349 / 0.339 |
| ~0.45–0.50 | 0.374–0.421 / 0.323–0.407 | | 0.407 / 0.399 |

Drag rises steeply with spin from a modern ball's low-spin value; lift follows a saturating curve close to
the measured lift of a modern ball. A law in W alone flies tm's balls too high for their carry; the measured
drag rise of a modern ball below Re 100k (the drag crisis, felt on the descent) closes most of that. Measured
coefficients alone do not work: the published drag of a modern ball was measured near 2250 rpm only, and no
measurement shows how it rises with spin, so fast, high-spin shots meet far too little drag. Published
spin-dependent models (Smits and Smith 1994) have the drag of an older ball and leave a driver some 40 yd short.

The table (`Private/OpenFairway/SpinRatioData.h`) was fitted in two passes by `Assets/Scripts/flight_fit.py`:

1. **All knots** (`fit`), weighted tm 2 : lm 1 : fs 0.5 as whole sessions, on carry (±3 yd), apex (±5 ft) and
   landing angle (±1.5°). Constraints: both curves never fall with spin; drag never drops below 0.17, the
   measured minimum of a modern ball; lift is zero without spin and, below the launch data, follows the measured
   lift of a modern ball (0.115 at W 0.06); both curves are smooth. The fit chose the drag-crisis share, 0.68.
   Its lift also matches the measured modern-ball lift at W 0.18 and 0.32, which the fit did not use.
2. **The low-spin knots** (`refine`): W below 0.16, refitted toward fs's driver and wood carries, with every shot
   launched at W 0.16 or more held within 0.5 yd of its carry and no session's carry error allowed to grow. This
   relaxed the measured low-spin lift in favour of fs (lift 0.130 at W 0.06).

Checks made while fitting: cross-validation within lm (test error 0.86–0.90 yd for a four-constant law), half
of fs predicting the other half (0.95–1.69 yd), and the women's tour rows predicted from the men's and lm.

## Limits

- The references disagree by several yards; no table matches all three.
- Outside the flight data (Re 35k–215k, W 0.06–0.8; about 36–175 mph) the curves are held at their ends, and
  the replay reports the time each shot spends outside (`outside_coverage_s`).
- Headwinds have not been compared with published wind data for this table; tailwind and crosswind direction
  and the height profile are tested (`UOpenGolf.Physics.Wind`, `WindHeight`).
- No roll reference is faster than 121 mph ball speed, and none was hit on rough or the other turf surfaces.

## Reproducing

1. Put the reference data in the local folder (`UOPENGOLF_DATA`, default `~/data/uopengolf`).
2. Replay and report (the editor must be closed):

   ```powershell
   python Assets/Scripts/shot_compare.py --build --check-baseline <local data>/baselines/pre_contact_20261116.csv
   python Assets/Scripts/carry_audit.py                                 # the flight table above
   python Assets/Scripts/flight_fit.py check                            # the same flight in Python (NumPy)
   ```

3. To refit the flight, keep the current results, change the table, then compare:

   ```powershell
   copy Saved\ShotLibrary\results.csv Saved\ShotLibrary\results_before.csv
   python Assets/Scripts/flight_fit.py fit --weight tm_tour_averages=2 --weight fs_trajectory_calculator=0.5
   python Assets/Scripts/flight_fit.py refine --target-session fs_trajectory_calculator   # needs SciPy
   # paste the printed table into SpinRatioData.h, then
   python Assets/Scripts/shot_compare.py --build
   python Assets/Scripts/carry_audit.py --before Saved/ShotLibrary/results_before.csv --markdown Saved/ShotLibrary/flight_comparison.md
   ```

   `flight_fit.py` reads the shipping table and the ball constants from the engine headers and flies the same
   RK4 flight; `check` reproduces the Unreal carries above to about 0.01 yd. A refit also changes landings, so
   check roll too (`shot_compare.py`), and refresh `UOpenGolf.Physics.GameplayPins` only for an intended change.
4. To refit the turf: `Assets/Scripts/ue.ps1 Test -Filter UOpenGolf.Calibration.Rollout` (see
   [Turf calibration](Reference.md#turf-calibration)).
