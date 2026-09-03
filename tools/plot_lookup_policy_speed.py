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
import os
import platform
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
                 policy: str) -> List[str]:
    """Build the controlled single-image benchmark command."""
    return [
        img2sixel,
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "-Qkmeans:Gw",
        "-Xoklab",
        "-Wgamma",
        "--diffusion=none",
        "--gpu-policy=off",
        f"--lookup-policy={policy}",
        "-p",
        str(colors),
        "-o",
        os.devnull,
        str(input_image),
    ]


def run_once(command: Sequence[str]) -> float:
    """Run one fresh process and return elapsed monotonic seconds."""
    started = time.perf_counter()
    proc = subprocess.run(
        list(command),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
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


def measure(img2sixel: str,
            input_image: Path,
            input_label: str,
            colors: Sequence[int],
            policies: Sequence[str],
            warmups: int,
            runs: int,
            revision: str) -> List[Dict[str, object]]:
    """Measure each point with rotated policy order to limit time drift."""
    rows: List[Dict[str, object]] = []
    for color_count in colors:
        commands = {
            policy: make_command(img2sixel, input_image, color_count, policy)
            for policy in policies
        }
        for warmup in range(warmups):
            offset = warmup % len(policies)
            order = list(policies[offset:]) + list(policies[:offset])
            for policy in order:
                run_once(commands[policy])

        samples: Dict[str, List[float]] = {policy: [] for policy in policies}
        for run_index in range(runs):
            offset = run_index % len(policies)
            order = list(policies[offset:]) + list(policies[:offset])
            for policy in order:
                samples[policy].append(run_once(commands[policy]))

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


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--colors", default=DEFAULT_COLORS)
    parser.add_argument("--policies", default=DEFAULT_POLICIES)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=9)
    parser.add_argument("--img2sixel")
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--output-csv", type=Path, required=True)
    parser.add_argument("--output-plot", type=Path, required=True)
    parser.add_argument("--title")
    return parser.parse_args()


def main() -> int:
    """Run the benchmark and produce its durable artifacts."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    input_image = args.input.resolve()
    input_label = os.path.relpath(input_image, Path.cwd())
    colors = parse_colors(args.colors)
    policies = parse_csv_strings(args.policies)
    if "none" not in policies:
        raise ValueError("The policy list must contain none for the baseline.")
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("Warmups must be non-negative and runs must be positive.")
    if not input_image.is_file():
        raise FileNotFoundError(f"Input image does not exist: {input_image}")

    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    rows = measure(
        img2sixel,
        input_image,
        input_label,
        colors,
        policies,
        args.warmups,
        args.runs,
        args.revision,
    )
    write_csv(args.output_csv, rows)
    title = args.title or (
        f"End-to-end lookup-policy runtime on {input_image.name}"
    )
    plot(args.output_plot, rows, colors, policies, title, args.runs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
