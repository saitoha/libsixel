#!/usr/bin/env python3
"""Generate deterministic introductory encoding-pipeline figures."""

from __future__ import annotations

import argparse
import html
import json
import tempfile
from pathlib import Path
from typing import Callable, Sequence


INK = "#172033"
MUTED = "#556176"
PAPER = "#FFFFFF"
PANEL = "#F8FAFC"
GRID = "#CBD5E1"
BLUE = "#2563EB"
BLUE_LIGHT = "#DBEAFE"
MAGENTA = "#BE185D"
MAGENTA_LIGHT = "#FCE7F3"
GOLD = "#D97706"
GOLD_LIGHT = "#FEF3C7"
TEAL = "#0F766E"
TEAL_LIGHT = "#CCFBF1"
SCREEN = "#111827"

ASSET_NAMES = (
    "encoding-pipeline-wide.svg",
    "encoding-pipeline-mobile.svg",
    "encoding-pipeline-figures.json",
)


def attrs(**values: object) -> str:
    """Return escaped SVG attributes."""
    parts = []
    for name, value in values.items():
        if value is None:
            continue
        parts.append(
            f'{name.replace("_", "-")}="{html.escape(str(value), quote=True)}"'
        )
    return " ".join(parts)


def tag(name: str, body: str = "", **values: object) -> str:
    """Return one SVG element."""
    attribute_text = attrs(**values)
    if body:
        return f"<{name} {attribute_text}>{body}</{name}>"
    return f"<{name} {attribute_text}/>"


def rect(x: float, y: float, width: float, height: float, fill: str,
         stroke: str = "none", stroke_width: float = 0.0,
         radius: float = 0.0, filter_id: str | None = None) -> str:
    """Return a rectangle."""
    return tag(
        "rect",
        x=f"{x:.1f}",
        y=f"{y:.1f}",
        width=f"{width:.1f}",
        height=f"{height:.1f}",
        rx=f"{radius:.1f}" if radius else None,
        fill=fill,
        stroke=stroke,
        stroke_width=f"{stroke_width:.1f}" if stroke_width else None,
        filter=f"url(#{filter_id})" if filter_id else None,
    )


def line(x1: float, y1: float, x2: float, y2: float, stroke: str,
         width: float = 3.0, marker: str | None = None,
         dash: str | None = None) -> str:
    """Return a line, optionally with an arrow marker."""
    return tag(
        "line",
        x1=f"{x1:.1f}",
        y1=f"{y1:.1f}",
        x2=f"{x2:.1f}",
        y2=f"{y2:.1f}",
        stroke=stroke,
        stroke_width=f"{width:.1f}",
        stroke_linecap="round",
        stroke_dasharray=dash,
        marker_end=f"url(#{marker})" if marker else None,
    )


def path(data: str, stroke: str, width: float = 3.0,
         marker: str | None = None, dash: str | None = None) -> str:
    """Return an unfilled path."""
    return tag(
        "path",
        d=data,
        fill="none",
        stroke=stroke,
        stroke_width=f"{width:.1f}",
        stroke_linecap="round",
        stroke_linejoin="round",
        stroke_dasharray=dash,
        marker_end=f"url(#{marker})" if marker else None,
    )


def text(x: float, y: float, value: str, size: int = 22,
         weight: int = 400, fill: str = INK,
         anchor: str = "start") -> str:
    """Return one text label."""
    return tag(
        "text",
        html.escape(value),
        x=f"{x:.1f}",
        y=f"{y:.1f}",
        fill=fill,
        font_size=size,
        font_weight=weight,
        font_family="system-ui, -apple-system, sans-serif",
        text_anchor=anchor,
    )


def text_lines(x: float, y: float, values: Sequence[str], size: int,
               line_height: float, weight: int = 400,
               fill: str = INK, anchor: str = "start") -> str:
    """Return vertically stacked text labels."""
    return "".join(
        text(x, y + index * line_height, value, size, weight, fill, anchor)
        for index, value in enumerate(values)
    )


