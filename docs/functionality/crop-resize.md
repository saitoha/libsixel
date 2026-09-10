# Crop and Resize

## Scope

Crop and resize are encoder preprocessing operations. They change the raster frame before palette application and SIXEL serialization; they do not ask the terminal to scale an already encoded image, and they are not presentation policy for decoded SIXEL. This document covers the `img2sixel` geometry options, their processing order, resampling behavior, precision and transparency interactions, and the corresponding public frame helpers.

The broader pipeline is described in the [Encoding Pipeline](encoding-pipeline.md), the concrete planner path in the [Encoder Execution Map](encoder-execution-map.md), and the command-order exception in the [CLI Design Policy](../cli/design-policy.md).

## Processing model

The geometry portion of the normal encoder path is:

```text
loaded frame
  -> optional crop in the current coordinate space
  -> optional conversion to the resize representation
  -> optional resize
  -> optional crop in the resized coordinate space
  -> optional conversion to the working representation
  -> palette application and SIXEL encoding
```

Only one crop and one resize operation are configured. The relative position of the effective `-c` option and the effective `-w` or `-h` options decides whether crop occupies the early or late slot. Color conversion around resize is planner-owned and does not add another geometry pass.

Loading has already produced the frame on which these coordinates operate. Format-specific orientation, canvas, and page handling therefore belong to the selected loader; crop coordinates are not raw offsets into the encoded source file.

## Cropping

`-c REGION` and `--crop=REGION` accept one fixed pixel grammar:

```text
WIDTHxHEIGHT+X+Y
```

`WIDTH` and `HEIGHT` must be positive decimal integers. `X` and `Y` must be non-negative decimal integers measured from the top-left corner of the frame entering the crop operation. Negative offsets, omitted offsets, alternate separators, percentages, and terminal-cell units are rejected.

For example, this selects a 320-by-200 rectangle whose top-left pixel is 40 pixels from the left edge and 20 pixels from the top edge:

```sh
img2sixel -c 320x200+40+20 input.png
```

Cropping does not add padding. When a requested rectangle starts inside the frame but extends past its right or bottom edge, libsixel shortens the effective width or height to the available pixels. When its origin is on or beyond an edge, the effective rectangle is empty and the CLI crop filter leaves the frame unchanged. Callers that need an empty result, padding, or an error for a non-overlapping rectangle must validate the region before invoking `img2sixel`.

## Resize dimensions

`-w WIDTH` / `--width=WIDTH` and `-h HEIGHT` / `--height=HEIGHT` configure the two axes of one resize operation.

| Form | Meaning |
| --- | --- |
| `auto` | Leave this axis unconstrained and derive it from the other explicit axis while preserving aspect ratio. This is the default. |
| `NUMBER` | Set the axis to `NUMBER` pixels. |
| `NUMBERpx` | Set the axis to `NUMBER` pixels; this is equivalent to the bare number. |
| `NUMBER%` | Multiply the corresponding dimension of the frame entering resize by `NUMBER / 100`. |
| `NUMBERc` | Multiply `NUMBER` by the terminal's reported cell width or cell height. |

All numeric values must be positive integers. Percentage dimensions use integer division and therefore round down, with a one-pixel minimum. When exactly one axis is explicit, the derived aspect-preserving axis rounds up:

```text
derived_height = ceil(source_height * target_width / source_width)
derived_width  = ceil(source_width  * target_height / source_height)
```

When both axes are `auto`, no resize operation is active.

The percentage is evaluated against the frame that reaches resize. A crop-before-resize invocation therefore measures percentages against the cropped dimensions, while a resize-before-crop invocation measures them against the loaded frame.

When both axes are explicit, libsixel uses both values independently and does not preserve aspect ratio. The two explicit values may use different forms, such as `-w 50% -h 20c`.

For an 800-by-600 input, representative results are:

| Options | Resize result | Reason |
| --- | --- | --- |
| `-w 400` | 400 by 300 | Height is derived from aspect ratio. |
| `-h 300px` | 400 by 300 | Width is derived from aspect ratio. |
| `-w 50%` | 400 by 300 | Width is halved, then height is derived. |
| `-w 400 -h 200` | 400 by 200 | Both axes are explicit, so the image is stretched. |
| `-w auto -h 300` | 400 by 300 | `auto` removes the width constraint. |

