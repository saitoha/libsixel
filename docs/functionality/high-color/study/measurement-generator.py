#!/usr/bin/env python3
"""Reproduce high-color pixels, color counts, quality, and API timings.

Use a clean, separately built checkout for --build. No installed libsixel is
loaded. --render-only and --verify work without the measured build directory.
"""

from __future__ import annotations

import argparse
import ctypes as ct
import datetime
import hashlib
import io
import json
import os
import platform
import re
import subprocess
import sys
import time
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image


MODES = ("256-none", "256-fs", "high")
LABELS = ("256 colors / no diffusion", "256 colors / FS", "High color (-I)")
COLORS = ("#0072B2", "#657A35", "#D55E00")
OPTIONS = [(".", "8bit"), ("q", "high"), ("G", "off"),
           ("t", "rgb"), ("E", "fast")]
ENV = {k: v for k, v in os.environ.items()
       if not k.startswith(("SIXEL_", "LSQA_"))}
ENV.update(LC_ALL="C", TZ="UTC")


def run(args, data=None, env=ENV):
    return subprocess.run(list(map(str, args)), input=data, env=env,
                          capture_output=True, check=True).stdout


def sha(data):
    return hashlib.sha256(data).hexdigest()


def rgb(path):
    return np.array(Image.open(path).convert("RGB"))


def color_counts(pixels):
    p = pixels.astype(np.uint32)
    packed = (p[..., 0] << 16) | (p[..., 1] << 8) | p[..., 2]
    return np.unique(packed, return_counts=True)


def key_count(pixels):
    p = pixels.astype(np.uint32) >> 3
    return len(np.unique((p[..., 0] << 10) | (p[..., 1] << 5) | p[..., 2]))


def wire_counts(data):
    definitions = re.findall(rb"#(\d+);2;(\d+);(\d+);(\d+)", data)
    return {"definitions": len(definitions),
            "defined_rgb_values": len({tuple(x[1:]) for x in definitions}),
            "defined_slots": len({x[0] for x in definitions}),
            "max_slot": max(int(x[0]) for x in definitions)}


def describe(pixels, mask=None):
    selected = pixels if mask is None else pixels[mask]
    _, counts = color_counts(selected)
    # Histogram bins count RGB colors, not pixels or register definitions.
    bins = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
            2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 4194305]
    band_colors = [len(color_counts(pixels[y:y + 6] if mask is None else
                                    pixels[y:y + 6][mask[y:y + 6]])[0])
                   for y in range(0, len(pixels), 6)]
    return {"rgb_colors": len(counts), "keys_15bit": key_count(selected),
            "pixels": int(counts.sum()),
            "occupancy_bin_edges": bins,
            "occupancy_histogram": np.histogram(counts, bins)[0].tolist(),
            "channel_histograms": [np.bincount(selected[..., c].ravel(),
                                              minlength=256).tolist()
                                   for c in range(3)],
            "band_rgb_colors": band_colors}


class Library:
    """Minimal public API binding; fresh encoder state for every sample."""

    def __init__(self, path):
        self.lib = ct.CDLL(str(path))
        p, i = ct.c_void_p, ct.c_int
        signatures = {
            "sixel_encoder_new": ([ct.POINTER(p), p], i),
            "sixel_encoder_setopt": ([p, i, ct.c_char_p], i),
            "sixel_encoder_encode_bytes": ([p, p, i, i, i, p, i], i),
            "sixel_encoder_unref": ([p], None),
            "sixel_decode_direct": ([p, i, ct.POINTER(p), ct.POINTER(i),
                                     ct.POINTER(i), p], i),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.lib, name)
            function.argtypes, function.restype = args, result
        self.libc = ct.CDLL(None)
        self.libc.free.argtypes = [p]

    @staticmethod
    def check(status):
        if status & 0x1000:
            raise RuntimeError(f"libsixel status {status:#x}")

    def encode(self, pixels, mode, threads, output):
        encoder = ct.c_void_p()
        self.check(self.lib.sixel_encoder_new(ct.byref(encoder), None))
        options = OPTIONS + [("=", str(threads)), ("o", str(output)),
                             ("d", "fs" if mode == "256-fs" else "none"),
                             ("I", "") if mode == "high" else ("p", "256")]
        try:
            for key, value in options:
                self.check(self.lib.sixel_encoder_setopt(
                    encoder, ord(key), value.encode()))
            # High-color API callers must permit mutable input. Copy outside
            # timing so every sample starts with exactly the same RGB bytes.
            data = np.ascontiguousarray(pixels).copy()
            begin = time.perf_counter_ns()
            status = self.lib.sixel_encoder_encode_bytes(
                encoder, data.ctypes.data, data.shape[1], data.shape[0],
                3, None, 0)
            elapsed = (time.perf_counter_ns() - begin) / 1e6
            self.check(status)
        finally:
            self.lib.sixel_encoder_unref(encoder)
        return elapsed

    def decode(self, data):
        source = ct.create_string_buffer(data)
        pixels, width, height = ct.c_void_p(), ct.c_int(), ct.c_int()
        begin = time.perf_counter_ns()
        status = self.lib.sixel_decode_direct(
            source, len(data), ct.byref(pixels), ct.byref(width),
            ct.byref(height), None)
        elapsed = (time.perf_counter_ns() - begin) / 1e6
        self.check(status)
        try:
            result = ct.string_at(pixels, width.value * height.value * 4)
        finally:
            self.libc.free(pixels)
        return elapsed, result, (width.value, height.value)


