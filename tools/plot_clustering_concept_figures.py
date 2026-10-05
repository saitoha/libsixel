#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the palette clustering concept figures.

Use Matplotlib 3.10, NumPy and Pillow.  Run without arguments to regenerate
the figures under docs/concepts/clustering/figures, or with --check to verify
that the committed files are current.  The algorithms are implemented here in
their textbook form to illustrate the ideas; they are not libsixel's
implementations, so the figures do not measure libsixel.
"""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/concepts/clustering/figures'
INK = '#183247'
MUTED = '#4c6070'
GRID = '#d9e1e7'
ACCENT = '#c0392b'
RC = {'svg.fonttype': 'none', 'svg.hashsalt': 'libsixel-clustering-concepts',
      'font.family': 'sans-serif', 'font.sans-serif': ['DejaVu Sans', 'Arial'],
      'font.size': 12, 'axes.edgecolor': MUTED, 'axes.labelcolor': INK,
      'xtick.color': MUTED, 'ytick.color': MUTED, 'axes.titlesize': 12,
      'axes.titlecolor': INK}
SEED = 20261006
CLUSTER_COLORS = ['#4e79a7', '#f28e2b', '#59a14f', '#e15759', '#76b7b2',
                  '#edc948', '#b07aa1', '#9c755f']


def mpl_svg(fig):
    buffer = io.StringIO()
    fig.savefig(buffer, format='svg', metadata={'Date': None, 'Creator': None},
                bbox_inches='tight', pad_inches=0.15, facecolor='white')
    plt.close(fig)
    return buffer.getvalue()


def bare(ax):
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_color(GRID)


# --------------------------------------------------------------------------
# Color conversion (sRGB, D65) for the image figures


def srgb_to_linear(encoded):
    return np.where(encoded <= 0.04045, encoded / 12.92,
                    ((encoded + 0.055) / 1.055) ** 2.4)


def srgb_to_lab(rgb):
    linear = srgb_to_linear(rgb)
    matrix = np.array([[0.4124, 0.3576, 0.1805],
                       [0.2126, 0.7152, 0.0722],
                       [0.0193, 0.1192, 0.9505]])
    xyz = linear @ matrix.T / np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]),
                     200 * (f[..., 1] - f[..., 2])], axis=-1)


def snake_pixels(width=240, height=180):
    image = Image.open(ROOT / 'images/snake.png').convert('RGB')
    image = image.resize((width, height), Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=float) / 255.0


# --------------------------------------------------------------------------
# Textbook clustering algorithms


def assign(points, centers):
    distance = ((points[:, None, :] - centers[None, :, :]) ** 2).sum(axis=2)
    return distance.argmin(axis=1)


def median_cut(points, count):
    """Split the box with the longest side at the median of that side."""
    boxes = [points]
    history = [list(boxes)]
    while len(boxes) < count:
        extents = [(box.max(axis=0) - box.min(axis=0)).max() if len(box) > 1 else -1
                   for box in boxes]
        index = int(np.argmax(extents))
        box = boxes.pop(index)
        axis = int(np.argmax(box.max(axis=0) - box.min(axis=0)))
        order = np.argsort(box[:, axis], kind='stable')
        half = len(box) // 2
        boxes[index:index] = [box[order[:half]], box[order[half:]]]
        history.append(list(boxes))
    return boxes, history


def kmeans(points, centers, iterations):
    trail = [centers.copy()]
    for _ in range(iterations):
        labels = assign(points, centers)
        updated = centers.copy()
        for k in range(len(centers)):
            members = points[labels == k]
            if len(members):
                updated[k] = members.mean(axis=0)
        centers = updated
        trail.append(centers.copy())
    return centers, trail


def farthest_first(points, count):
    """Gonzalez's farthest-point traversal for the k-center objective."""
    centers = [points[0]]
    distance = ((points - points[0]) ** 2).sum(axis=1)
    for _ in range(count - 1):
        index = int(np.argmax(distance))
        centers.append(points[index])
        distance = np.minimum(distance, ((points - points[index]) ** 2).sum(axis=1))
    return np.array(centers)


