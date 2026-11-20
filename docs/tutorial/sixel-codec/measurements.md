# Measurement Record of the SIXEL Codec Tutorial

This page is for readers who want to check or reproduce the numbers in the [tutorial](README.md). It records the conditions and the timings of individual phases. It does not explain the techniques; each chapter does that. The tables are generated from [`measurements/results.json`](measurements/results.json).

## Conditions

<!-- measurements:begin:environment -->
| Item | Value |
| --- | --- |
| Date | 2026-11-20 |
| Based on revision | `141c71ac03` |
| Machine | Apple M3 Max |
| System | Darwin 27.0.0 |
| Compiler | Apple clang version 21.0.0 (clang-2100.4.1.1) |
| Flags | `-O2 -Wall -Wextra` |
| Load average before, after | 6.11, 7.15 |
<!-- measurements:end -->

The inputs are two photographs from the [high-color study](../../encoder/output/high-color/README.md) with an adaptive 256-color palette and error diffusion, as `img2sixel` writes them, a synthetic two-color pattern, and uniformly random pixels. The pictures are decoded to indexes first, so no time is spent on palette construction or dithering. Every time is the fastest of repeated runs, in milliseconds, and differences of a few percent are noise. All of these numbers come from one machine; another processor can rank the techniques differently.

## The inner loop and the masks, one at a time

The second row is a copy of the full decoder patched to use masks for single characters, without the inner loop of [fewer questions](fewer-questions.md).

<!-- measurements:begin:attribution -->
| Decoder | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| full-decoder | 11.7 | 1.78 | 32.9 |
| full-decoder with masks for single characters | 6.17 | 2.01 | 26.6 |
| fewer-questions (inner loop, no masks) | 10.7 | 1.17 | 25.8 |
| masks (inner loop and masks) | 4.20 | 1.48 | 18.7 |
<!-- measurements:end -->

## Encoder phases

Classify sorts pixels into per-color rows, cut nodes finds each color's spans, and emit writes the stream. Each chapter that changes the encoder's work appears once.

<!-- measurements:begin:phases -->
| Chapter | Input | Classify | Cut nodes | Emit |
| --- | --- | --- | --- | --- |
| packing | Photograph, 1920×1080 | 0.73 | 14.3 | 6.95 |
| packing | Two colors, 1920×1080 | 0.71 | 0.21 | 1.37 |
| packing | Random noise, 1920×1080 | 1.72 | 58.0 | 90.7 |
| next-node | Photograph, 1920×1080 | 0.73 | 14.5 | 6.29 |
| next-node | Two colors, 1920×1080 | 0.71 | 0.23 | 1.30 |
| next-node | Random noise, 1920×1080 | 1.72 | 59.3 | 41.8 |
| column-words | Photograph, 1920×1080 | 1.29 | 14.6 | 6.36 |
| column-words | Two colors, 1920×1080 | 1.29 | 0.23 | 1.29 |
| column-words | Random noise, 1920×1080 | 1.53 | 60.2 | 43.2 |
| swar | Photograph, 1920×1080 | 5.59 | 14.4 | 6.29 |
| swar | Two colors, 1920×1080 | 1.66 | 0.24 | 1.29 |
| swar | Random noise, 1920×1080 | 2.39 | 60.0 | 42.1 |
| first-colors | Photograph, 1920×1080 | 3.19 | 14.4 | 6.09 |
| first-colors | Two colors, 1920×1080 | 1.94 | 0.24 | 1.29 |
| first-colors | Random noise, 1920×1080 | 3.59 | 58.7 | 42.1 |
| occupancy | Photograph, 1920×1080 | 3.30 | 2.17 | 6.11 |
| occupancy | Two colors, 1920×1080 | 2.11 | 0.50 | 1.29 |
| occupancy | Random noise, 1920×1080 | 4.06 | 21.6 | 42.7 |
| vector-lanes | Photograph, 1920×1080 | 2.17 | 2.21 | 6.56 |
| vector-lanes | Two colors, 1920×1080 | 0.86 | 0.50 | 1.29 |
| vector-lanes | Random noise, 1920×1080 | 7.66 | 21.5 | 44.2 |
<!-- measurements:end -->

## From column words to rows

The time of the pass from column words back to rows, without SIMD in the [word decoder](word-decoder.md) and with it in [transpose](transpose.md).

<!-- measurements:begin:materialize -->
| Decoder | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| word-decoder | 0.57 | 0.58 | 0.58 |
| transpose | 0.17 | 0.17 | 0.17 |
<!-- measurements:end -->

## Prescan of the parallel decoder

The serial prescan against the whole decode at the default thread count.

<!-- measurements:begin:prescan -->
| Input | Prescan | Whole decode |
| --- | --- | --- |
| Photograph, 1920×1080 | 0.07 | 0.51 |
| Two colors, 1920×1080 | 0.12 | 0.33 |
| Random noise, 1920×1080 | 0.36 | 2.36 |
<!-- measurements:end -->

