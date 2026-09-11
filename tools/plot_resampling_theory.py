#!/usr/bin/env python3
"""Generate mathematical resampling figures for the documentation."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
from typing import Dict, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image


FIGURE_SIZE = (10.8, 4.4)
FIGURE_DPI = 140
KERNEL_COLOR = "#0072B2"
ACCENT_COLOR = "#D55E00"
CONTEXT_COLOR = "#7A8491"
GRID_COLOR = "#D7DDE5"

KERNEL_SUPPORT: Dict[str, float] = {
    "gaussian": 1.0,
    "hanning": 1.0,
    "hamming": 1.0,
    "bilinear": 1.0,
    "welsh": 1.0,
    "bicubic": 2.0,
    "lanczos2": 2.0,
    "lanczos3": 3.0,
    "lanczos4": 4.0,
}

KERNEL_TITLES: Dict[str, str] = {
    "gaussian": "Gaussian: a truncated positive bell",
    "hanning": "Hanning: a raised cosine used directly",
    "hamming": "Hamming: a raised cosine with nonzero endpoints",
    "bilinear": "Bilinear: the radius-1 triangular kernel",
    "welsh": "Welsh: the API name for a parabolic Welch shape",
    "bicubic": "Bicubic: the Keys cubic-convolution family at a = -1",
    "lanczos2": "Lanczos2: sinc reconstruction with two-lobe support",
    "lanczos3": "Lanczos3: sinc reconstruction with three-lobe support",
    "lanczos4": "Lanczos4: sinc reconstruction with four-lobe support",
}


def kernel_values(name: str, x: np.ndarray) -> np.ndarray:
    """Evaluate the continuous weighting function used by scale.c."""
    distance = np.abs(x)
    support = KERNEL_SUPPORT[name]
    inside = distance <= support
    values = np.zeros_like(distance)

    if name == "gaussian":
        values[inside] = (
            np.exp(-2.0 * distance[inside] * distance[inside])
            * math.sqrt(2.0 / math.pi)
        )
    elif name == "hanning":
        values[inside] = 0.5 + 0.5 * np.cos(math.pi * distance[inside])
    elif name == "hamming":
        values[inside] = 0.54 + 0.46 * np.cos(
            math.pi * distance[inside]
        )
    elif name == "bilinear":
        active = distance < 1.0
        values[active] = 1.0 - distance[active]
    elif name == "welsh":
        active = distance < 1.0
        values[active] = 1.0 - distance[active] * distance[active]
    elif name == "bicubic":
        inner = distance <= 1.0
        outer = (distance > 1.0) & (distance <= 2.0)
        d = distance[inner]
        values[inner] = 1.0 + (d - 2.0) * d * d
        d = distance[outer]
        values[outer] = 4.0 + d * (-8.0 + d * (5.0 - d))
    elif name.startswith("lanczos"):
        radius = float(name[-1])
        active = distance < radius
        d = distance[active]
        values[active] = np.sinc(d) * np.sinc(d / radius)
    else:
        raise ValueError(f"unknown kernel: {name}")

    return values


def frequency_response(name: str) -> Tuple[np.ndarray, np.ndarray]:
    """Numerically transform one even finite-support continuous kernel."""
    support = KERNEL_SUPPORT[name]
    x = np.linspace(-support, support, 20001)
    weights = kernel_values(name, x)
    frequencies = np.linspace(0.0, 2.0, 801)
    response = np.empty_like(frequencies)

    for index, frequency in enumerate(frequencies):
        integrand = weights * np.cos(2.0 * math.pi * frequency * x)
        response[index] = np.sum(
            (integrand[1:] + integrand[:-1])
            * (x[1:] - x[:-1])
            * 0.5
        )

    response /= response[0]
    response_db = 20.0 * np.log10(np.maximum(np.abs(response), 1.0e-4))
    return frequencies, response_db


def finish_figure(fig: plt.Figure, output_path: Path) -> None:
    """Apply shared layout and save one deterministic PNG."""
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.94), w_pad=4.0, h_pad=2.0)
    fig.savefig(output_path, dpi=FIGURE_DPI, facecolor="white")
    plt.close(fig)


def plot_kernel(name: str, output_path: Path) -> None:
    """Plot one kernel in space and in continuous-frequency magnitude."""
    support = KERNEL_SUPPORT[name]
    x_limit = support + 0.35
    x = np.linspace(-x_limit, x_limit, 2401)
    weights = kernel_values(name, x)
    frequencies, response_db = frequency_response(name)
    fig, axes = plt.subplots(1, 2, figsize=FIGURE_SIZE)
    fig.suptitle(KERNEL_TITLES[name], x=0.055, ha="left", fontweight="bold")

    spatial = axes[0]
    spatial.plot(x, weights, color=KERNEL_COLOR, linewidth=2.4)
    spatial.fill_between(x, 0.0, weights, color=KERNEL_COLOR, alpha=0.12)
    spatial.axhline(0.0, color=CONTEXT_COLOR, linewidth=0.9)
    spatial.axvline(-support, color=ACCENT_COLOR, linestyle="--", linewidth=1.2)
    spatial.axvline(support, color=ACCENT_COLOR, linestyle="--", linewidth=1.2)
    taps = np.arange(-math.floor(support), math.floor(support) + 1)
    spatial.scatter(
        taps,
        kernel_values(name, taps.astype(float)),
        color=ACCENT_COLOR,
        edgecolor="white",
        linewidth=0.8,
        zorder=3,
    )
    spatial.text(
        0.02,
        0.96,
        f"nominal support: |x| <= {support:g}",
        transform=spatial.transAxes,
        va="top",
        color=CONTEXT_COLOR,
    )
    spatial.set_xlabel("Signed sample distance x")
    spatial.set_ylabel("Unnormalized weight h(x)")
    spatial.set_xlim(-x_limit, x_limit)
    spatial.grid(color=GRID_COLOR, linewidth=0.8)

    spectrum = axes[1]
    spectrum.plot(frequencies, response_db, color=KERNEL_COLOR, linewidth=2.4)
    spectrum.axvline(
        0.5,
        color=ACCENT_COLOR,
        linestyle="--",
        linewidth=1.2,
    )
    spectrum.text(
        0.51,
        -76.0,
        "unit-grid Nyquist",
        color=ACCENT_COLOR,
        rotation=90,
        va="bottom",
    )
    spectrum.text(
        0.98,
        0.96,
        "continuous transform; DC normalized to 0 dB",
        transform=spectrum.transAxes,
        ha="right",
        va="top",
        color=CONTEXT_COLOR,
    )
    spectrum.set_xlabel("Spatial frequency (cycles per sample)")
    spectrum.set_ylabel("|H(f)| (dB)")
    spectrum.set_xlim(0.0, 2.0)
    spectrum.set_ylim(-80.0, 4.0)
    spectrum.grid(color=GRID_COLOR, linewidth=0.8)

    finish_figure(fig, output_path)


def plot_nearest(output_path: Path) -> None:
    """Show the exact floor-index behavior of nearest-neighbor scaling."""
    source = np.array([0.10, 0.82, 0.30, 0.66])
    enlarged_width = 11
    reduced_source = np.array(
        [0.08, 0.88, 0.18, 0.78, 0.28, 0.68, 0.38, 0.58, 0.48, 0.98, 0.02]
    )
    reduced_width = 4
    enlarged_indices = (
        np.arange(enlarged_width) * len(source) // enlarged_width
    )
    reduced_indices = (
        np.arange(reduced_width) * len(reduced_source) // reduced_width
    )
    enlarged = source[enlarged_indices]
    reduced = reduced_source[reduced_indices]
    fig, axes = plt.subplots(1, 2, figsize=FIGURE_SIZE)
    fig.suptitle(
        "Nearest: exact source selection without a weighting kernel",
        x=0.055,
        ha="left",
        fontweight="bold",
    )

    axes[0].stairs(
        enlarged,
        np.arange(enlarged_width + 1),
        color=KERNEL_COLOR,
        linewidth=2.4,
        fill=True,
        alpha=0.22,
    )
    axes[0].scatter(
        np.arange(enlarged_width) + 0.5,
        enlarged,
        color=KERNEL_COLOR,
        zorder=3,
    )
    axes[0].set_title("4 to 11: selected samples become blocks", loc="left")
    axes[0].set_xlabel("Destination pixel index")
    axes[0].set_ylabel("Selected source value")
    axes[0].set_xlim(0.0, enlarged_width)
    axes[0].set_ylim(0.0, 1.0)
    axes[0].grid(color=GRID_COLOR, linewidth=0.8)

    source_positions = np.arange(len(reduced_source))
    axes[1].stem(
        source_positions,
        reduced_source,
        linefmt=CONTEXT_COLOR,
        markerfmt="o",
        basefmt=" ",
    )
    axes[1].scatter(
        reduced_indices,
        reduced,
        color=ACCENT_COLOR,
        s=72,
        label="samples retained by floor mapping",
        zorder=3,
    )
    for index in reduced_indices:
        axes[1].annotate(
            str(index),
            (index, reduced_source[index]),
            xytext=(0, 9),
            textcoords="offset points",
            ha="center",
            color=ACCENT_COLOR,
        )
    axes[1].set_title("11 to 4: most samples are discarded", loc="left")
    axes[1].set_xlabel("Source pixel index")
    axes[1].set_ylabel("Source value")
    axes[1].set_xlim(-0.5, len(reduced_source) - 0.5)
    axes[1].set_ylim(0.0, 1.08)
    axes[1].legend(frameon=False, loc="lower right")
    axes[1].grid(color=GRID_COLOR, linewidth=0.8)

    finish_figure(fig, output_path)


def plot_sampling_reconstruction(output_path: Path) -> None:
    """Illustrate point samples, sinc basis functions, and reconstruction."""
    sample_indices = np.arange(-32, 33)
    sample_values = (
        0.68 * np.sin(2.0 * math.pi * 0.18 * sample_indices)
        + 0.24 * np.cos(2.0 * math.pi * 0.38 * sample_indices)
    )
    x = np.linspace(-6.0, 6.0, 2401)
    original = (
        0.68 * np.sin(2.0 * math.pi * 0.18 * x)
        + 0.24 * np.cos(2.0 * math.pi * 0.38 * x)
    )
    bases = np.sinc(x[:, None] - sample_indices[None, :])
    reconstructed = bases @ sample_values
    visible = (sample_indices >= -6) & (sample_indices <= 6)
    fig, axes = plt.subplots(1, 2, figsize=FIGURE_SIZE)
    fig.suptitle(
        "Band-limited reconstruction is a weighted sum of shifted sinc bases",
        x=0.055,
        ha="left",
        fontweight="bold",
    )

    axes[0].plot(x, original, color=CONTEXT_COLOR, linewidth=3.0, label="original")
    axes[0].plot(
        x,
        reconstructed,
        color=KERNEL_COLOR,
        linewidth=1.8,
        linestyle="--",
        label="sinc reconstruction",
    )
    axes[0].stem(
        sample_indices[visible],
        sample_values[visible],
        linefmt=ACCENT_COLOR,
        markerfmt="o",
        basefmt=" ",
        label="integer samples",
    )
    axes[0].set_title("Uniform samples retain this two-tone signal", loc="left")
    axes[0].set_xlabel("Continuous coordinate x")
    axes[0].set_ylabel("Signal value")
    axes[0].set_xlim(-6.0, 6.0)
    axes[0].set_ylim(-1.15, 1.15)
    axes[0].legend(frameon=False, loc="lower right")
    axes[0].grid(color=GRID_COLOR, linewidth=0.8)

    center_indices = np.array([-1, 0, 1])
    center_values = sample_values[center_indices + 32]
    for index, value, color in zip(
        center_indices,
        center_values,
        ("#009E73", ACCENT_COLOR, "#CC79A7"),
    ):
        contribution = value * np.sinc(x - index)
        axes[1].plot(
            x,
            contribution,
            color=color,
            linewidth=1.8,
            label=f"sample {index}: value x shifted sinc",
        )
    axes[1].plot(
        x,
        reconstructed,
        color=KERNEL_COLOR,
        linewidth=2.6,
        label="sum of all sample contributions",
    )
    axes[1].set_title("Every sample contributes beyond its own cell", loc="left")
    axes[1].set_xlabel("Continuous coordinate x")
    axes[1].set_ylabel("Contribution")
    axes[1].set_xlim(-3.0, 3.0)
    axes[1].set_ylim(-1.15, 1.15)
    axes[1].legend(frameon=False, loc="lower right", fontsize=8.5)
    axes[1].grid(color=GRID_COLOR, linewidth=0.8)

    finish_figure(fig, output_path)


def triangular_spectrum(frequency: np.ndarray, center: float, width: float) -> np.ndarray:
    """Return a compact schematic spectrum centered at one sampling replica."""
    return np.maximum(1.0 - np.abs(frequency - center) / width, 0.0)


def plot_aliasing(output_path: Path) -> None:
    """Show separated and overlapping spectral replicas after sampling."""
    frequency = np.linspace(-1.6, 1.6, 2401)
    fig, axes = plt.subplots(2, 1, figsize=FIGURE_SIZE, sharex=True)
    fig.suptitle(
        "Sampling copies the spectrum; overlap becomes irreversible aliasing",
        x=0.055,
        ha="left",
        fontweight="bold",
    )

    for axis, width, title in (
        (axes[0], 0.34, "Prefiltered: replicas remain separate"),
        (axes[1], 0.78, "Not prefiltered: neighboring replicas overlap"),
    ):
        total = np.zeros_like(frequency)
        for replica, color in zip((-1, 0, 1), ("#CC79A7", KERNEL_COLOR, "#009E73")):
            values = triangular_spectrum(frequency, float(replica), width)
            total += values
            axis.fill_between(
                frequency,
                0.0,
                values,
                color=color,
                alpha=0.20,
            )
            axis.plot(frequency, values, color=color, linewidth=1.5)
        axis.plot(frequency, total, color="#20242A", linewidth=2.0)
        axis.axvspan(-0.5, 0.5, color=ACCENT_COLOR, alpha=0.07)
        axis.axvline(-0.5, color=ACCENT_COLOR, linestyle="--", linewidth=1.0)
        axis.axvline(0.5, color=ACCENT_COLOR, linestyle="--", linewidth=1.0)
        axis.set_title(title, loc="left")
        axis.set_ylabel("Spectral magnitude")
        axis.set_ylim(0.0, 2.2)
        axis.grid(color=GRID_COLOR, linewidth=0.8)

    axes[0].text(
        0.02,
        0.86,
        "one baseband copy fits inside +/-0.5 cycles/sample",
        transform=axes[0].transAxes,
        color=CONTEXT_COLOR,
    )
    axes[1].annotate(
        "different source bands add at the same sampled frequency",
        xy=(0.28, 1.35),
        xytext=(0.66, 1.85),
        arrowprops={"arrowstyle": "->", "color": ACCENT_COLOR},
        color=ACCENT_COLOR,
    )
    axes[1].set_xlabel("Frequency normalized to the destination sample grid")
    axes[1].set_xlim(-1.6, 1.6)

    finish_figure(fig, output_path)


def validate_outputs(output_dir: Path) -> None:
    """Check the generated file set and common raster dimensions."""
    expected = {
        "resampling-theory-sinc-reconstruction.png",
        "resampling-theory-aliasing.png",
        "resampling-kernel-nearest.png",
        *(f"resampling-kernel-{name}.png" for name in KERNEL_SUPPORT),
    }
    expected_size = (
        round(FIGURE_SIZE[0] * FIGURE_DPI),
        round(FIGURE_SIZE[1] * FIGURE_DPI),
    )

    for name in sorted(expected):
        path = output_dir / name
        with Image.open(path) as image:
            if image.format != "PNG":
                raise RuntimeError(f"not a PNG: {path}")
            if image.size != expected_size:
                raise RuntimeError(
                    f"unexpected dimensions for {path}: "
                    f"{image.size} != {expected_size}"
                )


def main() -> None:
    """Generate and validate every resampling-theory figure."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    plt.rcParams.update(
        {
            "font.size": 10.5,
            "axes.edgecolor": "#5E6672",
            "axes.spines.top": False,
            "axes.spines.right": False,
        }
    )

    plot_sampling_reconstruction(
        args.output_dir / "resampling-theory-sinc-reconstruction.png"
    )
    plot_aliasing(args.output_dir / "resampling-theory-aliasing.png")
    plot_nearest(args.output_dir / "resampling-kernel-nearest.png")
    for name in KERNEL_SUPPORT:
        plot_kernel(
            name,
            args.output_dir / f"resampling-kernel-{name}.png",
        )

    validate_outputs(args.output_dir)


if __name__ == "__main__":
    main()
