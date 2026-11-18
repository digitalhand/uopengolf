# Rollout acceptance baseline

`pre_contact_20261116.csv` is the fresh 124-shot replay made before the coupled-contact
and supported-launch changes. The editor build and all 21 physics tests passed.
It includes the user's existing turf and axis-aware spin changes; it is not the older
`historical` physics profile. Square conditions are assumed to be flat fairway,
75 F, sea level and no wind.

Square's selected club labels are unreliable. Acceptance uses measured launch values,
with equally weighted launch classes in calibration, and never selected club names.
Original Square output remains in `../ShotLibrary/square_20261113/square_export.csv`.
The November range recordings have no measured reference distances and are excluded
from distance accuracy calculations, but must still replay to rest.

Run the acceptance comparison from the repository root:

```text
python Assets/Scripts/shot_compare.py --build --check-baseline Assets/Data/ShotLibraryBaselines/pre_contact_20261116.csv
```

The baseline current-profile absolute errors are 3.423878 yd mean rollout,
6.022857 yd mean total, and 34.3367 yd worst rollout. Required worst rollout is
at most 25.752525 yd (25% improvement); neither aggregate mean may worsen, and
no launch group's rollout mean may worsen by more than 0.5 yd.

## Status (2026-11-17)

The current fit passes every gate (`shot_compare.py --build --check-baseline`, 2026-11-17):

| Gate | Baseline | Current | Result |
| --- | ---: | ---: | --- |
| Mean rollout error | 3.424 yd | 1.23 yd | Passes |
| Mean total error | 6.023 yd | 3.98 yd | Passes |
| Worst rollout | 34.34 yd | 7.80 yd (must be ≤ 25.75) | Passes |
| Ground-launched class, mean rollout | 22.17 yd | 0.8 yd | Passes |
| Every other launch class within +0.5 yd | | Low 2.3, mid < 4000 rpm 1.8, mid ≥ 4000 rpm 1.0, high 0.6 | Passes |
| Provenance | | Fresh | Passes |

The six Square rows launched below 0° (topped shots: no carry, no apex) are scored like
every other row. They are matched by a rule for full shots struck along the ground, which keep
their own share of the turf's grass drag for the whole roll; see the physics reference,
"Topped shots". The first 2026-11-17 fit, without that rule, failed three gates through those
rows: mean rollout 3.668 yd, worst rollout 47.06 yd and ground-launched class 29.38 yd. The
gates themselves were not changed.
