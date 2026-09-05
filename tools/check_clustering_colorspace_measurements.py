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
BINBITS = (4, 5, 6, 7, 8)
OCCUPANCY_COLORS = 64
COLORSPACES: Tuple[Tuple[str, str, int], ...] = (
    ("gamma", "gamma sRGB", 0),
    ("linear", "linear RGB", 1),
    ("oklab", "OKLab", 2),
    ("cielab", "CIELAB", 4),
    ("din99d", "DIN99d", 5),
)
PLOTS = (
    "clustering-colorspace-quality.png",
    "clustering-colorspace-binning-quality.png",
    "clustering-colorspace-binning-occupancy.png",
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
    if not isinstance(payload, dict) or payload.get("schema_version") != 2:
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
        "quantize_model": "kmeans:seed=1:binbits=6",
        "merge_policy": "none",
        "cover_policy": "off",
        "working_colorspace": "gamma",
        "diffusion": "none",
        "lookup_policy": "none",
        "gpu_policy": "off",
        "palette_type": "rgb",
        "binning_sensitivity_policies": ["hard", "none"],
        "binning_sensitivity_hard_binbits": list(BINBITS),
        "occupancy_binbits": list(BINBITS),
        "occupancy_palette_size": OCCUPANCY_COLORS,
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
                     timeline: bool = False,
                     binning_policy: str = "hard",
                     binbits: int = 6) -> None:
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
        f"--binning-policy={binning_policy}",
        f"--quantize-model=kmeans:seed=1:binbits={binbits}",
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


def validate_binning_quality(path: Path, revision: str) -> None:
    """Validate paired hard-grid and unaggregated quality observations."""
    rows = read_csv(path)
    actual: Set[Tuple[str, int, str, int]] = set()
    expected = {
        (colorspace, colors, policy, binbits)
        for colorspace, _label, _contract_value in COLORSPACES
        for colors in COLORS
        for policy, binbits in (
            *(("hard", value) for value in BINBITS),
            ("none", 6),
        )
    }
    for row in rows:
        policy = row.get("binning_policy", "")
        try:
            key = (
                row.get("colorspace", ""),
                int(row.get("colors", "")),
                policy,
                int(row.get("binbits", "")),
            )
        except ValueError as exc:
            raise ValueError(
                f"invalid binning quality identity in {path}"
            ) from exc
        if key in actual or policy not in ("hard", "none"):
            fail(f"invalid binning quality point in {path}: {key}")
        actual.add(key)
        if row.get("revision") != revision:
            fail(f"binning quality revision differs in {path}: {key}")
        validate_command(
            row,
            path,
            binning_policy=policy,
            binbits=key[3],
        )
        try:
            ms_ssim = float(row.get("MS-SSIM", ""))
            delta_e00 = float(row.get("Delta E00_mean", ""))
            delta_chroma = float(row.get("Delta Chroma_mean", ""))
        except ValueError as exc:
            raise ValueError(
                f"invalid binning quality value in {path}: {key}"
            ) from exc
        if not math.isfinite(ms_ssim) or not 0.0 <= ms_ssim <= 1.0:
            fail(f"invalid binning MS-SSIM in {path}: {key}")
        if not all(
                math.isfinite(value) and value >= 0.0
                for value in (delta_e00, delta_chroma)):
            fail(f"invalid binning color error in {path}: {key}")
    if actual != expected:
        fail(
            f"binning quality sweep mismatch in {path}: "
            f"missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )


def validate_binning_occupancy(path: Path, revision: str) -> None:
    """Validate effective hard-grid populations and compression ratios."""
    rows = read_csv(path)
    actual: Set[Tuple[str, int]] = set()
    expected = {
        (colorspace, binbits)
        for colorspace, _label, _contract_value in COLORSPACES
        for binbits in BINBITS
    }
    source_populations: Set[int] = set()
    for row in rows:
        try:
            binbits = int(row.get("binbits", ""))
            key = (row.get("colorspace", ""), binbits)
            source_points = int(row.get("source_points", ""))
            effective_points = int(row.get("effective_points", ""))
            ratio = float(row.get("compression_ratio", ""))
        except ValueError as exc:
            raise ValueError(
                f"invalid binning occupancy value in {path}"
            ) from exc
        if key in actual:
            fail(f"duplicate binning occupancy point in {path}: {key}")
        actual.add(key)
        source_populations.add(source_points)
        if row.get("revision") != revision:
            fail(f"binning occupancy revision differs in {path}: {key}")
        if int(row.get("colors", "0")) != OCCUPANCY_COLORS:
            fail(f"unexpected occupancy palette size in {path}: {key}")
        validate_command(row, path, binbits=binbits)
        if source_points < 1 or not 1 <= effective_points <= source_points:
            fail(f"invalid binning occupancy in {path}: {key}")
        if not math.isclose(
                ratio,
                source_points / effective_points,
                rel_tol=1e-12):
            fail(f"invalid binning compression ratio in {path}: {key}")
    if actual != expected:
        fail(
            f"binning occupancy sweep mismatch in {path}: "
            f"missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )
    if len(source_populations) != 1:
        fail("binning occupancy source population differs across the sweep")


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
    validate_binning_quality(
        directory / "clustering-colorspace-binning-quality.csv",
        revision,
    )
    validate_binning_occupancy(
        directory / "clustering-colorspace-binning-occupancy.csv",
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
