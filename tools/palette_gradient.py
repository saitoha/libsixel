#!/usr/bin/env python3
"""Select a source-image gradient and inspect the palette of its indexed output.

Optional analysis dependencies are isolated from the C library build. Run
--help for the browser workflow and deterministic batch alternatives.
"""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import math
import platform
from importlib.metadata import version as package_version
import secrets
import sys
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon
import numpy as np
from PIL import Image, ImageOps
from scipy.ndimage import gaussian_filter, map_coordinates

VERSION = 1
ACCENT = "#a32670"
BLUE = "#23668c"
FIGURE_NAMES = ("01-selected-gradient", "02-gradient-profile", "03-rgb-3d",
                "04-voronoi-slice", "05-chord-assignment")


def image_data(path):
    """Orient both images consistently; never silently resize or color-manage."""
    path = Path(path).resolve()
    with Image.open(path) as image:
        if image.mode not in ("RGB", "RGBA", "P", "L", "LA"):
            raise ValueError(f"{path.name}: expected an 8-bit RGB, indexed, or grayscale image")
        if getattr(image, "n_frames", 1) != 1:
            raise ValueError(f"{path.name}: supply one extracted frame, not an animation")
        record = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                  "mode": image.mode, "icc_profile_present": bool(image.info.get("icc_profile")),
                  "exif_orientation": int(image.getexif().get(274, 1))}
        image = ImageOps.exif_transpose(image)
        rgba = np.asarray(image.convert("RGBA")).copy()
        palette = None
        if image.mode == "P":
            entries = image.getpalette("RGB")
            if entries:
                palette = np.asarray(entries, dtype=np.uint8).reshape(-1, 3)
                # Transparent palette entries cannot participate in opaque RGB
                # reconstruction, including unused transparent table entries.
                alpha = image.info.get("transparency")
                valid = np.ones(len(palette), dtype=bool)
                if isinstance(alpha, int) and 0 <= alpha < len(valid):
                    valid[alpha] = False
                elif isinstance(alpha, bytes):
                    valid[:min(len(alpha), len(valid))] = np.frombuffer(alpha, np.uint8)[:len(valid)] == 255
                palette = (palette[valid].astype(float), np.flatnonzero(valid).tolist())
    record["size"] = [int(rgba.shape[1]), int(rgba.shape[0])]
    return rgba, palette, record


def output_palette(rgba, table):
    if table is not None:
        colors, ids = table
        origin = "Indexed image table, including unused opaque entries"
    else:
        colors = np.unique(rgba[rgba[:, :, 3] == 255, :3], axis=0).astype(float)
        ids = list(range(len(colors)))
        origin = "Colors observed in opaque output pixels; unused palette entries are unavailable"
    if not 1 <= len(colors) <= 256:
        raise ValueError(f"Output has {len(colors)} opaque colors; expected 1–256. Use a lossless quantized image.")
    return {"colors": colors, "ids": ids, "origin": origin}


def line_pixels(image, endpoints):
    """Read actual pixels on a rasterized segment, without color interpolation."""
    line = np.asarray(endpoints, dtype=float)
    if line.shape != (2, 2) or not np.isfinite(line).all():
        raise ValueError("A line needs two finite (x,y) endpoints")
    height, width = image.shape[:2]
    if (line < 0).any() or (line[:, 0] > width - 1).any() or (line[:, 1] > height - 1).any():
        raise ValueError("Line endpoints lie outside the oriented source image")
    line = np.rint(line).astype(int)
    count = int(np.abs(line[1] - line[0]).max()) + 1
    if count < 2:
        raise ValueError("Choose two distinct pixels")
    xy = np.rint(np.linspace(line[0], line[1], count)).astype(int)
    values = image[xy[:, 1], xy[:, 0]]
    if image.shape[2] == 4 and np.any(values[:, 3] != 255):
        raise ValueError("The selected line crosses transparent or partially transparent pixels")
    return xy, values[:, :3].astype(float)


