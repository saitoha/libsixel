#!/usr/bin/env python3
"""Measure and plot independent palette sampling and binning policies."""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import platform
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
    resolve_lsqa,
    run_quality_with_assessment_command,
    run_timeline,
    write_csv,
)


DEFAULT_COLORS = (8, 16, 32, 64, 128, 256)
DEFAULT_LOADER_ORDER = "libpng!"
CMS_LOADER_ORDER = "libpng:cms_engine=builtin!"
CMS_REFERENCE_LOADER_ORDER = "libpng,builtin!"
CMS_REFERENCE_LOADER_ENVIRONMENT = {
    "SIXEL_LOADER_LIBPNG_CMS_ENGINE": "builtin",
}
SAMPLE_TARGET = 16384
QUANTIZE_OPTION = (
    "kmeans:inittype=none:threshold=0.125:binbits=6:mapping=uniform:"
    "softdist=trilinear:feedback=0:prune=hamerly:seed=1:"
    "restarts=1:iter=20:miniter=0:polish_iter=0:feedback_slots=1:"
    f"feedback_interval=1:sample_target={SAMPLE_TARGET}"
)
PIPELINE_CONFIGS: Tuple[Tuple[str, str, str], ...] = (
    ("full-frame-hard", "full-frame", "hard"),
    ("adaptive-grid-hard", "adaptive-grid", "hard"),
    ("full-frame-none", "full-frame", "none"),
    ("full-frame-exact", "full-frame", "exact"),
    ("full-frame-soft", "full-frame", "soft"),
)
AXIS_ORDER = ("sampling", "binning")
AXIS_TITLES = {
    "sampling": "Sampling policy (hard binning)",
    "binning": "Binning policy (full-frame sampling)",
}
AXIS_CONFIGS = {
    "sampling": (
        ("full-frame-hard", "full frame"),
        ("adaptive-grid-hard", "adaptive grid"),
    ),
    "binning": (
        ("full-frame-none", "none"),
        ("full-frame-exact", "exact"),
        ("full-frame-hard", "hard"),
        ("full-frame-soft", "soft"),
    ),
}
STYLES = {
    ("sampling", "full-frame-hard"): ("#0072B2", "o", "-"),
    ("sampling", "adaptive-grid-hard"): ("#D55E00", "s", "--"),
    ("binning", "full-frame-none"): ("#333333", "o", "-"),
    ("binning", "full-frame-exact"): ("#0072B2", "s", "--"),
    ("binning", "full-frame-hard"): ("#D55E00", "^", "-."),
    ("binning", "full-frame-soft"): ("#009E73", "D", ":"),
}


def config_records() -> List[Dict[str, object]]:
    """Return each physical configuration exactly once."""
    return [
        {
            "config": config,
            "sampling_policy": sampling,
            "binning_policy": binning,
            "comparison_axes": [
                axis
                for axis in AXIS_ORDER
                if any(
                    axis_config == config
                    for axis_config, _label in AXIS_CONFIGS[axis]
                )
            ],
        }
        for config, sampling, binning in PIPELINE_CONFIGS
    ]


def measurement_records() -> List[Dict[str, str]]:
    """Return physical configurations without presentation-only fields."""
    return [
        {
            "config": config,
            "sampling_policy": sampling,
            "binning_policy": binning,
        }
        for config, sampling, binning in PIPELINE_CONFIGS
    ]


def quality_reference_options(loader_order: str) -> Tuple[str, ...]:
    """Return lsqa options that mirror the encoder's controlled loading."""
    if loader_order == DEFAULT_LOADER_ORDER:
        return ()
    return (
        f"--loaders={CMS_REFERENCE_LOADER_ORDER}",
        "--env",
        "SIXEL_LOADER_LIBPNG_CMS_ENGINE=builtin",
    )


