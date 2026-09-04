#!/usr/bin/env python3
"""Measure and plot the spatial spectrum of each static dither policy."""

from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import io
import json
import math
import platform
import shlex
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap
import numpy as np
from PIL import Image, __version__ as PILLOW_VERSION

from plot_lookup_policy_speed import (
    make_command_environment,
    program_record,
    read_build_configuration,
    resolve_img2sixel,
)


FIELD_SIZE = 512
CROP_SIZE = 256
RADIAL_BINS = 64
ANGULAR_BINS = 36
GRAY_LEVELS = 16
TONE_VALUES = tuple(
    round((index + 0.5) * 255.0 / (GRAY_LEVELS - 1))
    for index in range(GRAY_LEVELS - 1)
)
DITHER_METHODS: Tuple[Tuple[str, str], ...] = (
    ("none", "none:scan=raster"),
    ("fs", "fs:scan=raster"),
    ("atkinson", "atkinson:scan=raster"),
    ("jajuni", "jajuni:scan=raster"),
    ("stucki", "stucki:scan=raster"),
    ("burkes", "burkes:scan=raster"),
    ("sierra1", "sierra:variant=1:scan=raster"),
    ("sierra2", "sierra:variant=2:scan=raster"),
    ("sierra3", "sierra:variant=3:scan=raster"),
    ("lso2", "lso2:scan=raster"),
    ("a_dither", "a_dither:strength=0.150:scan=raster"),
    ("x_dither", "x_dither:strength=0.100:scan=raster"),
    (
        "bluenoise",
        "bluenoise:strength=0.055:gradient_factor=0:"
        "phase=0,0:channel=mono:size=64:scan=raster",
    ),
)
METHOD_GROUPS: Tuple[Tuple[str, Tuple[str, ...], str], ...] = (
    ("Compact and adaptive diffusion", ("fs", "atkinson", "lso2"), "blue"),
    (
        "Wide fixed-kernel diffusion",
        ("jajuni", "stucki", "burkes", "sierra1", "sierra2", "sierra3"),
        "orange",
    ),
    ("Positional methods", ("a_dither", "x_dither", "bluenoise"), "olive"),
)


@dataclass
class SpectrumResult:
    """Hold one method's averaged spectral measurements."""

    method: str
    diffusion: str
    active_tones: int
    example_error: np.ndarray
    power: Optional[np.ndarray]
    radial_frequency: np.ndarray
    radial_power_db: Optional[np.ndarray]
    radial_anisotropy_db: Optional[np.ndarray]
    low_frequency_ratio: Optional[float]
    peak_frequency: Optional[float]
    spectral_centroid: Optional[float]
    anisotropy_db: Optional[float]
    spectral_flatness: Optional[float]
    rms_luma_error: float
    mean_luma_bias: float


def make_command(img2sixel: str, diffusion: str) -> List[str]:
    """Build the fixed-palette command used for every generated tone."""
    return [
        img2sixel,
        "--threads=1",
        "--precision=8bit",
        "--quality=full",
        "--loaders=builtin!",
        "--builtin-palette=gray4",
        "-Wgamma",
        f"--diffusion={diffusion}",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "-o",
        "png:-",
        "-",
    ]


def command_template(command: Sequence[str], img2sixel: str) -> str:
    """Return a relocatable command string for metadata and CSV rows."""
    return shlex.join(command).replace(img2sixel, "{img2sixel}", 1)


def make_constant_ppm(value: int) -> bytes:
    """Create one deterministic constant RGB P6 image in memory."""
    header = f"P6\n{FIELD_SIZE} {FIELD_SIZE}\n255\n".encode("ascii")
    return header + bytes((value, value, value)) * (FIELD_SIZE * FIELD_SIZE)


def generated_input_sha256() -> str:
    """Hash the complete ordered set of synthetic input images."""
    digest = hashlib.sha256()
    for value in TONE_VALUES:
        digest.update(make_constant_ppm(value))
    return digest.hexdigest()


