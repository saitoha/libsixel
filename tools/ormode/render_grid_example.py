#!/usr/bin/env python3
"""Draw the recorded indexed example and its actual encoder paint commands."""

import json
import re
from html import escape
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "docs/functionality/or-mode/grid-example"
DATA = json.loads((OUT / "example.json").read_text())
ROWS = DATA["rows"]
COLORS = ["#" + "".join(f"{c:02x}" for c in rgb) for rgb in DATA["palette"]]


def render(mobile):
    width = 480 if mobile else 960
    parts = []

    def text(
        x, y, value, size=18, bold=False, color="#172b40", anchor="start", mono=False
    ):
        family = "monospace" if mono else "Arial, sans-serif"
        parts.append(
            f'<text x="{x}" y="{y}" font-family="{family}" font-size="{size}" font-weight="{700 if bold else 400}" fill="{color}" text-anchor="{anchor}">{escape(str(value))}</text>'
        )

    def rect(x, y, w, h, fill, stroke="#ccd6e0", radius=0):
        parts.append(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}" stroke="{stroke}"/>'
        )

    text(20, 32, "A real 3 × 6 image → SIXEL output", 25, True)
    text(20, 59, "18 pixels · 8 palette entries · fast encoding policy", 17)
    for r, row in enumerate(ROWS):
        for c, index in enumerate(row):
            rect(24 + c * 32, 92 + r * 32, 32, 32, COLORS[index])
            text(40 + c * 32, 114 + r * 32, index, 18, True, anchor="middle")
    text(24, 82, "Input pixels", 16, True)
    text(150, 91, "Palette: number → color", 18, True)
    for i in range(8):
        x = 150 + (i % 4) * 66
        y = 108 + (i // 4) * 55
        rect(x, y, 48, 36, COLORS[i], radius=4)
        text(x + 24, y + 25, i, 19, True, anchor="middle")
    text(150, 240, "Each square is one pixel.", 17)
    text(150, 267, "The numbers are color labels.", 17)
    y = 325
    text(20, y, "1  Normal: send one mask per color", 23, True)
    text(20, y + 27, "Each colored square is painted; dots are skipped.", 16)
    # The records preserve the encoder's real ordering and omitted trailing zeros.
    start_or = 0
    final = 0
    for mode, columns, start in [("normal", 2 if mobile else 4, y + 45), ("or", 3, 0)]:
        if mode == "or":
            y = start_or
            text(20, y, "2  OR: send one mask per index bit", 23, True)
            text(20, y + 27, "Teal squares turn on a bit; dots leave it unchanged.", 16)
            start = y + 45
        # Restrict matches to pattern bytes, excluding controls and selectors.
        tokens = re.findall(
            r"#(\d+)([?-~]+?)(?=\$|#|$|-)", DATA["outputs"][mode]["body"]
        )
        card_width = (width - 40) // columns
        for n, (selector, pattern) in enumerate(tokens):
            selector = int(selector)
            pattern = pattern.rstrip("$-")
            assert len(pattern) <= 3
            masks = [ord(c) - 63 for c in pattern] + [0] * (3 - len(pattern))
            expected = [
                sum(
                    (1 << r)
                    for r in range(6)
                    if (
                        bool(ROWS[r][c] & selector)
                        if mode == "or"
                        else ROWS[r][c] == selector
                    )
                )
                for c in range(3)
            ]
            assert masks == expected, (mode, selector, pattern, masks, expected)
            x = 20 + (n % columns) * card_width
            top = start + (n // columns) * 226
            rect(x, top, card_width - 10, 213, "#f4f7fa", radius=8)
            text(
                x + 12,
                top + 25,
                f"Bit {selector}" if mode == "or" else f"Color {selector}",
                18,
                True,
            )
            gx = x + 12
            gy = top + 38
            for r in range(6):
                for c in range(3):
                    on = bool(masks[c] & (1 << r))
                    rect(
                        gx + c * 25,
                        gy + r * 23,
                        25,
                        23,
                        "#087f83"
                        if mode == "or" and on
                        else COLORS[selector]
                        if on
                        else "#ffffff",
                    )
                    text(
                        gx + 12.5 + c * 25,
                        gy + 17 + r * 23,
                        "1" if on else "·",
                        15,
                        False,
                        "white" if mode == "or" and on else "#172b40",
                        anchor="middle",
                    )
            for c, char in enumerate(pattern):
                text(
                    gx + 12.5 + c * 25,
                    top + 197,
                    char,
                    19,
                    True,
                    anchor="middle",
                    mono=True,
                )
            text(
                x + 103 if card_width > 180 else x + 96,
                top + 61,
                f"#{selector}",
                16,
                True,
                mono=True,
            )
        bottom = start + ((len(tokens) + columns - 1) // columns) * 226
        text(20, bottom + 6, "Actual body (read left to right):", 17, True)
        body = DATA["outputs"][mode]["body"]
        if mode == "normal" and mobile:
            fragments = body.split("$")
            lines = ["$".join(fragments[:4]) + "$", "$".join(fragments[4:])]
        else:
            lines = [body]
        for i, line in enumerate(lines):
            text(20, bottom + 33 + i * 24, line, 16, mono=True)
        end = bottom + 33 + (len(lines) - 1) * 24
        text(
            20,
            end + 29,
            f"{DATA['outputs'][mode]['body_bytes']} body bytes · {DATA['outputs'][mode]['total_bytes']} bytes including header and palette",
            16,
        )
        if mode == "normal":
            start_or = end + 81
        else:
            final = end + 71
    text(20, final, "How the first OR column becomes “i”", 22, True)
    text(20, final + 29, "Bit 1, top to bottom: 0  1  0  1  0  1", 18)
    text(20, final + 56, "Row weights:              1  2  4  8 16 32", 17, mono=True)
    text(20, final + 84, "Set rows: 2 + 8 + 32 = 42; 42 + 63 = 105 (“i”).", 17)
    text(20, final + 113, "Both streams decode to exactly the input colors.", 18, True)
    height = final + 138
    opening = f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc"><title id="title">An indexed grid and its actual normal and OR SIXEL output</title><desc id="desc">The original 3 by 6 grid is split into eight color masks or three index-bit masks. Below each mask column is its emitted SIXEL character. The full recorded bodies contain 45 normal bytes and 19 OR bytes; both decode exactly.</desc><rect width="100%" height="100%" fill="white"/>'
    return opening + "\n" + "\n".join(parts) + "\n</svg>\n"


if __name__ == "__main__":
    for mobile in (False, True):
        (OUT / ("grid-mobile.svg" if mobile else "grid.svg")).write_text(render(mobile))
