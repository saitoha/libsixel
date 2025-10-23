#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
evaluate.py

Beginner-friendly image quality evaluator tailored for
quantization / error-diffusion artifacts (grain, stripes,
banding, clipping, chroma loss) with plain Python + numpy + Pillow.
Optional LPIPS if torch+lpips are available.

USAGE:
  python evaluate_sixel_quality.py --ref path/to/reference.png --out path/to/output.png
  # Optional: write CSV
  python evaluate_sixel_quality.py --ref ref.png --out out.png --csv report.csv

METRICS REPORTED:
- MS-SSIM (0..1, higher is better): multi-scale structural similarity (luma only)
- HighFreqRatio (0..1, lower is better for "grain"): proportion of high-frequency energy
- StripeScore (>=1, lower is better): spectral anisotropy/peakness (detects stripes/patterns)
- BandingIndex (relative, lower is better): average run length increase after 32-level luma quantization
- ClipRate_L/R/G/B (0..1, lower is better): saturation/clipping at 0 or 255
- Δ Chroma_mean (lower is better): average chroma loss in CIELAB
- a_var_ratio / b_var_ratio (≈1 is neutral): chroma variance out/ref per LAB axis
- (Optional) LPIPS (lower is better): learned perceptual distance if lpips is installed

NOTES:
- Everything runs on luma (Y) or RGB/LAB as noted.
- Only dependency required: Pillow, numpy
- LPIPS/FLIP are optional: install torch+lpips if you want them.
"""

import argparse
import math
import sys, io
import csv, json
from typing import Tuple, Dict

import numpy as np
from PIL import Image
import matplotlib.pyplot as plt

# ==============================
# Utility: image loading / prep
# ==============================


def load_rgb(path: str) -> np.ndarray:
    """Load image as float32 RGB in [0,1]."""
    if path == "-" or path == "/dev/stdin":
        data = io.BytesIO(sys.stdin.buffer.read())
    else:
        data = path
    im = Image.open(data).convert("RGB")
    arr = np.asarray(im, dtype=np.float32) / 255.0
    return arr


def to_luma709(rgb: np.ndarray) -> np.ndarray:
    """Rec.709 luma (gamma-corrected) Y = 0.2126 R + 0.7152 G + 0.0722 B."""
    return (0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]).astype(
        np.float32
    )


# ===================================
# SSIM / MS-SSIM (luma-only versions)
# ===================================


def gaussian_kernel1d(size: int, sigma: float) -> np.ndarray:
    """1D Gaussian kernel, normalized to sum=1."""
    ax = np.arange(size, dtype=np.float32) - (size - 1) / 2.0
    kernel = np.exp(-0.5 * (ax / sigma) ** 2)
    kernel /= np.sum(kernel)
    return kernel


def separable_conv2d(img: np.ndarray, k1d: np.ndarray) -> np.ndarray:
    """Separable convolution (reflect padding)."""
    # Convolve rows
    pad = len(k1d) // 2
    # pad along axis 1 (width)
    img_pad = np.pad(img, ((0, 0), (pad, pad)), mode="reflect")
    tmp = np.zeros_like(img, dtype=np.float32)
    for dx in range(len(k1d)):
        tmp += k1d[dx] * img_pad[:, dx : dx + img.shape[1]]
    # Convolve cols
    img_pad = np.pad(tmp, ((pad, pad), (0, 0)), mode="reflect")
    out = np.zeros_like(img, dtype=np.float32)
    for dy in range(len(k1d)):
        out += k1d[dy] * img_pad[dy : dy + img.shape[0], :]
    return out


def ssim_luma(
    x: np.ndarray, y: np.ndarray, K1=0.01, K2=0.03, win_size=11, sigma=1.5
) -> float:
    """SSIM computed on luma images x,y in [0,1]."""
    C1 = (K1 * 1.0) ** 2
    C2 = (K2 * 1.0) ** 2
    k = gaussian_kernel1d(win_size, sigma)
    mu_x = separable_conv2d(x, k)
    mu_y = separable_conv2d(y, k)
    mu_x2 = mu_x * mu_x
    mu_y2 = mu_y * mu_y
    mu_xy = mu_x * mu_y

    sigma_x2 = separable_conv2d(x * x, k) - mu_x2
    sigma_y2 = separable_conv2d(y * y, k) - mu_y2
    sigma_xy = separable_conv2d(x * y, k) - mu_xy

    numerator = (2 * mu_xy + C1) * (2 * sigma_xy + C2)
    denominator = (mu_x2 + mu_y2 + C1) * (sigma_x2 + sigma_y2 + C2)
    ssim_map = numerator / (denominator + 1e-12)
    return float(np.clip(np.mean(ssim_map), 0.0, 1.0))


def downsample2(img: np.ndarray) -> np.ndarray:
    """2x downsample via average pooling (even sizes)."""
    h, w = img.shape[:2]
    h2 = h // 2
    w2 = w // 2
    img = img[: h2 * 2, : w2 * 2]
    if img.ndim == 2:
        return 0.25 * (
            img[0::2, 0::2] + img[1::2, 0::2] + img[0::2, 1::2] + img[1::2, 1::2]
        )
    else:
        return 0.25 * (
            img[0::2, 0::2, :]
            + img[1::2, 0::2, :]
            + img[0::2, 1::2, :]
            + img[1::2, 1::2, :]
        )


def ms_ssim_luma(x: np.ndarray, y: np.ndarray, levels=5) -> float:
    """Multi-Scale SSIM on luma with default 5 levels (1/2^0 .. 1/2^4)."""
    weights = np.array([0.0448, 0.2856, 0.3001, 0.2363, 0.1333], dtype=np.float32)
    # From Wang et al. 2003
    assert levels == len(weights), "levels must match default weights length (5)."
    mssim = []
    cur_x = x.copy()
    cur_y = y.copy()
    for i in range(levels):
        s = ssim_luma(cur_x, cur_y)
        mssim.append(s)
        if i < levels - 1:
            cur_x = downsample2(cur_x)
            cur_y = downsample2(cur_y)
    mssim = np.clip(np.array(mssim), 0.0, 1.0)
    return float(np.sum(mssim * weights) / np.sum(weights))


# ==================================
# High-frequency energy & stripes
# ==================================


def high_frequency_ratio(img_y: np.ndarray, cutoff=0.25) -> float:
    """
    Ratio of high-frequency energy (> cutoff*Nyquist) to total energy in FFT.
    cutoff=0.25 -> keeps "top" 75% band as high-frequency.
    """
    h, w = img_y.shape
    # zero-mean
    z = img_y - np.mean(img_y)
    F = np.fft.fftshift(np.fft.fft2(z))
    P = np.abs(F) ** 2
    # radius grid
    yy, xx = np.mgrid[0:h, 0:w]
    cy, cx = (h // 2, w // 2)
    r = np.sqrt((yy - cy) ** 2 + (xx - cx) ** 2)
    r_norm = r / (0.5 * np.sqrt(h * h + w * w))  # relative to "diagonal Nyquist"
    mask_hi = r_norm >= cutoff
    hi = float(np.sum(P[mask_hi]))
    tot = float(np.sum(P))
    if tot == 0.0:
        return 0.0
    return hi / tot


def stripe_score(img_y: np.ndarray, bins=180) -> float:
    """
    Spectral anisotropy measure:
    Compute angular energy distribution in 2D FFT; score = max(angle_energy)/mean(angle_energy).
    >=1, lower is better (1 means isotropic). Larger means strong directional stripes/patterns.
    """
    h, w = img_y.shape
    z = img_y - np.mean(img_y)
    F = np.fft.fftshift(np.fft.fft2(z))
    P = np.abs(F) ** 2
    yy, xx = np.mgrid[0:h, 0:w]
    cy, cx = (h // 2, w // 2)
    ang = np.arctan2(yy - cy, xx - cx)  # [-pi, pi]
    ang = (ang + np.pi) % np.pi  # fold 180 degrees symmetry
    # exclude very low radius to ignore DC / low-freq blob
    r = np.sqrt((yy - cy) ** 2 + (xx - cx) ** 2)
    rmin = 0.05 * max(h, w)
    mask = r >= rmin
    ang_bins = np.linspace(0, np.pi, bins + 1, dtype=np.float32)
    hist = np.zeros(bins, dtype=np.float64)
    a = ang[mask].ravel()
    p = P[mask].ravel()
    # Bin by angle
    idx = np.clip(np.digitize(a, ang_bins) - 1, 0, bins - 1)
    np.add.at(hist, idx, p)
    m = float(np.mean(hist) + 1e-12)
    mx = float(np.max(hist))
    return mx / m


def plot_spectral_histogram(y_ref: np.ndarray, y_out: np.ndarray, out_path: str):
    def radial_hist(img_y):
        F = np.fft.fftshift(np.fft.fft2(img_y - np.mean(img_y)))
        P = np.abs(F) ** 2
        h, w = img_y.shape
        cy, cx = h // 2, w // 2
        yy, xx = np.mgrid[0:h, 0:w]
        r = np.sqrt((yy - cy) ** 2 + (xx - cx) ** 2)
        rmax = np.max(r)
        nbins = 256
        hist, edges = np.histogram(
            (r / (rmax + 1e-9)).ravel(), bins=nbins, weights=P.ravel()
        )
        hist = hist / (np.max(hist) + 1e-12)
        centers = 0.5 * (edges[:-1] + edges[1:])
        return centers, hist

    x1, h1 = radial_hist(y_ref)
    x2, h2 = radial_hist(y_out)

    plt.figure(figsize=(7, 4.5))
    plt.plot(x1, h1, label="ref")
    plt.plot(x2, h2, label="out")
    plt.xlabel("Normalized Frequency (0=low, 1=Nyquist)")
    plt.ylabel("Normalized Power")
    plt.title("Radial Power Spectrum")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


# ===============================
# Banding index (run-length based)
# ===============================


def banding_index_runlen(img_y: np.ndarray, levels=32) -> float:
    """
    Quantize luma to `levels`, compute average run length along rows;
    higher run length indicates longer flat plateaus (banding).
    Returns average run length normalized by image width.
    """
    h, w = img_y.shape
    q = np.clip((img_y * (levels - 1) + 0.5).astype(np.int32), 0, levels - 1)
    total_runs = 0
    total_segments = 0
    for row in range(h):
        line = q[row, :]
        run_len = 1
        segs = 0
        runs = 0
        for x in range(1, w):
            if line[x] == line[x - 1]:
                run_len += 1
            else:
                runs += run_len
                segs += 1
                run_len = 1
        runs += run_len
        segs += 1
        total_runs += runs / segs
        total_segments += 1
    avg_run = total_runs / total_segments
    return avg_run / max(1, w)


def banding_index_relative(ref_y: np.ndarray, out_y: np.ndarray, levels=32) -> float:
    """
    Relative banding increase = (runlen(out) - runlen(ref)).
    Positive means output is more "banded" than reference.
    """
    r_ref = banding_index_runlen(ref_y, levels=levels)
    r_out = banding_index_runlen(out_y, levels=levels)
    return r_out - r_ref


def _gaussian_blur(img: np.ndarray, sigma: float = 1.0, ksize: int = 7) -> np.ndarray:
    k = gaussian_kernel1d(ksize, sigma)
    return separable_conv2d(separable_conv2d(img, k), k)


def _finite_diff(img: np.ndarray):
    ip = np.pad(img, ((1, 1), (1, 1)), mode="reflect")
    dx = (ip[1:-1, 2:] - ip[1:-1, :-2]) * 0.5
    dy = (ip[2:, 1:-1] - ip[:-2, 1:-1]) * 0.5
    return dx, dy


def banding_index_gradient(img_y: np.ndarray) -> float:
    y = _gaussian_blur(img_y, sigma=1.0, ksize=7)
    dx, dy = _finite_diff(y)
    g = np.sqrt(dx * dx + dy * dy)
    g_flat = g.ravel()
    g99 = np.percentile(g_flat, 99.5) + 1e-9
    g = np.clip(g, 0, g99)
    bins = 128
    hist, edges = np.histogram(g, bins=bins, range=(0, g99))
    hist = hist.astype(np.float64) + 1e-12
    centers = 0.5 * (edges[:-1] + edges[1:])
    half = bins // 2
    x_tail = centers[half:]
    y_tail = hist[half:]
    logy = np.log(y_tail)
    A = np.vstack([x_tail, np.ones_like(x_tail)]).T
    sol, _, _, _ = np.linalg.lstsq(A, logy, rcond=None)
    b_est, c_est = -sol[0], sol[1]
    env = np.exp(c_est - b_est * centers)
    residual = np.maximum(0.0, hist - env)
    peakiness = np.sum(residual) / np.sum(hist)
    zero_thresh = 0.01 * g99
    zero_mass = float(np.mean(g_flat <= zero_thresh))
    score = 0.6 * peakiness + 0.4 * zero_mass
    return float(score)


def banding_index_gradient_relative(ref_y: np.ndarray, out_y: np.ndarray) -> float:
    return banding_index_gradient(out_y) - banding_index_gradient(ref_y)


# ==========================
# Clipping & luminance stats
# ==========================


def clipping_rates(rgb: np.ndarray, eps=1e-6) -> Dict[str, float]:
    """
    Fraction of pixels near 0 or 1 per channel and luma.
    Returns dict with keys: ClipRate_L, ClipRate_R, ClipRate_G, ClipRate_B
    """
    y = to_luma709(rgb)

    def clip_rate(ch: np.ndarray) -> float:
        lo = np.mean(ch <= eps)
        hi = np.mean(ch >= 1.0 - eps)
        return float(lo + hi)

    return {
        "ClipRate_L": clip_rate(y),
        "ClipRate_R": clip_rate(rgb[..., 0]),
        "ClipRate_G": clip_rate(rgb[..., 1]),
        "ClipRate_B": clip_rate(rgb[..., 2]),
    }


# =========================
# sRGB <-> CIELAB (D65)
# =========================

# XYZ conversion matrices for sRGB (D65)
M_RGB2XYZ = np.array(
    [
        [0.4124564, 0.3575761, 0.1804375],
        [0.2126729, 0.7151522, 0.0721750],
        [0.0193339, 0.1191920, 0.9503041],
    ],
    dtype=np.float32,
)
M_XYZ2RGB = np.linalg.inv(M_RGB2XYZ).astype(np.float32)
# D65 white
XYZ_WHITE = np.array([0.95047, 1.00000, 1.08883], dtype=np.float32)


def srgb_to_linear(c: np.ndarray) -> np.ndarray:
    a = 0.055
    return np.where(c <= 0.04045, c / 12.92, ((c + a) / (1 + a)) ** 2.4)


def linear_to_srgb(c: np.ndarray) -> np.ndarray:
    a = 0.055
    return np.where(c <= 0.0031308, 12.92 * c, (1 + a) * np.power(c, 1 / 2.4) - a)


def rgb_to_xyz(rgb: np.ndarray) -> np.ndarray:
    rlin = srgb_to_linear(rgb[..., 0])
    glin = srgb_to_linear(rgb[..., 1])
    blin = srgb_to_linear(rgb[..., 2])
    rl = rlin.reshape(-1, 1)
    gl = glin.reshape(-1, 1)
    bl = blin.reshape(-1, 1)
    X = M_RGB2XYZ[0, 0] * rl + M_RGB2XYZ[0, 1] * gl + M_RGB2XYZ[0, 2] * bl
    Y = M_RGB2XYZ[1, 0] * rl + M_RGB2XYZ[1, 1] * gl + M_RGB2XYZ[1, 2] * bl
    Z = M_RGB2XYZ[2, 0] * rl + M_RGB2XYZ[2, 1] * gl + M_RGB2XYZ[2, 2] * bl
    XYZ = np.concatenate([X, Y, Z], axis=1).reshape(rgb.shape)
    return XYZ


def f_lab(t: np.ndarray) -> np.ndarray:
    delta = 6 / 29
    return np.where(t > (delta**3), np.cbrt(t), t / (3 * delta * delta) + 4 / 29)


def xyz_to_lab(xyz: np.ndarray) -> np.ndarray:
    X = xyz[..., 0] / XYZ_WHITE[0]
    Y = xyz[..., 1] / XYZ_WHITE[1]
    Z = xyz[..., 2] / XYZ_WHITE[2]
    fx = f_lab(X)
    fy = f_lab(Y)
    fz = f_lab(Z)
    L = 116 * fy - 16
    a = 500 * (fx - fy)
    b = 200 * (fy - fz)
    return np.stack([L, a, b], axis=-1)


def rgb_to_lab(rgb: np.ndarray) -> np.ndarray:
    return xyz_to_lab(rgb_to_xyz(rgb))


def chroma_ab(lab: np.ndarray) -> np.ndarray:
    return np.sqrt(lab[..., 1] ** 2 + lab[..., 2] ** 2)


def deltaE00(lab1: np.ndarray, lab2: np.ndarray) -> np.ndarray:
    L1, a1, b1 = lab1[..., 0], lab1[..., 1], lab1[..., 2]
    L2, a2, b2 = lab2[..., 0], lab2[..., 1], lab2[..., 2]
    L_ = (L1 + L2) * 0.5
    C1 = np.sqrt(a1 * a1 + b1 * b1)
    C2 = np.sqrt(a2 * a2 + b2 * b2)
    C_ = 0.5 * (C1 + C2)
    C7 = C_**7
    G = 0.5 * (1 - np.sqrt(C7 / (C7 + 25**7 + 1e-12)))
    a1p = (1 + G) * a1
    a2p = (1 + G) * a2
    C1p = np.sqrt(a1p * a1p + b1 * b1)
    C2p = np.sqrt(a2p * a2p + b2 * b2)
    hp_fun = lambda a, b: (np.degrees(np.arctan2(b, a)) + 360) % 360
    h1p = hp_fun(a1p, b1)
    h2p = hp_fun(a2p, b2)
    dLp = L2 - L1
    dCp = C2p - C1p
    dhp = h2p - h1p
    dhp = dhp - 360 * (dhp > 180) + 360 * (dhp < -180)
    dHp = 2 * np.sqrt(C1p * C2p + 1e-12) * np.sin(np.radians(dhp) / 2.0)
    Lpm = (L1 + L2) * 0.5
    Cpm = (C1p + C2p) * 0.5
    hp_sum = h1p + h2p
    hp_diff = np.abs(h1p - h2p)
    Hpm = np.where(
        (C1p * C2p) == 0,
        h1p + h2p,
        np.where(
            hp_diff <= 180,
            hp_sum * 0.5,
            np.where(hp_sum < 360, (hp_sum + 360) * 0.5, (hp_sum - 360) * 0.5),
        ),
    )
    T = (
        1
        - 0.17 * np.cos(np.radians(Hpm - 30))
        + 0.24 * np.cos(np.radians(2 * Hpm))
        + 0.32 * np.cos(np.radians(3 * Hpm + 6))
        - 0.20 * np.cos(np.radians(4 * Hpm - 63))
    )
    Sl = 1 + (0.015 * (Lpm - 50) ** 2) / np.sqrt(20 + (Lpm - 50) ** 2)
    Sc = 1 + 0.045 * Cpm
    Sh = 1 + 0.015 * Cpm * T
    dTheta = 30 * np.exp(-(((Hpm - 275) / 25) ** 2))
    Rc = 2 * np.sqrt((Cpm**7) / (Cpm**7 + 25**7 + 1e-12))
    Rt = -Rc * np.sin(2 * np.radians(dTheta))
    kL = kC = kH = 1.0
    de = np.sqrt(
        (dLp / (kL * Sl)) ** 2
        + (dCp / (kC * Sc)) ** 2
        + (dHp / (kH * Sh)) ** 2
        + Rt * (dCp / (kC * Sc)) * (dHp / (kH * Sh))
    )
    return de


# --------------------
# GMSD (single-scale) & PSNR
# --------------------


def gmsd(ref_y: np.ndarray, out_y: np.ndarray) -> float:
    kx = np.array([[1, 0, -1], [2, 0, -2], [1, 0, -1]], dtype=np.float32) / 4.0
    ky = np.array([[1, 2, 1], [0, 0, 0], [-1, -2, -1]], dtype=np.float32) / 4.0

    def conv2(img, k):
        pad = 1
        ip = np.pad(img, ((pad, pad), (pad, pad)), mode="reflect")
        h, w = img.shape
        out = np.zeros_like(img, dtype=np.float32)
        for y in range(h):
            for x in range(w):
                out[y, x] = np.sum(ip[y : y + 3, x : x + 3] * k)
        return out

    gx1 = conv2(ref_y, kx)
    gy1 = conv2(ref_y, ky)
    gx2 = conv2(out_y, kx)
    gy2 = conv2(out_y, ky)
    gm1 = np.sqrt(gx1 * gx1 + gy1 * gy1) + 1e-12
    gm2 = np.sqrt(gx2 * gx2 + gy2 * gy2) + 1e-12
    c = 0.0026
    gms_map = (2 * gm1 * gm2 + c) / (gm1 * gm1 + gm2 * gm2 + c)
    return float(np.std(gms_map))


def psnr_luma(ref_y: np.ndarray, out_y: np.ndarray) -> float:
    mse = float(np.mean((ref_y - out_y) ** 2))
    if mse <= 1e-12:
        return 99.0
    return 10.0 * math.log10(1.0 / mse)


# ========================
# Optional LPIPS/FLIP eval
# ========================
def try_lpips(ref_rgb: np.ndarray, out_rgb: np.ndarray, net: str) -> float:
    # Suppress noisy prints & warnings from torchvision/lpips
    import warnings, contextlib, os

    try:
        import torch

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)
            warnings.filterwarnings("ignore", category=FutureWarning)
            with open(os.devnull, "w") as fnull, contextlib.redirect_stdout(
                fnull
            ), contextlib.redirect_stderr(fnull):
                import lpips  # pip install lpips

                net = lpips.LPIPS(net=net)

        # lpips expects [-1,1] normalized tensor (NCHW)
        def to_tensor(x):
            import torch as _torch

            t = _torch.from_numpy(x.transpose(2, 0, 1)).float().unsqueeze(0) * 2.0 - 1.0
            return t

        with torch.no_grad():
            d = net(to_tensor(ref_rgb), to_tensor(out_rgb))
        return float(d.item())
    except Exception as e:
        return float("nan")


# ========================
# Scoring (0..100, non-linear)
# ========================


def score_from_metrics(m: Dict[str, float]) -> Dict[str, float]:
    s = {}

    def clamp01(x):
        return float(np.clip(x, 0.0, 1.0))

    def invexp(x, k):
        return math.exp(-max(0.0, x) / max(1e-9, k))

    # MS-SSIM: emphasize high-end differences
    s["MS-SSIM"] = 100.0 * (m.get("MS-SSIM", 0.0) ** 4.0)

    # Grain: HighFreqRatio_delta (<=0 is good)
    d = m.get("HighFreqRatio_delta", 0.0)
    s["Grain"] = 100.0 if d <= 0 else 100.0 * invexp(d, 0.02)

    # Stripe: relative increase only
    ss_rel = max(0.0, m.get("StripeScore_rel", 0.0))
    s["Stripe"] = 100.0 * invexp(ss_rel, 0.5)  # 0.5 delta ≈ noticeable

    # Banding (runlen): positive is worse
    br = max(0.0, m.get("BandingIndex_rel", 0.0))
    s["Banding(runlen)"] = 100.0 * invexp(br, 0.02)

    # Banding (gradient-hist): positive is worse
    bg = max(0.0, m.get("BandingIndex_grad_rel", 0.0))
    s["Banding(grad)"] = 100.0 * invexp(bg, 0.05)

    # Clipping: worst positive delta among channels/luma
    clip_worst_rel = max(
        0.0,
        m.get("ClipRate_L_rel", 0.0),
        m.get("ClipRate_R_rel", 0.0),
        m.get("ClipRate_G_rel", 0.0),
        m.get("ClipRate_B_rel", 0.0),
    )
    s["Clipping"] = 100.0 * ((1.0 - clamp01(clip_worst_rel / 0.10)) ** 0.5)

    # Δ Chroma_mean: lower is better, noticeable ~3
    dch = m.get("Δ Chroma_mean", 0.0)
    s["Δ Chroma"] = 100.0 * ((1.0 - clamp01(dch / 3.0)) ** 1.0)

    # Δ E00_mean: lower is better, 2-5 noticeable
    de00 = m.get("Δ E00_mean", m.get("Δ E00_mean", 0.0))
    s["Δ E00"] = 100.0 * ((1.0 - clamp01(de00 / 5.0)) ** 1.0)

    # GMSD: lower is better, 0.10 ≈ poor
    gms = m.get("GMSD", 0.0)
    s["GMSD"] = 100.0 * ((1.0 - clamp01(gms / 0.10)) ** 1.0)

    # PSNR_Y: knee 25..45dB
    psnr = m.get("PSNR_Y", 0.0)
    t = clamp01((psnr - 25.0) / 20.0)  # 25->0, 45->1
    s["PSNR_Y"] = 100.0 * (t**0.6)

    # LPIPS (vgg): lower is better; 0.1 ~ quite different
    lp = m.get("LPIPS(vgg)", float("nan"))
    if not (isinstance(lp, float) and np.isnan(lp)):
        s["LPIPS(vgg)"] = 100.0 * ((1.0 - clamp01(lp / 1.00)) ** 0.5)
    else:
        s["LPIPS(vgg)"] = float("nan")

    # Overall (robust mean ignoring NaNs)
    vals = [v for v in s.values() if isinstance(v, float) and not np.isnan(v)]
    s["Overall"] = float(np.mean(vals)) if vals else float("nan")
    return s


# ========================
# Radar (scores) & HTML
# ========================


def plot_radar_scores(scores: Dict[str, float], out_path: str):
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
    values = [scores.get(k, float("nan")) for k in labels]
    values = [
        0.0 if (isinstance(v, float) and np.isnan(v)) else float(v) for v in values
    ]
    vals = [max(0.0, min(100.0, v)) / 100.0 for v in values]
    N = len(labels)
    angles = np.linspace(0, 2 * np.pi, N, endpoint=False).tolist()
    vals += vals[:1]
    angles += angles[:1]
    plt.figure(figsize=(6.5, 6.5))
    ax = plt.subplot(111, polar=True)
    ax.plot(angles, vals, linewidth=2)
    ax.fill(angles, vals, alpha=0.25)
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(labels)
    ax.set_yticks([0.25, 0.5, 0.75, 1.0])
    ax.set_yticklabels(["25", "50", "75", "100"])
    plt.title("Quality Radar (Scores 0..100; higher is better)")
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def write_html(prefix: str, metrics: Dict[str, float], scores: Dict[str, float]):
    html_path = f"{prefix}.html"

    def fmt(v):
        if isinstance(v, float):
            return "NaN" if np.isnan(v) else f"{v:.6f}"
        return str(v)

    rows_raw = "".join(
        [f"<tr><td>{k}</td><td>{fmt(v)}</td></tr>" for k, v in metrics.items()]
    )
    rows_scores = "".join(
        [f"<tr><td>{k}</td><td>{fmt(v)}</td></tr>" for k, v in scores.items()]
    )

    with open(html_path, "w", encoding="utf-8") as f:
        f.write(
            f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Image Quality Report</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{{font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif; margin:20px;}}
h1{{margin-top:0}}
table{{border-collapse:collapse; width:100%; max-width:1024px}}
td,th{{border:1px solid #ddd; padding:6px 8px; font-variant-numeric: tabular-nums;}}
th{{background:#f4f4f4; text-align:left}}
img{{max-width:100%; height:auto; display:block; margin:10px 0;}}
.flex{{display:flex; gap:24px; flex-wrap:wrap}}
.card{{flex:1 1 460px; min-width:320px;}}
</style>
</head>
<body>
<h1>Image Quality Report</h1>

<div class="flex">
  <div class="card">
    <h2>Scores (0..100)</h2>
    <table>
      <thead><tr><th>Metric</th><th>Score</th></tr></thead>
      <tbody>
        {rows_scores}
      </tbody>
    </table>
  </div>
  <div class="card">
    <h2>Raw Metrics</h2>
    <table>
      <thead><tr><th>Metric</th><th>Value</th></tr></thead>
      <tbody>
        {rows_raw}
      </tbody>
    </table>
  </div>
</div>

<h2>Charts</h2>
<h3>Radial Power Spectrum</h3>
<img src="{prefix}_spectrum.png" alt="Radial Spectrum">

<h3>Quality Radar (Scores)</h3>
<img src="{prefix}_radar_scores.png" alt="Quality Radar Scores">

<footer style="margin-top:24px; color:#666">
Generated by evaluate.py
</footer>
</body>
</html>"""
        )
    return html_path


