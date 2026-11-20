# Measurement Record of the SIXEL Codec Tutorial

This page is for readers who want to check or reproduce the numbers in the [tutorial](README.md). It records the conditions and the timings of individual phases. It does not explain the techniques; each chapter does that. The tables are generated from [`measurements/results.json`](measurements/results.json).

## Conditions

<!-- measurements:begin:environment -->
| Item | Value |
| --- | --- |
| Date | 2026-11-20 |
| Based on revision | `751bd54c1f` |
| Machine | Apple M3 Max |
| System | Darwin 27.0.0 |
| Compiler | Apple clang version 21.0.0 (clang-2100.4.1.1) |
| Flags | `-O2 -Wall -Wextra` |
| Load average before, after | 2.57, 3.64 |
<!-- measurements:end -->

The inputs are two photographs from the [high-color study](../../encoder/output/high-color/README.md) with an adaptive 256-color palette and error diffusion, as `img2sixel` writes them, a synthetic two-color pattern, and uniformly random pixels. The pictures are decoded to indexes first, so no time is spent on palette construction or dithering. Every time is the fastest of repeated runs, in milliseconds, and differences of a few percent are noise. All of these numbers come from one machine; another processor can rank the techniques differently.

## The inner loop and the masks, one at a time

The second row is a copy of the full decoder patched to use masks for single characters, without the inner loop of [fewer questions](fewer-questions.md).

<!-- measurements:begin:attribution -->
| Decoder | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| full-decoder | 10.3 | 2.14 | 30.1 |
| full-decoder with masks for single characters | 5.44 | 2.04 | 23.3 |
| fewer-questions (inner loop, no masks) | 10.0 | 0.96 | 25.4 |
| masks (inner loop and masks) | 4.29 | 1.51 | 18.6 |
<!-- measurements:end -->

## Encoder phases

Classify sorts pixels into per-color rows, cut nodes finds each color's spans, and emit writes the stream. Each chapter that changes the encoder's work appears once.

<!-- measurements:begin:phases -->
| Chapter | Input | Classify | Cut nodes | Emit |
| --- | --- | --- | --- | --- |
| packing | Photograph, 1920×1080 | 0.72 | 14.2 | 6.80 |
| packing | Two colors, 1920×1080 | 0.71 | 0.20 | 1.36 |
| packing | Random noise, 1920×1080 | 1.66 | 58.3 | 90.5 |
| next-node | Photograph, 1920×1080 | 0.72 | 14.1 | 6.22 |
| next-node | Two colors, 1920×1080 | 0.71 | 0.23 | 1.29 |
| next-node | Random noise, 1920×1080 | 1.78 | 59.1 | 42.8 |
| occupancy | Photograph, 1920×1080 | 2.46 | 2.25 | 6.23 |
| occupancy | Two colors, 1920×1080 | 3.27 | 0.49 | 1.29 |
| occupancy | Random noise, 1920×1080 | 2.71 | 22.1 | 44.4 |
| direct-writes | Photograph, 1920×1080 | 2.41 | 2.21 | 4.16 |
| direct-writes | Two colors, 1920×1080 | 3.26 | 0.49 | 0.42 |
| direct-writes | Random noise, 1920×1080 | 2.63 | 21.7 | 36.5 |
| vector-lanes | Photograph, 1920×1080 | 2.22 | 2.25 | 4.23 |
| vector-lanes | Two colors, 1920×1080 | 0.86 | 0.50 | 0.44 |
| vector-lanes | Random noise, 1920×1080 | 7.61 | 21.7 | 37.1 |
<!-- measurements:end -->

## Ideas that did not pay off

The tutorial up to commit `e2299882a8` had three more encoder chapters between [next node](next-node.md) and [occupancy](occupancy.md): one that packed the six pixels of a column into a 64-bit column word before sorting, one that found all rows of one color in such a word with SWAR arithmetic, and one that visited each color of a column only once by comparing the word with shifted copies of itself. Each was meant to prepare the occupancy bits, but none made the encoder faster, so they were left out. The table gives the encode time in milliseconds of each idea combined with the occupancy bits, from prototypes measured on the same machine.

| Encoder | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| next-node | 20.9 | 2.22 | 102.0 |
| occupancy bits on the byte map, as in [occupancy](occupancy.md) | 10.5 | 5.00 | 67.2 |
| column words and occupancy bits | 11.2 | 5.30 | 66.7 |
| SWAR compares and occupancy bits | 14.3 | 3.50 | 67.4 |
| one visit per color of a column and occupancy bits | 11.8 | 3.87 | 68.7 |

The occupancy bits alone give the whole gain on the photograph. The column-word variants help only the two-color pattern, which the SIMD of [vector lanes](vector-lanes.md) handles better.

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
| Photograph, 1920×1080 | 0.06 | 0.53 |
| Two colors, 1920×1080 | 0.08 | 0.27 |
| Random noise, 1920×1080 | 0.36 | 2.32 |
<!-- measurements:end -->

