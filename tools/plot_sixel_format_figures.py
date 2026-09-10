#!/usr/bin/env python3
"""Generate deterministic figures for SIXEL band encoding and node planning."""

from __future__ import annotations

import argparse
import html
import json
import tempfile
from dataclasses import dataclass
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


ASSET_NAMES = (
    "encoder-comparison-wide.svg",
    "encoder-comparison-mobile.svg",
    "node-scheduling-wide.svg",
    "node-scheduling-mobile.svg",
    "sixel-format-figures.json",
)

PIXEL_ROWS = (
    "BBAATTMM",
    "BBAATTMM",
    "BABATMTM",
    "BABATMTM",
    "AABBMMTT",
    "AABBMMTT",
)

NODE_ROWS = (
    "AAAA......AAAA..............",
    "..BBBBBB....................",
    "..............TTTTTT........",
    ".................MMMMMMMMM..",
    "............................",
    "............................",
)

TOKEN_COLORS = {
    "A": GOLD,
    "T": TEAL,
    "B": BLUE,
    "M": MAGENTA,
    ".": PAPER,
}

TOKEN_NAMES = {
    "A": "amber",
    "T": "green",
    "B": "blue",
    "M": "pink",
}

PALETTE = {
    "A": 0,
    "T": 1,
    "B": 2,
    "M": 3,
}

NETPBM_ROW_CHARACTERS = "@ACGO_"


@dataclass(frozen=True)
class Node:
    """One current-encoder color span in a six-row band."""

    token: str
    palette: int
    sx: int
    mx: int
    values: tuple[int, ...]
    serial: int


def panel(x: float, y: float, width: float, height: float,
          title_value: str, accent: str = BLUE,
          fill: str = PAPER) -> str:
    """Return a directly labeled figure panel."""
    return (
        rect(x, y, width, height, fill, accent, 1.5, 16)
        + rect(x, y, 7, height, accent, radius=3.5)
        + text(x + 27, y + 42, title_value, 22, 750, accent)
    )


def badge(x: float, y: float, value: str, color: str,
          width: float = 86) -> str:
    """Return a compact label with redundant text and color."""
    return (
        rect(x, y, width, 38, color, radius=10)
        + text(x + width / 2, y + 26, value, 15, 750,
               PAPER, "middle")
    )


def code_text(x: float, y: float, value: str, size: int = 16,
              weight: int = 650, fill: str = INK,
              anchor: str = "start") -> str:
    """Return one literal SIXEL fragment in a monospace font."""
    return tag(
        "text",
        html.escape(value),
        x=f"{x:.1f}",
        y=f"{y:.1f}",
        fill=fill,
        font_size=size,
        font_weight=weight,
        font_family="ui-monospace, SFMono-Regular, Menlo, monospace",
        text_anchor=anchor,
    )


def draw_grid(x: float, y: float, rows: Sequence[str],
              cell: float, gap: float = 2.0,
              mask_token: str | None = None) -> str:
    """Draw indexed pixels or one color plane."""
    body = []
    for row_index, row in enumerate(rows):
        for column_index, token in enumerate(row):
            visible = mask_token is None or token == mask_token
            fill = TOKEN_COLORS[token] if visible else PAPER
            body.append(
                rect(
                    x + column_index * cell,
                    y + row_index * cell,
                    cell - gap,
                    cell - gap,
                    fill,
                    GRID if not visible or token == "." else PAPER,
                    0.8,
                    2,
                )
            )
    return "".join(body)


def draw_row(x: float, y: float, row: str,
             cell: float = 20) -> str:
    """Draw one indexed source row."""
    return draw_grid(x, y, (row,), cell)


def palette_key(x: float, y: float, compact: bool = False) -> str:
    """Return the shared register-to-color mapping."""
    step = 132 if compact else 170
    width = 118 if compact else 150
    body = []
    for index, token in enumerate(("A", "T", "B", "M")):
        body.append(
            badge(
                x + index * step,
                y,
                f"#{PALETTE[token]} {TOKEN_NAMES[token]}",
                TOKEN_COLORS[token],
                width,
            )
        )
    return "".join(body)


