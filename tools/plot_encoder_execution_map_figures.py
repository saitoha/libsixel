#!/usr/bin/env python3
"""Generate deterministic encoder execution-map figures."""

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
    defs,
    path,
    rect,
    svg_document,
    text,
    text_lines,
)


RED = "#B42318"
RED_LIGHT = "#FEE4E2"
PURPLE = "#6941C6"
PURPLE_LIGHT = "#F4EBFF"
SLATE_LIGHT = "#EEF2F6"

ASSET_NAMES = (
    "encoder-scheduler-wide.svg",
    "encoder-scheduler-mobile.svg",
    "frame-preparation-wide.svg",
    "frame-preparation-mobile.svg",
    "palette-build-wide.svg",
    "palette-build-mobile.svg",
    "palette-apply-output-wide.svg",
    "palette-apply-output-mobile.svg",
    "encoder-execution-map-figures.json",
)


def arrow(data: str, color: str = BLUE, width: float = 4.0,
          dash: str | None = None) -> str:
    """Return a routed arrow using the shared context-stroke marker."""
    return path(data, color, width, "arrow-blue", dash)


def box(x: float, y: float, width: float, height: float,
        title_value: str, details: Sequence[str], accent: str = BLUE,
        fill: str = PAPER, title_size: int = 21,
        detail_size: int = 15) -> str:
    """Return a labeled process card."""
    body = [rect(x, y, width, height, fill, accent, 2.0, 15.0,
                 "card-shadow")]
    body.append(rect(x, y, 7, height, accent, radius=3.5))
    body.append(text(x + 22, y + 34, title_value,
                     title_size, 760, accent))
    body.append(text_lines(x + 22, y + 65, details,
                           detail_size, detail_size + 7, 450, MUTED))
    return "".join(body)


def panel(x: float, y: float, width: float, height: float,
          title_value: str, accent: str, fill: str = PAPER) -> str:
    """Return a quiet containing panel with a section label."""
    body = [rect(x, y, width, height, fill, GRID, 1.5, 20.0)]
    body.append(rect(x + 18, y + 18, 8, 24, accent, radius=4.0))
    body.append(text(x + 38, y + 39, title_value, 20, 760, INK))
    return "".join(body)


def pill(x: float, y: float, width: float, label: str,
         accent: str, fill: str) -> str:
    """Return a compact policy or artifact label."""
    body = [rect(x, y, width, 38, fill, accent, 1.5, 19.0)]
    body.append(text(x + width / 2, y + 25, label,
                     14, 700, accent, "middle"))
    return "".join(body)


def header(width: int, title_value: str, subtitle: str,
           view_label: str) -> list[str]:
    """Return the shared figure heading."""
    body = [rect(0, 0, width, 5000, PANEL)]
    body.append(text(42, 60, title_value, 40, 820, INK))
    body.append(text(42, 92, subtitle, 19, 550, MUTED))
    body.append(text(width - 42, 60, view_label,
                     15, 760, BLUE, "end"))
    return body