def discover_gradients(source, limit=16, roi=None):
    """Rank source-only smooth, directional transitions, not palette failures.

    A bounded working image supplies a color structure tensor and multiscale
    candidate scans. Reject isolated edges using effective transition width,
    and reject texture/reversals using path straightness. The final selected
    profile is always reread from full-resolution unsmoothed source pixels.
    """
    height, width = source.shape[:2]
    if roi is None:
        x0, y0, rw, rh = 0, 0, width, height
    else:
        x0, y0, rw, rh = roi
        if min(x0, y0) < 0 or min(rw, rh) < 2 or x0 + rw > width or y0 + rh > height:
            raise ValueError("ROI must lie inside the oriented source image")
    scale = min(1., 512. / max(rw, rh))
    dw, dh = max(2, round(rw * scale)), max(2, round(rh * scale))
    cropped = Image.fromarray(source[y0:y0 + rh, x0:x0 + rw, :3])
    small = np.asarray(cropped.resize((dw, dh), Image.Resampling.BILINEAR)).astype(float)
    small = gaussian_filter(small, (.75, .75, 0))
    gy, gx = np.gradient(small, axis=(0, 1))
    xx = gaussian_filter((gx * gx).sum(2), 1.5)
    yy = gaussian_filter((gy * gy).sum(2), 1.5)
    xy = gaussian_filter((gx * gy).sum(2), 1.5)
    angle = .5 * np.arctan2(2 * xy, xx - yy)
    coherence = np.sqrt((xx - yy) ** 2 + 4 * xy ** 2) / (xx + yy + 1e-9)
    candidates = []
    stride = max(3, min(dw, dh) // 48)
    for cy in range(3, dh - 3, stride):
        for cx in range(3, dw - 3, stride):
            if coherence[cy, cx] < .65 or xx[cy, cx] + yy[cy, cx] < .2:
                continue
            direction = np.array([math.cos(angle[cy, cx]), math.sin(angle[cy, cx])])
            for length in (12, 24, 48, 80):
                coords = np.array([cx, cy]) + np.linspace(-length / 2, length / 2, length + 1)[:, None] * direction
                if (coords < 0).any() or (coords[:, 0] >= dw - 1).any() or (coords[:, 1] >= dh - 1).any():
                    continue
                colors = np.array([map_coordinates(small[:, :, c], [coords[:, 1], coords[:, 0]], order=1)
                                   for c in range(3)]).T
                delta = colors[-1] - colors[0]
                span = float(np.linalg.norm(delta))
                steps = np.linalg.norm(np.diff(colors, axis=0), axis=1)
                travel = steps.sum()
                effective = float(travel ** 2 / max(float(steps @ steps), 1e-12))
                straightness = span / max(travel, 1e-12)
                if span < 24 or straightness < .86 or effective < max(6, length * .45) or steps.max() > span * .22:
                    continue
                progress = (colors - colors[0]) @ delta / span ** 2
                residual = colors - colors[0] - progress[:, None] * delta
                rms = float(np.sqrt(np.mean((residual ** 2).sum(1))))
                if rms > span * .10:
                    continue
                # Resize pixel centers map back to source centers. Round only
                # after mapping, so the saved coordinates are original pixels.
                ends = (coords[[0, -1]] + .5) * [rw / dw, rh / dh] - .5 + [x0, y0]
                ends = np.clip(np.rint(ends), [x0, y0], [x0 + rw - 1, y0 + rh - 1]).astype(int)
                try:
                    _, actual = line_pixels(source, ends)
                except ValueError:
                    continue
                if np.linalg.norm(actual[-1] - actual[0]) < 12:
                    continue
                score = span * math.sqrt(effective) * straightness / (1 + 5 * rms / span)
                candidates.append({"line": ends.tolist(), "score": float(score), "rgb_span": span,
                                   "straightness": float(straightness), "effective_steps": effective,
                                   "detection_rms": rms})
    candidates.sort(key=lambda c: (-c["score"], c["line"][0], c["line"][1]))
    chosen = []
    for candidate in candidates:
        line = np.asarray(candidate["line"])
        center = line.mean(0)
        direction = line[1] - line[0]
        length = np.linalg.norm(direction)
        duplicate = False
        for previous in chosen:
            other = np.asarray(previous["line"])
            vector = other[1] - other[0]
            if (np.linalg.norm(center - other.mean(0)) < .45 * min(length, np.linalg.norm(vector))
                    and abs(direction @ vector) > .8 * length * np.linalg.norm(vector)):
                duplicate = True
                break
        if not duplicate:
            candidate["id"] = len(chosen) + 1
            chosen.append(candidate)
            if len(chosen) == limit:
                break
    return chosen


def nearest(points, palette):
    flat = np.asarray(points, dtype=float).reshape(-1, 3)
    ids = np.empty(len(flat), dtype=int)
    for start in range(0, len(flat), 4096):
        d = ((flat[start:start + 4096, None] - palette[None]) ** 2).sum(2)
        ids[start:start + len(d)] = d.argmin(1)
    return ids.reshape(points.shape[:-1])


def segment_intervals(palette, start, end):
    """Exact interval intersections with all competing RGB halfspaces."""
    result = []
    for i, p in enumerate(palette):
        if i and np.any(np.all(palette[:i] == p, axis=1)):
            continue
        delta = palette - p
        coefficients = 2 * delta @ (end - start)
        rhs = (palette * palette).sum(1) - p @ p - 2 * delta @ start
        lo, hi = 0., 1.
        for j, (a, b) in enumerate(zip(coefficients, rhs)):
            if abs(a) < 1e-10:
                # Distinct sites can tie along the entire chord. Keep the
                # first site just as nearest() does, without double counting.
                if b < -1e-9 or (abs(b) < 1e-9 and j < i):
                    hi = -1.
                    break
            elif a > 0:
                hi = min(hi, b / a)
            else:
                lo = max(lo, b / a)
        if hi > lo + 1e-10:
            result.append({"index": i, "start": float(lo), "end": float(hi)})
    return sorted(result, key=lambda r: r["start"])


def plane_for(profile, explicit=None):
    """Choose one common plane from the source, never from a compared palette."""
    start, end = profile[[0, -1]]
    delta = end - start
    norm = np.linalg.norm(delta)
    if norm < 1e-8:
        raise ValueError("The two endpoint colors coincide; choose a nonconstant gradient")
    spans = np.ptp(profile, axis=0)
    fixed = int(np.argmin(spans))
    if explicit or spans[fixed] <= max(2., .08 * norm):
        if explicit:
            channel, value = explicit
            fixed = "RGB".index(channel)
        else:
            value = float(np.median(profile[:, fixed]))
        remaining = [i for i in range(3) if i != fixed]
        origin = np.zeros(3)
        origin[fixed] = value
        basis = np.eye(3)[remaining]
        label = f'{"RGB"[fixed]} = {value:g}'
        axes = ["RGB"[i] for i in remaining]
    else:
        # Include the exact endpoint chord. Fit only the remaining in-plane
        # direction; use a deterministic orthogonal axis for a straight ramp.
        first = delta / norm
        origin = (start + end) / 2
        residual = profile - origin - ((profile - origin) @ first)[:, None] * first
        if np.linalg.norm(residual) > 1e-7:
            second = np.linalg.svd(residual, full_matrices=False)[2][0]
        else:
            axis = np.eye(3)[np.argmin(np.abs(first))]
            second = axis - (axis @ first) * first
        second -= (second @ first) * first
        second /= np.linalg.norm(second)
        if second[np.argmax(np.abs(second))] < 0:
            second *= -1
        basis = np.array([first, second])
        label, axes = "Source-fitted RGB plane", ["U (RGB units)", "V (RGB units)"]
    projected = (profile - origin) @ basis.T
    reconstructed = origin + projected @ basis
    off = np.linalg.norm(profile - reconstructed, axis=1)
    return {"origin": origin, "basis": basis, "label": label, "axes": axes,
            "max_off_plane": float(off.max()), "rms_off_plane": float(np.sqrt(np.mean(off ** 2)))}


def clip_polygon(polygon, normal, bound):
    """Clip a convex polygon against one closed halfplane."""
    if len(polygon) == 0:
        return polygon
    result = []
    previous = polygon[-1]
    old = float(previous @ normal - bound)
    for point in polygon:
        value = float(point @ normal - bound)
        if (value <= 1e-9) != (old <= 1e-9):
            result.append(previous + (point - previous) * old / (old - value))
        if value <= 1e-9:
            result.append(point)
        previous, old = point, value
    return np.asarray(result).reshape(-1, 2)


def slice_cells(palette, plane, bounds):
    """Exact 3D Voronoi intersections, including the RGB cube boundary."""
    origin, basis = plane["origin"], plane["basis"]
    x0, x1, y0, y1 = bounds
    rectangle = np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]])
    domain = rectangle
    for j in range(3):
        domain = clip_polygon(domain, basis[:, j], 255 - origin[j])
        domain = clip_polygon(domain, -basis[:, j], origin[j])
    cells = []
    projected = (palette - origin) @ basis.T
    offset = palette - origin - projected @ basis
    penalty = (offset * offset).sum(1)
    for i, p in enumerate(palette):
        # Distinct 3D sites can induce identical scores on this plane. Assign
        # that entire tie to the first site, consistent with nearest().
        if any(np.allclose(projected[i], projected[j], atol=1e-9, rtol=0)
               and abs(penalty[i] - penalty[j]) < 1e-8 for j in range(i)):
            continue
        polygon = domain.copy()
        delta = palette - p
        normals = 2 * delta @ basis.T
        rhs = (palette * palette).sum(1) - p @ p - 2 * delta @ origin
        for normal, bound in zip(normals, rhs):
            polygon = clip_polygon(polygon, normal, bound)
            if len(polygon) < 3:
                break
        if len(polygon) >= 3:
            area = abs(np.dot(polygon[:, 0], np.roll(polygon[:, 1], 1))
                       - np.dot(polygon[:, 1], np.roll(polygon[:, 0], 1))) / 2
            if area > 1e-7:
                cells.append({"index": i, "vertices": polygon, "area": float(area),
                              "site_xy": projected[i], "off_plane_distance": float(np.sqrt(penalty[i]))})
    return cells


