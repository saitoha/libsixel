# Color-aware quality evaluation

`lsqa` separates the interpretation of an input sample, the physical color used for color difference, and the signal used for spatial quality metrics. These are independent choices. An RGB triplet alone does not identify a color.

The measurement foundation and explicit SIXEL input interpretation are implemented. **Wide-gamut PNG/CMS integration is not validated yet.** The legacy PNG loader can normalize or clip samples before `lsqa` sees them. Passing these numeric tests does not establish that a P3 PNG retains its gamut, that external CMS engines execute the new descriptor, or that the encoder can emit P3 colors correctly.

## Input interpretation

Each input has an independent option:

```sh
lsqa --reference-colorspace=srgb --target-colorspace=display-p3 \
    reference.six target.six
producer | lsqa --target-colorspace=display-p3 reference.six -
```

`-R` and `-T` are the corresponding short forms. The default is `auto` for both. Only one image input can read standard input.

| SPEC | Meaning |
| --- | --- |
| `auto` | Keep the legacy loader's metadata and fallback policy. Plain SIXEL is interpreted as sRGB by this adapter. |
| `srgb` | sRGB primaries, D65 white, sRGB transfer function. |
| `display-p3` | Display P3 primaries, D65 white, sRGB transfer function. |
| `rgb:wx,wy,rx,ry,gx,gy,bx,by:srgb` | Custom white and RGB primary xy coordinates with the sRGB transfer function. |
| `rgb:wx,wy,rx,ry,gx,gy,bx,by:gamma=r,g,b` | The same xy layout with three positive per-channel decoding exponents, each at most 10. |
| `icc:FILE` | RGB ICC input profile, relative colorimetric slot 1. |
| `icc:perceptual:FILE` | RGB ICC input profile, slot 0. |
| `icc:relative:FILE` | RGB ICC input profile, slot 1. |
| `icc:saturation:FILE` | RGB ICC input profile, slot 2. |

Names and field counts are exact. Invalid xy coordinates, nonfinite numbers, missing fields, unreadable or inapplicable ICC profiles are errors. An ICC profile is a local file, never standard input. The shared parser performs no file access; each adapter owns profile loading and lifetime.

Explicit definitions currently require a standalone SIXEL DCS stream. They apply to the decoded **original palette samples**, before background selection and before conversion to extended linear sRGB. SIXEL's integer percentage to RGB8 rounding is unchanged. The raw decoder has no new CMS policy.

Explicit PNG overrides report an error until the pre-normalization loader connection is validated. They must never reinterpret already normalized sRGB as though it were the original P3 samples. `auto` PNG keeps its existing loader CMS behavior. Explicit target definitions with `-d` dequantization are also rejected because that reconstruction needs its own color-space contract.

The new descriptor evaluator uses the builtin ICC PCS implementation. `--cms-engine` continues to select the legacy image-loader CMS; it does not change the descriptor evaluator. ICC input only needs a usable forward transform. An inverse table is not required. Slot selection does not add black-point compensation, absolute adaptation, or an external CMS profile-link operation.

## Physical color and comparison signal

The evaluator reads a frame's **current coordinate**, not an original profile tag. Gamma sRGB, linear sRGB, Oklab, normalized CIELAB and DIN99d working coordinates describe the existing sRGB/D65 basis. Their inverse transforms recover physical color before Bradford adaptation to XYZ D50. Signed and above-one intermediate components remain available to the measurement.

Color difference uses XYZ with `Y=1` for white and ICC D50 `(Xn, Yn, Zn) = (0.9642, 1, 0.8249)`. CIELAB then uses conventional `L*`, `a*`, `b*` units; it is not the library's normalized Lab storage. CIEDE2000 uses unit parametric factors. This replaces the old sRGB-only, D65 color-difference interpretation, which incorrectly decoded linear and opponent components as encoded RGB.

`-W / --compare-colorspace` selects the signal for MS-SSIM, PSNR-Y, GMSD and the other spatial/luma diagnostics:

| Selection | Signal |
| --- | --- |
| `reference` | Follow the reference's current coordinate; gamma uses encoded luma, other supported coordinates use linear Y. |
| `gamma` | Encoded sRGB luma, `0.2126 R' + 0.7152 G' + 0.0722 B'`. |
| `linear` | Linear sRGB Y with the same coefficients. |
| `oklab`, `cielab`, `din99d` | Recover linear sRGB and use Y. Opponent components are never weighted as RGB. |

Unchanged sRGB gamma/linear samples retain the existing luma coefficients and float32 representation. The coefficients are the historical metric definition; they are distinct from the full-precision primary matrix used for PCS.

`-P / --compare-precision=reference|8bit|float32` controls this signal. The reference default follows its storage. Eight-bit signals are clipped to 0..1 and rounded to 255 intervals; float32 signals retain extended values, with a nominal range of 0..1. PSNR-Y uses a unit peak, and spatial metric constants retain that nominal range. Clip-rate metrics describe this comparison signal. They cannot prove how much color an earlier loader or encoder clipped.

