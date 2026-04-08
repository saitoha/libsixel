#!/usr/bin/env python3
"""Fetch psd-tools fixtures and regenerate hybrid expected assets.

This script is only for regenerating static test assets.
It is not invoked by TAP tests.
"""

from __future__ import annotations

import argparse
import csv
import pathlib
import subprocess
import urllib.request


HERE = pathlib.Path(__file__).resolve().parent
DEFAULT_INPUT_DIR = HERE
DEFAULT_EXPECTED_DIR = HERE.parent / "loader" / "builtin_expected"
DEFAULT_BASE_URL = "https://raw.githubusercontent.com/psd-tools/psd-tools"
DEFAULT_REF = "c83ae643c24c05ab73e2697c23674a5edb380565"
OFFICIAL_PSDTOOLS_REPO = "https://github.com/psd-tools/psd-tools"
SOURCES_TSV = HERE / "psdtools-fixtures-sources.tsv"
MIT_REQUIRED_SNIPPETS = (
    "Permission is hereby granted, free of charge",
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND",
)


ASSETS = [
    {
        "upstream": "16bit5x5.psd",
        "local": "psdtools_16bit5x5.psd",
        "expected": "psdtools_16bit5x5_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "32bit5x5.psd",
        "local": "psdtools_32bit5x5.psd",
        "expected": "psdtools_32bit5x5_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "clipping-mask.psd",
        "local": "psdtools_clipping_mask.psd",
        "expected": "psdtools_clipping_mask_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "gradient-fill.psd",
        "local": "psdtools_gradient_fill.psd",
        "expected": "psdtools_gradient_fill_expected_coregraphics.six",
        "generator": "coregraphics",
    },
    {
        "upstream": "16bit5x5.psb",
        "local": "psdtools_16bit5x5.psb",
        "expected": "psdtools_16bit5x5_psb_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "group.psb",
        "local": "psdtools_group.psb",
        "expected": "psdtools_group_psb_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "artboard.psd",
        "local": "psdtools_artboard.psd",
        "expected": "psdtools_artboard_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "advanced-blending.psd",
        "local": "psdtools_advanced_blending.psd",
        "expected": "psdtools_advanced_blending_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "blend-modes/group-divider-blend-mode.psd",
        "local": "psdtools_group_divider_blend_mode.psd",
        "expected": "psdtools_group_divider_blend_mode_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "2layer_8ele_tblocks.psd",
        "local": "psdtools_2layer_8ele_tblocks.psd",
        "expected": "psdtools_2layer_8ele_tblocks_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "layer-name-emoji.psd",
        "local": "psdtools_layer_name_emoji.psd",
        "expected": "psdtools_layer_name_emoji_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "transparentbg-gimp.psd",
        "local": "psdtools_transparentbg_gimp.psd",
        "expected": "psdtools_transparentbg_gimp_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "layers-minimal/type-layer.psd",
        "local": "psdtools_layers_minimal_type_layer.psd",
        "expected": "psdtools_layers_minimal_type_layer_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "colormodes/4x4_8bit_duotone.psd",
        "local": "psdtools_colormodes_4x4_8bit_duotone.psd",
        "expected": "psdtools_colormodes_4x4_8bit_duotone_expected_coregraphics.six",
        "generator": "coregraphics",
    },
    {
        "upstream": "blend-and-clipping.psd",
        "local": "psdtools_blend_and_clipping.psd",
        "expected": "psdtools_blend_and_clipping_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "layers-minimal/pattern-fill.psd",
        "local": "psdtools_layers_minimal_pattern_fill.psd",
        "expected": "psdtools_layers_minimal_pattern_fill_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "effects/shape-fx.psd",
        "local": "psdtools_effects_shape_fx.psd",
        "expected": "psdtools_effects_shape_fx_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "transparency/fill-opacity.psd",
        "local": "psdtools_transparency_fill_opacity.psd",
        "expected": "psdtools_transparency_fill_opacity_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "transparency/clip-opacity.psd",
        "local": "psdtools_transparency_clip_opacity.psd",
        "expected": "psdtools_transparency_clip_opacity_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "vector-mask.psd",
        "local": "psdtools_vector_mask.psd",
        "expected": "psdtools_vector_mask_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "vector-mask-disabled.psd",
        "local": "psdtools_vector_mask_disabled.psd",
        "expected": "psdtools_vector_mask_disabled_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "effects/shape-fx2.psd",
        "local": "psdtools_effects_shape_fx2.psd",
        "expected": "psdtools_effects_shape_fx2_expected_psdtools.ppm",
        "generator": "psdtools",
    },
    {
        "upstream": "effects/stroke-composite.psd",
        "local": "psdtools_effects_stroke_composite.psd",
        "expected": "psdtools_effects_stroke_composite_expected_psdtools.ppm",
        "generator": "psdtools",
    },
]