def defs() -> str:
    """Return shared markers and a quiet card shadow."""
    marker_body = tag(
        "path",
        d="M 0 0 L 10 5 L 0 10 z",
        fill="context-stroke",
    )
    markers = "".join(
        tag(
            "marker",
            marker_body,
            id=name,
            viewBox="0 0 10 10",
            refX="9",
            refY="5",
            markerWidth="7",
            markerHeight="7",
            orient="auto-start-reverse",
        )
        for name in ("arrow-blue", "arrow-magenta", "arrow-gold")
    )
    shadow = (
        '<filter id="card-shadow" x="-10%" y="-10%" width="120%" '
        'height="130%">'
        '<feDropShadow dx="0" dy="5" stdDeviation="7" '
        'flood-color="#172033" flood-opacity="0.10"/>'
        '</filter>'
    )
    return tag("defs", markers + shadow)


def pixel_grid(x: float, y: float, columns: int = 7,
               rows: int = 6, cell: float = 13.0) -> str:
    """Return a small deterministic raster motif."""
    colors = (BLUE, "#60A5FA", "#93C5FD", "#E0F2FE", TEAL,
              "#22C55E", GOLD)
    body = []
    for row in range(rows):
        for column in range(columns):
            color_index = (column * 2 + row * 3 + column * row) % len(colors)
            body.append(rect(x + column * cell,
                             y + row * cell,
                             cell - 1.5,
                             cell - 1.5,
                             colors[color_index],
                             radius=1.5))
    return "".join(body)


def source_icon(x: float, y: float) -> str:
    """Return a file containing raster pixels."""
    body = [
        path(f"M {x:.1f} {y:.1f} H {x + 68:.1f} L {x + 92:.1f} "
             f"{y + 24:.1f} V {y + 114:.1f} H {x:.1f} Z",
             INK, 3.0),
        path(f"M {x + 68:.1f} {y:.1f} V {y + 24:.1f} "
             f"H {x + 92:.1f}", INK, 3.0),
        pixel_grid(x + 15, y + 43, 5, 4, 12),
    ]
    return "".join(body)


def loader_icon(x: float, y: float) -> str:
    """Return ordered decode stages."""
    body = []
    for index, width in enumerate((84, 68, 52)):
        body.append(rect(x, y + index * 28, width, 20,
                         BLUE_LIGHT, BLUE, 2.0, 6.0))
    body.append(path(f"M {x + 10:.1f} {y + 94:.1f} H {x + 74:.1f}",
                     BLUE, 4.0, "arrow-blue"))
    return "".join(body)


def palette_icon(x: float, y: float) -> str:
    """Return representative palette swatches."""
    colors = (BLUE, "#60A5FA", TEAL, "#16A34A", GOLD, MAGENTA)
    body = []
    for index, color in enumerate(colors):
        column = index % 3
        row = index // 3
        body.append(rect(x + column * 34, y + row * 34,
                         27, 27, color, radius=5.0))
    body.append(path(f"M {x + 8:.1f} {y + 84:.1f} "
                     f"C {x + 35:.1f} {y + 110:.1f}, "
                     f"{x + 66:.1f} {y + 110:.1f}, "
                     f"{x + 94:.1f} {y + 84:.1f}", MAGENTA, 3.0))
    return "".join(body)


def apply_icon(x: float, y: float) -> str:
    """Return pixel mapping with an error-feedback loop."""
    body = [pixel_grid(x, y, 5, 4, 12)]
    body.append(path(f"M {x + 74:.1f} {y + 12:.1f} H {x + 108:.1f}",
                     BLUE, 3.0, "arrow-blue"))
    body.append(rect(x + 113, y + 1, 22, 22, MAGENTA, radius=4.0))
    body.append(path(f"M {x + 124:.1f} {y + 35:.1f} "
                     f"C {x + 124:.1f} {y + 67:.1f}, "
                     f"{x + 82:.1f} {y + 68:.1f}, "
                     f"{x + 82:.1f} {y + 48:.1f}", TEAL, 3.0,
                     "arrow-blue", "6 5"))
    return "".join(body)


def indexed_icon(x: float, y: float) -> str:
    """Return indexed pixels beside their palette."""
    body = [pixel_grid(x, y, 5, 5, 11)]
    colors = (BLUE, TEAL, GOLD, MAGENTA)
    for index, color in enumerate(colors):
        body.append(rect(x + 70, y + index * 18,
                         15, 15, color, radius=2.0))
    return "".join(body)


