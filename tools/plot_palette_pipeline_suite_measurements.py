#!/usr/bin/env python3
"""Measure and plot the multi-fixture palette-pipeline suite."""

from __future__ import annotations

import argparse
import datetime
import json
import math
import os
import platform
import re
import statistics
import subprocess
import tempfile
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import colors as mpl_colors
from matplotlib import ticker

from plot_lookup_policy_speed import (
    display_path,
    file_sha256,
    program_record,
    read_build_configuration,
    require_work_format,
    resolve_img2sixel,
)
from plot_palette_pipeline_measurements import (
    AXIS_CONFIGS,
    CMS_LOADER_ORDER,
    CMS_REFERENCE_LOADER_ENVIRONMENT,
    CMS_REFERENCE_LOADER_ORDER,
    DEFAULT_COLORS,
    PIPELINE_CONFIGS,
    QUANTIZE_OPTION,
    SAMPLE_TARGET,
    STYLES,
    config_records,
    configured_source_root,
    git_source_snapshot,
    libsixel_linkage,
    make_command,
    make_command_environment,
    measure_quality_and_size,
    measurement_records,
    require_build_program,
    run_speed_command,
    williams_orders,
)
from plot_quantize_model_measurements import resolve_lsqa, write_csv


CONTENT_CLASSES = {
    "natural",
    "flat-artwork",
    "gradient",
    "rare-color",
    "broad-gamut",
}
DURABLE_PROTOCOL_FILES = (
    "tools/check_palette_pipeline_measurements.py",
    "tools/check_palette_pipeline_suite_measurements.py",
    "tools/plot_palette_pipeline_measurements.py",
    "tools/plot_palette_pipeline_suite_measurements.py",
    "tools/plot_lookup_policy_speed.py",
    "tools/plot_quantize_model_measurements.py",
    "tools/reproduce_palette_pipeline_suite_measurements.sh",
)
SCALE_COLORS = (64, 256)
SUITE_PLOTS = (
    "palette-pipeline-suite-quality.png",
    "palette-pipeline-suite-size.png",
    "palette-pipeline-suite-speed-content.png",
    "palette-pipeline-suite-speed-scale.png",
)
DIVERGING = mpl_colors.LinearSegmentedColormap.from_list(
    "libsixel-improvement",
    ("#D55E00", "#F7F7F7", "#0072B2"),
)


def read_png_dimensions(path: Path) -> Tuple[int, int]:
    """Read dimensions from the fixed-position PNG IHDR fields."""
    header = path.read_bytes()[:24]
    if (
        len(header) != 24
        or header[:8] != b"\x89PNG\r\n\x1a\n"
        or header[12:16] != b"IHDR"
    ):
        raise ValueError(f"suite fixture is not a PNG: {path}")
    return (
        int.from_bytes(header[16:20], "big"),
        int.from_bytes(header[20:24], "big"),
    )


