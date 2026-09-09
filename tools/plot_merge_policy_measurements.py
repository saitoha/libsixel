#!/usr/bin/env python3
"""Measure and plot final palette merge quality, size, and runtime."""

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
import tempfile
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
from plot_quantize_model_measurements import resolve_lsqa, write_csv


COLORS = (8, 16, 32, 64, 128, 256)
QUANTIZERS: Tuple[Tuple[str, str, str, str], ...] = (
    (
        "heckbert",
        "Heckbert median cut",
        "heckbert:profile=compat",
        "none",
    ),
    (
        "kmeans",
        "K-means",
        "kmeans:seed=1:restarts=1:feedback=0",
        "hard",
    ),
)
MERGE_CONFIGS: Tuple[Tuple[str, str, str, float, int], ...] = (
    (
        "none",
        "No final merge",
        "none:merge_oversplit=1.81:merge_lloyd=0:channel_l="
        "0.3333333333333333",
        1.81,
        0,
    ),
    (
        "ward-l0",
        "Ward only",
        "ward:merge_oversplit=1.81:merge_lloyd=0:channel_l="
        "0.3333333333333333",
        1.81,
        0,
    ),
    (
        "ward-l3",
        "Ward + 3 Lloyd passes",
        "ward:merge_oversplit=1.81:merge_lloyd=3:channel_l="
        "0.3333333333333333",
        1.81,
        3,
    ),
)
STYLES = {
    "none": ("#0072B2", "o", "-"),
    "ward-l0": ("#E69F00", "s", "--"),
    "ward-l3": ("#009E73", "^", "-."),
}


def quantizer_records() -> List[Dict[str, str]]:
    """Return quantizer metadata in presentation order."""
    return [
        {
            "quantizer": name,
            "quantizer_label": label,
            "quantize_option": option,
            "binning_policy": binning,
        }
        for name, label, option, binning in QUANTIZERS
    ]


def merge_records() -> List[Dict[str, object]]:
    """Return merge metadata in presentation order."""
    return [
        {
            "merge_config": name,
            "merge_label": label,
            "merge_option": option,
            "merge_oversplit": oversplit,
            "merge_lloyd": lloyd,
        }
        for name, label, option, oversplit, lloyd in MERGE_CONFIGS
    ]


def make_command(img2sixel: str,
                 input_image: Path,
                 colors: int,
                 quantizer: Dict[str, str],
                 merge: Dict[str, object],
                 discard_output: bool,
                 timeline_path: Path | None = None) -> List[str]:
    """Build one fully controlled merge-policy command."""
    command = [
        img2sixel,
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        f"--binning-policy={quantizer['binning_policy']}",
        f"--quantize-model={quantizer['quantize_option']}",
        f"--merge-policy={merge['merge_option']}",
        "--cover-policy=off",
        "--snap-policy=none",
        "-Xoklab",
        "-Wgamma",
        "-Ugamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "--encode-policy=fast",
        "-p",
        str(colors),
    ]
    if timeline_path is not None:
        command.extend(("-J", str(timeline_path)))
    if discard_output:
        command.extend(("-o", os.devnull))
    command.append(str(input_image))
    return command


def command_template(command: Sequence[str],
                     img2sixel: str,
                     input_image: Path,
                     timeline_path: Path | None = None) -> str:
    """Replace machine-specific command paths with stable placeholders."""
    text = shlex.join(command)
    text = text.replace(img2sixel, "{img2sixel}", 1)
    text = text.replace(str(input_image), "{input}", 1)
    if timeline_path is not None:
        text = text.replace(str(timeline_path), "{timeline}", 1)
    return text


