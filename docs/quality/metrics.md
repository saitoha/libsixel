# Image-Quality Metrics

## Scope

This document explains the image-quality metrics that `lsqa` reports: where each one comes from, the quantity it computes, how the libsixel implementation differs from the published definition, and what it can and cannot observe. It then explains how the metrics are used: why multiscale structure matters, how dither noise is measured, which metrics to combine for dithered output, and why the current spatial metrics are computed on a luminance-like signal.

[Visual perception for image quality](visual-perception/README.md) explains the sensory physiology these metrics rest on: perceptual quality, photoreceptors and adaptation, contrast sensitivity, and viewing geometry. [Color difference and color reproduction](color-difference.md) compares color-difference formulas and evaluates color across scales. The [image-quality measurement policy](measurement-policy.md) owns the primary gate, thresholds, fixtures, baselines, and comparison conditions. [Color-aware quality evaluation](color-evaluation.md) owns input interpretation, the XYZ D50 connection space, the comparison-signal options, and regional tiling. This page defines and explains; it does not set acceptance limits.

## Selecting metrics and gates

`-m NAME` selects a metric to print; `-b METRIC:min=VALUE` or `-b METRIC:max=VALUE` adds an inclusive lower or upper acceptance bound. Every condition names its metric, so argument order never associates an unnamed number with a previous `-m`. Both options may repeat and accept comma-separated members. For example, these invocations express the same gate:

```sh
lsqa -b MS-SSIM:min=0.98 -b "DE00_region_max:max=$color_limit" reference.png output.six
lsqa -b "MS-SSIM:min=0.98,DE00_region_max:max=$color_limit" reference.png output.six
```

