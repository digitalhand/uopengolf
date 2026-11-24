# Open Golf Environment

Reusable sun, sky, clouds and morning mist for Unreal Engine courses. The runtime module depends only on Unreal Engine. It does not read golf wind or change ball physics.

Enable the plugin and place one **Open Golf Environment** actor in the course map. Assign a shared **Open Golf Environment Profile** data asset. The bundled `DA_DefaultDaylight` starts at Day with gently drifting clouds and automatic time off. Enable **Override Starting Settings** for a map-specific starting time, cloud coverage, speed or daylight duration. Rotate the sun path through the profile's **Sun Orientation Degrees**.

Profiles contain curves over daylight 0 (Morning), 0.5 (Day), and 1 (Sunset). Morning height fog fades toward midday. The profile defines an artistic sun path. Automatic daylight takes 60 real minutes by default and holds at sunset. Cloud motion continues with a fixed daylight setting; speed zero freezes clouds. Game pause freezes both. Editor animation requires **Preview Animation in Editor** and viewport Realtime. Preview never saves its elapsed time as the starting time.

The game exposes these controls under **Settings → Environment**. Changes apply immediately for the current visit, including course rounds. **Reset to course defaults** restores the map/profile settings. These controls are separate from the other settings' Apply and Discard buttons. Leaving or reloading the course restores its authored settings.

`GetSettings`, `ApplySettings`, `SelectPreset` and `ResetToCourseDefaults` are available to C++ and Blueprints. All use the same evaluator. `OnSettingsChanged` reports manual changes. The game uses it to refresh the minimap. Slider changes notify when dragging ends.

Run `Scripts/BuildEnvironmentAssets.py` inside Unreal to create missing cloud material and default profile assets. The cloud material samples two built-in Unreal volume textures. Broad patches of moisture set the cloud layout and height. Smaller billows break up each bank and thin out its edges. Run the builder with `--refresh-clouds` to back up and rebuild only the cloud material from `Scripts/cumulus_density.hlsl`. A regular run preserves existing material graphs and repairs the required volumetric cloud usage flag.

For the existing range, run `Scripts/MigrateRangeEnvironment.py` inside Unreal with the UOpenGolf map open and Play stopped. It checks for conflicting lighting, backs up the disk map and current in-memory edits, and replaces only the range's known legacy lighting. Running it again does nothing when migration is complete. Other courses should place the actor directly.

Cloud ground shadows and volumetric fog are off. Height fog provides the morning mist. Real-time skylight capture follows the sun and sky atmosphere. Clouds render in the main view and stay out of skylight captures to avoid a UE 5.8 render error during cloud shader loading. Cloud speed is independent of gameplay wind. Preserve the project's anti-aliasing settings when evaluating shimmer.

The actor traces clouds at four times Unreal's default view sample count (**Clouds → Cloud Tracing → View Sample Count Scale**). At the engine default of 96 samples per 15 km, one step is nearly opaque in the cumulus material, and the per-frame jittered start makes the clouds shimmer. Eight costs twice as much without steadier clouds. After a cloud material or tracing change, compare `UOpenGolf.Visual.CoastalTerrain` runs with `-EnvironmentCloudReview`; add `-EnvironmentCloudSampleScale=<n>` for an A/B and `-CoastalCsvProfile -csvGpuStats` for per-view cloud GPU time.

Automation: `UOpenGolf.Environment` checks settings and the standalone actor. Rendered acceptance uses `UOpenGolf.Visual.CoastalTerrain` on the real course. Headless tests do not establish visual quality or frame rate.
