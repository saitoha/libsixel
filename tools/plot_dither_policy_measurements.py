#!/usr/bin/env python3
"""Measure static-image dither quality, encoded size, and end-to-end speed."""

from __future__ import annotations

import argparse
import csv
import datetime
import json
import os
import platform
import shlex
import shutil
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
    resolve_img2sixel,
    run_once,
)


DEFAULT_COLORS = (8, 16, 32, 64, 128, 256)
DITHER_METHODS: Tuple[Tuple[str, str], ...] = (
    ("none", "none:scan=raster"),
    ("fs", "fs:scan=raster"),
    ("atkinson", "atkinson:scan=raster"),
    ("jajuni", "jajuni:scan=raster"),
    ("stucki", "stucki:scan=raster"),
    ("burkes", "burkes:scan=raster"),
    ("sierra1", "sierra:variant=1:scan=raster"),
    ("sierra2", "sierra:variant=2:scan=raster"),
    ("sierra3", "sierra:variant=3:scan=raster"),
    ("lso2", "lso2:scan=raster"),
    ("a_dither", "a_dither:strength=0.150:scan=raster"),
    ("x_dither", "x_dither:strength=0.100:scan=raster"),
    (
        "bluenoise",
        "bluenoise:strength=0.055:gradient_factor=0:"
        "phase=0,0:channel=mono:size=64:scan=raster",
    ),
)


def resolve_lsqa(explicit: str | None, source_root: Path) -> str:
    """Resolve the lsqa executable."""
    candidates: List[str] = []
    if explicit:
        candidates.append(explicit)
    env_value = os.environ.get("LSQA_PATH")
    if env_value:
        candidates.append(env_value)
    candidates.append(str(source_root / "assessment" / "lsqa"))
    path_value = shutil.which("lsqa")
    if path_value:
        candidates.append(path_value)
    for candidate in candidates:
        path = Path(candidate)
        if path.is_file() and os.access(path, os.X_OK):
            return str(path.resolve())
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    raise FileNotFoundError("Could not find an executable lsqa.")


def make_command(img2sixel: str,
                 input_image: Path,
                 colors: int,
                 diffusion: str,
                 discard_output: bool) -> List[str]:
    """Build one controlled dither-policy command."""
    command = [
        img2sixel,
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=libpng!",
        "--quantize-model=kmeans:seed=1",
        "--merge-policy=ward",
        "-Xoklab",
        "-Wgamma",
        f"--diffusion={diffusion}",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "-p",
        str(colors),
    ]
    if discard_output:
        command.extend(("-o", os.devnull))
    command.append(str(input_image))
    return command


def command_template(command: Sequence[str],
                     img2sixel: str,
                     input_image: Path) -> str:
    """Return a relocatable command string for one CSV row."""
    text = shlex.join(command)
    text = text.replace(img2sixel, "{img2sixel}", 1)
    return text.replace(str(input_image), "{input}", 1)


def run_quality(command: Sequence[str],
                lsqa: str,
                input_image: Path,
                command_env: Dict[str, str]) -> Tuple[Dict[str, float], int]:
    """Encode once and return quality metrics plus encoded byte size."""
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
    with tempfile.TemporaryDirectory(prefix="lsqa-dither-") as directory:
        lsqa_env = command_env.copy()
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
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise RuntimeError("lsqa output is not valid JSON") from exc
    quality = payload.get("quality", payload)
    if not isinstance(quality, dict):
        raise RuntimeError("lsqa JSON does not contain a quality object")
    try:
        return (
            {
                "MS-SSIM": float(quality["MS-SSIM"]),
                "Delta E00_mean": float(quality["Δ E00_mean"]),
            },
            len(encoded.stdout),
        )
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError("lsqa output lacks a required quality metric") from exc