def blobs(rng, specs):
    parts = [rng.normal(center, spread, size=(count, 2))
             for center, spread, count in specs]
    return np.concatenate(parts)


# --------------------------------------------------------------------------
# Figures


def color_cloud():
    rgb = snake_pixels().reshape(-1, 3)
    rng = np.random.default_rng(SEED)
    sample = rgb[rng.choice(len(rgb), 6000, replace=False)]
    lab = srgb_to_lab(sample)
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 2, figsize=(7.2, 3.6), gridspec_kw={'wspace': 0.3})
        axes[0].scatter(lab[:, 1], lab[:, 2], c=sample, s=5, linewidths=0)
        axes[0].set_xlabel('a*  (green ← → red)')
        axes[0].set_ylabel('b*  (blue ← → yellow)')
        axes[0].set_title('Seen from above: hue and chroma')
        axes[1].scatter(lab[:, 2], lab[:, 0], c=sample, s=5, linewidths=0)
        axes[1].set_xlabel('b*  (blue ← → yellow)')
        axes[1].set_ylabel('L*  (dark ← → light)')
        axes[1].set_title('Seen from the side: lightness')
        for ax in axes:
            ax.grid(color=GRID, lw=0.8)
            ax.set_axisbelow(True)
        fig.suptitle('Every pixel of the photograph is a point in color space',
                     fontsize=13, fontweight='bold', color=INK, y=1.02)
        return mpl_svg(fig)


def voronoi():
    rng = np.random.default_rng(SEED)
    points = blobs(rng, [((-2.2, 1.6), 0.45, 220), ((1.8, 2.0), 0.6, 160),
                         ((0.2, -0.2), 0.35, 260), ((-1.6, -2.0), 0.5, 120),
                         ((2.4, -1.6), 0.4, 140)])
    centers, _ = kmeans(points, points[rng.choice(len(points), 5, replace=False)], 25)
    labels = assign(points, centers)
    grid_x, grid_y = np.meshgrid(np.linspace(-4, 4, 400), np.linspace(-4, 4, 400))
    region = assign(np.stack([grid_x.ravel(), grid_y.ravel()], axis=1),
                    centers).reshape(grid_x.shape)
    with plt.rc_context(RC):
        fig, ax = plt.subplots(figsize=(5.4, 4.6))
        light = [matplotlib.colors.to_rgba(c, 0.18) for c in CLUSTER_COLORS[:5]]
        ax.imshow(np.array(light)[region], extent=(-4, 4, -4, 4), origin='lower',
                  interpolation='nearest')
        ax.contour(grid_x, grid_y, region, levels=np.arange(0.5, 4.5), colors=MUTED,
                   linewidths=1.2)
        ax.scatter(points[:, 0], points[:, 1], s=6,
                   c=[CLUSTER_COLORS[k] for k in labels], linewidths=0)
        ax.scatter(centers[:, 0], centers[:, 1], s=150, c='white', edgecolors=INK,
                   linewidths=2.2, marker='o', zorder=3)
        ax.scatter(centers[:, 0], centers[:, 1], s=30, c=INK, zorder=4)
        ax.set_xlim(-4, 4)
        ax.set_ylim(-4, 4)
        bare(ax)
        ax.set_title('A palette divides color space into regions', fontsize=13,
                     fontweight='bold')
        ax.text(0.0, -0.06, 'Rings: palette colors. Shaded areas: all colors that map to '
                'each palette color (Voronoi regions).', transform=ax.transAxes,
                fontsize=10, color=MUTED, va='top', wrap=True)
        return mpl_svg(fig)


