#!/usr/bin/env python3
"""Generate deterministic decoding-pipeline diagrams."""

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
    line,
    path,
    rect,
    svg_document,
    tag,
    text,
    text_lines,
)


ASSET_NAMES = (
    "decoding-pipeline-wide.svg",
    "decoding-pipeline-mobile.svg",
    "decoder-parser.svg",
    "decoder-raster-policy.svg",
    "decoder-representations.svg",
    "decoder-output-policy.svg",
    "decoder-parallel.svg",
    "decoding-pipeline-figures.json",
)


def stage_card(x: float, y: float, width: float, height: float,
               title_value: str, details: Sequence[str], accent: str,
               fill: str = PAPER, title_size: int = 25) -> str:
    """Return a labeled stage card."""
    body = [rect(x, y, width, height, fill, accent, 2.0, 16.0,
                 "card-shadow")]
    body.append(rect(x, y, 8, height, accent, radius=4.0))
    body.append(text(x + 25, y + 43, title_value,
                     title_size, 760, accent))
    body.append(text_lines(x + 25, y + 78, details,
                           17, 25, 470, MUTED))
    return "".join(body)


def pill(x: float, y: float, width: float, value: str,
         accent: str, fill: str) -> str:
    """Return a compact labeled pill."""
    return "".join((
        rect(x, y, width, 38, fill, accent, 1.5, 19.0),
        text(x + width / 2, y + 26, value, 16, 700, accent, "middle"),
    ))


def byte_stream(x: float, y: float, compact: bool = False) -> str:
    """Return an abbreviated SIXEL byte stream."""
    labels = ("DCS q", '" raster', "# color", "! repeat", "?–~ paint", "ST")
    widths = (77, 92, 88, 88, 106, 50)
    colors = (BLUE, MAGENTA, MAGENTA, GOLD, TEAL, BLUE)
    if compact:
        labels = ("DCS q", "controls", "paint", "ST")
        widths = (100, 135, 115, 70)
        colors = (BLUE, MAGENTA, TEAL, BLUE)
    body = []
    cursor = x
    for label_value, width, color in zip(labels, widths, colors):
        body.append(rect(cursor, y, width, 52, PAPER, color,
                         2.0, 8.0))
        body.append(text(cursor + width / 2, y + 33,
                         label_value, 15, 720, color, "middle"))
        cursor += width + 8
    return "".join(body)


def overview_wide() -> str:
    """Return the landscape decoding mental model."""
    width = 1640
    height = 760
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(46, 64, "SIXEL is a paint program, not a pixel dump",
                     40, 820, INK))
    body.append(text(46, 98,
                     "Decode state turns protocol commands into a raster contract",
                     20, 550, MUTED))
    body.append(text(1594, 64, "DECODING MENTAL MODEL",
                     15, 780, BLUE, "end"))

    body.append(byte_stream(50, 158))
    body.append(text(50, 232, "ordered input bytes", 17, 680, MUTED))
    body.append(line(630, 184, 704, 184, BLUE, 5.0, "arrow-blue"))

    body.append(stage_card(705, 126, 315, 175, "Stateful parser",
                           ("cursor + repeat count", "active color register",
                            "raster declaration"), BLUE, PAPER, 28))
    body.append(line(1020, 213, 1092, 213, TEAL, 5.0, "arrow-blue"))
    body.append(stage_card(1095, 126, 335, 175, "Canvas painter",
                           ("grow or clip bounds", "record paint mask",
                            "track palette history"), TEAL, TEAL_LIGHT, 28))

    body.append(path("M 1262 301 V 365 H 821 V 410",
                     TEAL, 5.0, "arrow-blue"))
    body.append(text(1008, 350, "decoded image state",
                     17, 700, TEAL, "middle"))

    body.append(stage_card(515, 410, 305, 180, "Representation",
                           ("indexed + palette", "or direct RGBA",
                            "plus result flags"), MAGENTA,
                           MAGENTA_LIGHT, 27))
    body.append(line(820, 500, 900, 500, MAGENTA, 5.0,
                     "arrow-magenta"))
    body.append(stage_card(905, 410, 300, 180, "Output policy",
                           ("packed byte order", "optional reconstruction",
                            "optional resize"), GOLD, GOLD_LIGHT, 27))

    body.append(path("M 1205 455 H 1315 V 410 H 1455",
                     GOLD, 4.0, "arrow-gold"))
    body.append(path("M 1205 545 H 1315 V 590 H 1455",
                     GOLD, 4.0, "arrow-gold"))
    body.append(pill(1455, 391, 145, "caller pixels", BLUE, BLUE_LIGHT))
    body.append(pill(1455, 571, 145, "PNG / file", GOLD, GOLD_LIGHT))

    body.append(rect(48, 646, 1544, 70, PAPER, GRID, 1.5, 14.0))
    body.append(text(820, 687,
                     "Parsing decides what was painted; output policy decides how callers receive it.",
                     21, 700, INK, "middle"))
    return svg_document(
        width,
        height,
        "SIXEL decoding mental model",
        "SIXEL bytes flow through a stateful parser and canvas painter, then branch through representation and output policy to caller pixels or PNG output.",
        "\n".join(body),
    )


