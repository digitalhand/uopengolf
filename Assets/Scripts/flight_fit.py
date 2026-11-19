# SPDX-License-Identifier: MIT
"""Check or refit the gameplay flight table (Private/OpenFairway/SpinRatioData.h) against reference flights.

The table and the ball constants are read from the engine's headers, never copied. Every shot is flown in a copy of
the engine's flight: RK4 at 120 Hz, gravity, drag and Magnus lift on the air-relative velocity, Smits and Smith spin
decay, in its session's reference air. Drag and lift are piecewise-linear curves in the spin ratio W (surface speed
over air speed), plus a share of a modern ball's measured drag rise below Re 100k (BallData.h). Sessions come from the
shot library (shot_library.py); club labels are never read. Needs NumPy; refine also needs SciPy.

    python Assets/Scripts/flight_fit.py check
        Flies the shipping table and prints each session's carry, apex and landing-angle bias / mean absolute error.
    python Assets/Scripts/flight_fit.py fit --weight <session>=2 --weight <session>=0.5
        Refits every knot by Levenberg-Marquardt: residuals carry (3 yd), apex (5 ft) and landing angle (1.5 deg), each
        session weighted as a whole. Both curves never fall with spin, drag never drops below 0.17, lift is zero without
        spin and follows the measured low-spin lift of a modern ball below the launch data.
    python Assets/Scripts/flight_fit.py refine --target-session <session>
        Refits only the knots below --freeze-w toward one session's carries, keeping every shot launched at or above
        that spin ratio within 0.5 yd of its current carry and no session's carry error worse.

fit and refine print a candidate table; they never edit the engine. Paste it into SpinRatioData.h, then replay the
library in Unreal (shot_compare.py --build) and compare with carry_audit.py --before.
"""
import argparse
import math
from pathlib import Path
import re
import sys

try:
    import numpy as np
except ImportError:  # parsing works without NumPy; flying does not
    np = None

sys.path.insert(0, str(Path(__file__).resolve().parent))
from data_paths import ROOT  # noqa: E402
from shot_library import FEET_PER_METER, MPS_PER_MPH, PA_PER_PSI, YARDS_PER_METER, load_library  # noqa: E402

SOURCE = ROOT / "Plugins" / "OpenFairwayPhysics" / "Source" / "OpenFairwayPhysics"
SPIN_RATIO_DATA = SOURCE / "Private" / "OpenFairway" / "SpinRatioData.h"
BALL_DATA = SOURCE / "Private" / "OpenFairway" / "BallData.h"
GOLF_PHYSICS = SOURCE / "Public" / "OpenFairway" / "GolfPhysics.h"
PROFILES = SOURCE / "Public" / "OpenFairway" / "Profiles.h"

DRAG_FLOOR = 0.17          # measured modern-ball drag minimum (Lyu et al. 2020)
LOW_SPIN_LIFT = 0.115      # measured modern-ball lift at W 0.06 (Lyu et al. 2018, Fig 4a)
NUMBER = r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?"


def constants(path):
    """Every numeric constexpr in a header: scalars as floats, arrays as lists. Expressions are skipped."""
    text = Path(path).read_text(encoding="utf-8")
    values = {}
    for name, value in re.findall(r"constexpr\s+(?:float|int32)\s+(\w+)(?:\[[^\]]*\])?\s*=\s*(\{[^}]*\}|[^;]+);", text):
        value = value.strip()
        if value.startswith("{"):
            values[name] = [float(n) for n in re.findall(NUMBER, value)]
        elif re.fullmatch(NUMBER + r"f?", value):
            values[name] = float(value.rstrip("f"))
    return values


