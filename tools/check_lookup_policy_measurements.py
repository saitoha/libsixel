#!/usr/bin/env python3
"""Validate the completeness and provenance of lookup-policy measurements."""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
from pathlib import Path
from typing import Dict, List, Sequence, Set, Tuple


POLICIES = (
    "none",
    "5bit",
    "6bit",
    "certlut",
    "eytzinger",
    "fhedt",
    "vptree",
    "rbc",
    "mahalanobis",
)
BROAD_COLORS = (8, 16, 32, 64, 128, 256)
FOCUSED_COLORS = (128, 144, 160, 176, 192, 208, 224, 240, 256)
QUALITY_FILES = {
    "lookup-policy-color-error.csv": BROAD_COLORS,
    "lookup-policy-ms-ssim.csv": BROAD_COLORS,
    "lookup-policy-kmeans-color-error.csv": BROAD_COLORS,
    "lookup-policy-kmeans-ms-ssim.csv": BROAD_COLORS,
    "lookup-policy-kmeans-high-k.csv": FOCUSED_COLORS,
    "lookup-policy-heckbert-high-k.csv": FOCUSED_COLORS,
}
PLOT_FILES = tuple(name.replace(".csv", ".png") for name in QUALITY_FILES)
PLOT_FILES += ("lookup-policy-speed.png",)
POLICY_PATTERN = re.compile(r"--lookup-policy=([^ ]+)")
ACCELERATION_FILES = (
    "lookup-policy-shared-speed.csv",
    "lookup-policy-metal-speed.csv",
    "lookup-policy-shared-speed.png",
    "lookup-policy-metal-speed.png",
    "lookup-policy-acceleration-run.json",
)
SHARED_POLICIES = ("5bit", "6bit", "certlut")
METAL_POLICIES = ("none", "eytzinger")


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read a non-empty CSV file into dictionaries."""
    if not path.is_file():
        fail(f"missing measurement file: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"measurement file has no rows: {path}")
    return rows


def policy_from_template(template: str, path: Path) -> str:
    """Extract the one lookup policy embedded in a command template."""
    matches = POLICY_PATTERN.findall(template)
    if len(matches) != 1:
        fail(f"expected one lookup policy in {path}: {template}")
    policy = matches[0]
    if policy not in POLICIES:
        fail(f"unexpected lookup policy in {path}: {policy}")
    return policy


def validate_finite_metrics(row: Dict[str, str], path: Path) -> None:
    """Validate every metric column in one quality row."""
    fixed_fields = {"kind", "command", "template", "image", "colors"}
    metric_names = set(row) - fixed_fields
    if not metric_names:
        fail(f"no metric columns in {path}")
    for name in metric_names:
        try:
            value = float(row[name])
        except (TypeError, ValueError) as exc:
            raise ValueError(f"invalid {name} value in {path}") from exc
        if not math.isfinite(value):
            fail(f"non-finite {name} value in {path}")


def validate_quality_file(path: Path, colors: Sequence[int]) -> None:
    """Check one quality sweep for its complete policy/color Cartesian set."""
    rows = read_csv(path)
    actual: Set[Tuple[str, int]] = set()
    images = set()
    for row in rows:
        if row.get("kind") != "image":
            fail(f"unexpected aggregate row in single-image sweep: {path}")
        policy = policy_from_template(row.get("template", ""), path)
        try:
            color = int(row.get("colors", ""))
        except ValueError as exc:
            raise ValueError(f"invalid palette size in {path}") from exc
        key = (policy, color)
        if key in actual:
            fail(f"duplicate quality point in {path}: {policy}, K={color}")
        actual.add(key)
        images.add(row.get("image", ""))
        validate_finite_metrics(row, path)

    expected = {(policy, color) for policy in POLICIES for color in colors}
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        fail(f"quality sweep mismatch in {path}: missing={missing}, extra={extra}")
    if len(images) != 1 or "" in images:
        fail(f"quality sweep must contain exactly one named image: {path}")


def validate_speed_file(path: Path, metadata: Dict[str, object]) -> None:
    """Check speed rows, revision consistency, and baseline normalization."""
    rows = read_csv(path)
    protocol = metadata.get("protocol")
    source = metadata.get("source")
    if not isinstance(protocol, dict) or not isinstance(source, dict):
        fail("metadata lacks protocol or source object")
    colors = tuple(int(value) for value in protocol.get("speed_colors", []))
    policies = tuple(str(value) for value in protocol.get("policies", []))
    revision = str(source.get("revision", ""))
    if colors != BROAD_COLORS:
        fail(f"metadata speed colors differ from protocol: {colors}")
    if policies != POLICIES:
        fail(f"metadata policies differ from protocol: {policies}")
    if not revision or revision == "unknown":
        fail("metadata source revision is missing")

    actual: Set[Tuple[str, int]] = set()
    for row in rows:
        policy = row.get("policy", "")
        try:
            color = int(row.get("colors", ""))
            median = float(row.get("median_seconds", ""))
            speedup = float(row.get("speedup_vs_none", ""))
        except ValueError as exc:
            raise ValueError(f"invalid speed value in {path}") from exc
        key = (policy, color)
        if key in actual:
            fail(f"duplicate speed point in {path}: {policy}, K={color}")
        actual.add(key)
        if row.get("revision") != revision:
            fail(f"speed revision differs from metadata in {path}")
        if median <= 0.0 or not math.isfinite(median):
            fail(f"invalid median in {path}: {policy}, K={color}")
        if speedup <= 0.0 or not math.isfinite(speedup):
            fail(f"invalid speedup in {path}: {policy}, K={color}")
        if policy == "none" and not math.isclose(speedup, 1.0, rel_tol=1e-12):
            fail(f"none baseline is not 1.0 in {path}: K={color}")
        command_policy = policy_from_template(row.get("command", ""), path)
        if command_policy != policy:
            fail(f"speed command/policy mismatch in {path}: {policy}")

    expected = {
        (policy, color) for policy in POLICIES for color in BROAD_COLORS
    }
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        fail(f"speed sweep mismatch in {path}: missing={missing}, extra={extra}")


def validate_metadata(path: Path) -> Dict[str, object]:
    """Read and validate the common run manifest."""
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
    if not all(isinstance(item, dict) for item in (
        source,
        protocol,
        programs,
        input_record,
    )):
        fail(f"incomplete measurement metadata: {path}")
    if protocol.get("sixel_environment_removed") is not True:
        fail("measurement metadata does not confirm SIXEL_* isolation")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("checked-in measurements must originate from a clean worktree")
    for name in ("img2sixel", "lsqa"):
        record = programs.get(name)
        if not isinstance(record, dict):
            fail(f"missing {name} provenance in {path}")
        for field in ("launcher_sha256", "payload_sha256"):
            value = record.get(field)
            if not isinstance(value, str) or len(value) != 64:
                fail(f"invalid {name} {field} in {path}")
    input_hash = input_record.get("sha256")
    if not isinstance(input_hash, str) or len(input_hash) != 64:
        fail(f"invalid input SHA-256 in {path}")
    return payload


def validate_plots(directory: Path) -> None:
    """Require every plot to exist and contain data."""
    for name in PLOT_FILES:
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            fail(f"missing or empty plot: {path}")


def validate_acceleration_metadata(path: Path) -> Dict[str, object]:
    """Read and validate the shared-instance and Metal run manifest."""
    if not path.is_file():
        fail(f"missing acceleration metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        fail(f"unsupported acceleration metadata schema: {path}")
    source = payload.get("source")
    protocol = payload.get("protocol")
    if not isinstance(source, dict) or not isinstance(protocol, dict):
        fail(f"incomplete acceleration metadata: {path}")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("acceleration measurements must originate from a clean worktree")
    if protocol.get("sixel_environment_removed") is not True:
        fail("acceleration metadata does not confirm SIXEL_* isolation")
    colors = tuple(int(value) for value in protocol.get("colors", []))
    if colors != BROAD_COLORS:
        fail(f"acceleration colors differ from protocol: {colors}")

    shared = protocol.get("shared_instance")
    metal = protocol.get("metal")
    if not isinstance(shared, dict) or not isinstance(metal, dict):
        fail("acceleration metadata lacks shared-instance or Metal protocol")
    if tuple(shared.get("policies", [])) != SHARED_POLICIES:
        fail("acceleration metadata has unexpected shared-instance policies")
    if tuple(shared.get("values", [])) != (0, 1):
        fail("acceleration metadata must compare shared_instance=0 and 1")
    if shared.get("quantize_model") != "kmeans:merge=ward:seed=1":
        fail("shared-instance measurement must use controlled K-means")
    if int(shared.get("threads", 0)) < 2:
        fail("shared-instance measurement must use multiple threads")
    shared_equivalence = shared.get("output_equivalence")
    if not isinstance(shared_equivalence, dict):
        fail("shared-instance metadata lacks output-equivalence records")
    for policy in SHARED_POLICIES:
        record = shared_equivalence.get(policy)
        if not isinstance(record, dict) or record.get("byte_identical") is not True:
            fail(f"shared-instance output equivalence is absent for {policy}")
        digest = record.get("sha256")
        if not isinstance(digest, str) or len(digest) != 64:
            fail(f"invalid shared-instance output digest for {policy}")
    if tuple(metal.get("policies", [])) != METAL_POLICIES:
        fail("acceleration metadata has unexpected Metal policies")
    if tuple(metal.get("cpu_gpu_policies", [])) != ("off", "force"):
        fail("Metal measurement must compare gpu-policy=off and force")
    if metal.get("quantize_model") != "heckbert:cover=off:merge=none":
        fail("Metal measurement must exercise the Heckbert PaletteApply path")
    if metal.get("force_success_required") is not True:
        fail("Metal measurement does not require a forced GPU path")
    equivalence = metal.get("output_equivalence")
    if not isinstance(equivalence, dict):
        fail("Metal metadata lacks output-equivalence records")
    for policy in METAL_POLICIES:
        record = equivalence.get(policy)
        if not isinstance(record, dict) or record.get("byte_identical") is not True:
            fail(f"Metal output equivalence is not established for {policy}")
        digest = record.get("sha256")
        if not isinstance(digest, str) or len(digest) != 64:
            fail(f"invalid Metal output digest for {policy}")
    return payload


def validate_acceleration_speed_file(path: Path,
                                     metadata: Dict[str, object],
                                     comparison: str) -> None:
    """Validate one focused acceleration speed matrix."""
    rows = read_csv(path)
    source = metadata["source"]
    protocol = metadata["protocol"]
    revision = str(source.get("revision", ""))
    if not revision or revision == "unknown":
        fail("acceleration metadata source revision is missing")

    if comparison == "shared_instance":
        shared = protocol["shared_instance"]
        threads = int(shared["threads"])
        expected = {
            (policy, shared_value, color)
            for policy in SHARED_POLICIES
            for shared_value in (0, 1)
            for color in BROAD_COLORS
        }
    else:
        threads = 1
        expected = {
            (policy, gpu_policy, color)
            for policy in METAL_POLICIES
            for gpu_policy in ("off", "force")
            for color in BROAD_COLORS
        }

    actual = set()
    for row in rows:
        if row.get("comparison") != comparison:
            fail(f"unexpected comparison in {path}: {row.get('comparison')}")
        try:
            color = int(row.get("colors", ""))
            row_threads = int(row.get("threads", ""))
            median = float(row.get("median_seconds", ""))
        except ValueError as exc:
            raise ValueError(f"invalid acceleration value in {path}") from exc
        if row.get("revision") != revision:
            fail(f"acceleration revision differs from metadata in {path}")
        if row_threads != threads:
            fail(f"unexpected thread count in {path}: {row_threads}")
        if median <= 0.0 or not math.isfinite(median):
            fail(f"invalid acceleration median in {path}")
        policy = row.get("policy", "")
        command = row.get("command", "")
        if comparison == "shared_instance":
            try:
                shared_value = int(row.get("shared_instance", ""))
            except ValueError as exc:
                raise ValueError(f"invalid shared_instance in {path}") from exc
            key = (policy, shared_value, color)
            expected_option = (
                f"--lookup-policy={policy}:shared_instance={shared_value}"
            )
            if row.get("gpu_policy") != "off":
                fail(f"shared-instance run unexpectedly enables GPU in {path}")
            if "--quantize-model=kmeans:merge=ward:seed=1" not in command:
                fail(f"shared-instance run does not use K-means in {path}")
        else:
            gpu_policy = row.get("gpu_policy", "")
            key = (policy, gpu_policy, color)
            expected_option = f"--lookup-policy={policy}"
            if row.get("shared_instance") != "":
                fail(f"Metal run unexpectedly sets shared_instance in {path}")
            if f"--gpu-policy={gpu_policy}" not in command:
                fail(f"Metal command/gpu-policy mismatch in {path}")
            heckbert = "--quantize-model=heckbert:cover=off:merge=none"
            if heckbert not in command:
                fail(f"Metal run does not use the PaletteApply path in {path}")
        if key in actual:
            fail(f"duplicate acceleration point in {path}: {key}")
        actual.add(key)
        if expected_option not in command:
            fail(f"acceleration command/policy mismatch in {path}: {key}")
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        fail(f"acceleration sweep mismatch in {path}: missing={missing}, extra={extra}")


def validate_acceleration(directory: Path) -> None:
    """Validate all focused shared-instance and Metal artifacts."""
    metadata = validate_acceleration_metadata(
        directory / "lookup-policy-acceleration-run.json"
    )
    validate_acceleration_speed_file(
        directory / "lookup-policy-shared-speed.csv",
        metadata,
        "shared_instance",
    )
    validate_acceleration_speed_file(
        directory / "lookup-policy-metal-speed.csv",
        metadata,
        "metal",
    )
    for name in (
        "lookup-policy-shared-speed.png",
        "lookup-policy-metal-speed.png",
    ):
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            fail(f"missing or empty acceleration plot: {path}")


def main() -> int:
    """Validate one complete generated measurement directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--require-acceleration", action="store_true")
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    directory = args.directory

    metadata = validate_metadata(directory / "lookup-policy-run.json")
    for name, colors in QUALITY_FILES.items():
        validate_quality_file(directory / name, colors)
    validate_speed_file(directory / "lookup-policy-speed.csv", metadata)
    validate_plots(directory)
    acceleration_present = any(
        (directory / name).exists() for name in ACCELERATION_FILES
    )
    if args.require_acceleration or acceleration_present:
        validate_acceleration(directory)
    print(
        "validated 378 quality points and 54 speed points across 9 policies"
    )
    if args.require_acceleration or acceleration_present:
        print("validated 36 shared-instance and 24 Metal speed points")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
