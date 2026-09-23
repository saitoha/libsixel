# Pixel-Format Precision

For backend representation, color/alpha preservation and byte-path eligibility, see the [loader precision policy](../loader/precision-policy.md).

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

## Storage precision and arithmetic precision

A float32 path describes the stored samples, not the required type of every intermediate expression. Color-space conversion can require `double` for matrix products, chromatic adaptation, transfer functions, and accumulation even when both input and output buffers are float32. Signed matrix terms can nearly cancel, and a sequence of nonlinear and inverse operations can magnify earlier rounding. A result that fits float32 does not establish that all intermediate float32 operations are accurate or even finite.

Widen the operands before the sensitive calculation. Assigning an already rounded or overflowing float expression to a double variable cannot recover its information. Likewise, promoting an image after an earlier byte conversion or gamut clamp cannot undo that loss. The internal `sixel_color_convert_rgbf32()` in [`src/colorspace.c`](../../src/colorspace.c) loads float samples into double triplets, performs the coordinate conversion there, validates the result, and only then stores float samples. This does not imply double image buffers or that every CMS/SIMD backend uses identical arithmetic.

Keep coordinate conversion ahead of storage reduction. For example, linear `0.001` becomes encoded sRGB `0.01292`, which rounds to byte 3; rounding the linear value to an 8-bit index first produces zero, and a subsequent byte transfer lookup cannot recover it. An explicit frame setter request for `RGB888` now reads the original float coordinates into double triplets, converts directly to gamma sRGB, and clips and rounds only the final byte values. It does not require an intermediate gamma float32 allocation. Gamma float input already has the requested coordinates and bypasses a transfer roundtrip. The libpng and builtin PNG explicit 8-bit CMS targets perform their source interpretation and composition before reaching this boundary.

Widening also matters during the final byte rounding itself. The stored gamma float with bits `0x3b008080` is exactly `0.0019607841968536376953125`. Multiplying by 255 and adding 0.5 in double remains below 1 and produces byte 0. The same multiplication and addition in float round up to 1 and produce byte 1. A float image buffer therefore does not imply that its final rounding arithmetic should also use float.

The explicit float-to-byte setter publishes its new buffer and gamma tags only after every triplet succeeds. Allocation failure, nonfinite source samples, a coordinate-domain or double overflow failure, or invalid dimensions leave the original pixels, pointer, dimensions, tags, transparency mask, and ownership unchanged. Successful conversion releases owned source pixels once and leaves borrowed source storage untouched. A finite intermediate value may exceed `FLT_MAX` yet fit double and produce a valid bounded byte; such a value does not require float32 storage on this path. The private storage normalizer and byte resize API retain their separate contracts.

The independent [byte-boundary fixtures](../../tests/data/color-numeric/frame-byte.json) start from exact stored IEEE binary32 bits. The generator solves the forward Oklab and legacy Lab matrices independently, evaluates inverse coordinate equations in Decimal, and computes final gamma/byte values without calling production transforms. Gamma and linear cases include the nearest float and its two adjacent values at all 255 half-byte boundaries, as well as dark, negative, above-one, and finite extreme inputs. The tests compare final bytes exactly through direct, interface, borrowed, owned, and cloned frames; failed conversions also preserve the full original state.

For IEEE binary32 and binary64, normal values have 24 and 53 significant binary digits, respectively. These are relative precisions: the spacing between representable numbers grows with their magnitude. A wider exponent range reduces overflow risk, while more significant digits reduce rounding error; neither property makes arbitrary arithmetic exact. Goldberg's [What Every Computer Scientist Should Know About Floating-Point Arithmetic](https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html) explains the distinction between representable range, rounding, and cancellation.

### Double does not eliminate cancellation

Let `M = 2^100`, which is exactly representable in both binary32 and binary64. With binary64 operations and ordinary round-to-nearest evaluation:

```text
exact arithmetic:   M + (1 - M) = 1
binary64:           1 - M rounds to -M; the final sum is 0
```

The problem is the difference in scale between `M` and `1`, not whether either input fits in storage. Converting the final zero to float32 cannot restore the lost one. In a color matrix or a weighted sum, a small component can similarly disappear between large positive and negative terms. Mathematically equivalent rearrangements therefore need not have equivalent numerical behavior.