def sixel_icon(x: float, y: float) -> str:
    """Return six-pixel-high encoded bands."""
    body = []
    for column in range(8):
        height = 28 + (column % 4) * 12
        body.append(rect(x + column * 13,
                         y + 72 - height,
                         8,
                         height,
                         GOLD if column % 3 else BLUE,
                         radius=4.0))
    body.append(line(x, y + 82, x + 99, y + 82, INK, 2.0))
    return "".join(body)


def terminal_icon(x: float, y: float) -> str:
    """Return a terminal screen with SIXEL pixels."""
    body = [rect(x, y, 124, 104, SCREEN, radius=12.0)]
    body.append(pixel_grid(x + 17, y + 17, 7, 6, 12))
    body.append(rect(x + 30, y + 107, 64, 8, INK, radius=4.0))
    return "".join(body)


ICON_FUNCTIONS: dict[str, Callable[[float, float], str]] = {
    "source": source_icon,
    "loader": loader_icon,
    "pixels": pixel_grid,
    "palette": palette_icon,
    "apply": apply_icon,
    "indexed": indexed_icon,
    "sixel": sixel_icon,
    "terminal": terminal_icon,
}


def card(x: float, y: float, width: float, height: float,
         title: Sequence[str], detail: Sequence[str], icon: str,
         accent: str = BLUE, fill: str = PAPER,
         icon_x: float | None = None, icon_y: float | None = None,
         compact: bool = False) -> str:
    """Return a process or artifact card."""
    body = [rect(x, y, width, height, fill, accent, 2.0, 18.0,
                 "card-shadow")]
    body.append(rect(x, y, 8, height, accent, radius=4.0))
    resolved_icon_x = icon_x if icon_x is not None else x + 28
    resolved_icon_y = icon_y if icon_y is not None else y + 36
    body.append(ICON_FUNCTIONS[icon](resolved_icon_x, resolved_icon_y))
    if compact:
        label_x = x + 180
        label_y = y + 58
        body.append(text_lines(label_x, label_y, title,
                               29, 36, 750, accent))
        detail_y = label_y + len(title) * 36 + 18
        body.append(text_lines(label_x, detail_y, detail,
                               21, 28, 450, MUTED))
    else:
        title_size = 20 if width < 190 else 22
        detail_size = 14 if width < 190 else 16
        label_x = x + width / 2
        label_y = y + 185
        body.append(text_lines(label_x, label_y, title,
                               title_size, 27, 750, accent, "middle"))
        detail_y = label_y + len(title) * 27 + 14
        body.append(text_lines(label_x, detail_y, detail,
                               detail_size, 22, 450, MUTED, "middle"))
    return "".join(body)


def svg_document(width: int, height: int, title_value: str,
                 description: str, body: str) -> str:
    """Wrap an SVG body with accessible metadata."""
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
        f'height="{height}" viewBox="0 0 {width} {height}" role="img" '
        'aria-labelledby="figure-title figure-description">\n'
        f'<title id="figure-title">{html.escape(title_value)}</title>\n'
        f'<desc id="figure-description">{html.escape(description)}</desc>\n'
        f'{defs()}\n{body}\n</svg>\n'
    )