def run_dither(command: Sequence[str], ppm: bytes,
               command_env: Dict[str, str]) -> np.ndarray:
    """Run img2sixel and decode its palette-preserving PNG stdout."""
    completed = subprocess.run(
        list(command),
        input=ppm,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=command_env,
        check=False,
    )
    if completed.returncode != 0:
        diagnostic = completed.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"Command failed ({completed.returncode}): "
            f"{shlex.join(command)}\n{diagnostic}"
        )
    try:
        with Image.open(io.BytesIO(completed.stdout)) as image:
            rgb = np.asarray(image.convert("RGB"), dtype=np.float64) / 255.0
    except (OSError, ValueError) as exc:
        raise RuntimeError("img2sixel stdout is not a readable PNG") from exc
    if rgb.shape != (FIELD_SIZE, FIELD_SIZE, 3):
        raise RuntimeError(f"Unexpected generated image shape: {rgb.shape}")
    return rgb[:, :, 0]


def crop_center(image: np.ndarray) -> np.ndarray:
    """Remove causal image boundaries before spectral analysis."""
    offset = (FIELD_SIZE - CROP_SIZE) // 2
    return image[offset:offset + CROP_SIZE, offset:offset + CROP_SIZE]


def normalized_periodogram(error: np.ndarray) -> Optional[np.ndarray]:
    """Return a unit-energy Hann-windowed periodogram of one error field."""
    centered = error - float(np.mean(error))
    window_1d = np.hanning(CROP_SIZE)
    window = np.outer(window_1d, window_1d)
    transformed = np.fft.fftshift(np.fft.fft2(centered * window))
    power = np.abs(transformed) ** 2
    total = float(np.sum(power))
    if total <= np.finfo(np.float64).eps:
        return None
    return power / total


def frequency_geometry() -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return normalized radius, folded angle, and the circular Nyquist mask."""
    frequency = np.fft.fftshift(np.fft.fftfreq(CROP_SIZE))
    fy, fx = np.meshgrid(frequency, frequency, indexing="ij")
    radius = np.sqrt(fx * fx + fy * fy) / 0.5
    angle = np.mod(np.arctan2(fy, fx), math.pi)
    return radius, angle, radius <= 1.0


def radial_profiles(power: np.ndarray) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Compute radial power and frequency-dependent anisotropy curves."""
    radius, angle, circular = frequency_geometry()
    radial_edges = np.linspace(0.0, 1.0, RADIAL_BINS + 1)
    angular_edges = np.linspace(0.0, math.pi, ANGULAR_BINS + 1)
    frequencies = 0.5 * (radial_edges[:-1] + radial_edges[1:])
    radial = np.full(RADIAL_BINS, np.nan, dtype=np.float64)
    anisotropy = np.full(RADIAL_BINS, np.nan, dtype=np.float64)
    reference = float(np.mean(power[circular & (radius > 0.0)]))
    epsilon = max(reference * 1.0e-12, np.finfo(np.float64).tiny)
    for radial_index in range(RADIAL_BINS):
        ring = circular & (radius >= radial_edges[radial_index])
        ring &= radius < radial_edges[radial_index + 1]
        if not np.any(ring):
            continue
        radial[radial_index] = 10.0 * math.log10(
            max(float(np.mean(power[ring])), epsilon) / reference
        )
        sectors: List[float] = []
        for angular_index in range(ANGULAR_BINS):
            sector = ring & (angle >= angular_edges[angular_index])
            sector &= angle < angular_edges[angular_index + 1]
            if np.any(sector):
                sectors.append(float(np.mean(power[sector])))
        if len(sectors) < 4:
            continue
        sector_values = np.asarray(sectors, dtype=np.float64)
        sector_mean = float(np.mean(sector_values))
        if sector_mean > 0.0:
            coefficient_squared = float(np.var(sector_values)) / (
                sector_mean * sector_mean
            )
            anisotropy[radial_index] = 10.0 * math.log10(
                max(coefficient_squared, 1.0e-12)
            )
    return frequencies, radial, anisotropy


