# Measurement Record of the SIXEL Codec Tutorial

This page is for readers who want to check or reproduce the numbers in the [tutorial](README.md). It records the conditions and the timings of individual phases. It does not explain the techniques; each chapter does that. The tables are generated from [`measurements/results.json`](measurements/results.json).

## Conditions

<!-- measurements:begin:environment -->
| Item | Value |
| --- | --- |
| Date | 2026-11-27 |
| Based on revision | `133adefe4a` |
| Machine | Apple M3 Max |
| System | Darwin 27.0.0 |
| Compiler | Apple clang version 21.0.0 (clang-2100.4.1.1) |
| Flags | `-O2 -Wall -Wextra` |
| Load average before, after | 3.85, 3.98 |
<!-- measurements:end -->

The inputs are two photographs from the [high-color study](../../encoder/output/high-color/README.md) with an adaptive 256-color palette and error diffusion, as `img2sixel` writes them, a synthetic two-color pattern, and uniformly random pixels. The pictures are decoded to indexes first, so no time is spent on palette construction or dithering. Every time is the fastest of repeated runs, in milliseconds, and differences of a few percent are noise. All of these numbers come from one machine; another processor can rank the techniques differently.

## The inner loop and the masks, one at a time

The second row is a copy of the full decoder patched to use masks for single characters, without the inner loop of [fewer questions](fewer-questions.md).

<!-- measurements:begin:attribution -->
| Decoder | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| full-decoder | 10.4 | 2.19 | 30.5 |
| full-decoder with masks for single characters | 5.61 | 2.08 | 24.0 |
| fewer-questions (inner loop, no masks) | 9.93 | 0.94 | 25.6 |
| masks (inner loop and masks) | 4.30 | 1.50 | 18.9 |
<!-- measurements:end -->

## Encoder phases

Classify sorts pixels into per-color rows, cut nodes finds each color's spans, and emit writes the stream. Each chapter that changes the encoder's work appears once.

<!-- measurements:begin:phases -->
| Chapter | Input | Classify | Cut nodes | Emit |
| --- | --- | --- | --- | --- |
| packing | Photograph, 1920×1080 | 0.74 | 14.6 | 7.15 |
| packing | Two colors, 1920×1080 | 0.72 | 0.23 | 1.42 |
| packing | Random noise, 1920×1080 | 1.77 | 60.4 | 94.8 |
| next-node | Photograph, 1920×1080 | 0.74 | 14.6 | 6.38 |
| next-node | Two colors, 1920×1080 | 0.73 | 0.23 | 1.32 |
| next-node | Random noise, 1920×1080 | 1.79 | 59.6 | 43.1 |
| occupancy | Photograph, 1920×1080 | 2.45 | 2.25 | 6.33 |
| occupancy | Two colors, 1920×1080 | 3.32 | 0.51 | 1.32 |
| occupancy | Random noise, 1920×1080 | 2.76 | 22.1 | 44.2 |
| direct-writes | Photograph, 1920×1080 | 2.48 | 2.28 | 4.27 |
| direct-writes | Two colors, 1920×1080 | 3.23 | 0.50 | 0.42 |
| direct-writes | Random noise, 1920×1080 | 2.72 | 22.0 | 37.3 |
| vector-lanes | Photograph, 1920×1080 | 2.26 | 2.28 | 4.29 |
| vector-lanes | Two colors, 1920×1080 | 0.88 | 0.51 | 0.46 |
| vector-lanes | Random noise, 1920×1080 | 7.90 | 21.7 | 37.7 |
| wide-stores | Photograph, 1920×1080 | 2.24 | 2.22 | 2.89 |
| wide-stores | Two colors, 1920×1080 | 0.90 | 0.51 | 0.07 |
| wide-stores | Random noise, 1920×1080 | 7.78 | 22.3 | 33.9 |
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

The parallel encoder writes each thread's bands into a buffer of its own and then copies the buffers into the output in order. A prototype that first measured the length of every band, added the lengths up and then let every thread write straight into its place in the output saved nothing: the Full HD photograph encoded in 1.50 to 1.52 ms instead of 1.52 to 1.55 ms, and random noise in about 10.1 ms both ways, because the copy takes about 0.03 ms, so it was left out.

## From column words to rows

The time of the pass from column words back to rows, without SIMD in the [word decoder](word-decoder.md) and with it in [transpose](transpose.md).

<!-- measurements:begin:materialize -->
| Decoder | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| word-decoder | 0.58 | 0.59 | 0.58 |
| transpose | 0.17 | 0.17 | 0.17 |
<!-- measurements:end -->

## Prescan of the parallel decoder

The serial prescan against the whole decode at the default thread count.

<!-- measurements:begin:prescan -->
| Input | Prescan | Whole decode |
| --- | --- | --- |
| Photograph, 1920×1080 | 0.07 | 0.49 |
| Two colors, 1920×1080 | 0.09 | 0.29 |
| Random noise, 1920×1080 | 0.36 | 2.18 |
<!-- measurements:end -->