def scheduler_wide() -> str:
    """Return the landscape macro-DAG and concurrency map."""
    width = 1760
    height = 990
    body = header(width,
                  "The per-frame encoder scheduler",
                  "Six caller-thread nodes; one narrow palette overlap",
                  "IMPLEMENTATION MAP 1 / 4")

    body.append(panel(35, 125, 1690, 185,
                      "Planning happens before the six-node scheduler", BLUE))
    planning = (
        (70, "analyze", ("frame work", "and formats")),
        (330, "resolve sampling", ("full-frame or", "adaptive-grid")),
        (650, "schedule", ("thread budget", "and pipelines")),
        (940, "replan(palette_ready)", ("rebuild planner", "description")),
        (1295, "planner graph", ("preplan order and", "pipeline annotation")),
    )
    for x, title_value, detail in planning:
        body.append(box(x, 180, 215 if x != 940 else 285, 100,
                        title_value, detail, BLUE, BLUE_LIGHT, 18, 13))
    for x1, x2 in ((285, 330), (545, 650), (865, 940), (1225, 1295)):
        body.append(arrow(f"M {x1} 230 H {x2}", BLUE, 3.5))
    body.append(text(1675, 235, "not a runtime reschedule",
                     15, 700, RED, "end"))

    body.append(panel(35, 335, 1690, 455,
                      "Actual six-node scheduler: ready nodes are called synchronously",
                      TEAL))
    stages = (
        (65,  "LOAD", ("frame already", "loaded"), SLATE_LIGHT, MUTED),
        (325, "PALETTE LAUNCH", ("sample loaded frame", "before mutation"),
         MAGENTA_LIGHT, MAGENTA),
        (635, "PREPLAN", ("clip / convert /", "resize / convert"),
         BLUE_LIGHT, BLUE),
        (945, "PALETTE COLLECT", ("join worker or", "sync fallback"),
         MAGENTA_LIGHT, MAGENTA),
        (1265, "DITHER PLAN", ("configure gradient,", "diffusion, policies"),
         TEAL_LIGHT, TEAL),
        (1500, "OUTPUT", ("apply palette and", "encode SIXEL"),
         GOLD_LIGHT, GOLD),
    )
    widths = (205, 250, 250, 260, 190, 190)
    for (stage, stage_width) in zip(stages, widths):
        x, title_value, detail, fill, accent = stage
        body.append(box(x, 520, stage_width, 135,
                        title_value, detail, accent, fill, 17, 14))
    for x1, x2 in ((270, 325), (575, 635), (885, 945),
                   (1205, 1265), (1455, 1500)):
        body.append(arrow(f"M {x1} 588 H {x2}", TEAL, 4.0))
    body.append(pill(665, 690, 190, "caller thread", TEAL, TEAL_LIGHT))
    body.append(text(65, 754,
                     "The node runner does not execute independent nodes in parallel.",
                     16, 620, MUTED))

    body.append(box(375, 380, 475, 100,
                    "palette worker (conditional)",
                    ("adaptive sample is already complete",
                     "palette build may overlap PREPLAN"),
                    MAGENTA, MAGENTA_LIGHT, 18, 14))
    body.append(arrow("M 450 520 V 485 Q 450 465 470 465 H 470",
                      MAGENTA, 4.0, "8 6"))
    body.append(arrow("M 850 430 H 1025 Q 1040 430 1040 445 V 520",
                      MAGENTA, 4.0, "8 6"))
    body.append(pill(865, 395, 150, "join / wait", MAGENTA,
                     MAGENTA_LIGHT))

    body.append(panel(35, 820, 1690, 125,
                      "Separate concurrency domains", PURPLE, PURPLE_LIGHT))
    body.append(pill(80, 875, 430,
                     "multi-frame loader -> encoder handoff",
                     PURPLE, PURPLE_LIGHT))
    body.append(pill(585, 875, 520,
                     "palette-apply bands -> SIXEL band workers",
                     PURPLE, PURPLE_LIGHT))
    body.append(pill(1180, 875, 490,
                     "writer flushes completed bands in order",
                     PURPLE, PURPLE_LIGHT))

    return svg_document(
        width,
        height,
        "The per-frame encoder scheduler",
        "The six macro scheduler nodes run on the caller thread. Only a conditional palette worker overlaps palette construction with preprocessing. Replan rebuilds the planner description before scheduling and is not a runtime rescheduler. Loader handoff and palette-apply-to-encode bands are separate concurrency domains.",
        "\n".join(body),
    )


def scheduler_mobile() -> str:
    """Return the portrait macro-DAG and concurrency map."""
    width = 760
    height = 1970
    body = header(width,
                  "Encoder scheduler",
                  "Actual async boundary and replan meaning",
                  "MAP 1 / 4")
    body.append(panel(30, 120, 700, 430, "Planning before execution", BLUE))
    plan_stages = (
        (175, "analyze", "frame work + formats"),
        (255, "resolve sampling", "full-frame or adaptive-grid"),
        (335, "schedule", "thread budget + pipelines"),
        (415, "replan(palette_ready)", "rebuild planner description"),
    )
    for y, title_value, detail in plan_stages:
        body.append(box(75, y, 610, 58, title_value, (detail,),
                        BLUE, BLUE_LIGHT, 17, 13))
        if y != 415:
            body.append(arrow(f"M 380 {y + 58} V {y + 77}", BLUE, 3.0))
    body.append(pill(185, 492, 390, "not a runtime reschedule", RED,
                     RED_LIGHT))

    body.append(panel(30, 585, 700, 1110,
                      "Six caller-thread nodes", TEAL))
    stages = (
        (650, "LOAD", "frame already loaded", MUTED, SLATE_LIGHT),
        (805, "PALETTE LAUNCH", "sample before frame mutation",
         MAGENTA, MAGENTA_LIGHT),
        (1040, "PREPLAN", "clip / convert / resize / convert",
         BLUE, BLUE_LIGHT),
        (1245, "PALETTE COLLECT", "join worker or sync fallback",
         MAGENTA, MAGENTA_LIGHT),
        (1400, "DITHER PLAN", "configure policies; no pixel loop yet",
         TEAL, TEAL_LIGHT),
        (1555, "OUTPUT", "apply palette + encode SIXEL",
         GOLD, GOLD_LIGHT),
    )
    for y, title_value, detail, accent, fill in stages:
        body.append(box(75, y, 610, 105, title_value, (detail,),
                        accent, fill, 18, 14))
    for y1, y2 in ((755, 805), (1145, 1245),
                   (1350, 1400), (1505, 1555)):
        body.append(arrow(f"M 380 {y1} V {y2}", TEAL, 4.0))
    body.append(arrow("M 200 910 V 1040", TEAL, 4.0))

    body.append(box(365, 925, 320, 85,
                    "conditional palette worker",
                    ("build overlaps PREPLAN",),
                    MAGENTA, MAGENTA_LIGHT, 15, 12))
    body.append(arrow("M 525 910 V 925", MAGENTA, 3.5, "7 6"))
    body.append(arrow("M 525 1010 V 1245", MAGENTA, 3.5, "7 6"))

    body.append(panel(30, 1730, 700, 190,
                      "Other concurrency is separate", PURPLE,
                      PURPLE_LIGHT))
    body.append(text_lines(65, 1800,
                           ("• multi-frame loader handoff",
                            "• palette-apply / SIXEL band pipeline",
                            "• ordered writer flush"),
                           17, 30, 550, MUTED))
    return svg_document(
        width,
        height,
        "Encoder scheduler",
        "Portrait map of planning, six synchronous macro nodes, the conditional palette worker overlap, and separate loader and band-pipeline concurrency domains.",
        "\n".join(body),
    )


