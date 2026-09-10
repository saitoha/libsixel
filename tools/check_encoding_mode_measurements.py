#!/usr/bin/env python3
"""Validate checked-in encode-policy and high-color measurement artifacts."""

from __future__ import annotations

import argparse
import csv
import json
import shlex
from pathlib import Path
from typing import Dict, List


def read_rows(path: Path) -> List[Dict[str, str]]:
    """Read one measurement CSV."""
    with path.open("r", encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def require_digest(value: str, field: str) -> None:
    """Reject malformed SHA-256 fields."""
    if len(value) != 64 or any(character not in "0123456789abcdef"
                               for character in value):
        raise RuntimeError(f"{field} is not a lowercase SHA-256 digest")


def require_png(path: Path) -> None:
    """Reject missing or malformed PNG artifacts."""
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise RuntimeError(f"{path} is not a PNG file")


def validate_speed_rows(rows: List[Dict[str, str]], metadata: dict,
                        variants: List[str], family: str) -> None:
    """Validate the complete thread-by-variant encode timing grid."""
    timing = metadata["timing"]
    threads = [int(value) for value in timing["threads"]]
    runs = int(timing["runs"])
    expected = len(variants) * len(threads) * runs
    if len(rows) != expected:
        raise RuntimeError(
            f"{family} speed grid has {len(rows)} rows, expected {expected}"
        )
    observed = {
        (row["variant"], int(row["threads"]), int(row["sample"]))
        for row in rows
    }
    required = {
        (variant, threads_value, sample)
        for variant in variants
        for threads_value in threads
        for sample in range(1, runs + 1)
    }
    if observed != required:
        raise RuntimeError(f"{family} speed grid is incomplete")
    for row in rows:
        first = float(row["encode_first_worker_start_seconds"])
        last = float(row["encode_last_worker_done_seconds"])
        elapsed = float(row["encode_window_seconds"])
        if first < 0.0 or last <= first or elapsed <= 0.0:
            raise RuntimeError(f"{family} contains an invalid encode window")
        if abs((last - first) - elapsed) > 1e-12:
            raise RuntimeError(f"{family} encode window arithmetic disagrees")
        if (int(row["encode_worker_start_count"])
                != int(row["encode_worker_done_count"])):
            raise RuntimeError(f"{family} worker events are unpaired")
        tokens = shlex.split(row["command"])
        threads_token = f"--threads={row['threads']}"
        if threads_token not in tokens:
            raise RuntimeError(f"{family} command has the wrong thread budget")
        if "-J" not in tokens or "{timeline}" not in tokens:
            raise RuntimeError(f"{family} command lacks timeline logging")
        if "-w" not in tokens or "-h" not in tokens:
            raise RuntimeError(f"{family} command lacks speed dimensions")


def validate_encode_policy(directory: Path) -> None:
    """Validate the complete -E comparison."""
    rows = read_rows(directory / "encode-policy-comparison.csv")
    metadata = json.loads(
        (directory / "encode-policy-run.json").read_text(encoding="utf-8")
    )
    speed_rows = read_rows(directory / "encode-policy-speed.csv")
    if [row["policy"] for row in rows] != ["auto", "fast", "size"]:
        raise RuntimeError("encode-policy grid is incomplete or out of order")
    if metadata["source_state"] != "clean":
        raise RuntimeError("encode-policy provenance is not clean")
    decoded = {row["decoded_png_sha256"] for row in rows}
    if len(decoded) != 1:
        raise RuntimeError("encode policies do not decode to identical pixels")
    if rows[0]["encoded_sha256"] != rows[1]["encoded_sha256"]:
        raise RuntimeError("auto and fast no longer emit identical streams")
    if int(rows[2]["encoded_bytes"]) >= int(rows[1]["encoded_bytes"]):
        raise RuntimeError("size policy did not reduce this fixture")
    for row in rows:
        require_digest(row["encoded_sha256"], "encoded_sha256")
        require_digest(row["decoded_png_sha256"], "decoded_png_sha256")
    validate_speed_rows(
        speed_rows, metadata, ["auto", "fast", "size"], "encode-policy"
    )
    for row in speed_rows:
        tokens = shlex.split(row["command"])
        if tokens[tokens.index("--encode-policy") + 1] != row["variant"]:
            raise RuntimeError("encode-policy speed command has wrong policy")
    require_png(directory / "encode-policy-results.png")


def validate_high_color(directory: Path) -> None:
    """Validate the complete -I comparison."""
    rows = read_rows(directory / "high-color-comparison.csv")
    metadata = json.loads(
        (directory / "high-color-run.json").read_text(encoding="utf-8")
    )
    speed_rows = read_rows(directory / "high-color-speed.csv")
    if [row["mode"] for row in rows] != ["fixed256", "high15"]:
        raise RuntimeError("high-color grid is incomplete or out of order")
    if metadata["source_state"] != "clean":
        raise RuntimeError("high-color provenance is not clean")
    if " -I " not in f" {rows[1]['command']} ":
        raise RuntimeError("high-color command does not contain -I")
    if " -I " in f" {rows[0]['command']} ":
        raise RuntimeError("fixed-palette command unexpectedly contains -I")
    for row in rows:
        require_digest(row["encoded_sha256"], "encoded_sha256")
        require_digest(row["decoded_png_sha256"], "decoded_png_sha256")
        if int(row["encoded_bytes"]) <= 0:
            raise RuntimeError("high-color comparison contains an empty stream")
        if not 0.0 <= float(row["MS-SSIM"]) <= 1.0:
            raise RuntimeError("MS-SSIM is outside its valid range")
    validate_speed_rows(
        speed_rows, metadata, ["fixed256", "high15"], "high-color"
    )
    for row in speed_rows:
        tokens = shlex.split(row["command"])
        has_high_color = "-I" in tokens
        if has_high_color != (row["variant"] == "high15"):
            raise RuntimeError("high-color speed command has wrong mode")
    require_png(directory / "high-color-results.png")
    require_png(directory / "high-color-visual-comparison.png")


def parse_args() -> argparse.Namespace:
    """Parse artifact directories."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("encode_policy_directory", type=Path)
    parser.add_argument("high_color_directory", type=Path)
    return parser.parse_args()


def main() -> None:
    """Validate both measurement families."""
    args = parse_args()
    validate_encode_policy(args.encode_policy_directory)
    validate_high_color(args.high_color_directory)


if __name__ == "__main__":
    main()
