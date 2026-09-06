#!/usr/bin/env python3
"""Measure the cross-cutting 8-bit and float32 encoder precision axis."""

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

import plot_clustering_colorspace_measurements as clustering_measurements
import plot_dither_policy_measurements as dither_measurements
import plot_lookup_policy_speed as lookup_measurements
import plot_palette_pipeline_measurements as pipeline_measurements
import plot_quantize_model_measurements as quantizer_measurements
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
from plot_quantize_model_measurements import resolve_lsqa


PALETTE_COLORS = 64
PRECISIONS: Tuple[Tuple[str, str, str], ...] = (
    ("8bit", "8-bit", "rgb888"),
    ("float32", "float32", "rgb-f32"),
)
DOMAIN_ORDER = (
    "quantizer",
    "dither",
    "lookup",
    "palette-pipeline",
    "clustering-colorspace",
)
DOMAIN_TITLES = {
    "quantizer": "Quantize model",
    "dither": "Dither policy",
    "lookup": "Lookup policy",
    "palette-pipeline": "Sampling and binning",
    "clustering-colorspace": "Clustering color space",
}
PRECISION_STYLES = {
    "8bit": ("#0072B2", "o", "8-bit"),
    "float32": ("#D55E00", "s", "float32"),
}


def measurement_records() -> List[Dict[str, str]]:
    """Return every controlled configuration in presentation order."""
    records: List[Dict[str, str]] = []
    for record in quantizer_measurements.config_records():
        records.append(
            {
                "domain": "quantizer",
                "config": str(record["config"]),
                "label": (
                    f"{record['family']} / {record['label']}"
                    if record["family"] == "heckbert"
                    else str(record["label"])
                ),
                "option": str(record["quantize_option"]),
            }
        )
    for method, option in dither_measurements.DITHER_METHODS:
        records.append(
            {
                "domain": "dither",
                "config": method,
                "label": method,
                "option": option,
            }
        )
    for policy in lookup_measurements.parse_csv_strings(
            lookup_measurements.DEFAULT_POLICIES):
        records.append(
            {
                "domain": "lookup",
                "config": policy,
                "label": policy,
                "option": policy,
            }
        )
    for record in pipeline_measurements.measurement_records():
        records.append(
            {
                "domain": "palette-pipeline",
                "config": record["config"],
                "label": record["config"],
                "option": (
                    f"sampling={record['sampling_policy']};"
                    f"binning={record['binning_policy']}"
                ),
            }
        )
    for record in clustering_measurements.colorspace_records():
        records.append(
            {
                "domain": "clustering-colorspace",
                "config": str(record["colorspace"]),
                "label": str(record["label"]),
                "option": f"-X{record['colorspace']}",
            }
        )
    return records


def replace_option(command: List[str], prefix: str, replacement: str) -> None:
    """Replace one required long option in place."""
    matches = [index for index, token in enumerate(command)
               if token.startswith(prefix)]
    if len(matches) != 1:
        raise ValueError(f"Expected one {prefix} option in command.")
    command[matches[0]] = replacement


def pipeline_record(config: str) -> Dict[str, str]:
    """Resolve one named sampling and binning configuration."""
    for record in pipeline_measurements.measurement_records():
        if record["config"] == config:
            return record
    raise ValueError(f"Unknown palette-pipeline configuration: {config}")


def make_command(img2sixel: str,
                 input_image: Path,
                 record: Dict[str, str],
                 precision: str,
                 discard_output: bool) -> List[str]:
    """Reuse each topic's controlled command and replace only precision."""
    domain = record["domain"]
    if domain == "quantizer":
        command = quantizer_measurements.make_command(
            img2sixel,
            input_image,
            PALETTE_COLORS,
            record["option"],
            discard_output,
        )
    elif domain == "dither":
        command = dither_measurements.make_command(
            img2sixel,
            input_image,
            PALETTE_COLORS,
            record["option"],
            discard_output,
        )
    elif domain == "lookup":
        command = lookup_measurements.make_command(
            img2sixel,
            input_image,
            PALETTE_COLORS,
            record["option"],
            "none",
        )
        if not discard_output:
            output_index = command.index("-o")
            del command[output_index:output_index + 2]
    elif domain == "palette-pipeline":
        command = pipeline_measurements.make_command(
            img2sixel,
            input_image,
            PALETTE_COLORS,
            pipeline_record(record["config"]),
            discard_output,
            loader_order="builtin!",
        )
    elif domain == "clustering-colorspace":
        command = clustering_measurements.make_command(
            img2sixel,
            input_image,
            PALETTE_COLORS,
            record["config"],
            discard_output,
        )
    else:
        raise ValueError(f"Unknown measurement domain: {domain}")
    replace_option(command, "--precision=", f"--precision={precision}")
    replace_option(command, "--loaders=", "--loaders=builtin!")
    return command


