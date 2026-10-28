# SPDX-License-Identifier: MIT
"""Measure grass shimmer from the stationary frame sequences of UOpenGolf.Visual.RangeAssets.

Compares frames of a view's sequence over the lower (turf) part of the image: the mean of
each pixel's largest RGB-channel difference (out of 255) and the percentage of pixels whose
largest difference exceeds a threshold. The primary figure averages every consecutive frame
pair, which is far less noisy than one first-vs-last pair (also printed, matching the
original Windows measurements). View0 is the tee with
grass, View1 the same camera without grass, View4 the grass closeup and View5 the edge.
The coastal profile also measures a fixed tree-canopy box in the views that show trees, with
the share of pixels that sparkle (change by more than 16/255). Requires Pillow. Example:

    python3 Scripts/grass_stability.py Saved/Screenshots/RangeAssets --sequence 20261016-010000
"""
import argparse
from dataclasses import dataclass
from pathlib import Path
import sys

from PIL import Image, ImageChops

DEFAULT_CROP_FRACTION = 0.4
DEFAULT_THRESHOLD = 3
DEFAULT_BANDS = 20
SPARKLE_THRESHOLD = 16
VIEW_NAMES = {0: "TeeGrassOn", 1: "TeeGrassOff", 4: "GrassClose", 5: "GrassEdge"}
COASTAL_VIEW_NAMES = {0: "TeeGrassOn", 1: "TeeGrassOff", 4: "EdgeGrassOn", 5: "EdgeGrassOff", 6: "CloseGrassOn", 7: "CloseGrassOff"}
# Tree-canopy boxes (left, top, right, bottom fractions) in the coastal views that show trees, chosen
# on 2026-10-27 from the savanna range. The worst band grows with canopy area; a fixed box does not.
COASTAL_CANOPY_BOXES = {0: ("Tee", (0.0, 0.38, 1.0, 0.52)), 3: ("Green", (0.45, 0.16, 0.95, 0.34)),
                        4: ("Edge", (0.38, 0.19, 0.75, 0.40)), 8: ("Bunker", (0.0, 0.0, 1.0, 0.18))}


@dataclass(frozen=True)
class Stability:
    mean_max_difference: float
    changed_percent: float
    sparkle_percent: float = 0.0


def compare_frames(first, last, crop_fraction=DEFAULT_CROP_FRACTION, threshold=DEFAULT_THRESHOLD, band=None, box=None):
    """Compare two frames over the bottom crop_fraction of rows, band=(top, bottom) row fractions,
    or box=(left, top, right, bottom) fractions."""
    if box is None:
        if band is None:
            if not 0.0 < crop_fraction <= 1.0:
                raise ValueError(f"crop_fraction must be in (0, 1]: {crop_fraction}")
            band = (1.0 - crop_fraction, 1.0)
        box = (0.0, band[0], 1.0, band[1])
    left, top, right, bottom = box
    if not (0.0 <= top < bottom <= 1.0 and 0.0 <= left < right <= 1.0):
        raise ValueError(f"box must satisfy 0 <= left < right <= 1 and 0 <= top < bottom <= 1: {box}")
    with Image.open(first) as a, Image.open(last) as b:
        if a.size != b.size:
            raise ValueError(f"Frame sizes differ: {a.size} vs {b.size}")
        width, height = a.size
        x0, y0 = min(round(width * left), width - 1), min(round(height * top), height - 1)
        pixels_box = (x0, y0, max(x0 + 1, round(width * right)), max(y0 + 1, round(height * bottom)))
        difference = ImageChops.difference(a.convert("RGB").crop(pixels_box), b.convert("RGB").crop(pixels_box))
    red, green, blue = difference.split()
    largest = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    histogram = largest.histogram()
    pixels = sum(histogram)
    total = sum(level * count for level, count in enumerate(histogram))
    changed = sum(histogram[threshold + 1:])
    sparkle = sum(histogram[SPARKLE_THRESHOLD + 1:])
    return Stability(total / pixels, 100.0 * changed / pixels, 100.0 * sparkle / pixels)


def compare_sequence(frames, crop_fraction=DEFAULT_CROP_FRACTION, threshold=DEFAULT_THRESHOLD, band=None, box=None):
    """Average of compare_frames over every consecutive pair of frames."""
    if len(frames) < 2:
        raise ValueError("A sequence needs at least two frames")
    pairs = [compare_frames(a, b, crop_fraction, threshold, band, box) for a, b in zip(frames, frames[1:])]
    return Stability(sum(p.mean_max_difference for p in pairs) / len(pairs),
                     sum(p.changed_percent for p in pairs) / len(pairs),
                     sum(p.sparkle_percent for p in pairs) / len(pairs))


