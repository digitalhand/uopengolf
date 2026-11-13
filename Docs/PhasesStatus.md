# Phase status

## Sign-in: standing QR code, two buttons, switching users — 2026-11-13 (Windows)

Signed out, every Settings page shows a QR code under the last category, captioned
**Sign-In** with the short code to compare on the phone. It needs no button and is removed
at sign-in. Settings › Player now offers only **Sign In** and **Create Account**, side by
side; both use this computer's browser. **Sign in with QR**, **Sign in on this computer**
and **Open verification page** are gone.

- Plugin: `SignInWithQr()` became `KeepQrSignInReady()`, a lease the new `SOpenGolfQrSignIn`
  widget renews each tick. The service requests a device code, polls it, and replaces one
  that expired or was declined, only while the widget is on screen. Holding a code no longer
  counts as signing in, and cancelling a browser sign-in keeps the code.
- Switching users: browser sign-in always sends `prompt=login`. Before, a browser still
  logged in to Auth0 approved the sign-in silently for its last user, so a player who signed
  out was signed back in as the same account.
- The QR square is 70% of the category column's width. The code fills it inside a
  two-module white border, with module edges on whole screen pixels. The account block's
  reserved height dropped from 230 to 170 now that it holds no QR code.
- Settings: **Camera & HUD** is now **Camera**, and **HUD scale** moved to **Accessibility**,
  under **Text size**.

Validation: Unreal Engine 5.8.4, Windows x64, Development editor built. `OpenGolf.Online`
passed 6 of 6 (`Saved/Automation/20261113-001010-5bde5762`), with new checks for the lease,
expiry, decline, cancel and sign-out. The rendered `UOpenGolf.Visual.Online` passed
(`Saved/Automation/20261113-000914-b40664d5`); its screenshots
(`Saved/Screenshots/Online/7252EA43436F82B7D741E687BD69DDAF/`) show the QR code under
**Data & system** while signed out and no QR code while signed in. The full `UOpenGolf` run
passed 102 of 103 (`Saved/Automation/20261113-001023-b5e875e7`); the failure,
`UOpenGolf.Physics.ShotLibrary`, is the working tree's deletion of `Data/ShotLibrary`. That
run also compiled and covered the Data & system edits of 2026-11-12 (Browse under the path
field, no duplicate path, **Updates** without help text).

Not verified: a real Auth0 sign-in, by browser or by phone, including that `prompt=login`
lets a second account sign in; the QR code scanned from a real screen.

## Windows installer and in-game updates — 2026-11-12 (Windows)

New `OpenGameUpdate` plugin: checks the latest published GitHub release, downloads its Setup
with progress, verifies its SHA-256 and starts it silently on the game's folder; the game then
quits and Setup restarts it. The plugin has no UI or game code. Its network side is the
`IGameUpdateSource` interface (GitHub built in), so tests and other hosts can swap it. The game
adds an update screen with a progress bar, an **Update to v…** button on the home and pause
pages, a version notice, and **Settings › Data & system › Updates**. The Package Windows
workflow now also builds `UOpenGolf-<version>-Windows-x64-Setup.exe` (Inno Setup 6,
`Assets/Installer/`) with its `.sha256`. The same page gained **Browse** for both folders
(Windows) and a shorter Shot recording note.

