# Launch-monitor integration (Windows first)

This note records the Phase 1 integration decisions for the Windows track and
describes what Phase 2 implemented. Linux decisions are listed but not yet validated.

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

  Not ported: Godot nodes/GDScript, the PiTrac TCP listener, the GSPro key mapper,
  and the Linux BlueZ client.

## Platform dependencies

| Platform | Transport | Build dependency | Status |
| --- | --- | --- | --- |
| Windows 10/11 x64 | WinRT `BluetoothLEAdvertisementWatcher`, `BluetoothLEDevice`, `GattSession` | Windows SDK C++/WinRT headers (`Include/<sdk>/cppwinrt`) and `WindowsApp.lib`, added only to the plugin module by its `Build.cs`; C++ exceptions enabled in that module | Square Home connection, battery/firmware, shot and club data verified (2026-10-14) |
| Linux x86_64 | Planned: BlueZ over D-Bus (sd-bus or GDBus) against `Adapter1`, `Device1`, `GattCharacteristic1` | To be decided with the Linux track | Stub reports "not supported yet"; replay works |

The Windows build does not change engine/target settings (`bUseCPPWinRT` stays off),
so it works with an installed engine. The device is not paired; Windows Bluetooth
must be on. No special permissions are required on Windows.

## Contracts

- **Units:** what the device measures. Ball speed m/s (mph is presentation only),
  angles in degrees, spin in rpm. Unreal centimetres are a later physics-wrapper concern.
- **Signs (GSPro):** positive HLA, spin axis and side spin are to the right. The
  device reports axis/side spin positive-left; the decoder flips them, as upstream does.
- **Missing values:** the device's `0x8000` (ball) and `0xFFFF` (club) sentinels
  become `Missing`, never zero. Back/side spin computed from total spin and axis are
  marked `Derived`. Club fields have per-field `bHas…` flags. Nothing is fabricated.
- **Plausibility:** speed 0–250 m/s, spin 0–30,000 rpm and |VLA| < 90°; other shot
  frames are dropped but the device is still re-armed.
- **Shot identity and late club data:** each shot gets a `ShotNumber` per game
  instance. The session requests the `11 07` club frame after a shot and accepts one
  frame until it re-arms (3 s); `FLaunchMonitorClubData::ShotNumber` names its shot.
- **Timestamps:** `ReceivedUtc` is when the game thread received the frame; the
  device supplies no shot timestamp.
- **Threading:** all WinRT calls run on one worker thread in the multithreaded
  apartment. Results and device events are posted to the game thread; the session,
  subsystem, delegates and HUD run on the game thread only.
- **Ownership:** `UOpenLaunchMonitorSubsystem` (a game-instance subsystem) owns the
  session and transport. Mock/replay input enters through the same subsystem events.
- **Connection lifecycle:** Disconnected → Scanning → Connecting → Connected → Ready.
  Scans time out after 30 s. A dropped link (new versus upstream) reports an error
  and, when `bAutoReconnect` is set, retries with 2–30 s backoff up to five times.

## Session protocol summary

Characteristics: command `86602101-…` (write with response), events `86602102-…`
(notify), battery `00002a19-…`, firmware `86602003-…`. After connecting: read
battery and firmware, subscribe, heartbeat, club after 2 s, arm after 3 s, heartbeat
every 5 s. Duplicate notifications are ignored. See upstream `addons/square/PROTOCOL.md`.

## Remaining work

- Verify reconnect after link loss and club selection on a real Square Home under
  Windows (connection, battery/firmware, shots and club data are verified), and
  record the tested firmware and Windows build in [phase status](PhasesStatus.md).
- Linux track: BlueZ transport, permissions/dependencies, and hardware validation.
- Phase 1 physics items (OpenFairway fixtures and tolerances) are unaffected by this note.
