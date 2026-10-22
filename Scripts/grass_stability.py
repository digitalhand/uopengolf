# SPDX-License-Identifier: MIT
"""Measure grass shimmer from the stationary frame sequences of UOpenGolf.Visual.RangeAssets.

Compares frames of a view's sequence over the lower (turf) part of the image: the mean of
each pixel's largest RGB-channel difference (out of 255) and the percentage of pixels whose
largest difference exceeds a threshold. The primary figure averages every consecutive frame
pair, which is far less noisy than one first-vs-last pair (also printed, matching the
original Windows measurements). View0 is the tee with
grass, View1 the same camera without grass, View4 the grass closeup and View5 the edge.
Requires Pillow. Example:

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
VIEW_NAMES = {0: "TeeGrassOn", 1: "TeeGrassOff", 4: "GrassClose", 5: "GrassEdge"}
COASTAL_VIEW_NAMES = {0: "TeeGrassOn", 1: "TeeGrassOff", 4: "EdgeGrassOn", 5: "EdgeGrassOff", 6: "CloseGrassOn", 7: "CloseGrassOff"}


@dataclass(frozen=True)
class Stability:
    mean_max_difference: float
    changed_percent: float


def compare_frames(first, last, crop_fraction=DEFAULT_CROP_FRACTION, threshold=DEFAULT_THRESHOLD, band=None):
    """Compare two frames over the bottom crop_fraction of rows, or band=(top, bottom) row fractions."""
    if band is None:
        if not 0.0 < crop_fraction <= 1.0:
            raise ValueError(f"crop_fraction must be in (0, 1]: {crop_fraction}")
        band = (1.0 - crop_fraction, 1.0)
    if not 0.0 <= band[0] < band[1] <= 1.0:
        raise ValueError(f"band must satisfy 0 <= top < bottom <= 1: {band}")
    with Image.open(first) as a, Image.open(last) as b:
        if a.size != b.size:
            raise ValueError(f"Frame sizes differ: {a.size} vs {b.size}")
        width, height = a.size
        top = min(round(height * band[0]), height - 1)
        box = (0, top, width, max(top + 1, round(height * band[1])))
        difference = ImageChops.difference(a.convert("RGB").crop(box), b.convert("RGB").crop(box))
    red, green, blue = difference.split()
    largest = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    histogram = largest.histogram()
    pixels = sum(histogram)
    total = sum(level * count for level, count in enumerate(histogram))
    changed = sum(histogram[threshold + 1:])
    return Stability(total / pixels, 100.0 * changed / pixels)


def compare_sequence(frames, crop_fraction=DEFAULT_CROP_FRACTION, threshold=DEFAULT_THRESHOLD, band=None):
    """Average of compare_frames over every consecutive pair of frames."""
    if len(frames) < 2:
        raise ValueError("A sequence needs at least two frames")
    pairs = [compare_frames(a, b, crop_fraction, threshold, band) for a, b in zip(frames, frames[1:])]
    return Stability(sum(p.mean_max_difference for p in pairs) / len(pairs),
                     sum(p.changed_percent for p in pairs) / len(pairs))


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
    except (OSError, ValueError) as error:
        sys.exit(f"Grass stability measurement failed: {error}")


if __name__ == "__main__":
    main()
