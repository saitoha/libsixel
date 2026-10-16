#!/usr/bin/env python3
"""Generate deterministic figures for the SIXEL codec tutorial.

Every literal SIXEL fragment shown in a figure is computed here from the
pixel fixtures below, with the same rules as the tutorial programs in
docs/tutorial/sixel-codec/code, so the pictures cannot drift from the
code they explain.
"""

from __future__ import annotations

import argparse
import html
import json
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Sequence

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
)


WIDTH = 720
MONO = "ui-monospace, SFMono-Regular, Menlo, monospace"

# Pixel art tokens. '.' is an unpainted cell in lesson 0 and the white
# background register 0 in the encoder examples.
TOKEN_FILL = {
    ".": PAPER,
    "R": "#DC2626",
    "G": "#16A34A",
    "T": "#92400E",
    "S": "#7DD3FC",
    "Y": "#FACC15",
    "B": "#2563EB",
    "W": PAPER,
}
TOKEN_INK = {
    ".": MUTED,
    "R": PAPER,
    "G": PAPER,
    "T": PAPER,
    "S": INK,
    "Y": INK,
    "B": PAPER,
    "W": MUTED,
}

HEART = (
    ".RR.RR.",
    "RRRRRRR",
    "RRRRRRR",
    ".RRRRR.",
    "..RRR..",
    "...R...",
)

TREE = (
    "...G...",
    "..GGG..",
    ".GGGGG.",
    "..GGG..",
    ".GGGGG.",
    "GGGGGGG",
    "GGGGGGG",
    "...T...",
    "...T...",
    "...T...",
    "..TTT..",
    ".......",
)
TREE_REGISTERS = {"G": 1, "T": 2}

SCENE = (
    "SSSSSSYY",
    "SSSSSSYY",
    "SSSSSSSS",
    "SSSSSSSS",
    "GGSSSGGG",
    "GGGGGGGG",
)
SCENE_REGISTERS = {"S": 0, "Y": 1, "G": 2}

SHAPES = (
    ".RR....YY...........YY..",
    "RRRR...YY....GGG....YY..",
    "RRRR.........GGG........",
    ".RRBBBBBBBB..GGG........",
    "...BBBBBBBB..GGG........",
    "........................",
)
SHAPES_REGISTERS = {".": 0, "R": 1, "Y": 2, "G": 3, "B": 4}

NODE_GAP = 9

ASSET_NAMES = (
    "big-picture.svg",
    "journey.svg",
    "speed.svg",
    "pnm-layout.svg",
    "six-pin-stamp.svg",
    "heart-columns.svg",
    "character-table.svg",
    "tree-cursor.svg",
    "stream-anatomy.svg",
    "step1-passes.svg",
    "step1-repeat.svg",
    "step1-decoder-state.svg",
    "step2-classify.svg",
    "step2-nodes.svg",
    "step2-packing.svg",
    "step2-repeat-fill.svg",
    "step3-memory.svg",
    "step3-column-word.svg",
    "step3-swar.svg",
    "step3-first-colors.svg",
    "step3-occupancy.svg",
    "step3-masked-store.svg",
    "step4-lanes.svg",
    "step4-block.svg",
    "step4-transpose.svg",
    "step5-encoder.svg",
    "step5-decoder.svg",
    "step5-amdahl.svg",
    "sixel-codec-tutorial-figures.json",
)

# Measured results from docs/tutorial/sixel-codec/README.md, 2400x1800
# photograph: (encode ms, decode ms) per step.
PHOTO_2400 = (
    (319.8, 33.0),
    (15.6, 12.4),
    (15.3, 7.95),
    (12.7, 7.04),
    (2.18, 1.98),
)


# ------------------------------------------------------------------ #
# SIXEL rules shared with the tutorial programs.                      #


def column_values(rows: Sequence[str], token: str) -> list[int]:
    """Return the six-bit value of every column for one token."""
    width = len(rows[0])
    values = []
    for x in range(width):
        bits = 0
        for r, row in enumerate(rows[:6]):
            if row[x] == token:
                bits |= 1 << r
        values.append(bits)
    return values


def sixel_char(value: int) -> str:
    """Return the data character of a six-bit value."""
    return chr(63 + value)


def run(ch: str, count: int) -> str:
    """Write a run as buf_putrun() does: '!' only when it is shorter."""
    if count > 3:
        return f"!{count}{ch}"
    return ch * count


def put_span(values: Sequence[int], start: int, end: int) -> str:
    """Run-length code values[start:end]."""
    out = []
    x = start
    while x < end:
        n = 1
        while x + n < end and values[x + n] == values[x]:
            n += 1
        out.append(run(sixel_char(values[x]), n))
        x += n
    return "".join(out)


def step1_band(rows: Sequence[str], registers: dict[str, int]) -> list[str]:
    """Return step 1's line for every register of one band."""
    lines = []
    for token, c in sorted(registers.items(), key=lambda item: item[1]):
        values = column_values(rows, token)
        end = len(values)
        while end > 0 and values[end - 1] == 0:
            end -= 1
        prefix = f"#{c}" if c == 0 else f"$#{c}"
        lines.append(prefix + put_span(values, 0, end))
    return lines


def lesson_band(rows: Sequence[str], registers: dict[str, int]) -> str:
    """Return a hand-written style band: present colors only."""
    parts = []
    for token, c in sorted(registers.items(), key=lambda item: item[1]):
        values = column_values(rows, token)
        end = len(values)
        while end > 0 and values[end - 1] == 0:
            end -= 1
        if end == 0:
            continue
        parts.append(f"#{c}" + put_span(values, 0, end))
    return "$".join(parts)


@dataclass(frozen=True)
class Node:
    """One color span of a band, as in step 2."""

    token: str
    color: int
    sx: int
    mx: int


def compose_nodes(rows: Sequence[str],
                  registers: dict[str, int]) -> list[Node]:
    """Cut every color row into nodes and order them as step 2 does."""
    width = len(rows[0])
    buckets: list[list[Node]] = [[] for _ in range(width)]
    for token, c in sorted(registers.items(), key=lambda item: item[1]):
        values = column_values(rows, token)
        sx = 0
        while sx < width:
            while sx < width and values[sx] == 0:
                sx += 1
            if sx == width:
                break
            mx = sx + 1
            x = mx
            while x < width and x - mx <= NODE_GAP:
                if values[x] != 0:
                    mx = x + 1
                x += 1
            node = Node(token, c, sx, mx)
            bucket = buckets[sx]
            index = 0
            while index < len(bucket) and bucket[index].mx >= mx:
                index += 1
            bucket.insert(index, node)
            sx = mx
    return [node for bucket in buckets for node in bucket]


def pack_lines(nodes: Sequence[Node]) -> list[list[Node]]:
    """Greedily pack ordered nodes onto graphics lines."""
    remaining = list(nodes)
    lines = []
    while remaining:
        x = 0
        taken = []
        left = []
        for node in remaining:
            if node.sx < x:
                left.append(node)
                continue
            taken.append(node)
            x = node.mx
        lines.append(taken)
        remaining = left
    return lines


def step2_band(rows: Sequence[str], registers: dict[str, int]) -> str:
    """Return step 2's bytes for one band."""
    out = []
    color = -1
    for sweep in pack_lines(compose_nodes(rows, registers)):
        if color >= 0:
            out.append("$")
        x = 0
        for node in sweep:
            if node.color != color:
                out.append(f"#{node.color}")
                color = node.color
            out.append(run("?", node.sx - x))
            out.append(put_span(column_values(rows, node.token),
                                node.sx, node.mx))
            x = node.mx
    return "".join(out)


HEART_BODY = lesson_band(HEART, {"R": 1})
TREE_BANDS = (lesson_band(TREE[:6], TREE_REGISTERS),
              lesson_band(TREE[6:], TREE_REGISTERS))
TREE_BODY = "-".join(TREE_BANDS)
SCENE_LINES = step1_band(SCENE, SCENE_REGISTERS)
SHAPES_NODES = compose_nodes(SHAPES, SHAPES_REGISTERS)
SHAPES_LINES = pack_lines(SHAPES_NODES)
SHAPES_BODY = step2_band(SHAPES, SHAPES_REGISTERS)

assert HEART_BODY == "#1EN^}^NE", HEART_BODY


# ------------------------------------------------------------------ #
# Drawing helpers.                                                    #


def mono(x: float, y: float, value: str, size: int = 20,
         weight: int = 650, fill: str = INK,
         anchor: str = "start") -> str:
    """Return monospace text."""
    return tag(
        "text",
        html.escape(value),
        x=f"{x:.1f}",
        y=f"{y:.1f}",
        fill=fill,
        font_size=size,
        font_weight=weight,
        font_family=MONO,
        text_anchor=anchor,
        xml_space="preserve",
    )


def title(value: str, subtitle: str | None = None) -> str:
    """Return the figure heading."""
    body = text(32, 50, value, 28, 800, INK)
    if subtitle:
        body += text(32, 80, subtitle, 18, 500, MUTED)
    return body


