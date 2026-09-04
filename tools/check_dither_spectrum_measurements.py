#!/usr/bin/env python3
"""Validate dither-spectrum measurement completeness and provenance."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
from pathlib import Path
from typing import Dict, List, Set, Tuple

from PIL import Image


RADIAL_BINS = 64
TONE_COUNT = 15
METHODS: Tuple[Tuple[str, str], ...] = (
    ("none", "none:scan=raster"),
    ("fs", "fs:scan=raster"),
    ("atkinson", "atkinson:scan=raster"),
    ("jajuni", "jajuni:scan=raster"),
    ("stucki", "stucki:scan=raster"),
    ("burkes", "burkes:scan=raster"),
    ("sierra1", "sierra:variant=1:scan=raster"),
    ("sierra2", "sierra:variant=2:scan=raster"),
    ("sierra3", "sierra:variant=3:scan=raster"),
    ("lso2", "lso2:scan=raster"),
    ("a_dither", "a_dither:strength=0.150:scan=raster"),
    ("x_dither", "x_dither:strength=0.100:scan=raster"),
    (
        "bluenoise",
        "bluenoise:strength=0.055:gradient_factor=0:"
        "phase=0,0:channel=mono:size=64:scan=raster",
    ),
)
PLOTS = (
    "dither-policy-spectrum-atlas.png",
    "dither-policy-spectrum-overview.png",
)


def fail(message: str) -> None:
    """Raise one consistently formatted validation error."""
    raise ValueError(message)


def read_csv(path: Path) -> List[Dict[str, str]]:
    """Read a non-empty CSV file."""
    if not path.is_file():
        fail(f"missing measurement file: {path}")
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        fail(f"measurement file has no rows: {path}")
    return rows


def read_metadata(path: Path) -> Dict[str, object]:
    """Read and validate the measurement manifest."""
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
    if not all(
        isinstance(value, dict)
        for value in (source, protocol, programs, input_record)
    ):
        fail(f"incomplete measurement metadata: {path}")
    if source.get("tracked_worktree_state_at_start") != "clean":
        fail("checked-in measurements must originate from a clean worktree")
    revision = source.get("revision")
    if not isinstance(revision, str) or not revision or revision == "unknown":
        fail("measurement source revision is missing")
    if protocol.get("sixel_environment_removed") is not True:
        fail("measurement metadata does not confirm SIXEL_* isolation")
    expected_methods = [
        {"name": method, "diffusion_option": diffusion}
        for method, diffusion in METHODS
    ]
    if protocol.get("methods") != expected_methods:
        fail("measurement metadata has unexpected dither methods")
    expected_protocol = {
        "threads": 1,
        "precision": "8bit",
        "loader": "builtin!",
        "working_colorspace": "gamma",
        "lookup_policy": "none",
        "scan": "raster",
        "gpu_policy": "off",
        "window": "separable Hann",
        "radial_bins": RADIAL_BINS,
        "angular_bins": 36,
    }
    for name, expected in expected_protocol.items():
        if protocol.get(name) != expected:
            fail(f"measurement metadata has unexpected {name}")
    if input_record.get("field_size") != 512 or input_record.get("crop_size") != 256:
        fail("measurement metadata has unexpected field or crop size")
    tones = input_record.get("tone_values")
    if not isinstance(tones, list) or len(tones) != TONE_COUNT:
        fail("measurement metadata has unexpected tone set")
    input_hash = input_record.get("ordered_inputs_sha256")
    if not isinstance(input_hash, str) or len(input_hash) != 64:
        fail("invalid generated-input SHA-256")
    img2sixel = programs.get("img2sixel")
    if not isinstance(img2sixel, dict):
        fail("missing img2sixel provenance")
    for field in ("launcher_sha256", "payload_sha256"):
        value = img2sixel.get(field)
        if not isinstance(value, str) or len(value) != 64:
            fail(f"invalid img2sixel {field}")
    return payload


def validate_command(row: Dict[str, str], path: Path) -> None:
    """Check the controlled fixed-palette CLI shape."""
    method = row.get("method", "")
    options = dict(METHODS)
    if method not in options:
        fail(f"unexpected method in {path}: {method}")
    if row.get("diffusion_option") != options[method]:
        fail(f"method/diffusion mismatch in {path}: {method}")
    try:
        tokens = shlex.split(row.get("command", ""))
    except ValueError as exc:
        raise ValueError(f"invalid command in {path}: {method}") from exc
    required = {
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=builtin!",
        "--builtin-palette=gray4",
        "-Wgamma",
        f"--diffusion={options[method]}",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "png:-",
        "-",
    }
    missing = sorted(required - set(tokens))
    if missing:
        fail(f"command controls missing in {path}: {missing}")


def parse_optional(row: Dict[str, str], name: str) -> float:
    """Parse a required finite floating-point CSV field."""
    try:
        value = float(row.get(name, ""))
    except ValueError as exc:
        raise ValueError(f"invalid {name}: {row.get('method', '')}") from exc
    if not math.isfinite(value):
        fail(f"non-finite {name}: {row.get('method', '')}")
    return value


def validate_summary(path: Path, revision: str) -> None:
    """Validate one scalar summary row for every method."""
    rows = read_csv(path)
    actual: Set[str] = set()
    for row in rows:
        validate_command(row, path)
        method = row.get("method", "")
        if method in actual:
            fail(f"duplicate summary method in {path}: {method}")
        actual.add(method)
        if row.get("revision") != revision:
            fail(f"summary revision differs from metadata in {path}")
        try:
            tone_count = int(row.get("tone_count", ""))
            active_tones = int(row.get("active_tones", ""))
        except ValueError as exc:
            raise ValueError(f"invalid tone count in {path}: {method}") from exc
        if tone_count != TONE_COUNT or not 0 <= active_tones <= TONE_COUNT:
            fail(f"unexpected tone coverage in {path}: {method}")
        rms = parse_optional(row, "rms_luma_error")
        parse_optional(row, "mean_luma_bias")
        if rms <= 0.0:
            fail(f"invalid RMS luma error in {path}: {method}")
        spectral_fields = (
            "low_frequency_ratio",
            "peak_frequency_nyquist",
            "spectral_centroid_nyquist",
            "anisotropy_db",
            "spectral_flatness",
        )
        if method == "none":
            if active_tones != 0 or any(row.get(name, "") for name in spectral_fields):
                fail("none must have no non-DC spectral metrics")
            continue
        if active_tones < 1:
            fail(f"dither method has no active tones in {path}: {method}")
        low = parse_optional(row, "low_frequency_ratio")
        peak = parse_optional(row, "peak_frequency_nyquist")
        centroid = parse_optional(row, "spectral_centroid_nyquist")
        parse_optional(row, "anisotropy_db")
        flatness = parse_optional(row, "spectral_flatness")
        if not 0.0 <= low <= 1.0:
            fail(f"invalid low-frequency ratio in {path}: {method}")
        if not 0.0 <= peak <= 1.0 or not 0.0 <= centroid <= 1.0:
            fail(f"invalid normalized frequency in {path}: {method}")
        if not 0.0 <= flatness <= 1.0:
            fail(f"invalid spectral flatness in {path}: {method}")
    expected = {method for method, _diffusion in METHODS}
    if actual != expected:
        fail(
            f"summary method mismatch: missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )


def validate_curves(path: Path, revision: str) -> None:
    """Validate every radial power and anisotropy curve point."""
    rows = read_csv(path)
    grouped: Dict[str, List[Dict[str, str]]] = {}
    for row in rows:
        method = row.get("method", "")
        grouped.setdefault(method, []).append(row)
        if row.get("revision") != revision:
            fail(f"curve revision differs from metadata in {path}")
    expected = {method for method, _diffusion in METHODS}
    if set(grouped) != expected:
        fail("curve method set differs from expected methods")
    for method, method_rows in grouped.items():
        if len(method_rows) != RADIAL_BINS:
            fail(f"unexpected curve length in {path}: {method}")
        frequencies = [parse_optional(row, "frequency_nyquist") for row in method_rows]
        if any(lhs >= rhs for lhs, rhs in zip(frequencies, frequencies[1:])):
            fail(f"curve frequencies are not strictly increasing: {method}")
        for row in method_rows:
            power = row.get("radial_power_db", "")
            anisotropy = row.get("radial_anisotropy_db", "")
            if method == "none":
                if power or anisotropy:
                    fail("none curve must contain empty spectral values")
            else:
                parse_optional(row, "radial_power_db")
                if anisotropy:
                    parse_optional(row, "radial_anisotropy_db")


def validate_plots(directory: Path) -> None:
    """Require readable plots with enough pixels for their intended layout."""
    for name in PLOTS:
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            fail(f"missing or empty plot: {path}")
        with Image.open(path) as image:
            width, height = image.size
        if width < 1600 or height < 1000:
            fail(f"plot is too small for documentation use: {path}")


def main() -> int:
    """Validate one complete dither-spectrum measurement directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    metadata = read_metadata(args.directory / "dither-spectrum-run.json")
    revision = str(metadata["source"]["revision"])
    validate_summary(
        args.directory / "dither-spectrum-summary.csv",
        revision,
    )
    validate_curves(
        args.directory / "dither-spectrum-curves.csv",
        revision,
    )
    validate_plots(args.directory)
    print(
        "validated 13 dither summaries and 832 radial curve points "
        "across 15 generated tones"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
