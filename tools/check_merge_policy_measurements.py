#!/usr/bin/env python3
"""Validate merge-policy measurement completeness and provenance."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
import struct
from pathlib import Path
from typing import Dict, List, Set, Tuple

from plot_merge_policy_measurements import (
    COLORS,
    merge_records,
    quantizer_records,
)


CSV_NAME = "merge-policy-comparison.csv"
PLOTS = (
    "merge-policy-quality.png",
    "merge-policy-performance.png",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def read_metadata(path: Path) -> Dict[str, object]:
    """Read and validate the provenance manifest."""
    if not path.is_file():
        fail(f"missing measurement metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        fail("unsupported merge-policy metadata schema")
    source = payload.get("source")
    protocol = payload.get("protocol")
    programs = payload.get("programs")
    input_record = payload.get("input")
    chart = payload.get("chart_contract")
    if not all(isinstance(value, dict) for value in (
            source, protocol, programs, input_record, chart)):
        fail("incomplete merge-policy metadata")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("checked-in measurements must originate from a clean worktree")
    revision = source.get("revision")
    if not isinstance(revision, str) or len(revision) < 7:
        fail("measurement source revision is missing")
    expected = {
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
        "configuration_order_rotated_and_reversed": True,
        "sixel_environment_removed": True,
        "contract_preflight_points": (
            len(COLORS) * len(quantizer_records()) * len(merge_records())
        ),
    }
    for name, value in expected.items():
        if protocol.get(name) != value:
            fail(f"measurement metadata has unexpected {name}")
    if int(protocol.get("speed_warmups", -1)) < 0:
        fail("measurement metadata has invalid warm-up count")
    if int(protocol.get("speed_runs", 0)) < 1:
        fail("measurement metadata has invalid measured-run count")
    preflight = str(protocol.get("contract_preflight", ""))
    if "zero quantizer retries" not in preflight:
        fail("measurement metadata lacks fallback preflight")
    for name in ("img2sixel", "lsqa"):
        record = programs.get(name)
        if not isinstance(record, dict):
            fail(f"missing {name} provenance")
        for field in ("launcher_sha256", "payload_sha256"):
            value = record.get(field)
            if not isinstance(value, str) or len(value) != 64:
                fail(f"invalid {name} {field}")
    input_hash = input_record.get("sha256")
    if not isinstance(input_hash, str) or len(input_hash) != 64:
        fail("invalid input SHA-256")
    if chart.get("non_color_distinction") != "marker shape and line style":
        fail("chart contract lacks a non-color distinction")
    return payload


def expected_points() -> Set[Tuple[str, int, str]]:
    """Return the exact quantizer-by-K-by-merge grid."""
    return {
        (
            str(quantizer["quantizer"]),
            colors,
            str(merge["merge_config"]),
        )
        for quantizer in quantizer_records()
        for colors in COLORS
        for merge in merge_records()
    }


def validate_command(row: Dict[str, str], timeline: bool) -> None:
    """Validate all controlled options in one recorded command."""
    quantizers = {
        str(record["quantizer"]): record
        for record in quantizer_records()
    }
    merges = {
        str(record["merge_config"]): record
        for record in merge_records()
    }
    quantizer = quantizers.get(row.get("quantizer", ""))
    merge = merges.get(row.get("merge_config", ""))
    if quantizer is None or merge is None:
        fail("row contains an unknown quantizer or merge configuration")
    if row.get("quantizer_label") != quantizer["quantizer_label"]:
        fail("row quantizer label differs from the manifest")
    if row.get("quantize_option") != quantizer["quantize_option"]:
        fail("row quantize option differs from the manifest")
    if row.get("binning_policy") != quantizer["binning_policy"]:
        fail("row binning policy differs from the manifest")
    if row.get("merge_label") != merge["merge_label"]:
        fail("row merge label differs from the manifest")
    if row.get("merge_option") != merge["merge_option"]:
        fail("row merge option differs from the manifest")
    field = "timeline_command" if timeline else "command"
    try:
        tokens = shlex.split(row.get(field, ""))
    except ValueError as exc:
        raise ValueError(f"invalid {field}") from exc
    required = {
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
        "{input}",
    }
    missing = sorted(required - set(tokens))
    if missing:
        fail(f"controlled command options are missing: {missing}")
    try:
        command_colors = int(tokens[tokens.index("-p") + 1])
        row_colors = int(row["colors"])
    except (KeyError, ValueError, IndexError) as exc:
        raise ValueError("invalid palette size in command") from exc
    if command_colors != row_colors:
        fail("command palette size differs from the row")
    if timeline:
        if "-J" not in tokens or "{timeline}" not in tokens:
            fail("timeline command lacks its timeline placeholder")
    elif "-J" in tokens:
        fail("quality command unexpectedly enables timeline logging")


def finite_float(row: Dict[str, str], field: str) -> float:
    """Read one finite floating-point field."""
    try:
        value = float(row[field])
    except (KeyError, ValueError) as exc:
        raise ValueError(f"invalid numeric field: {field}") from exc
    if not math.isfinite(value):
        fail(f"non-finite numeric field: {field}")
    return value


def read_rows(path: Path, revision: str) -> List[Dict[str, str]]:
    """Read and validate all measurement rows."""
    if not path.is_file():
        fail(f"missing measurement CSV: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    actual: Set[Tuple[str, int, str]] = set()
    for row in rows:
        validate_command(row, False)
        validate_command(row, True)
        try:
            key = (
                row["quantizer"],
                int(row["colors"]),
                row["merge_config"],
            )
            encoded_bytes = int(row["encoded_bytes"])
            runs = int(row["runs"])
            lloyd = int(row["merge_lloyd"])
        except (KeyError, ValueError) as exc:
            raise ValueError("invalid row identity or integer field") from exc
        if key in actual:
            fail(f"duplicate measurement point: {key}")
        actual.add(key)
        if row.get("revision") != revision:
            fail(f"row revision differs from metadata: {key}")
        if encoded_bytes <= 0 or runs < 1:
            fail(f"invalid size or run count: {key}")
        expected_merge = next(
            record for record in merge_records()
            if record["merge_config"] == key[2]
        )
        if lloyd != expected_merge["merge_lloyd"]:
            fail(f"merge Lloyd count differs from manifest: {key}")
        fields = (
            "merge_oversplit",
            "MS-SSIM",
            "MS-SSIM_delta_vs_none",
            "Delta E00_mean",
            "Delta E00_delta_vs_none",
            "Delta Chroma_mean",
            "bytes_ratio_vs_none",
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
        )
        values = {field: finite_float(row, field) for field in fields}
        if not 0.0 <= values["MS-SSIM"] <= 1.0:
            fail(f"MS-SSIM is outside [0, 1]: {key}")
        if values["Delta E00_mean"] < 0.0 \
                or values["Delta Chroma_mean"] < 0.0:
            fail(f"negative quality error: {key}")
        for prefix in ("palette", "end_to_end"):
            if values[f"{prefix}_median_seconds"] <= 0.0:
                fail(f"non-positive {prefix} timing: {key}")
        if key[2] == "none":
            if abs(values["merge_stage_median_seconds"]) > 1e-15:
                fail(f"disabled merge recorded merge-stage work: {key}")
        elif values["merge_stage_median_seconds"] <= 0.0:
            fail(f"enabled merge recorded no merge-stage work: {key}")
    expected = expected_points()
    if actual != expected:
        fail(
            "measurement grid differs from the manifest: "
            f"missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )
    return rows


def check_derived_values(rows: List[Dict[str, str]]) -> None:
    """Recompute all values expressed relative to the no-merge baseline."""
    by_key = {
        (row["quantizer"], int(row["colors"]), row["merge_config"]): row
        for row in rows
    }
    for quantizer in quantizer_records():
        name = str(quantizer["quantizer"])
        for colors in COLORS:
            baseline = by_key[(name, colors, "none")]
            for merge in merge_records():
                key = (name, colors, str(merge["merge_config"]))
                row = by_key[key]
                expected = {
                    "MS-SSIM_delta_vs_none": (
                        float(row["MS-SSIM"])
                        - float(baseline["MS-SSIM"])
                    ),
                    "Delta E00_delta_vs_none": (
                        float(row["Delta E00_mean"])
                        - float(baseline["Delta E00_mean"])
                    ),
                    "bytes_ratio_vs_none": (
                        int(row["encoded_bytes"])
                        / int(baseline["encoded_bytes"])
                    ),
                    "palette_ratio_vs_none": (
                        float(row["palette_median_seconds"])
                        / float(baseline["palette_median_seconds"])
                    ),
                    "end_to_end_ratio_vs_none": (
                        float(row["end_to_end_median_seconds"])
                        / float(baseline["end_to_end_median_seconds"])
                    ),
                }
                for field, value in expected.items():
                    if not math.isclose(
                            float(row[field]),
                            value,
                            rel_tol=1e-9,
                            abs_tol=1e-12):
                        fail(f"derived field is stale: {key}: {field}")


def check_png(path: Path) -> None:
    """Require one non-trivial PNG with plausible dimensions."""
    if not path.is_file() or path.stat().st_size < 4096:
        fail(f"missing or trivial plot: {path}")
    with path.open("rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"plot is not a PNG: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if width < 800 or height < 500:
        fail(f"plot dimensions are unexpectedly small: {path}")


def main() -> int:
    """Validate one checked-in merge-policy measurement directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    metadata = read_metadata(args.directory / "merge-policy-run.json")
    revision = str(metadata["source"]["revision"])
    rows = read_rows(args.directory / CSV_NAME, revision)
    check_derived_values(rows)
    for name in PLOTS:
        check_png(args.directory / name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