def verify_contracts(img2sixel: str,
                     input_image: Path,
                     command_env: Dict[str, str]) -> int:
    """Reject a run if any requested pipeline falls back or changes format."""
    verified = 0
    for colors in COLORS:
        for quantizer in quantizer_records():
            for merge in merge_records():
                command = make_command(
                    img2sixel,
                    input_image,
                    colors,
                    quantizer,
                    merge,
                    True,
                )
                diagnostic_command = list(command[:-1]) + ["-v", command[-1]]
                trace_env = command_env.copy()
                trace_env["SIXEL_TRACE_TOPIC"] = "palette_contract"
                proc = subprocess.run(
                    diagnostic_command,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    env=trace_env,
                    check=False,
                )
                diagnostic = proc.stderr.decode("utf-8", errors="replace")
                merge_name = "none" if merge["merge_config"] == "none" \
                    else "ward"
                required = (
                    "rc=0",
                    f"model={quantizer['quantizer']}",
                    f"merge={merge_name}",
                    "lut=none",
                    "work=0",
                    "cluster=2",
                    f"binning_effective={quantizer['binning_policy']}",
                    "quantizer_retries=0",
                    "formats: source=rgb888 work=rgb888 scale_out=rgb888",
                )
                if proc.returncode != 0 or not all(
                        token in diagnostic for token in required):
                    raise RuntimeError(
                        "Merge-policy preflight failed for "
                        f"{quantizer['quantizer']}, "
                        f"{merge['merge_config']}, K={colors}:\n"
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
        command_env: Dict[str, str]) \
        -> Dict[Tuple[str, int, str], Dict[str, object]]:
    """Measure decoded quality and exact SIXEL size for every point."""
    rows: Dict[Tuple[str, int, str], Dict[str, object]] = {}
    for quantizer in quantizer_records():
        for colors in COLORS:
            group: List[Dict[str, object]] = []
            for merge in merge_records():
                command = make_command(
                    img2sixel,
                    input_image,
                    colors,
                    quantizer,
                    merge,
                    False,
                )
                metrics, encoded_bytes = run_quality(
                    command,
                    lsqa,
                    input_image,
                    command_env,
                )
                row: Dict[str, object] = {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": colors,
                    **quantizer,
                    **merge,
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
            baseline = next(
                row for row in group if row["merge_config"] == "none"
            )
            for row in group:
                row["MS-SSIM_delta_vs_none"] = (
                    float(row["MS-SSIM"]) - float(baseline["MS-SSIM"])
                )
                row["Delta E00_delta_vs_none"] = (
                    float(row["Delta E00_mean"])
                    - float(baseline["Delta E00_mean"])
                )
                row["bytes_ratio_vs_none"] = (
                    int(row["encoded_bytes"])
                    / int(baseline["encoded_bytes"])
                )
                key = (
                    str(row["quantizer"]),
                    int(row["colors"]),
                    str(row["merge_config"]),
                )
                rows[key] = row
    return rows


def read_timeline(path: Path) -> Tuple[float, float]:
    """Return total top-level palette and merge spans from one timeline."""
    starts: Dict[Tuple[int, int, str], float] = {}
    palette_durations: List[float] = []
    merge_durations: List[float] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            event = json.loads(line)
            if event.get("worker") != "palette/build":
                continue
            role = str(event.get("role", ""))
            if role not in ("palette", "palette/merge"):
                continue
            key = (
                int(event["session_id"]),
                int(event["job"]),
                role,
            )
            if event.get("event") == "start":
                if key in starts:
                    raise RuntimeError(f"duplicate timeline start: {key}")
                starts[key] = float(event["ts"])
            elif event.get("event") == "finish":
                if key not in starts:
                    raise RuntimeError(f"timeline finish without start: {key}")
                duration = float(event["ts"]) - starts.pop(key)
                if duration < 0.0:
                    raise RuntimeError(f"negative timeline duration: {key}")
                if role == "palette":
                    palette_durations.append(duration)
                else:
                    merge_durations.append(duration)
    if starts:
        raise RuntimeError(f"unfinished timeline spans: {sorted(starts)}")
    if not palette_durations:
        raise RuntimeError("timeline contains no complete palette span")
    return sum(palette_durations), sum(merge_durations)


def run_timeline(command: Sequence[str],
                 timeline_path: Path,
                 command_env: Dict[str, str]) -> Tuple[float, float]:
    """Run one instrumented encode and read its palette stage durations."""
    proc = subprocess.run(
        list(command),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        env=command_env,
        check=False,
    )
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"Command failed ({proc.returncode}): "
            f"{shlex.join(command)}\n{diagnostic}"
        )
    return read_timeline(timeline_path)


