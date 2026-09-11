# Resampling

## Scope

This document describes how the current byte and float32 scalers choose source samples, evaluate kernels, normalize edges, execute separable passes, and use threading and SIMD. [Crop and Resize](crop-resize.md) owns the CLI dimension grammar, crop ordering, resize colorspace and precision policy, and the measured policy comparison.

`img2sixel -r METHOD` and `img2sixel --resampling=METHOD` select a resampling method only when `-w` or `-h` activates resize. The default is `bilinear`.

## Method summary

| Method | Base support radius | Current kernel character |
| --- | ---: | --- |
| `nearest` | none | Select one source sample directly; no weighted intermediate. |
| `gaussian` | 1 | Positive Gaussian weights, truncated by the radius-1 sample window. |
| `hanning` | 1 | Raised-cosine Hanning weights. |
| `hamming` | 1 | Hamming cosine weights with a nonzero value at radius 1. |
| `bilinear` | 1 | Triangular weight `1 - distance`; the compatibility default. |
| `welsh` | 1 | Quadratic weight `1 - distance * distance`. |
| `bicubic` | 2 | Piecewise cubic reconstruction with one negative lobe. |
| `lanczos2` | 2 | `sinc(distance) * sinc(distance / 2)`. |
| `lanczos3` | 3 | `sinc(distance) * sinc(distance / 3)`. |
| `lanczos4` | 4 | `sinc(distance) * sinc(distance / 4)`. |

The radius is expressed in the sampling domain used for the current axis. During reduction, mapping source centers into destination space expands the corresponding source-pixel footprint, so a fixed radius still gathers more source samples and performs low-pass averaging. During enlargement, destination centers are mapped into source space and the radius is measured directly there.

## Kernel shapes

The following plots evaluate the functions in [`src/scale.c`](../../src/scale.c) before per-output normalization. They are separate figures because compact positive kernels and wide kernels with negative lobes have different useful y-axis ranges.

![Unnormalized Gaussian, Hanning, Hamming, bilinear, and Welsh weights over radius one](resampling/figures/resampling-kernels-radius-1.png)

*Figure 1. Radius-1 filtered kernels. The scaler divides the accumulated weighted samples by the sum of the weights actually present at each output position.*

![Unnormalized bicubic and Lanczos 2, 3, and 4 weights over radius four](resampling/figures/resampling-kernels-wide.png)

*Figure 2. Wide-support kernels. Negative lobes can preserve apparent sharpness but can also overshoot or ring near a hard edge. Float32 output is clamped to the unit interval after normalization; byte output is clamped to 0 through 255.*

The implementation chooses one method for both axes. There is no CLI mode that uses different horizontal and vertical kernels in the same resize.

## Nearest-neighbor mapping

`nearest` bypasses the weighted filter path. For destination coordinates `w` and `h`, it selects:

```text
x = floor(w * source_width  / destination_width)
y = floor(h * source_height / destination_height)
```

The top-left destination sample therefore maps to the top-left source sample. This path preserves source sample values exactly, but reduction can alias because it does not integrate the discarded source area.

## Filtered coordinate mapping

Filtered methods use pixel centers at half-integer coordinates. The expressions differ between enlargement and reduction so the kernel operates in the domain that keeps its nominal support appropriate.

For enlargement or equal size on the horizontal axis, destination pixel `w` maps into source coordinates as:

```text
center_x = (w + 0.5) * source_width / destination_width
distance = (source_x + 0.5) - center_x
```

For reduction, the destination center remains `w + 0.5`, while each source center maps into destination coordinates:

```text
center_x = w + 0.5
distance = (source_x + 0.5) * destination_width / source_width - center_x
```

The vertical pass uses the same expressions with height and `y` substituted for width and `x`.

![Source and output pixel-center mappings for a four-to-seven enlargement and a seven-to-four reduction](resampling/figures/resampling-coordinate-mapping.png)

*Figure 3. Enlargement maps destination centers into source space; reduction maps source centers into destination space. This is why a radius-1 reduction can cover more than the nearest two source pixels.*

Candidate indices are clipped to valid source bounds before evaluating the kernel. There is no synthetic black, transparent, reflected, or wrapped padding outside the image.

## Accumulation and edge normalization

For each output sample in a filtered pass, the scaler accumulates both the weighted channel sum and the total weight of the valid candidate samples:

```text
output_channel = sum(source_channel * kernel(abs(distance)))
                 / sum(kernel(abs(distance)))
```

This normalization is recomputed at every output coordinate. Near an image boundary, clipped-away candidates therefore do not darken the edge merely because their weights are absent.

The byte scalar path floors the normalized value and clamps it to `[0, 255]`. SIMD byte paths use their native float-to-integer conversion sequence, so a scalar and vector implementation are allowed to differ by rounding at some samples. The float32 path normalizes in float arithmetic and clamps each channel to `[0, 1]`; clamping is especially relevant for bicubic and Lanczos negative lobes.

## Separable two-pass execution

Every filtered method is separable:

1. the horizontal pass resamples each source row into an intermediate with destination width and source height;
2. the vertical pass resamples each intermediate column into destination height.

For RGB byte input, the filtered intermediate is approximately `destination_width * source_height * 3` bytes. For three-channel float32 input it is approximately four times larger. The destination and any format-conversion buffers are additional allocations owned by the higher frame/planner path; see [Crop and Resize](crop-resize.md) and [Pixel-Format Precision](../concepts/pixelformat-precision.md).

`nearest` does not allocate this filtered intermediate because it writes selected source samples directly to the destination.

## Threading and SIMD

The byte and float32 filtered paths have scalar implementations plus eligible SSE2, AVX, and NEON RGB-three-channel implementations when compiled for the corresponding target. The runtime SIMD base in `-j` is a ceiling over the native capability; `auto` selects the detected native level, while `none` and `scalar` force level 0. Platform and compile-time guards may lower the effective level further.

SIMD selection does not change the method, coordinate equations, kernel radius, or normalization contract. It can change final rounding because vector paths accumulate in float lanes while the scalar byte path uses double accumulators before flooring. Code requiring exact cross-platform pixels should request `-jscalar` and fix every downstream palette and encoding control as well.

When threading is enabled and the buffer is eligible, the scaler submits horizontal and vertical row bands to the shared thread pool. The horizontal pass must complete before the vertical pass begins because the second pass reads the intermediate. `--threads=1` removes this scheduling variable; `:scale_min_bytes=BYTES` and the corresponding environment override can defer parallel scaling for smaller buffers.

The resize precision policy is a separate planner decision. `preserve` sends gamma-coded byte RGB through the byte scaler, while `linear` and `float` normally promote gamma input to linear RGB float32 before calling the float scaler. Details and measured consequences are in [Crop and Resize](crop-resize.md).

## Transparent masks

Frame transparency uses a binary mask separate from color samples. The frame resize path converts that mask to float values, applies the selected resampling method with the same target geometry, and thresholds the result at `0.5`. An output value at or above the threshold is transparent.

The color plane and mask therefore use the same spatial kernel but different semantic postprocessing: RGB remains a color sample, while coverage is collapsed back to a binary omitted/painted decision. Wider or ringing kernels can move that decision boundary. The [Alpha Policy](../loader/alpha-policy.md) owns how a source's alpha becomes this mask and how omitted pixels are represented in the emitted SIXEL stream.

## API and implementation map

The public method constants and helper declarations are in [`include/sixel.h.in`](../../include/sixel.h.in). [`sixel_helper_scale_image()`](../../src/scale.c) operates on byte pixels, while `sixel_helper_scale_image_float32()` operates on supported float32 formats. The frame wrappers in [`src/frame.c`](../../src/frame.c) update ownership, dimensions, colorspace/pixel-format metadata, and transparent masks around those low-level calls.

The method switch, kernel functions, coordinate expressions, serial and thread-pool passes, SIMD dispatch, and normalization are all implemented in [`src/scale.c`](../../src/scale.c). Focused scale behavior is covered by the C tests under [`tests/processing/geometry/`](../../tests/processing/geometry/), while frame-level resize and mask behavior is covered under [`tests/processing/frame/`](../../tests/processing/frame/).

The kernel and coordinate figures are generated by [`tools/plot_crop_resize_measurements.py`](../../tools/plot_crop_resize_measurements.py) and checked by [`tools/check_crop_resize_measurements.py`](../../tools/check_crop_resize_measurements.py). Regenerate them with the workflow documented in [Crop and Resize](crop-resize.md#reproducing-and-validating-the-evidence).