def summarize_power(power: np.ndarray, frequencies: np.ndarray,
                    radial: np.ndarray) -> Tuple[float, float, float, float, float]:
    """Return scalar descriptors for the overview matrix."""
    radius, angle, circular = frequency_geometry()
    non_dc = circular & (radius > 0.0)
    analysis = circular & (radius >= 0.05)
    total = float(np.sum(power[non_dc]))
    low_ratio = float(np.sum(power[non_dc & (radius < 0.25)])) / total
    valid_radial = np.isfinite(radial) & (frequencies >= 0.02)
    peak_frequency = float(frequencies[valid_radial][np.argmax(radial[valid_radial])])
    centroid = float(np.sum(radius[non_dc] * power[non_dc])) / total

    angular_edges = np.linspace(0.0, math.pi, ANGULAR_BINS + 1)
    sectors: List[float] = []
    for angular_index in range(ANGULAR_BINS):
        sector = analysis & (angle >= angular_edges[angular_index])
        sector &= angle < angular_edges[angular_index + 1]
        sectors.append(float(np.mean(power[sector])))
    sector_values = np.asarray(sectors, dtype=np.float64)
    sector_mean = float(np.mean(sector_values))
    coefficient_squared = float(np.var(sector_values)) / (
        sector_mean * sector_mean
    )
    anisotropy_db = 10.0 * math.log10(max(coefficient_squared, 1.0e-12))

    values = power[analysis]
    arithmetic_mean = float(np.mean(values))
    epsilon = max(arithmetic_mean * 1.0e-12, np.finfo(np.float64).tiny)
    geometric_mean = float(np.exp(np.mean(np.log(values + epsilon))))
    flatness = geometric_mean / arithmetic_mean
    return low_ratio, peak_frequency, centroid, anisotropy_db, flatness


