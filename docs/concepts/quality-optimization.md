# Why Quantization Quality Needs Color, Display, and Vision Knowledge

## Scope

This document is an introduction to the concepts part of the documentation, written for programmers who tune the quality of palette quantization and dithering. It explains why that work, past its first steps, depends on knowledge of color spaces, display devices, and the physiology of vision, and how the need for each grows out of the limits of the method before it.

It describes a way of working, not a policy. The [image-quality measurement policy](../quality/measurement-policy.md) owns metrics, thresholds, fixtures, and comparison conditions, and the documents linked at the end explain each body of knowledge in detail.

## Where quality is decided

A quantized image is not judged in its pixel array. It is judged after a display has turned the numbers into light, after that light has crossed some distance to an eye adapted to some surroundings, and after the visual system has filtered, averaged, and interpreted it. libsixel aims at the fidelity of that perceived image, as the [visual perception reference](../quality/visual-perception/README.md#perceptual-quality) explains: the pixels of a dithered image may differ widely from the reference as long as the image the viewer sees does not.

Everything that follows is a consequence of this distance between what a program can inspect and what is actually being judged. Each method of guiding optimization closes part of that distance, and each stops working at the point where the remaining part dominates.

## Stage 1: visual inspection

At the start of an implementation, looking at the output is the right tool. Defects are large and unambiguous: wrong colors, broken bands, inverted channels, missing rows. A glance finds them, a fix removes them, and nobody needs a number to agree that the result is better.

This stops working sooner than it seems, for several reasons.

- **Improvements fall below the threshold of a side-by-side glance.** Once gross defects are gone, a typical change moves the result by an amount that a viewer cannot reliably see by alternating between two images, especially when the difference is spread over the whole frame rather than concentrated in one spot.
- **Expectation shapes what is seen.** A programmer who has just written a change meant to improve the image tends to see an improvement. Judgments made with knowledge of which image is which are biased toward the intended outcome, which is why perceptual studies hide that information from observers.
- **Fixing visible artifacts one by one moves error instead of removing it.** A palette of a few hundred colors cannot represent a photograph exactly, so a residual error always remains. Removing the artifact that currently catches the eye usually pushes its error somewhere else: banding traded for grain, grain traded for color drift, color drift traded for softened edges. Each step looks like progress, because the newly displaced error is not yet the one being looked at.
- **Viewing conditions are not fixed.** The same output looks different at another zoom level, on another monitor, in a brighter room, or after looking at a different image. A judgment that is not tied to stated conditions is not reproducible, even by the same person on another day.

Together these produce a characteristic failure: a long series of small, individually plausible changes that, in retrospect, circle around the same few trade-offs, and a final result that is worse than an earlier one when the two are finally compared directly. The lack of direction, not any single wrong decision, is the problem.

## Stage 2: measurement

The first remedy is measurement. A fixed metric, computed on fixed inputs under fixed conditions, gives every change a sign. It does not depend on what the programmer expects, it is indifferent to which artifact was noticed last, and it lets a result be compared with any earlier one rather than only with the previous step. A sequence of changes then has a direction, and a regression is visible as soon as it happens.

Measurement only provides this when it is controlled. The comparison must cover the complete path from input to decoded output, the inputs and settings must be held constant, and the metrics and their interpretation must be chosen before the result is seen; the [measurement policy](../quality/measurement-policy.md#controlled-comparisons) defines these conditions. A metric selected after the fact because it favors the change reintroduces the bias that measurement was meant to remove.

## Stage 3: the limits of metrics

After a period of steady progress, measurement runs into its own wall.

### Overfitting a metric

A metric is a model of perceived quality, and every model has blind spots. Optimizing hard against one metric eventually finds them. PSNR rewards blurring, because a smooth estimate minimizes squared error, and penalizes well-placed dither noise. SSIM-family metrics computed on luma, as `lsqa` computes them, do not see a gray area drifting toward a tint, as the [metrics reference](../quality/metrics.md#why-luminance-is-currently-emphasized) explains. A change that improves the score by exploiting such a gap makes the number better and the image worse.

The response is to combine metrics that observe different properties, so that a gain in one is checked by the others, and, where no existing metric observes the property in question, to build one. The [combination table for dithered output](../quality/metrics.md#combining-metrics-for-dithered-output) is an example: integrated color, multiscale structure, noise placement, contouring, colorfulness, and sparse faults are each observed separately.

### Error is moved, not removed

Combining metrics exposes a deeper problem. With a fixed palette, the total error has a floor that no processing can go below, and most of what quantization and dithering do is decide where the remaining error goes. In this sense error energy behaves approximately like a conserved quantity: the palette largely fixes how much there is, and the algorithm chooses how it is distributed.

It can be distributed along several axes:

- **across spatial scales**, from coarse blotches and bands to fine grain; error diffusion shapes quantization noise toward high frequencies, as the [dithering explanation](dithering/README.md#noise-spectrum-where-the-error-goes) derives;
- **across channels**, between luminance and the two chromatic channels;
- **across image content**, between flat areas, gradients, edges, and textures;
- **across properties**, for example between preserving structure and preserving local color, which [structure-preserving dithering](dithering/README.md#structure-preserving-dithering-and-luminance-priority) trades against each other.

Different metrics value these destinations differently. One scores error moved into fine-scale chroma noise as harmless and another as a large per-pixel color difference; one rewards sharper edges and another penalizes the oriented texture that sharpening introduces. When the metrics disagree, no amount of further measurement says which allocation is right. The question has become one of which error matters more.

## Stage 4: defining quality

At this stage the definition of quality itself is in question, and the only consistent answer is the one given at the beginning: what matters is the image formed in the viewer's visual system. A trade-off between two error allocations is resolved by asking which one that image is more sensitive to, under the conditions in which it will be seen. Answering requires three kinds of knowledge.

**Color spaces** determine where arithmetic means what it is meant to mean. The eye averages light, not encoded values, so a ditherer that averages gamma-encoded pixels reproduces mid-tones too dark; [mixing needs linear light](colorspace/README.md#why-mixing-and-background-composition-use-linear-rgb). Conversely, distances between colors are meaningful only in a space whose geometry approximates perceived difference, which is why nearest-color search and color-difference metrics use perceptual spaces. Choosing the wrong space for an operation produces a consistent bias that no amount of tuning elsewhere removes.

**Displays** determine what light the numbers become. The transfer function, the primaries and white point, the pixel density, the way subpixels or dots are arranged, and the way a terminal scales a character cell all decide which frequencies and which colors actually reach the eye. Dither noise that is invisible on a high-density panel can be conspicuous when a terminal enlarges each image pixel, as the discussion of [the effective density of a terminal image](displays/README.md#the-effective-density-of-a-terminal-image) shows, and the right trade-off depends on which of those situations is being targeted.

**Visual physiology** determines how that light is perceived. Contrast sensitivity falls at high spatial frequencies, and it falls faster for the chromatic channels than for luminance; the eye adapts to the overall light level and to its surroundings; it integrates detail finer than it can resolve. These facts, described in the [visual perception reference](../quality/visual-perception/README.md), are what make it possible to say that fine-scale chroma noise costs less than coarse luminance banding, or that preserving luminance structure is worth some loss of per-pixel color accuracy. They are also the basis on which existing metrics were designed and on which new ones can be.

Without this knowledge, a programmer confronted with conflicting metrics has no principled way to choose, and falls back on inspection or on whichever metric is most familiar. That is the loss of direction of the first stage, returning at a higher level.

## The stages together

| Stage | What gives direction | Where it fails | What is needed next |
| --- | --- | --- | --- |
| Visual inspection | Obvious defects | Small differences, expectation bias, error moved rather than removed, unstated viewing conditions | A fixed measure |
| Measurement | A metric under controlled conditions | Optimizing into the metric's blind spots | Several metrics, or new ones |
| Combined metrics | Properties observed separately | Metrics disagree about where error should go | A definition of quality |
| Perceptual model | Color, display, and vision knowledge applied to stated viewing conditions | Conditions outside those modeled | Explicit assumptions and checks against human judgment |

The stages do not replace one another. Inspection remains useful throughout, both to find defects that no metric observes and to form hypotheses, provided it is done under stated conditions, such as fixed magnifications and a known display, and its conclusions are confirmed by measurement. Metrics remain the means of keeping direction. Perceptual knowledge is what decides which metrics to trust for a given question and how to weigh them when they conflict.

## Working practices

A few habits follow from this progression.

- **State the viewing conditions.** Decide which display density, magnification, and surroundings a change is meant to serve before judging it, because the right trade-off depends on them.
- **Phrase a change as a perceptual hypothesis.** "Move error from coarse luminance to fine chroma" can be tested and can be wrong; "make it look better" cannot.
- **Choose the metrics before the experiment.** Pick metrics that observe the property the hypothesis is about and at least one that observes where the displaced error is expected to go.
- **Compare against fixed baselines, not only the previous step.** A sequence of small gains is verified only by comparing its end with its start.
- **Treat a metric gain without a visible or explainable cause as a warning.** It may be a blind spot being exploited rather than an improvement.

## Further reading

The rest of the concepts part follows the path of an image from source to viewer:

- [Visual perception for image quality](../quality/visual-perception/README.md) and [character cell perception](../quality/character-cell-perception/README.md) for how images are seen;
- [Color spaces](colorspace/README.md), [color management](color-management/README.md), and [gamut mapping](gamut-mapping/README.md) for how colors are represented and converted;
- [Color difference and color reproduction](../quality/color-difference.md) for how color errors are judged;
- [Display technologies](displays/README.md) for how numbers become light;
- [Palettes](palettes/README.md), [palette clustering](clustering/README.md), and [dithering](dithering/README.md) for how an image is drawn with few colors.

[Image-quality metrics](../quality/metrics.md) and the [measurement policy](../quality/measurement-policy.md) then show how this knowledge is turned into measurements.
