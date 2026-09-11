#!/usr/bin/env python3
"""Measure and plot the discrete behavior of libsixel resampling methods."""

from __future__ import annotations

import argparse
import csv
import ctypes
import hashlib
import json
import math
import os
import platform
import random
import shlex
import statistics
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Mapping, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image


METHODS: Tuple[Tuple[str, str, int, str, str], ...] = (
    ("nearest", "nearest", 0, "#000000", "-"),
    ("gaussian", "gaussian", 1, "#56B4E9", "-"),
    ("hanning", "hanning", 2, "#009E73", "--"),
    ("hamming", "hamming", 3, "#0072B2", ":"),
    ("bilinear", "bilinear", 4, "#E69F00", "-."),
    ("welsh", "welsh", 5, "#D55E00", (0, (5, 2))),
    ("bicubic", "bicubic", 6, "#CC79A7", "-"),
    ("lanczos2", "lanczos2", 7, "#6A3D9A", "--"),
    ("lanczos3", "lanczos3", 8, "#8C6D31", ":"),
    ("lanczos4", "lanczos4", 9, "#666666", "-."),
)
COMPACT_METHODS = tuple(method[0] for method in METHODS[:6])
WIDE_METHODS = tuple(method[0] for method in METHODS[6:])
METHOD_IDS = {method[0]: method[2] for method in METHODS}
METHOD_STYLE = {
    method[0]: (method[3], method[4]) for method in METHODS
}

QUALITY_SOURCE_WIDTH = 600
QUALITY_SOURCE_HEIGHT = 450
QUALITY_TARGET_WIDTH = 200
QUALITY_TARGET_HEIGHT = 150
QUALITY_SCALE = 3
ZOOM_X = 4
ZOOM_Y = 42
ZOOM_WIDTH = 80
ZOOM_HEIGHT = 60
ZOOM_SCALE = 4
SPEED_SOURCE_WIDTH = 2400
SPEED_SOURCE_HEIGHT = 1800
SPEED_TARGET_WIDTH = 600
SPEED_TARGET_HEIGHT = 450
MOIRE_SOURCE_SIZE = 1024
MOIRE_TARGET_SIZE = 256
FREQUENCY_TARGET_WIDTH = 256
FREQUENCY_PAD = 32
FREQUENCY_SCALE = 4
FREQUENCY_HARMONICS = tuple(range(2, 513, 4))
FREQUENCY_HARMONIC_COUNT = 128
ISOTROPY_TARGET_SIZE = 192
ISOTROPY_PAD = 32
ISOTROPY_SCALE = 4
ISOTROPY_FREQUENCY = 0.45
ISOTROPY_ANGLES = tuple(float(value) for value in range(0, 91, 5))
ISOTROPY_ANGLE_COUNT = 19
PALETTE_COLORS = 256


