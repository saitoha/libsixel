#!/usr/bin/env python3
"""Measure and plot img2sixel clustering-colorspace behavior."""

from __future__ import annotations

import argparse
import csv
import datetime
import json
import os
import platform
import re
import shlex
import statistics
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import ticker

from plot_lookup_policy_speed import (
    display_path,
    file_sha256,
    make_command_environment,
    percentile,
    program_record,
    read_build_configuration,
    require_work_format,
    resolve_img2sixel,
    run_once,
)
from plot_quantize_model_measurements import (
    command_template,
    resolve_lsqa,
    run_timeline,
    write_csv,
)


DEFAULT_COLORS = (8, 16, 32, 64, 128, 256)
BINBITS = (4, 5, 6, 7, 8)
OCCUPANCY_COLORS = 64
COLORSPACES: Tuple[Tuple[str, str, int], ...] = (
    ("gamma", "gamma sRGB", 0),
    ("linear", "linear RGB", 1),
    ("oklab", "OKLab", 2),
    ("cielab", "CIELAB", 4),
    ("din99d", "DIN99d", 5),
)
STYLES = {
    "gamma": ("#0072B2", "o", "-"),
    "linear": ("#E69F00", "s", "--"),
    "oklab": ("#009E73", "^", "-."),
    "cielab": ("#CC79A7", "D", ":"),
    "din99d": ("#D55E00", "v", (0, (5, 1))),
}


def colorspace_records() -> List[Dict[str, object]]:
    """Return color-space records in their presentation order."""
    return [
        {
            "colorspace": colorspace,
            "label": label,
            "contract_value": contract_value,
        }
        for colorspace, label, contract_value in COLORSPACES
    ]


def make_command(img2sixel: str,
                 input_image: Path,
                 colors: int,
                 colorspace: str,
                 discard_output: bool,
                 timeline_path: Path | None = None,
                 binning_policy: str = "hard",
                 binbits: int = 6,
                 seed: int = 1) -> List[str]:
    """Build one controlled clustering-color-space command."""
    command = [
        img2sixel,
        "--threads=1",
        "--precision=float32",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        f"--binning-policy={binning_policy}",
        f"--quantize-model=kmeans:seed={seed}:binbits={binbits}",
        "-F",
        "none",
        "-a",
        "off",
        f"-X{colorspace}",
        "-Wgamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "-p",
        str(colors),
    ]
    if timeline_path is not None:
        command.extend(("-J", str(timeline_path)))
    if discard_output:
        command.extend(("-o", os.devnull))
    command.append(str(input_image))
    return command


def run_quality(command: Sequence[str],
                lsqa: str,
                input_image: Path,
                command_env: Dict[str, str]) -> Tuple[Dict[str, float], int]:
    """Encode once and return perceptual metrics and exact stream size."""
    encoded = subprocess.run(
        list(command),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=command_env,
        check=False,
    )
    if encoded.returncode != 0:
        diagnostic = encoded.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"Command failed ({encoded.returncode}): "
            f"{shlex.join(command)}\n{diagnostic}"
        )
    with tempfile.TemporaryDirectory(
            prefix="lsqa-clustering-colorspace-") as directory:
        lsqa_env = command_env.copy()
        for name in list(lsqa_env):
            if name.startswith("LSQA_"):
                del lsqa_env[name]
        lsqa_env["LSQA_PREFIX"] = str(Path(directory) / "quality")
        lsqa_env["LSQA_VERBOSE"] = "0"
        assessed = subprocess.run(
            [lsqa, str(input_image), "-"],
            input=encoded.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=lsqa_env,
            check=False,
        )
    if assessed.returncode != 0:
        diagnostic = assessed.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"lsqa failed ({assessed.returncode}): {diagnostic}"
        )
    try:
        payload = json.loads(assessed.stdout.decode("utf-8", errors="strict"))
        quality = payload.get("quality", payload)
        metrics = {
            "MS-SSIM": float(quality["MS-SSIM"]),
            "Delta E00_mean": float(quality["Δ E00_mean"]),
            "Delta Chroma_mean": float(quality["Δ Chroma_mean"]),
        }
    except (AttributeError, KeyError, TypeError, UnicodeDecodeError,
            ValueError, json.JSONDecodeError) as exc:
        raise RuntimeError("lsqa output lacks valid quality metrics") from exc
    return metrics, len(encoded.stdout)


