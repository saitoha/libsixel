#!/usr/bin/env python3
"""Measure and visualize palette geometry and realized dither reachability."""

from __future__ import annotations

import argparse
import csv
import ctypes
import datetime
import itertools
import json
import math
import os
import platform
import shlex
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import colors
import numpy as np
from PIL import Image

from evaluate import M_RGB2XYZ, deltaE00, rgb_to_lab, srgb_to_linear, xyz_to_lab
from plot_lookup_policy_speed import (
    display_path,
    file_sha256,
    make_command_environment,
    program_record,
    read_build_configuration,
    resolve_img2sixel,
)
from plot_merge_policy_measurements import (
    command_template,
    make_command,
    merge_records,
    quantizer_records,
)


COLORS = 32
PATCH_SIZE = 32
PATCH_SIZES = (8, 16, 32, 64)
LAB_CELL_WIDTH = 1.0
STUCK_RATIO = 0.5
AMBIGUOUS_LOW = 0.2
AMBIGUOUS_HIGH = 0.8
DE_THRESHOLDS = (1.0, 2.0, 5.0, 10.0)
MERGE_NAMES = ("none", "ward-l0", "ward-l3")
MERGE_LABELS = {
    "none": "No final merge",
    "ward-l0": "Ward only",
    "ward-l3": "Ward + 3 Lloyd passes",
}
STYLES = {
    "none": ("#0072B2", "o", "-"),
    "ward-l0": ("#E69F00", "s", "--"),
    "ward-l3": ("#009E73", "^", "-."),
}


def resolve_libsixel(explicit: str | None, build_dir: Path) -> Path:
    """Resolve the built shared library used by the empirical probe."""
    candidates: List[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    candidates.extend((
        build_dir / "src" / ".libs" / "libsixel.1.dylib",
        build_dir / "src" / ".libs" / "libsixel.so.1",
        build_dir / "src" / ".libs" / "libsixel.so",
        build_dir / "src" / ".libs" / "libsixel.dll",
    ))
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError("Could not find a built libsixel shared library.")


class DitherLibrary:
    """Small ctypes binding for frozen-palette application through libsixel."""

    SIXEL_OK = 0
    SIXEL_DIFFUSE_FS = 0x3
    SIXEL_SCAN_RASTER = 0x1
    SIXEL_LUT_POLICY_NONE = 0x4
    SIXEL_PIXELFORMAT_RGBA8888 = 0x11
    TRANSPARENT_INDEX = 255

    def __init__(self, path: Path) -> None:
        self.path = path
        self.library = ctypes.CDLL(str(path))
        pointer = ctypes.c_void_p
        self.library.sixel_allocator_new.argtypes = [
            ctypes.POINTER(pointer), pointer, pointer, pointer, pointer,
        ]
        self.library.sixel_allocator_new.restype = ctypes.c_int
        self.library.sixel_allocator_unref.argtypes = [pointer]
        self.library.sixel_allocator_unref.restype = None
        self.library.sixel_allocator_free.argtypes = [pointer, pointer]
        self.library.sixel_allocator_free.restype = None
        self.library.sixel_dither_new.argtypes = [
            ctypes.POINTER(pointer), ctypes.c_int, pointer,
        ]
        self.library.sixel_dither_new.restype = ctypes.c_int
        self.library.sixel_dither_unref.argtypes = [pointer]
        self.library.sixel_dither_unref.restype = None
        self.library.sixel_dither_set_palette.argtypes = [
            pointer, ctypes.POINTER(ctypes.c_ubyte),
        ]
        self.library.sixel_dither_set_palette.restype = None
        self.library.sixel_dither_set_pixelformat.argtypes = [
            pointer, ctypes.c_int,
        ]
        self.library.sixel_dither_set_pixelformat.restype = None
        self.library.sixel_dither_set_diffusion_type.argtypes = [
            pointer, ctypes.c_int,
        ]
        self.library.sixel_dither_set_diffusion_type.restype = None
        self.library.sixel_dither_set_diffusion_scan.argtypes = [
            pointer, ctypes.c_int,
        ]
        self.library.sixel_dither_set_diffusion_scan.restype = None
        self.library.sixel_dither_set_lut_policy.argtypes = [
            pointer, ctypes.c_int,
        ]
        self.library.sixel_dither_set_lut_policy.restype = None
        self.library.sixel_dither_set_transparent.argtypes = [
            pointer, ctypes.c_int,
        ]
        self.library.sixel_dither_set_transparent.restype = None
        self.library.sixel_dither_apply_palette.argtypes = [
            pointer,
            ctypes.POINTER(ctypes.c_ubyte),
            ctypes.c_int,
            ctypes.c_int,
        ]
        self.library.sixel_dither_apply_palette.restype = (
            ctypes.POINTER(ctypes.c_ubyte)
        )
        self.library.sixel_decode_direct.argtypes = [
            ctypes.POINTER(ctypes.c_ubyte),
            ctypes.c_int,
            ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte)),
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int),
            pointer,
        ]
        self.library.sixel_decode_direct.restype = ctypes.c_int

    def apply(self, palette: np.ndarray, rgba: np.ndarray) -> np.ndarray:
        """Apply one frozen palette with FS raster diffusion and exact lookup."""
        allocator = ctypes.c_void_p()
        dither = ctypes.c_void_p()
        palette_bytes = np.ascontiguousarray(palette, dtype=np.uint8)
        pixels = np.ascontiguousarray(rgba, dtype=np.uint8)
        status = self.library.sixel_allocator_new(
            ctypes.byref(allocator), None, None, None, None
        )
        if status != self.SIXEL_OK:
            raise RuntimeError(f"sixel_allocator_new failed: {status:#x}")
        try:
            status = self.library.sixel_dither_new(
                ctypes.byref(dither), int(len(palette_bytes)), allocator
            )
            if status != self.SIXEL_OK:
                raise RuntimeError(f"sixel_dither_new failed: {status:#x}")
            try:
                palette_pointer = palette_bytes.ctypes.data_as(
                    ctypes.POINTER(ctypes.c_ubyte)
                )
                pixel_pointer = pixels.ctypes.data_as(
                    ctypes.POINTER(ctypes.c_ubyte)
                )
                self.library.sixel_dither_set_palette(
                    dither, palette_pointer
                )
                self.library.sixel_dither_set_pixelformat(
                    dither, self.SIXEL_PIXELFORMAT_RGBA8888
                )
                self.library.sixel_dither_set_diffusion_type(
                    dither, self.SIXEL_DIFFUSE_FS
                )
                self.library.sixel_dither_set_diffusion_scan(
                    dither, self.SIXEL_SCAN_RASTER
                )
                self.library.sixel_dither_set_lut_policy(
                    dither, self.SIXEL_LUT_POLICY_NONE
                )
                self.library.sixel_dither_set_transparent(
                    dither, self.TRANSPARENT_INDEX
                )
                result_pointer = self.library.sixel_dither_apply_palette(
                    dither,
                    pixel_pointer,
                    int(pixels.shape[1]),
                    int(pixels.shape[0]),
                )
                if not result_pointer:
                    raise RuntimeError("sixel_dither_apply_palette failed")
                try:
                    result = np.ctypeslib.as_array(
                        result_pointer,
                        shape=(pixels.shape[0] * pixels.shape[1],),
                    ).copy()
                finally:
                    self.library.sixel_allocator_free(
                        allocator, result_pointer
                    )
                return result.reshape(pixels.shape[:2])
            finally:
                if dither.value:
                    self.library.sixel_dither_unref(dither)
        finally:
            self.library.sixel_allocator_unref(allocator)

    def decode_direct(self, encoded: bytes) -> np.ndarray:
        """Decode one in-memory SIXEL stream through the production decoder."""
        allocator = ctypes.c_void_p()
        output_pointer = ctypes.POINTER(ctypes.c_ubyte)()
        width = ctypes.c_int()
        height = ctypes.c_int()
        source = (ctypes.c_ubyte * len(encoded)).from_buffer_copy(encoded)
        status = self.library.sixel_allocator_new(
            ctypes.byref(allocator), None, None, None, None
        )
        if status != self.SIXEL_OK:
            raise RuntimeError(f"sixel_allocator_new failed: {status:#x}")
        try:
            status = self.library.sixel_decode_direct(
                source,
                len(encoded),
                ctypes.byref(output_pointer),
                ctypes.byref(width),
                ctypes.byref(height),
                allocator,
            )
            if status != self.SIXEL_OK or not output_pointer:
                raise RuntimeError(f"sixel_decode_direct failed: {status:#x}")
            try:
                output = np.ctypeslib.as_array(
                    output_pointer,
                    shape=(height.value * width.value * 4,),
                ).copy()
            finally:
                self.library.sixel_allocator_free(allocator, output_pointer)
        finally:
            self.library.sixel_allocator_unref(allocator)
        return output.reshape(height.value, width.value, 4)


