#!/usr/bin/env python3
"""Validate checked-in cross-cutting encoder precision measurements."""

from __future__ import annotations

import argparse
import ast
import csv
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Sequence, Tuple


PALETTE_COLORS = 64
PRECISIONS: Tuple[Tuple[str, str, str], ...] = (
    ("8bit", "8-bit", "rgb888"),
    ("float32", "float32", "rgb-f32"),
)
DOMAIN_ORDER = (
    "quantizer",
    "dither",
    "lookup",
    "palette-pipeline",
    "clustering-colorspace",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def literal_assignment(path: Path, name: str) -> object:
    """Read one literal module assignment without importing plotting code."""
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(target, ast.Name) and target.id == name
                for target in node.targets):
            return ast.literal_eval(node.value)
        if (isinstance(node, ast.AnnAssign)
                and isinstance(node.target, ast.Name)
                and node.target.id == name
                and node.value is not None):
            return ast.literal_eval(node.value)
    fail(f"missing literal assignment {name} in {path}")


def current_measurement_records(tools_directory: Path) \
        -> List[Dict[str, str]]:
    """Derive the current manifest from each owning measurement script."""
    records: List[Dict[str, str]] = []
    quantizers = literal_assignment(
        tools_directory / "plot_quantize_model_measurements.py",
        "QUANTIZE_CONFIGS",
    )
    dithers = literal_assignment(
        tools_directory / "plot_dither_policy_measurements.py",
        "DITHER_METHODS",
    )
    lookups = literal_assignment(
        tools_directory / "plot_lookup_policy_speed.py",
        "DEFAULT_POLICIES",
    )
    pipelines = literal_assignment(
        tools_directory / "plot_palette_pipeline_measurements.py",
        "PIPELINE_CONFIGS",
    )
    colorspaces = literal_assignment(
        tools_directory / "plot_clustering_colorspace_measurements.py",
        "COLORSPACES",
    )
    if not all(isinstance(value, Sequence)
               for value in (quantizers, dithers, pipelines, colorspaces)):
        fail("an owning measurement manifest is not a sequence")
    if not isinstance(lookups, str):
        fail("the owning lookup-policy manifest is not a string")
    for config, label, family, option in quantizers:
        records.append(
            {
                "domain": "quantizer",
                "config": str(config),
                "label": (
                    f"{family} / {label}"
                    if family == "heckbert" else str(label)
                ),
                "option": str(option),
            }
        )
    for method, option in dithers:
        records.append(
            {
                "domain": "dither",
                "config": str(method),
                "label": str(method),
                "option": str(option),
            }
        )
    for policy in lookups.split(","):
        records.append(
            {
                "domain": "lookup",
                "config": policy,
                "label": policy,
                "option": policy,
            }
        )
    for config, sampling, binning in pipelines:
        records.append(
            {
                "domain": "palette-pipeline",
                "config": str(config),
                "label": str(config),
                "option": f"sampling={sampling};binning={binning}",
            }
        )
    for colorspace, label, _contract_value in colorspaces:
        records.append(
            {
                "domain": "clustering-colorspace",
                "config": str(colorspace),
                "label": str(label),
                "option": f"-X{colorspace}",
            }
        )
    return records


def validate_png(path: Path) -> None:
    """Validate one nontrivial PNG artifact."""
    if not path.is_file() or path.stat().st_size < 1024:
        fail(f"missing or unexpectedly small plot: {path}")
    with path.open("rb") as handle:
        if handle.read(8) != b"\x89PNG\r\n\x1a\n":
            fail(f"invalid PNG signature: {path}")


