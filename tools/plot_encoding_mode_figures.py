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


SAMPLE_ROWS = (
    "BBBGGGGB",
    "BBGGGGBB",
    "BGBBBGBB",
    "BGGBBGBB",
    "BBBGGGBB",
    "BBBBBBBB",
)


def band_wide() -> str:
    """Explain color masks inside one six-pixel-high band."""
    width = 1440
    height = 720
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(55, 64, "A SIXEL band becomes one mask per color", 38, 800),
        text(55, 102, "Each output character carries six vertical paint bits; $ returns to the band origin.", 20, 500, MUTED),
        card(55, 155, 330, 405, "1  Indexed pixels", BLUE),
        draw_grid(95, 225, SAMPLE_ROWS, 32),
        text(95, 450, "One band = six pixel rows", 19, 700),
        text(95, 480, "Blue and amber share columns", 17, 500, MUTED),
        line(390, 355, 455, 355, BLUE, 5, "arrow-blue"),
        card(465, 155, 425, 405, "2  Split into color masks", TEAL),
        draw_grid(505, 225, mask_rows(SAMPLE_ROWS, "B"), 22, 3, True),
        draw_grid(690, 225, mask_rows(SAMPLE_ROWS, "G"), 22, 3, True),
        text(585, 430, "#0 blue", 18, 700, BLUE, "middle"),
        text(770, 430, "#1 amber", 18, 700, GOLD, "middle"),
        text(505, 482, "Black cells set one or more of", 17, 500, MUTED),
        text(505, 508, "the six bits in a SIXEL character.", 17, 500, MUTED),
        line(895, 355, 960, 355, GOLD, 5, "arrow-gold"),
        card(970, 155, 415, 405, "3  Serialize the band", GOLD, GOLD_LIGHT),
        badge(1015, 230, "#0", BLUE, 74),
        badge(1105, 230, "mask run", TEAL, 122),
        badge(1243, 230, "$", INK, 54),
        badge(1015, 300, "#1", GOLD, 74),
        badge(1105, 300, "mask run", TEAL, 122),
        badge(1243, 300, "-", INK, 54),
        text(1015, 390, "#n", 18, 800, INK),
        text(1060, 390, "selects a palette register", 18, 500, MUTED),
        text(1015, 430, "$", 18, 800, INK),
        text(1060, 430, "returns to the left edge", 18, 500, MUTED),
        text(1015, 470, "-", 18, 800, INK),
        text(1060, 470, "moves to the next band", 18, 500, MUTED),
        rect(145, 605, 1150, 64, PAPER, GRID, 1.5, 14),
        text(720, 645, "The palette is already fixed here: -E only changes how these masks are serialized.", 20, 700, INK, "middle"),
    ])
    return svg_document(
        width,
        height,
        "A SIXEL band becomes one mask per color",
        "An eight by six indexed pixel grid is split into separate blue and amber masks, then serialized as palette selections, mask runs, a carriage return, and a next-band command.",
        "\n".join(body),
    )


def band_mobile() -> str:
    """Return the mobile layout of the band explanation."""
    width = 760
    height = 1470
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(38, 58, "One band, color by color", 38, 800),
        text(38, 92, "Each SIXEL character carries six vertical bits.", 18, 500, MUTED),
        card(55, 135, 650, 300, "1  Indexed pixels", BLUE),
        draw_grid(245, 200, SAMPLE_ROWS, 32),
        text(380, 418, "six rows × eight columns", 17, 600, MUTED, "middle"),
        line(380, 445, 380, 490, BLUE, 5, "arrow-blue"),
        card(55, 500, 650, 370, "2  One mask per color", TEAL),
        draw_grid(135, 585, mask_rows(SAMPLE_ROWS, "B"), 26, 3, True),
        draw_grid(425, 585, mask_rows(SAMPLE_ROWS, "G"), 26, 3, True),
        text(235, 770, "#0 blue", 18, 700, BLUE, "middle"),
        text(525, 770, "#1 amber", 18, 700, GOLD, "middle"),
        text(380, 820, "Black cells become the six paint bits.", 17, 500, MUTED, "middle"),
        line(380, 880, 380, 925, GOLD, 5, "arrow-gold"),
        card(55, 935, 650, 330, "3  Serialize", GOLD, GOLD_LIGHT),
        badge(115, 1025, "#0", BLUE, 70),
        badge(200, 1025, "mask", TEAL, 100),
        badge(315, 1025, "$", INK, 54),
        badge(384, 1025, "#1", GOLD, 70),
        badge(469, 1025, "mask", TEAL, 100),
        badge(584, 1025, "-", INK, 54),
        text(115, 1125, "$ returns left; - advances six rows.", 18, 500, MUTED),
        text(115, 1170, "-E changes this serialization stage,", 18, 700),
        text(115, 1198, "not the palette or indexed pixels.", 18, 700),
        rect(70, 1325, 620, 78, PAPER, GRID, 1.5, 14),
        text(380, 1372, "The decoded image should remain unchanged.", 20, 700, INK, "middle"),
    ])
    return svg_document(
        width,
        height,
        "One SIXEL band, color by color",
        "Mobile diagram showing indexed pixels, separate blue and amber masks, and the corresponding palette-selection and movement tokens.",
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
        "scope": "img2sixel -E band serialization and size-policy overpainting",
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
            "story_job": "explain the serialization boundary and overpainting",
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
