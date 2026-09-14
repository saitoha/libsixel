# Pixel-Format Precision

Pixel-format precision describes how a decoded or intermediate color sample is stored. It is distinct from the color space that gives the sample meaning, the number of colors in the final palette, and the number of palette indices used by a packed pixel format. In libsixel, the important encoder distinction is between three unsigned 8-bit channels (`rgb888`) and three float32 channels (`rgb-f32` or a color-space-specific float format).

This document explains the storage contract and the operations that can promote a nominally 8-bit pipeline to float32. The broader effect of `--precision` on quantization, dithering, lookup, speed, and SIXEL size is measured in [Encoder working precision](../functionality/precision.md). The interpretation of gamma, linear RGB, Oklab, CIELAB, DIN99d, and ICC profiles is defined in [Color spaces and loader color management](colorspace.md).

## Storage and meaning are separate

`RGB888` stores red, green, and blue as integers from 0 through 255. It occupies three bytes per pixel, before stride, alignment, ownership metadata, and any concurrent buffers are counted. The name means 8 bits per channel, not 8 bits per complete pixel.

The float32 family stores three 32-bit floating-point channels and therefore occupies twelve bytes per pixel under the same simplifying assumptions. The format name also identifies the coordinates:

| Pixel format | Storage | Color-space interpretation |
| --- | --- | --- |
| `RGB888` | 3 x unsigned 8-bit | gamma-encoded sRGB |
| `RGBFLOAT32` (`rgb-f32` in planner diagnostics) | 3 x float32 | gamma-encoded sRGB |
| `LINEARRGBFLOAT32` (`linear-f32`) | 3 x float32 | linear-light sRGB primaries |
| `OKLABFLOAT32` (`oklab-f32`) | 3 x float32 | Oklab |
| `CIELABFLOAT32` (`cielab-f32`) | 3 x float32 | CIELAB |
| `DIN99DFLOAT32` (`din99d-f32`) | 3 x float32 | [DIN99d](din99d.md), with all three native coordinates divided by 100 |

A float32 value is not automatically linear light, and an 8-bit triplet cannot be reinterpreted as Oklab merely by changing a label. Every conversion must know both the source and destination color spaces. Palette-index formats such as `PAL8` describe index storage rather than color-sample precision and belong to the separate [pixel-format and alpha contract](pixelformat.md).

For a normalized encoded channel `x`, the byte boundary applies approximately

```text
q8(x) = round(255 * clamp(x, 0, 1)) / 255
```

so one crossing has at most `1 / 510` encoded-coordinate rounding error before clipping. The error is not uniform in linear light because the sRGB transfer curve is nonlinear. A normal float32 value carries 24 bits of significand precision, but this does not guarantee a 24-bit-accurate final color: transform approximations, clipping, palette reduction, 101-level SIXEL palette serialization, lookup, and dithering impose separate errors. The practical advantage is that intermediate transforms can retain far more information and avoid repeated byte quantization until a boundary that truly requires it.

## Float frame coordinate changes

Converting an existing float frame between gamma sRGB, linear sRGB, Oklab, CIELAB, and DIN99d preserves finite RGB values below zero and above one. These are intermediate coordinates, not a request to store a bounded device RGB value. The conversion validates the entire result before modifying the frame; a nonfinite input, a mathematical domain error, or a result that cannot fit finite float32 returns `SIXEL_BAD_INPUT` and preserves the pixel buffer and its format/color-space tags. The validation pass repeats the arithmetic to avoid allocating another full image buffer.

The frame component's pixel view exposes the current float storage and its matching tags. A clone owns an independent copy of those float samples. Converting that clone to `RGB888` intentionally clips and quantizes the clone while the original float frame retains its samples. Callers must keep the original frame when they need both representations; converting the original frame to bytes is a destructive storage boundary. These frame guarantees do not by themselves establish source-loader, alpha-composition, resize, queue, or palette preservation.

## Float geometry and failure state

Float frame resize retains negative components and values above one in both separable filter passes. The internal extended RGB scaler is distinct from the bounded float helper used for transparent-mask coverage and historical resampling measurements. The latter continues to clamp coverage in each pass, then the frame classifies coverage at or above one half as transparent. Color samples do not determine that mask threshold. The caller still supplies the intended RGB resampling basis; the operation preserves the frame's typed RGB format and colorspace.

A resize that changes dimensions requires finite source components, including for nearest neighbor, and finite float32 horizontal and final results. An intermediate outside float32 storage is an error even if another filter pass might later reduce it. Ordinary magnitudes retain the selected eligible SIMD implementation; very large magnitudes use double scalar accumulation centered on one source value so a representable average does not fail merely because an unnormalized float sum overflows. A request with unchanged dimensions performs no arithmetic and does not revalidate sample values.