def measure_quality_and_size(
        img2sixel: str,
        lsqa: str,
        input_image: Path,
        input_label: str,
        revision: str,
        command_env: Dict[str, str],
) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Measure quality and encoded size for each method and palette size."""
    quality_rows: List[Dict[str, object]] = []
    size_rows: List[Dict[str, object]] = []
    for colors in DEFAULT_COLORS:
        color_size_rows: List[Dict[str, object]] = []
        for method, diffusion in DITHER_METHODS:
            command = make_command(
                img2sixel,
                input_image,
                colors,
                diffusion,
                False,
            )
            metrics, encoded_bytes = run_quality(
                command,
                lsqa,
                input_image,
                command_env,
            )
            template = command_template(command, img2sixel, input_image)
            quality_rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": colors,
                    "method": method,
                    "diffusion_option": diffusion,
                    "MS-SSIM": metrics["MS-SSIM"],
                    "Delta E00_mean": metrics["Delta E00_mean"],
                    "command": template,
                }
            )
            color_size_rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": colors,
                    "method": method,
                    "diffusion_option": diffusion,
                    "encoded_bytes": encoded_bytes,
                    "command": template,
                }
            )
        baseline = int(color_size_rows[0]["encoded_bytes"])
        for row in color_size_rows:
            row["bytes_vs_none"] = int(row["encoded_bytes"]) / baseline
            size_rows.append(row)
    return quality_rows, size_rows


def measure_speed(img2sixel: str,
                  input_image: Path,
                  input_label: str,
                  revision: str,
                  warmups: int,
                  runs: int,
                  command_env: Dict[str, str]) -> List[Dict[str, object]]:
    """Measure fresh-process latency with rotated method order."""
    rows: List[Dict[str, object]] = []
    methods = [method for method, _diffusion in DITHER_METHODS]
    options = dict(DITHER_METHODS)
    for colors in DEFAULT_COLORS:
        commands = {
            method: make_command(
                img2sixel,
                input_image,
                colors,
                options[method],
                True,
            )
            for method in methods
        }
        for warmup in range(warmups):
            offset = warmup % len(methods)
            order = methods[offset:] + methods[:offset]
            for method in order:
                run_once(commands[method], command_env)
        samples: Dict[str, List[float]] = {method: [] for method in methods}
        for run_index in range(runs):
            offset = run_index % len(methods)
            order = methods[offset:] + methods[:offset]
            for method in order:
                samples[method].append(run_once(commands[method], command_env))
        medians = {
            method: statistics.median(samples[method]) for method in methods
        }
        baseline = medians["none"]
        for method in methods:
            values = samples[method]
            rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": colors,
                    "method": method,
                    "diffusion_option": options[method],
                    "runs": runs,
                    "median_seconds": medians[method],
                    "q1_seconds": percentile(values, 0.25),
                    "q3_seconds": percentile(values, 0.75),
                    "min_seconds": min(values),
                    "max_seconds": max(values),
                    "speedup_vs_none": baseline / medians[method],
                    "command": command_template(
                        commands[method],
                        img2sixel,
                        input_image,
                    ),
                }
            )
    return rows


def write_csv(path: Path,
              rows: Sequence[Dict[str, object]],
              fieldnames: Sequence[str]) -> None:
    """Write one measurement table."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fieldnames,
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(rows)


def rows_for_method(rows: Sequence[Dict[str, object]],
                    method: str) -> List[Dict[str, object]]:
    """Return rows for one method ordered by palette size."""
    selected = [row for row in rows if row["method"] == method]
    selected.sort(key=lambda row: int(row["colors"]))
    return selected


def configure_axis(axis: plt.Axes,
                   row: int,
                   column: int,
                   ylabel: str) -> None:
    """Apply common small-multiple axis styling."""
    axis.set_xscale("log", base=2)
    axis.set_xticks(DEFAULT_COLORS)
    axis.xaxis.set_major_formatter(ticker.ScalarFormatter())
    axis.grid(True, color="#D9D9D9", linewidth=0.7)
    if row == 3:
        axis.set_xlabel("Palette size K")
    if column == 0:
        axis.set_ylabel(ylabel)