def summarize_samples(values: Sequence[float], prefix: str) \
        -> Dict[str, float]:
    """Return median, quartiles, and extrema with one field prefix."""
    return {
        f"{prefix}_median_seconds": statistics.median(values),
        f"{prefix}_q1_seconds": percentile(values, 0.25),
        f"{prefix}_q3_seconds": percentile(values, 0.75),
        f"{prefix}_min_seconds": min(values),
        f"{prefix}_max_seconds": max(values),
    }


def measure_speed(
        img2sixel: str,
        input_image: Path,
        warmups: int,
        runs: int,
        command_env: Dict[str, str]) \
        -> Dict[Tuple[str, int, str], Dict[str, object]]:
    """Measure uninstrumented latency and instrumented palette stages."""
    result: Dict[Tuple[str, int, str], Dict[str, object]] = {}
    merges = merge_records()
    names = [str(record["merge_config"]) for record in merges]
    by_name = {str(record["merge_config"]): record for record in merges}
    for quantizer in quantizer_records():
        for colors in COLORS:
            commands = {
                name: make_command(
                    img2sixel,
                    input_image,
                    colors,
                    quantizer,
                    by_name[name],
                    True,
                )
                for name in names
            }
            for round_index in range(warmups):
                offset = round_index % len(names)
                order = names[offset:] + names[:offset]
                if round_index % 2:
                    order.reverse()
                for name in order:
                    run_once(commands[name], command_env)
            end_samples: Dict[str, List[float]] = {
                name: [] for name in names
            }
            for round_index in range(runs):
                offset = round_index % len(names)
                order = names[offset:] + names[:offset]
                if round_index % 2:
                    order.reverse()
                for name in order:
                    end_samples[name].append(
                        run_once(commands[name], command_env)
                    )

            palette_samples: Dict[str, List[float]] = {
                name: [] for name in names
            }
            merge_samples: Dict[str, List[float]] = {
                name: [] for name in names
            }
            timeline_templates: Dict[str, str] = {}
            with tempfile.TemporaryDirectory(
                    prefix="libsixel-merge-policy-") as directory:
                timeline_dir = Path(directory)
                for round_index in range(warmups + runs):
                    offset = round_index % len(names)
                    order = names[offset:] + names[:offset]
                    if round_index % 2:
                        order.reverse()
                    for name in order:
                        timeline_path = timeline_dir / (
                            f"{quantizer['quantizer']}-{colors}-"
                            f"{round_index}-{name}.jsonl"
                        )
                        command = make_command(
                            img2sixel,
                            input_image,
                            colors,
                            quantizer,
                            by_name[name],
                            True,
                            timeline_path,
                        )
                        palette_time, merge_time = run_timeline(
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
                            palette_samples[name].append(palette_time)
                            merge_samples[name].append(merge_time)

            end_baseline = statistics.median(end_samples["none"])
            palette_baseline = statistics.median(palette_samples["none"])
            for name in names:
                end_summary = summarize_samples(
                    end_samples[name],
                    "end_to_end",
                )
                palette_summary = summarize_samples(
                    palette_samples[name],
                    "palette",
                )
                merge_summary = summarize_samples(
                    merge_samples[name],
                    "merge_stage",
                )
                key = (str(quantizer["quantizer"]), colors, name)
                result[key] = {
                    "runs": runs,
                    **end_summary,
                    **palette_summary,
                    **merge_summary,
                    "end_to_end_ratio_vs_none": (
                        float(end_summary["end_to_end_median_seconds"])
                        / end_baseline
                    ),
                    "palette_ratio_vs_none": (
                        float(palette_summary["palette_median_seconds"])
                        / palette_baseline
                    ),
                    "timeline_command": timeline_templates[name],
                }
    return result


def configure_x_axis(axis: plt.Axes) -> None:
    """Use the exact measured palette counts on a base-two axis."""
    axis.set_xscale("log", base=2)
    axis.set_xticks(COLORS)
    axis.xaxis.set_major_formatter(ticker.ScalarFormatter())
    axis.grid(True, color="#D9D9D9", linewidth=0.7)


def rows_for(rows: Sequence[Dict[str, object]],
             quantizer: str,
             merge_config: str) -> List[Dict[str, object]]:
    """Select one curve and order it by palette count."""
    selected = [
        row for row in rows
        if row["quantizer"] == quantizer
        and row["merge_config"] == merge_config
    ]
    selected.sort(key=lambda row: int(row["colors"]))
    return selected


def plot_metric(axis: plt.Axes,
                rows: Sequence[Dict[str, object]],
                quantizer: str,
                metric: str,
                divisor: float = 1.0,
                error_prefix: str | None = None) -> None:
    """Plot one metric for all merge configurations."""
    for merge in merge_records():
        name = str(merge["merge_config"])
        selected = rows_for(rows, quantizer, name)
        color, marker, linestyle = STYLES[name]
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
                label=str(merge["merge_label"]),
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
                label=str(merge["merge_label"]),
            )
    configure_x_axis(axis)


