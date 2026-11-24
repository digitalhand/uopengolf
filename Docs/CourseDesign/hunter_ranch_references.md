# Hunter Ranch: references and course identity

[Series](hunter_ranch_workflow.md) | Next: [Setup](hunter_ranch_setup.md)

**Inputs:** a browser and a place to save notes. **Outputs:** source register, Hole 1 reference
sheet, scorecard transcription and an uncertainty list. Complete this before tracing golf features.

## Table of contents

- [Establish the official reference set](#establish-the-official-reference-set)
- [Transcribe the scorecard](#transcribe-the-scorecard)
- [Identify Hole 1](#identify-hole-1)
- [Build a visual reference sheet](#build-a-visual-reference-sheet)
- [Record sources and uncertainty](#record-sources-and-uncertainty)
- [Check before proceeding](#check-before-proceeding)

## Establish the official reference set

1. Open [Hunter Ranch's website](https://www.hunterranchgolf.com/). Confirm the course is the
   one at 4041 Highway 46 East, Paso Robles, CA 93446, rather than another similarly named club.
2. Open [Course Information](https://www.hunterranchgolf.com/golf-course/information). Its
   description and embedded drone tour are useful for identifying landforms and course character.
   The club highlights rolling ground, blue oaks, lakes and greens with pronounced contours.
3. Open the [official scorecard](https://www.hunterranchgolf.com/golf-course/scorecard) and its
   enlarged image. Record the access date and any printed revision date separately.
4. Keep the URLs in `references/sources.md`. A page access date does not prove when a
   photograph was taken or when a scorecard was revised.

## Transcribe the scorecard

Create `references/scorecard.csv` with this header:

```csv
hole_id,par,tee_name,published_yards,source_id,verified_date,notes
```

1. Read the first hole column and all available tee rows. Use exactly the printed tee names.
2. Enter one row per hole and tee set. Leave unreadable fields blank and describe the problem
   in `notes`; zero is a real number and should not mean unknown.
3. Record par once verified, consistently across tee rows unless the card explicitly differs.
4. Recheck the transcription independently against the enlarged image. For each tee set,
   sum the front nine, back nine and all holes and compare with the printed totals.
5. Convert distances using `metres = yards * 0.9144`. Keep the original yardage alongside the
   conversion so you can audit it. Do not overwrite a published value with a GIS measurement.

**Illustrative arithmetic, not Hunter Ranch's Hole 1 yardage:** 400 yd is 365.76 m. A
measured direct tee-to-centre line can differ from a scorecard distance measured along a dogleg.
Never rescale a whole course to force one such line to agree.

## Identify Hole 1

1. Read any official hole labels available in the references. Look for the tee direction,
   green, road/clubhouse context and adjacent holes as a group.
2. Watch the official drone reference, if available, without assuming every transition denotes
   the next numbered hole. Record useful timestamps and what is visibly established.
3. Match the candidate route to imagery once it is acquired. Use two or more distinctive
   features, such as a bunker arrangement and nearby path junction, to support the match.
4. Mark `hole_id = 1` only when the match is credible. If identity remains uncertain, retain a
   temporary reference label and resolve it from better references or course-supplied information.
5. Distinguish the course's numbered greens from practice facilities. Counting green-shaped
   areas in imagery is not an authoritative hole count or routing method.

## Build a visual reference sheet

For Hole 1, create sections for the tee, intended landing area, approach, green and surrounding
context. Under each view record direction, source, date if known, visible feature and confidence.
Start with these questions:

- Is the landing area uphill, downhill or across a slope from this viewpoint?
- Which trees affect the view from a tee, rather than merely filling the background?
- Can you see a bunker's floor and lip, or only a bright sand footprint from above?
- Does the green have a visible plateau or slope break? Is that supported by elevation data?
- Is a feature absent, or hidden by a tree, shadow or camera angle?

Select one reference season for the art pass. Avoid mixing dry background vegetation from one
date with unrelated lighting and turf colours from another unless that mixture is intentional.
Keep vegetation placement and turf shading secondary to terrain and golf boundaries initially.

## Record sources and uncertainty

Use this template once per source:

```text
Source ID: HR-REF-001
Title:
URL / original filename:
Provider:
Access date:
Acquisition / publication date (or unknown):
Used for:
Known restrictions / attribution:
Reliability limitations:
Related hole IDs:
```

Maintain a separate table with `feature`, `observation`, `evidence`, `confidence`, and
`next_action`. For example, a tree-obscured bunker edge can be recorded as an approximate
trace awaiting a ground view. Do not label a designer-invented putting contour as measured.

Public viewing access is different from permission to redistribute imagery, logos or extracted
geometry. Keep references as references until their reuse terms are established. Use original
or appropriately licensed materials for the distributed course.

## Check before proceeding

You should be able to explain why the first-hole identification is correct, point to the source
of each transcribed number, and list what is still unknown. If you cannot, continue collecting
references before spending time on detailed Blender geometry. Go next to [setup](hunter_ranch_setup.md),
then [site boundary](hunter_ranch_site_boundary.md).
