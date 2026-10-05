#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Plot archived ColorSync measurements without invoking a CMS backend.

Requires Matplotlib (figures generated with 3.10.8). The recorded float32
words and decoded SIXEL images are inputs, not host-dependent test goldens.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import struct

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
TOPIC = ROOT / "docs/loader/color-management"
DATA = TOPIC / "measurements"
OUT = TOPIC / "figures"
INK = "#243746"
BLUE = "#096b9e"
ORANGE = "#b84314"


def f32(word):
    """Recover a recorded binary32 value without re-evaluating its curve."""
    return struct.unpack("<f", struct.pack("<I", word))[0]


def load_record():
    record = json.loads((DATA / "colorsync-gamma2.json").read_text())
    for name, expected in record["images_sha256"].items():
        if hashlib.sha256((DATA / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"Archived image changed: {name}")
        image = plt.imread(DATA / name)
        reference = plt.imread(DATA / "reference-sixel.png")
        changed = int((image != reference).any(axis=2).sum())
        assert changed == record["decoded_pixels_differ_from_reference"][name]
    samples = record["samples"]
    assert len(samples) == 64
    assert all(f32(row["input_f32_bits"]) ==
               f32(struct.unpack("<I", struct.pack("<f",
                   row["input_u16"] / 65535))[0]) for row in samples)
    assert all(a["input_u16"] < b["input_u16"]
               for a, b in zip(samples, samples[1:]))
    return record


def render(record, mobile):
    """Keep archived CMS values and post-quantization previews separate."""
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11,
                         "text.color": INK, "axes.labelcolor": INK,
                         "xtick.color": INK, "ytick.color": INK})
    fig = plt.figure(figsize=(7.2, 14) if mobile else (12, 8.2), dpi=120,
                     facecolor="white")
    grid = fig.add_gridspec(4 if mobile else 2, 1 if mobile else 3,
                           height_ratios=[1.4, 1, 1, 1] if mobile else [1.5, 1],
                           left=.13 if mobile else .085, right=.97,
                           top=.885, bottom=.085, hspace=.55 if mobile else .65,
                           wspace=.22)
    ax = fig.add_subplot(grid[0, :])
    rows = record["samples"]
    x = [f32(row["input_f32_bits"]) for row in rows]
    curv = [f32(row["native_curv_rgb_f32_bits"][0]) for row in rows]
    para = [f32(row["native_para3_rgb_f32_bits"][0]) for row in rows]
    ref = [f32(row["reference_rgb_f32_bits"][0]) for row in rows]
    ax.plot(x, curv, color=ORANGE, lw=2.8, label="ColorSync: original curv")
    ax.plot(x, para, color=BLUE, lw=2.5, ls="--",
            label="ColorSync: equivalent para3")
    ax.plot(x, ref, color=INK, lw=0, marker="o", ms=4, markevery=4,
            markerfacecolor="white", label="Independent ICC reference")
    ax.set(xlim=(0, .126), ylim=(0, .0165),
           xlabel="Normalized input (uint16 / 65535, stored as float32)",
           ylabel="Linear sRGB output (red component)")
    ax.set_xticks([0, .025, .05, .075, .1, .125])
    ax.set_yticks([0, .004, .008, .012, .016])
    ax.grid(color="#dce4e9", lw=.7)
    ax.set_axisbelow(True)
    ax.legend(loc="upper left", frameon=False, fontsize=9 if mobile else 11)
    ax.annotate("Dark tones lifted", xy=(x[16], curv[16]),
                xytext=(.060, .011), color=ORANGE, fontsize=10,
                arrowprops={"arrowstyle": "->", "color": ORANGE})
    title = ("ColorSync: the same gamma,\ndifferent dark tones" if mobile else
             "ColorSync: the same gamma, different dark tones")
    fig.suptitle(title, x=.05, y=.985, ha="left",
                 fontsize=16 if mobile else 19)
    fig.text(.05, .925,
             "Gamma 2 / Gray ICC / macOS 26.5.1 / measurements from 2026-09-15",
             fontsize=9 if mobile else 11, color="#526574")
    labels = [("native-curv-sixel.png", "Original curv", "995 / 4096 pixels differ"),
              ("equivalent-para3-sixel.png", "Equivalent para3", "Matches reference SIXEL"),
              ("reference-sixel.png", "Independent reference", "Reference encoded as SIXEL")]
    for i, (name, title, note) in enumerate(labels):
        image_ax = fig.add_subplot(grid[i + 1, 0] if mobile else grid[1, i])
        image_ax.imshow(plt.imread(DATA / name), interpolation="nearest")
        image_ax.set_title(title, fontsize=12, pad=9)
        image_ax.set_xticks([])
        image_ax.set_yticks([])
        image_ax.set_xlabel(note, fontsize=9, labelpad=8)
    fig.text(.05, .017,
             "Bottom: archived decoded SIXEL, 64 × 64, nearest-neighbor enlargement; no brightening."
             if not mobile else
             "Previews: archived decoded SIXEL, 64 × 64.\n"
             "Nearest-neighbor enlargement; no brightening.",
             fontsize=9, color="#526574")
    stream = io.BytesIO()
    fig.savefig(stream, format="png", dpi=120,
                metadata={"Software": "libsixel ColorSync gamma figure"})
    plt.close(fig)
    return stream.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="require byte-identical regenerated figures")
    args = parser.parse_args()
    record = load_record()
    for mobile, name in [(False, "colorsync-gamma2-wide.png"),
                         (True, "colorsync-gamma2-mobile.png")]:
        output = OUT / name
        payload = render(record, mobile)
        if args.check:
            if not output.exists() or output.read_bytes() != payload:
                raise SystemExit(f"Figure needs regeneration: {output}")
        else:
            OUT.mkdir(exist_ok=True)
            output.write_bytes(payload)
        print(f"{'verified' if args.check else 'wrote'} {output.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