`-g` makes the comparison signal grayscale. Color difference and region color means still measure the physical input colors. Neither `-W`, `-P`, nor `-g` changes the XYZ used for those color measurements.

Explicit SIXEL inputs use linear float32 after interpretation. Unpainted pixels use the requested `-B` background, or black, interpreted as encoded sRGB independently of the palette definition. The paint mask distinguishes holes from painted pixels even when all 256 palette entries are used. For `auto` inputs, legacy loader/frame alpha-composition behavior is retained; it is path-dependent and must be tested separately.

## Per-pixel and regional differences

`-m DELTA_E00` prints `Δ E00_mean`, the arithmetic mean of per-pixel CIEDE2000. `Δ Chroma_mean` averages absolute differences in CIELAB chroma magnitude. Both are lower-is-better and use the common D50 white.

The regional metrics first average XYZ within each tile, convert the two mean colors to Lab, then compute CIEDE2000:

- `-m DE00_region_mean`: area-weighted mean of tile color differences.
- `-m DE00_region_max`: maximum tile color difference.

`--region-window=8|16|32|64` (`-w`, default 16) selects fixed square tiles, anchored at the top-left corner. Right and bottom partial tiles are included; their actual pixel area supplies the weight. As with existing metrics, images of different dimensions are compared over their common top-left rectangle. Use the same window and alignment for all variants in a study.

A black/white dither with linear mean 0.5 can have zero regional error against a constant Y=0.5 patch while retaining substantial per-pixel error. Conversely, swapping black and white regions can preserve the whole-image mean while every local tile is wrong. Report both regional mean and maximum alongside spatial metrics: correct mean color does not imply acceptable grain, streaks, edges or banding. Natural images use the same fixed grid, not windows selected after seeing the result.

Scalar `-m` output remains one number. Full JSON includes `color_evaluation`: requested per-input definitions and their origin, actual current frame formats and coordinates, requested loader CMS, actual builtin evaluator, plain SIXEL and background definitions, signal precision/range, PCS white/units, and tile layout/count. `legacy-loader-managed` records a pipeline origin; it does not claim that metadata was successfully applied merely because it existed.

The existing `-b` CLI contract tests whether a metric is below a baseline. It is useful for similarity metrics such as MS-SSIM; it is not an upper-bound quality gate for color differences. Compare these lower-is-better values with an explicitly defined upper limit in the measurement driver.

## Independent validation

