# Image-Quality Metrics and Their Perceptual Basis

## Scope

This document explains the image-quality metrics that `lsqa` reports: where each one comes from, the quantity it computes, how the libsixel implementation differs from the published definition, and what it can and cannot observe. It then explains the perceptual and physiological reasoning behind combining them for dithered output, why multiscale structure matters, why dither noise should be pushed toward high spatial frequencies, how structure-preserving dithering prioritizes luminance, and why the current spatial metrics are computed on a luminance-like signal.

The [image-quality measurement policy](measurement-policy.md) owns the primary gate, thresholds, fixtures, baselines, and comparison conditions. [Color-aware quality evaluation](color-evaluation.md) owns input interpretation, the XYZ D50 connection space, the comparison-signal options, and regional tiling. This page defines and explains; it does not set acceptance limits.

## Reading the definitions

`lsqa` compares a reference image `x` with a test image `y` over their common top-left rectangle of `W × H` pixels. Two kinds of signal are used:

- **Spatial and luma metrics** (MS-SSIM, GMSD, PSNR-Y, HighFreqRatio, StripeScore, BandingIndex, and the luma ClipRate) read a single-channel comparison signal. By default it is encoded sRGB luma `Y' = 0.2126 R' + 0.7152 G' + 0.0722 B'` with the ITU-R BT.709 coefficients on gamma-encoded samples in the nominal range 0 to 1. `--compare-colorspace` can select linear `Y` instead; see [physical color and comparison signal](color-evaluation.md#physical-color-and-comparison-signal).
- **Color metrics** (`Δ E00_mean`, `DE00_max`, `Δ Chroma_mean`, and the regional ΔE00 values) read CIELAB computed from physical XYZ with the ICC D50 white. They do not depend on the comparison-signal options.

`Y'` is *luma*, a weighted sum of gamma-encoded components. It is not CIE luminance `Y`, and with saturated colors it departs from the lightness that a viewer perceives (the non-constant-luminance error of video luma). It is nevertheless roughly perceptually uniform in the way that encoded sRGB is, which is why it remains the default.

Formulas use plain-text notation: `Σ` is a sum, `Π` a product, `E[·]` an average over pixels, `*` convolution, and `|·|` an absolute value or magnitude. Unless stated otherwise, "higher is better" applies to similarity scores and "lower is better" to differences.

| Metric | Signal | Better | Family |
| --- | --- | --- | --- |
| `MS-SSIM` | luma | higher, 1 = identical | structural similarity |
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
α_M = β_j = γ_j = w_j
(w_1 .. w_5) = (0.0448, 0.2856, 0.3001, 0.2363, 0.1333)
```

The weights peak at the middle scales. That is the fitted psychophysical result: with the experiment's viewing geometry, distortions near the middle scales were the most visible, consistent with the band-pass shape of the luminance contrast sensitivity function described [below](#contrast-sensitivity).

### The `lsqa` MS-SSIM

`lsqa` implements a related but different multiscale score in [`assessment.c`](../../assessment/assessment.c) (`ssim_luma` and `ms_ssim_luma`):

```text
SSIM_j  = mean over all pixels of the full SSIM map at scale j
          (11-tap Gaussian, σ = 1.5, K1 = 0.01, K2 = 0.03, L = 1,
           mirror-reflected borders, result clamped to [0, 1])
scale j+1 = 2 × 2 box average of scale j (an odd last row or column is dropped)

lsqa MS-SSIM = Σ_j w_j · SSIM_j / Σ_j w_j
```

The weights are the published ones, and scales stop early when a dimension falls below two pixels, in which case the remaining weights are renormalized. The differences from the published definition have practical consequences:

- **Luminance is compared at every scale.** The published score applies `l` only once, at the coarsest scale. Here a uniform shift in mean level is penalized at the finest scale too. With `C1 = 10⁻⁴`, a black reference area (`μx = 0`) reproduced as luma 3/255 (`μy ≈ 0.0118`) gives `l = 10⁻⁴ / (0.0118² + 10⁻⁴) ≈ 0.42` everywhere in that area, independent of structure. This is why images dominated by pure black can score very low when the palette lacks exact black, even though the color difference is small (the [codec comparison](../sixel-format/comparison/README.md) documents such cases).
- **Pooling is arithmetic, not geometric.** In the published score a single badly damaged scale pulls the product toward zero; here it only reduces a weighted average.
- **The whole map is averaged,** including mirror-padded borders, rather than only the "valid" region where the window fits.

As a result, `lsqa` MS-SSIM values are internally consistent and suitable for libsixel's comparative gates, but they are not numerically comparable with MS-SSIM values published elsewhere or produced by other libraries. Report the tool and revision with any value.

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

## Perceptual quality

### Fidelity, quality, and their measurement

Image *fidelity* is how closely a reproduction matches a reference. Image *quality* is how good an image is judged to be. They can diverge: a sharpened reproduction can be preferred to an exact one. All metrics in `lsqa` are *full-reference* fidelity metrics. They need the reference and estimate how visible or objectionable the departure from it is.

The ground truth for any such metric is human judgment collected under controlled conditions, for example by the methods in ITU-R Recommendation BT.500 for television pictures, summarized as mean opinion scores. A metric is "perceptual" to the extent that its predictions agree with those judgments across many images and distortion types. SSIM, MS-SSIM, GMSD, and CIEDE2000 were each validated against such data; the libsixel-specific descriptors were not.

For libsixel, perceptual quality of a quantized and dithered image means this: viewed at the intended size and distance, it is not distinguishable from the reference, or where it is, the visible differences are the least objectionable available within the palette and size constraints. That definition depends on the viewing geometry, which a file does not record; the next sections explain why that dependence is fundamental and how the metrics hedge against it.

### Visual physiology

#### Photoreceptors and opponent channels

Daylight vision begins with three cone classes, sensitive to long, medium, and short wavelengths (L, M, S). The retina and lateral geniculate nucleus recombine the cone signals into an achromatic channel, roughly L + M, and two opponent chromatic channels, roughly L − M (red-green) and S − (L + M) (blue-yellow). This recoding was proposed in the opponent-process theory of Hering and quantified by Hurvich and Jameson (1957). S cones are sparse and absent from the center of the fovea, which already limits blue-yellow spatial resolution.

CIELAB, the opponent coordinates OKLab and DIN99d, and YCbCr video all reflect this organization: one lightness axis and two opponent chroma axes. The achromatic channel is the one with high spatial resolution.

#### Light level and adaptation

The visual system works over roughly ten orders of magnitude of luminance, from starlight to sunlit snow, but at any moment it uses a far narrower operating range set by *adaptation*. Two photoreceptor systems divide that span.

**Cones and rods.** The human retina has about six million cones and more than ninety million rods. Cones come in the three classes described above, so they support color vision; they are concentrated in the fovea and need comparatively high light levels. Rods contain a single photopigment, rhodopsin, so they cannot distinguish wavelength and give no color by themselves; they are far more sensitive, are absent from the central fovea (about one degree across), and dominate peripheral vision. In bright light rods are saturated and contribute little (Aguilar and Stiles, "Saturation of the rod mechanism of the retina at high levels of stimulation", Optica Acta 1(1), 1954).

**Photopic, mesopic, and scotopic vision.** Which system carries vision depends on the adaptation luminance:

| Regime | Approximate luminance | Receptors | Consequences |
| --- | --- | --- | --- |
| Photopic | above about 5 cd/m² | cones | full color vision, highest acuity, spectral sensitivity `V(λ)` peaking near 555 nm |
| Mesopic | about 0.005 to 5 cd/m² | cones and rods together | color discrimination and acuity decline progressively; blue-green looks relatively brighter and red darker |
| Scotopic | below about 0.005 cd/m² | rods | no hue discrimination, low acuity, the fovea is nearly blind, spectral sensitivity `V'(λ)` peaking near 507 nm |

The mesopic limits are those of CIE 191:2010, which defines photometry for that range. The movement of peak sensitivity from about 555 nm to about 507 nm as light falls is the Purkinje shift: in dusk, red flowers darken while blue-green foliage remains visible. Colorimetry, including CIELAB and CIEDE2000, assumes photopic vision throughout.

**How adaptation works.** Several mechanisms adjust sensitivity to the prevailing light:

- the pupil changes diameter between about 2 and 8 mm, a factor of about 16 in light admitted, which covers only a small part of the full range;
- photopigment bleaching and regeneration reduce receptor sensitivity in bright light and restore it in darkness;
- neural gain control in the retina scales each receptor class's signal to the local mean, so that the response encodes *contrast*, the ratio to the adapted level, rather than absolute light.

The last mechanism is why vision follows Weber's law over much of the photopic range: the just-visible luminance step is a roughly constant fraction of the adapted luminance. At lower light levels photon noise dominates and the threshold grows more slowly than the luminance (the de Vries–Rose regime), so relative sensitivity falls. CIELAB's cube-root lightness and SSIM's ratio-based luminance term both encode the Weber-law idea.

**Time course.** Light adaptation, from dark to bright, takes from seconds to a minute or two. Dark adaptation is slow. Cone thresholds reach a plateau after about ten minutes in darkness, at the rod–cone break rods become the more sensitive system, and full rod sensitivity takes thirty to forty minutes. A viewer who has just looked at a bright window is not adapted to a dim screen, and the reverse.

**Chromatic adaptation.** Gain control acts separately on the three cone classes, so the visual system also adapts to the color of the illumination: a white paper looks white under both daylight and incandescent light. Von Kries ("Chromatic adaptation", 1902) modeled this as independent scaling of each cone response. CIELAB's division of `X`, `Y`, and `Z` by the white point is a simplified version of the same idea, applied to XYZ rather than to cone responses; color appearance models such as CIECAM02 and CAM16 use a proper cone-space adaptation transform and also take the absolute adapting luminance as a parameter.

**Global and local adaptation.** The pupil and the balance between rods and cones follow the light of the whole visual field. Retinal gain control is partly local, and the eyes move constantly between regions, so the adapted state also varies across the scene and over time. A dark region inside a bright image therefore does not switch vision to rods: it is seen by light-adapted cones, whose signal is weak and whose contrast against the surrounding adaptation level is low. Vision becomes mesopic only when the overall field is dim, as with a dark room, a low display brightness, and content that is mostly dark. Image appearance models such as iCAM build local adaptation into their processing for this reason.

**Displays.** An SDR display typically has a white of 80 to 300 cd/m² and a black of a few tenths of a cd/m² or less, before room reflections. With an 80 cd/m² white, CIELAB `L* = 30` corresponds to about 5 cd/m², the photopic boundary, and `L* = 10` to about 0.9 cd/m². Shadows in an image routinely fall below the photopic boundary in absolute terms, but whether the viewer is mesopic depends on the whole field, as described above. [Dark colors and luminance level](color-difference.md#dark-colors-and-luminance-level) explains what this means for color differences.

#### Contrast sensitivity

The contrast sensitivity function (CSF) gives the reciprocal of the smallest visible contrast of a sinusoidal grating as a function of spatial frequency in cycles per degree of visual angle (cpd). Its shape differs by channel:

- **Achromatic:** band-pass. Under photopic conditions sensitivity peaks at a few cycles per degree and falls steeply above roughly 10 cpd, reaching the resolution limit near 50 to 60 cpd. Lateral inhibition in center-surround receptive fields suppresses the lowest frequencies, and optical blur and the cone sampling mosaic limit the highest.
- **Chromatic:** low-pass, with a much lower cutoff. Mullen ("The contrast sensitivity of human colour vision to red-green and blue-yellow chromatic gratings", Journal of Physiology 359, 1985) measured isoluminant gratings whose sensitivity falls off several times earlier than luminance gratings; blue-yellow resolution is lower still.

Campbell and Robson ("Application of Fourier analysis to the visibility of gratings", Journal of Physiology 197, 1968) showed that the visibility of complex gratings follows from the responses of separate channels tuned to bands of spatial frequency, which is the physiological motivation for multiscale models. Mannos and Sakrison (1974) gave an early analytic luminance CSF used in image coding.

Two further effects matter for quantization. Sensitivity depends on adaptation level: at low luminance the CSF drops and its peak shifts to lower frequencies, and near black the visibility of a fixed luminance step changes with the display's black level and surround. Chromatic discrimination likewise declines as adaptation moves from photopic toward mesopic light, as described under [light level and adaptation](#light-level-and-adaptation). Contrast masking reduces the visibility of an error placed on a busy texture compared with the same error on a flat area, so identical error energy is more visible in skies and gradients than in foliage.

#### From pixels to cycles per degree

Spatial frequency on the retina depends on display pitch and viewing distance:

```text
pixels per degree = d · tan(1°) / p          d = viewing distance, p = pixel pitch (same unit)
Nyquist frequency  = pixels per degree / 2    cycles per degree
```

A 100 ppi display (`p = 0.254 mm`) viewed at 50 cm gives about 34 pixels per degree, so a one-pixel checkerboard is about 17 cpd. A 220 ppi display at 40 cm gives about 60 pixels per degree and 30 cpd. In the first case the finest luminance pattern is attenuated but still visible, and the finest chroma pattern is close to invisible. In the second, the finest pattern is near the luminance resolution limit. Terminal image viewing adds a complication: users often enlarge character cells, which lowers the pixels per degree and moves the same dither pattern into the visible band.

## Why multiscale structure matters

A metric evaluated at one scale implicitly assumes one viewing geometry. Because the CSF is defined in cycles per degree, changing the distance or the display pitch moves every image frequency along the CSF, so a pattern that is invisible at one distance is obvious at another. Neither libsixel nor its user can know the final geometry in general.

A multiscale metric hedges against this uncertainty. Each dyadic scale corresponds to a band of image frequencies; evaluating structure in every band and weighting the bands approximates integrating visibility over the CSF across a range of plausible viewing conditions, in the spirit of Campbell and Robson's channels. The fitted MS-SSIM weights favor the middle scales because those carried the most visible distortion in the calibration experiment.

For dithered images the scale decomposition has a direct interpretation. A ditherer trades fine-scale fidelity, where it places quantization noise, for coarse-scale fidelity, where the locally averaged tone should match the reference. Single-scale SSIM at full resolution sees mostly the noise and can rank a dither below a posterized image whose flat areas happen to correlate locally. MS-SSIM's coarser scales see through the noise to the reproduced tone and structure, and its finest scale still registers the noise, so the score reflects the trade the ditherer actually makes.

## Noise spectrum and why dither noise belongs at high frequencies

### Quantization error as a signal

Palette quantization replaces each pixel with a palette color, leaving an error image `e = y − x`. Whether the result looks good depends far more on the *spectrum* of `e` than on its total power. Ulichney's analysis of halftoning (*Digital Halftoning*, MIT Press, 1987, and "Dithering with blue noise", Proceedings of the IEEE 76(1), 1988) classified dither patterns by the radially averaged power spectrum of their error:

- **White noise** (independent random dither): flat spectrum. It places substantial energy at the low and middle frequencies where the eye is most sensitive, so it looks grainy and mottled.
- **Low-frequency (red or clustered) noise**: energy concentrated at low frequencies, seen as blotches, contours, and banding. Plain nearest-color quantization of a smooth gradient produces this.
- **Periodic patterns** (ordered dither, Bayer, "An optimum method for two-level rendition of continuous-tone pictures", 1973): energy at discrete spectral peaks. The pattern is regular and can produce visible texture or moiré with image content and resampling.
- **Blue noise**: little energy below a principal frequency and most energy above it, with an isotropic, aperiodic distribution. Ulichney identified this as the ideal for a pattern that must carry tone with minimal visibility.

### Why push the error upward

The visual system acts as a low-pass filter at normal viewing distance: the CSF attenuates high-frequency luminance contrast, and chromatic contrast even more so. If the quantization error has little energy at low frequencies, then after this filtering the perceived image is close to the reference: the local average tone and color are preserved, while the remaining error lies where sensitivity is lowest. Blue-noise placement minimizes the visible part of a fixed error budget, which a palette makes unavoidable.

Error diffusion achieves this by *noise shaping*. Floyd and Steinberg ("An adaptive algorithm for spatial greyscale", Proceedings of the Society for Information Display 17(2), 1976) propagate each pixel's quantization error to unprocessed neighbors with fixed weights. Modeling the quantizer as additive noise `n`, the output error is the noise filtered by `(1 − H)`, where `H` is the transfer function of the diffusion filter:

```text
E(ω) = (1 − H(ω)) · N(ω)
```

Because the weights sum to one, `H(0) = 1`, so the error response is zero at DC and grows with frequency: a high-pass filter applied to the quantization noise. Kite, Evans, and Bovik ("Modeling and quality assessment of halftoning by error diffusion", IEEE Transactions on Image Processing 9(5), 2000) extended this linear model to account for the sharpening that error diffusion also introduces and used it to build CSF-weighted quality measures for halftones.

Pushing noise upward has limits that the metrics must expose:

- **Viewing geometry.** Blue noise is invisible only while its frequencies lie beyond the viewer's sensitivity. Enlarging terminal cells or viewing closely brings it into view.
- **Directionality.** Fixed raster-order diffusion produces oriented "worm" textures and start-up artifacts in highlights and shadows, which are anisotropic and therefore more conspicuous than isotropic noise.
- **Low-frequency leakage.** Error clamping, limited palettes near the gamut boundary, and serpentine or band-parallel processing can leave low-frequency residue, seen as drift in tone, color casts, or seams.
- **Stream size.** SIXEL compresses runs of equal palette indices within six-pixel-high bands. High-frequency noise shortens those runs, so a more thoroughly shaped dither usually produces a larger stream. Quality and size are separate axes and should be reported together.

### Measuring the noise spectrum

`HighFreqRatio_delta` shows how much luma energy processing moved above its cutoff, so a well-shaped dither raises it while posterization lowers it. `StripeScore_rel` shows whether the added energy is oriented or periodic. `BandingIndex_*` shows low-frequency contouring that should have been broken up. Regional ΔE00 checks the complementary property: that the low-frequency content, the part the eye integrates, is correct. A dither can score well on noise placement and still be wrong on average color; both views are needed.

These scalar descriptors summarize the spectrum. The [dithering reference](../encoder/palette-application/dithering/README.md#measured-spatial-error-spectra) shows the full radially averaged and two-dimensional error spectra of every libsixel dither method on controlled flat fields, which is the direct view of the noise classes above.

### Structure-preserving dithering and luminance priority

The noise-shaping view treats the ditherer's goal as reproducing tone after low-pass filtering, with the residual pushed out of sight. A complementary line of work adds an explicit *structure* objective: the dithered image should also preserve edges, fine lines, and texture orientation of the original, as judged by a structural measure, rather than only its local averages. Because structure is perceived mainly through luminance, these methods in practice protect luminance structure first.

Several approaches follow this idea:

- **Edge-enhancing error diffusion.** Eschbach and Knox ("Error-diffusion algorithm with edge enhancement", Journal of the Optical Society of America A 8(12), 1991) modulate the quantization threshold by the input signal, which sharpens edges that ordinary error diffusion softens.
- **Structure-aware halftoning.** Pang, Qu, Wong, Cohen-Or, and Heng ("Structure-aware halftoning", ACM Transactions on Graphics 27(3), 2008) optimize the halftone against an objective that combines tone similarity after Gaussian filtering with an SSIM-style structural similarity. Chang, Alain, and Ostromoukhov ("Structure-aware error diffusion", ACM Transactions on Graphics 28(5), 2009) obtain a similar effect within error diffusion by adapting it to local image structure.
- **Luminance priority in color dithering.** With a color palette, several candidate colors can give nearly the same local average color. Choosing among them so that luminance varies as little as possible keeps the dither noise mostly chromatic, where the eye is least sensitive, and leaves luminance structure intact. The minimum brightness variation criterion of Shaked, Arad, Fitzhugh, and Sobel ("Color diffusion: error diffusion for color halftones", Proceedings of SPIE 3648, 1999) formalizes this for printer colorants. Kolpatzik and Bouman ("Optimized error diffusion for image display", Journal of Electronic Imaging 1(3), 1992) designed error-diffusion filters with separate luminance and chrominance visual models, so that luminance error is shaped more aggressively than chrominance error.

Structure preservation and blue-noise tone reproduction can conflict. Sharpening edges or aligning dots with structure adds energy that is neither isotropic nor confined to high frequencies, and choosing colors to minimize luminance variation can increase chroma noise and per-pixel ΔE00. The metrics in this document observe the trade-off from both sides: `MS-SSIM` and `GMSD` on luma reward preserved luminance structure, regional ΔE00 and `Δ Chroma_mean` guard the integrated color, and `StripeScore_rel` shows when structure-following produces oriented texture. libsixel's current dither policies, listed in the [dithering reference](../encoder/palette-application/dithering/README.md), do not include an explicit structural objective or a luminance-priority color choice; this subsection describes the idea and its evaluation, not an implemented mode.

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

**Physiology.** The achromatic channel has the highest spatial resolution and contrast sensitivity, as described under [contrast sensitivity](#contrast-sensitivity). Structure, edges, and texture are perceived mainly through luminance contrast; an isoluminant image looks blurry and its edges are hard to locate. Fine chromatic error is attenuated far more than fine luminance error, which is why image and video coding keep luma at full resolution while subsampling chroma, as in JPEG and 4:2:0 video, with little visible loss. A metric that spends its spatial analysis on luminance therefore concentrates on the channel where spatial errors are most visible. The same reasoning motivates [structure-preserving dithering that prioritizes luminance](#structure-preserving-dithering-and-luminance-priority).

**Lineage and validation.** SSIM, MS-SSIM, and GMSD were defined and validated on grayscale or luma images. Using them on luma keeps `lsqa` values interpretable against their published behavior, apart from the implementation differences documented above. Extending them to color has no single accepted definition, and an ad hoc per-channel average would weight RGB channels as though they were perceptually equivalent, which they are not.

**Separation of concerns.** Keeping spatial structure on luma and color fidelity on CIELAB lets a report say *which* property changed. A palette or lookup change that shifts hue without moving edges appears in ΔE00 and `Δ Chroma_mean` but not in MS-SSIM; a dither change that moves noise between scales appears in MS-SSIM and the spectral descriptors.

**Limits.** This split under-represents chromatic spatial artifacts at low frequencies, which are visible: color banding in gradients, chroma noise in large flat areas, and slow hue or neutral drift caused by palette geometry, such as gray areas drifting toward a tint. The luma metrics are blind to these. Regional ΔE00 and `Δ Chroma_mean` are the current safeguards, which is why the [measurement policy](measurement-policy.md#primary-and-secondary-metrics) prefers per-pixel color summaries for palette studies. Luma itself is gamma-encoded and non-constant-luminance, so saturated colors are weighted imperfectly; linear `Y` is available as a comparison signal when that matters. A spatial color metric in the S-CIELAB family, filtering opponent channels by their own contrast sensitivity before computing color difference, would close this gap more directly; it is not implemented.

## References

- Aguilar, M., and W. S. Stiles. "Saturation of the rod mechanism of the retina at high levels of stimulation." Optica Acta 1(1), 1954.
- Bayer, B. E. "An optimum method for two-level rendition of continuous-tone pictures." IEEE International Conference on Communications, 1973.
- Campbell, F. W., and J. G. Robson. "Application of Fourier analysis to the visibility of gratings." Journal of Physiology 197, 1968.
- Chang, J., B. Alain, and V. Ostromoukhov. "Structure-aware error diffusion." ACM Transactions on Graphics 28(5), 2009.
- CIE Publication 191. Recommended system for mesopic photometry based on visual performance, 2010.
- Eschbach, R., and K. T. Knox. "Error-diffusion algorithm with edge enhancement." Journal of the Optical Society of America A 8(12), 1991.
- Fairchild, M. D., and G. M. Johnson. "iCAM framework for image appearance, differences, and quality." Journal of Electronic Imaging 13(1), 2004.
- Floyd, R. W., and L. Steinberg. "An adaptive algorithm for spatial greyscale." Proceedings of the Society for Information Display 17(2), 1976.
- Hurvich, L. M., and D. Jameson. "An opponent-process theory of color vision." Psychological Review 64(6), 1957.
- ITU-R Recommendation BT.500. Methodologies for the subjective assessment of the quality of television images.
- ITU-R Recommendation BT.709. Parameter values for the HDTV standards for production and international programme exchange.
- Kite, T. D., B. L. Evans, and A. C. Bovik. "Modeling and quality assessment of halftoning by error diffusion." IEEE Transactions on Image Processing 9(5), 2000.
- Kolpatzik, B. W., and C. A. Bouman. "Optimized error diffusion for image display." Journal of Electronic Imaging 1(3), 1992.
- Luo, M. R., G. Cui, and B. Rigg. "The development of the CIE 2000 colour-difference formula: CIEDE2000." Color Research and Application 26(5), 2001.
- Mannos, J. L., and D. J. Sakrison. "The effects of a visual fidelity criterion on the encoding of images." IEEE Transactions on Information Theory 20(4), 1974.
- Mullen, K. T. "The contrast sensitivity of human colour vision to red-green and blue-yellow chromatic gratings." Journal of Physiology 359, 1985.
- Pang, W.-M., Y. Qu, T.-T. Wong, D. Cohen-Or, and P.-A. Heng. "Structure-aware halftoning." ACM Transactions on Graphics 27(3), 2008.
- Shaked, D., N. Arad, A. Fitzhugh, and I. Sobel. "Color diffusion: error diffusion for color halftones." Proceedings of SPIE 3648, 1999.
- Sharma, G., W. Wu, and E. N. Dalal. "The CIEDE2000 color-difference formula: implementation notes, supplementary test data, and mathematical observations." Color Research and Application 30(1), 2005.
- Ulichney, R. *Digital Halftoning*. MIT Press, 1987.
- Ulichney, R. "Dithering with blue noise." Proceedings of the IEEE 76(1), 1988.
- von Kries, J. "Chromatic adaptation." Festschrift der Albrecht-Ludwigs-Universität, Freiburg, 1902. English translation in D. L. MacAdam, ed., *Sources of Color Science*, MIT Press, 1970.
- Wang, Z., A. C. Bovik, H. R. Sheikh, and E. P. Simoncelli. "Image quality assessment: from error visibility to structural similarity." IEEE Transactions on Image Processing 13(4), 2004.
- Wang, Z., E. P. Simoncelli, and A. C. Bovik. "Multiscale structural similarity for image quality assessment." 37th Asilomar Conference on Signals, Systems and Computers, 2003.
- Xue, W., L. Zhang, X. Mou, and A. C. Bovik. "Gradient magnitude similarity deviation: a highly efficient perceptual image quality index." IEEE Transactions on Image Processing 23(2), 2014.
- Zhang, X., and B. A. Wandell. "A spatial extension of CIELAB for digital color image reproduction." SID International Symposium Digest of Technical Papers, 1996.
