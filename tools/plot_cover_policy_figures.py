#!/usr/bin/env python3
"""Generate deterministic explanatory figures for the palette cover policy."""

from __future__ import annotations

import argparse
import binascii
import html
import json
import math
import random
import struct
import tempfile
import zlib
from pathlib import Path
from typing import Iterable, Sequence


WIDTH = 1200
INK = "#172033"
MUTED = "#556176"
GRID = "#CBD5E1"
PAPER = "#FFFFFF"
PANEL = "#F8FAFC"
BLUE = "#2563EB"
BLUE_LIGHT = "#DBEAFE"
GOLD = "#D97706"
GOLD_LIGHT = "#FEF3C7"
MAGENTA = "#BE185D"
MAGENTA_LIGHT = "#FCE7F3"
TEAL = "#0F766E"
TEAL_LIGHT = "#CCFBF1"

ASSET_NAMES = (
    "cover-convex-hull-3d.svg",
    "cover-hull-projection.svg",
    "cover-rgb-corners.svg",
    "cover-boundary-clamping.svg",
    "cover-reachability-limits.svg",
    "cover-flicker-keyframes.svg",
    "cover-flicker.png",
    "cover-policy-figures.json",
)


def attrs(**values: object) -> str:
    """Return escaped SVG attributes."""
    parts = []
    for name, value in values.items():
        if value is None:
            continue
        parts.append(
            f'{name.replace("_", "-")}="{html.escape(str(value), quote=True)}"'
        )
    return " ".join(parts)


def tag(name: str, body: str = "", **values: object) -> str:
    """Return one SVG element."""
    attribute_text = attrs(**values)
    if body:
        return f"<{name} {attribute_text}>{body}</{name}>"
    return f"<{name} {attribute_text}/>"


def text(x: float, y: float, value: str, size: int = 22,
         weight: int = 400, fill: str = INK, anchor: str = "start",
         family: str = "system-ui, sans-serif") -> str:
    """Return one SVG text label."""
    return tag(
        "text",
        html.escape(value),
        x=f"{x:.1f}",
        y=f"{y:.1f}",
        fill=fill,
        font_size=size,
        font_weight=weight,
        font_family=family,
        text_anchor=anchor,
    )


def line(x1: float, y1: float, x2: float, y2: float,
         stroke: str = INK, width: float = 2.0,
         dash: str | None = None, marker: str | None = None,
         opacity: float = 1.0) -> str:
    """Return one SVG line."""
    return tag(
        "line",
        x1=f"{x1:.1f}",
        y1=f"{y1:.1f}",
        x2=f"{x2:.1f}",
        y2=f"{y2:.1f}",
        stroke=stroke,
        stroke_width=f"{width:.1f}",
        stroke_dasharray=dash,
        marker_end=f"url(#{marker})" if marker else None,
        opacity=f"{opacity:.3f}",
        stroke_linecap="round",
    )


def circle(cx: float, cy: float, radius: float, fill: str,
           stroke: str = PAPER, width: float = 2.0,
           opacity: float = 1.0) -> str:
    """Return one SVG circle."""
    return tag(
        "circle",
        cx=f"{cx:.1f}",
        cy=f"{cy:.1f}",
        r=f"{radius:.1f}",
        fill=fill,
        stroke=stroke,
        stroke_width=f"{width:.1f}",
        opacity=f"{opacity:.3f}",
    )


def rect(x: float, y: float, width: float, height: float,
         fill: str = PAPER, stroke: str = "none",
         stroke_width: float = 0.0, radius: float = 0.0,
         opacity: float = 1.0) -> str:
    """Return one SVG rectangle."""
    return tag(
        "rect",
        x=f"{x:.1f}",
        y=f"{y:.1f}",
        width=f"{width:.1f}",
        height=f"{height:.1f}",
        rx=f"{radius:.1f}" if radius else None,
        fill=fill,
        stroke=stroke,
        stroke_width=f"{stroke_width:.1f}" if stroke_width else None,
        opacity=f"{opacity:.3f}",
    )


def polygon(points: Sequence[tuple[float, float]], fill: str,
            stroke: str = INK, width: float = 2.0,
            opacity: float = 1.0, dash: str | None = None) -> str:
    """Return one SVG polygon."""
    point_text = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
    return tag(
        "polygon",
        points=point_text,
        fill=fill,
        stroke=stroke,
        stroke_width=f"{width:.1f}",
        opacity=f"{opacity:.3f}",
        stroke_dasharray=dash,
        stroke_linejoin="round",
    )


def polyline(points: Sequence[tuple[float, float]], stroke: str = INK,
             width: float = 2.0, dash: str | None = None,
             marker: str | None = None) -> str:
    """Return one unfilled SVG polyline."""
    point_text = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
    return tag(
        "polyline",
        points=point_text,
        fill="none",
        stroke=stroke,
        stroke_width=f"{width:.1f}",
        stroke_dasharray=dash,
        marker_end=f"url(#{marker})" if marker else None,
        stroke_linecap="round",
        stroke_linejoin="round",
    )


def figure_defs() -> str:
    """Return shared markers and hatch patterns."""
    return """
<defs>
  <marker id="arrow-blue" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto" markerUnits="strokeWidth"><path d="M0,0 L8,4 L0,8 Z" fill="#2563EB"/></marker>
  <marker id="arrow-gold" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto" markerUnits="strokeWidth"><path d="M0,0 L8,4 L0,8 Z" fill="#D97706"/></marker>
  <marker id="arrow-magenta" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto" markerUnits="strokeWidth"><path d="M0,0 L8,4 L0,8 Z" fill="#BE185D"/></marker>
  <marker id="arrow-teal" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto" markerUnits="strokeWidth"><path d="M0,0 L8,4 L0,8 Z" fill="#0F766E"/></marker>
  <pattern id="outside-hatch" width="12" height="12" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="12" stroke="#E2E8F0" stroke-width="5"/></pattern>
</defs>
""".strip()


