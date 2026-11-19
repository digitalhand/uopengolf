# SPDX-License-Identifier: MIT
"""Fit the gameplay flight table (Private/OpenFairway/SpinRatioData.h) to reference flights.

Flies every shot with a reference carry in a copy of the engine's flight (RK4 at 120 Hz: gravity, drag
and Magnus lift on the air-relative velocity, Smits and Smith spin decay; it matches Unreal's carries to
0.01 yd), and fits drag and lift as piecewise-linear curves in the spin ratio W, plus a share of a
modern ball's measured drag rise below Re 100k, by Levenberg-Marquardt. Physical constraints are built
into the parameters: both curves never fall with spin, drag never drops below 0.17, lift is zero
without spin and follows the measured low-spin lift of a modern ball below the launch data.

Residuals are carry (3 yd), apex (5 ft) and landing angle (1.5 deg), each session weighted as a whole by
--weight SESSION=W (default 1). Sessions come from the shot library (data_paths.py); club labels are
never read. Needs NumPy. Prints the C++ table and each session's bias / mean absolute error.

    python Assets/Scripts/flight_fit.py --weight <session>=2 --weight <session>=0.5
"""
import argparse
import csv
import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from data_paths import shot_library_dirs  # noqa: E402

RADIUS = 0.021335          # m, GolfPhysics::Radius
MASS = 0.04592623          # kg
AREA = math.pi * RADIUS * RADIUS
GRAVITY = 9.81
RAD_PER_RPM = 0.10472
SPIN_DECAY = 2e-5          # Smits and Smith: d(omega)/dt = -2e-5 * speed / radius * omega
YD, FT = 1.0936133, 3.2808399
KNOTS = np.array([0.06, 0.10, 0.15, 0.22, 0.32, 0.50, 0.80])
DRAG_FLOOR = 0.17
LOW_SPIN_LIFT = 0.115      # measured modern-ball lift at W 0.06 (Lyu et al. 2018, Fig 4a)
# Modern-ball drag against Reynolds number near 2250 rpm (Lyu et al. 2020, Fig 3; ProV1Data.h).
CRISIS_RE = np.array([50000, 65000, 75000, 100000, 125000, 150000, 175000, 200000.0])
CRISIS_CD = np.array([.49, .33, .25, .215, .18, .17, .19, .20])


def density(temp_f, humidity, pressure_pa):
    """Atmosphere::Density (GolfPhysics.cpp)."""
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


def number(row, column, default=None):
    text = (row.get(column) or "").strip()
    return float(text) if text else default


def load_shots(include_unreferenced=False):
    """Positive launches; optionally include unreferenced recordings for preservation checks."""
    shots = []
    for root in shot_library_dirs():
        for path in sorted(Path(root).rglob("shots.csv")):
            session = path.parent.relative_to(root).as_posix()
            with path.open(newline="", encoding="utf-8") as handle:
                for row in csv.DictReader(handle):
                    carry = number(row, "carry_yd")
                    if (carry is None and not include_unreferenced) or number(row, "vla_deg", 0.0) <= 0:
                        continue
                    temp = number(row, "temperature_f", 75.0)
                    pressure = number(row, "pressure_psi")
                    shots.append({
                        "session": session, "shot": row['shot_name'], "speed": number(row, "speed_mph"), "vla": number(row, "vla_deg"),
                        "back": number(row, "backspin_rpm"), "side": number(row, "sidespin_rpm", 0.0), "carry": carry if carry is not None else math.nan,
                        "apex": number(row, "apex_ft", math.nan), "landing": number(row, "landing_angle_deg", math.nan),
                        "rho": density(temp, number(row, "humidity", 0.0), pressure * 6894.757 if pressure else 101325.0),
                        "mu": viscosity(temp),
                    })
    return shots


def softplus(x):
    return np.log1p(np.exp(-np.abs(x))) + np.maximum(x, 0)


def unpack(theta):
    n = len(KNOTS)
    return DRAG_FLOOR + np.cumsum(softplus(theta[:n])), np.cumsum(softplus(theta[n:2 * n])), softplus(theta[2 * n])


def coefficients(theta):
    drag, lift, crisis_scale = unpack(theta)
    lift_knots = np.concatenate([[0.0], KNOTS])
    lift_values = np.concatenate([[0.0], lift])

    def model(W, Re):
        crisis = np.maximum(np.interp(Re, CRISIS_RE, CRISIS_CD) - np.interp(100000.0, CRISIS_RE, CRISIS_CD), 0.0)
        return np.interp(W, KNOTS, drag) + crisis_scale * crisis, np.interp(W, lift_knots, lift_values)
    return model


