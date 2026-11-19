# OpenFairwayPhysics

Native golf-ball physics, a headless shot simulator, and an Unreal ball actor. The flight is a fitted table of drag and
lift against spin ratio, integrated with RK4 at 120 Hz; impact, roll, greens and cups follow equations originally
ported from ogolf-physics, with the turf fitted to launch-monitor roll-out. Chaos supplies only contact queries.

## Use

Enable the plugin, add `OpenFairwayPhysics` to your module dependencies, and use `AOpenFairwayBall` or
`OpenFairway::ShotSimulator`. Keep the physics core in SI units; the actor handles Unreal coordinates.

## Reference

- [Quick start](Docs/Reference.md#quick-start)
- How a shot plays: [launch](Docs/Reference.md#launch), [air and wind](Docs/Reference.md#air-and-wind),
  [flight](Docs/Reference.md#flight), [impact](Docs/Reference.md#impact),
  [ground roll](Docs/Reference.md#ground-roll-and-rest), including [topped shots](Docs/Reference.md#topped-shots)
- [Surfaces](Docs/Reference.md#surfaces), [greens](Docs/Reference.md#greens) and the [cup](Docs/Reference.md#cup-and-hole-out)
- [Tests](Docs/Reference.md#tests) and [calibration](Docs/Reference.md#calibration)
- [Accuracy against reference shots](Docs/Accuracy.md)
- [Known gaps](Docs/Reference.md#known-gaps)

Reference data used to check and fit the physics stays outside the repository
([Assets/Data](../../Assets/Data/README.md#where-the-data-lives)).

See [validation status](../../Docs/PhasesStatus.md) for tested platforms and remaining checks.
See [licensing](../../Docs/Licensing.md) and [third-party notices](../../THIRD_PARTY_NOTICES.md) before redistributing content.