def load_manifest(path: Path,
                  source_root: Path) -> Tuple[Dict[str, object],
                                              List[Dict[str, object]]]:
    """Load and validate the checked-in fixture suite manifest."""
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        raise ValueError(f"unsupported suite manifest schema: {path}")
    generator = payload.get("generator")
    if not isinstance(generator, str) or not generator:
        raise ValueError(f"suite manifest has no fixture generator: {path}")
    raw_fixtures = payload.get("fixtures")
    if not isinstance(raw_fixtures, list) or not raw_fixtures:
        raise ValueError(f"suite manifest has no fixtures: {path}")
    fixtures: List[Dict[str, object]] = []
    identifiers = set()
    for raw in raw_fixtures:
        if not isinstance(raw, dict):
            raise ValueError(f"invalid suite fixture record: {raw!r}")
        identifier = raw.get("id")
        label = raw.get("label")
        fixture_class = raw.get("class")
        relative_path = raw.get("path")
        roles = raw.get("roles")
        if (
            not isinstance(identifier, str)
            or re.fullmatch(r"[a-z0-9][a-z0-9-]*", identifier) is None
            or identifier in identifiers
        ):
            raise ValueError(f"invalid or duplicate fixture id: {identifier}")
        if not isinstance(label, str) or not label:
            raise ValueError(f"fixture has no label: {identifier}")
        if not isinstance(fixture_class, str) or not fixture_class:
            raise ValueError(f"fixture has no class: {identifier}")
        if not isinstance(relative_path, str) or not relative_path:
            raise ValueError(f"fixture has no path: {identifier}")
        if (
            not isinstance(roles, list)
            or not roles
            or len(roles) != len(set(roles))
            or any(role not in ("content", "scale") for role in roles)
        ):
            raise ValueError(f"fixture has invalid roles: {identifier}")
        fixture_path = (source_root / relative_path).resolve()
        try:
            fixture_path.relative_to(source_root)
        except ValueError as exc:
            raise ValueError(
                f"fixture escapes the source tree: {identifier}"
            ) from exc
        if not fixture_path.is_file():
            raise FileNotFoundError(f"suite fixture does not exist: {fixture_path}")
        width, height = read_png_dimensions(fixture_path)
        if (width, height) != (raw.get("width"), raw.get("height")):
            raise ValueError(f"fixture dimensions differ: {identifier}")
        identifiers.add(identifier)
        fixtures.append(
            {
                "id": identifier,
                "label": label,
                "class": fixture_class,
                "path": relative_path,
                "resolved_path": fixture_path,
                "width": width,
                "height": height,
                "pixels": width * height,
                "roles": list(roles),
                "sha256": file_sha256(fixture_path),
            }
        )
    content = [fixture for fixture in fixtures if "content" in fixture["roles"]]
    scale = [fixture for fixture in fixtures if "scale" in fixture["roles"]]
    if {fixture["class"] for fixture in content} != CONTENT_CLASSES:
        raise ValueError("content suite does not cover the required classes")
    if len(scale) < 3 or len({fixture["pixels"] for fixture in scale}) != len(scale):
        raise ValueError("scale suite requires at least three distinct pixel counts")
    return payload, fixtures


def require_tracked_inputs(source_root: Path,
                           manifest_path: Path,
                           manifest: Dict[str, object],
                           fixtures: Sequence[Dict[str, object]]) -> None:
    """Require every durable protocol input to belong to the revision."""
    paths = [source_root / name for name in DURABLE_PROTOCOL_FILES]
    paths.extend(
        (
            manifest_path,
            source_root / str(manifest["generator"]),
        )
    )
    paths.extend(Path(fixture["resolved_path"]) for fixture in fixtures)
    relative_paths = []
    for path in paths:
        resolved = path.resolve()
        try:
            relative = resolved.relative_to(source_root)
        except ValueError as exc:
            raise ValueError(
                f"durable suite input escapes the source revision: {resolved}"
            ) from exc
        if not resolved.is_file():
            raise FileNotFoundError(f"durable suite input is missing: {resolved}")
        relative_paths.append(str(relative))
    git = os.environ.get("GIT", "git")
    process = subprocess.run(
        (
            git,
            "-C",
            str(source_root),
            "ls-files",
            "--error-unmatch",
            "--",
            *relative_paths,
        ),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=False,
        text=True,
    )
    if process.returncode != 0:
        detail = process.stderr.strip()
        raise ValueError(
            "durable suite inputs must all be tracked by the recorded "
            f"revision: {detail}"
        )


def add_fixture_fields(rows: Iterable[Dict[str, object]],
                       fixture: Dict[str, object]) -> List[Dict[str, object]]:
    """Attach suite dimensions to raw single-fixture rows."""
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "fixture_id": fixture["id"],
                "fixture_label": fixture["label"],
                "fixture_class": fixture["class"],
                "fixture_roles": ";".join(fixture["roles"]),
                "width": fixture["width"],
                "height": fixture["height"],
                "pixels": fixture["pixels"],
                **row,
            }
        )
    return result