def overview_mobile() -> str:
    """Return the portrait decoding mental model."""
    width = 760
    height = 1450
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(40, 58, "SIXEL decoding", 42, 820, INK))
    body.append(text(40, 91, "Stateful paint-program mental model",
                     19, 580, MUTED))
    body.append(byte_stream(55, 135, True))

    stages = (
        (260, "Stateful parser",
         ("cursor, repeat, color register", "raster declaration"),
         BLUE, PAPER),
        (480, "Canvas painter",
         ("grow or clip bounds", "mask and palette history"),
         TEAL, TEAL_LIGHT),
        (700, "Representation",
         ("indexed + palette or RGBA", "result flags"),
         MAGENTA, MAGENTA_LIGHT),
        (920, "Output policy",
         ("packed order, reconstruction", "optional resize"),
         GOLD, GOLD_LIGHT),
    )
    for y, title_value, details, accent, fill in stages:
        body.append(stage_card(55, y, 650, 165, title_value,
                               details, accent, fill, 29))
    for y, accent, marker in ((207, BLUE, "arrow-blue"),
                              (425, TEAL, "arrow-blue"),
                              (645, MAGENTA, "arrow-magenta"),
                              (865, GOLD, "arrow-gold")):
        body.append(line(380, y, 380, y + 45, accent, 5.0, marker))

    body.append(path("M 380 1085 V 1155 H 205 V 1205",
                     GOLD, 4.0, "arrow-gold"))
    body.append(path("M 380 1155 H 555 V 1205",
                     GOLD, 4.0, "arrow-gold"))
    body.append(pill(105, 1205, 200, "caller pixels", BLUE, BLUE_LIGHT))
    body.append(pill(455, 1205, 200, "PNG / file", GOLD, GOLD_LIGHT))
    body.append(rect(55, 1320, 650, 76, PAPER, GRID, 1.5, 14.0))
    body.append(text_lines(380, 1352,
                           ("Parsing decides what was painted;",
                            "output policy decides how it is returned."),
                           18, 25, 680, INK, "middle"))
    return svg_document(
        width,
        height,
        "SIXEL decoding mental model",
        "Portrait flow from SIXEL bytes through parser, painter, representation, and output policy to caller pixels or PNG output.",
        "\n".join(body),
    )