The numeric suite uses the production-independent fixtures in `tests/data/color-numeric/`, constructed float frames, and published [CIEDE2000 supplementary test pairs](https://hajim.rochester.edu/ece/sites/gsharma/ciede2000/). The published four-decimal ΔE values are checked within 0.00005. Analytical coordinate comparisons use 1e-6 XYZ tolerance for the inverse opponent path; the typed float32 P3-red frame tests use 0.00002 ΔE to cover storage rounding. These are test-specific bounds, not a universal zero-error bound for all float32 colors.

Tests distinguish same physical colors in different coordinates, identical RGB numbers with different primaries, explicit P3 clipping, physical averaging, spatial cancellation, and partial-edge weighting. P3 numeric fixture gray is exactly 0.5; the PNG fixture stores RGB8 128, which is 128/255. They are separate inputs and must not share a presumed exact expected value.

See the [measurement policy](measurement-policy.md) for unchanged sRGB MS-SSIM gates and the [loader CMS guide](../loader/color-management.md) for the legacy normalization boundary.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| CE-01 | Published CIEDE2000 pairs, including zero chroma and hue wrap. | [tests/processing/assessment/0001_ciede2000_reference.t](../../tests/processing/assessment/0001_ciede2000_reference.t) |
| CE-02 | Same physical colors in gamma, linear and opponent coordinates. | [tests/processing/assessment/0002_same_physical_coordinates.t](../../tests/processing/assessment/0002_same_physical_coordinates.t) |
| CE-03 | Physical means at 8, 16, 32 and 64 pixels. | [tests/processing/assessment/0005_region_physical_average.t](../../tests/processing/assessment/0005_region_physical_average.t) |
| CE-04 | Local error is retained when global means match. | [tests/processing/assessment/0006_region_spatial_cancellation.t](../../tests/processing/assessment/0006_region_spatial_cancellation.t) |
| CE-05 | Partial tiles are included with pixel-area weights. | [tests/processing/assessment/0007_region_partial_edge.t](../../tests/processing/assessment/0007_region_partial_edge.t) |
| CE-06 | Current Lab frames are decoded before measurement. | [tests/processing/assessment/0008_frame_linear_lab.t](../../tests/processing/assessment/0008_frame_linear_lab.t) |
| CE-07 | Current Oklab frames are decoded before measurement. | [tests/processing/assessment/0009_frame_linear_oklab.t](../../tests/processing/assessment/0009_frame_linear_oklab.t) |
| CE-08 | Current DIN99d frames are decoded before measurement. | [tests/processing/assessment/0010_frame_linear_din99d.t](../../tests/processing/assessment/0010_frame_linear_din99d.t) |
| CE-09 | Target interpretation is independent. | [tests/quality_gate/cms/0011_target_definition_independent.t](../../tests/quality_gate/cms/0011_target_definition_independent.t) |
| CE-10 | Reference interpretation is independent. | [tests/quality_gate/cms/0012_reference_definition_independent.t](../../tests/quality_gate/cms/0012_reference_definition_independent.t) |
| CE-11 | Identical P3 interpretations have zero difference. | [tests/quality_gate/cms/0013_same_p3_definition.t](../../tests/quality_gate/cms/0013_same_p3_definition.t) |
| CE-12 | Explicit interpretation applies to stdin. | [tests/quality_gate/cms/0014_target_definition_stdin.t](../../tests/quality_gate/cms/0014_target_definition_stdin.t) |
| CE-13 | Forward-only ICC profiles are accepted as input. | [tests/quality_gate/cms/0015_forward_only_icc_input.t](../../tests/quality_gate/cms/0015_forward_only_icc_input.t) |
| CE-14 | Equivalent custom RGB and named P3 definitions agree. | [tests/quality_gate/cms/0016_simple_rgb_matches_p3.t](../../tests/quality_gate/cms/0016_simple_rgb_matches_p3.t) |
| CE-15 | Unknown definitions are rejected. | [tests/quality_gate/cms/0017_unknown_definition_error.t](../../tests/quality_gate/cms/0017_unknown_definition_error.t) |
| CE-16 | Unreadable ICC files are rejected. | [tests/quality_gate/cms/0018_unreadable_icc_error.t](../../tests/quality_gate/cms/0018_unreadable_icc_error.t) |
| CE-17 | PNG overrides cannot reinterpret normalized samples. | [tests/quality_gate/cms/0019_png_override_pending_error.t](../../tests/quality_gate/cms/0019_png_override_pending_error.t) |
| CE-18 | Explicit auto preserves default behavior. | [tests/quality_gate/cms/0020_auto_definition_compatibility.t](../../tests/quality_gate/cms/0020_auto_definition_compatibility.t) |
| CE-19 | JSON distinguishes requested CMS and effective evaluator. | [tests/quality_gate/cms/0021_color_context_json.t](../../tests/quality_gate/cms/0021_color_context_json.t) |
| CE-20 | Background definition is independent of the palette. | [tests/quality_gate/cms/0022_background_definition_independent.t](../../tests/quality_gate/cms/0022_background_definition_independent.t) |
| CE-21 | An 8-pixel window is accepted. | [tests/quality_gate/cms/0023_region_window_8.t](../../tests/quality_gate/cms/0023_region_window_8.t) |
| CE-22 | A 16-pixel window is accepted. | [tests/quality_gate/cms/0024_region_window_16.t](../../tests/quality_gate/cms/0024_region_window_16.t) |
| CE-23 | A 32-pixel window is accepted. | [tests/quality_gate/cms/0025_region_window_32.t](../../tests/quality_gate/cms/0025_region_window_32.t) |
| CE-24 | A 64-pixel window is accepted. | [tests/quality_gate/cms/0026_region_window_64.t](../../tests/quality_gate/cms/0026_region_window_64.t) |
| CE-25 | Signal quantization cannot clip color measurements. | [tests/quality_gate/cms/0027_signal_quantization_preserves_color.t](../../tests/quality_gate/cms/0027_signal_quantization_preserves_color.t) |
| CE-26 | Grayscale signal selection cannot discard physical color. | [tests/quality_gate/cms/0028_grayscale_signal_preserves_color.t](../../tests/quality_gate/cms/0028_grayscale_signal_preserves_color.t) |
| CE-27 | Invalid ICC bytes are rejected. | [tests/quality_gate/cms/0029_invalid_icc_error.t](../../tests/quality_gate/cms/0029_invalid_icc_error.t) |
| CE-28 | Two stdin readers are rejected. | [tests/quality_gate/cms/0030_dual_stdin_error.t](../../tests/quality_gate/cms/0030_dual_stdin_error.t) |
| CE-29 | Unsupported window sizes are rejected. | [tests/quality_gate/cms/0031_invalid_region_window_error.t](../../tests/quality_gate/cms/0031_invalid_region_window_error.t) |
| CE-30 | Explicit target definition and dequantization are rejected together. | [tests/quality_gate/cms/0032_explicit_dequantize_error.t](../../tests/quality_gate/cms/0032_explicit_dequantize_error.t) |

### Quality regression tests

The existing [sRGB quality gates](../../tests/quality_gate/ms_ssim/) and [legacy CMS comparisons](../../tests/quality_gate/cms/) retain their established thresholds. The color-coordinate tests above do not establish a perceptual quality floor for a newly encoded image.

### Defensive and malformed-input tests

The [measurement unit suite](../../tests/processing/assessment/) also exercises malformed numeric definitions, negative clipping controls, and the strict shared grammar. The source fixtures `red.six` and `hole.six` are hand-authored 16 by 16 standalone DCS streams: the first paints exact RGB endpoints; the second leaves its raster unpainted. They test adapter interpretation and background selection independently of the image encoder. The PNG integration boundary still requires separate loader validation.