def cell(x: float, y: float, size: float, fill: str,
         label: str = "", label_fill: str = INK,
         stroke: str = GRID, label_size: int | None = None,
         monospace: bool = True) -> str:
    """Return one square with an optional centered label."""
    body = rect(x, y, size, size, fill, stroke, 1.2, 3)
    if label:
        font = label_size or int(size * 0.5)
        if monospace:
            body += mono(x + size / 2, y + size / 2 + font * 0.36, label,
                         font, 700, label_fill, "middle")
        else:
            body += text(x + size / 2, y + size / 2 + font * 0.36, label,
                         font, 700, label_fill, "middle")
    return body


def grid(x: float, y: float, rows: Sequence[str], size: float,
         only: str | None = None, dim: bool = False) -> str:
    """Draw pixel art. With only, other tokens are drawn faintly."""
    body = []
    for r, row in enumerate(rows):
        for c, token in enumerate(row):
            fill = TOKEN_FILL[token]
            if only is not None and token != only:
                fill = PANEL if dim else PAPER
            body.append(rect(x + c * size, y + r * size, size - 2,
                             size - 2, fill, GRID, 1.0, 2))
    return "".join(body)


def box(x: float, y: float, width: float, height: float,
        accent: str = BLUE, fill: str = PAPER) -> str:
    """Return a rounded panel."""
    return rect(x, y, width, height, fill, accent, 1.6, 14)


def arrow(x1: float, y1: float, x2: float, y2: float,
          color: str = BLUE, width: float = 3.0) -> str:
    """Return an arrow; markers exist for blue, magenta and gold."""
    marker = {BLUE: "arrow-blue", MAGENTA: "arrow-magenta",
              GOLD: "arrow-gold"}.get(color, "arrow-blue")
    return line(x1, y1, x2, y2, color, width, marker)


def note(x: float, y: float, lines: Sequence[str], size: int = 18,
         fill: str = MUTED, weight: int = 500,
         anchor: str = "start", spacing: float = 1.4) -> str:
    """Return several lines of prose."""
    return "".join(text(x, y + i * size * spacing, value, size, weight,
                        fill, anchor) for i, value in enumerate(lines))


def document(height: int, heading: str, description: str,
             body: Sequence[str]) -> str:
    """Wrap a figure body."""
    return svg_document(WIDTH, height, heading, description,
                        rect(0, 0, WIDTH, height, PANEL) + "\n"
                        + "\n".join(body))


def bits_label(value: int) -> str:
    """Return a six-bit binary label, row 5 first as people write it."""
    return format(value, "06b")


# ------------------------------------------------------------------ #
# README figures.                                                     #


def big_picture() -> str:
    """Image -> encoder -> text -> decoder -> image."""
    body = [title("What a SIXEL codec does",
                  "An encoder turns pixels into text; a decoder turns it back.")]
    body.append(box(32, 110, 200, 210, TEAL))
    body.append(text(132, 145, "Picture", 22, 750, TEAL, "middle"))
    body.append(grid(80, 165, TREE, 9.5))
    body.append(text(132, 300, "7 × 12 pixels", 16, 600, MUTED, "middle"))

    body.append(box(488, 110, 200, 210, TEAL))
    body.append(text(588, 145, "Picture again", 22, 750, TEAL, "middle"))
    body.append(grid(536, 165, TREE, 9.5))
    body.append(text(588, 300, "the same pixels", 16, 600, MUTED,
                     "middle"))

    body.append(arrow(240, 200, 300, 200, BLUE))
    body.append(text(270, 185, "encode", 17, 700, BLUE, "middle"))
    body.append(arrow(420, 200, 480, 200, MAGENTA))
    body.append(text(450, 185, "decode", 17, 700, MAGENTA, "middle"))
    body.append(box(305, 150, 110, 100, GOLD, GOLD_LIGHT))
    body.append(text(360, 185, "text", 20, 750, GOLD, "middle"))
    body.append(text(360, 212, "you can", 15, 600, MUTED, "middle"))
    body.append(text(360, 232, "print", 15, 600, MUTED, "middle"))

    body.append(box(32, 350, 656, 120, GOLD))
    body.append(text(52, 382, "The text in the middle is a SIXEL stream:",
                     18, 650, INK))
    body.append(mono(52, 418, "ESC P q #1;2;0;60;0 #2;2;50;30;10", 18,
                     650, GOLD))
    body.append(mono(52, 448, TREE_BODY + " ESC \\", 18, 650, GOLD))
    body.append(note(32, 505, (
        "Send it to a terminal that understands SIXEL and the terminal",
        "draws the tree. Lesson 0 shows how to read every byte of it.",
    )))
    return document(560, "What a SIXEL codec does",
                    "A seven by twelve pixel tree is encoded into a short "
                    "line of text, the SIXEL stream "
                    + TREE_BODY + ", and decoded back into the same "
                    "pixels.", body)


JOURNEY = (
    ("0", "Read SIXEL", "draw a heart by hand", TEAL),
    ("1", "Simple codec", "30 lines each way", BLUE),
    ("2", "Nodes", "read each pixel once", BLUE),
    ("3", "64-bit words", "6 pixels in 1 number", MAGENTA),
    ("4", "SIMD", "16 columns at once", MAGENTA),
    ("5", "Threads", "many cores at once", GOLD),
)


def journey() -> str:
    """A staircase of the lessons."""
    height = 640
    body = [title("The journey",
                  "Each step keeps the output and changes one idea.")]
    step_w = 108
    for i, (number, name, idea, color) in enumerate(JOURNEY):
        x = 32 + i * step_w
        y = 400 - i * 52
        body.append(rect(x, y, step_w - 6, 560 - y, PAPER, color,
                         1.6, 10))
        body.append(rect(x, y, step_w - 6, 8, color, radius=4))
        body.append(text(x + (step_w - 6) / 2, y + 44, number, 30, 800,
                         color, "middle"))
        words = name.split(" ")
        for k, word in enumerate(words):
            body.append(text(x + (step_w - 6) / 2, y + 72 + k * 20, word,
                             16, 750, INK, "middle"))
        lines = []
        current = ""
        for word in idea.split(" "):
            candidate = (current + " " + word).strip()
            if len(candidate) > 11 and current:
                lines.append(current)
                current = word
            else:
                current = candidate
        lines.append(current)
        for k, value in enumerate(lines):
            body.append(text(x + (step_w - 6) / 2,
                             y + 80 + len(words) * 20 + k * 19, value,
                             14, 500, MUTED, "middle"))
    body.append(note(32, 596, (
        "Steps 2 to 5 write exactly the same bytes, so each one is a",
        "pure speed-up you can test.",
    ), 17))
    return document(height, "The journey",
                    "Six lessons as a staircase: 0 read SIXEL by hand, "
                    "1 a simple codec, 2 nodes, 3 64-bit words, 4 SIMD, "
                    "5 threads.", body)


def speed() -> str:
    """Large photographs per second, encode and decode, per step."""
    height = 580
    body = [title("How fast does it get?",
                  "Large photographs (2400 × 1800) per second, more is better")]
    top = 140
    bar_h = 26
    scale = 440 / 520
    body.append(text(160, top - 18, "encode", 16, 700, BLUE))
    body.append(text(260, top - 18, "decode", 16, 700, MAGENTA))
    for i, (enc, dec) in enumerate(PHOTO_2400):
        y = top + i * 72
        body.append(text(32, y + 34, f"Step {i + 1}", 19, 750, INK))
        for k, (ms, color) in enumerate(((enc, BLUE), (dec, MAGENTA))):
            per_second = 1000.0 / ms
            w = max(per_second * scale, 3)
            by = y + k * (bar_h + 4)
            body.append(rect(130, by, w, bar_h, color, radius=4))
            body.append(text(138 + w, by + 19, f"{per_second:.0f}", 16,
                             700, color))
    body.append(note(32, top + 5 * 72 + 20, (
        "Measured on an Apple M3 Max. Step 5 uses all cores; the",
        "others use one. The README table lists every input.",
    ), 15))
    return document(height, "How fast does it get?",
                    "Bar chart of large photographs encoded and decoded "
                    "per second at each step: encode "
                    + ", ".join(f"{1000 / e:.0f}" for e, _ in PHOTO_2400)
                    + "; decode "
                    + ", ".join(f"{1000 / d:.0f}" for _, d in PHOTO_2400)
                    + ".", body)