def command_template(command: Sequence[str],
                     program: str,
                     program_placeholder: str,
                     input_image: Path) -> str:
    """Return a relocatable command string for one CSV row."""
    replacements = {
        program: program_placeholder,
        str(input_image): "{input}",
        os.devnull: "{devnull}",
    }
    return shlex.join([replacements.get(token, token) for token in command])


def preflight_command(command: Sequence[str],
                      command_env: Dict[str, str],
                      expected_work_format: str) -> Tuple[str, int]:
    """Verify the effective work format and absence of quantizer fallback."""
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
    work_formats = re.findall(
        r"formats: source=\S+ work=(\S+) scale_out=\S+",
        diagnostic,
    )
    retries = [int(value) for value in re.findall(
        r"quantizer_retries=(\d+)", diagnostic
    )]
    if proc.returncode != 0:
        raise RuntimeError(
            f"Precision preflight failed ({proc.returncode}):\n"
            f"{diagnostic.strip()}"
        )
    if work_formats != [expected_work_format]:
        raise RuntimeError(
            "Precision preflight observed an unexpected work format: "
            f"expected {expected_work_format}, observed {work_formats}"
        )
    if not retries or any(retry != 0 for retry in retries):
        raise RuntimeError(
            "Precision preflight observed a missing or nonzero quantizer "
            f"retry count: {retries}"
        )
    return work_formats[0], max(retries)


def run_quality(command: Sequence[str],
                lsqa: str,
                input_image: Path,
                command_env: Dict[str, str]) \
        -> Tuple[Dict[str, float], int, str]:
    """Encode once and return quality, exact stream size, and provenance."""
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
            prefix="lsqa-precision-") as directory:
        lsqa_env = command_env.copy()
        for name in list(lsqa_env):
            if name.startswith("LSQA_"):
                del lsqa_env[name]
        lsqa_env["LSQA_PREFIX"] = str(Path(directory) / "quality")
        lsqa_env["LSQA_VERBOSE"] = "0"
        assessment_command = [lsqa, str(input_image), "-"]
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
        metrics = {
            "MS-SSIM": float(quality["MS-SSIM"]),
            "Delta E00_mean": float(quality["Δ E00_mean"]),
            "Delta Chroma_mean": float(quality["Δ Chroma_mean"]),
        }
    except (AttributeError, KeyError, TypeError, UnicodeDecodeError,
            ValueError, json.JSONDecodeError) as exc:
        raise RuntimeError("lsqa output lacks valid quality metrics") from exc
    assessment_template = command_template(
        assessment_command,
        lsqa,
        "{lsqa}",
        input_image,
    )
    return metrics, len(encoded.stdout), assessment_template


def measure_quality(img2sixel: str,
                    lsqa: str,
                    input_image: Path,
                    input_label: str,
                    revision: str,
                    command_env: Dict[str, str]) \
        -> List[Dict[str, object]]:
    """Measure quality and size after preflighting every configuration."""
    rows: List[Dict[str, object]] = []
    expected_formats = {
        precision: work_format
        for precision, _label, work_format in PRECISIONS
    }
    for record in measurement_records():
        print(
            f"quality: {record['domain']}/{record['config']}",
            flush=True,
        )
        for precision, precision_label, _work_format in PRECISIONS:
            quality_command = make_command(
                img2sixel,
                input_image,
                record,
                precision,
                False,
            )
            speed_command = make_command(
                img2sixel,
                input_image,
                record,
                precision,
                True,
            )
            work_format, retries = preflight_command(
                speed_command,
                command_env,
                expected_formats[precision],
            )
            metrics, encoded_bytes, assessment_command = run_quality(
                quality_command,
                lsqa,
                input_image,
                command_env,
            )
            rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": PALETTE_COLORS,
                    **record,
                    "precision": precision,
                    "precision_label": precision_label,
                    "work_format": work_format,
                    "preflight_quantizer_retries": retries,
                    **metrics,
                    "encoded_bytes": encoded_bytes,
                    "command": command_template(
                        quality_command,
                        img2sixel,
                        "{img2sixel}",
                        input_image,
                    ),
                    "assessment_command": assessment_command,
                    "speed_command": command_template(
                        speed_command,
                        img2sixel,
                        "{img2sixel}",
                        input_image,
                    ),
                }
            )
    return rows