def svg_document(height: int, title_value: str, description: str,
                 body: Iterable[str]) -> str:
    """Wrap figure elements in an accessible SVG document."""
    content = "\n".join(body)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {height}" role="img" aria-labelledby="title desc">
<title id="title">{html.escape(title_value)}</title>
<desc id="desc">{html.escape(description)}</desc>
{figure_defs()}
{rect(0, 0, WIDTH, height, PAPER)}
{content}
</svg>
"""


def rgb_hex(rgb: Sequence[float]) -> str:
    """Convert normalized RGB values to a CSS color."""
    values = [max(0, min(255, round(value * 255))) for value in rgb]
    return "#" + "".join(f"{value:02X}" for value in values)


def project_rgb(rgb: Sequence[float], origin: tuple[float, float],
                scale: float) -> tuple[float, float]:
    """Project RGB cube coordinates into an oblique explanatory view."""
    red, green, blue = rgb
    x = origin[0] + scale * (0.88 * red - 0.78 * green)
    y = origin[1] + scale * (-0.34 * red - 0.30 * green - 0.92 * blue)
    return x, y


def trilinear(vertices: Sequence[Sequence[float]], u: float, v: float,
              w: float) -> tuple[float, float, float]:
    """Return a convex trilinear mixture of eight ordered cube vertices."""
    weights = (
        (1 - u) * (1 - v) * (1 - w),
        u * (1 - v) * (1 - w),
        (1 - u) * v * (1 - w),
        (1 - u) * (1 - v) * w,
        u * v * (1 - w),
        u * (1 - v) * w,
        (1 - u) * v * w,
        u * v * w,
    )
    return tuple(
        sum(weights[index] * vertices[index][channel]
            for index in range(8))
        for channel in range(3)
    )


def convex_hull_3d() -> str:
    """Draw source RGB samples wrapped by their three-dimensional hull."""
    vertices = (
        (0.08, 0.10, 0.12),
        (0.86, 0.12, 0.12),
        (0.11, 0.80, 0.13),
        (0.09, 0.13, 0.80),
        (0.89, 0.82, 0.13),
        (0.87, 0.15, 0.80),
        (0.12, 0.83, 0.81),
        (0.90, 0.85, 0.81),
    )
    faces = (
        (0, 1, 4, 2),
        (3, 5, 7, 6),
        (0, 1, 5, 3),
        (2, 4, 7, 6),
        (0, 2, 6, 3),
        (1, 4, 7, 5),
    )
    edges = (
        (0, 1), (0, 2), (0, 3), (1, 4), (1, 5), (2, 4),
        (2, 6), (3, 5), (3, 6), (4, 7), (5, 7), (6, 7),
    )
    origin = (395.0, 590.0)
    scale = 345.0
    projected = [project_rgb(vertex, origin, scale) for vertex in vertices]
    rng = random.Random(0x51A3E1)
    samples = [
        (sum(vertex), vertex, project_rgb(vertex, origin, scale))
        for vertex in vertices
    ]
    for _ in range(180):
        u = 0.03 + rng.random() * 0.94
        v = 0.03 + rng.random() * 0.94
        w = 0.03 + rng.random() * 0.94
        value = trilinear(vertices, u, v, w)
        samples.append((sum(value), value, project_rgb(value, origin, scale)))

    body = [
        text(60, 62, "A color cloud has a three-dimensional boundary", 34, 700),
        text(60, 98,
             "Every dot is a sampled RGB color. The translucent shell joins the extreme samples.",
             20, fill=MUTED),
    ]
    for face in sorted(faces, key=lambda item: sum(sum(vertices[i]) for i in item)):
        body.append(polygon([projected[index] for index in face],
                            BLUE_LIGHT, BLUE, 2.0, 0.20))
    for _, value, point in sorted(samples, key=lambda item: item[0]):
        body.append(circle(point[0], point[1], 4.2, rgb_hex(value),
                           PAPER, 1.1, 0.92))
    for left, right in edges:
        body.append(line(*projected[left], *projected[right], BLUE, 2.4))

    axis_origin = project_rgb((0, 0, 0), origin, scale)
    axes = (
        ((1.08, 0, 0), "R", BLUE),
        ((0, 1.08, 0), "G", TEAL),
        ((0, 0, 1.08), "B", MAGENTA),
    )
    for endpoint, label, color in axes:
        point = project_rgb(endpoint, origin, scale)
        body.append(line(*axis_origin, *point, color, 3.0,
                         marker="arrow-blue" if color == BLUE else
                         "arrow-teal" if color == TEAL else "arrow-magenta"))
        body.append(text(point[0] + (12 if label == "R" else -14),
                         point[1] - 8, label, 24, 700, color))

    body.extend([
        rect(790, 155, 350, 382, PANEL, GRID, 1.5, 18),
        text(825, 205, "Convex hull", 27, 700, BLUE),
        text(825, 247, "The smallest convex shape", 20, 500),
        text(825, 278, "that contains every sample.", 20, 500),
        line(825, 306, 1100, 306, GRID, 1.5),
        text(825, 352, "Any spatial mixture is an average:", 19, fill=MUTED),
        text(825, 399, "p̄ = Σ wᵢ pᵢ", 31, 650, INK,
             family="Georgia, serif"),
        text(825, 438, "wᵢ ≥ 0    and    Σ wᵢ = 1", 19, fill=MUTED,
             family="ui-monospace, monospace"),
        text(825, 485, "So every possible average remains", 19, 500),
        text(825, 516, "inside the shell.", 19, 700),
        text(62, 670,
             "Schematic RGB geometry • points and faces are deterministic, not measurements from one image",
             16, fill=MUTED),
    ])
    return svg_document(
        710,
        "RGB sample cloud and its convex hull",
        "A three-dimensional RGB scatter plot sits inside a translucent convex polyhedron. A side note explains that weighted color averages stay inside the hull.",
        body,
    )


def map_plot(point: tuple[float, float], x: float, y: float,
             width: float, height: float) -> tuple[float, float]:
    """Map normalized data coordinates into an SVG plot rectangle."""
    return x + point[0] * width, y + (1.0 - point[1]) * height


def nearest_on_segment(point: tuple[float, float],
                       left: tuple[float, float],
                       right: tuple[float, float]) -> tuple[float, float]:
    """Return the closest point on one line segment."""
    dx = right[0] - left[0]
    dy = right[1] - left[1]
    denominator = dx * dx + dy * dy
    if denominator == 0.0:
        return left
    amount = ((point[0] - left[0]) * dx +
              (point[1] - left[1]) * dy) / denominator
    amount = max(0.0, min(1.0, amount))
    return left[0] + amount * dx, left[1] + amount * dy


def nearest_on_polygon(point: tuple[float, float],
                       polygon_points: Sequence[tuple[float, float]]) -> tuple[float, float]:
    """Return the closest point on a closed polygon boundary."""
    candidates = []
    for index, left in enumerate(polygon_points):
        right = polygon_points[(index + 1) % len(polygon_points)]
        candidate = nearest_on_segment(point, left, right)
        distance = math.hypot(point[0] - candidate[0],
                              point[1] - candidate[1])
        candidates.append((distance, candidate))
    return min(candidates, key=lambda item: item[0])[1]


def hull_projection() -> str:
    """Draw a 2D projection that exposes a source color outside the palette hull."""
    source_hull = (
        (0.05, 0.30), (0.20, 0.09), (0.78, 0.07), (0.96, 0.31),
        (0.92, 0.77), (0.68, 0.94), (0.35, 0.91), (0.08, 0.66),
    )
    palette_hull = (
        (0.18, 0.34), (0.33, 0.18), (0.69, 0.16), (0.82, 0.39),
        (0.73, 0.69), (0.49, 0.81), (0.22, 0.64),
    )
    target = (0.88, 0.72)
    nearest = nearest_on_polygon(target, palette_hull)
    plot_x, plot_y, plot_w, plot_h = 85.0, 155.0, 700.0, 450.0
    source_svg = [map_plot(point, plot_x, plot_y, plot_w, plot_h)
                  for point in source_hull]
    palette_svg = [map_plot(point, plot_x, plot_y, plot_w, plot_h)
                   for point in palette_hull]
    target_svg = map_plot(target, plot_x, plot_y, plot_w, plot_h)
    nearest_svg = map_plot(nearest, plot_x, plot_y, plot_w, plot_h)
    rng = random.Random(0xC0FEB0)

    body = [
        text(60, 60, "Projection turns a cover miss into a visible gap", 34, 700),
        text(60, 98,
             "This R–G view suppresses one dimension; the same inclusion rule holds in full RGB.",
             20, fill=MUTED),
        rect(plot_x, plot_y, plot_w, plot_h, PANEL, GRID, 1.5, 12),
    ]
    for amount in (0.25, 0.50, 0.75):
        body.append(line(plot_x + amount * plot_w, plot_y,
                         plot_x + amount * plot_w, plot_y + plot_h,
                         GRID, 1.0, "5 7"))
        body.append(line(plot_x, plot_y + amount * plot_h,
                         plot_x + plot_w, plot_y + amount * plot_h,
                         GRID, 1.0, "5 7"))
    body.append(polygon(source_svg, "none", MUTED, 2.2, 1.0, "9 7"))
    body.append(polygon(palette_svg, BLUE_LIGHT, BLUE, 3.0, 0.78))

    for _ in range(95):
        base = source_hull[rng.randrange(len(source_hull))]
        mix = source_hull[rng.randrange(len(source_hull))]
        amount = rng.random() * 0.62
        point = (
            0.52 + (base[0] - 0.52) * amount + (mix[0] - 0.52) * 0.15,
            0.50 + (base[1] - 0.50) * amount + (mix[1] - 0.50) * 0.15,
        )
        mapped = map_plot(point, plot_x, plot_y, plot_w, plot_h)
        body.append(circle(*mapped, 3.1, "#94A3B8", PAPER, 0.8, 0.72))
    for point in palette_hull:
        mapped = map_plot(point, plot_x, plot_y, plot_w, plot_h)
        body.append(circle(*mapped, 8.0, BLUE, PAPER, 2.0))

    body.extend([
        line(*target_svg, *nearest_svg, GOLD, 3.0, "7 6", "arrow-gold"),
        circle(*nearest_svg, 8.5, PAPER, GOLD, 3.0),
        polygon([
            (target_svg[0], target_svg[1] - 12),
            (target_svg[0] + 12, target_svg[1]),
            (target_svg[0], target_svg[1] + 12),
            (target_svg[0] - 12, target_svg[1]),
        ], GOLD, PAPER, 2.0),
        text(target_svg[0] + 18, target_svg[1] - 18,
             "source color", 18, 700, GOLD),
        text(nearest_svg[0] - 18, nearest_svg[1] + 34,
             "nearest reachable average", 17, 600, GOLD, "middle"),
        text(250, 190, "source-color hull", 18, 600, MUTED),
        text(300, 360, "palette mixture hull", 20, 700, BLUE),
        text(plot_x + plot_w / 2, plot_y + plot_h + 48,
             "R coordinate", 19, 600, MUTED, "middle"),
        text(plot_x - 48, plot_y + plot_h / 2,
             "G", 19, 700, MUTED, "middle"),
        rect(835, 157, 315, 448, PAPER, GRID, 1.5, 16),
        text(870, 207, "What the gap means", 25, 700),
        circle(883, 254, 8, BLUE, BLUE, 0),
        text(906, 261, "Inside blue:", 19, 700, BLUE),
        text(870, 293, "some weighted palette mix", 18, 500),
        text(870, 321, "can reach the color.", 18, 500),
        polygon([(883, 358), (892, 367), (883, 376), (874, 367)],
                GOLD, GOLD, 0),
        text(906, 374, "Outside blue:", 19, 700, GOLD),
        text(870, 406, "no dither kernel can remove", 18, 500),
        text(870, 434, "the mean color error.", 18, 500),
        line(870, 466, 1115, 466, GRID, 1.2),
        text(870, 506, "Cover repair adds or funds", 18, 500),
        text(870, 535, "an anchor near the missed", 18, 500),
        text(870, 564, "source-supported region.", 18, 700, TEAL),
        text(60, 675,
             "Schematic projection • the plotted geometry explains reachability, not a measured image gamut",
             16, fill=MUTED),
    ])
    return svg_document(
        715,
        "Two-dimensional projection of a palette cover miss",
        "A dashed source-color hull surrounds a smaller blue palette hull. An orange source color lies outside the palette hull, with an arrow to its nearest reachable average.",
        body,
    )


def rgb_cube_edges() -> tuple[tuple[int, int], ...]:
    """Return the twelve edges for binary RGB cube vertex order."""
    result = []
    for left in range(8):
        for bit in (1, 2, 4):
            right = left ^ bit
            if left < right:
                result.append((left, right))
    return tuple(result)


def rgb_corners() -> str:
    """Show that additive primaries and secondaries occupy RGB cube corners."""
    vertices = tuple(
        ((index & 1) != 0, (index & 2) != 0, (index & 4) != 0)
        for index in range(8)
    )
    vertices_float = tuple(tuple(float(channel) for channel in vertex)
                           for vertex in vertices)
    origin = (450.0, 640.0)
    scale = 300.0
    projected = [project_rgb(vertex, origin, scale)
                 for vertex in vertices_float]
    names = {
        0: "black",
        1: "red — primary",
        2: "green — primary",
        3: "yellow",
        4: "blue — primary",
        5: "magenta",
        6: "cyan",
        7: "white",
    }
    offsets = {
        0: (-48, 32), 1: (-22, 24), 2: (18, 36), 3: (28, 0),
        4: (-20, -20), 5: (-22, -8), 6: (-28, -12), 7: (25, 32),
    }
    body = [
        text(60, 60, "Pure RGB primaries are corners, not ordinary interior colors", 34, 700),
        text(60, 98,
             "A corner pins every channel to an extreme. There is no room to compensate farther outward.",
             20, fill=MUTED),
    ]
    faces = ((0, 1, 3, 2), (4, 5, 7, 6), (0, 2, 6, 4),
             (1, 3, 7, 5), (0, 1, 5, 4), (2, 3, 7, 6))
    for face in faces:
        body.append(polygon([projected[index] for index in face],
                            PANEL, GRID, 1.4, 0.45))
    for left, right in rgb_cube_edges():
        body.append(line(*projected[left], *projected[right], MUTED, 2.0))
    for index, point in enumerate(projected):
        color = rgb_hex(vertices_float[index])
        body.append(circle(*point, 14, color, INK, 2.2))
        dx, dy = offsets[index]
        body.append(line(point[0], point[1], point[0] + dx * 0.72,
                         point[1] + dy * 0.72, MUTED, 1.2))
        body.append(text(point[0] + dx, point[1] + dy,
                         names[index], 17, 700 if "primary" in names[index]
                         else 500, color if index in (1, 2, 4) else INK,
                         "end" if dx < 0 else "start"))

    body.extend([
        rect(800, 170, 335, 424, PANEL, GRID, 1.5, 18),
        text(837, 220, "Why a corner miss is severe", 24, 700),
        text(837, 268, "Pure red is (1, 0, 0).", 20, 650, "#C81E1E"),
        text(837, 306, "If the nearest palette red is", 18, 500),
        text(837, 334, "duller, its residual asks for:", 18, 500),
        text(837, 382, "more R, less G, less B", 22, 700, MAGENTA),
        text(837, 426, "All three directions point", 18, 500),
        text(837, 454, "outside the valid RGB cube.", 18, 700),
        line(837, 482, 1098, 482, GRID, 1.2),
        text(837, 523, "Clamping removes that signal", 18, 500),
        text(837, 551, "before lookup can choose a", 18, 500),
        text(837, 579, "compensating palette entry.", 18, 700),
        text(60, 682,
             "RGB cube schematic • coordinates are normalized to 0…1",
             16, fill=MUTED),
    ])
    return svg_document(
        720,
        "RGB primaries at cube corners",
        "An oblique RGB cube labels red, green, and blue as corners. A side explanation shows why quantization residuals at pure red point outside all three channel bounds.",
        body,
    )


def boundary_panel(body: list[str], x: float, y: float, width: float,
                   height: float, corner: bool) -> None:
    """Append one local gamut-boundary clamping diagram."""
    plot_x = x + 54
    plot_y = y + 98
    plot_w = width - 112
    plot_h = height - 172
    body.append(rect(x, y, width, height, PANEL, GRID, 1.5, 16))
    body.append(text(x + 30, y + 48,
                     "Corner: three channels pinned" if corner else
                     "Face: one channel pinned",
                     24, 700))
    body.append(rect(plot_x, plot_y, plot_w, plot_h,
                     PAPER, INK, 1.8, 0))
    body.append(rect(plot_x + plot_w, plot_y, 66, plot_h,
                     "url(#outside-hatch)", "none"))
    body.append(text(plot_x + plot_w + 34, plot_y - 12,
                     "outside", 15, 600, MUTED, "middle"))
    body.append(text(plot_x + plot_w, plot_y + plot_h + 36,
                     "channel maximum", 16, 600, MUTED, "middle"))
    body.append(line(plot_x, plot_y + plot_h, plot_x + plot_w + 60,
                     plot_y + plot_h, INK, 2.0, marker="arrow-blue"))
    body.append(line(plot_x, plot_y + plot_h, plot_x,
                     plot_y - 28, INK, 2.0, marker="arrow-blue"))

    if corner:
        source = (plot_x + plot_w, plot_y + plot_h)
        palette = (plot_x + plot_w * 0.74, plot_y + plot_h * 0.78)
        raw = (plot_x + plot_w + 52, plot_y + plot_h + 42)
        clamped = source
        body.append(circle(*palette, 10, BLUE, PAPER, 2.0))
        body.append(text(palette[0] - 12, palette[1] - 18,
                         "chosen palette color", 16, 600, BLUE, "end"))
        body.append(polygon([
            (source[0], source[1] - 12),
            (source[0] + 12, source[1]),
            (source[0], source[1] + 12),
            (source[0] - 12, source[1]),
        ], GOLD, PAPER, 2.0))
        body.append(text(source[0] - 18, source[1] - 26,
                         "pure primary", 17, 700, GOLD, "end"))
        body.append(line(*palette, *raw, MAGENTA, 3.0, "8 6",
                         "arrow-magenta"))
        body.append(line(*raw, *clamped, TEAL, 3.0, "5 5",
                         "arrow-teal"))
        body.append(text(plot_x + 42, plot_y + 37,
                         "residual asks for more of the", 16, 500, MAGENTA))
        body.append(text(plot_x + 42, plot_y + 61,
                         "maximum and less of the minima", 16, 700, MAGENTA))
        body.append(text(plot_x + plot_w - 72,
                         plot_y + plot_h - 10,
                         "clamps back to the same corner", 16, 700,
                         TEAL, "end"))
    else:
        source = (plot_x + plot_w, plot_y + plot_h * 0.42)
        palette = (plot_x + plot_w * 0.74, plot_y + plot_h * 0.55)
        raw = (plot_x + plot_w + 50, plot_y + plot_h * 0.29)
        clamped = (plot_x + plot_w, raw[1])
        body.append(circle(*palette, 10, BLUE, PAPER, 2.0))
        body.append(text(palette[0] - 12, palette[1] + 30,
                         "chosen palette color", 16, 600, BLUE, "end"))
        body.append(polygon([
            (source[0], source[1] - 12),
            (source[0] + 12, source[1]),
            (source[0], source[1] + 12),
            (source[0] - 12, source[1]),
        ], GOLD, PAPER, 2.0))
        body.append(text(source[0] - 14, source[1] + 38,
                         "source on face", 17, 700, GOLD, "end"))
        body.append(line(*palette, *raw, MAGENTA, 3.0, "8 6",
                         "arrow-magenta"))
        body.append(circle(*raw, 7, PAPER, MAGENTA, 2.5))
        body.append(line(*raw, *clamped, TEAL, 3.0, "5 5",
                         "arrow-teal"))
        body.append(circle(*clamped, 8, PAPER, TEAL, 3.0))
        body.append(text(raw[0] - 10, raw[1] - 24,
                         "raw corrected query", 16, 600, MAGENTA, "end"))
        body.append(text(clamped[0] - 12, clamped[1] + 35,
                         "after clamp", 16, 700, TEAL, "end"))
        body.append(text(plot_x + 35, plot_y + 38,
                         "Tangential error survives; the", 16, 500))
        body.append(text(plot_x + 35, plot_y + 62,
                         "outward channel component does not.", 16, 700))


def boundary_clamping() -> str:
    """Draw local slices for face and corner error clamping."""
    body = [
        text(60, 60, "Clamping makes faces fail as well as corners", 34, 700),
        text(60, 98,
             "Each panel is a local 2D slice. Dashed magenta is the desired error correction; green is what remains after RGB clamping.",
             19, fill=MUTED),
    ]
    boundary_panel(body, 60, 135, 520, 500, False)
    boundary_panel(body, 620, 135, 520, 500, True)
    body.extend([
        rect(175, 662, 850, 58, MAGENTA_LIGHT, "none", radius=12),
        text(600, 699,
             "A face needs a nearby partner that shares its pinned channel; distant corners are often never selected.",
             19, 700, MAGENTA, "middle"),
        text(60, 755,
             "Schematic local sections • arrows explain the sign of the residual, not a particular diffusion kernel",
             16, fill=MUTED),
    ])
    return svg_document(
        790,
        "Error clamping at RGB faces and corners",
        "Two local gamut slices show a corrected lookup query crossing an RGB face and a corner. Magenta outward residuals are projected back to the boundary by clamping.",
        body,
    )


def reachability_limits() -> str:
    """Explain why hull containment alone does not ensure a realized dither mix."""
    body = [
        text(60, 58, "Hull inclusion is necessary—but the algorithm must realize the mix", 33, 700),
        text(60, 95,
             "Geometry describes what is possible. Lookup, diffusion state, clipping, and finite regions determine what is actually visited.",
             19, fill=MUTED),
    ]
    panel_y = (135, 420, 705)
    titles = (
        "1  Geometry permits a mixture",
        "2  Lookup may visit only a subset",
        "3  Diffused error may leave a small region",
    )
    for index, y in enumerate(panel_y):
        body.append(rect(60, y, 1080, 238, PANEL, GRID, 1.5, 16))
        body.append(text(90, y + 43, titles[index], 23, 700))

    triangle = ((155, 328), (430, 328), (300, 190))
    target = (308, 274)
    body.extend([
        polygon(triangle, BLUE_LIGHT, BLUE, 3.0, 0.88),
        circle(*triangle[0], 11, "#334155", PAPER, 2.0),
        circle(*triangle[1], 11, "#F59E0B", PAPER, 2.0),
        circle(*triangle[2], 11, "#38BDF8", PAPER, 2.0),
        line(*target, *triangle[0], MUTED, 1.5, "5 5"),
        line(*target, *triangle[1], MUTED, 1.5, "5 5"),
        line(*target, *triangle[2], MUTED, 1.5, "5 5"),
        polygon([(target[0], target[1] - 10),
                 (target[0] + 10, target[1]),
                 (target[0], target[1] + 10),
                 (target[0] - 10, target[1])], GOLD, PAPER, 1.5),
        text(486, 212, "The target lies inside hull(P₁, P₂, P₃).", 20, 600),
        text(486, 252, "Some non-negative weights can reproduce it:", 18, 500),
        text(486, 301, "target = w₁P₁ + w₂P₂ + w₃P₃", 24, 700, BLUE,
             family="Georgia, serif"),
        text(486, 335, "This is a possibility statement, not an execution trace.",
             17, 500, MUTED),
    ])

    edge_left = (155, 612)
    edge_right = (430, 612)
    unvisited = (300, 500)
    target_two = (308, 556)
    body.extend([
        polygon((edge_left, edge_right, unvisited), "none", GRID, 2.0,
                1.0, "7 6"),
        line(*edge_left, *edge_right, BLUE, 8.0),
        circle(*edge_left, 11, "#334155", PAPER, 2.0),
        circle(*edge_right, 11, "#F59E0B", PAPER, 2.0),
        circle(*unvisited, 11, PAPER, GRID, 3.0),
        polygon([(target_two[0], target_two[1] - 10),
                 (target_two[0] + 10, target_two[1]),
                 (target_two[0], target_two[1] + 10),
                 (target_two[0] - 10, target_two[1])], GOLD, PAPER, 1.5),
        text(486, 490, "Approximate lookup, cache thresholds, or a", 18, 500),
        text(486, 521, "clamped query can keep P₃ out of the trace.", 18, 700),
        text(486, 568, "Then the realized hull is only hull(P₁, P₂).", 20, 700,
             BLUE),
        text(486, 607, "The original target is outside that smaller set.", 18, 500),
    ])

    grid_x, grid_y, cell = 132, 768, 23
    for row in range(6):
        for column in range(12):
            in_region = 3 <= column <= 8 and 1 <= row <= 4
            fill = "#DF493F" if in_region else "#E2E8F0"
            body.append(rect(grid_x + column * cell,
                             grid_y + row * cell,
                             cell - 2, cell - 2, fill, PAPER, 0.8, 2))
    for row, offset in ((1, 0), (2, 14), (3, -7), (4, 7)):
        start_x = grid_x + 8.5 * cell
        start_y = grid_y + (row + 0.5) * cell
        body.append(line(start_x, start_y,
                         start_x + 83, start_y + offset,
                         MAGENTA, 2.2, "5 4", "arrow-magenta"))
    body.extend([
        text(270, 928, "small saturated region", 16, 700, GOLD, "middle"),
        text(486, 775, "Error diffusion is spatial transport.", 20, 700),
        text(486, 815, "A finite patch may export its residual to neighbors", 18, 500),
        text(486, 846, "or the image boundary before its own average converges.", 18, 500),
        text(486, 893, "Cover repair improves the available palette geometry;", 18, 500),
        text(486, 924, "it cannot promise every lookup/dither trajectory.", 18, 700,
             MAGENTA),
        text(60, 990,
             "Schematic execution model • actual trajectories depend on lookup policy, diffusion policy, scan order, and region extent",
             16, fill=MUTED),
    ])
    return svg_document(
        1025,
        "Why convex-hull inclusion does not guarantee dither convergence",
        "Three stacked panels distinguish a target inside the full palette hull, a lookup trace that visits only a palette subset, and diffusion error leaving a small saturated region.",
        body,
    )


def flicker_keyframes() -> str:
    """Draw four static animation key frames for reduced-motion readers."""
    colors = ("#D94732", "#F12F67", "#C85B24", "#E32652")
    body = [
        text(60, 58, "A stable source color can flicker when its palette neighborhood moves", 32, 700),
        text(60, 95,
             "Four schematic frames from a cross-dissolve. The red badge is constant in the source.",
             19, fill=MUTED),
    ]
    for index, color in enumerate(colors):
        x = 70 + index * 280
        body.append(rect(x, 135, 250, 440, PANEL, GRID, 1.5, 16))
        body.append(text(x + 125, 174, f"frame {index + 1}", 19, 700,
                         MUTED, "middle"))
        body.append(text(x + 28, 215, "cover off", 17, 700, MAGENTA))
        body.append(circle(x + 82, 283, 42, color, INK, 2.0))
        body.append(text(x + 143, 278, color, 16, 600, color,
                         family="ui-monospace, monospace"))
        body.append(text(x + 143, 304, "changes", 16, 500, MUTED))
        body.append(line(x + 28, 342, x + 222, 342, GRID, 1.2))
        body.append(text(x + 28, 383, "cover protected", 17, 700, TEAL))
        body.append(circle(x + 82, 451, 42, "#FF0000", INK, 2.0))
        body.append(text(x + 143, 446, "#FF0000", 16, 600, TEAL,
                         family="ui-monospace, monospace"))
        body.append(text(x + 143, 472, "stays", 16, 500, MUTED))
        swatches = ("#334155", "#38BDF8", "#FBBF24", color)
        for swatch_index, swatch in enumerate(swatches):
            body.append(rect(x + 30 + swatch_index * 46, 516, 36, 22,
                             swatch, PAPER, 1.0, 3))
    body.extend([
        rect(175, 605, 850, 58, GOLD_LIGHT, "none", radius=12),
        text(600, 642,
             "A moving nearest color is especially visible on a small, high-saturation region.",
             19, 700, GOLD, "middle"),
        text(60, 704,
             "Schematic key frames • color values illustrate the mechanism and are not captured encoder output",
             16, fill=MUTED),
    ])
    return svg_document(
        738,
        "Static key frames for palette coverage flicker",
        "Four time steps compare an uncovered red badge whose displayed color changes with a cover-protected badge that stays pure red.",
        body,
    )


FONT_5X7 = {
    " ": ("00000",) * 7,
    "-": ("00000", "00000", "00000", "11111", "00000", "00000", "00000"),
    "/": ("00001", "00010", "00100", "01000", "10000", "00000", "00000"),
    ":": ("00000", "00100", "00100", "00000", "00100", "00100", "00000"),
    "0": ("01110", "10001", "10011", "10101", "11001", "10001", "01110"),
    "1": ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
    "2": ("01110", "10001", "00001", "00010", "00100", "01000", "11111"),
    "3": ("11110", "00001", "00001", "01110", "00001", "00001", "11110"),
    "4": ("00010", "00110", "01010", "10010", "11111", "00010", "00010"),
    "5": ("11111", "10000", "10000", "11110", "00001", "00001", "11110"),
    "6": ("01110", "10000", "10000", "11110", "10001", "10001", "01110"),
    "7": ("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
    "8": ("01110", "10001", "10001", "01110", "10001", "10001", "01110"),
    "9": ("01110", "10001", "10001", "01111", "00001", "00001", "01110"),
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "B": ("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
    "C": ("01111", "10000", "10000", "10000", "10000", "10000", "01111"),
    "D": ("11110", "10001", "10001", "10001", "10001", "10001", "11110"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "F": ("11111", "10000", "10000", "11110", "10000", "10000", "10000"),
    "G": ("01111", "10000", "10000", "10111", "10001", "10001", "01111"),
    "H": ("10001", "10001", "10001", "11111", "10001", "10001", "10001"),
    "I": ("01110", "00100", "00100", "00100", "00100", "00100", "01110"),
    "J": ("00111", "00010", "00010", "00010", "00010", "10010", "01100"),
    "K": ("10001", "10010", "10100", "11000", "10100", "10010", "10001"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "M": ("10001", "11011", "10101", "10101", "10001", "10001", "10001"),
    "N": ("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "P": ("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
    "Q": ("01110", "10001", "10001", "10001", "10101", "10010", "01101"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "U": ("10001", "10001", "10001", "10001", "10001", "10001", "01110"),
    "V": ("10001", "10001", "10001", "10001", "10001", "01010", "00100"),
    "W": ("10001", "10001", "10001", "10101", "10101", "10101", "01010"),
    "X": ("10001", "10001", "01010", "00100", "01010", "10001", "10001"),
    "Y": ("10001", "10001", "01010", "00100", "00100", "00100", "00100"),
    "Z": ("11111", "00001", "00010", "00100", "01000", "10000", "11111"),
}


class Raster:
    """Minimal RGBA drawing surface used to keep APNG generation dependency-free."""

    def __init__(self, width: int, height: int, background: tuple[int, int, int]):
        self.width = width
        self.height = height
        self.pixels = bytearray(background + (255,)) * (width * height)

    def set_pixel(self, x: int, y: int, color: tuple[int, int, int],
                  coverage: float = 1.0) -> None:
        """Blend one opaque RGB color into a pixel."""
        if x < 0 or y < 0 or x >= self.width or y >= self.height:
            return
        offset = (y * self.width + x) * 4
        amount = max(0.0, min(1.0, coverage))
        for channel in range(3):
            old = self.pixels[offset + channel]
            self.pixels[offset + channel] = round(
                old * (1.0 - amount) + color[channel] * amount
            )

    def rectangle(self, x: int, y: int, width: int, height: int,
                  color: tuple[int, int, int]) -> None:
        """Fill one axis-aligned rectangle."""
        left = max(0, x)
        top = max(0, y)
        right = min(self.width, x + width)
        bottom = min(self.height, y + height)
        pixel = bytes(color + (255,))
        row = pixel * max(0, right - left)
        for current_y in range(top, bottom):
            start = (current_y * self.width + left) * 4
            self.pixels[start:start + len(row)] = row

    def circle(self, cx: float, cy: float, radius: float,
               color: tuple[int, int, int]) -> None:
        """Draw an antialiased filled circle."""
        left = max(0, math.floor(cx - radius - 1))
        right = min(self.width, math.ceil(cx + radius + 1))
        top = max(0, math.floor(cy - radius - 1))
        bottom = min(self.height, math.ceil(cy + radius + 1))
        for y in range(top, bottom):
            for x in range(left, right):
                distance = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                coverage = max(0.0, min(1.0, radius + 0.5 - distance))
                if coverage > 0.0:
                    self.set_pixel(x, y, color, coverage)

    def label(self, x: int, y: int, value: str,
              color: tuple[int, int, int], scale: int = 2) -> None:
        """Draw uppercase text with the embedded five-by-seven bitmap font."""
        cursor = x
        for character in value.upper():
            glyph = FONT_5X7.get(character, FONT_5X7[" "])
            for row, pattern in enumerate(glyph):
                for column, bit in enumerate(pattern):
                    if bit == "1":
                        self.rectangle(cursor + column * scale,
                                       y + row * scale,
                                       scale, scale, color)
            cursor += 6 * scale


def mix_rgb(left: Sequence[int], right: Sequence[int],
            amount: float) -> tuple[int, int, int]:
    """Return a channelwise RGB interpolation."""
    return tuple(round(left[index] * (1.0 - amount) +
                       right[index] * amount) for index in range(3))


def draw_flicker_panel(raster: Raster, x: int, label_value: str,
                       left_background: tuple[int, int, int],
                       right_background: tuple[int, int, int],
                       badge: tuple[int, int, int],
                       footer: str, footer_color: tuple[int, int, int]) -> None:
    """Draw one panel of the schematic palette-flicker animation."""
    raster.rectangle(x, 64, 214, 220, (248, 250, 252))
    raster.rectangle(x + 6, 92, 101, 132, left_background)
    raster.rectangle(x + 107, 92, 101, 132, right_background)
    raster.label(x + 18, 73, label_value, (23, 32, 51), 2)
    raster.circle(x + 107, 158, 42, (23, 32, 51))
    raster.circle(x + 107, 158, 38, badge)
    raster.label(x + 18, 235, footer, footer_color, 1)
    swatches = (left_background, right_background, badge, (51, 65, 85))
    for index, swatch in enumerate(swatches):
        raster.rectangle(x + 18 + index * 45, 254, 34, 17, swatch)


def flicker_frames() -> list[bytes]:
    """Return deterministic RGBA frames for the cover-flicker APNG."""
    width, height = 720, 320
    displayed = (
        (217, 71, 50), (239, 47, 103), (200, 91, 36), (227, 38, 82),
        (205, 73, 41), (244, 55, 88),
    )
    frames = []
    for index in range(24):
        phase = index / 23.0
        pulse = 0.5 + 0.5 * math.sin(index * math.pi / 6.0)
        left_background = mix_rgb((42, 93, 151), (29, 135, 101), phase)
        right_background = mix_rgb((233, 183, 67), (123, 75, 156), phase)
        raster = Raster(width, height, (23, 32, 51))
        raster.label(28, 18, "PALETTE COVER AND TEMPORAL FLICKER",
                     (248, 250, 252), 2)
        raster.label(610, 20, f"{index + 1:02d}/24", (203, 213, 225), 1)
        draw_flicker_panel(raster, 12, "SOURCE",
                           left_background, right_background,
                           (255, 0, 0), "SOURCE RED STAYS", (37, 99, 235))
        draw_flicker_panel(raster, 253, "COVER OFF",
                           left_background, right_background,
                           displayed[index % len(displayed)],
                           "NEAREST RED MOVES", (190, 24, 93))
        draw_flicker_panel(raster, 494, "COVER ON",
                           left_background, right_background,
                           (255, 0, 0), "RED ANCHOR STAYS", (15, 118, 110))
        bar_width = round(190 * phase)
        raster.rectangle(265, 294, 190, 7, (71, 85, 105))
        raster.rectangle(265, 294, bar_width, 7,
                         mix_rgb((37, 99, 235), (217, 119, 6), pulse))
        raster.label(12, 296, "SCHEMATIC", (148, 163, 184), 1)
        raster.label(494, 296, "SOURCE RED IS CONSTANT", (148, 163, 184), 1)
        frames.append(bytes(raster.pixels))
    return frames


def png_chunk(name: bytes, payload: bytes) -> bytes:
    """Return one PNG chunk with its CRC."""
    checksum = binascii.crc32(name)
    checksum = binascii.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + name + payload + struct.pack(">I", checksum)


def filtered_rgba(frame: bytes, width: int, height: int) -> bytes:
    """Prefix each RGBA row with PNG filter type zero."""
    stride = width * 4
    rows = []
    for y in range(height):
        start = y * stride
        rows.append(b"\x00" + frame[start:start + stride])
    return b"".join(rows)


def encode_apng(frames: Sequence[bytes], width: int, height: int,
                delay_numerator: int = 1,
                delay_denominator: int = 8) -> bytes:
    """Encode full-frame RGBA images as a looping APNG."""
    output = bytearray(b"\x89PNG\r\n\x1a\n")
    output.extend(png_chunk(b"IHDR", struct.pack(
        ">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
    output.extend(png_chunk(b"acTL", struct.pack(">II", len(frames), 0)))
    sequence = 0
    for index, frame in enumerate(frames):
        frame_control = struct.pack(
            ">IIIIIHHBB", sequence, width, height, 0, 0,
            delay_numerator, delay_denominator, 0, 0)
        output.extend(png_chunk(b"fcTL", frame_control))
        sequence += 1
        compressed = zlib.compress(filtered_rgba(frame, width, height), 9)
        if index == 0:
            output.extend(png_chunk(b"IDAT", compressed))
        else:
            output.extend(png_chunk(b"fdAT", struct.pack(">I", sequence) +
                                    compressed))
            sequence += 1
    output.extend(png_chunk(b"IEND", b""))
    return bytes(output)


def manifest() -> dict[str, object]:
    """Return provenance for every generated explanatory asset."""
    return {
        "schema": 1,
        "generator": "tools/plot_cover_policy_figures.py",
        "kind": "deterministic schematic",
        "measurement": False,
        "assets": [
            {"path": "cover-convex-hull-3d.svg", "role": "3D sample hull"},
            {"path": "cover-hull-projection.svg", "role": "2D cover miss"},
            {"path": "cover-rgb-corners.svg", "role": "RGB corner geometry"},
            {"path": "cover-boundary-clamping.svg", "role": "face and corner clamping"},
            {"path": "cover-reachability-limits.svg", "role": "lookup and dither limits"},
            {"path": "cover-flicker-keyframes.svg", "role": "static animation fallback"},
            {"path": "cover-flicker.png", "role": "24-frame looping APNG"},
        ],
        "animation": {
            "frames": 24,
            "delay": "1/8 second",
            "loop_count": 0,
        },
        "random_seeds": {
            "convex_hull_3d": "0x51A3E1",
            "hull_projection": "0xC0FEB0",
        },
    }


def generate(output_dir: Path) -> None:
    """Generate every figure and its provenance manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    figures = {
        "cover-convex-hull-3d.svg": convex_hull_3d(),
        "cover-hull-projection.svg": hull_projection(),
        "cover-rgb-corners.svg": rgb_corners(),
        "cover-boundary-clamping.svg": boundary_clamping(),
        "cover-reachability-limits.svg": reachability_limits(),
        "cover-flicker-keyframes.svg": flicker_keyframes(),
    }
    for name, content in figures.items():
        (output_dir / name).write_text(content, encoding="utf-8")
    (output_dir / "cover-flicker.png").write_bytes(
        encode_apng(flicker_frames(), 720, 320)
    )
    (output_dir / "cover-policy-figures.json").write_text(
        json.dumps(manifest(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def check(output_dir: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(prefix="cover-policy-figures-") as temp:
        generated = Path(temp)
        generate(generated)
        mismatches = []
        for name in ASSET_NAMES:
            expected = output_dir / name
            actual = generated / name
            if not expected.is_file() or expected.read_bytes() != actual.read_bytes():
                mismatches.append(name)
        if mismatches:
            joined = ", ".join(mismatches)
            raise SystemExit(
                "cover-policy figures are stale or missing: " + joined
            )


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=source_root / "docs/functionality/cover-policy-figures",
        help="directory for generated SVG, APNG, and manifest files",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare committed assets with a fresh deterministic generation",
    )
    return parser.parse_args()


def main() -> None:
    """Generate or verify the cover-policy explanatory assets."""
    arguments = parse_args()
    if arguments.check:
        check(arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
