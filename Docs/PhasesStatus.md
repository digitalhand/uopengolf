# Phase status

## Hunter Ranch tree: Island Tree 02 as a blue-oak stand-in · 2026-11-24

### What changed

No open-licence blue oak model was found on Poly Haven or Sketchfab. The owner chose Poly Haven's Island Tree 02
(CC0) for now; the guide is [Course vegetation](CourseDesign/course_vegetation.md#3-get-a-tree-actor).

- **Source.** `FetchPolyHaven.py island_tree_02` pins the `.blend` and its 10 textures.
- **Exporter.** The jacaranda's Blender exporter is now the shared `blender/tree_export.py` (`TreeSpec`). The
  course's `Plugins/HunterRanch/Scripts/ExportIslandTree.py` builds `<art root>/IslandTree02/IslandTree02.blend`:
  - 9 m tall at scale 1
  - leaves at 0.9 times and their density at 2.6 times the source's, after scaling
  - 921,882 triangles: trunk 27,298, twigs 198,725, leaves 695,859
- **Colour.** The leaf `Tint` (1.051, 0.954, 3.583) and `Saturation` 0.387 are fitted to the foliage colour of the
  owner's reference photo (`export_rules.chromatic_tint`, `fit_saturation`). A tint alone would need about 5.75 times
  on the leaves' weak blue channel.
- **Import.** `ImportSavannaAssets.py` imports a manifest into the plugin it names (`import_destination.py`), using
  the savanna's shared card and solid parents, and applies its material settings. It now matches mesh slots by
  exact name, then the longest prefix. The old first-prefix match gave all three island tree slots the trunk
  material.
- **Assets.** The assets are in `/HunterRanch/Vegetation/Trees/IslandTree02/` (69 MB), with wind registered as
  `BroadleafTree` (150 m hold-still).
- **Blueprint.** `BuildTreeBlueprint.py` with `tree_blueprint_rules.py` made `BP_Tree_IslandTree02`.
- **Hole 1.** The owner's hand-placed `SM_Jacaranda_Hole1` (scenery only, pitched 10°) was replaced by
  `BP_Tree_IslandTree02_Hole1` at the same X/Y and yaw, upright on the ground and sunk 10 cm. The map was saved;
  its backup is `Saved/CourseDesign/Backups/20261125-064948-hole1-tree/`.

### Verified

- Linux, UE 5.8.4, Blender 5.0.1, the owner's DebugGame editor (one editor).
- Script tests passed:
  - OpenGolfSavanna scripts 88/88, including the import destination and slot tests
  - Blender rules 54/54, including the tint and saturation tests
  - OpenGolfCourse scripts 127/127, including the Blueprint rule tests
- The jacaranda re-exported through the shared exporter matches its pre-change export: same manifest, same textures
  and same FBX sizes.
- MCP checks:
  - `SM_IslandTree02` has Nanite with Preserve Area.
  - The wood helper collides on its triangles, with Nanite off; the canopy has one convex hull.
  - The leaf, twig and trunk slots each have their own instance, with savanna parents.
  - The Blueprint's parts have the profiles `NoCollision`, `BlockAll` and `OverlapAll`, the right tags and the right
    visibility.
  - Nothing changed under `/OpenGolfSavanna`.
- Look: Blender previews and editor captures of hole 1 are in `Saved/Automation/IslandTree02/`. The crown colour
  share (1.06, 1.00, 0.84) is close to the photo's (0.98, 1.03, 0.81). Its brightness relative to the ground is
  0.29, against the photo's 0.17.

### Still unverified

- Shots into the trunk and crown in Play.
- `UOpenGolf.Course.HunterRanch` and `UOpenGolf.Visual.HunterRanch` after the change.
- Shimmer and slow-motion sequences, wind at 10 and 20 mph, frame time, and the pack install check.
- The editor captures are look checks, not shimmer evidence.
- The hole 1 tree is in `Course.umap`, not a vegetation level.

## Full-width course lobby · 2026-11-24

The round lobby now fills the menu. The course preview and tabs stay above the scrolling settings, and the selected holes, par, distance and start button stay at the bottom. Course information moves under the settings on a narrow window. Back returns to the selected card and search.

- UE 5.8.4 on Linux. The Development game and both the Development and DebugGame editors built successfully.
- The rendered course browser test now exercises the same lobby from 800×600 through 2560×1080. With Hunter Ranch installed it also exercises the hole picker at 150% text and high contrast. The interactive editor was open after the build. The new automation and rendered captures are pending.

## Hunter Ranch course orientation · 2026-11-24

The course builder now maps east to +X and south to +Y. Terrain, playable surfaces, boundaries,
the lake, tees, cups and aim points share that conversion. Saved author corrections use geographic
east/north for both older maps and newly built maps.

- Rebuilt `/HunterRanch/Course/Course` from cached public sources on Linux with UE 5.8.4.
  The map contains 460 terrain squares, 228 surface regions and one lake. The previous plugin and
  corrections are backed up under `Saved/CourseDesign/Backups/HunterAxes/`.
- All 5,760,000 exported heights match the north-up source at the expected sample centres.
  Route lengths match for all 18 holes. Reading the old and rebuilt assets reproduced the same
  correction data, including hole 2 White and hole 10 Silver and Red.
- The Development editor built successfully. All 120 course-script tests passed.
  The asset-script suite ran 153 tests with four existing skips.

## Round setup tabs, course information and random wind · 2026-11-24

### What changed

The owner sent three screenshots of another simulator's match setup as the model for the round setup page. The rule
was to take the structure without copying it, and to offer only what the game supports. Owner decisions:
- tabs **Players · Match · Course**, with the holes at the bottom of Course rather than in their own tab;
- Players shows the signed-in player, or nothing;
- a sidebar with **Course information** (location, elevation, par, designer, total distance) and **Course conditions**;
- no wind choice: each round gets a random wind of 0–13 mph from any direction, fixed for the round;
- no time of day: rounds are played in daylight;
- elevation is shown only and does not change ball flight;
- the remaining "draft" names are renamed.

**Page.**
- `SRoundSetup` has Players / Match / Course tabs.
  - Players shows the signed-in player's avatar and name.
  - Match has the Gimmie switch and the 3 / 6 ft radius.
  - Course has the green speed, tee chips and the holes.
- The sidebar (`SCourseInformation`, `SCourseConditions`) shares an `SWrapBox` with the tabs, so it moves under them
  on a narrow window. Reset to defaults and Start round sit at its foot.
- The wind slider, quick picks, direction pad and time-of-day chips are gone.

**Wind and daylight.**
- `FOpenGolfRoundSetup::RollWind` picks a whole speed from 0 to 13 mph and a whole direction from 0 to 359°.
  The direction is world-fixed (clockwise from +X).
- The wind is rolled when the page opens, on Play again, and by the game mode when a travel URL carries none.
- `?Time=` is gone, and old URLs that carry it are ignored.
- Rounds set the sky's Day preset after `BeginPlay`.
- The definition's `WindMph` / `WindFromDeg` are removed.

**Course information.**
- `UGolfCourseDefinition` gains `Location`, `Designer` and `ElevationM`.
- `prepare_course_site.py` measures the elevation as the median ground inside the course boundary
  (`GroundGrid.elevation_inside`); for Hunter Ranch that is 263.0 m.
- The builder copies the elevation from the layout and the `location` / `designer` from the site recipe.
- Hunter Ranch's recipe now gives "Paso Robles, California" and "Ken Hunter Jr. and Mike McGinnis".

**Names.**
- `build_course_draft.py` → `build_course.py`, `course_layout_draft.py` → `course_layout.py` (`draft()` →
  `make_layout()`), and `test_course_layout_draft.py` → `test_course_layout.py`.
- The builder no longer adds " (draft)" to the display name. Its log marker is `COURSE_BUILD_COMPLETE`.
- The Course Designer says **Build course** and defaults to `/<Course>/Course`.
- Tests, packaging and docs use `/HunterRanch/Course/`.

### Verified

- RED before the code:
  - the plugin test did not compile without `RollWind` / `RoundWindRelativeFromDeg`;
  - the page test did not compile without the panels header;
  - the elevation tests failed with `AttributeError`.
- The Development editor build, the full `-DisableAdaptiveUnity` editor build and the Development game-target build pass
  with no warnings.
- The script suites pass: 117 tests in `Plugins/OpenGolfCourse/Scripts`, and 153 in `Assets/Scripts` with 4 existing
  skips.
- `elevation_inside` on the staged Hunter Ranch ground gives 263.0 m, matching an independent GDAL check
  (median 263.0 m, range 245.7–284.7 m inside the boundary).

### Still unverified

- The Hunter Ranch orientation repair rebuilt the course at the new package path with the round setup information.
  See the orientation entry above for checks against the rebuilt map.
- **Automation:** `Course.Play.RoundSetup`, `Menu.RoundSetup`, `Course.HunterRanch.BackNine` / `.PickedHoles` /
  `.RolledWind` / `.Round`, `Range.HolePlay`, `Visual.CourseBrowser` (narrow and wide captures), and every suite still
  pending from the entry below.
- The rendered look of the tabs and sidebar, and gamepad navigation between the tabs and the sidebar.
- **Course pack:** the Hunter Ranch pack must be re-cooked, because the definition's properties changed and its path
  moves.

## Round setup, fairway speed and Space fast-forward · 2026-11-24

### What changed

The owner asked for a reusable way to set a course round's conditions before play, a game-wide fairway speed, and a way
to fast-forward a long roll. Owner decisions:
- a full **Round setup** page opened by a course card's Play;
- the wind fixed over the course and picked against the first played hole's tee shot;
- a Space tap skips to where the ball stops, and a hold plays it fast while held;
- the speed-up uses the game clock (global time dilation).

**Round setup.**
- `OpenGolfPlay::FOpenGolfRoundSetup` (OpenGolfCourse `Play/`) holds a course round's setup:
  - tees and green stimp (8–15);
  - wind: any whole mph from 0 to 25 on a slider, with None, Calm 3 mph and Windy 15 mph as quick picks;
  - wind direction in 45° steps;
  - Gimmie on/off at 3 or 6 ft;
  - time of day: Morning, Midday or Sunset;
  - the holes, as all, front 9, back 9 or any ticked set.
- The setup travels in the URL. The game mode reads it in `StartPlay` and plays the holes in ascending order.
- The range holds the setup's wind in world space: the chosen direction turned by the first tee shot's bearing.
- The environment's preset is set after `BeginPlay`.
- `SRoundSetup` and `SChoiceChips` are new. The tee-set button moved from the course card to the page.
- Each row puts its control under the label, and the chips and hole boxes wrap, so no label is cut off on a narrow page.
  **Reset to defaults** sits beside **Start round**. Both follow owner feedback from a first look at the page.
- The final scorecard sums each nine played by the real hole numbers.
- **Play again** reuses the setup. `UGolfCourseSubsystem` remembers each course's last setup for the session.

**Fairway speed.**
- Game Defaults › **Fairway speed** offers 6, 7.4 (standard), 8, 9 and 10 ft on the stimpmeter, for every fairway in
  Training and rounds.
- At 7.4 ft the fairway is the fitted catalog value, bit for bit. Other readings scale it by the green's stimp solve,
  which now also takes grass form drag.
- The disabled "Fairway firmness" and "Time of day" placeholder rows are gone from Game Defaults.

**Gimmie.** The choices are Off, 3 ft and 6 ft everywhere. Saved 5, 8 and 10 ft become 6 ft.

**Fast-forward.**
- `UShotFastForward` sits on the player controller. While the ball is on the ground, holding Space plays at 4x and a
  tap plays at 60x until rest. It is back to 1x on rest, on cup capture, on a new shot, when the ball leaves `Rollout`,
  and with a menu or drill dialog open.
- The ball's 12-steps-per-frame cap scales with the dilation.
- The world's `MaxGlobalTimeDilation` is raised to the skip rate at runtime (the engine default is 20).
- There is no on-screen hint or caption for Space (owner request).

### Verified

- Development editor build, full `-DisableAdaptiveUnity` editor build and Development game-target build pass with no
  warnings (Linux, 5.8.4).
- The new headless tests failed to compile before the code existed (RED). Those were `Course.Play.RoundSetup`,
  `Course.Play.FastForward`, `Physics.FairwayStimp` and the `Menu.PreferenceValidation` additions. The game-side tests
  (`Menu.RoundSetup`, `Course.HunterRanch.BackNine`/`.PickedHoles`, `Range.FastForward`, the `Range.FrameRates` case and
  the `Visual.CourseBrowser` step) were written before their code, but no build ran in between.
- A Python mirror of `StepGround` and the stimp solve shows these readings:

  | Case | Reads | Note |
  | --- | --- | --- |
  | Fitted fairway | 7.397 ft | |
  | 6, 8, 9, 10 ft settings | within 0.005 m of target | |
  | Stimp-10 green | 9.97 ft | rolling friction 0.07466, matching the documented 0.0747 |

### Still unverified

- **No automation test has run yet.** The owner's DebugGame editor was open, so no second editor was started; every
  suite listed above, plus `UOpenGolf.Course` and `UOpenGolf.Range`, is pending.
- Rendered look of the Round setup page (`Visual.CourseBrowser` saves `Menu/RoundSetup-narrow.png`) and gamepad focus
  on the page.
- A real round with the setup's wind and time of day, and the Python fairway numbers in the engine.
- Fast-forward feel in a real round:
  - fast grass sway during a skip;
  - the tracer's coarser points at 60x;
  - Space after a mouse click on a HUD button.
- The DebugGame rebuild, Windows and packaged builds.

## Hunter Ranch downloadable course and Course Designer · 2026-11-24

### What changed

A Codex session began the [Course Designer](CourseDesign/course_designer.md) and a downloadable Hunter Ranch
on 2026-11-24. This entry finishes that work and makes it accurate to play. Owner decisions:
- one map per downloadable course, with Savanna kept as the startup map
- a public catalog, with the host chosen later
- keep the OpenStreetMap data and publish the derived layout under ODbL

**Layout data.**
- Fairways that the map stores as multipolygons are now read; that is 21 fairways previously missing.
- Lakes come from `natural=water`, `water=pond` and `golf=water_hazard`. Only one of the ten mapped lakes lies inside
  the course boundary; the other nine are vineyard ponds.
- Outlines are smoothed and cleaned so none cross.
- One cup per green is chosen on flat ground at least 4 m from the edge.
- Tee sets are placed by published yardage, then moved onto nearby level ground, each with a tee pad.
- Author fixes live in `Assets/Data/CourseDesign/HunterRanch/corrections.json`. `review_course_layout.py` draws every
  hole over NAIP 2022 aerial imagery and checks tees, cups and route lengths. Holes 2 (White) and 10 (Silver, Red)
  needed tee corrections. Hole 2's White tee measures about 30 yd longer than published, because no separate tee box
  is visible.

**Ground.**
- `OpenGolfTerrainTiles` triangulates each 64 m square with every outline as a constrained edge, so surface borders
  follow the outlines.
- Spacing: greens and bunkers 0.5 m, tees and fairways 1 m, rough 2 m. Before this, every surface used a 4 m grid.
- Neighbouring squares share identical edge vertices.
- The lake has a flat water surface (`M_Water`, procedural).
- `ImportElevation` now checks only the elevation grid (`ValidateElevation`); the legacy single-area budget does not
  apply to squares.

**Play.**
- A ball at rest in a lake or out of bounds costs a stroke and is replayed (`PenaltyAt`), and the HUD status line
  says why.
- The Courses card has a tee-set picker and one **Play** button.

**Downloads.**
- A new course mounts and plays without a restart.
- An update of a mounted course and an uninstall finish at the next start.
- Downloaded courses show **Update** and **Uninstall** on their card; catalog courses get a **Download** card.
- `package_course.py` publishes the ODbL layout beside each pack.
- `check_course_install.py` accepts Unreal's exit code 1 after `-TestExit`.

**Designer.**
- Paths come from the site recipe.
- **Save** removes replaced ground squares and lakes (`generated_folders.py --course`).
- **Save corrections** writes edits back to the corrections file.

### Verified

- Python: 105 course-script tests and 153 `Assets/Scripts` tests pass (4 skipped).
- C++: Development editor and game builds pass, including a full build with `-DisableAdaptiveUnity`.
- `UOpenGolf.Course` (23 tests) and `Range.HolePlay` passed on Codex's snapshot before these changes.
- After all changes: `UOpenGolf.Course` (26 tests) and `Range.HolePlay` pass.
- `Visual.CourseBrowser` passes after one fix. It expected exactly "1 installed", but course plugins are listed in the
  editor too, so it now expects the subsystem's own count.
- New or changed tests pass: `Course.Terrain.Tiles`, `Course.Terrain.Definition`, `Course.Play.Penalties`,
  `Course.HunterRanch.Round` and `Course.HunterRanch.Water`.
  - Round checks every hole's cup surface, tees, ground and opening aim, then plays 1 to 18.
  - Water chips a real ball into the lake and checks the penalty stroke and the replay.
- The draft builds headless in 38 s: 460 squares, 721 meshes, 228 surfaces, 1 lake. The content is 100 MB
  (38 MB before).
- Packaged Linux (Development): `check_course_install.py` passes absent, install (mounted and played in the same
  session), offline play, uninstall and absent, for `draft3` and again for `draft4` after the last fix. Each pack is 92 MB;
  the base package holds no course content.
- Rendered (off screen, 1792×1087, one editor): `UOpenGolf.Visual.HunterRanch` captured tee, approach and green
  views of holes 1, 9 and 12, 20 frames each, in `Saved/Automation/HunterReview/`. Surface borders follow the outlines
  smoothly and bunkers sit in the ground.
  - Still views changed by a mean of 0.13–0.15 grey levels per frame, with no pixel changing by more than 8, so there
    is no visible shimmer.
  - The green views pan 5 cm per frame and kept changes above 8 levels to 0.02–0.17 % of pixels.
  - Every view logged 8.3 ms per frame. That is the 120 fps cap, not a GPU measurement.
  - Only hole 1 shows its flag, because the capture plays hole 1.
  - No before/after pair exists: this is a new map, and Codex's 4 m draft is kept in `Saved/CourseDesign/Backups/`.

### Still unverified

- Owner visual review. The draft has no trees, uses the coastal surfaces, and the coastal environment's sea shows on
  the horizon, which is wrong for inland Paso Robles.
- Windows packs, S3/CloudFront hosting, and the full automation suite after these changes.
- DebugGame builds; the owner rebuilds those.
- Tee and cup positions are estimates. Green contours come from 1 m lidar, with no hand shaping.
- Pack size: 92 MB may need coarser rough beyond the boundary, or Nanite.

### Follow-up: tee pads, performance and review fixes · 2026-11-24

The owner noticed that hole 2's White tee had no tee turf. A cloud review of the branch ran at the same time.

**Fixed.**
- Tee pads were built at the tees estimated from the map, before the author's corrections moved them. Hole 2 White
  sat in rough, and hole 10 Silver and Red sat on fairway. Site preparation now builds each pad at the corrected tee.
  The layout review and `Course.HunterRanch.Round` both check that every tee of every set is on a tee pad. On the old
  layout the review flags exactly those three tees; after preparing again it is clean.
- `IsRoundComplete` turned true when the last ball dropped, 5 s before that hole reached the scorecard. Opening the
  menu in that window cached a final card without the last hole. The round now completes once its last hole is
  recorded.
- The Courses card totalled yards by tee index, but the round finds each hole's tee by set name.
  `FGolfCourseHole::FindTee` and `UGolfCourseDefinition::TeeSetYards` now serve both (`Course.Play.TeeSets`).
- A rebuilt lake left its old `Water_<GUID>` folder behind. **Save** now cleans lakes as well as ground squares
  (`generated_folders.py --course`, renamed from `--tiles`).
- The update panel kept showing an earlier helper failure after a new attempt failed. A new attempt now clears it.
  This is part of the Linux update work, and no test covers it.

**Performance.**
- Rough is built at 2 m within the play area (the boundary grown by 25 m) and at 4 m beyond it, where 54 of the
  ground's 76 MB was rough.
- Ground meshes keep the elevation grid's normals instead of recomputing them per mesh, which could not see across
  square edges, and skip lightmap UVs, which dynamic lighting never uses.
- Not changed: Nanite for the ground, and the 0.5 m spacing along every square edge. Nanite may open cracks at square
  edges at a distance, and coarser edges need a change to how neighbouring squares agree on edge points. Both need a
  rendered comparison first.

**Verified.**
- 115 course-script tests pass, as do the Development editor and game builds, including `-DisableAdaptiveUnity`.
- The draft was rebuilt headless in 26 s, after backing up the old one to
  `Saved/CourseDesign/Backups/Draft-20261124-164752`. It has 460 squares and 722 meshes.
- Sizes: rough fell from 54.0 to 37.6 MB, the ground squares from 76 to 60 MB, and the whole draft from 100 to 84 MB.
- Unreal reports the ground meshes' build settings: normals and tangents are not recomputed, there are no lightmap
  UVs, and there is one UV channel.
- `UOpenGolf.Course` passes 28 tests, including the new `Course.Play.TeeSets` and the tee-pad check in
  `Course.HunterRanch.Round`. `Range.HolePlay` passes.
- Rendered, off screen, with one editor: all 180 `Visual.HunterRanch` frames match the earlier review in
  `Saved/Automation/HunterReview/`, to a mean of 0.7 grey levels or less below the horizon. The coarser rough
  beyond the play area does not show in these views. Holes 2 and 10, whose tees moved, are not among the captured
  holes.

**Still unverified.**
- Owner review of holes 2 and 10 in Play.
- A new pack. The 92 MB `draft4` pack predates these changes; the next pack needs a new version.

### Follow-up: stall at each hole change · 2026-11-24

The owner saw lag after holing out, while the camera moved to the next hole. The DebugGame editor's log showed that
the hole map (the minimap's picture) was rebuilt on the game thread at every hole change. On Hunter Ranch this took
0.7–1.5 s, and on Savanna about 0.3 s.

Most of that time went on the distance-from-play field: 558–1330 ms on Hunter Ranch. Every 3 m sample measured its
distance to all 228 surfaces, and walked each outline up to three times.

**Changed.**
- `FOpenGolfPlayDistance` measures distance from play only up to 66 m, the farthest the dimming can tell apart.
- It measures only from the surfaces within reach of the hole's frame.
- One pass over each outline now answers both questions, inside or not and how far (`DistanceFromOutline`). This also
  speeds up grass scatter.
- The capture is read back from the GPU without waiting for it (`FHoleMapReadback`).
- A worker thread builds the distance field, dims, shades and contours the picture, and builds its mips.
- The game thread then publishes the picture, frame and heights together, so the minimap never mixes two holes.
- A result made stale by a newer hole change is dropped.

## Player badge, short club label, avatars · 2026-11-24

### What changed

**Short club label.** The club button shows the short label (DR, 3W, 7i, PW, PT) instead of the full name, in the
course tiles' number font (`HudStyle::ValueFont`, Bold 18 with their shadow). This covers both a club picked by hand
and the club Auto club picks. Screen readers still hear the full name. The button is as wide as the widest label, so
it is much narrower than the "Pitching Wedge" width it had. That may resolve the open regression below, where Place
ball and Record became unreachable at 150% HUD and text; this has not been rerun.

**Player badge.** In a course round, a signed-in player's avatar (36 px circle) and display name sit right of the club
button, in the same toolbar tile and font. A name wider than 220 px ends in an ellipsis. Nothing shows while signed
out or on the range.

**Avatar.** The game shows, in order:

1. the built-in avatar the player picked for this account;
2. otherwise the account picture that Auth0's `/userinfo` names (`picture` claim);
3. otherwise the default avatar (Golfer).

The pick is a local preference (`AvatarId`, with `AvatarAccount`, the account key it was picked for). Another account
signed in on the same computer ignores it.

**Account picture.** OpenGolfOnline keeps the `picture` address and downloads it (`OnlinePicture`):

- https only;
- no credentials (no Authorization header, no cookies);
- 15 s timeout and 1 MiB limit;
- PNG or JPEG only, at most 2048 px a side, centre-cropped to 128 px.

The download is not repeated while the address is unchanged. A failed one is retried at the next token refresh. Sign
out, a change of account and a changed address clear it. The picture is not cached on disk, so an offline start shows
the default avatar.

**Settings › Player › PROFILE.** The empty right half of the profile section now holds the avatar picker: the account
picture tile (when there is one), then 24 built-in avatars. The shown one has an accent ring, with its name in the
caption. A pick is part of Settings' draft, so the footer shows **Unsaved changes** and Apply/Discard work as for other
preferences. The account panel takes the picker through a new `ProfileAside` factory, so the plugin stays independent
of the game.

**Avatars.** 24 Fluent Emoji 3D images (MIT), pinned by `FetchFluentEmoji.py` and imported to
`/OpenGolfSavanna/UI/Avatars` as UI textures (no mips, uncompressed, sRGB). They are always cooked. See
[Player avatars](AssetLicenseRoadmap.md#player-avatars--2026-11-24).

### Verified

On Linux with UE 5.8.4:

- The Development editor build passed, including a full `-DisableAdaptiveUnity` build, with no warnings.
- `python3 -m unittest test_fetch_fluent_emoji test_fetch_polyhaven` (in `Plugins/OpenGolfSavanna/Scripts`): 11 passed.
  One of them checks that the fetch list and `AvatarCatalog.cpp` name the same avatars in the same order.
- `FetchFluentEmoji.py` downloaded and verified all 24 PNGs (256 × 256 RGBA) and the licence.
- All 24 textures were imported in the running editor through its MCP texture tools and saved. Their UI settings were
  read back. They are stored in LFS.

### Still unverified

- No automation test has run. The new or changed tests need the rebuilt DebugGame editor:
  - `UOpenGolf.Profile.Avatars`
  - `UOpenGolf.Hud.PlayerBadge`
  - `OpenGolf.Online.Picture`
  - `UOpenGolf.Menu.PreferenceValidation`
  - `UOpenGolf.Menu.PlayerProfile`
  - `Visual.RangeHudControls` (expects `8i`, the value font size and the badge's toolbar slot)
- No rendered capture yet of the badge, of the picker's layout, or of the round clip of photos and emoji.
- `ImportAvatars.py`, the rerunnable import script, has not been run. The editor's Slate inspector saw no windows, so
  the import used the editor's MCP texture tools with the same settings.
- A real Auth0 account picture download (gravatar redirect included), and the owner's look review.
- The DebugGame build, Windows and packaged builds.

## Course lie penalties, auto club, minimap lie header · 2026-11-23

### What changed

**Lie penalties.** In a course round, shots from the rough and from sand lose part of their measured ball speed and
spin. The amount depends on the kind of club: woods, irons or wedges.

- A greenside bunker, within 30 yd of the cup, plays as an explosion shot at −40% speed and −30% spin.
- The putter is never penalised.
- Launch angle and direction stay as measured.
- Recorders keep the raw shot.
- Training never applies penalties.

The rules are a reusable `UOpenGolfLieRules` data asset in OpenGolfCourse. A course definition can point at one; an
empty reference plays the standard. The standard values are provisional (see
[Lie rules](../Plugins/OpenGolfCourse/Docs/Reference.md#lie-rules)).

**Auto club.** Course rounds pick the club on the tee and when the ball turns to the pin for the next shot, from the
lie, its penalty and the distance. Carries are PGA Tour averages from TrackMan. Settings › Player › Auto club switches
this off; it replaces the disabled "Club suggestion" row.

**Club button.** The toolbar's club button now names the selected club beside its icon, for example "7 Iron". It is as
wide as the longest name ("Pitching Wedge"), so the toolbar doesn't shift when the club changes.
`Visual.RangeHudControls` now expects:
- the club's name on the closed button;
- a club button wider than the icon buttons but the same height;
- a button size that stays the same when the club changes.

**Club drawer fix.** The owner reported that some clubs in the drawer (7i, 6i) needed two clicks.

- **Cause.** A press focuses its row, and the drawer's scroll box centres a focused row (`InstantScroll`, kept so
  keyboard and gamepad focus stays in view). The row slid away before the release.
- **Fix.** Rows now pick on the press (`EButtonClickMethod::MouseDown`; touch uses `PreciseTap`).
- **Auto club.** It no longer replaces a club picked in the 2 s while a stopped shot is shown (`ClubAtRest`).
- **Tests.** The new rendered `Visual.ClubDrawerClick` presses and releases frames apart, as a hand does.
  `Course.LiePenalty` now checks that a pick is kept and that an untouched club is replaced.
- **RED.** Before the fix, both failed (`Saved/Automation/20261123-233602-club-red`).
- **GREEN.** After it, both passed (`Saved/Automation/20261123-233839-club-green`).

**Club name regression.** `Visual.RangeHudControls` failed in the same GREEN run. With a 150% HUD and 150% text in a
1024 × 640 window, the wider club button leaves Place ball and Record unreachable. This is still open.

**Minimap header.** The header no longer shows "Hole 1 · Par 4" or "Training". It shows the lie, the speed cut beside
it and the spin cut on the right, for example "Rough −10%" with "Spin −20%". The cuts are blank where a lie costs
nothing.

### Verified

On Linux with UE 5.8.4:

- The Development editor build passed, including a full `-DisableAdaptiveUnity` build, with no warnings.
- These headless runs passed in one editor process, with no other editor open:

  | Filter | Tests | Report |
  | --- | --- | --- |
  | `UOpenGolf.Course` | 22, including the new `Course.Play.LieRules`, `Course.Clubs` and `Course.LiePenalty` | `Saved/Automation/20261123-230951-6ZlGdk` |
  | `UOpenGolf.Menu` | 9 | `Saved/Automation/20261123-231038-43iG9a` |
  | `UOpenGolf.Minimap` | 11 | `Saved/Automation/20261123-231054-kEoGPV` |
  | `UOpenGolf.Range.HolePlay` | 1 | `Saved/Automation/20261123-231109-bFfTGE` |

- `Course.LiePenalty` hit the same 7 iron from the tee and from the rough. The rough shot launched at 0.92 of the
  tee shot's speed and 0.75 of its spin; the log reads "Lie …Rough, 7 Iron: ball speed -8%, spin -25%".
- An offscreen Vulkan run at 1792 × 1087 passed `Minimap.RenderedClick` and `Course.LiePenalty`. The only warnings were
  the editor's blocked telemetry upload. See `Saved/Automation/20261123-231421-rendered-minimap` and
  `Saved/Screenshots/Minimap-course-rough.png`, which shows "Rough −10%" and "Spin −20%" with the Driver selected.
- `Minimap.RenderedClick` now also checks the header in a course round, with the ball placed in the rough.

### Still unverified

- The DebugGame build.
- Windows.
- Packaged builds.
- The owner's review of the header's look and of the penalty and carry values.
- A full round played with Auto club and a real launch monitor.
- The club name on the club button. Its Development build passed, but `Visual.RangeHudControls` has not run, because
  the owner's editor was open again.

## Open-ended Distance Practice, lie-only minimap header, two-line hole heading · 2026-11-23

Distance Practice has no shot limit. The distance can change whenever the ball is at rest, and each
change starts a new hit count. Only the Distance Ladder offers 3 or 5 tries. The minimap header no
longer says In flight or Rolling: it keeps the lie the shot was hit from while the ball is in the air,
then names the surface. In a course round the top-right heading is two tiles tall, with "Hole 1" and
"Par 4 · 400 yd" on separate Bold 18 lines.

On Linux with UE 5.8.4, the Development editor build passed, including a full `-DisableAdaptiveUnity`
build. In one guarded editor with no other editor running:
- Six headless tests passed: `Training.Scoring` (3), `Range.ShotDisplay`, `Minimap.Labels` and
  `Range.TrainingGames`. See `Saved/Automation/20261123-223005-Headless-AD82Fl`.
- Four offscreen Vulkan tests at 1792 × 1087 passed: `Visual.RangeHudControls`,
  `Minimap.RenderedClick`, `Visual.DistanceLadderReplay` and `Visual.TrainingTarget`. Two were marked
  with warnings, which came only from the editor's blocked telemetry upload. See
  `Saved/Automation/20261123-223049-Rendered-2MskVx` and
  `Saved/Screenshots/RangeHud/20261123-223246-course-heading.png`.

Still unverified:
- The DebugGame build.
- Windows.
- Packaged builds.
- The owner's look review.
- A rendered shot from the tee box. The rendered in-flight check started from a rough lie.

## Training game picker and winning radius · 2026-11-23

The Training picker shows both games side by side, with a 3 ft, 6 ft, or 10 ft radius and a
choice of 3 or 5 shots or tries. Both games use the same line-touch rule and show a single ring.

On Linux with Unreal Engine 5.8.4, the Development and DebugGame editor builds passed.
Six Development tests passed in offscreen Play at 1045 × 801, including the three scoring tests,
training gameplay, the HUD controls, and the 35 yd replay. The replay stopped 91.5782 cm from
the cup and advanced to 45 yd after the HUD called it on the line. See
`Saved/Automation/TrainingPicker-Development-5ymrBw/index.json` and the captures
`Saved/Screenshots/RangeHud/training-game-picker.png` and
`Saved/Screenshots/RangeHud/20261123-215608-distance-ladder-10ft.png`.

The DebugGame target test passed separately in Vulkan SM6 at 1792 × 1087 offscreen with one
editor and six stationary frames per view with the ring shown and hidden. Compared with the
earlier two-ring Development capture, the 6 ft far-arc continuity stayed at 97.7%, 95.3%, and
100% at 15, 50, and 150 yd, with zero toggles. Ring-box sparkle went from 0.001% to 0.004% at
15 yd and stayed at 0.000% at 50 and 150 yd. A close view and a 3 cm camera move show one
labelled ring. Continuous camera motion was not measured. See
`Saved/Automation/TrainingPicker-DebugGame-gbKpeK/ring-stability.txt` and the frames in
`Saved/Screenshots/TrainingGames`.

## Distance Ladder line touches · 2026-11-23

The ladder now counts a stopped ball that touches the white 3 ft stripe. On Linux with UE 5.8.4,
both Development and DebugGame editor builds passed. A standalone check of the compiled scoring
function passed at 35 and 150 yd, including exact contact and a 1 mm gap.
All five tests passed in one DebugGame editor with Vulkan rendering. The 45 mph replay at 35 yd
stopped 91.578205834662398 cm from the cup, 1.38 mm past the old cutoff. The HUD showed “on the 3 ft
line” and advanced to 45 yd. The report is in `Saved/Automation/LadderLine-whH67k/index.json` and the
1606 × 906 Play capture is in `Saved/Screenshots/TrainingGames/ladder-35yd-rest.png`.
The original session logged distances rounded to tenths of a yard.

## Vulkan Nanite base-pass assertion · 2026-11-23

One of about 11 Linux Play in Standalone Game launches (UE 5.8.4, DebugGame, Vulkan SM6 with
`VK_EXT_descriptor_buffer` bindless, RTX 3060) failed `ByteOffset + NumBytes <= PackedUniformBuffers.Num()`
(`VulkanResources.h:1105`) on frame 3, in the main view's `Nanite::BasePass`. The run just before it, with
the same map, content and config, played 741 frames. The project has no shader code on this path.

The engine source points to a race. With the default `r.RDG.ParallelExecute=2`, the Nanite shading pass
records in `FRDGAsyncTask` tasks that are not awaited when the graph returns. In the first frames, PSO
precaching triggers static mesh updates that unregister and re-register Nanite shading bins. That mutates
the shared pipeline-group sets those tasks read, without `FRDGBuilder::WaitForAsyncExecuteTask()`. This is
inferred from the source; the interleaving itself was not captured. `Config/Linux/LinuxEngine.ini` sets
`r.RDG.ParallelExecute=1`, which keeps passes parallel but awaits them. It replaces an untested
`r.Nanite.ParallelBasePassBuild=0`, which still leaves the pass as an un-awaited async task.

After the change, 30 of 30 offscreen DebugGame Standalone launches of the public map ran 25 s past their
first frame with no assertion. Each log showed `r.RDG.ParallelExecute = "1"`, bindless Vulkan SM6, the same
first-frame PSO creation (100 by frame 3) and a hole-map capture. At the earlier rate, 30 clean runs would
occur by chance about 6% of the time. That rate rests on one failure, and no offscreen baseline was run.
No other editor ran during the batch. Windowed runs, packaged builds, Windows and the CPU cost of awaiting
the tasks are unmeasured.

## Training games · 2026-11-23

Linux UE 5.8.4 Development and DebugGame editor builds passed. The first rendered run passed 18 tests, including Distance Practice, range HUD controls, automatic tee-up, cup placement, and the minimap. The 26 yd tee capture shows both colored zones, the moved flag and matching minimap distance. The run is in `Saved/Automation/TrainingGamesReview/index.json` and the capture is in `Saved/Screenshots/RangeHud/20261123-175631-distance-practice.png`. The owner’s editor opened during the run, so it provides no controlled timing measurement. The expanded dialog, reset and close-target checks were built after this run. Their results are pending.

## Training ring shimmer, game prompt, HUD tooltips and 5-yard steps · 2026-11-23

The owner saw the 6 ft ring shimmer with the camera still, not while shots were played. From the tee
camera (2.2 m up, 70° FOV) the 5 cm edges were 0.3–0.8 px tall, and as `M_ShotTrail` translucency TSR's
thin-geometry and anti-flicker handling skip them, so each jittered frame drew a different dash pattern.
The edges now use `M_TargetRingLine` ([Range assets](RangeAssets.md)), which keeps them at least 2 px
wide on screen, and `UTargetRingMesh` pads the mesh bounds so the grown edges are not occlusion-culled.

Measured with `Assets/Scripts/ring_stability.py` on `UOpenGolf.Visual.TrainingTarget` (Linux, UE 5.8.4
Development editor, Vulkan SM6, 1792 × 1087 offscreen, one editor; six stationary frames per view, rings
shown and hidden). Toggles are the share of far-arc columns that switch between frames; sparkle counts
ring-box pixels changing by more than 16/255.

| Tee view | Far-arc continuity before → after | Toggles before → after | Ring-box sparkle before → after (rings hidden) |
|---|---|---|---|
| Chip (about 10 yd) | 19.4% → 97.7% | 25.1% → 0.0% | 2.595% → 0.008% (0.025%) |
| 15 yd | 38.7% → 97.7% | 36.2% → 0.0% | 2.613% → 0.001% (0.000%) |
| 50 yd | 24.9% → 95.3% | 29.2% → 0.0% | 2.559% → 0.000% (0.000%) |
| 150 yd | 0.0% → 100.0% | 0.0% → 0.0% | 0.144% → 0.000% (0.012%) |

Before the bounds fix, the grown 150 yd target blinked as a whole (60% toggles, 4.27% sparkle). Frames are
in `Saved/Automation/TrainingRings/{Before,After}-frames`. By eye the near rings are continuous white
ellipses, the 150 yd target is a steady 2 px bar, and the overhead view looks as before. In the test's 3 cm
pan step the lower-middle box changes as much before as after (18.05% and 18.13% of pixels), but that box
holds only the ring's near edge, so it is no ring measurement.

Picking the other training game while one runs opens the drill dialog with a caption ("Exit Drill to start
Distance Ladder, 3 tries."): Exit Drill ends the running game and starts the picked one; Restart Drill or Esc keeps
the running game. The Distance Practice slider moves in 5-yard steps. HUD toolbar: the training games button has no
tooltip, the club button's reads "Change Club" and the record button's "Record Shots" (the recording status stays in
its accessible name; failed saves are logged and turn the icon amber). While placing a ball the pointer is the HUD's
ball icon (a software pointer for the crosshair cursor). In the Development editor (one editor, 1792 × 1087) these
passed: `UOpenGolf.Range.TrainingGames` (prompt, pending choice, ring material, UV rows, padded bounds),
`UOpenGolf.Visual.RangeHudControls` (prompt from the HUD, tooltip texts and absence, ball pointer, 5-yard step),
`UOpenGolf.Training.Scoring`, `UOpenGolf.Visual.Menu` and the five shot-recording tests (`UOpenGolf.Menu.GimmieScoring`
passed earlier with the ring change), and the full `-DisableAdaptiveUnity` build passed. The tests were written before each fix
but could only run after it, so there is no recorded red run. The ball pointer is checked by its mapping, not in a
capture. Pending: the owner's DebugGame rebuild and
look review, a measured slow pan from the tee, packaged rendering and Windows. The flat labels remain
sub-pixel from the tee.

## Training target rings, try counter and Distance Ladder · 2026-11-23

The first Distance Practice zones measured 3 yd and 10 yd, not feet, and sat 0.6 cm above the ball-contact ground.
Cut-turf blades are 15 mm (fairway), 20 mm (collar) and up to 55 mm (rough). Within the grass draw distance they hid
the near half of the target, which showed only as specks between blades (owner screenshot of the 25 yd target from the
tee). The target is now white 3 ft and 6 ft rings with flat "3 ft" and "6 ft" labels, 6 cm above the ground. Each ring
edge widens with distance from the tee (5 cm, or 0.15% of the distance, whichever is larger).

The player picks 3 or 5 tries with the game. Distance Practice plays one set at the chosen yardage. Distance Ladder runs
15, 25 … 145, 150 yd: one ball inside 3 ft moves on, and missing every try moves back one target (not below 15 yd).
Balls are scored where they stop. While a game runs, launch-monitor shots wait for the ball to stop and are refused once
the game is finished.

Linux UE 5.8.4: the Development editor build passed, including a full unity build with `-DisableAdaptiveUnity`.
`UOpenGolf.Training.Scoring` (rings, target set, ladder; 3 tests) passed headless with no other editor running
(`Saved/Automation/20261123-191411-6pOHzy`). The DebugGame build, `UOpenGolf.Range.TrainingGames`, the range HUD controls test and
`UOpenGolf.Visual.TrainingTarget` (now with 15 yd and 150 yd tee views) have not run yet. Before-change captures from
the same test were not taken; the owner screenshot is the only before evidence. Rendered ring and label checks
(visibility over the grass, label orientation, stationary flicker, pan), packaged rendering of the engine text material
and Windows are pending.

## Rough mask behind the green · 2026-11-23

Brown zig-zag steps along the back of the green's fringe came from a stale savanna rough mask. It was baked before the
par-4 hole and left desert under the rough beside the back of the green and the greenside bunker. `RefreshRoughMask.py`
re-baked it from the current outlines in the owner's DebugGame editor (Linux, UE 5.8.4, Vulkan SM6, one editor), and
`BuildParFourHole.py` now re-bakes it too. Details are in
[Turf workflow](TurfWorkflow.md#rough-mask-follows-the-par-four-hole--2026-11-23).

Editor viewport captures show the steps gone. In a slow pan at the back of the green, lower-40% pixels whose second
difference exceeds 16/255 fell from 35.5% to 25.9%. The stationary comparison is inconclusive: editor captures are not a
controlled measurement. A rendered Play run, frame timing and owner acceptance are pending. The 70 OpenGolfSavanna
script tests pass.

## Fringe only around the green · 2026-11-23

The fairway's first-cut band was removed. The fairway now meets the 50 mm rough at its mowing line, and only the green
keeps a fringe, coloured like the old first cut. Details and placement counts are in
[Turf workflow](TurfWorkflow.md#fringe-only-around-the-green--2026-11-23). Terrain, physics and outlines are unchanged.

Rendered run: Linux, UE 5.8.4 DebugGame editor (the owner's, in Play), Vulkan SM6, one editor,
`UOpenGolf.Visual.CoastalTerrain` passed, 1792 × 1087 (`Saved/Automation/CoastalReview/NoFirstCut-20261123-215012`).
The before run is `Cloud-FullAfter-20261121-175057`. It came from a separate offscreen Development editor at
1787 × 1081 and has the same turf apart from the first cut and the collar colour. `UOpenGolf.Range.CoastalTerrain`
passed.

| Stationary view | Lower 40% changed, before → after | Worst 5% band changed |
| --- | --- | --- |
| Tee, grass on | 0.002% → 0.001% | 5.137% → 5.707% |
| Tee, grass off | 0.001% → 0.001% | 4.969% → 5.383% |
| Grass edge, on | 0.003% → 0.001% | 5.090% → 5.502% |
| Grass edge, off | 0.002% → 0.001% | 5.017% → 5.315% |
| Close grass, on | 0.024% → 0.037% | 4.767% → 4.644% |
| Close grass, off | 0.002% → 0.008% | 3.876% → 4.107% |

View 9 pan, lower 40%:
- Pixels whose second difference exceeds 16/255 rose from 3.6% to 9.7%, and the mean second difference from 5.9 to
  7.7/255 (`Saved/Automation/CutTurf/NoFirstCut/pan_flicker.py`).
- The rise is in the rough. The rough now fills the old first-cut band in the view's foreground, so it covers 32% of
  the lower frame, up from 15%.
- The fairway-only third went from 0.9% to 0.7%.

Limits:
- No Play view shows the putting green, so the fringe was checked only in editor-viewport captures, and its
  stationary shimmer is unverified.
- The before and after runs used different editor processes.
- Frame timing was not measured.
- Owner acceptance is pending.

## Course weather · 2026-11-21

The saved range uses the reusable OpenGolfEnvironment actor. The plugin material draws a continuous cloud field from Unreal’s shape and erosion volumes. Coverage zero clears the layer. Speed zero freezes cloud drift. The morning and sunset sun angles and brightness were adjusted for the range’s fixed exposure.

Rendered checks use the saved UOpenGolf map in Play on Linux with Unreal Engine 5.8.4, Vulkan SM6 and an RTX 3060. Each stationary view has 20 frames at 1792 × 1087. The previous material and its replacement use the same tee, grass-edge and close-grass cameras, grass switches and anti-aliasing settings. Both runs use one editor.

The capture folders under `Saved/Automation/CoastalReview` are `Environment-CumulusDay-20261121-081006` (previous clouds) and `Environment-FairWeatherDay-20261121-082413` (revised clouds). The latter contains `Cumulus-comparison.jpg`. Measure the captures with `Assets/Scripts/grass_stability.py <folder> --profile coastal`. Timing between screenshot requests includes capture overhead.

| Stationary view | Lower 40% changed, previous → revised | Worst 5% band changed, previous → revised |
| --- | --- | --- |
| Tee, grass on | 0.001% → 0.002% | 6.338% → 7.136% |
| Tee, grass off | 0.001% → 0.001% | 5.917% → 7.012% |
| Grass edge, on | 0.002% → 0.002% | 6.522% → 6.693% |
| Grass edge, off | 0.002% → 0.002% | 6.452% → 6.489% |
| Close grass, on | 0.027% → 0.027% | 6.009% → 7.353% |
| Close grass, off | 0.003% → 0.003% | 5.421% → 6.337% |

Values are mean percentages of pixels changing by more than 3/255 between successive captures. The fixed canopy boxes include visible sky. Pixels changing by more than 16/255 increased from 0.109% to 0.151% at the tee, 0.082% to 0.085% at the green, 0.108% to 0.184% at the edge and 0.151% to 0.194% at the bunker. These measurements include both intended cloud motion and temporal noise. The slow-moving camera sequence completed and its start, middle and end frames showed continuous cloud banks.

Median elapsed time per editor frame between screenshot requests went from 27.07 to 26.57 ms at the tee, 28.69 to 28.54 ms at the grass edge and 30.83 to 31.08 ms close to the grass, with grass on. Standalone packaged frame rate remains unmeasured.

## Cloud shimmer · 2026-11-21

The clouds above shimmered even with cloud speed zero. `AOpenGolfEnvironment` left the cloud component at Unreal's default of 96 view samples per 15 km, so each ray-march step was about 156 m. In `M_Clouds`, one such step is nearly opaque. Unreal jitters each frame's first sample with blue noise, so a pixel alternated between lit edge, shadowed interior and sky. The actor now sets **View Sample Count Scale** to 4 (about 39 m per step). The material and map are unchanged. `UOpenGolf.Environment.Actor` checks the setting and failed before the change.

Rendered runs: Linux, UE 5.8.4 Development editor, Vulkan SM6, RTX 3060, one offscreen editor at a time (`-RenderOffscreen -ResX=1792 -ResY=1087`, 1787 × 1081 captures), same saved map and anti-aliasing settings. Cloud GPU time comes from `-CoastalCsvProfile -csvGpuStats`. It is the median `GPU/VolumetricCloud` over each view's capture frames. Reports are under `Saved/Automation/20261121-09*-cloud-*`, and captures are in `Saved/Automation/CoastalReview/Cloud-*-20261121-17*`.

Cloud review (`-EnvironmentCloudReview`, speed 0, so any change is noise). Columns give the share of pixels in the top 40% that change by more than 3/255 between captures. Clear sky measured 0.034–0.035% in every run.

| View | Scale 1 (before) | Scale 4 | Scale 8 | Cloud GPU ms, 1 → 4 → 8 |
| --- | --- | --- | --- | --- |
| Tee, day | 0.924% | 0.073% | 0.076% | 0.28 → 0.97 → 1.88 |
| Across range | 1.293% | 0.000% | 0.000% | 0.33 → 1.14 → 2.19 |
| Overhead | 1.439% | 0.002% | 0.000% | 0.09 → 0.25 → 0.45 |
| Morning | 1.131% | 0.121% | 0.128% | 0.35 → 1.21 → 2.33 |
| Sunset | 1.004% | 0.123% | 0.114% | 0.42 → 1.46 → 2.82 |

Scale 8 doubles the cost without steadier clouds. At scale 4, all remaining change in the tee, morning and sunset views is in tree canopy at the bottom of the band. A repeat of scale 1 matched its first run to within 0.2 percentage points. A run with the shipped default (no override) reported scale 4.00 and matched the scale 4 row.

Full `UOpenGolf.Visual.CoastalTerrain` (default cloud drift), scale 1 → 4:

| Stationary view | Lower 40% changed | Worst 5% band changed |
| --- | --- | --- |
| Tee, grass on | 0.002% → 0.002% | 6.988% → 5.137% |
| Tee, grass off | 0.001% → 0.001% | 7.056% → 4.969% |
| Grass edge, on | 0.002% → 0.003% | 5.776% → 5.090% |
| Grass edge, off | 0.002% → 0.002% | 5.833% → 5.017% |
| Close grass, on | 0.023% → 0.024% | 6.806% → 4.767% |
| Close grass, off | 0.002% → 0.002% | 5.963% → 3.876% |

Canopy-box sparkle (> 16/255, boxes include sky) fell from 0.153% to 0.050% at the tee, 0.077% to 0.059% at the green, 0.120% to 0.057% at the edge and 0.184% to 0.095% at the bunker. The tee's top 40% went from 1.089% to 0.080%. The cloud pass went from 0.22–0.28 ms to 0.76–0.98 ms in these views. The slow-moving camera (a 5 cm slide per frame) showed continuous cloud banks with no smearing in the start, middle and end frames. Camera rotation, the DebugGame editor, packaged rendering and Windows remain unchecked.

At scale 1, part of the clouds' fine grain was this sampling noise. At scale 4 the cloud bodies look softer; the shapes are unchanged. Sharper billows would need a material change, which needs its own shimmer check.
