#!/usr/bin/env python3
"""Validate checked-in resampling measurements and raster figures."""

from __future__ import annotations

import argparse
import ast
import csv
import hashlib
import json
import math
import shlex
import statistics
import struct
from pathlib import Path
from typing import Dict, List, Mapping, Optional, Sequence, Tuple


PLOTTER = Path(__file__).with_name("plot_resampling_measurements.py")


def literal_assignment(path: Path, name: str) -> object:
    """Read one literal assignment without importing plotting dependencies."""
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
            isinstance(target, ast.Name) and target.id == name
            for target in node.targets
        ):
            return ast.literal_eval(node.value)
        if (
            isinstance(node, ast.AnnAssign)
            and isinstance(node.target, ast.Name)
            and node.target.id == name
            and node.value is not None
        ):
            return ast.literal_eval(node.value)
    raise ValueError(f"missing literal assignment {name} in {path}")


METHODS = tuple(literal_assignment(PLOTTER, "METHODS"))
METHOD_NAMES = tuple(str(method[0]) for method in METHODS)
QUALITY_TARGET_WIDTH = int(literal_assignment(PLOTTER, "QUALITY_TARGET_WIDTH"))
QUALITY_TARGET_HEIGHT = int(literal_assignment(PLOTTER, "QUALITY_TARGET_HEIGHT"))
ZOOM_WIDTH = int(literal_assignment(PLOTTER, "ZOOM_WIDTH"))
ZOOM_HEIGHT = int(literal_assignment(PLOTTER, "ZOOM_HEIGHT"))
ZOOM_SCALE = int(literal_assignment(PLOTTER, "ZOOM_SCALE"))
SPEED_SOURCE_WIDTH = int(literal_assignment(PLOTTER, "SPEED_SOURCE_WIDTH"))
SPEED_SOURCE_HEIGHT = int(literal_assignment(PLOTTER, "SPEED_SOURCE_HEIGHT"))
SPEED_TARGET_WIDTH = int(literal_assignment(PLOTTER, "SPEED_TARGET_WIDTH"))
SPEED_TARGET_HEIGHT = int(literal_assignment(PLOTTER, "SPEED_TARGET_HEIGHT"))
MOIRE_SOURCE_SIZE = int(literal_assignment(PLOTTER, "MOIRE_SOURCE_SIZE"))
MOIRE_TARGET_SIZE = int(literal_assignment(PLOTTER, "MOIRE_TARGET_SIZE"))
FREQUENCY_HARMONIC_COUNT = int(
    literal_assignment(PLOTTER, "FREQUENCY_HARMONIC_COUNT")
)
ISOTROPY_ANGLE_COUNT = int(
    literal_assignment(PLOTTER, "ISOTROPY_ANGLE_COUNT")
)
PALETTE_COLORS = int(literal_assignment(PLOTTER, "PALETTE_COLORS"))