def label_slice_sites(ax, entry):
    """Place site IDs in display coordinates without obscuring adjacent IDs."""
    x0, x1 = ax.get_xlim()
    y0, y1 = ax.get_ylim()
    visible = [c for c in entry["slice_cells"] if x0 <= c["site_xy"][0] <= x1 and y0 <= c["site_xy"][1] <= y1]
    active = set(entry["nearest_indices"])
    visible.sort(key=lambda c: (c["index"] not in active, -c["area"], c["index"]))
    pixel_scale = ax.figure.dpi / 72
    frame = ax.get_window_extent()
    occupied = []
    anchors = np.array([ax.transData.transform(c["site_xy"]) for c in visible])
    for cell in visible:
        label = str(entry["palette_ids"][cell["index"]])
        point = ax.transData.transform(cell["site_xy"])
        width, height = (len(label) * 5 + 3) * pixel_scale, 10 * pixel_scale
        options = []
        for radius in (10, 18, 28, 40, 56):
            for angle in (45, 135, -45, -135, 90, -90, 0, 180):
                displacement = radius * pixel_scale * np.array([math.cos(math.radians(angle)), math.sin(math.radians(angle))])
                center = point + displacement
                box = np.array([center[0] - width / 2, center[1] - height / 2,
                                center[0] + width / 2, center[1] + height / 2])
                outside = max(frame.x0 - box[0], 0) + max(box[2] - frame.x1, 0) + max(frame.y0 - box[1], 0) + max(box[3] - frame.y1, 0)
                overlap = sum(max(0, min(box[2], other[2]) - max(box[0], other[0])) *
                              max(0, min(box[3], other[3]) - max(box[1], other[1])) for other in occupied)
                covered_points = np.count_nonzero((anchors[:, 0] >= box[0] - 2) & (anchors[:, 0] <= box[2] + 2) &
                                                  (anchors[:, 1] >= box[1] - 2) & (anchors[:, 1] <= box[3] + 2))
                cost = outside * 10000 + overlap * 100 + covered_points * 2000 + radius
                options.append((cost, center, box))
        _, center, box = min(options, key=lambda row: row[0])
        occupied.append(box)
        position = ax.transData.inverted().transform(center)
        ax.annotate(label, cell["site_xy"], xytext=position, fontsize=8, ha="center", va="center",
                    weight="bold" if cell["index"] in active else "normal",
                    bbox=dict(facecolor="white", edgecolor="none", alpha=.86, pad=.4),
                    arrowprops=dict(arrowstyle="-", color="#666", lw=.4), zorder=5)


def save_figure(fig, output, name):
    fig.savefig(output / (name + ".png"), dpi=155, bbox_inches="tight")
    fig.savefig(output / (name + ".svg"), bbox_inches="tight")
    plt.close(fig)


