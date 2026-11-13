# Architecture

## Current foundation

The game module is `UOpenGolf`. Its C++ GameMode uses the map's saved range controller
(`AUOpenGolfRange`) and spawns a game-side `AUOpenGolfBall` on its tee. The terrain owns the ground. The flagstick and lighting are saved actors, selectable before Play. Multiple controllers are rejected to prevent duplicate
scenes. Range shots start from the tee; hole play uses the current lie and counts strokes and putts.
In range mode, the GameMode returns a stopped ball to the tee after the configured delay; the ball keeps
its last shot result until the next launch. The HUD
adds a Slate widget (`SRangeHud`) with shot tiles and a launch-monitor status box; the player controller
binds keys (C connect, T disconnect, H replay shot, Tab next club, R tee up, A/D held
to turn the aim) and runs the shot camera along the ball's aim line. The keys are Enhanced Input
actions and one mapping context built in code (no input assets); a HUD panel that holds keyboard
focus hands its keys to the controller (`ForwardUIKey`), so they act as they would from the viewport. There is no player pawn. Hole play supports a par 4, practice placement and gimmies. `UOpenGolf`
is the saved range map used for editor and game startup, and the project's only map. A world
without a range controller gets one with daylight and a flag; the ground always comes from a
terrain area.

The top-center wind readout shows whole mph and an arrow pointing where wind blows relative
to the camera. Down means headwind, up means tailwind. It uses the ball's effective wind,
including the captured wind during a moving shot, and falls back to the range without a ball.
Open `/Game/UOpenGolf/UI/WBP_WindIndicator` in the Widget Blueprint Designer to preview
it before Play. Select its WindIndicator child and edit Wind Speed Mph, Wind From Deg and
View Yaw Deg to try directions. The default preview is 10 mph from the right. The saved
Canvas sets the top-center position; gameplay embeds this same asset in the Slate HUD.
The native `UOpenGolfWindIndicator` wraps `SWindIndicator`, sharing the drawing in both views.
The editor command `uopengolf.CreateWindIndicatorAsset` creates the asset only if missing.

A shot result includes `TerminationReason` (`None`, `Rest`, or `SafetyStop`). A safety stop
still ends motion and schedules a re-tee, but the HUD labels it interrupted and does not show
completed total/roll or invent a carry without landing. Recordings add the reason and use null
for unavailable completed distances; existing fields and normal shot behavior are retained.

Shot recording is a game-owned service, independent of Slate and the world. It tracks
measured shot IDs separately from simulation IDs, accepts club data after ball rest,
and closes the club-data window three seconds after receipt. Completed records use
same-directory temporary writes followed by a rename. Failed saves retain their
session and file index in memory and retry during Play, even after recording is
switched off; shutdown makes a final attempt. This queue does not recover across
Play sessions or application restarts. The HUD exposes its saved count and failures.

The range builds from the `OpenGolfCourse` plugin, the course/surface layer. A surface is
declared by the physical material on the terrain's surface meshes (`PM_Fairway`, `PM_Green`,
`PM_Sand`, `PM_Rough` and the other `UOpenFairwayPhysicalMaterial` assets), and the ball reads
the contact's physical material. Component or actor `OpenFairway.Surface.*` tags remain an
explicit override. No replay-specific surface override is used.

The course kit owns the CC0 grass textures, golf ball, waving flagstick and turf
materials, with editable Blender sources (see the
[OpenGolfCourse README](../Plugins/OpenGolfCourse/README.md)). The game keeps the tracer
material in `Content/UOpenGolf/Art/`. The ball subclass
normalizes the visual mesh's actual bounds to a 42.67 mm diameter and owns a
collision-free procedural ribbon with a translucent material. It samples the
simulated path and fades during rollout; launch/reset clears the ribbon. CPU buffers
are reused, topology is recreated only when sample count changes, and camera-facing
vertices update an existing section otherwise. Unchanged geometry, opacity and
visibility updates are skipped. The inherited debug tracer is disabled on the game ball. Ground collision comes from the terrain's
generated surface meshes and outer landscapes, which reach beyond the ball's 1,000 m safety envelope.
The game ball sweeps and probes on its own trace channel, `GolfBall` (`AUOpenGolfBall::ContactChannel`,
set in `Config/DefaultEngine.ini`), rather than Visibility: every collision profile answers it as it
answers Visibility, except 3D UI widgets, which the ball ignores. The range's ground queries use the
same channel; the physics plugin's own default stays Visibility.
The terrain's 3D grass and ground cover are collision-free instances; `uopengolf.Grass3D` hides or
shows them at runtime. The physics plugin has no
dependency on these assets or on the game module; the course kit depends on the physics
plugin for the surface type, and the game depends on both.

