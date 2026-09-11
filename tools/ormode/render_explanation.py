#!/usr/bin/env python3
"""Generate deterministic SVG explanations of the six-pixel OR example."""

from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/functionality/or-mode"
COLORS = ["#fff", "#f4ad9f", "#f8d777", "#a9d7a5", "#a9ceef", "#d2b5eb", "#f1bfdb"]
INK = "#172b40"
TEAL = "#087f83"


def render(mobile=False):
    width, height = (480, 1160) if mobile else (960, 760)
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">',
        '<title id="title">Six colors, three bit planes, the same pixels</title>',
        '<desc id="desc">A six-pixel column with palette indices 1 through 6. Ordinary painting uses six color masks. OR encoding uses three masks selected by bits 1, 2 and 4. For pixel E, bits 1 and 4 reconstruct index 5, which selects the original purple palette entry.</desc>',
    ]

    def box(x, y, w, h, fill, stroke="none", radius=8):
        parts.append(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}" stroke="{stroke}"/>'
        )

    def text(x, y, value, size=18, fill=INK, bold=False, anchor="start"):
        parts.append(
            f'<text x="{x}" y="{y}" font-family="Arial, sans-serif" font-size="{size}" font-weight="{700 if bold else 400}" fill="{fill}" text-anchor="{anchor}">{escape(str(value))}</text>'
        )

    box(0, 0, width, height, "#ffffff", radius=0)
    text(24, 36, "Same pixels. Different stencils.", 26, bold=True)
    text(24, 65, "Six colors → three bit planes", 20)
    text(24, 93, "Numbers identify palette entries; they are not RGB values.", 15)

    def panel(x, y, is_or):
        box(x, y, 440, 408, "#f4f7fa", "#dce4ec", 12)
        text(
            x + 16,
            y + 30,
            "2  OR: one stencil per bit"
            if is_or
            else "1  Normal: one stencil per color",
            22,
            bold=True,
        )
        text(
            x + 16,
            y + 57,
            "A filled cell turns on that index bit."
            if is_or
            else "A filled cell paints that palette color.",
            17,
        )
        text(x + 19, y + 92, "Pixel", 15)
        text(x + 71, y + 92, "Input", 15)
        selectors = [1, 2, 4] if is_or else list(range(1, 7))
        step = 78 if is_or else 44
        start = x + 152
        for col, selector in enumerate(selectors):
            cx = start + col * step
            text(
                cx + 17,
                y + 92,
                f"Bit {selector}" if is_or else f"#{selector}",
                16,
                bold=True,
                anchor="middle",
            )
            mask = 0
            for row, index in enumerate(range(1, 7)):
                on = bool(index & selector) if is_or else index == selector
                mask |= int(on) << row
                cy = y + 108 + row * 36
                box(
                    cx,
                    cy,
                    34,
                    32,
                    TEAL if is_or and on else COLORS[index] if on else "#ffffff",
                    "#ccd6e0",
                    4,
                )
                text(
                    cx + 17,
                    cy + 22,
                    "1" if on else "0",
                    17,
                    "#ffffff" if is_or and on else INK if on else "#667789",
                    on,
                    "middle",
                )
            text(cx + 17, y + 351, chr(63 + mask), 21, bold=True, anchor="middle")
        for row, index in enumerate(range(1, 7)):
            cy = y + 108 + row * 36
            text(x + 35, cy + 22, chr(65 + row), 17, anchor="middle")
            box(x + 76, cy, 34, 32, COLORS[index], "#bcc9d5", 4)
            text(x + 93, cy + 22, index, 17, bold=True, anchor="middle")
        text(x + 16, y + 350, "SIXEL chars →", 14)
        text(
            x + 16,
            y + 386,
            "3 pattern characters" if is_or else "6 pattern characters",
            21,
            TEAL if is_or else INK,
            True,
        )

    panel(20, 116, False)
    panel(20 if mobile else 500, 544 if mobile else 116, True)
    y = 974 if mobile else 548
    text(24, y, "3  Rebuild pixel E, then look up its color", 21, bold=True)
    # Bits stay neutral/teal; only the final palette lookup introduces color.
    items = [("Bit 1", "001"), ("Bit 2", "000"), ("Bit 4", "100")]
    for i, (label, bits) in enumerate(items):
        x = 24 + i * 110
        text(x + 35, y + 27, label, 16, anchor="middle")
        box(x, y + 36, 70, 44, "#e1f1f0", "#aacdcd")
        text(x + 35, y + 65, bits, 23, TEAL, True, "middle")
        if i < 2:
            text(x + 88, y + 65, "OR", 15, anchor="middle")
    text(366, y + 65, "= 101", 24, bold=True)
    text(24, y + 113, "101 in binary = index 5 → palette[5] →", 19)
    box(393, y + 87, 48, 36, COLORS[5], "#a58bbd")
    text(417, y + 112, "5", 20, bold=True, anchor="middle")
    text(24, y + 150, "Pattern counts exclude headers and other commands.", 15)
    text(24, y + 173, "This does not mean the complete file is half the size.", 15)
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


if __name__ == "__main__":
    for mobile in (False, True):
        name = "encoding-explained-mobile.svg" if mobile else "encoding-explained.svg"
        (OUT / name).write_text(render(mobile))
