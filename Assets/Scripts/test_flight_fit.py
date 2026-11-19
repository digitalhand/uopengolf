# SPDX-License-Identifier: MIT
"""Engine-free checks for the flight fit: it reads the engine's table, and its flight behaves like a golf ball's."""
import unittest

import Assets.Scripts.flight_fit as flight_fit


class EngineTableTests(unittest.TestCase):
    def test_reads_the_shipping_table(self):
        table = flight_fit.engine()
        knots, drag, lift = table["knots"], table["drag"], table["lift"]
        self.assertEqual(len(knots), len(drag))
        self.assertEqual(len(knots), len(lift))
        self.assertGreaterEqual(len(knots), 2)
        self.assertEqual(knots, sorted(knots))
        self.assertTrue(all(a <= b for a, b in zip(drag, drag[1:])), "drag never falls with spin")
        self.assertTrue(all(a <= b for a, b in zip(lift, lift[1:])), "lift never falls with spin")
        self.assertGreaterEqual(min(drag), flight_fit.DRAG_FLOOR)
        self.assertGreater(table["crisis_scale"], 0.0)
        self.assertEqual(len(table["crisis_re"]), len(table["crisis_cd"]))

    def test_reads_the_ball_constants(self):
        table = flight_fit.engine()
        self.assertAlmostEqual(table["radius"], 0.021335)
        self.assertAlmostEqual(table["mass"], 0.04592623)
        self.assertAlmostEqual(table["gravity"], 9.81)
        self.assertAlmostEqual(table["rad_per_rpm"], 0.10472)
        self.assertAlmostEqual(table["spin_decay"], 2e-5)

    def test_skips_expressions(self):
        values = flight_fit.constants(flight_fit.GOLF_PHYSICS)
        self.assertNotIn("CrossSection", values)
        self.assertNotIn("SimulationDt", values)
        self.assertEqual(values["SimulationHz"], 120.0)


@unittest.skipUnless(flight_fit.np is not None, "needs NumPy")
class FlightTests(unittest.TestCase):
    def setUp(self):
        self.table = flight_fit.engine()
        np = flight_fit.np
        self.coefficients = flight_fit.model(self.table, np.array(self.table["knots"]), np.array(self.table["drag"]),
                                             np.array(self.table["lift"]), self.table["crisis_scale"])

    def shot(self, speed, vla, back):
        return {"session": "s", "shot": "x", "speed": speed, "vla": vla, "back": back, "side": 0.0, "carry": 0.0, "apex": 0.0,
                "landing": 0.0, "rho": flight_fit.density(75.0, 0.0, 101325.0), "mu": flight_fit.viscosity(75.0)}

    def test_model_matches_the_table_at_its_knots(self):
        np = flight_fit.np
        for w, drag, lift in zip(self.table["knots"], self.table["drag"], self.table["lift"]):
            cd, cl = self.coefficients(np.array([w]), np.array([150000.0]))
            self.assertAlmostEqual(cd[0], drag, places=6)
            self.assertAlmostEqual(cl[0], lift, places=6)
        cd, cl = self.coefficients(np.array([0.0]), np.array([150000.0]))
        self.assertEqual(cl[0], 0.0)

    def test_a_driver_carries_like_one(self):
        carry, apex, landing = flight_fit.fly([self.shot(150.0, 12.5, 2335.0)], self.table, self.coefficients)
        self.assertGreater(carry[0], 220.0)
        self.assertLess(carry[0], 280.0)
        self.assertGreater(apex[0], 40.0)
        self.assertGreater(landing[0], 20.0)

    def test_more_speed_carries_farther(self):
        carry, _, _ = flight_fit.fly([self.shot(100.0, 18.0, 6000.0), self.shot(120.0, 18.0, 6000.0)], self.table, self.coefficients)
        self.assertLess(carry[0], carry[1])


if __name__ == "__main__":
    unittest.main()