def write_csv(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    """Write dictionaries with one stable union of fields."""
    fieldnames: List[str] = []
    for row in rows:
        for field in row:
            if field not in fieldnames:
                fieldnames.append(field)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=fieldnames, lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)


def read_act(path: Path) -> np.ndarray:
    """Read the exported ACT palette and honor its four-byte count trailer."""
    payload = path.read_bytes()
    if len(payload) != 772:
        raise ValueError(f"unexpected ACT length: {len(payload)}")
    count = (payload[768] << 8) | payload[769]
    if count == 0:
        count = 256
    if not 1 <= count <= 256:
        raise ValueError(f"invalid ACT palette count: {count}")
    return np.frombuffer(payload[:count * 3], dtype=np.uint8).reshape(-1, 3).copy()


def run_palette_build(img2sixel: str,
                      input_image: Path,
                      merge: Dict[str, object],
                      output_act: Path,
                      command_env: Dict[str, str]) -> Tuple[str, bytes]:
    """Build one controlled K=32 Heckbert palette and capture SIXEL and ACT."""
    quantizer = next(
        record for record in quantizer_records()
        if record["quantizer"] == "heckbert"
    )
    command = make_command(
        img2sixel,
        input_image,
        COLORS,
        quantizer,
        merge,
        False,
    )
    command = list(command[:-1]) + [
        "-M", f"act:{output_act}",
        command[-1],
    ]
    proc = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=command_env,
        check=False,
    )
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"palette capture failed ({proc.returncode}): "
            f"{shlex.join(command)}\n{diagnostic}"
        )
    template = command_template(command, img2sixel, input_image)
    template = template.replace(str(output_act), "{palette}")
    return template, proc.stdout


