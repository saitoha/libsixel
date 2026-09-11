#!/usr/bin/env python3
"""Validate checked-in crop/resize measurement and figure artifacts."""

from __future__ import annotations

import argparse
import ast
import csv
import json
import math
import shlex
import statistics
import struct
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


PLOTTER = Path(__file__).with_name("plot_crop_resize_measurements.py")


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


POLICIES = tuple(literal_assignment(PLOTTER, "POLICIES"))
SIMD_MODES = tuple(literal_assignment(PLOTTER, "SIMD_MODES"))
VISUAL_WIDTH = int(literal_assignment(PLOTTER, "VISUAL_WIDTH"))
VISUAL_HEIGHT = int(literal_assignment(PLOTTER, "VISUAL_HEIGHT"))
ZOOM_WIDTH = int(literal_assignment(PLOTTER, "ZOOM_WIDTH"))
ZOOM_HEIGHT = int(literal_assignment(PLOTTER, "ZOOM_HEIGHT"))
ZOOM_SCALE = int(literal_assignment(PLOTTER, "ZOOM_SCALE"))
CHECKER_WIDTH = int(literal_assignment(PLOTTER, "CHECKER_WIDTH"))
CHECKER_HEIGHT = int(literal_assignment(PLOTTER, "CHECKER_HEIGHT"))
BOUNDARY_WIDTH = int(literal_assignment(PLOTTER, "BOUNDARY_WIDTH"))
BOUNDARY_HEIGHT = int(literal_assignment(PLOTTER, "BOUNDARY_HEIGHT"))
BOUNDARY_SAMPLE_X = int(literal_assignment(PLOTTER, "BOUNDARY_SAMPLE_X"))
BOUNDARY_ROWS = tuple(literal_assignment(PLOTTER, "BOUNDARY_ROWS"))
SPEED_SOURCE_WIDTH = int(
    literal_assignment(PLOTTER, "SPEED_SOURCE_WIDTH")
)
SPEED_SOURCE_HEIGHT = int(
    literal_assignment(PLOTTER, "SPEED_SOURCE_HEIGHT")
)
SPEED_TARGET_WIDTH = int(
    literal_assignment(PLOTTER, "SPEED_TARGET_WIDTH")
)
SPEED_TARGET_HEIGHT = int(
    literal_assignment(PLOTTER, "SPEED_TARGET_HEIGHT")
)
PALETTE_COLORS = int(literal_assignment(PLOTTER, "PALETTE_COLORS"))


MEASUREMENT_PNGS = (
    "crop-resize-boundary-input.png",
    "crop-resize-boundary-preserve.png",
    "crop-resize-boundary-linear.png",
    "crop-resize-boundary-float.png",
    "crop-resize-visual-reference-linear-scalar.png",
    "crop-resize-visual-preserve-auto.png",
    "crop-resize-visual-linear-auto.png",
    "crop-resize-visual-float-auto.png",
    "crop-resize-visual-reference-linear-scalar-zoom.png",
    "crop-resize-visual-preserve-auto-zoom.png",
    "crop-resize-visual-linear-auto-zoom.png",
    "crop-resize-visual-float-auto-zoom.png",
    "crop-resize-quality-natural-ms-ssim.png",
    "crop-resize-quality-natural-delta-e00.png",
    "crop-resize-quality-checker-ms-ssim.png",
    "crop-resize-quality-checker-delta-e00.png",
    "crop-resize-speed.png",
    "crop-resize-size.png",
)
MEASUREMENT_CSVS = (
    "crop-resize-quality.csv",
    "crop-resize-size.csv",
    "crop-resize-speed.csv",
    "crop-resize-boundary-samples.csv",
)
RESAMPLING_PNGS = (
    "resampling-kernels-radius-1.png",
    "resampling-kernels-wide.png",
    "resampling-coordinate-mapping.png",
)
POLICY_SVGS = (
    "crop-resize-policy-path-wide.svg",
    "crop-resize-policy-path-mobile.svg",
)


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


