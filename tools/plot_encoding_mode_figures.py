#!/usr/bin/env python3
"""Generate deterministic explanatory figures for -E and -I."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Callable, Iterable, Sequence

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
    line,
    path,
    rect,
    svg_document,
    tag,
    text,
    text_lines,
)


ENCODE_ASSETS = (
    "encode-policy-band-wide.svg",
    "encode-policy-band-mobile.svg",
    "encode-policy-overpaint-wide.svg",
    "encode-policy-overpaint-mobile.svg",
    "encode-policy-figures.json",
)
HIGH_COLOR_ASSETS = (
    "high-color-register-wide.svg",
    "high-color-register-mobile.svg",
    "high-color-passes-wide.svg",
    "high-color-passes-mobile.svg",
    "high-color-figures.json",
)


def circle(cx: float, cy: float, radius: float, fill: str,
           stroke: str = PAPER, stroke_width: float = 2.0) -> str:
    """Return a circle."""
    return tag(
        "circle",
        cx=f"{cx:.1f}",
        cy=f"{cy:.1f}",
        r=f"{radius:.1f}",
        fill=fill,
        stroke=stroke,
        stroke_width=f"{stroke_width:.1f}",
    )


def badge(x: float, y: float, value: str, fill: str,
          width: float = 98.0) -> str:
    """Return a compact labeled badge."""
    return (
        rect(x, y, width, 42, fill, radius=12)
        + text(x + width / 2, y + 28, value, 17, 750, PAPER, "middle")
    )


def code_text(x: float, y: float, value: str, size: int = 19,
              weight: int = 650, fill: str = INK,
              anchor: str = "start") -> str:
    """Return a monospace literal used for SIXEL stream fragments."""
    return tag(
        "text",
        value,
        x=f"{x:.1f}",
        y=f"{y:.1f}",
        fill=fill,
        font_size=size,
        font_weight=weight,
        font_family="ui-monospace, SFMono-Regular, Menlo, monospace",
        text_anchor=anchor,
    )


def card(x: float, y: float, width: float, height: float,
         title_value: str, accent: str = BLUE,
         fill: str = PAPER) -> str:
    """Return a labeled diagram card."""
    return (
        rect(x, y, width, height, fill, accent, 2.0, 18)
        + rect(x, y, 8, height, accent, radius=4)
        + text(x + 28, y + 42, title_value, 23, 750, accent)
    )


def draw_grid(x: float, y: float, rows: Sequence[str],
              cell: float = 30.0, gap: float = 3.0,
              outline: bool = False) -> str:
    """Draw a small pixel grid from B, G, M, dot, and hash tokens."""
    colors = {
        "B": BLUE,
        "G": GOLD,
        "T": TEAL,
        "M": MAGENTA,
        ".": PAPER,
        "#": "#334155",
        "x": "#94A3B8",
    }
    body = []
    for row_index, row in enumerate(rows):
        for column_index, token in enumerate(row):
            body.append(
                rect(
                    x + column_index * cell,
                    y + row_index * cell,
                    cell - gap,
                    cell - gap,
                    colors[token],
                    GRID if outline or token == "." else PAPER,
                    1.0,
                    3,
                )
            )
    return "".join(body)


def mask_rows(rows: Sequence[str], token: str) -> tuple[str, ...]:
    """Return a monochrome mask for one token."""
    return tuple(
        "".join("#" if value == token else "." for value in row)
        for row in rows
    )


FOUR_COLOR_ROWS = (
    "BBGGTTMM",
    "BBGGTTMM",
    "BGBGTMTM",
    "BGBGTMTM",
    "GGBBMMTT",
    "GGBBMMTT",
)

FAST_EXAMPLE_BODY = "#1o{BN#3o{BN$#0NB{o#2NB{o"
SIZE_EXAMPLE_BODY = "#1!4~#3!4~$#0NB{o#2NB{o"


def mask_literal_row(x: float, y: float, token: str, register: str,
                     label: str, literal: str, color: str) -> str:
    """Return one color mask and its literal emitted characters."""
    return (
        draw_grid(x, y, mask_rows(FOUR_COLOR_ROWS, token), 13, 2, True)
        + badge(x + 122, y + 12, register, color, 58)
        + text(x + 194, y + 38, label, 16, 700, color)
        + code_text(x + 302, y + 38, literal, 20, 750)
    )


def band_wide() -> str:
    """Explain one real four-color SIXEL paint body."""
    width = 1520
    height = 860
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(55, 62, "From four indexed colors to actual SIXEL characters", 38, 800),
        text(55, 100, "This 8 × 6 example is one complete band. The strings at right are literal encoder paint bodies.", 20, 500, MUTED),
        card(55, 150, 350, 545, "1  Indexed pixels", BLUE),
        draw_grid(78, 220, FOUR_COLOR_ROWS, 36),
        circle(95, 480, 9, BLUE),
        text(115, 487, "#0 blue", 17, 700, BLUE),
        circle(225, 480, 9, GOLD),
        text(245, 487, "#1 amber", 17, 700, GOLD),
        circle(95, 525, 9, TEAL),
        text(115, 532, "#2 green", 17, 700, TEAL),
        circle(225, 525, 9, MAGENTA),
        text(245, 532, "#3 pink", 17, 700, MAGENTA),
        text(78, 595, "Each column becomes a six-bit value.", 17, 600),
        code_text(78, 635, "? + bits = SIXEL character", 16, 650, MUTED),
        line(410, 420, 455, 420, BLUE, 5, "arrow-blue"),
        card(465, 150, 480, 545, "2  Exact color masks", TEAL),
        mask_literal_row(495, 215, "G", "#1", "amber", "o{BN", GOLD),
        mask_literal_row(495, 320, "M", "#3", "pink", "o{BN", MAGENTA),
        mask_literal_row(495, 425, "B", "#0", "blue", "NB{o", BLUE),
        mask_literal_row(495, 530, "T", "#2", "green", "NB{o", TEAL),
        text(495, 650, "o, {, B, and N are data characters, not labels.", 16, 600, MUTED),
        line(950, 420, 995, 420, GOLD, 5, "arrow-gold"),
        card(1005, 150, 460, 545, "3  Two valid paint plans", GOLD, GOLD_LIGHT),
        text(1040, 225, "auto / fast — exact masks", 18, 750, BLUE),
        rect(1038, 245, 390, 58, PAPER, BLUE, 1.5, 10),
        code_text(1055, 282, FAST_EXAMPLE_BODY, 16, 650),
        text(1040, 350, "size — fill, then repair", 18, 750, GOLD),
        rect(1038, 370, 390, 58, PAPER, GOLD, 1.5, 10),
        code_text(1055, 407, SIZE_EXAMPLE_BODY, 16, 650),
        code_text(1040, 490, "!4~", 24, 800, GOLD),
        text(1100, 489, "means: paint ~ four times", 17, 600, MUTED),
        code_text(1040, 535, "~", 24, 800, INK),
        text(1075, 534, "sets all six vertical bits", 17, 600, MUTED),
        code_text(1040, 580, "$", 24, 800, INK),
        text(1075, 579, "returns to the band's left edge", 17, 600, MUTED),
        text(1040, 638, "DCS wrapper and palette definitions omitted.", 15, 550, MUTED),
        rect(145, 745, 1230, 62, TEAL_LIGHT, TEAL, 1.5, 14),
        text(760, 784, "Both bodies decode to the same four-color pixels; size saves two bytes in this tiny example.", 20, 750, TEAL, "middle"),
    ])
    return svg_document(
        width,
        height,
        "Four indexed colors become literal SIXEL paint characters",
        "An eight by six four-color indexed grid is split into four masks. The exact-mask body is #1o{BN#3o{BN$#0NB{o#2NB{o. The size-policy body replaces two mask runs with #1!4~#3!4~ before repairing the blue and green pixels.",
        "\n".join(body),
    )


def band_mobile() -> str:
    """Return the mobile layout of the literal four-color example."""
    width = 760
    height = 1710
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(38, 58, "Pixels become real characters", 36, 800),
        text(38, 92, "One 8 × 6 band with four indexed colors.", 18, 500, MUTED),
        card(55, 135, 650, 365, "1  Four-color indexed band", BLUE),
        draw_grid(220, 205, FOUR_COLOR_ROWS, 40),
        text(90, 475, "#0 blue  •  #1 amber", 17, 700),
        text(390, 475, "#2 green  •  #3 pink", 17, 700),
        line(380, 510, 380, 555, BLUE, 5, "arrow-blue"),
        card(55, 565, 650, 465, "2  Four masks and their characters", TEAL),
        mask_literal_row(95, 640, "G", "#1", "amber", "o{BN", GOLD),
        mask_literal_row(95, 735, "M", "#3", "pink", "o{BN", MAGENTA),
        mask_literal_row(95, 830, "B", "#0", "blue", "NB{o", BLUE),
        mask_literal_row(95, 925, "T", "#2", "green", "NB{o", TEAL),
        line(380, 1040, 380, 1085, GOLD, 5, "arrow-gold"),
        card(55, 1095, 650, 405, "3  Literal encoder paint bodies", GOLD, GOLD_LIGHT),
        text(90, 1170, "auto / fast", 18, 750, BLUE),
        code_text(90, 1208, FAST_EXAMPLE_BODY, 16, 650),
        text(90, 1280, "size", 18, 750, GOLD),
        code_text(90, 1318, SIZE_EXAMPLE_BODY, 16, 650),
        code_text(90, 1380, "!4~", 22, 800, GOLD),
        text(150, 1379, "= repeat full-height ~ four times", 16, 600, MUTED),
        code_text(90, 1422, "$", 22, 800, INK),
        text(125, 1421, "= return to the left edge", 16, 600, MUTED),
        rect(70, 1550, 620, 92, TEAL_LIGHT, TEAL, 1.5, 14),
        text(380, 1588, "same decoded pixels", 20, 800, TEAL, "middle"),
        text(380, 1618, "size saves two bytes in this example", 17, 600, MUTED, "middle"),
    ])
    return svg_document(
        width,
        height,
        "Four indexed colors become literal SIXEL characters",
        "Mobile diagram showing a four-color indexed band, each color mask and its real data characters, followed by literal auto or fast and size-policy paint bodies.",
        "\n".join(body),
    )


def overpaint_rows(fill: bool) -> tuple[str, ...]:
    """Return the first-pass mask for the overpaint example."""
    if fill:
        return tuple("B" * 10 for _ in range(6))
    return (
        "BBBGGGGBBB",
        "BBGGGGGBBB",
        "BBBBGGBBBB",
        "BBBBGGBBBB",
        "BBGGGGGBBB",
        "BBBGGGGBBB",
    )


FINAL_ROWS = overpaint_rows(False)


def overpaint_panel(x: float, title_value: str, size_policy: bool) -> str:
    """Return one wide fast-or-size sequence panel."""
    accent = GOLD if size_policy else BLUE
    body = [card(x, 170, 610, 420, title_value, accent,
                 GOLD_LIGHT if size_policy else PAPER)]
    initial = overpaint_rows(size_policy)
    body.extend([
        text(x + 38, 250, "first blue pass", 18, 700, BLUE),
        draw_grid(x + 38, 275, initial, 25),
        text(x + 330, 250, "repaint amber", 18, 700, GOLD),
        draw_grid(x + 330, 275, mask_rows(FINAL_ROWS, "G"), 25, 3, True),
        line(x + 276, 350, x + 315, 350, accent, 4, "arrow-blue"),
        text(x + 38, 470, "result", 18, 700, TEAL),
        draw_grid(x + 38, 455, FINAL_ROWS, 20),
    ])
    if size_policy:
        body.extend([
            text(x + 330, 505, "More solid columns", 17, 700, GOLD),
            text(x + 330, 533, "favor repeat runs.", 17, 500, MUTED),
        ])
    else:
        body.extend([
            text(x + 330, 505, "Never paint a pixel", 17, 700, BLUE),
            text(x + 330, 533, "that belongs to another color.", 17, 500, MUTED),
        ])
    return "".join(body)


def overpaint_wide() -> str:
    """Compare exact-mask and overpainting serialization."""
    width = 1380
    height = 720
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(55, 62, "Size policy spends overdraw to create simpler runs", 38, 800),
        text(55, 100, "Later colors repair the temporary fill, so the final indexed image is unchanged.", 20, 500, MUTED),
        overpaint_panel(55, "fast / auto: preserve every mask", False),
        overpaint_panel(715, "size: fill first, then repaint", True),
        rect(170, 630, 1040, 54, TEAL_LIGHT, TEAL, 1.5, 13),
        text(690, 665, "same final pixels  •  different SIXEL byte stream", 21, 750, TEAL, "middle"),
    ])
    return svg_document(
        width,
        height,
        "Size policy spends overdraw to create simpler runs",
        "Two panels reach the same final blue-and-amber pixel grid. Fast policy paints exact masks; size policy first fills the blue span and then repaints amber pixels, favoring repeatable SIXEL runs.",
        "\n".join(body),
    )


def overpaint_mobile() -> str:
    """Return the mobile layout of the overpainting explanation."""
    width = 760
    height = 1360
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(38, 56, "Overdraw can save bytes", 38, 800),
        text(38, 90, "Both routes finish on the same pixels.", 18, 500, MUTED),
        card(55, 135, 650, 455, "fast / auto: exact masks", BLUE),
        text(90, 210, "blue pass", 17, 700, BLUE),
        draw_grid(90, 235, overpaint_rows(False), 25),
        text(390, 210, "amber pass", 17, 700, GOLD),
        draw_grid(390, 235, mask_rows(FINAL_ROWS, "G"), 25, 3, True),
        text(90, 445, "No temporary overpaint; masks retain their holes.", 17, 500, MUTED),
        line(380, 600, 380, 645, GOLD, 5, "arrow-gold"),
        card(55, 655, 650, 485, "size: fill, then repaint", GOLD, GOLD_LIGHT),
        text(90, 730, "solid blue fill", 17, 700, BLUE),
        draw_grid(90, 755, overpaint_rows(True), 25),
        text(390, 730, "amber repair", 17, 700, GOLD),
        draw_grid(390, 755, mask_rows(FINAL_ROWS, "G"), 25, 3, True),
        text(90, 965, "Solid columns can compress into shorter repeat runs.", 17, 500, MUTED),
        rect(90, 1195, 580, 82, TEAL_LIGHT, TEAL, 1.5, 14),
        text(380, 1232, "same final pixels", 21, 800, TEAL, "middle"),
        text(380, 1260, "different SIXEL byte stream", 17, 600, MUTED, "middle"),
    ])
    return svg_document(
        width,
        height,
        "Overdraw can save SIXEL bytes",
        "Mobile comparison of exact color masks and size-policy overpainting, both ending in the same blue-and-amber pixels.",
        "\n".join(body),
    )


def paint_pair(x: float, y: float, first: str, second: str) -> str:
    """Return two painted cells."""
    return (
        rect(x, y, 90, 90, first, PAPER, 3, 10)
        + rect(x + 110, y, 90, 90, second, PAPER, 3, 10)
    )


def register_wide() -> str:
    """Compare live-register and paint-time palette semantics."""
    width = 1440
    height = 760
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(55, 62, "High color depends on when a terminal resolves a register", 38, 800),
        text(55, 100, "The stream paints with #0, redefines #0, and paints again.", 20, 500, MUTED),
        badge(95, 155, "#0 = red", MAGENTA, 130),
        line(235, 176, 305, 176, MAGENTA, 5, "arrow-magenta"),
        badge(315, 155, "paint left", INK, 130),
        line(455, 176, 525, 176, BLUE, 5, "arrow-blue"),
        badge(535, 155, "#0 = blue", BLUE, 140),
        line(685, 176, 755, 176, BLUE, 5, "arrow-blue"),
        badge(765, 155, "paint right", INK, 145),
        line(920, 176, 990, 176, GOLD, 5, "arrow-gold"),
        text(1020, 183, "same wire events", 18, 700, MUTED),
        card(55, 260, 635, 365, "Live-register display", MAGENTA, MAGENTA_LIGHT),
        text_lines(95, 330, ("Earlier pixels retain register #0,", "not the red value it had then."), 18, 28, 500, MUTED),
        paint_pair(125, 415, BLUE, BLUE),
        text(225, 545, "left changes to blue", 19, 750, MAGENTA, "middle"),
        text(555, 455, "VT340-like", 18, 700, MAGENTA, "middle"),
        text(555, 486, "high color fails", 18, 700, MAGENTA, "middle"),
        card(750, 260, 635, 365, "Paint-time display", TEAL, TEAL_LIGHT),
        text_lines(790, 330, ("Each paint operation resolves #0", "to its current RGB value immediately."), 18, 28, 500, MUTED),
        paint_pair(820, 415, MAGENTA, BLUE),
        text(920, 545, "red stays red", 19, 750, TEAL, "middle"),
        text(1250, 455, "modern behavior", 18, 700, TEAL, "middle"),
        text(1250, 486, "high color works", 18, 700, TEAL, "middle"),
        rect(180, 665, 1080, 55, PAPER, GRID, 1.5, 13),
        text(720, 700, "-I is a terminal-compatibility extension, not a guarantee of the DEC hardware model.", 20, 750, INK, "middle"),
    ])
    return svg_document(
        width,
        height,
        "High color depends on register resolution timing",
        "The same register zero is red for the first paint and blue for the second. A live-register display turns both pixels blue; a paint-time display preserves the first red pixel, enabling high color.",
        "\n".join(body),
    )


def register_mobile() -> str:
    """Return the mobile layout of register semantics."""
    width = 760
    height = 1450
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(38, 58, "When is #0 resolved?", 38, 800),
        text(38, 92, "The same stream can produce two results.", 18, 500, MUTED),
        badge(85, 145, "#0 red", MAGENTA, 110),
        line(205, 166, 255, 166, MAGENTA, 4, "arrow-magenta"),
        badge(265, 145, "paint", INK, 92),
        line(367, 166, 417, 166, BLUE, 4, "arrow-blue"),
        badge(427, 145, "#0 blue", BLUE, 120),
        line(557, 166, 607, 166, BLUE, 4, "arrow-blue"),
        badge(617, 145, "paint", INK, 92),
        card(55, 245, 650, 440, "Live register: high color fails", MAGENTA, MAGENTA_LIGHT),
        text_lines(95, 325, ("Earlier pixels still point at #0.", "Redefining #0 changes them too."), 18, 29, 500, MUTED),
        paint_pair(230, 430, BLUE, BLUE),
        text(330, 555, "both blue", 20, 800, MAGENTA, "middle"),
        line(380, 700, 380, 745, TEAL, 5, "arrow-blue"),
        card(55, 755, 650, 440, "Paint time: high color works", TEAL, TEAL_LIGHT),
        text_lines(95, 835, ("Each paint captures the current RGB.", "Later definitions do not rewrite it."), 18, 29, 500, MUTED),
        paint_pair(230, 940, MAGENTA, BLUE),
        text(330, 1065, "red, then blue", 20, 800, TEAL, "middle"),
        rect(85, 1260, 590, 100, PAPER, GRID, 1.5, 14),
        text(380, 1302, "Check terminal support before using -I.", 20, 800, INK, "middle"),
        text(380, 1334, "The historical DEC model is the upper case.", 17, 500, MUTED, "middle"),
    ])
    return svg_document(
        width,
        height,
        "When is a palette register resolved?",
        "Mobile diagram comparing live-register and paint-time terminal rendering after register zero changes from red to blue.",
        "\n".join(body),
    )


def color_cloud(x: float, y: float, columns: int, rows: int,
                cell: float) -> str:
    """Return a deterministic many-color patch cloud."""
    body = []
    for row_index in range(rows):
        for column_index in range(columns):
            red = (column_index * 37 + row_index * 19) % 256
            green = (column_index * 11 + row_index * 47) % 256
            blue = (column_index * 53 + row_index * 7) % 256
            color = f"#{red:02X}{green:02X}{blue:02X}"
            body.append(rect(x + column_index * cell,
                             y + row_index * cell,
                             cell - 2, cell - 2, color, radius=2))
    return "".join(body)


def pass_stage(x: float, y: float, width: float, title_value: str,
               lines: Sequence[str], accent: str) -> str:
    """Return one high-color pipeline stage."""
    return (
        card(x, y, width, 155, title_value, accent,
             GOLD_LIGHT if accent == GOLD else PAPER)
        + text_lines(x + 28, y + 82, lines, 17, 26, 500, MUTED)
    )


def passes_wide() -> str:
    """Explain the repeated high-color pass algorithm."""
    width = 1520
    height = 790
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(55, 62, "-I turns a 15-bit color field into repeated 255-slot passes", 38, 800),
        text(55, 100, "Five bits per channel create 32 × 32 × 32 possible RGB values; register #255 remains the skip key.", 20, 500, MUTED),
        card(55, 150, 260, 390, "1  RGB pixels", BLUE),
        color_cloud(92, 225, 8, 9, 23),
        text(92, 465, "8-bit source channels", 17, 700, BLUE),
        text(92, 495, "many distinct colors", 17, 500, MUTED),
        line(320, 345, 380, 345, BLUE, 5, "arrow-blue"),
        card(390, 150, 275, 390, "2  Quantize to 15bpp", TEAL),
        badge(430, 235, "R: 5 bits", MAGENTA, 120),
        badge(430, 300, "G: 5 bits", TEAL, 120),
        badge(430, 365, "B: 5 bits", BLUE, 120),
        text(430, 455, "32³ = 32,768", 24, 800, TEAL),
        text(430, 490, "possible colors", 18, 500, MUTED),
        line(670, 345, 730, 345, TEAL, 5, "arrow-blue"),
        card(740, 150, 320, 390, "3  Fill one pass", GOLD, GOLD_LIGHT),
        badge(780, 230, "#0 … #254", GOLD, 160),
        text(780, 305, "assign up to 255 colors", 18, 700, INK),
        text(780, 342, "paint matching cells", 18, 700, INK),
        text(780, 379, "mark them complete", 18, 700, INK),
        badge(780, 430, "#255 = skip", INK, 160),
        line(1065, 345, 1125, 345, GOLD, 5, "arrow-gold"),
        card(1135, 150, 330, 390, "4  Reuse and repeat", MAGENTA, MAGENTA_LIGHT),
        text(1175, 235, "redefine used registers", 18, 700, MAGENTA),
        text(1175, 275, "paint remaining cells", 18, 700, MAGENTA),
        text(1175, 315, "repeat until no dirty", 18, 700, MAGENTA),
        path("M 1300 380 V 455 H 1095 V 570 H 900 V 548", MAGENTA, 4, "arrow-magenta", "8 6"),
        text(1150, 430, "another pass", 17, 700, MAGENTA),
        rect(120, 605, 1280, 110, PAPER, GRID, 1.5, 15),
        text(760, 647, "More colors mean more palette definitions and more passes.", 22, 800, INK, "middle"),
        text(760, 682, "That raises stream size, while bypassing fixed-palette construction and lookup can reduce encoder CPU time.", 18, 500, MUTED, "middle"),
        text(55, 758, "Conceptual loop • the exact pass boundary follows register availability and already-painted marks", 16, 500, MUTED),
    ])
    return svg_document(
        width,
        height,
        "High color uses repeated 255-slot passes",
        "RGB pixels are reduced to five bits per channel, assigned to palette registers zero through 254, painted and marked, then remaining colors are emitted in later passes while register 255 remains a skip key.",
        "\n".join(body),
    )


def passes_mobile() -> str:
    """Return the mobile layout of the high-color pass algorithm."""
    width = 760
    height = 1570
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(38, 58, "15-bit color, 255 slots at a time", 35, 800),
        text(38, 92, "#255 is kept as the skip key.", 18, 500, MUTED),
        pass_stage(55, 135, 650, "1  Normalize RGB", ("Source pixels enter as RGB888.", "High color bypasses palette construction."), BLUE),
        color_cloud(500, 190, 6, 4, 22),
        line(380, 300, 380, 345, BLUE, 5, "arrow-blue"),
        pass_stage(55, 355, 650, "2  Keep five bits per channel", ("R5 + G5 + B5 = 15 bits", "32³ = 32,768 possible colors"), TEAL),
        line(380, 520, 380, 565, TEAL, 5, "arrow-blue"),
        pass_stage(55, 575, 650, "3  Build one paint pass", ("Assign up to 255 new colors to #0…#254.", "Other pixels use #255 and wait."), GOLD),
        line(380, 740, 380, 785, GOLD, 5, "arrow-gold"),
        pass_stage(55, 795, 650, "4  Paint and mark", ("Emit changed definitions and matching cells.", "Marked cells will not be painted again."), MAGENTA),
        path("M 380 960 V 1010 H 695 V 575", MAGENTA, 4, "arrow-magenta", "8 6"),
        text(495, 995, "reuse slots for remaining colors", 16, 700, MAGENTA),
        rect(75, 1095, 610, 225, PAPER, GRID, 1.5, 15),
        text(380, 1140, "Tradeoff", 23, 800, INK, "middle"),
        text(115, 1185, "+ bypasses fixed-palette construction and lookup", 17, 700, TEAL),
        text(115, 1225, "+ offers a 32,768-color lattice", 17, 700, TEAL),
        text(115, 1265, "− repeats palette definitions and paint passes", 17, 700, MAGENTA),
        rect(75, 1370, 610, 100, MAGENTA_LIGHT, MAGENTA, 1.5, 15),
        text(380, 1412, "Requires paint-time register semantics", 20, 800, MAGENTA, "middle"),
        text(380, 1444, "in the receiving terminal.", 18, 600, MUTED, "middle"),
    ])
    return svg_document(
        width,
        height,
        "High color uses 255 palette slots at a time",
        "Mobile pipeline from RGB pixels through five-bit channel quantization, one 255-color paint pass, marking, register reuse, and repetition.",
        "\n".join(body),
    )


def encode_manifest() -> dict[str, object]:
    """Return metadata for encode-policy figures."""
    return {
        "schema": 1,
        "generator": "tools/plot_encoding_mode_figures.py",
        "measurement": False,
        "scope": "img2sixel -E literal band serialization and overpainting",
        "four_color_example": {
            "dimensions": "8x6",
            "palette": {
                "#0": "blue",
                "#1": "amber",
                "#2": "green",
                "#3": "pink",
            },
            "auto_fast_paint_body": FAST_EXAMPLE_BODY,
            "size_paint_body": SIZE_EXAMPLE_BODY,
            "wrapper": "DCS introducer, raster attributes, palette definitions, and ST omitted",
        },
        "assets": [
            {"path": name, "role": "mobile portrait" if "mobile" in name else "large screen"}
            for name in ENCODE_ASSETS[:-1]
        ],
        "color_roles": {
            "ordinary_path": BLUE,
            "size_policy": GOLD,
            "same_output": TEAL,
            "warning": MAGENTA,
            "context": MUTED,
        },
        "review": {
            "story_job": "show literal four-color serialization and overpainting",
            "data_shape": "conceptual process and side-by-side comparison",
            "primary": "responsive SVG diagrams",
            "encoding": "arrows show order; labels and layout repeat color roles",
            "fallback": "adjacent prose and figure alt text",
            "accessibility": "no distinction relies on color alone",
            "qa": "wide and mobile assets reviewed after final generation",
        },
    }


def high_color_manifest() -> dict[str, object]:
    """Return metadata for high-color figures."""
    return {
        "schema": 1,
        "generator": "tools/plot_encoding_mode_figures.py",
        "measurement": False,
        "scope": "img2sixel -I register semantics and repeated paint passes",
        "assets": [
            {"path": name, "role": "mobile portrait" if "mobile" in name else "large screen"}
            for name in HIGH_COLOR_ASSETS[:-1]
        ],
        "color_roles": {
            "fixed_or_source": BLUE,
            "paint_pass": GOLD,
            "compatible_result": TEAL,
            "register_redefinition": MAGENTA,
            "context": MUTED,
        },
        "review": {
            "story_job": "explain register semantics and repeated paint passes",
            "data_shape": "branching comparison and cyclic process",
            "primary": "responsive SVG diagrams",
            "encoding": "arrows show order; labels and layout repeat color roles",
            "fallback": "adjacent prose and figure alt text",
            "accessibility": "no distinction relies on color alone",
            "qa": "wide and mobile assets reviewed after final generation",
        },
    }


def write_assets(directory: Path,
                 assets: Iterable[tuple[str, Callable[[], str]]],
                 manifest_name: str,
                 manifest_value: dict[str, object]) -> None:
    """Write one figure family and its manifest."""
    directory.mkdir(parents=True, exist_ok=True)
    for name, factory in assets:
        (directory / name).write_text(factory(), encoding="utf-8")
    (directory / manifest_name).write_text(
        json.dumps(manifest_value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def generate(encode_directory: Path, high_color_directory: Path) -> None:
    """Generate both figure families."""
    write_assets(
        encode_directory,
        (
            ("encode-policy-band-wide.svg", band_wide),
            ("encode-policy-band-mobile.svg", band_mobile),
            ("encode-policy-overpaint-wide.svg", overpaint_wide),
            ("encode-policy-overpaint-mobile.svg", overpaint_mobile),
        ),
        "encode-policy-figures.json",
        encode_manifest(),
    )
    write_assets(
        high_color_directory,
        (
            ("high-color-register-wide.svg", register_wide),
            ("high-color-register-mobile.svg", register_mobile),
            ("high-color-passes-wide.svg", passes_wide),
            ("high-color-passes-mobile.svg", passes_mobile),
        ),
        "high-color-figures.json",
        high_color_manifest(),
    )


def compare_family(expected: Path, actual: Path,
                   names: Sequence[str]) -> list[str]:
    """Return stale or missing assets."""
    return [
        name
        for name in names
        if not (expected / name).is_file()
        or (expected / name).read_bytes() != (actual / name).read_bytes()
    ]


def check(encode_directory: Path, high_color_directory: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(prefix="encoding-mode-figures-") as temp:
        root = Path(temp)
        generated_encode = root / "encode"
        generated_high = root / "high-color"
        generate(generated_encode, generated_high)
        mismatches = compare_family(
            encode_directory, generated_encode, ENCODE_ASSETS
        )
        mismatches.extend(
            compare_family(high_color_directory, generated_high,
                           HIGH_COLOR_ASSETS)
        )
        if mismatches:
            raise SystemExit(
                "encoding-mode figures are stale or missing: "
                + ", ".join(mismatches)
            )


def parse_args() -> argparse.Namespace:
    """Parse output directories and check mode."""
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--encode-output-dir",
        type=Path,
        default=root / "docs/functionality/encode-policy-figures",
    )
    parser.add_argument(
        "--high-color-output-dir",
        type=Path,
        default=root / "docs/functionality/high-color-figures",
    )
    parser.add_argument("--check", action="store_true")
    return parser.parse_args()


def main() -> None:
    """Generate or verify all encoding-mode figures."""
    args = parse_args()
    if args.check:
        check(args.encode_output_dir, args.high_color_output_dir)
    else:
        generate(args.encode_output_dir, args.high_color_output_dir)


if __name__ == "__main__":
    main()
