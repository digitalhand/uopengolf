# Plugins

- `OpenLaunchMonitor/`: launch-monitor input (Square Home over Bluetooth LE on
  Windows, plus replay shots). See [launch-monitor integration](../Docs/LaunchMonitorIntegration.md).

A reusable OpenFairway native C++ physics plugin is planned. See
[architecture](../Docs/Architecture.md) and the [roadmap](../Docs/Roadmap.md).

`PrivateContent/` is reserved for separately obtained restricted content plugins
and ignored by Git. Public code, maps, and default configuration must never require
those plugins. Do not force-add their files or put them in public Git LFS.