Validation: Unreal Engine 5.8.4, Windows x64. Development editor and game targets built.
`OpenGameUpdate` passed 6 of 6 (`Saved/Automation/20261112-231907-20a2c046`); `UOpenGolf.Menu`
passed 9 of 9. `OpenGameUpdate.Live` read the real v0.0.7 release through GitHub's download
redirect, and its parsed checksum matched the published `.sha256`. The full `UOpenGolf` run
passed 87 of 88 (`Saved/Automation/20261112-231516-a4be4317`); the failure,
`UOpenGolf.Physics.ShotLibrary`, is the working tree's deletion of `Data/ShotLibrary`, not this
change. Release-script tests passed (34). With a stand-in package, Setup installed silently,
waited for a locked game executable during an upgrade, replaced content, kept `Saved\`, and
uninstalled cleanly.

Not yet verified: a full in-game update, which needs the first published release with a Setup
asset; the workflow's installer step on the runner; and the folder picker by hand.

## Repository cleanup — 2026-11-12 (Windows)

Fixed the savanna scripts' imports after the move to `Assets/Scripts`, and made
asset pruning scan that directory. A regression test protects assets named in
project and plugin sources. Public game-module dependencies now match the types
included by its public headers. Removed the unused native tree-count method and
the retired tracer benchmark implementation and switch.

Unreal's Asset Registry confirmed three unreferenced vegetation runs: 72 packages
under `Terrain/Generated/Vegetation_DC91AC…`, one under
`Terrain/Savanna/CutTurf/Vegetation_C8FD58…`, and 32 under
`Terrain/Savanna/CutTurf/Vegetation_C93E30…`. Removed all 105 packages (15,235,415 bytes,
14.53 MiB) after backing them up to
`Saved/AssetBackups/20261113-053216-generated-cleanup/`. The five ground meshes in
`Build_F1319D…` are referenced by the saved map and were retained. The map was not
resaved. This reduces checkout content; existing Git LFS history remains.

Audit evidence: `Saved/Cleanup/generated-assets-audit.json` and
`Saved/Cleanup/PruneGeneratedAssets.log`. Unreal deleted 71 packages, then reported
that the first directory could not be removed. With the editor stopped, the remaining
34 audited files were checked byte-for-byte against their backups and removed.

Validation: Unreal Engine 5.8.4, changelist 58902052, Windows x64. Development editor
build passed (`Saved/Logs/BuildEditor.log`). All 245 engine-free Python tests passed
with Python 3.14, Pillow 13.0.0 and NumPy 2.5.4 (82 project utilities, 58 course,
61 savanna and 44 Blender helpers). Fixed three Windows portability failures in
the existing tests. All tracked Markdown links to local files and heading anchors
resolve; missing references now point to maintained plugin documentation.

After content removal, `Saved/Automation/20261112-213323-af5729ef/index.json`
recorded 31 of 32 range tests passing, including saved layout, presentation, rollout
surfaces, frame-rate/hitch equivalence and shot-trail history. `WindControls` had two
stale widget-type expectations: the indicator is now a focusable `SButton`, rather
than its earlier `SBorder`. Corrected those assertions. The startup smoke test passed
in `Saved/Automation/20261112-213637-ee7ca364/index.json`.
After a successful editor rebuild, the focused `WindControls` rerun passed in
`Saved/Automation/20261112-213753-e014f6e0/index.json`, completing coverage of all
32 range checks across the full run and focused rerun. Linux, rendered gameplay
and packaged builds were not revalidated in this cleanup.

Follow-up build check: the ignored local `.vscode/tasks.json` still invoked
`Scripts/ue.ps1` in four tasks. Updated those paths to `Assets/Scripts/ue.ps1` and
executed the default VS Code editor-build task successfully. The standalone
Windows Development game target also built successfully (`Saved/Logs/BuildGame.log`).
The development guide now explains how to repair older local task configurations.

## Historical VS Code setup evidence

Linux, 2026-10-16: `projectfiles` and `compilecommands` succeeded with a writable
engine directory. The generated workspace supplied `cppdbg`/gdb launch entries.

An earlier Windows setup attempt (date not recorded) could not write
`Engine/Intermediate/ProjectFiles/PrimaryProjectPath.txt` in the protected engine
installation. Compilation database generation succeeded separately, and its
source/compiler/response-file paths were checked. Interactive VS Code IntelliSense,
debugging and full workspace generation were not verified in that attempt.
Machine-specific starter tasks and launch settings from that checkout were ignored
local files and are not supplied by a fresh clone.

## Shared online accounts — 2026-11-11

Implementation is available in the reusable `OpenGolfOnline` plugin, with a
UOpenGolf adapter for measured range shots. The plugin owns QR/browser sign-in,
secure session storage, refresh/logout, player API access, profile/history UI,
and the account-bound upload queue. Public host configuration and club artwork
remain in the game. No server deployment or database migration was performed.

### Build and tests

Engine: Unreal Engine 5.8.4, changelist 58902052. Platform: Linux x86-64.
Both the Development editor and standalone Development game targets built.
The Linux Development package also built, and its 60-second headless startup
check passed (`Saved/Automation/20261111-181349-package-ArKxo7/Game.log`). All four
club PNGs were staged and matched the source files byte for byte.
Windows code is implemented but has not been compiled or exercised here.

`Saved/Automation/Online-Verified/index.json` contains nine successful checks:

- Browser callback state/path/method checks, including duplicate-state rejection.
- An isolated OS credential save/read/delete test using a generated value.
- Live Auth0 device authorization over verified TLS: HTTP 200, without completing
  a player login or recording authorization codes in the report.
- Session refresh/logout/account switching and pending-queue restart recovery.
- Real-shot eligibility, late club readings and stable retry/session identity.
- API shot fields, rounding, null measurements and exact decimal JSON output.
- Request-origin, method, query and header-injection boundaries.
- Rendered main/pause/settings menus.
- Rendered QR, profile and per-club shot-history screens using isolated fake data.

The final QR screenshot was decoded with OpenCV and matched the expected test
verification URL. Screenshots are under
`Saved/Screenshots/Online/01A1CD81DB627884B804B4EBD8C4F2F3/`.

The preceding `Online-UI-Final` report also records sixteen successful existing
menu/range/recording regressions, including shot conversion, auto reset, HUD
controls, replay and recording recovery. Its two visual-test failures were
corrected in the test drivers and rerun successfully in `Online-Verified`:
Escape was being intercepted by the editor rather than the game, and the account
test needed to wait for Slate to display the completed login state. The later
JSON precision fix is covered by the final contract and restart tests, not by
those earlier regression results.

The Linux keyring library is kept loaded for the process because GLib retains
its registered type names. This fixes the crash found when restarting Play.
The corrected keyring test and repeated Play sessions in the rendered menu run
completed successfully.

Only Unreal build-rule C# files changed. The requested `OpenFairway.sln` command
was attempted and failed because that solution is absent. Its formatting check
is unavailable; UnrealBuildTool evaluated both updated module rules successfully.

### Rendered stability

Only two-dimensional club PNGs were added; terrain, turf, vegetation, lighting
and three-dimensional materials were unchanged. The public-map Play capture
`UOpenGolf.Visual.CoastalTerrain` completed 200 frames in
`Saved/Automation/CoastalReview/OnlinePlugin-20261112-012347/`.
The editor crashed afterward on the next Play startup due to the subsequently
fixed keyring lifetime issue; this capture was not a clean whole-process test run.

One editor rendered the sequences. Viewport size was 1541 × 953, with the same
camera and AA configuration for grass-on/off views. The following results came
from `Assets/Scripts/grass_stability.py --profile coastal`; changed pixels use a
3/255 threshold and are averaged across consecutive frames.

| View | Lower-region changed | Worst horizontal band changed |
| --- | ---: | ---: |
| Tee, grass on | 0.001% | 2.033% |
| Tee, grass off | 0.000% | 2.083% |
| Grass edge, on | 0.001% | 1.881% |
| Grass edge, off | 0.001% | 1.988% |
| Close grass, on | 0.017% | 0.762% |
| Close grass, off | 0.001% | 1.660% |

Canopy sparkle above 16/255 was 0.014% at the tee, 0.025% at the green,
0.026% at the edge and 0.010% at the bunker. A matched before capture for the new
UI artwork is unavailable, so these are measurements, not a before/after
improvement claim. The slow-camera sequence was captured; detailed motion review
remains pending. No terrain/texture changes followed this capture. Subsequent
changes concerned authentication, serialization, module dependencies and tests.

### Acceptance limits

A player has not yet approved a real login in this implementation. Confirm the
same account's existing profile/history in both games, then save a real measured
range shot in UOpenGolf and check it in OGolf. This is the remaining live
cross-game/PostgreSQL acceptance check; fake visual data is not evidence of it.
Also verify browser/social login, refresh across an application restart, and
Windows credential storage in a Windows build. A full packaged-game login has not been approved. No rendered FPS claim is made.

## Account access controls — 2026-11-11

The home/pause header now has an account button at its far right, on the same row
as UOPEN GOLF. It draws a login icon while signed out and a cog while signed in.
The cog opens user settings directly. The logout icon appears inside those
settings. Signed-in views no longer expose sign-in, account creation or the
removed "Try again" action. The old full-width account menu entry was removed.
The controls are drawn in Slate and have tooltips and accessible names.

User settings load club statistics after the first profile sync is ready. Opening
the active cog again preserves the current settings view and its edited name.

Development and DebugGame editor builds passed on Linux with Unreal 5.8.4.
`Saved/Automation/Account-Header-Verified/index.json` records four successful
checks: session behavior, menu input isolation, rendered menus, and the online
screen. The online screen check covers the login-to-cog change, absence of login
choices while signed in, logout through its icon, restoration of signed-out
choices, and waiting for a delayed first profile sync before loading statistics.
The rendered QR still decodes to the expected test verification URL. These tests
use an isolated account fixture and do not establish a real account login.

The Linux Development package was rebuilt. Its 60-second startup check passed
in `Saved/Automation/20261111-185637-package-5WJKHL/Game.log`. The rendered checks
finished before a separate DebugGame editor was observed; that editor subsequently
exited without being closed by the agent. No FPS comparison was made.

Screenshots are under
`Saved/Screenshots/Online/01A1CDA2E2F77F69A33D84BBF3D22D29/`.
No texture, terrain, lighting or vegetation assets were changed for these drawn
controls. No C# source changed in this UI revision.

## Menu and account page look — 2026-11-11

The main menu and the account page now share one look. Buttons are rounded and
outlined in the HUD colors, with the light accent for primary actions. Settings and
User settings use the same title row: a drawn back-arrow icon button, a 32-point
title and the page's right-hand control. The back arrow, gear, sign-in, sign-out
and delete icons are all drawn by one plugin widget. The account page groups its
controls into Profile, Range shots and Club averages sections. Club rows show
columns and are tagged by club. In shot history, the page back control and Escape
return to the club list before leaving the page.

The OpenGolfOnline plugin stays independent of the game. The game passes its
style through a new `FOpenGolfAccountStyle`, and other hosts keep the plugin
default. Fonts follow the Text size preference.

Development editor builds passed on Linux with Unreal 5.8.4, both with adaptive
unity disabled and with every changed file compiled on its own.
`Saved/Automation/20261111-202035-MenuRestyle/index.json` records
`UOpenGolf.Visual.Menu` and `UOpenGolf.Visual.Online` as successful. No other
editor was running. The online test now also covers back from shot history to the
club list and reopening a club. Screenshots are under
`Saved/Screenshots/Menu/20261111-2021*` and
`Saved/Screenshots/Online/01A1CDFD241275FCA94822586AD48149/`.

Limits: the online test renders the plugin panel with the game style and its own
title row. The game's account page, which uses the menu's page header, was not
captured. The DebugGame editor, packaged game and Windows build were not rebuilt
or checked. The owner has not yet reviewed the look.

## Range HUD status icons and recording — 2026-11-11

The range HUD's club picker could not be clicked: the whole shot HUD ignored the
mouse. Its root now passes clicks through, as the play buttons and wind controls do,
and only the club picker and the status box's two buttons take clicks.

The status box shows **Aim**, then two drawn icons. The launch-monitor icon is colored
by connection state and shows the battery level when known; it is captioned
**Connect [C]** while disconnected and opens Settings › Simulator when clicked. The
record toggle (**Record [F9]**) is a red dot with the saved count while recording and a
gray ring when off; clicking it does what **F9** does. Failed saves turn it amber, and
errors show beside the icons. The Launch Monitor and Battery text cells and the key-tip
line were removed.

Recording now decides online saving as well: while recording and signed in, eligible
range shots are queued for the online profile as well as written locally. The **Save to
profile** checkboxes (HUD and User settings) and the plugin's `save_to_profile` setting
were removed; an older player file's value is ignored.

Development editor builds passed on Linux with Unreal 5.8.4, both with adaptive unity
disabled and with every changed file compiled on its own. A new rendered test,
`UOpenGolf.Visual.RangeHudControls`, checks that clicks reach the club picker and both
status buttons, that the record button toggles recording, and that the launch-monitor
button opens Settings › Simulator. **It has not been run yet**, nor have the updated
`UOpenGolf.Range.ShotDisplay` and `UOpenGolf.Range.ShotRecording` checks: the owner's
DebugGame editor was open, and a second editor process was not started.

Not verified: a real launch monitor's battery and Ready state in the icons, and a real
recorded shot reaching the online profile. The DebugGame editor, packaged game and
Windows build were not rebuilt.

## Range HUD polish and no tooltips — 2026-11-11

The club picker shows standard abbreviations: DR, 3W–7W, 4i–9i, PW, GW, SW, LW and PT.
Recording file names and online club keys are unchanged.

The shot column used to resize when the club changed. Its width was the widest of the
heading row and the tile grid, and "Pitching Wedge" made the heading row wider. The grid
then stretched every tile. The column is now fixed at the grid's width, the picker has a
fixed size, and a long heading is shortened with an ellipsis.

"Waiting for a shot" was removed. While the ball waits at address, a soft glow hugs its
outline: red until the launch monitor reports the ball ready, green once it does. The owner
chose the outline-glow look. The HUD draws it over the ball's on-screen outline in Slate,
not as a post-process material, so no material asset, custom-depth setting or extra GPU
pass was added. It draws over anything in front of the ball.

While a ball is being placed, and for the rest of a hole with a placed ball, the hole
heading reads **Practice · Ball placement**. The Stroke, To Pin and Putts tiles and the
ball glow are hidden then too.

Recording is range-only. On the hole the record icon is hidden, **F9** is ignored and no
shot is written, even if recording was left on; the on/off state is kept for the range.
Cloud saving was already range-only.

Every tooltip was removed. This covers the HUD status icons, the menu's launch-monitor
button, the page back arrow, and the plugin's account, back, view-shots and delete
controls. Plugin icon buttons take a `Label` as their accessible name instead. The
rendered menu, online and HUD tests now fail if any widget in the game window or its
menus has a tooltip.

Development editor builds passed on Linux with Unreal 5.8.4, both with adaptive unity
disabled and with every changed file compiled on its own. No other editor was running
during the test runs below.

- `Saved/Automation/20261111-213049-HudPolishHeadless/index.json`: all 8 tests passed,
  covering ShotDisplay, ShotRecording and Online. The two warnings are the expected logs
  from the recording failure-injection tests.
- `Saved/Automation/20261111-213114-HudPolishRendered/index.json`: RangeHudControls,
  Menu and Online all failed. The causes were two test problems, now corrected:
  - The tooltip sweep counted the empty tooltips engine widgets carry: SComboButton's
    inner button, and a collapsed SSearchBox throbber. Slate never shows those
    (`SlateUser.cpp:1383`). The sweep now counts only non-empty tooltips on visible
    widgets.
  - The record button was clicked in the frame the HUD reappeared, so the first click
    missed and recording toggled one click late. The test now waits for layout.

  All other checks in that run passed:
  - the same heading and tile sizes with DR and PW;
  - the glow at address on the range and on a normal hole, and none while placing or on
    a practice hole;
  - the placement and practice headings, with the hole tiles hidden;
  - no record toggle on the hole;
  - the launch-monitor button opening Settings › Simulator.

  Screenshots are in `Saved/Screenshots/RangeHud/20261111-2132*`.
- The glow showed red and green as intended. At 1280 × 720 the ball is about 6 px across
  and the glow was hard to see, so its strokes were widened slightly after this run.
- Rerun with the corrected tests and the widened glow, with no other editor running:
  `Saved/Automation/20261111-220240-SettingsRendered/index.json`. RangeHudControls passed;
  its one warning is the engine's own analytics request to Epic failing without network.
  `Saved/Screenshots/RangeHud/20261111-220349-ball-not-ready.png` and
  `20261111-220351-ball-ready.png` show the wider red and green glow, clearly visible around
  the 6 px ball. The menu and online tests in that run cover the next section's changes.

Not verified: a real launch monitor turning the glow green, and the glow's size and
subtlety in real captures. The DebugGame editor, packaged game and Windows build were not
rebuilt.

## One Settings menu; the account in Settings › Player — 2026-11-11

Owner feedback merged the two settings screens. The cog was a signed-in-only **User
settings** page beside a separate **Settings** button; the logout icon only appeared on that
page, under the cog.

- **Top bar** on every menu page: **UOPEN GOLF**, the launch-monitor status, one account
  icon, then the cog. The account icon is the login icon while signed out (it opens
  Settings › Player) and the logout icon while signed in. The cog is always shown and is
  the only way into Settings; the home and pause pages lost their **Settings** button.
- **Settings › Player** holds the account. Signed out: the sign-in choices only. Signed in:
  member since, the display name and a **Shot history** row whose **View** opens club
  averages, then one club's shots; Back returns to Player. Settings rebuilds when the
  session starts or ends.
- **Time zone** (Player) sets shot-history times and the member-since date: Local (the
  default, this computer's zone), UTC or 16 listed zones such as Pacific (PT). Conversion
  uses ICU (`FText::AsDateTime` with an IANA ID), which applies daylight saving; the English
  ICU data set packaged with the game includes the zone data. It applies at once like the
  connection choices, and Reset All leaves it alone. CSV export keeps UTC.
- Clubs in averages and the history title use the standard abbreviations (7i, PW, DR).
- The **Range shots** pending/rejected counts were removed.
- Plugin: `SOpenGolfAccountButton` lost its cog state. Its sign-in and logout variants each
  show only in their session state (tags `AccountSignIn` and `AccountSignOut`).
  `SOpenGolfAccountPanel` gained `Page` (Account or Shots), `TimeZone` and `TimeZoneName`.

Development editor builds passed on Linux with Unreal 5.8.4, with adaptive unity disabled
and with `ue.sh build`. No other editor was running during these runs.

- `Saved/Automation/20261111-220212-SettingsHeadless/index.json`: all 19 tests passed,
  covering Menu, ShotDisplay, ShotRecording, the game's Online tests and the plugin's
  `OpenGolf.Online` tests. The new `OpenGolf.Online.Format` covers club abbreviations and
  Pacific times in winter (UTC−8) and summer (UTC−7). `UOpenGolf.Menu.PreferenceValidation`
  checks the time-zone default, recovery, disk round trip, and that each listed zone
  converts a July instant away from UTC, which catches a mistyped zone ID.
- The zone labels were then shortened, because the drop-down clipped "Local (this
  computer)". `Saved/Automation/20261111-220627-SettingsHeadless2` (Menu and Format, 7 tests)
  and `20261111-220644-SettingsRendered2` (Visual.Online and Visual.Menu) passed after that
  change.
- `UOpenGolf.Visual.Online` now drives a real menu against a stubbed service. It checks:
  - the top-bar order and which account icon shows;
  - that sign-in opens Player, with no profile options before sign-in;
  - the profile and Shot history after sign-in, with no range-shot counts;
  - choosing Pacific from the drop-down, the 7i club row, and the "Time (PT)" column
    showing the stub's 00:00 UTC shot as 4:00 PM on Nov 10;
  - back to the club list, then to Player;
  - logout from the top bar.

  `UOpenGolf.Visual.Menu` checks that the home and pause pages have no Settings button,
  the icon order, and the cog opening Settings. Captures are in
  `Saved/Screenshots/Online/01A1CE5E58297D32B8D9285BE0239D32/` and
  `Saved/Screenshots/Menu/20261111-2207*`.

Not verified: a real Auth0 sign-in and real shot data; time zones on Windows (ICU maps the
Windows zone for Local); the DebugGame editor, packaged game and Windows build.

### DebugGame crash on Play, fixed — 2026-11-11

The owner's DebugGame editor crashed starting Play: a segmentation fault in `FMemory::Memcpy`
(`GenericPlatformMemory.h:654`). Reproduced under gdb with `UnrealEditor-Linux-DebugGame`
(`UOpenGolf.Visual.Menu`). The menu's account button called `IsConfigured()` on an object that
was not the online service, and copying its "configuration" string read from address 0.

Cause: the menu's new `Online` argument. `SLATE_ARGUMENT` declares its member without an
initializer, and the player controller creates the menu without passing `Online`, so it held
leftover memory. In Development builds that memory happened to be null, so the earlier test
runs passed; in DebugGame it resolved to an unrelated object.

Fix: every raw-pointer Slate argument now defaults to `nullptr`. That covers `SMainMenu`'s
`Controller` and `Online`, `SClubPicker`'s `Monitor`, and `Online` on the plugin's account
button and panel. The new headless test `UOpenGolf.UI.ArgumentDefaults` builds each widget's
arguments in pattern-filled memory and checks those defaults. It failed with the menu's
defaults removed (`Saved/Automation/20261111-222634-ArgDefaultsRed`) and passes with them.

The Development (adaptive and non-adaptive) and DebugGame editor builds passed. Under gdb, the
DebugGame editor ran ArgumentDefaults, Visual.Menu, Visual.Online and Visual.RangeHudControls
with no crash, and all passed (`Saved/Automation/20261111-222808-DebugGameFixed`). The one
warning is the engine's analytics request to Epic failing without network.

## UI folders, cleanup pass and sensitive-data scan — 2026-11-12

After the DebugGame fix, HUD keyboard/gamepad navigation, a routed menu with cached pages, and
account states with a sign-out restore marker were added (2026-11-11, 22:42–22:56). Their last
headless run (`Saved/Automation/UiPolishHeadless`) had one failure and predated the final menu
edit. This pass reviewed the working tree.

- **Layout.** `Source/UOpenGolf/Private/UI` moved into `Common`, `Hud`, `Club`, `Wind` and
  `Menu` (45 files, includes rewritten in 47). See [Architecture](Architecture.md).
- **`UOpenGolf.Menu.ReturnToMainMenu`** failed in that earlier run; it passes on the current
  code, in Development and DebugGame.
- **Fixed.**
  - Shot history reopened on the last club's cached page; each View now builds a fresh page.
  - The account panel's own heading (hosts using `ShowHeading`) never updated; it now follows
    the page and session.
  - The Shots panel requested club averages twice on every visit. The new
    `OpenGolf.Online.PanelHeading` test found this.
- **Removed.** An unused gamepad flag, an unused panel revision field, and duplicate
  visibility code in Settings › Player.
- **Tests corrected, not game code.**
  - `Menu.RenderResolution` read this machine's saved video settings.
  - `Visual.WindIndicator` expected the HUD root to be hit-test invisible; it has been
    self-hit-test-invisible since its icons became clickable.
  - `Visual.RangeAssets` set the grass variable at a priority below the coastal test's, so it
    failed only when the whole Visual group ran in one process.

Builds passed: Development (adaptive unity off, and `ue.sh build`) and DebugGame. No other
editor ran during these runs.

- Headless, `Saved/Automation/20261112-181630-CleanupHeadless`: 23 of 23 passed (Menu, UI,
  Online, ShotDisplay, ShotRecording, InputBindings and the plugin's `OpenGolf.Online`). The
  two warnings are the recording tests' expected failure-injection logs.
- Rendered, all eleven `UOpenGolf.Visual` tests, `20261112-181701-CleanupRendered`: nine
  passed; RangeAssets and WindIndicator failed for the test reasons above. Rerun with
  CoastalTerrain after the corrections, `20261112-182445-CleanupRendered2`: all three passed.
- DebugGame under gdb, `20261112-182934-CleanupDebugGame`: ReturnToMainMenu, ArgumentDefaults,
  PanelHeading, Visual.Menu, Visual.Online and Visual.RangeHudControls passed with no signal.
  Captures: `Saved/Screenshots/Online/01A1D2BE401D7B88A55498AE1931896F/`.

**Sensitive-data scan** of 457 text files, 424 assets and all history (the repository is public):

- No passwords, tokens, private keys or `.env` files. The Auth0 client ID is a public
  native-application identifier.
- `Content/Developers/<account>/` is created by the editor. It was empty and never committed;
  it is now ignored.
- `Config/DefaultEditorPerProjectUserSettings.ini` carried a home-folder path in its terminal
  startup commands. Those lines moved to the ignored per-user layer. Earlier commits keep them.
- 185 assets embed their import source paths, which include a home folder. Left as they are by
  the owner's choice, as is the commit history's author emails.

Not verified: a real Auth0 sign-in and real shot data; a real gamepad (navigation is covered by
simulated key events only); Windows and packaged builds.
