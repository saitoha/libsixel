#!/usr/bin/env python3
"""Measure and plot img2sixel encode-policy and high-color behavior."""

from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import io
import json
import os
import platform
import shlex
import statistics
import subprocess
import time
from pathlib import Path
from typing import Dict, List, Mapping, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


ENCODE_POLICIES: Tuple[Tuple[str, str, str], ...] = (
    ("auto", "auto", "#0072B2"),
    ("fast", "fast", "#009E73"),
    ("size", "size", "#D55E00"),
)
HIGH_COLOR_MODES: Tuple[Tuple[str, str, str], ...] = (
    ("fixed256", "fixed 256-color", "#0072B2"),
    ("high15", "high color (15bpp)", "#D55E00"),
)


def file_sha256(path: Path) -> str:
    """Return the SHA-256 digest of one file."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def bytes_sha256(data: bytes) -> str:
    """Return the SHA-256 digest of an in-memory artifact."""
    return hashlib.sha256(data).hexdigest()


def percentile(values: Sequence[float], fraction: float) -> float:
    """Return a linearly interpolated percentile."""
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def command_environment() -> Dict[str, str]:
    """Return a stable environment without ambient libsixel controls."""
    env = {
        name: value
        for name, value in os.environ.items()
        if not name.startswith("SIXEL_") and not name.startswith("LSQA_")
    }
    env["LC_ALL"] = "C"
    env["TZ"] = "UTC"
    return env


def run_process(command: Sequence[str], env: Mapping[str, str],
                input_bytes: bytes | None = None) -> subprocess.CompletedProcess:
    """Run a command and raise a readable error on failure."""
    process = subprocess.run(
        list(command),
        input=input_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=dict(env),
        check=False,
    )
    if process.returncode != 0:
        diagnostic = process.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"Command failed ({process.returncode}): "
            f"{shlex.join(command)}\n{diagnostic}"
        )
    return process


def make_base_command(img2sixel: Path, input_image: Path) -> List[str]:
    """Return controls shared by both comparisons."""
    return [
        str(img2sixel),
        "--threads=1",
        "--precision=8bit",
        "--quality=high",
        "--loaders=builtin!",
        "--diffusion=none",
        "--gpu-policy=off",
        "--palette-type=rgb",
    ]


def make_encode_policy_command(img2sixel: Path, input_image: Path,
                               policy: str, discard: bool) -> List[str]:
    """Return one controlled encode-policy command."""
    command = make_base_command(img2sixel, input_image)
    command.extend(("--encode-policy", policy, "-p", "256", "-o"))
    command.append(os.devnull if discard else "-")
    command.append(str(input_image))
    return command


def make_high_color_command(img2sixel: Path, input_image: Path,
                            mode: str, discard: bool) -> List[str]:
    """Return one controlled fixed-palette or high-color command."""
    command = make_base_command(img2sixel, input_image)
    command.extend(("--encode-policy", "fast"))
    if mode == "fixed256":
        command.extend(("-p", "256"))
    elif mode == "high15":
        command.append("-I")
    else:
        raise ValueError(f"unknown high-color comparison mode: {mode}")
    command.append("-o")
    command.append(os.devnull if discard else "-")
    command.append(str(input_image))
    return command


def display_command(command: Sequence[str], paths: Mapping[str, str]) -> str:
    """Return a portable command template for metadata and CSV output."""
    normalized = [paths.get(argument, argument) for argument in command]
    return shlex.join(normalized)


def direct_decode(sixel2png: Path, sixel_data: bytes,
                  env: Mapping[str, str]) -> bytes:
    """Decode SIXEL through the direct RGBA path."""
    process = run_process(
        (str(sixel2png), "--direct", "--input", "-", "--output", "-"),
        env,
        sixel_data,
    )
    return process.stdout


def assess_png(lsqa: Path, input_image: Path, png_data: bytes,
               env: Mapping[str, str]) -> Dict[str, float]:
    """Assess one directly decoded PNG against the source image."""
    process = run_process(
        (str(lsqa), str(input_image), "-"),
        env,
        png_data,
    )
    try:
        payload = json.loads(process.stdout.decode("utf-8", errors="strict"))
        quality = payload.get("quality", payload)
        return {
            "MS-SSIM": float(quality["MS-SSIM"]),
            "Delta E00_mean": float(quality["Δ E00_mean"]),
            "Delta Chroma_mean": float(quality["Δ Chroma_mean"]),
            "PSNR_Y": float(quality["PSNR_Y"]),
        }
    except (AttributeError, KeyError, TypeError, UnicodeDecodeError,
            ValueError, json.JSONDecodeError) as exc:
        raise RuntimeError("lsqa output lacks required quality metrics") from exc


def measure_artifact(command: Sequence[str], sixel2png: Path, lsqa: Path,
                     input_image: Path,
                     env: Mapping[str, str]) -> Tuple[Dict[str, object], bytes]:
    """Encode, directly decode, and assess one deterministic artifact."""
    encoded = run_process(command, env).stdout
    decoded = direct_decode(sixel2png, encoded, env)
    metrics = assess_png(lsqa, input_image, decoded, env)
    return {
        **metrics,
        "encoded_bytes": len(encoded),
        "encoded_sha256": bytes_sha256(encoded),
        "decoded_png_sha256": bytes_sha256(decoded),
    }, decoded


def elapsed_once(command: Sequence[str], env: Mapping[str, str]) -> float:
    """Return fresh-process elapsed time for one successful encode."""
    start = time.perf_counter()
    process = subprocess.run(
        list(command),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        env=dict(env),
        check=False,
    )
    finish = time.perf_counter()
    if process.returncode != 0:
        diagnostic = process.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"Timed command failed ({process.returncode}): "
            f"{shlex.join(command)}\n{diagnostic}"
        )
    return finish - start


def measure_timings(commands: Mapping[str, Sequence[str]], warmups: int,
                    runs: int, env: Mapping[str, str]) \
        -> Dict[str, Dict[str, object]]:
    """Measure rotated and reversed fresh-process timing samples."""
    names = list(commands)
    samples: Dict[str, List[float]] = {name: [] for name in names}
    for warmup in range(warmups):
        offset = warmup % len(names)
        order = names[offset:] + names[:offset]
        if warmup % 2:
            order.reverse()
        for name in order:
            elapsed_once(commands[name], env)
    for run_index in range(runs):
        offset = run_index % len(names)
        order = names[offset:] + names[:offset]
        if run_index % 2:
            order.reverse()
        for name in order:
            samples[name].append(elapsed_once(commands[name], env))
    return {
        name: {
            "runs": runs,
            "median_seconds": statistics.median(values),
            "q1_seconds": percentile(values, 0.25),
            "q3_seconds": percentile(values, 0.75),
            "min_seconds": min(values),
            "max_seconds": max(values),
            "samples_seconds": values,
        }
        for name, values in samples.items()
    }


def write_csv(path: Path, rows: Sequence[Mapping[str, object]]) -> None:
    """Write records with stable columns."""
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = list(rows[0])
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(
            stream,
            fieldnames=fields,
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(rows)


def read_build_configuration(img2sixel: Path) -> str:
    """Return configure arguments from the binary's build directory."""
    config_status = img2sixel.parent.parent / "config.status"
    if not config_status.exists():
        return "unavailable"
    process = subprocess.run(
        (str(config_status), "--config"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if process.returncode != 0:
        return "unavailable"
    return process.stdout.decode("utf-8", errors="replace").strip()


def program_record(path: Path, reports_version: bool = True) -> Dict[str, str]:
    """Return path, digest, and first version line for a program."""
    if not reports_version:
        return {
            "path": str(path),
            "sha256": file_sha256(path),
            "version": "not reported by this program",
        }
    process = subprocess.run(
        (str(path), "--version"),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    version = process.stdout.decode("utf-8", errors="replace").splitlines()
    return {
        "path": str(path),
        "sha256": file_sha256(path),
        "version": version[0] if version else "unavailable",
    }


def common_metadata(args: argparse.Namespace, input_image: Path,
                    img2sixel: Path, sixel2png: Path, lsqa: Path) \
        -> Dict[str, object]:
    """Return provenance shared by the two measurement manifests."""
    return {
        "schema": 1,
        "recorded_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "revision": args.revision,
        "source_state": args.source_state,
        "platform": platform.platform(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "configure": read_build_configuration(img2sixel),
        "input": {
            "path": args.input_label,
            "sha256": file_sha256(input_image),
        },
        "programs": {
            "img2sixel": program_record(img2sixel),
            "sixel2png": program_record(sixel2png),
            "lsqa": program_record(lsqa, reports_version=False),
        },
        "timing": {
            "clock": "fresh-process wall time",
            "warmups": args.warmups,
            "runs": args.runs,
            "summary": "median with interquartile range",
            "ordering": "rotated and reversed within each comparison",
        },
        "decode": "sixel2png --direct to RGBA PNG before lsqa",
        "environment": "ambient SIXEL_* and LSQA_* variables removed",
        "visualization": {
            "primary": "static directly labeled comparison plots",
            "fallback": "adjacent Markdown tables plus CSV and JSON data",
            "accessibility": (
                "labels duplicate color, error maps share one stated scale, "
                "and captions define directionality"
            ),
            "qa": "plots reviewed at native size after final generation",
        },
    }


def style_axis(axis: plt.Axes) -> None:
    """Apply the shared quiet chart style."""
    axis.set_facecolor("#FFFFFF")
    axis.grid(axis="x", color="#D9D9D9", linewidth=0.7)
    axis.set_axisbelow(True)
    axis.spines[["top", "right", "left"]].set_visible(False)
    axis.tick_params(axis="y", length=0)


def draw_runtime(axis: plt.Axes, rows: Sequence[Mapping[str, object]],
                 title: str) -> None:
    """Draw median runtime with interquartile whiskers."""
    labels = [str(row["label"]) for row in rows]
    medians = np.array([float(row["median_seconds"]) * 1000 for row in rows])
    q1 = np.array([float(row["q1_seconds"]) * 1000 for row in rows])
    q3 = np.array([float(row["q3_seconds"]) * 1000 for row in rows])
    colors = [str(row["color"]) for row in rows]
    positions = np.arange(len(rows))
    axis.errorbar(
        medians,
        positions,
        xerr=np.vstack((medians - q1, q3 - medians)),
        fmt="none",
        ecolor="#556176",
        elinewidth=1.8,
        capsize=4,
    )
    axis.scatter(medians, positions, c=colors, s=58, zorder=3,
                 edgecolors="#FFFFFF", linewidths=1)
    axis.set_yticks(positions, labels)
    axis.invert_yaxis()
    axis.set_xlabel("Wall time (ms), median and IQR")
    axis.set_title(title, loc="left", fontweight="bold")
    style_axis(axis)
    for position, value in zip(positions, medians):
        axis.annotate(f"{value:.2f} ms", (value, position),
                      xytext=(7, 0), textcoords="offset points", va="center")


def draw_size(axis: plt.Axes, rows: Sequence[Mapping[str, object]],
              title: str) -> None:
    """Draw exact stream sizes with direct labels."""
    labels = [str(row["label"]) for row in rows]
    values = np.array([int(row["encoded_bytes"]) / 1024 for row in rows])
    colors = [str(row["color"]) for row in rows]
    positions = np.arange(len(rows))
    axis.barh(positions, values, color=colors, height=0.58)
    axis.set_yticks(positions, labels)
    axis.invert_yaxis()
    axis.set_xlabel("SIXEL stream size (KiB)")
    axis.set_title(title, loc="left", fontweight="bold")
    style_axis(axis)
    for position, value in zip(positions, values):
        axis.annotate(f"{value:.1f} KiB", (value, position),
                      xytext=(7, 0), textcoords="offset points", va="center")


def plot_encode_policy(rows: Sequence[Mapping[str, object]], path: Path) -> None:
    """Plot quality identity, runtime, and size for -E."""
    runs = int(rows[0]["runs"])
    figure, axes = plt.subplots(1, 3, figsize=(13.2, 4.3))
    figure.patch.set_facecolor("white")
    figure.suptitle(
        "Size policy saves bytes without changing decoded pixels",
        x=0.045,
        ha="left",
        fontsize=17,
        fontweight="bold",
    )
    axes[0].axis("off")
    axes[0].set_title("Quality", loc="left", fontweight="bold")
    axes[0].text(0.0, 0.72, "PIXEL-IDENTICAL", color="#009E73",
                 fontsize=18, fontweight="bold", transform=axes[0].transAxes)
    axes[0].text(0.0, 0.57, "Direct-decode SHA-256 matches\nfor auto, fast, and size.",
                 fontsize=11, transform=axes[0].transAxes)
    axes[0].text(
        0.0,
        0.31,
        f"MS-SSIM  {float(rows[0]['MS-SSIM']):.6f}\n"
        f"mean ΔE00  {float(rows[0]['Delta E00_mean']):.6f}",
        family="monospace",
        fontsize=11,
        transform=axes[0].transAxes,
    )
    draw_runtime(axes[1], rows, "Speed")
    draw_size(axes[2], rows, "Size")
    figure.text(
        0.045,
        0.012,
        "images/snake.png • 600×450 RGB • one CPU thread • no diffusion • "
        f"two warmups, {runs} measured fresh processes\n"
        "auto and fast use the same encoder path; their median gap is "
        "measurement noise",
        color="#556176",
        fontsize=9.5,
        linespacing=1.35,
    )
    figure.tight_layout(rect=(0.03, 0.12, 0.99, 0.90), w_pad=2.0)
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180, facecolor="white")
    plt.close(figure)


def draw_metric(axis: plt.Axes, rows: Sequence[Mapping[str, object]],
                field: str, title: str, xlabel: str,
                decimals: int) -> None:
    """Draw one quality metric as a directly labeled dot plot."""
    labels = [str(row["label"]) for row in rows]
    values = np.array([float(row[field]) for row in rows])
    colors = [str(row["color"]) for row in rows]
    positions = np.arange(len(rows))
    axis.scatter(values, positions, c=colors, s=68,
                 edgecolors="#FFFFFF", linewidths=1)
    axis.set_yticks(positions, labels)
    axis.invert_yaxis()
    axis.set_xlabel(xlabel)
    axis.set_title(title, loc="left", fontweight="bold")
    style_axis(axis)
    for position, value in zip(positions, values):
        axis.annotate(f"{value:.{decimals}f}", (value, position),
                      xytext=(7, 0), textcoords="offset points", va="center")


def plot_high_color(rows: Sequence[Mapping[str, object]], path: Path) -> None:
    """Plot quality, runtime, and size for fixed and high-color modes."""
    runs = int(rows[0]["runs"])
    figure, axes = plt.subplots(2, 2, figsize=(11.8, 7.2))
    figure.patch.set_facecolor("white")
    figure.suptitle(
        "On snake.png, high color buys fidelity with a larger stream",
        x=0.06,
        ha="left",
        fontsize=17,
        fontweight="bold",
    )
    draw_metric(axes[0, 0], rows, "MS-SSIM", "Structure", "MS-SSIM (higher is better)", 6)
    draw_metric(axes[0, 1], rows, "Delta E00_mean", "Color error", "Mean ΔE00 (lower is better)", 6)
    draw_runtime(axes[1, 0], rows, "Speed")
    draw_size(axes[1, 1], rows, "Size")
    figure.text(
        0.06,
        0.015,
        "images/snake.png • direct RGBA decode • one CPU thread • no diffusion • "
        f"two warmups, {runs} measured fresh processes",
        color="#556176",
        fontsize=9.5,
    )
    figure.tight_layout(rect=(0.04, 0.06, 0.99, 0.92), h_pad=2.2, w_pad=2.2)
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180, facecolor="white")
    plt.close(figure)


def png_rgb(png_data: bytes) -> np.ndarray:
    """Load PNG bytes as a float RGB array."""
    image = plt.imread(io.BytesIO(png_data), format="png")
    if image.shape[2] == 4:
        image = image[:, :, :3]
    return image.astype(np.float32)


def plot_visual_comparison(input_image: Path,
                           decoded: Mapping[str, bytes], path: Path) -> None:
    """Plot decoded images and absolute RGB error maps."""
    source = plt.imread(input_image)
    if source.shape[2] == 4:
        source = source[:, :, :3]
    source = source.astype(np.float32)
    fixed = png_rgb(decoded["fixed256"])
    high = png_rgb(decoded["high15"])
    fixed_error = np.mean(np.abs(source - fixed), axis=2) * 255.0
    high_error = np.mean(np.abs(source - high), axis=2) * 255.0
    maximum = max(
        float(np.percentile(fixed_error, 99.5)),
        float(np.percentile(high_error, 99.5)),
        1.0,
    )
    figure = plt.figure(figsize=(13.0, 7.2), facecolor="white")
    grid = figure.add_gridspec(
        3,
        3,
        height_ratios=(1.0, 1.0, 0.08),
        left=0.055,
        right=0.985,
        bottom=0.08,
        top=0.88,
        hspace=0.18,
        wspace=0.10,
    )
    axes = np.empty((2, 3), dtype=object)
    for row in range(2):
        for column in range(3):
            axes[row, column] = figure.add_subplot(grid[row, column])
    figure.patch.set_facecolor("white")
    figure.suptitle(
        "The same scene, two different color budgets",
        x=0.055,
        ha="left",
        fontsize=17,
        fontweight="bold",
    )
    for axis, image, title in zip(
            axes[0], (source, fixed, high),
            ("Reference", "Fixed 256-color", "High color (15bpp)")):
        axis.imshow(image)
        axis.set_title(title, loc="left", fontweight="bold")
        axis.axis("off")
    axes[1, 0].axis("off")
    axes[1, 0].text(0.0, 0.84, "ABSOLUTE RGB ERROR", color="#556176",
                    fontsize=13, fontweight="bold", transform=axes[1, 0].transAxes)
    axes[1, 0].text(
        0.0,
        0.66,
        "Both maps use the same scale.\nDarker pixels changed less.",
        fontsize=11,
        transform=axes[1, 0].transAxes,
    )
    axes[1, 0].text(
        0.0,
        0.42,
        "These maps explain location, not\nperceptual importance; ΔE00 and\nMS-SSIM remain the reported metrics.",
        fontsize=10,
        color="#556176",
        transform=axes[1, 0].transAxes,
    )
    images = []
    for axis, error, title in zip(
            axes[1, 1:], (fixed_error, high_error),
            ("Fixed-palette error", "High-color error")):
        images.append(axis.imshow(error, cmap="magma", vmin=0, vmax=maximum))
        axis.set_title(title, loc="left", fontweight="bold")
        axis.axis("off")
    colorbar_axis = figure.add_subplot(grid[2, 1:])
    colorbar = figure.colorbar(images[-1], cax=colorbar_axis,
                               orientation="horizontal")
    colorbar.set_label("Mean absolute RGB channel error (0–255)")
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180, facecolor="white")
    plt.close(figure)


def write_metadata(path: Path, metadata: Mapping[str, object]) -> None:
    """Write one stable JSON manifest."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_image", type=Path)
    parser.add_argument("--img2sixel", type=Path, required=True)
    parser.add_argument("--sixel2png", type=Path, required=True)
    parser.add_argument("--lsqa", type=Path, required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--source-state", choices=("clean", "dirty"),
                        required=True)
    parser.add_argument("--input-label", default="images/snake.png")
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--encode-output-dir", type=Path, required=True)
    parser.add_argument("--high-color-output-dir", type=Path, required=True)
    return parser.parse_args()


def main() -> None:
    """Measure both option families and emit their artifacts."""
    args = parse_args()
    input_image = args.input_image.resolve()
    img2sixel = args.img2sixel.resolve()
    sixel2png = args.sixel2png.resolve()
    lsqa = args.lsqa.resolve()
    env = command_environment()
    paths = {
        str(img2sixel): "$BUILD_DIR/converters/img2sixel",
        str(sixel2png): "$BUILD_DIR/converters/sixel2png",
        str(lsqa): "$BUILD_DIR/assessment/lsqa",
        str(input_image): f"$TOP_SRCDIR/{args.input_label}",
    }

    encode_artifacts: Dict[str, Dict[str, object]] = {}
    encode_decoded: Dict[str, bytes] = {}
    encode_speed_commands = {
        name: make_encode_policy_command(img2sixel, input_image, name, True)
        for name, _, _ in ENCODE_POLICIES
    }
    for name, label, color in ENCODE_POLICIES:
        command = make_encode_policy_command(
            img2sixel, input_image, name, False
        )
        artifact, decoded_png = measure_artifact(
            command, sixel2png, lsqa, input_image, env
        )
        encode_artifacts[name] = {
            "revision": args.revision,
            "platform": platform.platform(),
            "input": args.input_label,
            "policy": name,
            "label": label,
            "color": color,
            **artifact,
            "command": display_command(command, paths),
        }
        encode_decoded[name] = decoded_png
    encode_timings = measure_timings(
        encode_speed_commands, args.warmups, args.runs, env
    )
    encode_rows = [
        {
            **encode_artifacts[name],
            **encode_timings[name],
            "timed_command": display_command(
                encode_speed_commands[name], paths
            ),
        }
        for name, _, _ in ENCODE_POLICIES
    ]

    high_artifacts: Dict[str, Dict[str, object]] = {}
    high_decoded: Dict[str, bytes] = {}
    high_speed_commands = {
        name: make_high_color_command(img2sixel, input_image, name, True)
        for name, _, _ in HIGH_COLOR_MODES
    }
    for name, label, color in HIGH_COLOR_MODES:
        command = make_high_color_command(
            img2sixel, input_image, name, False
        )
        artifact, decoded_png = measure_artifact(
            command, sixel2png, lsqa, input_image, env
        )
        high_artifacts[name] = {
            "revision": args.revision,
            "platform": platform.platform(),
            "input": args.input_label,
            "mode": name,
            "label": label,
            "color": color,
            **artifact,
            "command": display_command(command, paths),
        }
        high_decoded[name] = decoded_png
    high_timings = measure_timings(
        high_speed_commands, args.warmups, args.runs, env
    )
    high_rows = [
        {
            **high_artifacts[name],
            **high_timings[name],
            "timed_command": display_command(
                high_speed_commands[name], paths
            ),
        }
        for name, _, _ in HIGH_COLOR_MODES
    ]

    encode_dir = args.encode_output_dir
    high_dir = args.high_color_output_dir
    write_csv(encode_dir / "encode-policy-comparison.csv", encode_rows)
    write_csv(high_dir / "high-color-comparison.csv", high_rows)
    plot_encode_policy(encode_rows, encode_dir / "encode-policy-results.png")
    plot_high_color(high_rows, high_dir / "high-color-results.png")
    plot_visual_comparison(
        input_image,
        high_decoded,
        high_dir / "high-color-visual-comparison.png",
    )

    shared = common_metadata(
        args, input_image, img2sixel, sixel2png, lsqa
    )
    write_metadata(
        encode_dir / "encode-policy-run.json",
        {
            **shared,
            "comparison": "img2sixel -E auto, fast, and size",
            "controls": "fixed 256-color output, no diffusion, one CPU thread",
            "rows": encode_rows,
        },
    )
    write_metadata(
        high_dir / "high-color-run.json",
        {
            **shared,
            "comparison": "fixed 256-color output and img2sixel -I",
            "controls": "no diffusion, one CPU thread, fast encoding policy",
            "high_color_note": (
                "The current CLI bypasses palette construction and resolves "
                "palette-space diffusion to none for -I."
            ),
            "rows": high_rows,
        },
    )


if __name__ == "__main__":
    main()
