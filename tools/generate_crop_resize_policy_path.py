#!/usr/bin/env python3
"""Generate responsive SVG diagrams for the crop/resize policy path."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import List, Tuple
from xml.sax.saxutils import escape


PRESERVE = "#E69F00"
LINEAR = "#0072B2"
FLOAT = "#CC79A7"
SIMD = "#008B8B"
MUTED = "#5E6672"
PANEL = "#F7F8FA"
STROKE = "#AAB2BD"


class Svg:
    """Build one small dependency-free SVG document."""

    def __init__(self, width: int, height: int, title: str, desc: str) -> None:
        self.width = width
        self.height = height
        self.parts: List[str] = [
            '<?xml version="1.0" encoding="UTF-8"?>',
            (
                f'<svg xmlns="http://www.w3.org/2000/svg" '
                f'viewBox="0 0 {width} {height}" role="img" '
                f'aria-labelledby="title desc">'
            ),
            f"<title id=\"title\">{escape(title)}</title>",
            f"<desc id=\"desc\">{escape(desc)}</desc>",
            "<defs>",
            (
                '<marker id="arrow" markerWidth="8" markerHeight="8" '
                'refX="7" refY="4" orient="auto" markerUnits="strokeWidth">'
                f'<path d="M0,0 L8,4 L0,8 z" fill="{MUTED}"/>'
                "</marker>"
            ),
            "</defs>",
            (
                "<style>"
                ".box{fill:#F7F8FA;stroke:#AAB2BD;stroke-width:2;}"
                ".title{font:700 24px system-ui,sans-serif;fill:#20242A;}"
                ".label{font:700 18px system-ui,sans-serif;fill:#20242A;}"
                ".body{font:15px system-ui,sans-serif;fill:#20242A;}"
                ".small{font:13px system-ui,sans-serif;fill:#5E6672;}"
                ".arrow{fill:none;stroke:#5E6672;stroke-width:2;"
                "marker-end:url(#arrow);}"
                "</style>"
            ),
            f'<rect width="{width}" height="{height}" fill="#FFFFFF"/>',
        ]

    def rect(
        self,
        x: int,
        y: int,
        width: int,
        height: int,
        stroke: str = STROKE,
        dash: str = "",
        fill: str = PANEL,
        radius: int = 10,
    ) -> None:
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        self.parts.append(
            f'<rect x="{x}" y="{y}" width="{width}" height="{height}" '
            f'rx="{radius}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="2"{dash_attr}/>'
        )

    def text(
        self,
        x: int,
        y: int,
        value: str,
        css_class: str = "body",
        anchor: str = "middle",
        fill: str = "",
    ) -> None:
        fill_attr = f' fill="{fill}"' if fill else ""
        self.parts.append(
            f'<text x="{x}" y="{y}" text-anchor="{anchor}" '
            f'class="{css_class}"{fill_attr}>{escape(value)}</text>'
        )

    def multiline(
        self,
        x: int,
        y: int,
        lines: List[str],
        css_class: str = "body",
        line_height: int = 20,
        fill: str = "",
    ) -> None:
        fill_attr = f' fill="{fill}"' if fill else ""
        self.parts.append(
            f'<text x="{x}" y="{y}" text-anchor="middle" '
            f'class="{css_class}"{fill_attr}>'
        )
        for index, line in enumerate(lines):
            dy = 0 if index == 0 else line_height
            self.parts.append(
                f'<tspan x="{x}" dy="{dy}">{escape(line)}</tspan>'
            )
        self.parts.append("</text>")

    def arrow(self, points: List[Tuple[int, int]]) -> None:
        data = " ".join(
            ("M" if index == 0 else "L") + f"{x},{y}"
            for index, (x, y) in enumerate(points)
        )
        self.parts.append(f'<path d="{data}" class="arrow"/>')

    def line(
        self,
        points: List[Tuple[int, int]],
        stroke: str = MUTED,
        dash: str = "",
        width: int = 2,
    ) -> None:
        data = " ".join(
            ("M" if index == 0 else "L") + f"{x},{y}"
            for index, (x, y) in enumerate(points)
        )
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        self.parts.append(
            f'<path d="{data}" fill="none" stroke="{stroke}" '
            f'stroke-width="{width}"{dash_attr}/>'
        )

    def finish(self) -> str:
        self.parts.append("</svg>")
        return "\n".join(self.parts) + "\n"


def add_stage(svg: Svg, x: int, y: int, width: int, title: str,
              subtitle: str) -> None:
    """Add one common pipeline stage."""
    svg.rect(x, y, width, 82)
    svg.text(x + width // 2, y + 31, title, "label")
    svg.text(x + width // 2, y + 57, subtitle, "small")


def add_policy_row(
    svg: Svg,
    y: int,
    color: str,
    dash: str,
    policy: str,
    before: List[str],
    resample: List[str],
    after: List[str],
) -> None:
    """Add one wide policy row."""
    svg.rect(390, y, 190, 110, color, dash, "#FFFFFF")
    svg.text(485, y + 31, policy, "label", fill=color)
    svg.multiline(485, y + 59, before, "small", 18)
    svg.rect(655, y, 330, 110, color, dash, "#FFFFFF")
    svg.multiline(820, y + 36, resample, "body", 22)
    svg.rect(1060, y, 330, 110, color, dash, "#FFFFFF")
    svg.multiline(1225, y + 36, after, "body", 22)
    svg.arrow([(580, y + 55), (655, y + 55)])
    svg.arrow([(985, y + 55), (1060, y + 55)])


def wide_svg() -> str:
    """Return the large-screen policy path."""
    svg = Svg(
        1880,
        690,
        "Resize precision policy path",
        (
            "The loaded frame is optionally cropped, routed through preserve, "
            "linear, or float resize representations, resampled, optionally "
            "converted to the working representation, optionally cropped after "
            "resize, and then sent to palette application and SIXEL encoding. "
            "The SIMD ceiling selects only eligible implementations and does "
            "not change the chosen resize policy."
        ),
    )
    svg.text(30, 45, "Resize precision policy path", "title", "start")
    svg.text(
        30,
        75,
        "Default RGB888 input and gamma working space; conditional stages are labeled.",
        "small",
        "start",
    )

    add_stage(svg, 30, 285, 150, "Loaded frame", "decoded pixels")
    add_stage(svg, 220, 285, 130, "Early crop", "when -c precedes -w/-h")
    svg.arrow([(180, 326), (220, 326)])
    svg.arrow([(350, 326), (370, 326), (370, 170), (390, 170)])
    svg.arrow([(370, 326), (390, 340)])
    svg.arrow([(370, 326), (370, 510), (390, 510)])

    add_policy_row(
        svg,
        115,
        PRESERVE,
        "10 6",
        "preserve",
        ["keep RGB888", "gamma-coded samples"],
        ["Resampling: selected kernel", "byte path: scalar or SIMD"],
        ["convert only if later work", "requests another format"],
    )
    add_policy_row(
        svg,
        285,
        LINEAR,
        "",
        "linear",
        ["promote to float32", "gamma -> linear RGB"],
        ["Resampling: selected kernel", "linear-f32: scalar or SIMD"],
        ["convert to working format", "normally RGB888"],
    )
    add_policy_row(
        svg,
        455,
        FLOAT,
        "3 5",
        "float",
        ["promote to float32", "gamma -> linear RGB"],
        ["Resampling: selected kernel", "linear-f32: scalar or SIMD"],
        ["retain float32 work", "RGB-f32 for -W gamma"],
    )

    svg.rect(625, 92, 390, 500, SIMD, "9 7", "none", 14)
    svg.text(820, 620, "SIMD ceiling", "label", fill=SIMD)
    svg.text(
        820,
        646,
        "Caps byte/float resampling kernels in the measured path.",
        "small",
        fill=SIMD,
    )

    svg.line([(1390, 170), (1450, 170), (1450, 326)])
    svg.line([(1390, 340), (1450, 340)])
    svg.line([(1390, 510), (1450, 510), (1450, 340)])
    add_stage(svg, 1480, 285, 150, "Late crop", "when -c follows -w/-h")
    add_stage(svg, 1670, 285, 180, "Palette / SIXEL", "common downstream path")
    svg.arrow([(1450, 340), (1450, 326), (1480, 326)])
    svg.arrow([(1630, 326), (1670, 326)])
    return svg.finish()


def add_mobile_policy(
    svg: Svg,
    y: int,
    color: str,
    dash: str,
    policy: str,
    lines: List[str],
) -> None:
    """Add one compact mobile policy description."""
    svg.rect(75, y, 610, 160, color, dash, "#FFFFFF", 12)
    svg.text(380, y + 35, policy, "label", fill=color)
    svg.multiline(380, y + 70, lines, "body", 23)


def mobile_svg() -> str:
    """Return the narrow-screen policy path."""
    svg = Svg(
        760,
        1540,
        "Resize precision policy path, narrow layout",
        (
            "The same resize policy path as the wide diagram, arranged as "
            "stacked preserve, linear, and float paths for narrow screens."
        ),
    )
    svg.text(30, 45, "Resize precision policy path", "title", "start")
    svg.text(30, 75, "Conditional stages remain explicit in the stacked view.",
             "small", "start")
    add_stage(svg, 240, 110, 280, "Loaded frame", "decoded pixels")
    add_stage(svg, 240, 225, 280, "Early crop", "when -c precedes -w/-h")
    svg.arrow([(380, 192), (380, 225)])
    svg.arrow([(380, 307), (380, 345)])
    svg.text(380, 375, "resize_precision selects one path", "label")

    add_mobile_policy(
        svg,
        415,
        PRESERVE,
        "10 6",
        "preserve",
        [
            "RGB888 gamma samples -> Resampling (selected byte kernel)",
            "-> convert only if later work requests another format",
        ],
    )
    add_mobile_policy(
        svg,
        600,
        LINEAR,
        "",
        "linear",
        [
            "promote + gamma -> linear-f32 -> Resampling",
            "-> convert to the working format, normally RGB888",
        ],
    )
    add_mobile_policy(
        svg,
        785,
        FLOAT,
        "3 5",
        "float",
        [
            "promote + gamma -> linear-f32 -> Resampling",
            "-> retain float32 work, RGB-f32 for -W gamma",
        ],
    )

    svg.rect(75, 975, 610, 135, SIMD, "9 7", "none", 14)
    svg.text(380, 1008, "SIMD ceiling", "label", fill=SIMD)
    svg.multiline(
        380,
        1038,
        [
            "Caps byte and float resampling kernels in this path.",
            "It does not change color-mixing policy or working precision.",
            "Measured promotion and float color conversion remain scalar.",
        ],
        "small",
        20,
        SIMD,
    )

    add_stage(svg, 240, 1150, 280, "Late crop", "when -c follows -w/-h")
    add_stage(svg, 220, 1275, 320, "Palette / SIXEL", "common downstream path")
    svg.arrow([(380, 1110), (380, 1150)])
    svg.arrow([(380, 1232), (380, 1275)])
    svg.multiline(
        380,
        1405,
        [
            "The selected resampling kernel is orthogonal to resize_precision.",
            "The measured matrix fixes bilinear and varies policy x SIMD ceiling.",
        ],
        "body",
        24,
    )
    return svg.finish()


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-directory", required=True, type=Path)
    return parser.parse_args()


def main() -> None:
    """Write the wide and narrow SVG variants."""
    args = parse_args()
    args.output_directory.mkdir(parents=True, exist_ok=True)
    (args.output_directory / "crop-resize-policy-path-wide.svg").write_text(
        wide_svg(), encoding="utf-8"
    )
    (args.output_directory / "crop-resize-policy-path-mobile.svg").write_text(
        mobile_svg(), encoding="utf-8"
    )


if __name__ == "__main__":
    main()