def frame_preparation_wide() -> str:
    """Return the landscape loader and preprocessing map."""
    width = 1760
    height = 1070
    body = header(width,
                  "Loader handoff and frame preparation",
                  "Fallback is ordered; CMS and alpha work are backend-local",
                  "IMPLEMENTATION MAP 2 / 4")
    body.append(panel(35, 125, 1690, 330,
                      "Loader manager candidate chain", BLUE))
    loader_boxes = (
        (70, "candidate predicate", ("magic / extension /", "forced selector")),
        (390, "backend attempt", ("decode frames", "and metadata")),
        (720, "backend-local finalize", ("ICC/CMS, orientation,", "alpha policy as supported")),
        (1100, "status gate", ("success / fallback-eligible", "/ terminal error")),
        (1435, "next candidate", ("only on explicit", "fallback statuses")),
    )
    loader_widths = (245, 250, 300, 270, 220)
    for (entry, entry_width) in zip(loader_boxes, loader_widths):
        x, title_value, detail = entry
        body.append(box(x, 230, entry_width, 130,
                        title_value, detail, BLUE, BLUE_LIGHT, 18, 14))
    for x1, x2 in ((315, 390), (640, 720), (1020, 1100),
                   (1370, 1435)):
        body.append(arrow(f"M {x1} 295 H {x2}", BLUE, 3.8))
    body.append(arrow("M 1545 360 V 415 H 500 V 360",
                      RED, 3.2, "8 6"))
    body.append(text(1020, 436,
                     "fallback loops to a new component, not a global decoder",
                     14, 650, RED, "middle"))

    body.append(panel(35, 485, 650, 535,
                      "Frame handoff artifact", TEAL))
    body.append(box(75, 555, 570, 150,
                    "frame { pixels + interpretation }",
                    ("dimensions, pixelformat, colorspace, timing",
                     "palette/index metadata and transparency metadata"),
                    TEAL, TEAL_LIGHT, 21, 15))
    body.append(pill(80, 745, 545,
                     "3 components (byte/float) + optional binary mask",
                     TEAL, TEAL_LIGHT))
    body.append(pill(80, 800, 545,
                     "indexed pixels + palette + transparent index",
                     TEAL, TEAL_LIGHT))
    body.append(pill(80, 855, 545,
                     "temporary packed alpha-bearing layouts are possible",
                     TEAL, TEAL_LIGHT))
    body.append(text_lines(80, 935,
                           ("There is no universal ‘Loaded Frame (RGBA)’ contract.",
                            "Consumers must read pixelformat and transparency metadata."),
                           15, 25, 600, MUTED))

    body.append(panel(720, 485, 1005, 535,
                      "Per-frame preplan and sampling-source split", MAGENTA))
    pre = (
        (755, "loaded frame", ("immutable source for", "adaptive sampling"),
         TEAL, TEAL_LIGHT),
        (990, "clip (early?)", ("when clipfirst", "is active"), BLUE, BLUE_LIGHT),
        (1205, "colorspace pre", ("to resize input", "often linear RGB f32"),
         PURPLE, PURPLE_LIGHT),
        (1450, "resize", ("resample in selected", "scale format"), BLUE, BLUE_LIGHT),
    )
    for x, title_value, detail, accent, fill in pre:
        body.append(box(x, 705, 190 if x != 1205 else 215, 125,
                        title_value, detail, accent, fill, 17, 13))
    for x1, x2 in ((945, 990), (1180, 1205), (1420, 1450)):
        body.append(arrow(f"M {x1} 768 H {x2}", BLUE, 3.5))
    body.append(box(1015, 870, 215, 110,
                    "clip (late?)", ("when clipfirst", "is false"),
                    BLUE, BLUE_LIGHT, 17, 13))
    body.append(box(1320, 870, 310, 110,
                    "colorspace post", ("to effective -W format / space",),
                    PURPLE, PURPLE_LIGHT, 17, 13))
    body.append(arrow("M 1545 830 V 850 H 1122 V 870", BLUE, 3.5))
    body.append(arrow("M 1230 925 H 1320", BLUE, 3.5))
    body.append(arrow("M 835 705 V 615 H 990",
                      MAGENTA, 3.5, "8 6"))
    body.append(box(990, 550, 285, 100,
                    "adaptive sample", ("owned stream from loaded frame",),
                    MAGENTA, MAGENTA_LIGHT, 17, 13))
    body.append(box(1365, 550, 285, 100,
                    "full-frame sample", ("borrows preprocessed frame",),
                    MAGENTA, MAGENTA_LIGHT, 17, 13))
    body.append(arrow("M 1475 980 V 1000 H 1505 V 650",
                      MAGENTA, 3.5, "8 6"))

    return svg_document(
        width,
        height,
        "Loader handoff and frame preparation",
        "Ordered loader candidates run their own decode, color management, orientation, and alpha handling. A frame records pixels, pixel format, color space, timing, palette metadata, and transparency metadata. Adaptive sampling reads the loaded frame before preprocessing; full-frame sampling borrows the preprocessed frame after optional clip, color conversion, resize, late clip, and post-resize conversion.",
        "\n".join(body),
    )


