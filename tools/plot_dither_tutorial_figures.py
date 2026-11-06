#!/usr/bin/env python3
"""Generate the figures of the dithering tutorial.

The pictures are real output of the tutorial programs, built from
docs/tutorial/dithering/code into a temporary directory. They are cut and
enlarged here without any resampling, so every pixel is the program's. The
diagrams compute their numbers with the same rules as the programs, and read
the kernels from step4.c.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

from plot_encoding_pipeline_figures import (
    BLUE,
    GOLD,
    GOLD_LIGHT,
    GRID,
    INK,
    MAGENTA,
    MUTED,
    PANEL,
    PAPER,
    TEAL,
    TEAL_LIGHT,
    line,
    rect,
    svg_document,
    text,
)

ROOT = Path(__file__).resolve().parent.parent
CODE = ROOT / "docs/tutorial/dithering/code"
SNAKE = ROOT / "images/snake.ppm"
WIDTH = 720
SNAKE_CROP = (270, 30, 330, 225)
SMOOTH_CROP = (420, 30, 160, 60)
SEAM_CROP = (240, 48, 160, 24)
GRAY_ENDS = ("0", "0", "0", "255", "255", "255")

# name: picture, palette, step, option, (x, y, width, height) or None, zoom
PICTURES = {
    "gray-original": ("gray", None, None, None, None, 1),
    "gray16-nearest": ("gray", "gray16", "step1", None, None, 1),
    "gray16-random": ("gray", "gray16", "step2", "random", None, 1),
    "gray16-bayer": ("gray", "gray16", "step2", None, None, 1),
    "gray16-fs": ("gray", "gray16", "step3", None, None, 1),
    "gray4-fs": ("gray", "gray4", "step3", None, None, 1),
    "gray16-random-zoom": ("gray", "gray16", "step2", "random",
                           (240, 0, 160, 32), 4),
    "gray16-bayer-zoom": ("gray", "gray16", "step2", None,
                          (240, 0, 160, 32), 4),
    "gray16-fs-zoom": ("gray", "gray16", "step3", None, (240, 0, 160, 32), 4),
    "gray4-fs-zoom": ("gray", "gray4", "step3", None, (240, 0, 160, 32), 4),
    "color-original": ("color", None, None, None, None, 1),
    "color-cube27-nearest": ("color", "cube27", "step1", None, None, 1),
    "color-cube27-fs": ("color", "cube27", "step3", None, None, 1),
    "color-cube27-atkinson": ("color", "cube27", "step4", "atkinson", None, 1),
    "flat-fs-zoom": ("flat", "gray2", "step3", None, None, 4),
    "flat-serpentine-zoom": ("flat", "gray2", "step4", "serpentine", None, 4),
    "flat-jajuni-zoom": ("flat", "gray2", "step4", "jajuni", None, 4),
    "flat-atkinson-zoom": ("flat", "gray2", "step4", "atkinson", None, 4),
    "snake-original": ("snake", None, None, None, SNAKE_CROP, 1),
    "snake-cube27-nearest": ("snake", "cube27", "step1", None, SNAKE_CROP, 1),
    "snake-cube27-bayer": ("snake", "cube27", "step2", None, SNAKE_CROP, 1),
    "snake-cube27-fs": ("snake", "cube27", "step3", None, SNAKE_CROP, 1),
    "snake-cube216-fs": ("snake", "cube216", "step3", None, SNAKE_CROP, 1),
    "snake-palette16-nearest": ("snake", "snake16", "step1", None,
                                SNAKE_CROP, 1),
    "snake-palette16-fs": ("snake", "snake16", "step3", None, SNAKE_CROP, 1),
    "snake-palette256-fs": ("snake", "snake256", "step3", None, SNAKE_CROP, 1),
    "gray2-stored": ("gray", "gray2", "step3", None, None, 1),
    "gray2-linear": ("gray", "gray2", "step7", None, None, 1),
    "snake-cube27-linear": ("snake", "cube27", "step7", None, SNAKE_CROP, 1),
    "snake-palette16-soft": ("snake", "snake16", "step8", "clamp=soft",
                             SNAKE_CROP, 1),
    "snake-palette16-hard": ("snake", "snake16", "step8", "clamp=hard",
                             SNAKE_CROP, 1),
    "snake-palette16-none": ("snake", "snake16", "step8", "clamp=none",
                             SNAKE_CROP, 1),
    "snake-palette256-zone0": ("snake", "snake256", "step8", "deadzone=0",
                               SMOOTH_CROP, 4),
    "snake-palette256-zone10": ("snake", "snake256", "step8", "deadzone=10",
                                SMOOTH_CROP, 4),
    "seam-one-band": ("tall", "snake16", "step10", "bands=1", SEAM_CROP, 4),
    "seam-cold": ("tall", "snake16", "step10", "bands=2,overlap=0", SEAM_CROP,
                  4),
    "seam-warm": ("tall", "snake16", "step10", "bands=2,overlap=6", SEAM_CROP,
                  4),
    "seam-bayer": ("tall", "snake16", "step10", "bayer,bands=2", SEAM_CROP, 4),
}
AVERAGES = {
    "color-cube27-nearest-average": "color-cube27-nearest",
    "color-cube27-fs-average": "color-cube27-fs",
}
# name: the picture to average in light, as an eye far away sees it
LIGHT_AVERAGES = {
    "gray2-stored-light": "gray2-stored",
    "gray2-linear-light": "gray2-linear",
}
DIAGRAMS = ("nearest-staircase.svg", "bayer-cells.svg", "error-walk.svg",
            "kernels.svg", "light-curve.svg", "candidates.svg",
            "warm-up.svg")
MANIFEST = "dither-tutorial-figures.json"


def read_ppm(data: bytes) -> tuple[int, int, bytes]:
    """Parse the binary PPM that the programs write."""
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", data)
    if match is None:
        raise SystemExit("unexpected program output")
    return int(match[1]), int(match[2]), data[match.end():]


def crop(picture: tuple[int, int, bytes], box: tuple[int, int, int, int] | None,
         zoom: int) -> tuple[int, int, bytes]:
    """Cut a rectangle and enlarge it by repeating pixels."""
    width, height, rgb = picture
    x0, y0, w, h = box if box is not None else (0, 0, width, height)
    rows = []
    for y in range(y0, y0 + h):
        row = rgb[(y * width + x0) * 3:(y * width + x0 + w) * 3]
        if zoom != 1:
            row = b"".join(row[i:i + 3] * zoom for i in range(0, len(row), 3))
        rows.extend([row] * zoom)
    return w * zoom, h * zoom, b"".join(rows)


def chunk(kind: bytes, payload: bytes) -> bytes:
    """Return one PNG chunk."""
    crc = zlib.crc32(kind + payload)
    return struct.pack(">I", len(payload)) + kind + payload \
        + struct.pack(">I", crc)


def png(picture: tuple[int, int, bytes]) -> bytes:
    """Encode a picture, with a palette when it has at most 256 colors."""
    width, height, rgb = picture
    colors = sorted({rgb[i:i + 3] for i in range(0, len(rgb), 3)})
    if len(colors) <= 256:
        index = {color: i for i, color in enumerate(colors)}
        values = bytes(index[rgb[i:i + 3]] for i in range(0, len(rgb), 3))
        header = struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)
        extra = chunk(b"PLTE", b"".join(colors))
        row_size = width
    else:
        values = rgb
        header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
        extra = b""
        row_size = width * 3
    raw = b"".join(b"\0" + values[y * row_size:(y + 1) * row_size]
                   for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + extra
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def png_pixels(data: bytes) -> bytes:
    """Return a PNG's header, palette and decompressed pixels, to compare."""
    position = 8
    parts = []
    while position < len(data):
        size = struct.unpack(">I", data[position:position + 4])[0]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + size]
        parts.append(zlib.decompress(payload) if kind == b"IDAT" else payload)
        position += 12 + size
    return b"".join(parts)


