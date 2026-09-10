#!/usr/bin/env python3
"""Generate deterministic image-loader architecture figures."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Sequence

from plot_encoding_pipeline_figures import (
    BLUE,
    BLUE_LIGHT,
    GOLD,
    GOLD_LIGHT,
    GRID,
    INK,
    MAGENTA,
    MAGENTA_LIGHT,
    MUTED,
    PANEL,
    PAPER,
    TEAL,
    TEAL_LIGHT,
    defs,
    path,
    rect,
    svg_document,
    text,
    text_lines,
)


RED = "#B42318"
RED_LIGHT = "#FEE4E2"
PURPLE = "#6941C6"
PURPLE_LIGHT = "#F4EBFF"
SLATE_LIGHT = "#EEF2F6"

ASSET_NAMES = (
    "loader-chain-wide.svg",
    "loader-chain-mobile.svg",
    "builtin-lineage-wide.svg",
    "builtin-lineage-mobile.svg",
    "loader-architecture-figures.json",
)


def arrow(data: str, color: str = BLUE, width: float = 4.0,
          dash: str | None = None) -> str:
    """Return a routed arrow using the shared marker."""
    return path(data, color, width, "arrow-blue", dash)


def box(x: float, y: float, width: float, height: float,
        title_value: str, details: Sequence[str], accent: str = BLUE,
        fill: str = PAPER, title_size: int = 21,
        detail_size: int = 15) -> str:
    """Return a labeled process card."""
    body = [rect(x, y, width, height, fill, accent, 2.0, 15.0,
                 "card-shadow")]
    body.append(rect(x, y, 7, height, accent, radius=3.5))
    body.append(text(x + 22, y + 34, title_value,
                     title_size, 760, accent))
    body.append(text_lines(x + 22, y + 65, details,
                           detail_size, detail_size + 7, 450, MUTED))
    return "".join(body)


def panel(x: float, y: float, width: float, height: float,
          title_value: str, accent: str, fill: str = PAPER) -> str:
    """Return a quiet containing panel."""
    body = [rect(x, y, width, height, fill, GRID, 1.5, 20.0)]
    body.append(rect(x + 18, y + 18, 8, 24, accent, radius=4.0))
    body.append(text(x + 38, y + 39, title_value, 20, 760, INK))
    return "".join(body)


def pill(x: float, y: float, width: float, label: str,
         accent: str, fill: str) -> str:
    """Return a compact direct label."""
    body = [rect(x, y, width, 38, fill, accent, 1.5, 19.0)]
    body.append(text(x + width / 2, y + 25, label,
                     14, 700, accent, "middle"))
    return "".join(body)


def header(width: int, title_value: str, subtitle: str,
           view_label: str) -> list[str]:
    """Return the shared figure heading."""
    body = [rect(0, 0, width, 5000, PANEL)]
    body.append(text(42, 60, title_value, 40, 820, INK))
    body.append(text(42, 92, subtitle, 19, 550, MUTED))
    body.append(text(width - 42, 60, view_label,
                     15, 760, BLUE, "end"))
    return body


def loader_chain_wide() -> str:
    """Return the landscape loader plan and selection flow."""
    width = 1760
    height = 1080
    body = header(width,
                  "How an image loader is selected",
                  "-L builds a chain; each backend owns its decode boundary",
                  "LOADER MAP 1 / 2")

    body.append(panel(35, 125, 1690, 235,
                      "1. Construct an ordered component chain", BLUE))
    body.append(box(70, 190, 270, 115, "-L entries",
                    ("explicit order", "unique prefixes + suboptions"),
                    BLUE, BLUE_LIGHT, 20, 14))
    body.append(box(415, 190, 270, 115, "Parse + deduplicate",
                    ("first occurrence wins", "environment, then CLI values"),
                    BLUE, BLUE_LIGHT, 20, 14))
    body.append(box(780, 190, 330, 115, "Append compiled defaults",
                    ("registry order", "unless the complete list ends in !"),
                    BLUE, BLUE_LIGHT, 20, 14))
    body.append(box(1210, 190, 430, 115, "Candidate chain",
                    ("libpng -> ... -> builtin -> ...", "build-dependent; builtin always present"),
                    TEAL, TEAL_LIGHT, 20, 14))
    body.append(arrow("M 340 248 H 415"))
    body.append(arrow("M 685 248 H 780"))
    body.append(arrow("M 1110 248 H 1210"))
    body.append(arrow("M 550 305 V 328 H 1405 V 305",
                      RED, 3.5, "8 6"))
    body.append(pill(820, 315, 270, "trailing ! bypasses append", RED,
                     RED_LIGHT))

    body.append(panel(35, 390, 1690, 450,
                      "2. Probe and invoke candidates in chain order", PURPLE))
    body.append(box(70, 535, 240, 120, "Next candidate",
                    ("component instance", "with resolved options"),
                    BLUE, BLUE_LIGHT, 20, 14))
    body.append(box(390, 535, 270, 120, "Predicate",
                    ("does the byte prefix", "look applicable?"),
                    BLUE, SLATE_LIGHT, 20, 14))
    body.append(box(755, 495, 360, 205, "Backend-owned load",
                    ("decode + metadata precedence", "CMS when supported", "animation + orientation", "alpha finalization"),
                    PURPLE, PURPLE_LIGHT, 20, 14))
    body.append(box(1215, 440, 430, 120, "SIXEL_OK: typed frame",
                    ("pixelformat + colorspace + timing", "palette and transparency metadata"),
                    TEAL, TEAL_LIGHT, 20, 14))
    body.append(box(1215, 595, 430, 105, "Allowed decode failure",
                    ("reject this candidate", "continue through the chain"),
                    RED, RED_LIGHT, 20, 14))
    body.append(box(1215, 735, 430, 75, "Terminal failure",
                    ("return immediately; do not retry",),
                    RED, PAPER, 20, 14))
    body.append(arrow("M 310 595 H 390"))
    body.append(arrow("M 660 595 H 755"))
    body.append(pill(670, 548, 75, "yes", BLUE, BLUE_LIGHT))
    body.append(arrow("M 525 655 V 760 H 190 V 655", RED, 3.5, "8 6"))
    body.append(pill(330, 718, 130, "no: skip", RED, RED_LIGHT))
    body.append(arrow("M 1115 545 H 1215", TEAL))
    body.append(arrow("M 1115 630 H 1215", RED))
    body.append(arrow("M 1115 675 V 772 H 1215", RED))
    body.append(arrow("M 1215 648 H 1160 V 795 H 190 V 655",
                      RED, 3.5, "8 6"))

    body.append(panel(35, 870, 1690, 160,
                      "3. The handoff is semantic, not a universal RGBA array",
                      TEAL, TEAL_LIGHT))
    labels = (
        (75, 270, "PAL + indexed pixels"),
        (385, 270, "RGB888 + mask"),
        (695, 320, "RGBFLOAT32 / typed float"),
        (1055, 245, "temporary RGBA"),
        (1340, 330, "timing + orientation"),
    )
    for x, pill_width, label in labels:
        body.append(pill(x, 940, pill_width, label, TEAL, PAPER))
    body.append(text(75, 1005,
                     "Source depth, palette retention, CMS, alpha policy, and backend capabilities decide the concrete frame.",
                     16, 620, MUTED))

    return svg_document(
        width,
        height,
        "How an image loader is selected",
        "Explicit loader entries are deduplicated and followed by compiled default-enabled loaders unless the list ends in an exclamation mark. Each candidate is predicate-gated and owns its decoding, metadata, color management, animation, orientation, and alpha work. Successful decoding returns a typed semantic frame; only recognized mismatch and decoder-family statuses permit fallback.",
        "\n".join(body),
    )


def loader_chain_mobile() -> str:
    """Return the portrait loader plan and selection flow."""
    width = 760
    height = 2280
    body = header(width,
                  "Loader selection",
                  "Chain construction, fallback, typed output",
                  "MAP 1 / 2")

    body.append(panel(30, 120, 700, 570, "1. Build the chain", BLUE))
    body.append(box(105, 180, 550, 105, "-L entries",
                    ("explicit order; unique prefixes; suboptions",),
                    BLUE, BLUE_LIGHT, 20, 15))
    body.append(arrow("M 380 285 V 330"))
    body.append(box(105, 330, 550, 115, "Parse + deduplicate",
                    ("first occurrence wins", "environment defaults, then explicit values"),
                    BLUE, BLUE_LIGHT, 20, 15))
    body.append(arrow("M 380 445 V 490"))
    body.append(box(105, 490, 550, 115, "Append compiled defaults",
                    ("registry order unless final !", "builtin is always registered"),
                    BLUE, BLUE_LIGHT, 20, 15))
    body.append(arrow("M 380 605 V 640"))
    body.append(pill(210, 635, 340, "final ! closes the chain", RED,
                     RED_LIGHT))

    body.append(panel(30, 730, 700, 990, "2. Try candidates", PURPLE))
    body.append(box(105, 790, 550, 105, "Next candidate",
                    ("component + resolved options",),
                    BLUE, BLUE_LIGHT, 20, 15))
    body.append(arrow("M 380 895 V 940"))
    body.append(box(105, 940, 550, 105, "Predicate matches?",
                    ("false skips the decoder",),
                    BLUE, SLATE_LIGHT, 20, 15))
    body.append(arrow("M 380 1045 V 1090"))
    body.append(pill(405, 1052, 90, "yes", BLUE, BLUE_LIGHT))
    body.append(box(105, 1090, 550, 175, "Backend-owned load",
                    ("decode + metadata precedence", "CMS + animation + orientation", "alpha finalization when supported"),
                    PURPLE, PURPLE_LIGHT, 20, 15))
    body.append(text(105, 1300, "One of three decode outcomes:",
                     16, 700, MUTED))
    body.append(box(105, 1325, 550, 105, "SIXEL_OK",
                    ("return a typed semantic frame",),
                    TEAL, TEAL_LIGHT, 20, 15))
    body.append(box(105, 1465, 550, 105, "Allowed decode failure",
                    ("advance to the next candidate",),
                    RED, RED_LIGHT, 20, 15))
    body.append(arrow("M 105 1517 H 70 V 842 H 105",
                      RED, 3.5, "8 6"))
    body.append(pill(70, 1060, 160, "no: next", RED, RED_LIGHT))
    body.append(arrow("M 105 1010 H 70 V 842 H 105",
                      RED, 3.5, "8 6"))
    body.append(box(105, 1605, 550, 75, "Terminal failure: stop",
                    ("allocation, cancellation, arguments, overflow, ...",),
                    RED, PAPER, 18, 13))

    body.append(panel(30, 1750, 700, 455,
                      "3. Output contract", TEAL, TEAL_LIGHT))
    outputs = (
        (1820, "PAL + indexed pixels"),
        (1890, "RGB888 + separate transparency"),
        (1960, "RGBFLOAT32 or typed float32"),
        (2030, "temporary alpha-bearing layout"),
        (2100, "colorspace + timing + metadata"),
    )
    for y, label in outputs:
        body.append(pill(105, y, 550, label, TEAL, PAPER))
    body.append(text(105, 2188, "Never infer RGBA from the loader name.",
                     16, 700, MUTED))

    return svg_document(
        width,
        height,
        "Loader selection",
        "The loader order becomes an explicit and default-appended candidate chain. Candidates are predicate-gated, and each backend owns decoding and local normalization. A successful candidate returns a typed frame, an allowed decoder failure tries the next candidate, and terminal failures stop.",
        "\n".join(body),
    )


def builtin_lineage_wide() -> str:
    """Return the landscape builtin-loader lineage and current split."""
    width = 1760
    height = 1160
    body = header(width,
                  "Builtin evolved beyond one stb_image wrapper",
                  "Feature and precision pressure extracted dedicated format families",
                  "LOADER MAP 2 / 2")

    body.append(panel(35, 125, 1690, 210,
                      "Historical pressure points", GOLD))
    timeline = (
        (80, 260, "2014", "stb_image 1.33", ("dependency-free", "multi-format baseline"), BLUE, BLUE_LIGHT),
        (430, 280, "2015", "fromgif", ("multi-frame GIF", "timing + disposal"), PURPLE, PURPLE_LIGHT),
        (830, 330, "2026", "precision work", ("PNG16 + JPEG float", "explicit colorspaces"), GOLD, GOLD_LIGHT),
        (1270, 380, "2026", "format subsystems", ("HDR / PSD / BMP / WebP", "metadata + animation"), PURPLE, PURPLE_LIGHT),
    )
    for x, box_width, year, title_value, details, accent, fill in timeline:
        body.append(text(x, 178, year, 15, 800, accent))
        body.append(box(x, 195, box_width, 105, title_value,
                        details, accent, fill, 19, 13))
    body.append(arrow("M 340 247 H 430", BLUE, 3.5))
    body.append(arrow("M 710 247 H 830", GOLD, 3.5))
    body.append(arrow("M 1160 247 H 1270", PURPLE, 3.5))

    body.append(panel(35, 365, 1690, 565,
                      "Current builtin dispatcher", PURPLE))
    body.append(box(70, 445, 310, 120, "builtin candidate",
                    ("byte-based dispatch", "common options + finalization"),
                    BLUE, BLUE_LIGHT, 20, 14))
    body.append(box(465, 420, 315, 155, "Direct top-level families",
                    ("SIXEL", "PNM / PAM", "GIF -> fromgif", "WebP -> fromwebp"),
                    PURPLE, PURPLE_LIGHT, 20, 14))
    body.append(box(465, 625, 315, 155, "Adapted stb route",
                    ("recognition + shared helpers", "JPEG modified float core", "TGA / PIC", "selected PNG / zlib helpers"),
                    BLUE, BLUE_LIGHT, 20, 14))
    body.append(arrow("M 380 500 H 465", BLUE))
    body.append(arrow("M 380 520 H 425 V 702 H 465", BLUE))

    modules = (
        (870, 415, 330, 120, "fromgif / fromwebp",
         ("animation state + metadata", "palette / ICC / alpha"), PURPLE),
        (1265, 415, 380, 120, "frompng",
         ("16-bit + APNG", "PNG metadata + background"), PURPLE),
        (870, 585, 330, 120, "modified JPEG core",
         ("float path before CMS", "avoid byte-rounding boundary"), BLUE),
        (1265, 585, 380, 120, "fromhdr / frompsd",
         ("radiance float", "depth, color modes, layers"), PURPLE),
        (870, 755, 330, 120, "frombmp",
         ("DIB + RLE + bitfields", "profiles + embedded payloads"), PURPLE),
        (1265, 755, 380, 120, "retained helpers",
         ("TGA / PIC", "palette and zlib machinery"), BLUE),
    )
    for x, y, box_width, box_height, title_value, details, accent in modules:
        fill = PURPLE_LIGHT if accent == PURPLE else BLUE_LIGHT
        body.append(box(x, y, box_width, box_height, title_value,
                        details, accent, fill, 18, 14))
    body.append(arrow("M 780 475 H 870", PURPLE))
    body.append(arrow("M 780 680 H 870", BLUE))
    body.append(arrow("M 780 650 H 805 V 545 H 1455 V 535", PURPLE))
    body.append(arrow("M 780 700 H 820 V 565 H 1455 V 585", PURPLE))
    body.append(arrow("M 780 720 H 835 V 815 H 870", PURPLE))
    body.append(arrow("M 780 740 H 1455 V 755", BLUE))
    body.append(pill(985, 895, 530,
                     "not every dedicated family is code-independent",
                     MUTED, SLATE_LIGHT))

    body.append(panel(35, 960, 1690, 150,
                      "The same ownership brings quality control and security cost",
                      RED, RED_LIGHT))
    body.append(pill(75, 1020, 465,
                     "preserve depth + explicit colorspace",
                     TEAL, TEAL_LIGHT))
    body.append(pill(625, 1020, 420,
                     "retain palette + animation semantics",
                     GOLD, GOLD_LIGHT))
    body.append(pill(1130, 1020, 550,
                     "more in-process parser states to audit and fuzz",
                     RED, PAPER))
    body.append(text(75, 1090,
                     "Dependency removal is one goal; it is not evidence that builtin is a sandbox or universally safer.",
                     16, 650, MUTED))

    return svg_document(
        width,
        height,
        "Builtin evolved beyond one stb_image wrapper",
        "The builtin loader started from stb_image 1.33, then extracted animated GIF and later high-depth or metadata-sensitive PNG, HDR, PSD, BMP, and WebP paths. JPEG keeps a modified stb-derived float core, while TGA, PIC, and shared helpers remain adapted. In-tree control preserves precision and semantics but expands the parser attack surface.",
        "\n".join(body),
    )


def builtin_lineage_mobile() -> str:
    """Return the portrait builtin-loader lineage and current split."""
    width = 760
    height = 2550
    body = header(width,
                  "Builtin lineage",
                  "From one stb integration to format families",
                  "MAP 2 / 2")

    body.append(panel(30, 120, 700, 660, "Historical pressure", GOLD))
    events = (
        (180, "2014 — stb_image 1.33", ("dependency-free multi-format baseline",), BLUE, BLUE_LIGHT),
        (325, "2015 — fromgif", ("multi-frame animation + disposal",), PURPLE, PURPLE_LIGHT),
        (470, "2026 — precision paths", ("PNG16 + modified JPEG float",), GOLD, GOLD_LIGHT),
        (615, "2026 — format subsystems", ("HDR / PSD / BMP / WebP",), PURPLE, PURPLE_LIGHT),
    )
    for y, title_value, details, accent, fill in events:
        body.append(box(105, y, 550, 100, title_value,
                        details, accent, fill, 19, 14))
        if y != 615:
            body.append(arrow(f"M 380 {y + 100} V {y + 145}", accent, 3.5))

    body.append(panel(30, 820, 700, 1340, "Current dispatcher", PURPLE))
    body.append(box(105, 880, 550, 110, "builtin candidate",
                    ("byte dispatch + common finalization",),
                    BLUE, BLUE_LIGHT, 20, 15))
    body.append(path("M 380 990 V 1025 H 80 V 2075", PURPLE, 3.0))
    body.append(pill(190, 1008, 380, "dispatches to one selected family",
                     PURPLE, PURPLE_LIGHT))
    current = (
        (1080, "Direct SIXEL + PNM/PAM", ("format-owned palette and alpha semantics",), PURPLE, PURPLE_LIGHT),
        (1225, "fromgif + fromwebp", ("animation, metadata, ICC, alpha",), PURPLE, PURPLE_LIGHT),
        (1370, "frompng", ("16-bit, APNG, metadata, background",), PURPLE, PURPLE_LIGHT),
        (1515, "Modified JPEG core", ("float path before color transforms",), BLUE, BLUE_LIGHT),
        (1660, "fromhdr + frompsd", ("radiance float; depth, modes, layers",), PURPLE, PURPLE_LIGHT),
        (1805, "frombmp", ("DIB, RLE, bitfields, profiles, payloads",), PURPLE, PURPLE_LIGHT),
        (1950, "Retained stb helpers", ("TGA, PIC, palette, zlib",), BLUE, BLUE_LIGHT),
    )
    for item in current:
        y, title_value, details, accent, fill = item
        body.append(arrow(f"M 80 {y + 52.5} H 105", accent, 3.0))
        body.append(box(105, y, 550, 105, title_value,
                        details, accent, fill, 19, 14))
    body.append(pill(105, 2090, 550,
                     "dedicated does not mean zero shared code",
                     MUTED, SLATE_LIGHT))

    body.append(panel(30, 2200, 700, 290,
                      "Ownership tradeoff", RED, RED_LIGHT))
    body.append(pill(105, 2260, 550,
                     "depth + colorspace + palette control",
                     TEAL, TEAL_LIGHT))
    body.append(pill(105, 2330, 550,
                     "more parser states to audit and fuzz",
                     RED, PAPER))
    body.append(text(105, 2435, "Builtin is not a sandbox.",
                     17, 800, RED))
    body.append(text(105, 2465, "Dependency removal is not proof of safety.",
                     15, 620, MUTED))

    return svg_document(
        width,
        height,
        "Builtin lineage",
        "The builtin loader evolved from stb_image into dedicated and modified format paths. This permits precision, colorspace, palette, and animation control while increasing the amount of in-process parser code that must be audited and fuzzed.",
        "\n".join(body),
    )


def manifest() -> dict[str, object]:
    """Return figure provenance and semantic color roles."""
    return {
        "generator": "tools/plot_loader_architecture_figures.py",
        "assets": list(ASSET_NAMES[:-1]),
        "sources": {
            "chain": [
                "src/loader-manager.c:g_sixel_loader_entries",
                "src/loader-manager.c:loader_manager_build_plan_from_resolution",
                "src/loader-manager.c:sixel_loader_manager_load_impl",
                "src/options-registry.c:SIXEL_OPTION_SCHEMA_LOADERS",
            ],
            "builtin": [
                "src/loader-builtin.c",
                "src/stb_image.h",
                "src/fromgif.c",
                "src/frompng.c",
                "src/frompnm.c",
                "src/fromhdr.c",
                "src/frompsd.c",
                "src/frombmp.c",
                "src/fromwebp.c",
            ],
            "history": [
                "81375f0bd",
                "30b80e140",
                "1d92429d7",
                "ca38e5e4a",
                "44045c67b",
                "158bb61ea",
                "8517810cc",
                "2a743d4b2",
                "e0d479e64",
                "4210a8a7e",
                "379c90cf5",
                "6e0efa0bb",
            ],
        },
        "color_roles": {
            "manager_policy_and_retained_stb": BLUE,
            "backend_owned_or_dedicated_family": PURPLE,
            "accepted_typed_frame": TEAL,
            "quality_or_feature_pressure": GOLD,
            "fallback_or_security_warning": RED,
            "palette_related": MAGENTA,
        },
    }


def generate(output_dir: Path) -> None:
    """Generate every SVG and its provenance manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    figures = {
        "loader-chain-wide.svg": loader_chain_wide(),
        "loader-chain-mobile.svg": loader_chain_mobile(),
        "builtin-lineage-wide.svg": builtin_lineage_wide(),
        "builtin-lineage-mobile.svg": builtin_lineage_mobile(),
    }
    for name, content in figures.items():
        (output_dir / name).write_text(content, encoding="utf-8")
    (output_dir / "loader-architecture-figures.json").write_text(
        json.dumps(manifest(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def check(output_dir: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(
            prefix="loader-architecture-figures-") as temp:
        generated = Path(temp)
        generate(generated)
        mismatches = []
        for name in ASSET_NAMES:
            expected = output_dir / name
            actual = generated / name
            if (not expected.is_file()
                    or expected.read_bytes() != actual.read_bytes()):
                mismatches.append(name)
        if mismatches:
            raise SystemExit(
                "loader architecture figures are stale or missing: "
                + ", ".join(mismatches)
            )


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=(source_root / "docs/loader"
                 / "loader-architecture-figures"),
        help="directory for generated SVG and manifest files",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare committed assets with a fresh deterministic generation",
    )
    return parser.parse_args()


def main() -> None:
    """Generate or verify the image-loader architecture assets."""
    arguments = parse_args()
    if arguments.check:
        check(arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