def wide_figure() -> str:
    """Return the landscape introductory mental model."""
    width = 1760
    height = 830
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(45, 62, "From image bytes to terminal pixels",
                     42, 800, INK))
    body.append(text(45, 94,
                     "The fixed-palette mental model",
                     20, 600, MUTED))
    body.append(text(1715, 62,
                     "INTRODUCTORY VIEW",
                     16, 750, BLUE, "end"))
    body.append(text(1715, 88,
                     "not an execution graph",
                     17, 500, MUTED, "end"))

    card_y = 345
    card_height = 285
    body.append(card(35, card_y, 185, card_height,
                     ("Source image", "bytes"),
                     ("Encoded image", "or caller pixels"),
                     "source", BLUE, PAPER, 78, card_y + 34))
    body.append(card(250, card_y, 190, card_height,
                     ("Loader",),
                     ("Decode frames", "and metadata"),
                     "loader", BLUE, PAPER, 294, card_y + 42))
    body.append(card(470, card_y, 220, card_height,
                     ("Normalized", "pixels"),
                     ("Resize / crop", "alpha / color"),
                     "pixels", BLUE, PAPER, 515, card_y + 47))
    body.append(card(900, card_y, 225, card_height,
                     ("Palette", "application"),
                     ("Lookup + dither", "choose an index"),
                     "apply", BLUE, PAPER, 936, card_y + 45))
    body.append(card(1155, card_y, 220, card_height,
                     ("Indexed pixels", "+ palette"),
                     ("One index per", "output pixel"),
                     "indexed", TEAL, PAPER, 1202, card_y + 48))
    body.append(card(1405, card_y, 175, card_height,
                     ("SIXEL", "encoding"),
                     ("Serialize", "six-pixel bands"),
                     "sixel", GOLD, GOLD_LIGHT, 1440, card_y + 50))
    body.append(card(1610, card_y, 115, card_height,
                     ("Terminal", "pixels"),
                     ("Rendered", "output"),
                     "terminal", GOLD, PAPER, 1605, card_y + 48))

    main_y = 490
    for start, end in ((220, 250), (440, 470), (690, 900),
                       (1125, 1155), (1375, 1405), (1580, 1610)):
        body.append(line(start, main_y, end, main_y,
                         BLUE if end < 1405 else GOLD,
                         5.0,
                         "arrow-blue" if end < 1405 else "arrow-gold"))

    body.append(rect(720, 135, 335, 170, MAGENTA_LIGHT,
                     MAGENTA, 2.0, 18.0, "card-shadow"))
    body.append(rect(720, 135, 8, 170, MAGENTA, radius=4.0))
    body.append(palette_icon(755, 184))
    body.append(text(865, 193, "Palette construction",
                     23, 750, MAGENTA))
    body.append(text_lines(865, 234,
                           ("Choose the colors", "available to the image"),
                           17, 24, 450, MUTED))
    body.append(path("M 580 345 V 270 H 720",
                     MAGENTA, 5.0, "arrow-magenta"))
    body.append(path("M 1055 220 H 1090 V 345 H 1010",
                     MAGENTA, 5.0, "arrow-magenta"))
    body.append(text(615, 255, "color samples", 17, 650, MAGENTA))
    body.append(text(1080, 205, "palette", 17, 650, MAGENTA, "end"))
    body.append(text(795, 512, "pixels", 17, 650, BLUE, "end"))

    body.append(rect(45, 685, 1670, 86, PAPER, GRID, 1.5, 16.0))
    body.append(text(880, 724,
                     "Palette construction decides which colors exist; palette application decides which index each pixel uses.",
                     22, 700, INK, "middle"))
    body.append(text(880, 753,
                     "Execution may overlap, repeat, fall back, or bypass stages; the implementation map expands those details.",
                     18, 500, MUTED, "middle"))
    return svg_document(
        width,
        height,
        "From image bytes to terminal pixels",
        "Introductory fixed-palette flow from source bytes through loading, normalization, palette construction, palette application, indexed pixels, and SIXEL encoding to terminal pixels.",
        "\n".join(body),
    )


def mobile_card(y: float, title: str, detail: str, icon: str,
                accent: str = BLUE, fill: str = PAPER) -> str:
    """Return one full-width mobile stage card."""
    return card(55, y, 650, 150,
                (title,), (detail,), icon, accent, fill,
                92, y + 34, True)


