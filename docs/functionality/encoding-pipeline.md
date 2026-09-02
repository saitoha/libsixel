# Encoding Pipeline

## Mental model

The normal `img2sixel` path is easiest to understand as four major stages:

```text
source bytes
    |
    v
loader -> normalized pixels -> palette construction -> palette application
                                                        |
                                                        v
                                              indexed pixels + palette
                                                        |
                                                        v
                                                  SIXEL encoding
```

Palette construction and palette application are different operations. The
former decides which colors are available. The latter decides which available
color represents each input pixel. SIXEL encoding then serializes those
decisions; it does not choose the colors again.

## Stage contracts

| Stage | Input | Output | Principal controls |
| --- | --- | --- | --- |
| Loading | Encoded image or caller pixels | Frames, pixels, dimensions, format, alpha, timing, and metadata | Loader choice and loader suboptions |
| Normalization | Loader frames | Pixels in the processing size, format, and colorspace | Resize, crop, alpha, and colorspace options |
| Palette construction | Normalized color samples | A bounded set of palette entries | `-p`, `-Q`, precision, and clustering colorspace |
| Palette application | Pixels and a completed palette | One palette index per output pixel | `-d`, `-~`, precision, transparency, and inter-frame policy |
| SIXEL encoding | Indexed pixels, palette, and frame metadata | SIXEL control sequences and image data | Encoding, 7-bit/8-bit, and output policies |

The loader contract ends when it has described the source image correctly.
Palette and dithering policy must therefore remain independent of the backend
that decoded PNG, JPEG, WebP, or another input format.

## Palette construction

Palette construction reduces the input color population to the number of
entries available to the encoder. `-Q MODEL` selects the solver and `-p COLORS`
sets the requested palette size. The result is a palette, not indexed image
data.

The model affects which errors the palette can represent well. For example, a
solver that minimizes aggregate error and one that limits the worst assignment
distance may choose different entries from the same input. See
[Palette Quantization](quantization.md).

## Palette application

After a palette exists, the encoder must map every non-transparent input pixel
to a palette index. Two independent policies cooperate in this stage:

- `-~ POLICY` or `--lookup-policy=POLICY` selects how a color is mapped to a
  palette entry;
- `-d METHOD` or `--diffusion=METHOD` selects how quantization error is shaped
  across space or time.

A useful conceptual loop is:

```text
for each pixel in the method's scan order:
    candidate = dither_adjust(source_pixel, position, carried_error)
    index = lookup(candidate, palette)
    error = candidate - palette[index]
    dither_update(error)
```

This is a model, not literal source code for every method. Ordered methods add
a position-dependent perturbation and may carry no error. `none` performs no
perturbation or propagation. Error-diffusion methods feed part of the error to
later pixels. In every case, lookup is the operation that returns the palette
index.

Consequently:

- `-d none` disables diffusion but still performs a palette lookup for every
  pixel;
- `--lookup-policy=none` disables lookup acceleration and scans the palette
  directly, but it does not disable dithering or palette application;
- changing `-Q` can change the palette itself, while changing `-d` or `-~`
  changes how that completed palette is applied.

See [Dithering](dithering.md) and [Lookup Policy](lookup-policy.md) for the two
parts of this stage.

## SIXEL encoding

Palette application produces indexed pixels and a palette. The core encoder
turns them into palette definitions, color selections, six-pixel-high bands,
runs, carriage movement, and the surrounding DCS sequence. Encoding policy may
change the ordering and size of the byte stream without changing the preceding
conceptual ownership of palette construction and application.

The implementation may pipeline, parallelize, or fuse work across these
boundaries. Those execution details must preserve the stage contracts above.

## Alternate paths

Not every invocation constructs a new palette:

- monochrome, built-in, and map-file modes provide a palette;
- caller-facing APIs may provide a prebuilt dither/palette object;
- high-color mode redefines palette registers while encoding instead of using
  one fixed palette for the entire image;
- animations may retain inter-frame state or reconstruct policy state for each
  frame.

The supplied-palette paths skip or replace palette construction, but they still
need palette application before ordinary indexed SIXEL encoding. High-color is
a specialized path and should not be used as the model for fixed-palette
encoding.

## Diagnosing changes

Measure the stage that a change actually affects:

- compare palette entries or palette-build telemetry for `-Q` changes;
- compare indexed assignments, lookup preparation, and per-pixel mapping for
  `-~` changes;
- compare spatial or temporal artifacts and perceptual image metrics for `-d`
  changes;
- compare output size and encoding time for core encoding changes.

End-to-end quality remains important, but it cannot by itself identify which
stage caused a regression. Follow the project-wide
[Quality Measurement Policy](../quality/measurement-policy.md) when comparing
visual output.

## Implementation and tests

The main implementation boundaries are represented by
[`loader.c`](../../src/loader.c), [`palette.c`](../../src/palette.c),
[`dither.c`](../../src/dither.c),
[`lookup-policy.c`](../../src/lookup-policy.c), and
[`encoder-core-encode.c`](../../src/encoder-core-encode.c). Palette and lookup
coverage lives primarily under [`tests/quant/palette/`](../../tests/quant/palette/),
with CLI option regressions under
[`tests/cli/options/`](../../tests/cli/options/).