def plot_metric(path: Path,
                rows: Sequence[Dict[str, object]],
                metric: str,
                title: str,
                ylabel: str,
                higher_is_better: bool,
                divisor: float = 1.0) -> None:
    """Plot one metric for each method against no dithering."""
    figure, axes = plt.subplots(
        4,
        3,
        figsize=(12.0, 10.5),
        sharex=True,
        sharey=True,
    )
    baseline_rows = rows_for_method(rows, "none")
    x_baseline = [int(row["colors"]) for row in baseline_rows]
    y_baseline = [float(row[metric]) / divisor for row in baseline_rows]
    for index, (method, _diffusion) in enumerate(DITHER_METHODS[1:]):
        row_index = index // 3
        column_index = index % 3
        axis = axes[row_index][column_index]
        method_rows = rows_for_method(rows, method)
        x_method = [int(row["colors"]) for row in method_rows]
        y_method = [float(row[metric]) / divisor for row in method_rows]
        axis.plot(
            x_baseline,
            y_baseline,
            color="#4D4D4D",
            linestyle="--",
            marker="o",
            linewidth=1.5,
            markersize=3.5,
            label="none",
        )
        axis.plot(
            x_method,
            y_method,
            color="#0072B2",
            linestyle="-",
            marker="s",
            linewidth=1.8,
            markersize=3.5,
            label="panel method",
        )
        axis.set_title(method, fontsize=10)
        configure_axis(axis, row_index, column_index, ylabel)
    direction = "higher is better" if higher_is_better else "lower is better"
    handles, labels = axes[0][0].get_legend_handles_labels()
    figure.suptitle(title, y=0.995)
    figure.legend(
        handles,
        labels,
        loc="upper center",
        bbox_to_anchor=(0.5, 0.972),
        ncol=2,
        frameon=False,
    )
    figure.text(
        0.99,
        0.008,
        "Each panel compares one method with none; shared scales; "
        f"{direction}",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.925))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_speed(path: Path,
               rows: Sequence[Dict[str, object]],
               runs: int,
               input_name: str) -> None:
    """Plot method and no-dither latency with IQR in shared-scale facets."""
    figure, axes = plt.subplots(
        4,
        3,
        figsize=(12.0, 10.5),
        sharex=True,
        sharey=True,
    )
    baseline_rows = rows_for_method(rows, "none")
    for index, (method, _diffusion) in enumerate(DITHER_METHODS[1:]):
        row_index = index // 3
        column_index = index % 3
        axis = axes[row_index][column_index]
        for selected, color, linestyle, marker, label in (
            (baseline_rows, "#4D4D4D", "--", "o", "none"),
            (
                rows_for_method(rows, method),
                "#D55E00",
                "-",
                "s",
                "panel method",
            ),
        ):
            x = [int(row["colors"]) for row in selected]
            median = [float(row["median_seconds"]) * 1000.0 for row in selected]
            q1 = [float(row["q1_seconds"]) * 1000.0 for row in selected]
            q3 = [float(row["q3_seconds"]) * 1000.0 for row in selected]
            lower = [center - bound for center, bound in zip(median, q1)]
            upper = [bound - center for center, bound in zip(median, q3)]
            axis.errorbar(
                x,
                median,
                yerr=[lower, upper],
                color=color,
                linestyle=linestyle,
                marker=marker,
                linewidth=1.5,
                markersize=3.5,
                capsize=2.0,
                label=label,
            )
        axis.set_title(method, fontsize=10)
        configure_axis(
            axis,
            row_index,
            column_index,
            "Median elapsed time (ms)",
        )
    handles, labels = axes[0][0].get_legend_handles_labels()
    figure.suptitle(
        f"End-to-end dither-policy runtime on {input_name}",
        y=0.995,
    )
    figure.legend(
        handles,
        labels,
        loc="upper center",
        bbox_to_anchor=(0.5, 0.972),
        ncol=2,
        frameon=False,
    )
    figure.text(
        0.99,
        0.008,
        f"Fresh process per sample; median of {runs}; bars show IQR; "
        "shared scales; lower is better",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.925))
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
                   clean_sixel_environment: bool) -> None:
    """Write provenance and the controlled measurement protocol."""
    payload = {
        "schema_version": 1,
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
            "colors": list(DEFAULT_COLORS),
            "methods": [
                {"name": method, "diffusion_option": diffusion}
                for method, diffusion in DITHER_METHODS
            ],
            "threads": 1,
            "precision": "8bit",
            "quantize_model": "kmeans:seed=1",
            "merge_policy": "ward",
            "clustering_colorspace": "oklab",
            "working_colorspace": "gamma",
            "lookup_policy": "none",
            "scan": "raster",
            "gpu_policy": "off",
            "speed_warmups": warmups,
            "speed_runs": runs,
            "method_order_rotated_each_round": True,
            "sixel_environment_removed": clean_sixel_environment,
            "size_measurement": "quality-pass SIXEL stdout byte length",
            "excluded": {
                "auto": "selector rather than a concrete method",
                "interframe": "requires an animation protocol",
                "stbn": "requires an animation protocol",
            },
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
    parser.add_argument("--output-ms-ssim-plot", type=Path, required=True)
    parser.add_argument("--output-delta-e00-plot", type=Path, required=True)
    parser.add_argument("--output-size-csv", type=Path, required=True)
    parser.add_argument("--output-size-plot", type=Path, required=True)
    parser.add_argument("--output-speed-csv", type=Path, required=True)
    parser.add_argument("--output-speed-plot", type=Path, required=True)
    parser.add_argument("--output-metadata", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the complete static-image dither comparison."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    input_image = args.input.resolve()
    if not input_image.is_file():
        raise FileNotFoundError(f"Input image does not exist: {input_image}")
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("Warmups must be non-negative and runs must be positive.")
    input_label = display_path(input_image, source_root)
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    lsqa = resolve_lsqa(args.lsqa, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    quality_rows, size_rows = measure_quality_and_size(
        img2sixel,
        lsqa,
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
    write_csv(
        args.output_quality_csv,
        quality_rows,
        (
            "revision",
            "platform",
            "input",
            "colors",
            "method",
            "diffusion_option",
            "MS-SSIM",
            "Delta E00_mean",
            "command",
        ),
    )
    write_csv(
        args.output_size_csv,
        size_rows,
        (
            "revision",
            "platform",
            "input",
            "colors",
            "method",
            "diffusion_option",
            "encoded_bytes",
            "bytes_vs_none",
            "command",
        ),
    )
    write_csv(
        args.output_speed_csv,
        speed_rows,
        (
            "revision",
            "platform",
            "input",
            "colors",
            "method",
            "diffusion_option",
            "runs",
            "median_seconds",
            "q1_seconds",
            "q3_seconds",
            "min_seconds",
            "max_seconds",
            "speedup_vs_none",
            "command",
        ),
    )
    plot_metric(
        args.output_ms_ssim_plot,
        quality_rows,
        "MS-SSIM",
        f"Dither-policy MS-SSIM on {input_image.name}",
        "MS-SSIM",
        True,
    )
    plot_metric(
        args.output_delta_e00_plot,
        quality_rows,
        "Delta E00_mean",
        f"Dither-policy mean Delta E00 on {input_image.name}",
        "Mean Delta E00",
        False,
    )
    plot_metric(
        args.output_size_plot,
        size_rows,
        "encoded_bytes",
        f"Encoded SIXEL size by dither policy on {input_image.name}",
        "Encoded SIXEL size (KiB)",
        False,
        1024.0,
    )
    plot_speed(
        args.output_speed_plot,
        speed_rows,
        args.runs,
        input_image.name,
    )
    write_metadata(
        args.output_metadata,
        source_root,
        (args.build_dir or source_root).resolve(),
        input_image,
        input_label,
        img2sixel,
        lsqa,
        args.warmups,
        args.runs,
        args.revision,
        args.source_state,
        args.clean_sixel_environment,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