def benchmark(args):
    """A separate process per budget keeps decoder thread state isolated."""
    os.environ.update(ENV)
    os.environ["SIXEL_THREADS"] = str(args.threads)
    lib = Library(args.library)
    metadata = json.loads((args.output / "results.json").read_text())
    fixtures = {name: rgb(args.output / f"{name}-source.png")
                for name in metadata["fixtures"]}
    streams = {(name, mode): (args.output / f"{name}-{mode}.six").read_bytes()
               for name in fixtures for mode in MODES}
    expected = {(name, mode): np.array(Image.open(
        args.output / f"{name}-{mode}.png").convert("RGBA")).tobytes()
                for name in fixtures for mode in MODES}
    # All fixtures get budgets 1 and 8; intermediate budgets target Full HD.
    configs = [(name, mode) for name in fixtures for mode in MODES
               if args.threads in (1, 8) or name == "snake-fhd"]
    samples = []
    for iteration in range(-2, args.runs):
        order = configs[iteration % len(configs):] + configs[:iteration % len(configs)]
        if iteration % 2:
            order = list(reversed(order))
        for position, (name, mode) in enumerate(order):
            encode_ms = lib.encode(fixtures[name], mode, args.threads, os.devnull)
            decode_ms, pixels, size = lib.decode(streams[name, mode])
            assert pixels == expected[name, mode]
            assert size == Image.open(args.output / f"{name}-{mode}.png").size
            samples.append(dict(fixture=name, mode=mode, threads=args.threads,
                                iteration=iteration, position=position,
                                encode_ms=encode_ms, decode_ms=decode_ms))
    print(json.dumps(samples))


