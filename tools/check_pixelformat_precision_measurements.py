#!/usr/bin/env python3
"""Validate checked-in pixel-format precision measurements."""

from __future__ import annotations

import argparse
import ast
import csv
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Sequence


def literal_assignment(path: Path, name: str) -> object:
    """Read one literal assignment without importing plotting dependencies."""
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
    raise ValueError(f"missing literal assignment {name} in {path}")


PLOTTER = Path(__file__).with_name(
    "plot_pixelformat_precision_measurements.py"
)
CHECKER_WIDTH = int(literal_assignment(PLOTTER, "CHECKER_WIDTH"))
CHECKER_HEIGHT = int(literal_assignment(PLOTTER, "CHECKER_HEIGHT"))
OUTPUT_WIDTH = int(literal_assignment(PLOTTER, "OUTPUT_WIDTH"))
OUTPUT_HEIGHT = int(literal_assignment(PLOTTER, "OUTPUT_HEIGHT"))
PALETTE_COLORS = int(literal_assignment(PLOTTER, "PALETTE_COLORS"))
PATH_CASES = tuple(literal_assignment(PLOTTER, "PATH_CASES"))
RESIZE_CASES = tuple(literal_assignment(PLOTTER, "RESIZE_CASES"))


EXPECTED_FORMATS = {
    "gamma-8bit": ("rgb888", "rgb888", "rgb888", "1", "rgb888"),
    "gamma-float32": ("rgb888", "rgb-f32", "rgb888", "3", "rgb888"),
    "working-linear-8bit": ("rgb888", "linear-f32", "rgb888", "3", "rgb888"),
    "working-linear-float32": ("rgb888", "linear-f32", "rgb888", "3", "rgb888"),
    "working-oklab-8bit": ("rgb888", "oklab-f32", "rgb888", "3", "rgb888"),
    "working-oklab-float32": ("rgb888", "oklab-f32", "rgb888", "3", "rgb888"),
    "cms-gamma-byte": ("rgb888", "rgb888", "rgb888", "1", "rgb888"),
    "cms-gamma-float": ("rgb-f32", "rgb-f32", "rgb-f32", "3", "rgb-f32"),
    "cms-linear-byte": ("rgb888", "linear-f32", "rgb888", "3", "rgb888"),
    "cms-linear-float": ("linear-f32", "linear-f32", "linear-f32", "3", "linear-f32"),
    "cms-oklab-8bit": ("oklab-f32", "oklab-f32", "oklab-f32", "3", "oklab-f32"),
    "cms-oklab-float32": ("oklab-f32", "oklab-f32", "oklab-f32", "3", "oklab-f32"),
}