class Programs:
    """The tutorial programs, built once, and their inputs."""

    def __init__(self, directory: Path) -> None:
        self.bin = directory
        subprocess.run(["make", "-s", f"O={directory}", "all"], cwd=CODE,
                       check=True)
        self.files = {
            "gray": self.tool("gray", "gradient", "640", "64", *GRAY_ENDS),
            "color": self.tool("color", "gradient", "640", "64"),
            "tall": self.tool("tall", "gradient", "640", "120"),
            "flat": self.tool("flat", "flat", "160", "40", "32", "32", "32"),
            "snake": SNAKE,
            "gray2": self.tool("gray2", "gray", "2"),
            "gray4": self.tool("gray4", "gray", "4"),
            "gray16": self.tool("gray16", "gray", "16"),
            "cube27": self.tool("cube27", "cube", "3"),
            "cube216": self.tool("cube216", "cube", "6"),
            "snake16": CODE / "palettes/snake16.ppm",
            "snake256": CODE / "palettes/snake256.ppm",
        }

    def tool(self, name: str, *arguments: str) -> Path:
        """Save the output of dtool as a file."""
        path = self.bin / f"input-{name}.ppm"
        with path.open("wb") as handle:
            subprocess.run([str(self.bin / "dtool"), *arguments], check=True,
                           stdout=handle)
        return path

    def output(self, picture: str, palette: str, step: str,
               option: str | None) -> bytes:
        """Run one step."""
        command = [str(self.bin / step), str(self.files[picture]),
                   str(self.files[palette])]
        if option is not None:
            command.append(option)
        return subprocess.run(command, check=True,
                              capture_output=True).stdout

    def average(self, data: bytes, size: int, command: str = "average") -> bytes:
        """Run dtool average, or dtool light, on a picture."""
        path = self.bin / "to-average.ppm"
        path.write_bytes(data)
        return subprocess.run([str(self.bin / "dtool"), command, str(size),
                               str(path)], check=True,
                              capture_output=True).stdout


