#!/usr/bin/env python3
"""Measure and plot img2sixel working-colorspace behavior."""

from __future__ import annotations

import argparse
import datetime
import json
import os
import platform
import re
import shlex
import statistics
import subprocess
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import ticker

from plot_clustering_colorspace_measurements import run_quality
from plot_lookup_policy_speed import (
    display_path,
    file_sha256,
    make_command_environment,
    percentile,
    program_record,
    read_build_configuration,
    resolve_img2sixel,
    run_once,
)
from plot_quantize_model_measurements import (
    command_template,
    resolve_lsqa,
    write_csv,
)


COLORS = (8, 16, 32, 64, 128, 256)
DIFFUSIONS = ("none", "fs")
COLORSPACES: Tuple[Tuple[str, str, int, str], ...] = (
    ("gamma", "gamma sRGB", 0, "rgb-f32"),
    ("linear", "linear RGB", 1, "linear-f32"),
    ("oklab", "Oklab", 2, "oklab-f32"),
    ("cielab", "CIELAB", 4, "cielab-f32"),
    ("din99d", "DIN99d", 5, "din99d-f32"),
)
STYLES = {
    "gamma": ("#0072B2", "o", "-"),
    "linear": ("#E69F00", "s", "--"),
    "oklab": ("#009E73", "^", "-."),
    "cielab": ("#CC79A7", "D", ":"),
    "din99d": ("#D55E00", "v", (0, (5, 1))),
}


def colorspace_records() -> List[Dict[str, object]]:
    """Return working-space records in presentation order."""
    return [
        {
            "colorspace": colorspace,
            "label": label,
            "contract_value": contract_value,
            "work_format": work_format,
        }
        for colorspace, label, contract_value, work_format in COLORSPACES
    ]


def make_command(img2sixel: str,
                 input_image: Path,
                 colors: int,
                 colorspace: str,
                 diffusion: str,
                 discard_output: bool) -> List[str]:
    """Build one controlled working-color-space command."""
    command = [
        img2sixel,
        "--threads=1",
        "--precision=float32",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        "-F",
        "none",
        "-a",
        "off",
        "-Xgamma",
        f"-W{colorspace}",
        "-Ugamma",
        f"--diffusion={diffusion}",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "--encode-policy=fast",
        "-p",
        str(colors),
    ]
    if discard_output:
        command.extend(("-o", os.devnull))
    command.append(str(input_image))
    return command


def run_preflight(img2sixel: str,
                  input_image: Path,
                  command_env: Dict[str, str]) -> int:
    """Reject measurements that do not use the requested pipeline."""
    verified = 0
    for diffusion in DIFFUSIONS:
        for record in colorspace_records():
            colorspace = str(record["colorspace"])
            expected_work = str(record["work_format"])
            expected_value = int(record["contract_value"])
            command = make_command(
                img2sixel,
                input_image,
                256,
                colorspace,
                diffusion,
                True,
            )
            trace_env = command_env.copy()
            trace_env["SIXEL_TRACE_TOPIC"] = "palette_contract"
            diagnostic_command = list(command[:-1]) + ["-v", command[-1]]
            proc = subprocess.run(
                diagnostic_command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                env=trace_env,
                check=False,
            )
            diagnostic = proc.stderr.decode("utf-8", errors="replace")
            formats = re.findall(
                r"formats: source=\S+ work=(\S+) scale_out=\S+",
                diagnostic,
            )
            required = (
                "model=kmeans",
                "merge=none",
                "lut=none",
                f"work={expected_value}",
                "cluster=0",
                "quantizer_retries=0",
                "binning_effective=hard",
                "WORKING_SET",
                "CLUSTER_SET",
            )
            if (proc.returncode != 0 or formats != [expected_work] or not all(
                    token in diagnostic for token in required)):
                raise RuntimeError(
                    "Working-colorspace preflight failed for "
                    f"{colorspace}, dither={diffusion}:\n"
                    f"{diagnostic.strip()}"
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
) -> Dict[Tuple[str, int, str], Dict[str, object]]:
    """Measure decoded quality and exact stream size for every point."""
    rows: Dict[Tuple[str, int, str], Dict[str, object]] = {}
    for diffusion in DIFFUSIONS:
        for colors in COLORS:
            group: List[Dict[str, object]] = []
            for record in colorspace_records():
                colorspace = str(record["colorspace"])
                command = make_command(
                    img2sixel,
                    input_image,
                    colors,
                    colorspace,
                    diffusion,
                    False,
                )
                metrics, encoded_bytes = run_quality(
                    command,
                    lsqa,
                    input_image,
                    command_env,
                )
                row = {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": colors,
                    "diffusion": diffusion,
                    **record,
                    "MS-SSIM": metrics["MS-SSIM"],
                    "Delta E00_mean": metrics["Delta E00_mean"],
                    "Delta Chroma_mean": metrics["Delta Chroma_mean"],
                    "encoded_bytes": encoded_bytes,
                    "command": command_template(
                        command,
                        img2sixel,
                        input_image,
                    ),
                }
                group.append(row)
            gamma_bytes = int(next(
                row["encoded_bytes"]
                for row in group
                if row["colorspace"] == "gamma"
            ))
            for row in group:
                row["size_ratio_vs_gamma"] = (
                    int(row["encoded_bytes"]) / gamma_bytes
                )
                key = (
                    str(row["diffusion"]),
                    int(row["colors"]),
                    str(row["colorspace"]),
                )
                rows[key] = row
    return rows


