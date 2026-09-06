#!/usr/bin/env python3
"""Measure and plot complete img2sixel quantize-model configurations."""

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
    require_work_format,
    resolve_img2sixel,
    run_once,
)


DEFAULT_COLORS = (8, 16, 32, 64, 128, 256)
QUANTIZE_CONFIGS: Tuple[Tuple[str, str, str, str], ...] = (
    (
        "heckbert-compat",
        "compat",
        "heckbert",
        "heckbert:profile=compat",
    ),
    (
        "heckbert-speed",
        "speed",
        "heckbert",
        "heckbert:profile=speed",
    ),
    (
        "heckbert-quality",
        "quality",
        "heckbert",
        "heckbert:profile=quality",
    ),
    ("kmeans", "k-means", "kmeans", "kmeans:seed=1"),
    ("medoids-pam", "PAM", "medoids", "medoids:algo=pam:seed=1"),
    (
        "medoids-sample",
        "CLARA",
        "medoids",
        "medoids:algo=sample:seed=1",
    ),
    (
        "medoids-random",
        "CLARANS",
        "medoids",
        "medoids:algo=random:seed=1",
    ),
    (
        "medoids-bandit",
        "BanditPAM",
        "medoids",
        "medoids:algo=bandit:seed=1",
    ),
    ("center-fft", "FFT", "center", "center:algo=fft:seed=1"),
    ("center-swap", "swap", "center", "center:algo=swap:seed=1"),
    (
        "center-hybrid",
        "hybrid",
        "center",
        "center:algo=hybrid:seed=1",
    ),
)
FAMILY_ORDER = ("heckbert", "kmeans", "medoids", "center")
FAMILY_TITLES = {
    "heckbert": "Heckbert profiles",
    "kmeans": "K-means",
    "medoids": "K-medoids algorithms",
    "center": "K-center algorithms",
}
STYLES = {
    "heckbert-compat": ("#56B4E9", "o", "--"),
    "heckbert-speed": ("#0072B2", "s", "-."),
    "heckbert-quality": ("#003F5C", "^", "-"),
    "kmeans": ("#D55E00", "D", "-"),
    "medoids-pam": ("#009E73", "o", "-"),
    "medoids-sample": ("#117733", "s", "--"),
    "medoids-random": ("#44AA99", "^", "-."),
    "medoids-bandit": ("#225522", "D", ":"),
    "center-fft": ("#E69F00", "o", "--"),
    "center-swap": ("#CC79A7", "s", "-."),
    "center-hybrid": ("#7A5195", "^", "-"),
}


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


def config_records() -> List[Dict[str, str]]:
    """Return named configuration records in their presentation order."""
    return [
        {
            "config": config,
            "label": label,
            "family": family,
            "quantize_option": option,
        }
        for config, label, family, option in QUANTIZE_CONFIGS
    ]