def make_pictures(programs: Programs) -> dict[str, bytes]:
    """Return every PNG, by name."""
    result: dict[str, bytes] = {}
    outputs: dict[str, bytes] = {}
    for name, (picture, palette, step, option, box, zoom) in PICTURES.items():
        if palette is None:
            data = programs.files[picture].read_bytes()
        else:
            data = programs.output(picture, palette, step, option)
        outputs[name] = data
        result[f"{name}.png"] = png(crop(read_ppm(data), box, zoom))
    for name, source in AVERAGES.items():
        averaged = programs.average(outputs[source], 16)
        result[f"{name}.png"] = png(read_ppm(averaged))
    for name, source in LIGHT_AVERAGES.items():
        averaged = programs.average(outputs[source], 16, "light")
        result[f"{name}.png"] = png(read_ppm(averaged))
    return result


def title(heading: str, subtitle: str) -> str:
    """Return a figure heading."""
    return text(32, 50, heading, 28, 800, INK) \
        + text(32, 80, subtitle, 18, 500, MUTED)


def document(height: int, heading: str, description: str,
             body: list[str]) -> str:
    """Wrap a diagram."""
    return svg_document(WIDTH, height, heading, description,
                        rect(0, 0, WIDTH, height, PANEL) + "\n"
                        + "\n".join(body))


def nearest_staircase() -> str:
    """Input and output of nearest color with four grays."""
    x0, y0, size = 90, 490, 380
    grays = (0, 85, 170, 255)
    body = [title("The nearest color makes a staircase",
                  "Four grays: a smooth ramp in, four flat steps out.")]
    body.append(rect(x0, y0 - size, size, size, PAPER, GRID, 1.2, 2))
    for gray in grays:
        y = y0 - gray / 255 * size
        body.append(line(x0, y, x0 + size, y, GRID, 1))
        body.append(text(x0 - 12, y + 5, str(gray), 15, 600, MUTED, "end"))
    body.append(line(x0, y0, x0 + size, y0 - size, MUTED, 2))
    previous = None
    for value in range(256):
        out = min(grays, key=lambda gray: abs(gray - value))
        x = x0 + value / 255 * size
        y = y0 - out / 255 * size
        if previous is not None:
            body.append(line(previous[0], previous[1], x, previous[1], BLUE, 4))
            if previous[1] != y:
                body.append(line(x, previous[1], x, y, MAGENTA, 4))
        previous = (x, y)
    body.append(text(x0 + size / 2, y0 + 34, "gray level in the picture", 16,
                     650, INK, "middle"))
    body.append(text(x0 + size + 20, y0 - size + 60, "Flat parts:", 17, 750,
                     BLUE))
    body.append(text(x0 + size + 20, y0 - size + 84, "the shading is gone",
                     16, 500, INK))
    body.append(text(x0 + size + 20, y0 - size + 140, "Jumps:", 17, 750,
                     MAGENTA))
    body.append(text(x0 + size + 20, y0 - size + 164, "visible edges that", 16,
                     500, INK))
    body.append(text(x0 + size + 20, y0 - size + 186, "the picture never had",
                     16, 500, INK))
    return document(550, "The nearest color makes a staircase",
                    "A diagonal ramp of input gray levels and the staircase "
                    "of outputs with the four grays 0, 85, 170 and 255.", body)