def fly(shots, model, dt=1 / 120, max_time=14.0):
    """Carry (yd), apex (ft) and landing angle (deg) of every shot, flown together."""
    n = len(shots)
    speed = np.array([s["speed"] for s in shots]) * 0.44704
    vla = np.radians([s["vla"] for s in shots])
    rho = np.array([s["rho"] for s in shots])
    mu = np.array([s["mu"] for s in shots])
    p = np.zeros((n, 3))
    p[:, 1] = RADIUS
    v = np.stack([speed * np.cos(vla), speed * np.sin(vla), np.zeros(n)], 1)
    w = np.stack([np.zeros(n), -np.array([s["side"] for s in shots]) * RAD_PER_RPM, np.array([s["back"] for s in shots]) * RAD_PER_RPM], 1)

    def derivative(p, v, w):
        air = np.linalg.norm(v, axis=1)
        omega = np.linalg.norm(w, axis=1)
        cd, cl = model(omega * RADIUS / np.maximum(air, 1e-6), rho * air * 2 * RADIUS / mu)
        q = 0.5 * rho * AREA * air / MASS
        a = -q[:, None] * cd[:, None] * v
        a += np.where(omega[:, None] > 0.1, q[:, None] * cl[:, None] * np.cross(w, v) / np.maximum(omega, 1e-9)[:, None], 0)
        a[:, 1] -= GRAVITY
        return v, a, -(SPIN_DECAY * air / RADIUS)[:, None] * w

    carry = np.full(n, np.nan)
    landing = np.full(n, np.nan)
    apex = np.full(n, RADIUS)
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
        hit = ~landed & (pn[:, 1] < RADIUS)
        f = np.where(hit, (p[:, 1] - RADIUS) / np.maximum(p[:, 1] - pn[:, 1], 1e-12), 0)
        carry = np.where(hit, (p[:, 0] + f * (pn[:, 0] - p[:, 0])) * YD, carry)
        landing = np.where(hit, np.degrees(np.arctan2(-vn[:, 1], np.hypot(vn[:, 0], vn[:, 2]))), landing)
        landed |= hit
        apex = np.where(landed, apex, np.maximum(apex, pn[:, 1]))
        p, v, w = pn, vn, wn
        t += dt
    return carry, apex * FT, landing


def residuals(shots, weights, theta, smooth=2.0):
    carry, apex, landing = fly(shots, coefficients(theta))
    sessions = np.array([s["session"] for s in shots])
    parts = []
    for session, weight in weights.items():
        mask = sessions == session
        scale = math.sqrt(weight / mask.sum())
        for engine, key, sigma in ((carry, "carry", 3.0), (apex, "apex", 5.0), (landing, "landing", 1.5)):
            reference = np.array([s[key] for s in shots])[mask]
            parts.append(scale * np.nan_to_num(engine[mask] - reference) / sigma)
    drag, lift, _ = unpack(theta)
    parts += [smooth * np.diff(drag, 2), smooth * np.diff(lift, 2), np.array([(lift[0] - LOW_SPIN_LIFT) / 0.015])]
    return np.nan_to_num(np.concatenate(parts), nan=10.0)


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


def inverse_softplus(y):
    return np.log(np.expm1(np.maximum(y, 1e-6)))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--weight", action="append", default=[], metavar="SESSION=W", help="a session's total weight (default 1)")
    args = parser.parse_args(argv)
    shots = load_shots()
    if not shots:
        sys.exit("No shots with a reference carry in the shot library; see Assets/Data/README.md.")
    weights = {s["session"]: 1.0 for s in shots}
    for item in args.weight:
        session, _, value = item.partition("=")
        if session not in weights:
            sys.exit(f"No session {session}; sessions: {', '.join(weights)}")
        weights[session] = float(value)
    print(f"{len(shots)} shots: " + ", ".join(f"{s} x{w:g}" for s, w in weights.items()))
    drag0 = np.array([0.20, 0.235, 0.27, 0.31, 0.35, 0.40, 0.43])
    lift0 = np.array([0.13, 0.18, 0.25, 0.30, 0.345, 0.375, 0.40])
    theta = np.concatenate([inverse_softplus(np.diff(np.concatenate([[DRAG_FLOOR], drag0]))),
                            inverse_softplus(np.diff(np.concatenate([[0.0], lift0]))), [inverse_softplus(0.8)]])
    theta = levenberg_marquardt(lambda t: residuals(shots, weights, t), theta)
    drag, lift, crisis_scale = unpack(theta)
    fmt = lambda values: ", ".join(f"{value:.4f}f" for value in values)
    print("\n    constexpr float SpinRatio[KnotCount] = {" + fmt(KNOTS) + "};")
    print("    constexpr float Drag[KnotCount] = {" + fmt(drag) + "};")
    print("    constexpr float Lift[KnotCount] = {" + fmt(lift) + "};")
    print(f"    constexpr float CrisisScale = {crisis_scale:.4f}f;\n")
    carry, apex, landing = fly(shots, coefficients(theta))
    sessions = np.array([s["session"] for s in shots])
    for session in weights:
        mask = sessions == session
        errors = [(engine[mask] - np.array([s[key] for s in shots])[mask]) for engine, key in ((carry, "carry"), (apex, "apex"), (landing, "landing"))]
        cells = [f"{label} {np.nanmean(e):+.2f}/{np.nanmean(np.abs(e)):.2f}" if np.isfinite(e).any() else f"{label} -"
                 for label, e in zip(("carry yd", "apex ft", "landing deg"), errors)]
        print(f"{session}: " + " | ".join(cells))
    return 0


if __name__ == "__main__":
    sys.exit(main())
