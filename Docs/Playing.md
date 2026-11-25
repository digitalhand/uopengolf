# Playing UOpenGolf

Choose **Training** to practise from the tee or a selected lie. For a scored round, open **Courses**, select **Savanna Range** to view its scorecard, then choose **Play**. Click the viewport once to focus the gameplay keys.

### Training games

In Training, open the target icon beside **X**. Choose a game and a **Winning radius** of **3 ft**,
**6 ft**, or **10 ft**; the Distance Ladder also asks for **3** or **5** tries per target. Press
**Start** to begin. Both games show
one white ring with the chosen radius. A ball counts as a hit when it stops inside or touching
the white line, or goes in the cup. Each shot starts from the tee. Aim with the minimap or A and D.

- **Distance Practice.** Hit as many balls as you like; there is no shot limit. The flag starts
  25 yards from the tee. Whenever the ball is at rest, use the slider to choose a distance from
  **10 to 250 yd** in 5-yard steps. The panel shows how many shots hit the ring at the current
  distance, and changing the distance starts a new count. Balls already hit stay on the ground.
- **Distance Ladder.** The targets are 15, 25, 35 … 145 yd, then 150 yd. One hit advances you to
  the next target. If you use all your tries without a hit, the ladder drops back one target,
  or stays at 15 yd if you are already there. The flag moves two seconds after the ball stops
  or just before you hit again. A hit at 150 yd completes the ladder.

Pressing **Start** during a drill opens a confirmation that lists the new game, its radius and,
for the ladder, its tries.
Choose **Exit Drill** to switch or press **Esc** to keep playing the current drill.

Finished balls stay where they stopped throughout Training. Press **Esc** or click **X** for
**Restart Drill** or **Exit Drill**. Restart (**Play Again** once the ladder is finished) keeps your
choices, clears the balls hit during this run and starts a new count; the ladder starts again at 15 yd. Exit restores
the original flag and keeps your finished balls. Press Esc again to return to the game. Leaving
Training clears the range.

### Installing and updating