## Thread scaling

The [parallel-decoder](parallel-decoder.md) program, whose encoder is the [parallel-bands](parallel-bands.md) chapter's, with `TUTORIAL_THREADS` set. Each cell is encode / decode time, with the speed-up over one thread in parentheses.

<!-- measurements:begin:threads -->
| Threads | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| 1 | 8.92 (1.0×) / 2.62 (1.0×) | 1.81 (1.0×) / 0.82 (1.0×) | 68.5 (1.0×) / 15.1 (1.0×) |
| 2 | 4.71 (1.9×) / 1.40 (1.9×) | 0.96 (1.9×) / 0.50 (1.6×) | 34.5 (2.0×) / 7.79 (1.9×) |
| 4 | 2.93 (3.0×) / 0.75 (3.5×) | 0.52 (3.5×) / 0.34 (2.4×) | 17.6 (3.9×) / 4.28 (3.5×) |
| 8 | 1.67 (5.3×) / 0.50 (5.2×) | 0.33 (5.5×) / 0.28 (2.9×) | 9.41 (7.3×) / 2.49 (6.1×) |
| 10 | 1.40 (6.4×) / 0.49 (5.3×) | 0.30 (6.0×) / 0.28 (2.9×) | 7.77 (8.8×) / 2.09 (7.2×) |
| 14 | 1.03 (8.7×) / 0.52 (5.0×) | 0.34 (5.3×) / 0.30 (2.7×) | 8.04 (8.5×) / 2.29 (6.6×) |
<!-- measurements:end -->

## Comparison with libsixel

The same indexed pictures go through the [transpose](transpose.md) program on one thread or the [parallel-decoder](parallel-decoder.md) program on 14 threads and through a libsixel build of the listed revision, using `sixel_decode_raw()` and `sixel_encode()` with the `fast` encoding policy. The libsixel calls do more than the tutorial functions: they keep coverage information, handle wide registers, transparency and output callbacks, and defend against hostile input. The comparison shows that the tutorial is a realistic model of the core loops, not that either program is better. libsixel breaks ties between equal nodes differently, so its streams are not byte-identical to the tutorial's.

<!-- measurements:begin:libsixel -->
| Input | Threads | Encode, tutorial | Encode, libsixel | Decode, tutorial | Decode, libsixel |
| --- | --- | --- | --- | --- | --- |
| Photograph, 1920×1080 | 1 | 8.61 | 7.47 | 2.67 | 2.79 |
| Photograph, 1920×1080 | 14 | 1.03 | 1.29 | 0.52 | 0.62 |
| Two colors, 1920×1080 | 1 | 1.72 | 3.04 | 0.81 | 0.91 |
| Two colors, 1920×1080 | 14 | 0.34 | 0.77 | 0.30 | 0.43 |
| Random noise, 1920×1080 | 1 | 65.4 | 51.8 | 15.0 | 15.4 |
| Random noise, 1920×1080 | 14 | 8.04 | 7.55 | 2.29 | 2.73 |
<!-- measurements:end -->

## Overpainting

The [next-node](next-node.md) and [overpaint](overpaint.md) programs run in turn on one thread, so they share the same conditions. The last column compares libsixel's `size` and `fast` policies on the same indexed pictures.

<!-- measurements:begin:overpaint -->
| Input | next-node bytes | overpaint bytes | Saved | Encode, ms | Decode, ms | Saved by libsixel `size` |
| --- | --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 328,750 | 302,133 | 8.1% | 3.49 → 3.52 | 0.78 → 0.78 | 8.1% |
| Photograph, 1920×1080 | 1,663,155 | 1,443,766 | 13.2% | 20.8 → 20.3 | 4.25 → 3.97 | 13.0% |
| Two colors, 1920×1080 | 692,322 | 347,802 | 49.8% | 2.22 → 1.58 | 1.49 → 0.81 | 48.9% |
| Random noise, 1920×1080 | 9,316,560 | 9,228,951 | 0.9% | 101.7 → 101.3 | 18.6 → 19.1 | 0.9% |
<!-- measurements:end -->

## Reproduction

Build libsixel with Meson in a separate directory if you want the comparison, then run the benchmarks and rewrite every table:

```sh
tools/update_sixel_codec_tutorial_measurements.py --measure \
    --libsixel-build BUILDDIR --libsixel-revision REVISION
tools/reproduce_sixel_codec_tutorial_figures.sh
```

Each `--measure` keeps the fastest value of every timing at the same revision, because other work on a machine only ever makes a run slower; pass `--fresh` to start over. `--only-overpaint` measures just the overpaint comparison and keeps every other result. Without `--measure`, the script only rewrites the tables from the results file, and `--check` verifies them. In the `code` directory, `make bench` prints just the timings behind the chapter tables.
