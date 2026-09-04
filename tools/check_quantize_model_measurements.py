#!/usr/bin/env python3
"""Validate quantize-model measurement completeness and provenance."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Set, Tuple


COLORS = (8, 16, 32, 64, 128, 256)
CONFIGS: Tuple[Tuple[str, str, str, str], ...] = (
    ("heckbert-compat", "compat", "heckbert", "heckbert:profile=compat"),
    ("heckbert-speed", "speed", "heckbert", "heckbert:profile=speed"),
    ("heckbert-quality", "quality", "heckbert", "heckbert:profile=quality"),
    ("kmeans", "k-means", "kmeans", "kmeans:seed=1"),
    ("medoids-pam", "PAM", "medoids", "medoids:algo=pam:seed=1"),
    ("medoids-sample", "CLARA", "medoids", "medoids:algo=sample:seed=1"),
    ("medoids-random", "CLARANS", "medoids", "medoids:algo=random:seed=1"),
    ("medoids-bandit", "BanditPAM", "medoids", "medoids:algo=bandit:seed=1"),
    ("center-fft", "FFT", "center", "center:algo=fft:seed=1"),
    ("center-swap", "swap", "center", "center:algo=swap:seed=1"),
    ("center-hybrid", "hybrid", "center", "center:algo=hybrid:seed=1"),
)
PLOTS = (
    "quantize-model-quality.png",
    "quantize-model-speed.png",
    "quantize-model-size.png",
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


def expected_config_records() -> List[Dict[str, str]]:
    """Return the exact configuration manifest expected in metadata."""
    return [
        {
            "config": config,
            "label": label,
            "family": family,
            "quantize_option": option,
        }
        for config, label, family, option in CONFIGS
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
        for value in (source, protocol, programs, input_record)
    ):
        fail(f"incomplete measurement metadata: {path}")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("checked-in measurements must originate from a clean worktree")
    revision = source.get("revision")
    if not isinstance(revision, str) or not revision or revision == "unknown":
        fail("measurement source revision is missing")
    expected_protocol = {
        "comparison_scope": "complete explicit -Q configurations",
        "threads": 1,
        "precision": "8bit",
        "quality": "full",
        "loader": "libpng!",
        "clustering_colorspace": "oklab",
        "working_colorspace": "gamma",
        "diffusion": "none",
        "lookup_policy": "none",
        "gpu_policy": "off",
        "merge_policy": "configuration default",
        "cover_policy": "configuration default",
        "configuration_order_rotated_each_round": True,
        "sixel_environment_removed": True,
        "size_measurement": "quality-pass SIXEL stdout byte length",
        "end_to_end_timing": "uninstrumented fresh-process monotonic wall time",
    }
    for name, expected in expected_protocol.items():
        if protocol.get(name) != expected:
            fail(f"measurement metadata has unexpected {name}")
    if tuple(protocol.get("colors", [])) != COLORS:
        fail("measurement metadata has unexpected palette sizes")
    if protocol.get("configurations") != expected_config_records():
        fail("measurement metadata has unexpected configurations")
    if int(protocol.get("speed_warmups", -1)) < 0:
        fail("measurement metadata has invalid warm-up count")
    if int(protocol.get("speed_runs", 0)) < 1:
        fail("measurement metadata has invalid measured-run count")
    palette_timing = protocol.get("palette_timing")
    if not isinstance(palette_timing, str) or "top-level" not in palette_timing:
        fail("measurement metadata has unexpected palette timing definition")
    auto_omission = protocol.get("auto_omission")
    if not isinstance(auto_omission, str) or "compat" not in auto_omission:
        fail("measurement metadata does not explain the auto omission")
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


def validate_command(row: Dict[str, str],
                     path: Path,
                     timeline: bool = False) -> None:
    """Check one recorded command and its row identity."""
    config = row.get("config", "")
    expected = {
        name: (label, family, option)
        for name, label, family, option in CONFIGS
    }
    if config not in expected:
        fail(f"unexpected configuration in {path}: {config}")
    label, family, option = expected[config]
    if row.get("label") != label or row.get("family") != family:
        fail(f"configuration labels differ in {path}: {config}")
    if row.get("quantize_option") != option:
        fail(f"quantize option differs in {path}: {config}")
    field = "timeline_command" if timeline else "command"
    try:
        tokens = shlex.split(row.get(field, ""))
    except ValueError as exc:
        raise ValueError(f"invalid {field} in {path}: {config}") from exc
    required = {
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=libpng!",
        f"--quantize-model={option}",
        "-Xoklab",
        "-Wgamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
    }
    missing = sorted(required - set(tokens))
    if missing:
        fail(f"command controls missing in {path}: {config}: {missing}")
    forbidden = {"--merge-policy", "-F", "--cover-policy", "-a"}
    if forbidden & set(tokens):
        fail(f"command overrides configuration post-processing in {path}")
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
            fail(f"timeline command lacks placeholder in {path}: {config}")
    elif has_timeline:
        fail(f"uninstrumented command contains timeline option in {path}")


def expected_points() -> Set[Tuple[str, int]]:
    """Return the complete configuration-by-K point set."""
    return {
        (config, colors)
        for config, _label, _family, _option in CONFIGS
        for colors in COLORS
    }


def validate_rows(path: Path, revision: str) -> List[Dict[str, str]]:
    """Validate common row identity and completeness."""
    rows = read_csv(path)
    actual: Set[Tuple[str, int]] = set()
    for row in rows:
        validate_command(row, path)
        try:
            key = (row.get("config", ""), int(row.get("colors", "")))
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
        key = (row["config"], row["colors"])
        try:
            ms_ssim = float(row.get("MS-SSIM", ""))
            delta_e00 = float(row.get("Delta E00_mean", ""))
        except ValueError as exc:
            raise ValueError(f"invalid quality value in {path}: {key}") from exc
        if not math.isfinite(ms_ssim) or not 0.0 <= ms_ssim <= 1.0:
            fail(f"invalid MS-SSIM in {path}: {key}")
        if not math.isfinite(delta_e00) or delta_e00 < 0.0:
            fail(f"invalid Delta E00 in {path}: {key}")


def validate_size(path: Path, revision: str) -> None:
    """Validate exact stream lengths and compatibility ratios."""
    for row in validate_rows(path, revision):
        key = (row["config"], row["colors"])
        try:
            encoded_bytes = int(row.get("encoded_bytes", ""))
            ratio = float(row.get("bytes_vs_compat", ""))
        except ValueError as exc:
            raise ValueError(f"invalid size value in {path}: {key}") from exc
        if encoded_bytes <= 0 or not math.isfinite(ratio) or ratio <= 0.0:
            fail(f"invalid size value in {path}: {key}")
        if row["config"] == "heckbert-compat" and not math.isclose(
                ratio, 1.0, rel_tol=1e-12):
            fail(f"compatibility size baseline is not 1.0 in {path}: {key}")


def validate_speed(path: Path, revision: str, expected_runs: int) -> None:
    """Validate both speed domains and their intervals."""
    for row in validate_rows(path, revision):
        validate_command(row, path, True)
        key = (row["config"], row["colors"])
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
            palette_speedup = float(row["palette_speedup_vs_compat"])
            end_speedup = float(row["end_to_end_speedup_vs_compat"])
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
        if row["config"] == "heckbert-compat":
            if not math.isclose(palette_speedup, 1.0, rel_tol=1e-12):
                fail(f"palette compatibility baseline is not 1.0: {key}")
            if not math.isclose(end_speedup, 1.0, rel_tol=1e-12):
                fail(f"end-to-end compatibility baseline is not 1.0: {key}")


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
    """Validate one generated quantize-model artifact directory."""
    args = parse_args()
    directory = args.directory.resolve()
    metadata = read_metadata(directory / "quantize-model-run.json")
    revision = str(metadata["source"]["revision"])
    runs = int(metadata["protocol"]["speed_runs"])
    validate_quality(directory / "quantize-model-quality.csv", revision)
    validate_size(directory / "quantize-model-size.csv", revision)
    validate_speed(
        directory / "quantize-model-speed.csv",
        revision,
        runs,
    )
    validate_plots(directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