def file_sha256(path: Path) -> str:
    """Return a file's SHA-256 digest."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def clean_environment() -> Dict[str, str]:
    """Return a deterministic environment without inherited SIXEL policy."""
    environment = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith("SIXEL_") and not key.startswith("LSQA_")
    }
    environment["LC_ALL"] = "C"
    environment["TZ"] = "UTC"
    environment["SIXEL_THREADS"] = "1"
    environment["SIXEL_SIMD_LEVEL"] = "scalar"
    return environment


def run_checked(
    command: Sequence[str],
    environment: Mapping[str, str],
    stdout: object = subprocess.DEVNULL,
) -> bytes:
    """Run one command and return captured stdout when requested."""
    proc = subprocess.run(
        command,
        stdout=stdout,
        stderr=subprocess.PIPE,
        env=environment,
        check=False,
    )
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace")
        raise RuntimeError(
            f"command failed ({proc.returncode}): {shlex.join(command)}\n"
            f"{diagnostic}"
        )
    if stdout == subprocess.PIPE:
        return proc.stdout
    return b""


def program_record(path: Path) -> Dict[str, str]:
    """Record a tool path, digest, and first version line."""
    proc = subprocess.run(
        [str(path), "--version"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    lines = proc.stdout.decode("utf-8", errors="replace").splitlines()
    return {
        "path": str(path),
        "sha256": file_sha256(path),
        "version": lines[0] if lines else "version not exposed",
    }


class SixelFloatScaler:
    """Call the current float32 scaler directly through its shared library."""

    def __init__(self, library_path: Path) -> None:
        self.library_path = library_path
        self.library = ctypes.CDLL(str(library_path))
        self.allocator = ctypes.c_void_p()
        self.library.sixel_allocator_new.argtypes = [
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_void_p,
        ]
        self.library.sixel_allocator_new.restype = ctypes.c_int
        self.library.sixel_allocator_unref.argtypes = [ctypes.c_void_p]
        self.library.sixel_allocator_unref.restype = None
        self.library.sixel_helper_scale_image_float32.argtypes = [
            ctypes.POINTER(ctypes.c_float),
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_void_p,
        ]
        self.library.sixel_helper_scale_image_float32.restype = ctypes.c_int
        if hasattr(self.library, "sixel_set_threads"):
            self.library.sixel_set_threads.argtypes = [ctypes.c_int]
            self.library.sixel_set_threads.restype = None
            self.library.sixel_set_threads(1)
        status = self.library.sixel_allocator_new(
            ctypes.byref(self.allocator), None, None, None, None
        )
        if status != 0:
            raise RuntimeError(f"sixel_allocator_new failed: {status}")

    def close(self) -> None:
        """Release the allocator owned by this wrapper."""
        if self.allocator:
            self.library.sixel_allocator_unref(self.allocator)
            self.allocator = ctypes.c_void_p()

    def __enter__(self) -> "SixelFloatScaler":
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> None:
        self.close()

    def scale_gray(
        self,
        source: np.ndarray,
        width: int,
        height: int,
        method: str,
    ) -> np.ndarray:
        """Resize a normalized grayscale plane through linear RGB float32."""
        source_gray = np.ascontiguousarray(source, dtype=np.float32)
        if source_gray.ndim != 2:
            raise ValueError("source must be a two-dimensional grayscale plane")
        source_rgb = np.ascontiguousarray(
            np.repeat(source_gray[:, :, None], 3, axis=2), dtype=np.float32
        )
        destination = np.empty((height, width, 3), dtype=np.float32)
        status = self.library.sixel_helper_scale_image_float32(
            destination.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            source_rgb.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            source_rgb.shape[1],
            source_rgb.shape[0],
            0x21,
            width,
            height,
            METHOD_IDS[method],
            self.allocator,
        )
        if status != 0:
            raise RuntimeError(f"float32 scale failed for {method}: {status}")
        return destination[:, :, 0]


def configure_plot() -> None:
    """Apply the shared documentation plot style."""
    plt.rcParams.update(
        {
            "font.size": 10,
            "axes.titlesize": 14,
            "axes.labelsize": 11,
            "axes.edgecolor": "#5E6672",
            "axes.linewidth": 0.8,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "grid.color": "#D7DCE2",
            "grid.linewidth": 0.7,
            "figure.facecolor": "white",
            "axes.facecolor": "white",
        }
    )


def finish_plot(fig: plt.Figure, path: Path) -> None:
    """Write a stable PNG figure."""
    fig.savefig(
        path,
        dpi=180,
        bbox_inches="tight",
        facecolor="white",
        metadata={"Software": "libsixel resampling measurement workflow"},
    )
    plt.close(fig)


def write_csv(
    path: Path,
    rows: Sequence[Mapping[str, object]],
    fieldnames: Sequence[str],
) -> None:
    """Write one stable CSV with LF line endings."""
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=fieldnames, lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)


def percentile(values: Sequence[float], fraction: float) -> float:
    """Return a linearly interpolated percentile."""
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def gray_to_rgb(gray: np.ndarray) -> np.ndarray:
    """Convert normalized grayscale values to display RGB bytes."""
    values = np.clip(np.rint(gray * 255.0), 0.0, 255.0).astype(np.uint8)
    return np.repeat(values[:, :, None], 3, axis=2)


def save_gray(path: Path, gray: np.ndarray) -> None:
    """Save a normalized grayscale plane as an RGB PNG."""
    Image.fromarray(gray_to_rgb(gray), mode="RGB").save(path)


def measure_discrete_stencils(
    scaler: SixelFloatScaler,
) -> List[Dict[str, object]]:
    """Measure actual enlargement and reduction stencils around mid-gray."""
    rows: List[Dict[str, object]] = []
    perturbation = 0.125
    enlarge_source_width = 17
    enlarge_destination_width = 68
    source_center = 8
    baseline = np.full((1, enlarge_source_width), 0.5, dtype=np.float32)
    perturbed = baseline.copy()
    perturbed[0, source_center] += perturbation
    for method, _, _, _, _ in METHODS:
        base_out = scaler.scale_gray(
            baseline, enlarge_destination_width, 1, method
        )[0]
        impulse_out = scaler.scale_gray(
            perturbed, enlarge_destination_width, 1, method
        )[0]
        response = (impulse_out - base_out) / perturbation
        for index, value in enumerate(response):
            coordinate = (
                (index + 0.5) * enlarge_source_width
                / enlarge_destination_width
                - (source_center + 0.5)
            )
            rows.append(
                {
                    "scenario": "enlarge-4x",
                    "method": method,
                    "coordinate": f"{coordinate:.8f}",
                    "response": f"{float(value):.8f}",
                }
            )

    reduction_source_width = 32
    reduction_destination_width = 8
    output_index = 4
    baseline = np.full((1, reduction_source_width), 0.5, dtype=np.float32)
    for method, _, _, _, _ in METHODS:
        base_value = scaler.scale_gray(
            baseline, reduction_destination_width, 1, method
        )[0, output_index]
        for source_index in range(reduction_source_width):
            perturbed = baseline.copy()
            perturbed[0, source_index] += perturbation
            output = scaler.scale_gray(
                perturbed, reduction_destination_width, 1, method
            )[0, output_index]
            coordinate = (
                (source_index + 0.5)
                * reduction_destination_width
                / reduction_source_width
                - (output_index + 0.5)
            )
            rows.append(
                {
                    "scenario": "reduce-4x",
                    "method": method,
                    "coordinate": f"{coordinate:.8f}",
                    "response": f"{float((output - base_value) / perturbation):.8f}",
                }
            )
    return rows


def plot_discrete_stencils(
    rows: Sequence[Mapping[str, object]], output_path: Path
) -> None:
    """Plot discrete responses measured through the current C scaler."""
    fig, axes = plt.subplots(2, 2, figsize=(12.0, 8.0), sharey="row")
    groups = (COMPACT_METHODS, WIDE_METHODS)
    for row_index, (scenario, title) in enumerate(
        (
            ("enlarge-4x", "4x enlargement: one source-sample perturbation"),
            ("reduce-4x", "4:1 reduction: weights for one output sample"),
        )
    ):
        for column_index, group in enumerate(groups):
            axis = axes[row_index, column_index]
            for method in group:
                selected = [
                    row for row in rows
                    if row["scenario"] == scenario and row["method"] == method
                ]
                x = [float(row["coordinate"]) for row in selected]
                y = [float(row["response"]) for row in selected]
                color, linestyle = METHOD_STYLE[method]
                axis.plot(
                    x,
                    y,
                    color=color,
                    linestyle=linestyle,
                    marker="o",
                    markersize=2.8,
                    linewidth=1.5,
                    label=method,
                )
            axis.axhline(0.0, color="#9AA1AA", linewidth=0.8)
            axis.grid(axis="y")
            axis.set_xlabel("Distance in source pixels" if row_index == 0
                            else "Distance in destination pixels")
            axis.set_title(
                f"{title}\n{'compact / positive' if column_index == 0 else 'wide / negative-lobe'}",
                loc="left",
                fontweight="bold",
            )
            axis.legend(frameon=False, ncol=2, fontsize=8)
    axes[0, 0].set_ylabel("Measured normalized response")
    axes[1, 0].set_ylabel("Measured normalized weight")
    fig.suptitle(
        "The implemented operator is discrete, phase-dependent, and normalized",
        x=0.06,
        ha="left",
        fontsize=16,
        fontweight="bold",
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    finish_plot(fig, output_path)


def measure_frequency_response(
    scaler: SixelFloatScaler,
) -> List[Dict[str, object]]:
    """Measure passband retention and above-Nyquist alias contrast."""
    rows: List[Dict[str, object]] = []
    destination_width = FREQUENCY_TARGET_WIDTH + 2 * FREQUENCY_PAD
    source_width = destination_width * FREQUENCY_SCALE
    source_height = 16
    destination_height = source_height // FREQUENCY_SCALE
    x = (np.arange(source_width, dtype=np.float64) + 0.5) / FREQUENCY_SCALE
    amplitude = 0.4
    for harmonic in FREQUENCY_HARMONICS:
        frequency = harmonic / FREQUENCY_TARGET_WIDTH
        row = 0.5 + amplitude * np.cos(2.0 * np.pi * frequency * x)
        source = np.repeat(row[None, :], source_height, axis=0)
        for method, _, _, _, _ in METHODS:
            output = scaler.scale_gray(
                source, destination_width, destination_height, method
            )
            crop = output[:, FREQUENCY_PAD:-FREQUENCY_PAD]
            gain = math.sqrt(2.0) * float(np.std(crop)) / amplitude
            rows.append(
                {
                    "method": method,
                    "frequency_cycles_per_output_pixel": f"{frequency:.8f}",
                    "contrast_gain": f"{gain:.8f}",
                    "region": "passband" if frequency <= 0.5 else "alias",
                }
            )
    return rows


def plot_frequency_group(
    rows: Sequence[Mapping[str, object]],
    methods: Sequence[str],
    title: str,
    output_path: Path,
) -> None:
    """Plot one readable group of measured frequency-response curves."""
    fig, axis = plt.subplots(figsize=(10.2, 5.2))
    axis.axvspan(0.5, 2.0, color="#F3E7E4", alpha=0.7, zorder=0)
    axis.axvline(0.5, color="#A23B3B", linewidth=1.2)
    axis.plot(
        [0.0, 0.5, 0.5, 2.0],
        [1.0, 1.0, 0.0, 0.0],
        color="#777777",
        linewidth=1.3,
        linestyle=(0, (2, 2)),
        label="ideal low-pass boundary",
    )
    for method in methods:
        selected = [row for row in rows if row["method"] == method]
        color, linestyle = METHOD_STYLE[method]
        axis.plot(
            [float(row["frequency_cycles_per_output_pixel"])
             for row in selected],
            [float(row["contrast_gain"]) for row in selected],
            color=color,
            linestyle=linestyle,
            linewidth=1.8,
            label=method,
        )
    axis.set_xlim(0.0, 2.0)
    axis.set_ylim(0.0, 1.28)
    axis.set_xlabel("Input frequency (cycles per output pixel)")
    axis.set_ylabel("Measured output contrast / input contrast")
    axis.set_title(title, loc="left", fontweight="bold")
    axis.text(
        0.505,
        1.20,
        "Above target Nyquist: visible contrast is alias leakage",
        color="#873838",
        fontsize=9,
    )
    axis.grid(axis="y")
    axis.legend(frameon=False, ncol=2, fontsize=8, loc="upper right")
    finish_plot(fig, output_path)


def measure_isotropy(
    scaler: SixelFloatScaler,
) -> List[Dict[str, object]]:
    """Measure directional contrast at one near-Nyquist radial frequency."""
    rows: List[Dict[str, object]] = []
    destination_size = ISOTROPY_TARGET_SIZE + 2 * ISOTROPY_PAD
    source_size = destination_size * ISOTROPY_SCALE
    coordinates = (
        np.arange(source_size, dtype=np.float64) + 0.5
    ) / ISOTROPY_SCALE
    xx, yy = np.meshgrid(coordinates, coordinates)
    amplitude = 0.4
    gains: Dict[str, List[float]] = {method[0]: [] for method in METHODS}
    for angle in ISOTROPY_ANGLES:
        radians = math.radians(angle)
        projection = xx * math.cos(radians) + yy * math.sin(radians)
        source = 0.5 + amplitude * np.cos(
            2.0 * np.pi * ISOTROPY_FREQUENCY * projection
        )
        for method, _, _, _, _ in METHODS:
            output = scaler.scale_gray(
                source, destination_size, destination_size, method
            )
            crop = output[
                ISOTROPY_PAD:-ISOTROPY_PAD,
                ISOTROPY_PAD:-ISOTROPY_PAD,
            ]
            gain = math.sqrt(2.0) * float(np.std(crop)) / amplitude
            gains[method].append(gain)
    for method, _, _, _, _ in METHODS:
        mean_gain = statistics.fmean(gains[method])
        for angle, gain in zip(ISOTROPY_ANGLES, gains[method]):
            rows.append(
                {
                    "method": method,
                    "angle_degrees": f"{angle:.1f}",
                    "radial_frequency": f"{ISOTROPY_FREQUENCY:.8f}",
                    "contrast_gain": f"{gain:.8f}",
                    "relative_to_angle_mean": f"{gain / mean_gain:.8f}",
                }
            )
    return rows


def plot_isotropy(
    rows: Sequence[Mapping[str, object]],
    heatmap_path: Path,
    spread_path: Path,
) -> None:
    """Plot directional gain and per-method anisotropy spread."""
    matrix = np.array(
        [
            [
                float(next(
                    row["contrast_gain"] for row in rows
                    if row["method"] == method[0]
                    and float(row["angle_degrees"]) == angle
                ))
                for angle in ISOTROPY_ANGLES
            ]
            for method in METHODS
        ]
    )
    fig, axis = plt.subplots(figsize=(11.0, 5.7))
    image = axis.imshow(
        matrix,
        aspect="auto",
        interpolation="nearest",
        cmap="viridis",
        vmin=0.0,
        vmax=max(1.0, float(matrix.max())),
    )
    axis.set_xticks(
        np.arange(len(ISOTROPY_ANGLES))[::2],
        [f"{angle:.0f}°" for angle in ISOTROPY_ANGLES[::2]],
    )
    axis.set_yticks(
        np.arange(len(METHODS)), [method[1] for method in METHODS]
    )
    axis.set_xlabel("Grating orientation")
    axis.set_title(
        "A separable filter does not preserve radial response perfectly",
        loc="left",
        fontweight="bold",
    )
    colorbar = fig.colorbar(image, ax=axis, pad=0.02)
    colorbar.set_label(
        f"Measured contrast gain at {ISOTROPY_FREQUENCY:.2f} cycles/pixel"
    )
    finish_plot(fig, heatmap_path)

    spreads = []
    for method_index, method in enumerate(METHODS):
        values = matrix[method_index]
        spreads.append(100.0 * (float(values.max()) - float(values.min()))
                       / float(values.mean()))
    fig, axis = plt.subplots(figsize=(9.6, 5.2))
    positions = np.arange(len(METHODS))
    colors = [method[3] for method in METHODS]
    axis.bar(positions, spreads, color=colors, width=0.68)
    for x, value in zip(positions, spreads):
        axis.text(x, value, f"{value:.1f}%", ha="center", va="bottom",
                  fontsize=8)
    axis.set_xticks(positions, [method[1] for method in METHODS], rotation=35,
                    ha="right")
    axis.set_ylabel("Directional gain range / mean")
    axis.set_title(
        "Directional variation at a fixed radial frequency",
        loc="left",
        fontweight="bold",
    )
    axis.grid(axis="y")
    finish_plot(fig, spread_path)


def zone_plate(size: int, edge_frequency: float = 0.45) -> np.ndarray:
    """Return a continuous-tone radial chirp sampled at pixel centers."""
    coordinates = np.arange(size, dtype=np.float64) + 0.5 - size / 2.0
    xx, yy = np.meshgrid(coordinates, coordinates)
    coefficient = edge_frequency / size
    phase = 2.0 * np.pi * coefficient * (xx * xx + yy * yy)
    return (0.5 + 0.5 * np.cos(phase)).astype(np.float32)


def zone_plate_area_reference(size: int, samples: int = 8) -> np.ndarray:
    """Integrate the generating field over each output pixel by supersampling."""
    high_size = size * samples
    source_span = MOIRE_SOURCE_SIZE
    coordinates = (
        np.arange(high_size, dtype=np.float64) + 0.5
    ) * source_span / high_size - source_span / 2.0
    xx, yy = np.meshgrid(coordinates, coordinates)
    coefficient = 0.45 / source_span
    phase = 2.0 * np.pi * coefficient * (xx * xx + yy * yy)
    high = 0.5 + 0.5 * np.cos(phase)
    return high.reshape(size, samples, size, samples).mean(axis=(1, 3))


def generate_moire_outputs(
    scaler: SixelFloatScaler, output_directory: Path
) -> List[Dict[str, object]]:
    """Write raw zone-plate input, reference, and direct scaler outputs."""
    source = zone_plate(MOIRE_SOURCE_SIZE)
    reference = zone_plate_area_reference(MOIRE_TARGET_SIZE)
    source_path = output_directory / "resampling-moire-input.png"
    reference_path = output_directory / "resampling-moire-reference.png"
    save_gray(source_path, source)
    save_gray(reference_path, reference)
    rows: List[Dict[str, object]] = []
    for method, _, _, _, _ in METHODS:
        output = scaler.scale_gray(
            source, MOIRE_TARGET_SIZE, MOIRE_TARGET_SIZE, method
        )
        path = output_directory / f"resampling-moire-{method}.png"
        save_gray(path, output)
        error = output.astype(np.float64) - reference
        rows.append(
            {
                "method": method,
                "rms_error_vs_area_reference": f"{math.sqrt(float(np.mean(error * error))):.8f}",
                "output_sha256": file_sha256(path),
            }
        )
    return rows


def linearize_srgb(rgb: np.ndarray) -> np.ndarray:
    """Convert normalized sRGB samples to linear light."""
    return np.where(
        rgb <= 0.04045,
        rgb / 12.92,
        ((rgb + 0.055) / 1.055) ** 2.4,
    )


def encode_srgb(linear: np.ndarray) -> np.ndarray:
    """Convert normalized linear-light samples to sRGB."""
    return np.where(
        linear <= 0.0031308,
        linear * 12.92,
        1.055 * np.power(linear, 1.0 / 2.4) - 0.055,
    )


def write_quality_reference(input_path: Path, output_path: Path) -> None:
    """Write an exact 3x3 linear-light area average of the natural fixture."""
    with Image.open(input_path) as image:
        source = np.asarray(image.convert("RGB"), dtype=np.float64) / 255.0
    if source.shape[:2] != (QUALITY_SOURCE_HEIGHT, QUALITY_SOURCE_WIDTH):
        raise RuntimeError(
            f"quality input must be {QUALITY_SOURCE_WIDTH}x{QUALITY_SOURCE_HEIGHT}"
        )
    linear = linearize_srgb(source)
    averaged = linear.reshape(
        QUALITY_TARGET_HEIGHT,
        QUALITY_SCALE,
        QUALITY_TARGET_WIDTH,
        QUALITY_SCALE,
        3,
    ).mean(axis=(1, 3))
    encoded = np.clip(np.rint(encode_srgb(averaged) * 255.0), 0, 255)
    Image.fromarray(encoded.astype(np.uint8), mode="RGB").save(output_path)


def encoder_command(
    img2sixel: Path,
    input_path: Path,
    output_path: Path,
    method: str,
    width: int,
    height: int,
) -> List[str]:
    """Return the fully controlled resampling comparison command."""
    return [
        str(img2sixel),
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        "-Fnone",
        "-aoff",
        "-Xgamma",
        "-Wgamma",
        "-Ugamma",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "--encode-policy=fast",
        f"-p{PALETTE_COLORS}",
        f"-w{width}",
        f"-h{height}",
        f"-r{method}",
        "-jscalar:resize_precision=linear",
        "-o",
        str(output_path),
        str(input_path),
    ]


def command_text(
    command: Sequence[str], replacements: Sequence[Tuple[str, str]]
) -> str:
    """Return a shell-quoted command with portable path placeholders."""
    translated = []
    replacement_map = dict(replacements)
    for argument in command:
        translated.append(replacement_map.get(argument, argument))
    return shlex.join(translated)


def encode_and_decode(
    command: Sequence[str],
    sixel2png: Path,
    png_path: Path,
    environment: Mapping[str, str],
) -> None:
    """Encode one stream and decode those exact bytes to PNG."""
    run_checked(command, environment)
    run_checked(
        [
            str(sixel2png),
            "-jscalar",
            "-i",
            str(command[-2]),
            "-o",
            str(png_path),
        ],
        environment,
    )


def parse_quality(payload: bytes) -> Dict[str, float]:
    """Parse the JSON quality object emitted by lsqa."""
    document = json.loads(payload.decode("utf-8"))
    quality = document.get("quality")
    if not isinstance(quality, dict):
        raise RuntimeError("lsqa output has no quality object")
    return {str(key): float(value) for key, value in quality.items()}


def write_zoom(source: Path, target: Path) -> None:
    """Write a fixed crop enlarged only with nearest-neighbor display scaling."""
    box = (ZOOM_X, ZOOM_Y, ZOOM_X + ZOOM_WIDTH, ZOOM_Y + ZOOM_HEIGHT)
    with Image.open(source) as image:
        crop = image.convert("RGB").crop(box)
        crop = crop.resize(
            (ZOOM_WIDTH * ZOOM_SCALE, ZOOM_HEIGHT * ZOOM_SCALE),
            Image.Resampling.NEAREST,
        )
        crop.save(target)


def measure_quality_and_size(
    img2sixel: Path,
    sixel2png: Path,
    lsqa: Path,
    input_path: Path,
    temporary: Path,
    output_directory: Path,
    environment: Mapping[str, str],
) -> List[Dict[str, object]]:
    """Measure post-SIXEL natural-image quality and exact stream size."""
    reference = temporary / "natural-area-reference.png"
    checked_reference = output_directory / "resampling-natural-reference-zoom.png"
    write_quality_reference(input_path, reference)
    write_zoom(reference, checked_reference)
    rows: List[Dict[str, object]] = []
    for method, _, _, _, _ in METHODS:
        sixel_path = temporary / f"natural-{method}.six"
        png_path = temporary / f"natural-{method}.png"
        command = encoder_command(
            img2sixel,
            input_path,
            sixel_path,
            method,
            QUALITY_TARGET_WIDTH,
            QUALITY_TARGET_HEIGHT,
        )
        encode_and_decode(command, sixel2png, png_path, environment)
        assessment = [
            str(lsqa),
            "-Woklab",
            "-Pfloat32",
            str(reference),
            str(png_path),
        ]
        metrics = parse_quality(
            run_checked(assessment, environment, subprocess.PIPE)
        )
        zoom_path = output_directory / f"resampling-natural-{method}-zoom.png"
        write_zoom(png_path, zoom_path)
        rows.append(
            {
                "method": method,
                "ms_ssim": f"{metrics['MS-SSIM']:.6f}",
                "delta_e00_mean": f"{metrics['Δ E00_mean']:.6f}",
                "delta_chroma_mean": f"{metrics['Δ Chroma_mean']:.6f}",
                "gmsd": f"{metrics['GMSD']:.6f}",
                "psnr_y": f"{metrics['PSNR_Y']:.6f}",
                "sixel_bytes": sixel_path.stat().st_size,
                "sixel_sha256": file_sha256(sixel_path),
                "decoded_png_sha256": file_sha256(png_path),
                "command": command_text(
                    command,
                    (
                        (str(img2sixel), "{img2sixel}"),
                        (str(input_path), "{input}"),
                        (str(sixel_path), "{output}"),
                    ),
                ),
                "assessment_command": command_text(
                    assessment,
                    (
                        (str(lsqa), "{lsqa}"),
                        (str(reference), "{area_reference}"),
                        (str(png_path), "{decoded_output}"),
                    ),
                ),
            }
        )
    return rows


def write_speed_fixture(input_path: Path, output_path: Path) -> None:
    """Nearest-expand the tracked natural input without inventing detail."""
    with Image.open(input_path) as image:
        source = image.convert("RGB").resize(
            (SPEED_SOURCE_WIDTH, SPEED_SOURCE_HEIGHT),
            Image.Resampling.NEAREST,
        )
    source.save(output_path)


def measure_speed(
    img2sixel: Path,
    input_path: Path,
    temporary: Path,
    environment: Mapping[str, str],
    warmups: int,
    runs: int,
) -> List[Dict[str, object]]:
    """Measure controlled end-to-end runtime in seeded interleaved order."""
    speed_input = temporary / "speed-input.png"
    write_speed_fixture(input_path, speed_input)
    cases = [
        (
            method[0],
            encoder_command(
                img2sixel,
                speed_input,
                Path(os.devnull),
                method[0],
                SPEED_TARGET_WIDTH,
                SPEED_TARGET_HEIGHT,
            ),
        )
        for method in METHODS
    ]
    for _ in range(warmups):
        for _, command in cases:
            run_checked(command, environment)
    samples: Dict[str, List[float]] = {method: [] for method, _ in cases}
    randomizer = random.Random(1)
    for _ in range(runs):
        order = list(cases)
        randomizer.shuffle(order)
        for method, command in order:
            started = time.perf_counter_ns()
            run_checked(command, environment)
            elapsed = (time.perf_counter_ns() - started) / 1_000_000.0
            samples[method].append(elapsed)
    rows: List[Dict[str, object]] = []
    for method, command in cases:
        values = samples[method]
        rows.append(
            {
                "method": method,
                "median_ms": f"{statistics.median(values):.6f}",
                "q1_ms": f"{percentile(values, 0.25):.6f}",
                "q3_ms": f"{percentile(values, 0.75):.6f}",
                "minimum_ms": f"{min(values):.6f}",
                "maximum_ms": f"{max(values):.6f}",
                "samples_ms": ";".join(f"{value:.6f}" for value in values),
                "command": command_text(
                    command,
                    (
                        (str(img2sixel), "{img2sixel}"),
                        (str(speed_input), "{speed_input}"),
                        (os.devnull, "{output}"),
                    ),
                ),
            }
        )
    return rows


def plot_method_metric(
    rows: Sequence[Mapping[str, object]],
    field: str,
    ylabel: str,
    title: str,
    output_path: Path,
    digits: int,
) -> None:
    """Plot one directly labeled method comparison."""
    fig, axis = plt.subplots(figsize=(10.2, 5.3))
    positions = np.arange(len(METHODS))
    values = [
        float(next(row[field] for row in rows if row["method"] == method[0]))
        for method in METHODS
    ]
    colors = [method[3] for method in METHODS]
    axis.scatter(positions, values, s=80, c=colors, zorder=3)
    formatter = f"{{:.{digits}f}}"
    for position, value in zip(positions, values):
        label = f"{int(value):,}" if digits < 0 else formatter.format(value)
        axis.annotate(
            label,
            (position, value),
            xytext=(0, 9),
            textcoords="offset points",
            ha="center",
            fontsize=8,
        )
    axis.set_xticks(
        positions, [method[1] for method in METHODS], rotation=35, ha="right"
    )
    axis.set_ylabel(ylabel)
    axis.set_title(title, loc="left", fontweight="bold")
    axis.grid(axis="y")
    minimum = min(values)
    maximum = max(values)
    margin = max((maximum - minimum) * 0.25, abs(maximum) * 0.02, 0.001)
    if field == "ms_ssim":
        axis.set_ylim(max(0.0, minimum - margin), 1.0005)
        axis.text(
            0.01,
            0.04,
            "Expanded y-axis; exact values are labeled",
            transform=axis.transAxes,
            color="#5E6672",
            fontsize=9,
        )
    elif field == "delta_e00_mean":
        axis.set_ylim(0.0, maximum + margin)
        axis.text(
            0.01,
            0.94,
            "Zero is the error-free baseline; lower is better",
            transform=axis.transAxes,
            color="#5E6672",
            fontsize=9,
        )
    else:
        axis.set_ylim(minimum - margin, maximum + margin)
        axis.text(
            0.01,
            0.04,
            "Expanded y-axis; exact byte counts are labeled",
            transform=axis.transAxes,
            color="#5E6672",
            fontsize=9,
        )
    finish_plot(fig, output_path)


def plot_speed(rows: Sequence[Mapping[str, object]], output_path: Path) -> None:
    """Plot median runtime and interquartile range."""
    fig, axis = plt.subplots(figsize=(10.2, 5.4))
    positions = np.arange(len(METHODS))
    for position, method in zip(positions, METHODS):
        row = next(row for row in rows if row["method"] == method[0])
        median = float(row["median_ms"])
        q1 = float(row["q1_ms"])
        q3 = float(row["q3_ms"])
        axis.errorbar(
            position,
            median,
            yerr=[[median - q1], [q3 - median]],
            fmt="o",
            color=method[3],
            markersize=8,
            capsize=4,
            linewidth=1.5,
            zorder=3,
        )
        axis.annotate(
            f"{median:.1f}",
            (position, median),
            xytext=(0, 10),
            textcoords="offset points",
            ha="center",
            fontsize=8,
        )
    axis.set_xticks(
        positions, [method[1] for method in METHODS], rotation=35, ha="right"
    )
    axis.set_ylabel("Median wall time (ms)")
    axis.set_title(
        "End-to-end runtime, 2400 x 1800 to 600 x 450",
        loc="left",
        fontweight="bold",
    )
    axis.text(
        0.01,
        0.94,
        "Error bars show interquartile range; one thread and scalar SIMD",
        transform=axis.transAxes,
        color="#5E6672",
        fontsize=9,
    )
    axis.grid(axis="y")
    maximum = max(float(row["maximum_ms"]) for row in rows)
    axis.set_ylim(0.0, maximum * 1.12)
    finish_plot(fig, output_path)


def artifact_hashes(output_directory: Path) -> Dict[str, str]:
    """Hash all generated artifacts except the self-referential manifest."""
    return {
        path.name: file_sha256(path)
        for path in sorted(output_directory.iterdir())
        if path.is_file() and path.name != "resampling-run.json"
    }


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--input-label", default="images/snake.png")
    parser.add_argument("--libsixel", type=Path, required=True)
    parser.add_argument("--img2sixel", type=Path, required=True)
    parser.add_argument("--sixel2png", type=Path, required=True)
    parser.add_argument("--lsqa", type=Path, required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--source-state", choices=("clean", "dirty"), required=True)
    parser.add_argument("--compiler-command", required=True)
    parser.add_argument("--cflags", default="")
    parser.add_argument("--cppflags", default="")
    parser.add_argument("--ldflags", default="")
    parser.add_argument("--configure-arguments", default="")
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=9)
    parser.add_argument("--output-directory", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Generate all direct-behavior, quality, speed, and size evidence."""
    args = parse_args()
    if args.warmups < 0 or args.runs < 3:
        raise ValueError("warmups must be nonnegative and runs must be at least 3")
    for path in (
        args.input,
        args.libsixel,
        args.img2sixel,
        args.sixel2png,
        args.lsqa,
    ):
        if not path.is_file():
            raise FileNotFoundError(path)
    args.output_directory.mkdir(parents=True, exist_ok=True)
    environment = clean_environment()
    os.environ.update(
        {
            "SIXEL_THREADS": "1",
            "SIXEL_SIMD_LEVEL": "scalar",
            "LC_ALL": "C",
            "TZ": "UTC",
        }
    )
    configure_plot()
    with tempfile.TemporaryDirectory(prefix="libsixel-resampling-") as name:
        temporary = Path(name)
        with SixelFloatScaler(args.libsixel) as scaler:
            stencil_rows = measure_discrete_stencils(scaler)
            plot_discrete_stencils(
                stencil_rows,
                args.output_directory / "resampling-discrete-stencils.png",
            )
            frequency_rows = measure_frequency_response(scaler)
            plot_frequency_group(
                frequency_rows,
                COMPACT_METHODS,
                "Measured 4:1 frequency response: compact kernels",
                args.output_directory
                / "resampling-frequency-response-compact.png",
            )
            plot_frequency_group(
                frequency_rows,
                WIDE_METHODS,
                "Measured 4:1 frequency response: wide kernels",
                args.output_directory
                / "resampling-frequency-response-wide.png",
            )
            isotropy_rows = measure_isotropy(scaler)
            plot_isotropy(
                isotropy_rows,
                args.output_directory / "resampling-isotropy-gain.png",
                args.output_directory / "resampling-isotropy-spread.png",
            )
            moire_rows = generate_moire_outputs(
                scaler, args.output_directory
            )

        quality_rows = measure_quality_and_size(
            args.img2sixel,
            args.sixel2png,
            args.lsqa,
            args.input,
            temporary,
            args.output_directory,
            environment,
        )
        speed_rows = measure_speed(
            args.img2sixel,
            args.input,
            temporary,
            environment,
            args.warmups,
            args.runs,
        )

    write_csv(
        args.output_directory / "resampling-discrete-stencils.csv",
        stencil_rows,
        ("scenario", "method", "coordinate", "response"),
    )
    write_csv(
        args.output_directory / "resampling-frequency-response.csv",
        frequency_rows,
        (
            "method",
            "frequency_cycles_per_output_pixel",
            "contrast_gain",
            "region",
        ),
    )
    write_csv(
        args.output_directory / "resampling-isotropy.csv",
        isotropy_rows,
        (
            "method",
            "angle_degrees",
            "radial_frequency",
            "contrast_gain",
            "relative_to_angle_mean",
        ),
    )
    write_csv(
        args.output_directory / "resampling-moire.csv",
        moire_rows,
        ("method", "rms_error_vs_area_reference", "output_sha256"),
    )
    write_csv(
        args.output_directory / "resampling-quality-size.csv",
        quality_rows,
        (
            "method",
            "ms_ssim",
            "delta_e00_mean",
            "delta_chroma_mean",
            "gmsd",
            "psnr_y",
            "sixel_bytes",
            "sixel_sha256",
            "decoded_png_sha256",
            "command",
            "assessment_command",
        ),
    )
    write_csv(
        args.output_directory / "resampling-speed.csv",
        speed_rows,
        (
            "method",
            "median_ms",
            "q1_ms",
            "q3_ms",
            "minimum_ms",
            "maximum_ms",
            "samples_ms",
            "command",
        ),
    )

    plot_method_metric(
        quality_rows,
        "ms_ssim",
        "MS-SSIM (higher is better)",
        "Natural-image quality versus a 3x3 linear-light area reference",
        args.output_directory / "resampling-quality-ms-ssim.png",
        6,
    )
    plot_method_metric(
        quality_rows,
        "delta_e00_mean",
        "Mean Delta E00 (lower is better)",
        "Natural-image color error versus the area reference",
        args.output_directory / "resampling-quality-delta-e00.png",
        3,
    )
    plot_speed(
        speed_rows, args.output_directory / "resampling-speed.png"
    )
    plot_method_metric(
        quality_rows,
        "sixel_bytes",
        "SIXEL stream bytes (lower is smaller)",
        "Exact stream size for the natural-image quality output",
        args.output_directory / "resampling-size.png",
        -1,
    )

    metadata = {
        "schema_version": 1,
        "source": {
            "revision": args.revision,
            "tracked_worktree_state_at_start": args.source_state,
            "input": args.input_label,
            "input_sha256": file_sha256(args.input),
        },
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "numpy": np.__version__,
            "matplotlib": matplotlib.__version__,
        },
        "build": {
            "compiler_command": args.compiler_command,
            "cflags": args.cflags,
            "cppflags": args.cppflags,
            "ldflags": args.ldflags,
            "configure_arguments": args.configure_arguments,
        },
        "tools": {
            "libsixel": {
                "path": str(args.libsixel),
                "sha256": file_sha256(args.libsixel),
            },
            "img2sixel": program_record(args.img2sixel),
            "sixel2png": program_record(args.sixel2png),
            "lsqa": program_record(args.lsqa),
        },
        "protocol": {
            "methods": [method[0] for method in METHODS],
            "direct_scaler": "sixel_helper_scale_image_float32",
            "direct_pixelformat": "linear RGB float32",
            "direct_threads": 1,
            "direct_simd": "scalar",
            "reduction_scale": FREQUENCY_SCALE,
            "target_nyquist_cycles_per_pixel": 0.5,
            "frequency_samples": len(FREQUENCY_HARMONICS),
            "isotropy_frequency": ISOTROPY_FREQUENCY,
            "isotropy_angles_degrees": list(ISOTROPY_ANGLES),
            "moire_source_size": [MOIRE_SOURCE_SIZE, MOIRE_SOURCE_SIZE],
            "moire_target_size": [MOIRE_TARGET_SIZE, MOIRE_TARGET_SIZE],
            "moire_reference": "8x8 supersampled area integration of the generating field",
            "quality_source_size": [QUALITY_SOURCE_WIDTH, QUALITY_SOURCE_HEIGHT],
            "quality_target_size": [QUALITY_TARGET_WIDTH, QUALITY_TARGET_HEIGHT],
            "quality_reference": "exact 3x3 linear-light source-pixel area average",
            "quality_metrics": ["MS-SSIM", "mean Delta E00"],
            "speed_source_size": [SPEED_SOURCE_WIDTH, SPEED_SOURCE_HEIGHT],
            "speed_target_size": [SPEED_TARGET_WIDTH, SPEED_TARGET_HEIGHT],
            "speed_warmups": args.warmups,
            "speed_runs": args.runs,
            "speed_order": "seeded interleaving with seed 1",
            "encoder_threads": 1,
            "encoder_simd": "scalar",
            "resize_precision": "linear",
            "palette_colors": PALETTE_COLORS,
            "diffusion": "none",
        },
    }
    metadata["artifacts"] = artifact_hashes(args.output_directory)
    (args.output_directory / "resampling-run.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