def measure_speed(rows: List[Dict[str, object]],
                  img2sixel: str,
                  input_image: Path,
                  warmups: int,
                  runs: int,
                  command_env: Dict[str, str]) -> None:
    """Measure adjacent precision pairs with alternating execution order."""
    records = measurement_records()
    samples: Dict[Tuple[str, str, str], List[float]] = {
        (record["domain"], record["config"], precision): []
        for record in records
        for precision, _label, _work_format in PRECISIONS
    }
    precision_names = [precision for precision, _label, _format in PRECISIONS]
    for round_index in range(warmups + runs):
        phase = "warmup" if round_index < warmups else "measured"
        print(
            f"speed: {phase} round {round_index + 1}/{warmups + runs}",
            flush=True,
        )
        for domain in DOMAIN_ORDER:
            domain_records = [record for record in records
                              if record["domain"] == domain]
            offset = round_index % len(domain_records)
            ordered = domain_records[offset:] + domain_records[:offset]
            if round_index % 2:
                ordered.reverse()
            for position, record in enumerate(ordered):
                precisions = list(precision_names)
                if (round_index + position) % 2:
                    precisions.reverse()
                for precision in precisions:
                    command = make_command(
                        img2sixel,
                        input_image,
                        record,
                        precision,
                        True,
                    )
                    elapsed = run_once(command, command_env)
                    if round_index >= warmups:
                        samples[(domain, record["config"], precision)].append(
                            elapsed
                        )
    for row in rows:
        key = (
            str(row["domain"]),
            str(row["config"]),
            str(row["precision"]),
        )
        values = samples[key]
        row.update(
            {
                "speed_runs": runs,
                "median_seconds": statistics.median(values),
                "q1_seconds": percentile(values, 0.25),
                "q3_seconds": percentile(values, 0.75),
                "min_seconds": min(values),
                "max_seconds": max(values),
            }
        )


def add_pair_comparisons(rows: List[Dict[str, object]]) -> None:
    """Add changes relative to the matching 8-bit row."""
    pairs: Dict[Tuple[str, str], Dict[str, Dict[str, object]]] = {}
    for row in rows:
        key = (str(row["domain"]), str(row["config"]))
        pairs.setdefault(key, {})[str(row["precision"])] = row
    for pair in pairs.values():
        baseline = pair["8bit"]
        for row in pair.values():
            row["MS-SSIM_delta_vs_8bit"] = (
                float(row["MS-SSIM"]) - float(baseline["MS-SSIM"])
            )
            row["Delta E00_delta_vs_8bit"] = (
                float(row["Delta E00_mean"])
                - float(baseline["Delta E00_mean"])
            )
            row["encoded_size_ratio_vs_8bit"] = (
                float(row["encoded_bytes"])
                / float(baseline["encoded_bytes"])
            )
            row["speed_ratio_vs_8bit"] = (
                float(row["median_seconds"])
                / float(baseline["median_seconds"])
            )