def pnm_layout() -> str:
    """A P5 file's pixel bytes are the indexed image in memory."""
    height = 640
    size = 30
    body = [title("A PGM file is the picture in memory",
                  "After a short header, the bytes are the pixels, row by row.")]
    gray = tuple("".join("1" if t == "R" else "0" for t in row)
                 for row in HEART)
    x0, y0 = 40, 130
    body.append(text(x0, y0 - 14, "heart.pgm as text (P2)", 15, 750, MUTED))
    lines = ("P2", "7 6", "1") + tuple(" ".join(row) for row in gray)
    body.append(rect(x0 - 10, y0, 230, 30 + 26 * len(lines), PAPER, GRID,
                     1.2, 10))
    for i, value in enumerate(lines):
        body.append(mono(x0 + 6, y0 + 32 + i * 26, value, 18,
                         700 if i < 3 else 600, MUTED if i < 3 else INK))
    body.append(text(x0 + 100, y0 + 58, "← kind, size,", 14, 650, MUTED))
    body.append(text(x0 + 100, y0 + 78, "   maxval", 14, 650, MUTED))
    gx = 420
    body.append(text(gx, y0 - 14, "img->pixels", 15, 750, MUTED))
    for r, row in enumerate(gray):
        for c, v in enumerate(row):
            on = v == "1"
            body.append(cell(gx + c * size, y0 + r * size, size - 3,
                             "#E5E7EB" if on else INK, v,
                             INK if on else PAPER, GRID, 13))
    body.append(text(gx, y0 + 6 * size + 22, "register 0 = black,", 14, 600,
                     MUTED))
    body.append(text(gx, y0 + 6 * size + 42, "register 1 = white", 14, 600,
                     MUTED))
    yy = 470
    body.append(text(32, yy - 14, "heart.pgm as binary (P5): header, then "
                     "42 pixel bytes", 15, 750, MUTED))
    header = "P5 7 6 1↵"
    body.append(rect(32, yy, 116, 30, GOLD_LIGHT, GOLD, 1.2, 4))
    body.append(mono(90, yy + 21, header, 15, 700, GOLD, "middle"))
    small = 12.4
    for i, v in enumerate("".join(gray)):
        on = v == "1"
        body.append(rect(156 + i * small, yy, small - 1.5, 30,
                         "#E5E7EB" if on else INK, GRID, 0.6, 2))
    body.append(note(32, yy + 80, (
        "Every pixel is one byte: its gray level, which is also its color",
        "register. The codec reads and writes this layout unchanged.",
    ), 17, INK, 600))
    return document(height, "A PGM file is the picture in memory",
                    "The heart written as a text P2 file and as a binary "
                    "P5 file. After the header, P5 holds forty-two pixel "
                    "bytes, exactly the indexed image the codec uses.",
                    body)


# ------------------------------------------------------------------ #
# Lesson 0.                                                           #


def six_pin_stamp() -> str:
    """One column of six pins with their weights."""
    height = 560
    body = [title("A stamp with six pins",
                  "Each character paints one column, six pixels tall.")]
    x0, y0, size = 60, 130, 52
    weights = (1, 2, 4, 8, 16, 32)
    pins = (0, 1, 1, 0, 1, 0)
    body.append(text(x0 + size / 2, y0 - 14, "pins", 16, 700, MUTED,
                     "middle"))
    for r in range(6):
        y = y0 + r * (size + 6)
        on = pins[r] == 1
        body.append(cell(x0, y, size, TOKEN_FILL["R"] if on else PAPER,
                         "1" if on else "0", PAPER if on else MUTED))
        body.append(text(x0 - 14, y + size / 2 + 6, f"row {r}", 15, 600,
                         MUTED, "end"))
        body.append(text(x0 + size + 30, y + size / 2 + 7,
                         f"worth {weights[r]}", 19, 650,
                         INK if on else MUTED))
    body.append(path(f"M {x0 + 180} {y0 + 10} h 20 v {6 * (size + 6) - 26}"
                     f" h -20", MUTED, 2))
    value = sum(w for w, p in zip(weights, pins) if p)
    body.append(box(260, 150, 428, 290, BLUE))
    body.append(text(285, 190, "Add up the lit pins:", 20, 700, INK))
    body.append(mono(285, 230, "2 + 4 + 16 = " + str(value), 24, 750, BLUE))
    body.append(text(285, 280, "Add 63 to make it a letter:", 20, 700,
                     INK))
    body.append(mono(285, 320, f"{value} + 63 = {value + 63}", 24, 750,
                     BLUE))
    body.append(mono(285, 360, f"character {value + 63} is " + "'"
                     + sixel_char(value) + "'", 24, 750, BLUE))
    body.append(text(285, 410, "Row 0, the top pin, is worth 1.", 17, 600,
                     MUTED))
    body.append(note(32, 505, (
        "Six pins give 64 patterns (0 to 63), so every column fits in",
        "one printable character from '?' (0) to '~' (63).",
    ), 17))
    return document(height, "A stamp with six pins",
                    "A column of six pixels. Rows 1, 2 and 4 are painted, "
                    "worth 2, 4 and 16. Their sum 22 plus 63 is 85, the "
                    "character U.", body)


def heart_columns() -> str:
    """The heart, its column values and its characters."""
    height = 640
    size = 50
    x0, y0 = 150, 120
    values = column_values(HEART, "R")
    body = [title("Reading a heart column by column",
                  "Seven columns become seven characters.")]
    body.append(grid(x0, y0, HEART, size))
    for r in range(6):
        body.append(text(x0 - 16, y0 + r * size + 31, f"{1 << r}", 17, 700,
                         MUTED, "end"))
    body.append(text(x0 - 16, y0 - 14, "worth", 15, 700, MUTED, "end"))
    body.append(text(x0 - 60, y0 + 6 * size + 40, "sum", 17, 700, MUTED,
                     "end"))
    body.append(text(x0 - 60, y0 + 6 * size + 80, "+ 63", 17, 700, MUTED,
                     "end"))
    body.append(text(x0 - 60, y0 + 6 * size + 120, "char", 17, 700, MUTED,
                     "end"))
    for c, value in enumerate(values):
        cx = x0 + c * size + (size - 2) / 2
        body.append(arrow(cx, y0 + 6 * size + 2, cx, y0 + 6 * size + 16,
                          MUTED, 1.6))
        body.append(mono(cx, y0 + 6 * size + 40, str(value), 20, 700, INK,
                         "middle"))
        body.append(mono(cx, y0 + 6 * size + 80, str(value + 63), 18, 600,
                         MUTED, "middle"))
        body.append(cell(cx - 20, y0 + 6 * size + 96, 40, BLUE_LIGHT,
                         sixel_char(value), BLUE, BLUE, 22))
    body.append(mono(32, 612, f"#1 selects red, then: {HEART_BODY[2:]}", 20,
                     700, BLUE))
    return document(height, "Reading a heart column by column",
                    "A seven by six heart. Column sums are "
                    + ", ".join(map(str, values))
                    + "; adding 63 gives the characters " + HEART_BODY[2:]
                    + ".", body)


def character_table() -> str:
    """All 64 data characters."""
    height = 560
    size = 76
    x0, y0 = 50, 120
    body = [title("All 64 sixel characters",
                  "Pattern value on top, character below. Row 0 is bit 1.")]
    for v in range(64):
        r, c = divmod(v, 8)
        x = x0 + c * (size + 2)
        y = y0 + r * (50)
        body.append(rect(x, y, size, 46, PAPER, GRID, 1.0, 6))
        body.append(text(x + 8, y + 18, str(v), 13, 600, MUTED))
        body.append(mono(x + size - 10, y + 36, sixel_char(v), 24, 750,
                         BLUE, "end"))
        # little six-pin preview
        for bit in range(6):
            on = v >> bit & 1
            body.append(rect(x + 8 + bit * 5, y + 26 + 0, 4, 12,
                             TOKEN_FILL["R"] if on else GRID, radius=1))
    body.append(text(32, 540, "'?' is an empty column. '~' paints all six "
                     "rows.", 17, 600, MUTED))
    return document(height, "All 64 sixel characters",
                    "A table of the values 0 to 63 and their characters, "
                    "from question mark to tilde.", body)


def tree_cursor() -> str:
    """How the cursor moves through two bands with $ and -."""
    height = 700
    size = 34
    x0, y0 = 110, 130
    body = [title("Two colors, two bands",
                  "$ goes back to the left; - moves down six rows.")]
    body.append(grid(x0, y0, TREE, size))
    for band in range(2):
        y = y0 + band * 6 * size
        body.append(rect(x0 - 14, y, 6, 6 * size - 2,
                         (BLUE, MAGENTA)[band], radius=3))
        body.append(text(x0 - 22, y + 3 * size + 6, f"band {band + 1}", 15,
                         700, (BLUE, MAGENTA)[band], "end"))
    right = x0 + 7 * size + 6

    def sweep(y: float, number: int, color: str) -> str:
        return (arrow(x0 - 4, y, right, y, color, 3.2)
                + rect(right + 6, y - 12, 24, 24, color, radius=12)
                + text(right + 18, y + 6, str(number), 15, 800, PAPER,
                       "middle"))

    def back(y1: float, y2: float, color: str, label: str) -> str:
        mid = right + 46
        return (path(f"M {right + 30} {y1} H {mid} V {y2} H {right + 34}",
                     color, 2.4, None, "6 5")
                + text(mid + 8, (y1 + y2) / 2 + 6, label, 20, 800, color))

    y_1 = y0 + 2.5 * size
    y_2 = y0 + 6.5 * size
    y_3 = y0 + 9.5 * size
    body.append(sweep(y_1, 1, BLUE))
    body.append(back(y_1, y_2, GOLD, "-"))
    body.append(sweep(y_2, 2, BLUE))
    body.append(back(y_2 + 8, y_3, MAGENTA, "$"))
    body.append(sweep(y_3, 3, MAGENTA))

    steps = (
        ("#1", "select green", INK),
        (TREE_BANDS[0][2:], "1  paint band 1", BLUE),
        ("-", "next band (down 6 rows)", GOLD),
        ("#1!7@", "2  green, 7 × the top pin", BLUE),
        ("$", "back to the left edge", MAGENTA),
        ("#2??O]O", "3  brown trunk", MAGENTA),
    )
    tx = 452
    body.append(box(tx - 18, 120, 254, 420, BLUE))
    body.append(text(tx, 156, "Read it left to right:", 19, 750, INK))
    for i, (code, meaning, color) in enumerate(steps):
        y = 200 + i * 56
        body.append(mono(tx, y, code, 22, 750, color))
        body.append(text(tx, y + 24, meaning, 16, 550, MUTED))
    body.append(note(32, 600, (
        "'!7@' means \"seven '@' in a row\": '@' is 1, the top pin only.",
        "'??' skips two columns without painting them.",
    ), 17))
    body.append(mono(32, 670, TREE_BODY, 20, 700, BLUE))
    return document(height, "Two colors, two bands",
                    "The tree split into two six-row bands. The cursor "
                    "paints band 1 in green, moves down with -, paints "
                    "green, returns with $, and paints the brown trunk. "
                    "Body: " + TREE_BODY, body)