def make_command(img2sixel: str,
                 input_image: Path,
                 colors: int,
                 record: Dict[str, str],
                 discard_output: bool,
                 loader_order: str = DEFAULT_LOADER_ORDER,
                 timeline_path: Path | None = None) -> List[str]:
    """Build one command with every non-compared axis controlled."""
    command = [
        img2sixel,
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        f"--loaders={loader_order}",
        f"--quantize-model={QUANTIZE_OPTION}:"
        f"sampling_policy={record['sampling_policy']}:"
        f"binning_policy={record['binning_policy']}",
        "-Xoklab",
        "-Wgamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "-F",
        "none",
        "-a",
        "off",
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
    """Return a relocatable command string for a measurement row."""
    replacements = {
        img2sixel: "{img2sixel}",
        str(input_image): "{input}",
        os.devnull: "{devnull}",
    }
    if timeline_path is not None:
        replacements[str(timeline_path)] = "{timeline}"
    return shlex.join([replacements.get(token, token) for token in command])


def assessment_command_template(command: Sequence[str],
                                lsqa: str,
                                input_image: Path) -> str:
    """Return a relocatable lsqa command exactly as it was executed."""
    replacements = {
        lsqa: "{lsqa}",
        str(input_image): "{input}",
    }
    return shlex.join([replacements.get(token, token) for token in command])


def williams_orders(names: Sequence[str]) -> List[List[str]]:
    """Return a Williams design balanced for first-order carryover."""
    count = len(names)
    first = [0]
    for step in range(1, count):
        if step % 2:
            first.append((step + 1) // 2)
        else:
            first.append(count - step // 2)
    orders = [
        [names[(index + shift) % count] for index in first]
        for shift in range(count)
    ]
    if count % 2:
        orders.extend([list(reversed(order)) for order in orders])
    return orders


def measure_quality_and_size(
        img2sixel: str,
        lsqa: str,
        input_image: Path,
        input_label: str,
        revision: str,
        command_env: Dict[str, str],
        loader_order: str = DEFAULT_LOADER_ORDER,
) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Measure one decoded stream for every policy and palette size."""
    quality_rows: List[Dict[str, object]] = []
    size_rows: List[Dict[str, object]] = []
    records = measurement_records()
    for colors in DEFAULT_COLORS:
        for record in records:
            command = make_command(
                img2sixel,
                input_image,
                colors,
                record,
                False,
                loader_order,
            )
            metrics, encoded_bytes, assessment_command = (
                run_quality_with_assessment_command(
                    command,
                    lsqa,
                    input_image,
                    command_env,
                    quality_reference_options(loader_order),
                )
            )
            common: Dict[str, object] = {
                "revision": revision,
                "platform": platform.platform(),
                "input": input_label,
                "colors": colors,
                **record,
                "quantize_option": QUANTIZE_OPTION,
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
                    "assessment_command": assessment_command_template(
                        assessment_command,
                        lsqa,
                        input_image,
                    ),
                }
            )
            size_rows.append(
                {
                    **common,
                    "encoded_bytes": encoded_bytes,
                }
            )
    return quality_rows, size_rows


def run_speed_command(domain: str,
                      img2sixel: str,
                      input_image: Path,
                      colors: int,
                      record: Dict[str, str],
                      command_env: Dict[str, str],
                      timeline_path: Path,
                      loader_order: str = DEFAULT_LOADER_ORDER) \
        -> Tuple[float, str]:
    """Run one timing process and return its raw duration and command."""
    if domain == "end-to-end":
        command = make_command(
            img2sixel,
            input_image,
            colors,
            record,
            True,
            loader_order,
        )
        return (
            run_once(command, command_env),
            command_template(command, img2sixel, input_image),
        )
    command = make_command(
        img2sixel,
        input_image,
        colors,
        record,
        True,
        loader_order,
        timeline_path,
    )
    return (
        run_timeline(command, timeline_path, command_env),
        command_template(
            command,
            img2sixel,
            input_image,
            timeline_path,
        ),
    )


def measure_speed(img2sixel: str,
                  input_image: Path,
                  input_label: str,
                  revision: str,
                  warmups: int,
                  runs: int,
                  command_env: Dict[str, str],
                  loader_order: str = DEFAULT_LOADER_ORDER) \
        -> List[Dict[str, object]]:
    """Record every raw timing observation in its executed order."""
    rows: List[Dict[str, object]] = []
    records = measurement_records()
    names = [record["config"] for record in records]
    by_name = {record["config"]: record for record in records}
    orders = williams_orders(names)
    washout_name = "full-frame-hard"
    with tempfile.TemporaryDirectory(
            prefix="libsixel-pipeline-timeline-") as directory:
        timeline_dir = Path(directory)
        for colors in DEFAULT_COLORS:
            for domain in ("end-to-end", "palette-build"):
                for round_index in range(warmups + runs):
                    sequence = round_index % len(orders)
                    phase = (
                        "warmup" if round_index < warmups else "measured"
                    )
                    timeline_path = timeline_dir / (
                        f"{domain}-{colors}-{round_index}-washout.jsonl"
                    )
                    duration, command_text = run_speed_command(
                        domain,
                        img2sixel,
                        input_image,
                        colors,
                        by_name[washout_name],
                        command_env,
                        timeline_path,
                        loader_order,
                    )
                    rows.append(
                        {
                            "revision": revision,
                            "platform": platform.platform(),
                            "input": input_label,
                            "colors": colors,
                            **by_name[washout_name],
                            "quantize_option": QUANTIZE_OPTION,
                            "domain": domain,
                            "phase": "washout",
                            "round": round_index,
                            "sequence": sequence,
                            "position": -1,
                            "seconds": duration,
                            "command": command_text,
                        }
                    )
                    for position, name in enumerate(orders[sequence]):
                        timeline_path = timeline_dir / (
                            f"{domain}-{colors}-{round_index}-{name}.jsonl"
                        )
                        duration, command_text = run_speed_command(
                            domain,
                            img2sixel,
                            input_image,
                            colors,
                            by_name[name],
                            command_env,
                            timeline_path,
                            loader_order,
                        )
                        rows.append(
                            {
                                "revision": revision,
                                "platform": platform.platform(),
                                "input": input_label,
                                "colors": colors,
                                **by_name[name],
                                "quantize_option": QUANTIZE_OPTION,
                                "domain": domain,
                                "phase": phase,
                                "round": round_index,
                                "sequence": sequence,
                                "position": position,
                                "seconds": duration,
                                "command": command_text,
                            }
                        )
    return rows


def summarize_speed(
        rows: Sequence[Dict[str, object]]) -> List[Dict[str, object]]:
    """Derive plotting statistics from measured raw timing rows."""
    summary: List[Dict[str, object]] = []
    for config, _sampling, _binning in PIPELINE_CONFIGS:
        for colors in DEFAULT_COLORS:
            record: Dict[str, object] = {
                "config": config,
                "colors": colors,
            }
            for prefix, domain in (
                    ("end_to_end", "end-to-end"),
                    ("palette", "palette-build")):
                values = [
                    float(row["seconds"])
                    for row in rows
                    if row["config"] == config
                    and int(row["colors"]) == colors
                    and row["domain"] == domain
                    and row["phase"] == "measured"
                ]
                record[f"{prefix}_median_seconds"] = statistics.median(
                    values
                )
                record[f"{prefix}_q1_seconds"] = percentile(values, 0.25)
                record[f"{prefix}_q3_seconds"] = percentile(values, 0.75)
            summary.append(record)
    return summary


def rows_for_config(rows: Sequence[Dict[str, object]],
                    config: str) -> List[Dict[str, object]]:
    """Return rows for one configuration ordered by palette size."""
    selected = [row for row in rows if row["config"] == config]
    selected.sort(key=lambda row: int(row["colors"]))
    return selected


def configs_for_axis(axis: str) -> List[Tuple[str, str]]:
    """Return configuration names and labels for one comparison axis."""
    return list(AXIS_CONFIGS[axis])


def configure_x_axis(axis: plt.Axes) -> None:
    """Use exact powers of two for palette size."""
    axis.set_xscale("log", base=2)
    axis.set_xticks(DEFAULT_COLORS)
    axis.xaxis.set_major_formatter(ticker.ScalarFormatter())
    axis.grid(True, color="#D9D9D9", linewidth=0.7)


def plot_axis_metric(axis: plt.Axes,
                     rows: Sequence[Dict[str, object]],
                     comparison_axis: str,
                     metric: str,
                     divisor: float = 1.0,
                     error_prefix: str | None = None) -> None:
    """Plot one metric for every policy on one controlled axis."""
    for config, label in configs_for_axis(comparison_axis):
        selected = rows_for_config(rows, config)
        color, marker, linestyle = STYLES[(comparison_axis, config)]
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
    axis.legend(fontsize=8.0, frameon=False, loc="best")


def plot_quality(path: Path,
                 rows: Sequence[Dict[str, object]],
                 input_name: str) -> None:
    """Plot both quality metrics in sampling and binning facets."""
    figure, axes = plt.subplots(
        2,
        2,
        figsize=(10.5, 7.0),
        sharex=True,
        sharey="row",
    )
    for column, comparison_axis in enumerate(AXIS_ORDER):
        plot_axis_metric(
            axes[0][column],
            rows,
            comparison_axis,
            "MS-SSIM",
        )
        plot_axis_metric(
            axes[1][column],
            rows,
            comparison_axis,
            "Delta E00_mean",
        )
        axes[0][column].set_title(AXIS_TITLES[comparison_axis], fontsize=10)
        axes[1][column].set_xlabel("Palette size K")
    axes[0][0].set_ylabel("MS-SSIM (higher is better)")
    axes[1][0].set_ylabel("Mean Delta E00 (lower is better)")
    figure.suptitle(
        f"Palette pipeline quality on {input_name}",
        y=0.995,
    )
    figure.text(
        0.99,
        0.008,
        "Fixed K-means; no dithering; exact palette lookup; shared row scales",
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
    """Plot palette-build spans and uninstrumented total latency."""
    figure, axes = plt.subplots(
        2,
        2,
        figsize=(10.5, 7.0),
        sharex=True,
        sharey="row",
    )
    for column, comparison_axis in enumerate(AXIS_ORDER):
        plot_axis_metric(
            axes[0][column],
            rows,
            comparison_axis,
            "palette_median_seconds",
            0.001,
            "palette",
        )
        plot_axis_metric(
            axes[1][column],
            rows,
            comparison_axis,
            "end_to_end_median_seconds",
            0.001,
            "end_to_end",
        )
        axes[0][column].set_yscale("log")
        axes[1][column].set_yscale("log")
        axes[0][column].set_title(AXIS_TITLES[comparison_axis], fontsize=10)
        axes[1][column].set_xlabel("Palette size K")
    axes[0][0].set_ylabel("Palette-build span (ms, log)")
    axes[1][0].set_ylabel("End-to-end time (ms, log)")
    figure.suptitle(
        f"Palette pipeline runtime on {input_name}",
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
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.96))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_size(path: Path,
              rows: Sequence[Dict[str, object]],
              input_name: str) -> None:
    """Plot exact SIXEL stream size in both comparison facets."""
    figure, axes = plt.subplots(
        1,
        2,
        figsize=(10.5, 4.0),
        sharex=True,
        sharey=True,
    )
    for column, comparison_axis in enumerate(AXIS_ORDER):
        plot_axis_metric(
            axes[column],
            rows,
            comparison_axis,
            "encoded_bytes",
            1024.0,
        )
        axes[column].set_title(AXIS_TITLES[comparison_axis], fontsize=10)
        axes[column].set_xlabel("Palette size K")
    axes[0].set_ylabel("SIXEL stream size (KiB)")
    figure.suptitle(
        f"Palette pipeline output size on {input_name}",
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
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.93))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def configured_source_root(build_dir: Path) -> Path:
    """Read the source root recorded by the generated Autotools Makefile."""
    makefile = build_dir / "Makefile"
    prefix = "abs_top_srcdir = "
    if not makefile.is_file():
        raise FileNotFoundError(f"Build Makefile does not exist: {makefile}")
    with makefile.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.startswith(prefix):
                continue
            value = line[len(prefix):].strip()
            if not value or "$" in value:
                break
            path = Path(value)
            if not path.is_absolute():
                path = build_dir / path
            return path.resolve()
    raise ValueError(f"Cannot resolve abs_top_srcdir from {makefile}")


def require_build_program(name: str, program: str, build_dir: Path) -> None:
    """Reject an executable that does not belong to the selected build."""
    path = Path(program).resolve()
    try:
        path.relative_to(build_dir)
    except ValueError as exc:
        raise ValueError(
            f"{name} does not belong to the selected build directory: "
            f"{path}"
        ) from exc


def git_source_snapshot(source_root: Path) -> Dict[str, str]:
    """Capture the revision and tracked state used by a measurement."""
    git = os.environ.get("GIT", "git")
    revision_proc = subprocess.run(
        [git, "-C", str(source_root), "rev-parse", "HEAD"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        text=True,
    )
    if revision_proc.returncode != 0:
        raise RuntimeError(revision_proc.stderr.strip())
    diff_proc = subprocess.run(
        [
            git,
            "-C",
            str(source_root),
            "diff",
            "--binary",
            "--no-ext-diff",
            "HEAD",
            "--",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if diff_proc.returncode != 0:
        raise RuntimeError(diff_proc.stderr.decode(errors="replace").strip())
    state = "dirty" if diff_proc.stdout else "clean"
    return {
        "revision": revision_proc.stdout.strip(),
        "tracked_worktree_state_at_start": state,
        "tracked_diff_sha256": hashlib.sha256(diff_proc.stdout).hexdigest(),
    }


def read_active_make_assignment(makefile: Path, name: str) -> str:
    """Read one active logical assignment from an Automake Makefile."""
    prefix = f"{name} = "
    with makefile.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.startswith(prefix):
                continue
            value = line[len(prefix):].rstrip()
            while value.endswith("\\"):
                value = value[:-1].rstrip() + " "
                try:
                    continuation = next(handle)
                except StopIteration as exc:
                    raise ValueError(
                        f"Unterminated {name} assignment in {makefile}"
                    ) from exc
                value += continuation.strip()
            return value.strip()
    raise ValueError(f"Cannot resolve {name} from {makefile}")


def libsixel_linkage(build_dir: Path, source_root: Path) -> Dict[str, object]:
    """Resolve whether the measured tools embed or link local libsixel."""
    assignments = (
        read_active_make_assignment(
            build_dir / "converters" / "Makefile", "img2sixel_LDADD"
        ),
        read_active_make_assignment(
            build_dir / "assessment" / "Makefile", "lsqa_LDADD"
        ),
    )
    if all("AMALGAMATION_" in value for value in assignments):
        return {"mode": "amalgamated", "artifacts": []}
    if not all("src/libsixel.la" in value for value in assignments):
        raise ValueError(
            "img2sixel and lsqa do not use one recognized libsixel link mode"
        )
    libtool_archive = build_dir / "src" / "libsixel.la"
    values = {}
    with libtool_archive.open(
            "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            for name in ("dlname", "old_library"):
                prefix = f"{name}='"
                stripped = line.rstrip("\n")
                if stripped.startswith(prefix) and stripped.endswith("'"):
                    values[name] = stripped[len(prefix):-1]
    role = "shared"
    filename = values.get("dlname", "")
    if not filename:
        role = "static"
        filename = values.get("old_library", "")
    artifact = build_dir / "src" / ".libs" / filename
    if not filename or not artifact.is_file():
        raise FileNotFoundError(
            f"Cannot resolve the linked libsixel artifact from {libtool_archive}"
        )
    resolved = artifact.resolve()
    return {
        "mode": "library",
        "artifacts": [
            {
                "role": role,
                "path": display_path(resolved, source_root),
                "sha256": file_sha256(resolved),
            }
        ],
    }


def artifact_snapshot(source_root: Path,
                      build_dir: Path,
                      input_image: Path,
                      img2sixel: str,
                      lsqa: str) -> Dict[str, object]:
    """Capture input and executable identities around the long run."""
    return {
        "input_sha256": file_sha256(input_image),
        "programs": {
            "img2sixel": program_record(img2sixel, source_root),
            "lsqa": program_record(lsqa, source_root),
        },
        "libsixel_linkage": libsixel_linkage(build_dir, source_root),
    }


def write_metadata(path: Path,
                   source_root: Path,
                   build_dir: Path,
                   build_source_root: Path,
                   input_label: str,
                   warmups: int,
                   runs: int,
                   revision: str,
                   source_state: str,
                   source_diff_sha256: str,
                   measurement_mode: str,
                   clean_sixel_environment: bool,
                   loader_order: str,
                   measured_artifacts: Dict[str, object]) -> None:
    """Write provenance and the controlled comparison protocol."""
    build_record = read_build_configuration(build_dir)
    build_record["configured_source_directory"] = str(build_source_root)
    build_record["source_directory_matches"] = (
        build_source_root == source_root
    )
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "source": {
            "revision": revision,
            "tracked_worktree_state_at_start": source_state,
            "tracked_diff_sha256": source_diff_sha256,
            "top_source_directory": str(source_root),
            "end_snapshot_matches": True,
        },
        "build": build_record,
        "host": {
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "matplotlib": matplotlib.__version__,
        },
        "input": {
            "path": input_label,
            "sha256": measured_artifacts["input_sha256"],
        },
        "programs": measured_artifacts["programs"],
        "libsixel_linkage": measured_artifacts["libsixel_linkage"],
        "protocol": {
            "measurement_mode": measurement_mode,
            "comparison_scope": (
                "independent palette sampling and binning policy axes"
            ),
            "colors": list(DEFAULT_COLORS),
            "configurations": config_records(),
            "comparison_facets": AXIS_CONFIGS,
            "threads": 1,
            "precision": "8bit",
            "required_work_format": "rgb888",
            "quality": "full",
            "loader": loader_order,
            "quality_reference_loader": (
                "automatic"
                if loader_order == DEFAULT_LOADER_ORDER
                else CMS_REFERENCE_LOADER_ORDER
            ),
            "quality_reference_loader_environment": (
                {}
                if loader_order == DEFAULT_LOADER_ORDER
                else CMS_REFERENCE_LOADER_ENVIRONMENT
            ),
            "quantize_option": QUANTIZE_OPTION,
            "sample_target": SAMPLE_TARGET,
            "clustering_colorspace": "oklab",
            "working_colorspace": "gamma",
            "diffusion": "none",
            "lookup_policy": "none",
            "gpu_policy": "off",
            "merge_policy": "none",
            "cover_policy": "off",
            "speed_warmups": warmups,
            "speed_runs": runs,
            "timing_order_design": {
                "name": "Williams first-order carryover balance",
                "physical_configurations": len(PIPELINE_CONFIGS),
                "sequence_period": len(williams_orders([
                    config for config, _sampling, _binning
                    in PIPELINE_CONFIGS
                ])),
                "boundary_washout": (
                    "one recorded but excluded full-frame-hard process "
                    "before every sequence"
                ),
            },
            "sixel_environment_removed": clean_sixel_environment,
            "lsqa_environment_removed": True,
            "provenance_rechecked_after_measurement": True,
            "libsixel_linkage_snapshotted": True,
            "size_measurement": "quality-pass SIXEL stdout byte length",
            "palette_timing": (
                "sum of complete top-level palette/build role=palette "
                "timeline spans, including multiple attempts or fallbacks"
            ),
            "end_to_end_timing": (
                "uninstrumented fresh-process monotonic wall time"
            ),
            "speed_rows": (
                "one raw duration per domain, K, round, position, and "
                "recorded boundary washout"
            ),
            "interpretation_limit": (
                "one fixture characterizes mechanisms but cannot select "
                "a project default"
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
    parser.add_argument(
        "--measurement-mode",
        choices=("durable", "exploratory"),
        default="durable",
    )
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument(
        "--loader-order",
        choices=(DEFAULT_LOADER_ORDER, CMS_LOADER_ORDER),
        default=DEFAULT_LOADER_ORDER,
    )
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--output-quality-csv", type=Path, required=True)
    parser.add_argument("--output-quality-plot", type=Path, required=True)
    parser.add_argument("--output-size-csv", type=Path, required=True)
    parser.add_argument("--output-size-plot", type=Path, required=True)
    parser.add_argument("--output-speed-csv", type=Path, required=True)
    parser.add_argument("--output-speed-plot", type=Path, required=True)
    parser.add_argument("--output-metadata", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the independent palette-pipeline comparison."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    build_dir = (args.build_dir or source_root).resolve()
    build_source_root = configured_source_root(build_dir)
    if build_source_root != source_root:
        raise ValueError(
            "Build directory was configured from a different source tree: "
            f"{build_source_root} != {source_root}"
        )
    input_image = args.input.resolve()
    if not input_image.is_file():
        raise FileNotFoundError(f"Input image does not exist: {input_image}")
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("Warmups must be non-negative and runs positive.")
    if args.measurement_mode == "durable":
        if args.warmups != 2 or args.runs != 10:
            raise ValueError(
                "Durable measurements require 2 warm-ups and 10 runs."
            )
        if args.source_state != "clean":
            raise ValueError(
                "Durable measurements require a clean tracked worktree."
            )
        if not args.clean_sixel_environment:
            raise ValueError(
                "Durable measurements require a clean SIXEL environment."
            )
    input_label = display_path(input_image, source_root)
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    lsqa = resolve_lsqa(args.lsqa, source_root)
    require_build_program("img2sixel", img2sixel, build_dir)
    require_build_program("lsqa", lsqa, build_dir)
    source_snapshot = git_source_snapshot(source_root)
    if source_snapshot["revision"] != args.revision:
        raise ValueError(
            "Recorded revision does not match the measurement source tree."
        )
    if (
        source_snapshot["tracked_worktree_state_at_start"]
        != args.source_state
    ):
        raise ValueError(
            "Recorded source state does not match the measurement source tree."
        )
    measured_artifacts = artifact_snapshot(
        source_root,
        build_dir,
        input_image,
        img2sixel,
        lsqa,
    )
    command_env = make_command_environment(args.clean_sixel_environment)
    require_work_format(
        make_command(
            img2sixel,
            input_image,
            DEFAULT_COLORS[0],
            measurement_records()[0],
            True,
            args.loader_order,
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
        args.loader_order,
    )
    speed_rows = measure_speed(
        img2sixel,
        input_image,
        input_label,
        args.revision,
        args.warmups,
        args.runs,
        command_env,
        args.loader_order,
    )
    if git_source_snapshot(source_root) != source_snapshot:
        raise RuntimeError("Source revision or tracked state changed during run.")
    if artifact_snapshot(
            source_root,
            build_dir,
            input_image,
            img2sixel,
            lsqa,
    ) != measured_artifacts:
        raise RuntimeError("Input or measured executable changed during run.")
    speed_summary = summarize_speed(speed_rows)
    common_fields = (
        "revision",
        "platform",
        "input",
        "colors",
        "config",
        "sampling_policy",
        "binning_policy",
        "quantize_option",
    )
    write_csv(
        args.output_quality_csv,
        quality_rows,
        common_fields + (
            "MS-SSIM",
            "Delta E00_mean",
            "command",
            "assessment_command",
        ),
    )
    write_csv(
        args.output_size_csv,
        size_rows,
        common_fields + (
            "encoded_bytes",
            "command",
        ),
    )
    write_csv(
        args.output_speed_csv,
        speed_rows,
        common_fields + (
            "domain",
            "phase",
            "round",
            "sequence",
            "position",
            "seconds",
            "command",
        ),
    )
    plot_quality(args.output_quality_plot, quality_rows, input_image.name)
    plot_size(args.output_size_plot, size_rows, input_image.name)
    plot_speed(
        args.output_speed_plot,
        speed_summary,
        input_image.name,
        args.runs,
    )
    write_metadata(
        args.output_metadata,
        source_root,
        build_dir,
        build_source_root,
        input_label,
        args.warmups,
        args.runs,
        args.revision,
        args.source_state,
        source_snapshot["tracked_diff_sha256"],
        args.measurement_mode,
        args.clean_sixel_environment,
        args.loader_order,
        measured_artifacts,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
