# Image Quality Measurement Policy

## Scope

This document defines how libsixel measures image quality and accepts or rejects
quality changes. It applies to encoding, quantization, palettes, dithering,
resizing, color management, transparency, animation, and decoder-side image
processing.

Image quality is a measured contract. Byte-correct output can still be a visual
regression.

## Measurement model

Use `lsqa` for reproducible comparison between a reference image and the
decoded result of produced output. Measure the complete behavior under review:

1. select a controlled reference input;
2. encode or transform it with the intended configuration;
3. decode the result through the relevant path;
4. normalize only dimensions, alpha background, or colorspace that the
   comparison explicitly requires;
5. compare the reference and output with `lsqa`;
6. record semantic assertions that a perceptual metric cannot prove.

Avoid comparing an in-memory intermediate with a final output when protocol
emission, decoding, palette state, or frame composition is part of the change.

## Primary and secondary metrics

Use MS-SSIM as the primary general-purpose perceptual gate. A normal target is:

```text
MS-SSIM:0.98
```

Never lower a quality threshold below `0.95` to make a regression pass.

`MS-SSIM` computes the reference five-scale product; its formula, minimum input dimensions and numerical tests are defined in [image-quality metrics](metrics.md#reference-ms-ssim). The incorrect historical arithmetic-average implementation is not a supported quality metric. Record the tool revision as well as the metric name with every baseline and benchmark. When migrating a historical gate, check valid and degraded controls and confirm that its threshold still rejects the intended defect. A historical score or policy ranking does not establish the corresponding reference-product result. Small fixtures must not be silently enlarged, assigned a reduced-scale score or allowed to pass with an unavailable result. Apply the [test-design decision](../testing/guide.md#image-quality-tests): use direct value assertions for a value contract, or redesign the fixture and its independent reference for a perceptual quality contract.

Use secondary metrics to explain changes that MS-SSIM alone does not
characterize. Depending on the defect, useful measurements include:

- GMSD for gradient-structure distortion;
- PSNR-Y for luma error;
- high-frequency change for lost or introduced detail;
- banding measures for smooth gradients;
- chroma difference for palette or colorspace changes;
- Delta E for perceptual color error.

See [color-aware evaluation](color-evaluation.md) for input interpretation, common XYZ D50 measurement, comparison signal controls, and regional color means. Wide-gamut PNG loading remains a separate integration gate.

`lsqa` reports complementary per-pixel color summaries:

- `Δ E00_mean` is the arithmetic mean of CIEDE2000 color difference after
  converting the reference and output pixels to CIELAB. It combines
  lightness, chroma, and hue terms; lower values indicate less color error.
- `Δ Chroma_mean` is the mean absolute difference between the CIELAB chroma
  magnitudes `C*ab = sqrt(a*^2 + b*^2)`. It isolates magnitude loss or gain in
  chroma but does not by itself measure hue or lightness error.

For palette-resolution studies, prefer these per-pixel summaries as the
primary explanatory curves. MS-SSIM spatially pools a luminance comparison and
can conceal the direct color error being investigated. Retain MS-SSIM as a
general perceptual gate, and use PSNR-Y when a separate luma error view is
required.

For sparse corruption, also inspect the maximum per-pixel color error and its location and tail counts, as defined in [color-aware evaluation](color-evaluation.md#per-pixel-and-regional-differences). Spatial and arithmetic means can hide isolated faults. In dither comparisons, inspect regional average colors alongside individual errors: intended high-frequency modulation is not automatically an artifact. A fixed palette has the same convex hull with or without error clamping; removing clamps may improve attainable spatial averages within that hull, but does not guarantee better local structure.

[Image-quality metrics](metrics.md) gives each metric's definition, origin, and implementation details, and explains how to combine them for dithered output; [visual perception for image quality](visual-perception/README.md) explains the physiology behind them.

A secondary metric is diagnostic unless its interpretation and threshold are
defined for the tested behavior. Do not select whichever metric makes a change
look best after seeing the result.

## Semantic assertions

Apply the [general assertion rules](../testing/guide.md#assertions-and-evidence) when interpreting image-quality measurements. Relevant image properties to check alongside the metrics include:

- width and height;
- frame count, timing, and disposal behavior;
- alpha and transparent-pixel semantics;
- orientation and metadata precedence;
- palette size, indices, and retained palette state;
- background composition;
- deterministic output or stable decoded pixels;
- error handling for malformed or unsupported input.

## Standard inputs

- MS-SSIM requires both aligned dimensions to be at least 176 pixels. Use `images/snake.png` at its original dimensions for general MS-SSIM quality gates, or a purpose-built fixture that meets the same minimum.
- Small loader and pixel-contract fixtures can remain approximately 16 by 16 pixels, and small dither fixtures approximately 64 by 64 pixels, when their assertions use sample values or an independently justified metric available at those dimensions.
- For alpha or transparency tests, prepare the required alpha content with
  ImageMagick and verify alpha semantics separately.
- Normally omit the color-count option (`-p`) so the 256-color default is
  exercised.
- Prefer small, checked-in fixtures over runtime fixture construction.
- Add a large fixture only when the test objective cannot be observed at the
  standard sizes.

The shared `tests/data/inputs/snake_192.png` quality fixture is generated from the original `images/snake.png` using `magick images/snake.png -filter Lanczos -resize '192x192!' -depth 8 -strip PNG24:tests/data/inputs/snake_192.png`. Its square sampling follows the small snake fixtures while retaining all five reference scales. Encoding tests compare their decoded output directly with this pre-encoding reference; they do not enlarge encoded pixels.

The resampling quality cases use a 240 by 240 source so their 80% and 120% targets have exact 192 by 192 and 288 by 288 dimensions. `python3 tests/data/inputs/generate_scaling_quality_fixtures.py` regenerates this source from the original photograph and each independent filter reference (ImageMagick for weighted kernels, documented floor-index selection for nearest). The same script creates the 192-pixel JPEG and 191-pixel independent resize reference used by SIMD integration cases. Absolute-height option tests retain small fixtures and assert dimensions directly.

Fixtures must have a documented purpose. Do not replace a reference image only
because a new output scores better against the replacement.

## Controlled comparisons

Compare like with like. Hold these variables constant unless one of them is the
subject of the experiment:

- effective thread count;
- loader and decode path;
- image dimensions and frame selection;
- requested precision, effective working format, and temporary format
  conversions;
- working and clustering colorspaces;
- palette size and palette source;
- alpha background;
- diffusion, lookup, and quantization policy;
- compiler optimization and relevant build options.

Run repeated measurements when nondeterminism is plausible. A quality gate must
not depend on a lucky palette seed, iteration order, or scheduling outcome.

### Precision and effective format

Treat encoder precision as a cross-cutting experimental variable rather than a
property of only one algorithm. It can change loader output, sampling and
binning, palette construction, dithering, palette lookup, and band encoding.
See the [working-precision guide](../encoder/precision/README.md) for the pipeline
boundaries and the maintained comparison.

Do not infer the effective path from the command line alone. Before collecting
a durable result, inspect the verbose palette-contract trace and require the
expected working format. A controlled 8-bit gamma run must report
`work=rgb888`; its float32 counterpart must report `work=rgb-f32`. Record this
preflight result in generated metadata so that a planner change cannot silently
change the experiment.

Distinguish three different facts in reports:

- the requested base precision selected by `--precision`;
- the effective shared working format used by the main encoder pipeline;
- a temporary float32 view created for an operation such as resizing or
  clustering.

A temporary conversion does not by itself make the whole pipeline a float32
pipeline. Conversely, a result is not an 8-bit-path measurement merely because
the input file stores 8-bit samples.

Gamma RGB clustering supports both 8-bit and float32 coordinates. A matched
`-Xgamma` comparison can therefore vary the clustering precision as well as the
surrounding base pipeline. Linear RGB and the Lab-family clustering spaces are
float32-only. There is no 8-bit Linear RGB, OKLab, CIELAB, or DIN99d clustering
mode: their matched `--precision=8bit|float32` rows keep clustering itself at
float32 and compare only the surrounding base pipeline. Label such figures as
base-precision comparisons, not as 8-bit-versus-float32 comparisons of those
color spaces.

When precision is not the independent variable, hold both the requested
precision and effective working format constant. When precision is the
independent variable, use adjacent matched pairs, alternate their execution
order, verify both effective formats before timing, and keep all other policy
choices fixed.

## Baseline policy

- Record the exact command, input fixture, build revision, relevant options,
  and before/after values.
- Understand the visual and algorithmic cause before replacing an expected
  image or changing a threshold.
- Prefer fixing the regression over weakening the gate.
- If a deliberate tradeoff is accepted, document the benefit, affected image
  classes, metric cost, and lower bound.
- Keep thresholds stable across platforms unless a platform-specific decode
  contract makes exact parity impossible and the variance is measured.

Never use a moving baseline generated by the code under test in the same test
run.

## Performance and quality

Treat performance and quality as separate axes. For performance comparisons:

- use stable inputs and a fixed configuration;
- report the build type, compiler, architecture, and effective concurrency;
- warm caches when that matches the intended workload;
- report multiple samples and a robust statistic such as the median;
- verify quality and semantic gates on the same implementation revision.

An optimization is not complete when it improves timing but violates a public,
semantic, safety, or quality contract.

## Review record

A non-trivial quality change should leave enough evidence for another developer
to reproduce it:

- tested revision and platform;
- commands and fixtures;
- before/after metric table;
- semantic test results;
- representative visual inspection when metrics are inconclusive;
- performance results when performance motivated the change;
- rationale for any accepted threshold or baseline update.
