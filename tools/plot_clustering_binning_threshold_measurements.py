#!/usr/bin/env python3
"""Measure effective-point sufficiency for high-K palette clustering."""

from __future__ import annotations

import argparse
import datetime
import json
import platform
import re
import statistics
import subprocess
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import ticker
from matplotlib.lines import Line2D

from plot_clustering_colorspace_measurements import (
    BINBITS,
    STYLES,
    colorspace_records,
    make_command,
    run_quality,
)
from plot_lookup_policy_speed import (
    display_path,
    file_sha256,
    make_command_environment,
    percentile,
    program_record,
    read_build_configuration,
    require_work_format,
    resolve_img2sixel,
)
from plot_palette_pipeline_measurements import (
    configured_source_root,
    git_source_snapshot,
    libsixel_linkage,
)
from plot_palette_pipeline_suite_measurements import load_manifest
from plot_quantize_model_measurements import (
    command_template,
    resolve_lsqa,
    write_csv,
)


DEFAULT_COLORS = (64, 128, 256)
DEFAULT_SEEDS = (1, 2, 3, 4, 5)
PASS_RATE_TARGET = 0.8
CRITERIA: Tuple[Tuple[str, float, float], ...] = (
    ("strict", 0.001, 0.05),
    ("balanced", 0.002, 0.15),
    ("relaxed", 0.005, 0.25),
)
TRACE_PATTERN = re.compile(
    r"LSXBSTAT1\|bits=(\d+)\|source_points=(\d+)\|"
    r"effective_points=(\d+)"
)
PLAN_POINTS_PATTERN = re.compile(
    r"LSXBPS1\|[^\n]*\|points=(\d+)\|quantizer_retries=0"
)


def content_fixtures(fixtures: Sequence[Dict[str, object]]) \
        -> List[Dict[str, object]]:
    """Return the content-comparison fixtures in manifest order."""
    return [fixture for fixture in fixtures if "content" in fixture["roles"]]


def parse_integer_list(value: str, name: str) -> Tuple[int, ...]:
    """Parse a non-empty, positive, duplicate-free integer list."""
    try:
        result = tuple(int(item) for item in value.split(","))
    except ValueError as exc:
        raise ValueError(f"{name} must contain comma-separated integers") \
            from exc
    if not result or any(item < 1 for item in result):
        raise ValueError(f"{name} must contain positive integers")
    if len(result) != len(set(result)):
        raise ValueError(f"{name} must not contain duplicates")
    return result


def measurement_configs() -> List[Tuple[str, int]]:
    """Return hard-grid candidates followed by the unaggregated baseline."""
    return [("hard", bits) for bits in BINBITS] + [("none", 0)]


def rotate(values: Sequence[Tuple[str, int]], offset: int) \
        -> List[Tuple[str, int]]:
    """Rotate a sequence to spread process-order effects deterministically."""
    position = offset % len(values)
    return list(values[position:]) + list(values[:position])


