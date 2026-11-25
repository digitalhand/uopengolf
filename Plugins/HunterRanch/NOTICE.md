# Hunter Ranch course data

This plugin is a downloadable course. It is cooked into its own course pack and is not part of the base game.

- **Layout.** Course outlines, hole routes and lakes are derived from OpenStreetMap data,
  © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright), under the
  [Open Database License 1.0](https://opendatacommons.org/licenses/odbl/1-0/). The derived layout and the author
  corrections are published under the same licence next to each course pack (`layout/` beside the pack files).
  The corrections are also in `Assets/Data/CourseDesign/HunterRanch/`.
- **Elevation.** Ground heights come from the USGS 3D Elevation Program one-metre bare-earth model
  ([source tile](https://www.sciencebase.gov/catalog/item/60d16eecd34e86b938ac09cf)), which is in the US public domain.
- **Review imagery.** Tee and cup positions were checked against USDA NAIP 2022 aerial imagery (US public domain).
  The imagery is not included.
- **Scorecard.** Pars and published tee yardages follow the club's
  [scorecard](https://www.hunterranchgolf.com/golf-course/scorecard). Tee and cup positions are estimates from the
  map data, elevation and aerial imagery, not surveyed positions. Green contours come from the one-metre elevation
  model; none are shaped by hand.

- **Tree.** [Island Tree 02](https://polyhaven.com/a/island_tree_02) by Rob Tuytel (scanning, processing) and Rico
  Cilliers (cleanup, processing), from Poly Haven, under [CC0 1.0](https://polyhaven.com/license)
  ([full text](LICENSES/CC0-1.0.txt)), in `Content/Vegetation/Trees/IslandTree02/`. It stands in for the course's
  native blue oak. Changes (2026-11-24, `Scripts/ExportIslandTree.py`):
  - scaled to about 9 m tall, with smaller, denser leaves
  - leaves tinted and desaturated toward a blue-oak foliage colour
  - textures re-saved as 8-bit PNGs at 2K (masks 1K)
  - hidden ball-contact helpers derived from it: the trunk and main limbs, and one convex hull around the crown

  CC0 needs no attribution; it is recorded for provenance.

The course uses the project's own surface materials, ball, cup and flag, which keep the project's licence. Its tree
uses the shared open-licence materials of the base game's `OpenGolfSavanna` plugin.