def mobile_figure() -> str:
    """Return the portrait introductory mental model."""
    width = 760
    height = 1660
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(40, 58, "Image to SIXEL", 42, 800, INK))
    body.append(text(40, 91, "Fixed-palette mental model",
                     19, 600, MUTED))
    body.append(text(720, 58, "INTRODUCTORY", 15, 750, BLUE, "end"))
    body.append(text(720, 84, "not an execution graph",
                     16, 500, MUTED, "end"))

    stages = (
        (125, "Source image bytes", "Encoded image or caller pixels",
         "source", BLUE, PAPER),
        (315, "Loader", "Decode frames and metadata",
         "loader", BLUE, PAPER),
        (505, "Normalized pixels", "Resize / crop / alpha / color",
         "pixels", BLUE, PAPER),
        (695, "Palette construction", "Choose the available colors",
         "palette", MAGENTA, MAGENTA_LIGHT),
        (885, "Palette application", "Lookup + dither choose an index",
         "apply", BLUE, PAPER),
        (1075, "Indexed pixels + palette", "One index per output pixel",
         "indexed", TEAL, PAPER),
        (1265, "SIXEL encoding", "Serialize six-pixel-high bands",
         "sixel", GOLD, GOLD_LIGHT),
        (1455, "Terminal pixels", "Interpret and render the SIXEL stream",
         "terminal", GOLD, PAPER),
    )
    for y, title_value, detail, icon, accent, fill in stages:
        body.append(mobile_card(y, title_value, detail,
                                icon, accent, fill))

    arrow_x = 380
    for start_y in (275, 465, 1035, 1225, 1415):
        marker = "arrow-gold" if start_y >= 1225 else "arrow-blue"
        stroke = GOLD if start_y >= 1225 else BLUE
        body.append(line(arrow_x, start_y, arrow_x, start_y + 35,
                         stroke, 5.0, marker))

    body.append(path("M 210 655 V 675 H 35 V 860 Q 35 875 50 875 "
                     "H 380 V 885",
                     BLUE, 4.0, "arrow-blue"))
    body.append(text(65, 865, "pixels", 17, 700, BLUE))
    body.append(path("M 550 655 V 695", MAGENTA, 4.0,
                     "arrow-magenta"))
    body.append(path("M 550 845 V 860 Q 550 875 535 875 H 495 V 885",
                     MAGENTA, 4.0, "arrow-magenta"))
    body.append(text(572, 855, "palette", 17, 700, MAGENTA))

    body.append(text(380, 1630,
                     "Conceptual ownership boundaries; execution details are expanded separately.",
                     17, 550, MUTED, "middle"))
    return svg_document(
        width,
        height,
        "Image to SIXEL",
        "Mobile introductory fixed-palette flow with separate pixel and palette paths joining at palette application.",
        "\n".join(body),
    )


def manifest() -> dict[str, object]:
    """Return provenance and semantic roles for the generated figures."""
    return {
        "schema": 1,
        "generator": "tools/plot_encoding_pipeline_figures.py",
        "kind": "deterministic introductory schematic",
        "measurement": False,
        "scope": "fixed-palette mental model, not the execution DAG",
        "assets": [
            {
                "path": "encoding-pipeline-wide.svg",
                "role": "large-screen introductory mental model",
            },
            {
                "path": "encoding-pipeline-mobile.svg",
                "role": "mobile portrait introductory mental model",
            },
        ],
        "color_roles": {
            "frame_data": BLUE,
            "palette_data": MAGENTA,
            "indexed_artifact": TEAL,
            "sixel_output": GOLD,
            "context": MUTED,
        },
        "stages": [
            "source-bytes",
            "loader",
            "normalized-pixels",
            "palette-construction",
            "palette-application",
            "indexed-pixels-and-palette",
            "sixel-encoding",
            "terminal-pixels",
        ],
    }


def generate(output_dir: Path) -> None:
    """Generate both SVG figures and their provenance manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "encoding-pipeline-wide.svg").write_text(
        wide_figure(), encoding="utf-8")
    (output_dir / "encoding-pipeline-mobile.svg").write_text(
        mobile_figure(), encoding="utf-8")
    (output_dir / "encoding-pipeline-figures.json").write_text(
        json.dumps(manifest(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8")


def check(output_dir: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(prefix="encoding-pipeline-figures-") as temp:
        generated = Path(temp)
        generate(generated)
        mismatches = []
        for name in ASSET_NAMES:
            expected = output_dir / name
            actual = generated / name
            if not expected.is_file() or expected.read_bytes() != actual.read_bytes():
                mismatches.append(name)
        if mismatches:
            raise SystemExit(
                "encoding-pipeline figures are stale or missing: "
                + ", ".join(mismatches)
            )


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=source_root / "docs/functionality/encoding-pipeline-figures",
        help="directory for generated SVG and manifest files",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare committed assets with a fresh deterministic generation",
    )
    return parser.parse_args()


def main() -> None:
    """Generate or verify the introductory encoding-pipeline assets."""
    arguments = parse_args()
    if arguments.check:
        check(arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
