#!/usr/bin/env python3
"""Pixel-by-pixel screenshot comparison for renderer parity checks.

Usage:
    python tools/screenshot_diff.py before.png after.png [--diff-out diff.png] [--tolerance N]

Compares two screenshots of identical size and reports differing pixel ratio,
max per-channel delta, and mean delta. Exits 0 when images are identical
within tolerance, 1 otherwise. Used to prove no visual regression between
renderer builds (classic vs chunk cache, baseline vs perf wins).
"""

from __future__ import annotations

import argparse
import sys

from PIL import Image, ImageChops


def compare(before: str, after: str, tolerance: int, diff_out: str | None) -> tuple[bool, dict]:
    img_a = Image.open(before).convert("RGB")
    img_b = Image.open(after).convert("RGB")
    if img_a.size != img_b.size:
        return False, {"error": f"size mismatch: {img_a.size} vs {img_b.size}"}

    diff = ImageChops.difference(img_a, img_b)
    total = img_a.size[0] * img_a.size[1]

    # A pixel differs when any channel exceeds tolerance. Collapse the
    # per-channel difference to a single mask via lighter().
    if tolerance > 0:
        diff = diff.point(lambda v: 0 if v <= tolerance else v)
    r, g, b = diff.split()
    gray = ImageChops.lighter(ImageChops.lighter(r, g), b)
    differing = total - gray.histogram()[0]

    extrema = diff.getextrema()
    max_delta = max(mx for _, mx in extrema)
    hist = diff.histogram()
    mean_delta = sum(i * c for band in range(3) for i, c in enumerate(hist[band * 256:(band + 1) * 256])) / (total * 3)

    stats = {
        "size": f"{img_a.size[0]}x{img_a.size[1]}",
        "differing_pixels": differing,
        "differing_ratio": round(differing / total, 6),
        "max_channel_delta": max_delta,
        "mean_channel_delta": round(mean_delta, 4),
    }
    if diff_out and differing:
        diff.point(lambda v: min(255, v * 4)).save(diff_out)
        stats["diff_image"] = diff_out
    return differing == 0, stats


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("before")
    ap.add_argument("after")
    ap.add_argument("--diff-out", default=None)
    ap.add_argument("--tolerance", type=int, default=0)
    args = ap.parse_args()

    ok, stats = compare(args.before, args.after, args.tolerance, args.diff_out)
    for k, v in stats.items():
        print(f"{k}: {v}")
    print("IDENTICAL" if ok else "DIFFERENT")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
