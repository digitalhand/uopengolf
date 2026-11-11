# OpenFairwayPhysics

Native golf-ball physics, a headless shot simulator, and an Unreal ball actor. Chaos supplies contact queries; OpenFairway calculates flight, bounce and roll.

## Use

Enable the plugin, add `OpenFairwayPhysics` to your module dependencies, and use `AOpenFairwayBall` or the headless simulator. Keep the physics core in SI units; the actor handles Unreal coordinates.

## Reference

- [Quick start](Docs/Reference.md#quick-start)
- [Surfaces and greens](Docs/Reference.md#surfaces)
- [Physics profiles](Docs/Reference.md#ball-profile-and-launch-regimes)
- [Tests and fixtures](Docs/Reference.md#tests-and-golden-fixtures)
- [Known gaps](Docs/Reference.md#known-gaps)

See [validation status](../../Docs/PhasesStatus.md) for tested platforms and remaining checks.
See [licensing](../../Docs/Licensing.md) and [third-party notices](../../THIRD_PARTY_NOTICES.md) before redistributing content.
