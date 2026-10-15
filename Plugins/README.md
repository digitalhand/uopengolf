# Future integrations

Phase 0 has one game module and no custom runtime plugins. See
[architecture](../Docs/Architecture.md) and the [roadmap](../Docs/Roadmap.md).

Planned boundaries are a launch-monitor adapter using `open-lm-connector` and a
reusable OpenFairway native C++ physics plugin with Unreal/Blueprint wrappers.
Their APIs and connector implementation approach will be designed in Phase 1.

`PrivateContent/` is reserved for separately obtained restricted content plugins
and ignored by Git. Public code, maps, and default configuration must never require
those plugins. Do not force-add their files or put them in public Git LFS.