def parser_figure() -> str:
    """Return a parser-control diagram."""
    width = 1540
    height = 840
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(45, 62, "One byte stream, several kinds of state",
                     38, 820, INK))
    body.append(text(45, 96,
                     "Controls update parser state; only SIXEL data bits paint pixels",
                     20, 550, MUTED))
    body.append(byte_stream(80, 145))
    body.append(line(700, 171, 770, 171, BLUE, 5.0, "arrow-blue"))
    body.append(stage_card(775, 128, 300, 126, "Envelope",
                           ("7-bit or 8-bit DCS", "q parameters + ST"),
                           BLUE, PAPER, 25))
    body.append(line(1075, 191, 1150, 191, BLUE, 5.0, "arrow-blue"))
    body.append(stage_card(1155, 128, 300, 126, "DECSIXEL body",
                           ("consume controls", "advance parser state"),
                           BLUE, BLUE_LIGHT, 25))

    controls = (
        (70, 350, '" raster', ("declare width/height", "set aspect metadata"),
         MAGENTA, MAGENTA_LIGHT),
        (430, 350, "# color", ("select or redefine", "a color register"),
         MAGENTA, MAGENTA_LIGHT),
        (790, 350, "! repeat", ("set run length", "for next SIXEL"),
         GOLD, GOLD_LIGHT),
        (1150, 350, "? through ~", ("six vertical bits", "paint at cursor x"),
         TEAL, TEAL_LIGHT),
        (430, 585, "$ carriage", ("x = 0", "stay in six-row band"),
         BLUE, BLUE_LIGHT),
        (790, 585, "- next band", ("x = 0", "y += 6"),
         BLUE, BLUE_LIGHT),
    )
    for x, y, title_value, details, accent, fill in controls:
        body.append(stage_card(x, y, 310, 145, title_value,
                               details, accent, fill, 24))

    body.append(path("M 1305 495 V 770 H 250 V 495",
                     TEAL, 4.0, "arrow-blue"))
    body.append(text(770, 797,
                     "All branches rejoin the same cursor, palette, raster, and canvas state",
                     20, 720, INK, "middle"))
    body.append(line(225, 495, 225, 770, MAGENTA, 3.0, None, "7 6"))
    body.append(line(585, 495, 585, 770, MAGENTA, 3.0, None, "7 6"))
    body.append(line(945, 495, 945, 585, GOLD, 3.0, None, "7 6"))
    body.append(line(585, 730, 585, 770, BLUE, 3.0, None, "7 6"))
    body.append(line(945, 730, 945, 770, BLUE, 3.0, None, "7 6"))
    return svg_document(
        width,
        height,
        "SIXEL parser controls and state",
        "The DCS envelope enters the DECSIXEL body. Raster, color, repeat, paint, carriage-return, and next-band controls update shared decoder state.",
        "\n".join(body),
    )


def raster_grid(x: float, y: float, columns: int, rows: int,
                painted_rows: int, clipped: bool = False) -> str:
    """Return a small canvas with declared and painted extents."""
    body = []
    cell = 36
    for row_index in range(rows):
        for column_index in range(columns):
            painted = row_index < painted_rows
            fill = TEAL_LIGHT if painted else PAPER
            if painted and (row_index + column_index) % 3 == 0:
                fill = "#99F6E4"
            body.append(rect(x + column_index * cell,
                             y + row_index * cell,
                             cell - 2, cell - 2,
                             fill, GRID, 1.0, 3.0))
    body.append(tag("rect", x=f"{x - 4:.1f}", y=f"{y - 4:.1f}",
                    width=f"{columns * cell + 6:.1f}",
                    height=f"{5 * cell + 6:.1f}", fill="none",
                    stroke=MAGENTA, stroke_width="4",
                    stroke_dasharray="10 7", rx="6"))
    if clipped:
        body.append(line(x - 12, y + 5 * cell + 12,
                         x + columns * cell + 12, y + 5 * cell + 12,
                         GOLD, 5.0))
    return "".join(body)


