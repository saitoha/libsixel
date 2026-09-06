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

Use secondary metrics to explain changes that MS-SSIM alone does not
characterize. Depending on the defect, useful measurements include:

- GMSD for gradient-structure distortion;
- PSNR-Y for luma error;
- high-frequency change for lost or introduced detail;
- banding measures for smooth gradients;
- chroma difference for palette or colorspace changes;
- Delta E for perceptual color error.

`lsqa` reports two complementary per-pixel color summaries:

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

A secondary metric is diagnostic unless its interpretation and threshold are
defined for the tested behavior. Do not select whichever metric makes a change
look best after seeing the result.

## Semantic assertions

Perceptual similarity cannot prove all image contracts. Pair quality metrics
with exact assertions for relevant properties, including:

- width and height;
- frame count, timing, and disposal behavior;
- alpha and transparent-pixel semantics;
- orientation and metadata precedence;
- palette size, indices, and retained palette state;
- background composition;
- deterministic output or stable decoded pixels;
- error handling for malformed or unsupported input.

Security and bounds tests must not be replaced by image-quality thresholds.

## Standard inputs

- For ordinary `lsqa` tests, use `images/snake.png` resized to approximately
  16 by 16 pixels. Retain enough structure for the measurement to be meaningful.
- For dithering-quality tests, use an approximately 64 by 64 pixel input.
- For alpha or transparency tests, prepare the required alpha content with
  ImageMagick and verify alpha semantics separately.
- Normally omit the color-count option (`-p`) so the 256-color default is
  exercised.
- Prefer small, checked-in fixtures over runtime fixture construction.
- Add a large fixture only when the test objective cannot be observed at the
  standard sizes.

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
See the [working-precision guide](../functionality/precision.md) for the pipeline
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
