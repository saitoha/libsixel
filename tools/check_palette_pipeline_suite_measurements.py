#!/usr/bin/env python3
"""Validate multi-fixture palette-pipeline measurements and provenance."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

from check_palette_pipeline_measurements import (
    CMS_LOADER_ORDER,
    CMS_REFERENCE_LOADER_ENVIRONMENT,
    CMS_REFERENCE_LOADER_ORDER,
    COLORS,
    COMPARISON_FACETS,
    CONFIGS,
    QUANTIZE_OPTION,
    expected_config_records,
    validate_assessment_command,
    validate_command,
    williams_orders,
)


CONTENT_CLASSES = {
    "natural",
    "flat-artwork",
    "gradient",
    "rare-color",
    "broad-gamut",
}
SCALE_COLORS = (64, 256)
PLOTS = (
    "palette-pipeline-suite-quality.png",
    "palette-pipeline-suite-size.png",
    "palette-pipeline-suite-speed-content.png",
    "palette-pipeline-suite-speed-scale.png",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read one non-empty suite CSV."""
    if not path.is_file():
        fail(f"missing suite measurement file: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"suite measurement file has no rows: {path}")
    return rows


def validate_programs(programs: object) -> None:
    """Validate recorded launcher and payload identities."""
    if not isinstance(programs, dict):
        fail("suite metadata lacks program provenance")
    for name in ("img2sixel", "lsqa"):
        record = programs.get(name)
        if not isinstance(record, dict):
            fail(f"suite metadata lacks {name} provenance")
        for field in ("launcher", "payload"):
            if not isinstance(record.get(field), str) or not record[field]:
                fail(f"suite metadata has invalid {name} {field}")
        for field in ("launcher_sha256", "payload_sha256"):
            value = record.get(field)
            if not isinstance(value, str) or len(value) != 64:
                fail(f"suite metadata has invalid {name} {field}")


def validate_linkage(linkage: object) -> None:
    """Validate amalgamated or local-library linkage provenance."""
    if not isinstance(linkage, dict):
        fail("suite metadata lacks libsixel linkage")
    mode = linkage.get("mode")
    artifacts = linkage.get("artifacts")
    if not isinstance(artifacts, list):
        fail("suite metadata has invalid libsixel linkage artifacts")
    if mode == "amalgamated":
        if artifacts:
            fail("amalgamated suite metadata has separate library artifacts")
        return
    if mode != "library" or len(artifacts) != 1:
        fail("suite metadata has unsupported libsixel linkage")
    artifact = artifacts[0]
    if not isinstance(artifact, dict):
        fail("suite metadata has invalid library artifact")
    if artifact.get("role") not in ("shared", "static"):
        fail("suite metadata has invalid library artifact role")
    if not isinstance(artifact.get("path"), str) or not artifact["path"]:
        fail("suite metadata has invalid library artifact path")
    digest = artifact.get("sha256")
    if not isinstance(digest, str) or len(digest) != 64:
        fail("suite metadata has invalid library artifact hash")


def validate_single_protocol(protocol: object,
                             mode: str,
                             warmups: int,
                             runs: int) -> None:
    """Validate the single-fixture protocol nested in suite metadata."""
    if not isinstance(protocol, dict):
        fail("suite metadata lacks the single-fixture protocol")
    expected = {
        "measurement_mode": mode,
        "comparison_scope": (
            "independent palette sampling and binning policy axes"
        ),
        "colors": list(COLORS),
        "configurations": expected_config_records(),
        "comparison_facets": COMPARISON_FACETS,
        "threads": 1,
        "precision": "8bit",
        "quality": "full",
        "loader": CMS_LOADER_ORDER,
        "quality_reference_loader": CMS_REFERENCE_LOADER_ORDER,
        "quality_reference_loader_environment": (
            CMS_REFERENCE_LOADER_ENVIRONMENT
        ),
        "quantize_option": QUANTIZE_OPTION,
        "diffusion": "none",
        "lookup_policy": "none",
        "gpu_policy": "off",
        "merge_policy": "none",
        "cover_policy": "off",
        "speed_warmups": warmups,
        "speed_runs": runs,
        "sixel_environment_removed": True,
        "lsqa_environment_removed": True,
        "provenance_rechecked_after_measurement": True,
    }
    for field, value in expected.items():
        if protocol.get(field) != value:
            fail(f"suite single-fixture protocol differs in {field}")


def validate_fixtures(records: object) -> List[Dict[str, object]]:
    """Validate fixture identities, coverage, and scale ordering inputs."""
    if not isinstance(records, list) or not records:
        fail("suite metadata has no fixtures")
    fixtures = []
    identifiers = set()
    for record in records:
        if not isinstance(record, dict):
            fail("suite metadata has an invalid fixture record")
        identifier = record.get("id")
        label = record.get("label")
        fixture_class = record.get("class")
        input_path = record.get("path")
        roles = record.get("roles")
        digest = record.get("sha256")
        try:
            width = int(record.get("width", 0))
            height = int(record.get("height", 0))
            pixels = int(record.get("pixels", 0))
        except (TypeError, ValueError) as exc:
            raise ValueError("suite metadata has invalid fixture dimensions") from exc
        if not isinstance(identifier, str) or not identifier:
            fail("suite metadata has an invalid fixture id")
        if identifier in identifiers:
            fail(f"suite metadata has duplicate fixture id: {identifier}")
        if not isinstance(label, str) or not label:
            fail(f"suite fixture lacks a label: {identifier}")
        if not isinstance(fixture_class, str) or not fixture_class:
            fail(f"suite fixture lacks a class: {identifier}")
        if not isinstance(input_path, str) or not input_path:
            fail(f"suite fixture lacks a path: {identifier}")
        if (
            not isinstance(roles, list)
            or not roles
            or len(roles) != len(set(roles))
            or any(role not in ("content", "scale") for role in roles)
        ):
            fail(f"suite fixture has invalid roles: {identifier}")
        if width <= 0 or height <= 0 or pixels != width * height:
            fail(f"suite fixture has invalid dimensions: {identifier}")
        if not isinstance(digest, str) or len(digest) != 64:
            fail(f"suite fixture has invalid hash: {identifier}")
        identifiers.add(identifier)
        fixtures.append(record)
    content = [fixture for fixture in fixtures if "content" in fixture["roles"]]
    scale = [fixture for fixture in fixtures if "scale" in fixture["roles"]]
    if {fixture["class"] for fixture in content} != CONTENT_CLASSES:
        fail("suite content fixtures do not cover the required classes")
    if len(scale) < 3 or len({fixture["pixels"] for fixture in scale}) != len(scale):
        fail("suite scale fixtures do not have distinct pixel counts")
    return fixtures


def read_metadata(path: Path, mode: str) \
        -> Tuple[Dict[str, object], List[Dict[str, object]]]:
    """Read and validate suite metadata and return its fixture records."""
    if not path.is_file():
        fail(f"missing suite metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        fail(f"unsupported suite metadata schema: {path}")
    source = payload.get("source")
    build = payload.get("build")
    host = payload.get("host")
    manifest = payload.get("manifest")
    protocol = payload.get("protocol")
    artifacts = payload.get("artifacts")
    if not all(
        isinstance(value, dict)
        for value in (source, build, host, manifest, protocol, artifacts)
    ):
        fail("suite metadata is incomplete")
    source_state = source.get("tracked_worktree_state_at_start")
    if source_state not in ("clean", "dirty"):
        fail("suite metadata has invalid source state")
    if mode == "durable" and source_state != "clean":
        fail("durable suite must originate from a clean worktree")
    diff_hash = source.get("tracked_diff_sha256")
    if not isinstance(diff_hash, str) or len(diff_hash) != 64:
        fail("suite metadata has invalid source diff hash")
    if source_state == "clean" and diff_hash != hashlib.sha256(b"").hexdigest():
        fail("clean suite metadata has a non-empty source diff")
    revision = source.get("revision")
    if not isinstance(revision, str) or not revision or revision == "unknown":
        fail("suite metadata has invalid revision")
    if source.get("suite_end_snapshot_matches") is not True:
        fail("suite metadata lacks the end-snapshot check")
    if (
        build.get("source_directory_matches") is not True
        or build.get("configured_source_directory")
        != source.get("top_source_directory")
    ):
        fail("suite build source differs from the source tree")
    platform_name = host.get("platform")
    if not isinstance(platform_name, str) or not platform_name:
        fail("suite metadata has invalid host platform")
    manifest_path = manifest.get("path")
    manifest_hash = manifest.get("sha256")
    if not isinstance(manifest_path, str) or not manifest_path:
        fail("suite metadata has invalid manifest path")
    if not isinstance(manifest_hash, str) or len(manifest_hash) != 64:
        fail("suite metadata has invalid manifest hash")
    validate_programs(payload.get("programs"))
    validate_linkage(payload.get("libsixel_linkage"))
    fixtures = validate_fixtures(payload.get("fixtures"))
    warmups = int(protocol.get("speed_warmups", -1))
    runs = int(protocol.get("speed_runs", 0))
    if warmups < 0 or runs < 1:
        fail("suite metadata has invalid run counts")
    if mode == "durable" and (warmups, runs) != (2, 10):
        fail("durable suite requires 2 warm-ups and 10 runs")
    expected_protocol = {
        "measurement_mode": mode,
        "comparison_scope": (
            "palette sampling and binning across content and pixel count"
        ),
        "content_fixture_ids": [
            fixture["id"] for fixture in fixtures
            if "content" in fixture["roles"]
        ],
        "scale_fixture_ids": [
            fixture["id"] for fixture in sorted(
                (item for item in fixtures if "scale" in item["roles"]),
                key=lambda item: int(item["pixels"]),
            )
        ],
        "content_classes": sorted(CONTENT_CLASSES),
        "quality_improvement": (
            "candidate minus control for MS-SSIM; control minus "
            "candidate for mean Delta E00"
        ),
        "size_improvement": "100 * (1 - candidate bytes / control bytes)",
        "speed_improvement": "control median / candidate median",
        "scale_plot_colors": list(SCALE_COLORS),
        "fixture_and_policy_orders_recorded": True,
        "shared_single_fixture_measurement_code": True,
        "provenance_rechecked_after_suite": True,
    }
    for field, value in expected_protocol.items():
        if protocol.get(field) != value:
            fail(f"suite metadata differs in {field}")
    validate_single_protocol(
        protocol.get("single_fixture_protocol"), mode, warmups, runs
    )
    fixture_order = protocol.get("fixture_order_design")
    if not isinstance(fixture_order, dict):
        fail("suite metadata lacks fixture order design")
    sequence_period = len(williams_orders([
        str(fixture["id"]) for fixture in fixtures
    ]))
    expected_fixture_order = {
        "name": "Williams-derived fixture order",
        "physical_fixtures": len(fixtures),
        "sequence_period": sequence_period,
        "measured_sequence_start": 0,
        "position_balance_period": len(fixtures),
        "full_carryover_balance_period": sequence_period,
    }
    if fixture_order != expected_fixture_order:
        fail("suite metadata has unexpected fixture order design")
    plots = artifacts.get("plots")
    if plots != list(PLOTS):
        fail("suite metadata has unexpected plots")
    return payload, fixtures


def validate_row_identity(row: Dict[str, str],
                          path: Path,
                          fixture: Dict[str, object],
                          revision: str,
                          platform_name: str) -> None:
    """Validate fields shared by every aggregate measurement row."""
    expected = {
        "fixture_id": str(fixture["id"]),
        "fixture_label": str(fixture["label"]),
        "fixture_class": str(fixture["class"]),
        "fixture_roles": ";".join(fixture["roles"]),
        "width": str(fixture["width"]),
        "height": str(fixture["height"]),
        "pixels": str(fixture["pixels"]),
        "revision": revision,
        "platform": platform_name,
        "input": str(fixture["path"]),
    }
    for field, value in expected.items():
        if row.get(field) != value:
            fail(
                f"suite row differs in {field} for {fixture['id']} in {path}"
            )


def expected_point_order(fixtures: Sequence[Dict[str, object]]) \
        -> List[Tuple[str, str, int]]:
    """Return exact fixture, configuration, and K order for point CSVs."""
    return [
        (str(fixture["id"]), config, colors)
        for fixture in fixtures
        for colors in COLORS
        for config, _sampling, _binning in CONFIGS
    ]


def validate_points(path: Path,
                    fixtures: Sequence[Dict[str, object]],
                    revision: str,
                    platform_name: str,
                    metric: str) -> int:
    """Validate quality or size point rows in exact executed order."""
    rows = read_csv(path)
    by_id = {str(fixture["id"]): fixture for fixture in fixtures}
    actual = []
    for row in rows:
        fixture_id = row.get("fixture_id", "")
        fixture = by_id.get(fixture_id)
        if fixture is None:
            fail(f"suite row has unknown fixture in {path}: {fixture_id}")
        validate_row_identity(row, path, fixture, revision, platform_name)
        validate_command(row, path, loader_order=CMS_LOADER_ORDER)
        if metric == "quality":
            validate_assessment_command(
                row, path, CMS_LOADER_ORDER, True
            )
        try:
            colors = int(row.get("colors", ""))
            if metric == "quality":
                ms_ssim = float(row.get("MS-SSIM", ""))
                delta_e00 = float(row.get("Delta E00_mean", ""))
                if not math.isfinite(ms_ssim) or not 0.0 <= ms_ssim <= 1.0:
                    fail(f"invalid suite MS-SSIM in {path}")
                if not math.isfinite(delta_e00) or delta_e00 < 0.0:
                    fail(f"invalid suite Delta E00 in {path}")
            else:
                encoded_bytes = int(row.get("encoded_bytes", ""))
                if encoded_bytes <= 0:
                    fail(f"invalid suite size in {path}")
        except ValueError as exc:
            raise ValueError(f"invalid suite point value in {path}") from exc
        actual.append((fixture_id, row.get("config", ""), colors))
    expected = expected_point_order(fixtures)
    if actual != expected:
        fail(f"suite point order or completeness differs in {path}")
    return len(rows)


def expected_speed_order(fixtures: Sequence[Dict[str, object]],
                         warmups: int,
                         runs: int) -> List[Tuple[object, ...]]:
    """Return the complete aggregate raw timing schedule."""
    names = [config for config, _sampling, _binning in CONFIGS]
    orders = williams_orders(names)
    fixture_ids = [str(fixture["id"]) for fixture in fixtures]
    fixture_orders = williams_orders(fixture_ids)
    schedule = []
    for colors in COLORS:
        for domain in ("end-to-end", "palette-build"):
            for round_index in range(warmups + runs):
                phase = "warmup" if round_index < warmups else "measured"
                phase_round = round_index - warmups
                sequence = phase_round % len(orders)
                fixture_sequence = phase_round % len(fixture_orders)
                for fixture_position, fixture_id in enumerate(
                        fixture_orders[fixture_sequence]):
                    schedule.append(
                        (
                            fixture_id,
                            colors,
                            domain,
                            round_index,
                            sequence,
                            -1,
                            "washout",
                            "full-frame-hard",
                            fixture_sequence,
                            fixture_position,
                        )
                    )
                    schedule.extend(
                        (
                            fixture_id,
                            colors,
                            domain,
                            round_index,
                            sequence,
                            position,
                            phase,
                            config,
                            fixture_sequence,
                            fixture_position,
                        )
                        for position, config in enumerate(orders[sequence])
                    )
    return schedule


def validate_speed(path: Path,
                   fixtures: Sequence[Dict[str, object]],
                   revision: str,
                   platform_name: str,
                   warmups: int,
                   runs: int) -> int:
    """Validate every raw suite timing row and exact schedule."""
    rows = read_csv(path)
    by_id = {str(fixture["id"]): fixture for fixture in fixtures}
    actual = []
    for row in rows:
        fixture_id = row.get("fixture_id", "")
        fixture = by_id.get(fixture_id)
        if fixture is None:
            fail(f"suite speed row has unknown fixture: {fixture_id}")
        validate_row_identity(row, path, fixture, revision, platform_name)
        domain = row.get("domain", "")
        validate_command(
            row,
            path,
            domain == "palette-build",
            True,
            CMS_LOADER_ORDER,
        )
        try:
            identity = (
                fixture_id,
                int(row.get("colors", "")),
                domain,
                int(row.get("round", "")),
                int(row.get("sequence", "")),
                int(row.get("position", "")),
                row.get("phase", ""),
                row.get("config", ""),
                int(row.get("fixture_sequence", "")),
                int(row.get("fixture_position", "")),
            )
            seconds = float(row.get("seconds", ""))
        except ValueError as exc:
            raise ValueError(f"invalid suite speed value in {path}") from exc
        if domain not in ("end-to-end", "palette-build"):
            fail(f"invalid suite speed domain in {path}")
        if not math.isfinite(seconds) or seconds <= 0.0:
            fail(f"invalid suite duration in {path}")
        actual.append(identity)
    expected = expected_speed_order(fixtures, warmups, runs)
    if actual != expected:
        mismatch = next(
            (
                index
                for index, (left, right) in enumerate(zip(actual, expected))
                if left != right
            ),
            min(len(actual), len(expected)),
        )
        fail(f"suite timing schedule differs at row {mismatch + 2} in {path}")
    return len(rows)


def validate_plots(directory: Path) -> None:
    """Reject missing or implausibly small aggregate plots."""
    for name in PLOTS:
        path = directory / name
        if not path.is_file() or path.stat().st_size < 20000:
            fail(f"missing or unexpectedly small suite plot: {path}")


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
    """Validate all aggregate suite artifacts."""
    args = parse_args()
    directory = args.directory.resolve()
    metadata, fixtures = read_metadata(
        directory / "palette-pipeline-suite-run.json", args.mode
    )
    source = metadata["source"]
    host = metadata["host"]
    protocol = metadata["protocol"]
    revision = str(source["revision"])
    platform_name = str(host["platform"])
    warmups = int(protocol["speed_warmups"])
    runs = int(protocol["speed_runs"])
    counts = {
        "quality": validate_points(
            directory / "palette-pipeline-suite-quality.csv",
            fixtures,
            revision,
            platform_name,
            "quality",
        ),
        "size": validate_points(
            directory / "palette-pipeline-suite-size.csv",
            fixtures,
            revision,
            platform_name,
            "size",
        ),
        "speed": validate_speed(
            directory / "palette-pipeline-suite-speed.csv",
            fixtures,
            revision,
            platform_name,
            warmups,
            runs,
        ),
    }
    if metadata["artifacts"].get("row_counts") != counts:
        fail("suite metadata row counts differ from aggregate CSV files")
    validate_plots(directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
