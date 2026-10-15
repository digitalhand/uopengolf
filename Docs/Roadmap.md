# Phased roadmap

Later milestones require their own design and implementation work; this document
does not authorize imports or migrations. Current status and completion evidence
for each phase are tracked in [phase status](PhasesStatus.md).

Phases 1 and 2 are split into a **Windows track** (current) and a **Linux track**
(deferred, not blocking Windows work). Each phase's exit still requires both tracks.

## Phase 0 — Hello-world foundation

- Create an Unreal 5.8.3 native C++ game with a blank startup map and visible greeting.
- Add original-work MIT licensing, third-party/asset policy, architecture, contributor
  guidance, and a README with a maintained linked table of contents.
- Supply Windows PowerShell and Linux Bash build/run/smoke commands. Keep Linux
  portability in the project from the beginning.
- Add one startup automation test with report validation and timeout handling.

**Exit:** Windows editor build, Play-mode greeting, and automation smoke verified.
Linux commands reviewed and explicitly unverified until run there. Public project
requires no sibling repositories, purchased assets, or hardware.

## Phase 1 — Linux baseline and integration design

- Build and run the foundation with native Linux Unreal 5.8.3, and validate its smoke test.
- Pin reference revisions of `open-lm-connector`, `openfairway`, and permitted `ogolf`
  references. Review licenses before any copying.
- Evaluate reusing the connector through a bridge versus native adaptation on both
  platforms. Record the chosen approach and platform dependencies in a design note.
- Establish shot units, axes, handedness, spin conventions, timestamps, optional
  measurements, late club-data association, threading, and connection ownership.
- Identify C# physics regression fixtures, fixed-step behavior, and numerical tolerances.

**Exit:** hello world works on Windows and Linux; a written integration design and
baseline fixtures are ready for the subsequent implementation phases. No bulk migration.

## Phase 2 — Launch-monitor input

- Integrate `open-lm-connector`, initially testing with **Square Home**.
- Add mock/replay input through the same game boundary and expose connection/shot events
  to Blueprints. Test event delivery without a simulator dependency.
- Handle reconnect, malformed input, absent measurements, and delayed club data.
  Do not fabricate Home measurements the device does not supply.
- Validate both Windows Bluetooth and Linux BlueZ paths on hardware; document tested
  firmware, permissions, dependencies, and outstanding platform gaps.

**Exit:** real Square Home shots and connection lifecycle verified on both platforms;
hardware-free replay works. Physics and golf-course implementation are not required yet.

## Phase 3 — Native OpenFairway

- Port the C# numerical calculations to a reusable C++ core in a physics plugin.
- Add Unreal collision/surface adapters and Blueprint-callable controls and events.
- Keep simulation ownership explicit; avoid competing Unreal/Chaos ball integration.
- Compare flight, spin, impact, bounce, roll and rest to pinned C# fixtures. Validate
  coordinate conversion, units, timestep behavior and numerical tolerances on both OSes.

**Exit:** native simulation passes the agreed baseline corpus on Windows and Linux;
both C++ and Blueprint callers can use the Unreal wrapper. Preserve upstream notices.

## Phase 4 — Greens and surfaces

- Reference permitted `ogolf` concepts/code for contour generation and green speed,
  matching rendered and collision terrain, lies, and contact-surface lookup.
- Supply surface/contact data to OpenFairway; keep impact/rollout formulas in the core.
- Add a minimal practice surface with green, fringe, rough and sand zones.

**Exit:** putting, slopes, surface transitions, contact normals, visual/collision
agreement and rest behavior have meaningful regression coverage on both platforms.

## Phase 5 — End-to-end golf slice

- Connect measured/replayed shots to native physics, course contact, ball presentation,
  basic shot information, and reset flow.
- Add Windows and Linux cooking/packaging automation and package startup checks.

**Exit:** a Square Home shot completes from measurement to ball rest on each platform;
the same flow passes with replay input, and public packages run without private content.

## Phase 6 — Community and content expansion

- Expand original courses/assets, gameplay and contributor documentation incrementally.
- Add optional separately maintained private content packs without required public references.
- Establish maintainable Windows/Linux CI and release validation as infrastructure permits;
  provision Unreal separately and respect its distribution terms.

**Exit:** public builds remain independently usable, hardware-free tests support
contributors, and both platform release checks are documented and reproducible.