def engine():
    """The engine's flight table, measured crisis drag and ball constants."""
    table, ball, golf = constants(SPIN_RATIO_DATA), constants(BALL_DATA), constants(GOLF_PHYSICS)
    decay = float(re.search(r"SpinDecayCoefficient\s*=\s*(" + NUMBER + ")", PROFILES.read_text(encoding="utf-8")).group(1))
    return {
        "knots": table["SpinRatio"], "drag": table["Drag"], "lift": table["Lift"],
        "crisis_scale": table["CrisisScale"], "crisis_start": table["CrisisStartReynolds"],
        "crisis_re": ball["DragReynolds"], "crisis_cd": ball["Drag"],
        "radius": golf["Radius"], "mass": golf["Mass"], "gravity": golf["Gravity"], "rad_per_rpm": golf["RadPerRpm"],
        "spin_decay": decay,
    }


def density(temp_f, humidity, pressure_pa):
    """Atmosphere::Density (Atmosphere.cpp) at a measured station pressure."""
    tc = (temp_f - 32) * 5 / 9
    tk = tc + 273.15
    if humidity <= 0:
        return pressure_pa / (287.058 * tk)
    vapour = humidity * 610.78 * 10 ** (7.5 * tc / (tc + 237.3))
    return ((pressure_pa - vapour) * 0.0289644 + vapour * 0.018016) / (8.314462618 * tk)


def viscosity(temp_f):
    """Atmosphere::DynamicViscosity (Sutherland)."""
    tk = (temp_f - 32) * 5 / 9 + 273.15
    return 1.716e-5 * (tk / 273.15) ** 1.5 * (273.15 + 110.4) / (tk + 110.4)


def flight_shots(include_unreferenced=False):
    """Library shots that fly (launched above 0 deg), each with its reference air; optionally those without a carry."""
    shots = []
    for shot in load_library():
        carry = shot["ref"]["carry"]
        if (carry is None and not include_unreferenced) or shot["vla"] <= 0:
            continue
        air = shot["air"]
        temp = air["temperature_f"] if air["temperature_f"] is not None else 75.0
        pressure = air["pressure_psi"] * PA_PER_PSI if air["pressure_psi"] else 101325.0
        shots.append({
            "session": shot["session"], "shot": shot["shot"], "speed": shot["speed"], "vla": shot["vla"],
            "back": shot["back"], "side": shot["side"], "carry": carry if carry is not None else math.nan,
            "apex": shot["ref"]["apex"] if shot["ref"]["apex"] is not None else math.nan,
            "landing": shot["ref"]["landing"] if shot["ref"]["landing"] is not None else math.nan,
            "rho": density(temp, air["humidity"] or 0.0, pressure), "mu": viscosity(temp),
        })
    return shots


def model(table, knots, drag, lift, crisis_scale):
    """Drag and lift coefficients at spin ratio W and Reynolds number Re, as SpinRatioData::DragAt and LiftAt."""
    crisis_re, crisis_cd, start = np.array(table["crisis_re"]), np.array(table["crisis_cd"]), table["crisis_start"]
    lift_knots, lift_values = np.concatenate([[0.0], knots]), np.concatenate([[0.0], lift])

    def coefficients(W, Re):
        crisis = np.where(Re < start, np.maximum(np.interp(Re, crisis_re, crisis_cd) - np.interp(start, crisis_re, crisis_cd), 0.0), 0.0)
        return np.interp(np.maximum(W, 0.0), knots, drag) + crisis_scale * crisis, np.interp(W, lift_knots, lift_values)
    return coefficients