def occupied_lab_cells(rgb_u8: np.ndarray) \
        -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Return one RGB representative, mass, Lab key, and inverse per cell."""
    flat = rgb_u8.reshape(-1, 3)
    lab = rgb_to_lab(flat.astype(np.float64) / 255.0)
    keys = np.floor(lab / LAB_CELL_WIDTH).astype(np.int32)
    unique_keys, inverse = np.unique(keys, axis=0, return_inverse=True)
    count = np.bincount(inverse).astype(np.int64)
    sums = np.stack([
        np.bincount(inverse, weights=flat[:, channel])
        for channel in range(3)
    ], axis=1)
    representatives = np.rint(sums / count[:, None]).clip(0, 255).astype(np.uint8)
    return representatives, count, unique_keys, inverse


def supporting_halfspaces(points: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """Enumerate oriented supporting planes of a small three-dimensional hull."""
    planes: Dict[Tuple[int, ...], Tuple[np.ndarray, float]] = {}
    centroid = points.mean(axis=0)
    scale = max(float(np.ptp(points, axis=0).max()), 1.0)
    side_epsilon = scale * 1.0e-9
    for first, second, third in itertools.combinations(range(len(points)), 3):
        origin = points[first]
        normal = np.cross(points[second] - origin, points[third] - origin)
        length = float(np.linalg.norm(normal))
        if length <= side_epsilon:
            continue
        normal /= length
        signed = (points - origin) @ normal
        if not (np.all(signed <= side_epsilon)
                or np.all(signed >= -side_epsilon)):
            continue
        if float((centroid - origin) @ normal) > 0.0:
            normal = -normal
        offset = -float(normal @ origin)
        key = tuple(np.rint(np.concatenate((normal, [offset])) * 1.0e8)
                    .astype(np.int64).tolist())
        planes[key] = (normal.copy(), offset)
    if not planes:
        raise RuntimeError("palette does not define a three-dimensional hull")
    normals = np.stack([plane[0] for plane in planes.values()])
    offsets = np.array([plane[1] for plane in planes.values()])
    return normals, offsets


def points_inside_hull(points: np.ndarray,
                       normals: np.ndarray,
                       offsets: np.ndarray) -> np.ndarray:
    """Test points against precomputed supporting halfspaces."""
    return np.all(points @ normals.T + offsets[None, :] <= 2.0e-7, axis=1)


def build_patch_batch(colors_rgb: np.ndarray,
                      patch_size: int,
                      columns: int = 16) -> Tuple[np.ndarray, List[Tuple[slice, slice]]]:
    """Pack flat patches behind alpha-zero fences in one RGBA image."""
    rows = int(math.ceil(len(colors_rgb) / columns))
    stride = patch_size + 1
    image = np.zeros((rows * stride, columns * stride, 4), dtype=np.uint8)
    regions: List[Tuple[slice, slice]] = []
    for index, color_value in enumerate(colors_rgb):
        row = index // columns
        column = index % columns
        y = row * stride
        x = column * stride
        image[y:y + patch_size, x:x + patch_size, :3] = color_value
        image[y:y + patch_size, x:x + patch_size, 3] = 255
        regions.append((slice(y, y + patch_size), slice(x, x + patch_size)))
    return image, regions


def linear_rgb_to_lab(linear_rgb: np.ndarray) -> np.ndarray:
    """Convert additive linear-light RGB means to CIELAB D65."""
    xyz = linear_rgb @ M_RGB2XYZ.T
    return xyz_to_lab(xyz)


def empirical_probe(library: DitherLibrary,
                    palette: np.ndarray,
                    targets: np.ndarray,
                    patch_size: int,
                    chunk_size: int = 1024) -> Dict[str, np.ndarray]:
    """Run frozen-palette flat patches through the real FS implementation."""
    gamma_means = np.empty((len(targets), 3), dtype=np.float64)
    linear_means = np.empty((len(targets), 3), dtype=np.float64)
    distinct = np.empty(len(targets), dtype=np.int32)
    palette_gamma = palette.astype(np.float64) / 255.0
    palette_linear = srgb_to_linear(palette_gamma)
    for start in range(0, len(targets), chunk_size):
        stop = min(start + chunk_size, len(targets))
        image, regions = build_patch_batch(targets[start:stop], patch_size)
        indexes = library.apply(palette, image)
        for local_index, (yslice, xslice) in enumerate(regions):
            values = indexes[yslice, xslice].reshape(-1).astype(np.int64)
            if np.any(values >= len(palette)):
                raise RuntimeError("transparent fence leaked into a probe patch")
            histogram = np.bincount(values, minlength=len(palette)).astype(np.float64)
            weights = histogram / histogram.sum()
            output_index = start + local_index
            gamma_means[output_index] = weights @ palette_gamma
            linear_means[output_index] = weights @ palette_linear
            distinct[output_index] = int(np.count_nonzero(histogram))
    target_gamma = targets.astype(np.float64) / 255.0
    nearest_distance = np.sqrt(np.min(
        np.sum((target_gamma[:, None, :] - palette_gamma[None, :, :]) ** 2,
               axis=2),
        axis=1,
    ))
    residual_distance = np.linalg.norm(target_gamma - gamma_means, axis=1)
    ratio = np.zeros_like(residual_distance)
    nonzero = nearest_distance > 1.0e-12
    ratio[nonzero] = residual_distance[nonzero] / nearest_distance[nonzero]
    target_lab = rgb_to_lab(target_gamma)
    output_lab = linear_rgb_to_lab(linear_means)
    return {
        "gamma_mean": gamma_means,
        "linear_mean": linear_means,
        "nearest_distance": nearest_distance,
        "residual_distance": residual_distance,
        "residual_ratio": ratio,
        "stuck": np.logical_and(nonzero, ratio >= STUCK_RATIO),
        "delta_e00": deltaE00(target_lab, output_lab),
        "distinct_entries": distinct,
    }


def weighted_percentile(values: np.ndarray,
                        weights: np.ndarray,
                        fraction: float) -> float:
    """Return an exact discrete weighted percentile."""
    if len(values) == 0:
        return 0.0
    order = np.argsort(values)
    sorted_values = values[order]
    sorted_weights = weights[order].astype(np.float64)
    cumulative = np.cumsum(sorted_weights)
    target = fraction * cumulative[-1]
    return float(sorted_values[np.searchsorted(cumulative, target, side="left")])


def percentage(mask: np.ndarray, weights: np.ndarray | None = None) -> float:
    """Return an unweighted or weighted percentage."""
    if weights is None:
        return 100.0 * float(np.count_nonzero(mask)) / float(len(mask))
    return 100.0 * float(weights[mask].sum()) / float(weights.sum())


def conditional_percentage(mask: np.ndarray,
                           domain: np.ndarray,
                           weights: np.ndarray | None = None) -> float:
    """Return the percentage of a mask within one explicitly bounded domain."""
    if not np.any(domain):
        return 0.0
    if weights is None:
        return (100.0 * float(np.count_nonzero(mask & domain))
                / float(np.count_nonzero(domain)))
    return (100.0 * float(weights[mask & domain].sum())
            / float(weights[domain].sum()))


def palette_uniformity(palette: np.ndarray) -> Dict[str, float]:
    """Summarize nearest-neighbor spacing in CIEDE2000."""
    lab = rgb_to_lab(palette.astype(np.float64) / 255.0)
    matrix = np.full((len(palette), len(palette)), np.inf, dtype=np.float64)
    for index in range(len(palette)):
        left = np.repeat(lab[index:index + 1], len(palette), axis=0)
        matrix[index] = deltaE00(left, lab)
        matrix[index, index] = np.inf
    nearest = matrix.min(axis=1)
    mean = float(nearest.mean())
    return {
        "palette_nn_de00_mean": mean,
        "palette_nn_de00_median": float(np.median(nearest)),
        "palette_nn_de00_p10": float(np.percentile(nearest, 10)),
        "palette_nn_de00_p90": float(np.percentile(nearest, 90)),
        "palette_nn_de00_cv": (
            float(nearest.std()) / mean if mean > 0.0 else 0.0
        ),
    }


def summarize_policy(name: str,
                     palette: np.ndarray,
                     mass: np.ndarray,
                     hull_inside: np.ndarray,
                     empirical: Dict[str, np.ndarray],
                     command: str) -> Dict[str, object]:
    """Build one row separating geometry, closed-loop behavior, and severity."""
    stuck = empirical["stuck"].astype(bool)
    unavailable = np.logical_or(~hull_inside, stuck & hull_inside)
    ambiguous = (
        (empirical["residual_ratio"] > AMBIGUOUS_LOW)
        & (empirical["residual_ratio"] < AMBIGUOUS_HIGH)
    )
    delta_e = empirical["delta_e00"]
    unavailable_delta_e = delta_e[unavailable]
    unavailable_mass = mass[unavailable]
    row: Dict[str, object] = {
        "merge_config": name,
        "merge_label": MERGE_LABELS[name],
        "colors": COLORS,
        "palette_entries": len(palette),
        "probe_cells": len(mass),
        "source_pixels": int(mass.sum()),
        "lab_cell_width": LAB_CELL_WIDTH,
        "patch_size": PATCH_SIZE,
        "diffusion": "fs",
        "scan": "raster",
        "lookup_policy": "none",
        "stuck_ratio_threshold": STUCK_RATIO,
        "geometric_unreachable_volume_percent": percentage(~hull_inside),
        "geometric_unreachable_area_percent": percentage(~hull_inside, mass),
        "ratio_classified_unreachable_volume_percent": percentage(
            unavailable
        ),
        "ratio_classified_unreachable_area_percent": percentage(
            unavailable, mass
        ),
        "hull_inside_ratio_classified_volume_percent": percentage(
            stuck & hull_inside
        ),
        "hull_inside_ratio_classified_area_percent": percentage(
            stuck & hull_inside, mass
        ),
        "ambiguous_ratio_volume_percent": conditional_percentage(
            ambiguous, hull_inside
        ),
        "ambiguous_ratio_area_percent": conditional_percentage(
            ambiguous, hull_inside, mass
        ),
        "ratio_classified_delta_e00_area_mean": (
            float(np.average(unavailable_delta_e, weights=unavailable_mass))
            if len(unavailable_delta_e) else 0.0
        ),
        "ratio_classified_delta_e00_area_p95": weighted_percentile(
            unavailable_delta_e, unavailable_mass, 0.95
        ),
        "all_delta_e00_area_mean": float(np.average(delta_e, weights=mass)),
        "all_delta_e00_area_p95": weighted_percentile(delta_e, mass, 0.95),
        "single_entry_volume_percent": percentage(
            empirical["distinct_entries"] == 1
        ),
        "single_entry_area_percent": percentage(
            empirical["distinct_entries"] == 1, mass
        ),
        "command": command,
        **palette_uniformity(palette),
    }
    for threshold in DE_THRESHOLDS:
        field = str(threshold).replace(".", "_")
        row[f"de00_coverage_area_at_{field}"] = percentage(
            delta_e <= threshold, mass
        )
    return row


def sensitivity_sample(keys: np.ndarray, mass: np.ndarray) -> np.ndarray:
    """Choose deterministic color-volume and high-area probes for size sweeps."""
    lexicographic = np.lexsort((keys[:, 2], keys[:, 1], keys[:, 0]))
    spaced_positions = np.linspace(
        0, len(lexicographic) - 1, 256, dtype=np.int64
    )
    volume_sample = lexicographic[spaced_positions]
    area_sample = np.argsort(-mass, kind="stable")[:256]
    return np.unique(np.concatenate((volume_sample, area_sample)))


def measure_patch_sensitivity(library: DitherLibrary,
                              palettes: Dict[str, np.ndarray],
                              targets: np.ndarray,
                              mass: np.ndarray,
                              sample_indexes: np.ndarray,
                              hull_inside_by_policy: Dict[str, np.ndarray]) \
        -> List[Dict[str, object]]:
    """Measure the finite-area layer on a deterministic probe subset."""
    rows: List[Dict[str, object]] = []
    sample_targets = targets[sample_indexes]
    sample_mass = mass[sample_indexes]
    for name in MERGE_NAMES:
        for patch_size in PATCH_SIZES:
            empirical = empirical_probe(
                library, palettes[name], sample_targets, patch_size
            )
            stuck = empirical["stuck"].astype(bool)
            hull_inside = hull_inside_by_policy[name][sample_indexes]
            unavailable = np.logical_or(~hull_inside, stuck & hull_inside)
            ambiguous = (
                (empirical["residual_ratio"] > AMBIGUOUS_LOW)
                & (empirical["residual_ratio"] < AMBIGUOUS_HIGH)
            )
            rows.append({
                "merge_config": name,
                "merge_label": MERGE_LABELS[name],
                "colors": COLORS,
                "patch_size": patch_size,
                "probe_sample_count": len(sample_indexes),
                "ratio_classified_unreachable_volume_percent": percentage(
                    unavailable
                ),
                "ratio_classified_unreachable_area_percent": percentage(
                    unavailable, sample_mass
                ),
                "ambiguous_ratio_volume_percent": conditional_percentage(
                    ambiguous, hull_inside
                ),
            })
    return rows


def style_axis(axis: plt.Axes) -> None:
    """Apply the shared restrained measurement style."""
    axis.grid(True, color="#D9D9D9", linewidth=0.7, zorder=0)
    axis.spines[["top", "right"]].set_visible(False)


def plot_mechanics(path: Path) -> None:
    """Draw a deterministic point-motion explanation of Ward and Lloyd."""
    rng = np.random.default_rng(7)
    groups = (
        rng.normal((0.20, 0.28), (0.055, 0.045), size=(28, 2)),
        rng.normal((0.42, 0.34), (0.045, 0.060), size=(22, 2)),
        rng.normal((0.73, 0.68), (0.070, 0.055), size=(34, 2)),
        rng.normal((0.58, 0.76), (0.035, 0.045), size=(14, 2)),
    )
    samples = np.vstack(groups)
    weights = np.array([len(group) for group in groups], dtype=np.float64)
    centers = np.array([group.mean(axis=0) for group in groups])
    ward_center = (
        weights[0] * centers[0] + weights[1] * centers[1]
    ) / (weights[0] + weights[1])
    ward_centers = np.vstack((ward_center, centers[2], centers[3]))
    assignments = np.argmin(
        np.sum((samples[:, None, :] - ward_centers[None, :, :]) ** 2, axis=2),
        axis=1,
    )
    lloyd_centers = np.stack([
        samples[assignments == index].mean(axis=0)
        for index in range(3)
    ])
    figure, axes = plt.subplots(1, 3, figsize=(12.0, 4.2), sharex=True, sharey=True)
    for axis in axes:
        axis.scatter(
            samples[:, 0], samples[:, 1], s=14, color="#94A3B8",
            alpha=0.65, edgecolors="none", zorder=1,
        )
        axis.set_xlim(0.05, 0.9)
        axis.set_ylim(0.12, 0.9)
        axis.set_xticks([])
        axis.set_yticks([])
        axis.spines[:].set_color("#CBD5E1")
    axes[0].scatter(
        centers[:, 0], centers[:, 1], marker="X", s=120,
        color="#0072B2", edgecolors="white", linewidths=1.0, zorder=3,
    )
    axes[0].plot(
        centers[:2, 0], centers[:2, 1], "--", color="#D55E00", linewidth=2
    )
    axes[0].set_title("1. Oversplit palette: Q centers", fontsize=11)
    for index in (0, 1):
        axes[1].annotate(
            "", xy=ward_center, xytext=centers[index],
            arrowprops={"arrowstyle": "->", "color": "#D55E00", "lw": 2},
        )
    axes[1].scatter(
        ward_centers[:, 0], ward_centers[:, 1], marker="X", s=120,
        color="#D55E00", edgecolors="white", linewidths=1.0, zorder=3,
    )
    axes[1].set_title("2. Ward: cheapest pair becomes one", fontsize=11)
    axes[2].scatter(
        ward_centers[:, 0], ward_centers[:, 1], marker="x", s=80,
        color="#64748B", linewidths=2, zorder=2,
    )
    for old, new in zip(ward_centers, lloyd_centers):
        axes[2].annotate(
            "", xy=new, xytext=old,
            arrowprops={"arrowstyle": "->", "color": "#009E73", "lw": 2},
        )
    axes[2].scatter(
        lloyd_centers[:, 0], lloyd_centers[:, 1], marker="X", s=120,
        color="#009E73", edgecolors="white", linewidths=1.0, zorder=3,
    )
    axes[2].set_title("3. Lloyd: reassign, then move means", fontsize=11)
    figure.suptitle("Ward reduces the count; Lloyd polishes the surviving centers")
    figure.text(
        0.5, 0.015,
        "Schematic 2D projection; arrows show center movement, not pixel motion",
        ha="center", fontsize=9, color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.05, 1.0, 0.93))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170, facecolor="white")
    plt.close(figure)


def delta_e_map(reference: np.ndarray, candidate: np.ndarray) -> np.ndarray:
    """Return a per-pixel CIEDE2000 map for two RGB byte images."""
    reference_lab = rgb_to_lab(reference.astype(np.float64) / 255.0)
    candidate_lab = rgb_to_lab(candidate.astype(np.float64) / 255.0)
    return deltaE00(reference_lab, candidate_lab)


def best_crop(error_none: np.ndarray,
              error_ward: np.ndarray) -> Tuple[slice, slice]:
    """Choose a deterministic crop with the largest Ward error reduction."""
    height, width = error_none.shape
    crop_height = min(180, height)
    crop_width = min(240, width)
    stride_y = max(crop_height // 4, 1)
    stride_x = max(crop_width // 4, 1)
    improvement = error_none - error_ward
    best_score = -math.inf
    best = (slice(0, crop_height), slice(0, crop_width))
    for y in range(0, height - crop_height + 1, stride_y):
        for x in range(0, width - crop_width + 1, stride_x):
            score = float(improvement[y:y + crop_height,
                                      x:x + crop_width].mean())
            if score > best_score:
                best_score = score
                best = (slice(y, y + crop_height), slice(x, x + crop_width))
    return best


def plot_visual_comparison(path: Path,
                           source: np.ndarray,
                           outputs: Dict[str, np.ndarray]) -> None:
    """Show a natural-image crop and spatial Delta E00 changes."""
    error_maps = {
        name: delta_e_map(source, outputs[name]) for name in MERGE_NAMES
    }
    crop = best_crop(error_maps["none"], error_maps["ward-l3"])
    vmax = max(5.0, float(np.percentile(np.concatenate([
        error[crop].ravel() for error in error_maps.values()
    ]), 98)))
    figure, axes = plt.subplots(
        2, 4, figsize=(13.0, 7.0), constrained_layout=True
    )
    panels = (("Source", source),) + tuple(
        (MERGE_LABELS[name], outputs[name]) for name in MERGE_NAMES
    )
    for column, (label, image_value) in enumerate(panels):
        axes[0][column].imshow(image_value[crop])
        axes[0][column].set_title(label, fontsize=10)
        axes[0][column].axis("off")
    yslice, xslice = crop
    axes[1][0].imshow(source)
    rectangle = plt.Rectangle(
        (xslice.start, yslice.start), xslice.stop - xslice.start,
        yslice.stop - yslice.start, fill=False, color="#D55E00", linewidth=2,
    )
    axes[1][0].add_patch(rectangle)
    axes[1][0].set_title("Automatic detail locator", fontsize=10)
    axes[1][0].axis("off")
    image_handle = None
    for column, name in enumerate(MERGE_NAMES, start=1):
        image_handle = axes[1][column].imshow(
            error_maps[name], cmap="magma", vmin=0.0, vmax=vmax
        )
        axes[1][column].set_title(
            f"{MERGE_LABELS[name]}: per-pixel Delta E00", fontsize=10
        )
        axes[1][column].axis("off")
    if image_handle is not None:
        colorbar = figure.colorbar(
            image_handle, ax=axes[1][1:], orientation="horizontal",
            fraction=0.045, pad=0.025,
        )
        colorbar.set_label("Delta E00 (lower is better)")
    figure.suptitle("What final merge changes on snake.png at K=32")
    figure.supxlabel(
        "Heckbert; 8bit; no dithering; exact lookup; cover and snap disabled",
        fontsize=9, color="#555555",
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170, facecolor="white")
    plt.close(figure)


def plot_bimodality(path: Path,
                    empirical_by_policy: Dict[str, Dict[str, np.ndarray]],
                    hull_inside_by_policy: Dict[str, np.ndarray],
                    mass: np.ndarray) -> None:
    """Plot residual ratios with and without source-area weighting."""
    bins = np.linspace(0.0, 1.25, 76)
    figure, axes = plt.subplots(1, 2, figsize=(11.5, 4.4), sharex=True)
    for name in MERGE_NAMES:
        hull_inside = hull_inside_by_policy[name]
        values = np.clip(
            empirical_by_policy[name]["residual_ratio"][hull_inside],
            0.0,
            bins[-1],
        )
        color_value, marker, line_style = STYLES[name]
        axes[0].hist(
            values, bins=bins, histtype="step", density=True,
            color=color_value, linestyle=line_style, linewidth=1.8,
            label=MERGE_LABELS[name],
        )
        axes[1].hist(
            values, bins=bins,
            weights=mass[hull_inside] / mass[hull_inside].sum(),
            histtype="step",
            color=color_value, linestyle=line_style, linewidth=1.8,
            label=MERGE_LABELS[name],
        )
    for axis in axes:
        axis.axvspan(
            AMBIGUOUS_LOW, AMBIGUOUS_HIGH, color="#94A3B8", alpha=0.12,
            label="ambiguous band" if axis is axes[0] else None,
        )
        axis.axvline(STUCK_RATIO, color="#111827", linewidth=1.0)
        axis.set_xlabel("Residual / nearest-entry distance")
        style_axis(axis)
    axes[0].set_ylabel("Occupied Lab-cell density")
    axes[1].set_ylabel("Fraction of source pixels")
    axes[0].set_title("Color-volume view (one vote per occupied cell)")
    axes[1].set_title("Image-area view (weighted by pixel population)")
    axes[0].legend(fontsize=8, frameon=False)
    figure.suptitle("Inside the convex hull, flat patches test the closed loop")
    figure.text(
        0.5, 0.01,
        "K=32 frozen palettes; 32x32 patches; actual FS raster diffusion; exact lookup",
        ha="center", fontsize=9, color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.04, 1.0, 0.93))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170, facecolor="white")
    plt.close(figure)


def plot_reachability(path: Path,
                      summary_rows: Sequence[Dict[str, object]],
                      patch_rows: Sequence[Dict[str, object]],
                      empirical_by_policy: Dict[str, Dict[str, np.ndarray]],
                      mass: np.ndarray) -> None:
    """Combine geometric, empirical, perceptual, and finite-area views."""
    figure, axes = plt.subplots(2, 2, figsize=(12.0, 8.4))
    x = np.arange(len(MERGE_NAMES), dtype=np.float64)
    width = 0.19
    fields = (
        ("geometric_unreachable_volume_percent", "Hull outside, volume", "#9CA3AF", "//"),
        ("ratio_classified_unreachable_volume_percent", "Ratio test, volume", "#D55E00", ""),
        ("geometric_unreachable_area_percent", "Hull outside, area", "#CBD5E1", "\\\\"),
        ("ratio_classified_unreachable_area_percent", "Ratio test, area", "#0072B2", ".."),
    )
    for index, (field, label, color_value, hatch) in enumerate(fields):
        values = [float(row[field]) for row in summary_rows]
        axes[0][0].bar(
            x + (index - 1.5) * width, values, width,
            label=label, color=color_value, edgecolor="#374151",
            linewidth=0.5, hatch=hatch, zorder=2,
        )
    axes[0][0].set_xticks(x, ["None", "Ward", "Ward+L3"])
    axes[0][0].set_ylabel("Unreachable (%)")
    axes[0][0].set_title(
        "Convex-hull upper bound vs experimental residual classifier"
    )
    axes[0][0].legend(fontsize=7.5, frameon=False, ncols=2)
    style_axis(axes[0][0])

    thresholds = np.linspace(0.0, 20.0, 161)
    for name in MERGE_NAMES:
        empirical = empirical_by_policy[name]
        values = [percentage(empirical["delta_e00"] <= threshold, mass)
                  for threshold in thresholds]
        color_value, marker, line_style = STYLES[name]
        axes[0][1].plot(
            thresholds, values, color=color_value, linestyle=line_style,
            linewidth=1.8, label=MERGE_LABELS[name],
        )
    for threshold in DE_THRESHOLDS:
        axes[0][1].axvline(
            threshold, color="#94A3B8", linewidth=0.6, linestyle=":"
        )
    axes[0][1].set_xlim(0.0, 20.0)
    axes[0][1].set_ylim(0.0, 100.0)
    axes[0][1].set_xlabel("Allowed Delta E00 after spatial linear-light mean")
    axes[0][1].set_ylabel("Source area covered (%)")
    axes[0][1].set_title("Perceptual threshold changes the coverage claim")
    axes[0][1].legend(fontsize=8, frameon=False)
    style_axis(axes[0][1])

    for name in MERGE_NAMES:
        selected = [
            row for row in patch_rows if row["merge_config"] == name
        ]
        selected.sort(key=lambda row: int(row["patch_size"]))
        color_value, marker, line_style = STYLES[name]
        axes[1][0].plot(
            [int(row["patch_size"]) for row in selected],
            [float(row["ratio_classified_unreachable_area_percent"])
             for row in selected],
            color=color_value, marker=marker, linestyle=line_style,
            linewidth=1.8, markersize=5, label=MERGE_LABELS[name],
        )
    axes[1][0].set_xscale("log", base=2)
    axes[1][0].set_xticks(PATCH_SIZES, [str(value) for value in PATCH_SIZES])
    axes[1][0].set_xlabel("Flat-patch width and height (pixels)")
    axes[1][0].set_ylabel("Ratio-classified unavailable area in subset (%)")
    axes[1][0].set_title("Finite area is a third reachability constraint")
    axes[1][0].legend(fontsize=8, frameon=False)
    style_axis(axes[1][0])

    positions = np.arange(len(MERGE_NAMES), dtype=np.float64)
    means = [float(row["palette_nn_de00_mean"]) for row in summary_rows]
    lower = [
        float(row["palette_nn_de00_mean"])
        - float(row["palette_nn_de00_p10"])
        for row in summary_rows
    ]
    upper = [
        float(row["palette_nn_de00_p90"])
        - float(row["palette_nn_de00_mean"])
        for row in summary_rows
    ]
    axes[1][1].errorbar(
        positions, means, yerr=[lower, upper], fmt="o", capsize=5,
        color="#332288", ecolor="#94A3B8", linewidth=1.5,
    )
    for index, row in enumerate(summary_rows):
        axes[1][1].annotate(
            f"CV {float(row['palette_nn_de00_cv']):.2f}",
            (positions[index], means[index]), xytext=(0, 12),
            textcoords="offset points", ha="center", fontsize=8,
        )
    axes[1][1].set_xticks(positions, ["None", "Ward", "Ward+L3"])
    axes[1][1].set_ylabel("Nearest-palette-neighbor Delta E00")
    axes[1][1].set_title("Palette spacing (mean, p10-p90; lower CV is uniform)")
    style_axis(axes[1][1])

    figure.suptitle("Final merge changes geometry, but geometry is not reachability")
    figure.text(
        0.5, 0.012,
        "snake.png; Heckbert K=32; frozen palette; FS raster; exact lookup; cover and snap disabled",
        ha="center", fontsize=9, color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.95))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170, facecolor="white")
    plt.close(figure)


def plot_hotspots(path: Path,
                  source: np.ndarray,
                  inverse: np.ndarray,
                  empirical_by_policy: Dict[str, Dict[str, np.ndarray]],
                  hull_inside_by_policy: Dict[str, np.ndarray],
                  mass: np.ndarray) -> None:
    """Map ratio-classified unavailable Lab cells back to source pixels."""
    height, width = source.shape[:2]
    figure, axes = plt.subplots(1, 4, figsize=(14.0, 4.1))
    axes[0].imshow(source)
    axes[0].set_title("Source: snake.png")
    axes[0].axis("off")
    magenta = np.array([0.86, 0.08, 0.38], dtype=np.float64)
    for axis, name in zip(axes[1:], MERGE_NAMES):
        stuck = empirical_by_policy[name]["stuck"].astype(bool)
        hull_inside = hull_inside_by_policy[name]
        unavailable = np.logical_or(~hull_inside, stuck & hull_inside)
        pixel_mask = unavailable[inverse].reshape(height, width)
        overlay = source.astype(np.float64) / 255.0
        muted = 0.22 + 0.58 * overlay
        rendered = muted.copy()
        rendered[pixel_mask] = (
            0.18 * overlay[pixel_mask] + 0.82 * magenta
        )
        axis.imshow(np.clip(rendered, 0.0, 1.0))
        axis.contour(pixel_mask.astype(np.uint8), levels=[0.5],
                     colors=["white"], linewidths=0.35)
        area = percentage(unavailable, mass)
        axis.set_title(
            f"{MERGE_LABELS[name]}\nClassifier-marked area {area:.1f}%"
        )
        axis.axis("off")
    figure.suptitle(
        "Candidate hard-to-realize colors under an experimental classifier"
    )
    figure.text(
        0.5, 0.015,
        "Magenta marks hull-exterior cells plus hull-interior cells retaining at least half the nearest-entry residual",
        ha="center", fontsize=9, color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.05, 1.0, 0.91))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170, facecolor="white")
    plt.close(figure)


def validate_delta_e() -> float:
    """Check the local CIEDE2000 implementation against Sharma pair 1."""
    left = np.array([[50.0, 2.6772, -79.7751]])
    right = np.array([[50.0, 0.0, -82.7485]])
    value = float(deltaE00(left, right)[0])
    if abs(value - 2.0425) > 0.0001:
        raise RuntimeError(f"CIEDE2000 self-check failed: {value:.6f}")
    return value


def validate_batch_fence(library: DitherLibrary,
                         palette: np.ndarray,
                         targets: np.ndarray) -> int:
    """Prove batched alpha fences match isolated patch application."""
    selected = targets[[0, len(targets) // 2, len(targets) - 1]]
    batched = empirical_probe(library, palette, selected, PATCH_SIZE)
    for index, target in enumerate(selected):
        isolated = empirical_probe(
            library, palette, target.reshape(1, 3), PATCH_SIZE
        )
        if not np.allclose(
                batched["gamma_mean"][index], isolated["gamma_mean"][0],
                rtol=0.0, atol=0.0):
            raise RuntimeError("alpha-fenced batch differs from isolated probe")
    return len(selected)


def write_metadata(path: Path,
                   source_root: Path,
                   build_dir: Path,
                   input_image: Path,
                   img2sixel: str,
                   library_path: Path,
                   revision: str,
                   source_state: str,
                   commands: Dict[str, str],
                   probe_count: int,
                   sensitivity_count: int,
                   supporting_plane_counts: Dict[str, int],
                   ciede_reference: float,
                   fence_validation_count: int) -> None:
    """Write provenance, protocol, and chart contracts."""
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
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
            "pillow": Image.__version__,
            "matplotlib": matplotlib.__version__,
        },
        "input": {
            "path": display_path(input_image, source_root),
            "sha256": file_sha256(input_image),
        },
        "programs": {
            "img2sixel": program_record(img2sixel, source_root),
            "libsixel": {
                "path": display_path(library_path, source_root),
                "sha256": file_sha256(library_path),
            },
        },
        "protocol": {
            "purpose": (
                "compare geometric palette reachability with closed-loop "
                "FS reachability after final merge"
            ),
            "colors": COLORS,
            "quantizer": "heckbert:profile=compat",
            "merge_configurations": MERGE_NAMES,
            "palette_build_diffusion": "none",
            "empirical_diffusion": "fs",
            "empirical_scan": "raster",
            "lookup_policy": "none",
            "gpu_policy": "off",
            "threads": 1,
            "precision": "8bit",
            "clustering_colorspace": "oklab",
            "working_colorspace": "gamma",
            "output_colorspace": "gamma",
            "cover_policy": "off",
            "snap_policy": "none",
            "lab_cell_width": LAB_CELL_WIDTH,
            "occupied_lab_cells": probe_count,
            "probe_representative": "pixel-mass mean RGB rounded to uint8",
            "patch_size": PATCH_SIZE,
            "patch_sizes": PATCH_SIZES,
            "stuck_ratio_threshold": STUCK_RATIO,
            "ambiguous_ratio_band": [AMBIGUOUS_LOW, AMBIGUOUS_HIGH],
            "ratio_classifier_status": (
                "experimental; ambiguity is measured and must be small before "
                "treating the threshold as a stable reachability metric"
            ),
            "ratio_classified_unreachable_definition": (
                "outside the linear-light palette hull, or inside the hull "
                "with residual divided by nearest-entry distance at least 0.5"
            ),
            "delta_e00_thresholds": DE_THRESHOLDS,
            "spatial_mean": (
                "arithmetic mean of emitted palette entries in linear-light "
                "sRGB, then CIELAB D65"
            ),
            "geometric_space": "linear-light sRGB",
            "geometric_domain": (
                "equal-volume occupied 1-unit CIELAB cells of this image; "
                "not the continuous RGB-cube volume"
            ),
            "sensitivity_sample": (
                "union of 256 Lab-lexicographic evenly spaced cells and 256 "
                "highest-source-area cells"
            ),
            "sensitivity_probe_count": sensitivity_count,
            "supporting_plane_counts": supporting_plane_counts,
            "batch_fence": (
                "one alpha-zero row and column between RGBA patches, using "
                "the production transparent-mask diffusion fence"
            ),
            "batch_fence_isolated_validation_count": fence_validation_count,
            "ciede2000_sharma_pair_1": ciede_reference,
            "commands": commands,
        },
        "chart_contract": {
            "mechanics": (
                "explain that Ward reduces Q to K while Lloyd moves K means"
            ),
            "visual_comparison": (
                "locate and show image regions whose per-pixel color error "
                "changes after final merge"
            ),
            "reachability": (
                "separate the convex-hull upper bound, experimental residual "
                "classifier, perceptual threshold, finite area, and palette "
                "spacing"
            ),
            "bimodality": (
                "test, rather than assume, residual-ratio bimodality inside "
                "each palette hull with cell and pixel weights"
            ),
            "hotspots": (
                "map ratio-classified occupied cells back to source pixels"
            ),
            "non_color_distinction": (
                "line style, marker shape, hatch, labels, and panel position"
            ),
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--img2sixel")
    parser.add_argument("--libsixel")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", default="unknown")
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument("--output-dir", type=Path, required=True)
    return parser.parse_args()


def main() -> None:
    """Run the controlled palette capture, empirical probes, and plots."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    build_dir = (args.build_dir or source_root).resolve()
    input_image = args.input
    if not input_image.is_absolute():
        input_image = (source_root / input_image).resolve()
    output_dir = args.output_dir
    if not output_dir.is_absolute():
        output_dir = (source_root / output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    library_path = resolve_libsixel(args.libsixel, build_dir)
    command_env = make_command_environment(args.clean_sixel_environment)
    source = np.asarray(Image.open(input_image).convert("RGB"), dtype=np.uint8)
    targets, mass, keys, inverse = occupied_lab_cells(source)
    ciede_reference = validate_delta_e()
    library = DitherLibrary(library_path)
    merge_by_name = {
        str(record["merge_config"]): record for record in merge_records()
    }
    palettes: Dict[str, np.ndarray] = {}
    outputs: Dict[str, np.ndarray] = {}
    commands: Dict[str, str] = {}
    empirical_by_policy: Dict[str, Dict[str, np.ndarray]] = {}
    hull_inside_by_policy: Dict[str, np.ndarray] = {}
    plane_counts: Dict[str, int] = {}
    summary_rows: List[Dict[str, object]] = []
    with tempfile.TemporaryDirectory(
            prefix="libsixel-merge-reachability-") as directory:
        temp_dir = Path(directory)
        for name in MERGE_NAMES:
            output_act = temp_dir / f"{name}.act"
            commands[name], encoded = run_palette_build(
                img2sixel,
                input_image,
                merge_by_name[name],
                output_act,
                command_env,
            )
            palettes[name] = read_act(output_act)
            outputs[name] = library.decode_direct(encoded)[..., :3]
            if outputs[name].shape != source.shape:
                raise RuntimeError(f"decoded shape changed for {name}")
            palette_linear = srgb_to_linear(
                palettes[name].astype(np.float64) / 255.0
            )
            target_linear = srgb_to_linear(
                targets.astype(np.float64) / 255.0
            )
            normals, offsets = supporting_halfspaces(palette_linear)
            plane_counts[name] = len(normals)
            hull_inside_by_policy[name] = points_inside_hull(
                target_linear, normals, offsets
            )
            empirical_by_policy[name] = empirical_probe(
                library, palettes[name], targets, PATCH_SIZE
            )
            summary_rows.append(summarize_policy(
                name,
                palettes[name],
                mass,
                hull_inside_by_policy[name],
                empirical_by_policy[name],
                commands[name],
            ))
        fence_validation_count = validate_batch_fence(
            library, palettes["none"], targets
        )
        sample_indexes = sensitivity_sample(keys, mass)
        patch_rows = measure_patch_sensitivity(
            library,
            palettes,
            targets,
            mass,
            sample_indexes,
            hull_inside_by_policy,
        )
        plot_mechanics(output_dir / "merge-policy-mechanics.png")
        plot_visual_comparison(
            output_dir / "merge-policy-visual-comparison.png",
            source,
            outputs,
        )
        plot_bimodality(
            output_dir / "merge-policy-reachability-bimodality.png",
            empirical_by_policy,
            hull_inside_by_policy,
            mass,
        )
        plot_reachability(
            output_dir / "merge-policy-reachability.png",
            summary_rows,
            patch_rows,
            empirical_by_policy,
            mass,
        )
        plot_hotspots(
            output_dir / "merge-policy-reachability-hotspots.png",
            source,
            inverse,
            empirical_by_policy,
            hull_inside_by_policy,
            mass,
        )
    write_csv(output_dir / "merge-policy-reachability-summary.csv", summary_rows)
    write_csv(
        output_dir / "merge-policy-reachability-patch-size.csv", patch_rows
    )
    write_metadata(
        output_dir / "merge-policy-reachability-run.json",
        source_root,
        build_dir,
        input_image,
        img2sixel,
        library_path,
        args.revision,
        args.source_state,
        commands,
        len(targets),
        len(sample_indexes),
        plane_counts,
        ciede_reference,
        fence_validation_count,
    )


if __name__ == "__main__":
    main()