def frame_preparation_mobile() -> str:
    """Return the portrait loader and preprocessing map."""
    width = 760
    height = 2310
    body = header(width,
                  "Frame preparation",
                  "Loader fallback, frame contract, resize color work",
                  "MAP 2 / 4")
    body.append(panel(30, 120, 700, 660,
                      "Ordered loader candidates", BLUE))
    loader = (
        (180, "candidate predicate", "magic / extension / forced selector"),
        (300, "backend attempt", "decode frames + metadata"),
        (420, "backend-local finalize", "CMS / orientation / alpha as supported"),
        (540, "status gate", "success / allowed fallback / terminal error"),
        (660, "next candidate", "only for explicit fallback statuses"),
    )
    for y, title_value, detail in loader:
        body.append(box(75, y, 610, 85, title_value, (detail,),
                        BLUE, BLUE_LIGHT, 17, 13))
        if y != 660:
            body.append(arrow(f"M 380 {y + 85} V {y + 115}", BLUE, 3.2))
    body.append(arrow("M 650 745 V 765 H 55 V 222 H 75",
                      RED, 3.0, "7 6"))

    body.append(panel(30, 815, 700, 470,
                      "Frame handoff artifact", TEAL))
    body.append(box(75, 875, 610, 120,
                    "frame { pixels + interpretation }",
                    ("size, format, colorspace, timing, palette, transparency",),
                    TEAL, TEAL_LIGHT, 19, 13))
    body.append(pill(75, 1030, 610,
                     "3 components + optional mask", TEAL, TEAL_LIGHT))
    body.append(pill(75, 1080, 610,
                     "indexed + palette + transparent index", TEAL,
                     TEAL_LIGHT))
    body.append(pill(75, 1130, 610,
                     "temporary packed alpha layout", TEAL, TEAL_LIGHT))
    body.append(text(380, 1220, "Never assume a universal RGBA frame.",
                     16, 700, RED, "middle"))

    body.append(panel(30, 1320, 700, 940,
                      "Preplan and sample sources", MAGENTA))
    stages = (
        (1380, "loaded frame", "adaptive sampling reads here",
         TEAL, TEAL_LIGHT),
        (1580, "optional early clip", "when clipfirst is active",
         BLUE, BLUE_LIGHT),
        (1725, "colorspace pre", "usually linear RGB float32 for resize",
         PURPLE, PURPLE_LIGHT),
        (1870, "resize", "resample in scale format",
         BLUE, BLUE_LIGHT),
        (2015, "optional late clip", "when clipfirst is false",
         BLUE, BLUE_LIGHT),
        (2160, "colorspace post", "to effective -W format / space",
         PURPLE, PURPLE_LIGHT),
    )
    for y, title_value, detail, accent, fill in stages:
        body.append(box(75, y, 610, 95, title_value, (detail,),
                        accent, fill, 17, 13))
    for y1, y2 in ((1675, 1725), (1820, 1870),
                   (1965, 2015), (2110, 2160)):
        body.append(arrow(f"M 380 {y1} V {y2}", BLUE, 3.4))
    body.append(arrow("M 200 1475 V 1580", BLUE, 3.4))
    body.append(box(350, 1495, 335, 62,
                    "adaptive: owned sample stream",
                    ("before mutation",), MAGENTA, MAGENTA_LIGHT, 14, 11))
    body.append(arrow("M 520 1475 V 1495", MAGENTA, 3.0, "7 6"))
    return svg_document(
        width,
        height,
        "Frame preparation",
        "Portrait map of ordered loader fallback, the polymorphic frame representation, adaptive sampling before mutation, and the preplan sequence with color conversion around resize.",
        "\n".join(body),
    )