`color_limit` represents a separately calibrated limit, not a universal color threshold. The [measurement policy](measurement-policy.md#primary-and-secondary-metrics) owns metric choice, calibration and degraded controls. Repeated gates are combined with AND. Failure of any bound, or an unavailable or nonfinite required result, exits with code 5; stderr describes every failed condition. Equality passes. Reporting a metric does not require gating it, and gating a metric does not add it to selected stdout. The evaluator computes the union of requested output and gate metrics from one loaded comparison pair, sharing normalization and intermediate data. Video uses these same selections and gates on its [duration-weighted means](video-evaluation.md#report-and-map-semantics).

One unique `-m` retains the numeric scalar interface (`nan` when unavailable for still images). For compatibility, an ungated scalar video request with an unavailable result exits with code 4 and no stdout. Multiple unique metrics produce a flat JSON object with canonical metric keys, in registry order, and `null` for unavailable results. Only selected fields appear. Repeated aliases select a metric once, including when that leaves just one scalar. `-b` without `-m` is quiet on success. With neither option, lsqa retains the full JSON report with metadata. Use full JSON when reproducing interpretation and comparison settings is necessary.

```sh
lsqa -m MS-SSIM -m DE00_region_max reference.png output.six
lsqa -m MS-SSIM,DE00_region_max reference.png output.six
lsqa -m PSNR_Y -b MS-SSIM:min=0.98 reference.png output.six
```

Legacy `METRIC:VALUE` remains accepted: S-CIELAB and S-CIELAB_max use upper bounds, and all other metrics use lower bounds, including legacy color-difference names. Prefer explicit directions for new gates. Legacy values retain float32 threshold rounding; explicit bounds retain parsed double precision. The existing metric collection precision is retained: still-image gates collect six-decimal JSON values into float32, and video gates collect temporal means into float32. A displayed value alone may therefore be insufficient to reproduce a decision at a rounding boundary. Names retain their existing aliases and case rules; `-m` also retains its established unique-prefix matching. `SSIM` remains an alias for MS-SSIM, not a single-scale metric. The `min=` and `max=` keywords are lowercase.

At most one lower and one upper bound may be supplied for each canonical metric. Duplicate bounds, including through aliases or mixed legacy/explicit spelling, are usage errors (exit 2). Opposite bounds form an inclusive interval; a lower bound greater than its upper bound is rejected in either order. Numeric values must be finite and parse without range errors or trailing garbage. Empty comma members and unknown directions are rejected. Spaces and tabs around list members are ignored.

`LSQA_METRICS` and `LSQA_BASELINE` accept the respective comma-list grammars. Any explicit `-m` replaces the entire environment metric list, and any explicit `-b` replaces the entire environment gate list; the two families are independent. Ignored environment lists are not parsed. Multiple CLI occurrences accumulate within their family rather than replacing earlier occurrences.

## Reading the definitions

`lsqa` compares a reference image `x` with a test image `y` over their common top-left rectangle of `W × H` pixels. Two kinds of signal are used:

- **Spatial and luma metrics** (MS-SSIM, VIF-P, GMSD, PSNR-Y, HighFreqRatio, StripeScore, BandingIndex, and the luma ClipRate) read a single-channel comparison signal. By default it is encoded sRGB luma `Y' = 0.2126 R' + 0.7152 G' + 0.0722 B'` with the ITU-R BT.709 coefficients on gamma-encoded samples in the nominal range 0 to 1. `--compare-colorspace` can select linear `Y` instead; see [physical color and comparison signal](color-evaluation.md#physical-color-and-comparison-signal).
- **Color metrics** (`Δ E00_mean`, `DE00_max`, `Δ Chroma_mean`, the regional ΔE00 values, and S-CIELAB) read CIELAB computed from physical XYZ with the ICC D50 white. They do not depend on the comparison-signal options.

`Y'` is *luma*, a weighted sum of gamma-encoded components. It is not CIE luminance `Y`, and with saturated colors it departs from the lightness that a viewer perceives (the non-constant-luminance error of video luma). It is nevertheless roughly perceptually uniform in the way that encoded sRGB is, which is why it remains the default.

All of these are *full-reference* metrics: they compare an output with its reference, and what makes them perceptual is discussed in [perceptual quality](visual-perception/README.md#perceptual-quality).

Formulas use plain-text notation: `Σ` is a sum, `Π` a product, `E[·]` an average over pixels, `*` convolution, and `|·|` an absolute value or magnitude. Unless stated otherwise, "higher is better" applies to similarity scores and "lower is better" to differences.

| Metric | Signal | Better | Family |
| --- | --- | --- | --- |
| `MS-SSIM` | luma | higher, 1 = identical | reference five-scale MS-SSIM product |
| `VIF-P` | luma | higher, 1 = informative identity; may exceed 1 | four-scale pixel-domain information fidelity |
| `GMSD` | luma | lower, 0 = identical | gradient similarity |
| `PSNR_Y` | luma | higher, 99 = identical | error power |
| `Δ E00_mean`, `DE00_max` | CIELAB | lower | per-pixel color difference |
| `DE00_region_mean`, `DE00_region_max` | CIELAB of tile means | lower | low-pass color difference |
| `S-CIELAB`, `S-CIELAB_max` | spatially filtered opponent color, then CIELAB | lower, 0 = identical | spatial CIE76 difference |
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

The separable implementation keeps five double-precision horizontal moments for only the eleven rows needed by the current vertical window. It consumes each complete window in row order and reuses that storage across pyramid levels. The two downsampled input planes remain separate. This reduces the metric workspace from `7 * width * height` doubles to `2 * width * height + 55 * (width - 10)` doubles without changing Gaussian tap order, spatial pooling order, scale weights or the admission rule.

#### Numerical validation

The [independent reference generator](../../tests/data/assessment/ms_ssim_reference.py) evaluates direct two-dimensional Gaussian windows in NumPy and emits each factor and the final product. It does not call the production separable filter or `lsqa`. The C observer checks MS-SSIM alone and alongside GMSD against the same literal expectations, covering flat levels, a fine checker pattern, a textured even-size image and an odd-size image. The checker averages to a constant after the first reduction, isolating the first contrast/structure exponent; the flat case isolates the sole coarsest-scale luminance factor. Odd-size and textured observations additionally exercise valid-window pooling, symmetric edge handling, scale weights and the last spatially pooled SSIM factor.

Scores use an absolute tolerance of `1.5e-5`, not bit identity. The reference begins with ideal gray levels (binary fractions except `3/255`); the observation includes float32 storage and BT.709 luma arithmetic before double statistics. The generator bounds input perturbations by `4e-7` times each image's maximum sample, accounting for input rounding, coefficient representation, three weighted products and two sums. Variance perturbation is bounded by `2 sigma delta + delta²`, covariance perturbation by `sigma_x delta_y + sigma_y delta_x + delta_x delta_y`, and the stabilized ratios and weighted product propagate those bounds to the final score. The resulting input-error bounds are below `6e-8` for the flat case, `4.5e-6` for the checker and `1.1e-5` for both textured cases. The remaining budget covers double statistics/reduction, `exp`/`pow` and float32 result storage. This budget applies to these fixtures, not every possible anticorrelated or near-zero-factor input. Availability, metric masks and stale-result clearing are exact state assertions. See the shared [floating-point testing rules](../testing/guide.md#intermediate-floating-point-error).

### What it measures

MS-SSIM is sensitive to changes in local mean, contrast, and correlation of the luma signal, summarized over five scales. It is insensitive to chroma, because only luma is compared, and to errors that preserve local statistics. On dithered output its finest scale sees the dither pattern as lost correlation, while coarser scales see the locally averaged tone, which is closer to what a viewer perceives at a distance. See [why multiscale structure matters](#why-multiscale-structure-matters). Its weights assume a viewing density of 32 pixels per degree; see [viewing geometry of each metric](#viewing-geometry-of-each-metric).

## Information fidelity: VIF-P

### Origin

Sheikh and Bovik introduced Visual Information Fidelity (VIF) in "Image information and visual quality" (IEEE Transactions on Image Processing 15(2), 2006). It compares the reference information available through a model of visual noise with the information preserved after distortion. Their [pixel-domain implementation](https://live.ece.utexas.edu/research/quality/VIF.htm) uses a scalar statistical model and Gaussian filtering at multiple scales. `VIF-P` names that variant explicitly; it is not the original wavelet-domain vector model and does not reproduce VMAF's separately exposed VIF features.

### Definition

The reference is `x` and the output is `y`; exchanging them generally changes the score. Local Gaussian moments estimate a gain `g` and residual distortion variance `sv²` at every valid window. The following information ratio pools all windows at all four scales before division:

```text
vx = max(0, E[x²] − E[x]²)       vy = max(0, E[y²] − E[y]²)
c  = E[xy] − E[x] E[y]
g  = c / (vx + epsilon)          sv² = vy − g c

VIF-P = Σ log(1 + g² vx / (sv² + sn²)) / Σ log(1 + vx / sn²)
epsilon = 10⁻¹⁰                 sn² = 2
```

The existing [comparison signal](color-evaluation.md#physical-color-and-comparison-signal) supplies BT.709-weighted luma, multiplied by 255 before evaluating these constants. This is a floating-point rescaling, not 8-bit quantization or clipping. The default is encoded sRGB luma; choosing another comparison space or precision changes the input signal and must be recorded with the score. No additional VIF-P parameter, trained model, or viewing distance is required.

Each scale uses the following normalized Gaussian window. Valid filtering discards the border where the complete window would not fit; it does not pad or reflect samples. At scales 2 through 4, low-pass filtering and subsampling precede the moment calculation, using that scale's same window for both operations.

| Scale | Gaussian window | Standard deviation | Input reduction |
| --- | --- | --- | --- |
| 1 | 17 × 17 | 3.4 | None |
| 2 | 9 × 9 | 1.8 | Valid filter, then retain rows/columns 0, 2, 4, ... |
| 3 | 5 × 5 | 1.0 | Valid filter, then retain rows/columns 0, 2, 4, ... |
| 4 | 3 × 3 | 0.6 | Valid filter, then retain rows/columns 0, 2, 4, ... |

After estimating gain and residual, a reference variance below `epsilon` sets `vx = 0`, `g = 0` and `sv² = vy`. Next, an output variance below `epsilon` sets `g = 0` and `sv² = 0`. A negative gain sets `g = 0` and `sv² = vy`. Finally, `sv²` is floored at `epsilon`. Flat targets and negative correlations therefore transmit no reference information. Statistics, Gaussian coefficients and information sums use double precision; the final result uses the existing float metric storage.

### Availability and interpretation

Both aligned dimensions must be at least 41 pixels: valid filtering and subsampling reduce 41 to 17, 7 and 3, leaving a complete final window. The evaluator does not enlarge smaller inputs or omit scales. A reference carrying zero information makes the ratio undefined, even when both images are the same uniform color. Such results, and nonfinite comparison samples reaching the kernel, are unavailable: scalar output is `nan`, JSON is `null`, and any required bound fails with exit code 5. Other selected metrics remain available.

An informative identical pair scores one to numerical precision. Ordinary information loss lowers the score toward zero, but contrast enhancement can raise it above one; the implementation does not clamp that gain. VIF-P is insensitive to a constant luma offset without clipping and does not directly measure chroma. Fine dither affects local correlation and residual noise, while coarser scales measure filtered structure. Use it alongside color differences and direct visual checks; its score is neither an MS-SSIM threshold nor a calibrated measure of all visible SIXEL artifacts.

### Numerical validation

The [independent reference generator](../../tests/data/assessment/vif_p_reference.py) reads the committed 64 × 64 snake BMP directly and uses dense two-dimensional Gaussian windows. It regenerates deterministic blur/noise, odd-size, contrast and boundary specimens offline, without calling the production filter or `lsqa`. The CLI tests check the resulting literal expectations through two-sided bounds: approximately `0.496366304262` for blur/noise, `0.496192151663` for its 63 × 61 crop, `0.512388752434` for the reversed pair and `1.316261244854` for contrast gain.

The reference begins with ideal byte samples and BT.709 coefficients. A `4e-7` relative input budget covers float32 normalization, coefficient representation, weighted products and summation. Positive normalized filters do not enlarge the absolute sample perturbation. The generator propagates variance and covariance bounds through gain, residual variance, the logarithms and the final ratio. Its conservative input-error bounds are below `2.02e-4` for the first three specimens and `2.52e-3` for contrast gain. Test tolerances are respectively `2.2e-4` and `2.6e-3`, including headroom for double arithmetic, float result storage, six-decimal JSON collection and gate rounding. These budgets apply to the specified specimens; unavailable states and exact zero results are asserted separately.

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

## Viewing geometry of each metric

Every spatial metric assumes, explicitly or implicitly, how large one image pixel appears to the viewer. That size is expressed in pixels per degree (ppd) of visual angle, computed from the pixel pitch and the viewing distance as described in [from pixels to cycles per degree](visual-perception/README.md#from-pixels-to-cycles-per-degree). The value that matters is the effective density of the image, not of the panel: a terminal or operating system that draws one image pixel with `k × k` device pixels divides the device's ppd by `k`. Desktop and notebook displays in current use range from about 34 to about 90 ppd at their usual distances; [pixel density and the eye](../concepts/displays/README.md#pixels-per-degree) traces how that range developed.

| Metric | Assumed geometry | Consequence |
| --- | --- | --- |
| `PSNR-Y`, `Δ E00_mean`, `DE00_max`, `Δ Chroma_mean` | None; each pixel is compared on its own | Equivalent to inspecting individual pixels. Dither noise counts in full at every viewing distance |
| `GMSD`, `HighFreqRatio_delta`, `StripeScore_rel`, `BandingIndex_*` | Fixed filters in pixel units, no ppd parameter | The same pixel-domain scale corresponds to a different visual angle at a different density |
| `DE00_region_mean`, `DE00_region_max` | Square tiles of 8 to 64 pixels, default 16 | A 16-pixel tile spans 0.5° at 32 ppd and about 0.27° at 60 ppd |
| `MS-SSIM` | Implicitly 32 ppd at scale 1 | The published scale weights were calibrated at that density |
| `VIF-P` | Four Gaussian scales in pixel units, no ppd parameter | Information is pooled across the fixed pyramid without display calibration |
| `S-CIELAB` | Explicit, `-v PPD`, default 60 | The opponent filters are defined in degrees of visual angle |

The MS-SSIM weights were fitted in a psychophysical experiment that used ten 64 × 64 grayscale images viewed at 32 pixels per degree (Wang, Simoncelli and Bovik, 2003, section 3.2). `lsqa` treats the input pixels as scale 1, so its MS-SSIM assumes that each image pixel subtends 1/32 of a degree: the density of the SIXEL terminals of the 1980s and of the desktop LCDs of the 2000s, and coarser than the high-density displays that followed. At a higher effective density the viewer integrates fine-scale error that the finest factors still count. Error diffusion and blue-noise dithering, which place their error at the highest frequencies, are therefore penalized more than a viewer at that density sees. `lsqa` has no viewing-geometry option for MS-SSIM. To approximate an effective density of `P` ppd, reduce the reference and the output by the same integer factor near `P / 32` with an area filter before the comparison, keeping both images at least 176 pixels in each dimension, and report the factor with the score.

Metrics with different assumed geometry can rank the same outputs in opposite order. The typical case is a trade between dither-noise amplitude and integrated color. A palette whose entries reach farther toward the edge of an image's color distribution lets diffusion reproduce local averages more closely, but at the cost of larger noise; a palette concentrated on the most frequent colors produces smaller noise but cannot reach the outer colors on average. Per-pixel ΔE00 and MS-SSIM at the image's own resolution favor the smaller noise, while S-CIELAB at 60 ppd and MS-SSIM on a reduced image favor the closer averages. Such a disagreement states which viewing distance each metric represents rather than a contradiction between metrics. Resolve it by naming the target effective density, as the [measurement policy](measurement-policy.md#controlled-comparisons) requires, not by selecting the metric whose ranking is preferred.

## Spatial color difference: S-CIELAB

`lsqa -m S-CIELAB reference.png output.six` reports the pixel-weighted mean spatial CIE76 difference; `-m S-CIELAB_max` reports its maximum. Both are lower-is-better, zero for identical inputs, and included in the full JSON report. Names are case-insensitive and use the shared exact/unique-prefix selection rules. For these two metrics, `-b S-CIELAB:VALUE` or `-b S-CIELAB_max:VALUE` is an **upper** bound: equality passes and a larger value exits with status 5. Previously available metrics retain their existing baseline behavior.

### Viewing geometry

`-v PPD` / `--scielab-ppd=PPD` sets pixels per degree of visual angle, accepting finite real values from 1 through 1024. The default is 60, an explicit viewing assumption, not a measurement of the terminal. Repeated options use the last value. Hold PPD fixed across comparisons. For physical pixel pitch `p` and viewing distance `D` in the same units, the small-angle approximation is `PPD = (pi / 180) * D / p`. Scaling an image on the display changes its effective pixel pitch. Higher PPD means each image pixel subtends a smaller angle, so fine alternating errors generally become less visible; it does not guarantee a monotonic score for arbitrary images.

The full JSON records `scielab_ppd`, `scielab_model`, `scielab_difference`, and `scielab_boundary` in `color_evaluation`. The metric is evaluated over the existing common top-left image rectangle, including every boundary pixel. It works on one-pixel axes and on images smaller than the filter support.

### Computation and reproducibility

The method follows Zhang and Wandell's S-CIELAB: transform each image's physical XYZ to linear opponent channels, filter the channels separately, transform back to XYZ, then evaluate ordinary CIELAB Euclidean distance. It filters the two images before the nonlinear Lab conversion; it does not blur a map of already-computed color differences. The [physical color contract](color-evaluation.md#physical-color-and-comparison-signal) supplies XYZ D50, white `(0.9642, 1, 0.8249)`, and preserves signed and above-white samples. `-W`, `-P`, and `-g` do not change these inputs. The difference formula is **CIE76**, not CIEDE2000; thresholds for ΔE00 are not interchangeable with S-CIELAB values.

The coefficients and sampled Gaussian convention come from the [author's released implementation](https://github.com/wandell/SCIELAB-1996/tree/cacbc977c07a29c3e92a1aec849ee63bba72e6bf), specifically `cmatrix.m`, `separableFilters.m`, and `gauss.m`. The XYZ transform, acting on column vectors, is:

```text
[ 0.2787336   0.7218031  -0.1065520 ]
[-0.4487736   0.2898056   0.0771569 ]
[ 0.0859513  -0.5899859   0.5011089 ]
```

Each channel's spatial kernel is a weighted sum of normalized 2-D Gaussians:

| Channel | Spread in degrees and weight |
| --- | --- |
| Achromatic | `(0.05, 1.00327)`, `(0.225, 0.114416)`, `(7.0, -0.117686)` |
| Red-green | `(0.0685, 0.616725)`, `(0.826, 0.383275)` |
| Blue-yellow | `(0.0920, 0.567885)`, `(0.6451, 0.432115)` |

To reproduce the discrete kernels, set `u = max(1, ceil(224 / PPD))`, `q = u * PPD`, and `r = ceil(q / 2) - 1`. On integer offsets `i = -r..r`, the released Gaussian convention is `g(i) = exp(-4 ln(2) * i² / (spread * q - 1)²)`. Convolve with the tent sequence `t(j) = 1 - abs(j)/u`, `j = 1-u..u-1`, and retain offsets `u*k`, `k = -floor(r/u)..floor(r/u)`, centered on zero. Normalize each resulting 1-D Gaussian to sum to one. Form its outer product, multiply by its channel weight, and sum the components. Production uses separable convolutions per component, with the negative achromatic weight applied once; taking the outer product of a mixture of 1-D kernels would introduce incorrect cross terms.

The declared variant is `zhang-wandell-1998-unit-dc`. It adds unit-DC normalization **after** resampling to preserve flat colors at every accepted PPD; the historical resampling can lose DC at the truncated support. It also repeats edge-repeating reflection as far as needed (`... b a | a b c | c b ...`), whereas the historical padding is limited by image size. These choices are intentional and mean that this is not a bit-for-bit MATLAB compatibility mode. Coefficient sets from later ISET variants or the earlier published table must not be substituted silently. CIELAB uses the exact standard rational breakpoint through the shared evaluator. Signed filter lobes and extended XYZ are not clipped.

### Interpretation and limits

S-CIELAB discounts fine spatial variations according to separate achromatic and chromatic filters. Compensating dither can therefore have much less spatial color error than a broad shift with the same per-pixel colors. It remains a general full-reference spatial color difference, not a banding detector or a counter of low-frequency artifacts. Blur, edge displacement and lost detail can also contribute. The maximum exposes a local peak while the mean reflects its area; neither alone classifies the cause.

[Video assessment](video-evaluation.md) evaluates this spatial metric per source frame or reference/display clock interval, exports its error maps and distributions, and supplies a separate screen-coordinate persistence diagnostic.

S-CIELAB itself is a CPU image metric. It has no temporal persistence weighting, motion tracking, gaze model, masking model, or universal perceptual pass threshold. A single-frame score cannot distinguish a transient defect from one that remains for seconds. Such evaluation needs a separately specified temporal layer.

### Numeric evidence

[The independent reference generator](../../tests/data/assessment/scielab_reference.py) uses NumPy dense 2-D mixtures and matrix inversion, without calling production C, and emits frozen maps for signed binary-fraction XYZ on a 5×4 image at PPD 12, 28.5, 60 and 224. Tests check every map pixel, mean and maximum. The dense-reference tolerance is `1e-6` CIE76 units. The reference sums at most `223²` terms per output pixel; double-precision accumulation error, a kernel absolute-weight sum at most 1.236, and the opponent/inverse matrix norms bound XYZ perturbation conservatively by `5e-11` per image on this fixture. Propagating both images through the largest Lab derivatives (about 7932, 3446 and 904 for a*, b* and L* under a uniform XYZ error bound) and the Euclidean norm stays below `1e-6`. This budget includes the different reduction order of dense versus separable convolution. No float32 storage lies between these fixture inputs and the metric. The frame integration check starts from exact binary-fraction float32 samples and separately verifies that luma-only options leave the physical input path intact.

Uniform neutral fields supply an analytic check independent of that generator: `Y=1/8` and `Y=1` have `L*=42` and `L*=100`, hence mean and maximum 58 at every PPD. Identity, chromatic spatial cancellation, thin-image symmetry, invalid inputs, result reuse and CLI behavior have separate tests. These checks establish the implementation contract, not subjective validation of a video quality threshold.

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

**Limits.** This split under-represents chromatic spatial artifacts at low frequencies, which are visible: color banding in gradients, chroma noise in large flat areas, and slow hue or neutral drift caused by palette geometry, such as gray areas drifting toward a tint. The luma metrics are blind to these. Regional ΔE00 and `Δ Chroma_mean` are the current safeguards, which is why the [measurement policy](measurement-policy.md#primary-and-secondary-metrics) prefers per-pixel color summaries for palette studies. Luma itself is gamma-encoded and non-constant-luminance, so saturated colors are weighted imperfectly; linear `Y` is available as a comparison signal when that matters. [S-CIELAB](#spatial-color-difference-s-cielab) adds opponent-channel spatial filtering to assess this gap directly at an explicit viewing geometry.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

This inventory covers reference MS-SSIM, VIF-P, S-CIELAB, and their coexistence with the other metrics. It does not claim to independently validate every other metric described in this document; color-coordinate coverage is owned by [color-aware evaluation](color-evaluation.md#test-coverage).

| ID | Contract | Owning test |
| --- | --- | --- |
| MO-01 | Identical textured inputs score one at the minimum admitted size. | [tests/assessment/ms-ssim/0001_ms_ssim_identity.t](../../tests/assessment/ms-ssim/0001_ms_ssim_identity.t) |
| MO-02 | A flat mean-level shift contributes luminance only at the coarsest scale. | [tests/assessment/ms-ssim/0002_ms_ssim_coarsest_luminance.t](../../tests/assessment/ms-ssim/0002_ms_ssim_coarsest_luminance.t) |
| MO-03 | A fine checker contributes through the first CS exponent and geometric product. | [tests/assessment/ms-ssim/0003_ms_ssim_geometric_product.t](../../tests/assessment/ms-ssim/0003_ms_ssim_geometric_product.t) |
| MO-04 | An even-sized textured input matches independently pooled reference factors. | [tests/assessment/ms-ssim/0004_ms_ssim_even_reference.t](../../tests/assessment/ms-ssim/0004_ms_ssim_even_reference.t) |
| MO-05 | Odd-sized reduction and valid windows match the independent reference. | [tests/assessment/ms-ssim/0005_ms_ssim_odd_reference.t](../../tests/assessment/ms-ssim/0005_ms_ssim_odd_reference.t) |
| MO-06 | Swapping reference and output preserves the reference score. | [tests/assessment/ms-ssim/0006_ms_ssim_symmetry.t](../../tests/assessment/ms-ssim/0006_ms_ssim_symmetry.t) |
| MO-07 | Inputs narrower than 176 pixels have no MS-SSIM result. | [tests/assessment/ms-ssim/0007_ms_ssim_undersized.t](../../tests/assessment/ms-ssim/0007_ms_ssim_undersized.t) |
| MO-08 | A negative mean factor has no real-valued MS-SSIM result. | [tests/assessment/ms-ssim/0008_ms_ssim_negative_factor.t](../../tests/assessment/ms-ssim/0008_ms_ssim_negative_factor.t) |
| MO-09 | An unavailable comparison clears a preceding valid score in either narrow orientation. | [tests/assessment/ms-ssim/0009_ms_ssim_reuse.t](../../tests/assessment/ms-ssim/0009_ms_ssim_reuse.t) |
| MO-10 | CLI selection is case-insensitive and accepts the matching MS-SSIM baseline. | [tests/assessment/ms-ssim/0045_reference_selection.t](../../tests/assessment/ms-ssim/0045_reference_selection.t) |
| MO-11 | The established -mm prefix selects the reference score. | [tests/assessment/ms-ssim/0046_reference_prefix.t](../../tests/assessment/ms-ssim/0046_reference_prefix.t) |
| MO-12 | An unavailable MS-SSIM result prints nan and fails even a zero baseline. | [tests/assessment/ms-ssim/0047_unavailable_baseline.t](../../tests/assessment/ms-ssim/0047_unavailable_baseline.t) |
| MO-13 | The full JSON report includes an unavailable MS-SSIM as null. | [tests/assessment/ms-ssim/0048_json_unavailable.t](../../tests/assessment/ms-ssim/0048_json_unavailable.t) |
| MO-14 | A finite MS-SSIM score below its baseline exits with quality failure. | [tests/assessment/ms-ssim/0049_finite_baseline.t](../../tests/assessment/ms-ssim/0049_finite_baseline.t) |
| MO-15 | Help describes the reference formula and minimum dimensions. | [tests/assessment/ms-ssim/0050_reference_help.t](../../tests/assessment/ms-ssim/0050_reference_help.t) |

| SC-01 | Identical signed color images have a zero error map. | [tests/assessment/scielab/0001_scielab_identity.t](../../tests/assessment/scielab/0001_scielab_identity.t) |
| SC-02 | Flat fields retain the analytic CIE76 difference across PPD settings. | [tests/assessment/scielab/0002_scielab_uniform.t](../../tests/assessment/scielab/0002_scielab_uniform.t) |
| SC-03 | Independent dense convolution matches every map pixel and both summaries. | [tests/assessment/scielab/0003_scielab_dense_reference.t](../../tests/assessment/scielab/0003_scielab_dense_reference.t) |
| SC-04 | Fine chromatic alternation is discounted relative to broad color regions. | [tests/assessment/scielab/0004_scielab_spatial_cancellation.t](../../tests/assessment/scielab/0004_scielab_spatial_cancellation.t) |
| SC-05 | One-pixel axes retain symmetry under transpose and input exchange. | [tests/assessment/scielab/0005_scielab_thin_symmetry.t](../../tests/assessment/scielab/0005_scielab_thin_symmetry.t) |
| SC-06 | Invalid settings, dimensions and samples leave result outputs unchanged. | [tests/assessment/scielab/0006_scielab_invalid.t](../../tests/assessment/scielab/0006_scielab_invalid.t) |
| SC-07 | Physical color metrics ignore luma-only gray and precision settings. | [tests/assessment/scielab/0007_scielab_physical_signal.t](../../tests/assessment/scielab/0007_scielab_physical_signal.t) |
| SC-08 | Selected results and availability are reset across failure and reuse. | [tests/assessment/scielab/0008_scielab_selection_reuse.t](../../tests/assessment/scielab/0008_scielab_selection_reuse.t) |
| SC-09 | Case-insensitive selection uses an upper-bound baseline. | [tests/assessment/scielab/0011_cli_selection.t](../../tests/assessment/scielab/0011_cli_selection.t) |
| SC-10 | A unique maximum prefix and fractional short PPD are accepted. | [tests/assessment/scielab/0012_max_prefix.t](../../tests/assessment/scielab/0012_max_prefix.t) |
| SC-11 | Viewing metadata records the last PPD and exact model. | [tests/assessment/scielab/0013_ppd_metadata.t](../../tests/assessment/scielab/0013_ppd_metadata.t) |
| SC-12 | An error above the ceiling exits with quality failure. | [tests/assessment/scielab/0014_baseline_failure.t](../../tests/assessment/scielab/0014_baseline_failure.t) |
| SC-13 | PPD rejects trailing numeric garbage. | [tests/assessment/scielab/0015_ppd_invalid.t](../../tests/assessment/scielab/0015_ppd_invalid.t) |
| SC-14 | Help declares the viewing geometry and difference units. | [tests/assessment/scielab/0016_help.t](../../tests/assessment/scielab/0016_help.t) |
| SC-15 | A target read from standard input has the same score. | [tests/assessment/scielab/0017_stdin.t](../../tests/assessment/scielab/0017_stdin.t) |
| SC-16 | Nonfinite PPD is rejected by the CLI. | [tests/assessment/scielab/0018_ppd_nonfinite.t](../../tests/assessment/scielab/0018_ppd_nonfinite.t) |
| SC-17 | Default JSON includes both metrics at 60 PPD. | [tests/assessment/scielab/0019_json_default.t](../../tests/assessment/scielab/0019_json_default.t) |

### VIF-P contracts

| ID | Contract | Owning test |
| --- | --- | --- |
| VP-01 | VIF-P identity selection is case-insensitive. | [tests/assessment/vif-p/0001_identity.t](../../tests/assessment/vif-p/0001_identity.t) |
| VP-02 | VIF-P even reference. | [tests/assessment/vif-p/0002_even_reference.t](../../tests/assessment/vif-p/0002_even_reference.t) |
| VP-03 | VIF-P odd reference. | [tests/assessment/vif-p/0003_odd_reference.t](../../tests/assessment/vif-p/0003_odd_reference.t) |
| VP-04 | VIF-P reference order. | [tests/assessment/vif-p/0004_reverse_reference.t](../../tests/assessment/vif-p/0004_reverse_reference.t) |
| VP-05 | VIF-P contrast gain above one. | [tests/assessment/vif-p/0005_contrast_gain.t](../../tests/assessment/vif-p/0005_contrast_gain.t) |
| VP-06 | VIF-P accepts the complete 41x41 pyramid. | [tests/assessment/vif-p/0006_minimum_size.t](../../tests/assessment/vif-p/0006_minimum_size.t) |
| VP-07 | VIF-P rejects width below 41. | [tests/assessment/vif-p/0007_narrow_unavailable.t](../../tests/assessment/vif-p/0007_narrow_unavailable.t) |
| VP-08 | VIF-P rejects height below 41. | [tests/assessment/vif-p/0008_short_unavailable.t](../../tests/assessment/vif-p/0008_short_unavailable.t) |
| VP-09 | VIF-P zero reference information is unavailable. | [tests/assessment/vif-p/0009_flat_reference.t](../../tests/assessment/vif-p/0009_flat_reference.t) |
| VP-10 | VIF-P flat target transmits zero information. | [tests/assessment/vif-p/0010_flat_target.t](../../tests/assessment/vif-p/0010_flat_target.t) |
| VP-11 | VIF-P negative correlation transmits zero information. | [tests/assessment/vif-p/0011_negative_correlation.t](../../tests/assessment/vif-p/0011_negative_correlation.t) |
| VP-12 | VIF-P coexists with an unavailable selected metric. | [tests/assessment/vif-p/0012_selected_json.t](../../tests/assessment/vif-p/0012_selected_json.t) |
| VP-13 | VIF-P appears in the full JSON report. | [tests/assessment/vif-p/0013_full_json.t](../../tests/assessment/vif-p/0013_full_json.t) |
| VP-14 | VIF-P rejects a finite score below its bound. | [tests/assessment/vif-p/0014_finite_gate.t](../../tests/assessment/vif-p/0014_finite_gate.t) |
| VP-15 | VIF-P unavailable result fails even a zero bound. | [tests/assessment/vif-p/0015_unavailable_gate.t](../../tests/assessment/vif-p/0015_unavailable_gate.t) |
| VP-16 | VIF-P unavailable JSON retains other metric results. | [tests/assessment/vif-p/0016_unavailable_json.t](../../tests/assessment/vif-p/0016_unavailable_json.t) |
| VP-17 | VIF-P accepts the existing metric and baseline environment. | [tests/assessment/vif-p/0017_environment.t](../../tests/assessment/vif-p/0017_environment.t) |
| VP-18 | VIF-P is included in the verbose metric report. | [tests/assessment/vif-p/0018_verbose.t](../../tests/assessment/vif-p/0018_verbose.t) |
| VP-19 | VIF-P participates in video mean selection and bounds. | [tests/assessment/vif-p/0019_video_mean.t](../../tests/assessment/vif-p/0019_video_mean.t) |

### Metric selection and gate contracts

These tests assert CLI behavior, including deliberate rejection controls; their bounds do not define encoder quality floors.

| ID | Contract | Test |
| --- | --- | --- |
| MG-01 | Output selection and gate metrics are independent. | [tests/assessment/ms-ssim/0023_lsqa_independent_output_and_gate.t](../../tests/assessment/ms-ssim/0023_lsqa_independent_output_and_gate.t) |
| MG-02 | Repeated metrics produce named JSON. | [tests/assessment/ms-ssim/0027_lsqa_repeated_metric_json.t](../../tests/assessment/ms-ssim/0027_lsqa_repeated_metric_json.t) |
| MG-03 | Metric aliases deduplicate to scalar output. | [tests/assessment/gates/0001_metric_list_dedup.t](../../tests/assessment/gates/0001_metric_list_dedup.t) |
| MG-04 | Comma lists emit metrics in registry order. | [tests/assessment/gates/0002_metric_list_order.t](../../tests/assessment/gates/0002_metric_list_order.t) |
| MG-05 | Structural and color gates pass quietly. | [tests/assessment/gates/0003_structure_color_pass.t](../../tests/assessment/gates/0003_structure_color_pass.t) |
| MG-06 | Color error fails despite passing structure. | [tests/assessment/gates/0004_color_failure.t](../../tests/assessment/gates/0004_color_failure.t) |
| MG-07 | Structure fails despite passing color. | [tests/assessment/gates/0005_structure_failure.t](../../tests/assessment/gates/0005_structure_failure.t) |
| MG-08 | Every failed bound is diagnosed. | [tests/assessment/gates/0006_all_failed_diagnostics.t](../../tests/assessment/gates/0006_all_failed_diagnostics.t) |
| MG-09 | Both inclusive bounds may gate one metric. | [tests/assessment/gates/0007_inclusive_interval.t](../../tests/assessment/gates/0007_inclusive_interval.t) |
| MG-10 | Contradictory bounds are rejected. | [tests/assessment/gates/0008_contradictory_interval.t](../../tests/assessment/gates/0008_contradictory_interval.t) |
| MG-11 | Contradiction detection is order independent. | [tests/assessment/gates/0009_reversed_interval.t](../../tests/assessment/gates/0009_reversed_interval.t) |
| MG-12 | Aliases cannot duplicate a bound. | [tests/assessment/gates/0010_duplicate_alias_bound.t](../../tests/assessment/gates/0010_duplicate_alias_bound.t) |
| MG-13 | Legacy color bounds retain minimum semantics. | [tests/assessment/gates/0011_legacy_color_direction.t](../../tests/assessment/gates/0011_legacy_color_direction.t) |
| MG-14 | Unavailable required metric fails a combined gate. | [tests/assessment/gates/0012_unavailable_required.t](../../tests/assessment/gates/0012_unavailable_required.t) |
| MG-15 | Selected JSON uses null for unavailable metrics. | [tests/assessment/gates/0013_json_unavailable.t](../../tests/assessment/gates/0013_json_unavailable.t) |
| MG-16 | Environment lists select and gate multiple metrics. | [tests/assessment/gates/0014_environment_lists.t](../../tests/assessment/gates/0014_environment_lists.t) |
| MG-17 | Explicit lists replace malformed environment lists. | [tests/assessment/gates/0015_cli_replaces_environment.t](../../tests/assessment/gates/0015_cli_replaces_environment.t) |
| MG-18 | CLI output selection retains environment gates. | [tests/assessment/gates/0016_environment_gate_independence.t](../../tests/assessment/gates/0016_environment_gate_independence.t) |
| MG-19 | CLI gates retain environment output selection. | [tests/assessment/gates/0017_environment_output_independence.t](../../tests/assessment/gates/0017_environment_output_independence.t) |
| MG-20 | Multiple metrics and gates share one stdin input. | [tests/assessment/gates/0018_stdin_shared.t](../../tests/assessment/gates/0018_stdin_shared.t) |
| MG-21 | Video reports named means and checks combined gates. | [tests/assessment/gates/0019_video_selection.t](../../tests/assessment/gates/0019_video_selection.t) |
| MG-22 | Video diagnoses all failed mean bounds. | [tests/assessment/gates/0020_video_failure.t](../../tests/assessment/gates/0020_video_failure.t) |
| MG-23 | Empty metric member is rejected. | [tests/assessment/gates/0021_empty_metric_member.t](../../tests/assessment/gates/0021_empty_metric_member.t) |
| MG-24 | Empty gate member is rejected. | [tests/assessment/gates/0022_empty_gate_member.t](../../tests/assessment/gates/0022_empty_gate_member.t) |
| MG-25 | Nonfinite bound is rejected. | [tests/assessment/gates/0023_nonfinite_bound.t](../../tests/assessment/gates/0023_nonfinite_bound.t) |
| MG-26 | Overflow bound is rejected. | [tests/assessment/gates/0024_overflow_bound.t](../../tests/assessment/gates/0024_overflow_bound.t) |
| MG-27 | Unknown direction is rejected. | [tests/assessment/gates/0025_unknown_direction.t](../../tests/assessment/gates/0025_unknown_direction.t) |
| MG-28 | An ungated unavailable scalar video metric retains exit 4 and no stdout. | [tests/assessment/gates/0026_video_scalar_unavailable.t](../../tests/assessment/gates/0026_video_scalar_unavailable.t) |
| MG-29 | Legacy limits outside float32 range are rejected before conversion. | [tests/assessment/gates/0027_legacy_float_overflow.t](../../tests/assessment/gates/0027_legacy_float_overflow.t) |
| MG-30 | VIF-P unequal frame durations give mean 0.25. | [tests/assessment/vif-p/0020_video_weighted_mean.t](../../tests/assessment/vif-p/0020_video_weighted_mean.t) |
| MG-31 | VIF-P temporal extrema remain 0 and 1. | [tests/assessment/vif-p/0021_video_extrema.t](../../tests/assessment/vif-p/0021_video_extrema.t) |
| MG-32 | VIF-P bounds reject weighted 0.25 despite plain mean 0.5. | [tests/assessment/vif-p/0022_video_weighted_gate.t](../../tests/assessment/vif-p/0022_video_weighted_gate.t) |

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

The [two-frame VIF-P fixture generator](../../tests/data/assessment/vif_video_fixture.py) repeats the decoded 64-by-64 snake reference and supplies identity then flat-black targets for 0.1 and 0.3 seconds. The known scores 1 and 0 give a duration-weighted mean of 0.25, temporal minimum 0 and maximum 1; an unweighted mean or last-frame-only result fails.
