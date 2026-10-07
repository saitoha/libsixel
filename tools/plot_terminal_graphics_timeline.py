#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the overview timeline of terminal graphics history.

Run without arguments to regenerate the figures under
docs/sixel-format/terminal-graphics-history/figures, or with --check to
verify that the committed files are current.  The events and their sources
are documentary data taken from the history document, not measurements.
"""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path

from plot_encoding_pipeline_figures import (
    GRID,
    INK,
    MUTED,
    PAPER,
    line,
    rect,
    svg_document,
    tag,
    text,
)

ASSET_NAMES = (
    "terminal-graphics-timeline-wide.svg",
    "terminal-graphics-timeline-mobile.svg",
    "terminal-graphics-timeline.json",
)

DOCUMENT = "docs/sixel-format/terminal-graphics-history/README.md"

# Lanes run from the oldest tradition at the top to the newest at the
# bottom.  Each entry is (key, label, color).
LANES = (
    ("art", "Character art", "#8e5a2b"),
    ("mosaic", "Mosaic characters and Unicode", "#b07d12"),
    ("render", "Character rendering software", "#c0392b"),
    ("terminal", "Vector and DEC terminals", "#0f7c80"),
    ("videotex", "Videotex and image coding", "#7a4fa0"),
    ("emulator", "Software terminals and SIXEL tools", "#17618c"),
    ("protocol", "Image protocols and proposals", "#2f8f5b"),
)

# (decimal year, lane, label, source).  The decimal part only orders events
# within a year; labels show the integer year.
EVENTS = (
    (1875.0, "art", "Typewriter-art advertisement",
     "https://en.wikipedia.org/wiki/ASCII_art#Typewriter_art"),
    (1893.0, "art", "Pitman typed ornaments",
     "https://www.themarginalian.org/2014/05/23/typewriter-art-laurence-king/"),
    (1898.8, "art", "Stacey's typed butterfly",
     "https://infostory.com/2014/05/23/the-history-of-typewriter-art/"),
    (1932.0, "art", "Paul Smith's typewriter pictures",
     "https://en.wikipedia.org/wiki/Paul_Smith_(artist)"),
    (1939.0, "art", "Artyping",
     "https://loriemerson.net/2014/01/26/artyping-in-1939/"),
    (1955.0, "art", "Concrete poetry",
     "https://en.wikipedia.org/wiki/Concrete_poetry"),
    (1962.0, "art", "Teleprinter and RTTY art",
     "https://en.wikipedia.org/wiki/ASCII_art#TTY_and_RTTY"),
    (1966.0, "art", "Studies in Perception I",
     "https://en.wikipedia.org/wiki/Leon_Harmon"),
    (1985.0, "art", "VT100 animations",
     "http://artscene.textfiles.com/vt100/"),
    (1987.9, "art", "Christmas Tree EXEC",
     "https://en.wikipedia.org/wiki/Christmas_Tree_EXEC"),
    (1991.0, "art", "BBS ANSI art",
     "https://en.wikipedia.org/wiki/ANSI_art"),
    (1995.0, "art", "Usenet ASCII art",
     "https://en.wikipedia.org/wiki/ASCII_art"),
    (1974.7, "mosaic", "Ceefax mosaics",
     "https://en.wikipedia.org/wiki/Teletext_character_set"),
    (1977.5, "mosaic", "TRS-80 semigraphics",
     "https://en.wikipedia.org/wiki/Semigraphics"),
    (1979.5, "mosaic", "PC-8001 2×4 graphics",
     "https://en.wikipedia.org/wiki/Semigraphics"),
    (1999.7, "mosaic", "Unicode 3.0 Braille",
     "https://en.wikipedia.org/wiki/Braille_Patterns"),
    (2002.2, "mosaic", "Unicode 3.2 quadrants",
     "https://www.unicode.org/L2/L2000/00159-ucsterminal.txt"),
    (2020.2, "mosaic", "Unicode 13 sextants",
     "https://www.unicode.org/L2/L2019/19025-terminals-prop.pdf"),
    (2024.7, "mosaic", "Unicode 16 octants",
     "https://www.unicode.org/L2/L2021/21235-terminals-supplement.pdf"),
    (1997.0, "render", "AAlib",
     "https://aa-project.sourceforge.net/aalib/"),
    (2003.9, "render", "libcaca",
     "http://caca.zoy.org/study/"),
    (2014.3, "render", "drawille",
     "https://github.com/asciimoo/drawille"),
    (2018.3, "render", "chafa",
     "https://github.com/hpjansson/chafa"),
    (1972.0, "terminal", "Tektronix 4010",
     "https://en.wikipedia.org/wiki/Tektronix_4010"),
    (1974.0, "terminal", "Tektronix 4014",
     "https://en.wikipedia.org/wiki/Tektronix_4010"),
    (1981.5, "terminal", "ReGIS on VT125",
     "https://en.wikipedia.org/wiki/ReGIS"),
    (1984.6, "terminal", "VT240 SIXEL",
     "https://en.wikipedia.org/wiki/VT220"),
    (1987.5, "terminal", "VT330 and VT340",
     "https://vt100.net/docs/vt3xx-gp/"),
    (1978.0, "videotex", "Telidon",
     "https://en.wikipedia.org/wiki/Telidon"),
    (1979.0, "videotex", "Prestel",
     "https://en.wikipedia.org/wiki/Prestel"),
    (1980.5, "videotex", "CCITT T.4 fax",
     "https://www.rfc-editor.org/rfc/rfc804"),
    (1982.0, "videotex", "Minitel",
     "https://en.wikipedia.org/wiki/Minitel"),
    (1983.0, "videotex", "NAPLPS",
     "https://en.wikipedia.org/wiki/NAPLPS"),
    (1983.5, "videotex", "Bildschirmtext",
     "https://en.wikipedia.org/wiki/Bildschirmtext"),
    (1984.9, "videotex", "CAPTAIN",
     "https://en.wikipedia.org/wiki/Captain_(videotex)"),
    (1985.5, "videotex", "PICA photovideotex",
     "https://www.itu.int/dms_pub/itu-s/opb/journal/S-JOURNAL-ICTS.V3I1-2020-13-PDF-E.pdf"),
    (1986.85, "videotex", "JPEG formed",
     "https://www.ecma-international.org/wp-content/uploads/IS-presentation-to-JTC1SC29WG1-regarding-JPEG25-celebration.pdf"),
    (1988.0, "videotex", "JBIG",
     "https://en.wikipedia.org/wiki/Joint_Bi-level_Image_Experts_Group"),
    (1992.0, "videotex", "RIPscrip",
     "https://en.wikipedia.org/wiki/Remote_Imaging_Protocol"),
    (1992.7, "videotex", "JPEG T.81",
     "https://www.w3.org/Graphics/JPEG/itu-t81.pdf"),
    (1995.9, "videotex", "image/naplps",
     "https://www.iana.org/assignments/media-types/image/naplps"),
    (1996.5, "videotex", "NAPLPS plug-in",
     "http://uclsun.ucl.co.jp/naplps/naptech-e.html"),
    (1994.0, "emulator", "Tera Term TEK",
     "https://en.wikipedia.org/wiki/Tera_Term"),
    (2001.75, "emulator", "w3m-img",
     "https://github.com/tats/w3m/blob/master/doc/README.img"),
    (2009.1, "emulator", "RLogin Tek",
     "https://github.com/kmiya-culti/RLogin/blob/master/docs/history.html"),
    (2010.7, "emulator", "RLogin SIXEL",
     "docs/project-history.md"),
    (2012.3, "emulator", "mlterm SIXEL",
     "docs/project-history.md"),
    (2012.85, "emulator", "PySixel",
     "https://pypi.org/project/PySixel/"),
    (2013.5, "emulator", "xterm SIXEL (experimental)",
     "https://invisible-island.net/xterm/xterm.log.html#xterm_294"),
    (2014.17, "emulator", "kmiya's sixel",
     "https://github.com/saitoha/sixel"),
    (2014.2, "emulator", "libsixel",
     "docs/project-history.md"),
    (2014.4, "emulator", "monosixel (tw-sixel)",
     "https://github.com/saitoha/libsixel/commit/d0db9b5c3d411442d9ca2af2a4465bb36f4dce7f"),
    (2014.76, "emulator", "ImageMagick SIXEL coder",
     "https://github.com/ImageMagick/ImageMagick6/commits/main/coders/sixel.c"),
    (2014.75, "emulator", "w3mimg-sixel",
     "https://github.com/uobikiemukot/sdump"),
    (2014.85, "emulator", "w3m SIXEL",
     "https://github.com/tats/w3m/blob/master/doc/README.sixel"),
    (2015.0, "emulator", "gnuplot sixeltek",
     "https://sourceforge.net/p/gnuplot/gnuplot-main/ci/master/tree/term/tek.trm"),
    (2017.67, "emulator", "gnuplot sixelgd",
     "https://sourceforge.net/p/gnuplot/gnuplot-main/ci/master/tree/NEWS"),
    (2020.63, "emulator", "xterm SIXEL by default",
     "https://invisible-island.net/xterm/xterm.log.html#xterm_359"),
    (2020.6, "emulator", "mintty Tek",
     "https://github.com/mintty/mintty/blob/master/wiki/Changelog.md"),
    (2024.1, "emulator", "tmux 3.4 SIXEL",
     "https://github.com/tmux/tmux/blob/master/CHANGES"),
    (2024.65, "emulator", "Windows Terminal SIXEL",
     "https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-22-release/"),
    (2011.8, "protocol", "GateOne",
     "https://github.com/liftoff/GateOne"),
    (2012.1, "protocol", "tanasinn OSC",
     "https://github.com/saitoha/tanasinn"),
    (2012.9, "protocol", "Terminology tycat",
     "https://github.com/borisfaure/terminology"),
    (2013.3, "protocol", "FinalTerm OSC 133",
     "https://github.com/p-e-w/finalterm"),
    (2014.05, "protocol", "iTerm2 OSC 1337",
     "https://iterm2.com/documentation-images.html"),
    (2015.6, "protocol", "DomTerm OSC 72",
     "https://domterm.org/Wire-byte-protocol.html"),
    (2017.8, "protocol", "kitty graphics",
     "https://sw.kovidgoyal.net/kitty/changelog/"),
    (2019.2, "protocol", "terminal-wg images",
     "https://gitlab.freedesktop.org/terminal-wg/specifications/issues/12"),
    (2021.4, "protocol", "STEGAP",
     "https://nick-black.com/dankwiki/index.php/Spriteful_TErminal_GrAphics_Protocol"),
    (2021.6, "protocol", "Windows Terminal DECDLD",
     "https://github.com/microsoft/terminal/issues/9164"),
    (2021.99, "protocol", "xterm.js image add-on",
     "https://github.com/jerch/xterm-addon-image"),
)

FIRST_YEAR = 1870
BREAK_YEAR = 1960
LAST_YEAR = 2026
LANE_COLOR = {key: color for key, _label, color in LANES}
LANE_LABEL = {key: label for key, label, _color in LANES}
FONT_SIZE = 13
CHAR_WIDTH = 6.9


def year_x(year: float, left: float, split: float, right: float) -> float:
    """Map a year onto an axis compressed before BREAK_YEAR."""
    if year < BREAK_YEAR:
        return left + (year - FIRST_YEAR) * (split - left) / (BREAK_YEAR - FIRST_YEAR)
    return split + (year - BREAK_YEAR) * (right - split) / (LAST_YEAR - BREAK_YEAR)


def label_text(year: float, label: str) -> str:
    """Return the label shown beside an event."""
    return f"{int(year)} {label}"


def pack_lane(events: list[tuple[float, str]], axis: tuple[float, float, float],
              right_limit: float) -> list[tuple[float, str, int, bool]]:
    """Assign labels to sub-rows so that they do not overlap.

    Each label is placed to the right of its marker, or to the left when it
    would run past the drawing area.  A label occupies the first sub-row
    whose previous label ends before it starts.
    """
    placed = []
    row_ends: list[float] = []
    for year, label in sorted(events):
        x = year_x(year, *axis)
        value = label_text(year, label)
        width = len(value) * CHAR_WIDTH
        left_side = x + 10 + width > right_limit
        start = x - 10 - width if left_side else x - 7
        end = x + 7 if left_side else x + 10 + width
        for index, row_end in enumerate(row_ends):
            if row_end + 8 <= start:
                row_ends[index] = end
                row = index
                break
        else:
            row_ends.append(end)
            row = len(row_ends) - 1
        placed.append((year, label, row, left_side))
    return placed


def wide_figure() -> str:
    """Return the overview: one band per lane on a shared year axis."""
    width = 1400
    label_column = 210
    left, split, right = label_column + 20.0, label_column + 210.0, width - 30.0
    axis = (left, split, right)
    top = 104.0
    sub_row = 19.0
    lane_gap = 14.0
    packed = {key: pack_lane([(y, l) for y, k, l, _s in EVENTS if k == key],
                             axis, right)
              for key, _label, _color in LANES}
    heights = {key: (max(r for _y, _l, r, _s in packed[key]) + 1) * sub_row + 16
               for key in packed}
    axis_y = top + sum(heights.values()) + lane_gap * (len(LANES) - 1) + 14
    height = int(axis_y + 70)
    title = "Terminal graphics, 1875–2024"
    body = [rect(0, 0, width, height, PAPER),
            text(30, 42, title, 26, 700),
            text(30, 70, "Seven traditions on one year axis; years before 1960 "
                 "are compressed.", 15, fill=MUTED)]
    ticks = [1875, 1900, 1925] + list(range(1960, 2021, 10))
    for year in ticks:
        x = year_x(year, *axis)
        body.append(line(x, top - 8, x, axis_y, GRID, 1.0))
        body.append(line(x, axis_y - 5, x, axis_y + 5, INK, 1.5))
        body.append(text(x, axis_y + 22, str(year), 13, fill=MUTED, anchor="middle"))
    body.append(line(left, axis_y, split - 6, axis_y, INK, 1.5))
    body.append(line(split + 6, axis_y, right, axis_y, INK, 1.5))
    body.append(line(split - 9, axis_y + 6, split - 3, axis_y - 6, INK, 1.5))
    body.append(line(split + 3, axis_y + 6, split + 9, axis_y - 6, INK, 1.5))
    y = top
    for key, label, color in LANES:
        band = heights[key]
        body.append(rect(20, y - 4, width - 40, band, "#f8fafc", radius=6.0))
        body.append(tag("rect", x="20.0", y=f"{y - 4:.1f}", width="6.0",
                        height=f"{band:.1f}", fill=color))
        body.append(text(36, y + 14, label, 13, 700, color))
        for year, event, row, left_side in packed[key]:
            x = year_x(year, *axis)
            cy = y + 10 + row * sub_row
            body.append(tag("circle", cx=f"{x:.1f}", cy=f"{cy:.1f}", r="5",
                            fill=color))
            value = label_text(year, event)
            if left_side:
                body.append(text(x - 10, cy + 4.5, value, FONT_SIZE, anchor="end"))
            else:
                body.append(text(x + 10, cy + 4.5, value, FONT_SIZE))
        y += band + lane_gap
    description = (
        "An overview timeline from 1875 to 2024 with seven horizontal bands: "
        "character art from typewriter art to Usenet ASCII art; mosaic "
        "characters from Ceefax to Unicode 16 octants; character rendering "
        "software from AAlib to chafa; Tektronix and DEC terminals from the "
        "Tektronix 4010 to the VT340; videotex and image coding from Telidon "
        "to JPEG and the NAPLPS plug-in; software terminals and SIXEL from "
        "Tera Term to Windows Terminal, including SIXEL tools from PySixel to gnuplot; and image protocols and proposals "
        "from GateOne to the xterm.js image add-on. Years before 1960 are "
        "compressed on the axis.")
    return svg_document(width, height, title, description, "".join(body))


def mobile_figure() -> str:
    """Return a vertical list of events, oldest first, colored by lane."""
    width = 380
    row = 23
    legend_top = 92
    top = legend_top + 20 * len(LANES) + 26
    events = sorted(EVENTS)
    height = int(top + row * len(events) + 20)
    axis_x = 66.0
    title = "Terminal graphics, 1875–2024"
    body = [rect(0, 0, width, height, PAPER),
            text(16, 38, title, 21, 700),
            text(16, 62, "Oldest first, colored by tradition", 14, fill=MUTED)]
    for index, (key, label, color) in enumerate(LANES):
        y = legend_top + index * 20
        body.append(tag("circle", cx="24.0", cy=f"{y - 5:.1f}", r="6", fill=color))
        body.append(text(38, y, label, 13, fill=MUTED))
    body.append(line(axis_x, top - 12, axis_x, top + row * (len(events) - 1) + 12,
                     GRID, 2.0))
    previous = None
    for index, (year, key, label, _source) in enumerate(events):
        y = top + index * row
        if int(year) != previous:
            body.append(text(axis_x - 14, y + 5, str(int(year)), 13, 700, MUTED, "end"))
        previous = int(year)
        body.append(tag("circle", cx=f"{axis_x:.1f}", cy=f"{y:.1f}", r="5",
                        fill=LANE_COLOR[key]))
        body.append(text(axis_x + 14, y + 5, label, 13))
    description = (
        "A vertical timeline of terminal graphics from 1875 to 2024, oldest "
        "first, with each event colored by one of seven traditions: character "
        "art, mosaic characters and Unicode, character rendering software, "
        "vector and DEC terminals, videotex and image coding, software "
        "terminals and SIXEL tools, and image protocols and proposals.")
    return svg_document(width, height, title, description, "".join(body))


def manifest() -> dict[str, object]:
    """Return the event data and their sources."""
    return {
        "generator": "tools/plot_terminal_graphics_timeline.py",
        "document": DOCUMENT,
        "kind": "documentary timeline from cited sources, not a measurement",
        "lanes": [{"key": key, "label": label} for key, label, _color in LANES],
        "events": [
            {"year": year, "lane": lane, "label": label, "source": source}
            for year, lane, label, source in sorted(EVENTS)
        ],
    }


def generate(output_dir: Path) -> None:
    """Generate both SVG figures and their provenance manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / ASSET_NAMES[0]).write_text(wide_figure(), encoding="utf-8")
    (output_dir / ASSET_NAMES[1]).write_text(mobile_figure(), encoding="utf-8")
    (output_dir / ASSET_NAMES[2]).write_text(
        json.dumps(manifest(), indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8")


def check(output_dir: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(prefix="terminal-graphics-timeline-") as temp:
        generated = Path(temp)
        generate(generated)
        stale = [name for name in ASSET_NAMES
                 if not (output_dir / name).is_file()
                 or (output_dir / name).read_bytes() != (generated / name).read_bytes()]
        if stale:
            raise SystemExit("terminal graphics timeline figures are stale or "
                             "missing: " + ", ".join(stale))


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir", type=Path,
        default=source_root / "docs/sixel-format/terminal-graphics-history/figures",
        help="directory for generated SVG and manifest files")
    parser.add_argument(
        "--check", action="store_true",
        help="compare committed assets with a fresh deterministic generation")
    return parser.parse_args()


def main() -> None:
    """Generate or verify the timeline figures."""
    arguments = parse_args()
    if arguments.check:
        check(arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