def raster_policy_figure() -> str:
    """Return a declared-raster versus painted-extent diagram."""
    width = 1540
    height = 790
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(45, 62, "Declared bounds and painted extent can disagree",
                     38, 820, INK))
    body.append(text(45, 96,
                     "A five-row declaration can still receive a six-bit SIXEL column",
                     20, 550, MUTED))

    cards = ((55, "Input stream", "declares 4 × 5", PAPER, BLUE),
             (550, "Default policy", "preserve 4 × 6", TEAL_LIGHT, TEAL),
             (1045, "Trust-raster policy", "clip to 4 × 5", GOLD_LIGHT, GOLD))
    for x, title_value, detail, fill, accent in cards:
        body.append(rect(x, 150, 440, 520, fill, accent,
                         2.0, 18.0, "card-shadow"))
        body.append(text(x + 28, 197, title_value, 27, 780, accent))
        body.append(text(x + 28, 230, detail, 18, 600, MUTED))

    body.append(raster_grid(195, 285, 4, 6, 6))
    body.append(raster_grid(690, 285, 4, 6, 6))
    body.append(raster_grid(1185, 285, 4, 5, 5, True))
    body.append(text(275, 545, "row 6 is painted", 18, 700, TEAL, "middle"))
    body.append(text_lines(770, 545,
                           ("PAINT_OUTSIDE_RASTER", "output grows to evidence"),
                           17, 27, 700, TEAL, "middle"))
    body.append(text_lines(1265, 545,
                           ("PAINT_OUTSIDE_RASTER", "+ CLIPPED_TO_RASTER"),
                           17, 27, 700, GOLD, "middle"))
    body.append(line(495, 410, 545, 410, BLUE, 5.0, "arrow-blue"))
    body.append(line(990, 410, 1040, 410, GOLD, 5.0, "arrow-gold"))
    body.append(rect(55, 710, 1430, 48, PAPER, GRID, 1.5, 12.0))
    body.append(text(770, 741,
                     "Magenta dashed outline = declared raster; teal cells = observed paint",
                     18, 680, INK, "middle"))
    return svg_document(
        width,
        height,
        "Declared raster and observed paint policy",
        "A four-by-five declared raster receives six painted rows. The default preserves the sixth row and reports it; trust-raster mode clips it and reports both paint-outside and clipped flags.",
        "\n".join(body),
    )


def palette_swatch(x: float, y: float) -> str:
    """Return an indexed plane and palette swatches."""
    values = ((0, 1, 1, 0), (0, 1, 2, 2), (3, 3, 2, 2))
    colors = (BLUE_LIGHT, MAGENTA_LIGHT, GOLD_LIGHT, TEAL_LIGHT)
    body = []
    for row_index, row_values in enumerate(values):
        for column_index, value in enumerate(row_values):
            body.append(rect(x + column_index * 35,
                             y + row_index * 35,
                             31, 31, colors[value], GRID, 1.0, 3.0))
            body.append(text(x + column_index * 35 + 15,
                             y + row_index * 35 + 22,
                             str(value), 14, 760, INK, "middle"))
    for index, color in enumerate((BLUE, MAGENTA, GOLD, TEAL)):
        body.append(rect(x + 175, y + index * 29,
                         24, 24, color, radius=4.0))
    return "".join(body)


def rgba_swatch(x: float, y: float) -> str:
    """Return direct RGBA cells."""
    colors = (BLUE, MAGENTA, GOLD, TEAL, "#E2E8F0", "#FFFFFF")
    body = []
    for index, color in enumerate(colors):
        column_index = index % 3
        row_index = index // 3
        body.append(rect(x + column_index * 52,
                         y + row_index * 52,
                         45, 45, color, GRID, 1.0, 6.0))
    body.append(text(x + 78, y + 130, "RGBA", 18, 760, MUTED, "middle"))
    return "".join(body)


def byte_order_swatch(x: float, y: float) -> str:
    """Return packed byte-order examples."""
    rows = (("RGB", ("R", "G", "B")),
            ("BGRA", ("B", "G", "R", "A")),
            ("XRGB", ("X", "R", "G", "B")))
    colors = {"R": MAGENTA_LIGHT, "G": TEAL_LIGHT,
              "B": BLUE_LIGHT, "A": GOLD_LIGHT, "X": "#E2E8F0"}
    body = []
    for row_index, (label_value, channels) in enumerate(rows):
        body.append(text(x, y + row_index * 56 + 27,
                         label_value, 15, 700, MUTED))
        for channel_index, channel in enumerate(channels):
            body.append(rect(x + 65 + channel_index * 43,
                             y + row_index * 56,
                             38, 38, colors[channel], GRID,
                             1.0, 4.0))
            body.append(text(x + 84 + channel_index * 43,
                             y + row_index * 56 + 26,
                             channel, 15, 760, INK, "middle"))
    return "".join(body)