The saved range exposes `WindSpeedMph`, `WindFromDeg` and `bPreviewWindInEditor` under
UOpenGolf → Wind in Details. Before Play, the range's wind drives the flag preview when
Realtime is enabled; it uses the default air density. During Play, authored settings
initialize the ball, and the range sends the ball's wind at flag height and air density to the cloth.
Changed `uopengolf.WindMph` and `uopengolf.WindFromDeg` commands update the Play-world
range controls. Console values left from a previous Play session do not replace the
map's settings on startup. A moving shot retains its launch environment, so the flag
keeps showing that wind until the shot ends. Default wind is zero.
`WindDirectionalSource` is not connected. See the
[wind validation steps](RangeAssets.md#validate-wind-and-flag-movement).

Wind has one owner per world:
- **Ball.** The range's controls set the ball's wind, and the ball is authoritative because its
  physics uses that wind. Its speed is the 10 m reading, which the log profile slows near the
  ground.
- **Scenery.** Every frame the range copies the ball's wind, air density and height profile to the
  world's `UOpenGolfWindSubsystem` (course kit). Before there is a ball, it copies the saved
  controls.
- **Vegetation.** The subsystem writes `MPC_Wind`. Vegetation materials read it through
  `MF_VegetationWind`, so trees and tall grass need no actor of their own. In the editor they see
  wind only while **Preview Wind in Editor** is on.
- **Flag.** The range gives the cloth the same wind, slowed to the flag's centre height.

A moving shot keeps its launch environment, so the ball, flag and vegetation all keep that wind
until the shot ends. `uopengolf.WindDebug 1` shows the wind at each height against the Beaufort
scale.

Green speed is the ball's `GreenStimpFeet` (8–15, default 10); during Play,
`uopengolf.GreenStimp` changes it from the next shot. OpenFairway solves the green's rolling
friction from the stimp together with the green's grass drag, so the range supplies only the
setting and no green formula. See
[Surfaces](../Plugins/OpenFairwayPhysics/Docs/Reference.md#surfaces).

The `OpenLaunchMonitor` runtime plugin is the launch-monitor adapter:

| Layer | Location (`Plugins/OpenLaunchMonitor/Source/OpenLaunchMonitor/`) | Notes |
| --- | --- | --- |
| Game boundary | `Public/OpenLaunchMonitorSubsystem.h`, `Public/LaunchMonitorTypes.h` | Game-instance subsystem; Blueprint and C++ events for state, ball ready, battery, shots, club data and errors; replay input |
| Square session | `Private/Square/SquareSession.*` | Game-thread state machine: scan, handshake, heartbeat, arming, duplicate filtering, late club data, link loss |
| Square protocol | `Private/Square/SquareProtocol.*` | Frame decoding and commands; depends only on Core (and Json for firmware text) |
| Transport | `Private/Bluetooth/GattTransport.h`, `Private/Windows/`, `Private/Linux/` | Bluetooth LE GATT interface; C++/WinRT on Windows, BlueZ over runtime-loaded sd-bus on Linux (`Private/Bluetooth/BlueZModel.*` holds its platform-neutral mapping logic) |

Both transports run every Bluetooth call on one worker thread and bound waits to ten
seconds per operation and thirty seconds for device opening/service discovery, checking
cancellation at most every 25 ms while waiting. Disconnect invalidates queued work and
old callbacks, and doesn't wait for the device to answer; shutdown cooperatively stops
the worker. Synchronous OS/driver cleanup is outside those wait limits. See [launch-monitor integration](LaunchMonitorIntegration.md) for contracts
and remaining hardware verification.

The `OpenFairwayPhysics` runtime plugin is the physics core and its Unreal wrapper:

| Layer | Location (`Plugins/OpenFairwayPhysics/Source/OpenFairwayPhysics/`) | Notes |
| --- | --- | --- |
| OpenFairway core | `Public/OpenFairway/`, `Private/OpenFairway/` | Flight, impact, ground and rest equations, surfaces, ball profile and launch regimes, headless `ShotSimulator`; depends only on Core, SI units in its own right-handed frame |
| Unreal wrapper | `Public/OpenFairwayBall.h`, `Public/OpenFairwayTypes.h`, `Public/OpenFairwayPhysicalMaterial.h` | Ball actor stepping the core at 120 Hz; Chaos sphere sweeps and line traces for contacts; surfaces from physical materials or tags; presentation-only visual roll; Blueprint launch, results and events |

Chaos only answers contact queries; the ball never simulates as a Chaos rigid body, so
OpenFairway owns the motion. See the [plugin README](../Plugins/OpenFairwayPhysics/README.md).
The launch-monitor and physics plugins do not depend on each other; the game module converts
shots to launches. The `OpenGolfCourse` plugin (course kit) depends only on the physics plugin.
The ball actor retains a 120 Hz fixed step and a twelve-step frame cap. Long stalls
discard excess catch-up time, so sustained frame rates below about 10 FPS slow
simulation relative to wall time. The game tests cover frame-rate/hitch equivalence;
rendered CPU measurements and their limits are recorded in
[phase status](PhasesStatus.md#reference-measurements).

The game's Slate UI lives in feature folders under `Source/UOpenGolf/Private/UI/`, included
by path from `Private` (`#include "UI/Club/ClubPicker.h"`):

| Folder | Contents |
| --- | --- |
| `Common/` | Styles and shared widgets: `HudStyle`, `MenuStyle`, `HudScale`, `HudLayers`, icon button, slider, switch, status glyph, page header |
| `Hud/` | The Training HUD: `SRangeHud` (shot tiles, status box), play buttons and hole tiles, shot formatting, ball-ready glow, test-shot list, keyboard/gamepad navigation (`FHudNavigation`) |
| `Club/` | Club picker and club-face glyph |
| `Wind/` | Wind indicator, its controls and readout, the `uopengolf.WindDebug` overlay, and the UMG wrapper `UOpenGolfWindIndicator` (its UCLASS header stays in `Public/`) |
| `Menu/` | `SMainMenu` (home, pause, Settings, Shot history, dialogs) and display-mode options |

A widget used by more than one area goes in `Common/`; a new screen or HUD feature gets its
own folder. Raw-pointer Slate arguments always get a `nullptr` default, which
`UOpenGolf.UI.ArgumentDefaults` checks.

Game and editor targets use UnrealBuildTool's standard C# configuration files.
Gameplay remains native C++. Python is used only by developer utilities.

## Responsibilities

| Component | Owns | Must remain separate from |
| --- | --- | --- |
| Game/session layer | Shot orchestration, measured shot records, sessions, UI and Blueprint events | Device protocol details and numerical integration |
| Launch-monitor adapter | `open-lm-connector` integration, connection state, device configuration, measured ball and optional club data | Course rules and physics tuning |
| OpenFairway C++ core | Flight, spin, impact, roll and rest calculations | Unreal worlds, rendering, device transports and UI |
| Unreal physics wrapper | Scheduling, collision/contact queries, native core adaptation, Blueprint controls/events | Monitor-specific packets |
| Course/surface layer | Geometry, collision, lies, contact surface resolution and green configuration | Duplicate impact/rollout formulas |
| Presentation/content | Ball visuals, HUD, camera and optional content packs | Authoritative simulation calculations |

These are architectural responsibilities, not frozen APIs. The launch-monitor
adapter, OpenFairway core and Unreal physics wrapper now exist as the two plugins
above; the game/session layer is the range mode in `UOpenGolf`. The course/surface
layer is the OpenGolfCourse plugin (surface materials, flagstick, ball, turf) and the
saved savanna range.

```mermaid
flowchart LR
    Square[Square Home] --> Connector[open-lm-connector integration]
    Connector --> Shots[Game shot orchestration]
    Mock[Mock / replay input] --> Shots
    Shots --> Physics[OpenFairway C++ through Unreal wrapper]
    Course[Course geometry and surfaces] --> Physics
    Physics --> View[Ball presentation and UI]
    Shots --> View
```

Every node exists in a first form: replay is built in, physics is the OpenFairway
plugin, course geometry is the saved terrain built from the OpenGolfCourse kit, and presentation
is the ball, tracer, shot camera and Slate HUD (shot tiles and launch-monitor status box).

## Reference projects

- `open-lm-connector`: Godot-facing C# entry points wrap Square session/protocol code
  and Bluetooth abstractions. Windows uses WinRT; Linux uses BlueZ. Its protocol,
  session and both transports were ported natively to C++ from pinned revisions;
  no .NET runtime or bridge process is used.
- `ogolf-physics`: the maintained C# golf physics (a successor of `openfairway`'s
  addon) with a Godot-free core and calibration data. Its core, profiles, surfaces and
  headless shot loop were ported to C++ from revision `e6d793a`; golden fixtures come
  from its C# sources through an optional .NET harness.
- `openfairway`: the earlier C# physics; superseded by `ogolf-physics` and not ported.
- `ogolf`: reference for contour/height data shared by rendering and collision, lie
  mapping, and resolving surfaces at actual contact points. Physics calculations
  belong to OpenFairway.

Current physics defaults add speed-dependent flight spin decay and continuous
interpolation of calibration scales and landing responses. The explicit
`HistoricalReference()` profile preserves the pinned C# behavior for golden tests;
parity with that profile is not independent physical validation. See the
[FlightScope analysis](FlightScopeAnalysis.md) for the changes and measurements.

The sibling repositories are not required, modified, or linked by the build. The
connector port is described in [launch-monitor integration](LaunchMonitorIntegration.md);
the physics port in the [OpenFairway Physics README](../Plugins/OpenFairwayPhysics/README.md).

## Data and simulation contracts

[Launch-monitor integration](LaunchMonitorIntegration.md) defines device units, optional
measurements, signs, threading and shot association. The [physics reference](../Plugins/OpenFairwayPhysics/Docs/Reference.md#units-and-coordinate-frames)
defines coordinate conversion and fixed-step ownership. Preserve measured input separately
from game-side lie adjustments. Historical fixtures check the port; they do not establish
physical accuracy.

## Windows and Linux

Both are supported targets. Shared game and physics code must stay portable;
Bluetooth/platform dependencies belong behind the connector boundary. Case-correct
paths, explicit engine locations, and equivalent Bash/PowerShell workflows are required.
Replay input supports development without hardware.

Track actual validation independently for each OS in [phase status](PhasesStatus.md).
A build or test run on one OS does not establish compiler, rendering, Bluetooth or
packaged-game behavior on the other. `ue.sh package` / `ue.ps1 Package` give both a
package and start check from one command.

## Content boundary

Maps, code and defaults reference only distributable project content or engine resources.
Restricted packs are never committed.

The game mode's editable `RangeClass` defaults to the native range; a map's game-mode subclass can
choose a range subclass.

### Before publishing

The repository is public, so machine and account details stay out of it:

- The editor's per-user folders (`Content/Developers/<account>/`) are ignored.
- Machine-specific settings, such as paths in editor terminal startup commands, go in the
  ignored per-user layer (`Saved/Config/<Platform>Editor/`) or `Config/User*.ini`, never in
  `Config/Default*.ini`.
- An imported asset records the path of its source file, which includes the importing
  account's home folder. Existing assets carry such paths; they grant no access.
- No credential belongs in the tree. The Auth0 client ID in `Config/DefaultGame.ini` is a
  public native-application identifier, and there is no client secret.
