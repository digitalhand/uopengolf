# Phased roadmap

Later milestones require their own design and implementation work; this document
does not authorize imports or migrations. Current status and completion evidence
for each phase are tracked in [phase status](PhasesStatus.md).

Windows and Linux are both supported platforms: each phase's exit requires both.

## Phase 0 — UOpenGolf foundation

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
- Pin reference revisions of `open-lm-connector`, `ogolf-physics` (which superseded
  `openfairway`), and `ogolf`.
- Evaluate reusing the connector through a bridge versus native adaptation on both
  platforms. Record the chosen approach and platform dependencies in a design note.
- Establish shot units, axes, handedness, spin conventions, timestamps, optional
  measurements, late club-data association, threading, and connection ownership.
- Identify C# physics regression fixtures, fixed-step behavior, and numerical tolerances.

**Exit:** UOpenGolf works on Windows and Linux; a written integration design and
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

- Port the C# numerical calculations (from `ogolf-physics`) to a reusable C++ core in a
  physics plugin.
- Add Unreal collision/surface adapters and Blueprint-callable controls and events.
- Keep simulation ownership explicit; avoid competing Unreal/Chaos ball integration.
- Compare flight, spin, impact, bounce, roll and rest to pinned C# fixtures. Validate
  coordinate conversion, units, timestep behavior and numerical tolerances on both OSes.

**Exit:** native simulation passes the agreed baseline corpus on Windows and Linux;
both C++ and Blueprint callers can use the Unreal wrapper.

## Phase 4 — Greens and surfaces

- Reference `ogolf` concepts/code for contour generation and green speed,
  matching rendered and collision terrain, lies, and contact-surface lookup.
- Supply surface/contact data to OpenFairway; keep impact/rollout formulas in the core.
- Add a minimal practice surface with green, fringe, rough and sand zones.

**Exit:** putting, slopes, surface transitions, contact normals, visual/collision
agreement and rest behavior have meaningful regression coverage on both platforms.

## Phase 5 — End-to-end golf slice

- Connect measured/replayed shots to native physics, course contact, ball presentation,
  basic shot information, and reset flow.
- Add Windows and Linux cooking/packaging automation and package startup checks
  (`ue.sh package` / `ue.ps1 Package`, added 2026-10-18).

**Exit:** a Square Home shot completes from measurement to ball rest on each platform;
the same flow passes with replay input, and public packages run without private content.

## Phase 6 — Community and content expansion

- Expand original courses/assets, gameplay and contributor documentation incrementally.
- Add optional separately maintained private content packs without required public references.
- Establish maintainable Windows/Linux CI and release validation as infrastructure permits;
  provision Unreal separately and respect its distribution terms.

**Exit:** public builds remain independently usable, hardware-free tests support
contributors, and both platform release checks are documented and reproducible.

## Technical debt

- **Tree collision is too broad (2026-10-20).** The jacaranda's hidden wood proxy collides on
  the trunk and every branch, down to small twigs, so a ball brushing the canopy can deflect
  sharply. The owner wants ball collision on the trunk only. Likely approach: build the proxy
  from the trunk section alone, or from a simplified trunk shape such as a few capsules along
  the main stems. Then rerun the wood sweeps and actual Play shots. Code:
  `Plugins/OpenGolfSavanna/Scripts/tree_wood_collision.py` and the `WOOD` slots in the
  private range's `private_jacaranda.py`. Status in
  [Phase status](PhasesStatus.md#jacaranda-applied-in-place--2026-10-20).
- **The rough's edge row reads as a darker band (2026-10-21).** Seen from a distance, the
  first 0.4 m of rough beside the fairway looks darker and smoother than the tiles behind it,
  so the 3D grass does not appear to meet the fairway exactly. That row is 1 x 0.25 m edge
  strips, about 70% of the tiles' height, starting 0.15 m outside the outline; the first
  0.15 m is bare rough ground. Options: taller strips or a closer clearance (blades may lean
  over the fairway, as on a real course), Nanite `PreserveArea` on the strips, or tinting the
  strip material to match the tiles. Recheck with the owner's high side view, then rerun the
  boundary test. Code: `ScatterBoundary` in
  `Plugins/OpenGolfCourse/Source/OpenGolfCourse/Private/OpenGolfCourseGrass.cpp` and
  `DA_BoundaryGrass` in `Plugins/OpenGolfSavanna/Scripts/BuildSavannaGrassPresets.py`.
- **Three range tests still check the flat range (2026-10-21).** On the savanna public map,
  `UOpenGolf.Range.EditableLayout`, `.Presentation` and `.RolloutSurfaces` fail because they
  assert the old flat-box range, whose boxes left the map with the coastal terrain:
  - `EditableLayout`: more than 20 layout pieces, and rough collision at fixed points
  - `Presentation`: a tee on `M_Turf`
  - `RolloutSurfaces`: lies at flat-layout offsets, and a flat headless rollout (37.69 yd). Its
    121.6-yard replay shot now lands in the moved bunker and stops.

  The owner chose to leave them failing for now. Options: point them at a small flat test map
  generated by `Scripts/BuildRangeMap.py`, which keeps the flat-rollout calibration and the
  layout builder covered, or rewrite them for the terrain course. Code:
  `Source/UOpenGolf/Private/Tests/{RangeLayoutTest,RangePresentationTest,RolloutSurfaceTest}.cpp`.
  Results in [Phase status](PhasesStatus.md#savanna-range-on-the-public-map--2026-10-21).
