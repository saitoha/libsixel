# Measurement Record of the SIXEL Codec Tutorial

This page is for readers who want to check or reproduce the numbers in the [tutorial](README.md). It records the conditions and the timings of individual phases. It does not explain the techniques; each lesson does that. The tables are generated from [`measurements/results.json`](measurements/results.json).

## Conditions

<!-- measurements:begin:environment -->
| Item | Value |
| --- | --- |
| Date | 2026-11-05 |
| Based on revision | `17e64f4b8` |
| Machine | Apple M3 Max |
| System | Darwin 27.0.0 |
| Compiler | Apple clang version 21.0.0 (clang-2100.4.1.1) |
| Flags | `-O2 -Wall -Wextra` |
| Load average before, after | 3.31, 3.64 |
<!-- measurements:end -->

The inputs are two photographs from the [high-color study](../../encoder/output/high-color/README.md) with an adaptive 256-color palette and error diffusion, as `img2sixel` writes them, a synthetic two-color pattern, and uniformly random pixels. The pictures are decoded to indexes first, so no time is spent on palette construction or dithering. Every time is the fastest of repeated runs, in milliseconds, and differences of a few percent are noise. All of these numbers come from one machine; another processor can rank the techniques differently.

## Decoding techniques of step 3, one at a time

The two middle rows are copies of the sources patched to apply one technique.

<!-- measurements:begin:attribution -->
| Decoder | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| Step 2 | 11.5 | 1.83 | 32.8 |
| Step 2 with masks for single characters | 6.22 | 2.00 | 26.4 |
| Step 3 without the masks | 10.4 | 1.11 | 25.2 |
| Step 3 | 4.17 | 1.47 | 18.6 |
<!-- measurements:end -->

## Encoder phases

Classify sorts pixels into per-color rows, cut nodes finds each color's spans, and emit writes the stream.

<!-- measurements:begin:phases -->
| Step | Input | Classify | Cut nodes | Emit |
| --- | --- | --- | --- | --- |
| 2 | Photograph, 1920×1080 | 0.73 | 14.3 | 6.99 |
| 2 | Two colors, 1920×1080 | 0.69 | 0.20 | 1.39 |
| 2 | Random noise, 1920×1080 | 1.73 | 58.4 | 91.2 |
| 3 | Photograph, 1920×1080 | 0.73 | 14.4 | 6.18 |
| 3 | Two colors, 1920×1080 | 0.71 | 0.22 | 1.29 |
| 3 | Random noise, 1920×1080 | 1.74 | 59.7 | 42.2 |
| 4 | Photograph, 1920×1080 | 3.35 | 2.20 | 6.16 |
| 4 | Two colors, 1920×1080 | 2.11 | 0.50 | 1.29 |
| 4 | Random noise, 1920×1080 | 4.06 | 21.8 | 42.5 |
| 5 | Photograph, 1920×1080 | 2.17 | 2.21 | 6.42 |
| 5 | Two colors, 1920×1080 | 0.86 | 0.50 | 1.29 |
| 5 | Random noise, 1920×1080 | 7.47 | 21.4 | 43.6 |
<!-- measurements:end -->

## From column words to rows

The time of the materializing pass in steps 4 and 5.

<!-- measurements:begin:materialize -->
| Step | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| 4 | 0.57 | 0.58 | 0.57 |
| 5 | 0.17 | 0.16 | 0.16 |
<!-- measurements:end -->

## Prescan of step 6

The serial prescan against the whole decode at the default thread count.

<!-- measurements:begin:prescan -->
| Input | Prescan | Whole decode |
| --- | --- | --- |
| Photograph, 1920×1080 | 0.07 | 0.53 |
| Two colors, 1920×1080 | 0.12 | 0.33 |
| Random noise, 1920×1080 | 0.36 | 2.30 |
<!-- measurements:end -->

## Thread scaling of step 6

Each cell is encode / decode time, with the speed-up over one thread in parentheses.

