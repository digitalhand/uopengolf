# SPDX-License-Identifier: MIT
"""Reproduce the constrained driver fit. Requires NumPy, SciPy and the local shot library.

Prints coefficients; never edits the engine. The original SpinRatioV1 table is the
preservation baseline. Shot names select validation cohorts only, never flight behavior.
Run from the repository root; results are written under Saved/CarryAudit.
"""
import json
from pathlib import Path
import numpy as np
from scipy.optimize import least_squares
import flight_fit as flight


def protected(shot):
    session, name = shot['session'], shot['shot']
    return (
        session == 'recorded_20261113' and any(token in name for token in ('club_pw', 'club_gw', 'club_7i', 'club_8i', 'club_9i'))
        or session == 'lm_20261113' and name.startswith(('6iron', '7iron'))
        or session == 'tm_tour_averages' and ('iron' in name or 'pw' in name)
    )


def main():
    shots = flight.load_shots(include_unreferenced=True)
    original_knots = np.array([.06, .10, .15, .22, .32, .50, .80])
    knots = np.array([.06, .08, .10, .12, .15, .16, .22, .32, .50, .80])
    drag = np.interp(knots, original_knots, [.1873, .2291, .2761, .2985, .3244, .3605, .3605])
    lift = np.interp(knots, original_knots, [.1133, .1789, .2310, .2852, .3319, .3528, .4130])
    calculator = np.array([s['session'] == 'fs_trajectory_calculator' for s in shots])
    preserve = np.array([protected(s) for s in shots])
    if not calculator.any() or not preserve.any():
        raise SystemExit('Missing calculator or preservation data; see Assets/Data/README.md.')

    def tables(delta):
        return drag + np.r_[delta[:5], np.zeros(5)], lift + np.r_[delta[5:], np.zeros(5)]

    def model(delta):
        cd, cl = tables(delta)

        def coefficients(w, re):
            crisis = np.where(re < 100000, np.maximum(np.interp(re, flight.CRISIS_RE, flight.CRISIS_CD) - .215, 0), 0)
            return (np.interp(w, knots, cd) + .6838 * crisis,
                    np.interp(w, np.r_[0, knots], np.r_[0, cl]))
        return coefficients

    baseline = np.array(flight.fly(shots, model(np.zeros(10))))
    target = np.array([s['carry'] for s in shots])
    apex = np.array([s['apex'] for s in shots])
    known = preserve & np.isfinite(target)

    def residual(delta):
        result = np.array(flight.fly(shots, model(delta), dt=1 / 60))
        error = result[0, calculator] - target[calculator]
        movement = result[0, preserve] - baseline[0, preserve]
        cd, cl = tables(delta)
        return np.r_[
            error - .7, np.maximum(np.abs(error) - 1.8, 0) * 30,
            (result[1, calculator] - apex[calculator]) / 3,
            movement * 4, np.maximum(np.abs(movement) - .45, 0) * 30,
            np.maximum(np.abs(result[0, known] - target[known]) - np.abs(baseline[0, known] - target[known]), 0) * 100,
            delta * 10, np.minimum(np.diff(cd), 0) * 1000, np.minimum(np.diff(cl), 0) * 1000,
        ]

    # Seed from the initial finer-table experiment. The optimizer freezes W >= .16.
    seed = [.00200972, -.00126906, -.00420365, -.00702861, -.03,
            .01687382, -.0013484, .00036553, -.00104465, -.00693362]
    fit = least_squares(residual, seed, bounds=(-.03, .03), diff_step=.002, max_nfev=80)
    result = np.array(flight.fly(shots, model(fit.x)))  # verify at engine's 120 Hz
    cd, cl = tables(fit.x)
    for name, values in [('SpinRatio', knots), ('Drag', cd), ('Lift', cl)]:
        print(name + ': ' + ', '.join(f'{v:.7f}f' for v in values))
    error = np.max(np.abs(result[0, calculator] - target[calculator]))
    movement = np.max(np.abs(result[0, preserve] - baseline[0, preserve]))
    print(f'Maximum calculator error: {error:.3f} yd; protected movement: {movement:.3f} yd')
    output = Path('Saved/CarryAudit/driver_fit.json')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({'delta': fit.x.tolist(), 'before': baseline.tolist(), 'after': result.tolist(), 'shots': shots}, indent=2))
    old_mae = np.mean(np.abs(baseline[0, known] - target[known]))
    new_mae = np.mean(np.abs(result[0, known] - target[known]))
    if (not np.isfinite(result).all() or error > 2 or movement > .5 or new_mae > old_mae + .001
            or cd.min() < .17 or cl.min() < 0 or np.min(np.diff(cd)) < 0 or np.min(np.diff(cl)) < 0):
        raise SystemExit('Candidate rejected: accuracy, preservation or monotonicity gate failed.')


if __name__ == '__main__':
    main()
