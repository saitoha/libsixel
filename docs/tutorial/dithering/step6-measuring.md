# Step 6: Measuring a Dither

Now put numbers on the questions of the last lesson. `lsqa`, libsixel's image-quality tool, compares any two pictures, including the output of your own programs. This lesson runs it on every method so far, reads the results against your eyes, and finds out when metrics mislead. The commands are collected in [`code/score.sh`](code/score.sh).

**You will learn:**

- how to run `lsqa` on your output and what each number looks at,
- why the most trusted metric can prefer a worse-looking picture,
- how metrics find what simple arithmetic missed,
- how to set up a fair comparison and what no metric tells you.

## Getting lsqa

`lsqa` is part of libsixel. Build it with either build system, as described in [Platform support](../../platform-support.md):

```sh
meson setup build
ninja -C build assessment/lsqa
```

Compare an original with a dithered output. Give it a picture of at least 176 pixels on each side. Its [main metric](../../quality/metrics.md#reference-ms-ssim) reduces the picture four times and has no answer for a smaller one:

```sh
cd docs/tutorial/dithering/code
./dtool gradient 640 240 > gradient.ppm
./dtool cube 3 > cube27.ppm
./step3 gradient.ppm cube27.ppm > fs.ppm
../../../../build/assessment/lsqa gradient.ppm fs.ppm
```

It prints a JSON report with dozens of metrics. To ask for one, use `-m`:

```sh
../../../../build/assessment/lsqa -m MS-SSIM gradient.ppm fs.ppm
```

## Scoring the methods

`score.sh` runs every method on a picture and palette and prints one row per method:

```sh
LSQA=../../../../build/assessment/lsqa ./score.sh gradient.ppm cube27.ppm
```

The column names below are short for these metrics, and [Image-quality metrics](../../quality/metrics.md) defines each of them:

| Column | `lsqa` metric | Question it looks at | Better is |
| --- | --- | --- | --- |
| MS-SSIM | `MS-SSIM` | Structure, across several scales | Higher |
| GMSD | `GMSD` | Evenness of the edges and gradients | Lower |
| Area ΔE00 | `DE00_region_mean` | Tone after averaging light in 16 × 16 tiles | Lower |
| High freq. | `HighFreqRatio_delta` | How much energy moved to fine detail | Depends |
| Stripes | `StripeScore_rel` | Oriented or periodic noise | Lower |
| Banding | `BandingIndex_grad_rel` | Contours, compared with the original | Lower |
| Bytes | Tutorial SIXEL encoder | Stream size | Lower |

Here is the test gradient with the 27-color cube:

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

Here is the photograph with the same palette:

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

And here it is with a palette of its own:

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

## Reading the tables

**Banding** does what it says. Only the nearest color is strongly positive, on every set.

**Stripes** finds the Bayer grid. On the photograph with the cube it scores about +57, and no other method comes close. That is the cross-hatching you saw in lesson 2. The gradient itself has a strong direction, which is why its values are large; compare methods, not the numbers between pictures.

**MS-SSIM misleads here.** On the gradient, it prefers the flat bands of the nearest color (0.846) to Floyd–Steinberg (0.641), although nobody would choose those bands. MS-SSIM compares local structure, and a coarse dither adds a lot of local structure the original does not have. It is libsixel's [primary gate](../../quality/measurement-policy.md#primary-and-secondary-metrics) for good reason, but this picture is far outside the range it is used in, where a [normal target](../../quality/measurement-policy.md#primary-and-secondary-metrics) is 0.98. Even on the photograph with its own palette, the nearest color (0.930) scores above Floyd–Steinberg (0.915), though below Atkinson and the Bayer pattern. So a higher MS-SSIM alone does not settle which dither looks better.

**GMSD also penalizes dots.** Dots are edges to it, so all dithers score worse than the nearest color on the gradient. As [its definition](../../quality/metrics.md#gradient-metric-gmsd) notes, compare it only between dither variants, not with no dither at all.

**Bytes have a price.** On the gradient, removing the bands costs more than 100 times the bytes. On the photograph with its own palette, it costs a bit more than double, and the difference between 56 and 129 kilobytes is the real decision for a slow connection. Atkinson needs about a fifth fewer bytes than Floyd–Steinberg here, and its MS-SSIM is better, while its area error is worse. [Step 7 of the codec tutorial](../sixel-codec/step7-overpaint.md) removes some of those bytes again without changing a pixel.

No row wins every column. That is the normal result, and it is why [no single metric captures dither quality](../../quality/metrics.md#combining-metrics-for-dithered-output).

## What the three numbers missed

Lesson 5's area error said Floyd–Steinberg was almost perfect on the gradient: about 0.7 out of 255. But `lsqa`'s area error on the same output is 5.95 ΔE00, a large difference. On the photograph with the cube it is 6.74. Who is right?

`dtool` averages the numbers stored in the file. `lsqa` averages [light](../../quality/metrics.md#color-difference-metrics), which is what the eye actually integrates. The stored numbers are gamma-encoded, so half of the number 255 is much less than half the light: a 50% mix of black and white, seen from afar, looks like about level 188. Our dither balances numbers, so its areas come out with the wrong brightness, and only a measure that works with light can see this. The [dithering concepts](../../concepts/dithering/README.md#dithering-in-color) page covers this effect.

libsixel can do the diffusion in either space, so the effect can be measured directly on the same picture and palette:

<!-- measurements:begin:light -->
| Floyd–Steinberg on the photograph, 27-color cube | Area ΔE00 |
| --- | --- |
| Tutorial step 3, on the stored numbers | 6.74 |
| libsixel, `-W gamma` | 6.46 |
| libsixel, `-W linear` | 0.94 |
<!-- measurements:end -->

The first row is the method you wrote, and libsixel gets about the same score when it also works with the stored numbers. Doing the arithmetic in linear light removes most of the error. The [working color space](../../encoder/palette-application/working-colorspace/README.md) reference describes this option.

> **Try it.** Make `step3` do its arithmetic in linear light: convert each input channel from sRGB to linear before adding the error, and compare in linear when looking for the nearest color. Does the score fall as far as libsixel's? Does your eye agree? [Lesson 7](step7-linear-light.md) is a worked answer.

## How far away, measured

Lesson 5 said that quality depends on the viewing distance. The S-CIEDE2000 metric takes this as a parameter, `--ppd`, the number of pixels per degree of the eye's field of view:

```sh
../../../../build/assessment/lsqa --ppd=16 -m S-CIEDE2000 gradient.ppm fs.ppm
```

Lower numbers below mean less visible error:

<!-- measurements:begin:ppd -->
| Method | 16 pixels per degree | 32 pixels per degree | 60 pixels per degree | 120 pixels per degree |
| --- | --- | --- | --- | --- |
| Nearest color (step 1) | 16.17 | 16.14 | 16.12 | 16.12 |
| Bayer pattern (step 2) | 9.31 | 6.69 | 5.93 | 5.78 |
| Floyd–Steinberg (step 3) | 9.63 | 6.60 | 5.98 | 5.87 |
<!-- measurements:end -->

The score of the nearest color hardly changes, because a band is visible from anywhere. The dithers get much better as pixels get smaller, which is what you saw by zooming. Any statement like "this dither is better" needs this number next to it, and the [measurement policy](../../quality/measurement-policy.md#controlled-comparisons) requires it.

## A fair comparison

Before you trust a comparison, make sure of these. The [measurement policy](../../quality/measurement-policy.md#controlled-comparisons) has the full list.

1. **Change one thing.** Use the same picture and palette for every method.
2. **Choose the metrics first.** After seeing the numbers, it is too easy to pick the one that agrees with you.
3. **Use more than one picture.** The tables above disagree between pictures. A gradient, a flat area and a photograph behave differently.
4. **State the viewing distance.**
5. **Look.** The metrics are summaries. As [the documentation says](../../quality/metrics.md#combining-metrics-for-dithered-output), keep looking at more than one magnification when they disagree.

There is also something no column here measures: how much the grain itself disturbs a viewer. `HighFreqRatio_delta` says how much energy moved, and `S-CIEDE2000` says how visible the error is at a distance, but neither is a grain detector, as the [S-CIELAB section](../../quality/metrics.md#interpretation-and-limits) says. In the end, your eyes decide that one.

> **Try it: your own experiment.** Take the kernel you invented in lesson 4 and a picture of your own. Decide first which columns matter for your use, for example Area ΔE00 and Bytes for slow connections. Add your method to `score.sh` and see whether your ranking from lesson 5 survives.

## Where to go from here

You can now write a dither, say what is wrong with it, and measure that. libsixel's [dithering reference](../../encoder/palette-application/dithering/README.md#measured-static-image-quality-size-and-speed) applies the same approach to all its methods, including error spectra measured on flat fields. The next four lessons use these measurements to improve the program, and [step 7](step7-linear-light.md) starts with the error that this lesson uncovered.
