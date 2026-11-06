# Measurement Record of the Dithering Tutorial

This page is for readers who want to check or reproduce the numbers in the [tutorial](README.md). It records the conditions and every table. It does not explain the methods or the metrics; the lessons and [Image-quality metrics](../../quality/metrics.md) do that. The tables are generated from [`measurements/results.json`](measurements/results.json).

## Conditions

<!-- measurements:begin:environment -->
| Item | Value |
| --- | --- |
| Date | 2026-11-06 |
| Based on revision | `80256a9c5` |
| Machine | Apple M3 Max |
| System | Darwin 27.0.0 |
| Compiler | Apple clang version 21.0.0 (clang-2100.4.1.1) |
| Load average before, after | 5.52, 4.99 |
<!-- measurements:end -->

The quality scores come from `lsqa` built at the listed revision, with its default settings unless a table says otherwise, which include a viewing density of 60 pixels per degree. All pictures are decoded as sRGB. Bytes are the length of the SIXEL stream written by step 2 of the [codec tutorial](../sixel-codec/step2-nodes.md). These are few pictures and one machine: they illustrate what the metrics do, and they are not a ranking of the methods. The [dithering reference](../../encoder/palette-application/dithering/README.md#measured-static-image-quality-size-and-speed) has libsixel's own comparison.

## Speed

Each value is the fastest of ten runs on one thread, on the 600×450 photograph.

<!-- measurements:begin:speed -->
| Method | 16 colors, ns per pixel | 256 colors, ns per pixel |
| --- | --- | --- |
| Nearest color (step 1) | 10.4 | 153.6 |
| Random nudge (step 2) | 23.4 | 164.3 |
| Bayer pattern (step 2) | 24.3 | 162.7 |
| Floyd–Steinberg (step 3) | 26.6 | 157.7 |
| Floyd–Steinberg, serpentine (step 4) | 27.3 | 174.4 |
| Atkinson (step 4) | 28.5 | 170.4 |
| Jarvis, Judice, Ninke (step 4) | 36.1 | 184.5 |
<!-- measurements:end -->

## The three numbers of lesson 5

<!-- measurements:begin:simple -->
| Picture | Method | Pixel error | Area error | Flat runs |
| --- | --- | --- | --- | --- |
| Gradient, 640×240, 27-color cube | Nearest color (step 1) | 35.9 | 34.8 | 128.0 |
| Gradient, 640×240, 27-color cube | Bayer pattern (step 2) | 51.5 | 0.7 | 1.4 |
| Gradient, 640×240, 27-color cube | Floyd–Steinberg (step 3) | 51.2 | 0.7 | 1.2 |
| Gradient, 640×240, 27-color cube | Atkinson (step 4) | 46.1 | 7.5 | 1.6 |
| Gradient, 640×240, 27-color cube | The original itself | 0.0 | 0.0 | 32.0 |
| Photograph, 600×450, its 16-color palette | Nearest color (step 1) | 10.6 | 6.5 | 11.5 |
| Photograph, 600×450, its 16-color palette | Bayer pattern (step 2) | 12.8 | 5.6 | 2.0 |
| Photograph, 600×450, its 16-color palette | Floyd–Steinberg (step 3) | 17.7 | 5.3 | 2.3 |
| Photograph, 600×450, its 16-color palette | Atkinson (step 4) | 11.9 | 5.5 | 3.7 |
| Photograph, 600×450, its 16-color palette | The original itself | 0.0 | 0.0 | 4.3 |
<!-- measurements:end -->

## Scores

The gradient with the 27-color cube:

<!-- measurements:begin:score-gradient-cube27 -->
| Method | MS-SSIM | GMSD | Area ΔE00 | High freq. | Stripes | Banding | Bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Nearest color (step 1) | 0.846 | 0.096 | 15.91 | +0.012 | +9.26 | +0.384 | 1,281 |
| Random nudge (step 2) | 0.331 | 0.339 | 6.11 | +0.451 | -66.81 | -0.592 | 89,149 |
| Bayer pattern (step 2) | 0.615 | 0.393 | 5.86 | +0.492 | +9.73 | -0.530 | 94,522 |
| Floyd–Steinberg (step 3) | 0.641 | 0.339 | 5.95 | +0.444 | -62.48 | -0.589 | 144,441 |
| Floyd–Steinberg, serpentine (step 4) | 0.633 | 0.336 | 5.88 | +0.446 | -64.84 | -0.594 | 145,590 |
| Atkinson (step 4) | 0.674 | 0.365 | 7.50 | +0.422 | -65.02 | -0.552 | 110,073 |
| Jarvis, Judice, Ninke (step 4) | 0.546 | 0.271 | 5.98 | +0.435 | -65.52 | -0.589 | 145,223 |
<!-- measurements:end -->

The photograph with the 27-color cube:

<!-- measurements:begin:score-snake-cube27 -->
| Method | MS-SSIM | GMSD | Area ΔE00 | High freq. | Stripes | Banding | Bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Nearest color (step 1) | 0.772 | 0.237 | 13.36 | +0.039 | +0.42 | +0.284 | 34,550 |
| Random nudge (step 2) | 0.578 | 0.331 | 6.93 | +0.528 | -1.93 | -0.522 | 170,546 |
| Bayer pattern (step 2) | 0.748 | 0.325 | 6.69 | +0.592 | +57.24 | -0.106 | 176,937 |
| Floyd–Steinberg (step 3) | 0.777 | 0.319 | 6.74 | +0.511 | +0.43 | -0.135 | 239,701 |
| Floyd–Steinberg, serpentine (step 4) | 0.772 | 0.320 | 6.73 | +0.512 | -0.97 | -0.121 | 240,164 |
| Atkinson (step 4) | 0.780 | 0.339 | 7.42 | +0.452 | -0.88 | -0.175 | 182,827 |
| Jarvis, Judice, Ninke (step 4) | 0.721 | 0.315 | 6.76 | +0.506 | -0.92 | -0.143 | 232,268 |
<!-- measurements:end -->

The photograph with its own 16 colors:

<!-- measurements:begin:score-snake-16 -->
| Method | MS-SSIM | GMSD | Area ΔE00 | High freq. | Stripes | Banding | Bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Nearest color (step 1) | 0.930 | 0.165 | 2.74 | +0.014 | -0.28 | +0.120 | 55,911 |
| Random nudge (step 2) | 0.907 | 0.197 | 2.44 | +0.064 | -0.96 | -0.067 | 120,068 |
| Bayer pattern (step 2) | 0.938 | 0.143 | 2.43 | +0.070 | +14.71 | -0.012 | 122,586 |
| Floyd–Steinberg (step 3) | 0.915 | 0.224 | 2.06 | +0.158 | -0.03 | -0.033 | 129,015 |
| Floyd–Steinberg, serpentine (step 4) | 0.915 | 0.220 | 2.05 | +0.159 | -1.31 | -0.024 | 130,144 |
| Atkinson (step 4) | 0.946 | 0.177 | 2.26 | +0.055 | -0.95 | +0.016 | 100,793 |
| Jarvis, Judice, Ninke (step 4) | 0.897 | 0.260 | 2.10 | +0.132 | -1.25 | -0.063 | 116,884 |
<!-- measurements:end -->

## Stored numbers against light

<!-- measurements:begin:light -->
| Floyd–Steinberg on the photograph, 27-color cube | Area ΔE00 |
| --- | --- |
| Tutorial step 3, on the stored numbers | 6.74 |
| libsixel, `-W gamma` | 6.46 |
| libsixel, `-W linear` | 0.94 |
<!-- measurements:end -->

## Viewing distance

S-CIEDE2000 of the gradient with the 27-color cube.

<!-- measurements:begin:ppd -->
| Method | 16 pixels per degree | 32 pixels per degree | 60 pixels per degree | 120 pixels per degree |
| --- | --- | --- | --- | --- |
| Nearest color (step 1) | 16.17 | 16.14 | 16.12 | 16.12 |
| Bayer pattern (step 2) | 9.31 | 6.69 | 5.93 | 5.78 |
| Floyd–Steinberg (step 3) | 9.63 | 6.60 | 5.98 | 5.87 |
<!-- measurements:end -->

## Linear light

Floyd–Steinberg on stored numbers (step 3) and in linear light (step 7).

<!-- measurements:begin:linear -->
| Picture and palette | Area ΔE00 | MS-SSIM | Bytes |
| --- | --- | --- | --- |
| Gradient, 640×240, 27-color cube | 5.95 → 0.58 | 0.641 → 0.633 | 144,441 → 144,731 |
| Photograph, 600×450, 27-color cube | 6.74 → 0.58 | 0.777 → 0.765 | 239,701 → 224,816 |
| Photograph, 600×450, its 16-color palette | 2.06 → 2.09 | 0.915 → 0.926 | 129,015 → 135,126 |
<!-- measurements:end -->

## Whole-number error

Step 8 on the 600×450 photograph. The times are the fastest of ten runs on one thread.

<!-- measurements:begin:carry-speed -->
| Palette | Floats (step 3), ns per pixel | Whole numbers (step 8), ns per pixel |
| --- | --- | --- |
| 16 colors | 26.6 | 41.2 |
| 255 colors | 158.2 | 175.3 |
<!-- measurements:end -->

The three limits, with the photograph's 16 colors:

<!-- measurements:begin:clamp -->
| Limit | Area ΔE00 | MS-SSIM | Bytes |
| --- | --- | --- | --- |
| `soft` | 2.03 | 0.921 | 124,089 |
| `hard` | 2.06 | 0.915 | 128,849 |
| `none` | 3.11 | 0.872 | 117,388 |
<!-- measurements:end -->

The deadzone, with `clamp=soft`:

<!-- measurements:begin:deadzone -->
| Picture and palette | Deadzone | Area ΔE00 | MS-SSIM | Bytes |
| --- | --- | --- | --- | --- |
| Flat gray 100, 320×240, 16 grays | 0 | 0.07 | 0.959 | 25,831 |
| Flat gray 100, 320×240, 16 grays | 5 | 0.75 | 1.000 | 349 |
| Flat gray 100, 320×240, 16 grays | 10 | 0.75 | 1.000 | 349 |
| Flat gray 100, 320×240, 16 grays | 20 | 0.75 | 1.000 | 349 |
| Photograph, 600×450, its 16-color palette | 0 | 2.03 | 0.921 | 124,089 |
| Photograph, 600×450, its 16-color palette | 5 | 2.03 | 0.921 | 123,857 |
| Photograph, 600×450, its 16-color palette | 10 | 2.07 | 0.923 | 119,120 |
| Photograph, 600×450, its 16-color palette | 20 | 2.34 | 0.926 | 91,089 |
| Photograph, 600×450, its 255-color palette | 0 | 0.39 | 0.991 | 325,485 |
| Photograph, 600×450, its 255-color palette | 5 | 0.57 | 0.990 | 279,258 |
| Photograph, 600×450, its 255-color palette | 10 | 0.73 | 0.991 | 251,202 |
| Photograph, 600×450, its 255-color palette | 20 | 0.86 | 0.991 | 240,376 |
| Photograph, 600×450, 27-color cube | 0 | 6.74 | 0.777 | 239,568 |
| Photograph, 600×450, 27-color cube | 5 | 6.73 | 0.776 | 239,506 |
| Photograph, 600×450, 27-color cube | 10 | 6.74 | 0.776 | 239,371 |
| Photograph, 600×450, 27-color cube | 20 | 6.74 | 0.776 | 238,699 |
<!-- measurements:end -->

## Lookup

Step 9 on the 600×450 photograph, on one thread. Each time includes building the tables of that run, because the tables belong to one palette. "Pixels changed" compares with step 3.

<!-- measurements:begin:lookup -->
| Palette | Method | ns per pixel | Colors compared per pixel | Pixels changed | Area ΔE00 |
| --- | --- | --- | --- | --- | --- |
| 16 colors | Compare with every color | 24.1 | 16.0 | 0.0% | 2.063 |
| 16 colors | Remember recent colors | 24.4 | 12.0 | 0.0% | 2.063 |
| 16 colors | One answer per cell | 15.9 | 1.1 | 32.1% | 2.069 |
| 16 colors | A list of candidates per cell | 25.3 | 5.5 | 0.0% | 2.063 |
| 255 colors | Compare with every color | 154.7 | 255.0 | 0.0% | 0.333 |
| 255 colors | Remember recent colors | 120.4 | 166.7 | 0.0% | 0.333 |
| 255 colors | One answer per cell | 19.8 | 8.1 | 54.0% | 0.337 |
| 255 colors | A list of candidates per cell | 48.6 | 31.1 | 0.0% | 0.333 |
<!-- measurements:end -->

## Threads

Step 10 on the 600×450 photograph, with the speed-up against one thread of the same row. Each value is the fastest of ten runs, in three rounds.

<!-- measurements:begin:threads -->
| 14 bands, ns per pixel | 1 thread | 2 threads | 4 threads | 8 threads | 14 threads |
| --- | --- | --- | --- | --- | --- |
| Floyd–Steinberg, 16 colors | 27.3 (1.0×) | 14.0 (1.9×) | 7.5 (3.6×) | 4.7 (5.8×) | 3.7 (7.4×) |
| Floyd–Steinberg, 255 colors | 157.7 (1.0×) | 83.7 (1.9×) | 45.7 (3.5×) | 27.2 (5.8×) | 22.9 (6.9×) |
| Bayer pattern, 16 colors | 22.3 (1.0×) | 11.9 (1.9×) | 6.5 (3.4×) | 4.0 (5.6×) | 3.1 (7.2×) |
| Bayer pattern, 255 colors | 155.7 (1.0×) | 81.9 (1.9×) | 44.8 (3.5×) | 26.6 (5.9×) | 21.8 (7.1×) |
<!-- measurements:end -->

Seams, with band starts taken from a split into four bands, and the photograph's 16 colors. "One band" has no band starts, and shows what those same rows look like without a split.

<!-- measurements:begin:seams -->
| Picture, 16-color palette | Bands | Color changes per row, band starts | Color changes per row, elsewhere | Bytes | Area ΔE00 |
| --- | --- | --- | --- | --- | --- |
| Gradient, 640×240 | One band | 231 | 232 | 37,564 | 16.17 |
| Gradient, 640×240 | Four bands | 376 | 235 | 40,263 | 16.17 |
| Gradient, 640×240 | Four bands, 6 warm-up rows | 230 | 233 | 38,208 | 16.18 |
| Flat gray 64, 640×240 | One band | 222 | 234 | 52,842 | 18.81 |
| Flat gray 64, 640×240 | Four bands | 639 | 238 | 56,694 | 18.66 |
| Flat gray 64, 640×240 | Four bands, 6 warm-up rows | 242 | 233 | 52,845 | 18.82 |
| Photograph, 600×450 | One band | 296 | 280 | 129,015 | 2.06 |
| Photograph, 600×450 | Four bands | 284 | 281 | 128,972 | 2.04 |
| Photograph, 600×450 | Four bands, 6 warm-up rows | 302 | 281 | 128,955 | 2.05 |
<!-- measurements:end -->

## Reproduction

Build libsixel, then run the measurements and rewrite every table:

```sh
tools/update_dither_tutorial_measurements.py --measure \
    --lsqa BUILDDIR/assessment/lsqa \
    --img2sixel BUILDDIR/converters/img2sixel \
    --sixel2png BUILDDIR/converters/sixel2png
tools/reproduce_dither_tutorial_figures.sh
```

Without `--measure`, the script only rewrites the tables from the results file, and `--check` verifies them. The palettes in `code/palettes` were made from `images/snake.png` by `img2sixel -d none`; `--palettes` makes them again.
