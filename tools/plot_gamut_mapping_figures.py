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
from matplotlib.patches import FancyBboxPatch
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT)
    parser.add_argument("--output-dir", type=Path, default=DOC)
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
        orange = sample(probe, np.array([[1., .45, 0.]]))[0]
        geometry(orange, figures)
        pipeline(figures)
        inputs = np.array([[colorsys.hls_to_rgb(h / 360, .5, s)
                            for s in np.linspace(0, 1, 192)] for h in HUES])
        data = sample(probe, inputs).reshape(6, 192, 5, 6)
        ramps(data, figures)
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
        }
        # Preserve a small, inspectable specimen of the visible Ray Trace
        # discontinuity. This is an observation, not an accepted error budget.
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