def netpbm_row_literal(row: str, row_index: int) -> str:
    """Model ppmtosixel's default horizontal repeat encoding for one row."""
    symbol = NETPBM_ROW_CHARACTERS[row_index]
    parts = []
    start = 0
    while start < len(row):
        end = start + 1
        while end < len(row) and row[end] == row[start]:
            end += 1
        count = end - start
        parts.append(f"#{PALETTE[row[start]]}")
        parts.append(symbol if count == 1 else f"!{count}{symbol}")
        start = end
    parts.append("$")
    return "".join(parts)


def ppmtosixel_body(rows: Sequence[str]) -> str:
    """Return ppmtosixel paint/control bytes with formatting LFs omitted."""
    body = [netpbm_row_literal(row, index)
            for index, row in enumerate(rows)]
    if len(rows) == 6:
        body.append("-")
    return "".join(body)


def active_tokens(rows: Sequence[str]) -> list[str]:
    """Return palette tokens in first-pixel discovery order."""
    seen = set()
    result = []
    for row in rows:
        for token in row:
            if token != "." and token not in seen:
                seen.add(token)
                result.append(token)
    return result


def plane_values(rows: Sequence[str], token: str) -> tuple[int, ...]:
    """Pack one color plane into per-column six-bit masks."""
    width = len(rows[0])
    values = []
    for column in range(width):
        value = 0
        for row_index, row in enumerate(rows):
            if row[column] == token:
                value |= 1 << row_index
        values.append(value)
    return tuple(values)


def compose_nodes(rows: Sequence[str]) -> tuple[Node, ...]:
    """Model the current node grouping and column-bucket ordering."""
    nodes = []
    serial = 0
    for token in active_tokens(rows):
        values = plane_values(rows, token)
        positions = [index for index, value in enumerate(values) if value]
        start_index = 0
        while start_index < len(positions):
            sx = positions[start_index]
            end_index = start_index
            while end_index + 1 < len(positions):
                blank_columns = positions[end_index + 1] - positions[end_index] - 1
                if blank_columns >= 10:
                    break
                end_index += 1
            mx = positions[end_index] + 1
            nodes.append(
                Node(token, PALETTE[token], sx, mx, values, serial)
            )
            serial += 1
            start_index = end_index + 1
    return tuple(
        sorted(nodes, key=lambda node: (node.sx, -node.mx, -node.serial))
    )


def schedule_nodes(nodes: Sequence[Node]) -> tuple[tuple[Node, ...], ...]:
    """Pack the ordered list into greedy non-overlapping emit sweeps."""
    remaining = list(nodes)
    sweeps = []
    while remaining:
        cursor = 0
        selected = []
        for node in remaining:
            if node.sx >= cursor:
                selected.append(node)
                cursor = node.mx
        selected_ids = {id(node) for node in selected}
        remaining = [node for node in remaining
                     if id(node) not in selected_ids]
        sweeps.append(tuple(selected))
    return tuple(sweeps)


def emit_run(symbol: str, count: int) -> str:
    """Model libsixel's repeat threshold for one symbol run."""
    return f"!{count}{symbol}" if count > 3 else symbol * count


def emit_values(values: Sequence[int]) -> str:
    """Serialize per-column masks with libsixel's run coalescing rule."""
    parts = []
    start = 0
    while start < len(values):
        end = start + 1
        while end < len(values) and values[end] == values[start]:
            end += 1
        parts.append(emit_run(chr(values[start] + ord("?")), end - start))
        start = end
    return "".join(parts)


def fast_body(rows: Sequence[str]) -> str:
    """Model the current non-fill node emitter for one six-row band."""
    nodes = compose_nodes(rows)
    sweeps = schedule_nodes(nodes)
    body = []
    active_palette = -1
    for sweep_index, sweep in enumerate(sweeps):
        cursor = 0
        if sweep_index:
            body.append("$")
        for node in sweep:
            if active_palette != node.palette:
                body.append(f"#{node.palette}")
                active_palette = node.palette
            if cursor < node.sx:
                body.append(emit_values((0,) * (node.sx - cursor)))
            body.append(emit_values(node.values[node.sx:node.mx]))
            cursor = node.mx
    return "".join(body)


