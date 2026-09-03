# Lookup-Policy Quality Comparison

## Question

The historical RGB555 path reduces each byte-valued color axis from 256 values
to 32 histogram and cache addresses. Does increasing the requested palette size
`K` overcome that loss, or does the five-bit representation remain a quality
limit?

This page answers that narrow question with a checked-in measurement. It is not
a universal ranking of lookup policies.

## Result

![Mean Delta E00 and mean chroma error for six lookup policies over palette sizes from 8 to 256](measurements/lookup-policy-color-error.png)

Lower values are better on both panels. The source data are available as
[`lookup-policy-color-error.csv`](measurements/lookup-policy-color-error.csv).

At `K = 256`, the measured values are:

| Policy | Mean Delta E00 | Mean absolute chroma error |
| --- | ---: | ---: |
| `none` | 2.447771 | 1.842988 |
| `5bit` | 3.155706 | 2.269776 |
| `6bit` | 2.596776 | 1.859324 |
| `certlut` | 2.403138 | 1.749753 |
| `eytzinger` | 3.032645 | 2.461793 |
| `vptree` | 2.400774 | 1.748358 |

On this fixture, `5bit` has 0.707935 more mean Delta E00 than the
full-histogram `none` control at `K = 256`, a 28.9 percent increase. Its mean
chroma error is 0.426788 higher, a 23.2 percent increase. More palette entries
do not recover distinctions already collapsed into the `32^3` Heckbert
histogram.

The curves also demonstrate why exactness labels must not be read as a total
quality ranking. Some accelerated policies score slightly better than `none`
at some values of `K`, while `eytzinger` has a larger chroma error at high `K`
on this image. Palette construction, approximate application, and the global
distribution of pixel errors all influence these averages.

## Perceptual reference

![MS-SSIM for the same six lookup policies and palette sizes](measurements/lookup-policy-ms-ssim.png)

MS-SSIM is spatially pooled and is less direct evidence of the color-space
binning mechanism, so it is a reference rather than the primary figure. It
nevertheless shows that the high-`K` color-error gap is perceptually relevant:
at `K = 256`, `5bit` reaches 0.973219 while `none` reaches 0.986088. The
underlying values are in
[`lookup-policy-ms-ssim.csv`](measurements/lookup-policy-ms-ssim.csv).

## Measurement design

The checked-in curve was measured on 2026-09-03 from a clean Autotools build of
revision `5a825023b` on Darwin 25.5.0 arm64. The input was
[`images/snake.png`](../../../images/snake.png), and the palette sizes were
`8, 16, 32, 64, 128, 256`.

Every series used:

```text
img2sixel \
  --threads=1 \
  --precision=8bit \
  --quality=full \
  --quantize-model=heckbert:cover=off:merge=none \
  --diffusion=none \
  --lookup-policy=POLICY \
  -p K \
  images/snake.png
```

`lsqa` then compared the original image with the SIXEL stream and reported its
existing `Δ E00_mean`, `Δ Chroma_mean`, and MS-SSIM metrics. Diffusion is
disabled so error propagation does not obscure the palette-construction and
lookup effect. One worker exercises the serial lazy-cache behavior of `5bit`
and `6bit`. Cover and final merge are disabled to keep the palette solver
profile narrow and stable.

This is an **end-to-end policy comparison**, not an isolated nearest-neighbor
microbenchmark. In the current RGB Heckbert path, `5bit` selects a `32^3`
histogram, `6bit` and the other accelerated policies select a `64^3` histogram,
and `none` selects a `256^3` histogram. The policy can therefore change both
the palette and the way it is applied. That historical coupling is exactly the
user-visible limitation under test.

## Reproducing the curve

Build `img2sixel` and `lsqa`, install Python Matplotlib, then run:

```sh
tools/plot_lookup_policy_quality.sh \
  docs/functionality/lookup-policies/measurements \
  images/snake.png
```

The wrapper uses
[`tools/plot_quality_curve.py`](../../../tools/plot_quality_curve.py) for
command execution, `lsqa` parsing, CSV output, and plotting. Override `PYTHON`,
`IMG2SIXEL_PATH`, or `LSQA_PATH` when the binaries or interpreter are outside
the source tree. Passing another image as the second argument preserves the
same policy and `K` sweep.

The CSV stores the full command template for every point. A rerun should be
treated as comparable only when the revision, input, loader behavior, build
options, and metric implementation are also recorded.

## Interpretation limits

- One natural image cannot characterize every color distribution. Add
  gradients, saturated synthetic colors, alpha cases, and a small image set
  before using the result to change a default.
- Mean Delta E00 includes lightness, chroma, and hue contributions but hides
  worst-case pixels. Mean chroma error omits hue and lightness.
- The test measures decoded image quality, not encoding time, memory use,
  output size, or palette-index stability. Those require separate curves.
- A fixed-palette experiment is required to isolate the palette-application
  algorithm from the Heckbert histogram-resolution coupling.

Return to the [Lookup Policy index](../lookup-policy.md).