def fly(shots, table, coefficients, dt=1 / 120, max_time=14.0):
    """Carry (yd), apex (ft) and landing angle (deg) of every shot, flown together."""
    radius, mass, gravity, rad_per_rpm = table["radius"], table["mass"], table["gravity"], table["rad_per_rpm"]
    area = math.pi * radius * radius
    n = len(shots)
    speed = np.array([s["speed"] for s in shots]) * MPS_PER_MPH
    vla = np.radians([s["vla"] for s in shots])
    rho = np.array([s["rho"] for s in shots])
    mu = np.array([s["mu"] for s in shots])
    p = np.zeros((n, 3))
    p[:, 1] = radius
    v = np.stack([speed * np.cos(vla), speed * np.sin(vla), np.zeros(n)], 1)
    w = np.stack([np.zeros(n), -np.array([s["side"] for s in shots]) * rad_per_rpm, np.array([s["back"] for s in shots]) * rad_per_rpm], 1)

    def derivative(p, v, w):
        air = np.linalg.norm(v, axis=1)
        omega = np.linalg.norm(w, axis=1)
        cd, cl = coefficients(omega * radius / np.maximum(air, 1e-6), rho * air * 2 * radius / mu)
        q = 0.5 * rho * area * air / mass
        a = -q[:, None] * cd[:, None] * v
        a += np.where(omega[:, None] > 0.1, q[:, None] * cl[:, None] * np.cross(w, v) / np.maximum(omega, 1e-9)[:, None], 0)
        a[:, 1] -= gravity
        return v, a, -(table["spin_decay"] * air / radius)[:, None] * w

    carry = np.full(n, np.nan)
    landing = np.full(n, np.nan)
    apex = np.full(n, radius)
    landed = np.zeros(n, bool)
    t = 0.0
    while t < max_time and not landed.all():
        k1 = derivative(p, v, w)
        k2 = derivative(p + dt / 2 * k1[0], v + dt / 2 * k1[1], w + dt / 2 * k1[2])
        k3 = derivative(p + dt / 2 * k2[0], v + dt / 2 * k2[1], w + dt / 2 * k2[2])
        k4 = derivative(p + dt * k3[0], v + dt * k3[1], w + dt * k3[2])
        pn = p + dt / 6 * (k1[0] + 2 * k2[0] + 2 * k3[0] + k4[0])
        vn = v + dt / 6 * (k1[1] + 2 * k2[1] + 2 * k3[1] + k4[1])
        wn = w + dt / 6 * (k1[2] + 2 * k2[2] + 2 * k3[2] + k4[2])
        hit = ~landed & (pn[:, 1] < radius)
        f = np.where(hit, (p[:, 1] - radius) / np.maximum(p[:, 1] - pn[:, 1], 1e-12), 0)
        carry = np.where(hit, (p[:, 0] + f * (pn[:, 0] - p[:, 0])) * YARDS_PER_METER, carry)
        landing = np.where(hit, np.degrees(np.arctan2(-vn[:, 1], np.hypot(vn[:, 0], vn[:, 2]))), landing)
        landed |= hit
        apex = np.where(landed, apex, np.maximum(apex, pn[:, 1]))
        p, v, w = pn, vn, wn
        t += dt
    return carry, apex * FEET_PER_METER, landing


def report(shots, flown, sessions=None):
    """Each session's carry, apex and landing-angle bias / mean absolute error."""
    names = np.array([s["session"] for s in shots])
    for session in sessions or dict.fromkeys(names):
        mask = names == session
        cells = []
        for engine_values, key, label in zip(flown, ("carry", "apex", "landing"), ("carry yd", "apex ft", "landing deg")):
            error = engine_values[mask] - np.array([s[key] for s in shots])[mask]
            cells.append(f"{label} {np.nanmean(error):+.2f}/{np.nanmean(np.abs(error)):.2f}" if np.isfinite(error).any() else f"{label} -")
        print(f"{session} ({mask.sum()} shots): " + " | ".join(cells))


def print_table(knots, drag, lift, crisis_scale):
    fmt = lambda values: ", ".join(f"{value:.7f}f" for value in values)
    print("\n    constexpr int32 KnotCount = %d;" % len(knots))
    print("    constexpr float SpinRatio[KnotCount] = {" + fmt(knots) + "};")
    print("    constexpr float Drag[KnotCount] = {" + fmt(drag) + "};")
    print("    constexpr float Lift[KnotCount] = {" + fmt(lift) + "};")
    print(f"    constexpr float CrisisScale = {crisis_scale:.4f}f;\n")


# ------------------------------------------------------------------ fit: every knot, Levenberg-Marquardt

def softplus(x):
    return np.log1p(np.exp(-np.abs(x))) + np.maximum(x, 0)


def inverse_softplus(y):
    return np.log(np.expm1(np.maximum(y, 1e-6)))