def generate_fixtures(build, output):
    fixtures = {}
    for name, path in (("snake", "images/snake.png"),
                       ("gradient", "images/measurements/palette-pipeline/"
                        "smooth-gradient-600x450.png")):
        source = Image.open(build / path).convert("RGB")
        source.thumbnail((600, 450), Image.Resampling.LANCZOS)
        source.save(output / f"{name}-source.png")
        fixtures[name] = {"origin": path, "origin_sha256": sha((build / path).read_bytes()),
                          "preparation": "RGB conversion; fit within 600x450, Lanczos, no enlargement"}
    ramp = np.tile(np.repeat(np.arange(256, dtype=np.uint8), 3), (96, 1))
    Image.fromarray(np.repeat(ramp[..., None], 3, axis=2)).save(output / "gray-source.png")
    fixtures["gray"] = {"origin": "generated", "preparation": "768x96; RGB=(floor(x/3),)*3"}
    # Visit all keys, then revisit them after eviction with different low bits.
    keys = np.arange(65536, dtype=np.uint32).reshape(256, 256)
    low = (keys // 32768) * 7
    lattice = np.stack([((keys >> shift) & 31) * 8 + low
                        for shift in (10, 5, 0)], axis=2).astype(np.uint8)
    Image.fromarray(lattice).save(output / "revisit-source.png")
    fixtures["revisit"] = {"origin": "generated", "preparation": "256x256; keys 0..32767 twice in row order; RGB=(key channels)*8 + (0 then 7)"}
    Image.open(output / "snake-source.png").resize(
        (1920, 1080), Image.Resampling.LANCZOS).save(output / "snake-fhd-source.png")
    fixtures["snake-fhd"] = {"origin": "snake-source.png", "preparation": "resize to 1920x1080 with Lanczos; timing control, aspect ratio changed"}
    return fixtures


def measure(args):
    build, output = args.build.resolve(), args.output.resolve()
    assert not run(["git", "-C", build, "status", "--porcelain", "-uno"]).strip(), "measured checkout must be clean"
    library = next((build / "src/.libs").glob("libsixel.dylib"), None)
    if library is None:
        library = build / "src/.libs/libsixel.so"
    lib = Library(library)
    output.mkdir(parents=True, exist_ok=True)
    fixtures = generate_fixtures(build, output)
    metadata = {"schema": 1, "revision": run(["git", "-C", build, "rev-parse", "HEAD"]).decode().strip(),
                "recorded_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "platform": platform.platform(), "processor": run(["uname", "-m"]).decode().strip(),
                "logical_cpus": os.cpu_count(), "python": sys.version,
                "numpy": np.__version__, "matplotlib": matplotlib.__version__,
                "pillow": Image.__version__, "runs": args.runs, "warmups": 2,
                "configure": (build / "config.log").read_text().splitlines()[6],
                "library_sha256": sha(library.read_bytes()),
                "script_sha256": sha(Path(__file__).read_bytes()),
                "compiler": run(["cc", "--version"]).decode().splitlines()[0],
                "timing_boundaries": {
                    "encode": "sixel_encoder_encode_bytes: prepared RGB through palette construction/application and SIXEL output to /dev/null; excludes encoder setup, input copy, loader, process startup, encoder destruction",
                    "decode": "sixel_decode_direct: fixed in-memory SIXEL through fresh RGBA allocation and decoding; excludes input copy, result comparison/free, PNG and process startup",
                    "ordering": "rotated and reversed configurations per round; budgets run sequentially in separate processes",
                    "uncertainty": "median and IQR of 11 samples after two warmups; no CPU affinity or exclusive host"},
                "fixtures": fixtures, "rows": [], "timings": []}
    for name, fixture in fixtures.items():
        path = output / f"{name}-source.png"
        source = rgb(path)
        fixture.update(sha256=sha(path.read_bytes()), width=source.shape[1],
                       height=source.shape[0], counts=describe(source))
        for mode in MODES:
            print(f"measure {name} {mode}", flush=True)
            stream_path = output / f"{name}-{mode}.six"
            png_path = output / f"{name}-{mode}.png"
            command = [build / "converters/img2sixel", "--loaders=builtin!", "--threads=1"]
            for key, value in OPTIONS + [("d", "fs" if mode == "256-fs" else "none")]:
                command.extend(["-" + key, value])
            command.extend(["-I"] if mode == "high" else ["-p", "256"])
            command.append(path)
            stream = run(command)
            if mode == "high":
                fs_command = command.copy()
                fs_command[fs_command.index("-d") + 1] = "fs"
                assert run(fs_command) == stream
            stream_path.write_bytes(stream)
            png = run([build / "converters/sixel2png", "-D", "--threads=1", "-i", "-", "-o", "-"], stream)
            png_path.write_bytes(png)
            decoded = np.array(Image.open(io.BytesIO(png)).convert("RGBA"))
            assert decoded.shape[0] >= source.shape[0] and decoded.shape[1] >= source.shape[1]
            visible = decoded[:source.shape[0], :source.shape[1]]
            assessment_png = io.BytesIO()
            composed = np.where(visible[..., 3:] == 255, visible[..., :3], 0)
            Image.fromarray(composed).save(assessment_png, format="PNG")
            quality = json.loads(run([build / "assessment/lsqa", path, "-"], assessment_png.getvalue()))
            quality = quality.get("quality", quality)
            row = dict(fixture=name, mode=mode, bytes=len(stream),
                       stream_sha256=sha(stream), png_sha256=sha(png),
                       rgba_sha256=sha(decoded.tobytes()),
                       ms_ssim=quality["MS-SSIM"], delta_e00=quality["Δ E00_mean"],
                       psnr_y=quality["PSNR_Y"], counts=describe(visible[..., :3], visible[..., 3] == 255),
                       missing_source_pixels=int((visible[..., 3] != 255).sum()),
                       decoded_width=decoded.shape[1], decoded_height=decoded.shape[0],
                       assessment_crop_to_source=decoded.shape[:2] != source.shape[:2],
                       painted_pixels_outside_source=int((decoded[..., 3] > 0).sum() - (visible[..., 3] > 0).sum()),
                       wire=wire_counts(stream),
                       command=[str(x).replace(str(build), "$BUILD").replace(str(output), "$OUT") for x in command])
            metadata["rows"].append(row)
            # Verify the in-memory entry against CLI at the same budget.
            # Cross-budget pixel equality is observed, never assumed.
            for threads in (1, 8):
                scratch = output / "api-verification.six"
                lib.encode(source, mode, threads, scratch)
                encoded = scratch.read_bytes()
                _, pixels, size = lib.decode(encoded)
                assert size == (decoded.shape[1], decoded.shape[0])
                row[f"api_threads{threads}_byte_equal"] = encoded == stream
                if threads == 1:
                    assert pixels == decoded.tobytes(), (name, mode, threads)
                else:
                    threaded_command = ["--threads=8" if x == "--threads=1" else x for x in command]
                    threaded_stream = run(threaded_command)
                    threaded_png = run([build / "converters/sixel2png", "-D", "--threads=1", "-i", "-", "-o", "-"], threaded_stream)
                    threaded_pixels = np.array(Image.open(io.BytesIO(threaded_png)).convert("RGBA"))
                    assert pixels == threaded_pixels.tobytes(), (name, mode, threads)
                    threaded_assessment = io.BytesIO()
                    threaded_visible = threaded_pixels[:source.shape[0], :source.shape[1]]
                    Image.fromarray(np.where(threaded_visible[..., 3:] == 255, threaded_visible[..., :3], 0)).save(threaded_assessment, format="PNG")
                    threaded_quality = json.loads(run([build / "assessment/lsqa", path, "-"], threaded_assessment.getvalue()))
                    threaded_quality = threaded_quality.get("quality", threaded_quality)
                    row["threads8"] = dict(bytes=len(threaded_stream),
                                           ms_ssim=threaded_quality["MS-SSIM"],
                                           delta_e00=threaded_quality["Δ E00_mean"],
                                           changed_pixels=int(np.any(threaded_pixels != decoded, axis=2).sum()),
                                           stream_sha256=sha(threaded_stream),
                                           rgba_sha256=sha(pixels))
                scratch.unlink()
    (output / "results.json").write_text(json.dumps(metadata, indent=2) + "\n")
    for threads in (1, 2, 3, 4, 8):
        print(f"benchmark threads={threads}", flush=True)
        samples = json.loads(run([sys.executable, Path(__file__), "--benchmark", "--library", library,
                                  "--output", output, "--threads", threads, "--runs", args.runs]))
        metadata["timings"].extend(samples)
        (output / "results.json").write_text(json.dumps(metadata, indent=2) + "\n")


def verify(output):
    metadata = json.loads((output / "results.json").read_text())
    for name, fixture in metadata["fixtures"].items():
        path = output / f"{name}-source.png"
        assert sha(path.read_bytes()) == fixture["sha256"]
        assert describe(rgb(path)) == fixture["counts"]
    for row in metadata["rows"]:
        stem = f"{row['fixture']}-{row['mode']}"
        stream = (output / f"{stem}.six").read_bytes()
        png = (output / f"{stem}.png").read_bytes()
        decoded = np.array(Image.open(io.BytesIO(png)).convert("RGBA"))
        assert sha(stream) == row["stream_sha256"] and len(stream) == row["bytes"]
        assert sha(png) == row["png_sha256"]
        assert sha(decoded.tobytes()) == row["rgba_sha256"]
        fixture = metadata["fixtures"][row["fixture"]]
        visible = decoded[:fixture["height"], :fixture["width"]]
        assert int((visible[..., 3] != 255).sum()) == row["missing_source_pixels"]
        assert describe(visible[..., :3], visible[..., 3] == 255) == row["counts"]
        assert wire_counts(stream) == row["wire"]
        assert row["wire"]["max_slot"] <= (254 if row["mode"] == "high" else 255)
        if row["mode"] != "high":
            assert row["counts"]["rgb_colors"] <= 256
    if metadata["timings"]:
        for name in metadata["fixtures"]:
            for mode in MODES:
                for threads in ((1, 2, 3, 4, 8) if name == "snake-fhd" else (1, 8)):
                    samples = [x for x in metadata["timings"] if
                               (x["fixture"], x["mode"], x["threads"]) == (name, mode, threads)]
                    assert sorted(x["iteration"] for x in samples) == list(range(-2, metadata["runs"]))
                    assert all(x["encode_ms"] > 0 and x["decode_ms"] > 0 for x in samples)
    return metadata


def render(output):
    metadata = verify(output)
    rows = {(x["fixture"], x["mode"]): x for x in metadata["rows"]}
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11,
                         "axes.spines.top": False, "axes.spines.right": False})
    for name in ("snake", "gradient", "gray", "revisit"):
        source = rgb(output / f"{name}-source.png")
        images = [rgb(output / f"{name}-{mode}.png")[:source.shape[0], :source.shape[1]] for mode in MODES]
        fig, axes = plt.subplots(2, 3, figsize=(15, 3.8 if name == "gray" else 7), layout="constrained")
        for column, (mode, label, pixels) in enumerate(zip(MODES, LABELS, images)):
            row = rows[name, mode]
            axes[0, column].imshow(pixels, interpolation="nearest")
            axes[0, column].set_title(f"{label}\n{row['counts']['rgb_colors']:,} RGB colors; MS-SSIM {row['ms_ssim']:.4f}")
            error = np.abs(pixels.astype(float) - source).mean(axis=2)
            plotted = axes[1, column].imshow(error, vmin=0, vmax=12, cmap="magma", interpolation="nearest")
            axes[1, column].set_title(f"RGB error | mean ΔE00 {row['delta_e00']:.3f}")
        for axis in axes.flat:
            axis.axis("off")
        fig.colorbar(plotted, ax=list(axes[1]), shrink=.8, label="RGB8 error (0–12, clipped above 12)")
        fig.suptitle(f"{name}: identical raster and display scale; reference linked separately", fontsize=15)
        fig.savefig(output / f"{name}-comparison.png", dpi=120)
        plt.close(fig)
    # Nearest-neighbor crops preserve artifacts instead of smoothing them.
    for name, crop in (("snake", (180, 210, 340, 300)),
                       ("gradient", (180, 180, 420, 252))):
        fig, axes = plt.subplots(2, 3, figsize=(15, 4.6 if name == "gradient" else 6.5), layout="constrained")
        source = rgb(output / f"{name}-source.png")
        left, top, right, bottom = crop
        for column, mode in enumerate(MODES):
            pixels = rgb(output / f"{name}-{mode}.png")
            axes[0, column].imshow(pixels[top:bottom, left:right], interpolation="nearest")
            axes[0, column].set_title(LABELS[column])
            error = (pixels.astype(float) - source).mean(axis=2)
            im = axes[1, column].imshow(error[top:bottom, left:right], vmin=-8, vmax=8,
                                        cmap="RdBu_r", interpolation="nearest")
            for y in range(6, bottom - top, 6):
                axes[1, column].axhline(y - .5, color="#666666", lw=.5, alpha=.5)
            axes[1, column].set_title("Signed RGB error; 6-row guides")
        for axis in axes.flat:
            axis.axis("off")
        fig.colorbar(im, ax=list(axes[1]), label="RGB8 error (−8 to +8)", shrink=.8)
        fig.suptitle(f"{name}: source crop x={left}:{right}, y={top}:{bottom}; nearest-neighbor enlargement")
        fig.savefig(output / f"{name}-detail.png", dpi=120)
        plt.close(fig)
    fig, axes = plt.subplots(1, 2, figsize=(13, 4.5), layout="constrained")
    axes[0].plot(np.repeat(np.arange(256), 3), color="#333333", label="Reference", lw=1.5)
    for mode, label, color in zip(MODES, LABELS, COLORS):
        pixels = rgb(output / f"gray-{mode}.png")
        axes[0].plot(pixels[48, :, 0], color=color, label=label, alpha=.8)
        hist = rows["gray", mode]["counts"]["channel_histograms"][0]
        axes[1].plot(range(256), hist, color=color, label=label)
    axes[0].set(xlabel="Source x (row 48)", ylabel="Red channel (RGB8)", xlim=(0, 767))
    axes[1].set(xlabel="Red channel value (RGB8)", ylabel="Pixels at that value", xlim=(0, 255), ylim=(0, None))
    axes[0].legend(fontsize=9)
    fig.suptitle("Grayscale ramp: transfer steps and exact channel histogram")
    fig.savefig(output / "gray-histogram.png", dpi=120)
    plt.close(fig)
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), layout="constrained")
    names = ("snake", "gradient", "gray", "revisit")
    for ax, name in zip(axes.flat, names):
        for mode, label, color in zip(MODES, LABELS, COLORS):
            counts = rows[name, mode]["counts"]["occupancy_histogram"]
            ax.step(range(len(counts)), counts, where="mid", color=color, label=label)
        ax.set(title=name, xlabel="Pixels per distinct RGB color (log2 bins)", ylabel="Number of RGB colors",
               yscale="symlog", ylim=(0, None), xlim=(-.5, 14.5))
        ax.set_xticks(range(0, 15, 2), [f"{2**x:,}" for x in range(0, 15, 2)])
    axes[0, 0].legend(fontsize=9)
    fig.suptitle("Color occupancy: one observation per distinct decoded RGB color")
    fig.savefig(output / "color-occupancy.png", dpi=120)
    plt.close(fig)
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout="constrained")
    for metric, ax in zip(("encode_ms", "decode_ms"), axes):
        for mode, label, color in zip(MODES, LABELS, COLORS):
            values = [[x[metric] for x in metadata["timings"] if x["fixture"] == "snake-fhd"
                       and x["mode"] == mode and x["threads"] == t and x["iteration"] >= 0]
                      for t in (1, 2, 3, 4, 8)]
            if not all(values):
                continue
            quantiles = np.array([np.quantile(v, [.25, .5, .75]) for v in values])
            ax.plot((1, 2, 3, 4, 8), quantiles[:, 1], marker="o", color=color, label=label)
            ax.fill_between((1, 2, 3, 4, 8), quantiles[:, 0], quantiles[:, 2], color=color, alpha=.15)
        ax.set(title="RGB encode API" if metric == "encode_ms" else "Fixed-stream direct decode API",
               xlabel="Requested threads", ylabel="Elapsed time (ms)", ylim=(0, None), xticks=(1, 2, 3, 4, 8))
        ax.legend(fontsize=9)
    fig.suptitle(f"1920×1080 snake: median and IQR, {metadata['runs']} runs after two warmups")
    fig.savefig(output / "thread-scaling.png", dpi=120)
    plt.close(fig)
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), layout="constrained")
    names = ("snake", "gradient", "gray")
    for ax, metric, title in zip(axes, ("ms_ssim", "delta_e00", "bytes"),
                                 ("MS-SSIM (higher is better)", "Mean ΔE00 (lower is better)", "SIXEL size (KiB)")):
        for i, (mode, label, color) in enumerate(zip(MODES, LABELS, COLORS)):
            values = [rows[name, mode][metric] / (1024 if metric == "bytes" else 1) for name in names]
            ax.bar(np.arange(len(names)) + (i - 1) * .25, values, width=.25, color=color, label=label)
        ax.set(title=title, xticks=range(len(names)), xticklabels=names, ylim=(0, 1.05 if metric == "ms_ssim" else None))
    axes[0].legend(fontsize=8, loc="lower left")
    fig.suptitle("One-thread output: quality and exact stream size; diagnostic revisit excluded")
    fig.savefig(output / "quality-size.png", dpi=120)
    plt.close(fig)
    # Exact lookup table kept next to the plots, without rounded chart values.
    import csv
    with (output / "summary.csv").open("w") as stream:
        writer = csv.writer(stream)
        writer.writerow(("fixture", "mode", "ms_ssim", "delta_e00", "bytes", "rgb_colors", "keys_15bit", "definitions", "defined_rgb_values"))
        for row in metadata["rows"]:
            writer.writerow([row[k] for k in ("fixture", "mode", "ms_ssim", "delta_e00", "bytes")]
                            + [row["counts"][k] for k in ("rgb_colors", "keys_15bit")]
                            + [row["wire"][k] for k in ("definitions", "defined_rgb_values")])
    with (output / "timings.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(metadata["timings"][0]))
        writer.writeheader()
        writer.writerows(metadata["timings"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=11)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--render-only", action="store_true")
    parser.add_argument("--benchmark", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--library", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--threads", type=int, help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.benchmark:
        benchmark(args)
    elif args.verify:
        verify(args.output)
        print("high-color artifacts verified")
    else:
        if not args.render_only:
            if args.build is None or args.runs < 3:
                parser.error("--build and at least three runs are required")
            measure(args)
        render(args.output)


if __name__ == "__main__":
    main()
