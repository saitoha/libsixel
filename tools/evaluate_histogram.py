#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Plot radial spectrum histograms for the reference/output pair."""

import argparse
import base64
import io
import json
import sys
from pathlib import Path
from typing import Tuple

import matplotlib.pyplot as plt
import numpy as np

from evaluate import load_rgb, to_luma709


def radial_histogram(img_y: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """Compute normalized radial power histogram for a luma image."""
    # ASCII map of the data flow used in this helper:
    #
    #   +------------+   FFT   +-----------+   Polar bins
    #   |  img_y     | -----> | spectrum  | -----------> histogram
    #   +------------+         +-----------+             (radius)
    centered = img_y - np.mean(img_y)
    spectrum = np.fft.fftshift(np.fft.fft2(centered))
    power = np.abs(spectrum) ** 2
    height, width = img_y.shape
    cy, cx = height // 2, width // 2
    yy, xx = np.mgrid[0:height, 0:width]
    radius = np.sqrt((yy - cy) ** 2 + (xx - cx) ** 2)
    rmax = np.max(radius)
    bins = 256
    hist, edges = np.histogram(
        (radius / (rmax + 1e-9)).ravel(), bins=bins, weights=power.ravel()
    )
    hist = hist / (np.max(hist) + 1e-12)
    centers = 0.5 * (edges[:-1] + edges[1:])
    return centers, hist


def plot_spectral_histogram(y_ref: np.ndarray, y_out: np.ndarray) -> bytes:
    """Plot two normalized histograms to visualize radial spectra."""
    ref_x, ref_hist = radial_histogram(y_ref)
    out_x, out_hist = radial_histogram(y_out)
    plt.figure(figsize=(7, 4.5))
    plt.plot(ref_x, ref_hist, label="ref")
    plt.plot(out_x, out_hist, label="out")
    plt.xlabel("Normalized Frequency (0=low, 1=Nyquist)")
    plt.ylabel("Normalized Power")
    plt.title("Radial Power Spectrum")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    buffer = io.BytesIO()
    plt.savefig(buffer, format="png")
    plt.close()
    return buffer.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Generate the radial spectrum PNG used by the report pipeline."
        )
    )
    parser.add_argument("--ref", required=True, help="Reference image path")
    parser.add_argument("--out", default="-", help="Output image path")
    parser.add_argument(
        "--output",
        help=(
            "Optional PNG filepath; when omitted the PNG is only emitted as"
            " base64 on stdout."
        ),
    )
    args = parser.parse_args()

    ref_rgb = load_rgb(args.ref)
    out_rgb = load_rgb(args.out)
    height = min(ref_rgb.shape[0], out_rgb.shape[0])
    width = min(ref_rgb.shape[1], out_rgb.shape[1])
    ref_rgb = ref_rgb[:height, :width, :]
    out_rgb = out_rgb[:height, :width, :]
    ref_y = to_luma709(ref_rgb)
    out_y = to_luma709(out_rgb)

    png_bytes = plot_spectral_histogram(ref_y, out_y)

    if args.output:
        output_path = Path(args.output)
        output_path.write_bytes(png_bytes)
        print("Wrote:", output_path, file=sys.stderr)

    payload = {
        "type": "histogram",
        "histogram_png_base64": base64.b64encode(png_bytes).decode("ascii"),
        "ref_shape": [int(height), int(width)],
    }
    json.dump(payload, sys.stdout, ensure_ascii=False)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
