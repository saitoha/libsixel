#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Convert metric JSON into score JSON and a radar chart."""

import argparse
import base64
import io
import json
import sys
from pathlib import Path
from typing import Dict

import matplotlib.pyplot as plt
import numpy as np


def clamp01(value: float) -> float:
    """Clamp a float to the inclusive range [0, 1]."""
    return max(0.0, min(1.0, float(value)))


def score_from_metrics(metrics: Dict[str, float]) -> Dict[str, float]:
    """Convert raw metric dict to 0..100 scores."""
    # ASCII flow summary:
    #
    #   metrics.json --> heuristics (clamp/curves) --> scores.json
    #                      |                                 |
    #                      +-------- radar chart ------------+
    scores: Dict[str, float] = {}
    ms = metrics.get("MS-SSIM", 0.0)
    scores["MS-SSIM"] = 100.0 * ((clamp01((ms - 0.6) / 0.4)) ** 1.5)
    hfr = abs(metrics.get("HighFreqRatio_delta", 0.0))
    scores["Grain"] = 100.0 * ((1.0 - clamp01(hfr / 0.2)) ** 0.5)
    stripe_rel = metrics.get("StripeScore_rel", 0.0)
    scores["Stripe"] = 100.0 * ((1.0 - clamp01(stripe_rel / 0.5)) ** 1.0)
    band_rel = metrics.get("BandingIndex_rel", 0.0)
    scores["Banding(runlen)"] = 100.0 * ((1.0 - clamp01(band_rel / 0.1)) ** 1.0)
    band_grad = metrics.get("BandingIndex_grad_rel", 0.0)
    scores["Banding(grad)"] = 100.0 * ((1.0 - clamp01(band_grad / 0.1)) ** 1.0)
    clip_l = abs(metrics.get("ClipRate_L_rel", 0.0))
    clip_r = abs(metrics.get("ClipRate_R_rel", 0.0))
    clip_g = abs(metrics.get("ClipRate_G_rel", 0.0))
    clip_b = abs(metrics.get("ClipRate_B_rel", 0.0))
    worst_clip = max(clip_l, clip_r, clip_g, clip_b)
    scores["Clipping"] = 100.0 * ((1.0 - clamp01(worst_clip / 0.10)) ** 0.5)
    delta_chroma = metrics.get("Δ Chroma_mean", 0.0)
    scores["Δ Chroma"] = 100.0 * ((1.0 - clamp01(delta_chroma / 3.0)) ** 1.0)
    delta_e00 = metrics.get("Δ E00_mean", 0.0)
    scores["Δ E00"] = 100.0 * ((1.0 - clamp01(delta_e00 / 5.0)) ** 1.0)
    gmsd_val = metrics.get("GMSD", 0.0)
    scores["GMSD"] = 100.0 * ((1.0 - clamp01(gmsd_val / 0.10)) ** 1.0)
    psnr = metrics.get("PSNR_Y", 0.0)
    psnr_norm = clamp01((psnr - 25.0) / 20.0)
    scores["PSNR_Y"] = 100.0 * (psnr_norm**0.6)
    lpips = metrics.get("LPIPS(vgg)", float("nan"))
    if not (isinstance(lpips, float) and np.isnan(lpips)):
        scores["LPIPS(vgg)"] = 100.0 * ((1.0 - clamp01(lpips / 1.00)) ** 0.5)
    else:
        scores["LPIPS(vgg)"] = float("nan")
    valid = [v for v in scores.values() if isinstance(v, float) and not np.isnan(v)]
    scores["Overall"] = float(np.mean(valid)) if valid else float("nan")
    return scores


def plot_radar_scores(scores: Dict[str, float]) -> bytes:
    """Create a radar chart visualizing the 0..100 scores."""
    labels = [
        "MS-SSIM",
        "Grain",
        "Stripe",
        "Banding(runlen)",
        "Banding(grad)",
        "Clipping",
        "Δ Chroma",
        "Δ E00",
        "GMSD",
        "PSNR_Y",
        "LPIPS(vgg)",
    ]
    raw_values = [scores.get(k, float("nan")) for k in labels]
    sanitized = [
        0.0 if (isinstance(val, float) and np.isnan(val)) else float(val)
        for val in raw_values
    ]
    normalized = [max(0.0, min(100.0, val)) / 100.0 for val in sanitized]
    count = len(labels)
    angles = np.linspace(0, 2 * np.pi, count, endpoint=False).tolist()
    normalized += normalized[:1]
    angles += angles[:1]
    plt.figure(figsize=(6.5, 6.5))
    axis = plt.subplot(111, polar=True)
    axis.plot(angles, normalized, linewidth=2)
    axis.fill(angles, normalized, alpha=0.25)
    axis.set_xticks(angles[:-1])
    axis.set_xticklabels(labels)
    axis.set_yticks([0.25, 0.5, 0.75, 1.0])
    axis.set_yticklabels(["25", "50", "75", "100"])
    plt.title("Quality Radar (Scores 0..100; higher is better)")
    plt.tight_layout()
    buffer = io.BytesIO()
    plt.savefig(buffer, format="png")
    plt.close()
    return buffer.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Consume metric JSON from stdin and emit scores JSON plus a"
            " radar chart image."
        )
    )
    parser.add_argument(
        "--output",
        help=(
            "Optional JSON filepath; when omitted the payload is written to"
            " stdout."
        ),
    )
    parser.add_argument(
        "--radar-output",
        help=(
            "Optional PNG filepath; without this flag the radar is only"
            " included as base64 inside the JSON payload."
        ),
    )
    args = parser.parse_args()

    metrics = json.load(sys.stdin)

    scores = score_from_metrics(metrics)
    png_bytes = plot_radar_scores(scores)

    if args.radar_output:
        radar_path = Path(args.radar_output)
        radar_path.write_bytes(png_bytes)
        print("Wrote:", radar_path, file=sys.stderr)

    payload = {
        "type": "scores",
        "metrics": metrics,
        "scores": scores,
        "radar_png_base64": base64.b64encode(png_bytes).decode("ascii"),
    }

    if args.output:
        output_path = Path(args.output)
        output_path.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        print("Wrote:", output_path, file=sys.stderr)
    else:
        json.dump(payload, sys.stdout, ensure_ascii=False)
        sys.stdout.write("\n")


if __name__ == "__main__":
    main()
