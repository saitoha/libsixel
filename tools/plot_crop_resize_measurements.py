#!/usr/bin/env python3
"""Measure resize precision/SIMD policies and generate documentation assets."""

from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import json
import math
import os
import platform
import random
import re
import shlex
import statistics
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D
from PIL import Image, __version__ as PILLOW_VERSION


POLICIES: Tuple[Tuple[str, str, int, str, str, str], ...] = (
    ("preserve", "preserve", 1, "rgb888", "rgb888", "rgb888"),
    ("linear", "linear", 2, "rgb888", "linear-f32", "linear-f32"),
    ("float", "float", 3, "rgb-f32", "linear-f32", "linear-f32"),
)
SIMD_MODES = ("scalar", "auto")
STYLE = {
    "preserve": ("#E69F00", "o", "--"),
    "linear": ("#0072B2", "s", "-"),
    "float": ("#CC79A7", "^", ":"),
}
VISUAL_WIDTH = 300
VISUAL_HEIGHT = 225
ZOOM_X = 0
ZOOM_Y = 65
ZOOM_WIDTH = 120
ZOOM_HEIGHT = 100
ZOOM_SCALE = 4
CHECKER_WIDTH = 600
CHECKER_HEIGHT = 450
BOUNDARY_WIDTH = 513
BOUNDARY_HEIGHT = 192
BOUNDARY_SAMPLE_X = 256
BOUNDARY_ROWS: Tuple[Tuple[str, int, Tuple[int, int, int],
                           Tuple[int, int, int]], ...] = (
    ("black-white", 32, (0, 0, 0), (255, 255, 255)),
    ("red-green", 96, (255, 0, 0), (0, 255, 0)),
    ("blue-yellow", 160, (0, 0, 255), (255, 255, 0)),
)
SPEED_SOURCE_WIDTH = 2400
SPEED_SOURCE_HEIGHT = 1800
SPEED_TARGET_WIDTH = 600
SPEED_TARGET_HEIGHT = 450
PALETTE_COLORS = 256