def representations_figure() -> str:
    """Return a comparison of decoder output representations."""
    width = 1540
    height = 820
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(45, 62, "Three useful views of the same painted image",
                     38, 820, INK))
    body.append(text(45, 96,
                     "Choose the representation for the consumer, not for the input filename",
                     20, 550, MUTED))

    body.append(stage_card(55, 150, 435, 555, "Indexed + palette",
                           ("one byte per pixel", "stable final palette required"),
                           MAGENTA, MAGENTA_LIGHT, 27))
    body.append(palette_swatch(160, 290))
    body.append(text_lines(272, 475,
                           ("sixel_decode_raw()", "unpainted index ≥ ncolors"),
                           18, 29, 680, INK, "middle"))
    body.append(pill(145, 585, 255, "compact and lossless to registers",
                     MAGENTA, PAPER))

    body.append(stage_card(552, 150, 435, 555, "Direct RGBA",
                           ("four bytes per pixel", "paint-time color is preserved"),
                           TEAL, TEAL_LIGHT, 27))
    body.append(rgba_swatch(690, 290))
    body.append(text_lines(770, 475,
                           ("sixel_decode_direct()", "unpainted alpha = 0"),
                           18, 29, 680, INK, "middle"))
    body.append(pill(650, 585, 240, "safe for palette redefinition",
                     TEAL, PAPER))

    body.append(stage_card(1049, 150, 435, 555, "Packed caller pixels",
                           ("requested channel order", "RGB/X composites background"),
                           BLUE, BLUE_LIGHT, 27))
    body.append(byte_order_swatch(1140, 280))
    body.append(text_lines(1267, 475,
                           ("sixel_decode_pixels()", "flags describe the result"),
                           18, 29, 680, INK, "middle"))
    body.append(pill(1140, 585, 255, "API-friendly ownership boundary",
                     BLUE, PAPER))

    body.append(rect(55, 740, 1429, 45, PAPER, GRID, 1.5, 12.0))
    body.append(text(770, 769,
                     "A palette-redefined stream cannot be reduced to one index plane plus one final palette.",
                     18, 700, INK, "middle"))
    return svg_document(
        width,
        height,
        "Decoder output representations",
        "Indexed pixels and a palette, direct RGBA, and packed caller-requested formats show distinct transparency and palette-redefinition contracts.",
        "\n".join(body),
    )


