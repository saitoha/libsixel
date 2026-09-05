#!/usr/bin/env python3
"""Validate clustering hard-binning point-sufficiency artifacts."""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
import sys
from pathlib import Path
from typing import Dict, List, Sequence, Set, Tuple


BINBITS = (4, 5, 6, 7, 8)
COLORSPACES = ("gamma", "linear", "oklab", "cielab", "din99d")
CRITERIA: Tuple[Tuple[str, float, float], ...] = (
    ("strict", 0.001, 0.05),
    ("balanced", 0.002, 0.15),
    ("relaxed", 0.005, 0.25),
)
PASS_RATE_TARGET = 0.8
PLOTS = (
    "clustering-binning-threshold-quality.png",
    "clustering-binning-threshold-pass-rate.png",
)


def fail(message: str) -> None:
    """Raise one concise validation failure."""
    raise ValueError(message)


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read a non-empty CSV artifact."""
    if not path.is_file():
        fail(f"missing CSV: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"empty CSV: {path}")
    return rows


def read_metadata(path: Path) -> Dict[str, object]:
    """Read and minimally validate the provenance manifest."""
    if not path.is_file():
        fail(f"missing metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        fail(f"unsupported metadata schema: {path}")
    source = payload.get("source")
    protocol = payload.get("protocol")
    fixtures = payload.get("fixtures")
    rows = payload.get("rows")
    if not all(
        isinstance(value, dict)
        for value in (source, protocol, rows)
    ) or not isinstance(fixtures, list):
        fail(f"incomplete metadata: {path}")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("durable threshold measurement did not start clean")
    if source.get("suite_end_snapshot_matches") is not True:
        fail("threshold measurement end snapshot was not verified")
    if payload.get("build", {}).get("source_directory_matches") is not True:
        fail("threshold measurement build came from another source tree")
    return payload


def percentile(values: Sequence[float], fraction: float) -> float:
    """Return a linearly interpolated percentile."""
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def close(actual: float, expected: float) -> bool:
    """Compare generated decimal values at serialization precision."""
    return math.isclose(actual, expected, rel_tol=1e-11, abs_tol=1e-12)


def validate_command(row: Dict[str, str]) -> None:
    """Validate the controlled command encoded in one raw row."""
    policy = row["binning_policy"]
    binbits = int(row["binbits"])
    command_bits = binbits if policy == "hard" else 6
    required = (
        "--threads=1",
        "--precision=float32",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        f"--binning-policy={policy}",
        f"--quantize-model=kmeans:seed={row['seed']}:binbits={command_bits}",
        "-F none",
        "-a off",
        f"-X{row['colorspace']}",
        "-Wgamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        f"-p {row['colors']}",
    )
    command = row["command"]
    if not all(token in command for token in required):
        fail(
            "raw command does not match the controlled protocol: "
            f"{row['fixture_id']}, {row['colorspace']}, "
            f"K={row['colors']}, seed={row['seed']}, {policy}:{binbits}"
        )


def validate_raw(path: Path,
                 metadata: Dict[str, object]) \
        -> Tuple[List[Dict[str, str]], Dict[
            Tuple[str, int, str, int], List[Dict[str, float]]
        ]]:
    """Validate raw paired rows and return recomputable regret samples."""
    rows = read_csv(path)
    protocol = metadata["protocol"]
    revision = str(metadata["source"]["revision"])
    fixtures = [str(record["id"]) for record in metadata["fixtures"]]
    colors_values = [int(value) for value in protocol["colors"]]
    seeds = [int(value) for value in protocol["seeds"]]
    expected = {
        (fixture, colors, colorspace, seed, policy, binbits)
        for fixture in fixtures
        for colors in colors_values
        for colorspace in COLORSPACES
        for seed in seeds
        for policy, binbits in (
            *(("hard", value) for value in BINBITS),
            ("none", 0),
        )
    }
    actual: Set[Tuple[str, int, str, int, str, int]] = set()
    population: Dict[Tuple[str, str, str, int], Tuple[int, int]] = {}
    by_pair: Dict[
        Tuple[str, int, str, int, str, int], Dict[str, str]
    ] = {}
    for row in rows:
        try:
            colors = int(row["colors"])
            seed = int(row["seed"])
            binbits = int(row["binbits"])
            source_points = int(row["source_points"])
            effective_points = int(row["effective_points"])
            points_per_color = float(row["effective_points_per_color"])
            ms_ssim = float(row["MS-SSIM"])
            delta_e = float(row["Delta E00_mean"])
            delta_chroma = float(row["Delta Chroma_mean"])
            encoded_bytes = int(row["encoded_bytes"])
        except (KeyError, ValueError) as exc:
            raise ValueError(f"invalid raw value in {path}") from exc
        key = (
            row["fixture_id"], colors, row["colorspace"], seed,
            row["binning_policy"], binbits,
        )
        if key in actual:
            fail(f"duplicate raw point in {path}: {key}")
        actual.add(key)
        by_pair[key] = row
        if row["revision"] != revision:
            fail(f"raw revision differs in {path}: {key}")
        if not (
            0.0 <= ms_ssim <= 1.0
            and delta_e >= 0.0
            and delta_chroma >= 0.0
            and encoded_bytes > 0
            and source_points >= effective_points >= 1
            and close(points_per_color, effective_points / colors)
        ):
            fail(f"invalid raw metric or population in {path}: {key}")
        if row["binning_policy"] == "none":
            if binbits != 0 or effective_points != source_points:
                fail(f"invalid none population in {path}: {key}")
        elif row["binning_policy"] != "hard" or binbits not in BINBITS:
            fail(f"invalid hard-grid identity in {path}: {key}")
        population_key = (
            row["fixture_id"], row["colorspace"],
            row["binning_policy"], binbits,
        )
        previous = population.setdefault(
            population_key,
            (source_points, effective_points),
        )
        if previous != (source_points, effective_points):
            fail(f"population changed across K or seed: {population_key}")
        validate_command(row)
    if actual != expected:
        fail(
            f"raw sweep mismatch: missing={len(expected - actual)}, "
            f"extra={len(actual - expected)}"
        )
    grouped: Dict[
        Tuple[str, int, str, int], List[Dict[str, float]]
    ] = {}
    for fixture in fixtures:
        for colors in colors_values:
            for colorspace in COLORSPACES:
                for binbits in BINBITS:
                    group_key = (fixture, colors, colorspace, binbits)
                    values = []
                    for seed in seeds:
                        hard = by_pair[
                            fixture, colors, colorspace, seed,
                            "hard", binbits,
                        ]
                        base = by_pair[
                            fixture, colors, colorspace, seed,
                            "none", 0,
                        ]
                        values.append(
                            {
                                "MS-SSIM_regret": (
                                    float(base["MS-SSIM"])
                                    - float(hard["MS-SSIM"])
                                ),
                                "Delta E00_regret": (
                                    float(hard["Delta E00_mean"])
                                    - float(base["Delta E00_mean"])
                                ),
                                "Delta Chroma_regret": (
                                    float(hard["Delta Chroma_mean"])
                                    - float(base["Delta Chroma_mean"])
                                ),
                                "encoded_size_ratio": (
                                    int(hard["encoded_bytes"])
                                    / int(base["encoded_bytes"])
                                ),
                            }
                        )
                    grouped[group_key] = values
    return rows, grouped


def validate_summary(path: Path,
                     metadata: Dict[str, object],
                     grouped: Dict[
                         Tuple[str, int, str, int],
                         List[Dict[str, float]]
                     ]) -> List[Dict[str, str]]:
    """Recompute all paired-regret summary statistics and pass rates."""
    rows = read_csv(path)
    revision = str(metadata["source"]["revision"])
    expected = set(grouped)
    actual: Set[Tuple[str, int, str, int]] = set()
    for row in rows:
        try:
            key = (
                row["fixture_id"],
                int(row["colors"]),
                row["colorspace"],
                int(row["binbits"]),
            )
            seeds = int(row["seeds"])
        except (KeyError, ValueError) as exc:
            raise ValueError(f"invalid summary identity in {path}") from exc
        if key in actual:
            fail(f"duplicate summary point in {path}: {key}")
        actual.add(key)
        values = grouped.get(key)
        if values is None or row["revision"] != revision:
            fail(f"unknown summary point in {path}: {key}")
        if seeds != len(values):
            fail(f"summary seed count differs in {path}: {key}")
        for metric in (
            "MS-SSIM_regret", "Delta E00_regret",
            "Delta Chroma_regret", "encoded_size_ratio",
        ):
            samples = [value[metric] for value in values]
            expected_stats = {
                "median": statistics.median(samples),
                "p90": percentile(samples, 0.9),
                "max": max(samples),
            }
            for statistic, expected_value in expected_stats.items():
                field = f"{metric}_{statistic}"
                if not close(float(row[field]), expected_value):
                    fail(f"summary {field} differs in {path}: {key}")
        for name, ms_limit, de_limit in CRITERIA:
            passed = sum(
                value["MS-SSIM_regret"] <= ms_limit
                and value["Delta E00_regret"] <= de_limit
                for value in values
            )
            if int(row[f"{name}_pass_count"]) != passed:
                fail(f"summary {name} count differs in {path}: {key}")
            if not close(float(row[f"{name}_pass_rate"]), passed / seeds):
                fail(f"summary {name} rate differs in {path}: {key}")
    if actual != expected:
        fail(
            f"summary sweep mismatch: missing={len(expected - actual)}, "
            f"extra={len(actual - expected)}"
        )
    return rows


def validate_thresholds(path: Path,
                        metadata: Dict[str, object],
                        summary_rows: Sequence[Dict[str, str]]) -> None:
    """Recompute observed and stable per-condition threshold selections."""
    rows = read_csv(path)
    revision = str(metadata["source"]["revision"])
    grouped: Dict[Tuple[str, int, str], List[Dict[str, str]]] = {}
    for row in summary_rows:
        key = (row["fixture_id"], int(row["colors"]), row["colorspace"])
        grouped.setdefault(key, []).append(row)
    expected = {
        (*key, criterion)
        for key in grouped
        for criterion, _ms_limit, _de_limit in CRITERIA
    }
    actual: Set[Tuple[str, int, str, str]] = set()
    for row in rows:
        key = (
            row["fixture_id"], int(row["colors"]),
            row["colorspace"], row["criterion"],
        )
        if key in actual:
            fail(f"duplicate threshold point in {path}: {key}")
        actual.add(key)
        if key not in expected or row["revision"] != revision:
            fail(f"unknown threshold point in {path}: {key}")
        criterion = key[3]
        candidates = sorted(
            grouped[key[:3]], key=lambda value: int(value["binbits"])
        )
        passing = [
            value for value in candidates
            if float(value[f"{criterion}_pass_rate"])
            >= PASS_RATE_TARGET
        ]
        stable = [
            value for index, value in enumerate(candidates)
            if all(
                float(candidate[f"{criterion}_pass_rate"])
                >= PASS_RATE_TARGET
                for candidate in candidates[index:]
            )
        ]
        observed = passing[0] if passing else None
        stable_row = stable[0] if stable else None
        expected_fields = {
            "observed_binbits": observed["binbits"] if observed else "",
            "observed_effective_points": (
                observed["effective_points"] if observed else ""
            ),
            "observed_points_per_color": (
                observed["effective_points_per_color"] if observed else ""
            ),
            "stable_binbits": stable_row["binbits"] if stable_row else "",
            "stable_effective_points": (
                stable_row["effective_points"] if stable_row else ""
            ),
            "stable_points_per_color": (
                stable_row["effective_points_per_color"]
                if stable_row else ""
            ),
            "stable_censored": "no" if stable_row else "yes",
        }
        for field, expected_value in expected_fields.items():
            if row[field] != expected_value:
                fail(f"threshold {field} differs in {path}: {key}")
    if actual != expected:
        fail(
            f"threshold sweep mismatch: missing={len(expected - actual)}, "
            f"extra={len(actual - expected)}"
        )


def validate_plots(directory: Path) -> None:
    """Reject absent or implausibly small figures."""
    for name in PLOTS:
        path = directory / name
        if not path.is_file() or path.stat().st_size < 20000:
            fail(f"missing or unexpectedly small plot: {path}")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    return parser.parse_args()


def main() -> int:
    """Validate one generated threshold-measurement directory."""
    args = parse_args()
    directory = args.directory.resolve()
    metadata = read_metadata(
        directory / "clustering-binning-threshold-run.json"
    )
    raw_rows, grouped = validate_raw(
        directory / "clustering-binning-threshold-raw.csv",
        metadata,
    )
    summary_rows = validate_summary(
        directory / "clustering-binning-threshold-summary.csv",
        metadata,
        grouped,
    )
    validate_thresholds(
        directory / "clustering-binning-thresholds.csv",
        metadata,
        summary_rows,
    )
    validate_plots(directory)
    expected_counts = {
        "raw": len(raw_rows),
        "summary": len(summary_rows),
        "thresholds": (
            len(metadata["fixtures"])
            * len(metadata["protocol"]["colors"])
            * len(COLORSPACES)
            * len(CRITERIA)
        ),
    }
    if metadata["rows"] != expected_counts:
        fail("metadata row counts differ from generated artifacts")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