NETPBM_ROWS = tuple(
    netpbm_row_literal(row, index)
    for index, row in enumerate(PIXEL_ROWS)
)
NETPBM_BODY = ppmtosixel_body(PIXEL_ROWS)
FAST_NODES = compose_nodes(PIXEL_ROWS)
FAST_SWEEPS = schedule_nodes(FAST_NODES)
FAST_BODY = fast_body(PIXEL_ROWS)
NODE_NODES = compose_nodes(NODE_ROWS)
NODE_SWEEPS = schedule_nodes(NODE_NODES)
NODE_BODY = fast_body(NODE_ROWS)

assert NETPBM_BODY == (
    "#2!2@#0!2@#1!2@#3!2@$"
    "#2!2A#0!2A#1!2A#3!2A$"
    "#2C#0C#2C#0C#1C#3C#1C#3C$"
    "#2G#0G#2G#0G#1G#3G#1G#3G$"
    "#0!2O#2!2O#3!2O#1!2O$"
    "#0!2_#2!2_#3!2_#1!2_$-"
)
assert len(NETPBM_BODY) == 135
assert FAST_BODY == "#0o{BN#3o{BN$#2NB{o#1NB{o"
assert len(FAST_BODY) == 25
assert [(node.palette, node.sx, node.mx) for node in NODE_NODES] == [
    (0, 0, 14),
    (2, 2, 8),
    (1, 14, 20),
    (3, 17, 26),
]
assert [[node.palette for node in sweep] for sweep in NODE_SWEEPS] == [
    [0, 1],
    [2, 3],
]
assert NODE_BODY == "#0!4@!6?!4@#1!6C$#2??!6A#3!9?!9G"


def comparison_wide() -> str:
    """Compare row-oriented and band-oriented encoding."""
    width = 1560
    height = 1160
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(45, 60, "One band, two legal SIXEL paint orders", 38, 800),
        text(45, 96,
             "Netpbm emits one source row at a time; img2sixel -E fast packs six rows into color-plane masks.",
             19, 550, MUTED),
        panel(45, 130, 1470, 230, "Shared 8 × 6 indexed input", TEAL),
        draw_grid(78, 185, PIXEL_ROWS, 25),
        palette_key(345, 185),
        text(345, 250, "The img2sixel run uses the same register order as ppmtosixel.",
             16, 600, MUTED),
        code_text(345, 292, "#0 amber   #1 green   #2 blue   #3 pink", 17, 700),
        text(345, 326, "Palette definitions, DCS/ST, and ppmtosixel's formatting LF bytes are outside the comparison.",
             15, 550, MUTED),
        panel(45, 395, 720, 685,
              "Netpbm ppmtosixel: scan six source rows", MAGENTA,
              MAGENTA_LIGHT),
        text(78, 458, "Each row chooses one single-bit character; only adjacent same-color pixels are repeated.",
             15, 600, MUTED),
        panel(795, 395, 720, 685,
              "img2sixel -E fast: scan four color planes", BLUE,
              BLUE_LIGHT),
        text(828, 458, "Each color plane contributes six-bit columns; node scheduling joins non-overlapping spans.",
             15, 600, MUTED),
    ])

    for index, (row, literal) in enumerate(zip(PIXEL_ROWS, NETPBM_ROWS)):
        y = 500 + index * 77
        symbol = NETPBM_ROW_CHARACTERS[index]
        body.extend([
            badge(78, y, f"y={index}  {symbol}", TOKEN_COLORS[row[0]], 80),
            draw_row(175, y + 3, row, 20),
            code_text(355, y + 25, literal, 16, 700),
        ])

    masks = (
        ("A", "#0", "o{BN"),
        ("M", "#3", "o{BN"),
        ("B", "#2", "NB{o"),
        ("T", "#1", "NB{o"),
    )
    for index, (token, register, literal) in enumerate(masks):
        y = 500 + index * 112
        body.extend([
            badge(828, y, f"{register} {TOKEN_NAMES[token]}",
                  TOKEN_COLORS[token], 112),
            draw_grid(958, y - 3, PIXEL_ROWS, 13, 2, token),
            code_text(1080, y + 27, f"{register}{literal}", 20, 750,
                      TOKEN_COLORS[token]),
            text(1080, y + 56, "one six-bit mask per column", 14, 550,
                 MUTED),
        ])

    body.extend([
        rect(78, 986, 654, 62, PAPER, MAGENTA, 1.2, 10),
        badge(92, 998, "135 bytes", MAGENTA, 105),
        text(215, 1024, "six row streams, six $ returns, final -", 16, 700,
             MAGENTA),
        rect(828, 986, 654, 62, PAPER, BLUE, 1.2, 10),
        badge(842, 998, "25 bytes", BLUE, 100),
        code_text(958, 1024, FAST_BODY, 15, 700),
        rect(195, 1100, 1170, 42, TEAL_LIGHT, TEAL, 1.2, 10),
        text(780, 1127,
             "Same indexed layout; the comparison isolates paint/control bytes, not palette construction or color rounding.",
             16, 700, TEAL, "middle"),
    ])
    return svg_document(
        width,
        height,
        "One band, two legal SIXEL paint orders",
        "The same eight by six four-color indexed band is encoded by Netpbm ppmtosixel as six row streams totaling 135 paint and control bytes, and by img2sixel fast as four six-bit color planes totaling 25 bytes. Palette definitions, envelopes, and Netpbm formatting line feeds are omitted.",
        "\n".join(body),
    )