def unpack(theta, n):
    """Monotonic curves by construction: drag from the floor up, lift from zero up, both by positive steps."""
    return DRAG_FLOOR + np.cumsum(softplus(theta[:n])), np.cumsum(softplus(theta[n:2 * n])), softplus(theta[2 * n])


def levenberg_marquardt(f, theta, iterations=14, step=0.01):
    damping = 1e-2
    r = f(theta)
    cost = r @ r
    for _ in range(iterations):
        jacobian = np.empty((len(r), len(theta)))
        for j in range(len(theta)):
            probe = theta.copy()
            probe[j] += step
            jacobian[:, j] = (f(probe) - r) / step
        a, g = jacobian.T @ jacobian, jacobian.T @ r
        while True:
            trial = theta - np.linalg.solve(a + damping * np.diag(np.diag(a) + 1e-9), g)
            r_trial = f(trial)
            if r_trial @ r_trial < cost:
                theta, r, cost = trial, r_trial, r_trial @ r_trial
                damping = max(damping / 3, 1e-6)
                break
            damping *= 4
            if damping > 1e6:
                return theta
        print(f"  cost {cost:.4f}", flush=True)
    return theta


def run_fit(args, table, shots):
    weights = {s["session"]: 1.0 for s in shots}
    for item in args.weight:
        session, _, value = item.partition("=")
        if session not in weights:
            sys.exit(f"No session {session}; sessions: {', '.join(weights)}")
        weights[session] = float(value)
    print(f"{len(shots)} shots: " + ", ".join(f"{s} x{w:g}" for s, w in weights.items()))
    knots = np.array(table["knots"])
    n = len(knots)
    # Start from the shipping table.
    theta = np.concatenate([inverse_softplus(np.diff(np.concatenate([[DRAG_FLOOR], table["drag"]]))),
                            inverse_softplus(np.diff(np.concatenate([[0.0], table["lift"]]))), [inverse_softplus(table["crisis_scale"])]])
    names = np.array([s["session"] for s in shots])

    def residuals(theta, smooth=2.0):
        drag, lift, crisis_scale = unpack(theta, n)
        flown = fly(shots, table, model(table, knots, drag, lift, crisis_scale))
        parts = []
        for session, weight in weights.items():
            mask = names == session
            scale = math.sqrt(weight / mask.sum())
            for engine_values, key, sigma in zip(flown, ("carry", "apex", "landing"), (3.0, 5.0, 1.5)):
                reference = np.array([s[key] for s in shots])[mask]
                parts.append(scale * np.nan_to_num(engine_values[mask] - reference) / sigma)
        parts += [smooth * np.diff(drag, 2), smooth * np.diff(lift, 2), np.array([(lift[0] - LOW_SPIN_LIFT) / 0.015])]
        return np.nan_to_num(np.concatenate(parts), nan=10.0)

    theta = levenberg_marquardt(residuals, theta)
    drag, lift, crisis_scale = unpack(theta, n)
    print_table(knots, drag, lift, crisis_scale)
    report(shots, fly(shots, table, model(table, knots, drag, lift, crisis_scale)), list(weights))


# ------------------------------------------------------------------ refine: the low-spin knots toward one session

def launch_spin_ratio(shot, table):
    return shot["back"] * table["rad_per_rpm"] * table["radius"] / max(shot["speed"] * MPS_PER_MPH, 1e-6)