def measure_method(img2sixel: str, method: str, diffusion: str,
                   command_env: Dict[str, str]) -> SpectrumResult:
    """Measure one method over every gray-palette midpoint tone."""
    command = make_command(img2sixel, diffusion)
    powers: List[np.ndarray] = []
    example_error: Optional[np.ndarray] = None
    squared_error = 0.0
    error_sum = 0.0
    pixel_count = 0
    example_value = TONE_VALUES[len(TONE_VALUES) // 2]
    for value in TONE_VALUES:
        output = crop_center(
            run_dither(command, make_constant_ppm(value), command_env)
        )
        error = output - value / 255.0
        if value == example_value:
            example_error = error.copy()
        squared_error += float(np.sum(error * error))
        error_sum += float(np.sum(error))
        pixel_count += error.size
        power = normalized_periodogram(error)
        if power is not None:
            powers.append(power)
    if example_error is None:
        raise RuntimeError("Midpoint example tone was not measured")

    rms_error = math.sqrt(squared_error / pixel_count)
    mean_bias = error_sum / pixel_count
    frequency = np.linspace(
        0.5 / RADIAL_BINS,
        1.0 - 0.5 / RADIAL_BINS,
        RADIAL_BINS,
    )
    if not powers:
        return SpectrumResult(
            method,
            diffusion,
            0,
            example_error,
            None,
            frequency,
            None,
            None,
            None,
            None,
            None,
            None,
            None,
            rms_error,
            mean_bias,
        )

    mean_power = np.mean(np.stack(powers), axis=0)
    mean_power /= float(np.sum(mean_power))
    frequency, radial, radial_anisotropy = radial_profiles(mean_power)
    low, peak, centroid, anisotropy_db, flatness = summarize_power(
        mean_power,
        frequency,
        radial,
    )
    return SpectrumResult(
        method,
        diffusion,
        len(powers),
        example_error,
        mean_power,
        frequency,
        radial,
        radial_anisotropy,
        low,
        peak,
        centroid,
        anisotropy_db,
        flatness,
        rms_error,
        mean_bias,
    )


def display_power_db(power: np.ndarray) -> np.ndarray:
    """Convert a periodogram to dB relative to circular mean power."""
    radius, _angle, circular = frequency_geometry()
    reference = float(np.mean(power[circular & (radius > 0.0)]))
    return 10.0 * np.log10(np.maximum(power / reference, 1.0e-12))


def plot_atlas(path: Path, results: Sequence[SpectrumResult]) -> None:
    """Plot error texture and three spectral views for every method."""
    figure, axes = plt.subplots(
        len(results),
        4,
        figsize=(13.5, 31.0),
        gridspec_kw={"width_ratios": (1.0, 1.0, 1.55, 1.55)},
    )
    error_cmap = LinearSegmentedColormap.from_list(
        "error_blue_orange",
        ("#0072B2", "#F7F7F7", "#D55E00"),
    )
    for row, result in enumerate(results):
        error_axis, spectrum_axis, radial_axis, anisotropy_axis = axes[row]
        error_axis.imshow(
            result.example_error,
            cmap=error_cmap,
            vmin=-0.04,
            vmax=0.04,
            interpolation="nearest",
        )
        error_axis.set_ylabel(result.method, rotation=0, ha="right", va="center")
        error_axis.set_xticks(())
        error_axis.set_yticks(())

        if result.power is None:
            spectrum_axis.text(
                0.5,
                0.5,
                "no AC\nenergy",
                ha="center",
                va="center",
                color="#555555",
                transform=spectrum_axis.transAxes,
            )
            radial_axis.text(
                0.5,
                0.5,
                "constant assignment",
                ha="center",
                va="center",
                color="#555555",
                transform=radial_axis.transAxes,
            )
            anisotropy_axis.text(
                0.5,
                0.5,
                "undefined",
                ha="center",
                va="center",
                color="#555555",
                transform=anisotropy_axis.transAxes,
            )
        else:
            spectrum_axis.imshow(
                display_power_db(result.power),
                cmap="gray_r",
                vmin=-25.0,
                vmax=25.0,
                interpolation="nearest",
            )
            radial_axis.plot(
                result.radial_frequency,
                result.radial_power_db,
                color="#0072B2",
                linewidth=1.25,
            )
            radial_axis.axhline(0.0, color="#777777", linewidth=0.6)
            radial_axis.text(
                0.98,
                0.90,
                f"low={100.0 * result.low_frequency_ratio:.2f}%  "
                f"peak={result.peak_frequency:.2f}",
                ha="right",
                va="top",
                fontsize=7,
                transform=radial_axis.transAxes,
            )
            anisotropy_axis.plot(
                result.radial_frequency,
                result.radial_anisotropy_db,
                color="#D55E00",
                linewidth=1.25,
            )
            anisotropy_axis.text(
                0.98,
                0.90,
                f"aggregate={result.anisotropy_db:.1f} dB",
                ha="right",
                va="top",
                fontsize=7,
                transform=anisotropy_axis.transAxes,
            )
        spectrum_axis.set_xticks(())
        spectrum_axis.set_yticks(())
        radial_axis.set_xlim(0.0, 1.0)
        radial_axis.set_ylim(-25.0, 25.0)
        radial_axis.grid(True, color="#E1E1E1", linewidth=0.5)
        anisotropy_axis.set_xlim(0.0, 1.0)
        anisotropy_axis.set_ylim(-30.0, 20.0)
        anisotropy_axis.grid(True, color="#E1E1E1", linewidth=0.5)
        if row + 1 < len(results):
            radial_axis.set_xticklabels(())
            anisotropy_axis.set_xticklabels(())
        else:
            radial_axis.set_xlabel("Spatial frequency / Nyquist")
            anisotropy_axis.set_xlabel("Spatial frequency / Nyquist")
        radial_axis.set_ylabel("Power (dB)")
        anisotropy_axis.set_ylabel("Anisotropy (dB)")

    axes[0][0].set_title("Mid-tone luma error")
    axes[0][1].set_title("Mean 2D periodogram")
    axes[0][2].set_title("Radial power")
    axes[0][3].set_title("Angular variance by radius")
    figure.suptitle(
        "Spatial error spectrum of libsixel dither policies",
        y=0.998,
        fontsize=15,
    )
    figure.text(
        0.5,
        0.990,
        "15 constant gray4 midpoint tones; central 256 x 256 crop; "
        "Hann window; raster scan; periodogram clipped to -25..+25 dB; "
        "lower anisotropy is more isotropic",
        ha="center",
        fontsize=9,
        color="#444444",
    )
    figure.subplots_adjust(
        left=0.085,
        right=0.97,
        bottom=0.035,
        top=0.977,
        hspace=0.24,
        wspace=0.30,
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def family_colors(root: str, count: int) -> List[str]:
    """Return explicit single-root shades for one overview family."""
    roots = {
        "blue": ("#9ECAE1", "#08519C"),
        "orange": ("#FDD0A2", "#A63603"),
        "olive": ("#D9D99B", "#666600"),
    }
    start, end = roots[root]
    cmap = LinearSegmentedColormap.from_list(f"{root}_family", (start, end))
    if count == 1:
        return [cmap(0.7)]
    return [cmap(0.15 + 0.8 * index / (count - 1)) for index in range(count)]


def format_metric(column: int, value: float) -> str:
    """Format one overview matrix cell."""
    if not math.isfinite(value):
        return "n/a"
    if column == 0:
        return f"{value:.0f}"
    if column == 1:
        return f"{100.0 * value:.2f}%"
    if column in (2, 3, 5):
        return f"{value:.2f}"
    if column == 4:
        return f"{value:.1f}"
    return f"{1000.0 * value:.1f}"


def plot_overview(path: Path, results: Sequence[SpectrumResult]) -> None:
    """Plot grouped radial curves and one all-policy descriptor matrix."""
    by_method = {result.method: result for result in results}
    figure = plt.figure(figsize=(15.0, 10.5))
    grid = figure.add_gridspec(2, 3, height_ratios=(1.0, 1.45), hspace=0.32)
    line_styles = ("-", "--", "-.", ":", (0, (5, 2)), (0, (3, 1, 1, 1)))
    for column, (title, methods, root) in enumerate(METHOD_GROUPS):
        axis = figure.add_subplot(grid[0, column])
        colors = family_colors(root, len(methods))
        for index, method in enumerate(methods):
            result = by_method[method]
            axis.plot(
                result.radial_frequency,
                result.radial_power_db,
                color=colors[index],
                linestyle=line_styles[index % len(line_styles)],
                linewidth=1.6,
                label=method,
            )
        axis.axhline(0.0, color="#666666", linewidth=0.7)
        axis.set_xlim(0.0, 1.0)
        axis.set_ylim(-25.0, 25.0)
        axis.set_title(title, fontsize=11)
        axis.set_xlabel("Spatial frequency / Nyquist")
        if column == 0:
            axis.set_ylabel("Radial power (dB)")
        axis.grid(True, color="#E1E1E1", linewidth=0.6)
        axis.legend(frameon=False, fontsize=8, ncol=2 if len(methods) > 3 else 1)

    heat_axis = figure.add_subplot(grid[1, :])
    metric_names = (
        "AC tones\n(/15)",
        "Low-frequency\nenergy",
        "Peak frequency\n(/ Nyquist)",
        "Spectral\ncentroid",
        "Anisotropy\n(dB)",
        "Spectral\nflatness",
        "RMS luma error\n(x 1000)",
    )
    values = np.asarray(
        [
            (
                result.active_tones,
                result.low_frequency_ratio,
                result.peak_frequency,
                result.spectral_centroid,
                result.anisotropy_db,
                result.spectral_flatness,
                result.rms_luma_error,
            )
            for result in results
        ],
        dtype=np.float64,
    )
    normalized = np.full(values.shape, np.nan, dtype=np.float64)
    for column in range(values.shape[1]):
        finite = np.isfinite(values[:, column])
        low = float(np.min(values[finite, column]))
        high = float(np.max(values[finite, column]))
        if high > low:
            normalized[finite, column] = (values[finite, column] - low) / (high - low)
        else:
            normalized[finite, column] = 0.5
    matrix_cmap = LinearSegmentedColormap.from_list(
        "overview_blue",
        ("#F7FBFF", "#6BAED6", "#08519C"),
    )
    matrix_cmap.set_bad("#EEEEEE")
    image = heat_axis.imshow(normalized, cmap=matrix_cmap, vmin=0.0, vmax=1.0, aspect="auto")
    heat_axis.set_xticks(range(len(metric_names)), labels=metric_names)
    heat_axis.set_yticks(
        range(len(results)),
        labels=[result.method for result in results],
    )
    heat_axis.tick_params(top=True, labeltop=True, bottom=False, labelbottom=False)
    for row in range(values.shape[0]):
        for column in range(values.shape[1]):
            value = values[row, column]
            text_color = "white" if normalized[row, column] >= 0.62 else "#222222"
            if not math.isfinite(value):
                text_color = "#666666"
            heat_axis.text(
                column,
                row,
                format_metric(column, value),
                ha="center",
                va="center",
                color=text_color,
                fontsize=8,
            )
    heat_axis.set_title(
        "Spectral descriptors (cell shade ranks values within each column; darker means larger)",
        fontsize=11,
        pad=34,
    )
    colorbar = figure.colorbar(image, ax=heat_axis, orientation="horizontal", pad=0.08, fraction=0.045)
    colorbar.set_ticks((0.0, 1.0), labels=("column minimum", "column maximum"))

    figure.suptitle(
        "Overview of libsixel dither error spectra",
        y=0.985,
        fontsize=16,
    )
    figure.text(
        0.5,
        0.952,
        "Mean of unit-energy periodograms from 15 constant gray4 midpoint tones; "
        "raster scan; the no-dither control has no non-DC spectrum",
        ha="center",
        fontsize=9,
        color="#444444",
    )
    figure.subplots_adjust(left=0.10, right=0.97, bottom=0.07, top=0.91, wspace=0.22)
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def write_csv(path: Path, rows: Sequence[Dict[str, object]],
              fieldnames: Sequence[str]) -> None:
    """Write one measurement table."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def optional_number(value: Optional[float]) -> object:
    """Represent an undefined metric as an empty CSV cell."""
    return "" if value is None or not math.isfinite(value) else value


def result_rows(results: Sequence[SpectrumResult], revision: str,
                img2sixel: str) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Convert measured results into summary and curve rows."""
    summary_rows: List[Dict[str, object]] = []
    curve_rows: List[Dict[str, object]] = []
    for result in results:
        template = command_template(make_command(img2sixel, result.diffusion), img2sixel)
        summary_rows.append(
            {
                "revision": revision,
                "platform": platform.platform(),
                "method": result.method,
                "diffusion_option": result.diffusion,
                "tone_count": len(TONE_VALUES),
                "active_tones": result.active_tones,
                "low_frequency_ratio": optional_number(result.low_frequency_ratio),
                "peak_frequency_nyquist": optional_number(result.peak_frequency),
                "spectral_centroid_nyquist": optional_number(result.spectral_centroid),
                "anisotropy_db": optional_number(result.anisotropy_db),
                "spectral_flatness": optional_number(result.spectral_flatness),
                "rms_luma_error": result.rms_luma_error,
                "mean_luma_bias": result.mean_luma_bias,
                "command": template,
            }
        )
        for index, frequency in enumerate(result.radial_frequency):
            curve_rows.append(
                {
                    "revision": revision,
                    "method": result.method,
                    "frequency_nyquist": frequency,
                    "radial_power_db": optional_number(
                        None
                        if result.radial_power_db is None
                        else result.radial_power_db[index]
                    ),
                    "radial_anisotropy_db": optional_number(
                        None
                        if result.radial_anisotropy_db is None
                        else result.radial_anisotropy_db[index]
                    ),
                }
            )
    return summary_rows, curve_rows


def write_metadata(path: Path, source_root: Path, build_dir: Path,
                   img2sixel: str, revision: str, source_state: str,
                   clean_sixel_environment: bool) -> None:
    """Write provenance and the synthetic spectral-analysis protocol."""
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source": {
            "revision": revision,
            "tracked_worktree_state_at_start": source_state,
        },
        "build": read_build_configuration(build_dir),
        "host": {
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "numpy": np.__version__,
            "matplotlib": matplotlib.__version__,
            "pillow": PILLOW_VERSION,
        },
        "programs": {
            "img2sixel": program_record(img2sixel, source_root),
        },
        "input": {
            "kind": "generated constant RGB P6 fields",
            "field_size": FIELD_SIZE,
            "crop_size": CROP_SIZE,
            "gray_palette": "gray4",
            "tone_values": list(TONE_VALUES),
            "ordered_inputs_sha256": generated_input_sha256(),
        },
        "protocol": {
            "methods": [
                {"name": method, "diffusion_option": diffusion}
                for method, diffusion in DITHER_METHODS
            ],
            "threads": 1,
            "precision": "8bit",
            "loader": "builtin!",
            "working_colorspace": "gamma",
            "lookup_policy": "none",
            "scan": "raster",
            "gpu_policy": "off",
            "output": "palette-preserving PNG stdout",
            "window": "separable Hann",
            "periodogram_normalization": "unit AC energy per active tone, then arithmetic mean",
            "radial_bins": RADIAL_BINS,
            "angular_bins": ANGULAR_BINS,
            "low_frequency_cutoff_nyquist": 0.25,
            "anisotropy_analysis_min_nyquist": 0.05,
            "sixel_environment_removed": clean_sixel_environment,
            "excluded": {
                "auto": "selector rather than a concrete method",
                "interframe": "requires an animation protocol",
                "stbn": "requires an animation protocol",
            },
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--img2sixel")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", default="unknown")
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument("--output-summary-csv", type=Path, required=True)
    parser.add_argument("--output-curves-csv", type=Path, required=True)
    parser.add_argument("--output-atlas", type=Path, required=True)
    parser.add_argument("--output-overview", type=Path, required=True)
    parser.add_argument("--output-metadata", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the complete synthetic dither-spectrum comparison."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve() if args.build_dir else source_root
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    results = [
        measure_method(img2sixel, method, diffusion, command_env)
        for method, diffusion in DITHER_METHODS
    ]
    summary_rows, curve_rows = result_rows(results, args.revision, img2sixel)
    write_csv(
        args.output_summary_csv,
        summary_rows,
        (
            "revision",
            "platform",
            "method",
            "diffusion_option",
            "tone_count",
            "active_tones",
            "low_frequency_ratio",
            "peak_frequency_nyquist",
            "spectral_centroid_nyquist",
            "anisotropy_db",
            "spectral_flatness",
            "rms_luma_error",
            "mean_luma_bias",
            "command",
        ),
    )
    write_csv(
        args.output_curves_csv,
        curve_rows,
        (
            "revision",
            "method",
            "frequency_nyquist",
            "radial_power_db",
            "radial_anisotropy_db",
        ),
    )
    plot_atlas(args.output_atlas, results)
    plot_overview(args.output_overview, results)
    write_metadata(
        args.output_metadata,
        source_root,
        build_dir,
        img2sixel,
        args.revision,
        args.source_state,
        args.clean_sixel_environment,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
