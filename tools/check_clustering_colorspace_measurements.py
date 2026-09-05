#!/usr/bin/env python3
"""Validate clustering-color-space measurements and provenance."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Set, Tuple


COLORS = (8, 16, 32, 64, 128, 256)
COLORSPACES: Tuple[Tuple[str, str, int], ...] = (
    ("gamma", "gamma sRGB", 0),
    ("linear", "linear RGB", 1),
    ("oklab", "OKLab", 2),
    ("cielab", "CIELAB", 4),
    ("din99d", "DIN99d", 5),
)
PLOTS = (
    "clustering-colorspace-quality.png",
    "clustering-colorspace-speed.png",
    "clustering-colorspace-size.png",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read a non-empty CSV file."""
    if not path.is_file():
        fail(f"missing measurement file: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"measurement file has no rows: {path}")
    return rows


def colorspace_records() -> List[Dict[str, object]]:
    """Return the exact configuration manifest expected in metadata."""
    return [
        {
            "colorspace": colorspace,
            "label": label,
            "contract_value": contract_value,
        }
        for colorspace, label, contract_value in COLORSPACES
    ]


def read_metadata(path: Path) -> Dict[str, object]:
    """Read and validate the measurement manifest."""
    if not path.is_file():
        fail(f"missing measurement metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        fail(f"unsupported measurement metadata schema: {path}")
    source = payload.get("source")
    protocol = payload.get("protocol")
    programs = payload.get("programs")
    input_record = payload.get("input")
    if not all(
            isinstance(value, dict)
            for value in (source, protocol, programs, input_record)):
        fail(f"incomplete measurement metadata: {path}")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("checked-in measurements must originate from a clean worktree")
    revision = source.get("revision")
    if not isinstance(revision, str) or not revision or revision == "unknown":
        fail("measurement source revision is missing")
    expected_protocol = {
        "comparison_scope": (
            "clustering color spaces with a fixed K-means pipeline"
        ),
        "threads": 1,
        "precision": "float32",
        "quality": "full",
        "loader": "builtin!",
        "sampling_policy": "full-frame",
        "binning_policy": "hard",
        "quantize_model": "kmeans:seed=1",
        "merge_policy": "none",
        "cover_policy": "off",
        "working_colorspace": "gamma",
        "diffusion": "none",
        "lookup_policy": "none",
        "gpu_policy": "off",
        "palette_type": "rgb",
        "configuration_order_rotated_each_round": True,
        "sixel_environment_removed": True,
        "size_measurement": "quality-pass SIXEL stdout byte length",
    }
    for name, expected in expected_protocol.items():
        if protocol.get(name) != expected:
            fail(f"measurement metadata has unexpected {name}")
    if tuple(protocol.get("colors", [])) != COLORS:
        fail("measurement metadata has unexpected palette sizes")
    if protocol.get("colorspaces") != colorspace_records():
        fail("measurement metadata has unexpected color spaces")
    if int(protocol.get("speed_warmups", -1)) < 0:
        fail("measurement metadata has invalid warm-up count")
    if int(protocol.get("speed_runs", 0)) < 1:
        fail("measurement metadata has invalid measured-run count")
    expected_points = len(COLORS) * len(COLORSPACES)
    if protocol.get("contract_preflight_points") != expected_points:
        fail("measurement metadata has incomplete contract preflight")
    preflight = protocol.get("contract_preflight")
    if not isinstance(preflight, str) or "zero quantizer retries" not in preflight:
        fail("measurement metadata has unexpected contract preflight")
    palette_timing = protocol.get("palette_timing")
    if not isinstance(palette_timing, str) or "outside" not in palette_timing:
        fail("measurement metadata has unexpected palette timing definition")
    end_timing = protocol.get("end_to_end_timing")
    if not isinstance(end_timing, str) or "fresh-process" not in end_timing:
        fail("measurement metadata has unexpected end-to-end timing")
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
    return payload


def require_pair(tokens: List[str], option: str, value: str, path: Path) -> None:
    """Require one two-token option/value pair."""
    try:
        index = tokens.index(option)
    except ValueError as exc:
        raise ValueError(f"command lacks {option} in {path}") from exc
    if index + 1 >= len(tokens) or tokens[index + 1] != value:
        fail(f"command has unexpected {option} value in {path}")


def validate_command(row: Dict[str, str],
                     path: Path,
                     timeline: bool = False) -> None:
    """Check one recorded command and its row identity."""
    colorspace = row.get("colorspace", "")
    expected = {
        name: label for name, label, _contract_value in COLORSPACES
    }
    if colorspace not in expected:
        fail(f"unexpected color space in {path}: {colorspace}")
    if row.get("label") != expected[colorspace]:
        fail(f"color-space label differs in {path}: {colorspace}")
    field = "timeline_command" if timeline else "command"
    try:
        tokens = shlex.split(row.get(field, ""))
    except ValueError as exc:
        raise ValueError(
            f"invalid {field} in {path}: {colorspace}"
        ) from exc
    required = {
        "--threads=1",
        "--precision=float32",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1",
        f"-X{colorspace}",
        "-Wgamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
    }
    missing = sorted(required - set(tokens))
    if missing:
        fail(f"command controls missing in {path}: {colorspace}: {missing}")
    require_pair(tokens, "-F", "none", path)
    require_pair(tokens, "-a", "off", path)
    try:
        color_index = tokens.index("-p") + 1
        command_colors = int(tokens[color_index])
        row_colors = int(row.get("colors", ""))
    except (ValueError, IndexError) as exc:
        raise ValueError(f"invalid command palette size in {path}") from exc
    if command_colors != row_colors:
        fail(f"command palette size differs from row in {path}")
    has_timeline = "-J" in tokens
    if timeline:
        if not has_timeline or "{timeline}" not in tokens:
            fail(f"timeline command lacks placeholder in {path}: {colorspace}")
    elif has_timeline:
        fail(f"uninstrumented command contains timeline option in {path}")


def expected_points() -> Set[Tuple[str, int]]:
    """Return the complete color-space-by-K point set."""
    return {
        (colorspace, colors)
        for colorspace, _label, _contract_value in COLORSPACES
        for colors in COLORS
    }


def validate_rows(path: Path, revision: str) -> List[Dict[str, str]]:
    """Validate common row identity and completeness."""
    rows = read_csv(path)
    actual: Set[Tuple[str, int]] = set()
    for row in rows:
        validate_command(row, path)
        try:
            key = (row.get("colorspace", ""), int(row.get("colors", "")))
        except ValueError as exc:
            raise ValueError(f"invalid palette size in {path}") from exc
        if key in actual:
            fail(f"duplicate measurement point in {path}: {key}")
        actual.add(key)
        if row.get("revision") != revision:
            fail(f"row revision differs from metadata in {path}: {key}")
    expected = expected_points()
    if actual != expected:
        fail(
            f"measurement sweep mismatch in {path}: "
            f"missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )
    return rows


def validate_quality(path: Path, revision: str) -> None:
    """Validate all quality values."""
    for row in validate_rows(path, revision):
        key = (row["colorspace"], row["colors"])
        try:
            ms_ssim = float(row.get("MS-SSIM", ""))
            delta_e00 = float(row.get("Delta E00_mean", ""))
            delta_chroma = float(row.get("Delta Chroma_mean", ""))
        except ValueError as exc:
            raise ValueError(f"invalid quality value in {path}: {key}") from exc
        if not math.isfinite(ms_ssim) or not 0.0 <= ms_ssim <= 1.0:
            fail(f"invalid MS-SSIM in {path}: {key}")
        for name, value in (
                ("Delta E00", delta_e00),
                ("Delta Chroma", delta_chroma)):
            if not math.isfinite(value) or value < 0.0:
                fail(f"invalid {name} in {path}: {key}")


def validate_size(path: Path, revision: str) -> None:
    """Validate exact stream lengths and gamma ratios."""
    for row in validate_rows(path, revision):
        key = (row["colorspace"], row["colors"])
        try:
            encoded_bytes = int(row.get("encoded_bytes", ""))
            ratio = float(row.get("bytes_vs_gamma", ""))
        except ValueError as exc:
            raise ValueError(f"invalid size value in {path}: {key}") from exc
        if encoded_bytes <= 0 or not math.isfinite(ratio) or ratio <= 0.0:
            fail(f"invalid size value in {path}: {key}")
        if row["colorspace"] == "gamma" and not math.isclose(
                ratio, 1.0, rel_tol=1e-12):
            fail(f"gamma size baseline is not 1.0 in {path}: {key}")


def validate_speed(path: Path, revision: str, expected_runs: int) -> None:
    """Validate both speed domains and their intervals."""
    for row in validate_rows(path, revision):
        validate_command(row, path, True)
        key = (row["colorspace"], row["colors"])
        try:
            runs = int(row.get("runs", ""))
            palette = [
                float(row[f"palette_{name}_seconds"])
                for name in ("median", "q1", "q3", "min", "max")
            ]
            end_to_end = [
                float(row[f"end_to_end_{name}_seconds"])
                for name in ("median", "q1", "q3", "min", "max")
            ]
            palette_speedup = float(row["palette_speedup_vs_gamma"])
            end_speedup = float(row["end_to_end_speedup_vs_gamma"])
        except (KeyError, ValueError) as exc:
            raise ValueError(f"invalid speed value in {path}: {key}") from exc
        if runs != expected_runs:
            fail(f"speed run count differs from metadata in {path}: {key}")
        values = palette + end_to_end + [palette_speedup, end_speedup]
        if not all(math.isfinite(value) and value > 0.0 for value in values):
            fail(f"invalid speed value in {path}: {key}")
        for domain, values in (("palette", palette), ("end", end_to_end)):
            median, q1, q3, minimum, maximum = values
            if not minimum <= q1 <= median <= q3 <= maximum:
                fail(f"invalid {domain} interval in {path}: {key}")
        if row["colorspace"] == "gamma":
            if not math.isclose(palette_speedup, 1.0, rel_tol=1e-12):
                fail(f"palette gamma baseline is not 1.0: {key}")
            if not math.isclose(end_speedup, 1.0, rel_tol=1e-12):
                fail(f"end-to-end gamma baseline is not 1.0: {key}")


def validate_plots(directory: Path) -> None:
    """Reject absent or implausibly small rendered figures."""
    for name in PLOTS:
        path = directory / name
        if not path.is_file():
            fail(f"missing plot: {path}")
        if path.stat().st_size < 20000:
            fail(f"plot is unexpectedly small: {path}")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    return parser.parse_args()


def main() -> int:
    """Validate one generated clustering-color-space artifact directory."""
    args = parse_args()
    directory = args.directory.resolve()
    metadata = read_metadata(directory / "clustering-colorspace-run.json")
    revision = str(metadata["source"]["revision"])
    runs = int(metadata["protocol"]["speed_runs"])
    validate_quality(
        directory / "clustering-colorspace-quality.csv",
        revision,
    )
    validate_size(
        directory / "clustering-colorspace-size.csv",
        revision,
    )
    validate_speed(
        directory / "clustering-colorspace-speed.csv",
        revision,
        runs,
    )
    validate_plots(directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
