# Launch-monitor integration

This note records the Phase 1 integration decisions and describes what Phase 2
implemented on Windows and Linux.

## Reference and decision

- Reference: [open-lm-connector](https://github.com/digitalhand/open-lm-connector)
  at `92eb021abe28d8eb1abe43f87767d3eb73e5939e`, MIT (`addons/LICENSE`).
- Decision: **native C++ port**, not an out-of-process bridge. The connector's
  Square protocol and session logic (~1,000 lines) has no Godot dependency and its
  Windows transport maps directly onto C++/WinRT. A bridge would require a .NET
  runtime and a second process for players and contributors.
- Ported files (now in `Plugins/OpenLaunchMonitor/Source/OpenLaunchMonitor/Private/`):

  | Upstream (`addons/`) | uopengolf |
  | --- | --- |
  | `square/SquareProtocol.cs`, `SquareCommandBuilder.cs`, `SquareShotMetrics.cs`, `SquareConnectionOptions.cs` | `Square/SquareProtocol.*` |
  | `square/SquareConnectionSession.cs` | `Square/SquareSession.*` |
  | `common/bluetooth/IBluetoothGattClient.cs` | `Bluetooth/GattTransport.h` |
  | `common/bluetooth/windows/WindowsBluetoothGattClient.cs` | `Windows/WindowsGattTransport.cpp` |
  | `common/bluetooth/linux/BlueZMapper.cs` | `Bluetooth/BlueZModel.*` |
  | `common/bluetooth/linux/LinuxBluetoothGattClient.cs` | `Linux/LinuxGattTransport.cpp` (over `Linux/BlueZBus.*`) |

  The Linux files come from the local checkout at `655fd96` (unchanged since `5edc4cf`);
  the pinned `92eb021` is not in that checkout. Not ported: Godot nodes/GDScript, the
  PiTrac TCP listener and the key mapper.

## Platform dependencies

| Platform | Transport | Build dependency | Status |
| --- | --- | --- | --- |
| Windows 10/11 x64 | WinRT `BluetoothLEAdvertisementWatcher`, `BluetoothLEDevice`, `GattSession` | Windows SDK C++/WinRT headers (`Include/<sdk>/cppwinrt`) and `WindowsApp.lib`, added only to the plugin module by its `Build.cs`; C++ exceptions enabled in that module | Square Home connection, battery/firmware, shot and club data verified (2026-10-14) |
| Linux x86_64 | BlueZ over the system D-Bus (`Adapter1`, `Device1`, `GattCharacteristic1`, `Battery1` fallback) through sd-bus | None at build time: `libsystemd.so.0` is loaded with `dlopen` at runtime. Running needs `bluetoothd` and a user allowed to use BlueZ (desktop default; the `bluetooth` group) | Implemented 2026-10-17; all tests pass on Ubuntu 26.04.1 with BlueZ 5.85, and a real Square Home connects, reports shots and club data, records, reconnects and disconnects (2026-10-17). No pairing needed |

The Windows build does not change engine/target settings (`bUseCPPWinRT` stays off),
so it works with an installed engine. The device is not paired; Windows Bluetooth
must be on. No special permissions are required on Windows.

## Contracts

- **Units:** what the device measures. Ball speed m/s (mph is presentation only),
  angles in degrees, spin in rpm. Unreal centimetres are a later physics-wrapper concern.
- **Signs:** positive HLA, spin axis and side spin are to the right. The
  device reports axis/side spin positive-left; the decoder flips them, as upstream does.
- **Missing values:** the device's `0x8000` (ball) and `0xFFFF` (club) sentinels
  become `Missing`, never zero. Back/side spin computed from total spin and axis are
  marked `Derived`. Club fields have per-field `bHas…` flags. Nothing is fabricated.
- **Plausibility:** speed 0–250 m/s, spin 0–30,000 rpm and |VLA| < 90°; other shot
  frames are dropped but the device is still re-armed.
- **Shot identity and late club data:** each shot gets a `ShotNumber` per game
  instance. The session requests the `11 07` club frame after a shot and accepts one
  frame until it re-arms (3 s); `FLaunchMonitorClubData::ShotNumber` names its shot.
  The game recorder matches this ID independently of the ball's simulation ID and
  can attach club data after a short shot stops. It waits for the response, or until
  three seconds after receipt, before finalizing a completed/interrupted shot.
  Recording-off and shutdown finalize available data immediately; see
  [recording behavior](../README.md#recording-shots).
- **Timestamps:** `ReceivedUtc` is when the game thread received the frame; the
  device supplies no shot timestamp.
- **Threading:** all WinRT calls (Windows) or sd-bus calls (Linux) run on one worker
  thread; on Windows it is in the multithreaded apartment, on Linux it owns the only
  D-Bus connection and pumps it every 25 ms. Results and device events are posted to the game thread; the session,
  subsystem, delegates and HUD run on the game thread only. Async operations use
  interruptible status waits, checking cancellation at most every 25 ms. Each
  operation has a 10 s deadline; opening the device and resolving characteristics
  share a 30 s deadline.
- **Ownership:** `UOpenLaunchMonitorSubsystem` (a game-instance subsystem) owns the
  session and transport. Mock/replay input enters through the same subsystem events.
- **Connection lifecycle:** Disconnected → Scanning → Connecting → Connected → Ready.
  Scans time out after 30 s. A dropped link (new versus upstream) reports an error
  and, when `bAutoReconnect` is set, retries with 2–30 s backoff up to five times.
  Disconnect immediately invalidates queued connection work and callbacks from the
  old connection. Shutdown requests cooperative cancellation and joins the worker.
  Cleanup revokes subscriptions and closes resources without waiting for the device:
  on Windows it doesn't wait for the notification-disable write, and on Linux
  `StopNotify` and `Device1.Disconnect` are queued without waiting for a reply and
  flushed when the bus closes (2026-10-18; previously each **D** and quit waited 2 s).
  BlueZ keeps a released link up for its 2 s disconnect timer, so a reconnect to the
  same device within 3 s first waits for `Device1.Connected` to turn false. Worker/apartment/scan failures and read exceptions
  or timeouts reach the error/state path; missing optional characteristics return
  empty values. These wait limits do not bound synchronous OS/driver calls during
  cancellation or cleanup.

## Session protocol summary

Characteristics: command `86602101-…` (write with response), events `86602102-…`
(notify), battery `00002a19-…`, firmware `86602003-…`. After connecting: read
battery and firmware, subscribe, heartbeat, club after 2 s, arm after 3 s, heartbeat
every 5 s. Duplicate notifications are ignored. See upstream `addons/square/PROTOCOL.md`.

## Remaining work

- Verify reconnect after link loss, cancellation during discovery/read/write/subscribe,
  and stopping Play during pending operations on a real Square Home under
  Windows (connection, battery/firmware, shots, club data and club selection are verified), and
  record the tested firmware and Windows build in [phase status](PhasesStatus.md).
- Linux: confirm on hardware that **T** (disconnect, formerly **D**) and quitting no longer
  wait (2026-10-18 change),
  and connect from a packaged game. Record the tested firmware (now logged as
  `Square Home firmware <x>.`) on both platforms.
- Phase 1 physics items (OpenFairway fixtures and tolerances) are unaffected by this note.
