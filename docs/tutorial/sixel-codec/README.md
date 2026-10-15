# Writing a SIXEL Codec, Step by Step

This tutorial builds a SIXEL encoder and decoder five times, each version adding one technique to the one before. It starts with a complete codec of about thirty lines per direction and ends with the techniques used in libsixel's own encoder and decoder: per-color nodes, six pixels packed into one 64-bit word, SIMD, and threads. Every step is a small, standalone C program that does not use libsixel, so the code can be read, built and changed without the rest of the repository.

The steps are:

| Step | Idea | Encoder | Decoder |
| --- | --- | --- | --- |
| [1. Run-length coding](step1-run-length.md) | SIXEL is simple | One pass over the band per color | One store per painted pixel |
| [2. Nodes](step2-nodes.md) | kmiya's encoder | One pass per band; color spans packed onto few lines | Full body syntax; repeats filled a row at a time |
| [3. 64-bit words](step3-column-words.md) | A column is a word | Column words, SWAR, bitmaps of occupied columns | One masked word store per sixel character |
| [4. SIMD](step4-simd.md) | Sixteen columns at once | Six vector compares per color and block | Columns back to rows with a byte transpose |
| [5. Threads](step5-threads.md) | Bands are independent | Bands encoded in parallel | Prescan for band starts, then bands in parallel |

Steps 2 to 5 write byte-identical SIXEL streams, and decoders 2 to 5 return identical images for any stream; the step 1 decoder agrees with them on the streams these encoders write. Each later step is therefore a pure optimization of the one before it, and the test script checks that it is.

## Before you start

The tutorial assumes the wire format described in the [SIXEL format overview](../../sixel-format/README.md): the DCS envelope, sixel data characters, and the body control functions `!`, `"`, `#`, `$` and `-`. Its sections on the [rendering model](../../sixel-format/README.md#rendering-model) and [ordinary band encoding](../../sixel-format/README.md#ordinary-band-encoding) are the background for steps 1 and 2.

The examples need a C99 compiler and POSIX threads. They have been checked with Apple clang on AArch64 (NEON) and x86-64 (SSE2).

## The common ground

All steps work on the same indexed image and implement the same two functions, declared in [`code/tutorial.h`](code/tutorial.h):

```c
typedef struct image {
    int width;
    int height;
    int ncolors;                    /* registers in use, 1 to 256 */
    unsigned char *pixels;          /* width * height palette indexes */
    unsigned char palette[256 * 3]; /* 8-bit RGB triplets */
} image_t;

int tutorial_encode(image_t const *img, buffer_t *out);
int tutorial_decode(unsigned char const *data, size_t size, image_t *img);
```

Palette construction and dithering are separate subjects, covered from the [palette construction pipeline](../../encoder/palette-construction/pipeline/README.md) onward. The harness in [`code/common.c`](code/common.c) keeps them out of the way: it reads a PPM file and uses its colors directly when there are at most 256, or maps it to a 6×6×6 color cube with ordered dithering otherwise. It also writes decoded images as PPM, generates synthetic test images, and times both directions.

Every encoder writes the same envelope: `ESC P q`, raster attributes `"1;1;W;H` with square pixels, one `#n;2;r;g;b` definition per register in percent, the body, and `ESC \`. Every decoder reads indexes, so decoded pixels can be compared exactly regardless of how palette percentages round.

## Building and checking

The `code` directory contains a POSIX [makefile](code/Makefile):

```sh
cd docs/tutorial/sixel-codec/code
make            # build step1 ... step5
make check      # self tests and cross-step comparisons
make bench      # time every step on the inputs of the table below
```

Each program has the same commands:

```sh
./step2 encode ../../../../images/snake.ppm snake.six
./step2 decode snake.six snake.ppm
./step2 selftest
./step2 bench ../../../../images/snake.ppm 4 10
```

[`code/check.sh`](code/check.sh) is the safety net for every optimization in this tutorial. It round-trips 50 synthetic images of awkward sizes through every step, requires the encoders of steps 2 to 5 to produce identical bytes for several thread counts, and requires decoders 2 to 5 to agree on the sample streams in `images/` and on hand-written streams that omit raster attributes, paint beyond them, use HLS colors, or redefine a register after painting with it. A round trip alone is not enough: an encoder can produce a larger but still valid stream after a mistake, and only the byte comparison notices.

## Measured results

The table gives the time to encode and to decode one image, in milliseconds, as measured by `make bench`. The inputs are `images/snake.ppm` (600×450, mapped to 216 colors with ordered dithering), the same photograph enlarged four times (2400×1800), a synthetic 2400×1800 two-color pattern, and 2400×1800 uniformly random pixels over 256 colors, the worst case for every step.

<!-- measurements:begin -->
| Step | Photograph, 600×450 | Photograph, 2400×1800 | Two colors, 2400×1800 | Random noise, 2400×1800 |
| --- | --- | --- | --- | --- |
| 1 | 21.0 / 2.42 | 319.8 / 33.0 | 3.52 / 4.61 | 592.9 / 373.7 |
| 2 | 1.21 / 1.12 | 15.6 / 12.4 | 4.84 / 3.77 | 339.1 / 71.3 |
| 3 | 1.20 / 0.71 | 15.3 / 7.95 | 8.27 / 4.24 | 150.7 / 56.2 |
| 4 | 1.04 / 0.60 | 12.7 / 7.04 | 5.76 / 3.44 | 161.8 / 55.2 |
| 5 | 1.05 / 0.32 | 2.18 / 1.98 | 1.04 / 0.63 | 25.0 / 49.8 |
<!-- measurements:end -->

Each cell is encode / decode time in milliseconds. Stream sizes are listed on the step pages; steps 2 to 5 write identical streams.

The machine was an Apple M3 Max (10 performance and 4 efficiency cores) running macOS 27 with Apple clang 21 at `-O2`, with little other load. `make bench` reports the fastest of 10 repetitions for each cell, and the table keeps the faster of two such runs; differences of a few percent are noise. Step 5 uses up to one thread per CPU but no more than the input justifies: the small photograph encodes on one thread and decodes on three.

Two results stand out. First, the largest single improvement is algorithmic: step 2 changes the encoder's cost from proportional to colors × pixels to proportional to pixels. Second, the lower-level techniques do not speed up every input equally. Step 3 speeds up node composition and most decoding but slows classification on some inputs, and step 4 shows that SIMD pays when neighboring pixels repeat colors and costs extra when they do not. Threads then multiply whatever the single-threaded code achieves, as long as the serial part stays small. Each step page explains its own numbers.

## From the tutorial to libsixel

The tutorial codecs are deliberately small. libsixel's [encoder](../../encoder/pipeline/README.md) and [decoder](../../decoder/pipeline/README.md) handle transparency and key colors, OR mode, high-color and 6delta output, the size-oriented [encoding policy](../../encoder/output/encode-policy/README.md), streaming output with an ordered writer, palette history during decoding, and the many [robustness](../../sixel-format/README.md#robustness-and-security) concerns of untrusted input. The page for each step points to the corresponding part of the libsixel source and documentation, where the same idea appears with those concerns included.