def palette_build_wide() -> str:
    """Return the landscape generated-palette map."""
    width = 1760
    height = 1210
    body = header(width,
                  "Generated palette construction",
                  "One common contract, four different solver lifecycles",
                  "IMPLEMENTATION MAP 3 / 4")

    body.append(panel(35, 125, 1690, 235,
                      "Common ingress", MAGENTA))
    ingress = (
        (70, "sample stream", ("adaptive: owned loaded-frame sample",
                               "full: borrowed preprocessed frame")),
        (440, "palette view", ("clone when required", "preserve transparency fence")),
        (790, "convert to -X", ("pixelformat + colorspace", "skip if already matched")),
        (1130, "binning", ("none / exact / hard / soft", "weighted point artifact")),
        (1470, "dispatch -Q", ("effective model", "and engine precision")),
    )
    ingress_widths = (310, 285, 275, 280, 200)
    for (entry, entry_width) in zip(ingress, ingress_widths):
        x, title_value, detail = entry
        body.append(box(x, 195, entry_width, 125,
                        title_value, detail, MAGENTA, MAGENTA_LIGHT, 18, 13))
    for x1, x2 in ((380, 440), (725, 790), (1065, 1130), (1410, 1470)):
        body.append(arrow(f"M {x1} 258 H {x2}", MAGENTA, 3.8))

    body.append(panel(35, 395, 1690, 420,
                      "Model-specific solver bodies", PURPLE))
    models = (
        (70, "Heckbert", ("histogram", "recursive box splits", "leaf representatives")),
        (480, "K-means", ("seed + restarts", "assign / mean iterations", "feedback / polish")),
        (890, "K-medoids", ("sample-constrained seeds", "PAM / CLARA / CLARANS", "/ BanditPAM search")),
        (1300, "K-center", ("collect + seed", "farthest-first and/or swaps", "radius-guarded polish")),
    )
    for x, title_value, detail in models:
        body.append(box(x, 485, 350, 205,
                        title_value, detail, PURPLE, PURPLE_LIGHT, 22, 15))
        body.append(pill(x + 30, 720, 290,
                         "init -> model work -> finalize",
                         PURPLE, PURPLE_LIGHT))
    body.append(text(880, 790,
                     "The shared telemetry phases do not imply a shared iteration algorithm.",
                     16, 700, RED, "middle"))

    body.append(panel(35, 850, 1690, 310,
                      "Post-solver composition and handoff", TEAL))
    post = (
        (70, "optional Ward", ("oversplit Q -> K", "model-dependent Lloyd"),
         PURPLE, PURPLE_LIGHT),
        (390, "final snap", ("early hooks live in solver", "mandatory final if enabled"),
         PURPLE, PURPLE_LIGHT),
        (725, "cover repair", ("after final snap today", "byte palette only"),
         MAGENTA, MAGENTA_LIGHT),
        (1045, "convert -X -> -W", ("palette entries", "for lookup + dither"),
         BLUE, BLUE_LIGHT),
        (1390, "optional key", ("append reserved", "transparency entry"),
         TEAL, TEAL_LIGHT),
    )
    post_widths = (250, 265, 250, 275, 260)
    for (entry, entry_width) in zip(post, post_widths):
        x, title_value, detail, accent, fill = entry
        body.append(box(x, 925, entry_width, 135,
                        title_value, detail, accent, fill, 18, 13))
    for x1, x2 in ((320, 390), (655, 725), (975, 1045), (1320, 1390)):
        body.append(arrow(f"M {x1} 992 H {x2}", TEAL, 3.6))
    body.append(pill(250, 1090, 1260,
                     "engine retry: float32 -> legacy when allowed; requested solver failure -> Heckbert compatibility fallback",
                     RED, RED_LIGHT))

    return svg_document(
        width,
        height,
        "Generated palette construction",
        "Samples form a typed stream, are converted to the clustering color space, binned, and dispatched to one of four solver families with different initialization and iteration behavior. Optional final merge and snap happen in solver-specific construction, cover repair follows, then the palette is converted from clustering to working color space and may receive a reserved transparency key.",
        "\n".join(body),
    )