def file_sha256(path: Path) -> str:
    """Return one file's SHA-256 digest."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def program_record(
    path: Path,
    version_args: Sequence[str] = ("--version",),
) -> Dict[str, str]:
    """Describe one invoked executable."""
    version = "not exposed; identify this program by source revision and SHA-256"
    if version_args:
        proc = subprocess.run(
            [str(path), *version_args],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        lines = proc.stdout.decode("utf-8", errors="replace").splitlines()
        if lines:
            version = lines[0]
    return {
        "path": str(path),
        "sha256": file_sha256(path),
        "version": version,
    }


def clean_environment() -> Dict[str, str]:
    """Return a deterministic environment without inherited libsixel policy."""
    environment = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith("SIXEL_") and not key.startswith("LSQA_")
    }
    environment["LC_ALL"] = "C"
    environment["TZ"] = "UTC"
    return environment


def run_checked(
    command: Sequence[str],
    environment: Dict[str, str],
    stdout: int = subprocess.DEVNULL,
) -> bytes:
    """Run one command and return captured stdout when requested."""
    proc = subprocess.run(
        list(command),
        stdout=stdout,
        stderr=subprocess.PIPE,
        env=environment,
        check=False,
    )
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace")
        raise RuntimeError(
            f"command failed ({proc.returncode}): "
            f"{shlex.join(command)}\n{diagnostic}"
        )
    if proc.stdout is None:
        return b""
    return proc.stdout


def command_text(
    command: Sequence[str],
    replacements: Iterable[Tuple[str, str]],
) -> str:
    """Return a relocatable shell representation of one command."""
    replaced = []
    replacement_list = tuple(replacements)
    for token in command:
        replacement = token
        for source, target in replacement_list:
            if token == source:
                replacement = target
                break
        replaced.append(replacement)
    return shlex.join(replaced)


def encoder_command(
    img2sixel: Path,
    input_path: Path,
    output_path: Path,
    policy: str,
    simd: str,
    width: int,
    height: int,
    resampling: str = "bilinear",
) -> List[str]:
    """Build one fully controlled end-to-end encoder command."""
    return [
        str(img2sixel),
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        "-Fnone",
        "-aoff",
        "-Xgamma",
        "-Wgamma",
        "-Ugamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "--encode-policy=fast",
        f"-p{PALETTE_COLORS}",
        f"-w{width}",
        f"-h{height}",
        f"-r{resampling}",
        f"-j{simd}:resize_precision={policy}",
        "-o",
        str(output_path),
        str(input_path),
    ]


def run_preflight(
    command: Sequence[str],
    environment: Dict[str, str],
) -> Dict[str, object]:
    """Parse effective formats and SIMD resolution from verbose diagnostics."""
    preflight = list(command)
    preflight[-2] = os.devnull
    preflight.insert(-1, "-v")
    trace_environment = environment.copy()
    trace_environment["SIXEL_TRACE_TOPIC"] = "runtime_contract"
    proc = subprocess.run(
        preflight,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        env=trace_environment,
        check=False,
    )
    diagnostic = proc.stderr.decode("utf-8", errors="replace")
    if proc.returncode != 0:
        raise RuntimeError(
            f"preflight failed ({proc.returncode}):\n{diagnostic}"
        )
    formats = re.findall(
        r"formats: source=(\S+) work=(\S+) scale_out=(\S+)",
        diagnostic,
    )
    resize = re.findall(r"resize: mode=(\d+) input=(\S+)", diagnostic)
    simd = re.findall(
        r"simd_cap=(\d+)\|native=(\d+)\|effective=(\d+)",
        diagnostic,
    )
    if len(formats) != 1 or len(resize) != 1 or len(simd) != 1:
        raise RuntimeError(
            "preflight did not expose one format, resize, and SIMD record:\n"
            + diagnostic
        )
    return {
        "source_format": formats[0][0],
        "work_format": formats[0][1],
        "scale_output_format": formats[0][2],
        "resize_mode": int(resize[0][0]),
        "resize_input_format": resize[0][1],
        "simd_cap": int(simd[0][0]),
        "simd_native": int(simd[0][1]),
        "simd_effective": int(simd[0][2]),
    }


def write_checker(path: Path) -> None:
    """Write a one-pixel black/white checker as binary PPM."""
    pixels = bytearray(CHECKER_WIDTH * CHECKER_HEIGHT * 3)
    offset = 0
    for y in range(CHECKER_HEIGHT):
        for x in range(CHECKER_WIDTH):
            value = 255 if (x + y) & 1 else 0
            pixels[offset:offset + 3] = bytes((value, value, value))
            offset += 3
    path.write_bytes(
        f"P6\n{CHECKER_WIDTH} {CHECKER_HEIGHT}\n255\n".encode("ascii")
        + pixels
    )


def write_checker_reference(path: Path) -> None:
    """Write the analytical sRGB encoding of a 50% linear-light gray."""
    linear = 0.5
    srgb = 1.055 * linear ** (1.0 / 2.4) - 0.055
    value = round(255.0 * srgb)
    pixels = bytes((value, value, value)) * (
        VISUAL_WIDTH * VISUAL_HEIGHT
    )
    path.write_bytes(
        f"P6\n{VISUAL_WIDTH} {VISUAL_HEIGHT}\n255\n".encode("ascii")
        + pixels
    )


def write_boundary_fixture(path: Path) -> None:
    """Write three two-color boundaries for bilinear mixing inspection."""
    image = Image.new("RGB", (8, BOUNDARY_HEIGHT))
    pixels = image.load()
    for row_index, (_, _, left, right) in enumerate(BOUNDARY_ROWS):
        y_start = row_index * 64
        y_end = y_start + 64
        for y in range(y_start, y_end):
            for x in range(8):
                pixels[x, y] = left if x < 4 else right
    image.save(path)


def write_speed_fixture(input_path: Path, output_path: Path) -> None:
    """Upscale the tracked natural image without inventing new samples."""
    with Image.open(input_path) as source:
        image = source.convert("RGB").resize(
            (SPEED_SOURCE_WIDTH, SPEED_SOURCE_HEIGHT),
            Image.Resampling.NEAREST,
        )
    image.save(output_path)


def encode_and_decode(
    command: Sequence[str],
    sixel2png: Path,
    png_path: Path,
    environment: Dict[str, str],
) -> None:
    """Encode one SIXEL stream and decode that exact stream to PNG."""
    run_checked(command, environment)
    sixel_path = Path(command[-2])
    run_checked(
        [
            str(sixel2png),
            "-jscalar",
            "-i",
            str(sixel_path),
            "-o",
            str(png_path),
        ],
        environment,
    )


def parse_quality(payload: bytes) -> Dict[str, float]:
    """Parse the JSON quality object emitted by lsqa."""
    document = json.loads(payload.decode("utf-8"))
    quality = document.get("quality")
    if not isinstance(quality, dict):
        raise RuntimeError("lsqa output has no quality object")
    return {str(key): float(value) for key, value in quality.items()}


def assess(
    lsqa: Path,
    reference: Path,
    target: Path,
    environment: Dict[str, str],
) -> Tuple[Dict[str, float], List[str]]:
    """Compare two images in float32 Oklab."""
    command = [
        str(lsqa),
        "-Woklab",
        "-Pfloat32",
        str(reference),
        str(target),
    ]
    payload = run_checked(command, environment, subprocess.PIPE)
    return parse_quality(payload), command


def copy_png(source: Path, target: Path) -> None:
    """Copy one decoded PNG through Pillow to keep output deterministic."""
    with Image.open(source) as image:
        image.save(target)


def write_zoom(source: Path, target: Path) -> None:
    """Write one exact crop enlarged with nearest-neighbor display scaling."""
    box = (
        ZOOM_X,
        ZOOM_Y,
        ZOOM_X + ZOOM_WIDTH,
        ZOOM_Y + ZOOM_HEIGHT,
    )
    with Image.open(source) as image:
        crop = image.convert("RGB").crop(box)
        crop = crop.resize(
            (ZOOM_WIDTH * ZOOM_SCALE, ZOOM_HEIGHT * ZOOM_SCALE),
            Image.Resampling.NEAREST,
        )
        crop.save(target)


def write_csv(path: Path, rows: Sequence[Dict[str, object]],
              fieldnames: Sequence[str]) -> None:
    """Write one stable CSV with explicit column order."""
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def percentile(values: Sequence[float], fraction: float) -> float:
    """Return a linearly interpolated percentile."""
    ordered = sorted(values)
    if not ordered:
        raise ValueError("cannot compute percentile of empty values")
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def policy_records() -> List[Dict[str, object]]:
    """Return policy metadata in presentation order."""
    return [
        {
            "policy": policy,
            "label": label,
            "resize_mode": mode,
            "work_format": work,
            "scale_output_format": scale_output,
            "resize_input_format": resize_input,
        }
        for policy, label, mode, work, scale_output, resize_input in POLICIES
    ]


def validate_preflight(
    policy: Dict[str, object],
    simd: str,
    preflight: Dict[str, object],
) -> None:
    """Reject a run that did not use the intended planner and SIMD path."""
    expected = (
        "rgb888",
        policy["work_format"],
        policy["scale_output_format"],
        policy["resize_mode"],
        policy["resize_input_format"],
    )
    observed = (
        preflight["source_format"],
        preflight["work_format"],
        preflight["scale_output_format"],
        preflight["resize_mode"],
        preflight["resize_input_format"],
    )
    if observed != expected:
        raise RuntimeError(
            f"effective resize path changed for {policy['policy']}: "
            f"{observed} != {expected}"
        )
    effective = int(preflight["simd_effective"])
    native = int(preflight["simd_native"])
    if simd == "scalar" and effective != 0:
        raise RuntimeError("scalar SIMD request did not resolve to scalar")
    if simd == "auto" and effective != native:
        raise RuntimeError("auto SIMD request did not resolve to native level")


def measure_policy_outputs(
    img2sixel: Path,
    sixel2png: Path,
    lsqa: Path,
    input_path: Path,
    temporary: Path,
    output_directory: Path,
    environment: Dict[str, str],
) -> Tuple[List[Dict[str, object]], List[Dict[str, object]],
           Dict[str, Dict[str, object]]]:
    """Measure natural/checker quality, stream size, and effective paths."""
    natural_outputs: Dict[Tuple[str, str], Tuple[Path, Path, List[str]]] = {}
    checker_outputs: Dict[Tuple[str, str], Tuple[Path, Path, List[str]]] = {}
    preflight_records: Dict[str, Dict[str, object]] = {}
    checker_input = temporary / "checker.ppm"
    checker_reference = temporary / "checker-reference.ppm"
    write_checker(checker_input)
    write_checker_reference(checker_reference)

    for policy in policy_records():
        policy_name = str(policy["policy"])
        for simd in SIMD_MODES:
            case = f"{policy_name}-{simd}"
            natural_sixel = temporary / f"natural-{case}.six"
            natural_png = temporary / f"natural-{case}.png"
            natural_command = encoder_command(
                img2sixel,
                input_path,
                natural_sixel,
                policy_name,
                simd,
                VISUAL_WIDTH,
                VISUAL_HEIGHT,
            )
            preflight_command = encoder_command(
                img2sixel,
                input_path,
                temporary / f"preflight-{case}.six",
                policy_name,
                simd,
                VISUAL_WIDTH,
                VISUAL_HEIGHT,
            )
            preflight = run_preflight(preflight_command, environment)
            validate_preflight(policy, simd, preflight)
            preflight_records[case] = preflight
            encode_and_decode(
                natural_command,
                sixel2png,
                natural_png,
                environment,
            )
            natural_outputs[(policy_name, simd)] = (
                natural_sixel,
                natural_png,
                natural_command,
            )

            checker_sixel = temporary / f"checker-{case}.six"
            checker_png = temporary / f"checker-{case}.png"
            checker_command = encoder_command(
                img2sixel,
                checker_input,
                checker_sixel,
                policy_name,
                simd,
                VISUAL_WIDTH,
                VISUAL_HEIGHT,
            )
            encode_and_decode(
                checker_command,
                sixel2png,
                checker_png,
                environment,
            )
            checker_outputs[(policy_name, simd)] = (
                checker_sixel,
                checker_png,
                checker_command,
            )

    natural_reference = natural_outputs[("linear", "scalar")][1]
    quality_rows: List[Dict[str, object]] = []
    size_rows: List[Dict[str, object]] = []
    for fixture, reference, outputs, input_token in (
        ("natural", natural_reference, natural_outputs, "{input}"),
        ("checker", checker_reference, checker_outputs, "{checker_input}"),
    ):
        for policy in policy_records():
            policy_name = str(policy["policy"])
            for simd in SIMD_MODES:
                sixel_path, png_path, command = outputs[(policy_name, simd)]
                metrics, assessment_command = assess(
                    lsqa, reference, png_path, environment
                )
                replacements = (
                    (str(img2sixel), "{img2sixel}"),
                    (str(input_path), "{input}"),
                    (str(checker_input), "{checker_input}"),
                    (str(sixel_path), "{output}"),
                )
                quality_rows.append(
                    {
                        "fixture": fixture,
                        "policy": policy_name,
                        "simd_requested": simd,
                        "simd_effective": preflight_records[
                            f"{policy_name}-{simd}"
                        ]["simd_effective"],
                        "ms_ssim": f"{metrics['MS-SSIM']:.6f}",
                        "delta_e00_mean": f"{metrics['Δ E00_mean']:.6f}",
                        "delta_chroma_mean": (
                            f"{metrics['Δ Chroma_mean']:.6f}"
                        ),
                        "gmsd": f"{metrics['GMSD']:.6f}",
                        "psnr_y": f"{metrics['PSNR_Y']:.6f}",
                        "output_sha256": file_sha256(sixel_path),
                        "decoded_png_sha256": file_sha256(png_path),
                        "command": command_text(command, replacements),
                        "assessment_command": command_text(
                            assessment_command,
                            (
                                (str(lsqa), "{lsqa}"),
                                (
                                    str(reference),
                                    "{natural_reference}"
                                    if fixture == "natural"
                                    else "{checker_reference}",
                                ),
                                (str(png_path), "{output_png}"),
                            ),
                        ),
                        "input_token": input_token,
                    }
                )
                if fixture == "natural":
                    size_rows.append(
                        {
                            "policy": policy_name,
                            "simd_requested": simd,
                            "simd_effective": preflight_records[
                                f"{policy_name}-{simd}"
                            ]["simd_effective"],
                            "sixel_bytes": sixel_path.stat().st_size,
                            "output_sha256": file_sha256(sixel_path),
                            "command": command_text(command, replacements),
                        }
                    )

    visual_sources = {
        "reference-linear-scalar": natural_outputs[("linear", "scalar")][1],
        "preserve-auto": natural_outputs[("preserve", "auto")][1],
        "linear-auto": natural_outputs[("linear", "auto")][1],
        "float-auto": natural_outputs[("float", "auto")][1],
    }
    for name, source in visual_sources.items():
        copy_png(
            source,
            output_directory / f"crop-resize-visual-{name}.png",
        )
        write_zoom(
            source,
            output_directory / f"crop-resize-visual-{name}-zoom.png",
        )
    return quality_rows, size_rows, preflight_records


def measure_boundary_outputs(
    img2sixel: Path,
    sixel2png: Path,
    temporary: Path,
    output_directory: Path,
    environment: Dict[str, str],
) -> List[Dict[str, object]]:
    """Generate and sample the end-to-end few-color boundary comparison."""
    input_path = temporary / "boundary-input.png"
    write_boundary_fixture(input_path)
    copy_png(
        input_path,
        output_directory / "crop-resize-boundary-input.png",
    )
    rows: List[Dict[str, object]] = []
    for policy in policy_records():
        policy_name = str(policy["policy"])
        sixel_path = temporary / f"boundary-{policy_name}.six"
        png_path = temporary / f"boundary-{policy_name}.png"
        command = encoder_command(
            img2sixel,
            input_path,
            sixel_path,
            policy_name,
            "auto",
            BOUNDARY_WIDTH,
            BOUNDARY_HEIGHT,
        )
        encode_and_decode(command, sixel2png, png_path, environment)
        target = output_directory / f"crop-resize-boundary-{policy_name}.png"
        copy_png(png_path, target)
        with Image.open(png_path) as image:
            rgb = image.convert("RGB")
            for boundary, y, left, right in BOUNDARY_ROWS:
                sample = rgb.getpixel((BOUNDARY_SAMPLE_X, y))
                rows.append(
                    {
                        "boundary": boundary,
                        "policy": policy_name,
                        "simd_requested": "auto",
                        "sample_x": BOUNDARY_SAMPLE_X,
                        "sample_y": y,
                        "source_left_rgb": "/".join(map(str, left)),
                        "source_right_rgb": "/".join(map(str, right)),
                        "output_midpoint_rgb": "/".join(map(str, sample)),
                        "sixel_bytes": sixel_path.stat().st_size,
                        "output_sha256": file_sha256(sixel_path),
                        "command": command_text(
                            command,
                            (
                                (str(img2sixel), "{img2sixel}"),
                                (str(input_path), "{boundary_input}"),
                                (str(sixel_path), "{output}"),
                            ),
                        ),
                    }
                )
    return rows


def measure_speed(
    img2sixel: Path,
    input_path: Path,
    temporary: Path,
    environment: Dict[str, str],
    warmups: int,
    runs: int,
    preflight: Dict[str, Dict[str, object]],
) -> List[Dict[str, object]]:
    """Measure end-to-end runtime with interleaved case order."""
    speed_input = temporary / "speed-input.png"
    write_speed_fixture(input_path, speed_input)
    cases: List[Tuple[str, str, List[str]]] = []
    for policy in policy_records():
        policy_name = str(policy["policy"])
        for simd in SIMD_MODES:
            command = encoder_command(
                img2sixel,
                speed_input,
                Path(os.devnull),
                policy_name,
                simd,
                SPEED_TARGET_WIDTH,
                SPEED_TARGET_HEIGHT,
            )
            cases.append((policy_name, simd, command))

    for _ in range(warmups):
        for _, _, command in cases:
            run_checked(command, environment)

    samples: Dict[Tuple[str, str], List[float]] = {
        (policy, simd): [] for policy, simd, _ in cases
    }
    rng = random.Random(1)
    for _ in range(runs):
        order = list(cases)
        rng.shuffle(order)
        for policy, simd, command in order:
            start = time.perf_counter_ns()
            run_checked(command, environment)
            elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
            samples[(policy, simd)].append(elapsed_ms)

    rows: List[Dict[str, object]] = []
    for policy, simd, command in cases:
        values = samples[(policy, simd)]
        rows.append(
            {
                "policy": policy,
                "simd_requested": simd,
                "simd_effective": preflight[
                    f"{policy}-{simd}"
                ]["simd_effective"],
                "median_ms": f"{statistics.median(values):.6f}",
                "q1_ms": f"{percentile(values, 0.25):.6f}",
                "q3_ms": f"{percentile(values, 0.75):.6f}",
                "minimum_ms": f"{min(values):.6f}",
                "maximum_ms": f"{max(values):.6f}",
                "samples_ms": ";".join(f"{value:.6f}" for value in values),
                "command": command_text(
                    command,
                    (
                        (str(img2sixel), "{img2sixel}"),
                        (str(speed_input), "{speed_input}"),
                        (os.devnull, "{output}"),
                    ),
                ),
            }
        )
    return rows


def configure_plot() -> None:
    """Apply a restrained documentation plot style."""
    plt.rcParams.update(
        {
            "font.size": 11,
            "axes.titlesize": 15,
            "axes.labelsize": 12,
            "axes.edgecolor": "#5E6672",
            "axes.linewidth": 0.8,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "grid.color": "#D7DCE2",
            "grid.linewidth": 0.7,
            "figure.facecolor": "white",
            "axes.facecolor": "white",
        }
    )


def finish_plot(fig: plt.Figure, path: Path) -> None:
    """Write one PNG plot with stable output metadata."""
    fig.savefig(
        path,
        dpi=180,
        bbox_inches="tight",
        facecolor="white",
        metadata={"Software": "libsixel crop/resize measurement workflow"},
    )
    plt.close(fig)


def plot_quality(
    rows: Sequence[Dict[str, object]],
    fixture: str,
    metric: str,
    ylabel: str,
    title: str,
    output_path: Path,
    higher_is_better: bool,
) -> None:
    """Plot one metric and fixture without combining unrelated evidence."""
    fig, ax = plt.subplots(figsize=(8.4, 4.6))
    x_positions = np.arange(len(POLICIES), dtype=float)
    offsets = {"scalar": -0.10, "auto": 0.10}
    values: List[float] = []
    for policy_index, policy_record in enumerate(POLICIES):
        policy = policy_record[0]
        color, marker, _ = STYLE[policy]
        for simd in SIMD_MODES:
            row = next(
                item for item in rows
                if item["fixture"] == fixture
                and item["policy"] == policy
                and item["simd_requested"] == simd
            )
            value = float(row[metric])
            values.append(value)
            face = "white" if simd == "scalar" else color
            ax.scatter(
                policy_index + offsets[simd],
                value,
                s=92,
                marker=marker if simd == "auto" else "o",
                facecolor=face,
                edgecolor=color,
                linewidth=2,
                zorder=3,
            )
            ax.annotate(
                f"{value:.6f}" if metric == "ms_ssim" else f"{value:.3f}",
                (policy_index + offsets[simd], value),
                xytext=(-4 if simd == "scalar" else 4, 9),
                textcoords="offset points",
                ha="right" if simd == "scalar" else "left",
                color=color,
                fontsize=9,
            )

    if higher_is_better:
        minimum = min(values)
        margin = max((1.0 - minimum) * 0.18, 0.0004)
        ax.set_ylim(max(0.0, minimum - margin), 1.0005)
        ax.text(
            0.01,
            0.02,
            "Expanded y-axis; higher is better",
            transform=ax.transAxes,
            color="#5E6672",
            fontsize=9,
        )
    else:
        maximum = max(values)
        ax.set_ylim(0.0, max(0.001, maximum * 1.20))
        ax.text(
            0.01,
            0.94,
            "Lower is better",
            transform=ax.transAxes,
            color="#5E6672",
            fontsize=9,
        )
    ax.set_xticks(x_positions, [record[1] for record in POLICIES])
    ax.set_xlim(-0.35, len(POLICIES) - 1 + 0.35)
    ax.set_ylabel(ylabel)
    ax.set_title(title, loc="left", fontweight="bold")
    ax.grid(axis="y")
    handles = [
        Line2D(
            [0], [0], marker="o", linestyle="none", markerfacecolor="white",
            markeredgecolor="#20242A", markeredgewidth=1.8,
            markersize=8, label="scalar",
        ),
        Line2D(
            [0], [0], marker="s", linestyle="none",
            markerfacecolor="#5E6672", markeredgecolor="#5E6672",
            markersize=8, label="auto (native ceiling)",
        ),
    ]
    ax.legend(handles=handles, frameon=False, loc="best")
    finish_plot(fig, output_path)


def plot_speed(rows: Sequence[Dict[str, object]], output_path: Path) -> None:
    """Plot median runtime and interquartile range."""
    fig, ax = plt.subplots(figsize=(8.4, 4.8))
    offsets = {"scalar": -0.10, "auto": 0.10}
    for policy_index, policy_record in enumerate(POLICIES):
        policy = policy_record[0]
        color, _, _ = STYLE[policy]
        for simd in SIMD_MODES:
            row = next(
                item for item in rows
                if item["policy"] == policy
                and item["simd_requested"] == simd
            )
            median = float(row["median_ms"])
            q1 = float(row["q1_ms"])
            q3 = float(row["q3_ms"])
            x = policy_index + offsets[simd]
            face = "white" if simd == "scalar" else color
            marker = "o" if simd == "scalar" else "s"
            ax.errorbar(
                x,
                median,
                yerr=[[median - q1], [q3 - median]],
                fmt=marker,
                markersize=9,
                markerfacecolor=face,
                markeredgecolor=color,
                markeredgewidth=2,
                ecolor=color,
                elinewidth=1.5,
                capsize=5,
                zorder=3,
            )
            ax.annotate(
                f"{median:.1f} ms",
                (x, median),
                xytext=(-4 if simd == "scalar" else 4, 10),
                textcoords="offset points",
                ha="right" if simd == "scalar" else "left",
                color=color,
                fontsize=9,
            )
    ax.set_xticks(np.arange(len(POLICIES)), [record[1] for record in POLICIES])
    ax.set_xlim(-0.35, len(POLICIES) - 1 + 0.35)
    ax.set_ylabel("Median wall time (ms)")
    ax.set_title(
        "End-to-end resize runtime, 2400 x 1800 -> 600 x 450",
        loc="left",
        fontweight="bold",
    )
    ax.text(
        0.01,
        0.94,
        "Error bars show the interquartile range; lower is better",
        transform=ax.transAxes,
        color="#5E6672",
        fontsize=9,
    )
    ax.grid(axis="y")
    ax.legend(
        handles=[
            Line2D(
                [0], [0], marker="o", linestyle="none",
                markerfacecolor="white", markeredgecolor="#20242A",
                markeredgewidth=1.8, markersize=8, label="scalar",
            ),
            Line2D(
                [0], [0], marker="s", linestyle="none",
                markerfacecolor="#5E6672", markeredgecolor="#5E6672",
                markersize=8, label="auto (native ceiling)",
            ),
        ],
        frameon=False,
        loc="best",
    )
    finish_plot(fig, output_path)


def plot_size(rows: Sequence[Dict[str, object]], output_path: Path) -> None:
    """Plot exact SIXEL stream sizes."""
    fig, ax = plt.subplots(figsize=(8.4, 4.8))
    offsets = {"scalar": -0.10, "auto": 0.10}
    for policy_index, policy_record in enumerate(POLICIES):
        policy = policy_record[0]
        color, _, _ = STYLE[policy]
        for simd in SIMD_MODES:
            row = next(
                item for item in rows
                if item["policy"] == policy
                and item["simd_requested"] == simd
            )
            value = int(row["sixel_bytes"])
            x = policy_index + offsets[simd]
            face = "white" if simd == "scalar" else color
            marker = "o" if simd == "scalar" else "s"
            ax.scatter(
                x,
                value,
                s=92,
                marker=marker,
                facecolor=face,
                edgecolor=color,
                linewidth=2,
                zorder=3,
            )
            ax.annotate(
                f"{value:,} B",
                (x, value),
                xytext=(-4 if simd == "scalar" else 4, 9),
                textcoords="offset points",
                ha="right" if simd == "scalar" else "left",
                color=color,
                fontsize=9,
            )
    ax.set_xticks(np.arange(len(POLICIES)), [record[1] for record in POLICIES])
    ax.set_xlim(-0.35, len(POLICIES) - 1 + 0.35)
    ax.set_ylabel("SIXEL stream bytes")
    ax.set_title(
        "Exact stream size, snake 600 x 450 -> 300 x 225",
        loc="left",
        fontweight="bold",
    )
    ax.grid(axis="y")
    ax.legend(
        handles=[
            Line2D(
                [0], [0], marker="o", linestyle="none",
                markerfacecolor="white", markeredgecolor="#20242A",
                markeredgewidth=1.8, markersize=8, label="scalar",
            ),
            Line2D(
                [0], [0], marker="s", linestyle="none",
                markerfacecolor="#5E6672", markeredgecolor="#5E6672",
                markersize=8, label="auto (native ceiling)",
            ),
        ],
        frameon=False,
        loc="best",
    )
    finish_plot(fig, output_path)


def kernel_values(name: str, distance: np.ndarray) -> np.ndarray:
    """Evaluate one resampling kernel exactly as scale.c defines it."""
    values = np.zeros_like(distance)
    if name == "gaussian":
        values = np.exp(-2.0 * distance * distance) * math.sqrt(2.0 / math.pi)
    elif name == "hanning":
        values = 0.5 + 0.5 * np.cos(distance * math.pi)
    elif name == "hamming":
        values = 0.54 + 0.46 * np.cos(distance * math.pi)
    elif name == "bilinear":
        values = np.where(distance < 1.0, 1.0 - distance, 0.0)
    elif name == "welsh":
        values = np.where(distance < 1.0, 1.0 - distance * distance, 0.0)
    elif name == "bicubic":
        values = np.where(
            distance <= 1.0,
            1.0 + (distance - 2.0) * distance * distance,
            np.where(
                distance <= 2.0,
                4.0 + distance * (-8.0 + distance * (5.0 - distance)),
                0.0,
            ),
        )
    elif name.startswith("lanczos"):
        radius = int(name[-1])
        with np.errstate(divide="ignore", invalid="ignore"):
            sinc = np.where(
                distance == 0.0,
                1.0,
                np.sin(math.pi * distance) / (math.pi * distance),
            )
            window = np.where(
                distance == 0.0,
                1.0,
                np.sin(math.pi * distance / radius)
                / (math.pi * distance / radius),
            )
        values = np.where(distance < radius, sinc * window, 0.0)
    else:
        raise ValueError(f"unknown kernel: {name}")
    return values


def plot_kernels(
    names: Sequence[str],
    radius: float,
    title: str,
    output_path: Path,
) -> None:
    """Plot one related kernel family."""
    colors = ("#0072B2", "#E69F00", "#009E73", "#CC79A7", "#D55E00")
    linestyles = ("-", "--", "-.", ":", (0, (5, 2)))
    distance = np.linspace(0.0, radius, 800)
    fig, ax = plt.subplots(figsize=(8.4, 4.8))
    for index, name in enumerate(names):
        ax.plot(
            distance,
            kernel_values(name, distance),
            color=colors[index % len(colors)],
            linestyle=linestyles[index % len(linestyles)],
            linewidth=2.2,
            label=name,
        )
    ax.axhline(0.0, color="#5E6672", linewidth=0.8)
    ax.set_xlim(0.0, radius)
    ax.set_xlabel("Absolute sample distance")
    ax.set_ylabel("Unnormalized weight")
    ax.set_title(title, loc="left", fontweight="bold")
    ax.grid()
    ax.legend(frameon=False, ncol=2, loc="best")
    finish_plot(fig, output_path)


def plot_coordinate_mapping(output_path: Path) -> None:
    """Plot the distinct enlargement and reduction coordinate formulas."""
    fig, axes = plt.subplots(2, 1, figsize=(8.4, 5.8), sharex=False)
    cases = ((4, 7, "Enlargement: destination centers map into source space"),
             (7, 4, "Reduction: source centers map into destination space"))
    for ax, (source_width, target_width, title) in zip(axes, cases):
        source_centers = np.arange(source_width, dtype=float) + 0.5
        target_indices = np.arange(target_width, dtype=float)
        if target_width >= source_width:
            mapped = (target_indices + 0.5) * source_width / target_width
            ax.scatter(source_centers, np.ones_like(source_centers),
                       color="#20242A", marker="s", label="source samples")
            ax.scatter(mapped, np.zeros_like(mapped), color="#0072B2",
                       marker="o", label="mapped output centers")
        else:
            mapped_source = source_centers * target_width / source_width
            target_centers = target_indices + 0.5
            ax.scatter(mapped_source, np.ones_like(mapped_source),
                       color="#20242A", marker="s", label="mapped source centers")
            ax.scatter(target_centers, np.zeros_like(target_centers),
                       color="#0072B2", marker="o", label="output centers")
        ax.set_yticks((0, 1), ("output", "source"))
        ax.set_title(title, loc="left", fontsize=12, fontweight="bold")
        ax.grid(axis="x")
        ax.legend(frameon=False, loc="upper right")
    axes[-1].set_xlabel("Coordinate in the sampling domain")
    fig.suptitle(
        "scale.c uses different coordinate expressions for enlargement and reduction",
        x=0.06,
        ha="left",
        fontsize=15,
        fontweight="bold",
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    finish_plot(fig, output_path)


def generate_plots(
    quality_rows: Sequence[Dict[str, object]],
    speed_rows: Sequence[Dict[str, object]],
    size_rows: Sequence[Dict[str, object]],
    output_directory: Path,
    resampling_directory: Path,
) -> None:
    """Generate separate evidence figures and resampling references."""
    configure_plot()
    plot_quality(
        quality_rows,
        "natural",
        "ms_ssim",
        "MS-SSIM",
        "Natural image: agreement with the linear-light scalar reference",
        output_directory / "crop-resize-quality-natural-ms-ssim.png",
        True,
    )
    plot_quality(
        quality_rows,
        "natural",
        "delta_e00_mean",
        "Mean Delta E00",
        "Natural image: perceptual difference from the linear-light scalar reference",
        output_directory / "crop-resize-quality-natural-delta-e00.png",
        False,
    )
    plot_quality(
        quality_rows,
        "checker",
        "ms_ssim",
        "MS-SSIM",
        "Analytical checker: agreement with the 50% linear-light reference",
        output_directory / "crop-resize-quality-checker-ms-ssim.png",
        True,
    )
    plot_quality(
        quality_rows,
        "checker",
        "delta_e00_mean",
        "Mean Delta E00",
        "Analytical checker: perceptual error from the linear-light reference",
        output_directory / "crop-resize-quality-checker-delta-e00.png",
        False,
    )
    plot_speed(speed_rows, output_directory / "crop-resize-speed.png")
    plot_size(size_rows, output_directory / "crop-resize-size.png")

    resampling_directory.mkdir(parents=True, exist_ok=True)
    plot_kernels(
        ("gaussian", "hanning", "hamming", "bilinear", "welsh"),
        1.0,
        "Radius-1 filtered resampling kernels",
        resampling_directory / "resampling-kernels-radius-1.png",
    )
    plot_kernels(
        ("bicubic", "lanczos2", "lanczos3", "lanczos4"),
        4.0,
        "Wide-support resampling kernels",
        resampling_directory / "resampling-kernels-wide.png",
    )
    plot_coordinate_mapping(
        resampling_directory / "resampling-coordinate-mapping.png"
    )


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--input-label", required=True)
    parser.add_argument("--img2sixel", required=True, type=Path)
    parser.add_argument("--sixel2png", required=True, type=Path)
    parser.add_argument("--lsqa", required=True, type=Path)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--source-state", required=True, choices=("clean",))
    parser.add_argument("--compiler-command", required=True)
    parser.add_argument("--cflags", required=True)
    parser.add_argument("--cppflags", required=True)
    parser.add_argument("--ldflags", required=True)
    parser.add_argument("--configure-arguments", required=True)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=9)
    parser.add_argument("--output-directory", required=True, type=Path)
    parser.add_argument(
        "--resampling-figure-directory", required=True, type=Path
    )
    return parser.parse_args()


def main() -> None:
    """Run the controlled matrix and write all measurement artifacts."""
    args = parse_args()
    if args.warmups < 0 or args.runs < 3:
        raise ValueError("warmups must be non-negative and runs must be at least 3")
    input_path = args.input.resolve()
    img2sixel = args.img2sixel.resolve()
    sixel2png = args.sixel2png.resolve()
    lsqa = args.lsqa.resolve()
    for path in (input_path, img2sixel, sixel2png, lsqa):
        if not path.is_file():
            raise FileNotFoundError(path)
    args.output_directory.mkdir(parents=True, exist_ok=True)
    environment = clean_environment()

    with tempfile.TemporaryDirectory(prefix="libsixel-crop-resize-") as name:
        temporary = Path(name)
        quality_rows, size_rows, preflight = measure_policy_outputs(
            img2sixel,
            sixel2png,
            lsqa,
            input_path,
            temporary,
            args.output_directory,
            environment,
        )
        boundary_rows = measure_boundary_outputs(
            img2sixel,
            sixel2png,
            temporary,
            args.output_directory,
            environment,
        )
        speed_rows = measure_speed(
            img2sixel,
            input_path,
            temporary,
            environment,
            args.warmups,
            args.runs,
            preflight,
        )

    write_csv(
        args.output_directory / "crop-resize-quality.csv",
        quality_rows,
        (
            "fixture", "policy", "simd_requested", "simd_effective",
            "ms_ssim", "delta_e00_mean", "delta_chroma_mean", "gmsd",
            "psnr_y", "output_sha256", "decoded_png_sha256", "command",
            "assessment_command", "input_token",
        ),
    )
    write_csv(
        args.output_directory / "crop-resize-size.csv",
        size_rows,
        (
            "policy", "simd_requested", "simd_effective", "sixel_bytes",
            "output_sha256", "command",
        ),
    )
    write_csv(
        args.output_directory / "crop-resize-speed.csv",
        speed_rows,
        (
            "policy", "simd_requested", "simd_effective", "median_ms",
            "q1_ms", "q3_ms", "minimum_ms", "maximum_ms", "samples_ms",
            "command",
        ),
    )
    write_csv(
        args.output_directory / "crop-resize-boundary-samples.csv",
        boundary_rows,
        (
            "boundary", "policy", "simd_requested", "sample_x", "sample_y",
            "source_left_rgb", "source_right_rgb", "output_midpoint_rgb",
            "sixel_bytes", "output_sha256", "command",
        ),
    )
    generate_plots(
        quality_rows,
        speed_rows,
        size_rows,
        args.output_directory,
        args.resampling_figure_directory,
    )

    artifacts = sorted(
        path.name for path in args.output_directory.iterdir()
        if path.is_file() and path.name != "crop-resize-run.json"
    )
    metadata = {
        "schema_version": 1,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "source": {
            "revision": args.revision,
            "tracked_worktree_state_at_start": args.source_state,
            "platform": platform.platform(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "input": args.input_label,
            "input_sha256": file_sha256(input_path),
        },
        "programs": {
            "img2sixel": program_record(img2sixel),
            "sixel2png": program_record(sixel2png),
            "lsqa": program_record(lsqa, ()),
        },
        "build": {
            "compiler_command": args.compiler_command,
            "cflags": args.cflags,
            "cppflags": args.cppflags,
            "ldflags": args.ldflags,
            "configure_arguments": args.configure_arguments,
        },
        "python": {
            "version": platform.python_version(),
            "matplotlib": matplotlib.__version__,
            "numpy": np.__version__,
            "pillow": PILLOW_VERSION,
        },
        "protocol": {
            "policies": [record[0] for record in POLICIES],
            "simd_modes": list(SIMD_MODES),
            "threads": 1,
            "resampling": "bilinear",
            "palette_colors": PALETTE_COLORS,
            "dither": "none",
            "gpu_policy": "off",
            "natural_source_size": [600, 450],
            "natural_target_size": [VISUAL_WIDTH, VISUAL_HEIGHT],
            "natural_reference": "linear-scalar post-SIXEL snapshot",
            "checker_source_size": [CHECKER_WIDTH, CHECKER_HEIGHT],
            "checker_target_size": [VISUAL_WIDTH, VISUAL_HEIGHT],
            "checker_reference_srgb_tone": 188,
            "comparison_colorspace": "oklab",
            "comparison_precision": "float32",
            "zoom_box": [ZOOM_X, ZOOM_Y, ZOOM_WIDTH, ZOOM_HEIGHT],
            "zoom_display_scale": ZOOM_SCALE,
            "boundary_source_size": [8, BOUNDARY_HEIGHT],
            "boundary_target_size": [BOUNDARY_WIDTH, BOUNDARY_HEIGHT],
            "boundary_sample_x": BOUNDARY_SAMPLE_X,
            "speed_source_size": [SPEED_SOURCE_WIDTH, SPEED_SOURCE_HEIGHT],
            "speed_target_size": [SPEED_TARGET_WIDTH, SPEED_TARGET_HEIGHT],
            "speed_warmups": args.warmups,
            "speed_runs": args.runs,
            "speed_case_order_seed": 1,
            "sixel_environment_removed": True,
        },
        "preflight": preflight,
        "artifacts": {
            "measurement_directory": artifacts,
            "resampling_figures": sorted(
                path.name
                for path in args.resampling_figure_directory.iterdir()
                if path.is_file()
            ),
        },
    }
    (args.output_directory / "crop-resize-run.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