def output_policy_figure() -> str:
    """Return the stateful decoder output-policy branches."""
    width = 1540
    height = 900
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(45, 62, "The CLI adds policy after protocol decoding",
                     38, 820, INK))
    body.append(text(45, 96,
                     "Reconstruction, direct output, resize, and PNG writing are downstream choices",
                     20, 550, MUTED))

    body.append(stage_card(70, 170, 300, 145, "Decoded state",
                           ("pixels + palette", "paint mask + flags"),
                           BLUE, BLUE_LIGHT, 26))
    body.append(line(370, 242, 455, 242, BLUE, 5.0, "arrow-blue"))
    body.append(stage_card(460, 170, 330, 145, "Representation choice",
                           ("stable palette?", "direct output requested?"),
                           MAGENTA, MAGENTA_LIGHT, 25))

    branches = (
        (930, 125, "Stable palette", ("PAL8 + palette", "small intermediate"),
         MAGENTA, MAGENTA_LIGHT),
        (930, 335, "Direct / redefined", ("RGBA8888", "preserve paint-time color"),
         TEAL, TEAL_LIGHT),
        (930, 545, "Reconstruction", ("k-undither / blur", "RGB or RGBA"),
         GOLD, GOLD_LIGHT),
    )
    for x, y, title_value, details, accent, fill in branches:
        body.append(stage_card(x, y, 340, 145, title_value,
                               details, accent, fill, 25))
    body.append(path("M 790 215 H 860 V 197 H 930",
                     MAGENTA, 4.0, "arrow-magenta"))
    body.append(path("M 790 242 H 835 V 407 H 930",
                     TEAL, 4.0, "arrow-blue"))
    body.append(path("M 790 270 H 810 V 617 H 930",
                     GOLD, 4.0, "arrow-gold"))

    body.append(path("M 1100 270 V 305 H 1330 V 690 H 1110",
                     MAGENTA, 3.0, "arrow-magenta"))
    body.append(path("M 1270 407 H 1330 V 690 H 1110",
                     TEAL, 3.0, "arrow-blue"))
    body.append(path("M 1270 617 H 1330 V 690 H 1110",
                     GOLD, 3.0, "arrow-gold"))
    body.append(stage_card(790, 690, 320, 130, "Optional resize",
                           ("long edge = -s SIZE", "preserve aspect ratio"),
                           BLUE, PAPER, 25))
    body.append(line(790, 755, 690, 755, BLUE, 5.0, "arrow-blue"))
    body.append(stage_card(365, 690, 325, 130, "PNG writer",
                           ("indexed, RGB, or RGBA", "stdout, path, clipboard"),
                           GOLD, GOLD_LIGHT, 25))

    body.append(rect(70, 405, 560, 175, PAPER, GRID, 1.5, 16.0))
    body.append(text(95, 446, "Why high-color bypasses indexed reconstruction",
                     20, 760, INK))
    body.append(text_lines(95, 483,
                           ("A color register may change after earlier pixels use it.",
                            "One final palette cannot reproduce both paint-time colors.",
                            "Direct decoding is therefore correctness, not decoration."),
                           17, 28, 470, MUTED))
    return svg_document(
        width,
        height,
        "Stateful decoder and CLI output policy",
        "Decoded state branches to stable indexed output, direct output for palette redefinition, or reconstruction, then optionally resizes and writes PNG.",
        "\n".join(body),
    )


def parallel_figure() -> str:
    """Return a parallel decoder safety diagram."""
    width = 1540
    height = 920
    body = [rect(0, 0, width, height, PANEL)]
    body.append(text(45, 62, "Parallel paint is earned by a validation pass",
                     38, 820, INK))
    body.append(text(45, 96,
                     "Arbitrary byte spans are useful only after their row ownership is known",
                     20, 550, MUTED))

    body.append(stage_card(65, 150, 280, 130, "Serial anchor",
                           ("establish raster", "and initial palette"),
                           BLUE, BLUE_LIGHT, 25))
    body.append(line(345, 215, 425, 215, BLUE, 5.0, "arrow-blue"))
    body.append(stage_card(430, 150, 280, 130, "Byte spans",
                           ("split remaining input", "advance to band boundary"),
                           BLUE, PAPER, 25))
    body.append(line(710, 215, 790, 215, BLUE, 5.0, "arrow-blue"))
    body.append(stage_card(795, 150, 300, 130, "Parallel scan",
                           ("validate controls", "derive painted rows"),
                           MAGENTA, MAGENTA_LIGHT, 25))
    body.append(line(1095, 215, 1175, 215, MAGENTA, 5.0,
                     "arrow-magenta"))
    body.append(stage_card(1180, 150, 295, 130, "Ownership check",
                           ("ordered?", "row ranges disjoint?"),
                           MAGENTA, PAPER, 25))

    body.append(path("M 1328 280 V 385 H 1080",
                     TEAL, 5.0, "arrow-blue"))
    body.append(text(1210, 365, "yes", 17, 760, TEAL, "middle"))
    body.append(stage_card(760, 385, 320, 130, "All-ready barrier",
                           ("create every worker", "release as one group"),
                           TEAL, TEAL_LIGHT, 25))
    body.append(line(760, 450, 680, 450, TEAL, 5.0, "arrow-blue"))
    body.append(stage_card(360, 385, 320, 130, "Parallel paint",
                           ("one row range per worker", "no shared-row locks"),
                           TEAL, TEAL_LIGHT, 25))

    body.append(path("M 1328 280 V 640 H 1070",
                     GOLD, 5.0, "arrow-gold"))
    body.append(text(1220, 620, "no / setup failure",
                     17, 760, GOLD, "middle"))
    body.append(stage_card(720, 640, 350, 140, "Clean serial fallback",
                           ("join blocked workers", "discard partial setup",
                            "paint once serially"), GOLD, GOLD_LIGHT, 25))

    body.append(path("M 360 450 H 190 V 825 H 720",
                     TEAL, 4.0, "arrow-blue"))
    body.append(line(895, 780, 895, 825, GOLD, 4.0, "arrow-gold"))
    body.append(pill(720, 825, 350, "one decoded image", BLUE, BLUE_LIGHT))
    body.append(rect(65, 640, 520, 140, PAPER, GRID, 1.5, 16.0))
    body.append(text(90, 682, "Performance rule", 21, 760, INK))
    body.append(text_lines(90, 720,
                           ("Scan and scheduling are overhead.",
                            "Small inputs can be faster on the serial path."),
                           17, 27, 470, MUTED))
    return svg_document(
        width,
        height,
        "Parallel decoder validation and fallback",
        "After a serial anchor, byte spans are scanned in parallel. Only ordered disjoint row ranges cross an all-ready barrier into parallel paint; overlap or setup failure returns to clean serial paint.",
        "\n".join(body),
    )