def plot_quality(path: Path,
                 rows: Sequence[Dict[str, object]],
                 input_name: str) -> None:
    """Plot spatial and pointwise decoded-image quality."""
    figure, axes = plt.subplots(2, 2, figsize=(11.5, 7.0), sharex=True)
    quantizers = quantizer_records()
    for column, quantizer in enumerate(quantizers):
        name = quantizer["quantizer"]
        plot_metric(axes[0][column], rows, name, "MS-SSIM")
        plot_metric(axes[1][column], rows, name, "Delta E00_mean")
        axes[0][column].set_title(quantizer["quantizer_label"], fontsize=11)
        axes[1][column].set_xlabel("Palette size K")
        axes[0][column].legend(fontsize=8, frameon=False, loc="best")
    axes[0][0].set_ylabel("MS-SSIM (higher is better)")
    axes[1][0].set_ylabel("Mean Delta E00 (lower is better)")
    figure.suptitle(f"Final merge quality on {input_name}", y=0.995)
    figure.text(
        0.99,
        0.008,
        "No dithering; exact lookup; cover and snap disabled",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.96))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_performance(path: Path,
                     rows: Sequence[Dict[str, object]],
                     input_name: str,
                     runs: int) -> None:
    """Plot palette time, end-to-end time, and encoded stream size."""
    figure, axes = plt.subplots(3, 2, figsize=(11.5, 9.4), sharex=True)
    quantizers = quantizer_records()
    for column, quantizer in enumerate(quantizers):
        name = quantizer["quantizer"]
        plot_metric(
            axes[0][column],
            rows,
            name,
            "palette_median_seconds",
            0.001,
            "palette",
        )
        plot_metric(
            axes[1][column],
            rows,
            name,
            "end_to_end_median_seconds",
            0.001,
            "end_to_end",
        )
        plot_metric(
            axes[2][column],
            rows,
            name,
            "encoded_bytes",
            1024.0,
        )
        axes[0][column].set_title(quantizer["quantizer_label"], fontsize=11)
        axes[2][column].set_xlabel("Palette size K")
        axes[0][column].legend(fontsize=8, frameon=False, loc="best")
    axes[0][0].set_ylabel("Palette build (ms)")
    axes[1][0].set_ylabel("End-to-end time (ms)")
    axes[2][0].set_ylabel("SIXEL stream size (KiB)")
    figure.suptitle(f"Final merge cost and size on {input_name}", y=0.995)
    figure.text(
        0.99,
        0.008,
        f"Timing median of {runs}; bars show IQR; lower is faster/smaller",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.97))
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
                   preflight_points: int) -> None:
    """Write measurement provenance and the controlled protocol."""
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
            "comparison_scope": (
                "final merge policy within each controlled quantizer"
            ),
            "colors": list(COLORS),
            "quantizers": quantizer_records(),
            "merge_configurations": merge_records(),
            "threads": 1,
            "precision": "8bit",
            "required_work_format": "rgb888",
            "quality": "full",
            "loader": "builtin!",
            "sampling_policy": "full-frame",
            "clustering_colorspace": "oklab",
            "working_colorspace": "gamma",
            "output_colorspace": "gamma",
            "diffusion": "none",
            "lookup_policy": "none",
            "gpu_policy": "off",
            "cover_policy": "off",
            "snap_policy": "none",
            "palette_type": "rgb",
            "encode_policy": "fast",
            "speed_warmups": warmups,
            "speed_runs": runs,
            "configuration_order_rotated_and_reversed": True,
            "sixel_environment_removed": clean_sixel_environment,
            "contract_preflight_points": preflight_points,
            "contract_preflight": (
                "requested quantizer, merge, binning, RGB888 work format, "
                "Oklab clustering, exact lookup, and zero quantizer retries"
            ),
            "quality_measurement": (
                "lsqa decoded SIXEL versus the source image"
            ),
            "size_measurement": "quality-pass SIXEL stdout byte length",
            "palette_timing": (
                "sum of complete top-level palette/build role=palette "
                "timeline spans"
            ),
            "merge_stage_timing": (
                "sum of palette/build role=palette/merge timeline spans"
            ),
            "end_to_end_timing": (
                "uninstrumented fresh-process monotonic wall time"
            ),
        },
        "chart_contract": {
            "question": (
                "Does Ward oversplit-and-merge, with or without Lloyd "
                "polishing, improve decoded quality enough to justify its "
                "runtime and SIXEL-size effects?"
            ),
            "family": "quantizer-faceted line charts",
            "x_axis": "palette colors K on a base-2 logarithmic scale",
            "quality_metrics": ["MS-SSIM", "mean Delta E00"],
            "performance_metrics": [
                "palette-build span",
                "fresh-process wall time",
                "SIXEL stream byte length",
            ],
            "non_color_distinction": "marker shape and line style",
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
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--output-csv", type=Path, required=True)
    parser.add_argument("--output-quality-plot", type=Path, required=True)
    parser.add_argument("--output-performance-plot", type=Path, required=True)
    parser.add_argument("--output-metadata", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the complete controlled merge-policy comparison."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    build_dir = (args.build_dir or source_root).resolve()
    input_image = args.input
    if not input_image.is_absolute():
        input_image = (source_root / input_image).resolve()
    if not input_image.is_file():
        raise FileNotFoundError(f"Input image does not exist: {input_image}")
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("Warmups must be non-negative and runs positive.")
    input_label = display_path(input_image, source_root)
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    lsqa = resolve_lsqa(args.lsqa, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    preflight_points = verify_contracts(
        img2sixel,
        input_image,
        command_env,
    )
    rows_by_key = measure_quality_and_size(
        img2sixel,
        lsqa,
        input_image,
        input_label,
        args.revision,
        command_env,
    )
    speed_by_key = measure_speed(
        img2sixel,
        input_image,
        args.warmups,
        args.runs,
        command_env,
    )
    for key, speed in speed_by_key.items():
        rows_by_key[key].update(speed)
    rows = list(rows_by_key.values())
    rows.sort(key=lambda row: (
        str(row["quantizer"]),
        int(row["colors"]),
        str(row["merge_config"]),
    ))
    fields = (
        "revision",
        "platform",
        "input",
        "colors",
        "quantizer",
        "quantizer_label",
        "quantize_option",
        "binning_policy",
        "merge_config",
        "merge_label",
        "merge_option",
        "merge_oversplit",
        "merge_lloyd",
        "MS-SSIM",
        "MS-SSIM_delta_vs_none",
        "Delta E00_mean",
        "Delta E00_delta_vs_none",
        "Delta Chroma_mean",
        "encoded_bytes",
        "bytes_ratio_vs_none",
        "runs",
        "palette_median_seconds",
        "palette_q1_seconds",
        "palette_q3_seconds",
        "palette_min_seconds",
        "palette_max_seconds",
        "palette_ratio_vs_none",
        "merge_stage_median_seconds",
        "merge_stage_q1_seconds",
        "merge_stage_q3_seconds",
        "merge_stage_min_seconds",
        "merge_stage_max_seconds",
        "end_to_end_median_seconds",
        "end_to_end_q1_seconds",
        "end_to_end_q3_seconds",
        "end_to_end_min_seconds",
        "end_to_end_max_seconds",
        "end_to_end_ratio_vs_none",
        "command",
        "timeline_command",
    )
    write_csv(args.output_csv, rows, fields)
    plot_quality(args.output_quality_plot, rows, input_image.name)
    plot_performance(
        args.output_performance_plot,
        rows,
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
        preflight_points,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