<!-- measurements:begin:threads -->
| Threads | Photograph, 1920×1080 | Two colors, 1920×1080 | Random noise, 1920×1080 |
| --- | --- | --- | --- |
| 1 | 10.8 (1.0×) / 2.63 (1.0×) | 2.65 (1.0×) / 0.80 (1.0×) | 74.1 (1.0×) / 15.4 (1.0×) |
| 2 | 6.07 (1.8×) / 1.39 (1.9×) | 1.40 (1.9×) / 0.52 (1.5×) | 40.4 (1.8×) / 8.01 (1.9×) |
| 4 | 3.96 (2.7×) / 0.77 (3.4×) | 0.83 (3.2×) / 0.37 (2.2×) | 22.8 (3.3×) / 4.41 (3.5×) |
| 8 | 2.33 (4.6×) / 0.51 (5.2×) | 0.54 (4.9×) / 0.32 (2.5×) | 12.1 (6.1×) / 2.55 (6.0×) |
| 10 | 2.20 (4.9×) / 0.43 (6.1×) | 0.49 (5.4×) / 0.31 (2.6×) | 10.8 (6.9×) / 2.18 (7.1×) |
| 14 | 1.58 (6.8×) / 0.53 (5.0×) | 0.46 (5.8×) / 0.33 (2.4×) | 10.2 (7.3×) / 2.35 (6.5×) |
<!-- measurements:end -->

## Comparison with libsixel

The same indexed pictures go through step 5 or step 6 of the tutorial and through a libsixel build of the listed revision, using `sixel_decode_raw()` and `sixel_encode()` with the `fast` encoding policy. The libsixel calls do more than the tutorial functions: they keep coverage information, handle wide registers, transparency and output callbacks, and defend against hostile input. The comparison shows that the tutorial is a realistic model of the core loops, not that either program is better. libsixel breaks ties between equal nodes differently, so its streams are not byte-identical to the tutorial's.

<!-- measurements:begin:libsixel -->
| Input | Threads | Encode, tutorial | Encode, libsixel | Decode, tutorial | Decode, libsixel |
| --- | --- | --- | --- | --- | --- |
| Photograph, 1920×1080 | 1 | 10.7 | 9.00 | 2.67 | 2.78 |
| Photograph, 1920×1080 | 14 | 1.58 | 1.55 | 0.53 | 0.64 |
| Two colors, 1920×1080 | 1 | 2.70 | 3.60 | 0.70 | 0.91 |
| Two colors, 1920×1080 | 14 | 0.46 | 0.91 | 0.33 | 0.44 |
| Random noise, 1920×1080 | 1 | 72.9 | 55.2 | 15.2 | 15.5 |
| Random noise, 1920×1080 | 14 | 10.2 | 7.93 | 2.35 | 2.75 |
<!-- measurements:end -->

## Overpainting of step 7

Step 3 and step 7 run in turn on one thread, so they share the same conditions. The last column compares libsixel's `size` and `fast` policies on the same indexed pictures.

<!-- measurements:begin:overpaint -->
| Input | Step 3 bytes | Step 7 bytes | Saved | Encode, ms | Decode, ms | Saved by libsixel `size` |
| --- | --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 328,750 | 302,133 | 8.1% | 3.71 → 3.62 | 0.75 → 0.71 | 8.1% |
| Photograph, 1920×1080 | 1,663,155 | 1,443,766 | 13.2% | 22.0 → 21.2 | 4.22 → 3.98 | 13.0% |
| Two colors, 1920×1080 | 692,322 | 347,802 | 49.8% | 2.22 → 1.58 | 1.48 → 0.80 | 48.9% |
| Random noise, 1920×1080 | 9,316,560 | 9,228,951 | 0.9% | 107.9 → 107.8 | 18.8 → 19.7 | 0.9% |
<!-- measurements:end -->

## Reproduction

Build libsixel with Meson in a separate directory if you want the comparison, then run the benchmarks and rewrite every table:

```sh
tools/update_sixel_codec_tutorial_measurements.py --measure \
    --libsixel-build BUILDDIR --libsixel-revision REVISION
tools/reproduce_sixel_codec_tutorial_figures.sh
```

Each `--measure` keeps the fastest value of every timing at the same revision, because other work on a machine only ever makes a run slower; pass `--fresh` to start over. `--only-overpaint` measures just step 7 and keeps every other result. Without `--measure`, the script only rewrites the tables from the results file, and `--check` verifies them. In the `code` directory, `make bench` prints just the table of the tutorial's main page.