PLOT_PNGS = (
    "resampling-discrete-stencils.png",
    "resampling-frequency-response-compact.png",
    "resampling-frequency-response-wide.png",
    "resampling-isotropy-gain.png",
    "resampling-isotropy-spread.png",
    "resampling-quality-ms-ssim.png",
    "resampling-quality-delta-e00.png",
    "resampling-speed.png",
    "resampling-size.png",
)
CSV_FILES = (
    "resampling-discrete-stencils.csv",
    "resampling-frequency-response.csv",
    "resampling-isotropy.csv",
    "resampling-moire.csv",
    "resampling-quality-size.csv",
    "resampling-speed.csv",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def file_sha256(path: Path) -> str:
    """Return a file's SHA-256 digest."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read one required non-empty CSV."""
    if not path.is_file():
        fail(f"missing CSV: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"empty CSV: {path}")
    return rows


def png_size(path: Path) -> Tuple[int, int]:
    """Read PNG dimensions from the IHDR chunk."""
    if not path.is_file():
        fail(f"missing PNG: {path}")
    with path.open("rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"invalid PNG signature: {path}")
    if header[12:16] != b"IHDR":
        fail(f"PNG does not begin with IHDR: {path}")
    return struct.unpack(">II", header[16:24])


def validate_png(
    path: Path,
    expected: Optional[Tuple[int, int]] = None,
    minimum: Optional[Tuple[int, int]] = None,
) -> None:
    """Validate dimensions and a nontrivial file size."""
    size = png_size(path)
    if expected is not None and size != expected:
        fail(f"unexpected PNG dimensions for {path}: {size}")
    if minimum is not None and (
        size[0] < minimum[0] or size[1] < minimum[1]
    ):
        fail(f"unexpectedly small PNG plot for {path}: {size}")
    if path.stat().st_size < 100:
        fail(f"unexpectedly small PNG file: {path}")


def finite(row: Mapping[str, str], fields: Sequence[str]) -> None:
    """Validate finite numeric fields."""
    for field in fields:
        if not math.isfinite(float(row[field])):
            fail(f"non-finite {field}: {row}")


def validate_method_order(rows: Sequence[Mapping[str, str]]) -> None:
    """Validate one-row-per-method tables."""
    observed = tuple(row["method"] for row in rows)
    if observed != METHOD_NAMES:
        fail(f"method order changed: {observed}")


def validate_encoder_command(
    command_text: str,
    method: str,
    input_token: str,
    width: int,
    height: int,
) -> None:
    """Validate controls shared by measured encoder commands."""
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
        f"-w{width}",
        f"-h{height}",
        f"-r{method}",
        "-jscalar:resize_precision=linear",
        "-o",
        "{output}",
        input_token,
    )
    if not all(token in command for token in required):
        fail(f"measurement command violates controls: {command_text}")


def validate_stencils(rows: Sequence[Mapping[str, str]]) -> None:
    """Validate discrete response coverage and finite samples."""
    expected_counts = {"enlarge-4x": 68, "reduce-4x": 32}
    for scenario, count in expected_counts.items():
        for method in METHOD_NAMES:
            selected = [
                row for row in rows
                if row["scenario"] == scenario and row["method"] == method
            ]
            if len(selected) != count:
                fail(f"unexpected stencil count for {scenario}/{method}")
            for row in selected:
                finite(row, ("coordinate", "response"))
    unexpected = [
        row for row in rows
        if row["scenario"] not in expected_counts
        or row["method"] not in METHOD_NAMES
    ]
    if unexpected:
        fail("stencil CSV contains an unknown scenario or method")


def validate_frequency(rows: Sequence[Mapping[str, str]]) -> None:
    """Validate frequency coverage on both sides of target Nyquist."""
    for method in METHOD_NAMES:
        selected = [row for row in rows if row["method"] == method]
        if len(selected) != FREQUENCY_HARMONIC_COUNT:
            fail(f"unexpected frequency count for {method}")
        frequencies = [
            float(row["frequency_cycles_per_output_pixel"])
            for row in selected
        ]
        if min(frequencies) >= 0.5 or max(frequencies) <= 0.5:
            fail(f"frequency sweep does not cross Nyquist for {method}")
        for row in selected:
            finite(row, ("frequency_cycles_per_output_pixel", "contrast_gain"))
            expected_region = (
                "passband"
                if float(row["frequency_cycles_per_output_pixel"]) <= 0.5
                else "alias"
            )
            if row["region"] != expected_region:
                fail(f"frequency region is inconsistent: {row}")


def validate_isotropy(rows: Sequence[Mapping[str, str]]) -> None:
    """Validate angle coverage and normalized directional gain."""
    for method in METHOD_NAMES:
        selected = [row for row in rows if row["method"] == method]
        if len(selected) != ISOTROPY_ANGLE_COUNT:
            fail(f"unexpected isotropy angle count for {method}")
        angles = tuple(float(row["angle_degrees"]) for row in selected)
        expected_angles = tuple(float(value) for value in range(0, 91, 5))
        if angles != expected_angles:
            fail(f"isotropy angle order changed for {method}")
        for row in selected:
            finite(row, ("radial_frequency", "contrast_gain",
                         "relative_to_angle_mean"))
        relative_mean = statistics.fmean(
            float(row["relative_to_angle_mean"]) for row in selected
        )
        if abs(relative_mean - 1.0) > 0.000001:
            fail(f"isotropy normalization is inconsistent for {method}")


def validate_moire(rows: Sequence[Mapping[str, str]]) -> None:
    """Validate one zone-plate score and digest per method."""
    validate_method_order(rows)
    for row in rows:
        finite(row, ("rms_error_vs_area_reference",))
        if float(row["rms_error_vs_area_reference"]) <= 0.0:
            fail(f"non-positive zone-plate error: {row}")
        if len(row["output_sha256"]) != 64:
            fail(f"invalid zone-plate digest: {row}")


def validate_quality(rows: Sequence[Mapping[str, str]]) -> None:
    """Validate post-SIXEL quality, size, digests, and commands."""
    validate_method_order(rows)
    for row in rows:
        finite(
            row,
            ("ms_ssim", "delta_e00_mean", "delta_chroma_mean",
             "gmsd", "psnr_y"),
        )
        if not 0.0 <= float(row["ms_ssim"]) <= 1.0:
            fail(f"MS-SSIM outside unit interval: {row}")
        if int(row["sixel_bytes"]) <= 0:
            fail(f"non-positive SIXEL size: {row}")
        if len(row["sixel_sha256"]) != 64:
            fail(f"invalid SIXEL digest: {row}")
        if len(row["decoded_png_sha256"]) != 64:
            fail(f"invalid decoded PNG digest: {row}")
        validate_encoder_command(
            row["command"],
            row["method"],
            "{input}",
            QUALITY_TARGET_WIDTH,
            QUALITY_TARGET_HEIGHT,
        )
        assessment = shlex.split(row["assessment_command"])
        if assessment != [
            "{lsqa}",
            "-Woklab",
            "-Pfloat32",
            "{area_reference}",
            "{decoded_output}",
        ]:
            fail(f"quality assessment controls changed: {row}")


def validate_speed(
    rows: Sequence[Mapping[str, str]], runs: int
) -> None:
    """Validate timing samples, summaries, and controls."""
    validate_method_order(rows)
    for row in rows:
        validate_encoder_command(
            row["command"],
            row["method"],
            "{speed_input}",
            SPEED_TARGET_WIDTH,
            SPEED_TARGET_HEIGHT,
        )
        values = [float(value) for value in row["samples_ms"].split(";")]
        if len(values) != runs or any(value <= 0.0 for value in values):
            fail(f"invalid timing samples: {row}")
        finite(
            row,
            ("median_ms", "q1_ms", "q3_ms", "minimum_ms", "maximum_ms"),
        )
        if abs(statistics.median(values) - float(row["median_ms"])) > 0.000001:
            fail(f"timing median disagrees with samples: {row}")
        if not (
            float(row["minimum_ms"])
            <= float(row["q1_ms"])
            <= float(row["median_ms"])
            <= float(row["q3_ms"])
            <= float(row["maximum_ms"])
        ):
            fail(f"timing summary order is invalid: {row}")


def validate_metadata(path: Path) -> Dict[str, object]:
    """Validate durable provenance and protocol controls."""
    if not path.is_file():
        fail(f"missing metadata: {path}")
    metadata = json.loads(path.read_text(encoding="utf-8"))
    if metadata.get("schema_version") != 1:
        fail("metadata schema changed")
    source = metadata.get("source")
    build = metadata.get("build")
    protocol = metadata.get("protocol")
    artifacts = metadata.get("artifacts")
    if not all(isinstance(item, dict) for item in (
        source, build, protocol, artifacts
    )):
        fail("metadata is missing source, build, protocol, or artifacts")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("durable measurements were not recorded from a clean worktree")
    if not source.get("revision") or len(source.get("input_sha256", "")) != 64:
        fail("source provenance is incomplete")
    for field in ("compiler_command", "configure_arguments"):
        if not build.get(field):
            fail(f"build provenance is missing {field}")
    if protocol.get("methods") != list(METHOD_NAMES):
        fail("metadata method inventory changed")
    if protocol.get("direct_scaler") != "sixel_helper_scale_image_float32":
        fail("metadata direct scaler changed")
    if protocol.get("direct_threads") != 1:
        fail("direct scaler thread count changed")
    if protocol.get("direct_simd") != "scalar":
        fail("direct scaler SIMD control changed")
    if protocol.get("target_nyquist_cycles_per_pixel") != 0.5:
        fail("target Nyquist boundary changed")
    if protocol.get("quality_target_size") != [
        QUALITY_TARGET_WIDTH, QUALITY_TARGET_HEIGHT
    ]:
        fail("quality geometry changed")
    if protocol.get("speed_source_size") != [
        SPEED_SOURCE_WIDTH, SPEED_SOURCE_HEIGHT
    ]:
        fail("speed source geometry changed")
    if protocol.get("speed_target_size") != [
        SPEED_TARGET_WIDTH, SPEED_TARGET_HEIGHT
    ]:
        fail("speed target geometry changed")
    if protocol.get("resize_precision") != "linear":
        fail("encoder resize precision changed")
    return metadata


def validate_artifact_hashes(
    directory: Path, metadata: Mapping[str, object]
) -> None:
    """Validate the complete non-manifest artifact inventory."""
    recorded = metadata["artifacts"]
    if not isinstance(recorded, dict):
        fail("metadata artifact inventory is not an object")
    observed = {
        path.name: file_sha256(path)
        for path in sorted(directory.iterdir())
        if path.is_file() and path.name != "resampling-run.json"
    }
    if observed != recorded:
        fail("artifact hashes or inventory changed")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    return parser.parse_args()


def main() -> int:
    """Validate all resampling evidence."""
    args = parse_args()
    directory = args.directory
    metadata = validate_metadata(directory / "resampling-run.json")
    protocol = metadata["protocol"]
    if not isinstance(protocol, dict):
        fail("metadata protocol is not an object")

    for filename in PLOT_PNGS:
        validate_png(directory / filename, minimum=(700, 300))
    validate_png(
        directory / "resampling-moire-input.png",
        expected=(MOIRE_SOURCE_SIZE, MOIRE_SOURCE_SIZE),
    )
    validate_png(
        directory / "resampling-moire-reference.png",
        expected=(MOIRE_TARGET_SIZE, MOIRE_TARGET_SIZE),
    )
    for method in METHOD_NAMES:
        validate_png(
            directory / f"resampling-moire-{method}.png",
            expected=(MOIRE_TARGET_SIZE, MOIRE_TARGET_SIZE),
        )
    zoom_size = (ZOOM_WIDTH * ZOOM_SCALE, ZOOM_HEIGHT * ZOOM_SCALE)
    validate_png(
        directory / "resampling-natural-reference-zoom.png",
        expected=zoom_size,
    )
    for method in METHOD_NAMES:
        validate_png(
            directory / f"resampling-natural-{method}-zoom.png",
            expected=zoom_size,
        )
    for filename in CSV_FILES:
        if not (directory / filename).is_file():
            fail(f"missing CSV: {directory / filename}")

    validate_stencils(read_csv(directory / "resampling-discrete-stencils.csv"))
    validate_frequency(read_csv(directory / "resampling-frequency-response.csv"))
    validate_isotropy(read_csv(directory / "resampling-isotropy.csv"))
    validate_moire(read_csv(directory / "resampling-moire.csv"))
    validate_quality(read_csv(directory / "resampling-quality-size.csv"))
    validate_speed(
        read_csv(directory / "resampling-speed.csv"),
        int(protocol["speed_runs"]),
    )
    validate_artifact_hashes(directory, metadata)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