def comparison_mobile() -> str:
    """Return the mobile comparison layout."""
    width = 760
    height = 2400
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(38, 56, "One band, two paint orders", 36, 800),
        text(38, 90, "Row-at-a-time versus six-row color planes", 18, 550,
             MUTED),
        panel(55, 125, 650, 345, "Shared 8 × 6 indexed input", TEAL),
        draw_grid(232, 190, PIXEL_ROWS, 37),
        palette_key(90, 430, True),
        panel(55, 510, 650, 835,
              "Netpbm ppmtosixel: six rows", MAGENTA,
              MAGENTA_LIGHT),
        text(85, 572, "One single-bit character per source row.", 15, 600,
             MUTED),
    ])
    for index, (row, literal) in enumerate(zip(PIXEL_ROWS, NETPBM_ROWS)):
        y = 615 + index * 105
        symbol = NETPBM_ROW_CHARACTERS[index]
        body.extend([
            badge(85, y, f"y={index}  {symbol}", TOKEN_COLORS[row[0]], 80),
            draw_row(185, y + 3, row, 24),
            code_text(85, y + 74, literal, 15, 700),
        ])
    body.extend([
        badge(85, 1275, "135 bytes", MAGENTA, 110),
        text(215, 1301, "six $ returns and a final -", 15, 650, MAGENTA),
        panel(55, 1390, 650, 760,
              "img2sixel -E fast: four color planes", BLUE,
              BLUE_LIGHT),
        text(85, 1452, "Each data character carries up to six vertical bits.",
             15, 600, MUTED),
    ])
    masks = (
        ("A", "#0", "o{BN"),
        ("M", "#3", "o{BN"),
        ("B", "#2", "NB{o"),
        ("T", "#1", "NB{o"),
    )
    for index, (token, register, literal) in enumerate(masks):
        column = index % 2
        row_index = index // 2
        x = 85 + column * 300
        y = 1490 + row_index * 245
        body.extend([
            badge(x, y, f"{register} {TOKEN_NAMES[token]}",
                  TOKEN_COLORS[token], 112),
            draw_grid(x, y + 55, PIXEL_ROWS, 18, 2, token),
            code_text(x + 165, y + 94, f"{register}{literal}", 17, 750,
                      TOKEN_COLORS[token]),
        ])
    body.extend([
        rect(85, 2010, 590, 64, PAPER, BLUE, 1.2, 10),
        badge(98, 2023, "25 bytes", BLUE, 100),
        code_text(215, 2049, FAST_BODY, 15, 700),
        rect(70, 2200, 620, 140, TEAL_LIGHT, TEAL, 1.2, 12),
        text(380, 2240, "Same indexed layout", 20, 800, TEAL, "middle"),
        text(380, 2275, "135 → 25 paint/control bytes in this band", 17,
             700, TEAL, "middle"),
        text_lines(380, 2310,
                   ("DCS, palette definitions, ST, and Netpbm LF",
                    "formatting bytes are omitted."),
                   14, 21, 550, MUTED, "middle"),
    ])
    return svg_document(
        width,
        height,
        "One band, two legal SIXEL paint orders",
        "Mobile comparison of Netpbm's six row streams and img2sixel fast's four six-bit color-plane masks for one shared indexed band.",
        "\n".join(body),
    )