def load_metadata(path: Path,
                  expected_records: Sequence[Dict[str, str]]) \
        -> Dict[str, object]:
    """Read and validate the stable metadata contract."""
    if not path.is_file():
        fail(f"missing precision metadata: {path}")
    with path.open("r", encoding="utf-8") as handle:
        metadata = json.load(handle)
    if int(metadata.get("schema_version", 0)) != 1:
        fail("precision metadata schema is not version 1")
    source = metadata.get("source", {})
    protocol = metadata.get("protocol", {})
    artifacts = metadata.get("artifacts", {})
    if not isinstance(source, dict) or not isinstance(protocol, dict):
        fail("precision metadata is incomplete")
    if not isinstance(artifacts, dict):
        fail("precision metadata artifact manifest is missing")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("precision measurements were not recorded from a clean tree")
    if not source.get("revision"):
        fail("precision metadata has no source revision")
    if protocol.get("colors") != PALETTE_COLORS:
        fail("precision metadata has an unexpected palette size")
    if protocol.get("domains") != list(expected_records):
        fail("precision metadata configuration manifest is stale")
    expected_precisions = [
        {
            "precision": precision,
            "label": label,
            "required_work_format": work_format,
        }
        for precision, label, work_format in PRECISIONS
    ]
    if protocol.get("precisions") != expected_precisions:
        fail("precision metadata work-format contract is stale")
    if (protocol.get("threads") != 1
            or protocol.get("loader") != "builtin!"
            or protocol.get("working_colorspace") != "gamma"
            or protocol.get("gpu_policy") != "off"):
        fail("precision metadata controlled axes are stale")
    if not protocol.get("sixel_environment_removed"):
        fail("precision metadata does not confirm SIXEL_* isolation")
    if (int(protocol.get("speed_warmups", -1)) < 0
            or int(protocol.get("speed_runs", 0)) < 1):
        fail("precision metadata has invalid timing counts")
    if artifacts.get("plots") != [
            f"precision-{domain}.png" for domain in DOMAIN_ORDER]:
        fail("precision metadata plot manifest is stale")
    return metadata


def require_command_contract(row: Dict[str, str]) -> None:
    """Validate the common controlled command tokens."""
    command = shlex.split(row["command"])
    required = (
        "{img2sixel}",
        "--threads=1",
        f"--precision={row['precision']}",
        "--quality=full",
        "--loaders=builtin!",
        "-Wgamma",
        "--gpu-policy=off",
        "-p",
        str(PALETTE_COLORS),
        "{input}",
    )
    if not all(token in command for token in required):
        fail(
            "precision CSV command violates its controlled contract: "
            f"{row['domain']}/{row['config']}/{row['precision']}"
        )
    if "-v" in command or "-o" in command:
        fail("quality command unexpectedly enables diagnostics or discards output")
    speed_command = shlex.split(row["speed_command"])
    if ("-o" not in speed_command
            or "{devnull}" not in speed_command
            or "-v" in speed_command):
        fail("speed command does not discard output without instrumentation")
    assessment = shlex.split(row["assessment_command"])
    if assessment != ["{lsqa}", "{input}", "-"]:
        fail("precision CSV has an unexpected assessment command")


