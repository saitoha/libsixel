# Image-Quality Metrics

## Scope

This document explains the image-quality metrics that `lsqa` reports: where each one comes from, the quantity it computes, how the libsixel implementation differs from the published definition, and what it can and cannot observe. It then explains how the metrics are used: why multiscale structure matters, how dither noise is measured, which metrics to combine for dithered output, and why the current spatial metrics are computed on a luminance-like signal.

[Visual perception for image quality](visual-perception/README.md) explains the sensory physiology these metrics rest on: perceptual quality, photoreceptors and adaptation, contrast sensitivity, and viewing geometry. [Color difference and color reproduction](color-difference.md) compares color-difference formulas and evaluates color across scales. The [image-quality measurement policy](measurement-policy.md) owns the primary gate, thresholds, fixtures, baselines, and comparison conditions. [Color-aware quality evaluation](color-evaluation.md) owns input interpretation, the XYZ D50 connection space, the comparison-signal options, and regional tiling. This page defines and explains; it does not set acceptance limits.

## Reading the definitions

`lsqa` compares a reference image `x` with a test image `y` over their common top-left rectangle of `W × H` pixels. Two kinds of signal are used:

- **Spatial and luma metrics** (MS-SSIM, GMSD, PSNR-Y, HighFreqRatio, StripeScore, BandingIndex, and the luma ClipRate) read a single-channel comparison signal. By default it is encoded sRGB luma `Y' = 0.2126 R' + 0.7152 G' + 0.0722 B'` with the ITU-R BT.709 coefficients on gamma-encoded samples in the nominal range 0 to 1. `--compare-colorspace` can select linear `Y` instead; see [physical color and comparison signal](color-evaluation.md#physical-color-and-comparison-signal).
- **Color metrics** (`Δ E00_mean`, `DE00_max`, `Δ Chroma_mean`, and the regional ΔE00 values) read CIELAB computed from physical XYZ with the ICC D50 white. They do not depend on the comparison-signal options.

`Y'` is *luma*, a weighted sum of gamma-encoded components. It is not CIE luminance `Y`, and with saturated colors it departs from the lightness that a viewer perceives (the non-constant-luminance error of video luma). It is nevertheless roughly perceptually uniform in the way that encoded sRGB is, which is why it remains the default.

All of these are *full-reference* metrics: they compare an output with its reference, and what makes them perceptual is discussed in [perceptual quality](visual-perception/README.md#perceptual-quality).

Formulas use plain-text notation: `Σ` is a sum, `Π` a product, `E[·]` an average over pixels, `*` convolution, and `|·|` an absolute value or magnitude. Unless stated otherwise, "higher is better" applies to similarity scores and "lower is better" to differences.

| Metric | Signal | Better | Family |
| --- | --- | --- | --- |
| `MS-SSIM` | luma | higher, 1 = identical | reference five-scale MS-SSIM product |
| `GMSD` | luma | lower, 0 = identical | gradient similarity |
| `PSNR_Y` | luma | higher, 99 = identical | error power |
| `Δ E00_mean`, `DE00_max` | CIELAB | lower | per-pixel color difference |
| `DE00_region_mean`, `DE00_region_max` | CIELAB of tile means | lower | low-pass color difference |
| `Δ Chroma_mean` | CIELAB | lower | chroma magnitude |
| `HighFreqRatio_*` | luma | descriptive | spectral distribution |
| `StripeScore_*` | luma | descriptive | spectral orientation |
| `BandingIndex_*` | luma | lower `_rel` | contouring heuristic |
| `ClipRate_*` | luma, R, G, B | descriptive | range saturation |

## Error-power metric: PSNR-Y

### Origin

Mean squared error and the peak signal-to-noise ratio derived from it come from signal-processing and communication engineering, where an error is treated as additive noise whose power can be measured. They became the default image-coding benchmark because they are simple, differentiable, and comparable across systems, not because they model vision. Their weak correlation with perceived quality was the motivation for the structural metrics below.

### Definition

```text
MSE    = E[(x − y)²]                     over the luma signal
PSNR_Y = 10 · log10(1 / MSE)  dB         peak value 1
PSNR_Y = 99 dB                           when MSE ≤ 10⁻¹²
```

### What it measures

PSNR-Y measures the power of the luma error and nothing about where it lies. Every pixel and every spatial frequency counts equally. A fine checkerboard dither and a smooth low-frequency tone shift of the same error power receive the same score, although a viewer at normal distance sees only the second. For dithered output, PSNR-Y therefore mostly reports how much noise power the ditherer added, which error diffusion deliberately increases in exchange for better low-frequency fidelity. Use it as a luma error view, not as a perceptual ranking of dithers.

## Structural metrics: SSIM and MS-SSIM

### Origin

Wang, Bovik, Sheikh, and Simoncelli introduced SSIM in "Image quality assessment: from error visibility to structural similarity" (IEEE Transactions on Image Processing 13(4), 2004). Its premise is that the visual system is adapted to extract structure, so a metric should compare local structure rather than add up error visibility. Wang, Simoncelli, and Bovik extended it across scales in "Multiscale structural similarity for image quality assessment" (37th Asilomar Conference on Signals, Systems and Computers, 2003), with scale weights fitted to a psychophysical experiment in which observers matched distortions across scales.

### Single-scale SSIM

At each pixel, local statistics are taken under a normalized Gaussian window `g`:

```text
μx = g * x            σx² = g * x² − μx²
μy = g * y            σy² = g * y² − μy²
                      σxy = g * (x·y) − μx·μy
```

SSIM combines three comparisons:

```text
l(x, y) = (2 μx μy + C1) / (μx² + μy² + C1)      luminance
c(x, y) = (2 σx σy + C2) / (σx² + σy² + C2)      contrast
s(x, y) = (σxy + C3) / (σx σy + C3)              structure

with C3 = C2 / 2 the product simplifies to

SSIM(x, y) = (2 μx μy + C1)(2 σxy + C2) / ((μx² + μy² + C1)(σx² + σy² + C2))
C1 = (K1 L)²,  C2 = (K2 L)²,  K1 = 0.01,  K2 = 0.03,  L = dynamic range
```

The mean of the SSIM map over the image is the single-scale score.

### Published MS-SSIM

MS-SSIM evaluates contrast and structure at `M = 5` dyadic scales. Scale 1 is the original; each further scale low-pass filters and subsamples by two. Luminance is compared only at the coarsest scale, and the terms are combined as a weighted geometric product:

```text
MS-SSIM = l_M^{α_M} · Π_{j=1..M} c_j^{β_j} · s_j^{γ_j}
α_M = w_M,  β_j = γ_j = w_j
(w_1 .. w_5) = (0.0448, 0.2856, 0.3001, 0.2363, 0.1333)
```

The weights peak at the middle scales. That is the fitted psychophysical result: with the experiment's viewing geometry, distortions near the middle scales were the most visible, consistent with the band-pass shape of the luminance contrast sensitivity function described [in visual perception](visual-perception/README.md#contrast-sensitivity).

### Reference MS-SSIM

Select the reference metric with `lsqa -m ms-ssim reference.png output.six` and set a baseline with `-b MS-SSIM:0.98`. Names are case-insensitive. `MS_SSIM`, `SSIM` and the established prefixes, including `-mm`, select the same reference score. The JSON key is `MS-SSIM`.

The implementation follows the default five-scale product in the authors' [MATLAB reference code](https://ece.uwaterloo.ca/~z70wang/research/iwssim/msssim.zip), distributed from their [reference implementation page](https://ece.uwaterloo.ca/~z70wang/research/iwssim/). It uses an 11 × 11 normalized Gaussian window with sigma 1.5, `K1 = 0.01`, `K2 = 0.03` and unit signal range. At each scale, only window positions wholly inside the image contribute to the spatial mean. The first four factors average contrast times structure (`CS`); the fifth averages the full local SSIM product, including luminance:

```text
score = mean(CS_1)^0.0448 · mean(CS_2)^0.2856
      · mean(CS_3)^0.3001 · mean(CS_4)^0.2363
      · mean(l_5 · CS_5)^0.1333
```

The rounded published weights are used as printed, without renormalizing them. Between scales, a 2 × 2 box is correlated with symmetric boundaries and subsampled at its first, third, fifth and subsequent positions. The window looks right and down; an odd final row or column is reflected onto itself. Downsampling therefore retains `ceil(width/2) × ceil(height/2)` samples. Local moments, downsampling and spatial sums use double arithmetic; the assessment result is stored as float32. Only roundoff above the final upper bound of one is capped. A negative mean factor has no real-valued fractional power and makes this metric unavailable instead of silently replacing that factor with zero.

Both aligned comparison dimensions must be at least 176 pixels, matching the reference's `11 × 2^4` admission rule. Smaller images do not acquire fewer scales, narrower windows or implicit enlargement. An unavailable result is `null` in JSON and `nan` with `-m`; the usual `-b` handling rejects it with exit code 5 even when the threshold is zero. Other selected metrics remain available. Nonfinite comparison samples also make the metric unavailable if they reach metric evaluation.

The reference formula uses the existing [comparison signal](color-evaluation.md#physical-color-and-comparison-signal), which defaults to BT.709-weighted encoded luma. Reproducing another tool's score also requires matching its input samples, RGB-to-gray conversion and preprocessing. For two constant gray images of 0 and `3/255`, all contrast/structure factors are one: the reference score is approximately `0.419448476^0.1333 = 0.890641772`, rather than the approximately `0.419448476` produced by the incorrect arithmetic-average implementation.

Historical `lsqa` results labeled `MS-SSIM` may use an incorrect weighted arithmetic average of full SSIM at every scale, reflected window borders and a shortened pyramid for small inputs. That formula is no longer implemented. Its scores and policy rankings cannot be carried over to the reference product. Record the tool revision as well as the metric name with baselines and benchmarks; the [measurement policy](measurement-policy.md#primary-and-secondary-metrics) governs test migration.

#### Numerical validation

The [independent reference generator](../../tests/data/assessment/ms_ssim_reference.py) evaluates direct two-dimensional Gaussian windows in NumPy and emits each factor and the final product. It does not call the production separable filter or `lsqa`. The C observer checks MS-SSIM alone and alongside GMSD against the same literal expectations, covering flat levels, a fine checker pattern, a textured even-size image and an odd-size image. The checker averages to a constant after the first reduction, isolating the first contrast/structure exponent; the flat case isolates the sole coarsest-scale luminance factor. Odd-size and textured observations additionally exercise valid-window pooling, symmetric edge handling, scale weights and the last spatially pooled SSIM factor.

Scores use an absolute tolerance of `1.5e-5`, not bit identity. The reference begins with ideal gray levels (binary fractions except `3/255`); the observation includes float32 storage and BT.709 luma arithmetic before double statistics. The generator bounds input perturbations by `4e-7` times each image's maximum sample, accounting for input rounding, coefficient representation, three weighted products and two sums. Variance perturbation is bounded by `2 sigma delta + delta²`, covariance perturbation by `sigma_x delta_y + sigma_y delta_x + delta_x delta_y`, and the stabilized ratios and weighted product propagate those bounds to the final score. The resulting input-error bounds are below `6e-8` for the flat case, `4.5e-6` for the checker and `1.1e-5` for both textured cases. The remaining budget covers double statistics/reduction, `exp`/`pow` and float32 result storage. This budget applies to these fixtures, not every possible anticorrelated or near-zero-factor input. Availability, metric masks and stale-result clearing are exact state assertions. See the shared [floating-point testing rules](../testing/guide.md#intermediate-floating-point-error).

### What it measures

MS-SSIM is sensitive to changes in local mean, contrast, and correlation of the luma signal, summarized over five scales. It is insensitive to chroma, because only luma is compared, and to errors that preserve local statistics. On dithered output its finest scale sees the dither pattern as lost correlation, while coarser scales see the locally averaged tone, which is closer to what a viewer perceives at a distance. See [why multiscale structure matters](#why-multiscale-structure-matters).

## Gradient metric: GMSD

### Origin

Xue, Zhang, Mou, and Bovik proposed Gradient Magnitude Similarity Deviation in "Gradient magnitude similarity deviation: a highly efficient perceptual image quality index" (IEEE Transactions on Image Processing 23(2), 2014). Its observation is that image gradients carry most structural information and that the *variation* of local quality across an image predicts perceived quality better than its mean: a distortion concentrated in some regions is judged worse than the same average damage spread evenly.

### Definition

```text
mx = |(hx * x, hy * x)|        gradient magnitude of the reference
my = |(hx * y, hy * y)|        gradient magnitude of the test image

GMS(i) = (2 mx(i) my(i) + c) / (mx(i)² + my(i)² + c)
GMSD   = standard deviation of GMS over all pixels
```

The published method downsamples both images by two with a 2 × 2 average, uses Prewitt filters (rows of `(1, 0, −1) / 3`), and uses `c = 170` for 8-bit data. `lsqa` ([`gmsd_metric`](../../assessment/assessment.c)) uses Sobel-weighted 3 × 3 filters (`(1, 2, 1) / 4` smoothing across the derivative), does not downsample, uses mirror-reflected borders, and uses `c = 0.0026` on the 0 to 1 range, which is the published constant rescaled (`170 / 255² ≈ 0.0026`). Values are therefore of the same kind as published GMSD but not identical.

### What it measures

GMSD measures how unevenly edges and gradients are preserved. It is useful for detecting localized structural damage such as broken edges, halos, and seams, and it complements MS-SSIM's averaging. It is computed at full resolution, so on dithered output the dither texture itself produces gradient disagreement; compare GMSD between dither variants under identical settings rather than reading it as an absolute quality.

## Color-difference metrics

### CIEDE2000

#### Origin

CIELAB (CIE 1976) was designed so that equal Euclidean distances would correspond to roughly equal perceived differences, but its uniformity is imperfect, especially in blues, near neutrals, and at high chroma. CIEDE2000 was developed by CIE Technical Committee 1-47 and published by Luo, Cui, and Rigg as "The development of the CIE 2000 colour-difference formula: CIEDE2000" (Color Research and Application 26(5), 2001). It corrects CIELAB with weighting functions fitted to several visual datasets of small color differences. Sharma, Wu, and Dalal published implementation notes and verification data in Color Research and Application 30(1), 2005; `lsqa` is tested against those pairs.

#### Definition

```text
C*ab = √(a*² + b*²)   per sample;   C̄ = (C*ab,1 + C*ab,2) / 2
G    = 0.5 · (1 − √(C̄⁷ / (C̄⁷ + 25⁷)))
a'   = (1 + G) · a*;      C' = √(a'² + b*²);      h' = atan2(b*, a') in degrees

ΔL' = L*_2 − L*_1
ΔC' = C'_2 − C'_1
ΔH' = 2 √(C'_1 C'_2) · sin(Δh' / 2)          Δh' = hue difference wrapped to ±180°

L̄', C̄', h̄' = means of the two samples (h̄' with the 180° wrap rule)
T    = 1 − 0.17 cos(h̄' − 30°) + 0.24 cos(2h̄') + 0.32 cos(3h̄' + 6°) − 0.20 cos(4h̄' − 63°)
S_L  = 1 + 0.015 (L̄' − 50)² / √(20 + (L̄' − 50)²)
S_C  = 1 + 0.045 C̄'
S_H  = 1 + 0.015 C̄' T
R_T  = −2 √(C̄'⁷ / (C̄'⁷ + 25⁷)) · sin(60° · exp(−((h̄' − 275°) / 25°)²))

ΔE00 = √( (ΔL'/(k_L S_L))² + (ΔC'/(k_C S_C))² + (ΔH'/(k_H S_H))²
          + R_T (ΔC'/(k_C S_C)) (ΔH'/(k_H S_H)) )
```

`lsqa` uses `k_L = k_C = k_H = 1`, the D50 white `(0.9642, 1, 0.8249)`, and the zero-chroma hue convention described in [color-aware evaluation](color-evaluation.md#physical-color-and-comparison-signal). It reports the arithmetic mean over pixels (`Δ E00_mean`), the maximum (`DE00_max`), and, in the full JSON report, the location of the maximum and the counts above fixed levels.

#### What it measures

ΔE00 measures the visible difference between two uniform color patches seen side by side. [Color difference and color reproduction](color-difference.md) compares it with other formulas and discusses tolerance grades. A value around 1 is often used as a rough just-noticeable difference for such patches. CIELAB compression discounts chroma and hue differences in dark colors in proportion to lightness, but neither CIELAB nor CIEDE2000 models absolute luminance or display flare; see [dark colors and luminance level](color-difference.md#dark-colors-and-luminance-level). It was not fitted to spatially varying content: applied pixel by pixel to a dithered image it measures the color amplitude of the dither pattern, which the eye largely averages away at normal viewing distance. The per-pixel mean is therefore an indicator of dither noise energy in color terms, the maximum and tail counts expose sparse faults, and neither alone is a perceptual quality score for dithered images.

### Regional ΔE00

`DE00_region_mean` and `DE00_region_max` first average physical XYZ within fixed square tiles (8, 16, 32, or 64 pixels, default 16), convert each tile mean to CIELAB, and compute ΔE00 between corresponding tiles. The mean is area-weighted over tiles, including partial right and bottom tiles.

```text
X̄_t = mean of XYZ in tile t         (the same for the test image)
DE00_region_mean = Σ_t area_t · ΔE00(Lab(X̄_t^x), Lab(X̄_t^y)) / Σ_t area_t
DE00_region_max  = max_t ΔE00(...)
```

Averaging in XYZ, which is linear in light, models the optical integration of a dither pattern whose elements are too small to resolve. The tile average is a crude box low-pass filter; it is a deliberately simple relative of spatial color metrics such as S-CIELAB (Zhang and Wandell, "A spatial extension of CIELAB for digital color image reproduction", SID 1996), which filters opponent color channels with contrast-sensitivity kernels before computing a color difference. Regional ΔE00 asks whether the dithered image reproduces the right color *on average over an area*, which is the primary job of a ditherer.

### Mean chroma difference

```text
Δ Chroma_mean = E[ |C*ab(y) − C*ab(x)| ]
```

It isolates gain or loss of colorfulness, independent of hue and lightness. A palette that lacks saturated entries, or a lookup that drifts toward neutral, raises it even when ΔE00 changes little.

## Spectral and texture descriptors

These are libsixel diagnostics without a single literature origin. They describe the luma signal's spectrum or tone structure; most are reported for the reference, the output, and their difference (`_rel` or `_delta` is output minus reference), so an image's own content can be separated from what processing added.

### HighFreqRatio

```text
s = luma − mean(luma), zero-padded to power-of-two dimensions P_w × P_h
P(u, v) = |FFT2(s)|²               centered spectrum
ρ(u, v) = r(u, v) / (½ √(P_w² + P_h²))      r = distance from the spectrum center in bins
HighFreqRatio = Σ_{ρ ≥ 0.25} P / Σ P
```

The normalization makes `ρ = 1` at the spectrum corners. For a square image, `ρ = 0.25` corresponds to about 0.18 cycles per pixel along an axis, so the ratio is the fraction of non-DC luma energy above roughly a third of the Nyquist frequency. `HighFreqRatio_delta` greater than zero means that processing added high-frequency energy (dither noise, ringing, aliasing); less than zero means it removed detail (blurring, posterization). Zero padding introduces edge discontinuities, so compare values between variants of the same image rather than across images.

### StripeScore

```text
same centered spectrum P; discard bins with r < 0.05 · max(W, H)
fold orientation θ = atan2(v, u) into [0°, 180°) and accumulate power into 180 one-degree bins
StripeScore = max_bin / mean_bin
```

An isotropic spectrum gives a score near 1; energy concentrated at one orientation, such as stripes, scan-line artifacts, a regular ordered-dither lattice, or directional error-diffusion "worms", raises it. `StripeScore_rel` isolates what processing added.

### BandingIndex

`BandingIndex_rel` is a run-length measure. Each row of luma is quantized to 32 levels; the mean run length of equal levels in the row, divided by the image width, is averaged over rows, and the reference value is subtracted. Positive values mean longer flat runs than the reference, the signature of contouring in a gradient.

`BandingIndex_grad_rel` is a gradient-histogram heuristic. Luma is blurred with a 7-tap Gaussian (`σ = 1`), and the gradient magnitude is taken by central differences and clipped at its 99.5th percentile `g99`. A 128-bin histogram is formed, and an exponential envelope is fitted by least squares to the logarithm of its upper half. `R` is the fraction of histogram mass above the envelope, the excess of isolated strong steps. `Z` is the fraction of pixels whose gradient is at most `0.01 · g99`, the flat areas. The index is `0.6 R + 0.4 Z`, reported as output minus reference. The weights are empirical; read the value as a contouring indicator for smooth content, not as a calibrated perceptual scale.

### ClipRate

```text
ClipRate_c = fraction of pixels with c ≤ 10⁻⁶ or c ≥ 1 − 10⁻⁶,    c ∈ {luma, R, G, B}
```

It reports saturation at the ends of the comparison signal's range. It describes the signal being compared; it cannot show how much an earlier loader or encoder stage clipped.

## Why multiscale structure matters

A metric evaluated at one scale implicitly assumes one viewing geometry. Because the [contrast sensitivity function](visual-perception/README.md#contrast-sensitivity) is defined in cycles per degree ([from pixels to cycles per degree](visual-perception/README.md#from-pixels-to-cycles-per-degree)), changing the distance or the display pitch moves every image frequency along the CSF, so a pattern that is invisible at one distance is obvious at another. Neither libsixel nor its user can know the final geometry in general.

A multiscale metric hedges against this uncertainty. Each dyadic scale corresponds to a band of image frequencies; evaluating structure in every band and weighting the bands approximates integrating visibility over the CSF across a range of plausible viewing conditions, in the spirit of Campbell and Robson's spatial-frequency channels. The fitted MS-SSIM weights favor the middle scales because those carried the most visible distortion in the calibration experiment.

For dithered images the scale decomposition has a direct interpretation. A ditherer trades fine-scale fidelity, where it places quantization noise, for coarse-scale fidelity, where the locally averaged tone should match the reference. Single-scale SSIM at full resolution sees mostly the noise and can rank a dither below a posterized image whose flat areas happen to correlate locally. MS-SSIM's coarser scales see through the noise to the reproduced tone and structure, and its finest scale still registers the noise, so the score reflects the trade the ditherer actually makes.

## Measuring dither noise

[Dithering](../concepts/dithering/README.md) explains why dither error should be pushed toward fine scales, how the main methods distribute it, and how structure-preserving dithering trades noise placement for edges. The metrics below observe those properties.

`HighFreqRatio_delta` shows how much luma energy processing moved above its cutoff, so a well-shaped dither raises it while posterization lowers it. `StripeScore_rel` shows whether the added energy is oriented or periodic. `BandingIndex_*` shows low-frequency contouring that should have been broken up. Regional ΔE00 checks the complementary property: that the low-frequency content, the part the eye integrates, is correct. A dither can score well on noise placement and still be wrong on average color; both views are needed.

These scalar descriptors summarize the spectrum. The [dithering reference](../encoder/palette-application/dithering/README.md#measured-spatial-error-spectra) shows the full radially averaged and two-dimensional error spectra of every libsixel dither method on controlled flat fields.

Structure-preserving dithering is observed from both sides: `MS-SSIM` and `GMSD` on luma reward preserved luminance structure, regional ΔE00 and `Δ Chroma_mean` guard the integrated color, and `StripeScore_rel` shows when structure-following produces oriented texture.

## Combining metrics for dithered output

No single metric captures dither quality, because a ditherer intentionally adds error to reduce visible error. Combine metrics that observe different properties, chosen before the experiment, under the comparison conditions owned by the [measurement policy](measurement-policy.md#controlled-comparisons):

| Property | What it means | Metrics |
| --- | --- | --- |
| Integrated tone and color | After the eye's low-pass filtering, does each area have the right color? | `DE00_region_mean`, `DE00_region_max`; coarse-scale behavior of `MS-SSIM` |
| Multiscale structure | Are edges and textures preserved across viewing scales, especially in luminance? | `MS-SSIM`, `GMSD` |
| Noise placement | Is the added error high-frequency, isotropic, and aperiodic? | `HighFreqRatio_delta`, `StripeScore_rel` |
| Contouring | Are smooth gradients free of bands? | `BandingIndex_rel`, `BandingIndex_grad_rel` |
| Colorfulness | Is chroma preserved within the palette's limits? | `Δ Chroma_mean` |
| Sparse faults | Are there isolated wrong pixels, such as black or white specks? | `DE00_max` and its location and tail counts |
| Noise energy | How much raw error was added? | `Δ E00_mean`, `PSNR_Y` (diagnostic only for dithers) |

Read the rows together. A dither that lowers regional ΔE00 and banding while raising `HighFreqRatio_delta` and per-pixel ΔE00 is doing its job. A dither that raises regional ΔE00 or `StripeScore_rel` is adding visible error, even if PSNR-Y improves. A fixed palette bounds what any ditherer can achieve, since local averages cannot leave the palette's convex hull, so compare dithers on the same palette when the dither is the variable. Visual inspection at more than one magnification remains necessary when the metrics disagree, because the descriptors are heuristics and the viewing geometry is unknown.

## Why luminance is currently emphasized

The spatial metrics in `lsqa` operate on a single luma signal, while color is measured separately with per-pixel and regional ΔE00. There are physiological and practical reasons for this split, and limits to it.

**Physiology.** The achromatic channel has the highest spatial resolution and contrast sensitivity, and structure, edges, and texture are perceived mainly through luminance contrast; see [luminance and chroma in spatial vision](visual-perception/README.md#luminance-and-chroma-in-spatial-vision). A metric that spends its spatial analysis on luminance therefore concentrates on the channel where spatial errors are most visible. The same reasoning motivates [structure-preserving dithering that prioritizes luminance](../concepts/dithering/README.md#structure-preserving-dithering-and-luminance-priority).

**Lineage and validation.** SSIM, MS-SSIM, and GMSD were defined and validated on grayscale or luma images. Using them on luma keeps `lsqa` values interpretable against their published behavior, apart from the implementation differences documented above. Extending them to color has no single accepted definition, and an ad hoc per-channel average would weight RGB channels as though they were perceptually equivalent, which they are not.

**Separation of concerns.** Keeping spatial structure on luma and color fidelity on CIELAB lets a report say *which* property changed. A palette or lookup change that shifts hue without moving edges appears in ΔE00 and `Δ Chroma_mean` but not in MS-SSIM; a dither change that moves noise between scales appears in MS-SSIM and the spectral descriptors.

**Limits.** This split under-represents chromatic spatial artifacts at low frequencies, which are visible: color banding in gradients, chroma noise in large flat areas, and slow hue or neutral drift caused by palette geometry, such as gray areas drifting toward a tint. The luma metrics are blind to these. Regional ΔE00 and `Δ Chroma_mean` are the current safeguards, which is why the [measurement policy](measurement-policy.md#primary-and-secondary-metrics) prefers per-pixel color summaries for palette studies. Luma itself is gamma-encoded and non-constant-luminance, so saturated colors are weighted imperfectly; linear `Y` is available as a comparison signal when that matters. A spatial color metric in the S-CIELAB family, filtering opponent channels by their own contrast sensitivity before computing color difference, would close this gap more directly; it is not implemented.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

This inventory covers reference MS-SSIM and its coexistence with the other metrics. It does not claim to independently validate every other metric described in this document; color-coordinate coverage is owned by [color-aware evaluation](color-evaluation.md#test-coverage).

| ID | Contract | Owning test |
| --- | --- | --- |
| MO-01 | Identical textured inputs score one at the minimum admitted size. | [tests/processing/assessment/0017_ms_ssim_identity.t](../../tests/processing/assessment/0017_ms_ssim_identity.t) |
| MO-02 | A flat mean-level shift contributes luminance only at the coarsest scale. | [tests/processing/assessment/0018_ms_ssim_coarsest_luminance.t](../../tests/processing/assessment/0018_ms_ssim_coarsest_luminance.t) |
| MO-03 | A fine checker contributes through the first CS exponent and geometric product. | [tests/processing/assessment/0019_ms_ssim_geometric_product.t](../../tests/processing/assessment/0019_ms_ssim_geometric_product.t) |
| MO-04 | An even-sized textured input matches independently pooled reference factors. | [tests/processing/assessment/0020_ms_ssim_even_reference.t](../../tests/processing/assessment/0020_ms_ssim_even_reference.t) |
| MO-05 | Odd-sized reduction and valid windows match the independent reference. | [tests/processing/assessment/0021_ms_ssim_odd_reference.t](../../tests/processing/assessment/0021_ms_ssim_odd_reference.t) |
| MO-06 | Swapping reference and output preserves the reference score. | [tests/processing/assessment/0022_ms_ssim_symmetry.t](../../tests/processing/assessment/0022_ms_ssim_symmetry.t) |
| MO-07 | Inputs narrower than 176 pixels have no MS-SSIM result. | [tests/processing/assessment/0023_ms_ssim_undersized.t](../../tests/processing/assessment/0023_ms_ssim_undersized.t) |
| MO-08 | A negative mean factor has no real-valued MS-SSIM result. | [tests/processing/assessment/0024_ms_ssim_negative_factor.t](../../tests/processing/assessment/0024_ms_ssim_negative_factor.t) |
| MO-09 | An unavailable comparison clears a preceding valid score in either narrow orientation. | [tests/processing/assessment/0025_ms_ssim_reuse.t](../../tests/processing/assessment/0025_ms_ssim_reuse.t) |
| MO-10 | CLI selection is case-insensitive and accepts the matching MS-SSIM baseline. | [tests/quality_gate/ms_ssim/0091_reference_selection.t](../../tests/quality_gate/ms_ssim/0091_reference_selection.t) |
| MO-11 | The established -mm prefix selects the reference score. | [tests/quality_gate/ms_ssim/0092_reference_prefix.t](../../tests/quality_gate/ms_ssim/0092_reference_prefix.t) |
| MO-12 | An unavailable MS-SSIM result prints nan and fails even a zero baseline. | [tests/quality_gate/ms_ssim/0093_unavailable_baseline.t](../../tests/quality_gate/ms_ssim/0093_unavailable_baseline.t) |
| MO-13 | The full JSON report includes an unavailable MS-SSIM as null. | [tests/quality_gate/ms_ssim/0094_json_unavailable.t](../../tests/quality_gate/ms_ssim/0094_json_unavailable.t) |
| MO-14 | A finite MS-SSIM score below its baseline exits with quality failure. | [tests/quality_gate/ms_ssim/0095_finite_baseline.t](../../tests/quality_gate/ms_ssim/0095_finite_baseline.t) |
| MO-15 | Help describes the reference formula and minimum dimensions. | [tests/quality_gate/ms_ssim/0096_reference_help.t](../../tests/quality_gate/ms_ssim/0096_reference_help.t) |

These are value, state and CLI-contract assertions. They do not establish an encoder quality floor or transfer an old threshold to the new metric.

## References

- Campbell, F. W., and J. G. Robson. "Application of Fourier analysis to the visibility of gratings." Journal of Physiology 197, 1968.
- ITU-R Recommendation BT.709. Parameter values for the HDTV standards for production and international programme exchange.
- Luo, M. R., G. Cui, and B. Rigg. "The development of the CIE 2000 colour-difference formula: CIEDE2000." Color Research and Application 26(5), 2001.
- Sharma, G., W. Wu, and E. N. Dalal. "The CIEDE2000 color-difference formula: implementation notes, supplementary test data, and mathematical observations." Color Research and Application 30(1), 2005.
- Wang, Z., A. C. Bovik, H. R. Sheikh, and E. P. Simoncelli. "Image quality assessment: from error visibility to structural similarity." IEEE Transactions on Image Processing 13(4), 2004.
- Wang, Z., E. P. Simoncelli, and A. C. Bovik. "Multiscale structural similarity for image quality assessment." 37th Asilomar Conference on Signals, Systems and Computers, 2003.
- Xue, W., L. Zhang, X. Mou, and A. C. Bovik. "Gradient magnitude similarity deviation: a highly efficient perceptual image quality index." IEEE Transactions on Image Processing 23(2), 2014.
- Zhang, X., and B. A. Wandell. "A spatial extension of CIELAB for digital color image reproduction." SID International Symposium Digest of Technical Papers, 1996.