# ========================
# Evaluate
# ========================


def evaluate_core(ref: np.ndarray, out: np.ndarray) -> Dict[str, float]:
    # Align sizes if needed
    h = min(ref.shape[0], out.shape[0])
    w = min(ref.shape[1], out.shape[1])
    ref = ref[:h, :w, :]
    out = out[:h, :w, :]

    ref_y = to_luma709(ref)
    out_y = to_luma709(out)

    # MS-SSIM
    ms = ms_ssim_luma(ref_y, out_y)

    # High frequency ratio (delta)
    hfr_out = high_frequency_ratio(out_y, cutoff=0.25)
    hfr_ref = high_frequency_ratio(ref_y, cutoff=0.25)
    hfr_delta = hfr_out - hfr_ref

    # Stripe (absolute & relative)
    stripe_ref = stripe_score(ref_y, bins=180)
    stripe_out = stripe_score(out_y, bins=180)
    stripe_rel = stripe_out - stripe_ref

    # Banding (run-length) relative
    band_rel = banding_index_relative(ref_y, out_y, levels=32)

    # Banding (gradient-hist) relative
    band_grad_rel = banding_index_gradient_relative(ref_y, out_y)

    # Clipping (absolute & relative)
    clip_ref = clipping_rates(ref)
    clip_out = clipping_rates(out)
    clip_L_rel = clip_out["ClipRate_L"] - clip_ref["ClipRate_L"]
    clip_R_rel = clip_out["ClipRate_R"] - clip_ref["ClipRate_R"]
    clip_G_rel = clip_out["ClipRate_G"] - clip_ref["ClipRate_G"]
    clip_B_rel = clip_out["ClipRate_B"] - clip_ref["ClipRate_B"]

    # Color metrics
    ref_lab = rgb_to_lab(ref)
    out_lab = rgb_to_lab(out)
    ref_ch = chroma_ab(ref_lab)
    out_ch = chroma_ab(out_lab)
    delta_chroma_mean = float(np.mean(np.abs(out_ch - ref_ch)))
    de00_mean = float(np.mean(deltaE00(ref_lab, out_lab)))

    # GMSD & PSNR
    gmsd_val = gmsd(ref_y, out_y)
    psnr_y = psnr_luma(ref_y, out_y)

    # LPIPS (vgg)
    lpips_vgg = try_lpips(ref, out, "vgg")

    # LPIPS (alex)
    # lpips_alex = try_lpips(ref, out, 'alex')

    result = {
        "MS-SSIM": ms,
        "HighFreqRatio_out": hfr_out,
        "HighFreqRatio_ref": hfr_ref,
        "HighFreqRatio_delta": hfr_delta,
        # Stripe (absolute + relative)
        "StripeScore_ref": stripe_ref,
        "StripeScore_out": stripe_out,
        "StripeScore_rel": stripe_rel,
        # Banding
        "BandingIndex_rel": band_rel,
        "BandingIndex_grad_rel": band_grad_rel,
        # Clipping (absolute for ref/out + relative deltas)
        "ClipRate_L_ref": clip_ref["ClipRate_L"],
        "ClipRate_R_ref": clip_ref["ClipRate_R"],
        "ClipRate_G_ref": clip_ref["ClipRate_G"],
        "ClipRate_B_ref": clip_ref["ClipRate_B"],
        "ClipRate_L_out": clip_out["ClipRate_L"],
        "ClipRate_R_out": clip_out["ClipRate_R"],
        "ClipRate_G_out": clip_out["ClipRate_G"],
        "ClipRate_B_out": clip_out["ClipRate_B"],
        "ClipRate_L_rel": clip_L_rel,
        "ClipRate_R_rel": clip_R_rel,
        "ClipRate_G_rel": clip_G_rel,
        "ClipRate_B_rel": clip_B_rel,
        # Color & others
        "Δ Chroma_mean": delta_chroma_mean,
        "Δ E00_mean": de00_mean,
        "GMSD": gmsd_val,
        "PSNR_Y": psnr_y,
        "LPIPS(vgg)": lpips_vgg,
    }
    return result


