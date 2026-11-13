# Using Open Golf Online

Copy the plugin to another Unreal project, enable it, and add `OpenGolfOnline`
to that project's module dependencies. It depends only on Unreal modules and
Unreal's bundled OpenSSL and libcurl. It has no dependency on UOpenGolf,
OpenLaunchMonitor, the physics library, maps, or project artwork.

The host supplies public configuration in `DefaultGame.ini`. Quote URLs because
Unreal treats unquoted `//` as a comment. A native application has no client secret.

```ini
[OpenGolfOnline]
Authority="https://your-tenant.example.com/"
ClientId=your_public_native_client_id
Audience="https://your-api.example.com"
ApiOrigin="https://your-server.example.com"
ApplicationName=YourGame
StorageName=YourGame
+CallbackPorts=47801
+CallbackPorts=47802
+CallbackPorts=47803
```

Use a different `StorageName` in each game. Reusing an Auth0 application does not
mean sharing refresh tokens: independently rotated tokens must stay separate.
Register the callback URLs `http://127.0.0.1:<port>/callback` in Auth0. Enable the
Authorization Code, Device Code and Refresh Token grants, rotating refresh tokens,
and offline access for the API. The existing backend must accept this application's
client ID as well as its API audience.

Test logout in both games when reusing one native application. Auth0's tenant
setting **Refresh Token Revocation Deletes Grant** determines whether revocation
also invalidates other tokens for that user/application/audience grant.
See [Auth0's revocation documentation](https://auth0.com/docs/secure/tokens/refresh-tokens/revoke-refresh-tokens).
The plugin does not change tenant policy.

Get `UOpenGolfOnlineSubsystem` from the game instance. It resumes a securely stored
session, exposes account/profile state, and provides QR sign-in, browser sign-in,
account creation, cancellation and logout. `SOpenGolfAccountPanel` is an optional
Slate view; hosts can build their own UI from the same state:

- **Account state.** `GetAccountState()` returns `EOpenGolfAccountState`: `Restoring`,
  `SignedOut`, `SigningIn`, `LoadingProfile`, `Ready`, `Reconnecting` or `SigningOut`.
- **Change counters.** `GetRevision()` changes with any state change.
  `GetHistoryRevision()` changes only when club averages or shot history change.
- **Events.** `OnAccountStateChanged`, `OnProfileChanged`, `OnHistoryChanged` and
  `OnStatusChanged` fire on the game thread for hosts that prefer them to polling.

Logout writes a `signed-out` marker next to the local profile file. Until the next
successful explicit sign-in, startup does not restore a saved session from the
credential store.
`SOpenGolfAccountButton` supplies the header's account icon: by default the login
icon (tag `AccountSignIn`), shown only while signed out; with `.SignOut(true)` the
logout icon (tag `AccountSignOut`), shown only while signed in. Each sets its own
visibility, so a host places both side by side and one shows at a time; the host
decides what a click does. A settings cog belongs to the host: `SOpenGolfIconButton`
draws one with `EOpenGolfAccountIcon::Settings`. Icon buttons take a `Label` that
names them for screen readers; no plugin widget shows a tooltip.

`SOpenGolfAccountPanel` shows one part of the account, chosen with `.Page(...)`:

- `EOpenGolfAccountPage::Account` (default): login choices while signed out, the
  profile (member since, display name) while signed in. It is laid out at its content
  height without a scroll box of its own, so a host can place it among its own rows.
- `EOpenGolfAccountPage::Shots`: club averages, with clubs abbreviated (`7I` shows
  as **7i**), then one club's shot history; a sign-in hint while signed out.

`.TimeZone(...)` takes an IANA zone ID such as `America/Los_Angeles` for shot times and
the member-since date (empty is the computer's zone; ICU applies daylight saving), and
`.TimeZoneName(...)` the short name in the time column's caption, such as `PT`. CSV
export keeps the API's UTC times.

Both widgets take an optional `FOpenGolfAccountStyle` (`.Style(...)`): a font
factory, text, label, accent and icon colors, text sizes, button styles and the
section background. The default is the plugin's own look; pass the host's values
so the account page matches its menus. `SOpenGolfIconButton` draws the same line
icons (sign in, settings, sign out, back, close) for host controls.

By default the panel draws its own title row: a back icon while one club's shot
history is open, the page title and the logout icon while signed in. All three follow
the page and the session as they change (`OpenGolf.Online.PanelHeading`). A host with its own page
header passes `.ShowHeading(false)`, binds its title to `GetTitle()` (the panel
changes page without the host rebuilding) and routes its back control and Escape
through `HandleBack()`, which leaves shot history and returns false when there is
no inner page to leave. Club rows are tagged `ClubRow:<club>` and the panel's back
control `PageBack`, for tests and automation.

The transport restricts requests to configured HTTPS origins and the existing
OGolf player API routes. It verifies certificates and hostnames, refuses redirects,
bounds response sizes, and runs requests off the game thread. Completion callbacks
run on the game thread. The plugin's player API client deliberately targets
`/api/v1/ogolf/me`; it is not an unrestricted HTTP client.

Hosts produce completed shot JSON using the shared API contract and call
`QueueShot` with the account key captured at launch. The plugin handles local
persistence, retries, duplicate-safe uploads, club statistics and history. Keep
sensor interpretation, simulation correlation, club selection and recording
eligibility in the host. `OnBeforeReadProfile` lets a host release a completed shot
waiting for late measurements. `OnAccountChanged` lets it discard unfinished work.

The player API always determines ownership from the validated access token. The
local account key only keeps queues separated; it is not an authorization credential.
Cached profile data never grants a login session. Never put tokens in shot JSON.

Linux credentials use Secret Service. Windows credentials use Credential Manager,
which protects them through the operating system. If secure storage is unavailable,
the session remains in memory, and the UI explains that it will not survive exit.
The local profile/outbox file contains player data but no credentials. API tokens,
private device codes and complete login request URLs are not logged.

## Checks

`OpenGolf.Online` covers callbacks, session/account behavior, display formatting
(`Format`) and the panel heading (`PanelHeading`) using fake HTTP and isolated
temporary storage. Its live probes are opt-in:

- `-OpenGolfOnlineLive`: starts an Auth0 device authorization request without
  completing a login or writing player data. Codes are excluded from the report.
- `-OpenGolfOnlineKeyring`: saves, reads and deletes a generated test credential
  under a unique storage name. It never reads the host game's credential.

The host project's game and visual tests cover its adapter and screens. Browser
approval, social-login setup, deployment settings and platform-specific packaging
still require checks in the intended runtime environment.