def palette_build_mobile() -> str:
    """Return the portrait generated-palette map."""
    width = 760
    height = 2600
    body = header(width,
                  "Palette construction",
                  "Common boundaries; distinct solver internals",
                  "MAP 3 / 4")
    body.append(panel(30, 120, 700, 650, "Common ingress", MAGENTA))
    ingress = (
        (180, "sample stream", "adaptive owned / full-frame borrowed"),
        (295, "palette view", "clone when representation must change"),
        (410, "convert to -X", "clustering pixelformat + colorspace"),
        (525, "binning", "weighted points: none / exact / hard / soft"),
        (640, "dispatch -Q", "effective model + precision engine"),
    )
    for y, title_value, detail in ingress:
        body.append(box(75, y, 610, 80, title_value, (detail,),
                        MAGENTA, MAGENTA_LIGHT, 17, 13))
        if y != 640:
            body.append(arrow(f"M 380 {y + 80} V {y + 110}",
                              MAGENTA, 3.2))

    body.append(panel(30, 805, 700, 1050,
                      "Four solver bodies", PURPLE))
    models = (
        (870, "Heckbert", ("histogram + recursive box splits",
                            "leaf representatives")),
        (1095, "K-means", ("seed / restarts",
                             "assign -> mean iterations / polish")),
        (1320, "K-medoids", ("sample-constrained seeds",
                               "PAM / CLARA / CLARANS / BanditPAM")),
        (1545, "K-center", ("farthest-first and/or swaps",
                              "radius-guarded polish")),
    )
    for y, title_value, detail in models:
        body.append(box(75, y, 610, 170, title_value, detail,
                        PURPLE, PURPLE_LIGHT, 20, 14))
        body.append(pill(155, y + 120, 450,
                         "model-specific init / work / finalize",
                         PURPLE, PURPLE_LIGHT))
    body.append(text(380, 1815,
                     "No universal solver iteration loop.",
                     16, 760, RED, "middle"))

    body.append(panel(30, 1890, 700, 660,
                      "Post-solver handoff", TEAL))
    post = (
        (1950, "optional Ward + supported Lloyd", PURPLE, PURPLE_LIGHT),
        (2065, "final snap (early hooks live in solver)", PURPLE,
         PURPLE_LIGHT),
        (2180, "cover repair (after snap today)", MAGENTA, MAGENTA_LIGHT),
        (2295, "convert palette -X -> -W", BLUE, BLUE_LIGHT),
        (2410, "optional transparency key", TEAL, TEAL_LIGHT),
    )
    for y, title_value, accent, fill in post:
        body.append(box(75, y, 610, 80, title_value, ("",),
                        accent, fill, 17, 12))
        if y != 2410:
            body.append(arrow(f"M 380 {y + 80} V {y + 110}", TEAL, 3.2))
    body.append(pill(80, 2510, 600,
                     "fallbacks are explicit and recorded",
                     RED, RED_LIGHT))
    return svg_document(
        width,
        height,
        "Palette construction",
        "Portrait map of the common sample, clustering-space, binning, and dispatch boundaries; four distinct solver lifecycles; post-solver merge, snap, cover, clustering-to-working conversion, and transparency key attachment.",
        "\n".join(body),
    )


def apply_output_wide() -> str:
    """Return the landscape palette-application and output map."""
    width = 1760
    height = 1090
    body = header(width,
                  "Palette application and SIXEL output",
                  "Lookup preparation is policy-specific; mapping is per pixel",
                  "IMPLEMENTATION MAP 4 / 4")

    body.append(panel(35, 125, 1690, 250,
                      "Lookup preparation after palette collect", MAGENTA))
    body.append(box(70, 205, 285, 120,
                    "working palette", ("generated -X -> -W", "or supplied palette"),
                    MAGENTA, MAGENTA_LIGHT, 19, 14))
    body.append(box(455, 185, 360, 155,
                    "eager planner filter", ("FHEDT grid", "VP-tree", "Eytzinger layout"),
                    PURPLE, PURPLE_LIGHT, 19, 14))
    body.append(box(920, 185, 360, 155,
                    "prepare at apply", ("none / 5bit / 6bit / certlut", "RBC / Mahalanobis / auto", "or reuse prepared policy"),
                    BLUE, BLUE_LIGHT, 19, 14))
    body.append(box(1385, 205, 285, 120,
                    "lookup interface", ("map_pixel(candidate)", "shared or worker-local state"),
                    TEAL, TEAL_LIGHT, 19, 14))
    body.append(arrow("M 355 265 H 455", MAGENTA, 3.8))
    body.append(arrow("M 355 285 H 900 Q 920 285 920 265", BLUE, 3.8))
    body.append(arrow("M 815 265 H 1385", PURPLE, 3.8))
    body.append(arrow("M 1280 265 H 1385", BLUE, 3.8))

    body.append(panel(35, 410, 1690, 330,
                      "PaletteApply CPU model (policy implementations own the loop)", TEAL))
    loop = (
        (70, "read sample", ("current -W format", "+ transparency fence"),
         BLUE, BLUE_LIGHT),
        (375, "dither candidate", ("position / carried error", "or unchanged sample"),
         TEAL, TEAL_LIGHT),
        (705, "lookup", ("map_pixel", "returns palette index"),
         PURPLE, PURPLE_LIGHT),
        (1000, "emit index", ("respect keycolor", "and 6delta keep"),
         MAGENTA, MAGENTA_LIGHT),
        (1310, "update state", ("candidate - palette[index]", "diffuse / advance"),
         TEAL, TEAL_LIGHT),
    )
    loop_widths = (235, 260, 225, 245, 350)
    for (entry, entry_width) in zip(loop, loop_widths):
        x, title_value, detail, accent, fill = entry
        body.append(box(x, 505, entry_width, 135,
                        title_value, detail, accent, fill, 18, 13))
    for x1, x2 in ((305, 375), (635, 705), (930, 1000), (1245, 1310)):
        body.append(arrow(f"M {x1} 572 H {x2}", TEAL, 3.6))
    body.append(arrow("M 1515 640 V 690 H 495 V 640",
                      TEAL, 3.4, "8 6"))
    body.append(text(1015, 716,
                     "scan-order loop; transparent pixels can bypass lookup",
                     14, 650, MUTED, "middle"))

    body.append(panel(35, 780, 1690, 260,
                      "Wire serialization", GOLD))
    output = (
        (70, "index rows", ("serial plane or", "completed bands"),
         TEAL, TEAL_LIGHT),
        (390, "palette copy", ("keep reusable -W", "palette unchanged"),
         MAGENTA, MAGENTA_LIGHT),
        (710, "convert -W -> -U", ("float when available", "then byte representation"),
         BLUE, BLUE_LIGHT),
        (1050, "RGB or HLS", ("DECGCI type 2 RGB %", "or RGB -> DEC HLS type 1"),
         PURPLE, PURPLE_LIGHT),
        (1390, "SIXEL bands", ("palette definitions, runs,", "DCS envelope + ordered I/O"),
         GOLD, GOLD_LIGHT),
    )
    output_widths = (240, 240, 270, 275, 290)
    for (entry, entry_width) in zip(output, output_widths):
        x, title_value, detail, accent, fill = entry
        body.append(box(x, 855, entry_width, 130,
                        title_value, detail, accent, fill, 18, 13))
    for x1, x2 in ((310, 390), (630, 710), (980, 1050), (1325, 1390)):
        body.append(arrow(f"M {x1} 920 H {x2}", GOLD, 3.8))
    body.append(pill(520, 1000, 720,
                     "band workers may overlap PaletteApply; writer preserves output order",
                     PURPLE, PURPLE_LIGHT))
    return svg_document(
        width,
        height,
        "Palette application and SIXEL output",
        "Selected lookup policies are prepared eagerly after palette collection while others prepare or reuse their policy at palette application. Dither policy implementations scan pixels, respect transparency, form candidates, call map_pixel, emit indices, and update diffusion state. The core copies and converts the working palette to output color space, optionally converts RGB to DEC HLS percentages, and emits ordered SIXEL bands.",
        "\n".join(body),
    )