def bayer_value(x: int, y: int, bits: int) -> int:
    """The same recursive pattern as step2.c."""
    value = 0
    for i in range(bits):
        value = value << 2 | ((x ^ y) >> i & 1) << 1 | (y >> i & 1)
    return value


def bayer_cells() -> str:
    """A threshold pattern and the dots of three tones."""
    size = 42
    body = [title("A pattern of nudges",
                  "A lighter tone passes the threshold in more cells.")]
    body.append(text(40, 128, "The 4 × 4 pattern", 17, 750, INK))
    for y in range(4):
        for x in range(4):
            value = bayer_value(x, y, 2)
            body.append(rect(40 + x * size, 142 + y * size, size - 3, size - 3,
                             GOLD_LIGHT, GOLD, 1.2, 4))
            body.append(text(40 + x * size + (size - 3) / 2,
                             142 + y * size + 27, str(value), 18, 750, INK,
                             "middle"))
    for number, tone in enumerate((0.25, 0.5, 0.75)):
        x0 = 268 + number * 148
        body.append(text(x0, 128, f"Gray {round(tone * 100)}%", 17, 750, INK))
        for y in range(4):
            for x in range(4):
                nudge = (bayer_value(x, y, 2) + 0.5) / 16 - 0.5
                lit = (tone + nudge) >= 0.5
                body.append(rect(x0 + x * 32, 142 + y * 32, 30, 30,
                                 "#f4f4f4" if lit else "#1a1a1a", GRID, 1, 2))
    body.append(text(40, 372, "Each cell's nudge is (value + 0.5) / 16 − 0.5, "
                     "times the gap between", 17, 500, MUTED))
    body.append(text(40, 398, "palette colors. Then the pixel takes the nearest "
                     "color as before.", 17, 500, MUTED))
    return document(424, "A pattern of nudges",
                    "The 4 by 4 threshold pattern and the dots it gives for "
                    "gray tones of 25, 50 and 75 percent with black and white.",
                    body)


def error_walk() -> str:
    """Seven pixels of gray 100 with black and white."""
    size = 76
    x0 = 118
    body = [title("Handing on the error",
                  "Gray 100 with only black and white: following one row.")]
    labels = (("Wanted", 150), ("Shown", 214), ("Error", 278),
              ("Passed →", 342))
    for label, y in labels:
        body.append(text(32, y, label, 16, 750, MUTED))
    carry = 0.0
    for i in range(7):
        want = 100 + carry
        shown = 0 if want < 127.5 else 255
        error = want - shown
        carry = error * 7 / 16
        x = x0 + i * (size + 8)
        body.append(rect(x, 122, size, 44, PAPER, GRID, 1.2, 6))
        body.append(text(x + size / 2, 150, f"{want:.0f}", 19, 750, INK,
                         "middle"))
        fill = "#f4f4f4" if shown else "#1a1a1a"
        body.append(rect(x, 186, size, 44, fill, GRID, 1.2, 6))
        body.append(text(x + size / 2, 214, str(shown), 18, 750,
                         INK if shown else "#f4f4f4", "middle"))
        accent = MAGENTA if error < 0 else TEAL
        body.append(rect(x, 250, size, 44, PAPER, accent, 1.2, 6))
        body.append(text(x + size / 2, 278, f"{error:+.0f}", 19, 750, accent,
                         "middle"))
        body.append(rect(x, 314, size, 44, TEAL_LIGHT if error > 0 else PAPER,
                         GRID, 1.2, 6))
        body.append(text(x + size / 2, 342, f"{carry:+.0f}", 19, 750, INK,
                         "middle"))
    body.append(text(32, 404, "Each pixel passes 7/16 of its error to the "
                     "right; the other 9/16 goes to the", 17, 500, MUTED))
    body.append(text(32, 430, "row below, which is not shown. A white pixel "
                     "overshoots, so it passes negative error on.", 17, 500, MUTED))
    return document(456, "Handing on the error",
                    "A row of seven pixels of gray 100 with black and white: "
                    "error piles up until a pixel turns white, and the "
                    "negative error then holds the next ones back.", body)


