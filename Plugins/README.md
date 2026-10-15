# Plugins

- [`OpenLaunchMonitor/`](OpenLaunchMonitor/README.md): launch-monitor input (Square
  Home over Bluetooth LE on Windows, plus replay shots).
- [`OpenFairwayPhysics/`](OpenFairwayPhysics/README.md): native golf ball physics
  ported from ogolf-physics, with a ball actor that uses Chaos scene queries for contact.

Each plugin README covers its features, API, tests, limitations and structure. See
[architecture](../Docs/Architecture.md) for how they fit together.

`PrivateContent/` is reserved for separately obtained restricted content plugins
and ignored by Git. Public code, maps, and default configuration must never require
those plugins. Do not force-add their files or put them in public Git LFS.