def load_sources_metadata(path: pathlib.Path) -> dict[str, dict[str, str]]:
    metadata = {}
    with path.open("r", encoding="utf-8") as f:
        reader = csv.reader(f, delimiter="\t")
        next(reader)
        for row in reader:
            if len(row) < 7:
                raise RuntimeError(f"invalid metadata row in {path}: {row!r}")
            local_name = pathlib.Path(row[3]).name
            metadata[local_name] = {
                "upstream_repo_url": row[0],
                "upstream_ref": row[1],
                "upstream_path": row[2],
                "local_fixture_path": row[3],
                "expected_path": row[4],
                "expected_generator": row[5],
                "license": row[6],
            }
    return metadata


def assert_upstream_mit_license(upstream_ref: str) -> None:
    url = f"{DEFAULT_BASE_URL}/{upstream_ref}/LICENSE"
    with urllib.request.urlopen(url, timeout=30) as response:
        text = response.read().decode("utf-8", "replace")
    for snippet in MIT_REQUIRED_SNIPPETS:
        if snippet not in text:
            raise RuntimeError(
                f"upstream license does not look like MIT for ref {upstream_ref}"
            )


def verify_asset_sources_metadata(assets: list[dict[str, str]]) -> None:
    metadata = load_sources_metadata(SOURCES_TSV)
    refs = set()
    for asset in assets:
        local_name = asset["local"]
        row = metadata.get(local_name)
        if row is None:
            raise RuntimeError(
                f"missing metadata row in {SOURCES_TSV} for {local_name}"
            )
        if row["upstream_repo_url"] != OFFICIAL_PSDTOOLS_REPO:
            raise RuntimeError(
                f"{local_name}: unsupported upstream repo "
                f"{row['upstream_repo_url']}"
            )
        if row["license"] != "MIT":
            raise RuntimeError(
                f"{local_name}: unsupported license {row['license']}"
            )
        expected_upstream_path = f"tests/psd_files/{asset['upstream']}"
        if row["upstream_path"] != expected_upstream_path:
            raise RuntimeError(
                f"{local_name}: upstream path mismatch "
                f"{row['upstream_path']} != {expected_upstream_path}"
            )
        refs.add(row["upstream_ref"])
    for upstream_ref in sorted(refs):
        assert_upstream_mit_license(upstream_ref)


def download_file(url: str, dest: pathlib.Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url) as src, dest.open("wb") as out:
        out.write(src.read())


def write_ppm_from_pillow_image(image, dest: pathlib.Path) -> None:
    rgb = image.convert("RGB")
    width, height = rgb.size
    payload = rgb.tobytes()
    dest.parent.mkdir(parents=True, exist_ok=True)
    with dest.open("wb") as f:
        f.write(f"P6\n{width} {height}\n255\n".encode("ascii"))
        f.write(payload)


def generate_psdtools_expected(src_path: pathlib.Path, dest_path: pathlib.Path) -> None:
    try:
        from psd_tools import PSDImage
    except Exception as exc:  # pragma: no cover - environment dependent
        raise RuntimeError(
            "psd-tools is required to generate psdtools-based expected assets"
        ) from exc

    psd = PSDImage.open(src_path)
    image = psd.composite(force=True)
    write_ppm_from_pillow_image(image, dest_path)


def generate_coregraphics_expected(
    src_path: pathlib.Path, dest_path: pathlib.Path, img2sixel: str
) -> None:
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [img2sixel, "-Lcoregraphics!", "-o", str(dest_path), str(src_path)],
        check=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs-dir", default=str(DEFAULT_INPUT_DIR))
    parser.add_argument("--expected-dir", default=str(DEFAULT_EXPECTED_DIR))
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--upstream-ref", default=DEFAULT_REF)
    parser.add_argument("--download", action="store_true")
    parser.add_argument("--img2sixel", default="img2sixel")
    args = parser.parse_args()

    inputs_dir = pathlib.Path(args.inputs_dir)
    expected_dir = pathlib.Path(args.expected_dir)

    for asset in ASSETS:
        src_path = inputs_dir / asset["local"]
        if args.download:
            url = (
                f"{args.base_url}/{args.upstream_ref}/tests/psd_files/{asset['upstream']}"
            )
            print(f"download {url} -> {src_path}")
            download_file(url, src_path)

        if not src_path.exists():
            raise RuntimeError(f"missing source fixture: {src_path}")

        dest_path = expected_dir / asset["expected"]
        if asset["generator"] == "psdtools":
            print(f"generate psdtools expected: {dest_path}")
            generate_psdtools_expected(src_path, dest_path)
        elif asset["generator"] == "coregraphics":
            print(f"generate coregraphics expected: {dest_path}")
            generate_coregraphics_expected(src_path, dest_path, args.img2sixel)
        else:  # pragma: no cover - configuration error
            raise RuntimeError(f"unknown generator type: {asset['generator']}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