def read_kernels() -> list[tuple[str, int, list[tuple[int, int, int]]]]:
    """Read the kernel table from step4.c, the source of truth."""
    source = (CODE / "step4.c").read_text(encoding="utf-8")
    table = source[source.index("kernels[] = {"):source.index("};")]
    result = []
    for match in re.finditer(r'\{"(\w+)", (\d+), \d+, \{(.*?)\}\},', table,
                             re.S):
        taps = [tuple(int(v) for v in tap)
                for tap in re.findall(r"\{(-?\d+), (-?\d+), (\d+)\}",
                                      match[3] + "}")]
        result.append((match[1], int(match[2]), taps))
    if len(result) != 5:
        raise SystemExit("could not read the kernels of step4.c")
    return result


def kernels() -> str:
    """Every kernel of step 4 as a small grid."""
    size = 38
    names = {"fs": "Floyd–Steinberg", "atkinson": "Atkinson",
             "jajuni": "Jarvis, Judice, Ninke", "stucki": "Stucki",
             "sierra": "Sierra"}
    body = [title("Five ways to share the error",
                  "The star is the current pixel; numbers are its shares.")]
    positions = ((32, 116), (372, 116), (32, 316), (372, 316), (32, 516))
    for (x0, y0), (name, divisor, taps) in zip(positions, read_kernels()):
        total = sum(tap[2] for tap in taps)
        kept = "all of it" if total == divisor else f"only {total}/{divisor}"
        body.append(text(x0, y0, names[name], 18, 750, INK))
        body.append(text(x0, y0 + 22, f"÷ {divisor}, handing on {kept}", 15,
                         500, MUTED))
        for dy in range(3):
            for dx in range(-2, 3):
                x = x0 + (dx + 2) * size
                y = y0 + 34 + dy * size
                weight = next((tap[2] for tap in taps
                               if tap[0] == dx and tap[1] == dy), None)
                if dy == 0 and dx == 0:
                    body.append(rect(x, y, size - 3, size - 3, BLUE, BLUE, 1, 4))
                    body.append(text(x + 17, y + 25, "★", 18, 700, PAPER,
                                     "middle"))
                elif weight is not None:
                    body.append(rect(x, y, size - 3, size - 3, GOLD_LIGHT,
                                     GOLD, 1.2, 4))
                    body.append(text(x + 17, y + 24, str(weight), 16, 750, INK,
                                     "middle"))
                else:
                    body.append(rect(x, y, size - 3, size - 3, PAPER, GRID, 1,
                                     4))
    return document(690, "Five ways to share the error",
                    "The error-sharing weights of Floyd-Steinberg, Atkinson, "
                    "Jarvis-Judice-Ninke, Stucki and Sierra, as in step4.c.",
                    body)


def stored_to_light(stored: float) -> float:
    """The sRGB curve, with the same formula as step7.c."""
    value = stored / 255
    return value / 12.92 if value <= 0.04045 \
        else ((value + 0.055) / 1.055) ** 2.4