def validate_rows(path: Path,
                  metadata: Dict[str, object],
                  expected_records: Sequence[Dict[str, str]]) \
        -> Dict[Tuple[str, str], Dict[str, Dict[str, str]]]:
    """Validate CSV coverage, values, commands, and effective formats."""
    if not path.is_file():
        fail(f"missing precision CSV: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    expected_count = len(expected_records) * len(PRECISIONS)
    if len(rows) != expected_count:
        fail(f"precision CSV has {len(rows)} rows, expected {expected_count}")
    artifacts = metadata["artifacts"]
    source = metadata["source"]
    protocol = metadata["protocol"]
    if not isinstance(artifacts, dict):
        fail("precision metadata artifact manifest is invalid")
    if not isinstance(source, dict) or not isinstance(protocol, dict):
        fail("precision metadata source or protocol is invalid")
    if int(artifacts["row_count"]) != expected_count:
        fail("precision metadata row count is stale")
    revision = str(source["revision"])
    runs = int(protocol["speed_runs"])
    expected_formats = {
        precision: work_format
        for precision, _label, work_format in PRECISIONS
    }
    pairs: Dict[Tuple[str, str], Dict[str, Dict[str, str]]] = {}
    for row in rows:
        key = (row["domain"], row["config"])
        precision = row["precision"]
        if precision not in expected_formats:
            fail(f"unknown precision in CSV: {precision}")
        if precision in pairs.setdefault(key, {}):
            fail(f"duplicate precision CSV row: {key}/{precision}")
        pairs[key][precision] = row
        if row["revision"] != revision:
            fail(f"precision CSV revision differs from metadata: {key}")
        if int(row["colors"]) != PALETTE_COLORS:
            fail(f"precision CSV has an unexpected palette size: {key}")
        if row["work_format"] != expected_formats[precision]:
            fail(f"precision CSV has an unexpected work format: {key}")
        if int(row["preflight_quantizer_retries"]) != 0:
            fail(f"precision CSV records a quantizer fallback: {key}")
        if int(row["speed_runs"]) != runs:
            fail(f"precision CSV timing count differs from metadata: {key}")
        ms_ssim = float(row["MS-SSIM"])
        delta_e = float(row["Delta E00_mean"])
        encoded_bytes = int(row["encoded_bytes"])
        minimum = float(row["min_seconds"])
        q1 = float(row["q1_seconds"])
        median = float(row["median_seconds"])
        q3 = float(row["q3_seconds"])
        maximum = float(row["max_seconds"])
        if not 0.0 <= ms_ssim <= 1.0 or delta_e < 0.0:
            fail(f"precision CSV quality value is out of range: {key}")
        if encoded_bytes <= 0:
            fail(f"precision CSV stream size is not positive: {key}")
        if not 0.0 < minimum <= q1 <= median <= q3 <= maximum:
            fail(f"precision CSV timing summary is inconsistent: {key}")
        require_command_contract(row)
    expected_keys = {
        (record["domain"], record["config"])
        for record in expected_records
    }
    if set(pairs) != expected_keys:
        fail("precision CSV configuration coverage is stale")
    if any(set(pair) != set(expected_formats) for pair in pairs.values()):
        fail("precision CSV does not contain complete precision pairs")
    return pairs


def close(left: float, right: float) -> bool:
    """Compare a derived CSV value with a tight relative tolerance."""
    return math.isclose(left, right, rel_tol=1e-10, abs_tol=1e-12)


def validate_pair_comparisons(
        pairs: Dict[Tuple[str, str], Dict[str, Dict[str, str]]]) -> None:
    """Recompute every value relative to the matching 8-bit result."""
    for key, pair in pairs.items():
        baseline = pair["8bit"]
        for row in pair.values():
            expected = float(row["MS-SSIM"]) - float(baseline["MS-SSIM"])
            if not close(float(row["MS-SSIM_delta_vs_8bit"]), expected):
                fail(f"stale MS-SSIM delta: {key}/{row['precision']}")
            expected = (
                float(row["Delta E00_mean"])
                - float(baseline["Delta E00_mean"])
            )
            if not close(float(row["Delta E00_delta_vs_8bit"]), expected):
                fail(f"stale Delta E00 delta: {key}/{row['precision']}")
            expected = (
                float(row["encoded_bytes"])
                / float(baseline["encoded_bytes"])
            )
            if not close(float(row["encoded_size_ratio_vs_8bit"]), expected):
                fail(f"stale stream-size ratio: {key}/{row['precision']}")
            expected = (
                float(row["median_seconds"])
                / float(baseline["median_seconds"])
            )
            if not close(float(row["speed_ratio_vs_8bit"]), expected):
                fail(f"stale speed ratio: {key}/{row['precision']}")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    return parser.parse_args()


def main() -> int:
    """Validate one complete precision measurement directory."""
    args = parse_args()
    directory = args.directory.resolve()
    tools_directory = Path(__file__).resolve().parent
    expected_records = current_measurement_records(tools_directory)
    metadata = load_metadata(
        directory / "precision-run.json",
        expected_records,
    )
    pairs = validate_rows(
        directory / "precision-comparison.csv",
        metadata,
        expected_records,
    )
    validate_pair_comparisons(pairs)
    for domain in DOMAIN_ORDER:
        validate_png(directory / f"precision-{domain}.png")
    print("precision measurement artifacts are consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