## Thread scaling

The [parallel-decoder](parallel-decoder.md) program, whose encoder is the [parallel-bands](parallel-bands.md) chapter's, with `TUTORIAL_THREADS` set. Each cell is encode / decode time, with the speed-up over one thread in parentheses.

<!-- measurements:begin:threads -->
| Threads | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| 1 | 11.2 (1.0×) / 2.69 (1.0×) | 2.67 (1.0×) / 0.81 (1.0×) | 74.6 (1.0×) / 15.1 (1.0×) |
| 2 | 6.51 (1.7×) / 1.40 (1.9×) | 1.52 (1.8×) / 0.53 (1.5×) | 40.6 (1.8×) / 8.19 (1.8×) |
| 4 | 3.63 (3.1×) / 0.82 (3.3×) | 0.85 (3.1×) / 0.37 (2.2×) | 20.6 (3.6×) / 4.30 (3.5×) |
| 8 | 2.64 (4.2×) / 0.53 (5.1×) | 0.55 (4.9×) / 0.34 (2.4×) | 13.4 (5.5×) / 2.54 (6.0×) |
| 10 | 2.13 (5.2×) / 0.43 (6.3×) | 0.47 (5.7×) / 0.32 (2.5×) | 11.1 (6.7×) / 2.16 (7.0×) |
| 14 | 1.51 (7.4×) / 0.50 (5.4×) | 0.48 (5.6×) / 0.35 (2.3×) | 9.38 (7.9×) / 2.34 (6.5×) |
<!-- measurements:end -->

## Comparison with libsixel

The same indexed pictures go through the [transpose](transpose.md) program on one thread or the [parallel-decoder](parallel-decoder.md) program on 14 threads and through a libsixel build of the listed revision, using `sixel_decode_raw()` and `sixel_encode()` with the `fast` encoding policy. The libsixel calls do more than the tutorial functions: they keep coverage information, handle wide registers, transparency and output callbacks, and defend against hostile input. The comparison shows that the tutorial is a realistic model of the core loops, not that either program is better. libsixel breaks ties between equal nodes differently, so its streams are not byte-identical to the tutorial's.

<!-- measurements:begin:libsixel -->
| Input | Threads | Encode, tutorial | Encode, libsixel | Decode, tutorial | Decode, libsixel |
| --- | --- | --- | --- | --- | --- |
| Photograph, 1920×1080 | 1 | 10.9 | 7.46 | 2.71 | 2.81 |
| Photograph, 1920×1080 | 14 | 1.51 | 1.41 | 0.50 | 0.70 |
| Two colors, 1920×1080 | 1 | 2.75 | 3.05 | 0.72 | 0.91 |
| Two colors, 1920×1080 | 14 | 0.48 | 0.68 | 0.35 | 0.42 |
| Random noise, 1920×1080 | 1 | 73.8 | 51.7 | 15.2 | 15.5 |
| Random noise, 1920×1080 | 14 | 9.38 | 7.41 | 2.34 | 2.77 |
<!-- measurements:end -->

## Overpainting

The [next-node](next-node.md) and [overpaint](overpaint.md) programs run in turn on one thread, so they share the same conditions. The last column compares libsixel's `size` and `fast` policies on the same indexed pictures.

<!-- measurements:begin:overpaint -->
| Input | next-node bytes | overpaint bytes | Saved | Encode, ms | Decode, ms | Saved by libsixel `size` |
| --- | --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 328,750 | 302,133 | 8.1% | 3.61 → 3.57 | 0.78 → 0.78 | 8.1% |
| Photograph, 1920×1080 | 1,663,155 | 1,443,766 | 13.2% | 21.1 → 20.4 | 4.20 → 3.88 | 13.0% |
| Two colors, 1920×1080 | 692,322 | 347,802 | 49.8% | 2.22 → 1.58 | 1.47 → 0.79 | 48.9% |
| Random noise, 1920×1080 | 9,316,560 | 9,228,951 | 0.9% | 102.2 → 101.7 | 18.5 → 19.1 | 0.9% |
<!-- measurements:end -->

## Reproduction

Build libsixel with Meson in a separate directory if you want the comparison, then run the benchmarks and rewrite every table:

```sh
tools/update_sixel_codec_tutorial_measurements.py --measure \
    --libsixel-build BUILDDIR --libsixel-revision REVISION
tools/reproduce_sixel_codec_tutorial_figures.sh
```

Each `--measure` keeps the fastest value of every timing at the same revision, because other work on a machine only ever makes a run slower; pass `--fresh` to start over. `--only-overpaint` measures just the overpaint comparison and keeps every other result. Without `--measure`, the script only rewrites the tables from the results file, and `--check` verifies them. In the `code` directory, `make bench` prints just the timings behind the chapter tables.