def measure_speed(img2sixel: str,
                  input_image: Path,
                  warmups: int,
                  runs: int,
                  command_env: Dict[str, str]) \
        -> Dict[Tuple[str, int, str], Dict[str, float]]:
    """Measure fresh-process time with rotated and reversed ordering."""
    result: Dict[Tuple[str, int, str], Dict[str, float]] = {}
    names = [str(record["colorspace"]) for record in colorspace_records()]
    for diffusion in DIFFUSIONS:
        for colors in COLORS:
            commands = {
                name: make_command(
                    img2sixel,
                    input_image,
                    colors,
                    name,
                    diffusion,
                    True,
                )
                for name in names
            }
            for warmup in range(warmups):
                offset = warmup % len(names)
                order = names[offset:] + names[:offset]
                if warmup % 2:
                    order.reverse()
                for name in order:
                    run_once(commands[name], command_env)
            samples: Dict[str, List[float]] = {
                name: [] for name in names
            }
            for run_index in range(runs):
                offset = run_index % len(names)
                order = names[offset:] + names[:offset]
                if run_index % 2:
                    order.reverse()
                for name in order:
                    samples[name].append(
                        run_once(commands[name], command_env)
                    )
            medians = {
                name: statistics.median(samples[name]) for name in names
            }
            gamma_median = medians["gamma"]
            for name in names:
                values = samples[name]
                result[(diffusion, colors, name)] = {
                    "runs": runs,
                    "median_seconds": medians[name],
                    "q1_seconds": percentile(values, 0.25),
                    "q3_seconds": percentile(values, 0.75),
                    "min_seconds": min(values),
                    "max_seconds": max(values),
                    "time_ratio_vs_gamma": medians[name] / gamma_median,
                }
    return result


def merge_rows(
        quality: Dict[Tuple[str, int, str], Dict[str, object]],
        speed: Dict[Tuple[str, int, str], Dict[str, float]],
) -> List[Dict[str, object]]:
    """Combine quality, size, and timing values in stable order."""
    rows: List[Dict[str, object]] = []
    for diffusion in DIFFUSIONS:
        for colors in COLORS:
            for record in colorspace_records():
                key = (diffusion, colors, str(record["colorspace"]))
                rows.append({**quality[key], **speed[key]})
    return rows


def rows_for(rows: Sequence[Dict[str, object]],
             diffusion: str,
             colorspace: str) -> List[Dict[str, object]]:
    """Return one line series in ascending palette-size order."""
    selected = [
        row for row in rows
        if row["diffusion"] == diffusion
        and row["colorspace"] == colorspace
    ]
    selected.sort(key=lambda row: int(row["colors"]))
    return selected


def configure_axis(axis: plt.Axes) -> None:
    """Apply the shared palette-size axis and quiet grid."""
    axis.set_xscale("log", base=2)
    axis.set_xticks(COLORS)
    axis.xaxis.set_major_formatter(ticker.ScalarFormatter())
    axis.grid(True, color="#D9D9D9", linewidth=0.7)
    axis.set_xlabel("Palette colors (K)")


def plot_series(axis: plt.Axes,
                rows: Sequence[Dict[str, object]],
                diffusion: str,
                field: str,
                scale: float = 1.0) -> None:
    """Plot every working space with color-independent distinctions."""
    for record in colorspace_records():
        colorspace = str(record["colorspace"])
        color, marker, linestyle = STYLES[colorspace]
        selected = rows_for(rows, diffusion, colorspace)
        axis.plot(
            [int(row["colors"]) for row in selected],
            [float(row[field]) * scale for row in selected],
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=1.8,
            markersize=5.5,
            markerfacecolor="white" if colorspace in ("linear", "cielab")
            else color,
            label=str(record["label"]),
        )
    configure_axis(axis)