def draw_boxes(ax, boxes, colors=True):
    for index, box in enumerate(boxes):
        lo, hi = box.min(axis=0), box.max(axis=0)
        color = CLUSTER_COLORS[index % len(CLUSTER_COLORS)]
        ax.scatter(box[:, 0], box[:, 1], s=4, c=color if colors else MUTED, linewidths=0)
        ax.add_patch(matplotlib.patches.Rectangle(lo, *(hi - lo), fill=False,
                                                  edgecolor=INK, lw=1.4))
        mean = box.mean(axis=0)
        ax.scatter([mean[0]], [mean[1]], s=60, c='white', edgecolors=INK,
                   linewidths=1.8, zorder=3)


def median_cut_figure():
    rng = np.random.default_rng(SEED + 1)
    points = blobs(rng, [((-2.0, 1.4), 0.6, 300), ((1.6, 1.8), 0.5, 160),
                         ((0.4, -1.2), 0.8, 300), ((2.6, -2.2), 0.35, 80)])
    _, history = median_cut(points, 8)
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 4, figsize=(7.4, 2.3), gridspec_kw={'wspace': 0.06})
        for ax, count in zip(axes, (1, 2, 4, 8)):
            draw_boxes(ax, history[count - 1])
            ax.set_xlim(-4.2, 4.2)
            ax.set_ylim(-4.2, 4.2)
            ax.set_aspect('equal')
            bare(ax)
            ax.set_title(f'{count} box{"es" if count > 1 else ""}')
        return mpl_svg(fig)


def kmeans_figure():
    rng = np.random.default_rng(SEED + 2)
    points = blobs(rng, [((-2.0, 1.8), 0.5, 200), ((2.0, 1.6), 0.5, 200),
                         ((0.0, -1.8), 0.6, 240), ((2.6, -2.4), 0.35, 80)])
    start = np.array([[-0.5, 0.5], [0.0, 0.2], [0.4, 0.6], [-0.2, -0.3]])
    _, trail = kmeans(points, start.copy(), 12)
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 4, figsize=(7.4, 2.3), gridspec_kw={'wspace': 0.06})
        for ax, (step, title) in zip(axes, ((0, 'Start'), (1, 'After 1 step'),
                                            (3, 'After 3 steps'), (12, 'Converged'))):
            centers = trail[step]
            labels = assign(points, centers)
            ax.scatter(points[:, 0], points[:, 1], s=4,
                       c=[CLUSTER_COLORS[k] for k in labels], linewidths=0)
            path = np.array(trail[:step + 1])
            for k in range(len(centers)):
                ax.plot(path[:, k, 0], path[:, k, 1], color=INK, lw=1, alpha=0.6)
            ax.scatter(centers[:, 0], centers[:, 1], s=70, c='white', edgecolors=INK,
                       linewidths=2, zorder=3)
            ax.set_xlim(-4, 4)
            ax.set_ylim(-4, 4)
            ax.set_aspect('equal')
            bare(ax)
            ax.set_title(title)
        return mpl_svg(fig)


def mean_vs_max():
    rng = np.random.default_rng(SEED + 3)
    big = blobs(rng, [((-1.0, -0.4), 1.0, 900)])
    # Keep the common colors in a compact cloud so that the comparison is
    # about the small vivid group, not about stray outliers.
    big = big[np.hypot(big[:, 0] + 1.0, big[:, 1] + 0.4) < 2.6]
    # Splitting the large cloud lowers the summed squared error more than
    # covering these few points does, so k-means leaves them uncovered.
    small = blobs(rng, [((3.4, 2.8), 0.15, 8)])
    points = np.concatenate([big, small])
    start = points[rng.choice(len(big), 3, replace=False)]
    mean_centers, _ = kmeans(points, start, 30)
    max_centers = farthest_first(points, 3)
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 2, figsize=(7.2, 3.3), gridspec_kw={'wspace': 0.08})
        for ax, centers, title in ((axes[0], mean_centers, 'Minimize average error (k-means)'),
                                   (axes[1], max_centers, 'Minimize worst error (k-center)')):
            ax.scatter(big[:, 0], big[:, 1], s=5, c='#8a9aa6', linewidths=0)
            ax.scatter(small[:, 0], small[:, 1], s=16, c=ACCENT, linewidths=0)
            ax.scatter(centers[:, 0], centers[:, 1], s=120, c='white', edgecolors=INK,
                       linewidths=2.2, zorder=3)
            worst = np.sqrt(((points[:, None] - centers[None]) ** 2).sum(axis=2).min(axis=1)).max()
            ax.set_title(f'{title}\nworst distance {worst:.1f}', fontsize=11.5)
            ax.set_xlim(-4.5, 4.5)
            ax.set_ylim(-3.5, 4)
            ax.set_aspect('equal')
            bare(ax)
        axes[0].annotate('a small but vivid\ngroup of colors', xy=(3.4, 2.8),
                         xytext=(0.4, 3.2), fontsize=10, color=ACCENT,
                         arrowprops={'arrowstyle': '->', 'color': ACCENT})
        return mpl_svg(fig)