def interval_bar(x: float, y: float, scale: float, node: Node,
                 height: float = 34) -> str:
    """Return one directly labeled node interval."""
    start = x + node.sx * scale
    width = (node.mx - node.sx) * scale
    label = f"#{node.palette} [{node.sx},{node.mx})"
    return (
        rect(start, y, width, height, TOKEN_COLORS[node.token],
             radius=7)
        + text(start + width / 2, y + 23, label, 13, 750,
               PAPER, "middle")
    )


def interval_axis(x: float, y: float, scale: float,
                  width_columns: int) -> str:
    """Return a quiet column axis for node intervals."""
    body = [line(x, y, x + width_columns * scale, y, GRID, 1)]
    for column in range(0, width_columns + 1, 2):
        position = x + column * scale
        body.append(line(position, y - 4, position, y + 4, GRID, 1))
        if column % 4 == 0:
            body.append(text(position, y + 22, str(column), 12, 550,
                             MUTED, "middle"))
    return "".join(body)


def node_scheduling_wide() -> str:
    """Explain node composition, ordering, and non-overlapping sweeps."""
    width = 1560
    height = 1040
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(45, 60, "libsixel turns color spans into non-overlapping sweeps",
             38, 800),
        text(45, 96,
             "A node is a palette register plus one horizontal [sx, mx) interval and its six-bit column map.",
             19, 550, MUTED),
        panel(45, 130, 1470, 310,
              "1  Compose nodes from per-color band maps", TEAL),
        draw_grid(78, 205, NODE_ROWS, 20),
        interval_axis(78, 188, 20, 28),
        path("M 78 345 V 365 H 358 V 345", GOLD, 2),
        text(218, 390, "#0: x=0..3 and x=10..13", 15, 750,
             GOLD, "middle"),
        text(218, 416, "six blank columns stay inside one node [0,14)",
             14, 600, MUTED, "middle"),
        badge(475, 350, "white = key color", MUTED, 155),
        text_lines(720, 215,
                   ("Scan one color plane from left to right.",
                    "Start at its first nonzero column sx.",
                    "Bridge gaps of at most nine zero columns.",
                    "Stop at mx before a gap of ten or at plane end."),
                   16, 35, 600, INK),
        rect(715, 365, 745, 48, GOLD_LIGHT, GOLD, 1.2, 9),
        text(1088, 396,
             "A node is a horizontal span, not necessarily one connected pixel component.",
             15, 700, GOLD, "middle"),
        panel(45, 480, 720, 475,
              "2  Order the node list", BLUE, BLUE_LIGHT),
        text(78, 542, "sx ascending; for equal sx, farther mx first",
             15, 650, MUTED),
        interval_axis(195, 575, 18, 28),
        panel(795, 480, 720, 475,
              "3  Greedily form emit sweeps", MAGENTA,
              MAGENTA_LIGHT),
        text(828, 542,
             "Take a node only when sx ≥ cursor; leave overlaps for the next $ sweep.",
             15, 650, MUTED),
        interval_axis(915, 575, 18, 28),
    ])
    for index, node in enumerate(NODE_NODES):
        y = 620 + index * 72
        body.extend([
            text(78, y + 23, str(index + 1), 16, 800,
                 TOKEN_COLORS[node.token]),
            text(105, y + 23, TOKEN_NAMES[node.token], 14, 650, INK),
            interval_bar(195, y, 18, node),
        ])

    lane_y = (640, 775)
    for sweep_index, sweep in enumerate(NODE_SWEEPS):
        y = lane_y[sweep_index]
        body.append(text(828, y + 23, f"sweep {sweep_index + 1}",
                         15, 750, MAGENTA))
        for node in sweep:
            body.append(interval_bar(915, y, 18, node))
        if sweep_index:
            badge(828, y + 48, "$ reset", MAGENTA, 82)
    body.extend([
        text(915, 730, "#0 ends at 14, so #1 can follow without $",
             14, 650, GOLD),
        text(915, 865, "#2 ends at 8; padding reaches #3 at 17",
             14, 650, BLUE),
        rect(828, 892, 654, 44, PAPER, MAGENTA, 1.2, 9),
        code_text(845, 920, NODE_BODY, 15, 700),
        rect(190, 980, 1180, 42, TEAL_LIGHT, TEAL, 1.2, 10),
        text(780, 1007,
             "The heuristic reduces carriage returns and palette revisits; it does not search every possible byte-minimal ordering.",
             16, 700, TEAL, "middle"),
    ])
    return svg_document(
        width,
        height,
        "libsixel turns color spans into non-overlapping sweeps",
        "A twenty-eight-column six-row example becomes four nodes ordered by start and end columns. The greedy emitter places palette zero then palette one in the first non-overlapping sweep, resets with a carriage return, and places palette two then palette three in the second sweep.",
        "\n".join(body),
    )