def plot_timing_series(axis: plt.Axes,
                       rows: Sequence[Dict[str, object]],
                       diffusion: str) -> None:
    """Plot timing medians with interquartile error bars."""
    for record in colorspace_records():
        colorspace = str(record["colorspace"])
        color, marker, linestyle = STYLES[colorspace]
        selected = rows_for(rows, diffusion, colorspace)
        medians = [float(row["median_seconds"]) * 1000.0 for row in selected]
        lower = [
            (float(row["median_seconds"]) - float(row["q1_seconds"]))
            * 1000.0
            for row in selected
        ]
        upper = [
            (float(row["q3_seconds"]) - float(row["median_seconds"]))
            * 1000.0
            for row in selected
        ]
        axis.errorbar(
            [int(row["colors"]) for row in selected],
            medians,
            yerr=[lower, upper],
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=1.8,
            elinewidth=0.8,
            capsize=2.0,
            markersize=5.5,
            markerfacecolor="white" if colorspace in ("linear", "cielab")
            else color,
            label=str(record["label"]),
        )
    configure_axis(axis)


def place_figure_header(figure: plt.Figure,
                        title: str,
                        subtitle: str,
                        handles: Sequence[object],
                        labels: Sequence[str]) -> None:
    """Place a non-overlapping title, subtitle, and shared legend."""
    figure.suptitle(title, fontsize=15, y=0.995)
    figure.text(
        0.5,
        0.955,
        subtitle,
        ha="center",
        fontsize=9,
        color="#4D4D4D",
    )
    figure.legend(
        handles,
        labels,
        loc="upper center",
        bbox_to_anchor=(0.5, 0.925),
        ncol=5,
        frameon=False,
    )


def plot_quality(path: Path,
                 rows: Sequence[Dict[str, object]],
                 input_label: str) -> None:
    """Plot spatial and pointwise quality for both dither conditions."""
    figure, axes = plt.subplots(2, 2, figsize=(12.0, 8.0), sharex=True)
    for row_index, diffusion in enumerate(DIFFUSIONS):
        plot_series(axes[row_index, 0], rows, diffusion, "MS-SSIM")
        plot_series(axes[row_index, 1], rows, diffusion, "Delta E00_mean")
        axes[row_index, 0].set_ylabel("MS-SSIM (higher is better)")
        axes[row_index, 1].set_ylabel("Mean Delta E00 (lower is better)")
        label = "No dithering" if diffusion == "none" else "Floyd-Steinberg"
        axes[row_index, 0].set_title(f"MS-SSIM · {label}")
        axes[row_index, 1].set_title(f"Mean Delta E00 · {label}")
    handles, labels = axes[0, 0].get_legend_handles_labels()
    place_figure_header(
        figure,
        "Working-color-space quality",
        f"{input_label}; -Xgamma; float32; exact lookup; CPU; 1 thread",
        handles,
        labels,
    )
    figure.tight_layout(rect=(0.0, 0.0, 1.0, 0.87))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160, bbox_inches="tight")
    plt.close(figure)


