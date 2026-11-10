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
  and, when `bAutoReconnect` is set, retries with 0.5, 1, 2, 4 and then 8 s waits, up to
  12 times (about three minutes). Attempts connect straight to the last device without a
  scan (8 s deadline); every fourth attempt scans, in case it changed. A Square Home takes a
  while to boot before it advertises, and a half-booted one accepts the link but not reads,
  so those fail within 3 s and the next attempt follows soon. Measured (2026-11-10): Ready
  again 102 s after a power cycle with the earlier 5-attempt schedule; the attempt that
  succeeded took 1.9 s.
- **Dead-link detection:** a heartbeat goes out every second, and the device answers each one
  with an `11 03` notification within about 0.2 s; it also sends sensor frames every 1.3–2.2 s
  and clock ticks every 3–6 s. Two heartbeat intervals in a row without any notification with
  data drop the link. A switched-off Square sends a burst of empty notifications, which do not
  count. Windows did not report the switched-off device itself, so this is the main signal;
  a closed GATT session (Windows) or two failed heartbeat writes also drop the link. Reads,
  writes and notification setup time out after 3 s. Tunable with `olm.Square.HeartbeatInterval`
  and `olm.Square.MissedHeartbeats`, read at each connect. Measured (2026-11-10): the link was
  dropped 2.7 s after the last frame, before the empty-notification fix; the earlier 5 s
  heartbeat took about 13 s.
- **Disconnect:** the game sees Disconnected at once, and callbacks from the old connection
  are ignored. A connected Square is first told to stop detecting (`DetectBall` mode 0),
  which turns its red lights off; the link is released when that write completes, or after
  0.5 s at most. Releasing alone was not enough: the Square does not advertise while
  connected, and after a full release it advertised again only 6.5 s later (measured
  2026-11-10), because Windows drops a released link when it chooses, for every app.
- **Exit:** quitting (or ending PIE) sends the same stop command and releases the device
  while shutdown waits, at most 1.5 s; the worker is then joined.
- **Release:** on Windows a requested disconnect clears `MaintainConnection`, turns
  notifications off (at most 1 s per write, skipped at exit and after a link loss), and
  closes the services, session and device; `olm.Bluetooth.GracefulRelease 0` releases the
  objects only. On Linux `StopNotify` and `Device1.Disconnect` are queued without waiting
  for a reply and flushed when the bus closes (2026-10-18; previously each **D** and quit
  waited 2 s).
  BlueZ keeps a released link up for its 2 s disconnect timer, so a reconnect to the
  same device within 3 s first waits for `Device1.Connected` to turn false. Worker/apartment/scan failures and read exceptions
  or timeouts reach the error/state path; missing optional characteristics return
  empty values. These wait limits do not bound synchronous OS/driver calls during
  cancellation or cleanup.

## Session protocol summary

Characteristics: command `86602101-…` (write with response), events `86602102-…`
(notify), battery `00002a19-…`, firmware `86602003-…`. After connecting: read
battery and firmware, subscribe, heartbeat, club at once, arm 0.5 s later, heartbeat
every second. Upstream waits 2 s and 3 s; the shorter waits are tunable with the console
variables `olm.Square.ClubDelay` and `olm.Square.ArmDelay`, read at each connect. Re-arming
after a shot still waits 3 s. On Windows, characteristics come from the system cache in one
query per service (the device is asked only if a required one is missing), and each GATT
operation wakes the worker as it completes instead of being polled. Duplicate notifications are ignored. See upstream `addons/square/PROTOCOL.md`.

## Open Connect provider (third-party connectors)

uopengolf accepts shots from connector apps that speak the Open Connect v1 protocol. Many
launch monitors have such a connector. Choose it in
**Settings → Simulator → Launch monitor**.
- **No Apply needed:** the provider, port and "Allow connectors on other PCs" are saved and take
  effect as soon as they change, like Connect and Disconnect. They are left out of Apply,
  Discard and Reset All.
- **Starting and stopping:** choosing Open Connect starts the listener, and a port or LAN change
  restarts it. Choosing Square Home stops it; press Connect to scan.
