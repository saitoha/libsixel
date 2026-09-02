# Palette Quantization

## Purpose

Palette quantization constructs a small set of colors that represents a much
larger input color population. In the encoding pipeline it runs after loading
and normalization and before palette application:

```text
normalized pixels -> quantization -> palette entries
```

It answers **which colors are available to the encoder**. It does not assign a
palette index to every pixel; dithering and lookup perform that later step.

## CLI surface

`img2sixel` selects the palette solver with:

```text
-Q MODEL
--quantize-model=MODEL
```

`MODEL` may include model-specific suboptions. Use `img2sixel -H` or the
[`img2sixel(1)` manual](../../converters/img2sixel.1) for the current accepted
suboptions. `-p COLORS` sets the requested palette size and normally defaults
to 256 colors.

The base models are:

| Model | Primary objective and behavior |
| --- | --- |
| `auto` | Preserves the historical Heckbert median-cut selection |
| `heckbert` | Recursively splits color boxes and chooses representatives |
| `kmeans` | Iteratively places centroids to reduce aggregate assignment error |
| `medoids` | Chooses centers from observed samples and minimizes assignment cost |
| `center` | Keeps centers on sampled points and minimizes maximum assignment radius |

Model-specific initialization, sampling, iteration, merge, cover, and polishing
settings change how a solver reaches or repairs its result. They remain part of
palette construction even when their names refer to assignment or lookup
inside the solver.

## Relationship to colorspaces and precision

Distance has meaning only in a particular representation. Clustering
colorspace and arithmetic precision can therefore alter the palette produced
from the same decoded pixels. Output palette conversion is a later concern and
must not be confused with the space in which the solver compares samples.

Tests for a quantizer change should state the colorspace, precision, palette
size, seed, and other non-default inputs that make the result reproducible.
When exact palette bytes are not the contract, assess the decoded result with
the [Quality Measurement Policy](../quality/measurement-policy.md).

## Bypassing construction

Monochrome, built-in palette, and palette-map modes supply palette entries
instead of deriving them from the source image. They therefore bypass or
replace normal `-Q` processing. The supplied palette is still applied to each
pixel by the dithering and lookup stage.

High-color mode is different again: it can redefine palette registers during
encoding and is not a fixed-palette quantizer model.

## Design rules

- Keep the quantizer responsible for palette entries and build telemetry, not
  final indexed pixels or SIXEL byte ordering.
- Keep accepted model and suboption names synchronized through the option
  registry rather than duplicating parser-only lists.
- Make fallback behavior explicit. The palette dispatcher currently falls
  back to the Heckbert path when a requested newer solver cannot complete.
- Evaluate both aggregate quality and failure cases such as rare saturated
  colors; one metric does not describe every palette defect.
- Separate palette-build cost from palette-application cost in benchmarks.

## Implementation and tests

Palette orchestration is in [`palette.c`](../../src/palette.c). The individual
models are implemented by [`palette-heckbert.c`](../../src/palette-heckbert.c),
[`palette-kmeans.c`](../../src/palette-kmeans.c),
[`palette-kmedoids.c`](../../src/palette-kmedoids.c), and
[`palette-kcenter.c`](../../src/palette-kcenter.c). Focused solver tests and CLI
quality cases are under [`tests/quant/palette/`](../../tests/quant/palette/).

For the surrounding data flow, start with
[Encoding Pipeline](encoding-pipeline.md).