def apply_output_mobile() -> str:
    """Return the portrait palette-application and output map."""
    width = 760
    height = 2250
    body = header(width,
                  "Apply and output",
                  "Policy preparation, per-pixel loop, wire conversion",
                  "MAP 4 / 4")
    body.append(panel(30, 120, 700, 510,
                      "Lookup preparation", MAGENTA))
    body.append(box(75, 180, 610, 90,
                    "working palette", ("generated -X -> -W or supplied",),
                    MAGENTA, MAGENTA_LIGHT, 18, 13))
    body.append(box(75, 310, 610, 105,
                    "eager after collect", ("FHEDT / VP-tree / Eytzinger",),
                    PURPLE, PURPLE_LIGHT, 18, 13))
    body.append(box(75, 455, 610, 115,
                    "prepare or reuse at apply", ("none / dense LUTs / certlut / RBC / Mahalanobis / auto",),
                    BLUE, BLUE_LIGHT, 18, 13))
    body.append(arrow("M 380 270 V 310", MAGENTA, 3.4))
    body.append(arrow("M 610 270 V 455", BLUE, 3.4))

    body.append(panel(30, 665, 700, 905,
                      "Per-pixel PaletteApply loop", TEAL))
    loop = (
        (725, "read sample", "current -W format + transparency fence",
         BLUE, BLUE_LIGHT),
        (875, "dither candidate", "position / carried error / unchanged",
         TEAL, TEAL_LIGHT),
        (1025, "lookup.map_pixel", "select one palette index",
         PURPLE, PURPLE_LIGHT),
        (1175, "emit index", "keycolor and 6delta fences",
         MAGENTA, MAGENTA_LIGHT),
        (1325, "update policy state", "diffuse error or advance ordered state",
         TEAL, TEAL_LIGHT),
    )
    for y, title_value, detail, accent, fill in loop:
        body.append(box(75, y, 610, 100, title_value, (detail,),
                        accent, fill, 18, 13))
    for y1, y2 in ((825, 875), (975, 1025), (1125, 1175), (1275, 1325)):
        body.append(arrow(f"M 380 {y1} V {y2}", TEAL, 3.4))
    body.append(arrow("M 650 1425 V 1495 H 55 V 775 H 75",
                      TEAL, 3.0, "8 6"))
    body.append(pill(145, 1500, 470,
                     "transparent pixels may bypass lookup",
                     RED, RED_LIGHT))

    body.append(panel(30, 1605, 700, 595,
                      "SIXEL wire serialization", GOLD))
    output = (
        (1665, "index rows / ready bands", TEAL, TEAL_LIGHT),
        (1780, "copy palette; keep reusable -W", MAGENTA, MAGENTA_LIGHT),
        (1895, "convert palette -W -> -U", BLUE, BLUE_LIGHT),
        (2010, "emit RGB % or convert RGB -> DEC HLS", PURPLE,
         PURPLE_LIGHT),
        (2125, "palette definitions + SIXEL bands", GOLD, GOLD_LIGHT),
    )
    for y, title_value, accent, fill in output:
        body.append(box(75, y, 610, 80, title_value, ("",),
                        accent, fill, 17, 12))
        if y != 2125:
            body.append(arrow(f"M 380 {y + 80} V {y + 110}", GOLD, 3.2))
    return svg_document(
        width,
        height,
        "Apply and output",
        "Portrait map of lookup preparation, the policy-owned per-pixel palette application loop, working-to-output palette color conversion, optional RGB-to-DEC-HLS conversion, and ordered SIXEL output.",
        "\n".join(body),
    )


