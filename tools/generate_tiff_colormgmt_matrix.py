#!/usr/bin/env python3
"""
Generate TIFF color-management matrix fixtures and optional CoreGraphics references.

Matrix axes (binary):
  - ICC profile (Tag 34675)
  - WhitePoint (Tag 318)
  - PrimaryChromaticities (Tag 319)
  - TransferFunction (Tag 301)

Outputs are written under:
  tests/data/colormgmt/input/tiff/{rgb,lab}
  tests/data/colormgmt/reference/tiff/{rgb,lab}   (optional, when --with-reference)

The script also emits a small hash-group analysis report for each color space.
"""

from __future__ import annotations

import argparse
import hashlib
import itertools
import subprocess
import sys
from pathlib import Path

import numpy as np
import tifffile
from PIL import Image


RGB_NAME = "rgb"
LAB_NAME = "lab"
GRAYSCALE_NAME = "grayscale"
PALETTE_NAME = "palette"
SPACES = (RGB_NAME, LAB_NAME, GRAYSCALE_NAME, PALETTE_NAME)


def detect_icc_profile() -> Path | None:
    candidates = [
        Path("/System/Library/ColorSync/Profiles/AdobeRGB1998.icc"),
        Path("/Library/ColorSync/Profiles/AdobeRGB1998.icc"),
    ]
    for path in candidates:
        if path.is_file():
            return path
    return None


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def write_group_report(report_path: Path, space_dir: Path) -> None:
    rows: list[tuple[str, str]] = []
    groups: dict[str, list[str]] = {}
    for six in sorted(space_dir.glob("*.six")):
        digest = hashlib.sha256(six.read_bytes()).hexdigest()[:12]
        rows.append((six.name, digest))
        groups.setdefault(digest, []).append(six.name)

    lines: list[str] = []
    lines.append("file\tsha256_12")
    for name, digest in rows:
        lines.append(f"{name}\t{digest}")
    lines.append("")
    lines.append(f"unique_groups\t{len(groups)}")
    for digest in sorted(groups):
        lines.append(f"group\t{digest}\t{len(groups[digest])}\t{groups[digest][0]}")
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate_inputs(
    repo_root: Path,
    icc_bytes: bytes,
    base_rgb: np.ndarray,
    base_lab: np.ndarray,
    base_grayscale: np.ndarray,
    base_palette: np.ndarray,
    palette_colormap: np.ndarray,
) -> None:
    input_root = repo_root / "tests/data/colormgmt/input/tiff"
    for space in SPACES:
        (input_root / space).mkdir(parents=True, exist_ok=True)

    white = (3457, 10000, 3585, 10000)  # D50-like xy
    prim = (
        6400, 10000, 3300, 10000,  # red
        2100, 10000, 7100, 10000,  # green (AdobeRGB-like)
        1500, 10000, 600, 10000,   # blue
    )

    x = np.linspace(0.0, 1.0, 256)
    tr = np.clip(np.round((x ** (1.0 / 1.8)) * 65535.0), 0, 65535).astype(np.uint16)
    transfer_rgb = np.concatenate([tr, tr, tr])
    transfer_gray = tr

    for space, base, photometric, transfer in (
        (RGB_NAME, base_rgb, "rgb", transfer_rgb),
        (LAB_NAME, base_lab, "cielab", transfer_rgb),
        (GRAYSCALE_NAME, base_grayscale, "miniswhite", transfer_gray),
        (PALETTE_NAME, base_palette, "palette", transfer_rgb),
    ):
        out_dir = input_root / space
        for icc, wp, pc, tf in itertools.product([0, 1], [0, 1], [0, 1], [0, 1]):
            name = f"img_tiff_{space}_icc{icc}_wp{wp}_pc{pc}_trc{tf}.tiff"
            tags = []
            if wp:
                tags.append((318, "2I", 2, white, False))
            if pc:
                tags.append((319, "2I", 6, prim, False))
            if tf:
                tags.append((301, "H", len(transfer), transfer, False))
            if icc:
                tags.append((34675, "B", len(icc_bytes), icc_bytes, False))
            write_kwargs = {
                "photometric": photometric,
                "compression": "deflate",
                "extratags": tags,
            }
            if space == PALETTE_NAME:
                write_kwargs["colormap"] = palette_colormap
            tifffile.imwrite(
                out_dir / name,
                base,
                **write_kwargs,
            )


