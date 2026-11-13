# UOpenGolf shared accounts

UOpenGolf uses the existing OGolf native Auth0 application and player API. Signing
in with the same tenant identity resolves the same PostgreSQL player record,
club statistics and shot history. The game communicates with the HTTPS API;
database credentials remain on the server.

## Reusable plugin

`Plugins/OpenGolfOnline` owns QR device authorization, system-browser PKCE sign-in,
account creation, secure session restore, rotating refresh tokens, logout,
restricted HTTPS requests, cached profiles and the account-bound upload queue.
It has no dependency on UOpenGolf, the launch monitor, physics or map content.
Configuration is supplied by the host project's `DefaultGame.ini`.

The plugin also supplies an optional Slate account/profile panel and the existing
OGolf player API operations: profile sync/read/update, range-shot upload, per-club
averages/history and deletion. Its [integration guide](../Plugins/OpenGolfOnline/Docs/Integration.md)
explains how another Unreal project can use it. Public configuration is separate
from credentials. No client secret is shipped with a native application.

The UOpenGolf adapter interprets launch-monitor measurements and simulation
results. It captures the account, club and save preference at strike, correlates
late club data, excludes replays and interrupted/safety-stopped shots, and sends
completed Range-mode shots. Local F9 replay recording remains independent.

## Screens and data

The home and pause menus share a header: **UOPEN GOLF**, the launch-monitor status,
an account icon and a cog. While signed out the account icon is a login symbol that
opens **Settings › Player**; while signed in it is the logout icon. The cog is the only
way into Settings. Settings › Player holds the account: while signed out, the QR
sign-in, browser sign-in and account-creation choices and no profile options; while
signed in, the member-since date, display-name editing and **Shot history › View**,
with no sign-in, account-creation or "Try again" controls. While a saved session is
being restored or the profile is loading, the account block reads "Loading your
account…". Auth0's hosted page retains the existing OGolf application name.

Shot history shows club averages, with clubs abbreviated (**DR**, **7i**, **PW**), then
one club's shots, with confirmed deletion and pipe-delimited CSV export. Shot times
show in the **Time zone** chosen in Settings › Player: this computer's zone by default,
or UTC or a listed zone such as Pacific, with daylight saving applied. The CSV keeps
the API's UTC times. Statistics use the existing 90-day window and history limit of
200 shots. Pending and rejected upload counts are not shown. Each **View** opens on
freshly loaded club averages. The history filter does not change the club selected
for the next shot. The range HUD has a clickable club picker using the approved
OGolf artwork. Tab still changes the next shot's club. Recording (**F9** or the HUD's
record icon) decides whether shots are saved: while recording and signed in, eligible
range shots are queued for the online profile as well as written locally. There is no
separate save-to-profile setting; an older player file's `save_to_profile` value is ignored. The current server excludes lob wedge
and putter from cloud range statistics; both remain selectable for play.

Shot JSON matches the existing API fields and units: carry and signed roll in
yards, launch speed in mph, angles in degrees and spin in rpm. Missing or invalid
measurements remain null. A shot retains its generated UUID across retries.
UOpenGolf identifies its course as `uopengolf-range`. No database migration is needed.

## Session and local storage

Access tokens stay in memory. Refresh tokens use Linux Secret Service or Windows
Credential Manager under the UOpenGolf storage name, independently of OGolf's
credentials. Concurrent refresh requests share one operation. Rotated tokens are
saved before subsequent identity requests. Session versions prevent late work
from restoring a signed-out session or changing a replacement account.

The API validates JWT signature, issuer, audience, expiry and authorized client;
player ownership is determined from the authenticated subject. The local account
hash only separates cached queues and never grants access. The plugin does not
trust cached profile data as a login credential or send ID tokens to the API.

The profile and pending shots are stored under the platform's user settings
folder in `UOpenGolf/Online/player.json`, without credentials. Writes replace the
file atomically; Linux files are private to the current user. An unreadable file
is preserved with a `.corrupt-…` suffix. The queue holds up to 5,000 shots and sends
batches of at most 100. Network failures and rate limits keep pending data; invalid
batches are divided to isolate rejected records. Switching accounts clears the
old queue, while a same-account reconnect keeps it. Explicit logout clears local
identity and queued data, attempts a final batch upload and revokes the refresh
token. Before any of that it writes a `signed-out` marker beside `player.json`. While the
marker exists, startup never restores a saved session, even if the credential store
could not delete the token. The next successful explicit sign-in removes it. If the
marker cannot be written, logout stops and says so, rather than leaving a session that
would come back at the next start. If it cannot be removed after a sign-in, the session
works but is not restored automatically, and the page says so. Deletion waits for active uploads and removes the shot from the local queue
before asking the server to delete it.

If the OS credential store is unavailable, the session stays in memory and the UI
explains that it will not survive exit. Local write failures are surfaced and
retried. Offline gameplay remains available.

## Reference and validation

The reference is the maintainer's OGolf implementation at revision `41ada2d` and
the adjacent Spring/Node player API contracts. No backend deployment or database
schema was changed by this implementation.

Validation results and remaining platform/manual acceptance checks are recorded
in [Phase status](PhasesStatus.md). The native app's live device-authorization
request has returned HTTP 200 over verified TLS. That probe does not establish
that a player has completed login or that cross-game reads/writes have been
accepted against a real account.

Only Unreal build-rule C# files changed. The requested
`OpenFairway.sln --exclude addons && dotnet format OpenFairway.sln --verify-no-changes --exclude addons`
command cannot run in this checkout: `OpenFairway.sln` is absent. UnrealBuildTool
is used for these build rules; missing solution checks are not counted as passed.