def manifest() -> dict[str, object]:
    """Return provenance and implementation anchors for the figures."""
    return {
        "schema": 1,
        "generator": "tools/plot_encoder_execution_map_figures.py",
        "kind": "deterministic implementation map",
        "measurement": False,
        "scope": "normal per-frame palette path and adjacent concurrency domains",
        "assets": [
            {"path": name, "role": name.removesuffix(".svg")}
            for name in ASSET_NAMES
            if name.endswith(".svg")
        ],
        "implementation_anchors": {
            "macro_scheduler": [
                "src/encoder.c:sixel_encode_dag_run_nodes",
                "src/encoder.c:sixel_encoder_encode_frame_internal",
            ],
            "planner_graph": [
                "src/planner.c:sixel_encoding_planner_build_dag",
                "src/planner.c:sixel_encoding_planner_replan",
            ],
            "loader": [
                "src/loader-manager.c:sixel_loader_manager_load",
                "src/loader.c:sixel_helper_load_image_file",
            ],
            "palette": [
                "src/encoder.c:sixel_encoder_prepare_palette",
                "src/palette.c:sixel_palette_vtbl_generate",
            ],
            "apply": [
                "src/dither.c:sixel_dither_apply_palette",
                "src/dither.c:sixel_dither_resolve_indexes",
            ],
            "output": [
                "src/encoder-core-encode.c:sixel_encode_dither",
                "src/encoder-core-encode.c:output_hls_palette_definition",
            ],
        },
        "color_roles": {
            "frame_and_pixel_data": BLUE,
            "palette_data": MAGENTA,
            "policy_or_algorithm": PURPLE,
            "indexed_or_apply_state": TEAL,
            "wire_output": GOLD,
            "warning_or_fallback": RED,
        },
    }


def generate(output_dir: Path) -> None:
    """Generate every SVG and its provenance manifest."""
    output_dir.mkdir(parents=True, exist_ok=True)
    figures = {
        "encoder-scheduler-wide.svg": scheduler_wide(),
        "encoder-scheduler-mobile.svg": scheduler_mobile(),
        "frame-preparation-wide.svg": frame_preparation_wide(),
        "frame-preparation-mobile.svg": frame_preparation_mobile(),
        "palette-build-wide.svg": palette_build_wide(),
        "palette-build-mobile.svg": palette_build_mobile(),
        "palette-apply-output-wide.svg": apply_output_wide(),
        "palette-apply-output-mobile.svg": apply_output_mobile(),
    }
    for name, content in figures.items():
        (output_dir / name).write_text(content, encoding="utf-8")
    (output_dir / "encoder-execution-map-figures.json").write_text(
        json.dumps(manifest(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def check(output_dir: Path) -> None:
    """Regenerate in a temporary directory and compare exact bytes."""
    with tempfile.TemporaryDirectory(
            prefix="encoder-execution-map-figures-") as temp:
        generated = Path(temp)
        generate(generated)
        mismatches = []
        for name in ASSET_NAMES:
            expected = output_dir / name
            actual = generated / name
            if (not expected.is_file()
                    or expected.read_bytes() != actual.read_bytes()):
                mismatches.append(name)
        if mismatches:
            raise SystemExit(
                "encoder execution-map figures are stale or missing: "
                + ", ".join(mismatches)
            )


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    source_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=(source_root / "docs/functionality"
                 / "encoder-execution-map-figures"),
        help="directory for generated SVG and manifest files",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare committed assets with a fresh deterministic generation",
    )
    return parser.parse_args()


def main() -> None:
    """Generate or verify the encoder execution-map assets."""
    arguments = parse_args()
    if arguments.check:
        check(arguments.output_dir)
    else:
        generate(arguments.output_dir)


if __name__ == "__main__":
    main()