def palette_results():
    rgb = snake_pixels()
    pixels = rgb.reshape(-1, 3)
    lab = srgb_to_lab(pixels)
    corners = np.array([[r, g, b] for r in (0.0, 1.0) for g in (0.0, 1.0) for b in (0.0, 1.0)])
    boxes, _ = median_cut(pixels, 8)
    cut = np.array([box.mean(axis=0) for box in boxes])
    rng = np.random.default_rng(SEED)
    sample = pixels[rng.choice(len(pixels), 8000, replace=False)]
    means, _ = kmeans(sample, cut.copy(), 20)
    results = {}
    with plt.rc_context(RC):
        fig = plt.figure(figsize=(7.4, 2.6))
        grid = fig.add_gridspec(2, 3, height_ratios=[5, 0.55], hspace=0.04, wspace=0.05)
        for column, (name, palette) in enumerate((('Fixed RGB corners', corners),
                                                  ('Median cut', cut),
                                                  ('k-means', means))):
            labels = assign(pixels, palette)
            mapped = palette[labels]
            error = np.sqrt(((srgb_to_lab(mapped) - lab) ** 2).sum(axis=1)).mean()
            results[name] = round(float(error), 2)
            ax = fig.add_subplot(grid[0, column])
            ax.imshow(mapped.reshape(rgb.shape), interpolation='none')
            ax.set_title(f'{name}\nmean ΔE*ab {error:.1f}', fontsize=11.5)
            bare(ax)
            swatch = fig.add_subplot(grid[1, column])
            order = np.argsort(srgb_to_lab(palette)[:, 0])
            swatch.imshow(palette[order][None, :, :], interpolation='none', aspect='auto')
            bare(swatch)
        fig.suptitle('Eight colors chosen three ways (no dithering)', fontsize=13,
                     fontweight='bold', color=INK, y=1.04)
        return mpl_svg(fig), results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    results_svg, errors = palette_results()
    assets = {
        'color-cloud.svg': color_cloud(),
        'voronoi.svg': voronoi(),
        'median-cut.svg': median_cut_figure(),
        'kmeans.svg': kmeans_figure(),
        'mean-vs-max.svg': mean_vs_max(),
        'palette-results.svg': results_svg,
    }
    assets['figures.json'] = json.dumps({
        'generator': 'tools/plot_clustering_concept_figures.py',
        'plot_dependency': 'matplotlib 3.10, numpy, Pillow',
        'seed': SEED,
        'algorithms': {
            'median_cut': 'split the box with the longest side at its median; box means',
            'kmeans': 'Lloyd iterations; palette results start from the median-cut colors',
            'k_center': 'Gonzalez farthest-first traversal',
        },
        'palette_results': {'image': 'images/snake.png at 240 x 180',
                            'space': 'encoded sRGB for clustering, CIELAB D65 for error',
                            'mean_delta_e_ab': errors},
    }, indent=2) + '\n'
    if not args.check:
        OUT.mkdir(parents=True, exist_ok=True)
    for name, content in assets.items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text() != content:
                raise SystemExit(f'Stale figure: {path}')
        else:
            path.write_text(content)
    print(f'{"Verified" if args.check else "Generated"} {len(assets)} assets')


if __name__ == '__main__':
    main()