def node_scheduling_mobile() -> str:
    """Return the mobile node-scheduling layout."""
    width = 760
    height = 2250
    body = [rect(0, 0, width, height, PANEL)]
    body.extend([
        text(38, 56, "From color spans to emit sweeps", 36, 800),
        text(38, 90, "The current libsixel node heuristic", 18, 550,
             MUTED),
        panel(55, 125, 650, 545,
              "1  Compose nodes from color maps", TEAL),
        interval_axis(95, 205, 20, 28),
        draw_grid(95, 235, NODE_ROWS, 20),
        path("M 95 375 V 395 H 375 V 375", GOLD, 2),
        text(235, 423, "six blank columns", 15, 750,
             GOLD, "middle"),
        text(235, 450, "still one #0 node [0,14)", 14, 600,
             MUTED, "middle"),
        badge(440, 382, "white = key color", MUTED, 155),
        text_lines(85, 510,
                   ("Start at the first nonzero sx.",
                    "Bridge at most nine zero columns.",
                    "Split before a gap of ten; mx is exclusive.",
                    "A node can contain internal ? columns."),
                   15, 31, 600, INK),
        panel(55, 710, 650, 690,
              "2  Order nodes by sx, then mx", BLUE, BLUE_LIGHT),
        text(85, 772, "The numbered list is the emitter's scan order.",
             15, 600, MUTED),
        interval_axis(165, 815, 17, 28),
    ])
    for index, node in enumerate(NODE_NODES):
        y = 865 + index * 115
        body.extend([
            badge(85, y, str(index + 1), TOKEN_COLORS[node.token], 48),
            text(85, y + 66, TOKEN_NAMES[node.token], 14, 650, INK),
            interval_bar(165, y, 17, node, 38),
        ])
    body.extend([
        panel(55, 1440, 650, 650,
              "3  Pack non-overlapping sweeps", MAGENTA,
              MAGENTA_LIGHT),
        text_lines(85, 1502,
                   ("Take sx ≥ cursor in the current sweep.",
                    "Skip overlaps without deleting them.",
                    "Reset with $ and repeat until none remain."),
                   15, 29, 600, MUTED),
        interval_axis(165, 1620, 17, 28),
        text(85, 1688, "sweep 1", 15, 750, MAGENTA),
        text(85, 1833, "sweep 2", 15, 750, MAGENTA),
        badge(85, 1875, "$ reset", MAGENTA, 82),
    ])
    for node in NODE_SWEEPS[0]:
        body.append(interval_bar(165, 1660, 17, node, 40))
    for node in NODE_SWEEPS[1]:
        body.append(interval_bar(165, 1805, 17, node, 40))
    body.extend([
        rect(85, 1970, 590, 50, PAPER, MAGENTA, 1.2, 9),
        code_text(100, 2002, NODE_BODY, 15, 700),
        rect(70, 2135, 620, 78, TEAL_LIGHT, TEAL, 1.2, 12),
        text(380, 2168, "Fewer $ resets and palette revisits", 18, 800,
             TEAL, "middle"),
        text(380, 2196, "deterministic greedy heuristic, not an exhaustive optimum",
             14, 600, MUTED, "middle"),
    ])
    return svg_document(
        width,
        height,
        "From color spans to emit sweeps",
        "Mobile explanation of the current libsixel node heuristic: nearby same-color runs become span nodes, the nodes are ordered by start and end columns, and a greedy scan packs them into non-overlapping emit sweeps separated by carriage returns.",
        "\n".join(body),
    )


