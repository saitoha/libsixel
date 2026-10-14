#!/usr/bin/env python3
"""Regenerate independent, five-scale resampling quality references.

The 240-pixel source gives exact 192/288-pixel targets at 80/120 percent.
Source pixels come from the original photograph, before any SIXEL encoding.
ImageMagick supplies weighted-filter and JPEG references. Nearest references
use the documented floor-coordinate selector, not Point center sampling.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DATA = Path(__file__).resolve().parent

def magick(*args):
    subprocess.run(["magick", *map(str, args)], check=True)

magick(ROOT / "images/snake.png", "-filter", "Lanczos", "-resize", "240x240!",
       "-depth", "8", "-strip", "PNG24:" + str(DATA / "snake_240.png"))
filters = {
    "gaussian": ["-filter", "Gaussian"],
    "hanning": ["-filter", "Hann"],
    "hamming": ["-filter", "Hamming", "-define", "filter:support=1"],
    "bilinear": ["-filter", "Triangle"],
    "welsh": ["-filter", "Welch"],
    "bicubic": ["-filter", "Cubic"],
    "lanczos2": ["-filter", "Lanczos", "-define", "filter:lobes=2"],
    "lanczos3": ["-filter", "Lanczos", "-define", "filter:lobes=3"],
    "lanczos4": ["-filter", "Lanczos", "-define", "filter:lobes=4"],
}
for name, flags in filters.items():
    for percent in (80, 120):
        target = DATA / "scaling" / f"snake_240_{name}_{percent}pct.png"
        magick(DATA / "snake_240.png", *flags, "-resize", f"{percent}%",
               "-depth", "8", "-strip", "PNG24:" + str(target))
# Nearest selects floor(output_index * source_size / output_size).
# ImageMagick Point uses a different sampling phase and is not this oracle.
source_rgb = subprocess.check_output([
    "magick", str(DATA / "snake_240.png"), "-depth", "8", "rgb:-"])
assert len(source_rgb) == 240 * 240 * 3
for percent in (80, 120):
    size = 240 * percent // 100
    expected = bytearray()
    for y in range(size):
        for x in range(size):
            offset = ((y * 240 // size) * 240 + x * 240 // size) * 3
            expected.extend(source_rgb[offset:offset + 3])
    subprocess.run([
        "magick", "-size", f"{size}x{size}", "-depth", "8", "rgb:-",
        "-strip", "PNG24:" + str(DATA / "scaling" /
                                  f"snake_240_nearest_{percent}pct.png")],
        input=expected, check=True)
magick(DATA / "snake_192.png", "-sampling-factor", "4:4:4", "-quality", "95",
       "-strip", DATA / "snake_192.jpg")
magick(DATA / "snake_192.jpg", "-filter", "Triangle", "-resize", "191x191!",
       "-depth", "8", "-strip",
       "PNG24:" + str(DATA / "snake_192_jpg_resize_191x.png"))
