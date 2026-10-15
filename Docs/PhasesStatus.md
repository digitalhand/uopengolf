# Phase status

Current status and completion evidence for each phase of the
[roadmap](Roadmap.md). The roadmap defines each phase's scope and exit criteria;
this document records where each phase stands. Update it as evidence changes,
including the engine version, OS and date actually tested.

Phases 1 and 2 are split into a **Windows track** (current) and a **Linux track**
(deferred, not blocking Windows work). Each phase's exit still requires both tracks.

| Phase | Windows track | Linux track |
| --- | --- | --- |
| 0 — Hello-world foundation | Complete | Commands provided; not run |
| 1 — Linux baseline and integration design | Launch-monitor design done; OpenFairway fixtures not started | Not started |
| 2 — Launch-monitor input | Implemented; hardware validation pending | Not started (no BlueZ transport) |
| 3 — Native OpenFairway | Not started | Not started |
| 4 — Greens and surfaces | Not started | Not started |
| 5 — End-to-end golf slice | Not started | Not started |
| 6 — Community and content expansion | Not started | Not started |

## Phase 0 — Hello-world foundation

**Status:** complete on Windows. The editor target builds and
`UOpenGolf.Smoke.HelloWorld` passes headless with Unreal 5.8.3 (2026-10-14), after
aligning the HUD greeting with the test. The maintainer opened and compiled the
project in the editor; the rendered greeting should still be checked visually.
Linux execution remains unverified.

## Phase 1 — Linux baseline and integration design

**Status:** Windows track done for the launch monitor: see
[launch-monitor integration](LaunchMonitorIntegration.md) (pinned revision, native
C++ port decision, units, signs, missing values, threading, ownership, lifecycle).
Linux baseline and the OpenFairway fixture work have not started.

## Phase 2 — Launch-monitor input

**Status:** Windows implementation in place, hardware validation pending. The
`OpenLaunchMonitor` plugin ports the Square protocol/session to C++, adds a C++/WinRT
Bluetooth LE transport, replay input, Blueprint events, a HUD status panel, and
hardware-free protocol, session and replay tests (all passing, 2026-10-14). A real
Square Home has not yet been connected. Linux BlueZ is not implemented.

## Phases 3–6

**Status:** not started. No OpenFairway or ogolf code has been imported.
