#!/usr/bin/env python3
"""Validate working-color-space measurements and provenance."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Set, Tuple


COLORS = (8, 16, 32, 64, 128, 256)
DIFFUSIONS = ("none", "fs")
COLORSPACES: Tuple[Tuple[str, str, int, str], ...] = (
    ("gamma", "gamma sRGB", 0, "rgb-f32"),
    ("linear", "linear RGB", 1, "linear-f32"),
    ("oklab", "Oklab", 2, "oklab-f32"),
    ("cielab", "CIELAB", 4, "cielab-f32"),
    ("din99d", "DIN99d", 5, "din99d-f32"),
)
CSV_NAME = "working-colorspace-comparison.csv"
PLOTS = (
    "working-colorspace-quality.png",
    "working-colorspace-performance.png",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def records() -> List[Dict[str, object]]:
    """Return the exact expected color-space manifest."""
    return [
        {
            "colorspace": name,
            "label": label,
            "contract_value": value,
            "work_format": work_format,
        }
        for name, label, value, work_format in COLORSPACES
    ]


def read_metadata(path: Path) -> Dict[str, object]:
    """Read and validate the provenance manifest."""
    if not path.is_file():
        fail(f"missing measurement metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        fail("unsupported working-colorspace metadata schema")
    source = payload.get("source")
    protocol = payload.get("protocol")
    programs = payload.get("programs")
    input_record = payload.get("input")
    chart = payload.get("chart_contract")
    if not all(isinstance(value, dict) for value in (
            source, protocol, programs, input_record, chart)):
        fail("incomplete working-colorspace metadata")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("checked-in measurements must originate from a clean worktree")
    revision = source.get("revision")
    if not isinstance(revision, str) or len(revision) < 7:
        fail("measurement source revision is missing")
    expected = {
        "comparison_scope": (
            "working color spaces with fixed gamma palette construction"
        ),
        "colors": list(COLORS),
        "colorspaces": records(),
        "diffusions": list(DIFFUSIONS),
        "threads": 1,
        "precision": "float32",
        "quality": "full",
        "loader": "builtin!",
        "sampling_policy": "full-frame",
        "binning_policy": "hard",
        "quantize_model": "kmeans:seed=1:binbits=6",
        "merge_policy": "none",
        "cover_policy": "off",
        "clustering_colorspace": "gamma",
        "output_colorspace": "gamma",
        "lookup_policy": "none",
        "gpu_policy": "off",
        "palette_type": "rgb",
        "encode_policy": "fast",
        "configuration_order_rotated_and_reversed": True,
        "sixel_environment_removed": True,
        "contract_preflight_points": len(DIFFUSIONS) * len(COLORSPACES),
    }
    for name, value in expected.items():
        if protocol.get(name) != value:
            fail(f"measurement metadata has unexpected {name}")
    if int(protocol.get("speed_warmups", -1)) < 0:
        fail("measurement metadata has invalid warm-up count")
    if int(protocol.get("speed_runs", 0)) < 1:
        fail("measurement metadata has invalid measured-run count")
    if "zero quantizer retries" not in str(
            protocol.get("contract_preflight", "")):
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


def validate_command(row: Dict[str, str], path: Path) -> None:
    """Validate controlled options in one recorded command."""
    colorspace = row.get("colorspace", "")
    diffusion = row.get("diffusion", "")
    manifest = {name: (label, work) for name, label, _value, work in COLORSPACES}
    if colorspace not in manifest or diffusion not in DIFFUSIONS:
        fail(f"unexpected row identity in {path}")
    label, work_format = manifest[colorspace]
    if row.get("label") != label or row.get("work_format") != work_format:
        fail(f"color-space contract differs in {path}: {colorspace}")
    try:
        tokens = shlex.split(row.get("command", ""))
    except ValueError as exc:
        raise ValueError(f"invalid command in {path}") from exc
    required = {
        "--threads=1",
        "--precision=float32",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        "-Xgamma",
        f"-W{colorspace}",
        "-Ugamma",
        f"--diffusion={diffusion}",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "--encode-policy=fast",
        "{input}",
    }
    missing = sorted(required - set(tokens))
    if missing:
        fail(f"command controls missing in {path}: {missing}")
    for option, value in (("-F", "none"), ("-a", "off")):
        try:
            index = tokens.index(option)
        except ValueError as exc:
            raise ValueError(f"command lacks {option} in {path}") from exc
        if index + 1 >= len(tokens) or tokens[index + 1] != value:
            fail(f"command has unexpected {option} value in {path}")
    try:
        color_index = tokens.index("-p") + 1
        command_colors = int(tokens[color_index])
        row_colors = int(row.get("colors", ""))
    except (ValueError, IndexError) as exc:
        raise ValueError(f"invalid palette size in {path}") from exc
    if command_colors != row_colors:
        fail(f"command palette size differs from row in {path}")


def expected_points() -> Set[Tuple[str, int, str]]:
    """Return the complete dither-by-K-by-working-space grid."""
    return {
        (diffusion, colors, colorspace)
        for diffusion in DIFFUSIONS
        for colors in COLORS
        for colorspace, _label, _value, _work in COLORSPACES
    }


def read_rows(path: Path, revision: str) -> List[Dict[str, str]]:
    """Read and validate every measurement row."""
    if not path.is_file():
        fail(f"missing measurement CSV: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    actual: Set[Tuple[str, int, str]] = set()
    for row in rows:
        validate_command(row, path)
        try:
            key = (
                row["diffusion"],
                int(row["colors"]),
                row["colorspace"],
            )
            values = {
                "MS-SSIM": float(row["MS-SSIM"]),
                "Delta E00_mean": float(row["Delta E00_mean"]),
                "Delta Chroma_mean": float(row["Delta Chroma_mean"]),
                "size_ratio_vs_gamma": float(row["size_ratio_vs_gamma"]),
                "median_seconds": float(row["median_seconds"]),
                "q1_seconds": float(row["q1_seconds"]),
                "q3_seconds": float(row["q3_seconds"]),
                "min_seconds": float(row["min_seconds"]),
                "max_seconds": float(row["max_seconds"]),
                "time_ratio_vs_gamma": float(row["time_ratio_vs_gamma"]),
            }
            encoded_bytes = int(row["encoded_bytes"])
            runs = int(row["runs"])
        except (KeyError, ValueError) as exc:
            raise ValueError(f"invalid numeric row in {path}") from exc
        if key in actual:
            fail(f"duplicate measurement point in {path}: {key}")
        actual.add(key)
        if row.get("revision") != revision:
            fail(f"row revision differs from metadata in {path}: {key}")
        if not all(math.isfinite(value) for value in values.values()):
            fail(f"non-finite measurement in {path}: {key}")
        if not 0.0 <= values["MS-SSIM"] <= 1.0:
            fail(f"invalid MS-SSIM in {path}: {key}")
        if values["Delta E00_mean"] < 0.0 or values["Delta Chroma_mean"] < 0.0:
            fail(f"negative color error in {path}: {key}")
        if encoded_bytes < 1 or runs < 1 or values["median_seconds"] <= 0.0:
            fail(f"invalid size or timing in {path}: {key}")
        order = (
            values["min_seconds"],
            values["q1_seconds"],
            values["median_seconds"],
            values["q3_seconds"],
            values["max_seconds"],
        )
        if tuple(sorted(order)) != order:
            fail(f"timing summary is unordered in {path}: {key}")
    if actual != expected_points():
        fail("working-colorspace measurement grid is incomplete")
    return rows


def validate_ratios(rows: List[Dict[str, str]]) -> None:
    """Recompute gamma-relative size and timing ratios."""
    by_key = {
        (row["diffusion"], int(row["colors"]), row["colorspace"]): row
        for row in rows
    }
    for diffusion in DIFFUSIONS:
        for colors in COLORS:
            gamma = by_key[(diffusion, colors, "gamma")]
            gamma_size = int(gamma["encoded_bytes"])
            gamma_time = float(gamma["median_seconds"])
            for colorspace, _label, _value, _work in COLORSPACES:
                row = by_key[(diffusion, colors, colorspace)]
                size_ratio = int(row["encoded_bytes"]) / gamma_size
                time_ratio = float(row["median_seconds"]) / gamma_time
                if not math.isclose(
                        size_ratio,
                        float(row["size_ratio_vs_gamma"]),
                        rel_tol=1.0e-12):
                    fail("stored size ratio does not match raw values")
                if not math.isclose(
                        time_ratio,
                        float(row["time_ratio_vs_gamma"]),
                        rel_tol=1.0e-12):
                    fail("stored time ratio does not match raw values")


def validate_plots(directory: Path) -> None:
    """Require non-empty PNG artifacts."""
    for name in PLOTS:
        path = directory / name
        if not path.is_file() or path.stat().st_size < 10000:
            fail(f"missing or unexpectedly small plot: {path}")
        with path.open("rb") as handle:
            if handle.read(8) != b"\x89PNG\r\n\x1a\n":
                fail(f"measurement plot is not a PNG: {path}")


def main() -> int:
    """Validate one working-color-space measurement directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    metadata = read_metadata(args.directory / "working-colorspace-run.json")
    revision = str(metadata["source"]["revision"])
    rows = read_rows(args.directory / CSV_NAME, revision)
    validate_ratios(rows)
    validate_plots(args.directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