On Windows, download `UOpenGolf-<version>-Windows-x64-Setup.exe` from the
[latest release](https://github.com/digitalhand/uopengolf/releases/latest) and run it. It installs for your account
only (no administrator prompt) and adds UOpenGolf to the Start menu; uninstall it from **Apps**. The `.zip` is the same
game as a portable folder.

On Ubuntu amd64, download `UOpenGolf-<version>-Linux-amd64.deb` and open it with your
graphical package installer. Installation asks for administrator authorization; no `chmod`
is needed. Launch **UOpenGolf** from the application menu. If your desktop does not offer a
package installer, run `sudo apt install ./UOpenGolf-<version>-Linux-amd64.deb` from the
download folder, replacing `<version>` with the downloaded version. Install a newer `.deb`
the same way after closing the game. Remove it through your package manager or
`sudo apt remove uopengolf`; your settings and recordings are kept. See [Ubuntu installer](Releases.md#ubuntu-installer).

On Fedora or openSUSE (x86_64), download `UOpenGolf-<version>-Linux-x86_64.rpm` and run
`sudo dnf install ./UOpenGolf-<version>-Linux-x86_64.rpm` (Fedora) or
`sudo zypper install --allow-unsigned-rpm ./UOpenGolf-<version>-Linux-x86_64.rpm` (openSUSE). See
[Fedora and openSUSE installer](Releases.md#fedora-and-opensuse-installer). On any other distribution, extract the
`.tar.gz` into a folder you own and run `UOpenGolf.sh`; it needs no administrator access.

The game checks for a newer release each time it starts. When one exists, the home and pause pages show
**Update to v…**, and the version in the corner says it is available. **Settings › Data & system › Updates** checks on
demand. The update screen shows what's new and the download size; **Download and install** shows a progress bar, then the
game closes, installs the update and starts again. Your settings, sign-in and recordings are kept. Leaving the update
screen cancels a download. On Linux, a game installed from the `.deb` or `.rpm` asks for your administrator password
after it closes, because the package manager installs the update; cancel the prompt and the current version starts
again. A `.tar.gz` folder updates without a password. Where the game can't update its own folder, the button opens the
release page instead.

### Controls

| Key | Action |
| --- | --- |
| **C** | Connect to a Square Home (scans for up to 30 s) |
| **T** | Disconnect the launch monitor |
| **Tab** | Next club: Driver (**DR**), 3/5/7 Wood (**3W**–**7W**), 4–9 Iron (**4i**–**9i**), Pitching/Gap/Sand Wedge (**PW**, **GW**, **SW**), Lob Wedge (**LW**; club code `0d06`; the Square Home arms and measures with it, but it may treat it like a sand wedge), Putter (**PT**), then back to Driver |
| **F9** | On the range: record shots on/off, the same as clicking the record icon; while signed in, recorded shots also go to your online profile. Does nothing on the hole |
| **H** | Hit the selected test shot again |
| **P** | Open or close the test-shot list |
| **R** | In free Training, reset the ball to its tee or practice lie |
| **B** | In free Training, place the ball, then click the ground where it should go (right-click or Escape cancels) |
| **W** | In Training, open or close the wind controls under the wind indicator (the same as clicking the indicator) |
| **Space** | While the ball rolls on the ground (not in the air): hold to play the roll four times faster, or tap to skip to where it stops. The ball stops in the same place either way, and the game is back at normal speed the moment it rests or drops into the cup. Nothing on screen marks it. In F6 mode Space presses the focused HUD button instead |
| **A** / **D** | Turn the aim line left / right: a tap turns exactly 0.25° (0.1° with **Shift**), a hold starts slowly and speeds up to 20°/s (with **Shift**, stays at 2°/s), up to 45° either way (from the target line on the range, from the line to the cup on the hole), while the ball waits for the shot. The camera turns with it, the aim stays set across shots, and Offline is measured from it. Use it to hit into the rough |
| **~** | Unreal console, for the [settings](#settings) below |
| **Escape** | During a drill, open or dismiss the drill dialog. In free Training, close the current tool or open the pause menu |
| **F6** | Move the keys onto the HUD's buttons, or back to the game (see [Without a mouse](#without-a-mouse)) |

Use the mouse for the toolbar, settings cog, wind controls, test-shot panel, and ball
placement. Hover over a toolbar icon for its name or key; the training games button has no tooltip.

#### Without a mouse

The HUD's buttons also work from the keyboard or a gamepad:

- **F6** (gamepad: **View**) moves the keys onto the HUD's buttons. These include the Training toolbar (back, games, club,
  record and place ball), the settings cog, the wind controls and the test-shot list.
- **Up**/**Down**, **Tab**/**Shift+Tab** or the D-pad move between them.
- **Enter**, **Space** or **A** presses the focused one.
- **F6** hands the keys back to the game so **Tab** changes club again. **Escape** or gamepad **B**
  opens the drill dialog during a training game and backs out of HUD controls in free Training.
- The gamepad **Menu** button opens the menu, and goes back a page while it is open. In the
  menu, **B** goes back like **Escape**, and the shoulder buttons change Settings category.
- While placing a ball (**B** key or the ball button), the pointer is a ball icon centred where the
  ball will go; the left stick moves it and **A** places the ball there.

Only simulated key events are covered by the automated tests; a real gamepad has not been
checked.

### HUD

- **Ball-ready glow**: while the ball waits at address, a soft ring hugs its outline:
  red until the launch monitor detects the ball, green once it does. Without a launch
  monitor it stays red. It hides while the ball flies or rolls, until it is teed up again,
  while you place a ball and on a practice hole (a placed ball).
- **Top right.** The cog opens Settings directly. The heading reads **Training**,
  or **Training · Ball Placement** while placing or playing a placed ball. In Courses it
  shows **Savanna Range**, and the tile grows to twice its height for two large lines below it:
  the hole (**Hole 1**), then its par and length (**Par 4 · 400 yd**). The shot tiles below
  show game results, club data, and raw launch-monitor values. `…` means a value is pending,
  `—` means it is missing, `*` means it is derived, and `L`/`R` indicate direction.
- **Top left.** The club button shows the selected club's short label beside its icon, at the size of the
  course's stroke and distance numbers (for example **7i**, **DR** for the driver or **PW** for the pitching wedge);
  click it to open the club drawer. In a course round, while you are signed in, your avatar and display name follow
  it (see [Menu and account](#menu-and-account)). The Training ball icon lets you choose a practice lie. The record icon toggles range recording, as does **F9**. It turns red
  while recording or amber if a save fails; each failed save is logged with its error (see [Recording errors and retries](#recording-errors-and-retries)).
- **Course round.** Below the toolbar, the hole tiles show the current stroke, distance **To Pin**, and
  **Putts**. A banner names the score when the ball drops.
- **Bottom left: hole map.** A view of the hole from above, tee at the bottom and flag at the top, with the
  ball as a white dot. Shading and contour lines show the ground's rise and fall. The view zooms in on the ball and
  the flag as you play, and holds still while the ball flies.
  - **Header:** your lie (Tee, Fairway, Rough, Bunker, Green, Fringe and so on). In a course round:
    - Beside the lie is what the lie takes off your ball speed with the selected club, for example **Rough −8%**.
    - On the right is what it takes off your spin, for example **Spin −25%**.
    - Both are blank where the lie costs nothing (see [Lie penalties](#lie-penalties)).

    The hole and par are in the top-right heading. While the ball is in the air the header keeps the lie and penalties
    you hit with; once the ball lands it names the surface the ball is on.
  - **Flag:** the distance to the pin.
  - **At address:** your aim runs to a target ring that shows its own distance and height change. A thin line
    continues from the ring to the flag. Click anywhere on the map to aim at that point and put the target there. A
    and D turn the aim and keep the target's distance.
  - **Footer:** the elevation to the pin: how far above (▲) or below (▼) the ball it is, in feet and inches (for
    example `▲ 3"` or `▼ 5' 1"`), or Level.
  - When the ball is off the map, an arrow on the edge points to it. Clicks on the map never place the ball, and the
    map shrinks to fit short windows.
- **Wind** (top centre): the wind speed and an arrow for the way it blows, turned with the
  camera. In Training, click it (or press **W**) to slide out the wind controls under it:
  - an **On/Off** switch; off is calm, and the speed is kept for when it is switched back on;
  - **Speed**, slid from calm to 30 mph in 1 mph steps;
  - **Direction**, slid in 5° steps through where the wind comes from against the target
    line (0° headwind, 90° from the right, 180° tailwind, 270° from the left), named in golf
    terms under the slider (Headwind, Into · left to right, Left to right, Helping · right to
    left and so on).

  The arrow and the flag follow as you slide, and the next shot flies in the new wind; a shot
  already in the air keeps the wind it was hit in. Click the indicator again, or press **W**
  or **Escape**, to close the controls. Wind set from the console shows in them too, and
  nothing set during Play is saved to the map.
- **Test shots [P] | Hit [H]** (bottom left): shown when manual shots are enabled and
  no launch monitor is connected. See [Selecting test shots](#selecting-test-shots).
- **Version** (bottom-left corner, under the test-shot button): `v` plus `ProjectVersion`
  from `Config/DefaultGame.ini` (`[/Script/EngineSettings.GeneralProjectSettings]`), Change the version there; packaged builds use the same value.
  Release builds from CI use the tag's version instead (see [Versioning](Releases.md#versioning)).

On the range, a stopped ball stays put for 3 seconds with its result shown, then goes back to
the tee. The tiles keep the last shot until the next one.

### Playing the hole

Open **Courses**, select **Savanna Range**, and choose **Play**, then **Start round** in the [Club House](#club-house). Its wind
starts at 3 mph from right to left. The pin is 400 yards
out on a gently contoured green, with a fairway bunker on the left of the landing area and a
bunker right of the green.

- The ball is played as it lies. Each shot that stops is a stroke; a shot hit from the green
  is also a putt. Two seconds after it stops, the ball turns to the cup and the camera moves
  behind it; on the green or within 30 yards the view is lower and closer, looking along the
  putt.
- The cup is regulation size (4.25 in). A putt drops if it is slow enough for its line (about
  1.6 m/s, or 3.7 mph, through the centre, less toward the edge of the cup). A firmer one
  dips into the cup, strikes the far lip and hops over it, losing speed, and off centre the
  rim turns it; one only a little too firm through the centre can hit the back of the cup and
  drop. The hole tiles say how fast it crossed and how slow it had to be. On a stimp-10 green
  a putt drops if it would finish no more than about 8 feet past (a little more through the
  dead centre). A shot landing in the cup drops too. The flag lifts out while the ball is
  within 5 feet.
- From 8 feet out the camera slowly closes in behind the ball. It backs out as the ball rolls
  away, to the normal distance by 8 feet past or once a miss stops; a holed ball keeps the
  close view through the hole-out banner.
- After the banner, the final scorecard shows par, strokes and score relative to par (**E** for level, otherwise
  such as **+1** or **-2**). Choose **Play again** for a fresh round or **Courses** to return to the browser.

### Club House

**Play** on a course card opens the **Club House** on the Players tab. Use the back arrow beside the title,
**Escape**, or controller **Back** to return to your selected card and search. The game remembers each course’s
round choices until you close it.

| Tab | Row | Choices | Starts at |
| --- | --- | --- | --- |
| Players | Player | Your avatar and display name, or **Guest** when signed out. The open seat is reserved for a future player. | |
| Players | Tees | Each tee set and its full-course distance | The first set |
| Match | Gimmie | On or off | **Settings › Game Defaults › Gimmie radius** |
| Match | Gimmie radius | 3 ft or 6 ft | **Settings › Game Defaults › Gimmie radius** |
| Course | Green speed | Move the slider in whole feet from 8 to 15. 8–9 Slow, 10–11 Medium, 12–13 Fast, 14–15 Tour | **Settings › Game Defaults › Stimp** |
| Course | Holes to play | **All**, **Front 9** or **Back 9** (on courses with more than nine holes), or tick the holes to play; **Custom** shows a hand-picked set. A line under the boxes adds up the holes, par and length | All |

Course details sit beside the settings on wide windows and follow them in the same scrolling area on narrow windows.

- **Course information:** where the course is, its elevation, par, designer, and its total length from the picked
  tees. Elevation is information only; the ball flies the same on every course.
- **Course conditions:** the round's wind. It is not a choice: each round gets a random wind from calm to 13 mph,
  from any direction. It stays fixed over the whole course, as it does outside, so it plays differently on each hole.
  The panel names it against the tee shot of the first hole you picked, such as "Right to left · hole 1".

The bottom bar stays visible while you scroll. It shows the selected holes, par and distance. **Reset** restores the starting choices and keeps the wind. **Start round** begins play.

- Rounds are played in daylight.
- Holes play in order of their numbers. With nothing ticked, **Start round** waits and the page says why. The
  final scorecard adds up each nine you played (**Front 9**, **Back 9**) and the total against par for the holes played.
- **Play again** on the final scorecard starts another round with the same choices and a new wind.
- The wind indicator shows the round's wind for the hole you are on, and the HUD's wind controls stay closed in a round.
- Fairway speed is not part of the setup: one value in **Settings › Game Defaults** applies to every course and to
  Training.

### Downloaded courses and 18-hole rounds

Courses that are not part of the game appear in **Courses** as cards with **Download** when a course catalog is
configured. A downloaded course plays straight away and keeps playing offline. Its card adds **Update** when a newer
version is published and **Uninstall**; an update and an uninstall finish the next time the game starts.

Hunter Ranch (Paso Robles, California) is the first downloadable course: 18 holes, par 72, from the club's published
scorecard. Its tees and cups are placed from map data (see [Course Designer](CourseDesign/course_designer.md#limits)).

- Select the card and choose **Play**. In the [Club House](#club-house), pick the tees (Blue, White, Combo, Silver or
  Red), the green speed, the Gimmie and the holes, then **Start round**.
- Each hole opens along its line of play, so the first aim follows a dogleg. After each hole-out the score is kept and
  the next picked hole starts; after the last one the scorecard shows each nine played and the total.
- A ball that comes to rest in a lake or out of bounds costs one penalty stroke and is replayed from where it was
  hit; the line under the hole tiles says which. A ball that carries a lake and lands beyond it plays on.

#### Lie penalties

In a course round, a shot from the rough or a bunker plays with less ball speed and spin than your launch monitor
measured. Your launch angle and direction stay as measured. The minimap header shows both cuts before you hit. They
depend on the kind of club selected:

| Lie | Woods | Irons | Wedges |
| --- | --- | --- | --- |
| Tee, fairway, fringe, green | none | none | none |
| Rough | −10%, spin −20% | −8%, spin −25% | −6%, spin −35% |
| Fairway bunker (30 yd or more from the pin) | −15%, spin −25% | −10%, spin −25% | −8%, spin −25% |
| Greenside bunker (within 30 yd) | −40%, spin −30% | −40%, spin −30% | −40%, spin −30% |

- The putter is never penalised: the grass already slows the roll.
- A greenside bunker shot is an explosion shot, so swing harder than you would for the same distance from grass.
- These are provisional standard values. A course can set its own. Their sources and the estimated parts are in
  [Lie rules](../Plugins/OpenGolfCourse/Docs/Reference.md#lie-rules).
- Training never applies them, and shot recordings keep the shot as your launch monitor measured it.

### Changing clubs

The club button at the top left names the selected club. Press **Tab** to select the next club, or click the button
and pick one from the list. When a Square Home is connected, the club is also sent to the
device. Choose the club before you hit: recorded shots are tagged with the club selected
when the shot arrives. The club resets to Driver at the start of every Play session.

#### Auto club

In a course round, the game picks a club for you on the tee and each time the ball turns to the pin for the next shot.
Turn this off in **Settings › Player › Auto club**. Tab or the club list still changes the club, and your choice stays
until the next shot. A club you pick while the stopped shot is still shown is kept when the ball turns to the pin.

The game picks a club in this order:

1. **Putter** on the green or fringe.
2. **Sand wedge** in a greenside bunker.
3. Otherwise, the club whose average PGA Tour carry, less its lie penalty, is nearest the distance to the pin. Of two
   equally near clubs it takes the longer one.

The Driver is picked only on the tee. Carries come from TrackMan's PGA Tour averages, so most players will need more
club than it picks:

| Club | Carry (yd) | Club | Carry (yd) |
| --- | --- | --- | --- |
| Driver | 275 | 7 Iron | 172 |
| 3 Wood | 243 | 8 Iron | 160 |
| 5 Wood | 230 | 9 Iron | 148 |
| 7 Wood | 225 | Pitching Wedge | 136 |
| 4 Iron | 203 | Gap Wedge | 124 |
| 5 Iron | 194 | Sand Wedge | 110 |
| 6 Iron | 183 | Lob Wedge | 95 |

Sources:
- Driver to Pitching Wedge are TrackMan's PGA Tour averages, as listed in GolfLink's
  [swing speed and distance chart][carry-golflink] (published March 2024; it gives no year for the data).
- That table has no 7 Wood or wedges below the pitching wedge. The 7 Wood uses its hybrid carry (225 yd), and the gap,
  sand and lob wedge carries are estimates.
- TrackMan's 2023 figures, reported by [Golf Monthly][carry-golfmonthly], run 4 to 7 yards longer.

[carry-golflink]: https://www.golflink.com/instruction/swing-speed-chart
[carry-golfmonthly]: https://www.golfmonthly.com/tour/how-far-pga-tour-players-hit-every-club-in-the-bag

### Recording shots

Real launch-monitor shots can be saved, tagged with the selected club, to replay them
in other tools (such as openfairway's shot injector) and
compare with this engine. While you are signed in, recording also saves eligible range
shots to your online profile, for the club averages and shot history in Settings › Player
(see [Menu and account](#menu-and-account)). Recording off saves
nothing, locally or online. Recording is a range feature: the hole shows no record icon,
**F9** does nothing there, and hole shots are never recorded. A fresh Training session starts with recording off.

1. Connect the launch monitor (**C**) and check for **Ready** in Settings › Launch Monitor.
   The record icon is a white ring while recording is off.
2. Press **F9** or click the record icon to start recording. The dot turns red. Press **F9** or
   click it again to stop. Recording is off at every Play start.
3. Select the club with **Tab** before each shot; the club selected when the shot
   arrives is the one recorded.

Each time recording is switched on, the first shot reserves a unique session folder
(created when its first record is saved):
`Documents/UOpenGolf/Saved/ShotRecordings/session_<YYYYMMDD-HHMMSS>-<unique-id>/` in your user folder
(on Windows `%USERPROFILE%\Documents\...`; on Linux the XDG documents folder, usually
`~/Documents/...`), so recordings survive rebuilds and reinstalls. Each shot is one file named
`club_<club>_shot_<n>.json`, such as `club_driver_shot_1.json` then
`club_7i_shot_2.json`: `n` counts every shot in the session, whatever the club. Club
codes are `driver`, `3w`, `5w`, `7w`, `4i` to `9i`, `pw`, `gw`, `sw` and `putter`.
The club is the one selected when the shot arrived, not a measurement. Imported into the
shot library, it is kept as `reported_club` and never read by the physics or the comparison.

Only shots from the connected device are recorded; replay and test shots (**H**,
**P**) never are. A completed shot waits for its club response, or until three seconds
have elapsed since receipt, before being saved. Club data arriving after a short shot
stops is still included. Each engine result is matched to the simulation that belongs
to that recorded shot. A shot interrupted before rest has no engine result;
it can still receive club data until its deadline. Turning recording off or ending
Play saves the information available immediately.

The file holds the ball and club data (ball speed in mph, angles in degrees, spin in rpm;
positive HLA, spin axis and side spin are to the right) plus a `UOpenGolf` block:

```json
{
  "DeviceID": "Square Home", "Units": "Yards", "ShotNumber": 1, "APIVersion": "1",
  "BallData": { "Speed": 114.46, "VLA": 6.95, "HLA": -0.63, "TotalSpin": 1933, "SpinAxis": -1.42,
                "BackSpin": 1932, "SideSpin": -48, "CarryDistance": 0 },
  "ClubData": { "Path": -0.18, "FaceToTarget": -0.74, "AngleOfAttack": -5.16, "Loft": 12.10 },
  "ShotDataOptions": { "ContainsBallData": true, "ContainsClubData": true },
  "UOpenGolf": { "Club": "driver", "ClubName": "Driver", "LeftHanded": false,
                 "DeviceShotNumber": 7, "RecordedLocal": "2026-10-15T14:03:22",
                 "Firmware": "...",
                 "Engine": { "CarryYd": 95.30, "TotalYd": 113.70, "RollYd": 18.40, "LateralYd": -1.40,
                             "ApexFt": 21.00, "HangTimeS": 3.10, "LandingAngleDeg": 12.00, "AtRest": true } }
}
```

Decimal values (speeds, angles, distances, times) are rounded to two decimal places;
whole numbers (spin rpm, shot numbers) are written as they are. Values the device did
not measure are left out rather than written as 0, and
`ClubData` is left out (with `ContainsClubData: false`) when the shot has no club
reading. The maintainer verified recording with a real Square Home on 2026-10-15;
the subsequent retry and delayed-data changes have automated coverage only.

To check that a shot was saved, look for `Recorded shot <path>` under
`LogUOpenGolfRecording` in the Output Log.

### Recording errors and retries

Files are written to a temporary file in the recording folder and renamed only after
the write closes successfully. Existing final files are never replaced. If creating
the folder, writing, or renaming fails, the shot remains in memory and retries once
per second during Play. Later shots retain their own file numbers and session folders.
The saved count advances only on success.

The record icon stays amber while records are unsaved, including when disconnected or recording is
off, and each failed save is logged as a `LogUOpenGolfRecording` warning with its error and file. Restore access to the folder while Play
remains open to let the retries complete. Ending Play makes one final save attempt
and logs any remaining unsaved count and destination. **There is no recovery across
Play sessions or application restarts**; failed recordings still in memory are lost.

### Menu and account

The home page, and the pause menu that **Escape** opens during Training, share a top bar:
**UOPEN GOLF**, an account icon and a settings cog. The cog opens **Settings** from either the
menu or the training HUD. While signed out the account icon signs in (it opens Settings › Player);
while signed in it is the logout icon. The launch monitor's connection status is in
**Settings › Launch Monitor**.

While signed out, the home page also shows the sign-in QR code under **Exit**, so you can sign
in with a phone without opening Settings (see below). It disappears once you are signed in.

**Settings › Player** holds the account:

- Signed out: **Sign In** and **Create Account**, and no profile options. Both open the login
  page in this computer's browser. **Sign In** always shows the login page, even if the
  browser is still logged in, so after signing out you can sign in as a different player.
- Signed out, every Settings page also shows a QR code under the categories on the left,
  captioned **Sign-In**. Scan it with a phone, check that the phone shows the same code as the
  screen, and approve. It signs in the account your phone's browser is logged in to. The QR
  code disappears once you are signed in.
- Signed in: the member-since date, the **Display name** (**Save display name** saves it at
  once) and **Shot history**. Its **View** shows club averages for the last 90 days:
  ball speed (mph), carry and total (carry plus roll, in yards) and the shot count, with the
  longest average carry at the top. Carry averages every shot from the server; ball speed and
  total average each club's newest 200 shots and fill in club by club after the list appears
  (`…` while loading, `—` when no shot measured it). Select a club to view individual shots.
  Scroll the shot table horizontally to see every measurement. Select a row to delete that shot. Use **Clear club** inside a club
  or **Clear all** on the overview. Clear actions include pending shots and the newest 200 saved shots per club from the last 90 days.
  Shot times and the member-since date use this computer's local time. CSV exports use UTC.
- Signed in, the right half of **Profile** is the **Avatar** picker. The course shows, beside the club button, the
  avatar you pick here; without a pick, your account's own picture, or the Golfer when the account has none. Your
  account picture is the first tile once it has downloaded; the 24 built-in avatars follow. The one shown has a
  ring, with its name above the tiles. A pick is saved with **Apply**, on this computer, for the account that made
  it. The account picture is downloaded from the address your login service gives, with no sign-in details sent,
  and is not stored on disk.

### Settings

Wind controls on **RangeSettings** save with the map. Changes made during Play do not
save back to the map. The settings menu saves supported preferences when you apply them. Console values can
remain between Play sessions in the same editor process.

| Setting | Default | How to change |
| --- | --- | --- |
| Club | Driver | **Tab** |
| Record shots | Off | **F9** or the record icon |
| Recording folder | `Documents/UOpenGolf/Saved/ShotRecordings` | **Settings › Data & system**: type a folder, or **Browse** on Windows |
| Selected test shot | First preset (low wood) | **P**, then pick one |
| 3D grass and ground cover | On | Console: `uopengolf.Grass3D 0` (off) or `1` (on); see [Range assets](RangeAssets.md) |
| Wind speed | 0 mph (calm) | In Play: click the wind indicator (up to 30 mph). Saved with the map: select **RangeSettings → Details → UOpenGolf → Wind → Wind Speed (mph)** |
| Wind direction | 0° (headwind) | In Play: the wind controls' **Direction**. Saved with the map: **Wind From (degrees)**: `90` from the right, `180` tailwind, `270` from the left |
| Editor flag preview | On | **Preview Wind in Editor**; viewport Realtime must be enabled |
| Green speed | Stimp 10 ft | **Settings › Game Defaults › Stimp** (Training, and the starting choice for a round); a round's own on its [Club House](#club-house). Console: `uopengolf.GreenStimp 8` to `15`; applies from the next shot |
| Fairway speed | Standard (7.4 ft, the calibrated fairway) | **Settings › Game Defaults › Fairway speed**: 6, 7.4, 8, 9 or 10 ft on the stimpmeter, for every fairway in the game from the next shot |
| Gimmie radius | Off | **Settings › Game Defaults › Gimmie radius**: Off, 3 ft or 6 ft (earlier 5, 8 and 10 ft choices became 6 ft); a round's own on its round setup |
| Auto-reconnect after a dropped link | On, up to twelve attempts | `UOpenLaunchMonitorSubsystem::bAutoReconnect` |
| Left-handed | Off | `UOpenLaunchMonitorSubsystem::SetLeftHanded` |

Shots use the saved terrain, with default air at 75 °F and sea level. Wind is editable before Play, and
during Play from the wind controls or the console; green speed has a console control. There
is no in-game weather or altitude menu yet. The flag previews wind in the editor and uses the ball's wind at flag height
during Play, about 72% of the authored speed, which is the 10 m reading. In calm air it hangs beside the pole. For editor controls, expected movement and
optional console commands, see
[Validate wind and flag movement](RangeAssets.md#validate-wind-and-flag-movement).

The wind's direction and its growth with height are tested; how much a strong headwind costs has not yet been
compared with published wind data (see the physics [known gaps](../Plugins/OpenFairwayPhysics/Docs/Reference.md#known-gaps)).

### Selecting test shots

Click **Test shots [P]** at the bottom left, or press **P**, to open the dropdown.
Choose a preset and click **Hit selected shot [H]**. Selecting an item only updates
the preview; **H** hits and repeats the selected shot from the tee. **P** or **Close**
returns to the range. The selection lasts for the current Play session.

Ten presets include a 114.5 mph low wood and a 75.7 mph mid iron, the Square Home
57.5 mph and putt captures, and a driver, iron, wedge, chip, high flop and straight
putt. The panel shows launch/spin inputs; synthetic samples are identified. See the
[replay catalog](../Plugins/OpenLaunchMonitor/Docs/Reference.md#replay-shots) for details.

#### Custom shots

Click **Custom** next to the dropdown to type in your own shot. The fields start as a
basic straight shot: 100 mph ball speed, 16° launch angle, 0° direction, 5000 rpm total
spin and 0° spin axis. Ball speed is in your speed units (Settings › Player); a negative
direction or axis is left, positive is right. Press **Enter** or click another control to
keep a value. Text that is not a number puts the field back, and a value beyond what a
launch monitor can report is limited to it. **Straight shot** restores the defaults.

**Hit custom shot [H]** hits the values, and **H** hits them again from the tee as often as
you like, so the same shot can be repeated exactly. Keys typed into a field stay there:
H, P and the other play keys only act once you leave the field, and Tab moves to the next
field. Choosing a preset from the dropdown goes back to the presets; the custom values are
kept until you return to the main menu or quit. Custom shots are not recorded, like the presets.