def manifest() -> dict[str, object]:
    """Return provenance and semantic data for the generated figures."""
    return {
        "schema": 1,
        "generator": "tools/plot_sixel_format_figures.py",
        "kind": "deterministic explanatory schematic with literal bodies",
        "measurement": False,
        "scope": "ordinary SIXEL band encoding and current libsixel node scheduling",
        "assets": [
            {
                "path": name,
                "role": "mobile portrait" if "mobile" in name
                else "large screen",
            }
            for name in ASSET_NAMES[:-1]
        ],
        "comparison_fixture": {
            "dimensions": "8x6",
            "rows": list(PIXEL_ROWS),
            "palette": {
                f"#{PALETTE[token]}": TOKEN_NAMES[token]
                for token in ("A", "T", "B", "M")
            },
            "ppmtosixel_rows": list(NETPBM_ROWS),
            "ppmtosixel_body_without_lf": NETPBM_BODY,
            "ppmtosixel_body_bytes_without_lf": len(NETPBM_BODY),
            "img2sixel_fast_body": FAST_BODY,
            "img2sixel_fast_body_bytes": len(FAST_BODY),
            "omitted": [
                "DCS envelope",
                "raster attributes",
                "palette definitions",
                "string terminator",
                "ppmtosixel formatting LF bytes",
            ],
        },
        "node_fixture": {
            "dimensions": "28x6",
            "keycolor": 4,
            "dot": "unpainted key-color pixel",
            "rows": list(NODE_ROWS),
            "nodes": [
                {
                    "palette": node.palette,
                    "sx": node.sx,
                    "mx": node.mx,
                }
                for node in NODE_NODES
            ],
            "sweeps": [
                [node.palette for node in sweep]
                for sweep in NODE_SWEEPS
            ],
            "body": NODE_BODY,
        },
        "sources": {
            "netpbm_manual": "https://netpbm.sourceforge.net/doc/ppmtosixel.html",
            "netpbm_source": "https://svn.code.sf.net/p/netpbm/code/stable/converter/ppm/ppmtosixel.c",
            "libsixel_source": "src/encoder-core-encode.c",
        },
        "color_roles": {
            "ordinary_encoder": BLUE,
            "classic_row_encoder": MAGENTA,
            "node_composition": TEAL,
            "node_gap_heuristic": GOLD,
            "context": MUTED,
        },
        "review": {
            "story_job": "compare paint orders and explain node scheduling",
            "primary": "responsive inspectable SVG",
            "fallback": "adjacent prose, literal text blocks, and alt text",
            "mobile": "separate portrait layouts preserve the same evidence",
            "accessibility": "register labels and intervals repeat every color role",
            "qa": "wide and mobile assets require rendered visual review",
        },
    }


def write_assets(output_dir: Path,
                 assets: Iterable[tuple[str, Callable[[], str]]]) -> None:
    """Write SVG assets and their manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, factory in assets:
        (output_dir / name).write_text(factory(), encoding="utf-8")
    (output_dir / "sixel-format-figures.json").write_text(
        json.dumps(manifest(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def generate(output_dir: Path) -> None:
    """Generate the SIXEL format figure family."""
    write_assets(
        output_dir,
        (
            ("encoder-comparison-wide.svg", comparison_wide),
            ("encoder-comparison-mobile.svg", comparison_mobile),
            ("node-scheduling-wide.svg", node_scheduling_wide),
            ("node-scheduling-mobile.svg", node_scheduling_mobile),
        ),
    )


def check(output_dir: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(prefix="sixel-format-figures-") as temp:
        generated = Path(temp)
        generate(generated)
        mismatches = [
            name
            for name in ASSET_NAMES
            if not (output_dir / name).is_file()
            or (output_dir / name).read_bytes()
            != (generated / name).read_bytes()
        ]
        if mismatches:
            raise SystemExit(
                "SIXEL format figures are stale or missing: "
                + ", ".join(mismatches)
            )


def parse_args() -> argparse.Namespace:
    """Parse output-directory and check-mode options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=source_root / "docs/sixel-format-figures",
    )
    parser.add_argument("--check", action="store_true")
    return parser.parse_args()


def main() -> None:
    """Generate or verify the SIXEL format figures."""
    arguments = parse_args()
    if arguments.check:
        check(arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
