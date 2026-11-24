# SPDX-License-Identifier: MIT
"""Measure training-ring shimmer from the stationary sequences of UOpenGolf.Visual.TrainingTarget.

Each tee view is captured twice with the same camera: <view>-NN.png with the rings shown and
<view>-hidden-NN.png with them hidden. <view>-ring.json gives, as viewport fractions, the selected
ring's screen box and points along its far (top) arc, which breaks into dashes when it is
thinner than a pixel. For each view this prints:

- far-arc continuity: the share of the arc's columns where the shown frame is brighter than the
  mean hidden frame within a few rows of the projected arc (mean and worst frame);
- arc toggles: the share of those columns that switch between present and missing from one
  frame to the next, which is the visible shimmer of a still camera;
- the stationary change and sparkle (> 16/255) inside the ring box, shown and hidden.

Requires Pillow. Example:

    python3 Assets/Scripts/ring_stability.py Saved/Screenshots/TrainingGames
"""
import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import sys

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from grass_stability import SPARKLE_THRESHOLD, Stability, compare_sequence  # noqa: E402  (run as a script or imported as Assets.Scripts.ring_stability)

DEFAULT_RADIUS = 2
# Luma levels above the hidden frame that count as ring; far above turf frame-to-frame noise.
DEFAULT_THRESHOLD = 24


@dataclass(frozen=True)
class RingStability:
    frames: int
    continuity_mean: float
    continuity_min: float
    toggle_percent: float
    shown: Stability
    hidden: Stability


def arc_columns(points):
    """(x, y) pixel positions of the polyline through points, one per integer column it spans."""
    if len(points) < 2:
        raise ValueError("The far arc needs at least two points")
    ordered = sorted(points)
    columns = []
    for (x0, y0), (x1, y1) in zip(ordered, ordered[1:]):
        start = round(x0) if not columns else columns[-1][0] + 1
        for x in range(start, round(x1) + 1):
            t = 0.0 if x1 == x0 else min(max((x - x0) / (x1 - x0), 0.0), 1.0)
            columns.append((x, y0 + (y1 - y0) * t))
    return columns


def _luma(source):
    """Luma of an image or of the image file at a path."""
    if isinstance(source, Image.Image):
        return source.convert("L")
    with Image.open(source) as image:
        return image.convert("L")


def mean_frame(paths):
    """Per-pixel mean of the frames, as luma."""
    mean = None
    for count, path in enumerate(paths, start=1):
        luma = _luma(path)
        if mean is None:
            mean = luma
            continue
        if luma.size != mean.size:
            raise ValueError(f"Frame sizes differ: {mean.size} vs {luma.size}")
        mean = Image.blend(mean, luma, 1.0 / count)
    if mean is None:
        raise ValueError("No frames to average")
    return mean


def arc_presence(frame, background, columns, radius=DEFAULT_RADIUS, threshold=DEFAULT_THRESHOLD):
    """Whether each arc column shows the ring: frame brighter than background within radius rows.

    frame and background are paths or images; background is the rings-hidden view.
    """
    shown, hidden = _luma(frame), _luma(background)
    if shown.size != hidden.size:
        raise ValueError(f"Frame sizes differ: {shown.size} vs {hidden.size}")
    width, height = shown.size
    on, off = shown.load(), hidden.load()
    presence = []
    for x, y in columns:
        if not 0 <= x < width:
            continue
        rows = range(max(0, round(y) - radius), min(height, round(y) + radius + 1))
        presence.append(any(on[x, row] - off[x, row] > threshold for row in rows))
    return presence


def continuity(presence):
    """Percentage of arc columns where the ring shows."""
    if not presence:
        raise ValueError("The far arc lies outside the frame")
    return 100.0 * sum(presence) / len(presence)


def toggle_percent(presences):
    """Mean percentage of arc columns that switch between consecutive frames."""
    if len(presences) < 2:
        raise ValueError("A sequence needs at least two frames")
    pairs = [sum(a != b for a, b in zip(first, last)) / len(first) for first, last in zip(presences, presences[1:])]
    return 100.0 * sum(pairs) / len(pairs)


def _frames(directory, pattern):
    return sorted(Path(directory).glob(pattern))


def find_views(directory):
    """Views that have a ring sidecar, sorted by name."""
    return sorted(path.name[: -len("-ring.json")] for path in Path(directory).glob("*-ring.json"))


def measure_view(directory, view, radius=DEFAULT_RADIUS, threshold=DEFAULT_THRESHOLD):
    directory = Path(directory)
    sidecar = json.loads((directory / f"{view}-ring.json").read_text())
    shown = _frames(directory, f"{view}-[0-9][0-9].png")
    hidden = _frames(directory, f"{view}-hidden-[0-9][0-9].png")
    if len(shown) < 2 or len(hidden) < 2:
        raise FileNotFoundError(f"{view} needs at least two shown and two hidden frames in {directory}")
    with Image.open(shown[0]) as first:
        width, height = first.size
    columns = arc_columns([(x * width, y * height) for x, y in sidecar["far_arc"]])
    background = mean_frame(hidden)
    presences = [arc_presence(path, background, columns, radius, threshold) for path in shown]
    scores = [continuity(presence) for presence in presences]
    box = tuple(sidecar["box"])
    return RingStability(len(shown), sum(scores) / len(scores), min(scores), toggle_percent(presences),
                         compare_sequence(shown, box=box), compare_sequence(hidden, box=box))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("directory", type=Path, help="Saved/Screenshots/TrainingGames")
    parser.add_argument("--radius", type=int, default=DEFAULT_RADIUS, help="rows searched either side of the projected arc")
    parser.add_argument("--threshold", type=int, default=DEFAULT_THRESHOLD, help="luma levels above the hidden frame that count as ring")
    args = parser.parse_args()
    try:
        views = find_views(args.directory)
        if not views:
            raise FileNotFoundError(f"No *-ring.json sidecars in {args.directory}")
        print(f"Ring stability in {args.directory} (arc ±{args.radius} rows, ring > {args.threshold}/255 above hidden; "
              f"sparkle > {SPARKLE_THRESHOLD}/255)")
        for view in views:
            result = measure_view(args.directory, view, args.radius, args.threshold)
            print(f"  {view:<10} {result.frames} frames; far arc continuity mean {result.continuity_mean:.1f}% "
                  f"(worst {result.continuity_min:.1f}%), toggles {result.toggle_percent:.1f}%")
            print(f"  {'':<10} ring box shown: changed {result.shown.changed_percent:.3f}%, sparkle {result.shown.sparkle_percent:.3f}%; "
                  f"hidden: changed {result.hidden.changed_percent:.3f}%, sparkle {result.hidden.sparkle_percent:.3f}%")
    except (OSError, ValueError, KeyError) as error:
        sys.exit(f"Ring stability measurement failed: {error}")


if __name__ == "__main__":
    main()