def fail(message: str) -> None:
    """Raise one consistently formatted validation failure."""
    raise ValueError(message)


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read one required non-empty CSV."""
    if not path.is_file():
        fail(f"missing measurement CSV: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"measurement CSV is empty: {path}")
    return rows


def validate_png(path: Path) -> None:
    """Validate one nontrivial PNG plot."""
    if not path.is_file() or path.stat().st_size < 20000:
        fail(f"missing or unexpectedly small plot: {path}")
    with path.open("rb") as handle:
        if handle.read(8) != b"\x89PNG\r\n\x1a\n":
            fail(f"invalid PNG signature: {path}")


def validate_metadata(path: Path) -> Dict[str, object]:
    """Validate source provenance and the stable protocol."""
    if not path.is_file():
        fail(f"missing measurement metadata: {path}")
    metadata = json.loads(path.read_text(encoding="utf-8"))
    if metadata.get("schema_version") != 1:
        fail("metadata schema is not version 1")
    source = metadata.get("source")
    protocol = metadata.get("protocol")
    artifacts = metadata.get("artifacts")
    if not isinstance(source, dict) or not isinstance(protocol, dict):
        fail("metadata source or protocol is missing")
    if not isinstance(artifacts, dict):
        fail("metadata artifact manifest is missing")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("durable measurements were not recorded from a clean worktree")
    if not source.get("revision"):
        fail("measurement source revision is missing")
    expected_protocol = {
        "checker_size": [CHECKER_WIDTH, CHECKER_HEIGHT],
        "output_size": [OUTPUT_WIDTH, OUTPUT_HEIGHT],
        "reference_srgb_tone": 188,
        "resampling": "bilinear",
        "colors": PALETTE_COLORS,
        "dither": "none",
        "threads": 1,
        "comparison_colorspace": "oklab",
        "comparison_precision": "float32",
        "path_cases": [list(record) for record in PATH_CASES],
        "resize_cases": [list(record) for record in RESIZE_CASES],
        "sixel_environment_removed": True,
    }
    if protocol != expected_protocol:
        fail("measurement protocol does not match the current manifest")
    if artifacts != {
        "paths_csv": "pixelformat-precision-paths.csv",
        "resize_csv": "pixelformat-precision-resize.csv",
        "plot": "pixelformat-precision-resize.png",
    }:
        fail("metadata artifact manifest is stale")
    return metadata


def validate_command(command_text: str) -> None:
    """Validate controls shared by path and resize commands."""
    command = shlex.split(command_text)
    required = (
        "{img2sixel}",
        "--threads=1",
        "--quality=full",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        "-Fnone",
        "-aoff",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "-p",
        str(PALETTE_COLORS),
        "-o",
        "{output}",
        "{input}",
    )
    if not all(token in command for token in required):
        fail(f"measurement command violates the controlled contract: {command_text}")
    if "-v" in command:
        fail("recorded measurement command unexpectedly enables diagnostics")


def validate_paths(rows: Sequence[Dict[str, str]]) -> None:
    """Validate requested/effective pixel-format transitions."""
    expected_ids = [record[0] for record in PATH_CASES]
    if [row.get("case") for row in rows] != expected_ids:
        fail("path measurement rows do not match PATH_CASES")
    by_id = {row["case"]: row for row in rows}
    for row in rows:
        validate_command(row["command"])
        expected = EXPECTED_FORMATS[row["case"]]
        observed = (
            row["source_format"],
            row["work_format"],
            row["scale_output_format"],
            row["resize_mode"],
            row["resize_input_format"],
        )
        if observed != expected:
            fail(f"effective format contract changed for {row['case']}: {observed}")
        if len(row["output_sha256"]) != 64 or int(row["sixel_bytes"]) <= 0:
            fail(f"invalid output record for {row['case']}")
    for left, right in (
        ("working-linear-8bit", "working-linear-float32"),
        ("working-oklab-8bit", "working-oklab-float32"),
        ("cms-oklab-8bit", "cms-oklab-float32"),
    ):
        if by_id[left]["output_sha256"] != by_id[right]["output_sha256"]:
            fail(f"implicit and explicit float32 outputs differ: {left}, {right}")
    cms_contracts = {
        "cms-gamma-byte": ("gamma", "1", "rgb888"),
        "cms-gamma-float": ("gamma", "0", "rgbfloat32"),
        "cms-linear-byte": ("linear", "1", "rgb888"),
        "cms-linear-float": ("linear", "0", "linearrgbfloat32"),
        "cms-oklab-8bit": ("oklab", "0", "oklabfloat32"),
        "cms-oklab-float32": ("oklab", "0", "oklabfloat32"),
    }
    for case_id, expected in cms_contracts.items():
        row = by_id[case_id]
        observed = (
            row["loader_cms_target"],
            row["loader_prefer_8bit"],
            row["loader_pixelformat"],
        )
        if observed != expected:
            fail(f"loader CMS contract changed for {case_id}: {observed}")


def validate_resize(rows: Sequence[Dict[str, str]]) -> None:
    """Validate the resize comparison and quality direction."""
    expected_modes = [record[0] for record in RESIZE_CASES]
    if [row.get("mode") for row in rows] != expected_modes:
        fail("resize measurement rows do not match RESIZE_CASES")
    expected_formats = {
        "preserve": ("rgb888", "rgb888", "rgb888", "1", "rgb888"),
        "auto": ("rgb888", "rgb888", "linear-f32", "2", "linear-f32"),
        "float": ("rgb888", "rgb-f32", "linear-f32", "3", "linear-f32"),
    }
    by_mode = {row["mode"]: row for row in rows}
    for row in rows:
        validate_command(row["command"])
        expected = expected_formats[row["mode"]]
        observed = (
            row["source_format"],
            row["work_format"],
            row["scale_output_format"],
            row["resize_mode"],
            row["resize_input_format"],
        )
        if observed != expected:
            fail(f"resize format contract changed for {row['mode']}: {observed}")
        command = shlex.split(row["command"])
        if ("--precision=8bit" not in command
                or "-w50%" not in command
                or "-rbilinear" not in command):
            fail(f"resize command is not controlled for {row['mode']}")
        override = next(
            item[2] for item in RESIZE_CASES if item[0] == row["mode"]
        )
        resize_tokens = [
            token for token in command
            if token.startswith("-jauto:resize_precision=")
        ]
        expected_tokens = (
            [f"-jauto:resize_precision={override}"] if override else []
        )
        if resize_tokens != expected_tokens:
            fail(f"resize override is stale for {row['mode']}")
        if row["assessment_command"] != (
                "{lsqa} -Woklab -Pfloat32 {reference} {output}"):
            fail("resize assessment command is stale")
        for field in (
                "ms_ssim", "delta_e00_mean", "delta_chroma_mean",
                "gmsd", "psnr_y"):
            value = float(row[field])
            if not math.isfinite(value):
                fail(f"non-finite resize metric {field} for {row['mode']}")
    preserve_ms = float(by_mode["preserve"]["ms_ssim"])
    preserve_de = float(by_mode["preserve"]["delta_e00_mean"])
    for mode in ("auto", "float"):
        if float(by_mode[mode]["ms_ssim"]) < preserve_ms:
            fail(f"linear-light resize has lower MS-SSIM than preserve: {mode}")
        if float(by_mode[mode]["delta_e00_mean"]) > preserve_de:
            fail(f"linear-light resize has higher Delta E00 than preserve: {mode}")
    if by_mode["auto"]["output_sha256"] != by_mode["float"]["output_sha256"]:
        fail("transient and retained float32 resize outputs differ on the fixture")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("measurement_directory")
    return parser.parse_args()


def main() -> int:
    """Validate all checked-in precision artifacts."""
    args = parse_args()
    directory = Path(args.measurement_directory)
    metadata = validate_metadata(directory / "pixelformat-precision-run.json")
    paths = read_csv(directory / "pixelformat-precision-paths.csv")
    resize = read_csv(directory / "pixelformat-precision-resize.csv")
    validate_png(directory / "pixelformat-precision-resize.png")
    validate_paths(paths)
    validate_resize(resize)
    if not isinstance(metadata.get("programs"), dict):
        fail("measurement program provenance is missing")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
