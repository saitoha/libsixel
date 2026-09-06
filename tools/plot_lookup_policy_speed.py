#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Measure and plot end-to-end img2sixel lookup-policy runtime.

The benchmark deliberately uses the normal palette-generation path.  A fixed
palette loaded with ``-m`` currently disables optimized lookup, so it cannot
distinguish the accelerated policies from ``none`` through the CLI.
"""

from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import json
import os
import platform
import re
import shlex
import shutil
import statistics
import subprocess
import time
from pathlib import Path
from typing import Dict, List, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import ticker


DEFAULT_COLORS = "8,16,32,64,128,256"
DEFAULT_POLICIES = (
    "none,5bit,6bit,certlut,eytzinger,fhedt,vptree,rbc,mahalanobis"
)

# The Okabe-Ito palette is combined with markers and line styles so the figure
# remains legible with common color-vision deficiencies and in monochrome.
PLOT_COLORS = (
    "#4D4D4D",
    "#D55E00",
    "#0072B2",
    "#009E73",
    "#CC79A7",
    "#E69F00",
    "#56B4E9",
    "#F0E442",
    "#332288",
)
PLOT_MARKERS = ("o", "s", "^", "D", "v", "P", "X", "<", ">")
PLOT_LINESTYLES = (
    "-",
    "--",
    "-.",
    ":",
    (0, (5, 2)),
    (0, (3, 1, 1, 1)),
    (0, (1, 1)),
    (0, (5, 1, 1, 1)),
    (0, (3, 2, 1, 2)),
)


def parse_csv_strings(raw: str) -> List[str]:
    """Parse a non-empty comma-separated list."""
    values = [value.strip() for value in raw.split(",")]
    values = [value for value in values if value]
    if not values:
        raise ValueError("The list must not be empty.")
    return values


def parse_colors(raw: str) -> List[int]:
    """Parse positive palette sizes, preserving the requested order."""
    values = [int(value) for value in parse_csv_strings(raw)]
    if any(value <= 0 for value in values):
        raise ValueError("Palette sizes must be positive.")
    if len(values) != len(set(values)):
        raise ValueError("Palette sizes must not be repeated.")
    return values


def resolve_img2sixel(explicit: str | None, source_root: Path) -> str:
    """Resolve the img2sixel executable."""
    candidates: List[str] = []
    if explicit:
        candidates.append(explicit)
    env_value = os.environ.get("IMG2SIXEL_PATH")
    if env_value:
        candidates.append(env_value)
    candidates.append(str(source_root / "converters" / "img2sixel"))
    path_value = shutil.which("img2sixel")
    if path_value:
        candidates.append(path_value)

    for candidate in candidates:
        path = Path(candidate)
        if path.is_file() and os.access(path, os.X_OK):
            return str(path.resolve())
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    raise FileNotFoundError("Could not find an executable img2sixel.")


def percentile(values: Sequence[float], fraction: float) -> float:
    """Return a linearly interpolated percentile for a sorted sample."""
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def make_command(img2sixel: str,
                 input_image: Path,
                 colors: int,
                 policy: str,
                 diffusion: str) -> List[str]:
    """Build the controlled single-image benchmark command."""
    return [
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
        f"--lookup-policy={policy}",
        "-p",
        str(colors),
        "-o",
        os.devnull,
        str(input_image),
    ]


def make_command_environment(clean_sixel_environment: bool) -> Dict[str, str]:
    """Return the inherited environment with optional SIXEL_* isolation."""
    env = os.environ.copy()
    if clean_sixel_environment:
        for name in list(env):
            if name.startswith("SIXEL_"):
                del env[name]
    return env


def run_once(command: Sequence[str], env: Dict[str, str]) -> float:
    """Run one fresh process and return elapsed monotonic seconds."""
    started = time.perf_counter()
    proc = subprocess.run(
        list(command),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        env=env,
        check=False,
    )
    elapsed = time.perf_counter() - started
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"Command failed ({proc.returncode}): {shlex.join(command)}\n"
            f"{diagnostic}"
        )
    return elapsed


def require_work_format(command: Sequence[str],
                        env: Dict[str, str],
                        expected: str,
                        input_data: bytes | None = None) -> str:
    """Run a planner probe and require one effective working format."""
    trace_env = env.copy()
    trace_env["SIXEL_TRACE_TOPIC"] = "palette_contract"
    diagnostic_command = list(command[:-1]) + ["-v", command[-1]]
    proc = subprocess.run(
        diagnostic_command,
        input=input_data,
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
    if proc.returncode != 0:
        raise RuntimeError(
            f"Working-format preflight failed ({proc.returncode}):\n"
            f"{diagnostic.strip()}"
        )
    if formats != [expected]:
        raise RuntimeError(
            "Working-format preflight observed an unexpected format: "
            f"expected {expected}, observed {formats}"
        )
    return formats[0]


def measure(img2sixel: str,
            input_image: Path,
            input_label: str,
            colors: Sequence[int],
            policies: Sequence[str],
            diffusion: str,
            warmups: int,
            runs: int,
            revision: str,
            command_env: Dict[str, str]) -> List[Dict[str, object]]:
    """Measure each point with rotated policy order to limit time drift."""
    rows: List[Dict[str, object]] = []
    for color_count in colors:
        commands = {
            policy: make_command(
                img2sixel,
                input_image,
                color_count,
                policy,
                diffusion,
            )
            for policy in policies
        }
        for warmup in range(warmups):
            offset = warmup % len(policies)
            order = list(policies[offset:]) + list(policies[:offset])
            for policy in order:
                run_once(commands[policy], command_env)

        samples: Dict[str, List[float]] = {policy: [] for policy in policies}
        for run_index in range(runs):
            offset = run_index % len(policies)
            order = list(policies[offset:]) + list(policies[:offset])
            for policy in order:
                samples[policy].append(run_once(commands[policy], command_env))

        medians = {
            policy: statistics.median(samples[policy]) for policy in policies
        }
        none_median = medians["none"]
        for policy in policies:
            policy_samples = samples[policy]
            command_text = shlex.join(commands[policy])
            command_text = command_text.replace(img2sixel, "{img2sixel}", 1)
            command_text = command_text.replace(
                str(input_image),
                "{input}",
                1,
            )
            rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "input": input_label,
                    "colors": color_count,
                    "policy": policy,
                    "runs": runs,
                    "median_seconds": medians[policy],
                    "q1_seconds": percentile(policy_samples, 0.25),
                    "q3_seconds": percentile(policy_samples, 0.75),
                    "min_seconds": min(policy_samples),
                    "max_seconds": max(policy_samples),
                    "speedup_vs_none": none_median / medians[policy],
                    "command": command_text,
                }
            )
    return rows


def write_csv(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    """Write measurements with enough context to audit every point."""
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "revision",
        "platform",
        "input",
        "colors",
        "policy",
        "runs",
        "median_seconds",
        "q1_seconds",
        "q3_seconds",
        "min_seconds",
        "max_seconds",
        "speedup_vs_none",
        "command",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fieldnames,
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(rows)


def plot(path: Path,
         rows: Sequence[Dict[str, object]],
         colors: Sequence[int],
         policies: Sequence[str],
         title: str,
         runs: int) -> None:
    """Plot median runtime with its interquartile range."""
    figure, runtime_ax = plt.subplots(figsize=(9.0, 4.8))
    for index, policy in enumerate(policies):
        policy_rows = [row for row in rows if row["policy"] == policy]
        policy_rows.sort(key=lambda row: int(row["colors"]))
        median_ms = [float(row["median_seconds"]) * 1000.0 for row in policy_rows]
        q1_ms = [float(row["q1_seconds"]) * 1000.0 for row in policy_rows]
        q3_ms = [float(row["q3_seconds"]) * 1000.0 for row in policy_rows]
        lower = [median - q1 for median, q1 in zip(median_ms, q1_ms)]
        upper = [q3 - median for median, q3 in zip(median_ms, q3_ms)]
        style = {
            "color": PLOT_COLORS[index % len(PLOT_COLORS)],
            "marker": PLOT_MARKERS[index % len(PLOT_MARKERS)],
            "linestyle": PLOT_LINESTYLES[index % len(PLOT_LINESTYLES)],
        }
        runtime_ax.errorbar(
            colors,
            median_ms,
            yerr=[lower, upper],
            linewidth=1.8,
            markersize=5.5,
            capsize=2.5,
            label=policy,
            **style,
        )

    runtime_ax.set_title(title)
    runtime_ax.set_xlabel("Palette size K")
    runtime_ax.set_ylabel("Median elapsed time (ms)")
    runtime_ax.yaxis.set_major_formatter(ticker.StrMethodFormatter("{x:.0f}"))
    runtime_ax.set_xticks(colors)
    runtime_ax.grid(True, color="#D9D9D9", linewidth=0.7)
    runtime_ax.legend(ncol=3, frameon=False)
    figure.text(
        0.99,
        0.01,
        f"Fresh process per sample; median of {runs}; bars show IQR; lower is better",
        horizontalalignment="right",
        verticalalignment="bottom",
        fontsize=8,
        color="#555555",
    )

    figure.tight_layout(rect=(0.0, 0.035, 1.0, 1.0))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def file_sha256(path: Path) -> str:
    """Return a SHA-256 digest for one file without loading it at once."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def display_path(path: Path, source_root: Path) -> str:
    """Prefer a repository-relative path in checked-in metadata."""
    resolved = path.resolve()
    try:
        return str(resolved.relative_to(source_root.resolve()))
    except ValueError:
        return str(resolved)


