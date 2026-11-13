# OpenGameUpdate

In-game updates from GitHub Releases: checks for a newer published release, downloads its installer with progress,
verifies its SHA-256 and starts it. The plugin has no UI and no game code; the host decides when to check and draws
its own screens.

## Use

Enable the plugin, add three values to `Config/DefaultGame.ini`, then read `UGameUpdateSubsystem` from the game
instance. Each release must carry the installer and a `sha256sum`-style `.sha256` file next to it.

## Reference

- [Configuration](Docs/Reference.md#configuration)
- [Quick start](Docs/Reference.md#quick-start)
- [How an update runs](Docs/Reference.md#how-an-update-runs)
- [Other update sources](Docs/Reference.md#other-update-sources)
- [Limitations](Docs/Reference.md#limitations)

UOpenGolf's installer and release pipeline are described in [Builds and releases](../../Docs/Releases.md#in-game-updates).