def manifest() -> dict[str, object]:
    """Return provenance and semantic roles for generated figures."""
    return {
        "schema": 1,
        "generator": "tools/plot_decoding_pipeline_figures.py",
        "kind": "deterministic architecture schematics",
        "measurement": False,
        "assets": [
            {"path": "decoding-pipeline-wide.svg",
             "role": "large-screen decoding mental model"},
            {"path": "decoding-pipeline-mobile.svg",
             "role": "mobile decoding mental model"},
            {"path": "decoder-parser.svg",
             "role": "wire controls and shared parser state"},
            {"path": "decoder-raster-policy.svg",
             "role": "declared raster versus observed paint"},
            {"path": "decoder-representations.svg",
             "role": "indexed, direct, and packed output contracts"},
            {"path": "decoder-output-policy.svg",
             "role": "stateful decoder and CLI output branches"},
            {"path": "decoder-parallel.svg",
             "role": "parallel validation, barrier, and fallback"},
        ],
        "color_roles": {
            "protocol_and_control": BLUE,
            "palette_and_representation": MAGENTA,
            "paint_and_safe_parallel_work": TEAL,
            "policy_and_fallback": GOLD,
            "context": MUTED,
        },
    }


def generate(output_dir: Path) -> None:
    """Generate SVG figures and their provenance manifest."""
    figures = {
        "decoding-pipeline-wide.svg": overview_wide(),
        "decoding-pipeline-mobile.svg": overview_mobile(),
        "decoder-parser.svg": parser_figure(),
        "decoder-raster-policy.svg": raster_policy_figure(),
        "decoder-representations.svg": representations_figure(),
        "decoder-output-policy.svg": output_policy_figure(),
        "decoder-parallel.svg": parallel_figure(),
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, contents in figures.items():
        (output_dir / name).write_text(contents, encoding="utf-8")
    (output_dir / "decoding-pipeline-figures.json").write_text(
        json.dumps(manifest(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def check(output_dir: Path) -> None:
    """Regenerate into a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(
            prefix="decoding-pipeline-figures-") as temporary:
        generated = Path(temporary)
        generate(generated)
        mismatches = []
        for name in ASSET_NAMES:
            expected = output_dir / name
            actual = generated / name
            if (not expected.is_file() or
                    expected.read_bytes() != actual.read_bytes()):
                mismatches.append(name)
        if mismatches:
            raise SystemExit(
                "decoding-pipeline figures are stale or missing: "
                + ", ".join(mismatches)
            )


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=source_root / "docs/functionality/decoding-pipeline-figures",
        help="directory for generated SVG and manifest files",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare committed assets with fresh deterministic generation",
    )
    return parser.parse_args()


def main() -> None:
    """Generate or verify the decoding-pipeline assets."""
    arguments = parse_args()
    if arguments.check:
        check(arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