def worst_band(frames, bands=DEFAULT_BANDS, threshold=DEFAULT_THRESHOLD):
    """The horizontal band (top, bottom fractions) with the most change, and its Stability.

    Localized flicker such as distant moire near the horizon is diluted, or missed entirely,
    by a whole-frame or lower-crop average.
    """
    if bands < 1:
        raise ValueError(f"bands must be at least 1: {bands}")
    if len(frames) < 2:
        raise ValueError("A sequence needs at least two frames")
    means, changes = [0.0] * bands, [0.0] * bands
    # Decode each pair once, then measure all bands from the same difference image.
    for first, last in zip(frames, frames[1:]):
        with Image.open(first) as a, Image.open(last) as b:
            if a.size != b.size:
                raise ValueError(f"Frame sizes differ: {a.size} vs {b.size}")
            width, height = a.size
            difference = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
        red, green, blue = difference.split()
        largest = ImageChops.lighter(ImageChops.lighter(red, green), blue)
        for index in range(bands):
            top = min(round(height * (index / bands)), height - 1)
            bottom = max(top + 1, round(height * ((index + 1) / bands)))
            histogram = largest.crop((0, top, width, bottom)).histogram()
            pixels = sum(histogram)
            means[index] += sum(level * count for level, count in enumerate(histogram)) / pixels
            changes[index] += 100.0 * sum(histogram[threshold + 1:]) / pixels
    pairs = len(frames) - 1
    results = [((index / bands, (index + 1) / bands),
                Stability(means[index] / pairs, changes[index] / pairs))
               for index in range(bands)]
    return max(results, key=lambda item: item[1].changed_percent)


def find_sequence(directory, sequence_id, view):
    """Captured frames of one view's stationary sequence, in frame order."""
    frames = sorted(Path(directory).glob(f"{sequence_id}-View{view}-Frame[0-9][0-9].png"))
    if len(frames) < 2:
        raise FileNotFoundError(f"No frame sequence for {sequence_id} view {view} in {directory}")
    return frames


def latest_sequence(directory):
    frames = sorted(Path(directory).glob("*-View0-Frame[0-9][0-9].png"))
    if not frames:
        raise FileNotFoundError(f"No stationary sequences in {directory}")
    return frames[-1].name.rsplit("-View0-", 1)[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("directory", type=Path, help="Saved/Screenshots/RangeAssets")
    parser.add_argument("--sequence", help="sequence id prefix (default: newest)")
    parser.add_argument("--profile", choices=("range", "coastal"), default="range", help="capture view numbering (default: range)")
    parser.add_argument("--crop", type=float, default=DEFAULT_CROP_FRACTION, help="lower fraction of rows to compare")
    parser.add_argument("--threshold", type=int, default=DEFAULT_THRESHOLD, help="levels above which a pixel counts as changed")
    args = parser.parse_args()
    try:
        sequence = args.sequence or latest_sequence(args.directory)
        print(f"Sequence {sequence} (lower {args.crop:.0%} of rows, changed > {args.threshold}/255)")
        views = COASTAL_VIEW_NAMES if args.profile == "coastal" else VIEW_NAMES
        for view, name in views.items():
            try:
                frames = find_sequence(args.directory, sequence, view)
            except FileNotFoundError:
                continue
            result = compare_sequence(frames, args.crop, args.threshold)
            ends = compare_frames(frames[0], frames[-1], args.crop, args.threshold)
            with Image.open(frames[0]) as frame:
                width, height = frame.size
            print(f"  View{view} {name:<11} {width}x{height}, {len(frames)} frames; "
                  f"pairs: mean max diff {result.mean_max_difference:.4f}/255, "
                  f"changed {result.changed_percent:.3f}% (first-vs-last {ends.changed_percent:.3f}%)")
            (top, bottom), worst = worst_band(frames, threshold=args.threshold)
            print(f"  {'':<17} worst band rows {top:.0%}-{bottom:.0%}: changed {worst.changed_percent:.3f}%")
        if args.profile == "coastal":
            print(f"Tree canopy boxes (changed > {args.threshold}/255, sparkle > {SPARKLE_THRESHOLD}/255)")
            for view, (name, box) in COASTAL_CANOPY_BOXES.items():
                try:
                    frames = find_sequence(args.directory, sequence, view)
                except FileNotFoundError:
                    continue
                result = compare_sequence(frames, threshold=args.threshold, box=box)
                print(f"  View{view} {name:<11} box {box}: mean max diff {result.mean_max_difference:.4f}/255, "
                      f"changed {result.changed_percent:.3f}%, sparkle {result.sparkle_percent:.3f}%")
    except (OSError, ValueError) as error:
        sys.exit(f"Grass stability measurement failed: {error}")


if __name__ == "__main__":
    main()
