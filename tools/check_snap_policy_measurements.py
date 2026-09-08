#!/usr/bin/env python3
"""Validate snap-policy measurement completeness, controls, and provenance."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Sequence, Set, Tuple


COLORS = (8, 16, 32, 64, 128, 256)
CONTROL_COLORS = 64
COLORSPACES = ("gamma", "linear", "oklab", "cielab", "din99d")
WORK_FORMATS = (
    "rgb-f32",
    "linear-f32",
    "oklab-f32",
    "cielab-f32",
    "din99d-f32",
)
CONFIGS = (
    "off",
    "nearest-quarter",
    "nearest-once",
    "nearest-all",
    "reversible-all",
)
RATES = (0.0, 0.25, 0.5, 0.75, 1.0)
TIMINGS = ("once", "polish", "merge", "resolve", "all")
CHANNEL_FACTORS = (0.0, 0.25, 0.5, 0.75, 1.0)
LAB_COLORSPACES = ("oklab", "cielab", "din99d")
PLOTS = (
    "snap-policy-quality.png",
    "snap-policy-fixed-point.png",
    "snap-policy-size.png",
    "snap-policy-speed.png",
    "snap-policy-controls.png",
    "snap-policy-fixture-summary.png",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read one non-empty measurement table."""
    if not path.is_file():
        fail(f"missing measurement file: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"measurement file has no rows: {path}")
    return rows


def read_metadata(path: Path, mode: str) -> Dict[str, object]:
    """Read and validate the stable metadata envelope."""
    if not path.is_file():
        fail(f"missing measurement metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        metadata = json.load(handle)
    if not isinstance(metadata, dict) or metadata.get("schema_version") != 1:
        fail("unsupported snap measurement metadata schema")
    if metadata.get("measurement_mode") != mode:
        fail("snap measurement mode differs from checker mode")
    source = metadata.get("source")
    programs = metadata.get("programs")
    fixtures = metadata.get("fixtures")
    protocol = metadata.get("protocol")
    artifacts = metadata.get("artifacts")
    if not all(
            isinstance(value, dict)
            for value in (source, programs, protocol, artifacts)):
        fail("snap measurement metadata is incomplete")
    if not isinstance(fixtures, list) or len(fixtures) != 5:
        fail("snap measurement fixture manifest is incomplete")
    if mode == "durable" and source.get(
            "tracked_worktree_state_at_start") != "clean":
        fail("durable snap measurements must originate from a clean tree")
    revision = source.get("revision")
    if not isinstance(revision, str) or not revision or revision == "unknown":
        fail("snap measurement source revision is missing")
    manifest_hash = source.get("manifest_sha256")
    if not isinstance(manifest_hash, str) or len(manifest_hash) != 64:
        fail("snap fixture manifest hash is invalid")
    for name in ("img2sixel", "lsqa"):
        record = programs.get(name)
        if not isinstance(record, dict):
            fail(f"missing {name} provenance")
        for field in ("launcher_sha256", "payload_sha256"):
            value = record.get(field)
            if not isinstance(value, str) or len(value) != 64:
                fail(f"invalid {name} {field}")
    for fixture in fixtures:
        if not isinstance(fixture, dict):
            fail("snap fixture provenance record is invalid")
        digest = fixture.get("sha256")
        if not isinstance(digest, str) or len(digest) != 64:
            fail("snap fixture SHA-256 is invalid")
    expected_protocol = {
        "primary_fixture": "natural-snake",
        "colors": list(COLORS),
        "control_colors": CONTROL_COLORS,
        "threads": 1,
        "precision": "float32",
        "quality": "full",
        "loader": "builtin!",
        "sampling_policy": "full-frame",
        "binning_policy": "hard",
        "quantize_model": "kmeans:seed=1:binbits=6",
        "main_merge_policy": "none",
        "cover_policy": "off",
        "diffusion": "fs:scan=raster",
        "lookup_policy": "none",
        "gpu_policy": "off",
        "palette_type": "rgb",
        "matched_working_and_clustering_spaces": True,
        "rate_sweep": list(RATES),
        "timing_sweep": list(TIMINGS),
        "timing_merge_policy": "ward",
        "channel_l_sweep": list(CHANNEL_FACTORS),
        "channel_l_colorspaces": list(LAB_COLORSPACES),
        "speed_order_rotated_each_round": True,
        "sixel_environment_removed": True,
    }
    for name, expected in expected_protocol.items():
        if protocol.get(name) != expected:
            fail(f"snap protocol has unexpected {name}")
    spaces = protocol.get("colorspaces")
    if not isinstance(spaces, list) or [
            record.get("name") for record in spaces
            if isinstance(record, dict)] != list(COLORSPACES):
        fail("snap protocol colorspace order is stale")
    if [
            record.get("working_format") for record in spaces
            if isinstance(record, dict)] != list(WORK_FORMATS):
        fail("snap protocol working formats are stale")
    configurations = protocol.get("configurations")
    if not isinstance(configurations, list) or [
            record.get("config") for record in configurations
            if isinstance(record, dict)] != list(CONFIGS):
        fail("snap protocol configuration order is stale")
    warmups = int(protocol.get("speed_warmups", -1))
    runs = int(protocol.get("speed_runs", 0))
    if warmups < 0 or runs < 1:
        fail("snap timing protocol is invalid")
    if mode == "durable" and (warmups != 2 or runs != 9):
        fail("durable snap timing protocol must use 2 warm-ups and 9 runs")
    if artifacts.get("plots") != list(PLOTS):
        fail("snap plot manifest is stale")
    if artifacts.get("tables") != [
            "snap-policy-quality.csv",
            "snap-policy-speed.csv",
            "snap-policy-controls.csv"]:
        fail("snap table manifest is stale")
    return metadata


def require_finite(row: Dict[str, str], fields: Sequence[str], context: str) -> None:
    """Require finite numeric values in one row."""
    for field in fields:
        try:
            value = float(row[field])
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f"invalid {field} in {context}") from exc
        if not math.isfinite(value):
            fail(f"non-finite {field} in {context}")


def require_common_values(row: Dict[str, str], context: str) -> None:
    """Validate metrics, palette statistics, and encoded size."""
    require_finite(
        row,
        (
            "MS-SSIM", "Delta E00_mean", "Delta Chroma_mean",
            "unsafe_channel_fraction", "mean_abs_roundtrip_drift",
            "max_abs_roundtrip_drift",
        ),
        context,
    )
    if not 0.0 <= float(row["MS-SSIM"]) <= 1.0:
        fail(f"MS-SSIM is outside [0,1] in {context}")
    if float(row["Delta E00_mean"]) < 0.0:
        fail(f"negative Delta E00 in {context}")
    if float(row["Delta Chroma_mean"]) < 0.0:
        fail(f"negative Delta Chroma in {context}")
    entries = int(row["palette_entries"])
    channels = int(row["palette_channels"])
    unsafe_entries = int(row["unsafe_entries"])
    unsafe_channels = int(row["unsafe_channels"])
    if not 1 <= entries <= int(row["colors"]):
        fail(f"palette entry count is invalid in {context}")
    if channels != entries * 3:
        fail(f"palette channel count is invalid in {context}")
    if not 0 <= unsafe_entries <= entries:
        fail(f"unsafe palette entry count is invalid in {context}")
    if not 0 <= unsafe_channels <= channels:
        fail(f"unsafe palette channel count is invalid in {context}")
    expected_fraction = unsafe_channels / channels
    if not math.isclose(
            float(row["unsafe_channel_fraction"]),
            expected_fraction,
            rel_tol=1e-12,
            abs_tol=1e-12):
        fail(f"unsafe channel fraction is inconsistent in {context}")
    if float(row["max_abs_roundtrip_drift"]) > 2.0:
        fail(f"palette round-trip drift is unexpectedly large in {context}")
    if int(row["encoded_bytes"]) <= 0:
        fail(f"encoded stream is empty in {context}")


def require_command(
        row: Dict[str, str],
        context: str,
        speed: bool,
) -> None:
    """Validate one recorded command and matched colorspace contract."""
    try:
        tokens = shlex.split(row["command"])
    except (KeyError, ValueError) as exc:
        raise ValueError(f"invalid command in {context}") from exc
    colorspace = row["colorspace"]
    required = {
        "{img2sixel}",
        "--threads=1",
        "--precision=float32",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        "--cover-policy=off",
        f"-X{colorspace}",
        f"-W{colorspace}",
        "--diffusion=fs:scan=raster",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        str(row["colors"]),
        "{input}",
    }
    missing = sorted(required - set(tokens))
    if missing:
        fail(f"controlled command tokens missing in {context}: {missing}")
    if speed:
        if "-o" not in tokens or "{devnull}" not in tokens:
            fail(f"speed command does not discard output in {context}")
        if any(token.startswith("--mapfile-output=") for token in tokens):
            fail(f"speed command unexpectedly exports a palette in {context}")
    else:
        if "-o" in tokens:
            fail(f"quality command unexpectedly discards output in {context}")
        if "--mapfile-output=act:{palette}" not in tokens:
            fail(f"quality command does not export ACT in {context}")
        assessment = shlex.split(row.get("assessment_command", ""))
        if assessment != [
                "{lsqa}", "--loaders=builtin!", "{input}", "-"]:
            fail(f"assessment command is stale in {context}")


def require_snap_state(
        row: Dict[str, str],
        context: str,
        enabled: bool,
) -> None:
    """Validate enabled syntax and exact-rate fixed-point behavior."""
    tokens = shlex.split(row["command"])
    snap_tokens = [token for token in tokens if token.startswith("--snap-policy=")]
    if len(snap_tokens) != 1:
        fail(f"row lacks exactly one snap policy in {context}")
    if enabled and snap_tokens[0] == "--snap-policy=none":
        fail(f"enabled row selects none in {context}")
    if not enabled and snap_tokens[0] != "--snap-policy=none":
        fail(f"disabled row does not select none in {context}")
    if enabled and row["policy"] == "none":
        fail(f"enabled row records none in {context}")
    if not enabled and row["policy"] != "none":
        fail(f"disabled row does not record none in {context}")
    if (enabled
            and "unsafe_channels" in row
            and math.isclose(float(row["rate"]), 1.0)):
        if int(row["unsafe_channels"]) != 0:
            fail(f"exact rate leaves an unsafe palette channel in {context}")
        if float(row["max_abs_roundtrip_drift"]) != 0.0:
            fail(f"exact rate leaves palette drift in {context}")


def validate_main(
        path: Path,
        metadata: Dict[str, object],
) -> List[Dict[str, str]]:
    """Validate the full K-by-space-by-configuration table."""
    rows = read_csv(path)
    expected_count = len(COLORS) * len(COLORSPACES) * len(CONFIGS)
    if len(rows) != expected_count:
        fail(f"main snap table has {len(rows)} rows, expected {expected_count}")
    revision = str(metadata["source"]["revision"])
    seen: Set[Tuple[int, str, str]] = set()
    for row in rows:
        context = f"main/{row.get('colors')}/{row.get('colorspace')}/{row.get('config')}"
        colors = int(row["colors"])
        colorspace = row["colorspace"]
        config = row["config"]
        key = (colors, colorspace, config)
        if key in seen:
            fail(f"duplicate main snap row: {context}")
        seen.add(key)
        if colors not in COLORS or colorspace not in COLORSPACES:
            fail(f"unexpected main axis value in {context}")
        if config not in CONFIGS:
            fail(f"unexpected main configuration in {context}")
        if row["revision"] != revision or row["fixture_id"] != "natural-snake":
            fail(f"main provenance differs in {context}")
        require_common_values(row, context)
        require_command(row, context, False)
        require_snap_state(row, context, row["enabled"] == "True")
    if len(seen) != expected_count:
        fail("main snap table is incomplete")
    return rows


def validate_speed(
        path: Path,
        metadata: Dict[str, object],
) -> List[Dict[str, str]]:
    """Validate fresh-process timing coverage and statistics."""
    rows = read_csv(path)
    expected_count = len(COLORS) * len(COLORSPACES) * len(CONFIGS)
    if len(rows) != expected_count:
        fail(f"snap speed table has {len(rows)} rows, expected {expected_count}")
    revision = str(metadata["source"]["revision"])
    expected_runs = int(metadata["protocol"]["speed_runs"])
    seen: Set[Tuple[int, str, str]] = set()
    for row in rows:
        context = f"speed/{row.get('colors')}/{row.get('colorspace')}/{row.get('config')}"
        key = (int(row["colors"]), row["colorspace"], row["config"])
        if key in seen:
            fail(f"duplicate snap speed row: {context}")
        seen.add(key)
        if key[0] not in COLORS or key[1] not in COLORSPACES:
            fail(f"unexpected snap speed axis in {context}")
        if key[2] not in CONFIGS:
            fail(f"unexpected snap speed configuration in {context}")
        if row["revision"] != revision or row["fixture_id"] != "natural-snake":
            fail(f"snap speed provenance differs in {context}")
        require_finite(
            row,
            (
                "median_seconds", "q1_seconds", "q3_seconds",
                "min_seconds", "max_seconds", "time_ratio_vs_off",
            ),
            context,
        )
        minimum = float(row["min_seconds"])
        q1 = float(row["q1_seconds"])
        median = float(row["median_seconds"])
        q3 = float(row["q3_seconds"])
        maximum = float(row["max_seconds"])
        if not 0.0 < minimum <= q1 <= median <= q3 <= maximum:
            fail(f"snap timing statistics are inconsistent in {context}")
        if int(row["runs"]) != expected_runs:
            fail(f"snap timing run count differs in {context}")
        if float(row["time_ratio_vs_off"]) <= 0.0:
            fail(f"snap timing ratio is invalid in {context}")
        if key[2] == "off" and not math.isclose(
                float(row["time_ratio_vs_off"]), 1.0,
                rel_tol=1e-12, abs_tol=1e-12):
            fail(f"snap-off timing ratio is not one in {context}")
        require_command(row, context, True)
        require_snap_state(row, context, row["enabled"] == "True")
    if len(seen) != expected_count:
        fail("snap speed table is incomplete")
    return rows


def validate_controls(
        path: Path,
        metadata: Dict[str, object],
) -> List[Dict[str, str]]:
    """Validate rate, timing, and channel_l control coverage."""
    rows = read_csv(path)
    fixture_ids = [str(record["id"]) for record in metadata["fixtures"]]
    expected_count = (
        len(fixture_ids) * len(COLORSPACES) * len(RATES)
        + len(COLORSPACES) * (1 + len(TIMINGS))
        + len(LAB_COLORSPACES) * (1 + len(CHANNEL_FACTORS))
    )
    if len(rows) != expected_count:
        fail(
            f"snap controls table has {len(rows)} rows, expected "
            f"{expected_count}"
        )
    revision = str(metadata["source"]["revision"])
    seen: Set[Tuple[str, str, str, str]] = set()
    for row in rows:
        context = (
            f"control/{row.get('sweep')}/{row.get('fixture_id')}/"
            f"{row.get('colorspace')}/{row.get('control_value')}"
        )
        sweep = row["sweep"]
        fixture_id = row["fixture_id"]
        colorspace = row["colorspace"]
        control_value = row["control_value"]
        key = (sweep, fixture_id, colorspace, control_value)
        if key in seen:
            fail(f"duplicate snap control row: {context}")
        seen.add(key)
        if row["revision"] != revision or int(row["colors"]) != CONTROL_COLORS:
            fail(f"snap control provenance differs in {context}")
        if fixture_id not in fixture_ids or colorspace not in COLORSPACES:
            fail(f"unexpected snap control axis in {context}")
        if sweep == "rate":
            if float(control_value) not in RATES:
                fail(f"unexpected snap rate in {context}")
            if row["merge_policy"] != "none":
                fail(f"rate sweep merge policy differs in {context}")
            enabled = float(control_value) > 0.0
        elif sweep == "timing":
            if fixture_id != "natural-snake":
                fail(f"timing sweep uses a non-primary fixture in {context}")
            if control_value not in ("off", *TIMINGS):
                fail(f"unexpected snap timing in {context}")
            if row["merge_policy"] != "ward":
                fail(f"timing sweep does not enable Ward merge in {context}")
            enabled = control_value != "off"
        elif sweep == "channel_l":
            if fixture_id != "natural-snake" or colorspace not in LAB_COLORSPACES:
                fail(f"channel_l sweep axes differ in {context}")
            if control_value != "off" and float(control_value) not in CHANNEL_FACTORS:
                fail(f"unexpected channel_l value in {context}")
            if row["merge_policy"] != "none":
                fail(f"channel_l sweep merge policy differs in {context}")
            enabled = control_value != "off"
        else:
            fail(f"unexpected snap control sweep in {context}")
        require_common_values(row, context)
        require_command(row, context, False)
        require_snap_state(row, context, enabled)
    return rows


def validate_png(path: Path) -> None:
    """Require one nontrivial PNG plot."""
    if not path.is_file() or path.stat().st_size < 4096:
        fail(f"missing or unexpectedly small snap plot: {path}")
    with path.open("rb") as handle:
        if handle.read(8) != b"\x89PNG\r\n\x1a\n":
            fail(f"invalid PNG signature: {path}")


def parse_args() -> argparse.Namespace:
    """Parse checker arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory")
    parser.add_argument(
        "--mode",
        choices=("durable", "exploratory"),
        default="durable",
    )
    return parser.parse_args()


def main() -> int:
    """Validate all snap-policy measurement artifacts."""
    args = parse_args()
    directory = Path(args.directory).resolve()
    metadata = read_metadata(directory / "snap-policy-run.json", args.mode)
    main_rows = validate_main(directory / "snap-policy-quality.csv", metadata)
    speed_rows = validate_speed(directory / "snap-policy-speed.csv", metadata)
    control_rows = validate_controls(
        directory / "snap-policy-controls.csv",
        metadata,
    )
    artifacts = metadata["artifacts"]
    if int(artifacts["main_row_count"]) != len(main_rows):
        fail("snap metadata main row count is stale")
    if int(artifacts["speed_row_count"]) != len(speed_rows):
        fail("snap metadata speed row count is stale")
    if int(artifacts["control_row_count"]) != len(control_rows):
        fail("snap metadata control row count is stale")
    for name in PLOTS:
        validate_png(directory / name)
    print(
        f"validated {len(main_rows)} main, {len(speed_rows)} speed, "
        f"and {len(control_rows)} control rows"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