def generate_references(repo_root: Path, img2sixel: Path) -> None:
    ref_root = repo_root / "tests/data/colormgmt/reference/tiff"
    inp_root = repo_root / "tests/data/colormgmt/input/tiff"
    for space in SPACES:
        in_dir = inp_root / space
        out_dir = ref_root / space
        out_dir.mkdir(parents=True, exist_ok=True)
        for tif in sorted(in_dir.glob("*.tiff")):
            six = out_dir / (tif.stem + ".six")
            with six.open("wb") as fp:
                subprocess.run(
                    [str(img2sixel), "-Lcoregraphics!", str(tif)],
                    check=True,
                    stdout=fp,
                )

        report = ref_root / f"{space}_groups.tsv"
        write_group_report(report, out_dir)


def build_lab_source(repo_root: Path, tmp_path: Path) -> np.ndarray:
    base_tiff = repo_root / "tests/data/inputs/snake_64.tiff"
    run(["magick", str(base_tiff), "-colorspace", "Lab", str(tmp_path)])
    try:
        return tifffile.imread(tmp_path)
    finally:
        tmp_path.unlink(missing_ok=True)


def build_grayscale_source(base_rgb: np.ndarray) -> np.ndarray:
    image = Image.fromarray(base_rgb, mode="RGB")
    return np.array(image.convert("L"), dtype=np.uint8)


def build_palette_source(base_rgb: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    image = Image.fromarray(base_rgb, mode="RGB")
    paletted = image.quantize(colors=256, method=Image.MEDIANCUT)
    indexed = np.array(paletted, dtype=np.uint8)
    palette = list(paletted.getpalette() or [])
    if len(palette) < 256 * 3:
        palette.extend([0] * (256 * 3 - len(palette)))
    colormap = np.zeros((3, 256), dtype=np.uint16)
    for idx in range(256):
        colormap[0, idx] = int(palette[idx * 3 + 0]) * 257
        colormap[1, idx] = int(palette[idx * 3 + 1]) * 257
        colormap[2, idx] = int(palette[idx * 3 + 2]) * 257
    return indexed, colormap


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="libsixel repository root",
    )
    parser.add_argument(
        "--with-reference",
        action="store_true",
        help="also render CoreGraphics references (*.six)",
    )
    parser.add_argument(
        "--img2sixel",
        type=Path,
        default=None,
        help="img2sixel binary path (default: <repo>/converters/img2sixel)",
    )
    parser.add_argument(
        "--icc-profile",
        type=Path,
        default=None,
        help="ICC profile to embed for icc=1 fixtures",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    if not repo_root.is_dir():
        print(f"error: repo root not found: {repo_root}", file=sys.stderr)
        return 1

    icc_profile = args.icc_profile.resolve() if args.icc_profile else detect_icc_profile()
    if icc_profile is None or not icc_profile.is_file():
        print(
            "error: ICC profile not found. pass --icc-profile <path>.",
            file=sys.stderr,
        )
        return 1
    icc_bytes = icc_profile.read_bytes()

    base_rgb_path = repo_root / "tests/data/inputs/snake_64.tiff"
    if not base_rgb_path.is_file():
        print(f"error: missing base TIFF fixture: {base_rgb_path}", file=sys.stderr)
        return 1
    base_rgb = np.array(Image.open(base_rgb_path).convert("RGB"), dtype=np.uint8)
    base_lab = build_lab_source(repo_root, repo_root / "tests/data/colormgmt/input/tiff/.tmp_lab_source.tiff")
    base_grayscale = build_grayscale_source(base_rgb)
    base_palette, palette_colormap = build_palette_source(base_rgb)

    generate_inputs(repo_root,
                    icc_bytes,
                    base_rgb,
                    base_lab,
                    base_grayscale,
                    base_palette,
                    palette_colormap)
    print("generated TIFF input matrix under tests/data/colormgmt/input/tiff")

    if args.with_reference:
        img2sixel = args.img2sixel.resolve() if args.img2sixel else (repo_root / "converters/img2sixel")
        if not img2sixel.is_file():
            print(f"error: img2sixel not found: {img2sixel}", file=sys.stderr)
            return 1
        generate_references(repo_root, img2sixel)
        print("generated CoreGraphics references under tests/data/colormgmt/reference/tiff")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
