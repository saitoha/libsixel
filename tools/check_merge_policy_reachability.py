#!/usr/bin/env python3
"""Validate final-merge reachability artifacts and their provenance."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
import struct
from pathlib import Path
from typing import Dict, List, Set, Tuple

from plot_merge_policy_reachability import (
    AMBIGUOUS_HIGH,
    AMBIGUOUS_LOW,
    COLORS,
    DE_THRESHOLDS,
    LAB_CELL_WIDTH,
    MERGE_NAMES,
    PATCH_SIZE,
    PATCH_SIZES,
    STUCK_RATIO,
)


SUMMARY_CSV = "merge-policy-reachability-summary.csv"
PATCH_CSV = "merge-policy-reachability-patch-size.csv"
METADATA = "merge-policy-reachability-run.json"
PLOTS = (
    "merge-policy-mechanics.png",
    "merge-policy-visual-comparison.png",
    "merge-policy-reachability.png",
    "merge-policy-reachability-bimodality.png",
    "merge-policy-reachability-hotspots.png",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def finite_float(row: Dict[str, str], field: str) -> float:
    """Read one finite float from a CSV row."""
    try:
        value = float(row[field])
    except (KeyError, ValueError) as exc:
        raise ValueError(f"invalid numeric field: {field}") from exc
    if not math.isfinite(value):
        fail(f"non-finite numeric field: {field}")
    return value


def percentage(row: Dict[str, str], field: str) -> float:
    """Read one percentage and enforce its closed range."""
    value = finite_float(row, field)
    if not 0.0 <= value <= 100.0:
        fail(f"percentage outside 0..100: {field}")
    return value


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read one required non-empty CSV."""
    if not path.is_file():
        fail(f"missing CSV: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"empty CSV: {path}")
    return rows


def validate_command(command_text: str, name: str) -> None:
    """Require the controlled palette-build options in one command template."""
    try:
        tokens = shlex.split(command_text)
    except ValueError as exc:
        raise ValueError(f"invalid command template for {name}") from exc
    merge_option = {
        "none": (
            "--merge-policy=none:merge_oversplit=1.81:merge_lloyd=0:"
            "channel_l=0.3333333333333333"
        ),
        "ward-l0": (
            "--merge-policy=ward:merge_oversplit=1.81:merge_lloyd=0:"
            "channel_l=0.3333333333333333"
        ),
        "ward-l3": (
            "--merge-policy=ward:merge_oversplit=1.81:merge_lloyd=3:"
            "channel_l=0.3333333333333333"
        ),
    }[name]
    required = {
        "{img2sixel}",
        "{input}",
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=none",
        "--quantize-model=heckbert:profile=compat",
        merge_option,
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
    }
    missing = sorted(required - set(tokens))
    if missing:
        fail(f"controlled command options missing for {name}: {missing}")
    try:
        palette_size = int(tokens[tokens.index("-p") + 1])
    except (ValueError, IndexError) as exc:
        raise ValueError(f"invalid palette size for {name}") from exc
    if palette_size != COLORS:
        fail(f"unexpected palette size for {name}")
    if "-M" not in tokens:
        fail(f"palette capture is missing for {name}")


def validate_summary(rows: List[Dict[str, str]]) -> None:
    """Validate the exact policy grid and all reachability layers."""
    if len(rows) != len(MERGE_NAMES):
        fail("summary CSV does not contain exactly one row per policy")
    actual: Set[str] = set()
    percent_fields = (
        "geometric_unreachable_volume_percent",
        "geometric_unreachable_area_percent",
        "ratio_classified_unreachable_volume_percent",
        "ratio_classified_unreachable_area_percent",
        "hull_inside_ratio_classified_volume_percent",
        "hull_inside_ratio_classified_area_percent",
        "ambiguous_ratio_volume_percent",
        "ambiguous_ratio_area_percent",
        "single_entry_volume_percent",
        "single_entry_area_percent",
    )
    for row in rows:
        name = row.get("merge_config", "")
        if name not in MERGE_NAMES or name in actual:
            fail("summary CSV has an unknown or duplicate policy")
        actual.add(name)
        if int(row.get("colors", 0)) != COLORS:
            fail("summary CSV has an unexpected palette request")
        if int(row.get("palette_entries", 0)) != COLORS:
            fail("captured palette does not contain the requested K entries")
        if int(row.get("probe_cells", 0)) < 1000:
            fail("too few occupied Lab cells were measured")
        if int(row.get("source_pixels", 0)) <= 0:
            fail("source pixel count is invalid")
        if finite_float(row, "lab_cell_width") != LAB_CELL_WIDTH:
            fail("Lab cell width differs from the protocol")
        if int(row.get("patch_size", 0)) != PATCH_SIZE:
            fail("main patch size differs from the protocol")
        if row.get("diffusion") != "fs" or row.get("scan") != "raster":
            fail("empirical diffusion controls changed")
        if row.get("lookup_policy") != "none":
            fail("empirical lookup is not exact")
        if finite_float(row, "stuck_ratio_threshold") != STUCK_RATIO:
            fail("stuck threshold differs from the protocol")
        for field in percent_fields:
            percentage(row, field)
        if (percentage(row, "ratio_classified_unreachable_volume_percent")
                + 1.0e-9
                < percentage(row, "geometric_unreachable_volume_percent")):
            fail("ratio-classified volume omits geometric exclusions")
        if (percentage(row, "ratio_classified_unreachable_area_percent")
                + 1.0e-9
                < percentage(row, "geometric_unreachable_area_percent")):
            fail("ratio-classified area omits geometric exclusions")
        if (percentage(row, "hull_inside_ratio_classified_volume_percent")
                > percentage(
                    row, "ratio_classified_unreachable_volume_percent"
                )
                + 1.0e-9):
            fail("hull-inside ratio volume exceeds all classified volume")
        if (percentage(row, "hull_inside_ratio_classified_area_percent")
                > percentage(
                    row, "ratio_classified_unreachable_area_percent"
                )
                + 1.0e-9):
            fail("hull-inside ratio area exceeds all classified area")
        for field in (
                "ratio_classified_delta_e00_area_mean",
                "ratio_classified_delta_e00_area_p95",
                "all_delta_e00_area_mean", "all_delta_e00_area_p95",
                "palette_nn_de00_mean", "palette_nn_de00_median",
                "palette_nn_de00_p10", "palette_nn_de00_p90",
                "palette_nn_de00_cv"):
            if finite_float(row, field) < 0.0:
                fail(f"negative metric: {field}")
        previous = -1.0
        for threshold in DE_THRESHOLDS:
            field = str(threshold).replace(".", "_")
            value = percentage(row, f"de00_coverage_area_at_{field}")
            if value + 1.0e-9 < previous:
                fail("Delta E00 threshold coverage is not monotonic")
            previous = value
        validate_command(row.get("command", ""), name)
    if actual != set(MERGE_NAMES):
        fail("summary policy grid is incomplete")


def validate_patch_rows(rows: List[Dict[str, str]]) -> int:
    """Validate the policy-by-patch-size finite-area grid."""
    expected: Set[Tuple[str, int]] = {
        (name, patch_size)
        for name in MERGE_NAMES for patch_size in PATCH_SIZES
    }
    actual: Set[Tuple[str, int]] = set()
    sample_count = 0
    for row in rows:
        try:
            key = (row["merge_config"], int(row["patch_size"]))
            row_sample_count = int(row["probe_sample_count"])
        except (KeyError, ValueError) as exc:
            raise ValueError("invalid patch-size row") from exc
        if key not in expected or key in actual:
            fail("patch-size CSV has an unknown or duplicate point")
        actual.add(key)
        if int(row.get("colors", 0)) != COLORS:
            fail("patch-size CSV has an unexpected palette request")
        if row_sample_count < 256:
            fail("finite-area sensitivity sample is too small")
        if sample_count == 0:
            sample_count = row_sample_count
        elif sample_count != row_sample_count:
            fail("finite-area sample count changes between rows")
        for field in (
                "ratio_classified_unreachable_volume_percent",
                "ratio_classified_unreachable_area_percent",
                "ambiguous_ratio_volume_percent"):
            percentage(row, field)
    if actual != expected:
        fail("patch-size grid is incomplete")
    return sample_count


def validate_metadata(path: Path,
                      summary_rows: List[Dict[str, str]],
                      sample_count: int) -> None:
    """Validate provenance and the exact measurement protocol."""
    if not path.is_file():
        fail(f"missing metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        fail("unsupported reachability metadata schema")
    source = payload.get("source")
    protocol = payload.get("protocol")
    programs = payload.get("programs")
    chart = payload.get("chart_contract")
    if not all(isinstance(value, dict) for value in (
            source, protocol, programs, chart)):
        fail("reachability metadata is incomplete")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("checked-in reachability data must start from a clean tree")
    revision = source.get("revision")
    if not isinstance(revision, str) or len(revision) < 7:
        fail("measurement source revision is missing")
    expected = {
        "colors": COLORS,
        "quantizer": "heckbert:profile=compat",
        "merge_configurations": list(MERGE_NAMES),
        "palette_build_diffusion": "none",
        "empirical_diffusion": "fs",
        "empirical_scan": "raster",
        "lookup_policy": "none",
        "gpu_policy": "off",
        "threads": 1,
        "precision": "8bit",
        "clustering_colorspace": "oklab",
        "working_colorspace": "gamma",
        "output_colorspace": "gamma",
        "cover_policy": "off",
        "snap_policy": "none",
        "lab_cell_width": LAB_CELL_WIDTH,
        "patch_size": PATCH_SIZE,
        "patch_sizes": list(PATCH_SIZES),
        "stuck_ratio_threshold": STUCK_RATIO,
        "ambiguous_ratio_band": [AMBIGUOUS_LOW, AMBIGUOUS_HIGH],
        "delta_e00_thresholds": list(DE_THRESHOLDS),
        "sensitivity_probe_count": sample_count,
        "batch_fence_isolated_validation_count": 3,
    }
    for name, value in expected.items():
        if protocol.get(name) != value:
            fail(f"metadata has unexpected {name}")
    if int(protocol.get("occupied_lab_cells", 0)) != int(
            summary_rows[0]["probe_cells"]):
        fail("metadata probe count differs from the summary")
    if abs(float(protocol.get("ciede2000_sharma_pair_1", 0.0)) - 2.0425) > 0.0001:
        fail("metadata lacks the CIEDE2000 reference self-check")
    commands = protocol.get("commands")
    plane_counts = protocol.get("supporting_plane_counts")
    if not isinstance(commands, dict) or not isinstance(plane_counts, dict):
        fail("metadata lacks commands or supporting-plane counts")
    for name in MERGE_NAMES:
        validate_command(str(commands.get(name, "")), name)
        if int(plane_counts.get(name, 0)) < 4:
            fail(f"too few supporting planes for {name}")
    for name in ("img2sixel", "libsixel"):
        record = programs.get(name)
        if not isinstance(record, dict):
            fail(f"missing {name} provenance")
    for field in ("launcher_sha256", "payload_sha256"):
        value = programs["img2sixel"].get(field)
        if not isinstance(value, str) or len(value) != 64:
            fail(f"invalid img2sixel {field}")
    library_hash = programs["libsixel"].get("sha256")
    if not isinstance(library_hash, str) or len(library_hash) != 64:
        fail("invalid libsixel hash")
    if "alpha-zero" not in str(protocol.get("batch_fence", "")):
        fail("metadata does not identify the production batch fence")
    if not str(protocol.get("ratio_classifier_status", "")).startswith(
            "experimental;"):
        fail("metadata presents the ratio classifier as established")
    if "occupied 1-unit CIELAB cells" not in str(
            protocol.get("geometric_domain", "")):
        fail("metadata does not bound the volume interpretation")
    if chart.get("non_color_distinction") != (
            "line style, marker shape, hatch, labels, and panel position"):
        fail("chart contract lacks non-color distinctions")


def validate_png(path: Path) -> None:
    """Require a non-trivial readable PNG and plausible dimensions."""
    if not path.is_file() or path.stat().st_size < 10000:
        fail(f"missing or unexpectedly small plot: {path}")
    payload = path.read_bytes()[:24]
    if payload[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"invalid PNG signature: {path}")
    width, height = struct.unpack(">II", payload[16:24])
    if width < 800 or height < 400:
        fail(f"plot dimensions are too small: {path}")


def parse_args() -> argparse.Namespace:
    """Parse the artifact directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    return parser.parse_args()


def main() -> None:
    """Validate every durable reachability artifact."""
    args = parse_args()
    summary_rows = read_csv(args.directory / SUMMARY_CSV)
    patch_rows = read_csv(args.directory / PATCH_CSV)
    validate_summary(summary_rows)
    sample_count = validate_patch_rows(patch_rows)
    validate_metadata(
        args.directory / METADATA, summary_rows, sample_count
    )
    for name in PLOTS:
        validate_png(args.directory / name)


if __name__ == "__main__":
    main()