The frame resize API accepts dimensions up to `SIXEL_WIDTH_LIMIT` and `SIXEL_HEIGHT_LIMIT`, currently 1,000,000 pixels per axis, subject to checked buffer-size arithmetic and successful allocation. Percentage target calculations are clamped to those limits; an oversized explicit target is rejected by the frame resize path.

### Terminal-cell units

The `c` suffix converts cells to pixels before image loading. On supported systems, `img2sixel` opens `/dev/tty`, queries `TIOCGWINSZ`, and derives cell size from the reported pixel and row/column dimensions. The option fails when no controlling terminal is available, the platform does not support the query, or the terminal reports only rows and columns without usable pixel dimensions.

Cell units describe the terminal geometry observed when the option is parsed. They do not embed cell metadata in the SIXEL stream, and later terminal font or window changes do not retroactively alter the already computed pixel target.

## Crop and resize order

Crop and resize are non-commutative, so `img2sixel` intentionally treats their relative command-line order as meaningful:

```sh
# Select source pixels first, then scale the selected rectangle.
img2sixel -c 320x200+40+20 -w 640 input.png

# Scale the source first, then select pixels in resized coordinates.
img2sixel -w 640 -c 320x200+40+20 input.png
```

For an 800-by-600 input, the following pair also shows that a percentage has a different reference frame in the two orders:

```sh
# 400x300 crop, then 50% resize: 200x150 output.
img2sixel -c 400x300+0+0 -w 50% input.png

# 50% resize to 400x300, then the same crop: 400x300 output.
img2sixel -w 50% -c 400x300+0+0 input.png
```

Repeated crop values replace the previous crop geometry. Repeated width and height values replace only their own axes. Swapping `-w` and `-h` never creates two resize passes. After replacements are resolved, the last occurrence across the crop family and the resize-dimension family determines which operation runs first. `-r` selects the resize filter but does not participate in this ordering rule.

All other encoder options retain their stage-defined order. The geometry exception does not make the rest of the command line an image-operation stack.

## Resampling filters

`-r METHOD` and `--resampling=METHOD` select the filter used by resize. Without `-w` or `-h`, the setting has no visible effect. The default is `bilinear`.

| Method | Kernel and base support | Practical character |
| --- | --- | --- |
| `nearest` | Point selection, no interpolation | Preserves exact source samples and hard pixel-art edges, but can alias strongly. |
| `gaussian` | Gaussian weights, radius 1 | Smooth local averaging. |
| `hanning` | Raised-cosine Hanning weights, radius 1 | Smooth windowed interpolation. |
| `hamming` | Hamming cosine weights, radius 1 | Similar compact smoothing with different edge weights. |
| `bilinear` | Triangular weights, radius 1 | Low-cost general-purpose interpolation and the default. |
| `welsh` | Quadratic Welsh weights, radius 1 | Compact smooth interpolation. |
| `bicubic` | Cubic kernel, radius 2 | A wider, sharper reconstruction than the radius-1 filters. |
| `lanczos2` | Windowed sinc, radius 2 | Sharp reconstruction with possible ringing. |
| `lanczos3` | Windowed sinc, radius 3 | Wider reconstruction with more work and possible ringing. |
| `lanczos4` | Windowed sinc, radius 4 | Widest available reconstruction, with the greatest work and ringing exposure in this family. |

The filtered methods use normalized, separable horizontal and vertical passes. Their fixed base support keeps work linear in the number of processed pixels for a selected method, but wider kernels increase the constant cost. Large eligible byte-buffer resizes may execute row bands through the shared thread pool without changing the filter math. `nearest` uses a direct sample-selection path and avoids the filtered intermediate buffer.

There is no universally best filter. `nearest` is appropriate when exact block structure matters; `bilinear` is the compatibility default; wider bicubic or Lanczos kernels can retain more apparent sharpness but may ring near strong edges. Quality comparisons should fix the target geometry, colorspace and precision path, palette settings, and dither policy so filter differences are not confused with later quantization.

## Precision and colorspace

The normal quality-preserving resize of integer gamma RGB input converts the frame to linear RGB float32, resamples in linear light, and converts to the requested working representation afterward. This avoids treating gamma-encoded channel values as proportional light. The selected resampling kernel and the selected resize representation are independent controls.

