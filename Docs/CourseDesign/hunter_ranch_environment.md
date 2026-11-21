# Hunter Ranch: vegetation, water, paths and visual finishing

[Series](hunter_ranch_workflow.md) | Previous: [Unreal](hunter_ranch_unreal.md) | Next: [Full course](hunter_ranch_full_course.md)

**Inputs:** validated first-hole terrain and dated course references. **Outputs:** a consistent
environment pass with source records and performance observations. Use this chapter after
terrain scale and surface behaviour are understood; scenery can otherwise hide important defects.

## Table of contents

- [Choose a visual target](#choose-a-visual-target)
- [Place significant trees first](#place-significant-trees-first)
- [Add grass and ground materials](#add-grass-and-ground-materials)
- [Build water and paths](#build-water-and-paths)
- [Model clubhouse context](#model-clubhouse-context)
- [Set lighting and wind](#set-lighting-and-wind)
- [Measure the cost of detail](#measure-the-cost-of-detail)

## Choose a visual target

Revisit the [official course information and drone reference](https://www.hunterranchgolf.com/golf-course/information).
Its blue oaks, rolling land and water features provide useful priorities. Create a small
reference board for the selected season: turf colour, dry background colour, oak canopy form,
sand tone and sky conditions. Record unknown photo dates rather than treating every image as current.

Capture the first-hole tee, approach and green from fixed camera positions in the test map.
Use these views throughout the art pass. At first, assess silhouette, scale and placement;
close-up bark and grass detail cannot fix a wrongly positioned landmark tree.

## Place significant trees first

1. In QGIS, identify tree groups from the chosen imagery and reference views. A canopy footprint
   does not necessarily identify the trunk centre or exact height; record those estimates.
2. Transfer a few important tree positions into Unreal using the shared coordinates. Place
   individual actors initially so each can be compared against a reference.
3. Choose appropriately licensed models with plausible scale and form. Inspect an asset's
   stated dimensions in-engine, rather than scaling it solely until its canopy fills a screenshot.
4. Check tee and approach sightlines. Distinguish trees influencing the shot from distant scenery.
5. Once the main placements work, use Foliage Mode for repeated groups. Keep separate foliage
   types for large trees, smaller shrubs and ground cover so density/culling can be tuned independently.

Epic's [Foliage Mode guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/foliage-mode-in-unreal-engine)
describes painting and instance controls. Use it for repeated placement, not as a substitute
for reference-based landmark positions. Do not randomly fill the entire rough with trees.

Tree collision and canopy interaction are gameplay decisions. Confirm the current physics
plugin's obstacle/foliage conventions before assigning arbitrary blocking volumes. Cosmetic
leaf cards should not inadvertently become ground hits.

## Add grass and ground materials

Start with consistent ground materials and surface boundaries. Set colour and texture scale
from player height, then compare from the tee. Mowing direction is an art parameter; it should
not require deforming the underlying golf geometry.

Read the maintained [turf workflow](../TurfWorkflow.md) before modifying shared grass sources.
It describes an existing surface field used to align turf appearance with outlines. An imported
Hunter Ranch mesh does not automatically generate that field. Decide whether to implement a
compatible field later or use a separate tested course material setup; do not run broad asset
regeneration scripts merely to experiment with this course.

Keep dense blades near gameplay cameras and measure the effect. Start with grass disabled on
the putting surface until the ball remains clearly visible. If blades change colour at material
boundaries, inspect their colour source rather than disguising the problem with more density.

## Build water and paths

Use verified water boundaries from the layout. Create a simple visual water surface first,
placing it at a justified height and inspecting its intersection with banks. Do not use a
sloping DEM over a lake as proof that the visible water surface should slope the same way.
Document unknown bed depths and artistic shoreline adjustments.

A water shader only supplies appearance. Penalty areas, ball interaction, drops and scoring
require explicit gameplay integration. Keep a list of water regions needing those decisions.
If using Unreal's Water plugin later, evaluate it separately rather than assuming it connects
to the golf rules automatically.

For paths, follow the traced centreline or footprint using a mesh/spline workflow. Match the
terrain without creating an invisible raised collision strip. Decide whether path surfaces use
the existing Firm preset or need another tested behaviour. Check crossing shots and rolls.

## Model clubhouse context

Begin with measured or estimated building footprints, simple roof masses and reference-based
placement. Use labelled proxy geometry until dimensions and viewpoints are credible. Add doors,
windows and furnishings only where they matter from gameplay cameras.

Keep an asset ledger with `asset`, `creator/provider`, `source URL`, `licence`, `modifications`
and `intended use`. Reference photographs and official logos should not automatically become
distributed textures. Original generic geometry can establish visual context while more specific
details remain unresolved.

## Set lighting and wind

Choose a stable lighting setup for validation screenshots before adding a day/night system.
Use a plausible sun direction and soft enough exposure that green contours remain readable.
Do not confuse a shadow-induced slope impression with the actual collision geometry.

For wind, consult the [course kit reference](../../Plugins/OpenGolfCourse/Docs/Reference.md).
Its wind system is connected through the game's existing consumers; adding a generic wind actor
does not prove that flags, foliage and ball simulation share one wind state. Record which systems
are connected in the Hunter Ranch prototype.

## Measure the cost of detail

Record frame times at the fixed tee, approach and green viewpoints with no decorative foliage,
then with trees, then with grass. Keep resolution, scalability and camera position identical.
Use [the validation guide](hunter_ranch_validation.md#measure-performance) for recording.

If a pass causes a regression, reduce or adjust that category before adding more. Consider
instance density, distance culling, shadow cost and mesh complexity separately. Preserve the
terrain's gameplay collision while optimising visible scenery. Proceed to the full course only
after the first-hole visual target is recognisable and its performance cost is understood.