def verify_palette_contracts(img2sixel: str,
                             input_image: Path,
                             command_env: Dict[str, str]) -> int:
    """Reject a sweep if any point falls back from the requested pipeline."""
    verified = 0
    for colors in DEFAULT_COLORS:
        for record in colorspace_records():
            colorspace = str(record["colorspace"])
            expected_cluster = int(record["contract_value"])
            command = make_command(
                img2sixel,
                input_image,
                colors,
                colorspace,
                True,
            )
            trace_env = command_env.copy()
            trace_env["SIXEL_TRACE_TOPIC"] = "palette_contract"
            proc = subprocess.run(
                command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                env=trace_env,
                check=False,
            )
            diagnostic = proc.stderr.decode("utf-8", errors="replace")
            required = (
                "model=kmeans",
                "merge=none",
                "lut=none",
                f"cluster={expected_cluster}",
                "quantizer_retries=0",
                "binning_effective=hard",
            )
            if proc.returncode != 0 or not all(
                    token in diagnostic for token in required):
                raise RuntimeError(
                    "Palette contract preflight failed for "
                    f"{colorspace}, K={colors}:\n{diagnostic.strip()}"
                )
            verified += 1
    return verified


def measure_quality_and_size(
        img2sixel: str,
        lsqa: str,
        input_image: Path,
        input_label: str,
        revision: str,
        command_env: Dict[str, str],
) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Measure one decoded stream for every color space and K."""
    quality_rows: List[Dict[str, object]] = []
    size_rows: List[Dict[str, object]] = []
    for colors in DEFAULT_COLORS:
        color_size_rows: List[Dict[str, object]] = []
        for record in colorspace_records():
            colorspace = str(record["colorspace"])
            command = make_command(
                img2sixel,
                input_image,
                colors,
                colorspace,
                False,
            )
            metrics, encoded_bytes = run_quality(
                command,
                lsqa,
                input_image,
                command_env,
            )
            common: Dict[str, object] = {
                "revision": revision,
                "platform": platform.platform(),
                "input": input_label,
                "colors": colors,
                "colorspace": colorspace,
                "label": record["label"],
                "command": command_template(
                    command,
                    img2sixel,
                    input_image,
                ),
            }
            quality_rows.append({**common, **metrics})
            color_size_rows.append(
                {
                    **common,
                    "encoded_bytes": encoded_bytes,
                }
            )
        gamma_bytes = next(
            int(row["encoded_bytes"])
            for row in color_size_rows
            if row["colorspace"] == "gamma"
        )
        for row in color_size_rows:
            row["bytes_vs_gamma"] = (
                int(row["encoded_bytes"]) / gamma_bytes
            )
            size_rows.append(row)
    return quality_rows, size_rows


def measure_binning_quality(
        img2sixel: str,
        lsqa: str,
        input_image: Path,
        input_label: str,
        revision: str,
        command_env: Dict[str, str],
        hard_rows: Sequence[Dict[str, object]],
) -> List[Dict[str, object]]:
    """Measure quality across hard-grid resolutions and no aggregation."""
    rows: List[Dict[str, object]] = []
    hard_by_key = {
        (str(row["colorspace"]), int(row["colors"])): row
        for row in hard_rows
    }
    for colors in DEFAULT_COLORS:
        for record in colorspace_records():
            colorspace = str(record["colorspace"])
            for binning_policy, binbits in (
                    *(("hard", value) for value in BINBITS),
                    ("none", 6)):
                if binning_policy == "hard" and binbits == 6:
                    measured = dict(hard_by_key[(colorspace, colors)])
                    metrics = {
                        name: float(measured[name])
                        for name in (
                            "MS-SSIM",
                            "Delta E00_mean",
                            "Delta Chroma_mean",
                        )
                    }
                    command_value = str(measured["command"])
                else:
                    command = make_command(
                        img2sixel,
                        input_image,
                        colors,
                        colorspace,
                        False,
                        binning_policy=binning_policy,
                        binbits=binbits,
                    )
                    metrics, _encoded_bytes = run_quality(
                        command,
                        lsqa,
                        input_image,
                        command_env,
                    )
                    command_value = command_template(
                        command,
                        img2sixel,
                        input_image,
                    )
                rows.append(
                    {
                        "revision": revision,
                        "platform": platform.platform(),
                        "input": input_label,
                        "colors": colors,
                        "colorspace": colorspace,
                        "label": record["label"],
                        "binning_policy": binning_policy,
                        "binbits": binbits,
                        **metrics,
                        "command": command_value,
                    }
                )
    return rows


def measure_binning_occupancy(
        img2sixel: str,
        input_image: Path,
        input_label: str,
        revision: str,
        command_env: Dict[str, str],
) -> List[Dict[str, object]]:
    """Measure the quantizer population produced by each hard grid."""
    rows: List[Dict[str, object]] = []
    pattern = re.compile(
        r"LSXBSTAT1\|bits=(\d+)\|source_points=(\d+)\|"
        r"effective_points=(\d+)"
    )
    for binbits in BINBITS:
        for record in colorspace_records():
            colorspace = str(record["colorspace"])
            command = make_command(
                img2sixel,
                input_image,
                OCCUPANCY_COLORS,
                colorspace,
                True,
                binbits=binbits,
            )
            trace_env = command_env.copy()
            trace_env["SIXEL_TRACE_TOPIC"] = "palette_contract"
            proc = subprocess.run(
                command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                env=trace_env,
                check=False,
            )
            diagnostic = proc.stderr.decode("utf-8", errors="replace")
            matches = pattern.findall(diagnostic)
            if proc.returncode != 0 or len(matches) != 1:
                raise RuntimeError(
                    "Binning occupancy trace failed for "
                    f"{colorspace}, bits={binbits}:\n{diagnostic.strip()}"
                )
            traced_bits, source_points, effective_points = (
                int(value) for value in matches[0]
            )
            if traced_bits != binbits or effective_points < 1:
                raise RuntimeError(
                    "Binning occupancy trace is inconsistent for "
                    f"{colorspace}, bits={binbits}"
                )
            rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": OCCUPANCY_COLORS,
                    "colorspace": colorspace,
                    "label": record["label"],
                    "binbits": binbits,
                    "source_points": source_points,
                    "effective_points": effective_points,
                    "compression_ratio": (
                        source_points / effective_points
                    ),
                    "command": command_template(
                        command,
                        img2sixel,
                        input_image,
                    ),
                }
            )
    return rows


def measure_speed(img2sixel: str,
                  input_image: Path,
                  input_label: str,
                  revision: str,
                  warmups: int,
                  runs: int,
                  command_env: Dict[str, str]) -> List[Dict[str, object]]:
    """Measure uninstrumented total time and instrumented palette spans."""
    rows: List[Dict[str, object]] = []
    records = colorspace_records()
    names = [str(record["colorspace"]) for record in records]
    by_name = {str(record["colorspace"]): record for record in records}
    for colors in DEFAULT_COLORS:
        commands = {
            name: make_command(
                img2sixel,
                input_image,
                colors,
                name,
                True,
            )
            for name in names
        }
        for warmup in range(warmups):
            offset = warmup % len(names)
            order = names[offset:] + names[:offset]
            for name in order:
                run_once(commands[name], command_env)
        end_samples: Dict[str, List[float]] = {
            name: [] for name in names
        }
        for run_index in range(runs):
            offset = run_index % len(names)
            order = names[offset:] + names[:offset]
            for name in order:
                end_samples[name].append(
                    run_once(commands[name], command_env)
                )

        palette_samples: Dict[str, List[float]] = {
            name: [] for name in names
        }
        timeline_templates: Dict[str, str] = {}
        with tempfile.TemporaryDirectory(
                prefix="libsixel-colorspace-timeline-") as directory:
            timeline_dir = Path(directory)
            for round_index in range(warmups + runs):
                offset = round_index % len(names)
                order = names[offset:] + names[:offset]
                for name in order:
                    timeline_path = timeline_dir / (
                        f"{colors}-{round_index}-{name}.jsonl"
                    )
                    command = make_command(
                        img2sixel,
                        input_image,
                        colors,
                        name,
                        True,
                        timeline_path,
                    )
                    duration = run_timeline(
                        command,
                        timeline_path,
                        command_env,
                    )
                    timeline_templates[name] = command_template(
                        command,
                        img2sixel,
                        input_image,
                        timeline_path,
                    )
                    if round_index >= warmups:
                        palette_samples[name].append(duration)

        end_medians = {
            name: statistics.median(end_samples[name]) for name in names
        }
        palette_medians = {
            name: statistics.median(palette_samples[name]) for name in names
        }
        end_gamma = end_medians["gamma"]
        palette_gamma = palette_medians["gamma"]
        for name in names:
            end_values = end_samples[name]
            palette_values = palette_samples[name]
            rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": colors,
                    "colorspace": name,
                    "label": by_name[name]["label"],
                    "runs": runs,
                    "palette_median_seconds": palette_medians[name],
                    "palette_q1_seconds": percentile(
                        palette_values, 0.25
                    ),
                    "palette_q3_seconds": percentile(
                        palette_values, 0.75
                    ),
                    "palette_min_seconds": min(palette_values),
                    "palette_max_seconds": max(palette_values),
                    "palette_speedup_vs_gamma": (
                        palette_gamma / palette_medians[name]
                    ),
                    "end_to_end_median_seconds": end_medians[name],
                    "end_to_end_q1_seconds": percentile(end_values, 0.25),
                    "end_to_end_q3_seconds": percentile(end_values, 0.75),
                    "end_to_end_min_seconds": min(end_values),
                    "end_to_end_max_seconds": max(end_values),
                    "end_to_end_speedup_vs_gamma": (
                        end_gamma / end_medians[name]
                    ),
                    "command": command_template(
                        commands[name],
                        img2sixel,
                        input_image,
                    ),
                    "timeline_command": timeline_templates[name],
                }
            )
    return rows


def rows_for_colorspace(rows: Sequence[Dict[str, object]],
                        colorspace: str) -> List[Dict[str, object]]:
    """Return one color-space series ordered by palette size."""
    selected = [
        row for row in rows if row["colorspace"] == colorspace
    ]
    selected.sort(key=lambda row: int(row["colors"]))
    return selected


def configure_x_axis(axis: plt.Axes) -> None:
    """Use exact powers of two for palette size."""
    axis.set_xscale("log", base=2)
    axis.set_xticks(DEFAULT_COLORS)
    axis.xaxis.set_major_formatter(ticker.ScalarFormatter())
    axis.grid(True, color="#D9D9D9", linewidth=0.7)


def plot_metric(axis: plt.Axes,
                rows: Sequence[Dict[str, object]],
                metric: str,
                divisor: float = 1.0,
                error_prefix: str | None = None) -> None:
    """Plot one metric for every clustering color space."""
    for colorspace, label, _contract_value in COLORSPACES:
        selected = rows_for_colorspace(rows, colorspace)
        color, marker, linestyle = STYLES[colorspace]
        x = [int(row["colors"]) for row in selected]
        y = [float(row[metric]) / divisor for row in selected]
        if error_prefix is None:
            axis.plot(
                x,
                y,
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=1.8,
                markersize=4.5,
                label=label,
            )
        else:
            q1 = [
                float(row[f"{error_prefix}_q1_seconds"]) / divisor
                for row in selected
            ]
            q3 = [
                float(row[f"{error_prefix}_q3_seconds"]) / divisor
                for row in selected
            ]
            lower = [center - bound for center, bound in zip(y, q1)]
            upper = [bound - center for center, bound in zip(y, q3)]
            axis.errorbar(
                x,
                y,
                yerr=[lower, upper],
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=1.6,
                markersize=4.5,
                capsize=2.0,
                label=label,
            )
    configure_x_axis(axis)


def plot_quality(path: Path,
                 rows: Sequence[Dict[str, object]],
                 input_name: str) -> None:
    """Plot spatial, perceptual, and chroma quality metrics."""
    figure, axes = plt.subplots(1, 3, figsize=(13.2, 4.3), sharex=True)
    plot_metric(axes[0], rows, "MS-SSIM")
    plot_metric(axes[1], rows, "Delta E00_mean")
    plot_metric(axes[2], rows, "Delta Chroma_mean")
    axes[0].set_ylabel("MS-SSIM (higher is better)")
    axes[1].set_ylabel("Mean Delta E00 (lower is better)")
    axes[2].set_ylabel("Mean Delta Chroma (lower is better)")
    for axis in axes:
        axis.set_xlabel("Palette size K")
    axes[0].legend(fontsize=8.0, frameon=False, loc="best")
    figure.suptitle(
        f"Clustering-color-space quality on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        "K-means; hard binning; no dithering; exact palette lookup",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.94))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_binning_quality(path: Path,
                         rows: Sequence[Dict[str, object]],
                         input_name: str) -> None:
    """Plot quality recovered by removing finite-grid aggregation."""
    figure, axes = plt.subplots(1, 3, figsize=(13.2, 4.3), sharex=True)
    metrics = (
        ("MS-SSIM", "MS-SSIM improvement"),
        ("Delta E00_mean", "Mean Delta E00 reduction"),
        ("Delta Chroma_mean", "Mean Delta Chroma reduction"),
    )
    for colorspace, label, _contract_value in COLORSPACES:
        color, marker, linestyle = STYLES[colorspace]
        selected = {
            (
                int(row["colors"]),
                str(row["binning_policy"]),
                int(row["binbits"]),
            ): row
            for row in rows
            if row["colorspace"] == colorspace
        }
        x = list(DEFAULT_COLORS)
        for axis, (metric, ylabel) in zip(axes, metrics):
            if metric == "MS-SSIM":
                y = [
                    float(selected[(colors, "none", 6)][metric])
                    - float(selected[(colors, "hard", 6)][metric])
                    for colors in x
                ]
            else:
                y = [
                    float(selected[(colors, "hard", 6)][metric])
                    - float(selected[(colors, "none", 6)][metric])
                    for colors in x
                ]
            axis.plot(
                x,
                y,
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=1.8,
                markersize=4.5,
                label=label,
            )
            axis.set_ylabel(ylabel)
    for axis in axes:
        configure_x_axis(axis)
        axis.axhline(0.0, color="#777777", linewidth=0.8)
        axis.set_xlabel("Palette size K")
    axes[0].legend(fontsize=8.0, frameon=False, loc="best")
    figure.suptitle(
        f"Quality recovered by disabling hard binning on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        "Positive values favor unaggregated input; same seeded K-means",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.94))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_binning_occupancy(path: Path,
                           rows: Sequence[Dict[str, object]],
                           input_name: str) -> None:
    """Plot occupied hard-grid cells across coordinate resolutions."""
    figure, axis = plt.subplots(1, 1, figsize=(7.6, 4.5))
    for colorspace, label, _contract_value in COLORSPACES:
        color, marker, linestyle = STYLES[colorspace]
        selected = sorted(
            (row for row in rows if row["colorspace"] == colorspace),
            key=lambda row: int(row["binbits"]),
        )
        axis.plot(
            [int(row["binbits"]) for row in selected],
            [int(row["effective_points"]) for row in selected],
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=1.8,
            markersize=4.5,
            label=label,
        )
    axis.set_yscale("log")
    axis.set_xticks(BINBITS)
    axis.set_xlabel("Hard-binning bits per axis")
    axis.set_ylabel("Weighted points passed to K-means")
    axis.grid(True, color="#D9D9D9", linewidth=0.7)
    axis.legend(fontsize=8.0, frameon=False, loc="best")
    figure.suptitle(
        f"Hard-grid occupancy by clustering space on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        "Full-frame samples; K=64; occupancy is measured before K-means",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.94))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_speed(path: Path,
               rows: Sequence[Dict[str, object]],
               input_name: str,
               runs: int) -> None:
    """Plot instrumented solver spans and uninstrumented total latency."""
    figure, axes = plt.subplots(1, 2, figsize=(10.2, 4.3), sharex=True)
    plot_metric(
        axes[0],
        rows,
        "palette_median_seconds",
        0.001,
        "palette",
    )
    plot_metric(
        axes[1],
        rows,
        "end_to_end_median_seconds",
        0.001,
        "end_to_end",
    )
    axes[0].set_ylabel("Palette-build span (ms)")
    axes[1].set_ylabel("End-to-end time (ms)")
    for axis in axes:
        axis.set_xlabel("Palette size K")
    axes[0].legend(fontsize=8.0, frameon=False, loc="best")
    figure.suptitle(
        f"Clustering-color-space runtime on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        f"Median of {runs}; bars show IQR; fresh process per sample",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.94))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_size(path: Path,
              rows: Sequence[Dict[str, object]],
              input_name: str) -> None:
    """Plot exact SIXEL stream size."""
    figure, axis = plt.subplots(1, 1, figsize=(7.2, 4.3))
    plot_metric(axis, rows, "encoded_bytes", 1024.0)
    axis.set_xlabel("Palette size K")
    axis.set_ylabel("SIXEL stream size (KiB)")
    axis.legend(fontsize=8.0, frameon=False, loc="best")
    figure.suptitle(
        f"Clustering-color-space output size on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        "Same no-dither streams used for quality measurement",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.94))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def write_metadata(path: Path,
                   source_root: Path,
                   build_dir: Path,
                   input_image: Path,
                   input_label: str,
                   img2sixel: str,
                   lsqa: str,
                   warmups: int,
                   runs: int,
                   revision: str,
                   source_state: str,
                   clean_sixel_environment: bool,
                   verified_points: int) -> None:
    """Write provenance and the controlled comparison protocol."""
    payload = {
        "schema_version": 2,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "source": {
            "revision": revision,
            "tracked_worktree_state_at_start": source_state,
        },
        "build": read_build_configuration(build_dir),
        "host": {
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "matplotlib": matplotlib.__version__,
        },
        "input": {
            "path": input_label,
            "sha256": file_sha256(input_image),
        },
        "programs": {
            "img2sixel": program_record(img2sixel, source_root),
            "lsqa": program_record(lsqa, source_root),
        },
        "protocol": {
            "comparison_scope": (
                "clustering color spaces with a fixed K-means pipeline"
            ),
            "colors": list(DEFAULT_COLORS),
            "colorspaces": colorspace_records(),
            "threads": 1,
            "precision": "float32",
            "required_work_format": "rgb-f32",
            "quality": "full",
            "loader": "builtin!",
            "sampling_policy": "full-frame",
            "binning_policy": "hard",
            "quantize_model": "kmeans:seed=1:binbits=6",
            "merge_policy": "none",
            "cover_policy": "off",
            "working_colorspace": "gamma",
            "diffusion": "none",
            "lookup_policy": "none",
            "gpu_policy": "off",
            "palette_type": "rgb",
            "binning_sensitivity_policies": ["hard", "none"],
            "binning_sensitivity_hard_binbits": list(BINBITS),
            "occupancy_binbits": list(BINBITS),
            "occupancy_palette_size": OCCUPANCY_COLORS,
            "occupancy_measurement": (
                "LSXBSTAT1 effective_points emitted after binning and before "
                "K-means"
            ),
            "speed_warmups": warmups,
            "speed_runs": runs,
            "configuration_order_rotated_each_round": True,
            "sixel_environment_removed": clean_sixel_environment,
            "contract_preflight": (
                "model=kmeans, merge=none, lookup=none, hard binning, "
                "requested clustering space, and zero quantizer retries"
            ),
            "contract_preflight_points": verified_points,
            "size_measurement": "quality-pass SIXEL stdout byte length",
            "palette_timing": (
                "sum of complete top-level palette/build role=palette "
                "timeline spans; clustering-frame conversion is outside "
                "this span"
            ),
            "end_to_end_timing": (
                "uninstrumented fresh-process monotonic wall time"
            ),
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--img2sixel")
    parser.add_argument("--lsqa")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", default="unknown")
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=9)
    parser.add_argument("--output-quality-csv", type=Path, required=True)
    parser.add_argument("--output-quality-plot", type=Path, required=True)
    parser.add_argument(
        "--output-binning-quality-csv", type=Path, required=True
    )
    parser.add_argument(
        "--output-binning-quality-plot", type=Path, required=True
    )
    parser.add_argument(
        "--output-binning-occupancy-csv", type=Path, required=True
    )
    parser.add_argument(
        "--output-binning-occupancy-plot", type=Path, required=True
    )
    parser.add_argument("--output-size-csv", type=Path, required=True)
    parser.add_argument("--output-size-plot", type=Path, required=True)
    parser.add_argument("--output-speed-csv", type=Path, required=True)
    parser.add_argument("--output-speed-plot", type=Path, required=True)
    parser.add_argument("--output-metadata", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the complete clustering-color-space comparison."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    build_dir = (args.build_dir or source_root).resolve()
    input_image = args.input.resolve()
    if not input_image.is_file():
        raise FileNotFoundError(f"Input image does not exist: {input_image}")
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("Warmups must be non-negative and runs must be positive.")
    input_label = display_path(input_image, source_root)
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    lsqa = resolve_lsqa(args.lsqa, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    require_work_format(
        make_command(
            img2sixel,
            input_image,
            DEFAULT_COLORS[0],
            COLORSPACES[0][0],
            True,
        ),
        command_env,
        "rgb-f32",
    )
    verified_points = verify_palette_contracts(
        img2sixel,
        input_image,
        command_env,
    )
    quality_rows, size_rows = measure_quality_and_size(
        img2sixel,
        lsqa,
        input_image,
        input_label,
        args.revision,
        command_env,
    )
    binning_quality_rows = measure_binning_quality(
        img2sixel,
        lsqa,
        input_image,
        input_label,
        args.revision,
        command_env,
        quality_rows,
    )
    occupancy_rows = measure_binning_occupancy(
        img2sixel,
        input_image,
        input_label,
        args.revision,
        command_env,
    )
    speed_rows = measure_speed(
        img2sixel,
        input_image,
        input_label,
        args.revision,
        args.warmups,
        args.runs,
        command_env,
    )
    common_fields = (
        "revision",
        "platform",
        "input",
        "colors",
        "colorspace",
        "label",
    )
    write_csv(
        args.output_quality_csv,
        quality_rows,
        common_fields + (
            "MS-SSIM",
            "Delta E00_mean",
            "Delta Chroma_mean",
            "command",
        ),
    )
    write_csv(
        args.output_size_csv,
        size_rows,
        common_fields + (
            "encoded_bytes",
            "bytes_vs_gamma",
            "command",
        ),
    )
    write_csv(
        args.output_binning_quality_csv,
        binning_quality_rows,
        common_fields + (
            "binning_policy",
            "binbits",
            "MS-SSIM",
            "Delta E00_mean",
            "Delta Chroma_mean",
            "command",
        ),
    )
    write_csv(
        args.output_binning_occupancy_csv,
        occupancy_rows,
        common_fields + (
            "binbits",
            "source_points",
            "effective_points",
            "compression_ratio",
            "command",
        ),
    )
    write_csv(
        args.output_speed_csv,
        speed_rows,
        common_fields + (
            "runs",
            "palette_median_seconds",
            "palette_q1_seconds",
            "palette_q3_seconds",
            "palette_min_seconds",
            "palette_max_seconds",
            "palette_speedup_vs_gamma",
            "end_to_end_median_seconds",
            "end_to_end_q1_seconds",
            "end_to_end_q3_seconds",
            "end_to_end_min_seconds",
            "end_to_end_max_seconds",
            "end_to_end_speedup_vs_gamma",
            "command",
            "timeline_command",
        ),
    )
    plot_quality(args.output_quality_plot, quality_rows, input_image.name)
    plot_binning_quality(
        args.output_binning_quality_plot,
        binning_quality_rows,
        input_image.name,
    )
    plot_binning_occupancy(
        args.output_binning_occupancy_plot,
        occupancy_rows,
        input_image.name,
    )
    plot_size(args.output_size_plot, size_rows, input_image.name)
    plot_speed(
        args.output_speed_plot,
        speed_rows,
        input_image.name,
        args.runs,
    )
    write_metadata(
        args.output_metadata,
        source_root,
        build_dir,
        input_image,
        input_label,
        img2sixel,
        lsqa,
        args.warmups,
        args.runs,
        args.revision,
        args.source_state,
        args.clean_sixel_environment,
        verified_points,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
