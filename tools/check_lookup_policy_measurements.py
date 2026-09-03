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


def main() -> int:
    """Validate one complete generated measurement directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    directory = args.directory

    metadata = validate_metadata(directory / "lookup-policy-run.json")
    for name, colors in QUALITY_FILES.items():
        validate_quality_file(directory / name, colors)
    validate_speed_file(directory / "lookup-policy-speed.csv", metadata)
    validate_plots(directory)
    print(
        "validated 378 quality points and 54 speed points across 9 policies"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