def plot_performance(path: Path,
                     rows: Sequence[Dict[str, object]],
                     input_label: str,
                     runs: int) -> None:
    """Plot runtime and exact stream size for both dither conditions."""
    figure, axes = plt.subplots(2, 2, figsize=(12.0, 8.0), sharex=True)
    for row_index, diffusion in enumerate(DIFFUSIONS):
        plot_timing_series(axes[row_index, 0], rows, diffusion)
        plot_series(
            axes[row_index, 1], rows, diffusion, "encoded_bytes", 1.0 / 1024
        )
        axes[row_index, 0].set_ylabel("Median wall time (ms)")
        axes[row_index, 0].set_ylim(bottom=0.0)
        axes[row_index, 1].set_ylabel("SIXEL stream size (KiB)")
        axes[row_index, 1].set_ylim(bottom=0.0)
        label = "No dithering" if diffusion == "none" else "Floyd-Steinberg"
        axes[row_index, 0].set_title(f"Fresh-process runtime · {label}")
        axes[row_index, 1].set_title(f"Encoded size · {label}")
    handles, labels = axes[0, 0].get_legend_handles_labels()
    place_figure_header(
        figure,
        "Working-color-space performance and size",
        f"{input_label}; -Xgamma; float32; exact lookup; CPU; 1 thread; {runs}-run median",
        handles,
        labels,
    )
    figure.tight_layout(rect=(0.0, 0.0, 1.0, 0.87))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160, bbox_inches="tight")
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
                   preflight_points: int) -> None:
    """Write measurement provenance and the chart contract."""
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "source": {
            "revision": revision,
            "tracked_worktree_state_at_start": source_state,
        },
        "input": {
            "path": input_label,
            "sha256": file_sha256(input_image),
        },
        "programs": {
            "img2sixel": program_record(img2sixel, source_root),
            "lsqa": program_record(lsqa, source_root),
        },
        "build": read_build_configuration(build_dir),
        "protocol": {
            "comparison_scope": (
                "working color spaces with fixed gamma palette construction"
            ),
            "colors": list(COLORS),
            "colorspaces": colorspace_records(),
            "diffusions": list(DIFFUSIONS),
            "threads": 1,
            "precision": "float32",
            "quality": "full",
            "loader": "builtin!",
            "sampling_policy": "full-frame",
            "binning_policy": "hard",
            "quantize_model": "kmeans:seed=1:binbits=6",
            "merge_policy": "none",
            "cover_policy": "off",
            "clustering_colorspace": "gamma",
            "output_colorspace": "gamma",
            "lookup_policy": "none",
            "gpu_policy": "off",
            "palette_type": "rgb",
            "encode_policy": "fast",
            "speed_warmups": warmups,
            "speed_runs": runs,
            "configuration_order_rotated_and_reversed": True,
            "sixel_environment_removed": clean_sixel_environment,
            "contract_preflight_points": preflight_points,
            "contract_preflight": (
                "typed work format, explicit X/W state, fixed K-means, hard "
                "binning, exact lookup, and zero quantizer retries"
            ),
            "quality_measurement": (
                "lsqa decoded SIXEL versus the source image"
            ),
            "size_measurement": "quality-pass SIXEL stdout byte length",
            "timing_measurement": (
                "fresh process wall time with output discarded"
            ),
        },
        "chart_contract": {
            "question": (
                "How does -W change quality, runtime, and stream size when "
                "palette construction and precision are fixed?"
            ),
            "family": "multi-series line small multiples",
            "x_axis": "palette colors K on a base-2 logarithmic scale",
            "quality_metrics": ["MS-SSIM", "mean Delta E00"],
            "performance_metrics": [
                "fresh-process median wall time",
                "SIXEL stream byte length",
            ],
            "non_color_distinction": "marker shape and line style",
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, ensure_ascii=False)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input")
    parser.add_argument("--img2sixel")
    parser.add_argument("--lsqa")
    parser.add_argument("--build-dir", default=".")
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", default="unknown")
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--output-csv", required=True)
    parser.add_argument("--output-quality-plot", required=True)
    parser.add_argument("--output-performance-plot", required=True)
    parser.add_argument("--output-metadata", required=True)
    return parser.parse_args()


def main() -> int:
    """Run the controlled sweep and write every durable artifact."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    input_image = Path(args.input)
    if not input_image.is_absolute():
        input_image = (source_root / input_image).resolve()
    input_label = display_path(input_image, source_root)
    build_dir = Path(args.build_dir).resolve()
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    lsqa = resolve_lsqa(args.lsqa, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    if args.runs < 1 or args.warmups < 0:
        raise ValueError("runs must be positive and warmups non-negative")

    preflight_points = run_preflight(
        img2sixel, input_image, command_env
    )
    quality = measure_quality_and_size(
        img2sixel,
        lsqa,
        input_image,
        input_label,
        args.revision,
        command_env,
    )
    speed = measure_speed(
        img2sixel,
        input_image,
        args.warmups,
        args.runs,
        command_env,
    )
    rows = merge_rows(quality, speed)
    fieldnames = (
        "revision", "platform", "input", "colors", "diffusion",
        "colorspace", "label", "contract_value", "work_format",
        "MS-SSIM", "Delta E00_mean", "Delta Chroma_mean",
        "encoded_bytes", "size_ratio_vs_gamma", "runs",
        "median_seconds", "q1_seconds", "q3_seconds", "min_seconds",
        "max_seconds", "time_ratio_vs_gamma", "command",
    )
    write_csv(Path(args.output_csv), rows, fieldnames)
    plot_quality(Path(args.output_quality_plot), rows, input_label)
    plot_performance(
        Path(args.output_performance_plot), rows, input_label, args.runs
    )
    write_metadata(
        Path(args.output_metadata),
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
        preflight_points,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