def make_command(img2sixel: str,
                 input_image: Path,
                 colors: int,
                 quantize_option: str,
                 discard_output: bool,
                 timeline_path: Path | None = None) -> List[str]:
    """Build one controlled complete-configuration command."""
    command = [
        img2sixel,
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=libpng!",
        f"--quantize-model={quantize_option}",
        "-Xoklab",
        "-Wgamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
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
    """Return a relocatable command string for one measurement row."""
    text = shlex.join(command)
    text = text.replace(img2sixel, "{img2sixel}", 1)
    text = text.replace(str(input_image), "{input}", 1)
    if timeline_path is not None:
        text = text.replace(str(timeline_path), "{timeline}", 1)
    return text


def run_quality_with_assessment_command(
        command: Sequence[str],
        lsqa: str,
        input_image: Path,
        command_env: Dict[str, str],
        lsqa_options: Sequence[str] = (),
) -> Tuple[Dict[str, float], int, List[str]]:
    """Encode once and return metrics, size, and the actual lsqa command."""
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
    with tempfile.TemporaryDirectory(prefix="lsqa-quantize-") as directory:
        lsqa_env = command_env.copy()
        # Assessment settings are part of the measurement protocol.  Do not
        # let a caller's interactive lsqa configuration silently change them.
        for name in list(lsqa_env):
            if name.startswith("LSQA_"):
                del lsqa_env[name]
        lsqa_env["LSQA_PREFIX"] = str(Path(directory) / "quality")
        lsqa_env["LSQA_VERBOSE"] = "0"
        assessment_command = [
            lsqa,
            *lsqa_options,
            str(input_image),
            "-",
        ]
        assessed = subprocess.run(
            assessment_command,
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
        return (
            {
                "MS-SSIM": float(quality["MS-SSIM"]),
                "Delta E00_mean": float(quality["Δ E00_mean"]),
            },
            len(encoded.stdout),
            assessment_command,
        )
    except (AttributeError, KeyError, TypeError, UnicodeDecodeError,
            ValueError, json.JSONDecodeError) as exc:
        raise RuntimeError("lsqa output lacks valid quality metrics") from exc


def run_quality(command: Sequence[str],
                lsqa: str,
                input_image: Path,
                command_env: Dict[str, str],
                lsqa_options: Sequence[str] = ()) \
        -> Tuple[Dict[str, float], int]:
    """Encode once and return perceptual metrics and the stream length."""
    metrics, encoded_bytes, _assessment_command = (
        run_quality_with_assessment_command(
            command,
            lsqa,
            input_image,
            command_env,
            lsqa_options,
        )
    )
    return metrics, encoded_bytes


def measure_quality_and_size(
        img2sixel: str,
        lsqa: str,
        input_image: Path,
        input_label: str,
        revision: str,
        command_env: Dict[str, str],
) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Measure one decoded stream for every configuration and K."""
    quality_rows: List[Dict[str, object]] = []
    size_rows: List[Dict[str, object]] = []
    records = config_records()
    for colors in DEFAULT_COLORS:
        color_size_rows: List[Dict[str, object]] = []
        for record in records:
            command = make_command(
                img2sixel,
                input_image,
                colors,
                record["quantize_option"],
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
                **record,
                "command": command_template(
                    command,
                    img2sixel,
                    input_image,
                ),
            }
            quality_rows.append(
                {
                    **common,
                    "MS-SSIM": metrics["MS-SSIM"],
                    "Delta E00_mean": metrics["Delta E00_mean"],
                }
            )
            color_size_rows.append(
                {
                    **common,
                    "encoded_bytes": encoded_bytes,
                }
            )
        baseline = next(
            int(row["encoded_bytes"])
            for row in color_size_rows
            if row["config"] == "heckbert-compat"
        )
        for row in color_size_rows:
            row["bytes_vs_compat"] = int(row["encoded_bytes"]) / baseline
            size_rows.append(row)
    return quality_rows, size_rows


def run_timeline(command: Sequence[str],
                 timeline_path: Path,
                 command_env: Dict[str, str]) -> float:
    """Run one logged encode and sum complete top-level palette spans."""
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
            f"Command failed ({proc.returncode}): {shlex.join(command)}\n"
            f"{diagnostic}"
        )
    starts: Dict[Tuple[int, int], float] = {}
    durations: List[float] = []
    with timeline_path.open("r", encoding="utf-8") as handle:
        for line in handle:
            event = json.loads(line)
            if event.get("worker") != "palette/build":
                continue
            if event.get("role") != "palette":
                continue
            key = (int(event["session_id"]), int(event["job"]))
            if event.get("event") == "start":
                if key in starts:
                    raise RuntimeError(f"duplicate palette start event: {key}")
                starts[key] = float(event["ts"])
            elif event.get("event") == "finish":
                if key not in starts:
                    raise RuntimeError(f"palette finish without start: {key}")
                duration = float(event["ts"]) - starts.pop(key)
                if duration < 0.0:
                    raise RuntimeError(f"negative palette duration: {key}")
                durations.append(duration)
    if starts:
        raise RuntimeError(f"unfinished palette spans: {sorted(starts)}")
    if not durations:
        raise RuntimeError("timeline contains no complete palette spans")
    return sum(durations)


def measure_speed(img2sixel: str,
                  input_image: Path,
                  input_label: str,
                  revision: str,
                  warmups: int,
                  runs: int,
                  command_env: Dict[str, str]) -> List[Dict[str, object]]:
    """Measure uninstrumented total time and instrumented palette spans."""
    rows: List[Dict[str, object]] = []
    records = config_records()
    names = [record["config"] for record in records]
    by_name = {record["config"]: record for record in records}
    for colors in DEFAULT_COLORS:
        commands = {
            name: make_command(
                img2sixel,
                input_image,
                colors,
                by_name[name]["quantize_option"],
                True,
            )
            for name in names
        }
        for warmup in range(warmups):
            offset = warmup % len(names)
            order = names[offset:] + names[:offset]
            for name in order:
                run_once(commands[name], command_env)
        end_samples: Dict[str, List[float]] = {name: [] for name in names}
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
                prefix="libsixel-quantize-timeline-") as directory:
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
                        by_name[name]["quantize_option"],
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
        end_baseline = end_medians["heckbert-compat"]
        palette_baseline = palette_medians["heckbert-compat"]
        for name in names:
            end_values = end_samples[name]
            palette_values = palette_samples[name]
            rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": colors,
                    **by_name[name],
                    "runs": runs,
                    "palette_median_seconds": palette_medians[name],
                    "palette_q1_seconds": percentile(palette_values, 0.25),
                    "palette_q3_seconds": percentile(palette_values, 0.75),
                    "palette_min_seconds": min(palette_values),
                    "palette_max_seconds": max(palette_values),
                    "palette_speedup_vs_compat": (
                        palette_baseline / palette_medians[name]
                    ),
                    "end_to_end_median_seconds": end_medians[name],
                    "end_to_end_q1_seconds": percentile(end_values, 0.25),
                    "end_to_end_q3_seconds": percentile(end_values, 0.75),
                    "end_to_end_min_seconds": min(end_values),
                    "end_to_end_max_seconds": max(end_values),
                    "end_to_end_speedup_vs_compat": (
                        end_baseline / end_medians[name]
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


def rows_for_config(rows: Sequence[Dict[str, object]],
                    config: str) -> List[Dict[str, object]]:
    """Return rows for one configuration ordered by palette size."""
    selected = [row for row in rows if row["config"] == config]
    selected.sort(key=lambda row: int(row["colors"]))
    return selected


def configs_for_family(family: str) -> List[Tuple[str, str]]:
    """Return configuration names and labels for one family."""
    return [
        (config, label)
        for config, label, selected_family, _option in QUANTIZE_CONFIGS
        if selected_family == family
    ]


def configure_x_axis(axis: plt.Axes) -> None:
    """Use exact powers of two for palette size."""
    axis.set_xscale("log", base=2)
    axis.set_xticks(DEFAULT_COLORS)
    axis.xaxis.set_major_formatter(ticker.ScalarFormatter())
    axis.grid(True, color="#D9D9D9", linewidth=0.7)


def plot_family_metric(axis: plt.Axes,
                       rows: Sequence[Dict[str, object]],
                       family: str,
                       metric: str,
                       divisor: float = 1.0,
                       error_prefix: str | None = None) -> None:
    """Plot one metric for all configurations in a family."""
    for config, label in configs_for_family(family):
        selected = rows_for_config(rows, config)
        color, marker, linestyle = STYLES[config]
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
                markersize=4.0,
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
                markersize=4.0,
                capsize=2.0,
                label=label,
            )
    configure_x_axis(axis)
    axis.legend(fontsize=7.5, frameon=False, loc="best")


def plot_quality(path: Path,
                 rows: Sequence[Dict[str, object]],
                 input_name: str) -> None:
    """Plot MS-SSIM and mean Delta E00 in family-grouped facets."""
    figure, axes = plt.subplots(
        2,
        4,
        figsize=(14.0, 6.8),
        sharex=True,
        sharey="row",
    )
    for column, family in enumerate(FAMILY_ORDER):
        plot_family_metric(axes[0][column], rows, family, "MS-SSIM")
        plot_family_metric(
            axes[1][column],
            rows,
            family,
            "Delta E00_mean",
        )
        axes[0][column].set_title(FAMILY_TITLES[family], fontsize=10)
        axes[1][column].set_xlabel("Palette size K")
    axes[0][0].set_ylabel("MS-SSIM (higher is better)")
    axes[1][0].set_ylabel("Mean Delta E00 (lower is better)")
    figure.suptitle(
        f"Quantize-model quality on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        "No dithering; exact palette lookup; shared scales within each row",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.96))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_speed(path: Path,
               rows: Sequence[Dict[str, object]],
               input_name: str,
               runs: int) -> None:
    """Plot instrumented palette spans and uninstrumented total latency."""
    figure, axes = plt.subplots(
        2,
        4,
        figsize=(14.0, 6.8),
        sharex=True,
        sharey="row",
    )
    for column, family in enumerate(FAMILY_ORDER):
        plot_family_metric(
            axes[0][column],
            rows,
            family,
            "palette_median_seconds",
            0.001,
            "palette",
        )
        plot_family_metric(
            axes[1][column],
            rows,
            family,
            "end_to_end_median_seconds",
            0.001,
            "end_to_end",
        )
        axes[0][column].set_yscale("log")
        axes[1][column].set_yscale("log")
        axes[0][column].set_title(FAMILY_TITLES[family], fontsize=10)
        axes[1][column].set_xlabel("Palette size K")
    axes[0][0].set_ylabel("Palette-build span (ms, log)")
    axes[1][0].set_ylabel("End-to-end time (ms, log)")
    figure.suptitle(
        f"Quantize-model runtime on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        f"Median of {runs}; bars show IQR; fresh process per sample; "
        "lower is better",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.96))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_size(path: Path,
              rows: Sequence[Dict[str, object]],
              input_name: str) -> None:
    """Plot exact SIXEL stream size in family-grouped facets."""
    figure, axes = plt.subplots(
        1,
        4,
        figsize=(14.0, 3.7),
        sharex=True,
        sharey=True,
    )
    for column, family in enumerate(FAMILY_ORDER):
        plot_family_metric(
            axes[column],
            rows,
            family,
            "encoded_bytes",
            1024.0,
        )
        axes[column].set_title(FAMILY_TITLES[family], fontsize=10)
        axes[column].set_xlabel("Palette size K")
    axes[0].set_ylabel("SIXEL stream size (KiB)")
    figure.suptitle(
        f"Quantize-model output size on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        "Same no-dither streams used for quality measurement; lower is smaller",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.93))
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
    """Write provenance and the controlled comparison protocol."""
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
            "comparison_scope": "complete explicit -Q configurations",
            "colors": list(DEFAULT_COLORS),
            "configurations": config_records(),
            "auto_omission": (
                "auto currently selects Heckbert compatibility behavior; "
                "heckbert:profile=compat is the measured reference"
            ),
            "threads": 1,
            "precision": "8bit",
            "required_work_format": "rgb888",
            "quality": "full",
            "loader": "libpng!",
            "clustering_colorspace": "oklab",
            "working_colorspace": "gamma",
            "diffusion": "none",
            "lookup_policy": "none",
            "gpu_policy": "off",
            "merge_policy": "configuration default",
            "cover_policy": "configuration default",
            "speed_warmups": warmups,
            "speed_runs": runs,
            "configuration_order_rotated_each_round": True,
            "sixel_environment_removed": clean_sixel_environment,
            "size_measurement": "quality-pass SIXEL stdout byte length",
            "palette_timing": (
                "sum of complete top-level palette/build role=palette "
                "timeline spans, including multiple attempts or fallbacks"
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
    parser.add_argument("--output-size-csv", type=Path, required=True)
    parser.add_argument("--output-size-plot", type=Path, required=True)
    parser.add_argument("--output-speed-csv", type=Path, required=True)
    parser.add_argument("--output-speed-plot", type=Path, required=True)
    parser.add_argument("--output-metadata", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the complete quantize-model comparison."""
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
            QUANTIZE_CONFIGS[0][3],
            True,
        ),
        command_env,
        "rgb888",
    )
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
    common_fields = (
        "revision",
        "platform",
        "input",
        "colors",
        "config",
        "label",
        "family",
        "quantize_option",
    )
    write_csv(
        args.output_quality_csv,
        quality_rows,
        common_fields + ("MS-SSIM", "Delta E00_mean", "command"),
    )
    write_csv(
        args.output_size_csv,
        size_rows,
        common_fields + ("encoded_bytes", "bytes_vs_compat", "command"),
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
            "palette_speedup_vs_compat",
            "end_to_end_median_seconds",
            "end_to_end_q1_seconds",
            "end_to_end_q3_seconds",
            "end_to_end_min_seconds",
            "end_to_end_max_seconds",
            "end_to_end_speedup_vs_compat",
            "command",
            "timeline_command",
        ),
    )
    plot_quality(args.output_quality_plot, quality_rows, input_image.name)
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
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