def program_record(path_text: str, source_root: Path) -> Dict[str, object]:
    """Describe a program launcher and its libtool payload when present."""
    launcher = Path(path_text).resolve()
    payload = launcher.parent / ".libs" / launcher.name
    if not payload.is_file():
        payload = launcher
    return {
        "launcher": display_path(launcher, source_root),
        "launcher_sha256": file_sha256(launcher),
        "payload": display_path(payload, source_root),
        "payload_sha256": file_sha256(payload),
    }


def read_build_configuration(build_dir: Path) -> Dict[str, object]:
    """Read the generated Autotools configuration without guessing flags."""
    source_root = Path(__file__).resolve().parent.parent
    result: Dict[str, object] = {
        "directory": display_path(build_dir, source_root)
    }
    config_status = build_dir / "config.status"
    if config_status.is_file() and os.access(config_status, os.X_OK):
        proc = subprocess.run(
            [str(config_status), "--config"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            text=True,
        )
        if proc.returncode == 0:
            result["configure_arguments"] = proc.stdout.strip()

    makefile = build_dir / "Makefile"
    variables = ("CC", "CFLAGS", "CPPFLAGS", "LDFLAGS")
    if makefile.is_file():
        wanted = set(variables)
        with makefile.open("r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                for name in tuple(wanted):
                    prefix = f"{name} = "
                    if line.startswith(prefix):
                        result[name.lower()] = line[len(prefix):].rstrip("\n")
                        wanted.remove(name)
                        break
                if not wanted:
                    break
    compiler = result.get("cc")
    if isinstance(compiler, str) and compiler:
        proc = subprocess.run(
            [*shlex.split(compiler), "--version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
            text=True,
        )
        if proc.returncode == 0:
            result["compiler_version"] = proc.stdout.strip()
    return result


def resolve_metadata_program(explicit: str | None,
                             source_root: Path,
                             relative_path: str) -> str | None:
    """Resolve an optional metadata-only program path."""
    if explicit:
        path = Path(explicit)
    else:
        path = source_root / relative_path
    if path.is_file():
        return str(path.resolve())
    return None


def write_metadata(path: Path,
                   source_root: Path,
                   build_dir: Path,
                   input_image: Path,
                   input_label: str,
                   img2sixel: str,
                   lsqa: str | None,
                   colors: Sequence[int],
                   policies: Sequence[str],
                   diffusion: str,
                   warmups: int,
                   runs: int,
                   revision: str,
                   source_state: str,
                   clean_sixel_environment: bool) -> None:
    """Write common provenance for the quality and performance artifacts."""
    programs = {"img2sixel": program_record(img2sixel, source_root)}
    if lsqa is not None:
        programs["lsqa"] = program_record(lsqa, source_root)
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
        "programs": programs,
        "protocol": {
            "precision": "8bit",
            "required_work_format": "rgb888",
            "policies": list(policies),
            "diffusion": diffusion,
            "speed_colors": list(colors),
            "speed_warmups": warmups,
            "speed_runs": runs,
            "policy_order_rotated_each_round": True,
            "sixel_environment_removed": clean_sixel_environment,
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
    parser.add_argument("--colors", default=DEFAULT_COLORS)
    parser.add_argument("--policies", default=DEFAULT_POLICIES)
    parser.add_argument("--diffusion", choices=("none", "fs"), default="none")
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=9)
    parser.add_argument("--img2sixel")
    parser.add_argument("--lsqa")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", default="unknown")
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument("--output-csv", type=Path, required=True)
    parser.add_argument("--output-plot", type=Path, required=True)
    parser.add_argument("--output-metadata", type=Path)
    parser.add_argument("--title")
    return parser.parse_args()


def main() -> int:
    """Run the benchmark and produce its durable artifacts."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    input_image = args.input.resolve()
    input_label = display_path(input_image, source_root)
    colors = parse_colors(args.colors)
    policies = parse_csv_strings(args.policies)
    if "none" not in policies:
        raise ValueError("The policy list must contain none for the baseline.")
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("Warmups must be non-negative and runs must be positive.")
    if not input_image.is_file():
        raise FileNotFoundError(f"Input image does not exist: {input_image}")

    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    require_work_format(
        make_command(
            img2sixel,
            input_image,
            colors[0],
            policies[0],
            args.diffusion,
        ),
        command_env,
        "rgb888",
    )
    rows = measure(
        img2sixel,
        input_image,
        input_label,
        colors,
        policies,
        args.diffusion,
        args.warmups,
        args.runs,
        args.revision,
        command_env,
    )
    write_csv(args.output_csv, rows)
    title = args.title or (
        f"End-to-end lookup-policy runtime on {input_image.name}"
    )
    plot(args.output_plot, rows, colors, policies, title, args.runs)
    if args.output_metadata is not None:
        lsqa = resolve_metadata_program(
            args.lsqa,
            source_root,
            "assessment/lsqa",
        )
        write_metadata(
            args.output_metadata,
            source_root,
            (args.build_dir or source_root).resolve(),
            input_image,
            input_label,
            img2sixel,
            lsqa,
            colors,
            policies,
            args.diffusion,
            args.warmups,
            args.runs,
            args.revision,
            args.source_state,
            args.clean_sixel_environment,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