def stream_anatomy() -> str:
    """The pieces of a complete SIXEL stream."""
    height = 580
    pieces = (
        ("ESC P q", "start", "\"Here comes a SIXEL picture.\"", BLUE),
        ('"1;1;7;12', "size", "square pixels, 7 wide, 12 tall", TEAL),
        ("#1;2;0;60;0", "color", "register 1 = R 0%, G 60%, B 0%", MAGENTA),
        ("#2;2;50;30;10", "color", "register 2 = brown", MAGENTA),
        (TREE_BODY, "body", "the pixels, band by band", GOLD),
        ("ESC \\", "end", "\"The picture is finished.\"", BLUE),
    )
    body = [title("A complete stream, piece by piece",
                  "Everything the terminal needs, in order.")]
    for i, (code, label, meaning, color) in enumerate(pieces):
        y = 120 + i * 62
        body.append(rect(32, y, 656, 52, PAPER, color, 1.4, 10))
        body.append(rect(32, y, 8, 52, color, radius=4))
        body.append(text(58, y + 32, label, 16, 800, color))
        body.append(mono(130, y + 23, code, 18, 750, INK))
        body.append(text(130, y + 44, meaning, 15, 550, MUTED))
    body.append(note(32, 510, (
        "ESC is the escape character, byte 27 (\\033 in printf).",
        "#n;2;r;g;b defines a color in percent; #n alone selects it.",
    ), 17))
    return document(height, "A complete stream, piece by piece",
                    "ESC P q starts the picture, \"1;1;7;12 gives its "
                    "size, #1;2;0;60;0 and #2;2;50;30;10 define colors, "
                    "the body paints, and ESC backslash ends it.", body)


# ------------------------------------------------------------------ #
# Step 1.                                                             #