def render_analysis(source, outputs, endpoints, directory, selection, explicit_plane=None):
    directory = Path(directory)
    xy, profile = line_pixels(source, endpoints)
    plane = plane_for(profile, explicit_plane)
    origin, basis = plane["origin"], plane["basis"]
    projected = (profile - origin) @ basis.T
    span = np.maximum(np.ptp(projected, axis=0), 12.)
    low, high = projected.min(0) - span * .3 - 8, projected.max(0) + span * .3 + 8
    bounds = [float(low[0]), float(high[0]), float(low[1]), float(high[1])]
    result = {"version": VERSION, "generator": {"script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "python": platform.python_version(), "packages": {name: package_version(name) for name in ("numpy", "scipy", "matplotlib", "pillow")}},
              "selection": selection, "line": np.asarray(endpoints).tolist(),
              "sample_xy": xy.tolist(), "source_rgb": profile.tolist(),
              "plane": {k: v.tolist() if isinstance(v, np.ndarray) else v for k, v in plane.items()},
              "slice_bounds": bounds, "space": "Euclidean 8-bit RGB code values; no CMS or diffusion simulation",
              "outputs": []}
    for item in outputs:
        _, actual = line_pixels(item["rgba"], endpoints)
        p = item["palette"]["colors"]
        ids = nearest(profile, p)
        cells = slice_cells(p, plane, bounds)
        intervals = segment_intervals(p, profile[0], profile[-1])
        assert abs(sum(v["end"] - v["start"] for v in intervals) - 1) < 1e-8
        entry = {"name": item["name"], "image": item["record"], "palette_origin": item["palette"]["origin"],
                 "palette_rgb": p.astype(int).tolist(), "palette_ids": item["palette"]["ids"],
                 "actual_rgb": actual.tolist(), "nearest_indices": ids.tolist(),
                 "nearest_rgb": p[ids].tolist(), "intervals": intervals,
                 "source_minus_actual_mean": (profile - actual).mean(0).tolist(),
                 "actual_rgb_rmse": float(np.sqrt(np.mean((profile - actual) ** 2))),
                 "nearest_rgb_rmse": float(np.sqrt(np.mean((profile - p[ids]) ** 2))),
                 "slice_cells": [{k: v.tolist() if isinstance(v, np.ndarray) else v for k, v in c.items()} for c in cells]}
        result["outputs"].append(entry)
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10,
                         "axes.spines.top": False, "axes.spines.right": False, "text.parse_math": False})
    count = len(outputs)
    fig, axs = plt.subplots(1, count + 1, figsize=(6 * (count + 1), 4.6), squeeze=False, layout="constrained")
    for ax, array, name in zip(axs[0], [source] + [o["rgba"] for o in outputs], ["Source: selected gradient"] + [o["name"] for o in outputs]):
        ax.imshow(array)
        ax.plot(xy[:, 0], xy[:, 1], color="white", lw=4)
        ax.plot(xy[:, 0], xy[:, 1], color=ACCENT, lw=2)
        ax.scatter(xy[[0, -1], 0], xy[[0, -1], 1], color=ACCENT, s=20)
        ax.set_title(name)
        ax.axis("off")
    save_figure(fig, directory, "01-selected-gradient")
    fig, axs = plt.subplots(4, 1, figsize=(11, 8.5), layout="constrained", height_ratios=[1.7, 1, 1, 1])
    strips, labels = [profile], ["Source (actual pixels)"]
    for entry in result["outputs"]:
        strips.extend([entry["actual_rgb"], entry["nearest_rgb"]])
        labels.extend([entry["name"] + " / actual output", entry["name"] + " / exact nearest"])
    axs[0].imshow(np.asarray(strips) / 255, aspect="auto", interpolation="nearest", extent=[0, len(profile) - 1, len(strips) - .5, -.5])
    axs[0].set(yticks=range(len(labels)), yticklabels=labels, title="Actual image samples and the frozen-palette nearest-color model")
    for channel, ax in enumerate(axs[1:]):
        ax.plot(profile[:, channel], color="#20262c", lw=2, label="Source")
        for entry in result["outputs"]:
            line, = ax.plot(np.array(entry["actual_rgb"])[:, channel], lw=1, label=entry["name"] + " actual")
            ax.plot(np.array(entry["nearest_rgb"])[:, channel], ls="--", color=line.get_color(), lw=1.2, label=entry["name"] + " nearest")
        ax.set(ylabel="RGB"[channel], xlim=(0, len(profile) - 1)); ax.grid(alpha=.15)
    axs[1].legend(fontsize=8, ncol=2)
    axs[-1].set_xlabel("Pixel sample along selected spatial line")
    save_figure(fig, directory, "02-gradient-profile")
    # Shared limits permit a fair visual comparison; nearby points are a display
    # filter only. All palette entries participated in every distance query.
    show = []
    for entry in result["outputs"]:
        p = np.array(entry["palette_rgb"], dtype=float)
        near = np.all((p >= profile.min(0) - 28) & (p <= profile.max(0) + 28), axis=1)
        near[np.unique(entry["nearest_indices"])] = True
        show.append(np.flatnonzero(near))
    visible_colors = np.vstack([profile] + [np.array(e["palette_rgb"])[which] for e, which in zip(result["outputs"], show)])
    rgb_low, rgb_high = np.maximum(0, visible_colors.min(0) - 5), np.minimum(255, visible_colors.max(0) + 5)
    fig = plt.figure(figsize=(6.6 * count, 6), layout="constrained")
    for j, (entry, which) in enumerate(zip(result["outputs"], show)):
        ax = fig.add_subplot(1, count, j + 1, projection="3d")
        p = np.array(entry["palette_rgb"], dtype=float)
        ax.scatter(*p[which].T, c=p[which] / 255, edgecolors="#444", linewidths=.6, s=42)
        ax.plot(*profile.T, color=ACCENT, lw=1.8, label="Source pixels")
        ax.plot(*profile[[0, -1]].T, color="black", ls="--", lw=1, label="Endpoint chord")
        for index in np.unique(entry["nearest_indices"]):
            ax.text(*p[index], " " + str(entry["palette_ids"][index]), fontsize=8)
        ax.set(xlabel="R", ylabel="G", zlabel="B", title=entry["name"] + " / RGB palette sites",
               xlim=(rgb_low[0], rgb_high[0]), ylim=(rgb_low[1], rgb_high[1]), zlim=(rgb_low[2], rgb_high[2]))
        ax.set_box_aspect(np.maximum(rgb_high - rgb_low, 12)); ax.view_init(23, -54)
        if j == 0:
            ax.legend(fontsize=8)
    save_figure(fig, directory, "03-rgb-3d")
    slice_height = max(3.1, min(7., 7 * (bounds[3] - bounds[2]) / (bounds[1] - bounds[0]) + 1.6))
    fig, axs = plt.subplots(1, count, figsize=(7 * count, slice_height), squeeze=False, layout="constrained")
    for ax, entry in zip(axs[0], result["outputs"]):
        p = np.array(entry["palette_rgb"], dtype=float)
        for cell in entry["slice_cells"]:
            index = cell["index"]
            ax.add_patch(Polygon(cell["vertices"], facecolor=p[index] / 255, edgecolor="#6c747c", linewidth=.6))
            site = cell["site_xy"]
            if low[0] <= site[0] <= high[0] and low[1] <= site[1] <= high[1]:
                ax.scatter(*site, c=[p[index] / 255], s=32, edgecolors="#111", zorder=4)
        ax.plot(*projected.T, color=ACCENT, lw=2, label="Source projected into plane")
        ax.plot(*projected[[0, -1]].T, "k--", lw=1, label="Endpoint chord projection")
        ax.set(xlim=bounds[:2], ylim=bounds[2:], xlabel=plane["axes"][0], ylabel=plane["axes"][1], aspect="equal",
               title=entry["name"] + " / " + plane["label"])
    fig.suptitle(f"All palette sites compete in 3D; circles mark their plane projections.\n"
                 f"Source off-plane distance: RMS {plane['rms_off_plane']:.2f}, max {plane['max_off_plane']:.2f} RGB units.", fontsize=11)
    handles, labels = axs[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="outside lower center", fontsize=8, ncol=2, frameon=False)
    fig.canvas.draw()
    for ax, entry in zip(axs[0], result["outputs"]):
        label_slice_sites(ax, entry)
    save_figure(fig, directory, "04-voronoi-slice")
    t = np.linspace(0, 1, 1025)
    chord = profile[0] + t[:, None] * (profile[-1] - profile[0])
    fig, axs = plt.subplots(2, 1, figsize=(11, 4.8), layout="constrained", height_ratios=[1, 1.4])
    colors = [chord]
    for entry in result["outputs"]:
        p = np.array(entry["palette_rgb"], dtype=float)
        selected = p[nearest(chord, p)]
        colors.append(selected)
        axs[1].plot(t, np.linalg.norm(chord - selected, axis=1), label=entry["name"])
    axs[0].imshow(np.asarray(colors) / 255, aspect="auto", extent=[0, 1, len(colors) - .5, -.5], interpolation="nearest")
    axs[0].set(yticks=range(len(colors)), yticklabels=["Source endpoint chord"] + [e["name"] for e in result["outputs"]])
    axs[1].set(xlim=(0, 1), xlabel="t along the RGB endpoint chord (a model, not the spatial scanline)", ylabel="RGB distance to nearest site")
    axs[1].legend(); axs[1].grid(alpha=.15)
    save_figure(fig, directory, "05-chord-assignment")
    (directory / "analysis.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    write_report(result, directory)
    return result


def write_report(result, directory):
    sections = []
    for entry in result["outputs"]:
        rows = []
        for cell in entry["slice_cells"]:
            i = cell["index"]
            rows.append(f'<tr><td>{entry["palette_ids"][i]}</td><td>{tuple(entry["palette_rgb"][i])}</td>'
                        f'<td>{cell["off_plane_distance"]:.3f}</td><td>{cell["area"]:.3f}</td></tr>')
        sections.append(f'<h2>{html.escape(entry["name"])}</h2><p>{html.escape(entry["palette_origin"])}</p>'
                        '<div class="table-wrap"><table><tr><th>Site ID</th><th>RGB</th><th>Off-plane distance</th><th>Clipped cell area (RGB²)</th></tr>'
                        + ''.join(rows) + '</table></div>')
    figures = ''.join(f'<a href="{name}.png"><img src="{name}.png" alt="{label}"></a>' for name, label in [
        ("01-selected-gradient", "Selected source and output line"), ("02-gradient-profile", "Actual and nearest-color RGB profiles"),
        ("03-rgb-3d", "RGB palette sites and source trajectory"), ("04-voronoi-slice", "Exact Voronoi plane intersections and projected palette sites"),
        ("05-chord-assignment", "Endpoint chord and nearest-color error")])
    body = ('<h1>Palette gradient analysis</h1><p>One selected spatial line, measured in 8-bit RGB code values. '
            'The source pixels, straight endpoint chord, and plane projection are distinct. This is a local diagnostic, not a perceptual quality ranking.</p>'
            '<p>Actual output includes the encoder’s diffusion, lookup and output rounding; the nearest-color model does not simulate these stages. '
            'No ICC conversion is performed. Input coordinates follow EXIF orientation.</p>'
            f'<p>Selected endpoints: {html.escape(str(result["line"]))}. Plane: {html.escape(result["plane"]["label"])}. '
            f'RMS / max off-plane error: {result["plane"]["rms_off_plane"]:.3f} / {result["plane"]["max_off_plane"]:.3f}.</p>'
            '<p><a href="analysis.json">Exact data, sites and analytic chord intervals</a> · <a href="selection.json">Reusable source selection</a></p>'
            + figures + ''.join(sections))
    (directory / "report.html").write_text('<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
                                          '<title>Palette gradient analysis</title><style>' + CSS + '</style><main>' + body + '</main></html>', encoding="utf-8")


CSS = '''*{box-sizing:border-box}body{margin:0;background:#f4f6f7;color:#23313b;font:15px/1.65 system-ui,sans-serif}main{max-width:1440px;margin:auto;padding:28px}h1{font-size:27px;line-height:1.4}h2{font-size:19px}button,select{font:inherit;padding:8px 12px;border:1px solid #adb8c2;border-radius:5px;background:white}button{cursor:pointer}button.primary{background:#235f7f;color:white}button:disabled{opacity:.5;cursor:wait}button:focus-visible,select:focus-visible,a:focus-visible{outline:3px solid #a32670;outline-offset:3px}input{font:inherit;padding:6px;max-width:90px;border:1px solid #adb8c2;border-radius:4px}details{margin:12px 0}.toolbar{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin:16px 0}.panels{display:grid;grid-template-columns:1fr 1fr;gap:18px}.panel{min-width:0;background:white;padding:14px;border:1px solid #d7dfe4;border-radius:8px}svg,img{display:block;width:100%;height:auto}.note{font-size:13px;color:#526370}.status{min-height:30px}.table-wrap{overflow:auto}table{border-collapse:collapse;width:100%;font-size:13px}td,th{border-bottom:1px solid #dce3e7;padding:7px;text-align:left}a{color:#185d83}#results img{margin:20px 0}pre{white-space:pre-wrap;overflow-wrap:anywhere}@media(max-width:760px){main{padding:16px}.panels{grid-template-columns:1fr}h1{font-size:23px}.toolbar>*{max-width:100%}}'''

UI = '''<!doctype html><html lang="ja"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Palette gradient — select a source region</title><style>CSS_HERE</style><main>
<h1>グラデーションを1本選ぶ</h1><p>元画像だけから抽出した候補です。候補を切り替えるか、元画像上で線分の両端を指定してください。</p>
<div class="toolbar"><label>候補 <select id="candidate" aria-label="グラデーション候補"></select></label><button id="previous">前へ</button><button id="next">次へ</button><button id="manual" aria-pressed="false">両端を指定</button><button id="generate" class="primary">この経路で図を作成</button></div>
<details><summary>座標・断面を指定</summary><div class="toolbar"><label>始点X <input id="x0" type="number" min="0" aria-label="始点X"></label><label>Y <input id="y0" type="number" min="0" aria-label="始点Y"></label><label>終点X <input id="x1" type="number" min="0" aria-label="終点X"></label><label>Y <input id="y1" type="number" min="0" aria-label="終点Y"></label><button id="apply-line">座標を適用</button></div><div class="toolbar"><label>断面 <select id="plane" aria-label="断面"><option value="auto">ソースから自動選択</option value="R">Rを固定</option><option value="G">Gを固定</option><option value="B">Bを固定</option></select></label><label>固定値 <input id="plane-value" type="number" min="0" max="255" value="252" aria-label="断面の固定値" disabled></label></div></details>
<p id="details" class="note"></p><p id="status" class="status" role="status" aria-live="polite"></p>
<div class="panels"><section class="panel"><h2>ソース画像</h2><svg id="source" role="img" aria-label="ソース画像上の選択経路"></svg></section><section class="panel"><h2>パレット化画像</h2><img src="/input-quantized.png" alt="比較するパレット化画像"><p id="palette-note" class="note"></p></section></div>
<p class="note">RGB符号値で解析します。自動リサイズ・ICC色変換は行いません。画像の色変換やリサイズを伴うエンコードでは、対応する処理後のソース画像を用意してください。</p>
<div id="results"></div></main><script>
const state=STATE_HERE;let selected=state.candidates[0]?.line||null,manual=false,first=null;
const select=document.querySelector('#candidate'),planeSelect=document.querySelector('#plane'),planeValue=document.querySelector('#plane-value'),svg=document.querySelector('#source'),status=document.querySelector('#status'),button=document.querySelector('#generate');
for(const c of state.candidates){const o=document.createElement('option');o.value=c.id;o.textContent=`候補 ${c.id} · 色の変化 ${c.rgb_span.toFixed(1)}`;select.append(o);}
if(!state.candidates.length){const o=document.createElement('option');o.textContent='自動候補なし — 両端を指定してください';select.append(o);}
if(state.slice){planeSelect.value=state.slice[0];planeValue.value=state.slice[1];planeValue.disabled=false;}planeSelect.onchange=()=>{planeValue.disabled=planeSelect.value==='auto';clearResults();};planeValue.oninput=clearResults;
svg.setAttribute('viewBox',`0 0 ${state.width} ${state.height}`);document.querySelector('#palette-note').textContent=state.palette_note;
function clearResults(){document.querySelector('#results').innerHTML='';}
function draw(){clearResults();if(selected){for(const [id,value] of [['x0',selected[0][0]],['y0',selected[0][1]],['x1',selected[1][0]],['y1',selected[1][1]]])document.querySelector('#'+id).value=value;}const stroke=Math.max(state.width/350,2);svg.innerHTML=`<image href="/input-source.png" width="${state.width}" height="${state.height}"/>`;
if(selected){const [[x0,y0],[x1,y1]]=selected;svg.innerHTML+=`<line x1="${x0}" y1="${y0}" x2="${x1}" y2="${y1}" stroke="white" stroke-width="${stroke*3}"/><line x1="${x0}" y1="${y0}" x2="${x1}" y2="${y1}" stroke="#a32670" stroke-width="${stroke}"/><circle cx="${x0}" cy="${y0}" r="${stroke*2}" fill="#a32670"/><circle cx="${x1}" cy="${y1}" r="${stroke*2}" fill="#a32670"/>`;}
if(first)svg.innerHTML+=`<circle cx="${first[0]}" cy="${first[1]}" r="${stroke*3}" fill="#a32670"/>`;
document.querySelector('#details').textContent=selected?`経路: (${selected[0]}) → (${selected[1]}) · 座標は元画像のピクセル`:'元画像上で2点を指定すると解析できます。';button.disabled=!selected||!!first;}
function choose(){selected=state.candidates.find(c=>c.id===Number(select.value))?.line||null;manual=false;first=null;document.querySelector('#manual').setAttribute('aria-pressed','false');status.textContent='';draw();}
document.querySelector('#apply-line').onclick=()=>{const v=['x0','y0','x1','y1'].map(id=>Number(document.querySelector('#'+id).value));if(!v.every(Number.isInteger)||v[0]<0||v[2]<0||v[1]<0||v[3]<0||v[0]>=state.width||v[2]>=state.width||v[1]>=state.height||v[3]>=state.height){status.textContent='画像内の整数座標を入力してください。';return;}selected=[[v[0],v[1]],[v[2],v[3]]];first=null;manual=false;document.querySelector('#manual').setAttribute('aria-pressed','false');status.textContent='指定した経路で図を作成できます。';draw();};
select.onchange=choose;document.querySelector('#previous').onclick=()=>{select.selectedIndex=Math.max(0,select.selectedIndex-1);choose();};document.querySelector('#next').onclick=()=>{select.selectedIndex=Math.min(select.options.length-1,select.selectedIndex+1);choose();};
document.querySelector('#manual').onclick=()=>{manual=!manual;first=null;document.querySelector('#manual').setAttribute('aria-pressed',String(manual));status.textContent=manual?'元画像で始点と終点をクリックしてください。':'';draw();};
svg.onclick=e=>{if(!manual)return;const box=svg.getBoundingClientRect();const p=[Math.min(state.width-1,Math.max(0,Math.round((e.clientX-box.left)/box.width*state.width))),Math.min(state.height-1,Math.max(0,Math.round((e.clientY-box.top)/box.height*state.height)))];if(!first){first=p;status.textContent='終点をクリックしてください。';}else{selected=[first,p];first=null;manual=false;document.querySelector('#manual').setAttribute('aria-pressed','false');status.textContent='指定した経路で図を作成できます。';}draw();};
function generationRequest(){return {token:state.token,line:selected,slice:planeSelect.value==='auto'?null:[planeSelect.value,Number(planeValue.value)],candidate:state.candidates.find(c=>JSON.stringify(c.line)===JSON.stringify(selected))?.id||null};}
button.onclick=async()=>{const request=generationRequest();button.disabled=true;status.textContent='RGBの経路とボロノイ断面を計算しています…';try{const response=await fetch('/generate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(request)});const data=await response.json();if(!response.ok)throw Error(data.error);if(JSON.stringify(request)!==JSON.stringify(generationRequest())){status.textContent='選択が変わりました。現在の経路で再生成してください。';return;}const stamp=Date.now();document.querySelector('#results').innerHTML=`<h2>解析結果</h2><p><a href="/report.html" target="_blank">図・代表色の表・測定条件を開く</a> · <a href="/selection.json">選択を保存</a></p>`+data.figures.map(n=>`<a href="/${n}.png?v=${stamp}" target="_blank"><img src="/${n}.png?v=${stamp}" alt="${n}"></a>`).join('');status.textContent='図を保存しました。別の経路を選んで再生成できます。';}catch(error){status.textContent=error.message;}finally{button.disabled=false;}};draw();
</script></html>'''


def parse_line(text):
    try:
        values = [int(x) for x in text.split(",")]
        if len(values) != 4:
            raise ValueError
    except ValueError as error:
        raise argparse.ArgumentTypeError("use x0,y0,x1,y1") from error
    return [values[:2], values[2:]]


def parse_roi(text):
    flat = sum(parse_line(text), [])
    return flat


def parse_plane(text):
    try:
        channel, value = text.upper().split("=")
        value = float(value)
        if channel not in ("R", "G", "B") or not 0 <= value <= 255:
            raise ValueError
    except ValueError as error:
        raise argparse.ArgumentTypeError("use R=VALUE, G=VALUE or B=VALUE in 0..255") from error
    return channel, value


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path, help="source image before quantization")
    parser.add_argument("quantized", type=Path, help="aligned lossless quantized image (1–256 colors)")
    parser.add_argument("--compare", action="append", default=[], metavar="NAME=IMAGE", help="additional aligned output image (up to two)")
    parser.add_argument("--output", type=Path, default=Path("palette-gradient-analysis"), help="artifact directory")
    parser.add_argument("--candidates", type=int, default=16, help="maximum source-only candidates (1–32)")
    parser.add_argument("--roi", type=parse_roi, help="limit extraction to x,y,width,height")
    batch = parser.add_mutually_exclusive_group()
    batch.add_argument("--line", type=parse_line, help="render specified endpoints directly: x0,y0,x1,y1")
    batch.add_argument("--candidate", type=int, help="render numbered automatic candidate without browser")
    batch.add_argument("--selection", type=Path, help="reuse a saved source selection without browser")
    parser.add_argument("--slice", type=parse_plane, help="explicit RGB slice, e.g. G=252; otherwise chosen from source")
    parser.add_argument("--no-open", action="store_true", help="print local URL without opening a browser")
    parser.add_argument("--port", type=int, default=0, help="loopback port (default: automatic)")
    args = parser.parse_args(argv)
    if not 1 <= args.candidates <= 32 or not 0 <= args.port <= 65535 or len(args.compare) > 2:
        parser.error("candidates must be 1–32, port 0–65535, and at most two --compare images are supported")
    try:
        source, _, source_record = image_data(args.source)
        outputs = []
        specifications = [(args.quantized.stem, args.quantized)]
        for value in args.compare:
            name, sep, filename = value.partition("=")
            if not sep or not name or not filename:
                raise ValueError("--compare expects NAME=IMAGE")
            specifications.append((name, Path(filename)))
        for name, path in specifications:
            rgba, table, record = image_data(path)
            if rgba.shape != source.shape:
                raise ValueError("Source and output dimensions must match after EXIF orientation; no automatic resize is performed")
            outputs.append({"name": name, "rgba": rgba, "record": record, "palette": output_palette(rgba, table)})
        args.output = args.output.resolve()
        filenames = {"input-source.png", "input-quantized.png", "selection.json", "analysis.json", "report.html", "candidates.json"}
        filenames.update(n + ext for n in FIGURE_NAMES for ext in (".png", ".svg"))
        reserved = {(args.output / name).resolve() for name in filenames | {"server-url.txt"}}
        if any(Path(r["path"]) in reserved for r in [source_record] + [o["record"] for o in outputs]):
            raise ValueError("Choose an output directory whose generated files do not overwrite an input")
        args.output.mkdir(parents=True, exist_ok=True)
        Image.fromarray(source).save(args.output / "input-source.png")
        Image.fromarray(outputs[0]["rgba"]).save(args.output / "input-quantized.png")
        candidates = [] if args.line or args.selection else discover_gradients(source, args.candidates, args.roi)
        extraction = {"version": VERSION, "source": source_record, "candidates": candidates,
                      "method": "Source-only structure tensor, multiscale directional scans and smoothness filters",
                      "roi": args.roi, "candidates_limit": args.candidates}
        (args.output / "candidates.json").write_text(json.dumps(extraction, indent=2), encoding="utf-8")

        def generate(line, candidate=None, chosen_plane=None):
            line_pixels(source, line)
            selection = {"version": VERSION, "source": source_record, "line": line, "candidate": candidate,
                         "slice": list(chosen_plane) if chosen_plane else None, "roi": args.roi}
            result = render_analysis(source, outputs, line, args.output, selection, chosen_plane)
            (args.output / "selection.json").write_text(json.dumps(selection, indent=2), encoding="utf-8")
            return result

        if args.selection:
            saved = json.loads(args.selection.read_text(encoding="utf-8"))
            if saved.get("version") != VERSION or saved.get("source", {}).get("sha256") != source_record["sha256"]:
                raise ValueError("Saved selection belongs to a different source image or unsupported version")
            if args.slice is None and saved.get("slice"):
                args.slice = parse_plane("%s=%s" % tuple(saved["slice"]))
            generate(saved["line"], saved.get("candidate"), args.slice)
        elif args.line:
            generate(args.line, chosen_plane=args.slice)
        elif args.candidate is not None:
            if not 1 <= args.candidate <= len(candidates):
                raise ValueError(f"Candidate must be in 1..{len(candidates)}; use browser manual selection if no candidates were found")
            generate(candidates[args.candidate - 1]["line"], args.candidate, args.slice)
        else:
            token = secrets.token_urlsafe(24)
            state = {"width": source.shape[1], "height": source.shape[0], "candidates": candidates,
                     "palette_note": ("インデックス画像の色テーブルを使用。未使用の不透明色も含みます。" if outputs[0]["record"]["mode"] == "P" else "出力画像に現れた不透明色を使用。未使用のパレット色は復元できません。"), "token": token, "slice": args.slice}
            encoded = json.dumps(state).replace("<", "\\u003c")
            page = UI.replace("CSS_HERE", CSS).replace("STATE_HERE", encoded).encode()

            class Handler(BaseHTTPRequestHandler):
                def log_message(self, format, *values):
                    pass

                def send(self, code, data, mime):
                    self.send_response(code)
                    self.send_header("Content-Type", mime)
                    self.send_header("Content-Length", str(len(data)))
                    self.send_header("Cache-Control", "no-store")
                    self.end_headers()
                    self.wfile.write(data)

                def valid_host(self):
                    # A loopback bind alone does not reject a rebinding Host.
                    allowed = {f"127.0.0.1:{server.server_port}", f"localhost:{server.server_port}"}
                    if self.headers.get("Host") not in allowed:
                        self.send(403, b'{"error":"Loopback host required"}', "application/json")
                        return False
                    return True

                def do_GET(self):
                    if not self.valid_host():
                        return
                    path = self.path.split("?", 1)[0]
                    if path == "/":
                        self.send(200, page, "text/html; charset=utf-8")
                    elif path[1:] in filenames and (args.output / path[1:]).is_file():
                        file = args.output / path[1:]
                        mime = {".png": "image/png", ".svg": "image/svg+xml", ".json": "application/json", ".html": "text/html; charset=utf-8"}[file.suffix]
                        self.send(200, file.read_bytes(), mime)
                    else:
                        self.send(404, b"Not found", "text/plain")

                def do_POST(self):
                    if not self.valid_host():
                        return
                    try:
                        length = int(self.headers.get("Content-Length", "0"))
                        if self.path != "/generate" or not 0 < length <= 4096:
                            raise ValueError("Invalid generation request")
                        request = json.loads(self.rfile.read(length))
                        if not isinstance(request, dict):
                            raise ValueError("Expected a generation request object")
                        if request.get("token") != token:
                            self.send(403, b'{"error":"Invalid session"}', "application/json")
                            return
                        chosen_plane = request.get("slice")
                        if chosen_plane is not None:
                            if not isinstance(chosen_plane, list) or len(chosen_plane) != 2:
                                raise ValueError("Invalid slice")
                            chosen_plane = parse_plane("%s=%s" % tuple(chosen_plane))
                        generate(request["line"], request.get("candidate"), chosen_plane)
                        self.send(200, json.dumps({"figures": FIGURE_NAMES}).encode(), "application/json")
                    except (ValueError, KeyError, TypeError, argparse.ArgumentTypeError) as error:
                        self.send(400, json.dumps({"error": str(error)}, ensure_ascii=False).encode(), "application/json")

            server = HTTPServer(("127.0.0.1", args.port), Handler)
            url = f"http://127.0.0.1:{server.server_port}/"
            (args.output / "server-url.txt").write_text(url + "\n", encoding="utf-8")
            print(f"{len(candidates)} candidates. Select a gradient: {url}\nArtifacts: {args.output}\nPress Ctrl-C to stop the server; saved figures remain available.", flush=True)
            if not args.no_open:
                webbrowser.open(url)
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                pass
            finally:
                server.server_close()
            return 0
        print(f"Analysis saved: {args.output / 'report.html'}")
        return 0
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    sys.exit(main())