def png_size(path: Path) -> Tuple[int, int]:
    """Read PNG dimensions directly from the IHDR chunk."""
    if not path.is_file():
        fail(f"missing PNG: {path}")
    with path.open("rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"invalid PNG signature: {path}")
    if header[12:16] != b"IHDR":
        fail(f"PNG does not begin with IHDR: {path}")
    return struct.unpack(">II", header[16:24])


def validate_png(path: Path, expected: Optional[Tuple[int, int]] = None,
                 minimum: Optional[Tuple[int, int]] = None) -> None:
    """Validate one PNG signature, dimensions, and nontrivial size."""
    width, height = png_size(path)
    if expected is not None and (width, height) != expected:
        fail(f"unexpected PNG size for {path}: {(width, height)}")
    if minimum is not None and (width < minimum[0] or height < minimum[1]):
        fail(f"unexpectedly small plot for {path}: {(width, height)}")
    if path.stat().st_size < 100:
        fail(f"unexpectedly small PNG file: {path}")


def validate_svg(path: Path) -> None:
    """Validate the policy diagram's required semantics."""
    if not path.is_file():
        fail(f"missing policy SVG: {path}")
    text = path.read_text(encoding="utf-8")
    required = (
        "<svg", "<title", "<desc", "preserve", "linear", "float",
        "Resampling", "SIMD ceiling", "Palette / SIXEL",
    )
    if not all(token in text for token in required):
        fail(f"policy SVG is missing required path semantics: {path}")


def validate_command(command_text: str, input_token: str) -> None:
    """Validate controls shared by all measured encoder commands."""
    command = shlex.split(command_text)
    required = (
        "{img2sixel}",
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        "-Fnone",
        "-aoff",
        "-Xgamma",
        "-Wgamma",
        "-Ugamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "--encode-policy=fast",
        f"-p{PALETTE_COLORS}",
        "-rbilinear",
        "-o",
        "{output}",
        input_token,
    )
    if not all(token in command for token in required):
        fail(f"measurement command violates controlled policy: {command_text}")
    if "-v" in command:
        fail("recorded command unexpectedly enables diagnostics")


def validate_policy_runtime(command_text: str, policy: str, simd: str) -> None:
    """Validate one combined SIMD/resize policy argument."""
    command = shlex.split(command_text)
    expected = f"-j{simd}:resize_precision={policy}"
    runtime = [token for token in command if token.startswith("-j")]
    if runtime != [expected]:
        fail(f"runtime policy token changed: {runtime} != {[expected]}")


def finite(row: Dict[str, str], fields: Sequence[str]) -> None:
    """Validate finite numeric fields."""
    for field in fields:
        value = float(row[field])
        if not math.isfinite(value):
            fail(f"non-finite {field}: {row}")


def expected_cases() -> List[Tuple[str, str]]:
    """Return the policy/SIMD matrix in recording order."""
    return [
        (str(policy[0]), simd)
        for policy in POLICIES
        for simd in SIMD_MODES
    ]


def validate_quality(rows: Sequence[Dict[str, str]]) -> None:
    """Validate quality rows, command controls, and semantic direction."""
    expected = [
        (fixture, policy, simd)
        for fixture in ("natural", "checker")
        for policy, simd in expected_cases()
    ]
    observed = [
        (row["fixture"], row["policy"], row["simd_requested"])
        for row in rows
    ]
    if observed != expected:
        fail("quality rows do not match the full fixture/policy/SIMD matrix")
    by_key = {
        (row["fixture"], row["policy"], row["simd_requested"]): row
        for row in rows
    }
    for row in rows:
        validate_command(row["command"], row["input_token"])
        validate_policy_runtime(
            row["command"], row["policy"], row["simd_requested"]
        )
        finite(
            row,
            ("ms_ssim", "delta_e00_mean", "delta_chroma_mean",
             "gmsd", "psnr_y"),
        )
        if len(row["output_sha256"]) != 64:
            fail("quality row has an invalid SIXEL digest")
        if len(row["decoded_png_sha256"]) != 64:
            fail("quality row has an invalid PNG digest")
        output_token = "{output_png}"
        if (row["fixture"] == "natural"
                and row["policy"] == "linear"
                and row["simd_requested"] == "scalar"):
            output_token = "{natural_reference}"
        expected_assessment = shlex.join(
            (
                "{lsqa}",
                "-Woklab",
                "-Pfloat32",
                "{natural_reference}"
                if row["fixture"] == "natural"
                else "{checker_reference}",
                output_token,
            )
        )
        if row["assessment_command"] != expected_assessment:
            fail("quality assessment command changed")

    natural_reference = by_key[("natural", "linear", "scalar")]
    if (abs(float(natural_reference["ms_ssim"]) - 1.0) > 0.000001
            or abs(float(natural_reference["delta_e00_mean"])) > 0.000001):
        fail("natural linear-scalar reference is not self-identical")
    for simd in SIMD_MODES:
        preserve = by_key[("checker", "preserve", simd)]
        for policy in ("linear", "float"):
            candidate = by_key[("checker", policy, simd)]
            if float(candidate["ms_ssim"]) <= float(preserve["ms_ssim"]):
                fail("linear-light checker MS-SSIM did not beat preserve")
            if (float(candidate["delta_e00_mean"])
                    >= float(preserve["delta_e00_mean"])):
                fail("linear-light checker Delta E00 did not beat preserve")


def validate_sizes(rows: Sequence[Dict[str, str]],
                   quality: Sequence[Dict[str, str]]) -> None:
    """Validate exact stream sizes and cross-CSV digests."""
    observed = [(row["policy"], row["simd_requested"]) for row in rows]
    if observed != expected_cases():
        fail("size rows do not match the policy/SIMD matrix")
    quality_by_key = {
        (row["policy"], row["simd_requested"]): row
        for row in quality if row["fixture"] == "natural"
    }
    for row in rows:
        validate_command(row["command"], "{input}")
        validate_policy_runtime(
            row["command"], row["policy"], row["simd_requested"]
        )
        if int(row["sixel_bytes"]) <= 0:
            fail("size row has a non-positive stream size")
        if row["output_sha256"] != quality_by_key[
                (row["policy"], row["simd_requested"])]["output_sha256"]:
            fail("size and quality CSV digests disagree")


def validate_speed(rows: Sequence[Dict[str, str]], runs: int) -> None:
    """Validate timing samples and their summaries."""
    observed = [(row["policy"], row["simd_requested"]) for row in rows]
    if observed != expected_cases():
        fail("speed rows do not match the policy/SIMD matrix")
    for row in rows:
        validate_command(row["command"], "{speed_input}")
        validate_policy_runtime(
            row["command"], row["policy"], row["simd_requested"]
        )
        command = shlex.split(row["command"])
        for token in (
                f"-w{SPEED_TARGET_WIDTH}",
                f"-h{SPEED_TARGET_HEIGHT}"):
            if token not in command:
                fail("speed command target geometry changed")
        samples = [float(value) for value in row["samples_ms"].split(";")]
        if len(samples) != runs or any(value <= 0.0 for value in samples):
            fail("speed sample count or value is invalid")
        finite(
            row,
            ("median_ms", "q1_ms", "q3_ms", "minimum_ms", "maximum_ms"),
        )
        if abs(float(row["median_ms"]) - statistics.median(samples)) > 0.000001:
            fail("recorded speed median disagrees with samples")
        if not (
                float(row["minimum_ms"]) <= float(row["q1_ms"])
                <= float(row["median_ms"]) <= float(row["q3_ms"])
                <= float(row["maximum_ms"])):
            fail("speed summary ordering is invalid")


def parse_rgb(value: str) -> Tuple[int, int, int]:
    """Parse one slash-delimited RGB sample."""
    parts = tuple(int(item) for item in value.split("/"))
    if len(parts) != 3 or any(item < 0 or item > 255 for item in parts):
        fail(f"invalid RGB sample: {value}")
    return parts


def validate_boundaries(rows: Sequence[Dict[str, str]]) -> None:
    """Validate few-color boundary samples and their quality distinction."""
    expected = [
        (str(boundary[0]), str(policy[0]))
        for policy in POLICIES
        for boundary in BOUNDARY_ROWS
    ]
    observed = [(row["boundary"], row["policy"]) for row in rows]
    if observed != expected:
        fail("boundary rows do not match the policy/boundary matrix")
    by_key = {(row["boundary"], row["policy"]): row for row in rows}
    for row in rows:
        validate_command(row["command"], "{boundary_input}")
        validate_policy_runtime(row["command"], row["policy"], "auto")
        command = shlex.split(row["command"])
        if (f"-w{BOUNDARY_WIDTH}" not in command
                or f"-h{BOUNDARY_HEIGHT}" not in command):
            fail("boundary command geometry changed")
        if int(row["sample_x"]) != BOUNDARY_SAMPLE_X:
            fail("boundary sample x coordinate changed")
        parse_rgb(row["source_left_rgb"])
        parse_rgb(row["source_right_rgb"])
        parse_rgb(row["output_midpoint_rgb"])
    preserve_gray = parse_rgb(
        by_key[("black-white", "preserve")]["output_midpoint_rgb"]
    )
    linear_gray = parse_rgb(
        by_key[("black-white", "linear")]["output_midpoint_rgb"]
    )
    float_gray = parse_rgb(
        by_key[("black-white", "float")]["output_midpoint_rgb"]
    )
    if linear_gray[0] - preserve_gray[0] < 40:
        fail("linear-light boundary did not remain visibly brighter")
    if max(abs(left - right) for left, right in zip(linear_gray, float_gray)) > 3:
        fail("linear and float boundary midpoints diverged unexpectedly")


def validate_metadata(path: Path) -> Dict[str, object]:
    """Validate provenance, protocol, and artifact manifests."""
    if not path.is_file():
        fail(f"missing metadata: {path}")
    metadata = json.loads(path.read_text(encoding="utf-8"))
    if metadata.get("schema_version") != 1:
        fail("metadata schema is not version 1")
    source = metadata.get("source")
    protocol = metadata.get("protocol")
    preflight = metadata.get("preflight")
    artifacts = metadata.get("artifacts")
    if not all(isinstance(value, dict) for value in (
            source, protocol, preflight, artifacts)):
        fail("metadata is missing source, protocol, preflight, or artifacts")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("durable measurements were not recorded from a clean worktree")
    if not source.get("revision") or len(source.get("input_sha256", "")) != 64:
        fail("measurement source provenance is incomplete")
    expected_protocol = {
        "policies": [str(record[0]) for record in POLICIES],
        "simd_modes": list(SIMD_MODES),
        "threads": 1,
        "resampling": "bilinear",
        "palette_colors": PALETTE_COLORS,
        "dither": "none",
        "gpu_policy": "off",
        "natural_source_size": [600, 450],
        "natural_target_size": [VISUAL_WIDTH, VISUAL_HEIGHT],
        "natural_reference": "linear-scalar post-SIXEL snapshot",
        "checker_source_size": [CHECKER_WIDTH, CHECKER_HEIGHT],
        "checker_target_size": [VISUAL_WIDTH, VISUAL_HEIGHT],
        "checker_reference_srgb_tone": 188,
        "comparison_colorspace": "oklab",
        "comparison_precision": "float32",
        "zoom_box": [
            int(literal_assignment(PLOTTER, "ZOOM_X")),
            int(literal_assignment(PLOTTER, "ZOOM_Y")),
            ZOOM_WIDTH,
            ZOOM_HEIGHT,
        ],
        "zoom_display_scale": ZOOM_SCALE,
        "boundary_source_size": [8, BOUNDARY_HEIGHT],
        "boundary_target_size": [BOUNDARY_WIDTH, BOUNDARY_HEIGHT],
        "boundary_sample_x": BOUNDARY_SAMPLE_X,
        "speed_source_size": [SPEED_SOURCE_WIDTH, SPEED_SOURCE_HEIGHT],
        "speed_target_size": [SPEED_TARGET_WIDTH, SPEED_TARGET_HEIGHT],
        "speed_warmups": protocol.get("speed_warmups"),
        "speed_runs": protocol.get("speed_runs"),
        "speed_case_order_seed": 1,
        "sixel_environment_removed": True,
    }
    if protocol != expected_protocol:
        fail("measurement protocol does not match the current manifest")
    expected_preflight = set(
        f"{policy}-{simd}" for policy, simd in expected_cases()
    )
    if set(preflight) != expected_preflight:
        fail("preflight matrix is incomplete")
    native_values = set()
    policy_by_name = {str(record[0]): record for record in POLICIES}
    for policy, simd in expected_cases():
        record = preflight[f"{policy}-{simd}"]
        expected_format = policy_by_name[policy]
        observed = (
            record["source_format"],
            record["work_format"],
            record["scale_output_format"],
            record["resize_mode"],
            record["resize_input_format"],
        )
        expected = (
            "rgb888", expected_format[3], expected_format[4],
            expected_format[2], expected_format[5],
        )
        if observed != expected:
            fail(f"preflight format path changed for {policy}-{simd}")
        native_values.add(int(record["simd_native"]))
        if simd == "scalar" and int(record["simd_effective"]) != 0:
            fail("scalar preflight did not resolve to scalar")
        if (simd == "auto" and int(record["simd_effective"])
                != int(record["simd_native"])):
            fail("auto preflight did not resolve to native")
    if len(native_values) != 1:
        fail("native SIMD level changed during one measurement run")
    expected_artifacts = sorted(MEASUREMENT_PNGS + MEASUREMENT_CSVS)
    if artifacts.get("measurement_directory") != expected_artifacts:
        fail("measurement artifact manifest is stale")
    if artifacts.get("resampling_figures") != sorted(RESAMPLING_PNGS):
        fail("resampling figure manifest is stale")
    return metadata


def validate_files(measurements: Path, policy_figures: Path,
                   resampling_figures: Path) -> None:
    """Validate all generated image and SVG files."""
    validate_png(
        measurements / "crop-resize-boundary-input.png",
        expected=(8, BOUNDARY_HEIGHT),
    )
    for policy in ("preserve", "linear", "float"):
        validate_png(
            measurements / f"crop-resize-boundary-{policy}.png",
            expected=(BOUNDARY_WIDTH, BOUNDARY_HEIGHT),
        )
    for name in (
            "reference-linear-scalar", "preserve-auto", "linear-auto",
            "float-auto"):
        validate_png(
            measurements / f"crop-resize-visual-{name}.png",
            expected=(VISUAL_WIDTH, VISUAL_HEIGHT),
        )
        validate_png(
            measurements / f"crop-resize-visual-{name}-zoom.png",
            expected=(ZOOM_WIDTH * ZOOM_SCALE, ZOOM_HEIGHT * ZOOM_SCALE),
        )
    for name in MEASUREMENT_PNGS[12:]:
        validate_png(measurements / name, minimum=(900, 500))
    for name in RESAMPLING_PNGS:
        validate_png(resampling_figures / name, minimum=(900, 500))
    for name in POLICY_SVGS:
        validate_svg(policy_figures / name)


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument("measurement_directory", type=Path)
    parser.add_argument("policy_figure_directory", type=Path)
    parser.add_argument("resampling_figure_directory", type=Path)
    return parser.parse_args()


def main() -> None:
    """Validate all durable crop/resize documentation artifacts."""
    args = parse_args()
    metadata = validate_metadata(
        args.measurement_directory / "crop-resize-run.json"
    )
    protocol = metadata["protocol"]
    quality = read_csv(
        args.measurement_directory / "crop-resize-quality.csv"
    )
    sizes = read_csv(args.measurement_directory / "crop-resize-size.csv")
    speed = read_csv(args.measurement_directory / "crop-resize-speed.csv")
    boundaries = read_csv(
        args.measurement_directory / "crop-resize-boundary-samples.csv"
    )
    validate_quality(quality)
    validate_sizes(sizes, quality)
    validate_speed(speed, int(protocol["speed_runs"]))
    validate_boundaries(boundaries)
    validate_files(
        args.measurement_directory,
        args.policy_figure_directory,
        args.resampling_figure_directory,
    )


if __name__ == "__main__":
    main()