def step1_passes() -> str:
    """The step 1 encoder reads the band once per color."""
    height = 640
    size = 26
    body = [title("Step 1 reads the band once per color",
                  "Three colors means three passes over the same 48 pixels.")]
    tokens = sorted(SCENE_REGISTERS, key=SCENE_REGISTERS.get)
    names = {"S": "sky", "Y": "sun", "G": "grass"}
    for i, token in enumerate(tokens):
        y = 120 + i * 160
        body.append(text(32, y + 20, f"Pass {i + 1}: #{i} {names[token]}",
                         19, 750, INK))
        body.append(grid(32, y + 34, SCENE, size // 1.6, token, True))
        values = column_values(SCENE, token)
        gx = 200
        for c, value in enumerate(values):
            body.append(cell(gx + c * 38, y + 34, 34,
                             BLUE_LIGHT if value else PAPER,
                             sixel_char(value), BLUE if value else MUTED,
                             BLUE if value else GRID, 18))
        body.append(mono(gx, y + 105, SCENE_LINES[i], 19, 750, BLUE))
    body.append(note(32, 600, (
        "A photograph with 216 colors needs 216 passes per band.",
    ), 17, INK, 650))
    return document(height, "Step 1 reads the band once per color",
                    "An eight by six band of sky, sun and grass. The "
                    "encoder scans all pixels three times and writes "
                    + " then ".join(SCENE_LINES) + ".", body)


def step1_repeat() -> str:
    """When the repeat introducer pays off."""
    height = 430
    body = [title("Saying \"eight times\" instead of eight letters",
                  "!n followed by a character repeats it n times.")]
    body.append(mono(32, 140, "~~~~~~~~", 30, 750, INK))
    body.append(arrow(220, 130, 300, 130, BLUE))
    body.append(mono(320, 140, "!8~", 30, 750, BLUE))
    body.append(text(440, 140, "8 bytes → 3 bytes", 19, 650, MUTED))
    rows = (
        (1, "~", "~"),
        (2, "~~", "~~"),
        (3, "~~~", "~~~"),
        (4, "~~~~", "!4~"),
        (5, "~~~~~", "!5~"),
    )
    body.append(text(32, 200, "run", 16, 750, MUTED))
    body.append(text(110, 200, "letters", 16, 750, MUTED))
    body.append(text(300, 200, "with !", 16, 750, MUTED))
    body.append(text(470, 200, "we write", 16, 750, MUTED))
    for i, (n, plain, rep) in enumerate(rows):
        y = 230 + i * 34
        use_rep = n > 3
        body.append(mono(32, y, str(n), 20, 700, INK))
        body.append(mono(110, y, plain, 20, 700, MUTED if use_rep else INK))
        body.append(mono(300, y, f"!{n}~", 20, 700,
                         BLUE if use_rep else MUTED))
        body.append(mono(470, y, rep, 20, 750, BLUE if use_rep else INK))
    body.append(text(32, 410, "A run of 4 or more is shorter with !, so "
                     "flush() uses it only then.", 17, 600, INK))
    return document(height, "Saying eight times instead of eight letters",
                    "Eight tildes become !8~. Runs of one to three stay as "
                    "letters; four or more use the repeat form.", body)


def step1_decoder_state() -> str:
    """The decoder's four variables changing as it reads."""
    height = 660
    body = [title("The decoder is a robot with four notes",
                  "Each byte changes a note or paints pixels.")]
    headers = ("read", "x", "y", "color", "repeat", "does")
    xs = (32, 190, 250, 310, 400, 480)
    for x, h in zip(xs, headers):
        body.append(text(x, 130, h, 17, 800, MUTED))
    rows = (
        ("#1", "0", "0", "1", "1", "select green"),
        (TREE_BANDS[0][2], "1", "0", "1", "1", "paint 1 column"),
        (TREE_BANDS[0][3:], "7", "0", "1", "1", "paint 6 more"),
        ("-", "0", "6", "1", "1", "next band"),
        ("!7", "0", "6", "1", "7", "remember 7"),
        ("@", "7", "6", "1", "1", "paint 7 columns"),
        ("$", "0", "6", "1", "1", "back to left"),
        ("#2", "0", "6", "2", "1", "select brown"),
        ("??", "2", "6", "2", "1", "skip 2 columns"),
        ("O]O", "5", "6", "2", "1", "paint the trunk"),
    )
    for i, values in enumerate(rows):
        y = 170 + i * 44
        body.append(rect(24, y - 28, 672, 40, PAPER if i % 2 else BLUE_LIGHT,
                         radius=6))
        for k, (x, v) in enumerate(zip(xs, values)):
            if k == 0:
                body.append(mono(x, y, v, 20, 750, BLUE))
            elif k == 5:
                body.append(text(x, y, v, 16, 550, INK))
            else:
                body.append(mono(x, y, v, 20, 700, INK))
    body.append(note(32, 630, (
        "x and y are the cursor; the values are after the byte is read.",
    ), 16))
    return document(height, "The decoder is a robot with four notes",
                    "A table tracing x, y, color and repeat while decoding "
                    + TREE_BODY + ".", body)


# ------------------------------------------------------------------ #
# Step 2.                                                             #


def step2_classify() -> str:
    """One pass drops every pixel's bit into its color's row."""
    height = 610
    size = 34
    body = [title("Sorting pixels into drawers",
                  "Each pixel adds its row's weight to its color's drawer.")]
    size = 28
    body.append(grid(32, 140, SCENE, size))
    for r in range(6):
        body.append(text(32 + 8 * size + 6, 140 + r * size + 19,
                         f"+{1 << r}", 14, 700, MUTED))
    tokens = sorted(SCENE_REGISTERS, key=SCENE_REGISTERS.get)
    names = {"S": "sky", "Y": "sun", "G": "grass"}
    gx = 384
    body.append(text(gx, 116, "map: one drawer row per color", 16, 750,
                     MUTED))
    for i, token in enumerate(tokens):
        y = 156 + i * 64
        body.append(rect(gx, y - 22, 16, 16, TOKEN_FILL[token], GRID, 1, 3))
        body.append(text(gx + 22, y - 9, f"#{i} {names[token]}", 14, 700,
                         MUTED))
        values = column_values(SCENE, token)
        for c, value in enumerate(values):
            body.append(cell(gx + c * 38, y, 34, PAPER if not value
                             else BLUE_LIGHT, str(value),
                             BLUE if value else MUTED,
                             BLUE if value else GRID, 15))
    body.append(box(32, 360, 656, 150, BLUE))
    body.append(text(52, 395, "One pixel, one store:", 19, 750, INK))
    body.append(mono(52, 430, "map[color][x] |= 1 << row;", 21, 750, BLUE))
    body.append(note(52, 468, (
        "The pixel at row 4, column 0 is grass, so the grass drawer",
        "at column 0 gets +16. The image is read only once.",
    ), 16))
    body.append(note(32, 550, (
        "After the band, each drawer holds the finished column values;",
        "a zero means the color is absent from that column.",
    ), 17))
    return document(height, "Sorting pixels into drawers",
                    "An eight by six band is read once. Each pixel ORs its "
                    "row weight into map[color][x], giving one row of "
                    "column values per color.", body)


def node_bar(x0: float, y: float, scale: float, node: Node,
             label: str | None = None) -> str:
    """Draw one node as a bar."""
    fill = TOKEN_FILL[node.token if node.token != "." else "W"]
    stroke = INK if node.token in ".W" else fill
    body = rect(x0 + node.sx * scale, y, (node.mx - node.sx) * scale - 2, 26,
                fill, stroke, 1.4, 6)
    text_fill = TOKEN_INK[node.token]
    body += text(x0 + node.sx * scale + 6, y + 19,
                 label or f"#{node.color}", 14, 750, text_fill)
    return body


def step2_nodes() -> str:
    """Cutting a color row into nodes with the gap rule."""
    height = 580
    size = 26
    x0 = 40
    body = [title("Cutting a row into nodes",
                  "A node is a stretch of one color's columns.")]
    body.append(grid(x0, 110, SHAPES, size))
    yellow = column_values(SHAPES, "Y")
    yy = 330
    body.append(text(x0, yy - 52, "the yellow drawer "
                     f"(#{SHAPES_REGISTERS['Y']}):", 17, 750, INK))
    for c, value in enumerate(yellow):
        body.append(cell(x0 + c * size, yy, size - 2,
                         TOKEN_FILL["Y"] if value else PAPER,
                         str(value) if value else "", INK, GRID, 12))
    nodes = [n for n in SHAPES_NODES if n.token == "Y"]
    for n in nodes:
        body.append(path(f"M {x0 + n.sx * size} {yy + 34} v 10 H "
                         f"{x0 + n.mx * size - 2} v -10", GOLD, 3))
        body.append(text(x0 + (n.sx + n.mx) * size / 2, yy + 66,
                         f"node [{n.sx}, {n.mx})", 15, 750, GOLD, "middle"))
    gap_from = nodes[0].mx
    gap_to = nodes[1].sx
    body.append(path(f"M {x0 + gap_from * size} {yy - 4} v -8 H "
                     f"{x0 + gap_to * size - 2} v 8", MAGENTA, 2))
    body.append(text(x0 + (gap_from + gap_to) * size / 2, yy - 18,
                     f"gap of {gap_to - gap_from} empty columns", 15, 750,
                     MAGENTA, "middle"))
    body.append(box(32, 430, 656, 120, GOLD))
    body.append(note(52, 465, (
        f"Gap of {NODE_GAP} or fewer: stay in one node and write '!n?'.",
        f"Gap of {NODE_GAP + 1} or more: end the node; the empty stretch",
        "can hold nodes of other colors instead.",
    ), 17, INK, 600))
    return document(height, "Cutting a row into nodes",
                    "The yellow color appears in columns 7 to 8 and 20 to "
                    "21. The gap of eleven empty columns is longer than "
                    "nine, so it becomes two nodes.", body)


def step2_packing() -> str:
    """Packing nodes onto graphics lines like books on shelves."""
    height = 720
    scale = 25
    x0 = 40
    body = [title("Packing nodes onto lines",
                  "Like putting books on shelves: fill left to right.")]
    body.append(text(x0, 115, "1. All nodes, sorted by start column:", 17,
                     750, INK))
    for i, node in enumerate(SHAPES_NODES):
        body.append(node_bar(x0, 130 + i * 32, scale, node))
    for c in range(0, 25, 4):
        body.append(text(x0 + c * scale, 130 + len(SHAPES_NODES) * 32 + 14,
                         str(c), 12, 600, MUTED, "middle"))
    top = 130 + len(SHAPES_NODES) * 32 + 50
    body.append(text(x0, top, "2. Take every node that starts after the "
                     "last one ends:", 17, 750, INK))
    for i, sweep in enumerate(SHAPES_LINES):
        y = top + 22 + i * 46
        body.append(rect(x0 - 6, y - 6, 24 * scale + 10, 38, PAPER, GRID,
                         1, 8))
        body.append(text(x0 + 24 * scale + 14, y + 19, f"line {i + 1}", 15,
                         750, MUTED))
        for node in sweep:
            body.append(node_bar(x0, y, scale, node))
    y = top + 22 + len(SHAPES_LINES) * 46 + 20
    body.append(text(x0, y, "3. Write the lines, with $ between them:", 17,
                     750, INK))
    chunks = SHAPES_BODY.split("$")
    for i, chunk in enumerate(chunks):
        body.append(mono(x0, y + 34 + i * 30, ("$" if i else "") + chunk,
                         18, 750, BLUE))
    return document(height, "Packing nodes onto lines",
                    "Six nodes sorted by start column are packed greedily "
                    f"onto {len(SHAPES_LINES)} lines: "
                    + "; ".join(" ".join(f"#{n.color}" for n in sweep)
                                for sweep in SHAPES_LINES)
                    + ". Body: " + SHAPES_BODY, body)


def step2_repeat_fill() -> str:
    """Filling a repeat a row at a time."""
    height = 470
    size = 30
    body = [title("Painting \"!8~\" quickly",
                  "Same pixels, far fewer steps.")]
    for k, (label, color) in enumerate((("pixel by pixel: 48 stores", MUTED),
                                        ("row by row: 6 memset()", BLUE))):
        x0 = 32 + k * 340
        body.append(text(x0, 125, label, 18, 750, color))
        for r in range(6):
            for c in range(8):
                body.append(rect(x0 + c * size, 145 + r * size, size - 3,
                                 size - 3, TOKEN_FILL["G"], radius=3))
                if k == 0:
                    body.append(text(x0 + c * size + 13, 145 + r * size + 19,
                                     str(r * 8 + c + 1), 10, 600, PAPER,
                                     "middle"))
            if k == 1:
                body.append(arrow(x0 - 4, 145 + r * size + 13,
                                  x0 + 8 * size + 4, 145 + r * size + 13,
                                  GOLD, 3))
    body.append(note(32, 360, (
        "Image rows lie one after another in memory, so one row of",
        "eight pixels is eight neighbouring bytes: one memset() call.",
        "A single character (no !) still uses plain stores.",
    ), 17))
    return document(height, "Painting !8~ quickly",
                    "A repeated full column fills an eight by six block. "
                    "Pixel by pixel needs 48 stores; row by row needs six "
                    "memset calls.", body)


# ------------------------------------------------------------------ #
# Step 3.                                                             #


def step3_memory() -> str:
    """Rows lie end to end, so a column is scattered."""
    height = 560
    size = 34
    body = [title("Where the pixels live in memory",
                  "Memory is one long street of numbered boxes.")]
    rows = ("abcdefgh", "ijklmnop", "qrstuvwx", "yzABCDEF", "GHIJKLMN",
            "OPQRSTUV")
    x0, y0 = 32, 120
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            hot = c == 2
            body.append(cell(x0 + c * size, y0 + r * size, size - 3,
                             GOLD_LIGHT if hot else PAPER, ch,
                             GOLD if hot else MUTED,
                             GOLD if hot else GRID, 15, False))
    body.append(text(x0, y0 + 6 * size + 24, "the band on screen", 15, 700,
                     MUTED))
    body.append(text(340, 140, "A sixel character needs column 2:", 17,
                     750, INK))
    body.append(mono(340, 170, "c  k  s  A  I  Q", 20, 750, GOLD))
    body.append(text(340, 200, "They are 8 boxes apart in memory.", 17,
                     600, MUTED))
    small = 13
    yy = 400
    body.append(text(32, yy - 16, "the same band in memory, row after row:",
                     16, 700, MUTED))
    flat = "".join(rows)
    for i, ch in enumerate(flat):
        hot = i % 8 == 2
        x = 32 + i * small
        body.append(rect(x, yy, small - 1, 30,
                         GOLD if hot else (PAPER if (i // 8) % 2 == 0
                                           else BLUE_LIGHT), GRID, 0.6, 2))
    for r in range(6):
        body.append(text(32 + (r * 8 + 4) * small, yy + 50, f"row {r}", 12,
                         650, MUTED, "middle"))
    body.append(note(32, 490, (
        "In a real image the gap is the image width: 2400 boxes. The six",
        "pixels of one column land in six different parts of memory.",
    ), 17))
    return document(height, "Where the pixels live in memory",
                    "An eight by six band with letters. Column 2 holds c, "
                    "k, s, A, I, Q, which are eight boxes apart when the "
                    "rows are laid end to end in memory.", body)


COLUMN = (5, 2, 2, 5, 7, 2)


def word_cells(x: float, y: float, values: Sequence[object], size: float,
               color: str = BLUE, fills: Sequence[str] | None = None,
               labels_above: bool = True) -> str:
    """Draw one 64-bit word as eight byte boxes, byte 0 on the right."""
    body = []
    for slot in range(8):
        value = values[slot] if slot < len(values) else 0
        index = 7 - slot
        fill = fills[slot] if fills else (PAPER if slot >= 6 else BLUE_LIGHT)
        body.append(cell(x + index * size, y, size - 4, fill, str(value),
                         INK if slot < 6 else MUTED, color, 17))
        if labels_above:
            body.append(text(x + index * size + (size - 4) / 2, y - 8,
                             f"byte {slot}", 11, 650, MUTED, "middle"))
    return "".join(body)


def step3_column_word() -> str:
    """Six pixels of one column packed into one 64-bit number."""
    height = 560
    size = 48
    body = [title("Six pixels in one number",
                  "Turn each column on its side and put it in a box of 8 bytes.")]
    x0, y0 = 60, 120
    for r, v in enumerate(COLUMN):
        body.append(cell(x0, y0 + r * size, size - 4, BLUE_LIGHT, str(v),
                         INK, BLUE, 20))
        body.append(text(x0 - 10, y0 + r * size + 28, f"row {r}", 14, 650,
                         MUTED, "end"))
    body.append(text(x0 + 22, y0 + 6 * size + 18, "column x", 14, 700,
                     MUTED, "middle"))
    body.append(path(f"M {x0 + 60} {y0 + 140} C {x0 + 140} {y0 + 140} "
                     f"{x0 + 140} {y0 + 80} {x0 + 200} {y0 + 80}", BLUE, 3,
                     "arrow-blue"))
    body.append(text(x0 + 125, y0 + 170, "turn", 15, 700, BLUE, "middle"))
    body.append(word_cells(280, 180, COLUMN, 50))
    body.append(text(280, 260, "one uint64_t: 8 bytes, the top two unused",
                     15, 650, MUTED))
    body.append(box(32, 440, 656, 95, MAGENTA))
    body.append(note(52, 475, (
        "A band of width w becomes w numbers in a row. A 64-bit CPU",
        "handles a whole number in one step, so it sees 6 pixels at once.",
    ), 17, INK, 600))
    return document(height, "Six pixels in one number",
                    "A column with palette indexes 5, 2, 2, 5, 7, 2 is "
                    "packed into the low six bytes of one 64-bit word, "
                    "row 0 in byte 0.", body)


def step3_swar() -> str:
    """Which rows of the column have color 2?"""
    height = 760
    size = 50
    x0 = 220
    color = 2
    xored = [v ^ color for v in COLUMN] + [color, color]
    flags = [1 if v == 0 else 0 for v in xored]
    bits = sum(1 << r for r in range(6) if flags[r])
    body = [title("Asking all six rows at once",
                  f"Which rows of this column have color {color}?")]
    stages = (
        ("the column", list(COLUMN) + [0, 0], None, BLUE),
        (f"copies of {color}", [color] * 8, None, MUTED),
        ("XOR: equal → 0", xored,
         [GOLD_LIGHT if v == 0 else PAPER for v in xored], GOLD),
        ("zero? (zero_bytes)", flags,
         [TEAL_LIGHT if f else PAPER for f in flags], TEAL),
    )
    for i, (label, values, fills, accent) in enumerate(stages):
        y = 130 + i * 110
        body.append(text(32, y + 30, label, 17, 750, accent))
        body.append(word_cells(x0, y, values, size, accent, fills, i == 0))
    y = 130 + 4 * 110
    body.append(text(32, y + 30, "gather the flags", 17, 750, MAGENTA))
    body.append(mono(x0, y + 32, f"rows 1, 2, 5 → {bits_label(bits)} = {bits}",
                     22, 750, MAGENTA))
    body.append(mono(x0, y + 72, f"{bits} + 63 = '{sixel_char(bits)}'", 22,
                     750, MAGENTA))
    body.append(note(32, 700, (
        "Every box is handled by the same few operations on the whole",
        "number. This is called SWAR: SIMD within a register.",
    ), 17))
    return document(height, "Asking all six rows at once",
                    "The column 5, 2, 2, 5, 7, 2 is XORed with copies of "
                    "2. Zero bytes mark rows 1, 2 and 5, which gather to "
                    "the six-bit value 38, character e.", body)


def step3_first_colors() -> str:
    """Finding each color of a column once."""
    height = 520
    size = 52
    body = [title("Visiting each color once",
                  "A row is new if no row above it has the same color.")]
    x0, y0 = 70, 120
    seen: list[int] = []
    for r, v in enumerate(COLUMN):
        first = v not in seen
        seen.append(v)
        body.append(cell(x0, y0 + r * size, size - 4,
                         TEAL_LIGHT if first else PAPER, str(v), INK,
                         TEAL if first else GRID, 20))
        body.append(text(x0 - 12, y0 + r * size + 30, f"row {r}", 14, 650,
                         MUTED, "end"))
        verdict = "new color" if first else f"same as row {COLUMN.index(v)}"
        body.append(text(x0 + 70, y0 + r * size + 32, verdict, 17, 700,
                         TEAL if first else MUTED))
    colors = []
    for v in COLUMN:
        if v not in colors:
            colors.append(v)
    body.append(box(320, 130, 368, 250, TEAL))
    body.append(text(340, 165, "So this column needs", 18, 700, INK))
    body.append(text(340, 192, f"{len(colors)} visits, not 6:", 18, 700,
                     INK))
    for i, c in enumerate(colors):
        value = sum(1 << r for r, v in enumerate(COLUMN) if v == c)
        body.append(mono(340, 240 + i * 40,
                         f"color {c}: {bits_label(value)} '{sixel_char(value)}'",
                         20, 750, TEAL))
    body.append(note(32, 460, (
        "The code compares the word with copies of itself shifted down",
        "by 1 to 5 rows, which tests every pair of rows at once.",
    ), 17))
    return document(height, "Visiting each color once",
                    "In the column 5, 2, 2, 5, 7, 2 rows 0, 1 and 4 start "
                    "a new color, so the column is visited for colors 5, "
                    "2 and 7 only.", body)


def step3_occupancy() -> str:
    """A bitmap of occupied columns and the jump to the next one."""
    height = 440
    size = 36
    occupied = (0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0)
    body = [title("Lights for occupied columns",
                  "One bit per column: is this color in it?")]
    x0, y0 = 40, 140
    for c, on in enumerate(occupied):
        body.append(cell(x0 + c * size, y0, size - 4,
                         GOLD if on else PAPER, "1" if on else "0",
                         INK if on else MUTED, GOLD if on else GRID, 16))
        body.append(text(x0 + c * size + 16, y0 - 10, str(c), 11, 600,
                         MUTED, "middle"))
    hops = [c for c, on in enumerate(occupied) if on]
    prev = -1
    for c in hops:
        start = x0 + prev * size + 16 if prev >= 0 else x0 - 10
        end = x0 + c * size + 16
        depth = 16 + (end - start) * 0.18
        body.append(path(f"M {start} {y0 + 40} Q {(start + end) / 2} "
                         f"{y0 + 40 + depth * 2} {end} {y0 + 42}", BLUE,
                         2.6, "arrow-blue"))
        prev = c
    body.append(text(x0, y0 + 130, "count trailing zeros jumps straight to "
                     "the next 1:", 17, 700, INK))
    body.append(mono(x0, y0 + 165, "x = ctz64(bits);  bits &= bits - 1;", 19,
                     750, BLUE))
    body.append(note(x0, y0 + 215, (
        "64 columns fit in one word, so most empty columns are skipped",
        "without being looked at.",
    ), 17))
    return document(height, "Lights for occupied columns",
                    "Sixteen columns, of which 2, 3, 9 and 14 are "
                    "occupied. Count trailing zeros jumps from one set "
                    "bit to the next.", body)


def step3_masked_store() -> str:
    """Painting into a column word with a mask."""
    height = 640
    size = 50
    x0 = 230
    bits = 0b100110
    color = 3
    old = list(COLUMN) + [0, 0]
    mask = ["ff" if bits >> s & 1 else "00" for s in range(6)] + ["00", "00"]
    paint = [color if bits >> s & 1 else 0 for s in range(6)] + [0, 0]
    new = [paint[s] if bits >> s & 1 else old[s] for s in range(8)]
    body = [title("Painting into a column word",
                  f"The decoder reads 'e' ({bits_label(bits)}) "
                  f"with color {color} selected.")]
    stages = (
        ("old column", old, None, MUTED),
        ("mask from bits", mask,
         [GOLD_LIGHT if m == "ff" else PAPER for m in mask], GOLD),
        (f"color {color} in masked rows", paint,
         [GOLD_LIGHT if m == "ff" else PAPER for m in mask], GOLD),
        ("new column", new,
         [TEAL_LIGHT if m == "ff" else BLUE_LIGHT for m in mask], TEAL),
    )
    for i, (label, values, fills, accent) in enumerate(stages):
        y = 130 + i * 100
        body.append(text(32, y + 30, label, 17, 750, accent))
        body.append(word_cells(x0, y, values, size, accent, fills, i == 0))
    body.append(mono(32, 560, "col = (col & ~mask) | (color6 & mask);", 20,
                     750, BLUE))
    body.append(text(32, 600, "One store paints all the rows; a repeat "
                     "just does it for the next words.", 16, 600, MUTED))
    return document(height, "Painting into a column word",
                    "The data character e sets rows 1, 2 and 5. A mask "
                    "with ff in those bytes replaces them with color 3 "
                    "and keeps the other rows.", body)


# ------------------------------------------------------------------ #
# Step 4.                                                             #


def step4_lanes() -> str:
    """One instruction compares sixteen pairs."""
    height = 470
    size = 38
    body = [title("SIMD: one instruction, sixteen answers",
                  "A vector register holds 16 bytes, called lanes.")]
    row = (5, 5, 2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 2, 2, 2, 2)
    x0 = 40
    for k, (label, values, color) in enumerate((
            ("row of pixels", row, BLUE),
            ("v_set1(2)", (2,) * 16, MUTED),
            ("v_eq →", tuple(0xff if v == 2 else 0 for v in row), GOLD))):
        y = 130 + k * 80
        body.append(text(x0, y - 10, label, 15, 750, color))
        for i, v in enumerate(values):
            hot = k == 2 and v
            body.append(cell(x0 + i * size, y, size - 4,
                             GOLD_LIGHT if hot else PAPER,
                             ("ff" if v == 0xff else ("00" if k == 2 else
                                                     str(v))),
                             INK if (k != 2 or hot) else MUTED,
                             color if (k != 2 or hot) else GRID, 13))
    m = sum(1 << i for i, v in enumerate(row) if v == 2)
    body.append(text(x0, 380, "v_movemask squeezes the lanes into one "
                     "16-bit number:", 17, 700, INK))
    body.append(mono(x0, 415, format(m, "016b")[::-1] + "  (lane 0 first)",
                     20, 750, GOLD))
    return document(height, "SIMD: one instruction, sixteen answers",
                    "Sixteen pixels are compared with color 2 in one "
                    "instruction. Equal lanes become ff, and movemask "
                    "packs them into a sixteen-bit number.", body)


BLOCK_ROWS = (
    (5, 5, 2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 2, 2, 2, 2),
    (5, 5, 2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 2, 2, 2, 2),
    (5, 5, 5, 2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 2, 2, 2),
    (5, 5, 5, 2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 2, 2, 2),
    (5, 5, 5, 5, 2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 2, 2),
    (5, 5, 5, 5, 2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 2, 2),
)


def step4_block() -> str:
    """Six compares give one color's characters for 16 columns."""
    height = 590
    size = 38
    x0 = 70
    color = 2
    body = [title("Sixteen columns, one color, six compares",
                  f"Which pixels of this 16 × 6 block are color {color}?")]
    for r, row in enumerate(BLOCK_ROWS):
        y = 120 + r * (size + 4)
        body.append(text(x0 - 10, y + 25, f"×{1 << r}", 14, 700, MUTED,
                         "end"))
        for i, v in enumerate(row):
            hot = v == color
            body.append(cell(x0 + i * size, y, size - 4,
                             GOLD_LIGHT if hot else PAPER, str(v),
                             INK if hot else MUTED, GOLD if hot else GRID,
                             15))
    y = 120 + 6 * (size + 4) + 20
    body.append(text(x0, y, "OR of the weighted matches, per lane:", 16,
                     750, INK))
    values = [sum(1 << r for r in range(6) if BLOCK_ROWS[r][i] == color)
              for i in range(16)]
    for i, v in enumerate(values):
        body.append(cell(x0 + i * size, y + 14, size - 4,
                         BLUE_LIGHT if v else PAPER, str(v),
                         BLUE if v else MUTED, BLUE if v else GRID, 14))
        body.append(mono(x0 + i * size + 17, y + 82, sixel_char(v), 20, 750,
                         BLUE, "middle"))
    body.append(text(x0 - 10, y + 82, "char", 14, 700, MUTED, "end"))
    body.append(note(32, 550, (
        "One 16-byte store puts all sixteen values into the map at once.",
    ), 17, INK, 600))
    return document(height, "Sixteen columns, one color, six compares",
                    "A sixteen by six block of colors 5, 2 and 7. Six "
                    "vector compares with color 2, weighted by row and "
                    "ORed, give the sixteen column values "
                    + ", ".join(map(str, values)) + ".", body)


ROW_FILL = ("#DBEAFE", "#FCE7F3", "#FEF3C7", "#CCFBF1", "#EDE9FE",
            "#FFE4E6", "#F1F5F9", "#F1F5F9")


def lane_strip(x: float, y: float, cells: Sequence[tuple[int, int]],
               size: float) -> str:
    """Draw 16 bytes, each a (column, row) pair."""
    body = []
    for i, (c, r) in enumerate(cells):
        body.append(cell(x + i * size, y, size - 2, ROW_FILL[r], f"{c}",
                         INK if r < 6 else MUTED, GRID, 13))
    return "".join(body)


def step4_transpose() -> str:
    """Three rounds of interleaving turn columns back into rows."""
    height = 790
    size = 36
    x0 = 70
    columns = [[(c, r) for r in range(8)] for c in range(8)]

    def zip_units(a, b, unit, high=False):
        units_a = [a[i:i + unit] for i in range(0, 16, unit)]
        units_b = [b[i:i + unit] for i in range(0, 16, unit)]
        half = len(units_a) // 2
        if high:
            units_a, units_b = units_a[half:], units_b[half:]
        else:
            units_a, units_b = units_a[:half], units_b[:half]
        out = []
        for ua, ub in zip(units_a, units_b):
            out.extend(ua)
            out.extend(ub)
        return out

    padded = [col + [(0, 7)] * 8 for col in columns]
    a = [zip_units(padded[2 * i], padded[2 * i + 1], 1) for i in range(4)]
    b = [zip_units(a[0], a[1], 2), zip_units(a[0], a[1], 2, True),
         zip_units(a[2], a[3], 2), zip_units(a[2], a[3], 2, True)]
    out = [zip_units(b[0], b[2], 4), zip_units(b[0], b[2], 4, True),
           zip_units(b[1], b[3], 4)]
    for vector, rows in zip(out, ((0, 1), (2, 3), (4, 5))):
        assert [r for _, r in vector] == [rows[0]] * 8 + [rows[1]] * 8
        assert [c for c, _ in vector] == list(range(8)) * 2

    body = [title("Turning columns back into rows",
                  "Numbers are column indexes; colors are rows (r0 to r7).")]
    y = 120
    body.append(text(32, y, "8 column words (one per line), row 0 on the "
                     "left:", 16, 750, INK))
    for c in range(8):
        body.append(lane_strip(x0, y + 14 + c * 27, columns[c], 27))
        body.append(text(x0 - 8, y + 14 + c * 27 + 18, f"col {c}", 12, 650,
                         MUTED, "end"))
    for r in range(8):
        body.append(text(x0 + r * 27 + 12, y + 14 + 8 * 27 + 16, f"r{r}",
                         12, 650, MUTED, "middle"))
    body.append(note(x0 + 8 * 27 + 30, y + 60, (
        "Rows 6 and 7 are the",
        "unused top bytes.",
        "",
        "We want each row's 8",
        "bytes side by side.",
    ), 16))
    y = 395
    stages = (
        ("round 1: zip bytes of columns 0 and 1", a[0], "0 1 0 1 …"),
        ("round 2: zip 2-byte pairs", b[0], "0 1 2 3 0 1 2 3 …"),
        ("round 3: zip 4-byte quads", out[0], "rows 0 and 1 in order"),
    )
    for i, (label, vector, hint) in enumerate(stages):
        yy = y + i * 110
        body.append(text(32, yy, label, 16, 750, INK))
        body.append(lane_strip(x0, yy + 14, vector, size))
        body.append(text(x0, yy + 72, hint, 14, 600, MUTED))
    body.append(note(32, 745, (
        "Each round doubles the runs of same-row bytes: 1, 2, 4, then 8.",
    ), 17, INK, 600))
    return document(height, "Turning columns back into rows",
                    "Eight column words of eight bytes are transposed in "
                    "three rounds of interleaving, bytes then pairs then "
                    "quads, until each row is eight contiguous bytes.",
                    body)


# ------------------------------------------------------------------ #
# Step 5.                                                             #


WORKER_COLORS = (BLUE, MAGENTA, GOLD, TEAL)
WORKER_LIGHT = (BLUE_LIGHT, MAGENTA_LIGHT, GOLD_LIGHT, TEAL_LIGHT)


def step5_encoder() -> str:
    """Bands split between threads, outputs glued in order."""
    height = 560
    body = [title("Encoding with four helpers",
                  "Each thread encodes its own bands into its own buffer.")]
    bands = 12
    for b in range(bands):
        w = b * 4 // bands
        y = 120 + b * 28
        body.append(rect(40, y, 220, 24, WORKER_LIGHT[w], WORKER_COLORS[w],
                         1.2, 4))
        body.append(text(50, y + 17, f"band {b + 1}", 13, 700,
                         WORKER_COLORS[w]))
    for w in range(4):
        y = 130 + w * 84
        body.append(arrow(268, y + 30, 330, y + 30, WORKER_COLORS[w], 2.6))
        body.append(rect(340, y, 150, 60, PAPER, WORKER_COLORS[w], 1.6, 10))
        body.append(text(415, y + 26, f"thread {w + 1}", 16, 750,
                         WORKER_COLORS[w], "middle"))
        body.append(text(415, y + 48, "own buffer", 13, 600, MUTED,
                         "middle"))
        body.append(arrow(498, y + 30, 548, 300, WORKER_COLORS[w], 2.2))
    body.append(rect(556, 200, 132, 200, PAPER, INK, 1.6, 10))
    body.append(text(622, 236, "glue in", 16, 750, INK, "middle"))
    body.append(text(622, 258, "order", 16, 750, INK, "middle"))
    for w in range(4):
        body.append(rect(572, 280 + w * 26, 100, 22, WORKER_LIGHT[w],
                         WORKER_COLORS[w], 1, 4))
    body.append(note(32, 490, (
        "This works because every band starts by selecting its own",
        "color, so no band depends on the one before it.",
    ), 17))
    return document(height, "Encoding with four helpers",
                    "Twelve bands are split into four ranges. Each thread "
                    "encodes its range into its own buffer, and the "
                    "buffers are joined in order.", body)


def step5_decoder() -> str:
    """Prescan for band starts, then paint in parallel."""
    height = 640
    body = [title("Decoding with helpers: find the bands first",
                  "Where does each band start, and in which color?")]
    stream = "#1~~~-#2!5N-#1@@@#3ww-#3!4~-#1}}}"
    x0, y0, size = 32, 140, 19.5
    band = 0
    starts = [(0, "1")]
    current = "1"
    i = 0
    while i < len(stream):
        ch = stream[i]
        if ch == "#":
            current = stream[i + 1]
        if ch == "-":
            band += 1
            starts.append((i + 1, current))
        i += 1
    band = 0
    for i, ch in enumerate(stream):
        if ch == "-":
            band += 1
        w = min(band // 2, 3) if False else (0 if band < 2 else 1)
        fill = WORKER_LIGHT[w]
        stroke = GRID
        if ch == "-":
            fill, stroke = GOLD, GOLD
        elif ch == "#":
            fill, stroke = MAGENTA_LIGHT, MAGENTA
        body.append(cell(x0 + i * size, y0, size - 2, fill, ch, INK, stroke,
                         12))
    body.append(text(x0, y0 - 14, "the stream", 15, 750, MUTED))
    body.append(text(32, 225, "1. Prescan: one quick pass that stops only at "
                     "'-', '#' and '\"'.", 16, 750, INK))
    body.append(text(32, 252, "It writes down where each band starts and "
                     "its color:", 16, 550, MUTED))
    for k, (offset, color) in enumerate(starts):
        y = 285 + k * 30
        body.append(mono(52, y, f"band {k + 1}: byte {offset:2d}, color "
                         f"#{color}", 17, 700, INK))
    y = 285 + len(starts) * 30 + 20
    body.append(text(32, y, "2. Paint: each thread decodes a slice of bands "
                     "starting", 16, 750, INK))
    body.append(text(32, y + 24, "from its byte offset and color.", 16, 750,
                     INK))
    body.append(rect(32, y + 40, 300, 34, BLUE_LIGHT, BLUE, 1.2, 6))
    body.append(text(182, y + 62, "thread 1: bands 1–2", 15, 700, BLUE,
                     "middle"))
    body.append(rect(345, y + 40, 343, 34, MAGENTA_LIGHT, MAGENTA, 1.2, 6))
    body.append(text(516, y + 62, "thread 2: bands 3–5", 15, 700, MAGENTA,
                     "middle"))
    body.append(text(32, y + 110, "3. Check: paint outside the expected "
                     "size? Decode again on one thread.", 16, 750, INK))
    return document(height, "Decoding with helpers: find the bands first",
                    "A short stream with five bands. A prescan records "
                    "the byte offset and selected color at each band "
                    "start; threads then decode slices of bands in "
                    "parallel, with a serial fallback.", body)


def step5_amdahl() -> str:
    """The serial part limits the speed-up."""
    height = 470
    body = [title("Why 14 cores do not give 14×",
                  "The part that cannot be shared stays the same length.")]
    serial = 1.0
    parallel = 9.0
    scale = 48
    for i, n in enumerate((1, 2, 4, 14)):
        y = 130 + i * 62
        body.append(text(32, y + 24, f"{n} thread{'s' if n > 1 else ''}", 17,
                         750, INK))
        body.append(rect(150, y, serial * scale, 34, GOLD, radius=4))
        body.append(rect(150 + serial * scale + 2, y,
                         parallel / n * scale, 34, BLUE, radius=4))
        total = serial + parallel / n
        body.append(text(150 + total * scale + 14, y + 24,
                         f"{(serial + parallel) / total:.1f}× faster", 16,
                         700, MUTED))
    body.append(rect(150, 395, 24, 18, GOLD, radius=3))
    body.append(text(182, 410, "serial (prescan, header)", 15, 650, INK))
    body.append(rect(420, 395, 24, 18, BLUE, radius=3))
    body.append(text(452, 410, "shared by threads", 15, 650, INK))
    body.append(text(32, 450, "With 10% serial work, the speed-up can never "
                     "pass 10×.", 16, 650, MUTED))
    return document(height, "Why 14 cores do not give 14 times",
                    "Bars with a fixed serial part and a parallel part "
                    "divided by 1, 2, 4 and 14 threads: speed-ups 1.0, "
                    "1.8, 3.1 and 6.1.", body)


# ------------------------------------------------------------------ #


FACTORIES: tuple[tuple[str, Callable[[], str]], ...] = (
    ("big-picture.svg", big_picture),
    ("journey.svg", journey),
    ("speed.svg", speed),
    ("pnm-layout.svg", pnm_layout),
    ("six-pin-stamp.svg", six_pin_stamp),
    ("heart-columns.svg", heart_columns),
    ("character-table.svg", character_table),
    ("tree-cursor.svg", tree_cursor),
    ("stream-anatomy.svg", stream_anatomy),
    ("step1-passes.svg", step1_passes),
    ("step1-repeat.svg", step1_repeat),
    ("step1-decoder-state.svg", step1_decoder_state),
    ("step2-classify.svg", step2_classify),
    ("step2-nodes.svg", step2_nodes),
    ("step2-packing.svg", step2_packing),
    ("step2-repeat-fill.svg", step2_repeat_fill),
    ("step3-memory.svg", step3_memory),
    ("step3-column-word.svg", step3_column_word),
    ("step3-swar.svg", step3_swar),
    ("step3-first-colors.svg", step3_first_colors),
    ("step3-occupancy.svg", step3_occupancy),
    ("step3-masked-store.svg", step3_masked_store),
    ("step4-lanes.svg", step4_lanes),
    ("step4-block.svg", step4_block),
    ("step4-transpose.svg", step4_transpose),
    ("step5-encoder.svg", step5_encoder),
    ("step5-decoder.svg", step5_decoder),
    ("step5-amdahl.svg", step5_amdahl),
)


def manifest() -> dict[str, object]:
    """Return provenance and the literal fixtures of the figures."""
    return {
        "schema": 1,
        "generator": "tools/plot_sixel_codec_tutorial_figures.py",
        "kind": "deterministic explanatory schematic with literal bodies",
        "measurement": False,
        "measured_assets": {
            "speed.svg": "docs/tutorial/sixel-codec/README.md table, "
                         "2400x1800 photograph",
        },
        "assets": [name for name, _ in FACTORIES],
        "fixtures": {
            "heart": {"rows": list(HEART), "body": HEART_BODY},
            "tree": {"rows": list(TREE), "registers": TREE_REGISTERS,
                     "body": TREE_BODY},
            "scene": {"rows": list(SCENE), "registers": SCENE_REGISTERS,
                      "step1_lines": SCENE_LINES},
            "shapes": {
                "rows": list(SHAPES),
                "registers": SHAPES_REGISTERS,
                "node_gap": NODE_GAP,
                "nodes": [[n.color, n.sx, n.mx] for n in SHAPES_NODES],
                "lines": [[n.color for n in sweep]
                          for sweep in SHAPES_LINES],
                "step2_body": SHAPES_BODY,
            },
        },
        "sources": {
            "tutorial_code": "docs/tutorial/sixel-codec/code",
        },
    }


def generate(output_dir: Path) -> None:
    """Write every figure and the manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, factory in FACTORIES:
        (output_dir / name).write_text(factory(), encoding="utf-8")
    (output_dir / "sixel-codec-tutorial-figures.json").write_text(
        json.dumps(manifest(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def check(output_dir: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(prefix="sixel-codec-figures-") as temp:
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
                "SIXEL codec tutorial figures are stale or missing: "
                + ", ".join(mismatches)
            )


def parse_args() -> argparse.Namespace:
    """Parse output-directory and check-mode options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=source_root / "docs/tutorial/sixel-codec/figures",
    )
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--print-fixtures", action="store_true",
                        help="print the literal bodies and exit")
    return parser.parse_args()


def main() -> None:
    """Generate or verify the figures."""
    arguments = parse_args()
    if arguments.print_fixtures:
        print(json.dumps(manifest()["fixtures"], indent=2))
        return
    if arguments.check:
        check(arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
