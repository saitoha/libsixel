#!/usr/bin/env python3
"""Generate the timeline of image formats and terminal standards."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path

from plot_encoding_pipeline_figures import (
    BLUE,
    GOLD,
    GRID,
    INK,
    MUTED,
    PAPER,
    TEAL,
    line,
    rect,
    svg_document,
    tag,
    text,
)

ASSET_NAMES = (
    "codec-timeline-wide.svg",
    "codec-timeline-mobile.svg",
    "codec-timeline-figures.json",
)

FIRST_YEAR = 1970
LAST_YEAR = 2025

# (decimal year, lane, label, detail, source). The lane selects the side of
# the axis: "format" above it, "terminal" and "sixel" below it.
EVENTS = (
    (1971.9, "terminal", "ECMA-35", "code extension (ISO 2022)",
     "https://ecma-international.org/publications-and-standards/standards/ecma-35/"),
    (1976.2, "terminal", "ECMA-48", "control functions, DCS/ST",
     "https://ecma-international.org/publications-and-standards/standards/ecma-48/"),
    (1984.0, "sixel", "VT240", "SIXEL on a DEC screen",
     "https://terminals-wiki.org/wiki/index.php/DEC_VT240"),
    (1986.85, "format", "JPEG committee", "ISDN photo-videotex",
     "https://www.itu.int/dms_pub/itu-s/opb/journal/S-JOURNAL-ICTS.V3I1-2020-13-PDF-E.pdf"),
    (1987.3, "sixel", "VT340", "color SIXEL terminal",
     "https://terminals-wiki.org/wiki/index.php/DEC_VT340"),
    (1987.45, "format", "GIF87a", "CompuServe VIDTEX",
     "https://www.w3.org/Graphics/GIF/spec-gif87.txt"),
    (1990.58, "format", "GIF89a", "ESC [ > 0 g dialogue",
     "https://www.w3.org/Graphics/GIF/spec-gif89a.txt"),
    (1992.7, "format", "JPEG T.81", "telematic T series",
     "https://www.w3.org/Graphics/JPEG/itu-t81.pdf"),
    (1996.75, "format", "PNG 1.0", "signature detects damage",
     "http://www.libpng.org/pub/png/pnghist.html"),
    (2000.9, "format", "JPEG 2000", "JP2 box may end at EOF",
     "https://www.itu.int/rec/T-REC-T.800"),
    (2010.7, "sixel", "RLogin 2.9.0", "SIXEL returns",
     "https://github.com/kmiya-culti/RLogin"),
    (2012.18, "terminal", "Gate One 1.0", "cat JPEG/PNG",
     "https://liftoff.github.io/GateOne/ReleaseNotes/index.html"),
    (2014.0, "sixel", "libsixel", "library forms",
     "docs/project-history.md"),
    (2021.9, "format", "QOI", "no entropy coding",
     "https://phoboslab.org/log/2021/11/qoi-fast-lossless-image-compression"),
)

LANE_COLORS = {"format": GOLD, "terminal": BLUE, "sixel": TEAL}

def year_x(year: float, left: float, right: float) -> float:
    """Map a decimal year to the horizontal axis."""
    return left + (year - FIRST_YEAR) * (right - left) / (LAST_YEAR - FIRST_YEAR)


def legend(x: float, y: float) -> str:
    """Return the lane legend."""
    items = (("format", "Image format"), ("terminal", "Terminal standard or emulator"),
             ("sixel", "SIXEL terminal or library"))
    body = []
    for name, label in items:
        body.append(tag("circle", cx=f"{x + 6:.1f}", cy=f"{y - 6:.1f}", r="6",
                        fill=LANE_COLORS[name]))
        body.append(text(x + 18, y, label, 15, fill=MUTED))
        x += 40 + len(label) * 7.4
    return "".join(body)


def wide_figure() -> str:
    """Return the timeline: one row per event, positioned on a year axis."""
    width = 1200
    left, right = 60.0, 1140.0
    top, row = 118.0, 38.0
    axis = top + row * (len(EVENTS) - 1) + 30
    height = int(axis + 90)
    body = [rect(0, 0, width, height, PAPER),
            text(40, 44, "Image formats and the terminal, 1971-2021", 28, 700),
            text(40, 74, "One event per row, placed on a common year axis.", 17, fill=MUTED)]
    for decade in range(FIRST_YEAR, LAST_YEAR + 1, 10):
        x = year_x(decade, left, right)
        body.append(line(x, top - 20, x, axis, GRID, 1.0))
        body.append(line(x, axis - 6, x, axis + 6, INK, 2.0))
        body.append(text(x, axis + 26, str(decade), 15, fill=MUTED, anchor="middle"))
    body.append(line(left, axis, right, axis, INK, 2.0))
    for index, (year, lane, label, detail, _source) in enumerate(EVENTS):
        x = year_x(year, left, right)
        y = top + index * row
        body.append(tag("circle", cx=f"{x:.1f}", cy=f"{y:.1f}", r="7",
                        fill=LANE_COLORS[lane]))
        value = f"{int(year)} {label} - {detail}"
        if x > right - 330:
            body.append(text(x - 14, y + 6, value, 16, 600, anchor="end"))
        else:
            body.append(text(x + 14, y + 6, value, 16, 600))
    body.append(legend(40, height - 24))
    return svg_document(
        width, height, "Image formats and the terminal, 1971-2021",
        "A timeline from 1970 to 2025 with one row per event, oldest first: "
        "ECMA-35 1971, ECMA-48 1976, VT240 1984, JPEG committee 1986, VT340 1987, "
        "GIF87a 1987, GIF89a specification 1990, JPEG T.81 1992, PNG 1.0 1996, "
        "JPEG 2000 2000, RLogin 2.9.0 2010, Gate One 1.0 2012, libsixel 2014 "
        "and QOI 2021. Colors distinguish image formats, terminal standards or "
        "emulators, and SIXEL terminals or libraries.",
        "".join(body))


def mobile_figure() -> str:
    """Return the vertical timeline."""
    width = 380
    row = 58
    top = 118
    height = top + row * (len(EVENTS) - 1) + 44 + 3 * 20 + 10
    axis_x = 86.0
    body = [rect(0, 0, width, height, PAPER),
            text(20, 40, "Image formats and the terminal", 22, 700),
            text(20, 66, "1971-2021, oldest first", 15, fill=MUTED),
            line(axis_x, top - 18, axis_x, top + row * (len(EVENTS) - 1) + 18, INK, 2.0)]
    for index, (year, lane, label, detail, _source) in enumerate(EVENTS):
        y = top + index * row
        body.append(text(axis_x - 16, y + 6, str(int(year)), 16, 700, MUTED, "end"))
        body.append(tag("circle", cx=f"{axis_x:.1f}", cy=f"{y:.1f}", r="7",
                        fill=LANE_COLORS[lane]))
        body.append(text(axis_x + 18, y, label, 17, 700))
        body.append(text(axis_x + 18, y + 20, detail, 14, fill=MUTED))
    legend_top = top + row * (len(EVENTS) - 1) + 44
    for index, (name, label) in enumerate((("format", "Image format"),
                                           ("terminal", "Terminal standard or emulator"),
                                           ("sixel", "SIXEL terminal or library"))):
        y = legend_top + index * 20
        body.append(tag("circle", cx="26.0", cy=f"{y - 5:.1f}", r="6",
                        fill=LANE_COLORS[name]))
        body.append(text(40, y, label, 14, fill=MUTED))
    return svg_document(
        width, height, "Image formats and the terminal, 1971-2021",
        "A vertical timeline listing, oldest first: ECMA-35, ECMA-48, VT240, JPEG "
        "committee, VT340, GIF87a, GIF89a, JPEG T.81, PNG 1.0, JPEG 2000, RLogin "
        "2.9.0, Gate One 1.0, libsixel and QOI.",
        "".join(body))


def manifest() -> dict[str, object]:
    """Return the event data and their sources."""
    return {
        "generator": "tools/plot_codec_timeline_figures.py",
        "kind": "documentary timeline from cited sources, not a measurement",
        "events": [
            {"year": year, "lane": lane, "label": label, "detail": detail,
             "source": source}
            for year, lane, label, detail, source in EVENTS
        ],
    }


def generate(output_dir: Path) -> None:
    """Generate both SVG figures and their provenance manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "codec-timeline-wide.svg").write_text(wide_figure(), encoding="utf-8")
    (output_dir / "codec-timeline-mobile.svg").write_text(mobile_figure(), encoding="utf-8")
    (output_dir / "codec-timeline-figures.json").write_text(
        json.dumps(manifest(), indent=2, sort_keys=True) + "\n", encoding="utf-8")


def check(output_dir: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(prefix="codec-timeline-figures-") as temp:
        generated = Path(temp)
        generate(generated)
        stale = [name for name in ASSET_NAMES
                 if not (output_dir / name).is_file()
                 or (output_dir / name).read_bytes() != (generated / name).read_bytes()]
        if stale:
            raise SystemExit("codec timeline figures are stale or missing: "
                             + ", ".join(stale))


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir", type=Path,
        default=source_root / "docs/sixel-format/comparison/figures",
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