The runtime policy `-j auto:resize_precision=preserve` keeps integer input in an `RGB888` resize path. It reduces the affected three-channel buffer cost from roughly twelve bytes per pixel to three, but interpolation then operates in gamma-encoded space. `resize_precision=linear` uses the transient linear-float32 resize path, while `resize_precision=float` also retains float32 for compatible later work. Explicit `--precision=float32` likewise requests a float32 main work path.

Allocation failure in the float path is an error rather than an automatic fallback to `preserve`, because that fallback would silently change image values. The detailed storage, quality measurement, memory accounting, and resolved-path diagnostics are in [Pixel-Format Precision](../concepts/pixelformat-precision.md) and [Encoder Working Precision](precision.md).

Use `-v` to inspect the actual plan rather than inferring it from the requested options alone. The planner dump reports crop/scale edges and fields such as `source`, `work`, `scale_out`, resize mode, and resize input format.

## Transparency and indexed input

A frame can carry a binary transparent mask separately from its color samples. Crop applies the same rectangle to the color plane and mask. Resize converts the mask to float coverage, applies the selected resampling method, and classifies output coverage at a threshold of `0.5`; an output sample at or above the threshold remains transparent.

Geometry processing of indexed transparent input can promote palette indices to RGB samples plus a transparent mask so interpolation does not treat a palette index as a color coordinate. Resize also prevents the loader's indexed-palette fast path from bypassing the required color interpolation. These rules preserve omitted-pixel semantics through geometry, while the later alpha policy still decides how those pixels map to SIXEL painting and `P2` behavior. See [Pixel Formats and Alpha Representation](../concepts/pixelformat.md) and the [Alpha Policy](../loader/alpha-policy.md).

The transparency mask is binary at the frame boundary; resize does not introduce fractional alpha into the SIXEL stream. Kernel choice can nevertheless move the `0.5` coverage boundary and therefore change which output pixels are omitted.

## Public C interfaces

The CLI builds its geometry preprocessing from the same frame and scale primitives available to embedders:

| Interface | Contract |
| --- | --- |
| `sixel_frame_clip()` | Crops a frame in place and applies the same geometry to its transparent mask. The caller must provide a valid in-bounds rectangle; CLI intersection and empty-region behavior belong to the higher-level clip filter. |
| `sixel_frame_resize()` | Resizes a frame in place, normalizing the color plane to `RGB888`, and resizes any transparent mask with the same method. |
| `sixel_frame_resize_float32()` | Resizes an `RGBFLOAT32` or `LINEARRGBFLOAT32` frame in place without converting it to byte RGB, and resizes any transparent mask. Other float coordinate systems are not accepted directly. |
| `sixel_helper_scale_image()` | Scales caller-provided byte pixels into caller-provided storage. It does not update frame dimensions, ownership, palette, colorspace, or transparency metadata. |
| `sixel_helper_scale_image_float32()` | Float32 counterpart of the low-level scale helper, with the same metadata boundary. |

The public constants and declarations are in [`include/sixel.h.in`](../../include/sixel.h.in). Frame mutation is implemented in [`src/frame.c`](../../src/frame.c), filter-level geometry in [`src/filter-clip.c`](../../src/filter-clip.c) and [`src/filter-resize.c`](../../src/filter-resize.c), target planning in [`src/planner.c`](../../src/planner.c), and the resampling kernels in [`src/scale.c`](../../src/scale.c).

Do not infer a decode-time resize feature from these helpers. The generic decoded-pixel result reports the raster represented by the SIXEL input; display-specific scaling belongs to the consuming application's presentation layer.

## Validation and focused tests

The crop/resize ordering contract is exercised by [`tests/loader/builtin/1513_loader_builtin_pal8_trns_clipfirst_order_preserved.t`](../../tests/loader/builtin/1513_loader_builtin_pal8_trns_clipfirst_order_preserved.t). Filter dispatch and dimension mutation are covered by [`tests/processing/filter/0003_filter_resize.c`](../../tests/processing/filter/0003_filter_resize.c). Transparent-mask crop and resize behavior is covered by the frame tests beginning with [`tests/processing/frame/0003_frame_transparent_mask_clip.c`](../../tests/processing/frame/0003_frame_transparent_mask_clip.c).

The geometry suite under [`tests/processing/geometry/`](../../tests/processing/geometry/) compares every exposed filter in representative downscale and upscale cases, checks representative dimension spellings, exercises the precision planner, and covers extreme resize and crop targets. Normal source changes in this area must pass `make staticcheck` followed by `make check`.
