#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Illustrate the built gamut mapper, separately from complete SIXEL encoding.

Requires a configured Autotools static library, NumPy, Matplotlib and Pillow.
Use --build-dir for an out-of-tree build. Internal functions are reached through
a small statically linked probe; no mapping algorithm is reimplemented here.
"""

from __future__ import annotations

import argparse
import colorsys
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import zlib

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle
import numpy as np
from PIL import Image, PngImagePlugin, __version__ as pillow_version

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "docs/concepts/gamut-mapping"
POLICIES = ("clip", "local-minde", "edge-seeker", "ray-trace")
LABELS = ("RGB clip", "Local MINDE", "EdgeSeeker", "Ray Trace")
COLORS = ("#be185d", "#2563eb", "#0f766e", "#d97706")
HUES = (0, 30, 90, 140, 190, 300)
NAMES = ("Red", "Orange", "Yellow-green", "Green", "Cyan", "Magenta")
FLAGS = ["--threads=1", "-.float32",
         "-L", "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
         "-Q", "kmeans:seed=1", "-X", "oklab", "-W", "oklab",
         "--sampling-policy=full-frame", "--binning-policy=none",
         "-F", "none", "-a", "off", "-_", "none", "-~", "none",
         "-0", "soft", "-t", "rgb", "-p", "32", "-=", "2"]


def run(argv, **kwargs):
    return subprocess.run([str(x) for x in argv], check=True,
                          stdout=subprocess.PIPE, **kwargs).stdout


def encode_srgb(values):
    values = np.clip(values, 0, 1)
    return np.where(values <= 0.0031308, 12.92 * values,
                    1.055 * values ** (1 / 2.4) - 0.055)


def save_plot(fig, path):
    fig.savefig(path, metadata={"Date": None}, facecolor="white")
    plt.close(fig)
    # Matplotlib leaves spaces at the ends of SVG path-data lines.
    # Normalize only that serialization whitespace for clean source diffs.
    path.write_text("\n".join(line.rstrip() for line in
                    path.read_text(encoding="utf-8").splitlines()) + "\n",
                    encoding="utf-8")


def sample(probe, rgb, profile=None):
    text = "".join("%.17g %.17g %.17g\n" % tuple(p)
                   for p in rgb.reshape(-1, 3))
    result = run([probe, *([profile] if profile else [])], input=text.encode("ascii"))
    values = np.loadtxt(io.BytesIO(result), delimiter=",").reshape(-1, 5, 6)
    assert np.isfinite(values).all()
    assert np.all(values[:, 1:, :3] >= -2e-7)
    assert np.all(values[:, 1:, :3] <= 1 + 2e-7)
    # An in-gamut control catches coordinate/column mistakes in this tool.
    inside = np.all((values[:, 0, :3] >= 0) &
                    (values[:, 0, :3] <= 1), axis=1)
    assert np.array_equal(values[inside, 1:, :3],
                          np.repeat(values[inside, :1, :3], 4, axis=1))
    return values


def geometry(data, directory):
    origin = data[0, 3:]
    fig, axes = plt.subplots(1, 2, figsize=(10.8, 4.3))
    fig.subplots_adjust(left=.08, right=.98, bottom=.19, top=.81, wspace=.3)
    fig.suptitle("One Display P3 orange, four sRGB replacements", fontsize=16)
    fig.text(.5, .88, "Source: encoded P3 (1, 0.45, 0) — markers show actual library results",
             ha="center", fontsize=10, color="#475569")
    for ax in axes:
        ax.set_facecolor("#f8fafc")
        ax.grid(alpha=.25)
        ax.tick_params(labelsize=9)
    axes[0].plot([0, origin[1]], [0, origin[2]], "--", color="#94a3b8")
    axes[0].set(xlabel="Oklab a", ylabel="Oklab b",
                xlim=(.105, .17), ylim=(.105, .175))
    axes[1].set(xlabel="Chroma C = sqrt(a² + b²)", ylabel="Oklab lightness L",
                xlim=(.16, .24), ylim=(.688, .738))
    axes[1].axhline(origin[0], color="#94a3b8", linestyle="--")
    for i, ax in enumerate(axes):
        point = (origin[1], origin[2]) if i == 0 else (
            np.hypot(origin[1], origin[2]), origin[0])
        ax.scatter(*point, marker="*", s=140, color="#172033", zorder=5)
        ax.annotate("Source", point, xytext=(-45, 8), textcoords="offset points")
        for j, row in enumerate(data[1:]):
            lab = row[3:]
            target = (lab[1], lab[2]) if i == 0 else (
                np.hypot(lab[1], lab[2]), lab[0])
            ax.annotate("", target, point, arrowprops={
                "arrowstyle": "->", "color": COLORS[j], "alpha": .7})
            style = ({"facecolors": "none", "edgecolors": COLORS[j]}
                     if j == 2 else {"color": COLORS[j]})
            ax.scatter(*target, s=(95 if j == 2 else 40),
                       marker=("o", "s", "o", "+")[j], **style,
                       linewidths=1.7, zorder=6, label=LABELS[j])
    axes[0].legend(loc="lower right", fontsize=8, framealpha=.95)
    fig.text(.5, .045, "EdgeSeeker and Ray Trace nearly coincide here. These are coordinate plots, not gamut-boundary plots.",
             ha="center", fontsize=9, color="#475569")
    save_plot(fig, directory / "orange-coordinates.svg")


def ramps(data, directory):
    fig, axes = plt.subplots(4, 6, figsize=(12, 4.8))
    fig.subplots_adjust(left=.12, right=.99, bottom=.15, top=.8,
                        wspace=.07, hspace=.28)
    fig.suptitle("Continuous Display P3 → sRGB mapping", fontsize=16)
    fig.text(.55, .88, "Each strip increases P3 saturation left → right; lightness is 0.5 in encoded P3 HSL",
             ha="center", fontsize=10, color="#475569")
    for h, name in enumerate(NAMES):
        source = data[h, :, 0, :3]
        out = np.any((source < -2e-7) | (source > 1 + 2e-7), axis=1)
        boundary = np.flatnonzero(out)[0] / (data.shape[1] - 1)
        for m in range(4):
            ax = axes[m, h]
            rgb = encode_srgb(data[h, :, m + 1, :3])
            ax.imshow(np.repeat(rgb[None, :, :], 24, axis=0),
                      extent=(0, 1, 0, 1), aspect="auto", interpolation="nearest")
            ax.axvline(boundary, color="white", linestyle=":", linewidth=1)
            ax.set(xticks=[], yticks=[])
            if m == 0:
                ax.set_title(name, fontsize=10, pad=8)
            if h == 0:
                ax.set_ylabel(LABELS[m], rotation=0, ha="right", va="center",
                              fontsize=10, labelpad=12)
            for spine in ax.spines.values():
                spine.set_visible(False)
    fig.text(.55, .075, "Dotted line: first source sample outside sRGB. Left side stays unchanged in every method.",
             ha="center", fontsize=10)
    fig.text(.55, .028, "sRGB preview; no palette, snap, dithering or RGB101 rounding. The original out-of-gamut colors are not shown.",
             ha="center", fontsize=9, color="#475569")
    save_plot(fig, directory / "continuous-ramps.svg")


def ramp_profiles(data, directory, measurements):
    """Plot undifferentiated coordinates so a lost gradient becomes a plateau.

    All plots use the original floating samples, not colors read back from
    the 8-bit preview. A chroma plateau alone is not proof of identical
    colors; the stored full Oklab triples establish that distinction.
    """
    saturation = np.linspace(0, 1, data.shape[1])
    lab = data[:, :, :, 3:]
    chroma = np.linalg.norm(lab[:, :, :, 1:], axis=-1)
    hue = np.degrees(np.arctan2(lab[:, :, :, 2], lab[:, :, :, 1])) % 360
    hue[chroma < 1e-4] = np.nan
    steps = np.linalg.norm(np.diff(lab, axis=1), axis=-1)
    assert np.all(steps[:, :, 0] > 0)
    styles = ("-", "-.", "-", "--")
    boundaries = []
    for h in range(len(HUES)):
        source = data[h, :, 0, :3]
        outside = np.any((source < -2e-7) | (source > 1 + 2e-7), axis=1)
        boundaries.append(saturation[np.flatnonzero(outside)[0]])

    # Keep all coordinates, including the input, for independent inspection.
    with (measurements / "ramp-coordinates.csv").open("w", newline="") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(["hue_degrees", "saturation"] +
                        [f"{name}_{axis}" for name in ("source", *POLICIES)
                         for axis in ("L", "a", "b")])
        for h, angle in enumerate(HUES):
            for s, coordinates in zip(saturation, lab[h]):
                writer.writerow([angle, format(s, ".17g")] +
                                [format(x, ".17g") for x in coordinates.ravel()])

    def draw(ax, h, values):
        ax.axvspan(boundaries[h], 1, color="#f1f5f9", zorder=0)
        ax.axvline(boundaries[h], color="#64748b", linestyle=":", lw=1)
        ax.plot(saturation, values[:, 0], color="#172033", linestyle=":",
                lw=2, label="Original P3")
        for m in range(4):
            ax.plot(saturation, values[:, m + 1], color=COLORS[m],
                    linestyle=styles[m], lw=1.6, label=LABELS[m])
        ax.set(xlim=(0, 1), xticks=(0, .25, .5, .75, 1))
        ax.grid(alpha=.18)
        ax.tick_params(labelsize=9)

    fig, axes = plt.subplots(2, 3, figsize=(11.6, 6.7), sharex=True, sharey=True)
    fig.subplots_adjust(left=.085, right=.985, bottom=.19, top=.80,
                        wspace=.14, hspace=.34)
    fig.suptitle("When input chroma keeps rising, does output chroma follow?", fontsize=16)
    fig.text(.54, .925, "The same six P3 saturation ramps, shown as coordinates rather than color swatches",
             ha="center", fontsize=10, color="#475569")
    for h, ax in enumerate(axes.flat):
        draw(ax, h, chroma[h])
        ax.set(title=NAMES[h], ylim=(-.006, np.max(chroma) * 1.08))
        if h % 3 == 0:
            ax.set_ylabel("Oklab chroma C", fontsize=10)
        if h >= 3:
            ax.set_xlabel("Position along ramp / P3 saturation", fontsize=9)
        if h in (0, 5):
            flat = np.flatnonzero(steps[h, :, 1] < 1e-12)
            if len(flat):
                index = flat[len(flat) // 2]
                ax.annotate("clip: flat", (saturation[index], chroma[h, index, 1]),
                            xytext=(.68, .08), fontsize=9, color=COLORS[0],
                            arrowprops={"arrowstyle": "->", "color": COLORS[0]})
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", bbox_to_anchor=(.54, .895),
               ncol=5, frameon=False, fontsize=10)
    fig.text(.54, .075, "A horizontal curve means chroma no longer changes; a rising curve retains chroma variation, possibly at a different rate.",
             ha="center", fontsize=9)
    fig.text(.54, .027, "Shading: source outside sRGB. Original floating coordinates, 192 samples per ramp; no differentiation or smoothing.",
             ha="center", fontsize=9, color="#475569")
    save_plot(fig, directory / "ramp-chroma.svg")

    selected = (0, 1, 5)
    # Give the different hue regions equal vertical spans, while retaining
    # their actual degree labels. A shared absolute 0..360 axis hides shifts.
    hue_span = max(np.nanmax(hue[h]) - np.nanmin(hue[h]) for h in selected) * 1.15
    fig, axes = plt.subplots(2, 3, figsize=(11.6, 6.5), sharex=True)
    fig.subplots_adjust(left=.09, right=.985, bottom=.15, top=.80,
                        wspace=.23, hspace=.27)
    fig.suptitle("Chroma is only part of the gradient", fontsize=16)
    fig.text(.54, .925, "Actual lightness and hue along three of the same ramps — these are values, not derivatives",
             ha="center", fontsize=10, color="#475569")
    for col, h in enumerate(selected):
        draw(axes[0, col], h, lab[h, :, :, 0])
        draw(axes[1, col], h, hue[h])
        axes[0, col].set(title=NAMES[h], ylim=(
            np.min(lab[list(selected), :, :, 0]) - .005,
            np.max(lab[list(selected), :, :, 0]) + .005))
        center = (np.nanmin(hue[h]) + np.nanmax(hue[h])) / 2
        axes[1, col].set(ylim=(center - hue_span / 2, center + hue_span / 2),
                         xlabel="Position along ramp / P3 saturation")
        axes[1, col].xaxis.label.set_fontsize(9)
    axes[0, 0].set_ylabel("Oklab lightness L", fontsize=10)
    axes[1, 0].set_ylabel("Oklab hue (degrees)", fontsize=10)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", bbox_to_anchor=(.54, .895),
               ncol=5, frameon=False, fontsize=10)
    fig.text(.54, .072, "Hue panels have different origins but equal degree spans. Hue is omitted near neutral (chroma < 0.0001).",
             ha="center", fontsize=9)
    fig.text(.54, .027, "Encoded P3 HSL lightness is 0.5 throughout; that does not make Oklab lightness or hue constant.",
             ha="center", fontsize=9, color="#475569")
    save_plot(fig, directory / "ramp-lightness-hue.svg")

    return {
        "samples_per_strip": int(data.shape[1]),
        "coordinate_data": "ramp-coordinates.csv",
        "plotted_values": "Oklab L, C=sqrt(a*a+b*b), h=atan2(b,a) in degrees",
        "flat_reporting_threshold_delta_e_ok": 1e-12,
        "hue_neutral_chroma_cutoff": 1e-4,
        "strips": [{
            "name": name, "hue_degrees": HUES[h],
            "first_outside_sample_saturation": float(boundaries[h]),
            "methods": [{
                "policy": policy,
                "last_step_ratio": float(steps[h, -1, m + 1] / steps[h, -1, 0]),
                "flat_intervals": int(np.count_nonzero(steps[h, :, m + 1] < 1e-12)),
                "peak_chroma_saturation": float(saturation[np.argmax(chroma[h, :, m + 1])]),
                "peak_chroma": float(np.max(chroma[h, :, m + 1])),
                "endpoint_chroma": float(chroma[h, -1, m + 1]),
                "endpoint_linear_srgb": data[h, -1, m + 1, :3].tolist(),
            } for m, policy in enumerate(POLICIES)],
        } for h, name in enumerate(NAMES)],
    }


def cusp_geometry(directory):
    """Explain the two sides of a cusp without inventing a measured gamut.

    The coordinates below choose a readable schematic shape only. The
    measured six-ramp results remain in ramp_profiles and its CSV/JSON.
    """
    cusp_l, cusp_c = .62, .85
    lower_l = np.linspace(0, cusp_l, 80)
    upper_l = np.linspace(cusp_l, 1, 80)
    lower_c = cusp_c * lower_l / cusp_l
    upper_c = cusp_c * np.sqrt((1 - upper_l) / (1 - cusp_l))
    fig, ax = plt.subplots(figsize=(10.2, 4.8))
    fig.subplots_adjust(left=.13, right=.57, bottom=.20, top=.79)
    fig.suptitle("At one hue, more lightness can mean less available chroma", fontsize=15)
    fig.text(.5, .89, "A fixed-hue L–C slice: the cusp is the most chromatic point, not the brightest point",
             ha="center", fontsize=10, color="#475569")
    ax.fill_betweenx(np.r_[lower_l, upper_l], 0, np.r_[lower_c, upper_c],
                     color="#f1f5f9")
    ax.plot(lower_c, lower_l, color="#2563eb", lw=2)
    ax.plot(upper_c, upper_l, color="#b45309", lw=2)
    ax.plot([0, cusp_c], [cusp_l, cusp_l], ":", color="#94a3b8")
    ax.scatter([0, cusp_c, 0], [0, cusp_l, 1], color="#172033", s=28, zorder=3)
    ax.annotate("cusp", (cusp_c, cusp_l), xytext=(7, 0),
                textcoords="offset points", va="center", fontsize=11)
    for c, l, color in ((lower_c, lower_l, "#2563eb"), (upper_c, upper_l, "#b45309")):
        ax.annotate("", (c[55], l[55]), (c[28], l[28]),
                    arrowprops={"arrowstyle": "-|>", "color": color, "lw": 2})
    ax.text(.23, .53, "reproducible\ncolors", ha="center", fontsize=10,
            color="#475569")
    ax.set(xlim=(-.025, 1.05), ylim=(-.015, 1.03),
           xticks=(0, cusp_c), xticklabels=("0", "C at cusp"),
           yticks=(0, cusp_l, 1), yticklabels=("0: black", "L at cusp", "1: white"),
           xlabel="Chroma C", ylabel="Oklab lightness L")
    ax.spines[["top", "right"]].set_visible(False)
    fig.text(.62, .67, "Below the cusp", color="#2563eb", weight="bold", fontsize=12)
    fig.text(.62, .58, "Moving toward the cusp allows\nlightness and chroma to rise together.",
             fontsize=10, linespacing=1.6)
    fig.text(.62, .44, "Above the cusp", color="#b45309", weight="bold", fontsize=12)
    fig.text(.62, .35, "Moving toward white increases lightness\nbut reduces the available chroma.",
             fontsize=10, linespacing=1.6)
    fig.text(.62, .22, "Each hue has a different cusp.\nThe same L can be below one and above another.",
             fontsize=10, linespacing=1.6)
    fig.text(.5, .06, "Schematic geometry only: curve shape and cusp position are illustrative, not measured sRGB coordinates.",
             ha="center", fontsize=9, color="#475569")
    save_plot(fig, directory / "cusp-geometry.svg")


def png16(rgb, profile):
    def chunk(name, data):
        return (struct.pack(">I", len(data)) + name + data +
                struct.pack(">I", zlib.crc32(name + data)))
    height, width, _ = rgb.shape
    pixels = np.floor(np.clip(rgb, 0, 1) * 65535 + .5).astype(">u2")
    scanlines = b"".join(b"\0" + row.tobytes() for row in pixels)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(
        ">IIBBBBB", width, height, 16, 2, 0, 0, 0)) +
        chunk(b"iCCP", b"Display P3\0\0" + zlib.compress(profile)) +
        chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))


def pipeline(directory):
    fig, ax = plt.subplots(figsize=(11.5, 8))
    fig.subplots_adjust(left=.015, right=.985, bottom=.025, top=.97)
    ax.set(xlim=(0, 1100), ylim=(790, 0))
    ax.axis("off")

    def box(x, y, w, title, detail, mapped=False):
        ax.add_patch(FancyBboxPatch((x, y), w, 72, boxstyle="round,pad=5",
                     facecolor="#dbeafe" if mapped else "#f1f5f9",
                     edgecolor="#2563eb" if mapped else "#94a3b8"))
        ax.text(x + w / 2, y + 26, title, ha="center", fontsize=12,
                weight="bold", color="#172033")
        ax.text(x + w / 2, y + 53, detail, ha="center", fontsize=9,
                color="#475569")

    def arrow(start, end, dashed=False):
        ax.annotate("", end, start, arrowprops={"arrowstyle": "->",
                    "color": "#64748b", "lw": 1.5,
                    "linestyle": "--" if dashed else "-"})

    ax.text(550, 18, "Where the explicit mapper runs", ha="center",
            fontsize=19, weight="bold", color="#172033")
    box(290, 48, 520, "Loader / CMS → owned float frame",
        "Source interpretation is retained; the destination is separate.")
    box(25, 172, 410, "Choose palette-construction samples",
        "Adaptive-grid: loaded samples; full-frame: processed pixels.")
    box(655, 172, 420, "Geometry + working-space conversion",
        "Crop / resize as selected, then the planned -W representation.")
    box(25, 300, 410, "A  Convert to -X, then map samples",
        "Give clustering achievable destination colors.", True)
    box(655, 300, 420, "B  Map the processed visible pixels",
        "In their current coordinates, before lookup / diffusion.", True)
    box(25, 428, 410, "Construct and refine the palette",
        "Binning, quantizer, merge, snap and cover follow their policies.")
    box(25, 556, 410, "C  Map application palette in -W",
        "Recheck representatives; prepare output colors afterwards.", True)
    box(655, 556, 420, "Lookup + dither in -W",
        "Use B pixels and C palette. Do not remap feedback candidates.")
    box(655, 684, 420, "-U device encoding → RGB101 → SIXEL",
        "Destination transfer and final bounded wire values.")
    arrow((400, 125), (230, 165))
    arrow((700, 125), (865, 165))
    for x in (230, 865):
        arrow((x, 249), (x, 293))
    arrow((230, 377), (230, 421))
    arrow((230, 505), (230, 549))
    arrow((865, 377), (865, 549))
    arrow((440, 592), (650, 592))
    arrow((865, 633), (865, 677))
    # Full-frame sampling uses B, whereas adaptive sampling is independent
    # of the resized application frame. The dashed return makes this
    # dependency visible without suggesting a repeated geometry pass.
    ax.plot([650, 548, 548, 440], [336, 336, 208, 208], "--",
            color="#64748b", lw=1.3)
    arrow((468, 208), (440, 208), True)
    ax.text(553, 258, "full-frame", fontsize=9, ha="left", color="#475569")
    ax.text(230, 685, "Blue boxes: destination gamut mapping\n"
            "Diagram: generated-palette CPU float path\n"
            "Fixed palettes and high-color are described in the text.",
            ha="center", va="top", fontsize=10, linespacing=1.7,
            color="#475569")
    save_plot(fig, directory / "pipeline.svg")


def encode_comparison(build, source, continuous, directory, manifest):
    fig, axes = plt.subplots(3, 4, figsize=(12, 6.5))
    fig.subplots_adjust(left=.07, right=.995, top=.83, bottom=.09,
                        wspace=.045, hspace=.04)
    fig.suptitle("From continuous mapping to 32-color SIXEL", fontsize=16)
    fig.text(.52, .92, "Same 16-bit P3 source; the lower rows include palette construction and wire rounding",
             ha="center", fontsize=10, color="#475569")
    for m, policy in enumerate(POLICIES):
        rgb = np.floor(encode_srgb(continuous[:, m + 1, :3]) * 255 + .5)
        reference = Image.fromarray(rgb.astype(np.uint8).reshape(96, 192, 3))
        info = PngImagePlugin.PngInfo()
        info.add(b"sRGB", b"\0")
        reference.save(directory / f"continuous-{policy}.png", pnginfo=info)
        axes[0, m].imshow(reference, interpolation="nearest")
        axes[0, m].set(xticks=[], yticks=[])
        axes[0, m].set_title(LABELS[m], fontsize=11)
    axes[0, 0].set_ylabel("Continuous", fontsize=10)
    for d, method in enumerate(("none", "fs")):
        for m, policy in enumerate(POLICIES):
            command = [build / "converters/img2sixel", source, *FLAGS,
                       "-d", method, "-U", "gamma:gamut=" + policy]
            sixel = run(command)
            png = run([build / "converters/sixel2png", "-T", "srgb"], input=sixel)
            decoded = Image.open(io.BytesIO(png)).convert("RGB")
            assert decoded.size == (192, 96)
            name = f"encoded-{policy}-{method}.png"
            # Explicit sRGB metadata avoids inheriting any source P3 tag.
            info = PngImagePlugin.PngInfo()
            info.add(b"sRGB", b"\0")
            decoded.save(directory / name, pnginfo=info)
            axes[d + 1, m].imshow(decoded, interpolation="nearest")
            axes[d + 1, m].set(xticks=[], yticks=[])
            if m == 0:
                axes[d + 1, m].set_ylabel("No dither" if d == 0 else "FS + soft",
                                      fontsize=10)
            manifest.setdefault("encodings", []).append({
                "policy": policy, "dither": method, "sixel_bytes": len(sixel),
                "decoded_rgb_sha256": hashlib.sha256(decoded.tobytes()).hexdigest()})
    fig.text(.52, .03, "All previews: sRGB, nearest-neighbor enlargement. Continuous row: no palette. Lower rows: decoded SIXEL.",
             ha="center", fontsize=9, color="#475569")
    save_plot(fig, directory / "encoded-comparison.svg")


ROSE_SOURCE = {
    "title": "Adobe RGB 2017-07-02 17-15-33.jpg (Rose Nostalgie from Tantau)",
    "author": "Berthold Werner",
    "source_page": "https://commons.wikimedia.org/wiki/File:Adobe_RGB_2017-07-02_17-15-33.jpg",
    "original_url": "https://upload.wikimedia.org/wikipedia/commons/3/30/Adobe_RGB_2017-07-02_17-15-33.jpg",
    "original_sha1": "2023983e5a3f9d2828ab0774544baa74a9020e81",
    "original_dimensions": [6016, 4000],
    "license": "CC BY-SA 3.0",
    "license_url": "https://creativecommons.org/licenses/by-sa/3.0/",
    "crop_xyxy": [1320, 800, 2440, 1920],
    "resized_dimensions": [560, 560],
    "preparation": "Crop, then Pillow BOX 2:1 reduction in encoded source RGB; retain embedded ICC; no saturation adjustment",
}


def prepare_rose(original, figures):
    """Keep a small reproducible photograph input instead of a 12 MB JPEG."""
    path = figures / "rose-adobe-rgb.png"
    if original:
        assert hashlib.sha1(original.read_bytes()).hexdigest() == ROSE_SOURCE["original_sha1"]
        with Image.open(original) as image:
            profile = image.info["icc_profile"]
            crop = image.crop(ROSE_SOURCE["crop_xyxy"])
            crop = crop.resize(ROSE_SOURCE["resized_dimensions"], Image.Resampling.BOX)
            # Do not carry unrelated EXIF or write a misleading sRGB tag.
            clean = Image.fromarray(np.asarray(crop))
            clean.save(path, icc_profile=profile)
    elif path != DOC / "figures/rose-adobe-rgb.png":
        path.write_bytes((DOC / "figures/rose-adobe-rgb.png").read_bytes())
    return path


def photograph(probe, source, tmp, figures, measurements):
    """Measure spatially extended channel plateaus before output rounding.

    A flat RGB component is a lost degree of freedom, not proof that all
    visible detail disappeared. Preserve both that distinction and the
    spatial scale; palette runs or single clipped pixels are not regions.
    """
    with Image.open(source) as image:
        rgb = np.asarray(image) / 255.
        profile = image.info["icc_profile"]
    profile_path = tmp / "rose.icc"
    profile_path.write_bytes(profile)
    height, width, _ = rgb.shape
    data = sample(probe, rgb, profile_path).reshape(height, width, 5, 6)
    previews = encode_srgb(data[:, :, 1:, :3])
    info = PngImagePlugin.PngInfo()
    info.add(b"sRGB", b"\0")
    for m in (0, 2):
        Image.fromarray(np.floor(previews[:, :, m] * 255 + .5).astype(np.uint8)).save(
            figures / f"rose-{POLICIES[m]}.png", pnginfo=info)

    scales = []
    for side in (8, 16, 32):
        rows, cols = height // side, width // side
        tiles = data[:rows * side, :cols * side].reshape(rows, side, cols, side, 5, 6)
        ranges = np.ptp(tiles, axis=(1, 3))
        # All pixels in a complete tile must have the same clipped red.
        # Source variation must be appreciable in linear-R coordinates.
        red = (ranges[:, :, 0, 0] >= .025) & (ranges[:, :, 1, 0] <= 1e-12)
        full_rgb = np.all(ranges[:, :, 1, :3] <= 1e-12, axis=-1) & red
        scales.append({"tile_side": side, "complete_tiles": rows * cols,
                       "red_plateau_tiles": int(red.sum()),
                       "also_flat_in_all_rgb_channels": int(full_rgb.sum()),
                       "red_plateau_tile_xy": np.argwhere(red)[:, ::-1].tolist()})
        if side == 16:
            mask = np.repeat(np.repeat(red, side, axis=0), side, axis=1)
            assert mask.shape == (height, width)
            Image.fromarray(mask.astype(np.uint8) * 255).save(figures / "rose-red-plateau-mask.png")

    # The diagnostic background is gray so the detected red-channel loss
    # is not confused with an out-of-gamut or merely high-saturation mask.
    gray = previews[:, :, 0].mean(axis=-1) * .65
    diagnostic = np.repeat(gray[:, :, None], 3, axis=-1)
    diagnostic[mask] = [.98, .76, .08]
    fig, axes = plt.subplots(1, 3, figsize=(11.6, 5.1))
    fig.subplots_adjust(left=.02, right=.98, bottom=.19, top=.81, wspace=.04)
    fig.suptitle("What clipping removes from a rose photograph", fontsize=16)
    fig.text(.5, .90, "Embedded Adobe RGB → sRGB; continuous colors, before palette selection or dithering",
             ha="center", fontsize=10, color="#475569")
    for ax, pixels, title in zip(axes, (previews[:, :, 0], previews[:, :, 2], diagnostic),
                                 ("RGB clip", "EdgeSeeker", "Detected red-channel plateaus")):
        ax.imshow(pixels, interpolation="nearest")
        ax.set(title=title, xticks=[], yticks=[])
        # This strip is measured below; it covers six detected 16×16 tiles.
        ax.add_patch(Rectangle((271.5, 127.5), 48, 32, fill=False, edgecolor="white", lw=2.2))
        ax.add_patch(Rectangle((271.5, 127.5), 48, 32, fill=False, edgecolor="#172033", lw=.9))
    for tx, ty in scales[1]["red_plateau_tile_xy"]:
        axes[2].add_patch(Rectangle((tx * 16 - .5, ty * 16 - .5), 16, 16,
                         fill=False, edgecolor="#172033", lw=.5))
    fig.text(.5, .13, "Yellow cells: 16×16 pixels with source linear-R range ≥ 0.025, but clipped R is constant throughout.",
             ha="center", fontsize=9)
    fig.text(.5, .087, f"{scales[1]['red_plateau_tiles']} cells detected. Green/blue still vary: this is lost red gradation, not complete loss of texture.",
             ha="center", fontsize=9)
    fig.text(.5, .035, "Photo: Berthold Werner / Wikimedia Commons, CC BY-SA 3.0. Cropped, reduced, mapped and annotated.",
             ha="center", fontsize=9, color="#475569")
    save_plot(fig, figures / "rose-comparison.svg")

    # Average over a 32-pixel-high strip and non-overlapping 8-pixel-wide
    # bins. Plot the values themselves: no differentiation or smoothing.
    roi = data[128:160, 272:320]
    line = roi.reshape(32, 6, 8, 5, 6).mean(axis=(0, 2))
    positions = np.arange(6) * 8 + 275.5
    with (measurements / "rose-strip.csv").open("w", newline="") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(["center_x", "policy", "mean_linear_R", "mean_oklab_L", "mean_oklab_a", "mean_oklab_b"])
        for x, point in zip(positions, line):
            for name, row in zip(("source", *POLICIES), point):
                writer.writerow([x, name, *[format(value, ".17g") for value in row[[0, 3, 4, 5]]]])
    fig, axes = plt.subplots(1, 2, figsize=(10.6, 4.3))
    fig.subplots_adjust(left=.085, right=.975, bottom=.24, top=.73, wspace=.24)
    fig.suptitle("A broad patch can lose one gradation while retaining others", fontsize=15)
    fig.text(.5, .90, "Marked 48×32-pixel region; each point averages an 8×32-pixel area, before 8-bit preview rounding",
             ha="center", fontsize=9, color="#475569")
    for ax, index, title in zip(axes, (0, 3), ("Linear sRGB red component", "Oklab lightness L")):
        ax.plot(positions, line[:, 0, index], ":", color="#172033", label="Source", lw=2, marker=".")
        for m in (0, 1, 2):
            ax.plot(positions, line[:, m + 1, index], color=COLORS[m],
                    linestyle=("-", "-.", "--")[m], label=LABELS[m], lw=1.6)
        ax.set(title=title, xlabel="Horizontal position in the 560×560 crop", xticks=positions[::2])
        ax.grid(alpha=.2)
    axes[0].set_ylim(.97, 1.33)
    axes[1].set_ylim(.70, .83)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", bbox_to_anchor=(.5, .855), ncol=4, frameon=False)
    fig.text(.5, .10, "Clip removes the source R variation. EdgeSeeker also approaches R=1, but changes G/B to preserve lightness.",
             ha="center", fontsize=9)
    fig.text(.5, .045, "The lightness curve is not flat: a channel plateau must not be described as identical RGB colors.",
             ha="center", fontsize=9, color="#475569")
    save_plot(fig, figures / "rose-region.svg")
    manifest = dict(ROSE_SOURCE)
    manifest.update({
        "input_png_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "icc_sha256": hashlib.sha256(profile).hexdigest(),
        "destination": "srgb", "stage": "continuous mapping, no palette or dithering",
        "source_red_range_min": .025, "output_red_range_max": 1e-12,
        "threshold_scope": "Illustration-specific channel-plateau detector, not a perceptual threshold or a regression tolerance",
        "scales": scales, "strip_xyxy": [272, 128, 320, 160],
        "strip_bin_dimensions": [8, 32],
        "region_source_linear_R_minmax": [float(roi[:, :, 0, 0].min()), float(roi[:, :, 0, 0].max())],
        "region_clip_linear_R_minmax": [float(roi[:, :, 1, 0].min()), float(roi[:, :, 1, 0].max())],
        "region_mean_oklab_L": {name: float(roi[:, :, m, 3].mean()) for m, name in enumerate(("source", *POLICIES))},
    })
    (measurements / "rose.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT)
    parser.add_argument("--output-dir", type=Path, default=DOC)
    parser.add_argument("--rose-original", type=Path,
                        help="recreate the photo input from the hash-checked original JPEG")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    out = args.output_dir.resolve()
    figures = out / "figures"
    measurements = out / "measurements"
    figures.mkdir(parents=True, exist_ok=True)
    measurements.mkdir(parents=True, exist_ok=True)
    # Ignore user tuning environments so the command line fully specifies
    # the experiment, including output interpretation for the decoder.
    for name in list(os.environ):
        if name.startswith("SIXEL_"):
            del os.environ[name]
    matplotlib.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10,
                                "svg.hashsalt": "libsixel-gamut-guide",
                                "svg.fonttype": "none"})
    with tempfile.TemporaryDirectory(prefix="libsixel-gamut-doc-") as tmp:
        tmp = Path(tmp)
        probe = tmp / "probe"
        run([build / "libtool", "--quiet", "--mode=link",
             *shlex.split(os.environ.get("CC", "cc")), "-static", "-std=c99",
             "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT),
             "-I" + str(build / "include"), "-I" + str(ROOT / "include"),
             ROOT / "tools/gamut_mapping_samples.c", build / "src/libsixel.la",
             "-o", probe])
        rose = prepare_rose(args.rose_original, figures)
        photograph(probe, rose, tmp, figures, measurements)
        orange = sample(probe, np.array([[1., .45, 0.]]))[0]
        geometry(orange, figures)
        pipeline(figures)
        inputs = np.array([[colorsys.hls_to_rgb(h / 360, .5, s)
                            for s in np.linspace(0, 1, 192)] for h in HUES])
        data = sample(probe, inputs).reshape(6, 192, 5, 6)
        ramps(data, figures)
        profiles = ramp_profiles(data, figures, measurements)
        cusp_geometry(figures)
        fixture = ROOT / "tests/data/color-numeric/png-p3-16-rgb.png"
        with Image.open(fixture) as image:
            profile = image.info["icc_profile"]
        profile_path = tmp / "source.icc"
        profile_path.write_bytes(profile)
        source_rgb = np.array([[colorsys.hls_to_rgb(h, l, .95)
                                for h in np.linspace(0, 1, 192)]
                               for l in np.linspace(.85, .15, 96)])
        source = figures / "p3-hue-lightness.png"
        source.write_bytes(png16(source_rgb, profile))
        # Match the quantized source file, not unrounded pattern coordinates.
        source_rgb = np.floor(source_rgb * 65535 + .5) / 65535
        continuous = sample(probe, source_rgb, profile_path)
        manifest = {
            "schema": 1,
            "scope": "Illustrative implementation samples, not independent test expectations",
            "destination": "srgb", "policies": POLICIES,
            "versions": {"numpy": np.__version__, "matplotlib": matplotlib.__version__,
                         "pillow": pillow_version},
            "implementation_sources": {
                name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                for name in ("src/gamut.c", "src/colorspace.c", "src/cms.c",
                             "src/encoder.c", "src/palette.c")},
            "orange_source": {"definition": "named Display P3", "encoded_rgb": [1, .45, 0]},
            "pattern_source": {"width": 192, "height": 96, "bits_per_channel": 16,
                               "icc_sha256": hashlib.sha256(profile).hexdigest(),
                               "png_sha256": hashlib.sha256(source.read_bytes()).hexdigest()},
            "orange": [{"method": name, "linear_srgb": row[:3].tolist(),
                        "oklab": row[3:].tolist(),
                        "encoded_srgb": encode_srgb(row[:3]).tolist() if i else None}
                       for i, (name, row) in enumerate(zip(("source", *POLICIES), orange))],
            "encoding_flags": FLAGS,
            "ramp_profiles": profiles,
        }
        # Record the largest adjacent change so local behavior remains
        # inspectable separately from aggregate quality or quantization.
        # This observation does not define an accepted error budget.
        grid = continuous.reshape(96, 192, 5, 6)
        jumps = np.linalg.norm(np.diff(grid[:, :, 4, 3:], axis=1), axis=-1)
        y, x = np.unravel_index(np.argmax(jumps), jumps.shape)
        manifest["ray_trace_largest_horizontal_step"] = {
            "xy": [int(x), int(y)],
            "source_p3": source_rgb[y, x:x + 2].tolist(),
            "mapped_linear_srgb": grid[y, x:x + 2, 4, :3].tolist(),
            "mapped_delta_e_ok": float(jumps[y, x]),
            "source_delta_e_ok": float(np.linalg.norm(
                grid[y, x + 1, 0, 3:] - grid[y, x, 0, 3:])),
        }
        encode_comparison(build, source, continuous, figures, manifest)
        (measurements / "examples.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Generated gamut mapping figures in {out}")


if __name__ == "__main__":
    main()
