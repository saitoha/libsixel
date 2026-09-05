#!/usr/bin/env python3
"""Validate palette-pipeline measurement completeness and provenance."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Set, Tuple


COLORS = (8, 16, 32, 64, 128, 256)
DEFAULT_LOADER_ORDER = "libpng!"
CMS_LOADER_ORDER = "libpng:cms_engine=builtin!"
CMS_REFERENCE_LOADER_ORDER = "libpng,builtin!"
CMS_REFERENCE_LOADER_ENVIRONMENT = {
    "SIXEL_LOADER_LIBPNG_CMS_ENGINE": "builtin",
}
SAMPLE_TARGET = 16384
QUANTIZE_OPTION = (
    "kmeans:inittype=none:threshold=0.125:binbits=6:mapping=uniform:"
    "softdist=trilinear:feedback=0:prune=hamerly:seed=1:"
    "restarts=1:iter=20:miniter=0:polish_iter=0:feedback_slots=1:"
    f"feedback_interval=1:sample_target={SAMPLE_TARGET}"
)
CONFIGS: Tuple[Tuple[str, str, str], ...] = (
    ("full-frame-hard", "full-frame", "hard"),
    ("adaptive-grid-hard", "adaptive-grid", "hard"),
    ("full-frame-none", "full-frame", "none"),
    ("full-frame-exact", "full-frame", "exact"),
    ("full-frame-soft", "full-frame", "soft"),
)
COMPARISON_FACETS = {
    "sampling": [
        ["full-frame-hard", "full frame"],
        ["adaptive-grid-hard", "adaptive grid"],
    ],
    "binning": [
        ["full-frame-none", "none"],
        ["full-frame-exact", "exact"],
        ["full-frame-hard", "hard"],
        ["full-frame-soft", "soft"],
    ],
}
PLOTS = (
    "palette-pipeline-quality.png",
    "palette-pipeline-speed.png",
    "palette-pipeline-size.png",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def williams_orders(names: List[str]) -> List[List[str]]:
    """Rebuild the required first-order carryover-balanced design."""
    count = len(names)
    first = [0]
    for step in range(1, count):
        if step % 2:
            first.append((step + 1) // 2)
        else:
            first.append(count - step // 2)
    orders = [
        [names[(index + shift) % count] for index in first]
        for shift in range(count)
    ]
    if count % 2:
        orders.extend([list(reversed(order)) for order in orders])
    return orders


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read a non-empty CSV file."""
    if not path.is_file():
        fail(f"missing measurement file: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"measurement file has no rows: {path}")
    return rows


def expected_config_records() -> List[Dict[str, object]]:
    """Return the exact configuration manifest expected in metadata."""
    return [
        {
            "config": config,
            "sampling_policy": sampling,
            "binning_policy": binning,
            "comparison_axes": [
                axis
                for axis, entries in COMPARISON_FACETS.items()
                if any(entry[0] == config for entry in entries)
            ],
        }
        for config, sampling, binning in CONFIGS
    ]


def read_metadata(path: Path, expected_mode: str) -> Dict[str, object]:
    """Read and validate the measurement manifest."""
    if not path.is_file():
        fail(f"missing measurement metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        fail(f"unsupported measurement metadata schema: {path}")
    source = payload.get("source")
    build = payload.get("build")
    host = payload.get("host")
    protocol = payload.get("protocol")
    programs = payload.get("programs")
    libsixel_linkage = payload.get("libsixel_linkage")
    input_record = payload.get("input")
    if not all(
        isinstance(value, dict)
        for value in (
            source,
            build,
            host,
            protocol,
            programs,
            input_record,
            libsixel_linkage,
        )
    ):
        fail(f"incomplete measurement metadata: {path}")
    source_state = source.get("tracked_worktree_state_at_start")
    if source_state not in ("clean", "dirty"):
        fail("measurement source state is missing")
    if expected_mode == "durable" and source_state != "clean":
        fail("durable measurements must originate from a clean worktree")
    source_diff_hash = source.get("tracked_diff_sha256")
    if not isinstance(source_diff_hash, str) or len(source_diff_hash) != 64:
        fail("measurement tracked-diff identity is missing")
    if (
        source_state == "clean"
        and source_diff_hash != hashlib.sha256(b"").hexdigest()
    ):
        fail("clean measurement has a non-empty tracked-diff identity")
    revision = source.get("revision")
    if not isinstance(revision, str) or not revision or revision == "unknown":
        fail("measurement source revision is missing")
    expected_protocol = {
        "measurement_mode": expected_mode,
        "comparison_scope": (
            "independent palette sampling and binning policy axes"
        ),
        "comparison_facets": COMPARISON_FACETS,
        "threads": 1,
        "precision": "8bit",
        "quality": "full",
        "quantize_option": QUANTIZE_OPTION,
        "sample_target": SAMPLE_TARGET,
        "clustering_colorspace": "oklab",
        "working_colorspace": "gamma",
        "diffusion": "none",
        "lookup_policy": "none",
        "gpu_policy": "off",
        "merge_policy": "none",
        "cover_policy": "off",
        "timing_order_design": {
            "name": "Williams first-order carryover balance",
            "physical_configurations": 5,
            "sequence_period": 10,
            "boundary_washout": (
                "one recorded but excluded full-frame-hard process "
                "before every sequence"
            ),
        },
        "sixel_environment_removed": True,
        "provenance_rechecked_after_measurement": True,
        "libsixel_linkage_snapshotted": True,
        "size_measurement": "quality-pass SIXEL stdout byte length",
        "end_to_end_timing": (
            "uninstrumented fresh-process monotonic wall time"
        ),
        "speed_rows": (
            "one raw duration per domain, K, round, position, and "
            "recorded boundary washout"
        ),
    }
    for name, expected in expected_protocol.items():
        if protocol.get(name) != expected:
            fail(f"measurement metadata has unexpected {name}")
    loader_order = protocol.get("loader")
    if loader_order not in (DEFAULT_LOADER_ORDER, CMS_LOADER_ORDER):
        fail("measurement metadata has unexpected loader order")
    if "quality_reference_loader" in protocol:
        if protocol.get("lsqa_environment_removed") is not True:
            fail("measurement protocol retains inherited LSQA environment")
        reference_loader = protocol.get("quality_reference_loader")
        reference_environment = protocol.get(
            "quality_reference_loader_environment"
        )
        expected_reference_loader = (
            "automatic"
            if loader_order == DEFAULT_LOADER_ORDER
            else CMS_REFERENCE_LOADER_ORDER
        )
        expected_reference_environment = (
            {}
            if loader_order == DEFAULT_LOADER_ORDER
            else CMS_REFERENCE_LOADER_ENVIRONMENT
        )
        if reference_loader != expected_reference_loader:
            fail("measurement metadata has unexpected reference loader")
        if reference_environment != expected_reference_environment:
            fail("measurement metadata has unexpected reference loader environment")
    elif loader_order != DEFAULT_LOADER_ORDER:
        fail("CMS measurement metadata lacks reference loader controls")
    elif (
        "lsqa_environment_removed" in protocol
        and protocol["lsqa_environment_removed"] is not True
    ):
        # Older committed measurements predate explicit LSQA environment
        # isolation.  Only that legacy default-loader form may omit the flag.
        fail("measurement protocol retains inherited LSQA environment")
    if tuple(protocol.get("colors", [])) != COLORS:
        fail("measurement metadata has unexpected palette sizes")
    if protocol.get("configurations") != expected_config_records():
        fail("measurement metadata has unexpected configurations")
    warmups = int(protocol.get("speed_warmups", -1))
    runs = int(protocol.get("speed_runs", 0))
    if warmups < 0:
        fail("measurement metadata has invalid warm-up count")
    if runs < 1:
        fail("measurement metadata has invalid measured-run count")
    if expected_mode == "durable" and (warmups, runs) != (2, 10):
        fail("durable measurements require 2 warm-ups and 10 runs")
    palette_timing = protocol.get("palette_timing")
    if not isinstance(palette_timing, str) or "top-level" not in palette_timing:
        fail("measurement metadata has unexpected palette timing definition")
    limit = protocol.get("interpretation_limit")
    if not isinstance(limit, str) or "cannot select" not in limit:
        fail("measurement metadata lacks the single-fixture limitation")
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
    input_path = input_record.get("path")
    platform_name = host.get("platform")
    if not isinstance(input_path, str) or not input_path:
        fail("measurement input path is missing")
    if not isinstance(platform_name, str) or not platform_name:
        fail("measurement host platform is missing")
    source_directory = source.get("top_source_directory")
    build_source_directory = build.get("configured_source_directory")
    if (
        not isinstance(source_directory, str)
        or not source_directory
        or build_source_directory != source_directory
        or build.get("source_directory_matches") is not True
    ):
        fail("measurement build source does not match the recorded source tree")
    if source.get("end_snapshot_matches") is not True:
        fail("measurement source was not rechecked after measurement")
    build_directory_value = build.get("directory")
    if not isinstance(build_directory_value, str) or not build_directory_value:
        fail("measurement build directory is missing")
    build_directory = Path(build_directory_value)
    if not build_directory.is_absolute():
        build_directory = Path(source_directory) / build_directory
    build_directory = build_directory.resolve()
    for name in ("img2sixel", "lsqa"):
        record = programs[name]
        for field in ("launcher", "payload"):
            value = record.get(field)
            if not isinstance(value, str) or not value:
                fail(f"invalid {name} {field}")
            program_path = Path(value)
            if not program_path.is_absolute():
                program_path = Path(source_directory) / program_path
            try:
                program_path.resolve().relative_to(build_directory)
            except ValueError as exc:
                raise ValueError(
                    f"{name} {field} is outside the recorded build directory"
                ) from exc
    link_mode = libsixel_linkage.get("mode")
    link_artifacts = libsixel_linkage.get("artifacts")
    if link_mode not in ("amalgamated", "library"):
        fail("invalid libsixel linkage mode")
    if not isinstance(link_artifacts, list):
        fail("invalid libsixel linkage artifact list")
    if link_mode == "amalgamated" and link_artifacts:
        fail("amalgamated tools must not name a separate libsixel artifact")
    if link_mode == "library" and len(link_artifacts) != 1:
        fail("library-linked tools require one resolved libsixel artifact")
    for record in link_artifacts:
        if not isinstance(record, dict):
            fail("invalid libsixel linkage artifact record")
        if record.get("role") not in ("shared", "static"):
            fail("invalid libsixel linkage artifact role")
        value = record.get("path")
        digest = record.get("sha256")
        if not isinstance(value, str) or not value:
            fail("invalid libsixel linkage artifact path")
        if not isinstance(digest, str) or len(digest) != 64:
            fail("invalid libsixel linkage artifact hash")
        library_path = Path(value)
        if not library_path.is_absolute():
            library_path = Path(source_directory) / library_path
        try:
            library_path.resolve().relative_to(build_directory)
        except ValueError as exc:
            raise ValueError(
                "libsixel artifact is outside the recorded build directory"
            ) from exc
    return payload


def config_map() -> Dict[str, Tuple[str, str]]:
    """Map configuration ids to their controlled policy fields."""
    return {
        config: (sampling, binning)
        for config, sampling, binning in CONFIGS
    }


def validate_command(row: Dict[str, str],
                     path: Path,
                     timeline: bool = False,
                     discard_output: bool = False,
                     loader_order: str = DEFAULT_LOADER_ORDER) -> None:
    """Check one recorded command and its row identity."""
    config = row.get("config", "")
    expected = config_map()
    if config not in expected:
        fail(f"unexpected configuration in {path}: {config}")
    sampling, binning = expected[config]
    if row.get("sampling_policy") != sampling:
        fail(f"sampling policy differs in {path}: {config}")
    if row.get("binning_policy") != binning:
        fail(f"binning policy differs in {path}: {config}")
    if row.get("quantize_option") != QUANTIZE_OPTION:
        fail(f"quantize option differs in {path}: {config}")
    try:
        colors = int(row.get("colors", ""))
        tokens = shlex.split(row.get("command", ""))
    except ValueError as exc:
        raise ValueError(f"invalid command in {path}: {config}") from exc
    expected_tokens = [
        "{img2sixel}",
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        f"--loaders={loader_order}",
        f"--sampling-policy={sampling}",
        f"--binning-policy={binning}",
        f"--quantize-model={QUANTIZE_OPTION}",
        "-Xoklab",
        "-Wgamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "-F",
        "none",
        "-a",
        "off",
        "-p",
        str(colors),
    ]
    if timeline:
        expected_tokens.extend(("-J", "{timeline}"))
    if discard_output:
        expected_tokens.extend(("-o", "{devnull}"))
    expected_tokens.append("{input}")
    if tokens != expected_tokens:
        fail(f"command is not canonical in {path}: {config}")


def validate_assessment_command(row: Dict[str, str],
                                path: Path,
                                loader_order: str,
                                required: bool) -> None:
    """Check the recorded lsqa command, including reference CMS controls."""
    command = row.get("assessment_command", "")
    if not command and not required:
        return
    try:
        tokens = shlex.split(command)
    except ValueError as exc:
        raise ValueError(f"invalid assessment command in {path}") from exc
    expected_tokens = ["{lsqa}"]
    if loader_order == CMS_LOADER_ORDER:
        expected_tokens.extend(
            (
                f"--loaders={CMS_REFERENCE_LOADER_ORDER}",
                "--env",
                "SIXEL_LOADER_LIBPNG_CMS_ENGINE=builtin",
            )
        )
    expected_tokens.extend(("{input}", "-"))
    if tokens != expected_tokens:
        fail(f"assessment command is not canonical in {path}")


def expected_points() -> Set[Tuple[str, int]]:
    """Return the complete configuration-by-K point set."""
    return {
        (config, colors)
        for config, _sampling, _binning in CONFIGS
        for colors in COLORS
    }


def validate_rows(path: Path,
                  revision: str,
                  input_path: str,
                  platform_name: str,
                  loader_order: str) -> List[Dict[str, str]]:
    """Validate common row identity and completeness."""
    rows = read_csv(path)
    actual: Set[Tuple[str, int]] = set()
    for row in rows:
        validate_command(row, path, loader_order=loader_order)
        try:
            key = (row.get("config", ""), int(row.get("colors", "")))
        except ValueError as exc:
            raise ValueError(f"invalid palette size in {path}") from exc
        if key in actual:
            fail(f"duplicate measurement point in {path}: {key}")
        actual.add(key)
        if row.get("revision") != revision:
            fail(f"row revision differs from metadata in {path}: {key}")
        if row.get("input") != input_path:
            fail(f"row input differs from metadata in {path}: {key}")
        if row.get("platform") != platform_name:
            fail(f"row platform differs from metadata in {path}: {key}")
    expected = expected_points()
    if actual != expected:
        fail(
            f"measurement sweep mismatch in {path}: "
            f"missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )
    return rows


def validate_quality(path: Path,
                     revision: str,
                     input_path: str,
                     platform_name: str,
                     loader_order: str,
                     require_assessment_command: bool) -> None:
    """Validate all quality values."""
    rows = validate_rows(
        path, revision, input_path, platform_name, loader_order
    )
    for row in rows:
        validate_assessment_command(
            row, path, loader_order, require_assessment_command
        )
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


def validate_size(path: Path,
                  revision: str,
                  input_path: str,
                  platform_name: str,
                  loader_order: str) -> None:
    """Validate raw stream sizes."""
    rows = validate_rows(
        path, revision, input_path, platform_name, loader_order
    )
    for row in rows:
        key = (row["config"], row["colors"])
        try:
            encoded_bytes = int(row.get("encoded_bytes", ""))
        except ValueError as exc:
            raise ValueError(f"invalid size value in {path}: {key}") from exc
        if encoded_bytes <= 0:
            fail(f"invalid size value in {path}: {key}")


def validate_speed(path: Path,
                   revision: str,
                   input_path: str,
                   platform_name: str,
                   warmups: int,
                   runs: int,
                   loader_order: str) -> None:
    """Validate every raw timing row and its exact executed schedule."""
    rows = read_csv(path)
    names = [config for config, _sampling, _binning in CONFIGS]
    orders = williams_orders(names)
    expected_schedule: List[Tuple[object, ...]] = []
    actual_schedule: List[Tuple[object, ...]] = []
    for colors in COLORS:
        for domain in ("end-to-end", "palette-build"):
            for round_index in range(warmups + runs):
                sequence = round_index % len(orders)
                phase = "warmup" if round_index < warmups else "measured"
                expected_schedule.append(
                    (
                        colors,
                        domain,
                        round_index,
                        sequence,
                        -1,
                        "washout",
                        "full-frame-hard",
                    )
                )
                expected_schedule.extend(
                    (
                        colors,
                        domain,
                        round_index,
                        sequence,
                        position,
                        phase,
                        config,
                    )
                    for position, config in enumerate(orders[sequence])
                )
    for row in rows:
        domain = row.get("domain", "")
        validate_command(
            row,
            path,
            domain == "palette-build",
            True,
            loader_order,
        )
        key = (row.get("config", ""), row.get("colors", ""))
        try:
            identity = (
                int(row.get("colors", "")),
                domain,
                int(row.get("round", "")),
                int(row.get("sequence", "")),
                int(row.get("position", "")),
                row.get("phase", ""),
                row.get("config", ""),
            )
            duration = float(row.get("seconds", ""))
        except ValueError as exc:
            raise ValueError(f"invalid speed value in {path}: {key}") from exc
        if domain not in ("end-to-end", "palette-build"):
            fail(f"invalid timing domain in {path}: {key}")
        if not math.isfinite(duration) or duration <= 0.0:
            fail(f"invalid speed value in {path}: {key}")
        if row.get("revision") != revision:
            fail(f"row revision differs from metadata in {path}: {key}")
        if row.get("input") != input_path:
            fail(f"row input differs from metadata in {path}: {key}")
        if row.get("platform") != platform_name:
            fail(f"row platform differs from metadata in {path}: {key}")
        actual_schedule.append(identity)
    if actual_schedule != expected_schedule:
        mismatch = next(
            (
                index
                for index, (actual, expected) in enumerate(
                    zip(actual_schedule, expected_schedule)
                )
                if actual != expected
            ),
            min(len(actual_schedule), len(expected_schedule)),
        )
        actual = (
            actual_schedule[mismatch]
            if mismatch < len(actual_schedule)
            else "missing"
        )
        expected = (
            expected_schedule[mismatch]
            if mismatch < len(expected_schedule)
            else "none"
        )
        fail(
            f"raw timing schedule mismatch in {path} at row {mismatch + 2}: "
            f"actual={actual}, expected={expected}"
        )


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
    parser.add_argument(
        "--mode",
        choices=("durable", "exploratory"),
        default="durable",
    )
    return parser.parse_args()


def main() -> int:
    """Validate every durable palette-pipeline artifact."""
    args = parse_args()
    directory = args.directory.resolve()
    metadata = read_metadata(
        directory / "palette-pipeline-run.json",
        args.mode,
    )
    source = metadata["source"]
    host = metadata["host"]
    input_record = metadata["input"]
    protocol = metadata["protocol"]
    revision = str(source["revision"])
    input_path = str(input_record["path"])
    platform_name = str(host["platform"])
    warmups = int(protocol["speed_warmups"])
    runs = int(protocol["speed_runs"])
    loader_order = str(protocol["loader"])
    require_assessment_command = "quality_reference_loader" in protocol
    validate_quality(
        directory / "palette-pipeline-quality.csv",
        revision,
        input_path,
        platform_name,
        loader_order,
        require_assessment_command,
    )
    validate_size(
        directory / "palette-pipeline-size.csv",
        revision,
        input_path,
        platform_name,
        loader_order,
    )
    validate_speed(
        directory / "palette-pipeline-speed.csv",
        revision,
        input_path,
        platform_name,
        warmups,
        runs,
        loader_order,
    )
    validate_plots(directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
