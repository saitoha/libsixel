#!/usr/bin/env python3
"""Validate checked-in encode-policy and high-color measurement artifacts."""

from __future__ import annotations

import argparse
import csv
import json
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


def validate_encode_policy(directory: Path) -> None:
    """Validate the complete -E comparison."""
    rows = read_rows(directory / "encode-policy-comparison.csv")
    metadata = json.loads(
        (directory / "encode-policy-run.json").read_text(encoding="utf-8")
    )
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
        if int(row["runs"]) != int(metadata["timing"]["runs"]):
            raise RuntimeError("encode-policy timing count disagrees with metadata")
    require_png(directory / "encode-policy-results.png")


def validate_high_color(directory: Path) -> None:
    """Validate the complete -I comparison."""
    rows = read_rows(directory / "high-color-comparison.csv")
    metadata = json.loads(
        (directory / "high-color-run.json").read_text(encoding="utf-8")
    )
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
        if int(row["runs"]) != int(metadata["timing"]["runs"]):
            raise RuntimeError("high-color timing count disagrees with metadata")
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