def trace_population(img2sixel: str,
                     fixture: Dict[str, object],
                     colorspace: str,
                     contract_value: int,
                     policy: str,
                     binbits: int,
                     command_env: Dict[str, str]) -> Dict[str, object]:
    """Measure one filter population and verify the requested palette path."""
    command_bits = binbits if policy == "hard" else 6
    command = make_command(
        img2sixel,
        Path(fixture["resolved_path"]),
        max(DEFAULT_COLORS),
        colorspace,
        True,
        binning_policy=policy,
        binbits=command_bits,
        seed=DEFAULT_SEEDS[0],
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
    matches = TRACE_PATTERN.findall(diagnostic)
    plan_points = PLAN_POINTS_PATTERN.findall(diagnostic)
    required = (
        "model=kmeans",
        "merge=none",
        "lut=none",
        f"cluster={contract_value}",
        "quantizer_retries=0",
        f"binning_effective={policy}",
    )
    if (
        proc.returncode != 0
        or (policy == "hard" and len(matches) != 1)
        or (policy == "none" and len(plan_points) != 1)
        or not all(token in diagnostic for token in required)
    ):
        raise RuntimeError(
            "Population preflight failed for "
            f"{fixture['id']}, {colorspace}, {policy}, bits={binbits}:\n"
            f"{diagnostic.strip()}"
        )
    if policy == "hard":
        traced_bits, source_points, effective_points = (
            int(value) for value in matches[0]
        )
    else:
        traced_bits = 0
        source_points = int(plan_points[0])
        effective_points = source_points
    expected_bits = binbits if policy == "hard" else 0
    if (
        traced_bits != expected_bits
        or source_points < 1
        or not 1 <= effective_points <= source_points
        or (policy == "none" and effective_points != source_points)
    ):
        raise RuntimeError(
            "Population trace is inconsistent for "
            f"{fixture['id']}, {colorspace}, {policy}, bits={binbits}"
        )
    return {
        "source_points": source_points,
        "effective_points": effective_points,
        "trace_command": command_template(
            command,
            img2sixel,
            Path(fixture["resolved_path"]),
        ),
    }


def measure_populations(fixtures: Sequence[Dict[str, object]],
                        img2sixel: str,
                        command_env: Dict[str, str]) \
        -> Dict[Tuple[str, str, str, int], Dict[str, object]]:
    """Measure policy populations once per fixture and clustering space."""
    result: Dict[Tuple[str, str, str, int], Dict[str, object]] = {}
    total = len(fixtures) * len(colorspace_records()) * 6
    index = 0
    for fixture in fixtures:
        for record in colorspace_records():
            colorspace = str(record["colorspace"])
            for policy, binbits in measurement_configs():
                index += 1
                print(
                    f"[population {index}/{total}] {fixture['id']} "
                    f"{colorspace} {policy}:{binbits}",
                    flush=True,
                )
                result[(
                    str(fixture["id"]),
                    colorspace,
                    policy,
                    binbits,
                )] = trace_population(
                    img2sixel,
                    fixture,
                    colorspace,
                    int(record["contract_value"]),
                    policy,
                    binbits,
                    command_env,
                )
    return result


def quality_row(fixture: Dict[str, object],
                revision: str,
                colors: int,
                colorspace: str,
                label: object,
                seed: int,
                policy: str,
                binbits: int,
                population: Dict[str, object],
                metrics: Dict[str, float],
                encoded_bytes: int,
                command_value: str) -> Dict[str, object]:
    """Build one raw paired-quality record."""
    effective_points = int(population["effective_points"])
    return {
        "revision": revision,
        "platform": platform.platform(),
        "fixture_id": fixture["id"],
        "fixture_label": fixture["label"],
        "fixture_class": fixture["class"],
        "input": fixture["path"],
        "width": fixture["width"],
        "height": fixture["height"],
        "colors": colors,
        "colorspace": colorspace,
        "label": label,
        "seed": seed,
        "binning_policy": policy,
        "binbits": binbits,
        "source_points": population["source_points"],
        "effective_points": effective_points,
        "effective_points_per_color": effective_points / colors,
        **metrics,
        "encoded_bytes": encoded_bytes,
        "command": command_value,
        "trace_command": population["trace_command"],
    }


def measure_quality(fixtures: Sequence[Dict[str, object]],
                    img2sixel: str,
                    lsqa: str,
                    revision: str,
                    colors_values: Sequence[int],
                    seeds: Sequence[int],
                    populations: Dict[
                        Tuple[str, str, str, int], Dict[str, object]
                    ],
                    command_env: Dict[str, str]) \
        -> List[Dict[str, object]]:
    """Measure paired hard-grid and unaggregated quality across the suite."""
    rows: List[Dict[str, object]] = []
    configs = measurement_configs()
    total = (
        len(fixtures)
        * len(colorspace_records())
        * len(colors_values)
        * len(seeds)
        * len(configs)
    )
    index = 0
    for fixture_index, fixture in enumerate(fixtures):
        input_image = Path(fixture["resolved_path"])
        for color_index, colors in enumerate(colors_values):
            for space_index, record in enumerate(colorspace_records()):
                colorspace = str(record["colorspace"])
                for seed_index, seed in enumerate(seeds):
                    order = rotate(
                        configs,
                        fixture_index + color_index + space_index + seed_index,
                    )
                    for policy, binbits in order:
                        index += 1
                        print(
                            f"[quality {index}/{total}] {fixture['id']} "
                            f"K={colors} {colorspace} seed={seed} "
                            f"{policy}:{binbits}",
                            flush=True,
                        )
                        command_bits = binbits if policy == "hard" else 6
                        command = make_command(
                            img2sixel,
                            input_image,
                            colors,
                            colorspace,
                            False,
                            binning_policy=policy,
                            binbits=command_bits,
                            seed=seed,
                        )
                        metrics, encoded_bytes = run_quality(
                            command,
                            lsqa,
                            input_image,
                            command_env,
                        )
                        population = populations[(
                            str(fixture["id"]),
                            colorspace,
                            policy,
                            binbits,
                        )]
                        rows.append(
                            quality_row(
                                fixture,
                                revision,
                                colors,
                                colorspace,
                                record["label"],
                                seed,
                                policy,
                                binbits,
                                population,
                                metrics,
                                encoded_bytes,
                                command_template(
                                    command,
                                    img2sixel,
                                    input_image,
                                ),
                            )
                        )
    return rows


def median(values: Iterable[float]) -> float:
    """Return a median after materializing an iterable."""
    return statistics.median(list(values))


def summarize(raw_rows: Sequence[Dict[str, object]]) \
        -> List[Dict[str, object]]:
    """Summarize paired quality regret for every hard-grid observation."""
    baseline = {
        (
            str(row["fixture_id"]),
            int(row["colors"]),
            str(row["colorspace"]),
            int(row["seed"]),
        ): row
        for row in raw_rows
        if row["binning_policy"] == "none"
    }
    grouped: Dict[
        Tuple[str, int, str, int], List[Dict[str, float]]
    ] = {}
    source_rows: Dict[Tuple[str, int, str, int], Dict[str, object]] = {}
    for row in raw_rows:
        if row["binning_policy"] != "hard":
            continue
        key = (
            str(row["fixture_id"]),
            int(row["colors"]),
            str(row["colorspace"]),
            int(row["binbits"]),
        )
        base = baseline[(key[0], key[1], key[2], int(row["seed"]))]
        grouped.setdefault(key, []).append(
            {
                "MS-SSIM_regret": (
                    float(base["MS-SSIM"]) - float(row["MS-SSIM"])
                ),
                "Delta E00_regret": (
                    float(row["Delta E00_mean"])
                    - float(base["Delta E00_mean"])
                ),
                "Delta Chroma_regret": (
                    float(row["Delta Chroma_mean"])
                    - float(base["Delta Chroma_mean"])
                ),
                "encoded_size_ratio": (
                    int(row["encoded_bytes"]) / int(base["encoded_bytes"])
                ),
            }
        )
        source_rows[key] = row
    result: List[Dict[str, object]] = []
    for key in sorted(grouped):
        values = grouped[key]
        source = source_rows[key]
        output: Dict[str, object] = {
            name: source[name]
            for name in (
                "revision",
                "platform",
                "fixture_id",
                "fixture_label",
                "fixture_class",
                "input",
                "width",
                "height",
                "colors",
                "colorspace",
                "label",
                "binbits",
                "source_points",
                "effective_points",
                "effective_points_per_color",
            )
        }
        output["seeds"] = len(values)
        for metric in (
                "MS-SSIM_regret",
                "Delta E00_regret",
                "Delta Chroma_regret",
                "encoded_size_ratio"):
            samples = [value[metric] for value in values]
            output[f"{metric}_median"] = statistics.median(samples)
            output[f"{metric}_p90"] = percentile(samples, 0.9)
            output[f"{metric}_max"] = max(samples)
        for name, ms_limit, de_limit in CRITERIA:
            passed = sum(
                value["MS-SSIM_regret"] <= ms_limit
                and value["Delta E00_regret"] <= de_limit
                for value in values
            )
            output[f"{name}_pass_count"] = passed
            output[f"{name}_pass_rate"] = passed / len(values)
        result.append(output)
    return result


def threshold_rows(summary_rows: Sequence[Dict[str, object]]) \
        -> List[Dict[str, object]]:
    """Report observed and monotone-suffix sufficiency for each condition."""
    grouped: Dict[
        Tuple[str, int, str], List[Dict[str, object]]
    ] = {}
    for row in summary_rows:
        key = (
            str(row["fixture_id"]),
            int(row["colors"]),
            str(row["colorspace"]),
        )
        grouped.setdefault(key, []).append(row)
    result: List[Dict[str, object]] = []
    for key in sorted(grouped):
        candidates = sorted(grouped[key], key=lambda row: int(row["binbits"]))
        source = candidates[0]
        for criterion, ms_limit, de_limit in CRITERIA:
            passing = [
                row for row in candidates
                if float(row[f"{criterion}_pass_rate"])
                >= PASS_RATE_TARGET
            ]
            stable = [
                row for index, row in enumerate(candidates)
                if all(
                    float(candidate[f"{criterion}_pass_rate"])
                    >= PASS_RATE_TARGET
                    for candidate in candidates[index:]
                )
            ]
            observed = passing[0] if passing else None
            stable_row = stable[0] if stable else None
            result.append(
                {
                    **{
                        name: source[name]
                        for name in (
                            "revision",
                            "platform",
                            "fixture_id",
                            "fixture_label",
                            "fixture_class",
                            "input",
                            "colors",
                            "colorspace",
                            "label",
                            "source_points",
                        )
                    },
                    "criterion": criterion,
                    "MS-SSIM_regret_limit": ms_limit,
                    "Delta E00_regret_limit": de_limit,
                    "pass_rate_target": PASS_RATE_TARGET,
                    "observed_binbits": (
                        int(observed["binbits"]) if observed else ""
                    ),
                    "observed_effective_points": (
                        int(observed["effective_points"]) if observed else ""
                    ),
                    "observed_points_per_color": (
                        float(observed["effective_points_per_color"])
                        if observed else ""
                    ),
                    "stable_binbits": (
                        int(stable_row["binbits"]) if stable_row else ""
                    ),
                    "stable_effective_points": (
                        int(stable_row["effective_points"])
                        if stable_row else ""
                    ),
                    "stable_points_per_color": (
                        float(stable_row["effective_points_per_color"])
                        if stable_row else ""
                    ),
                    "stable_censored": "no" if stable_row else "yes",
                }
            )
    return result


def plot_quality_regret(path: Path,
                        summary_rows: Sequence[Dict[str, object]]) -> None:
    """Plot effective population against paired median quality regret."""
    figure, axes = plt.subplots(1, 2, figsize=(12.0, 5.7))
    panels = (
        ("MS-SSIM_regret_median", "Paired median MS-SSIM regret", 0.002),
        ("Delta E00_regret_median", "Paired median Delta E00 regret", 0.15),
    )
    marker_by_colors = {64: "s", 128: "o", 256: "^"}
    colors_values = sorted({int(row["colors"]) for row in summary_rows})
    for axis, (field, title, limit) in zip(axes, panels):
        for record in colorspace_records():
            colorspace = str(record["colorspace"])
            color, _marker, _line = STYLES[colorspace]
            for colors in colors_values:
                selected = [
                    row for row in summary_rows
                    if row["colorspace"] == colorspace
                    and int(row["colors"]) == colors
                ]
                axis.scatter(
                    [float(row["effective_points_per_color"])
                     for row in selected],
                    [float(row[field]) for row in selected],
                    s=28,
                    marker=marker_by_colors[colors],
                    facecolors=(
                        "none" if colors in (64, 128) else color
                    ),
                    edgecolors=color,
                    linewidths=0.9,
                    alpha=0.8,
                )
        axis.axhline(0.0, color="#333333", linewidth=0.9)
        axis.axhline(
            limit,
            color="#666666",
            linestyle="--",
            linewidth=0.9,
        )
        axis.axvline(
            32.0,
            color="#999999",
            linestyle=":",
            linewidth=0.9,
        )
        axis.set_xscale("log", base=2)
        axis.xaxis.set_major_formatter(
            ticker.FuncFormatter(lambda value, _position: f"{value:g}")
        )
        axis.set_xlabel("Effective weighted points per palette color (S / K)")
        axis.set_ylabel(title)
        axis.set_title(title)
        axis.grid(True, color="#DDDDDD", linewidth=0.6, alpha=0.8)
    handles = [
        Line2D(
            [0], [0], marker="o", linestyle="none", color=color,
            markerfacecolor=color, label=str(record["label"]),
        )
        for record in colorspace_records()
        for color, _marker, _line in (STYLES[str(record["colorspace"])],)
    ]
    handles.extend(
        Line2D(
            [0], [0], marker=marker_by_colors[colors], linestyle="none",
            color="#333333",
            markerfacecolor="none" if colors in (64, 128) else "#333333",
            label=f"K={colors}",
        )
        for colors in colors_values
    )
    handles.extend((
        Line2D(
            [0], [0], color="#666666", linestyle="--",
            label="balanced limit",
        ),
        Line2D(
            [0], [0], color="#999999", linestyle=":",
            label="exploratory 32 x K",
        ),
    ))
    figure.legend(
        handles=handles,
        loc="lower center",
        bbox_to_anchor=(0.5, 0.065),
        ncol=5,
        fontsize=8,
        frameon=False,
    )
    figure.suptitle("High-K hard-binning population and quality regret")
    figure.text(
        0.5,
        0.018,
        "Five content fixtures; five paired K-means seeds; no dithering; "
        "positive regret means hard binning is worse than none.",
        ha="center",
        fontsize=9,
    )
    figure.tight_layout(rect=(0.0, 0.21, 1.0, 0.94))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def plot_pass_rate(path: Path,
                   summary_rows: Sequence[Dict[str, object]]) -> None:
    """Plot the paired-seed balanced pass rate against effective population."""
    colors_values = sorted({int(row["colors"]) for row in summary_rows})
    figure, axes_grid = plt.subplots(
        1,
        len(colors_values),
        figsize=(5.4 * len(colors_values), 5.5),
        sharey=True,
        squeeze=False,
    )
    axes = axes_grid[0]
    for axis, colors in zip(axes, colors_values):
        for record in colorspace_records():
            colorspace = str(record["colorspace"])
            color, marker, _line = STYLES[colorspace]
            selected = [
                row for row in summary_rows
                if row["colorspace"] == colorspace
                and int(row["colors"]) == colors
            ]
            axis.scatter(
                [float(row["effective_points_per_color"])
                 for row in selected],
                [float(row["balanced_pass_rate"]) for row in selected],
                s=34,
                color=color,
                marker=marker,
                alpha=0.8,
                label=str(record["label"]),
            )
        axis.axhline(
            PASS_RATE_TARGET,
            color="#333333",
            linestyle="--",
            linewidth=0.9,
        )
        axis.axvline(32.0, color="#999999", linestyle=":", linewidth=0.9)
        axis.set_xscale("log", base=2)
        axis.xaxis.set_major_formatter(
            ticker.FuncFormatter(lambda value, _position: f"{value:g}")
        )
        axis.set_ylim(-0.05, 1.05)
        axis.set_xlabel("Effective weighted points per palette color (S / K)")
        axis.set_title(f"K={colors}")
        axis.grid(True, color="#DDDDDD", linewidth=0.6, alpha=0.8)
    axes[0].set_ylabel("Paired-seed balanced pass rate")
    handles, labels = axes[0].get_legend_handles_labels()
    figure.legend(
        handles,
        labels,
        loc="lower center",
        bbox_to_anchor=(0.5, 0.075),
        ncol=5,
        fontsize=8,
        frameon=False,
    )
    figure.suptitle("High-K hard-binning quality pass rate")
    figure.text(
        0.5,
        0.02,
        "Pass: MS-SSIM regret <= 0.002 and Delta E00 regret <= 0.15; "
        "dashed line: 4 of 5 paired seeds.",
        ha="center",
        fontsize=9,
    )
    figure.tight_layout(rect=(0.0, 0.19, 1.0, 0.94))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def snapshot(source_root: Path,
             build_dir: Path,
             manifest_path: Path,
             fixtures: Sequence[Dict[str, object]],
             img2sixel: str,
             lsqa: str) -> Dict[str, object]:
    """Capture all mutable protocol inputs before and after measurement."""
    return {
        "source": git_source_snapshot(source_root),
        "manifest_sha256": file_sha256(manifest_path),
        "fixture_sha256": {
            str(fixture["id"]): file_sha256(Path(fixture["resolved_path"]))
            for fixture in fixtures
        },
        "programs": {
            "img2sixel": program_record(img2sixel, source_root),
            "lsqa": program_record(lsqa, source_root),
        },
        "libsixel_linkage": libsixel_linkage(build_dir, source_root),
    }


def write_metadata(path: Path,
                   source_root: Path,
                   build_dir: Path,
                   manifest_path: Path,
                   fixtures: Sequence[Dict[str, object]],
                   colors_values: Sequence[int],
                   seeds: Sequence[int],
                   revision: str,
                   source_state: str,
                   start: Dict[str, object],
                   row_counts: Dict[str, int]) -> None:
    """Write provenance, controlled axes, criteria, and artifact counts."""
    build = read_build_configuration(build_dir)
    build_source_root = configured_source_root(build_dir)
    build["configured_source_directory"] = str(build_source_root)
    build["source_directory_matches"] = build_source_root == source_root
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "source": {
            "revision": revision,
            "tracked_worktree_state_at_start": source_state,
            "top_source_directory": str(source_root),
            "suite_end_snapshot_matches": True,
        },
        "build": build,
        "host": {
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "matplotlib": matplotlib.__version__,
        },
        "manifest": {
            "path": display_path(manifest_path, source_root),
            "sha256": start["manifest_sha256"],
        },
        "fixtures": [
            {
                key: fixture[key]
                for key in (
                    "id",
                    "label",
                    "class",
                    "path",
                    "width",
                    "height",
                    "sha256",
                )
            }
            for fixture in fixtures
        ],
        "programs": start["programs"],
        "libsixel_linkage": start["libsixel_linkage"],
        "protocol": {
            "comparison_scope": (
                "effective hard-bin population versus paired no-binning "
                "quality at high palette sizes"
            ),
            "colors": list(colors_values),
            "seeds": list(seeds),
            "binbits": list(BINBITS),
            "baseline": "none",
            "threads": 1,
            "precision": "float32",
            "required_work_format": "rgb-f32",
            "quality": "full",
            "loader": "builtin!",
            "sampling_policy": "full-frame",
            "working_colorspace": "gamma",
            "diffusion": "none",
            "lookup_policy": "none",
            "gpu_policy": "off",
            "merge_policy": "none",
            "criteria": [
                {
                    "name": name,
                    "MS-SSIM_regret_limit": ms_limit,
                    "Delta E00_regret_limit": de_limit,
                }
                for name, ms_limit, de_limit in CRITERIA
            ],
            "pass_rate_target": PASS_RATE_TARGET,
            "stable_threshold": (
                "the coarsest observed grid for which it and every finer "
                "observed grid meet the paired-seed pass-rate target"
            ),
            "order": (
                "six policy/depth configurations rotated by fixture, K, "
                "colorspace, and seed"
            ),
            "interpretation_limit": (
                "point count is correlated with hard-grid distortion and "
                "does not by itself establish a universal causal threshold"
            ),
            "sixel_environment_removed": True,
            "lsqa_environment_removed": True,
        },
        "rows": row_counts,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--img2sixel")
    parser.add_argument("--lsqa")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", default="unknown")
    parser.add_argument("--colors", default="64,128,256")
    parser.add_argument("--seeds", default="1,2,3,4,5")
    parser.add_argument("--clean-sixel-environment", action="store_true")
    return parser.parse_args()


def main() -> int:
    """Run, aggregate, plot, and record the complete threshold experiment."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    build_dir = (args.build_dir or source_root).resolve()
    manifest_path = args.manifest.resolve()
    output_dir = args.output_dir.resolve()
    colors_values = parse_integer_list(args.colors, "colors")
    seeds = parse_integer_list(args.seeds, "seeds")
    _manifest, all_fixtures = load_manifest(manifest_path, source_root)
    fixtures = content_fixtures(all_fixtures)
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    lsqa = resolve_lsqa(args.lsqa, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    for name in list(command_env):
        if name.startswith("LSQA_"):
            del command_env[name]
    require_work_format(
        make_command(
            img2sixel,
            Path(fixtures[0]["resolved_path"]),
            colors_values[0],
            "gamma",
            True,
        ),
        command_env,
        "rgb-f32",
    )
    start = snapshot(
        source_root,
        build_dir,
        manifest_path,
        fixtures,
        img2sixel,
        lsqa,
    )
    if start["source"]["revision"] != args.revision:
        raise ValueError("recorded revision differs from the source tree")
    if (
        start["source"]["tracked_worktree_state_at_start"]
        != args.source_state
    ):
        raise ValueError("recorded source state differs from the source tree")
    if configured_source_root(build_dir) != source_root:
        raise ValueError("build directory was configured from another tree")
    populations = measure_populations(fixtures, img2sixel, command_env)
    raw_rows = measure_quality(
        fixtures,
        img2sixel,
        lsqa,
        args.revision,
        colors_values,
        seeds,
        populations,
        command_env,
    )
    end = snapshot(
        source_root,
        build_dir,
        manifest_path,
        fixtures,
        img2sixel,
        lsqa,
    )
    if start != end:
        raise RuntimeError(
            "source, fixtures, executables, or linkage changed during run"
        )
    summary_rows = summarize(raw_rows)
    thresholds = threshold_rows(summary_rows)
    raw_fields = (
        "revision", "platform", "fixture_id", "fixture_label",
        "fixture_class", "input", "width", "height", "colors",
        "colorspace", "label", "seed", "binning_policy", "binbits",
        "source_points", "effective_points", "effective_points_per_color",
        "MS-SSIM", "Delta E00_mean", "Delta Chroma_mean",
        "encoded_bytes", "command", "trace_command",
    )
    summary_fields = (
        "revision", "platform", "fixture_id", "fixture_label",
        "fixture_class", "input", "width", "height", "colors",
        "colorspace", "label", "binbits", "source_points",
        "effective_points", "effective_points_per_color", "seeds",
        *(
            f"{metric}_{statistic}"
            for metric in (
                "MS-SSIM_regret", "Delta E00_regret",
                "Delta Chroma_regret", "encoded_size_ratio",
            )
            for statistic in ("median", "p90", "max")
        ),
        *(
            field
            for name, _ms_limit, _de_limit in CRITERIA
            for field in (f"{name}_pass_count", f"{name}_pass_rate")
        ),
    )
    threshold_fields = (
        "revision", "platform", "fixture_id", "fixture_label",
        "fixture_class", "input", "colors", "colorspace", "label",
        "source_points", "criterion", "MS-SSIM_regret_limit",
        "Delta E00_regret_limit", "pass_rate_target",
        "observed_binbits", "observed_effective_points",
        "observed_points_per_color", "stable_binbits",
        "stable_effective_points", "stable_points_per_color",
        "stable_censored",
    )
    write_csv(
        output_dir / "clustering-binning-threshold-raw.csv",
        raw_rows,
        raw_fields,
    )
    write_csv(
        output_dir / "clustering-binning-threshold-summary.csv",
        summary_rows,
        summary_fields,
    )
    write_csv(
        output_dir / "clustering-binning-thresholds.csv",
        thresholds,
        threshold_fields,
    )
    plot_quality_regret(
        output_dir / "clustering-binning-threshold-quality.png",
        summary_rows,
    )
    plot_pass_rate(
        output_dir / "clustering-binning-threshold-pass-rate.png",
        summary_rows,
    )
    write_metadata(
        output_dir / "clustering-binning-threshold-run.json",
        source_root,
        build_dir,
        manifest_path,
        fixtures,
        colors_values,
        seeds,
        args.revision,
        args.source_state,
        start,
        {
            "raw": len(raw_rows),
            "summary": len(summary_rows),
            "thresholds": len(thresholds),
        },
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