def light_curve() -> str:
    """Stored numbers against amounts of light."""
    x0, y0, size = 90, 490, 380
    body = [title("A stored number is not an amount of light",
                  "The sRGB curve: most numbers are spent on dark tones.")]
    body.append(rect(x0, y0 - size, size, size, PAPER, GRID, 1.2, 2))
    for stored in (0, 64, 128, 188, 255):
        x = x0 + stored / 255 * size
        body.append(line(x, y0, x, y0 - size, GRID, 1))
        body.append(text(x, y0 + 22, str(stored), 15, 600, MUTED, "middle"))
    for percent in (0, 25, 50, 75, 100):
        y = y0 - percent / 100 * size
        body.append(line(x0, y, x0 + size, y, GRID, 1))
        body.append(text(x0 - 12, y + 5, f"{percent}%", 15, 600, MUTED, "end"))
    previous = None
    for stored in range(256):
        point = (x0 + stored / 255 * size,
                 y0 - stored_to_light(stored) * size)
        if previous is not None:
            body.append(line(previous[0], previous[1], point[0], point[1],
                             BLUE, 4))
        previous = point
    for stored, color in ((128, MAGENTA), (188, TEAL)):
        x = x0 + stored / 255 * size
        y = y0 - stored_to_light(stored) * size
        body.append(line(x, y0, x, y, color, 2.5))
        body.append(line(x0, y, x, y, color, 2.5))
    body.append(text(x0 + size / 2, y0 + 50, "stored number", 16, 650, INK,
                     "middle"))
    side = x0 + size + 20
    body.append(text(side, y0 - size + 60, "Number 128:", 17, 750, MAGENTA))
    body.append(text(side, y0 - size + 84,
                     f"{stored_to_light(128) * 100:.0f}% of white's light", 16,
                     500, INK))
    body.append(text(side, y0 - size + 140, "50% of the light:", 17, 750, TEAL))
    body.append(text(side, y0 - size + 164, "number 188", 16, 500, INK))
    return document(560, "A stored number is not an amount of light",
                    "The sRGB curve from stored number to light: the number "
                    "128 is 22 percent of the light of white, and half of the "
                    "light is the number 188.", body)


def candidates() -> str:
    """Which palette colors a grid cell has to keep."""
    scale = 3.0
    x0, y0 = 40, 112
    extent = 128
    cell = (56.0, 56.0, 16.0)
    center = (cell[0] + cell[2] / 2, cell[1] + cell[2] / 2)
    radius = cell[2] / 2 * 2 ** 0.5
    colors = []
    state = 2463534242
    for _ in range(40):
        pair = []
        for _ in range(2):
            state ^= state << 13 & 0xffffffff
            state ^= state >> 17
            state ^= state << 5 & 0xffffffff
            pair.append(state % extent)
        colors.append(tuple(pair))
    distance = [((c[0] - center[0]) ** 2 + (c[1] - center[1]) ** 2) ** 0.5
                for c in colors]
    nearest = min(distance)
    limit = nearest + 2 * radius

    def px(x: float, y: float) -> tuple[float, float]:
        return x0 + x * scale, y0 + y * scale
    body = [title("Most colors can never win inside a cell",
                  f"One slice of the color cube, with {len(colors)} palette "
                  "colors.")]
    corner = px(0, 0)
    body.append(rect(corner[0], corner[1], extent * scale, extent * scale,
                     PAPER, GRID, 1.2, 2))
    box = px(cell[0], cell[1])
    body.append(rect(box[0], box[1], cell[2] * scale, cell[2] * scale,
                     GOLD_LIGHT, GOLD, 2, 2))
    middle = px(*center)
    body.append(f'<circle cx="{middle[0]:.1f}" cy="{middle[1]:.1f}" '
                f'r="{limit * scale:.1f}" fill="none" stroke="{TEAL}" '
                'stroke-width="2.5"/>')
    body.append(f'<circle cx="{middle[0]:.1f}" cy="{middle[1]:.1f}" r="4" '
                f'fill="{INK}"/>')
    kept = 0
    for color, length in zip(colors, distance):
        point = px(*color)
        inside = length <= limit
        kept += inside
        fill = TEAL if inside else MUTED
        if length == nearest:
            fill = BLUE
            body.append(line(middle[0], middle[1], point[0], point[1], BLUE, 2))
        body.append(f'<circle cx="{point[0]:.1f}" cy="{point[1]:.1f}" r="6" '
                    f'fill="{fill}"/>')
    side = x0 + extent * scale + 24
    rows = ((BLUE, "n: nearest to the", "center, at distance d"),
            (GOLD, "The cell: every color", "in it is within r"),
            (TEAL, f"Inside d + 2r: {kept} of", f"{len(colors)} colors stay"),
            (MUTED, "Outside: can never", "be the nearest"))
    for number, (color, first, second) in enumerate(rows):
        y = y0 + 30 + number * 84
        body.append(text(side, y, first, 16, 750, color))
        body.append(text(side, y + 24, second, 16, 500, INK))
    return document(520, "Most colors can never win inside a cell",
                    "A grid cell, the nearest color to its center, and the "
                    "circle of radius d plus 2r that holds every color which "
                    "can be nearest for some color of the cell.", body)