def measure_suite_quality(
        fixtures: Sequence[Dict[str, object]],
        img2sixel: str,
        lsqa: str,
        revision: str,
        command_env: Dict[str, str],
) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Measure quality and size once for each controlled fixture."""
    quality_rows: List[Dict[str, object]] = []
    size_rows: List[Dict[str, object]] = []
    for index, fixture in enumerate(fixtures, start=1):
        print(
            f"[quality {index}/{len(fixtures)}] {fixture['id']}",
            flush=True,
        )
        quality, size = measure_quality_and_size(
            img2sixel,
            lsqa,
            Path(fixture["resolved_path"]),
            str(fixture["path"]),
            revision,
            command_env,
            CMS_LOADER_ORDER,
        )
        quality_rows.extend(add_fixture_fields(quality, fixture))
        size_rows.extend(add_fixture_fields(size, fixture))
    return quality_rows, size_rows


def suite_speed_row(fixture: Dict[str, object],
                    revision: str,
                    colors: int,
                    record: Dict[str, str],
                    domain: str,
                    phase: str,
                    round_index: int,
                    sequence: int,
                    position: int,
                    fixture_sequence: int,
                    fixture_position: int,
                    duration: float,
                    command: str) -> Dict[str, object]:
    """Build one raw timing row with both scheduling dimensions."""
    return {
        "fixture_id": fixture["id"],
        "fixture_label": fixture["label"],
        "fixture_class": fixture["class"],
        "fixture_roles": ";".join(fixture["roles"]),
        "width": fixture["width"],
        "height": fixture["height"],
        "pixels": fixture["pixels"],
        "revision": revision,
        "platform": platform.platform(),
        "input": fixture["path"],
        "colors": colors,
        **record,
        "quantize_option": QUANTIZE_OPTION,
        "domain": domain,
        "phase": phase,
        "round": round_index,
        "sequence": sequence,
        "position": position,
        "fixture_sequence": fixture_sequence,
        "fixture_position": fixture_position,
        "seconds": duration,
        "command": command,
    }


def measure_suite_speed(
        fixtures: Sequence[Dict[str, object]],
        img2sixel: str,
        revision: str,
        warmups: int,
        runs: int,
        command_env: Dict[str, str],
) -> List[Dict[str, object]]:
    """Measure timing with independently alternated fixture and policy order."""
    rows: List[Dict[str, object]] = []
    records = measurement_records()
    by_config = {record["config"]: record for record in records}
    config_orders = williams_orders(list(by_config))
    by_fixture = {
        str(fixture["id"]): fixture for fixture in fixtures
    }
    fixture_orders = williams_orders(list(by_fixture))
    washout = by_config["full-frame-hard"]
    with tempfile.TemporaryDirectory(
            prefix="libsixel-pipeline-suite-timeline-") as directory:
        timeline_directory = Path(directory)
        for colors in DEFAULT_COLORS:
            for domain in ("end-to-end", "palette-build"):
                for round_index in range(warmups + runs):
                    phase = (
                        "warmup" if round_index < warmups else "measured"
                    )
                    # Negative indices reserve the sequences immediately
                    # before measured sequence zero for warm-up rounds.
                    phase_round = round_index - warmups
                    sequence = phase_round % len(config_orders)
                    fixture_sequence = phase_round % len(fixture_orders)
                    fixture_order = fixture_orders[fixture_sequence]
                    print(
                        f"[speed K={colors} {domain} round "
                        f"{round_index + 1}/{warmups + runs}]",
                        flush=True,
                    )
                    for fixture_position, fixture_id in enumerate(
                            fixture_order):
                        fixture = by_fixture[fixture_id]
                        input_image = Path(fixture["resolved_path"])
                        timeline_path = timeline_directory / (
                            f"{domain}-{colors}-{round_index}-{fixture_id}-"
                            "washout.jsonl"
                        )
                        duration, command = run_speed_command(
                            domain,
                            img2sixel,
                            input_image,
                            colors,
                            washout,
                            command_env,
                            timeline_path,
                            CMS_LOADER_ORDER,
                        )
                        rows.append(
                            suite_speed_row(
                                fixture,
                                revision,
                                colors,
                                washout,
                                domain,
                                "washout",
                                round_index,
                                sequence,
                                -1,
                                fixture_sequence,
                                fixture_position,
                                duration,
                                command,
                            )
                        )
                        for position, config in enumerate(
                                config_orders[sequence]):
                            record = by_config[config]
                            timeline_path = timeline_directory / (
                                f"{domain}-{colors}-{round_index}-"
                                f"{fixture_id}-{config}.jsonl"
                            )
                            duration, command = run_speed_command(
                                domain,
                                img2sixel,
                                input_image,
                                colors,
                                record,
                                command_env,
                                timeline_path,
                                CMS_LOADER_ORDER,
                            )
                            rows.append(
                                suite_speed_row(
                                    fixture,
                                    revision,
                                    colors,
                                    record,
                                    domain,
                                    phase,
                                    round_index,
                                    sequence,
                                    position,
                                    fixture_sequence,
                                    fixture_position,
                                    duration,
                                    command,
                                )
                            )
    return rows


def content_fixtures(fixtures: Sequence[Dict[str, object]]) \
        -> List[Dict[str, object]]:
    """Return fixtures used to compare content sensitivity."""
    return [fixture for fixture in fixtures if "content" in fixture["roles"]]


def scale_fixtures(fixtures: Sequence[Dict[str, object]]) \
        -> List[Dict[str, object]]:
    """Return scale fixtures ordered by pixel count."""
    selected = [fixture for fixture in fixtures if "scale" in fixture["roles"]]
    selected.sort(key=lambda fixture: int(fixture["pixels"]))
    return selected


def row_index(rows: Sequence[Dict[str, object]]) \
        -> Dict[Tuple[str, str, int], Dict[str, object]]:
    """Index one point row by fixture, configuration, and palette size."""
    return {
        (str(row["fixture_id"]), str(row["config"]), int(row["colors"])): row
        for row in rows
    }


def heatmap(
        axis: plt.Axes,
        matrix: Sequence[Sequence[float]],
        row_labels: Sequence[str],
        title: str,
        annotation: Callable[[float], str],
        colorbar_label: str,
        annotation_matrix: Sequence[Sequence[float]] | None = None,
) -> None:
    """Draw an annotated, zero-centered improvement heatmap."""
    flattened = [abs(value) for row in matrix for value in row]
    bound = max(max(flattened, default=0.0), 1e-12)
    norm = mpl_colors.TwoSlopeNorm(vmin=-bound, vcenter=0.0, vmax=bound)
    image = axis.imshow(matrix, cmap=DIVERGING, norm=norm, aspect="auto")
    axis.set_title(title, fontsize=10)
    axis.set_xticks(range(len(DEFAULT_COLORS)), DEFAULT_COLORS)
    axis.set_yticks(range(len(row_labels)), row_labels, fontsize=7.5)
    axis.set_xlabel("Palette size K")
    axis.set_xticks(
        [position - 0.5 for position in range(1, len(DEFAULT_COLORS))],
        minor=True,
    )
    axis.set_yticks(
        [position - 0.5 for position in range(1, len(row_labels))],
        minor=True,
    )
    axis.grid(which="minor", color="white", linewidth=0.7)
    axis.tick_params(which="minor", bottom=False, left=False)
    for y, row in enumerate(matrix):
        for x, value in enumerate(row):
            ink = "white" if abs(value) > bound * 0.55 else "#222222"
            annotation_value = value
            if annotation_matrix is not None:
                annotation_value = annotation_matrix[y][x]
            axis.text(
                x,
                y,
                annotation(annotation_value),
                ha="center",
                va="center",
                fontsize=6.5,
                color=ink,
            )
    colorbar = axis.figure.colorbar(image, ax=axis, fraction=0.035, pad=0.02)
    colorbar.set_label(colorbar_label, fontsize=8)
    colorbar.ax.tick_params(labelsize=7)


def comparison_rows(fixtures: Sequence[Dict[str, object]],
                    candidates: Sequence[Tuple[str, str]]) \
        -> Tuple[List[Tuple[str, str]], List[str]]:
    """Return fixture/config pairs and compact display labels."""
    pairs = []
    labels = []
    for fixture in fixtures:
        for config, policy_label in candidates:
            pairs.append((str(fixture["id"]), config))
            labels.append(f"{fixture['label']} / {policy_label}")
    return pairs, labels


def plot_quality(path: Path,
                 rows: Sequence[Dict[str, object]],
                 fixtures: Sequence[Dict[str, object]]) -> None:
    """Plot within-fixture quality improvements over controlled baselines."""
    selected = content_fixtures(fixtures)
    index = row_index(rows)
    sampling_pairs, sampling_labels = comparison_rows(
        selected, (("adaptive-grid-hard", "adaptive grid"),)
    )
    binning_pairs, binning_labels = comparison_rows(
        selected,
        (
            ("full-frame-none", "none"),
            ("full-frame-exact", "exact"),
            ("full-frame-soft", "soft"),
        ),
    )

    def quality_matrix(
            pairs: Sequence[Tuple[str, str]],
            metric: str,
            baseline: str,
            reverse: bool,
    ) -> List[List[float]]:
        matrix = []
        for fixture_id, config in pairs:
            values = []
            for colors in DEFAULT_COLORS:
                candidate = float(index[(fixture_id, config, colors)][metric])
                control = float(index[(fixture_id, baseline, colors)][metric])
                values.append(control - candidate if reverse else candidate - control)
            matrix.append(values)
        return matrix

    figure, axes = plt.subplots(2, 2, figsize=(14.0, 11.0))
    panels = (
        (
            axes[0][0],
            quality_matrix(
                sampling_pairs, "MS-SSIM", "full-frame-hard", False
            ),
            sampling_labels,
            "Sampling: MS-SSIM improvement",
            lambda value: f"{value:+.3f}",
            "candidate - full frame",
        ),
        (
            axes[0][1],
            quality_matrix(
                sampling_pairs, "Delta E00_mean", "full-frame-hard", True
            ),
            sampling_labels,
            "Sampling: mean Delta E00 improvement",
            lambda value: f"{value:+.2f}",
            "full frame - candidate",
        ),
        (
            axes[1][0],
            quality_matrix(
                binning_pairs, "MS-SSIM", "full-frame-hard", False
            ),
            binning_labels,
            "Binning: MS-SSIM improvement",
            lambda value: f"{value:+.3f}",
            "candidate - hard",
        ),
        (
            axes[1][1],
            quality_matrix(
                binning_pairs, "Delta E00_mean", "full-frame-hard", True
            ),
            binning_labels,
            "Binning: mean Delta E00 improvement",
            lambda value: f"{value:+.2f}",
            "hard - candidate",
        ),
    )
    for panel in panels:
        heatmap(*panel)
    figure.suptitle("Palette pipeline quality across content fixtures", y=0.995)
    figure.text(
        0.99,
        0.008,
        "Positive values are better; fixed K-means; no dithering; exact lookup",
        ha="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.97))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_size(path: Path,
              rows: Sequence[Dict[str, object]],
              fixtures: Sequence[Dict[str, object]]) -> None:
    """Plot within-fixture encoded-size reduction over controlled baselines."""
    selected = content_fixtures(fixtures)
    index = row_index(rows)
    sampling_pairs, sampling_labels = comparison_rows(
        selected, (("adaptive-grid-hard", "adaptive grid"),)
    )
    binning_pairs, binning_labels = comparison_rows(
        selected,
        (
            ("full-frame-none", "none"),
            ("full-frame-exact", "exact"),
            ("full-frame-soft", "soft"),
        ),
    )

    def size_matrix(pairs: Sequence[Tuple[str, str]],
                    baseline: str) -> List[List[float]]:
        matrix = []
        for fixture_id, config in pairs:
            values = []
            for colors in DEFAULT_COLORS:
                candidate = int(index[(fixture_id, config, colors)][
                    "encoded_bytes"
                ])
                control = int(index[(fixture_id, baseline, colors)][
                    "encoded_bytes"
                ])
                values.append(100.0 * (1.0 - candidate / control))
            matrix.append(values)
        return matrix

    figure, axes = plt.subplots(1, 2, figsize=(14.0, 7.5))
    heatmap(
        axes[0],
        size_matrix(sampling_pairs, "full-frame-hard"),
        sampling_labels,
        "Sampling: stream-size reduction",
        lambda value: f"{value:+.1f}%",
        "reduction vs full frame (%)",
    )
    heatmap(
        axes[1],
        size_matrix(binning_pairs, "full-frame-hard"),
        binning_labels,
        "Binning: stream-size reduction",
        lambda value: f"{value:+.1f}%",
        "reduction vs hard (%)",
    )
    figure.suptitle("Palette pipeline SIXEL size across content fixtures", y=0.995)
    figure.text(
        0.99,
        0.008,
        "Positive values are smaller; same no-dither streams used for quality",
        ha="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.96))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def measured_median(rows: Sequence[Dict[str, object]],
                    fixture_id: str,
                    config: str,
                    colors: int,
                    domain: str) -> float:
    """Return the median of raw measured timing observations."""
    values = [
        float(row["seconds"])
        for row in rows
        if row["fixture_id"] == fixture_id
        and row["config"] == config
        and int(row["colors"]) == colors
        and row["domain"] == domain
        and row["phase"] == "measured"
    ]
    if not values:
        raise ValueError(
            f"missing timing samples: {fixture_id} {config} {colors} {domain}"
        )
    return statistics.median(values)


def plot_speed_content(path: Path,
                       rows: Sequence[Dict[str, object]],
                       fixtures: Sequence[Dict[str, object]]) -> None:
    """Plot policy speedups for each content class and timing domain."""
    selected = content_fixtures(fixtures)
    sampling_pairs, sampling_labels = comparison_rows(
        selected, (("adaptive-grid-hard", "adaptive grid"),)
    )
    binning_pairs, binning_labels = comparison_rows(
        selected,
        (
            ("full-frame-none", "none"),
            ("full-frame-exact", "exact"),
            ("full-frame-soft", "soft"),
        ),
    )

    def speed_matrix(pairs: Sequence[Tuple[str, str]],
                     baseline: str,
                     domain: str) -> Tuple[List[List[float]],
                                           List[List[float]]]:
        log_values = []
        ratios = []
        for fixture_id, config in pairs:
            log_row = []
            ratio_row = []
            for colors in DEFAULT_COLORS:
                candidate = measured_median(
                    rows, fixture_id, config, colors, domain
                )
                control = measured_median(
                    rows, fixture_id, baseline, colors, domain
                )
                ratio = control / candidate
                log_row.append(math.log2(ratio))
                ratio_row.append(ratio)
            log_values.append(log_row)
            ratios.append(ratio_row)
        return log_values, ratios

    figure, axes = plt.subplots(2, 2, figsize=(14.0, 11.0))
    for row_index_value, domain in enumerate(("palette-build", "end-to-end")):
        for column, (pairs, labels, baseline, title) in enumerate(
            (
                (
                    sampling_pairs,
                    sampling_labels,
                    "full-frame-hard",
                    "Sampling",
                ),
                (binning_pairs, binning_labels, "full-frame-hard", "Binning"),
            )
        ):
            logarithms, ratios = speed_matrix(pairs, baseline, domain)
            heatmap(
                axes[row_index_value][column],
                logarithms,
                labels,
                f"{title}: {domain} speedup",
                lambda value: f"{value:.2f}x",
                "log2(control / candidate)",
                ratios,
            )
    figure.suptitle("Palette pipeline speed across content fixtures", y=0.995)
    figure.text(
        0.99,
        0.008,
        "Values above 1x are faster; medians use raw measured fresh processes",
        ha="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.025, 1.0, 0.97))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def plot_speed_scale(path: Path,
                     rows: Sequence[Dict[str, object]],
                     fixtures: Sequence[Dict[str, object]],
                     runs: int) -> None:
    """Plot palette-build scaling on one continuous synthetic image family."""
    selected = scale_fixtures(fixtures)
    figure, axes = plt.subplots(
        len(SCALE_COLORS),
        2,
        figsize=(11.0, 7.5),
        sharex=True,
        sharey="row",
    )
    for row_index_value, colors in enumerate(SCALE_COLORS):
        for column, comparison_axis in enumerate(("sampling", "binning")):
            axis = axes[row_index_value][column]
            for config, label in AXIS_CONFIGS[comparison_axis]:
                values = [
                    1000.0 * measured_median(
                        rows,
                        str(fixture["id"]),
                        config,
                        colors,
                        "palette-build",
                    )
                    for fixture in selected
                ]
                color, marker, linestyle = STYLES[(comparison_axis, config)]
                axis.plot(
                    [int(fixture["pixels"]) for fixture in selected],
                    values,
                    color=color,
                    marker=marker,
                    linestyle=linestyle,
                    linewidth=1.7,
                    markersize=4.5,
                    label=label,
                )
            axis.set_xscale("log")
            axis.set_yscale("log")
            axis.grid(True, color="#D9D9D9", linewidth=0.7)
            axis.set_title(
                f"{'Sampling' if column == 0 else 'Binning'}, K={colors}",
                fontsize=10,
            )
            axis.legend(fontsize=7.5, frameon=False, loc="best")
            if row_index_value == len(SCALE_COLORS) - 1:
                axis.set_xlabel("Input pixels (log)")
        axes[row_index_value][0].set_ylabel("Palette-build span (ms, log)")
    ticks = [int(fixture["pixels"]) for fixture in selected]
    labels = [
        f"{int(fixture['pixels']) / 1000.0:.0f}k\n"
        f"{fixture['width']}x{fixture['height']}"
        for fixture in selected
    ]
    for axis in axes[-1]:
        axis.xaxis.set_major_locator(ticker.FixedLocator(ticks))
        axis.xaxis.set_major_formatter(ticker.FixedFormatter(labels))
        axis.xaxis.set_minor_formatter(ticker.NullFormatter())
    figure.suptitle("Palette-build scaling on the smooth-gradient family", y=0.995)
    figure.text(
        0.99,
        0.008,
        f"Median of {runs}; same continuous RGB function at each resolution",
        ha="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.03, 1.0, 0.97))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def suite_snapshot(source_root: Path,
                   build_dir: Path,
                   manifest_path: Path,
                   fixtures: Sequence[Dict[str, object]],
                   img2sixel: str,
                   lsqa: str) -> Dict[str, object]:
    """Capture all mutable inputs around the complete suite run."""
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


def single_fixture_protocol(mode: str,
                            warmups: int,
                            runs: int) -> Dict[str, object]:
    """Describe the controlled axes shared by all suite fixtures."""
    return {
        "measurement_mode": mode,
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
        "loader": CMS_LOADER_ORDER,
        "quality_reference_loader": CMS_REFERENCE_LOADER_ORDER,
        "quality_reference_loader_environment": (
            CMS_REFERENCE_LOADER_ENVIRONMENT
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
                "before every fixture block"
            ),
        },
        "sixel_environment_removed": True,
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
            "one raw duration per domain, fixture, K, round, position, "
            "and recorded boundary washout"
        ),
        "interpretation_limit": (
            "the fixture suite characterizes mechanisms but does not alone "
            "select a project default"
        ),
    }


def write_metadata(path: Path,
                   source_root: Path,
                   build_dir: Path,
                   manifest_path: Path,
                   fixtures: Sequence[Dict[str, object]],
                   mode: str,
                   warmups: int,
                   runs: int,
                   row_counts: Dict[str, int],
                   snapshot: Dict[str, object]) -> None:
    """Write suite provenance, experimental design, and artifact counts."""
    build = read_build_configuration(build_dir)
    build_source_root = configured_source_root(build_dir)
    build["configured_source_directory"] = str(build_source_root)
    build["source_directory_matches"] = build_source_root == source_root
    records = []
    for fixture in fixtures:
        records.append(
            {
                key: fixture[key]
                for key in (
                    "id",
                    "label",
                    "class",
                    "path",
                    "width",
                    "height",
                    "pixels",
                    "roles",
                    "sha256",
                )
            }
        )
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "source": {
            **snapshot["source"],
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
            "sha256": snapshot["manifest_sha256"],
        },
        "fixtures": records,
        "programs": snapshot["programs"],
        "libsixel_linkage": snapshot["libsixel_linkage"],
        "protocol": {
            "measurement_mode": mode,
            "comparison_scope": (
                "palette sampling and binning across content and pixel count"
            ),
            "content_fixture_ids": [
                fixture["id"]
                for fixture in content_fixtures(fixtures)
            ],
            "scale_fixture_ids": [
                fixture["id"]
                for fixture in scale_fixtures(fixtures)
            ],
            "content_classes": sorted(CONTENT_CLASSES),
            "quality_improvement": (
                "candidate minus control for MS-SSIM; control minus "
                "candidate for mean Delta E00"
            ),
            "size_improvement": "100 * (1 - candidate bytes / control bytes)",
            "speed_improvement": "control median / candidate median",
            "scale_plot_colors": list(SCALE_COLORS),
            "speed_warmups": warmups,
            "speed_runs": runs,
            "single_fixture_protocol": single_fixture_protocol(
                mode, warmups, runs
            ),
            "fixture_order_design": {
                "name": "Williams-derived fixture order",
                "physical_fixtures": len(fixtures),
                "sequence_period": len(williams_orders([
                    str(fixture["id"]) for fixture in fixtures
                ])),
                "measured_sequence_start": 0,
                "position_balance_period": len(fixtures),
                "full_carryover_balance_period": len(williams_orders([
                    str(fixture["id"]) for fixture in fixtures
                ])),
            },
            "fixture_and_policy_orders_recorded": True,
            "shared_single_fixture_measurement_code": True,
            "provenance_rechecked_after_suite": True,
        },
        "artifacts": {
            "row_counts": row_counts,
            "plots": list(SUITE_PLOTS),
        },
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
    parser.add_argument("--revision", required=True)
    parser.add_argument("--source-state", choices=("clean", "dirty"), required=True)
    parser.add_argument(
        "--measurement-mode",
        choices=("durable", "exploratory"),
        required=True,
    )
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=10)
    return parser.parse_args()


def main() -> int:
    """Run, validate, aggregate, and plot the complete fixture suite."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    build_dir = (args.build_dir or source_root).resolve()
    manifest_path = args.manifest.resolve()
    output_dir = args.output_dir.resolve()
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("warmups must be non-negative and runs positive")
    if args.measurement_mode == "durable":
        if (args.warmups, args.runs) != (2, 10):
            raise ValueError("durable suite requires 2 warm-ups and 10 runs")
        if args.source_state != "clean":
            raise ValueError("durable suite requires a clean tracked worktree")
    if configured_source_root(build_dir) != source_root:
        raise ValueError("suite build directory was configured from another tree")
    manifest, fixtures = load_manifest(manifest_path, source_root)
    if args.measurement_mode == "durable":
        require_tracked_inputs(
            source_root, manifest_path, manifest, fixtures
        )
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    lsqa = resolve_lsqa(args.lsqa, source_root)
    require_build_program("img2sixel", img2sixel, build_dir)
    require_build_program("lsqa", lsqa, build_dir)
    start = suite_snapshot(
        source_root, build_dir, manifest_path, fixtures, img2sixel, lsqa
    )
    if start["source"]["revision"] != args.revision:
        raise ValueError("suite revision differs from the source tree")
    if (
        start["source"]["tracked_worktree_state_at_start"]
        != args.source_state
    ):
        raise ValueError("suite source state differs from the source tree")
    command_env = make_command_environment(True)
    require_work_format(
        make_command(
            img2sixel,
            Path(fixtures[0]["resolved_path"]),
            DEFAULT_COLORS[0],
            measurement_records()[0],
            True,
            CMS_LOADER_ORDER,
        ),
        command_env,
        "rgb888",
    )
    quality_rows, size_rows = measure_suite_quality(
        fixtures,
        img2sixel,
        lsqa,
        args.revision,
        command_env,
    )
    speed_rows = measure_suite_speed(
        fixtures,
        img2sixel,
        args.revision,
        args.warmups,
        args.runs,
        command_env,
    )
    end = suite_snapshot(
        source_root, build_dir, manifest_path, fixtures, img2sixel, lsqa
    )
    if end != start:
        raise RuntimeError("suite source, inputs, or executables changed during run")
    output_dir.mkdir(parents=True, exist_ok=True)
    common_fields = (
        "fixture_id",
        "fixture_label",
        "fixture_class",
        "fixture_roles",
        "width",
        "height",
        "pixels",
        "revision",
        "platform",
        "input",
        "colors",
        "config",
        "sampling_policy",
        "binning_policy",
        "quantize_option",
    )
    quality_path = output_dir / "palette-pipeline-suite-quality.csv"
    size_path = output_dir / "palette-pipeline-suite-size.csv"
    speed_path = output_dir / "palette-pipeline-suite-speed.csv"
    write_csv(
        quality_path,
        quality_rows,
        common_fields + (
            "MS-SSIM",
            "Delta E00_mean",
            "command",
            "assessment_command",
        ),
    )
    write_csv(
        size_path,
        size_rows,
        common_fields + ("encoded_bytes", "command"),
    )
    write_csv(
        speed_path,
        speed_rows,
        common_fields + (
            "domain",
            "phase",
            "round",
            "sequence",
            "position",
            "fixture_sequence",
            "fixture_position",
            "seconds",
            "command",
        ),
    )
    plot_quality(output_dir / SUITE_PLOTS[0], quality_rows, fixtures)
    plot_size(output_dir / SUITE_PLOTS[1], size_rows, fixtures)
    plot_speed_content(
        output_dir / SUITE_PLOTS[2], speed_rows, fixtures
    )
    plot_speed_scale(
        output_dir / SUITE_PLOTS[3], speed_rows, fixtures, args.runs
    )
    write_metadata(
        output_dir / "palette-pipeline-suite-run.json",
        source_root,
        build_dir,
        manifest_path,
        fixtures,
        args.measurement_mode,
        args.warmups,
        args.runs,
        {
            "quality": len(quality_rows),
            "size": len(size_rows),
            "speed": len(speed_rows),
        },
        start,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
