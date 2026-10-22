# Plugins

- [`OpenLaunchMonitor/`](OpenLaunchMonitor/README.md): launch-monitor input (Square
  Home over Bluetooth LE on Windows and Linux, plus replay shots).
- [`OpenFairwayPhysics/`](OpenFairwayPhysics/README.md): native golf ball physics
  ported from ogolf-physics, with a ball actor that uses Chaos scene queries for contact.
- [`OpenGolfCourse/`](OpenGolfCourse/README.md): reusable course kit. Surface materials
  declare OpenFairway surfaces through physical materials; it also has a waving flagstick,
  a marked golf ball and turf grass, with editable Blender sources.

Each plugin README covers its features, API, tests, limitations and structure. See
[architecture](../Docs/Architecture.md) for how they fit together.

`PrivateContent/` is reserved for separately obtained restricted content plugins
and ignored by Git. Public code, maps, and default configuration must never require
those plugins. Do not force-add their files or put them in public Git LFS.