def warm_up() -> str:
    """Bands of rows, each with its own error history."""
    body = [title("Each band has its own history",
                  "A worker starts above its band and throws those rows away.")]
    rows = (("Band 1", 120, 110, BLUE, None),
            ("Band 2", 262, 110, TEAL, 58),
            ("Band 3", 404, 110, MAGENTA, 58))
    x0, width = 150, 400
    for name, y, height, color, overlap in rows:
        body.append(rect(x0, y, width, height, PAPER, color, 2, 4))
        body.append(text(x0 + width / 2, y + height / 2 + 6,
                         "rows this worker keeps", 16, 650, color, "middle"))
        body.append(text(32, y + height / 2 + 6, name, 17, 750, INK))
        if overlap is not None:
            body.append(rect(x0 + 40, y - overlap, width - 80, overlap,
                             GOLD_LIGHT, GOLD, 1.5, 4))
            body.append(text(x0 + width / 2, y - overlap / 2 + 6,
                             "warm-up rows, computed and not kept", 15, 600,
                             INK, "middle"))
    body.append(text(32, 556, "A band starts with no error from above. Warm-up "
                     "rows let its pattern", 17, 500, MUTED))
    body.append(text(32, 582, "get going first. No two workers ever share "
                     "error, so none has to wait.", 17, 500, MUTED))
    return document(606, "Each band has its own history",
                    "Three bands of rows; each band after the first has a "
                    "worker that starts above it on warm-up rows it does not "
                    "keep.", body)


FACTORIES = {
    "nearest-staircase.svg": nearest_staircase,
    "bayer-cells.svg": bayer_cells,
    "error-walk.svg": error_walk,
    "kernels.svg": kernels,
    "light-curve.svg": light_curve,
    "candidates.svg": candidates,
    "warm-up.svg": warm_up,
}


def generate_all() -> dict[str, bytes]:
    """Return every asset by name."""
    with tempfile.TemporaryDirectory(prefix="dither-figures-") as temp:
        assets = make_pictures(Programs(Path(temp)))
    for name, factory in FACTORIES.items():
        assets[name] = factory().encode("utf-8")
    assets[MANIFEST] = (json.dumps({
        "assets": sorted(assets),
        "pictures": {name: {"picture": value[0], "palette": value[1],
                            "step": value[2], "option": value[3],
                            "crop": value[4], "zoom": value[5]}
                     for name, value in PICTURES.items()},
        "averages": AVERAGES,
        "light_averages": LIGHT_AVERAGES,
        "sources": {"programs": "docs/tutorial/dithering/code"},
    }, indent=2, sort_keys=True) + "\n").encode("utf-8")
    return assets


def same(name: str, a: bytes, b: bytes) -> bool:
    """Compare assets; PNG files by their pixels, not by compressed bytes."""
    if name.endswith(".png"):
        return png_pixels(a) == png_pixels(b)
    return a == b


def main() -> None:
    """Generate or verify the figures."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=ROOT / "docs/tutorial/dithering/figures")
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    assets = generate_all()
    if arguments.check:
        stale = [name for name, data in assets.items()
                 if not (arguments.output_dir / name).is_file()
                 or not same(name, (arguments.output_dir / name).read_bytes(),
                             data)]
        if stale:
            raise SystemExit("dithering tutorial figures are stale or "
                             "missing: " + ", ".join(stale))
        return
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    for name, data in assets.items():
        (arguments.output_dir / name).write_bytes(data)


if __name__ == "__main__":
    main()