## Thread scaling

The [thread-pool](thread-pool.md) program, whose encoder and decoder are those of the [parallel-bands](parallel-bands.md) and [parallel-decoder](parallel-decoder.md) chapters with persistent threads, with `TUTORIAL_THREADS` set. Each cell is encode / decode time, with the speed-up over one thread in parentheses.

<!-- measurements:begin:threads -->
| Threads | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| 1 | 7.52 (1.0×) / 2.76 (1.0×) | 1.47 (1.0×) / 0.63 (1.0×) | 65.2 (1.0×) / 13.8 (1.0×) |
| 2 | 3.92 (1.9×) / 1.47 (1.9×) | 0.79 (1.9×) / 0.39 (1.6×) | 33.7 (1.9×) / 7.22 (1.9×) |
| 4 | 2.53 (3.0×) / 0.87 (3.2×) | 0.41 (3.6×) / 0.27 (2.3×) | 17.5 (3.7×) / 3.98 (3.5×) |
| 8 | 1.36 (5.5×) / 0.51 (5.4×) | 0.24 (6.1×) / 0.22 (2.9×) | 9.30 (7.0×) / 2.28 (6.0×) |
| 10 | 1.18 (6.4×) / 0.50 (5.5×) | 0.29 (5.1×) / 0.24 (2.6×) | 8.55 (7.6×) / 2.50 (5.5×) |
| 14 | 1.10 (6.8×) / 0.50 (5.5×) | 0.27 (5.4×) / 0.28 (2.2×) | 7.87 (8.3×) / 2.41 (5.7×) |
<!-- measurements:end -->

## Comparison with libsixel

The same indexed pictures go through the [wide-stores](wide-stores.md) program on one thread or the [thread-pool](thread-pool.md) program on 14 threads and through a libsixel build of the listed revision, using `sixel_decode_raw()` and `sixel_encode()` with the `fast` encoding policy. The libsixel calls do more than the tutorial functions: they keep coverage information, handle wide registers, transparency and output callbacks, and defend against hostile input. The comparison shows that the tutorial is a realistic model of the core loops, not that either program is better. libsixel breaks ties between equal nodes differently, so its streams are not byte-identical to the tutorial's.

<!-- measurements:begin:libsixel -->
| Input | Threads | Encode, tutorial | Encode, libsixel | Decode, tutorial | Decode, libsixel |
| --- | --- | --- | --- | --- | --- |
| Photograph, 1920×1080 | 1 | 7.25 | 7.58 | 2.84 | 2.82 |
| Photograph, 1920×1080 | 14 | 1.10 | 1.40 | 0.50 | 0.65 |
| Two colors, 1920×1080 | 1 | 1.43 | 3.27 | 0.55 | 0.89 |
| Two colors, 1920×1080 | 14 | 0.27 | 0.90 | 0.28 | 0.41 |
| Random noise, 1920×1080 | 1 | 63.3 | 52.2 | 13.5 | 15.7 |
| Random noise, 1920×1080 | 14 | 7.87 | 8.08 | 2.41 | 2.85 |
<!-- measurements:end -->

## Overpainting

The [next-node](next-node.md) and [overpaint](overpaint.md) programs run in turn on one thread, so they share the same conditions. The last column compares libsixel's `size` and `fast` policies on the same indexed pictures.

<!-- measurements:begin:overpaint -->
| Input | next-node bytes | overpaint bytes | Saved | Encode, ms | Decode, ms | Saved by libsixel `size` |
| --- | --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 328,750 | 302,133 | 8.1% | 3.58 → 3.60 | 0.79 → 0.78 | 8.1% |
| Photograph, 1920×1080 | 1,663,155 | 1,443,766 | 13.2% | 21.6 → 20.9 | 4.40 → 4.09 | 13.0% |
| Two colors, 1920×1080 | 692,322 | 347,802 | 49.8% | 2.26 → 1.61 | 1.54 → 0.83 | 48.9% |
| Random noise, 1920×1080 | 9,316,560 | 9,228,951 | 0.9% | 104.9 → 104.2 | 19.2 → 19.6 | 0.9% |
<!-- measurements:end -->

## Reproduction

Build libsixel with Meson in a separate directory if you want the comparison, then run the benchmarks and rewrite every table:

```sh
tools/update_sixel_codec_tutorial_measurements.py --measure \
    --libsixel-build BUILDDIR --libsixel-revision REVISION
tools/reproduce_sixel_codec_tutorial_figures.sh
```

Each `--measure` keeps the fastest value of every timing at the same revision, because other work on a machine only ever makes a run slower; pass `--fresh` to start over. `--only-overpaint` measures just the overpaint comparison and keeps every other result. Without `--measure`, the script only rewrites the tables from the results file, and `--check` verifies them. In the `code` directory, `make bench` prints just the timings behind the chapter tables.
