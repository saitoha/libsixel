# Color Difference and Color Reproduction

## Scope

This document explains how color differences are quantified, which measures can evaluate the color reproduction of a whole image at global, regional, and multiple spatial scales, why palette-limited SIXEL output is prone to color distortion and global color shifts, and which color-difference grades are reasonable targets when the goal is output that does not look wrong.

[Image-quality metrics](metrics.md) defines every metric `lsqa` reports and explains the visual physiology behind them. [Color-aware quality evaluation](color-evaluation.md) owns how `lsqa` interprets inputs and computes color in XYZ D50. The [measurement policy](measurement-policy.md) owns thresholds and gates; the grades below are guidance for designing and reading experiments, not acceptance limits.

## What a color difference is

A color difference is a number intended to grow with the perceived difference between two colors. It is meaningful only in a space where equal distances are perceived as roughly equal steps. RGB is not such a space: the same numeric step is a large visible change in dark blues and a negligible one in bright yellows. CIE colorimetry therefore converts colors to a *uniform color space* before measuring distance.

Any color difference can be decomposed into three perceptual components, which are often more informative than the total:

- **ΔL**: lightness difference (darker or lighter);
- **ΔC**: chroma difference (more or less colorful);
- **ΔH**: hue difference (shifted toward a neighboring hue).

`ΔH` is defined so that `ΔE² = ΔL² + ΔC² + ΔH²` in CIELAB. A palette that loses saturation shows up mostly in `ΔC`; a gray that drifts toward green shows up in `ΔH` and `Δb*` or `Δa*`; a dithering bias in gamma space shows up in `ΔL`. Reporting the components tells which mechanism failed, which a single ΔE cannot.

## Color-difference formulas