def write_csv(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    """Write one auditable row per configuration and precision."""
    fieldnames = (
        "revision", "platform", "input", "colors", "domain", "config",
        "label", "option", "precision", "precision_label", "work_format",
        "preflight_quantizer_retries", "MS-SSIM",
        "MS-SSIM_delta_vs_8bit", "Delta E00_mean",
        "Delta E00_delta_vs_8bit", "Delta Chroma_mean", "encoded_bytes",
        "encoded_size_ratio_vs_8bit", "speed_runs", "median_seconds",
        "q1_seconds", "q3_seconds", "min_seconds", "max_seconds",
        "speed_ratio_vs_8bit", "command", "assessment_command",
        "speed_command",
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def domain_rows(rows: Sequence[Dict[str, object]],
                domain: str) -> List[Dict[str, object]]:
    """Return one domain in manifest and precision order."""
    by_key = {
        (str(row["config"]), str(row["precision"])): row
        for row in rows if row["domain"] == domain
    }
    ordered: List[Dict[str, object]] = []
    for record in measurement_records():
        if record["domain"] != domain:
            continue
        for precision, _label, _work_format in PRECISIONS:
            ordered.append(by_key[(record["config"], precision)])
    return ordered


def metric_values(rows: Sequence[Dict[str, object]],
                  precision: str,
                  field: str,
                  scale: float = 1.0) -> List[float]:
    """Return one plotted metric in configuration order."""
    return [float(row[field]) * scale
            for row in rows if row["precision"] == precision]


def plot_metric(axis: plt.Axes,
                rows: Sequence[Dict[str, object]],
                field: str,
                title: str,
                scale: float = 1.0,
                error_fields: Tuple[str, str] | None = None) -> None:
    """Draw a paired horizontal dot comparison for one metric."""
    positions = list(range(len(rows) // len(PRECISIONS)))
    offsets = {"8bit": -0.10, "float32": 0.10}
    values = {
        precision: metric_values(rows, precision, field, scale)
        for precision, _label, _work_format in PRECISIONS
    }
    for position, left, right in zip(
            positions, values["8bit"], values["float32"]):
        axis.plot(
            (left, right),
            (position + offsets["8bit"], position + offsets["float32"]),
            color="#B8B8B8",
            linewidth=1.0,
            zorder=1,
        )
    for precision, _precision_label, _work_format in PRECISIONS:
        color, marker, label = PRECISION_STYLES[precision]
        marker_positions = [position + offsets[precision]
                            for position in positions]
        xerr = None
        if error_fields is not None:
            lower = metric_values(rows, precision, error_fields[0], scale)
            upper = metric_values(rows, precision, error_fields[1], scale)
            xerr = (
                [value - low for value, low in zip(values[precision], lower)],
                [high - value for value, high in zip(values[precision], upper)],
            )
        axis.errorbar(
            values[precision], marker_positions, xerr=xerr, fmt=marker,
            color=color,
            markerfacecolor=(color if precision == "8bit" else "white"),
            markeredgecolor=color, markersize=5.5,
            capsize=2.0 if xerr is not None else 0.0, linewidth=1.0,
            label=label, zorder=2,
        )
    axis.set_title(title, fontsize=10, loc="left")
    axis.grid(axis="x", color="#DDDDDD", linewidth=0.7)
    axis.set_axisbelow(True)
    axis.spines[["top", "right", "left"]].set_visible(False)
    axis.tick_params(axis="y", length=0)


def plot_domain(path: Path,
                rows: Sequence[Dict[str, object]],
                domain: str,
                input_name: str,
                runs: int) -> None:
    """Plot one domain as four aligned paired-dot panels."""
    selected = domain_rows(rows, domain)
    labels = [str(row["label"])
              for row in selected if row["precision"] == "8bit"]
    height = max(8.0, 0.48 * len(labels) + 3.0)
    figure, axes = plt.subplots(2, 2, figsize=(15.5, height), sharey=True)
    plot_metric(axes[0][0], selected, "MS-SSIM", "MS-SSIM (higher is better)")
    plot_metric(
        axes[0][1], selected, "Delta E00_mean",
        "Mean Delta E00 (lower is better)",
    )
    plot_metric(
        axes[1][0], selected, "median_seconds",
        f"End-to-end latency, median of {runs} (ms)", 1000.0,
        ("q1_seconds", "q3_seconds"),
    )
    plot_metric(
        axes[1][1], selected, "encoded_bytes", "SIXEL stream size (KiB)",
        1.0 / 1024.0,
    )
    positions = list(range(len(labels)))
    for axis in axes[:, 0]:
        axis.set_yticks(positions, labels)
    axes[0][0].invert_yaxis()
    for axis in axes[:, 1]:
        axis.tick_params(labelleft=False)
    axes[0][0].xaxis.set_major_formatter(ticker.FormatStrFormatter("%.5f"))
    axes[0][1].xaxis.set_major_formatter(ticker.FormatStrFormatter("%.3f"))
    axes[1][0].xaxis.set_major_formatter(ticker.FormatStrFormatter("%.1f"))
    axes[1][1].xaxis.set_major_formatter(ticker.FormatStrFormatter("%.1f"))
    handles, legend_labels = axes[0][0].get_legend_handles_labels()
    figure.legend(
        handles, legend_labels, loc="upper right", bbox_to_anchor=(0.98, 0.965),
        frameon=False, ncol=2,
    )
    figure.suptitle(
        f"{DOMAIN_TITLES[domain]}: 8-bit and float32 precision", x=0.07,
        y=0.975, ha="left", fontsize=15, fontweight="bold",
    )
    figure.text(
        0.07, 0.935,
        (
            f"{input_name}, K={PALETTE_COLORS}, one thread, builtin loader, "
            "gamma working space, GPU off. Whiskers show timing IQR."
        ),
        ha="left", va="top", fontsize=9, color="#444444",
    )
    figure.subplots_adjust(
        left=0.20, right=0.98, top=0.82, bottom=0.08,
        hspace=0.30, wspace=0.18,
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180)
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
                   row_count: int) -> None:
    """Write provenance and the cross-cutting comparison contract."""
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
        "input": {"path": input_label, "sha256": file_sha256(input_image)},
        "programs": {
            "img2sixel": program_record(img2sixel, source_root),
            "lsqa": program_record(lsqa, source_root),
        },
        "protocol": {
            "comparison_scope": (
                "controlled K=64 cross-section of every documented "
                "quantizer, dither, lookup, sampling/binning, and "
                "clustering-color-space configuration"
            ),
            "colors": PALETTE_COLORS,
            "precisions": [
                {
                    "precision": precision,
                    "label": label,
                    "required_work_format": work_format,
                }
                for precision, label, work_format in PRECISIONS
            ],
            "domains": measurement_records(),
            "threads": 1,
            "quality": "full",
            "loader": "builtin!",
            "working_colorspace": "gamma",
            "gpu_policy": "off",
            "speed_warmups": warmups,
            "speed_runs": runs,
            "precision_pair_order_alternated_each_round": True,
            "configuration_order_rotated_and_reversed": True,
            "sixel_environment_removed": clean_sixel_environment,
            "preflight": (
                "every row must report its required planner work format and "
                "quantizer_retries=0"
            ),
            "quality_metrics": (
                "lsqa MS-SSIM, mean Delta E00, and mean Delta Chroma"
            ),
            "size_measurement": "quality-pass SIXEL stdout byte length",
            "end_to_end_timing": (
                "uninstrumented fresh-process monotonic wall time"
            ),
            "interpretation_limit": (
                "this single K and fixture expose precision sensitivity; "
                "they do not replace each topic's palette-size sweep"
            ),
        },
        "artifacts": {
            "csv": "precision-comparison.csv",
            "plots": [f"precision-{domain}.png" for domain in DOMAIN_ORDER],
            "row_count": row_count,
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
    parser.add_argument("--output-directory", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the cross-cutting precision comparison."""
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
    rows = measure_quality(
        img2sixel, lsqa, input_image, input_label, args.revision, command_env,
    )
    measure_speed(
        rows, img2sixel, input_image, args.warmups, args.runs, command_env,
    )
    add_pair_comparisons(rows)
    output_directory = args.output_directory.resolve()
    write_csv(output_directory / "precision-comparison.csv", rows)
    for domain in DOMAIN_ORDER:
        plot_domain(
            output_directory / f"precision-{domain}.png", rows, domain,
            input_image.name, args.runs,
        )
    write_metadata(
        output_directory / "precision-run.json", source_root, build_dir,
        input_image, input_label, img2sixel, lsqa, args.warmups, args.runs,
        args.revision, args.source_state, args.clean_sixel_environment,
        len(rows),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