Resampling is the weighted mean `sum(w_i * x_i) / sum(w_i)`. Rewriting it as `a + sum(w_i * (x_i - a)) / sum(w_i)` introduces an arbitrary reference value `a`; it is not a required neighboring-pixel difference operation. If a zero-weight extreme sample supplies `a`, this rearrangement can erase the sole contributing ordinary sample. Direct compensated accumulation, excluding zero-weight terms, and preserving one-contributor or constant identities address different failure modes. Symmetric filter weights also need symmetric evaluation: compensation for addition cannot repair coefficients that were already rounded differently. The implemented choices and their storage limits are in [Float geometry and failure state](#float-geometry-and-failure-state).

Underflow is a separate concern. Two contributions of half the smallest positive float32 subnormal can each round to zero even though their sum should be that subnormal. A SIMD recovery decision must account for this absolute rounding error as well as cancellation between normal values. Wider arithmetic and recovery checks also have a cost; measure ordinary images as well as extreme numerical specimens before adopting an implementation strategy.

### Numerical validation follows the operation

Validate color conversion with independent expected coordinates in each direction, including mixed channels, transfer-curve boundaries, negative and above-one values, and mathematical-domain failures. A forward/reverse pair can share an error and still roundtrip successfully. Validate resampling with independent weights, zero-weight extremes, small residuals between opposite large contributions, constants, and subnormal inputs. Compare a small expected residual at its own scale; a tolerance scaled by the largest input can accept its complete loss. The [coordinate and geometry coverage](#test-coverage) separates these value contracts from image-quality thresholds and checks the executed SIMD or scalar path.

Numerical range checks and color-gamut decisions are separate. Keeping extended coordinates does not mean accepting NaN, infinity, undefined nonlinear operations, or a value that cannot fit the next storage representation. Conversely, clamping finite coordinates into `[0,1]` is not a numerical-accuracy repair. See [Out-of-gamut and imaginary colors](colorspace.md#out-of-gamut-and-imaginary-colors) for the color meaning of those intermediates.

## Float frame coordinate changes

Converting an existing float frame between gamma sRGB, linear sRGB, Oklab, CIELAB, and DIN99d preserves finite RGB values below zero and above one. These are intermediate coordinates, not a request to store a bounded device RGB value. The conversion validates the entire result before modifying the frame; a nonfinite input, a mathematical domain error, or a result that cannot fit finite float32 returns `SIXEL_BAD_INPUT` and preserves the pixel buffer and its format/color-space tags. The validation pass repeats the arithmetic to avoid allocating another full image buffer.

The frame component's pixel view exposes the current float storage and its matching tags. A clone owns an independent copy of those float samples. Converting that clone to `RGB888` intentionally clips and quantizes the clone while the original float frame retains its samples. Callers must keep the original frame when they need both representations; converting the original frame to bytes is a destructive storage boundary. These frame guarantees do not by themselves establish source-loader, alpha-composition, resize, queue, or palette preservation.

## Float geometry and failure state

Float frame resize retains negative components and values above one in both separable filter passes. The internal extended RGB scaler is distinct from the bounded float helper used for transparent-mask coverage and historical resampling measurements. The latter continues to clamp coverage in each pass, then the frame classifies coverage at or above one half as transparent. Color samples do not determine that mask threshold. The caller still supplies the intended RGB resampling basis; the operation preserves the frame's typed RGB format and colorspace.

A resize that changes dimensions requires finite source components, including for nearest neighbor, and finite float32 horizontal and final results. An intermediate outside float32 storage is an error even if another filter pass might later reduce it. Ordinary magnitudes retain the selected eligible SIMD implementation; very large magnitudes use compensated double sums so a representable average does not fail merely because an unnormalized float sum overflows. Extended-color footprints with unresolved SIMD cancellation also use that scalar calculation. The dispatch decision uses a bound on accumulated float roundoff, including an absolute gradual-underflow term and does not relax the finite-result or float32-storage requirements. Zero-weight samples do not contribute. Scalar constant footprints, including `FLT_MAX`, and single-contributor footprints preserve their sample value exactly. A request with unchanged dimensions performs no arithmetic and does not revalidate sample values.

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

When a loader applies an ICC transform, `cms_target` selects the target color space and `prefer_8bit` selects the requested representation after that transform. With `prefer_8bit=0`, gamma produces `RGBFLOAT32` and linear or Lab-family targets produce their typed float32 formats. A float source frame remains float through the planner, including with a `-.8bit` base preference, so the encoder cannot silently down-convert it before palette construction.

`prefer_8bit=1` requests `RGB888`, even when `cms_target` names a non-gamma space. This is intentionally a request for an 8-bit gamma storage boundary, not an 8-bit encoding of linear RGB or Oklab. A later `-Wlinear` or Lab-family working-space conversion may promote that byte frame again, but information rounded at the CMS boundary cannot be reconstructed.

CMS promotion occurs only when the selected loader actually applies a color transform. A plain image without an applicable profile may remain `RGB888`; the presence of `--cms-engine` or a float-capable target name alone does not prove that a transform ran.

### Native loader intermediates

PNG chromaticities also illustrate the difference between exact classification and numerical evaluation. After the external getter and coordinate checks, the libpng adapter reconstructs the 100000-scale integers and rejects exactly collinear primaries using an `int64_t` signed area. A mathematically zero determinant can acquire a floating residual larger than a singularity tolerance. Each integer product is bounded by `1e10` and the uncoupled subtraction by `2e10`, so a 32-bit intermediate is insufficient. This guard preserves the surviving-metadata [fallback contract](../loader/png-color-metadata.md#exact-collinear-fallback-contracts); it does not improve the subsequent floating matrix solve, change coefficient storage, or remove the pre-matrix float32 store.

A float output buffer does not by itself prove that earlier native drawing or color-conversion stages retained source precision. The [CoreGraphics PNG source path](../loader/coregraphics.md#why-a-float-destination-is-insufficient) reads original integer samples and keeps alpha separate before explicit float CMS conversion. Its observed native drawing controls are scoped to the tested path, while source normalization, canonical storage and final byte conversion remain distinct boundaries.

The [native RGB power preparation](../loader/color-management.md#behavioral-contract-tests) copies source float bits when a paired channel/profile exchange is necessary; it does not predecode into another float buffer. This avoids adding an intermediate rounding or underflow point before the native matrix. Remaining native arithmetic is a separate precision boundary: subnormal results and small residuals from large matrix cancellation can still be lost or inaccurate. The dark relative tests cover their stated original 16-bit vectors and ordinary stored matrices, not every representable normalized float.

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

The [tests/processing/frame/0098_frame_float_dynamic_range.c](../../tests/processing/frame/0098_frame_float_dynamic_range.c) checks small results independently of large noncontributors and cancelling terms. A final footprint with one contributing RGB sample must return exactly `(1, 0.25, -0.5)`, including an unchanged axis. Three- and seven-sample symmetric triangle reductions must retain that small sample with weights `3/7` and `7/37`, respectively, in both horizontal and vertical passes. The seven-sample case places the cancelling extremes at mirrored inner taps (indices 2 and 4, each weight `6/7`), with zeros elsewhere except the center. This distinguishes symmetric distance evaluation from independently rounded divided coordinates. The absolute tolerance is `2e-6` on the small result; it is never scaled by the cancelling `FLT_MAX` or `1e20` magnitude. Each case exercises both the frame API and its internal extended helper under scalar and automatic dispatch. Compensated direct sums, symmetric filter distances, and local SIMD recovery preserve these residuals; constant and unary footprints avoid roundoff at the float32 storage limits. Bounded mask arithmetic retains its existing order.

The [tests/processing/frame/0018_frame_float_resampling.c](../../tests/processing/frame/0018_frame_float_resampling.c) uses constant preservation, independent triangle weights, and the existing cubic kernel's exact rational quarter-pixel weights. It compares channel- and pixel-varying values with a `2e-6` absolute tolerance; the finite `FLT_MAX` constant average uses a `2e-7` relative bound. Nonconstant extreme inputs normalize by their exact float magnitude before comparison at the same `2e-6` bound. The mixed-sign downscale has independent normalized results `3/7, 3/7, 1/49`; the large cubic case uses fields `2*x-1, 2*y-1, (2*x-1)*(2*y-1)` scaled by `FLT_MAX/8`. Bilinear downscale uses separable weights `2:3:2`. For cubic enlargement, the right-hand source weights are `-3/16, 1/4, 3/4, 19/16`; RGB fields `x, y, x*y` distinguish both axes and intermediate clipping. These are formula-derived values, not saved scaler output. Single-pixel Gaussian enlargements additionally require exact identity: `1→6` at `FLT_MAX` rejects phantom scalar overflow from multiply/divide roundoff, while `1→2` checks ordinary non-binary fractions and subnormal samples against the original float bits. Both axes and scalar/automatic dispatch are covered. Two-source triangle means separately check subnormal constants, mixed subnormals, and zero at exact endpoint/halfway values. The constant channels have magnitude `2^-149`, so both half-weighted contributions round to zero in float SIMD; unweighted nonzero source information distinguishes them from an exactly zero channel. The mixed center is `(3,4,-5) * 2^-149`, derived from endpoint triples `(1,2,-3) * 2^-149` and `(5,6,-7) * 2^-149`; no CMS or scaler output supplies the expectation.

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
| FP-66 | A zero-weight extreme preserves the final x sample (FLT_MAX, frame, scalar). | [tests/processing/frame/0098_frame_float_zero_weight_x_max_frame_scalar.t](../../tests/processing/frame/0098_frame_float_zero_weight_x_max_frame_scalar.t) |
| FP-67 | A zero-weight extreme preserves the final x sample (FLT_MAX, frame, auto). | [tests/processing/frame/0099_frame_float_zero_weight_x_max_frame_auto.t](../../tests/processing/frame/0099_frame_float_zero_weight_x_max_frame_auto.t) |
| FP-68 | A zero-weight extreme preserves the final x sample (FLT_MAX, helper, scalar). | [tests/processing/frame/0100_frame_float_zero_weight_x_max_helper_scalar.t](../../tests/processing/frame/0100_frame_float_zero_weight_x_max_helper_scalar.t) |
| FP-69 | A zero-weight extreme preserves the final x sample (FLT_MAX, helper, auto). | [tests/processing/frame/0101_frame_float_zero_weight_x_max_helper_auto.t](../../tests/processing/frame/0101_frame_float_zero_weight_x_max_helper_auto.t) |
| FP-70 | A zero-weight extreme preserves the final x sample (1e20, frame, scalar). | [tests/processing/frame/0102_frame_float_zero_weight_x_1e20_frame_scalar.t](../../tests/processing/frame/0102_frame_float_zero_weight_x_1e20_frame_scalar.t) |
| FP-71 | A zero-weight extreme preserves the final x sample (1e20, frame, auto). | [tests/processing/frame/0103_frame_float_zero_weight_x_1e20_frame_auto.t](../../tests/processing/frame/0103_frame_float_zero_weight_x_1e20_frame_auto.t) |
| FP-72 | A zero-weight extreme preserves the final x sample (1e20, helper, scalar). | [tests/processing/frame/0104_frame_float_zero_weight_x_1e20_helper_scalar.t](../../tests/processing/frame/0104_frame_float_zero_weight_x_1e20_helper_scalar.t) |
| FP-73 | A zero-weight extreme preserves the final x sample (1e20, helper, auto). | [tests/processing/frame/0105_frame_float_zero_weight_x_1e20_helper_auto.t](../../tests/processing/frame/0105_frame_float_zero_weight_x_1e20_helper_auto.t) |
| FP-74 | A zero-weight extreme preserves the final y sample (FLT_MAX, frame, scalar). | [tests/processing/frame/0106_frame_float_zero_weight_y_max_frame_scalar.t](../../tests/processing/frame/0106_frame_float_zero_weight_y_max_frame_scalar.t) |
| FP-75 | A zero-weight extreme preserves the final y sample (FLT_MAX, frame, auto). | [tests/processing/frame/0107_frame_float_zero_weight_y_max_frame_auto.t](../../tests/processing/frame/0107_frame_float_zero_weight_y_max_frame_auto.t) |
| FP-76 | A zero-weight extreme preserves the final y sample (FLT_MAX, helper, scalar). | [tests/processing/frame/0108_frame_float_zero_weight_y_max_helper_scalar.t](../../tests/processing/frame/0108_frame_float_zero_weight_y_max_helper_scalar.t) |
| FP-77 | A zero-weight extreme preserves the final y sample (FLT_MAX, helper, auto). | [tests/processing/frame/0109_frame_float_zero_weight_y_max_helper_auto.t](../../tests/processing/frame/0109_frame_float_zero_weight_y_max_helper_auto.t) |
| FP-78 | A zero-weight extreme preserves the final y sample (1e20, frame, scalar). | [tests/processing/frame/0110_frame_float_zero_weight_y_1e20_frame_scalar.t](../../tests/processing/frame/0110_frame_float_zero_weight_y_1e20_frame_scalar.t) |
| FP-79 | A zero-weight extreme preserves the final y sample (1e20, frame, auto). | [tests/processing/frame/0111_frame_float_zero_weight_y_1e20_frame_auto.t](../../tests/processing/frame/0111_frame_float_zero_weight_y_1e20_frame_auto.t) |
| FP-80 | A zero-weight extreme preserves the final y sample (1e20, helper, scalar). | [tests/processing/frame/0112_frame_float_zero_weight_y_1e20_helper_scalar.t](../../tests/processing/frame/0112_frame_float_zero_weight_y_1e20_helper_scalar.t) |
| FP-81 | A zero-weight extreme preserves the final y sample (1e20, helper, auto). | [tests/processing/frame/0113_frame_float_zero_weight_y_1e20_helper_auto.t](../../tests/processing/frame/0113_frame_float_zero_weight_y_1e20_helper_auto.t) |
| FP-82 | An unchanged axis preserves a small sample (FLT_MAX, frame, scalar). | [tests/processing/frame/0114_frame_float_zero_weight_same_axis_max_frame_scalar.t](../../tests/processing/frame/0114_frame_float_zero_weight_same_axis_max_frame_scalar.t) |
| FP-83 | An unchanged axis preserves a small sample (FLT_MAX, frame, auto). | [tests/processing/frame/0115_frame_float_zero_weight_same_axis_max_frame_auto.t](../../tests/processing/frame/0115_frame_float_zero_weight_same_axis_max_frame_auto.t) |
| FP-84 | An unchanged axis preserves a small sample (FLT_MAX, helper, scalar). | [tests/processing/frame/0116_frame_float_zero_weight_same_axis_max_helper_scalar.t](../../tests/processing/frame/0116_frame_float_zero_weight_same_axis_max_helper_scalar.t) |
| FP-85 | An unchanged axis preserves a small sample (FLT_MAX, helper, auto). | [tests/processing/frame/0117_frame_float_zero_weight_same_axis_max_helper_auto.t](../../tests/processing/frame/0117_frame_float_zero_weight_same_axis_max_helper_auto.t) |
| FP-86 | An unchanged axis preserves a small sample (1e20, frame, scalar). | [tests/processing/frame/0118_frame_float_zero_weight_same_axis_1e20_frame_scalar.t](../../tests/processing/frame/0118_frame_float_zero_weight_same_axis_1e20_frame_scalar.t) |
| FP-87 | An unchanged axis preserves a small sample (1e20, frame, auto). | [tests/processing/frame/0119_frame_float_zero_weight_same_axis_1e20_frame_auto.t](../../tests/processing/frame/0119_frame_float_zero_weight_same_axis_1e20_frame_auto.t) |
| FP-88 | An unchanged axis preserves a small sample (1e20, helper, scalar). | [tests/processing/frame/0120_frame_float_zero_weight_same_axis_1e20_helper_scalar.t](../../tests/processing/frame/0120_frame_float_zero_weight_same_axis_1e20_helper_scalar.t) |
| FP-89 | An unchanged axis preserves a small sample (1e20, helper, auto). | [tests/processing/frame/0121_frame_float_zero_weight_same_axis_1e20_helper_auto.t](../../tests/processing/frame/0121_frame_float_zero_weight_same_axis_1e20_helper_auto.t) |
| FP-90 | Three horizontal samples retain a small cancellation residual (FLT_MAX, frame, scalar). | [tests/processing/frame/0122_frame_float_cancel_three_max_frame_scalar.t](../../tests/processing/frame/0122_frame_float_cancel_three_max_frame_scalar.t) |
| FP-91 | Three horizontal samples retain a small cancellation residual (FLT_MAX, frame, auto). | [tests/processing/frame/0123_frame_float_cancel_three_max_frame_auto.t](../../tests/processing/frame/0123_frame_float_cancel_three_max_frame_auto.t) |
| FP-92 | Three horizontal samples retain a small cancellation residual (FLT_MAX, helper, scalar). | [tests/processing/frame/0124_frame_float_cancel_three_max_helper_scalar.t](../../tests/processing/frame/0124_frame_float_cancel_three_max_helper_scalar.t) |
| FP-93 | Three horizontal samples retain a small cancellation residual (FLT_MAX, helper, auto). | [tests/processing/frame/0125_frame_float_cancel_three_max_helper_auto.t](../../tests/processing/frame/0125_frame_float_cancel_three_max_helper_auto.t) |
| FP-94 | Three horizontal samples retain a small cancellation residual (1e20, frame, scalar). | [tests/processing/frame/0126_frame_float_cancel_three_1e20_frame_scalar.t](../../tests/processing/frame/0126_frame_float_cancel_three_1e20_frame_scalar.t) |
| FP-95 | Three horizontal samples retain a small cancellation residual (1e20, frame, auto). | [tests/processing/frame/0127_frame_float_cancel_three_1e20_frame_auto.t](../../tests/processing/frame/0127_frame_float_cancel_three_1e20_frame_auto.t) |
| FP-96 | Three horizontal samples retain a small cancellation residual (1e20, helper, scalar). | [tests/processing/frame/0128_frame_float_cancel_three_1e20_helper_scalar.t](../../tests/processing/frame/0128_frame_float_cancel_three_1e20_helper_scalar.t) |
| FP-97 | Three horizontal samples retain a small cancellation residual (1e20, helper, auto). | [tests/processing/frame/0129_frame_float_cancel_three_1e20_helper_auto.t](../../tests/processing/frame/0129_frame_float_cancel_three_1e20_helper_auto.t) |
| FP-98 | Seven horizontal samples retain a small cancellation residual (FLT_MAX, frame, scalar). | [tests/processing/frame/0130_frame_float_cancel_seven_max_frame_scalar.t](../../tests/processing/frame/0130_frame_float_cancel_seven_max_frame_scalar.t) |
| FP-99 | Seven horizontal samples retain a small cancellation residual (FLT_MAX, frame, auto). | [tests/processing/frame/0131_frame_float_cancel_seven_max_frame_auto.t](../../tests/processing/frame/0131_frame_float_cancel_seven_max_frame_auto.t) |
| FP-100 | Seven horizontal samples retain a small cancellation residual (FLT_MAX, helper, scalar). | [tests/processing/frame/0132_frame_float_cancel_seven_max_helper_scalar.t](../../tests/processing/frame/0132_frame_float_cancel_seven_max_helper_scalar.t) |
| FP-101 | Seven horizontal samples retain a small cancellation residual (FLT_MAX, helper, auto). | [tests/processing/frame/0133_frame_float_cancel_seven_max_helper_auto.t](../../tests/processing/frame/0133_frame_float_cancel_seven_max_helper_auto.t) |
| FP-102 | Seven horizontal samples retain a small cancellation residual (1e20, frame, scalar). | [tests/processing/frame/0134_frame_float_cancel_seven_1e20_frame_scalar.t](../../tests/processing/frame/0134_frame_float_cancel_seven_1e20_frame_scalar.t) |
| FP-103 | Seven horizontal samples retain a small cancellation residual (1e20, frame, auto). | [tests/processing/frame/0135_frame_float_cancel_seven_1e20_frame_auto.t](../../tests/processing/frame/0135_frame_float_cancel_seven_1e20_frame_auto.t) |
| FP-104 | Seven horizontal samples retain a small cancellation residual (1e20, helper, scalar). | [tests/processing/frame/0136_frame_float_cancel_seven_1e20_helper_scalar.t](../../tests/processing/frame/0136_frame_float_cancel_seven_1e20_helper_scalar.t) |
| FP-105 | Seven horizontal samples retain a small cancellation residual (1e20, helper, auto). | [tests/processing/frame/0137_frame_float_cancel_seven_1e20_helper_auto.t](../../tests/processing/frame/0137_frame_float_cancel_seven_1e20_helper_auto.t) |
| FP-106 | Three vertical samples retain a small cancellation residual (FLT_MAX, frame, scalar). | [tests/processing/frame/0138_frame_float_cancel_three_y_max_frame_scalar.t](../../tests/processing/frame/0138_frame_float_cancel_three_y_max_frame_scalar.t) |
| FP-107 | Three vertical samples retain a small cancellation residual (FLT_MAX, frame, auto). | [tests/processing/frame/0139_frame_float_cancel_three_y_max_frame_auto.t](../../tests/processing/frame/0139_frame_float_cancel_three_y_max_frame_auto.t) |
| FP-108 | Three vertical samples retain a small cancellation residual (FLT_MAX, helper, scalar). | [tests/processing/frame/0140_frame_float_cancel_three_y_max_helper_scalar.t](../../tests/processing/frame/0140_frame_float_cancel_three_y_max_helper_scalar.t) |
| FP-109 | Three vertical samples retain a small cancellation residual (FLT_MAX, helper, auto). | [tests/processing/frame/0141_frame_float_cancel_three_y_max_helper_auto.t](../../tests/processing/frame/0141_frame_float_cancel_three_y_max_helper_auto.t) |
| FP-110 | Three vertical samples retain a small cancellation residual (1e20, frame, scalar). | [tests/processing/frame/0142_frame_float_cancel_three_y_1e20_frame_scalar.t](../../tests/processing/frame/0142_frame_float_cancel_three_y_1e20_frame_scalar.t) |
| FP-111 | Three vertical samples retain a small cancellation residual (1e20, frame, auto). | [tests/processing/frame/0143_frame_float_cancel_three_y_1e20_frame_auto.t](../../tests/processing/frame/0143_frame_float_cancel_three_y_1e20_frame_auto.t) |
| FP-112 | Three vertical samples retain a small cancellation residual (1e20, helper, scalar). | [tests/processing/frame/0144_frame_float_cancel_three_y_1e20_helper_scalar.t](../../tests/processing/frame/0144_frame_float_cancel_three_y_1e20_helper_scalar.t) |
| FP-113 | Three vertical samples retain a small cancellation residual (1e20, helper, auto). | [tests/processing/frame/0145_frame_float_cancel_three_y_1e20_helper_auto.t](../../tests/processing/frame/0145_frame_float_cancel_three_y_1e20_helper_auto.t) |
| FP-114 | Seven vertical samples retain a small cancellation residual (FLT_MAX, frame, scalar). | [tests/processing/frame/0146_frame_float_cancel_seven_y_max_frame_scalar.t](../../tests/processing/frame/0146_frame_float_cancel_seven_y_max_frame_scalar.t) |
| FP-115 | Seven vertical samples retain a small cancellation residual (FLT_MAX, frame, auto). | [tests/processing/frame/0147_frame_float_cancel_seven_y_max_frame_auto.t](../../tests/processing/frame/0147_frame_float_cancel_seven_y_max_frame_auto.t) |
| FP-116 | Seven vertical samples retain a small cancellation residual (FLT_MAX, helper, scalar). | [tests/processing/frame/0148_frame_float_cancel_seven_y_max_helper_scalar.t](../../tests/processing/frame/0148_frame_float_cancel_seven_y_max_helper_scalar.t) |
| FP-117 | Seven vertical samples retain a small cancellation residual (FLT_MAX, helper, auto). | [tests/processing/frame/0149_frame_float_cancel_seven_y_max_helper_auto.t](../../tests/processing/frame/0149_frame_float_cancel_seven_y_max_helper_auto.t) |
| FP-118 | Seven vertical samples retain a small cancellation residual (1e20, frame, scalar). | [tests/processing/frame/0150_frame_float_cancel_seven_y_1e20_frame_scalar.t](../../tests/processing/frame/0150_frame_float_cancel_seven_y_1e20_frame_scalar.t) |
| FP-119 | Seven vertical samples retain a small cancellation residual (1e20, frame, auto). | [tests/processing/frame/0151_frame_float_cancel_seven_y_1e20_frame_auto.t](../../tests/processing/frame/0151_frame_float_cancel_seven_y_1e20_frame_auto.t) |
| FP-120 | Seven vertical samples retain a small cancellation residual (1e20, helper, scalar). | [tests/processing/frame/0152_frame_float_cancel_seven_y_1e20_helper_scalar.t](../../tests/processing/frame/0152_frame_float_cancel_seven_y_1e20_helper_scalar.t) |
| FP-121 | Seven vertical samples retain a small cancellation residual (1e20, helper, auto). | [tests/processing/frame/0153_frame_float_cancel_seven_y_1e20_helper_auto.t](../../tests/processing/frame/0153_frame_float_cancel_seven_y_1e20_helper_auto.t) |
| FP-122 | A one-sample Gaussian mean is exact (large, x, scalar). | [tests/processing/frame/0154_frame_float_identity_large_x_scalar.t](../../tests/processing/frame/0154_frame_float_identity_large_x_scalar.t) |
| FP-123 | A one-sample Gaussian mean is exact (large, x, auto). | [tests/processing/frame/0155_frame_float_identity_large_x_auto.t](../../tests/processing/frame/0155_frame_float_identity_large_x_auto.t) |
| FP-124 | A one-sample Gaussian mean is exact (large, y, scalar). | [tests/processing/frame/0156_frame_float_identity_large_y_scalar.t](../../tests/processing/frame/0156_frame_float_identity_large_y_scalar.t) |
| FP-125 | A one-sample Gaussian mean is exact (large, y, auto). | [tests/processing/frame/0157_frame_float_identity_large_y_auto.t](../../tests/processing/frame/0157_frame_float_identity_large_y_auto.t) |
| FP-126 | A one-sample Gaussian mean is exact (small, x, scalar). | [tests/processing/frame/0158_frame_float_identity_small_x_scalar.t](../../tests/processing/frame/0158_frame_float_identity_small_x_scalar.t) |
| FP-127 | A one-sample Gaussian mean is exact (small, x, auto). | [tests/processing/frame/0159_frame_float_identity_small_x_auto.t](../../tests/processing/frame/0159_frame_float_identity_small_x_auto.t) |
| FP-128 | A one-sample Gaussian mean is exact (small, y, scalar). | [tests/processing/frame/0160_frame_float_identity_small_y_scalar.t](../../tests/processing/frame/0160_frame_float_identity_small_y_scalar.t) |
| FP-129 | A one-sample Gaussian mean is exact (small, y, auto). | [tests/processing/frame/0161_frame_float_identity_small_y_auto.t](../../tests/processing/frame/0161_frame_float_identity_small_y_auto.t) |
| FP-130 | A one-sample Gaussian mean is exact (subnormal, x, scalar). | [tests/processing/frame/0162_frame_float_identity_subnormal_x_scalar.t](../../tests/processing/frame/0162_frame_float_identity_subnormal_x_scalar.t) |
| FP-131 | A one-sample Gaussian mean is exact (subnormal, x, auto). | [tests/processing/frame/0163_frame_float_identity_subnormal_x_auto.t](../../tests/processing/frame/0163_frame_float_identity_subnormal_x_auto.t) |
| FP-132 | A one-sample Gaussian mean is exact (subnormal, y, scalar). | [tests/processing/frame/0164_frame_float_identity_subnormal_y_scalar.t](../../tests/processing/frame/0164_frame_float_identity_subnormal_y_scalar.t) |
| FP-133 | A one-sample Gaussian mean is exact (subnormal, y, auto). | [tests/processing/frame/0165_frame_float_identity_subnormal_y_auto.t](../../tests/processing/frame/0165_frame_float_identity_subnormal_y_auto.t) |
| FP-134 | A two-contributor triangle mean retains subnormals or zero (constant, x, scalar). | [tests/processing/frame/0166_frame_float_tiny_constant_x_scalar.t](../../tests/processing/frame/0166_frame_float_tiny_constant_x_scalar.t) |
| FP-135 | A two-contributor triangle mean retains subnormals or zero (constant, x, auto). | [tests/processing/frame/0167_frame_float_tiny_constant_x_auto.t](../../tests/processing/frame/0167_frame_float_tiny_constant_x_auto.t) |
| FP-136 | A two-contributor triangle mean retains subnormals or zero (constant, y, scalar). | [tests/processing/frame/0168_frame_float_tiny_constant_y_scalar.t](../../tests/processing/frame/0168_frame_float_tiny_constant_y_scalar.t) |
| FP-137 | A two-contributor triangle mean retains subnormals or zero (constant, y, auto). | [tests/processing/frame/0169_frame_float_tiny_constant_y_auto.t](../../tests/processing/frame/0169_frame_float_tiny_constant_y_auto.t) |
| FP-138 | A two-contributor triangle mean retains subnormals or zero (zero, x, scalar). | [tests/processing/frame/0170_frame_float_tiny_zero_x_scalar.t](../../tests/processing/frame/0170_frame_float_tiny_zero_x_scalar.t) |
| FP-139 | A two-contributor triangle mean retains subnormals or zero (zero, x, auto). | [tests/processing/frame/0171_frame_float_tiny_zero_x_auto.t](../../tests/processing/frame/0171_frame_float_tiny_zero_x_auto.t) |
| FP-140 | A two-contributor triangle mean retains subnormals or zero (zero, y, scalar). | [tests/processing/frame/0172_frame_float_tiny_zero_y_scalar.t](../../tests/processing/frame/0172_frame_float_tiny_zero_y_scalar.t) |
| FP-141 | A two-contributor triangle mean retains subnormals or zero (zero, y, auto). | [tests/processing/frame/0173_frame_float_tiny_zero_y_auto.t](../../tests/processing/frame/0173_frame_float_tiny_zero_y_auto.t) |
| FP-142 | A two-contributor triangle mean retains subnormals or zero (mixed, x, scalar). | [tests/processing/frame/0174_frame_float_tiny_mixed_x_scalar.t](../../tests/processing/frame/0174_frame_float_tiny_mixed_x_scalar.t) |
| FP-143 | A two-contributor triangle mean retains subnormals or zero (mixed, x, auto). | [tests/processing/frame/0175_frame_float_tiny_mixed_x_auto.t](../../tests/processing/frame/0175_frame_float_tiny_mixed_x_auto.t) |
| FP-144 | A two-contributor triangle mean retains subnormals or zero (mixed, y, scalar). | [tests/processing/frame/0176_frame_float_tiny_mixed_y_scalar.t](../../tests/processing/frame/0176_frame_float_tiny_mixed_y_scalar.t) |
| FP-145 | A two-contributor triangle mean retains subnormals or zero (mixed, y, auto). | [tests/processing/frame/0177_frame_float_tiny_mixed_y_auto.t](../../tests/processing/frame/0177_frame_float_tiny_mixed_y_auto.t) |
| FP-146 | Float-to-byte gamma borrowed direct numeric. | [tests/processing/frame/0178_frame_byte_gamma_borrowed_direct_numeric.t](../../tests/processing/frame/0178_frame_byte_gamma_borrowed_direct_numeric.t) |
| FP-147 | Float-to-byte gamma owned direct numeric. | [tests/processing/frame/0179_frame_byte_gamma_owned_direct_numeric.t](../../tests/processing/frame/0179_frame_byte_gamma_owned_direct_numeric.t) |
| FP-148 | Float-to-byte gamma borrowed interface numeric. | [tests/processing/frame/0180_frame_byte_gamma_borrowed_interface_numeric.t](../../tests/processing/frame/0180_frame_byte_gamma_borrowed_interface_numeric.t) |
| FP-149 | Float-to-byte gamma owned interface numeric. | [tests/processing/frame/0181_frame_byte_gamma_owned_interface_numeric.t](../../tests/processing/frame/0181_frame_byte_gamma_owned_interface_numeric.t) |
| FP-150 | Float-to-byte gamma clone interface numeric. | [tests/processing/frame/0182_frame_byte_gamma_clone_interface_numeric.t](../../tests/processing/frame/0182_frame_byte_gamma_clone_interface_numeric.t) |
| FP-151 | Float-to-byte linear borrowed direct numeric. | [tests/processing/frame/0183_frame_byte_linear_borrowed_direct_numeric.t](../../tests/processing/frame/0183_frame_byte_linear_borrowed_direct_numeric.t) |
| FP-152 | Float-to-byte linear owned direct numeric. | [tests/processing/frame/0184_frame_byte_linear_owned_direct_numeric.t](../../tests/processing/frame/0184_frame_byte_linear_owned_direct_numeric.t) |
| FP-153 | Float-to-byte linear borrowed interface numeric. | [tests/processing/frame/0185_frame_byte_linear_borrowed_interface_numeric.t](../../tests/processing/frame/0185_frame_byte_linear_borrowed_interface_numeric.t) |
| FP-154 | Float-to-byte linear owned interface numeric. | [tests/processing/frame/0186_frame_byte_linear_owned_interface_numeric.t](../../tests/processing/frame/0186_frame_byte_linear_owned_interface_numeric.t) |
| FP-155 | Float-to-byte linear clone interface numeric. | [tests/processing/frame/0187_frame_byte_linear_clone_interface_numeric.t](../../tests/processing/frame/0187_frame_byte_linear_clone_interface_numeric.t) |
| FP-156 | Float-to-byte oklab borrowed direct numeric. | [tests/processing/frame/0188_frame_byte_oklab_borrowed_direct_numeric.t](../../tests/processing/frame/0188_frame_byte_oklab_borrowed_direct_numeric.t) |
| FP-157 | Float-to-byte oklab owned direct numeric. | [tests/processing/frame/0189_frame_byte_oklab_owned_direct_numeric.t](../../tests/processing/frame/0189_frame_byte_oklab_owned_direct_numeric.t) |
| FP-158 | Float-to-byte oklab borrowed interface numeric. | [tests/processing/frame/0190_frame_byte_oklab_borrowed_interface_numeric.t](../../tests/processing/frame/0190_frame_byte_oklab_borrowed_interface_numeric.t) |
| FP-159 | Float-to-byte oklab owned interface numeric. | [tests/processing/frame/0191_frame_byte_oklab_owned_interface_numeric.t](../../tests/processing/frame/0191_frame_byte_oklab_owned_interface_numeric.t) |
| FP-160 | Float-to-byte oklab clone interface numeric. | [tests/processing/frame/0192_frame_byte_oklab_clone_interface_numeric.t](../../tests/processing/frame/0192_frame_byte_oklab_clone_interface_numeric.t) |
| FP-161 | Float-to-byte cielab borrowed direct numeric. | [tests/processing/frame/0193_frame_byte_cielab_borrowed_direct_numeric.t](../../tests/processing/frame/0193_frame_byte_cielab_borrowed_direct_numeric.t) |
| FP-162 | Float-to-byte cielab owned direct numeric. | [tests/processing/frame/0194_frame_byte_cielab_owned_direct_numeric.t](../../tests/processing/frame/0194_frame_byte_cielab_owned_direct_numeric.t) |
| FP-163 | Float-to-byte cielab borrowed interface numeric. | [tests/processing/frame/0195_frame_byte_cielab_borrowed_interface_numeric.t](../../tests/processing/frame/0195_frame_byte_cielab_borrowed_interface_numeric.t) |
| FP-164 | Float-to-byte cielab owned interface numeric. | [tests/processing/frame/0196_frame_byte_cielab_owned_interface_numeric.t](../../tests/processing/frame/0196_frame_byte_cielab_owned_interface_numeric.t) |
| FP-165 | Float-to-byte cielab clone interface numeric. | [tests/processing/frame/0197_frame_byte_cielab_clone_interface_numeric.t](../../tests/processing/frame/0197_frame_byte_cielab_clone_interface_numeric.t) |
| FP-166 | Float-to-byte din99d borrowed direct numeric. | [tests/processing/frame/0198_frame_byte_din99d_borrowed_direct_numeric.t](../../tests/processing/frame/0198_frame_byte_din99d_borrowed_direct_numeric.t) |
| FP-167 | Float-to-byte din99d owned direct numeric. | [tests/processing/frame/0199_frame_byte_din99d_owned_direct_numeric.t](../../tests/processing/frame/0199_frame_byte_din99d_owned_direct_numeric.t) |
| FP-168 | Float-to-byte din99d borrowed interface numeric. | [tests/processing/frame/0200_frame_byte_din99d_borrowed_interface_numeric.t](../../tests/processing/frame/0200_frame_byte_din99d_borrowed_interface_numeric.t) |
| FP-169 | Float-to-byte din99d owned interface numeric. | [tests/processing/frame/0201_frame_byte_din99d_owned_interface_numeric.t](../../tests/processing/frame/0201_frame_byte_din99d_owned_interface_numeric.t) |
| FP-170 | Float-to-byte din99d clone interface numeric. | [tests/processing/frame/0202_frame_byte_din99d_clone_interface_numeric.t](../../tests/processing/frame/0202_frame_byte_din99d_clone_interface_numeric.t) |
| FP-171 | Float-to-byte gamma allocation borrowed atomic. | [tests/processing/frame/0203_frame_byte_gamma_allocation_borrowed_atomic.t](../../tests/processing/frame/0203_frame_byte_gamma_allocation_borrowed_atomic.t) |
| FP-172 | Float-to-byte gamma allocation owned atomic. | [tests/processing/frame/0204_frame_byte_gamma_allocation_owned_atomic.t](../../tests/processing/frame/0204_frame_byte_gamma_allocation_owned_atomic.t) |
| FP-173 | Float-to-byte gamma nan borrowed atomic. | [tests/processing/frame/0205_frame_byte_gamma_nan_borrowed_atomic.t](../../tests/processing/frame/0205_frame_byte_gamma_nan_borrowed_atomic.t) |
| FP-174 | Float-to-byte gamma nan owned atomic. | [tests/processing/frame/0206_frame_byte_gamma_nan_owned_atomic.t](../../tests/processing/frame/0206_frame_byte_gamma_nan_owned_atomic.t) |
| FP-175 | Float-to-byte gamma infinity borrowed atomic. | [tests/processing/frame/0207_frame_byte_gamma_infinity_borrowed_atomic.t](../../tests/processing/frame/0207_frame_byte_gamma_infinity_borrowed_atomic.t) |
| FP-176 | Float-to-byte gamma infinity owned atomic. | [tests/processing/frame/0208_frame_byte_gamma_infinity_owned_atomic.t](../../tests/processing/frame/0208_frame_byte_gamma_infinity_owned_atomic.t) |
| FP-177 | Float-to-byte linear allocation borrowed atomic. | [tests/processing/frame/0209_frame_byte_linear_allocation_borrowed_atomic.t](../../tests/processing/frame/0209_frame_byte_linear_allocation_borrowed_atomic.t) |
| FP-178 | Float-to-byte linear allocation owned atomic. | [tests/processing/frame/0210_frame_byte_linear_allocation_owned_atomic.t](../../tests/processing/frame/0210_frame_byte_linear_allocation_owned_atomic.t) |
| FP-179 | Float-to-byte linear nan borrowed atomic. | [tests/processing/frame/0211_frame_byte_linear_nan_borrowed_atomic.t](../../tests/processing/frame/0211_frame_byte_linear_nan_borrowed_atomic.t) |
| FP-180 | Float-to-byte linear nan owned atomic. | [tests/processing/frame/0212_frame_byte_linear_nan_owned_atomic.t](../../tests/processing/frame/0212_frame_byte_linear_nan_owned_atomic.t) |
| FP-181 | Float-to-byte linear infinity borrowed atomic. | [tests/processing/frame/0213_frame_byte_linear_infinity_borrowed_atomic.t](../../tests/processing/frame/0213_frame_byte_linear_infinity_borrowed_atomic.t) |
| FP-182 | Float-to-byte linear infinity owned atomic. | [tests/processing/frame/0214_frame_byte_linear_infinity_owned_atomic.t](../../tests/processing/frame/0214_frame_byte_linear_infinity_owned_atomic.t) |
| FP-183 | Float-to-byte oklab allocation borrowed atomic. | [tests/processing/frame/0215_frame_byte_oklab_allocation_borrowed_atomic.t](../../tests/processing/frame/0215_frame_byte_oklab_allocation_borrowed_atomic.t) |
| FP-184 | Float-to-byte oklab allocation owned atomic. | [tests/processing/frame/0216_frame_byte_oklab_allocation_owned_atomic.t](../../tests/processing/frame/0216_frame_byte_oklab_allocation_owned_atomic.t) |
| FP-185 | Float-to-byte oklab nan borrowed atomic. | [tests/processing/frame/0217_frame_byte_oklab_nan_borrowed_atomic.t](../../tests/processing/frame/0217_frame_byte_oklab_nan_borrowed_atomic.t) |
| FP-186 | Float-to-byte oklab nan owned atomic. | [tests/processing/frame/0218_frame_byte_oklab_nan_owned_atomic.t](../../tests/processing/frame/0218_frame_byte_oklab_nan_owned_atomic.t) |
| FP-187 | Float-to-byte oklab infinity borrowed atomic. | [tests/processing/frame/0219_frame_byte_oklab_infinity_borrowed_atomic.t](../../tests/processing/frame/0219_frame_byte_oklab_infinity_borrowed_atomic.t) |
| FP-188 | Float-to-byte oklab infinity owned atomic. | [tests/processing/frame/0220_frame_byte_oklab_infinity_owned_atomic.t](../../tests/processing/frame/0220_frame_byte_oklab_infinity_owned_atomic.t) |
| FP-189 | Float-to-byte cielab allocation borrowed atomic. | [tests/processing/frame/0221_frame_byte_cielab_allocation_borrowed_atomic.t](../../tests/processing/frame/0221_frame_byte_cielab_allocation_borrowed_atomic.t) |
| FP-190 | Float-to-byte cielab allocation owned atomic. | [tests/processing/frame/0222_frame_byte_cielab_allocation_owned_atomic.t](../../tests/processing/frame/0222_frame_byte_cielab_allocation_owned_atomic.t) |
| FP-191 | Float-to-byte cielab nan borrowed atomic. | [tests/processing/frame/0223_frame_byte_cielab_nan_borrowed_atomic.t](../../tests/processing/frame/0223_frame_byte_cielab_nan_borrowed_atomic.t) |
| FP-192 | Float-to-byte cielab nan owned atomic. | [tests/processing/frame/0224_frame_byte_cielab_nan_owned_atomic.t](../../tests/processing/frame/0224_frame_byte_cielab_nan_owned_atomic.t) |
| FP-193 | Float-to-byte cielab infinity borrowed atomic. | [tests/processing/frame/0225_frame_byte_cielab_infinity_borrowed_atomic.t](../../tests/processing/frame/0225_frame_byte_cielab_infinity_borrowed_atomic.t) |
| FP-194 | Float-to-byte cielab infinity owned atomic. | [tests/processing/frame/0226_frame_byte_cielab_infinity_owned_atomic.t](../../tests/processing/frame/0226_frame_byte_cielab_infinity_owned_atomic.t) |
| FP-195 | Float-to-byte din99d allocation borrowed atomic. | [tests/processing/frame/0227_frame_byte_din99d_allocation_borrowed_atomic.t](../../tests/processing/frame/0227_frame_byte_din99d_allocation_borrowed_atomic.t) |
| FP-196 | Float-to-byte din99d allocation owned atomic. | [tests/processing/frame/0228_frame_byte_din99d_allocation_owned_atomic.t](../../tests/processing/frame/0228_frame_byte_din99d_allocation_owned_atomic.t) |
| FP-197 | Float-to-byte din99d nan borrowed atomic. | [tests/processing/frame/0229_frame_byte_din99d_nan_borrowed_atomic.t](../../tests/processing/frame/0229_frame_byte_din99d_nan_borrowed_atomic.t) |
| FP-198 | Float-to-byte din99d nan owned atomic. | [tests/processing/frame/0230_frame_byte_din99d_nan_owned_atomic.t](../../tests/processing/frame/0230_frame_byte_din99d_nan_owned_atomic.t) |
| FP-199 | Float-to-byte din99d infinity borrowed atomic. | [tests/processing/frame/0231_frame_byte_din99d_infinity_borrowed_atomic.t](../../tests/processing/frame/0231_frame_byte_din99d_infinity_borrowed_atomic.t) |
| FP-200 | Float-to-byte din99d infinity owned atomic. | [tests/processing/frame/0232_frame_byte_din99d_infinity_owned_atomic.t](../../tests/processing/frame/0232_frame_byte_din99d_infinity_owned_atomic.t) |
| FP-201 | Float-to-byte din99d domain borrowed atomic. | [tests/processing/frame/0233_frame_byte_din99d_domain_borrowed_atomic.t](../../tests/processing/frame/0233_frame_byte_din99d_domain_borrowed_atomic.t) |
| FP-202 | Float-to-byte din99d domain owned atomic. | [tests/processing/frame/0234_frame_byte_din99d_domain_owned_atomic.t](../../tests/processing/frame/0234_frame_byte_din99d_domain_owned_atomic.t) |
| FP-203 | Float-to-byte zero width borrowed atomic. | [tests/processing/frame/0235_frame_byte_zero_width_borrowed_atomic.t](../../tests/processing/frame/0235_frame_byte_zero_width_borrowed_atomic.t) |
| FP-204 | Float-to-byte zero width owned atomic. | [tests/processing/frame/0236_frame_byte_zero_width_owned_atomic.t](../../tests/processing/frame/0236_frame_byte_zero_width_owned_atomic.t) |
| FP-205 | Float-to-byte dimension overflow borrowed atomic. | [tests/processing/frame/0237_frame_byte_dimension_overflow_borrowed_atomic.t](../../tests/processing/frame/0237_frame_byte_dimension_overflow_borrowed_atomic.t) |
| FP-206 | Float-to-byte dimension overflow owned atomic. | [tests/processing/frame/0238_frame_byte_dimension_overflow_owned_atomic.t](../../tests/processing/frame/0238_frame_byte_dimension_overflow_owned_atomic.t) |