def run_refine(args, table, all_shots):
    try:
        from scipy.optimize import least_squares
    except ImportError:
        sys.exit("refine needs SciPy.")
    knots = np.array(table["knots"])
    free = knots < args.freeze_w
    count = int(free.sum())
    if count == 0:
        sys.exit(f"No knot below W {args.freeze_w}.")
    drag0, lift0 = np.array(table["drag"]), np.array(table["lift"])
    target = np.array([s["session"] == args.target_session for s in all_shots])
    # Launches at or above the frozen spin ratio (irons and wedges) must keep their carry.
    protected = np.array([launch_spin_ratio(s, table) >= args.freeze_w for s in all_shots])
    if not target.any() or not protected.any():
        sys.exit(f"No shots in session {args.target_session}, or none launched at or above W {args.freeze_w}.")
    reference = np.array([s["carry"] for s in all_shots])
    apex = np.array([s["apex"] for s in all_shots])
    known = np.isfinite(reference)

    def tables(delta):
        drag, lift = drag0.copy(), lift0.copy()
        drag[free] += delta[:count]
        lift[free] += delta[count:]
        return drag, lift

    def flown(delta, dt=1 / 120):
        drag, lift = tables(delta)
        return np.array(fly(all_shots, table, model(table, knots, drag, lift, table["crisis_scale"]), dt=dt))

    baseline = flown(np.zeros(2 * count))
    print(f"{len(all_shots)} shots: {target.sum()} in {args.target_session}, {protected.sum()} protected (launch W >= {args.freeze_w})")

    def residual(delta):
        result = flown(delta, dt=1 / 60)  # fit fast, verify at 120 Hz
        error = result[0, target] - reference[target]
        movement = result[0, protected] - baseline[0, protected]
        drag, lift = tables(delta)
        return np.r_[
            error, np.maximum(np.abs(error) - args.max_error, 0) * 30,
            (result[1, target] - apex[target]) / 3,
            movement * 4, np.maximum(np.abs(movement) - args.max_movement, 0) * 30,
            np.maximum(np.abs(result[0, known] - reference[known]) - np.abs(baseline[0, known] - reference[known]), 0) * 100,
            delta * 10, np.minimum(np.diff(drag), 0) * 1000, np.minimum(np.diff(lift), 0) * 1000,
        ]

    fit = least_squares(residual, np.zeros(2 * count), bounds=(-.03, .03), diff_step=.002, max_nfev=80)
    drag, lift = tables(fit.x)
    result = flown(fit.x)
    print_table(knots, drag, lift, table["crisis_scale"])
    error = np.nanmax(np.abs(result[0, target] - reference[target]))
    movement = np.nanmax(np.abs(result[0, protected] - baseline[0, protected]))
    print(f"Largest {args.target_session} carry error {error:.3f} yd; largest protected carry change {movement:.3f} yd")
    report(all_shots, result)
    if (not np.isfinite(result[0, target]).all() or error > args.max_error or movement > args.max_movement
            or drag.min() < DRAG_FLOOR or lift.min() < 0 or np.min(np.diff(drag)) < 0 or np.min(np.diff(lift)) < 0):
        sys.exit("Candidate rejected: accuracy, preservation or monotonicity gate failed.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0], formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check", help="fly the shipping table")
    fit = commands.add_parser("fit", help="refit every knot")
    fit.add_argument("--weight", action="append", default=[], metavar="SESSION=W", help="a session's total weight (default 1)")
    refine = commands.add_parser("refine", help="refit the low-spin knots toward one session")
    refine.add_argument("--target-session", required=True, help="the session whose carries the low-spin knots move toward")
    refine.add_argument("--freeze-w", type=float, default=0.16, help="knots at or above this spin ratio stay (default 0.16)")
    refine.add_argument("--max-error", type=float, default=2.0, help="largest target carry error accepted, yd (default 2)")
    refine.add_argument("--max-movement", type=float, default=0.5, help="largest protected carry change accepted, yd (default 0.5)")
    args = parser.parse_args(argv)
    if np is None:
        sys.exit("flight_fit.py needs NumPy.")
    table = engine()
    shots = flight_shots(include_unreferenced=args.command == "refine")
    if not shots:
        sys.exit("No shots with a reference carry in the shot library; see Assets/Data/README.md.")
    if args.command == "check":
        print(f"Shipping table ({len(table['knots'])} knots), {len(shots)} shots with a reference carry, flat ground, no wind:")
        print_table(table["knots"], table["drag"], table["lift"], table["crisis_scale"])
        report(shots, fly(shots, table, model(table, np.array(table["knots"]), np.array(table["drag"]), np.array(table["lift"]), table["crisis_scale"])))
    elif args.command == "fit":
        run_fit(args, table, shots)
    else:
        run_refine(args, table, shots)
    return 0


if __name__ == "__main__":
    sys.exit(main())
