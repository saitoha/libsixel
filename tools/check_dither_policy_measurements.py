#!/usr/bin/env python3
"""Validate dither-policy measurement completeness and provenance."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Set, Tuple


COLORS = (8, 16, 32, 64, 128, 256)
METHODS: Tuple[Tuple[str, str], ...] = (
    ("none", "none:scan=raster"),
    ("fs", "fs:scan=raster"),
    ("atkinson", "atkinson:scan=raster"),
    ("jajuni", "jajuni:scan=raster"),
    ("stucki", "stucki:scan=raster"),
    ("burkes", "burkes:scan=raster"),
    ("sierra1", "sierra:variant=1:scan=raster"),
    ("sierra2", "sierra:variant=2:scan=raster"),
    ("sierra3", "sierra:variant=3:scan=raster"),
    ("lso2", "lso2:scan=raster"),
    ("a_dither", "a_dither:strength=0.150:scan=raster"),
    ("x_dither", "x_dither:strength=0.100:scan=raster"),
    (
        "bluenoise",
        "bluenoise:strength=0.055:gradient_factor=0:"
        "phase=0,0:channel=mono:size=64:scan=raster",
    ),
)
PLOTS = (
    "dither-policy-ms-ssim.png",
    "dither-policy-delta-e00.png",
    "dither-policy-speed.png",
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
        for value in (source, protocol, programs, input_record)
    ):
        fail(f"incomplete measurement metadata: {path}")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("checked-in measurements must originate from a clean worktree")
    revision = source.get("revision")
    if not isinstance(revision, str) or not revision or revision == "unknown":
        fail("measurement source revision is missing")
    if protocol.get("sixel_environment_removed") is not True:
        fail("measurement metadata does not confirm SIXEL_* isolation")
    if tuple(protocol.get("colors", [])) != COLORS:
        fail("measurement metadata has unexpected palette sizes")
    expected_methods = [
        {"name": method, "diffusion_option": diffusion}
        for method, diffusion in METHODS
    ]
    if protocol.get("methods") != expected_methods:
        fail("measurement metadata has unexpected dither methods")
    if protocol.get("method_order_rotated_each_round") is not True:
        fail("measurement metadata does not confirm rotated method order")
    if int(protocol.get("speed_warmups", -1)) < 0:
        fail("measurement metadata has invalid warm-up count")
    if int(protocol.get("speed_runs", 0)) < 1:
        fail("measurement metadata has invalid measured-run count")
    expected_controls = {
        "threads": 1,
        "precision": "8bit",
        "quantize_model": "kmeans:merge=ward:seed=1",
        "clustering_colorspace": "oklab",
        "working_colorspace": "gamma",
        "lookup_policy": "none",
        "scan": "raster",
        "gpu_policy": "off",
    }
    for name, expected in expected_controls.items():
        if protocol.get(name) != expected:
            fail(f"measurement metadata has unexpected {name}")
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


def validate_command(row: Dict[str, str], path: Path) -> None:
    """Check the controlled CLI shape recorded in one row."""
    method = row.get("method", "")
    options = dict(METHODS)
    if method not in options:
        fail(f"unexpected method in {path}: {method}")
    diffusion = row.get("diffusion_option", "")
    if diffusion != options[method]:
        fail(f"method/diffusion mismatch in {path}: {method}")
    try:
        tokens = shlex.split(row.get("command", ""))
    except ValueError as exc:
        raise ValueError(f"invalid command in {path}: {method}") from exc
    required = {
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=libpng!",
        "--quantize-model=kmeans:merge=ward:seed=1",
        "-Xoklab",
        "-Wgamma",
        f"--diffusion={diffusion}",
        "--gpu-policy=off",
        "--lookup-policy=none",
    }
    missing = sorted(required - set(tokens))
    if missing:
        fail(f"command controls missing in {path}: {missing}")
    try:
        color_index = tokens.index("-p") + 1
        command_colors = int(tokens[color_index])
        row_colors = int(row.get("colors", ""))
    except (ValueError, IndexError) as exc:
        raise ValueError(f"invalid command palette size in {path}") from exc
    if command_colors != row_colors:
        fail(f"command palette size differs from row in {path}")


def validate_quality(path: Path, revision: str) -> None:
    """Validate all dither quality points and metrics."""
    rows = read_csv(path)
    actual: Set[Tuple[str, int]] = set()
    for row in rows:
        validate_command(row, path)
        method = row.get("method", "")
        try:
            colors = int(row.get("colors", ""))
            ms_ssim = float(row.get("MS-SSIM", ""))
            delta_e00 = float(row.get("Delta E00_mean", ""))
        except ValueError as exc:
            raise ValueError(f"invalid quality value in {path}") from exc
        key = (method, colors)
        if key in actual:
            fail(f"duplicate quality point in {path}: {key}")
        actual.add(key)
        if row.get("revision") != revision:
            fail(f"quality revision differs from metadata in {path}")
        if not math.isfinite(ms_ssim) or not 0.0 <= ms_ssim <= 1.0:
            fail(f"invalid MS-SSIM in {path}: {key}")
        if not math.isfinite(delta_e00) or delta_e00 < 0.0:
            fail(f"invalid Delta E00 in {path}: {key}")
    expected = {
        (method, colors)
        for method, _diffusion in METHODS
        for colors in COLORS
    }
    if actual != expected:
        fail(
            f"quality sweep mismatch in {path}: "
            f"missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )


def validate_speed(path: Path, revision: str, expected_runs: int) -> None:
    """Validate all speed points and the no-dither normalization."""
    rows = read_csv(path)
    actual: Set[Tuple[str, int]] = set()
    for row in rows:
        validate_command(row, path)
        method = row.get("method", "")
        try:
            colors = int(row.get("colors", ""))
            runs = int(row.get("runs", ""))
            median = float(row.get("median_seconds", ""))
            q1 = float(row.get("q1_seconds", ""))
            q3 = float(row.get("q3_seconds", ""))
            speedup = float(row.get("speedup_vs_none", ""))
        except ValueError as exc:
            raise ValueError(f"invalid speed value in {path}") from exc
        key = (method, colors)
        if key in actual:
            fail(f"duplicate speed point in {path}: {key}")
        actual.add(key)
        if row.get("revision") != revision:
            fail(f"speed revision differs from metadata in {path}")
        if runs != expected_runs:
            fail(f"speed run count differs from metadata in {path}: {key}")
        if not all(math.isfinite(value) for value in (median, q1, q3, speedup)):
            fail(f"non-finite speed value in {path}: {key}")
        if median <= 0.0 or speedup <= 0.0 or not q1 <= median <= q3:
            fail(f"invalid speed interval in {path}: {key}")
        if method == "none" and not math.isclose(
            speedup,
            1.0,
            rel_tol=1e-12,
        ):
            fail(f"none speed baseline is not 1.0 in {path}: {key}")
    expected = {
        (method, colors)
        for method, _diffusion in METHODS
        for colors in COLORS
    }
    if actual != expected:
        fail(
            f"speed sweep mismatch in {path}: "
            f"missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )


def validate_plots(directory: Path) -> None:
    """Require each plot to exist and contain data."""
    for name in PLOTS:
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            fail(f"missing or empty plot: {path}")


def main() -> int:
    """Validate one complete dither measurement directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    metadata = read_metadata(args.directory / "dither-policy-run.json")
    source = metadata["source"]
    protocol = metadata["protocol"]
    revision = str(source["revision"])
    runs = int(protocol["speed_runs"])
    validate_quality(
        args.directory / "dither-policy-quality.csv",
        revision,
    )
    validate_speed(
        args.directory / "dither-policy-speed.csv",
        revision,
        runs,
    )
    validate_plots(args.directory)
    print("validated 78 quality and 78 speed points across 13 dither methods")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
