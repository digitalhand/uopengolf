# Playing UOpenGolf

Choose Training from the main menu to enter the driving range. Every shot, from a Square Home or a test shot, is hit
from the tee until you switch to [playing the hole](#playing-the-hole). Click in the viewport once so it has keyboard focus; the keys below do
nothing until it does.

### Controls

| Key | Action |
| --- | --- |
| **C** | Connect to a Square Home (scans for up to 30 s) |
| **T** | Disconnect the launch monitor |
| **Tab** | Next club: Driver (**DR**), 3/5/7 Wood (**3W**–**7W**), 4–9 Iron (**4i**–**9i**), Pitching/Gap/Sand Wedge (**PW**, **GW**, **SW**), Lob Wedge (**LW**; club code `0d06`; the Square Home arms and measures with it, but it may treat it like a sand wedge), Putter (**PT**), then back to Driver |
| **F9** | On the range: record shots on/off, the same as clicking the record icon; while signed in, recorded shots also go to your online profile. Does nothing on the hole |
| **H** | Hit the selected test shot again |
| **P** | Open or close the test-shot list |
| **R** | Tee the ball up again straight away (on the hole, a new hole) |
| **G** | Play the hole / back to the range; the ball goes back to the tee either way |
| **B** | On the hole: place the ball, then click the ground where it should go (right-click or Escape cancels) |
| **W** | Open or close the wind controls under the wind indicator (the same as clicking the indicator) |
| **A** / **D** | Turn the aim line left / right: a tap turns exactly 0.25° (0.1° with **Shift**), a hold starts slowly and speeds up to 20°/s (with **Shift**, stays at 2°/s), up to 45° either way (from the target line on the range, from the line to the cup on the hole), while the ball waits for the shot. The camera turns with it, the aim stays set across shots, and Offline is measured from it. Use it to hit into the rough |
| **~** | Unreal console, for the [settings](#settings) below |
| **Escape** | Cancel placing the ball, or close the wind controls or the test-shot list; otherwise open the pause menu |
| **F6** | Move the keys onto the HUD's buttons, or back to the game (see [Without a mouse](#without-a-mouse)) |

The mouse is used for the test-shot panel at the bottom left, the play buttons at the top
left, the wind indicator and its controls at the top centre, the club picker, the status
box's two icons, and placing the ball. The shot tiles take no clicks. Nothing in the game
shows a tooltip; captions under the icons name what they do.

#### Without a mouse

The HUD's buttons also work from the keyboard or a gamepad:

- **F6** (gamepad: **View**) moves the keys onto the HUD's buttons. These are the club picker,
  the status icons, the play buttons, the wind controls and the test-shot list.
- **Up**/**Down**, **Tab**/**Shift+Tab** or the D-pad move between them.
- **Enter**, **Space** or **A** presses the focused one.
- **Escape** (gamepad: **B**) or **F6** again hands the keys back to the game, so **Tab**
  changes club again.
- The gamepad **Menu** button opens the menu, and goes back a page while it is open. In the
  menu, **B** goes back like **Escape**, and the shoulder buttons change Settings category.
- While placing a ball (**B** key or the ball button), the left stick moves the cursor and **A**
  places the ball there.

Only simulated key events are covered by the automated tests; a real gamepad has not been
checked.

### HUD

- **Ball-ready glow**: while the ball waits at address, a soft ring hugs its outline:
  red until the launch monitor detects the ball, green once it does. Without a launch
  monitor it stays red. It hides while the ball flies or rolls, until it is teed up again,
  while you place a ball and on a practice hole (a placed ball).
- **Shot tiles** (right edge): a header with the shot number and its state (in flight,
  rolling, at rest; empty before the first shot) and the club picker, then the results
  (total, carry, roll, apex, offline, hang time, landing), the club data and, in darker
  tiles, the raw launch-monitor values. The column keeps its width whatever the club or
  heading. `…` means the value is still coming, `—` that it is missing, `*` that it is
  derived from other measurements, and `L`/`R` the direction.
- **Club picker** (in the tile header): the club face and its abbreviation (**DR**, **7i**,
  **PW**, …). Click it to choose a club from the list; **Tab** also changes clubs.
- **Status box** (bottom right): **Aim** (Straight, or degrees left/right of the target
  line, in tenths under 10°; on the hole also how far wide of the cup the line passes, in
  yards, or feet in the short game), then two icons:
  - the **launch monitor**, green when Ready, amber while scanning or connecting and gray
    when disconnected, with its battery level when the device reports one. Its caption reads
    **Connect [C]** while disconnected. Click it to open Settings › Simulator; **C** connects
    and **T** disconnects.
  - on the range only, the **record** toggle, captioned **Record [F9]**: a red dot with the
    number of shots saved while recording, a gray ring when off. Click it or press **F9**. The
    hole has no record toggle and never records.

  A failed save turns the record icon amber with the unsaved count, and the error shows
  beside the icons while retrying; see [Recording errors and retries](#recording-errors-and-retries).
  A launch-monitor error shows there too.
- **Play buttons** (top left): round icon buttons in the tile style. The flag button
  (**Play hole [G]**) switches to the hole; while playing it is ringed and reads **Range [G]**,
  and a ball button (**Place ball [B]**) appears beside it. Under them the hole tiles show
  **Hole 1 · Par 4 · 400 yd**, the stroke being played, the distance **To Pin** (feet on the
  green, yards elsewhere) and **Putts**. While you place a ball, and for the rest of a hole
  with a placed ball, the heading reads **Practice · Ball placement** and those three tiles
  are hidden. A banner names the score when the ball drops.
- **Wind** (top centre): the wind speed and an arrow for the way it blows, turned with the
  camera. Click it (or press **W**) to slide out the wind controls under it:
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
- **Test shots [P] | Hit [H]** (bottom left): only shown while no launch monitor is
  connected. See [Selecting test shots](#selecting-test-shots).
- **Version** (bottom-left corner, under the test-shot button): `v` plus `ProjectVersion`
  from `Config/DefaultGame.ini` (`[/Script/EngineSettings.GeneralProjectSettings]`),
  currently **v0.0.7**. Change the version there; packaged builds use the same value.
  Release builds from CI use the tag's version instead (see [Versioning](Releases.md#versioning)).

On the range, a stopped ball stays put for 3 seconds with its result shown, then goes back to
the tee. The tiles keep the last shot until the next one.

### Playing the hole

Press **G** (or click the flag button) to play the range as a par 4: the pin is 400 yards
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
  close view until the ball is reset (automatically, or with **R**).
- When the ball drops, a banner names the score (Birdie, Par, Bogey and so on) with the
  strokes and putts, and after 5 seconds the ball goes back to the tee for a new hole.
- **B** (or the ball button), then a click on the ground, places the ball there to practise,
  for example a chip from beside the green. Placing adds no stroke, but the hole then counts
  as practice and the banner reads "Holed in N".
- **G** again goes back to the range. See
[Hitting shots on the range](#controls) for details.

### Changing clubs

Press **Tab** to select the next club, or click the club at the top of the shot tiles and
pick one from the list. When a Square Home is connected, the club is also sent to the
device. Choose the club before you hit: recorded shots are tagged with the club selected
when the shot arrives. The club resets to Driver at the start of every Play session.

### Recording shots

Real launch-monitor shots can be saved, tagged with the selected club, to replay them
in other tools (openfairway's shot injector, the FlightScope Trajectory Optimizer) and
compare with this engine. While you are signed in, recording also saves eligible range
shots to your online profile, for the club averages and shot history in Settings › Player
(see [Menu and account](#menu-and-account)). Recording off saves
nothing, locally or online. Recording is a range feature: the hole shows no record icon,
**F9** does nothing there, and hole shots are never recorded. Recording keeps its on/off
state for when you return to the range.

1. Connect the launch monitor (**C**) and wait for **Ready**: the launch-monitor icon turns
   green. The record icon is a gray ring.
2. Press **F9** or click the record icon to start recording. The dot turns red and counts
   saved shots. Press **F9** or click it again to stop. Recording is off at every Play start.
3. Select the club with **Tab** before each shot; the club selected when the shot
   arrives is the one recorded.

Each time recording is switched on, the first shot reserves a unique session folder
(created when its first record is saved):
`Documents/UOpenGolf/Saved/ShotRecordings/session_<YYYYMMDD-HHMMSS>-<unique-id>/` in your user folder
(on Windows `%USERPROFILE%\Documents\...`; on Linux the XDG documents folder, usually
`~/Documents/...`), so recordings survive rebuilds and reinstalls. Each shot is one file named
`club_<club>_shot_<n>.json`, such as `club_driver_shot_1.json` then
`club_7i_shot_2.json`: `n` counts every shot in the session, whatever the club. Club
codes match the shot library: `driver`, `3w`, `5w`, `7w`, `4i` to `9i`, `pw`, `gw`,
`sw`, `putter`.

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

The status box shows the number of unsaved records and the save error, including
while disconnected or recording is off. Restore access to the folder while Play
remains open to let the retries complete. Ending Play makes one final save attempt
and logs any remaining unsaved count and destination. **There is no recovery across
Play sessions or application restarts**; failed recordings still in memory are lost.

### Menu and account

The home page, and the pause menu that **Escape** opens during Training, share a top bar:
**UOPEN GOLF**, the launch-monitor status (click it for Settings › Simulator), an account icon
and a cog. The cog is the only way into **Settings**; the pages have no Settings button. While
signed out the account icon signs in (it opens Settings › Player); while signed in it is the
logout icon.

**Settings › Player** holds the account:

- Signed out: the sign-in choices (QR code, this computer, create account) and no profile
  options.
- Signed in: the member-since date, the **Display name** (**Save display name** saves it at
  once) and **Shot history**. Its **View** opens the club averages of the last 90 days, with
  clubs abbreviated (**DR**, **7i**, **PW**); select a club for its shots. Back returns to
  Player.
- **Time zone**: the zone for shot-history times and the member-since date. **Local** (this
  computer's zone) is the default; UTC and common zones such as **Pacific (PT)** are listed,
  with daylight saving applied. It changes at once, without Apply. CSV exports stay in UTC.

### Settings

Wind controls on **Range settings** save with the map. Changes made during Play do not
save back to the map. The settings menu saves supported preferences when you apply them. Console values can
remain between Play sessions in the same editor process.

| Setting | Default | How to change |
| --- | --- | --- |
| Club | Driver | **Tab** |
| Record shots | Off | **F9** or the record icon |
| Recording folder | `Documents/UOpenGolf/Saved/ShotRecordings` | Fixed |
| Selected test shot | First preset (low wood) | **P**, then pick one |
| 3D grass and ground cover | On | Console: `uopengolf.Grass3D 0` (off) or `1` (on); see [Range assets](RangeAssets.md) |
| Wind speed | 0 mph (calm) | In Play: click the wind indicator (up to 30 mph). Saved with the map: select **Range settings → Details → UOpenGolf → Wind → Wind Speed (mph)** |
| Wind direction | 0° (headwind) | In Play: the wind controls' **Direction**. Saved with the map: **Wind From (degrees)**: `90` from the right, `180` tailwind, `270` from the left |
| Editor flag preview | On | **Preview Wind in Editor**; viewport Realtime must be enabled |
| Green speed | Stimp 10 ft | Console: `uopengolf.GreenStimp 8` to `15`; applies from the next shot |
| Auto-reconnect after a dropped link | On, up to twelve attempts | `UOpenLaunchMonitorSubsystem::bAutoReconnect` |
| Left-handed | Off | `UOpenLaunchMonitorSubsystem::SetLeftHanded` |

Shots use the saved terrain, with default air at 75 °F and sea level. Wind is editable before Play, and
during Play from the wind controls or the console; green speed has a console control. There
is no in-game weather or altitude menu yet. The flag previews wind in the editor and uses the ball's wind at flag height
during Play, about 72% of the authored speed, which is the 10 m reading. In calm air it hangs beside the pole. For editor controls, expected movement and
optional console commands, see
[Validate wind and flag movement](RangeAssets.md#validate-wind-and-flag-movement).

The current flight calibration understates headwinds:
- **Driver:** into 30 mph the 150 mph replay climbs higher and carries 52 yd shorter.
  Published aerodynamics predict a loss of 67–87 yd.
- **Low-launch shots:** these gain carry. The low wood test shot carries 9 yd farther into 30 mph.

The wind's direction is correct. The cause, the measurements and the references are in
[Headwinds](../Plugins/OpenFairwayPhysics/Docs/Reference.md#headwinds); the fix is a recalibration that
has not been done yet.

### Selecting test shots

Click **Test shots [P]** at the bottom left, or press **P**, to open the dropdown.
Choose a preset and click **Hit selected shot [H]**. Selecting an item only updates
the preview; **H** hits and repeats the selected shot from the tee. **P** or **Close**
returns to the range. The selection lasts for the current Play session.

Ten presets include both supplied screenshots (114.5 mph low wood and 75.7 mph
mid iron), the Square Home 57.5 mph and putt captures, and a driver, iron, wedge,
chip, high flop and straight putt. The panel shows launch/spin inputs and available FlightScope
targets; synthetic samples are identified. See the
[replay catalog](../Plugins/OpenLaunchMonitor/Docs/Reference.md#replay-shots) for details.