def main():
    ap = argparse.ArgumentParser(
        description="Evaluate image quality for ED/quantization artifacts (grain/stripes/banding/clipping/chroma loss)."
    )
    ap.add_argument("--ref", required=True, help="Reference (original) image path")
    ap.add_argument("--out", default="-", help="Output (processed) image path")
    ap.add_argument(
        "--prefix", default="report", help="Output prefix (default: report)"
    )
    ap.add_argument("--csv", default="", help="Optional path to write a CSV report")
    args = ap.parse_args()

    ref = load_rgb(args.ref)
    out = load_rgb(args.out)

    metrics = evaluate_core(ref, out)
    scores = score_from_metrics(metrics)

    # Pretty print
    print("\n=== Image Quality Report (Raw) ===")
    for k, v in metrics.items():
        if isinstance(v, float):
            print(f"{k:>24s}: {'NaN' if np.isnan(v) else f'{v:.6f}'}")
        else:
            print(f"{k:>24s}: {v}")

    print("\n=== Scores (0..100; higher is better) ===")
    for k, v in scores.items():
        if isinstance(v, float):
            print(f"{k:>24s}: {'NaN' if np.isnan(v) else f'{v:0.2f}'}")
        else:
            print(f"{k:>24s}: {v}")

    # Save JSON + optional CSV
    with open(f"{args.prefix}_metrics.json", "w", encoding="utf-8") as f:
        json.dump(metrics, f, ensure_ascii=False, indent=2)
    with open(f"{args.prefix}_scores.json", "w", encoding="utf-8") as f:
        json.dump(scores, f, ensure_ascii=False, indent=2)
    if args.csv:
        with open(args.csv, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["metric", "value"])
            for k, v in metrics.items():
                w.writerow([k, v])

    # Charts
    h = min(ref.shape[0], out.shape[0])
    w = min(ref.shape[1], out.shape[1])
    ref = ref[:h, :w, :]
    out = out[:h, :w, :]
    y_ref = to_luma709(ref)
    y_out = to_luma709(out)
    plot_spectral_histogram(y_ref, y_out, f"{args.prefix}_spectrum.png")
    plot_radar_scores(scores, f"{args.prefix}_radar_scores.png")

    # HTML
    html_path = write_html(args.prefix, metrics, scores)

    print("\nWrote:", f"{args.prefix}_metrics.json")
    print("Wrote:", f"{args.prefix}_scores.json")
    if args.csv:
        print("Wrote:", args.csv)
    print("Wrote:", f"{args.prefix}_spectrum.png")
    print("Wrote:", f"{args.prefix}_radar_scores.png")
    print("Wrote:", html_path)


if __name__ == "__main__":
    main()
