# Open Launch Monitor

Launch-monitor input for Unreal Engine 5.8: a native C++ port of the
[open-lm-connector](https://github.com/digitalhand/open-lm-connector) Square protocol and
session, Bluetooth LE transports for Windows and Linux, and hardware-free replay shots, all delivered
through one game-instance subsystem with Blueprint and C++ events.

Part of [uopengolf](../../README.md). open-lm-connector is the maintainer's own MIT
project; the port is covered by this repository's MIT license.

## Table of contents

- [Status](#status)
- [Features](#features)
- [Requirements](#requirements)
- [Enabling the plugin](#enabling-the-plugin)
- [Quick start](#quick-start)
  - [Blueprint](#blueprint)
  - [C++](#c)
- [Connection lifecycle](#connection-lifecycle)
- [Shot and club data](#shot-and-club-data)
- [Replay shots](#replay-shots)
- [Threading and ownership](#threading-and-ownership)
- [Tests](#tests)
- [Known limitations](#known-limitations)
- [Plugin structure](#plugin-structure)
- [License](#license)

## Status

| Area | Windows | Linux |
| --- | --- | --- |
| Square protocol and session | Working; tested with a real Square Home (2026-10-14) | Same code; working with a real Square Home (2026-10-17) |
| Bluetooth transport | C++/WinRT; connection, battery, firmware, shot and club data verified | BlueZ via runtime-loaded sd-bus; connection, battery, firmware, shot and club data verified (BlueZ 5.85, 2026-10-17) |
| Replay shots | Working | Working; replay and range tests pass |
| Club selection and club toggle on hardware | Verified on a real Square Home (2026-10-15) | Verified (2026-10-17) |
| Reconnect after link loss on hardware | Implemented; not yet tested on hardware | Verified by power-cycling the Square (2026-10-17) |
| Disconnect and quit while connected | Not separately tested on hardware | Verified (2026-10-17); changed 2026-10-18 to not wait for the device, hardware recheck pending |

Firmware and battery are logged on connect (`Square Home firmware <x>.`, `Battery <n>%.`)
on both platforms.

Ported from open-lm-connector revision `92eb021abe28d8eb1abe43f87767d3eb73e5939e`. Design
notes: [launch-monitor integration](../../Docs/LaunchMonitorIntegration.md).

## Features

- Square Home connection: scan, connect, handshake, heartbeat, arm, and re-arm after each shot.
- Shot data with a source for each value: measured, derived (for example back/side spin from
  total spin and axis) or missing. Missing values are never reported as zero.
- Club data (path, face to target, attack angle, dynamic loft, impact location) matched to its
  shot, with per-field flags.
- Club and handedness selection sent to the device.
- Nine built-in replay shots (both FlightScope comparisons, varied synthetic shots and hardware captures) and
  arbitrary replay frames in hex,
  through the same events as the device.
- Automatic reconnect with backoff after a dropped link.

## Requirements

- Unreal Engine 5.8 with C++.
- Windows: the Windows SDK's C++/WinRT headers (`Include/<version>/cppwinrt`), installed with
  the SDK. The plugin's `Build.cs` adds them and `WindowsApp.lib` to this module only.
- For a real device: Windows 10/11 with Bluetooth LE on, and a Square Home. No pairing or
  extra permissions are needed. Replay shots need no hardware.

## Enabling the plugin

The plugin is enabled in `UOpenGolf.uproject`. In another project, copy
`Plugins/OpenLaunchMonitor/`, add `{"Name": "OpenLaunchMonitor", "Enabled": true}` to the
`.uproject`, and add `"OpenLaunchMonitor"` to the dependent module's `Build.cs`.

## Quick start

### Blueprint

1. Get the **Open Launch Monitor Subsystem** from the game instance.
2. Bind **On Shot Received**, and optionally **On Club Data Received**,
   **On Connection State Changed**, **On Ball Ready Changed** and **On Error**.
3. Call **Connect**, or **Inject Replay Shot** to test without hardware.

### C++

```cpp
#include "OpenLaunchMonitorSubsystem.h"

UOpenLaunchMonitorSubsystem* LaunchMonitor = GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
LaunchMonitor->OnShotReceivedNative.AddLambda([](const FLaunchMonitorShot& Shot)
{
    UE_LOG(LogTemp, Log, TEXT("%.1f mph, VLA %.1f"), Shot.GetBallSpeedMph(), Shot.VerticalLaunchDeg);
});
LaunchMonitor->Connect();          // real device
LaunchMonitor->InjectReplayShot(); // or a built-in replay
```

In uopengolf, `AUOpenGolfGameMode` turns each shot into an OpenFairway launch and hits it
on the range (see [OpenFairway Physics](../OpenFairwayPhysics/README.md)).

## Connection lifecycle

`Disconnected → Scanning → Connecting → Connected → Ready`

- **Scanning:** looks for a device whose name starts with the Square prefix; stops after 30 s.
- **Connecting / Connected:** reads battery and firmware, subscribes to events, starts the
  heartbeat, sends the club after 2 s and arms after 3 s.
- **Ready:** armed; the next shot is reported. The device re-arms 3 s after each shot,
  including unreadable strikes.
- **Link loss:** reports an error and, with `bAutoReconnect` (default on), retries with
  2–30 s backoff up to five times.
- **Cancellation / failure:** disconnect invalidates queued work and old callbacks;
  shutdown cooperatively stops the worker. Async operations have a 10 s deadline,
  and opening the device/resolving characteristics has a 30 s overall deadline.
  See [Threading and ownership](#threading-and-ownership) for limits and verification.

## Shot and club data

`FLaunchMonitorShot` uses the device's units: ball speed in m/s (`GetBallSpeedMph()` for
mph), angles in degrees, spin in rpm. Positive HLA, spin axis and side spin are to the
right. Each value has an `ELaunchMonitorValueSource`.

| Field | Unit | Notes |
| --- | --- | --- |
| `BallSpeedMps` | m/s | |
| `VerticalLaunchDeg`, `HorizontalLaunchDeg` | degrees | |
| `TotalSpinRpm`, `SpinAxisDeg` | rpm, degrees | |
| `BackSpinRpm`, `SideSpinRpm` | rpm | Derived from total and axis when not measured |
| `ShotType` | | Full, Putt or Unknown (from the device's metadata byte) |
| `ShotNumber`, `ReceivedUtc`, `Source` | | Per game instance; `Source` is "Square Home" or "Replay" |

`FLaunchMonitorClubData::ShotNumber` names the shot it belongs to. Club data arrives up to
about a second after the shot; fields the device did not measure have their `bHas…` flag
cleared, and `HasReading()` is false for an untracked shot (no club sticker). The
session accepts one matching club response until re-arming three seconds after the shot.

Recording belongs to the game module. Its recorder associates monitor shot IDs with
simulation IDs, waits for delayed club data after short shots, and retries failed saves
during Play. See [Recording shots](../../README.md#recording-shots) and
[Recording errors and retries](../../README.md#recording-errors-and-retries).

## Replay shots

- `GetReplayShotNames()` and `GetReplayShotDescription(Index)` expose the catalog and
  decoded inputs without firing shot events. Descriptions identify synthetic versus
  captured shots and available FlightScope references.
- `InjectReplayShotAt(Index)` delivers a chosen preset; an invalid index returns false
  without firing an event. Repeating an index repeats the inputs with a new shot number.
- `InjectReplayShot()` still cycles through the catalog. The game's **Test shots [P]**
  dropdown instead selects an index; **H** repeats that selection. These controls are
  in the game module, keeping the plugin independent of Slate and physics.
- `InjectReplayFrames(BallHex, ClubHex)` decodes your own frames; malformed input is
  reported through `OnError` and not delivered.

| Index | Preset | Source |
| ---: | --- | --- |
| 0 | Low wood, 114.5 mph / 6.95° / 1933 rpm | Synthetic; 23:05 screenshot, FS carry 119.8 yd |
| 1 | Square Home, 57.5 mph | Recorded ball and club frames |
| 2 | Square Home putt | Recorded, untracked club data |
| 3 | Mid iron, 75.7 mph / 20.78° / 4463 rpm | Synthetic; 22:05 screenshot, FS carry 87.6 yd, HLA mismatch noted |
| 4 | Driver, 150 mph / 12.5° / 2335 rpm | Synthetic |
| 5 | Iron, 102.1 mph / 17.4° / 5391 rpm | Synthetic |
| 6 | Wedge, 66.4 mph / 23.2° / 6449 rpm | Synthetic |
| 7 | Chip, 24.7 mph / 17.94° / 3204 rpm | Synthetic |
| 8 | High flop, 68.1 mph / 45° / 12000 rpm | Synthetic |

Speeds are rounded for display; Square packets store 0.01 m/s increments. Captures
come from open-lm-connector's pinned test fixtures. New synthetic frames leave
back/side spin missing so the normal decoder derives them from total spin and
axis; the original wood frame retains its supplied components. Additional presets
have no fabricated club readings or FlightScope targets. Selecting or injecting a
replay does not connect to hardware or change the device's selected club.

## Threading and ownership

All WinRT calls run on one worker thread. Async operation waits check cancellation
at most every 25 ms, with a 10-second per-operation deadline and a 30-second overall
device-open/service-discovery deadline. Retry delays are interruptible. Disconnect
invalidates queued work immediately; shutdown requests cooperative cancellation and
joins the worker. Cleanup revokes callbacks and closes the connection without waiting
for another remote notification-disable write.

Results, discovery, notifications and link-loss events return to the game thread with
generation checks, so queued events from an old connection cannot update a new one.
Worker, apartment and scan initialization failures use the existing error/state path.
Read exceptions and timeouts also fail the connection sequence instead of silently
keeping it pending. Missing optional characteristics still return an empty value.

These are application-side wait limits, not a guarantee against a blocking OS/driver
call inside cancellation or resource release. The cancellation/deadline policy and
session lifecycle have hardware-free tests. Real-device shutdown and reconnect are
verified on Linux; on Windows they still need hardware verification.

## Tests

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Test -Filter UOpenGolf.LaunchMonitor
```

| Test | What it checks |
| --- | --- |
| `UOpenGolf.LaunchMonitor.Protocol` | Frame decoding, sentinels, signs and commands against open-lm-connector's test expectations |
| `UOpenGolf.LaunchMonitor.Session` | Handshake timing, heartbeat, arming, club data matching, link loss and reconnecting, with a fake transport and scheduler |
| `UOpenGolf.LaunchMonitor.OperationWait` | Completion, cancellation, shutdown and deadline behavior with a deterministic clock |
| `UOpenGolf.LaunchMonitor.Cancellation` | Repeated disconnect/reconnect, obsolete completions and worker initialization failure with a fake transport |
| `UOpenGolf.LaunchMonitor.Replay` | All presets decode, indexed repeat, screenshot inputs, invalid-index and malformed-frame rejection, and absent club-data clearing |
| `UOpenGolf.LaunchMonitor.BlueZModel` | BlueZ object mapping: discovery filter, device/address lookup, characteristic resolution per device, error classification and HUD messages |
| `UOpenGolf.LaunchMonitor.BlueZBus` | Linux only: missing-library reporting, system-bus calls, cancellation without waiting, posting without a reply, adapter property reads (skips without a bus or adapter) |

None of these need hardware. `BlueZBus` talks to the local BlueZ daemon when one is running.

## Linux troubleshooting

- "The Bluetooth service isn't running" in the HUD: start it with
  `sudo systemctl start bluetooth`. `bluetoothctl show` should list a powered controller.
- "Bluetooth is turned off or blocked" in the HUD: run `rfkill unblock bluetooth` or turn
  Bluetooth on in system settings.
- "needs systemd's libsystemd.so.0": the transport loads sd-bus at runtime; non-systemd
  distributions are not supported.
- "The Square didn't answer…" in the HUD: a Bluetooth LE device talks to one host at a
  time, so disconnect the Square Home from Windows (or any phone app) and press **C** again.
- No pairing is needed; the Square Home accepts unpaired connections under BlueZ.

## Known limitations

- The first matching device found is used; there is no device picker.
- Square Omni, alignment and spin-mode selection are not exposed.
- On Windows, the updated cancellation/shutdown paths and reconnect after link loss have
  automated coverage but have not been verified on hardware.
- Linux needs systemd's `libsystemd.so.0` and a running `bluetoothd`; non-systemd
  distributions are not supported.
- Linux shot recordings finalize with a hard link, so saving to a filesystem without hard
  links (exFAT, FAT) fails and keeps retrying. `Documents` on a normal Linux filesystem works.
- Linux: a failed BlueZ signal subscription isn't reported (connection events would then
  be missed silently), signal subscriptions are added synchronously, and libsystemd may
  be unloaded when the last transport is destroyed.

## Plugin structure

```text
OpenLaunchMonitor.uplugin
Source/OpenLaunchMonitor/
  Public/                   Subsystem and Blueprint types
  Private/Square/           Protocol decoder/encoder and session state machine
  Private/Bluetooth/        Transport interface and BlueZ object mapping (all platforms)
  Private/Windows/          C++/WinRT Bluetooth LE transport
  Private/Linux/            BlueZ transport over runtime-loaded sd-bus
  Private/Tests/            Protocol, session, replay and BlueZ automation tests
```

## License

MIT, as part of uopengolf. Derived from open-lm-connector (MIT), the maintainer's own
project; see [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md).