For a normally initialized float frame, any resize allocation or numeric failure preserves the original pixel pointer and every sample, transparent-mask pointer and bytes, transparency marker, dimensions, pixelformat, and colorspace. This applies to both borrowed and frame-owned pixels. The new buffers become visible only after color and mask processing succeed. This guarantee does not redefine the existing permissive handling of malformed mask metadata.

Crop performs no numeric conversion: it moves the selected float bytes and mask bytes, including NaN, infinity, and signed zero. Direct float frame crop requires a positive rectangle wholly inside the source and rejects an invalid rectangle before changing frame state. Bounds checks subtract an already validated origin instead of adding possibly overflowing signed coordinates. The CLI crop filter retains its documented behavior of trimming oversized rectangles and skipping empty intersections before calling the frame operation; see [crop and resize](../functionality/crop-resize.md#cropping).

## Requested precision is not an upper bound

`--precision=8bit` requests the gamma `RGB888` base path. It is a preference for the lowest sufficient representation, not a prohibition on float32 intermediates. An operation whose mathematical domain requires linear RGB or a Lab-family space promotes the relevant part of the pipeline instead of rounding those coordinates into an undefined byte format.

`--precision=float32` requests gamma `RGBFLOAT32` for the main encoder work buffer. It does not imply that the loader must decode every source directly to float32, so a planner report can legitimately show `source=rgb888 work=rgb-f32`. Conversion at the boundary is explicit.

The following simplified flow shows the independently typed boundaries:

```text
encoded image
    |
    v
loader decode -- optional ICC/CMS transform --> source frame
    |
    v
optional pre-resize conversion --> resize --> optional post-resize conversion
    |
    v
working frame --> sampling/binning --> clustering-space palette view
    |
    v
palette application/dither --> output-space conversion --> SIXEL palette + indices
```

The `source`, resize input/output, main `work`, and clustering palette view need not share one format. Reporting only the command-line request loses information about the path that actually ran.

For why linear RGB is needed and when the planner inserts or skips conversions, see [Lazy conversion at operation boundaries](colorspace.md#lazy-conversion-at-operation-boundaries). A mathematical linear RGB or XYZ intermediate can exist inside a single-pixel transform without allocating a separate image buffer; precision promotion describes the storage that a processing stage actually requires.

## Operations that promote to float32

### Linear-light resize

The normal filtered-resize path converts a gamma byte source to `LINEARRGBFLOAT32`, resamples linear-light values, and converts to the requested working format afterward. With `--precision=8bit -Wgamma`, the float buffer is transient: planner diagnostics report `source=rgb888 work=rgb888 scale_out=linear-f32`. This is an implicit high-precision operation even though the main work buffer remains byte-based.

`-j auto:resize_precision=preserve` explicitly keeps an integer source in `RGB888` while resizing. It reduces the storage of each affected RGB buffer from twelve to three bytes per pixel, but interpolation then averages gamma-encoded values rather than light. This mode is a memory-recovery policy with an accuracy cost, not an equivalent implementation of the default.

`-j auto:resize_precision=float` retains a float32 main work buffer after linear-light resizing. Explicit `--precision=float32` also selects the float work path. If allocation of the required float buffer fails, libsixel stops rather than silently falling back to `preserve`; the diagnostic identifies the lower-memory command so the user makes the quality trade-off deliberately.

### Working color-space conversion

`-Wlinear`, `-Woklab`, `-Wcielab`, and `-Wdin99d` require color-space-specific float32 work formats. They therefore override an 8-bit base preference. `-Wgamma` is the only working space that can use either `RGB888` or `RGBFLOAT32`.

This order-independent rule is why these two commands have the same effective linear work format and, in the checked-in measurement, the same output hash:

```sh
img2sixel --precision=8bit -Xlinear -Wlinear image.png
img2sixel --precision=float32 -Xlinear -Wlinear image.png
```

The same equality holds for the measured Oklab pair. It proves that `--precision=8bit` does not truncate a float-only working space; it does not prove that every option outside the controlled pair is irrelevant.

`-X` selects the clustering coordinates rather than the main work coordinates. A non-gamma `-X` can create a float32 palette-construction view while `-Wgamma` remains byte-based. The detailed ownership and cost model are in [Palette clustering color space](../functionality/clustering-colorspace.md).

### Loader CMS output

When a loader applies an ICC transform, `cms_target` selects the target color space and `prefer_8bit` selects the requested representation after that transform. With `prefer_8bit=0`, gamma produces `RGBFLOAT32` and linear or Lab-family targets produce their typed float32 formats. A float source frame remains float through the planner so the encoder cannot silently down-convert it before palette construction.

`prefer_8bit=1` requests `RGB888`, even when `cms_target` names a non-gamma space. This is intentionally a request for an 8-bit gamma storage boundary, not an 8-bit encoding of linear RGB or Oklab. A later `-Wlinear` or Lab-family working-space conversion may promote that byte frame again, but information rounded at the CMS boundary cannot be reconstructed.

CMS promotion occurs only when the selected loader actually applies a color transform. A plain image without an applicable profile may remain `RGB888`; the presence of `--cms-engine` or a float-capable target name alone does not prove that a transform ran.

## Measured effective paths

The checked-in path inventory was recorded on 2026-09-09 from clean revision `8aa4fab9a` using planner and loader contract diagnostics. The selected rows below use `K=256`, K-means with fixed seed, hard binning, exact lookup, no dither, one thread, and CPU execution. Plain rows use [`images/snake.png`](../../images/snake.png); CMS rows use the embedded Adobe RGB fixture [`images/measurements/palette-pipeline/a98-gamut-600x450.png`](../../images/measurements/palette-pipeline/a98-gamut-600x450.png).

| Operation | Requested precision | Loader/source format | Effective work format | Resize output | Result |
| --- | --- | --- | --- | --- | --- |
| gamma baseline | 8-bit | `rgb888` | `rgb888` | `rgb888` | no promotion |
| gamma baseline | float32 | `rgb888` | `rgb-f32` | `rgb888` | explicit main-work promotion |
| `-Xlinear -Wlinear` | 8-bit | `rgb888` | `linear-f32` | `rgb888` | implicit working-space promotion |
| `-Xoklab -Woklab` | 8-bit | `rgb888` | `oklab-f32` | `rgb888` | implicit working-space promotion |
| 50% resize, default policy | 8-bit | `rgb888` | `rgb888` | `linear-f32` | transient linear-light promotion |
| 50% resize, `resize_precision=preserve` | 8-bit | `rgb888` | `rgb888` | `rgb888` | explicit byte path |
| CMS gamma, `prefer_8bit=0` | 8-bit | `rgb-f32` | `rgb-f32` | `rgb-f32` | float loader output is retained |
| CMS linear, `prefer_8bit=0` | 8-bit | `linear-f32` | `linear-f32` | `linear-f32` | typed float target is retained |
| CMS Oklab, `prefer_8bit=0` | 8-bit | `oklab-f32` | `oklab-f32` | `oklab-f32` | typed float target is retained |

The complete inventory, including the byte-preferred CMS controls, exact commands, SIXEL sizes, and output hashes, is in [`pixelformat-precision-paths.csv`](pixelformat-precision/measurements/pixelformat-precision-paths.csv). The measured requested-8-bit and requested-float32 outputs are byte-identical for the linear, Oklab, and CMS-Oklab pairs because each pair resolves to the same float-only path.

The Adobe RGB CMS linear rows are also byte-identical on this 8-bit source fixture even though one path crosses an `RGB888` source boundary and the other retains `LINEARRGBFLOAT32`. That observation must not be generalized into a claim that CMS precision never matters: the source has only 8-bit samples, the final SIXEL palette is quantized, and this one solver/input combination can mask intermediate rounding. The path inventory establishes retained precision; a quality benefit must be demonstrated by a case that is sensitive to it.

## Measured resize quality

The resize experiment uses a deterministic one-pixel black-and-white checker. Each 2-by-2 source cell contains equal black and white energy. The physically meaningful average is therefore linear intensity `0.5`, which the sRGB transfer function maps to `187.516...` and the reference rounds to byte tone `188`. Gamma-space averaging instead treats encoded channel numbers as light values.

![Quality of byte-preserving and float32 linear-light resize paths](pixelformat-precision/measurements/pixelformat-precision-resize.png)

The default automatic path resolves to a transient `linear-f32` resize even though the command requests `--precision=8bit`. It raises MS-SSIM from `0.961644` to `0.999993` and lowers mean Delta E00 from `9.971917` to `0.168512` against the analytical reference. Retaining float32 for the later work path produces the same quantized output on this fixture, so the gain here comes from linear-light resizing rather than from a different palette or dither policy.

This is deliberately a stress test at the spatial Nyquist limit, not an estimate of the average improvement on photographs. It isolates the mathematical error, while the large margin makes a regression easy to detect. The experiment fixes bilinear filtering, `K=256`, K-means with a fixed seed, hard binning, exact lookup, dither off, one thread, and CPU execution. MS-SSIM describes the spatial result, while mean Delta E00 describes pointwise perceptual color error. The raw measurements are in [`pixelformat-precision-resize.csv`](pixelformat-precision/measurements/pixelformat-precision-resize.csv).

## Cost model and policy choice

For `N` pixels, a three-channel `RGB888` buffer requires approximately `3N` bytes and a three-channel float32 buffer requires approximately `12N` bytes. Conversion is `O(N)`. Resampling remains linear in output pixels for a fixed-support filter, but float arithmetic and pre/post color conversion increase its constant cost. Peak memory can include source, converted input, resized output, work, palette, and threaded scratch buffers simultaneously, so multiplying one buffer size is not a complete peak-memory estimate.

Use the default resize policy when accuracy is the priority. Use `resize_precision=preserve` only when the lower peak-memory path is needed and the gamma-space interpolation error is acceptable. Use `--precision=float32` when later quantization, lookup, or dithering should also remain in the high-precision gamma path. Selecting a non-gamma `-W` already requires float32, so adding `--precision=float32` documents intent but does not create a distinct lower-level implementation.

## Observing the resolved path

Verbose planner output is the authoritative local check:

```sh
img2sixel --precision=8bit -Wgamma -w50% -v -o /dev/null image.png
```

Read all of these fields together:

```text
formats: source=rgb888 work=rgb888 scale_out=linear-f32
resize: mode=2 input=linear-f32
```

`work=rgb888` alone would miss the transient float resize. For loader CMS work, enable the stable loader trace as well:

```sh
SIXEL_TRACE_TOPIC=loader_contract img2sixel \
  --cms-engine=builtin \
  -L 'builtin:cms_engine=builtin:cms_target=oklab:prefer_8bit=0!' \
  -Xoklab -Woklab -v -o /dev/null image.png
```

## Reproduction and validation

Commit the implementation being measured, provide Python with Matplotlib, and run from a tracked-clean worktree:

```sh
PYTHON=.venv/bin/python tools/reproduce_pixelformat_precision_measurements.sh
```

The runner rebuilds `img2sixel` and `lsqa`, removes inherited `SIXEL_*` and `LSQA_*` policy variables, generates the checker and analytical reference in a temporary directory, records stable planner/loader contracts, creates the figure and CSV files, and validates the result. Source revision, input and executable hashes, host information, and the complete protocol are stored in [`pixelformat-precision-run.json`](pixelformat-precision/measurements/pixelformat-precision-run.json).

Validate an existing artifact directory without Matplotlib:

```sh
python3 tools/check_pixelformat_precision_measurements.py \
  docs/concepts/pixelformat-precision/measurements
```

Rerun the measurement after changes to loader CMS output selection, frame pixel-format conversion, resize planning or filtering, working-space resolution, palette construction, palette application, SIXEL writing, `lsqa`, or the measurement tools.

## Implementation and tests

Effective path selection is implemented in [`src/planner.c`](../../src/planner.c) and [`src/encoder.c`](../../src/encoder.c). CMS output selection is implemented in [`src/loader-common.c`](../../src/loader-common.c), and typed conversion is implemented in [`src/pixelformat.c`](../../src/pixelformat.c).

Focused regression coverage includes [`tests/cli/options/matching/0269_option_matching_precision_8bit_working_gamma_order.t`](../../tests/cli/options/matching/0269_option_matching_precision_8bit_working_gamma_order.t), [`tests/cli/options/matching/0270_option_matching_precision_8bit_float_working_space.t`](../../tests/cli/options/matching/0270_option_matching_precision_8bit_float_working_space.t), [`tests/cli/options/matching/0272_option_matching_precision_float32_working_gamma.t`](../../tests/cli/options/matching/0272_option_matching_precision_float32_working_gamma.t), [`tests/processing/planner/0001_resize_linear_float32.c`](../../tests/processing/planner/0001_resize_linear_float32.c), [`tests/processing/planner/0002_resize_allocation_failure.c`](../../tests/processing/planner/0002_resize_allocation_failure.c), [`tests/cli/options/regression/0109_loader_cms_target_image_regression.t`](../../tests/cli/options/regression/0109_loader_cms_target_image_regression.t), and [`tests/cli/options/regression/0110_loader_prefer_8bit_image_regression.t`](../../tests/cli/options/regression/0110_loader_prefer_8bit_image_regression.t).

The measurement driver is [`tools/plot_pixelformat_precision_measurements.py`](../../tools/plot_pixelformat_precision_measurements.py), the one-command wrapper is [`tools/reproduce_pixelformat_precision_measurements.sh`](../../tools/reproduce_pixelformat_precision_measurements.sh), and the artifact contract is enforced by [`tools/check_pixelformat_precision_measurements.py`](../../tools/check_pixelformat_precision_measurements.py).

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The coordinate tests use independent Decimal-derived reference values and a `1e-6` absolute tolerance for float32 input/output rounding and the stored opponent-space matrices. Each direction has its own expected values; a production roundtrip is not the oracle.

| ID | Contract | Owning test |
| --- | --- | --- |
| FP-01 | Extended linear RGB to gamma sRGB. | [tests/processing/frame/0006_frame_extended_linear_gamma.t](../../tests/processing/frame/0006_frame_extended_linear_gamma.t) |
| FP-02 | Extended gamma sRGB to linear RGB. | [tests/processing/frame/0007_frame_extended_gamma_linear.t](../../tests/processing/frame/0007_frame_extended_gamma_linear.t) |
| FP-03 | Extended linear RGB to Oklab. | [tests/processing/frame/0008_frame_extended_linear_oklab.t](../../tests/processing/frame/0008_frame_extended_linear_oklab.t) |
| FP-04 | Oklab to extended linear RGB. | [tests/processing/frame/0009_frame_extended_oklab_linear.t](../../tests/processing/frame/0009_frame_extended_oklab_linear.t) |
| FP-05 | Extended linear RGB to CIELAB. | [tests/processing/frame/0010_frame_extended_linear_cielab.t](../../tests/processing/frame/0010_frame_extended_linear_cielab.t) |
| FP-06 | CIELAB to extended linear RGB. | [tests/processing/frame/0011_frame_extended_cielab_linear.t](../../tests/processing/frame/0011_frame_extended_cielab_linear.t) |
| FP-07 | Extended linear RGB to DIN99d. | [tests/processing/frame/0012_frame_extended_linear_din99d.t](../../tests/processing/frame/0012_frame_extended_linear_din99d.t) |
| FP-08 | DIN99d to extended linear RGB. | [tests/processing/frame/0013_frame_extended_din99d_linear.t](../../tests/processing/frame/0013_frame_extended_din99d_linear.t) |
| FP-09 | A clone starts with identical float samples; its byte conversion preserves the original float frame. | [tests/processing/frame/0014_frame_clone_float_canonical.t](../../tests/processing/frame/0014_frame_clone_float_canonical.t) |
| FP-10 | A nonfinite final pixel rejects conversion without changing the valid prefix or tags. | [tests/processing/frame/0015_frame_float_nonfinite_atomic.t](../../tests/processing/frame/0015_frame_float_nonfinite_atomic.t) |
| FP-11 | A final pixel outside the DIN99d logarithm domain leaves all pixels and tags unchanged. | [tests/processing/frame/0016_frame_float_domain_atomic.t](../../tests/processing/frame/0016_frame_float_domain_atomic.t) |
| FP-12 | A result beyond finite float32 storage rejects conversion without changing any pixels or tags. | [tests/processing/frame/0017_frame_float_overflow_atomic.t](../../tests/processing/frame/0017_frame_float_overflow_atomic.t) |

### Float geometry contract tests

The [tests/processing/frame/0018_frame_float_resampling.c](../../tests/processing/frame/0018_frame_float_resampling.c) uses constant preservation, independent triangle weights, and the existing cubic kernel's exact rational quarter-pixel weights. It compares channel- and pixel-varying values with a `2e-6` absolute tolerance; the finite `FLT_MAX` constant average uses a `2e-7` relative bound. Nonconstant extreme inputs normalize by their exact float magnitude before comparison at the same `2e-6` bound. The mixed-sign downscale has independent normalized results `3/7, 3/7, 1/49`; the large cubic case uses fields `2*x-1, 2*y-1, (2*x-1)*(2*y-1)` scaled by `FLT_MAX/8`. Bilinear downscale uses separable weights `2:3:2`. For cubic enlargement, the right-hand source weights are `-3/16, 1/4, 3/4, 19/16`; RGB fields `x, y, x*y` distinguish both axes and intermediate clipping. These are formula-derived values, not saved scaler output.

The [tests/processing/frame/0054_frame_float_geometry_state.c](../../tests/processing/frame/0054_frame_float_geometry_state.c) compares pixel and mask storage exactly after failures and crop. Its successful resize independently interpolates a linear color ramp and binary checker coverage. SIMD-specific owners explicitly skip when either the compiled implementation or CPU path is unavailable; an `auto` or ceiling request alone is not evidence that AVX, SSE2, or NEON executed. All small buffers are also exercised by the sanitizer build.

| ID | Contract | Owning tests |
| --- | --- | --- |
| FP-13 | Resize constant nearest. | [tests/processing/frame/0018_frame_float_resize_constant_nearest_scalar.t](../../tests/processing/frame/0018_frame_float_resize_constant_nearest_scalar.t), [tests/processing/frame/0019_frame_float_resize_constant_nearest_auto.t](../../tests/processing/frame/0019_frame_float_resize_constant_nearest_auto.t) |
| FP-14 | Resize constant gaussian. | [tests/processing/frame/0020_frame_float_resize_constant_gaussian_scalar.t](../../tests/processing/frame/0020_frame_float_resize_constant_gaussian_scalar.t), [tests/processing/frame/0021_frame_float_resize_constant_gaussian_auto.t](../../tests/processing/frame/0021_frame_float_resize_constant_gaussian_auto.t) |
| FP-15 | Resize constant hanning. | [tests/processing/frame/0022_frame_float_resize_constant_hanning_scalar.t](../../tests/processing/frame/0022_frame_float_resize_constant_hanning_scalar.t), [tests/processing/frame/0023_frame_float_resize_constant_hanning_auto.t](../../tests/processing/frame/0023_frame_float_resize_constant_hanning_auto.t) |
| FP-16 | Resize constant hamming. | [tests/processing/frame/0024_frame_float_resize_constant_hamming_scalar.t](../../tests/processing/frame/0024_frame_float_resize_constant_hamming_scalar.t), [tests/processing/frame/0025_frame_float_resize_constant_hamming_auto.t](../../tests/processing/frame/0025_frame_float_resize_constant_hamming_auto.t) |
| FP-17 | Resize constant bilinear. | [tests/processing/frame/0026_frame_float_resize_constant_bilinear_scalar.t](../../tests/processing/frame/0026_frame_float_resize_constant_bilinear_scalar.t), [tests/processing/frame/0027_frame_float_resize_constant_bilinear_auto.t](../../tests/processing/frame/0027_frame_float_resize_constant_bilinear_auto.t) |
| FP-18 | Resize constant welsh. | [tests/processing/frame/0028_frame_float_resize_constant_welsh_scalar.t](../../tests/processing/frame/0028_frame_float_resize_constant_welsh_scalar.t), [tests/processing/frame/0029_frame_float_resize_constant_welsh_auto.t](../../tests/processing/frame/0029_frame_float_resize_constant_welsh_auto.t) |
| FP-19 | Resize constant bicubic. | [tests/processing/frame/0030_frame_float_resize_constant_bicubic_scalar.t](../../tests/processing/frame/0030_frame_float_resize_constant_bicubic_scalar.t), [tests/processing/frame/0031_frame_float_resize_constant_bicubic_auto.t](../../tests/processing/frame/0031_frame_float_resize_constant_bicubic_auto.t) |
| FP-20 | Resize constant lanczos2. | [tests/processing/frame/0032_frame_float_resize_constant_lanczos2_scalar.t](../../tests/processing/frame/0032_frame_float_resize_constant_lanczos2_scalar.t), [tests/processing/frame/0033_frame_float_resize_constant_lanczos2_auto.t](../../tests/processing/frame/0033_frame_float_resize_constant_lanczos2_auto.t) |
| FP-21 | Resize constant lanczos3. | [tests/processing/frame/0034_frame_float_resize_constant_lanczos3_scalar.t](../../tests/processing/frame/0034_frame_float_resize_constant_lanczos3_scalar.t), [tests/processing/frame/0035_frame_float_resize_constant_lanczos3_auto.t](../../tests/processing/frame/0035_frame_float_resize_constant_lanczos3_auto.t) |
| FP-22 | Resize constant lanczos4. | [tests/processing/frame/0036_frame_float_resize_constant_lanczos4_scalar.t](../../tests/processing/frame/0036_frame_float_resize_constant_lanczos4_scalar.t), [tests/processing/frame/0037_frame_float_resize_constant_lanczos4_auto.t](../../tests/processing/frame/0037_frame_float_resize_constant_lanczos4_auto.t) |
| FP-23 | Resize bilinear. | [tests/processing/frame/0038_frame_float_resize_bilinear_scalar.t](../../tests/processing/frame/0038_frame_float_resize_bilinear_scalar.t), [tests/processing/frame/0039_frame_float_resize_bilinear_auto.t](../../tests/processing/frame/0039_frame_float_resize_bilinear_auto.t) |
| FP-24 | Resize down. | [tests/processing/frame/0040_frame_float_resize_down_scalar.t](../../tests/processing/frame/0040_frame_float_resize_down_scalar.t), [tests/processing/frame/0041_frame_float_resize_down_auto.t](../../tests/processing/frame/0041_frame_float_resize_down_auto.t) |
| FP-25 | Resize bicubic. | [tests/processing/frame/0042_frame_float_resize_bicubic_scalar.t](../../tests/processing/frame/0042_frame_float_resize_bicubic_scalar.t), [tests/processing/frame/0043_frame_float_resize_bicubic_auto.t](../../tests/processing/frame/0043_frame_float_resize_bicubic_auto.t) |
| FP-26 | Resize nearest. | [tests/processing/frame/0044_frame_float_resize_nearest_scalar.t](../../tests/processing/frame/0044_frame_float_resize_nearest_scalar.t), [tests/processing/frame/0045_frame_float_resize_nearest_auto.t](../../tests/processing/frame/0045_frame_float_resize_nearest_auto.t) |
| FP-27 | Resize large. | [tests/processing/frame/0046_frame_float_resize_large_scalar.t](../../tests/processing/frame/0046_frame_float_resize_large_scalar.t), [tests/processing/frame/0047_frame_float_resize_large_auto.t](../../tests/processing/frame/0047_frame_float_resize_large_auto.t) |
| FP-28 | Resize nonfinite. | [tests/processing/frame/0048_frame_float_resize_nonfinite_scalar.t](../../tests/processing/frame/0048_frame_float_resize_nonfinite_scalar.t), [tests/processing/frame/0049_frame_float_resize_nonfinite_auto.t](../../tests/processing/frame/0049_frame_float_resize_nonfinite_auto.t) |
| FP-29 | Resize overflow. | [tests/processing/frame/0050_frame_float_resize_overflow_scalar.t](../../tests/processing/frame/0050_frame_float_resize_overflow_scalar.t), [tests/processing/frame/0051_frame_float_resize_overflow_auto.t](../../tests/processing/frame/0051_frame_float_resize_overflow_auto.t) |
| FP-30 | Resize nonfinite nearest. | [tests/processing/frame/0052_frame_float_resize_nonfinite_nearest_scalar.t](../../tests/processing/frame/0052_frame_float_resize_nonfinite_nearest_scalar.t), [tests/processing/frame/0053_frame_float_resize_nonfinite_nearest_auto.t](../../tests/processing/frame/0053_frame_float_resize_nonfinite_nearest_auto.t) |
| FP-31 | Clip gamma bits. | [tests/processing/frame/0054_frame_float_clip_gamma_bits.t](../../tests/processing/frame/0054_frame_float_clip_gamma_bits.t) |
| FP-32 | Clip linear bits. | [tests/processing/frame/0055_frame_float_clip_linear_bits.t](../../tests/processing/frame/0055_frame_float_clip_linear_bits.t) |
| FP-33 | Clip oklab bits. | [tests/processing/frame/0056_frame_float_clip_oklab_bits.t](../../tests/processing/frame/0056_frame_float_clip_oklab_bits.t) |
| FP-34 | Clip cielab bits. | [tests/processing/frame/0057_frame_float_clip_cielab_bits.t](../../tests/processing/frame/0057_frame_float_clip_cielab_bits.t) |
| FP-35 | Clip din99d bits. | [tests/processing/frame/0058_frame_float_clip_din99d_bits.t](../../tests/processing/frame/0058_frame_float_clip_din99d_bits.t) |
| FP-36 | Clip negative x atomic. | [tests/processing/frame/0059_frame_float_clip_negative_x_atomic.t](../../tests/processing/frame/0059_frame_float_clip_negative_x_atomic.t) |
| FP-37 | Clip negative y atomic. | [tests/processing/frame/0060_frame_float_clip_negative_y_atomic.t](../../tests/processing/frame/0060_frame_float_clip_negative_y_atomic.t) |
| FP-38 | Clip right atomic. | [tests/processing/frame/0061_frame_float_clip_right_atomic.t](../../tests/processing/frame/0061_frame_float_clip_right_atomic.t) |
| FP-39 | Clip bottom atomic. | [tests/processing/frame/0062_frame_float_clip_bottom_atomic.t](../../tests/processing/frame/0062_frame_float_clip_bottom_atomic.t) |
| FP-40 | Clip x overflow atomic. | [tests/processing/frame/0063_frame_float_clip_x_overflow_atomic.t](../../tests/processing/frame/0063_frame_float_clip_x_overflow_atomic.t) |
| FP-41 | Clip y overflow atomic. | [tests/processing/frame/0064_frame_float_clip_y_overflow_atomic.t](../../tests/processing/frame/0064_frame_float_clip_y_overflow_atomic.t) |
| FP-42 | Clip cli trim. | [tests/processing/frame/0065_frame_float_clip_cli_trim.t](../../tests/processing/frame/0065_frame_float_clip_cli_trim.t) |
| FP-43 | Clip cli empty. | [tests/processing/frame/0066_frame_float_clip_cli_empty.t](../../tests/processing/frame/0066_frame_float_clip_cli_empty.t) |
| FP-44 | Resize pixels atomic. | [tests/processing/frame/0067_frame_float_resize_pixels_borrowed_atomic.t](../../tests/processing/frame/0067_frame_float_resize_pixels_borrowed_atomic.t), [tests/processing/frame/0068_frame_float_resize_pixels_owned_atomic.t](../../tests/processing/frame/0068_frame_float_resize_pixels_owned_atomic.t) |
| FP-45 | Resize scratch atomic. | [tests/processing/frame/0069_frame_float_resize_scratch_borrowed_atomic.t](../../tests/processing/frame/0069_frame_float_resize_scratch_borrowed_atomic.t), [tests/processing/frame/0070_frame_float_resize_scratch_owned_atomic.t](../../tests/processing/frame/0070_frame_float_resize_scratch_owned_atomic.t) |
| FP-46 | Resize mask source atomic. | [tests/processing/frame/0071_frame_float_resize_mask_source_borrowed_atomic.t](../../tests/processing/frame/0071_frame_float_resize_mask_source_borrowed_atomic.t), [tests/processing/frame/0072_frame_float_resize_mask_source_owned_atomic.t](../../tests/processing/frame/0072_frame_float_resize_mask_source_owned_atomic.t) |
| FP-47 | Resize mask target atomic. | [tests/processing/frame/0073_frame_float_resize_mask_target_borrowed_atomic.t](../../tests/processing/frame/0073_frame_float_resize_mask_target_borrowed_atomic.t), [tests/processing/frame/0074_frame_float_resize_mask_target_owned_atomic.t](../../tests/processing/frame/0074_frame_float_resize_mask_target_owned_atomic.t) |
| FP-48 | Resize mask bytes atomic. | [tests/processing/frame/0075_frame_float_resize_mask_bytes_borrowed_atomic.t](../../tests/processing/frame/0075_frame_float_resize_mask_bytes_borrowed_atomic.t), [tests/processing/frame/0076_frame_float_resize_mask_bytes_owned_atomic.t](../../tests/processing/frame/0076_frame_float_resize_mask_bytes_owned_atomic.t) |
| FP-49 | Resize mask scratch atomic. | [tests/processing/frame/0077_frame_float_resize_mask_scratch_borrowed_atomic.t](../../tests/processing/frame/0077_frame_float_resize_mask_scratch_borrowed_atomic.t), [tests/processing/frame/0078_frame_float_resize_mask_scratch_owned_atomic.t](../../tests/processing/frame/0078_frame_float_resize_mask_scratch_owned_atomic.t) |
| FP-50 | Resize nonfinite atomic. | [tests/processing/frame/0079_frame_float_resize_nonfinite_borrowed_atomic.t](../../tests/processing/frame/0079_frame_float_resize_nonfinite_borrowed_atomic.t), [tests/processing/frame/0080_frame_float_resize_nonfinite_owned_atomic.t](../../tests/processing/frame/0080_frame_float_resize_nonfinite_owned_atomic.t) |
| FP-51 | Resize overflow atomic. | [tests/processing/frame/0081_frame_float_resize_overflow_borrowed_atomic.t](../../tests/processing/frame/0081_frame_float_resize_overflow_borrowed_atomic.t), [tests/processing/frame/0082_frame_float_resize_overflow_owned_atomic.t](../../tests/processing/frame/0082_frame_float_resize_overflow_owned_atomic.t) |
| FP-52 | Resize mask. | [tests/processing/frame/0083_frame_float_resize_mask_borrowed.t](../../tests/processing/frame/0083_frame_float_resize_mask_borrowed.t), [tests/processing/frame/0084_frame_float_resize_mask_owned.t](../../tests/processing/frame/0084_frame_float_resize_mask_owned.t) |
| FP-53 | Resize bilinear avx. | [tests/processing/frame/0085_frame_float_resize_bilinear_avx.t](../../tests/processing/frame/0085_frame_float_resize_bilinear_avx.t) |
| FP-54 | Resize bicubic avx. | [tests/processing/frame/0086_frame_float_resize_bicubic_avx.t](../../tests/processing/frame/0086_frame_float_resize_bicubic_avx.t) |
| FP-55 | Resize bilinear sse2. | [tests/processing/frame/0087_frame_float_resize_bilinear_sse2.t](../../tests/processing/frame/0087_frame_float_resize_bilinear_sse2.t) |
| FP-56 | Resize bicubic sse2. | [tests/processing/frame/0088_frame_float_resize_bicubic_sse2.t](../../tests/processing/frame/0088_frame_float_resize_bicubic_sse2.t) |
| FP-57 | Resize bilinear neon. | [tests/processing/frame/0089_frame_float_resize_bilinear_neon.t](../../tests/processing/frame/0089_frame_float_resize_bilinear_neon.t) |
| FP-58 | Resize bicubic neon. | [tests/processing/frame/0090_frame_float_resize_bicubic_neon.t](../../tests/processing/frame/0090_frame_float_resize_bicubic_neon.t) |
| FP-59 | Bounded coverage clamps both axes (scalar). | [tests/processing/frame/0091_frame_float_mask_coverage_scalar.t](../../tests/processing/frame/0091_frame_float_mask_coverage_scalar.t) |
| FP-60 | Bounded coverage clamps both axes (auto). | [tests/processing/frame/0092_frame_float_mask_coverage_auto.t](../../tests/processing/frame/0092_frame_float_mask_coverage_auto.t) |
| FP-61 | Bounded coverage clamps both axes (avx). | [tests/processing/frame/0093_frame_float_mask_coverage_avx.t](../../tests/processing/frame/0093_frame_float_mask_coverage_avx.t) |
| FP-62 | Nonconstant extreme mixed values (scalar). | [tests/processing/frame/0094_frame_float_resize_mixed_scalar.t](../../tests/processing/frame/0094_frame_float_resize_mixed_scalar.t) |
| FP-63 | Nonconstant extreme mixed values (auto). | [tests/processing/frame/0095_frame_float_resize_mixed_auto.t](../../tests/processing/frame/0095_frame_float_resize_mixed_auto.t) |
| FP-64 | Nonconstant extreme large bicubic values (scalar). | [tests/processing/frame/0096_frame_float_resize_large_bicubic_scalar.t](../../tests/processing/frame/0096_frame_float_resize_large_bicubic_scalar.t) |
| FP-65 | Nonconstant extreme large bicubic values (auto). | [tests/processing/frame/0097_frame_float_resize_large_bicubic_auto.t](../../tests/processing/frame/0097_frame_float_resize_large_bicubic_auto.t) |
