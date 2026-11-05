# Measurement Record of the Dithering Tutorial

This page is for readers who want to check or reproduce the numbers in the [tutorial](README.md). It records the conditions and every table. It does not explain the methods or the metrics; the lessons and [Image-quality metrics](../../quality/metrics.md) do that. The tables are generated from [`measurements/results.json`](measurements/results.json).

## Conditions

<!-- measurements:begin:environment -->
| Item | Value |
| --- | --- |
| Date | 2026-11-05 |
| Based on revision | `7dba05582` |
| Machine | Apple M3 Max |
| System | Darwin 27.0.0 |
| Compiler | Apple clang version 21.0.0 (clang-2100.4.1.1) |
| Load average before, after | 4.79, 4.51 |
<!-- measurements:end -->

The quality scores come from `lsqa` built at the listed revision, with its default settings unless a table says otherwise, which include a viewing density of 60 pixels per degree. All pictures are decoded as sRGB. Bytes are the length of the SIXEL stream written by step 2 of the [codec tutorial](../sixel-codec/step2-nodes.md). These are few pictures and one machine: they illustrate what the metrics do, and they are not a ranking of the methods. The [dithering reference](../../encoder/palette-application/dithering/README.md#measured-static-image-quality-size-and-speed) has libsixel's own comparison.

## Speed

Each value is the fastest of ten runs on one thread, on the 600×450 photograph.

<!-- measurements:begin:speed -->
| Method | 16 colors, ns per pixel | 256 colors, ns per pixel |
| --- | --- | --- |
| Nearest color (step 1) | 10.8 | 164.9 |
| Random nudge (step 2) | 23.3 | 162.9 |
| Bayer pattern (step 2) | 23.9 | 164.3 |
| Floyd–Steinberg (step 3) | 26.8 | 166.3 |
| Floyd–Steinberg, serpentine (step 4) | 27.4 | 176.5 |
| Atkinson (step 4) | 28.4 | 175.5 |
| Jarvis, Judice, Ninke (step 4) | 35.0 | 187.9 |
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