| Formula | Space | Origin | Notes |
| --- | --- | --- | --- |
| ΔE\*ab (CIE76) | CIELAB | CIE, 1976 | Euclidean distance. Simple, but overstates differences at high chroma and is non-uniform in blue and near neutral. |
| ΔE\*94 (CIE94) | CIELAB | CIE, 1995 | Weights chroma and hue differences down as chroma grows. |
| ΔE CMC(l:c) | CIELAB | Colour Measurement Committee of the Society of Dyers and Colourists, 1984 | Textile-industry tolerance formula with lightness and chroma weights, commonly 2:1. |
| ΔE00 (CIEDE2000) | CIELAB | CIE, 2001 | Adds a lightness weight, a hue-dependent term, and a blue-region rotation term; the current CIE recommendation for small differences. Defined in [image-quality metrics](metrics.md#ciede2000). |
| ΔE99 (DIN99, DIN99d) | DIN99 | DIN 6176 | Euclidean distance after a transformation that makes CIELAB more uniform. |
| ΔEok | OKLab | Ottosson, 2020 | Euclidean distance in OKLab, designed for uniform hue and lightness in image processing; not fitted to small-difference tolerance data. |
| ΔE ITP | ICtCp | ITU-R BT.2124, 2019 | Intended for HDR and wide-gamut video; one unit is about a just-noticeable difference for that viewing condition. |

`lsqa` reports CIEDE2000 with unit parametric factors. The same pair of colors yields different numbers under different formulas: ΔE00 is usually smaller than ΔE\*ab for chromatic differences, because it compresses the chroma axis, and comparable for small differences near neutral. A tolerance stated in one formula cannot be transferred to another by a fixed factor. Mahy, Van Eycken, and Oosterlinck ("Evaluation of uniform color spaces developed after the adoption of CIELAB and CIELUV", Color Research and Application 19(2), 1994) estimated a just-noticeable difference of about 2.3 ΔE\*ab; ΔE00 is commonly read with a just-noticeable difference near 1 for uniform patches. Both figures are averages for controlled side-by-side viewing, not thresholds for images.

All of these formulas were fitted to judgments of **uniform patches** of moderate size, viewed side by side under standard illumination. Images differ in every respect: colors vary spatially, the eye integrates fine detail, and the viewer usually has no reference to compare against. That is why the evaluation of images needs the spatial measures below and why the grades later in this document depend on scale and viewing situation.

## Dark colors and luminance level

Cones, which carry color vision, work best at photopic light levels. As light decreases, color discrimination weakens until rods, which do not distinguish hue, take over. It is therefore reasonable to ask whether a color difference discounts chroma and hue differences in shadows. CIELAB and CIEDE2000 do so partly and only in relative terms; they do not model the absolute light level.

### What the formulas already discount

**Cube-root compression in CIELAB.** CIELAB computes its opponent axes as differences of compressed, white-normalized tristimulus values:

```text
L* = 116 · f(Y/Yn) − 16
a* = 500 · (f(X/Xn) − f(Y/Yn))
b* = 200 · (f(Y/Yn) − f(Z/Zn))
f(t) = t^(1/3)                 t > 0.008856
f(t) = 7.787 · t + 16/116      t ≤ 0.008856 (near black)
```

Scaling a color's light by `k` without changing its chromaticity scales `a*` and `b*` by about `k^(1/3)`, so chroma `C*` falls roughly in proportion to lightness `L*`. Near black, where `f` is linear, chroma falls in proportion to the light itself. For sRGB red with a D65 white, full intensity gives `L* ≈ 53` and `C* ≈ 105`; one eighth of the light gives `L* ≈ 19` and `C* ≈ 49`; one sixty-fourth gives `L* ≈ 3` and `C* ≈ 14`. A chromaticity error of fixed size therefore produces a smaller `Δa*b*` in a dark color than in a light one. The hue component `ΔH = 2 √(C₁ C₂) · sin(Δh / 2)` is proportional to chroma, so a hue-angle error in a dark, low-chroma color also yields a small `ΔH`. This is a proportional, Weber-like discount built into the space.

**The lightness weight in CIEDE2000.** `S_L = 1 + 0.015 (L̄' − 50)² / √(20 + (L̄' − 50)²)` divides the lightness difference. It is 1 at `L* = 50` and grows toward both ends: 1.45 at `L* = 20`, 1.60 at `L* = 10`, and 1.67 at `L* = 5`. A given `ΔL*` near black therefore counts about 40% less than the same step at mid lightness. The weight reflects visual data collected against a mid-gray surround, where steps close to the background lightness are most visible (crispening).

CIEDE2000 has no lightness-dependent weight on chroma or hue. `S_C` depends on mean chroma and `S_H` on mean chroma and hue; any discount of dark chromatic differences comes only from the CIELAB compression above.

### What the formulas do not model

**Absolute luminance.** CIELAB is relative colorimetry: every value is normalized by the white `Yn`, so a display at 80 cd/m² and one at 500 cd/m² produce identical numbers. Effects that depend on the absolute level are absent. Colorfulness increases with luminance (the Hunt effect), perceived contrast increases with luminance (the Stevens effect), and chromatic discrimination declines through the mesopic range, which CIE 191:2010 places between about 0.005 and 5 cd/m². Color appearance models such as CIECAM02 and CAM16 include the adapting luminance explicitly; CAM16 computes colorfulness as `M = C · F_L^0.25`, where `F_L` grows with the adapting luminance. Distances in CAM16-UCS (Li et al., 2017) therefore respond to the absolute viewing level, which CIEDE2000 cannot.

**Display black and flare.** A real display does not show zero light for black. Its own black level and reflected room light, called veiling glare or flare, add a roughly constant luminance to every pixel. That addition dilutes chromaticity differences between dark colors, much as diluting two pigments with the same gray makes them more alike. A color difference computed directly from encoded pixel values ignores it and so overstates differences near black. The sRGB specification (IEC 61966-2-1) includes viewing flare in its reference viewing conditions for this reason.

**Range of the calibration data.** CIEDE2000 was fitted mainly to surface colors of moderate lightness viewed under bright illumination. Its behavior close to black is less thoroughly validated than its behavior at mid lightness.

### Local darkness is not dark adaptation

Whether rods take over depends on the adaptation state of the visual system, which follows the overall light in the field of view, not on one dark region. A shadow inside a normally lit image is still seen by light-adapted cones. Its colors are harder to distinguish because the cone signal is weak and its contrast against the adapted level is small, not because vision has become mesopic. CIELAB's proportional compression is a reasonable approximation of that effect.

The situation changes when the whole display is dim: a low-brightness setting, a dark room, or a dark-themed terminal whose content is mostly near black. With a white of 80 cd/m², `L* = 30` is about 5 cd/m², `L* = 10` about 0.9 cd/m², and `L* = 5` about 0.4 cd/m². If most of the field is that dark, adaptation moves toward the mesopic range, and chromatic differences there are discounted more strongly by vision than by CIELAB.

### Consequences for `lsqa`

`lsqa` computes CIELAB relative to the ICC D50 white with no flare term and no absolute luminance. Dark chromatic differences are discounted only through CIELAB compression, and differences between near-black colors are somewhat overstated compared with what a viewer sees on a real display. The compression is nevertheless strong enough that a palette whose darkest entry is slightly above black usually produces only a small ΔE00, even where the luma-based MS-SSIM penalizes the same area heavily; see the near-black example in [image-quality metrics](metrics.md#the-lsqa-ms-ssim).

Two extensions would model dark viewing more faithfully. Neither is implemented:

- **Flare simulation.** Add the same black level, a small fraction of white, to the XYZ of both images before converting to CIELAB, and to the luma comparison signal before structural metrics. This models a real display's black directly and would also soften the near-black sensitivity of SSIM's luminance term, for which the constant `C1` currently acts as a very small floor.
- **CAM16-UCS color difference.** Report a color difference in a color appearance space parameterized by the display luminance and surround, so that absolute viewing level enters the result.

Either extension requires the [measurement policy](measurement-policy.md) to fix the assumed viewing conditions, since results would depend on them.

## Evaluating color reproduction across scales

A color reproduction can be wrong at different spatial scales, and each needs a different measure.

### Per-pixel differences

Per-pixel ΔE00 compares each output pixel with the corresponding reference pixel. `lsqa` reports the mean (`Δ E00_mean`), the maximum (`DE00_max`), the maximum's location, and counts above 1, 5, 10, 20, and 50.

For dithered output, the per-pixel difference mostly measures the dither pattern, which is intended and largely invisible at normal viewing distance. The mean is a measure of noise energy in color terms; the maximum and tail counts are useful for sparse faults such as isolated black or white pixels. Per-pixel values are not a measure of whether the image *looks* the right color.

### Regional differences

Regional ΔE00 averages physical XYZ inside fixed tiles before computing the color difference, which models the optical integration of fine detail. `lsqa` reports the area-weighted mean and the maximum over tiles (`DE00_region_mean`, `DE00_region_max`) for one window size of 8, 16, 32, or 64 pixels; see [regional differences](color-evaluation.md#per-pixel-and-regional-differences).

The window must be chosen with the viewing geometry in mind. A tile larger than the dither period averages the dither away and leaves the reproduced color. A tile much larger than an object mixes the object with its surroundings and can hide a local color error. One window size therefore answers one question.

### Global differences

Global measures summarize color over the whole image. They detect the distortions that a viewer notices without any reference: a color cast, a loss of colorfulness, or a shift of skin or neutral tones.

- **Mean-color difference.** Average XYZ over the whole image for both images, convert the two means to CIELAB, and compute ΔE00. This is the regional measure with one tile covering the image. It detects an overall cast; it is blind to errors that cancel, such as a red region turning orange while an orange region turns red.
- **Signed component bias.** The mean of `Δa*` and `Δb*` over pixels or tiles is a color-cast vector; the mean of `ΔL*` is a lightness bias. Unlike ΔE, signed means show the direction of a shift. Restricting them to near-neutral reference pixels (low `C*`) measures *neutral drift*, the tendency of grays to acquire a tint, which viewers detect readily.
- **Chroma ratio.** The ratio of mean `C*` in the output to that of the reference, or the mean of `ΔC*`, measures saturation loss or gain. `Δ Chroma_mean` reports the mean absolute chroma difference.
- **Hue-sector statistics.** Grouping pixels by reference hue angle and reporting mean `ΔH` and `ΔC` per sector shows whether specific hues, typically saturated primaries or skin tones, are distorted.
- **Distribution distance.** The Earth Mover's Distance between color histograms in CIELAB (Rubner, Tomasi, and Guibas, "The Earth Mover's Distance as a metric for image retrieval", International Journal of Computer Vision 40(2), 2000) measures how much color mass must move, and how far, to turn one color distribution into the other. It ignores spatial position, which makes it a measure of palette and gamut usage rather than of local fidelity.
- **Gamut and reachability.** Whether the reference colors lie within the convex hull of the palette determines whether any dither can reproduce their local averages. The [palette cover policy](../encoder/palette-construction/cover-policy/README.md#three-layers-of-reachability) defines layered reachability measures for this.

### Multiscale evaluation

A single regional window or the whole-image mean each answer the question at one scale. Multiscale measures evaluate several scales at once, for the same reason MS-SSIM does for structure ([why multiscale structure matters](metrics.md#why-multiscale-structure-matters)): the viewing geometry is unknown.

- **Regional pyramid.** Evaluating regional ΔE00 at 8, 16, 32, and 64 pixels and the whole-image mean gives a color-difference profile across scales. A good ditherer shows large per-pixel and 8-pixel values that fall quickly with tile size; a palette with a color shift shows a floor that does not fall, because the error survives integration. `lsqa` supports this by running it once per window size.
- **Gaussian pyramid color difference.** A smoother variant low-pass filters XYZ with a Gaussian and subsamples by two per level, then computes per-pixel ΔE00 at each level. Gaussian filtering avoids the tile-alignment dependence of fixed tiles. This is not implemented in `lsqa`.
- **S-CIELAB.** Zhang and Wandell ("A spatial extension of CIELAB for digital color image reproduction", SID 1996) transform the image into an opponent color space, filter the luminance, red-green, and blue-yellow channels with separate kernels derived from their contrast sensitivity at a stated viewing distance in samples per degree, convert back, and compute ΔE per pixel. Its filters are stronger for the chromatic channels, matching the lower chromatic acuity described in [contrast sensitivity](metrics.md#contrast-sensitivity). It needs the viewing geometry as an input, and evaluating it at several distances gives a physiologically motivated multiscale profile. This is not implemented in `lsqa`.
- **Image-difference and appearance models.** The improved color image difference metric (Preiss, Fernandes, and Urban, "Color-image quality assessment: from prediction to optimization", IEEE Transactions on Image Processing 23(3), 2014) combines lightness, chroma, and hue comparison terms in the style of SSIM after a viewing-condition-dependent normalization. Image appearance models such as iCAM (Fairchild and Johnson, "iCAM framework for image appearance, differences, and quality", Journal of Electronic Imaging 13(1), 2004) model adaptation and spatial filtering before computing differences. These are research-grade predictors with more parameters to validate; they are not implemented in `lsqa`.

Until a multiscale color metric is implemented, the regional pyramid together with signed global biases is the practical multiscale view. Fix the window set and alignment before measuring and use them for every variant in a study.

## Why palette-limited images distort color

A SIXEL image carries at most a few hundred palette colors per frame and often far fewer. Every reference color must be represented by a single palette entry or, through dithering, by a spatial mixture of entries. Several mechanisms produce color distortion, and they become stronger as the palette shrinks.

**Allocation follows pixel count, not visual importance.** Median cut and k-means place palette entries where the most pixels are, because they minimize total error weighted by area. A small but visually important region, such as a face, a saturated logo, or a small sign, receives no entry of its own and is approximated by entries allocated to its surroundings. Its hue moves toward theirs. The [cover policy](../encoder/palette-construction/cover-policy/README.md#why-palette-coverage-matters) explains why saturated primaries and faces fail first.

**Dithering cannot leave the palette's convex hull.** Error diffusion reproduces local averages only for colors inside the convex hull of the palette. A color outside it is reproduced at the nearest reachable mixture, which is less saturated or of a different hue. The error is not noise but a systematic shift that survives any spatial averaging, which is exactly what regional and global measures detect and per-pixel means understate. Even inside the hull, error diffusion may not converge to the right mean near the hull boundary or with clamped error; see the [cover policy](../encoder/palette-construction/cover-policy/README.md#convex-containment-is-necessary-not-sufficient) and the [dither clamp policy](../encoder/palette-application/clamp-policy.md).

**Representative colors can be biased.** A palette entry stands for a cluster of colors. If it is chosen as the center of a bounding box rather than the mean of its members, or if a sparse tail of the cluster pulls it away from the dense core, the whole cluster is rendered with an offset. In neutral regions this appears as a gray that turns slightly colored, a shift that is small in ΔE but conspicuous because viewers expect neutrals to be neutral. The [palette gradient analysis](palette-gradient-analysis.md) workflow inspects such cases through Voronoi slices and pixel profiles.

**The clustering and working spaces matter.** Clustering in a non-uniform space spends entries unevenly, typically too few in dark and blue regions where RGB steps are perceptually large. Dithering in gamma-encoded RGB averages encoded values rather than light, so a spatial mixture of two entries is perceived lighter or darker than the intended mean. See the [clustering color space](../encoder/palette-construction/clustering-colorspace/README.md) and the [palette working color space](../encoder/palette-application/working-colorspace/README.md).

**Lookup and merging add their own error.** Approximate nearest-color lookup can choose a slightly wrong entry systematically within a region, and final palette merging can collapse distinct colors; see the [lookup policy](../encoder/palette-application/lookup-policy/README.md) and the [final palette merge policy](../encoder/palette-construction/merge-policy/README.md).

The net effect is that a palette-limited image can have an acceptable per-pixel error budget and still show a global color shift: correct texture and structure, wrong overall color. These mechanisms strengthen as the palette shrinks, and with 16 or fewer colors many photographs cannot be reproduced without some visible global difference.

## Tolerance grades

### Industrial grades

Color industries grade tolerances for uniform samples by ΔE\*ab. Nippon Denshoku, a color-measurement instrument manufacturer, publishes one widely used example ([Allowance by color](https://www.nippondenshoku.co.jp/web/english/colorstory/08_allowance_by_color.htm)):

| ΔE\*ab | Grade | Meaning for side-by-side uniform samples |
| --- | --- | --- |
| 0 – 0.2 | — | Below the limit of discrimination. |
| 0.2 – 0.4 | AAA | Distinguishable only by trained observers; the reproducibility limit of instruments. |
| 0.4 – 0.8 | AA | The tightest tolerance that can be set by visual judgment. |
| 0.8 – 1.6 | A | Noticeable when the samples are placed side by side. |
| 1.6 – 3.2 | B | Generally regarded as the same color when compared at a distance or in sequence. |
| 3.2 – 6.5 | C | Regarded as the same color in general use; can cause complaints in paint and ink work. |
| 6.5 – 13.0 | D | About the spacing between adjacent JIS standard color chips. |
| 13.0 – 25.0 | — | Distinctly different colors. |

The older NBS unit scale describes differences as trace (0 – 0.5), slight (0.5 – 1.5), noticeable (1.5 – 3.0), appreciable (3.0 – 6.0), much (6.0 – 12.0), and very much (above 12). Both scales express the same idea: below about 1.5 a difference needs a direct comparison, around 3 it is noticeable but tolerable, and above about 6 it is a different color.

These grades are stated in ΔE\*ab for uniform samples. When applying them to `lsqa`'s ΔE00 results, remember that ΔE00 is generally smaller than ΔE\*ab for saturated colors. A ΔE00 value at a grade boundary therefore corresponds to an equal or larger ΔE\*ab, so reading ΔE00 against these ΔE\*ab boundaries is somewhat lenient for chromatic colors and roughly equivalent near neutral.

### Viewing situation

The grades assume a *side-by-side* comparison, the strictest situation. Images are seldom viewed that way:

- **Side-by-side comparison** (the original next to the output, as in a review tool): the industrial grades apply most directly.
- **Successive or remembered comparison** (switching between versions, or recognizing a familiar photograph): tolerance is roughly a grade looser, because color memory is less precise than direct comparison.
- **Viewing without a reference** (the normal case for a terminal image): the viewer cannot measure a difference and judges only plausibility. Differences in colors with strong expectations remain noticeable: neutral grays and whites, skin tones, sky, foliage, and brand or interface colors. Other colors tolerate shifts of a grade or more.

### Recommended targets for libsixel output

For "no perceptual discomfort" in normal viewing, the target depends on the spatial scale of the difference, because the eye integrates fine detail and judges large areas:

| Scale | Measure | Target grade | Rationale |
| --- | --- | --- | --- |
| Whole image | Mean-color difference; signed bias | **B** (ΔE00 within about 2) | A global cast is visible without any reference; B is "regarded as the same color". |
| Neutral and memory colors | Neutral drift, regional ΔE00 restricted to near-neutral or skin regions | **A to B** (ΔE00 about 1 to 2) | Viewers notice tinted grays and unnatural skin first. |
| Large regions (tiles of 32 – 64 pixels) | `DE00_region_mean` | **B to C** | Large areas are judged as colors; C remains acceptable in general use. |
| Small regions (tiles of 8 – 16 pixels) | `DE00_region_mean`, `DE00_region_max` | **C**, with the maximum below **D** | Small areas near the dither scale integrate imperfectly; no region should become a distinctly different color (13 and above). |
| Individual pixels | `Δ E00_mean`, `DE00_max` | no grade | Dithering intentionally creates large per-pixel differences; use tails and locations to find faults. |

The ΔE00 values in the table are set below the corresponding ΔE\*ab grade boundaries to offset the lenience of reading ΔE00 against ΔE\*ab grades for chromatic colors.

The grades tighten as the scale grows, because a large-area color is judged as a color and compared against expectation, while small-scale differences are partly integrated away. With small palettes (16 colors or fewer), the small-region targets are generally unattainable for photographs; prioritize the global and neutral targets, which preserve the impression of correct color, over local accuracy.

These targets are for interpreting experiments and choosing between alternatives. They become test thresholds only when the [measurement policy](measurement-policy.md) or a topic's documented error budget adopts them, with the fixtures and conditions under which they were validated. ΔE00 grades on natural images have not yet been validated against viewer judgments for libsixel output; treat them as engineering targets derived from uniform-sample tolerances and the viewing considerations above.

## References

- CIE Publication 15. Colorimetry. Commission Internationale de l'Éclairage.
- CIE Publication 142. Improvement to industrial colour-difference evaluation (CIEDE2000), 2001.
- CIE Publication 159. A colour appearance model for colour management systems: CIECAM02, 2004.
- CIE Publication 191. Recommended system for mesopic photometry based on visual performance, 2010.
- Fairchild, M. D., and G. M. Johnson. "iCAM framework for image appearance, differences, and quality." Journal of Electronic Imaging 13(1), 2004.
- Hunt, R. W. G. "Light and dark adaptation and the perception of color." Journal of the Optical Society of America 42(3), 1952.
- IEC 61966-2-1. Multimedia systems and equipment: colour measurement and management, part 2-1: default RGB colour space, sRGB, 1999.
- ITU-R Recommendation BT.2124. Objective metric for the assessment of the potential visibility of colour differences in television, 2019.
- Li, C., Z. Li, Z. Wang, Y. Xu, M. R. Luo, G. Cui, M. Melgosa, M. H. Brill, and M. Pointer. "Comprehensive color solutions: CAM16, CAT16, and CAM16-UCS." Color Research and Application 42(6), 2017.
- Luo, M. R., G. Cui, and B. Rigg. "The development of the CIE 2000 colour-difference formula: CIEDE2000." Color Research and Application 26(5), 2001.
- Mahy, M., L. Van Eycken, and A. Oosterlinck. "Evaluation of uniform color spaces developed after the adoption of CIELAB and CIELUV." Color Research and Application 19(2), 1994.
- Nippon Denshoku Industries. "Allowance by color." Color story, <https://www.nippondenshoku.co.jp/web/english/colorstory/08_allowance_by_color.htm>.
- Ottosson, B. "A perceptual color space for image processing." 2020, <https://bottosson.github.io/posts/oklab/>.
- Preiss, J., F. Fernandes, and P. Urban. "Color-image quality assessment: from prediction to optimization." IEEE Transactions on Image Processing 23(3), 2014.
- Rubner, Y., C. Tomasi, and L. J. Guibas. "The Earth Mover's Distance as a metric for image retrieval." International Journal of Computer Vision 40(2), 2000.
- Sharma, G., W. Wu, and E. N. Dalal. "The CIEDE2000 color-difference formula: implementation notes, supplementary test data, and mathematical observations." Color Research and Application 30(1), 2005.
- Stevens, J. C., and S. S. Stevens. "Brightness function: effects of adaptation." Journal of the Optical Society of America 53(3), 1963.
- Zhang, X., and B. A. Wandell. "A spatial extension of CIELAB for digital color image reproduction." SID International Symposium Digest of Technical Papers, 1996.
