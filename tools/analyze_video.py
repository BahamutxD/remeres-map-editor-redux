"""Frame-level analysis of a recorded RME session.

Extracts frames with ffmpeg, reports per-frame brightness/black/magenta/change metrics,
writes a contact sheet of the whole session and saves the worst frames full size so they can
be looked at directly.

Usage: python analyze_video.py <video> <workdir>
"""
from __future__ import annotations

import csv
import os
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

# Last-resort location used only when ffmpeg is neither on PATH nor given via RME_FFMPEG.
_FALLBACK_FFMPEG = r"C:\Users\user\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-9.0-full_build\bin\ffmpeg.exe"
FPS = 15
SCALE_W = 960


def _resolve_ffmpeg() -> str:
    """Find ffmpeg: RME_FFMPEG env var first, then PATH, then a known winget location."""
    candidate = os.environ.get("RME_FFMPEG", "")
    if candidate and (shutil.which(candidate) or os.path.isfile(candidate)):
        return candidate
    found = shutil.which("ffmpeg")
    if found:
        return found
    return _FALLBACK_FFMPEG


FFMPEG = _resolve_ffmpeg()


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def extract(video: str, frames_dir: str) -> None:
    os.makedirs(frames_dir, exist_ok=True)
    for f in os.listdir(frames_dir):
        os.remove(os.path.join(frames_dir, f))
    run([FFMPEG, "-y", "-hide_banner", "-loglevel", "error", "-i", video,
         "-vf", f"fps={FPS},scale={SCALE_W}:-2", "-q:v", "3",
         os.path.join(frames_dir, "f%05d.jpg")])


def metrics(frames_dir: str) -> list[dict]:
    names = sorted(os.listdir(frames_dir))
    rows: list[dict] = []
    prev: np.ndarray | None = None
    for name in names:
        img = Image.open(os.path.join(frames_dir, name)).convert("RGB")
        a = np.asarray(img, dtype=np.int16)
        mx = a.max(axis=2)
        black = float((mx < 20).mean()) * 100.0
        dark = float((mx < 40).mean()) * 100.0
        r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]
        magenta = float(((r > 200) & (g < 60) & (b > 200)).mean()) * 100.0
        changed = 0.0
        if prev is not None:
            changed = float((np.abs(a - prev).max(axis=2) > 16).mean()) * 100.0
        # map area: ignore the window chrome (top 90 px) and the right/bottom panels
        m = a[90:, :, :]
        map_black = float((m.max(axis=2) < 20).mean()) * 100.0
        rows.append({"frame": len(rows), "file": name, "lum": float(a.mean()),
                     "black": black, "dark": dark, "map_black": map_black,
                     "magenta": magenta, "changed": changed})
        prev = a
    return rows


def contact_sheet(frames_dir: str, rows: list[dict], out: str, cols: int = 8, rows_n: int = 6) -> None:
    count = cols * rows_n
    n = len(rows)
    if n <= count:
        picks = list(rows)
    else:
        # Stratified picks over the whole session (floor(i * n / count) is strictly
        # increasing for i in [0, count)), plus the final frame, so a session slightly
        # longer than the sheet never loses its tail.
        indices = sorted({(i * n) // count for i in range(count)} | {n - 1})
        picks = [rows[i] for i in indices]
    cell_w, cell_h = 232, 132
    sheet = Image.new("RGB", (cols * cell_w, rows_n * (cell_h + 14)), (12, 12, 16))
    draw = ImageDraw.Draw(sheet)
    for i, row in enumerate(picks):
        img = Image.open(os.path.join(frames_dir, row["file"])).convert("RGB")
        img.thumbnail((cell_w - 4, cell_h - 4))
        cx = (i % cols) * cell_w
        cy = (i // cols) * (cell_h + 14)
        sheet.paste(img, (cx + 2, cy + 12))
        draw.text((cx + 3, cy + 1), f"f{row['frame']} bk{row['black']:.1f} ch{row['changed']:.0f}",
                  fill=(200, 220, 255))
    sheet.save(out)


def full_frame(video: str, index: int, out: str) -> None:
    # metrics() frames come from an fps=FPS re-encode, so sampled frame `index` sits at
    # index / FPS seconds in the source; sample the original video by that timestamp
    # instead of by the original stream's frame index (which is a different frame rate).
    timestamp = index / FPS
    run([FFMPEG, "-y", "-hide_banner", "-loglevel", "error", "-ss", f"{timestamp:.3f}", "-i", video,
         "-frames:v", "1", "-q:v", "2", out])


def main() -> None:
    video = sys.argv[1]
    work = sys.argv[2]
    frames_dir = os.path.join(work, "frames")
    extract(video, frames_dir)
    rows = metrics(frames_dir)
    if not rows:
        print(f"no analyzable frames: ffmpeg produced no decodable frames from {video}", file=sys.stderr)
        return 1
    print(f"frames: {len(rows)}")
    with open(os.path.join(work, "video_metrics.csv"), "w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    by_black = sorted(rows, key=lambda r: -r["black"])[:12]
    by_mapblack = sorted(rows, key=lambda r: -r["map_black"])[:12]
    by_change = sorted(rows, key=lambda r: -r["changed"])[:12]
    by_magenta = sorted(rows, key=lambda r: -r["magenta"])[:6]

    def show(title: str, items: list[dict], key: str) -> None:
        print(f"\n== {title} ==")
        for r in items:
            print(f"  f{r['frame']:5d} {key}={r[key]:7.2f} black={r['black']:6.2f} map_black={r['map_black']:6.2f} "
                  f"magenta={r['magenta']:5.2f} changed={r['changed']:6.2f} lum={r['lum']:6.1f}")

    show("blackest (full frame)", by_black, "black")
    show("blackest (map area)", by_mapblack, "map_black")
    show("largest frame-to-frame change", by_change, "changed")
    show("most magenta (missing-sprite fallback)", by_magenta, "magenta")

    print("\n== worst 4 map-black frames + worst 4 change frames written full size ==")
    picks = {r["frame"] for r in by_mapblack[:4]} | {r["frame"] for r in by_change[:4]}
    for index in sorted(picks):
        out = os.path.join(work, f"anomaly_f{index:05d}.png")
        full_frame(video, index, out)
        print("  " + out)

    sheet = os.path.join(work, "contact_sheet.png")
    contact_sheet(frames_dir, rows, sheet)
    print("\ncontact sheet: " + sheet)


if __name__ == "__main__":
    main()