- **C**/**T** and the Start/Stop listening buttons also start and stop it.

**Code:**
- `Private/OpenConnect/OpenConnectProtocol.*`: framing, decoding and replies, with no sockets.
- `Private/OpenConnect/OpenConnectServer.*`: the TCP worker.
- The provider switch in `UOpenLaunchMonitorSubsystem`.

**Transport:**
- uopengolf listens; the connector connects to it.
- The default address is `127.0.0.1`. **Allow connectors on other PCs** listens on every
  interface instead, and Windows may ask to allow it through the firewall. The protocol has no
  authentication, so in that mode only loopback, private (10/8, 172.16/12, 192.168/16) and
  link-local peers are accepted; others are refused and logged once.
- On Windows the port is bound with `SO_EXCLUSIVEADDRUSE`, so no other process can share or
  take it over while the game listens.
- The port defaults to **4921** on every platform. Set the connector to send to it. It is
  configurable.
- The listener fails with a message if the port is taken, for example by another simulator.
- One worker thread owns the sockets. One connector is served at a time. A new connection
  from the same address replaces the old one, as a restarted connector would; one from
  another address is refused until the current connector has sent no complete message for
  10 s. Bytes that never form a message don't count as activity.
- **Rate limit:** 20 messages a second, bursts of 40. Over the limit the worker stops reading,
  so TCP slows the sender; a connector that stays over it for 5 s is disconnected. At most
  32 events reach the game thread per frame.
- Events reach the subsystem on the game thread through the core ticker. Stopping joins the worker.

**Framing:** the protocol defines no delimiter. The reader extracts each top-level `{…}`
object from the byte stream, skipping braces inside strings and honouring escapes. This
handles split and coalesced messages. More than 64 KiB without a complete object
clears the buffer and replies 413. A connector that sends 256 KiB without completing a
message (oversized objects or junk outside any object) is disconnected. The reader is ported from `open-lm-connector`'s
`TcpServer.cs`.

**Messages:**
- **Ball data:** `Speed` is mph and is converted to m/s; `VLA`, `HLA` and `SpinAxis` are
  degrees; `TotalSpin`, `BackSpin` and `SideSpin` are rpm. Positive HLA, spin axis and
  side spin are to the right, which is already this plugin's convention, so nothing is flipped.
- **Required fields:** `Speed` and `VLA`; otherwise the reply is 501.
- **Missing spin:** if the connector sends total spin and axis, back and side spin are
  derived from them and marked `Derived`. If it sends back and side spin, total spin and
  axis are derived the same way.
- **Plausibility:** the same limits as the Square decoder (`Private/LaunchMonitorLimits.h`).
  Every ball number must be finite and in range before it is rounded: HLA under 90°, spin
  axis within ±180°, back and side spin under 30000 rpm; otherwise the reply is 501.
  Out-of-range club fields are dropped and the shot still flies.
- **`DeviceID`:** control characters and bidirectional overrides are removed and it is cut
  to 64 characters. Rejected messages are logged as a 160-character excerpt, at Warning for
  the first five per connection and at Verbose after that.
- **`ClubData`:** read only when `ContainsClubData` is true, and only for the keys sent:
  `Path`, `FaceToTarget`, `AngleOfAttack`, `Loft` (stored as dynamic loft) and the
  face-impact fields. `CarryDistance` and club speed are ignored.
- **Heartbeats and status:** `IsHeartBeat`, and status messages without ball data, never fly.
  `LaunchMonitorIsReady` drives the ball-ready flag.

**Replies:** every message gets one JSON reply, with no delimiter.
- **200** for a shot the game accepted, and for heartbeats and status messages.
- **501 with a reason** for a malformed message, or for a shot the game refused
  (menu open, missing speed or VLA, launch failed). A shot handler reports a refusal
  with `RejectShot` while the shot is being broadcast.
- **201 player information** (`Handed` RH/LH and an Open Connect club code such as `DR`, `I7`
  or `PT`) when a connector attaches, and whenever the club or handedness changes.

**State mapping:**

| Listener | `ELaunchMonitorConnectionState` |
| --- | --- |
| Stopped | Disconnected |
| Listening, no connector | Connecting |
| Connector attached | Connected |
| Connector reports ready, or sends a shot | Ready |

The device name is the connector's `DeviceID`, or its address until it sends one. No battery is reported.

**Recording:** F9 records Open Connect shots as it does Square shots. Their source is
`Open Connect: <DeviceID>`, and the recording's `DeviceID` is the connector's own.
Square spin mode applies only to Square Home.

**Tested connectors** (record each with its version and the date):

| Connector | Launch monitor | Result |
| --- | --- | --- |
| Automated loopback client (`UOpenGolf.LaunchMonitor.OpenConnect.Loopback`) | none | Passes |
| MLM2PRO connector (springbok) | Rapsodo MLM2PRO, FlightScope Mevo+ | Not yet tested |
| OpenSkyPlus2 | SkyTrak / ST+ | Not yet tested |
| Garmin R10 connector | Garmin R10 | Not yet tested |
| PiTrac | PiTrac | Not yet tested |

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
