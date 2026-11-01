# Color-aware quality evaluation

`lsqa` separates the interpretation of an input sample, the physical color used for color difference, and the signal used for spatial quality metrics. These are independent choices. An RGB triplet alone does not identify a color.

Explicit SIXEL interpretation and image-loader CMS are separate boundaries. The SIXEL descriptor tests do not establish that every PNG loader preserves extended colors or applies an embedded profile. For PNG references, select a CMS-capable loader and a float-preserving configuration within its documented [support and test scope](../loader/color-management/README.md). A normalized frame uses the canonical sRGB basis; that basis alone does not imply clipping, because float coordinates can retain colors outside the sRGB gamut.

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
| `adobe-rgb` | Adobe RGB (1998), D65 white, decoding exponent 563/256. |
| `dci-p3` | P3 primaries, DCI white, decoding exponent 2.6. |
| `rgb:wx,wy,rx,ry,gx,gy,bx,by:srgb` | Custom white and RGB primary xy coordinates with the sRGB transfer function. |
| `rgb:wx,wy,rx,ry,gx,gy,bx,by:gamma=r,g,b` | The same xy layout with three positive per-channel decoding exponents, each at most 10. |
| `icc:FILE` | RGB ICC input profile, relative colorimetric slot 1. |
| `icc:perceptual:FILE` | RGB ICC input profile, slot 0. |
| `icc:relative:FILE` | RGB ICC input profile, slot 1. |
| `icc:saturation:FILE` | RGB ICC input profile, slot 2. |

Names and field counts are exact. Invalid xy coordinates, nonfinite numbers, missing fields, unreadable or inapplicable ICC profiles are errors. An ICC profile is a local file, never standard input. The shared parser performs no file access; each adapter owns profile loading and lifetime.

Explicit definitions currently require a standalone SIXEL DCS stream. They apply to the decoded **original palette or paint-time samples**, before background selection and before conversion to extended linear sRGB. SIXEL's integer percentage to RGB8 rounding is unchanged. The raw decoder has no new CMS policy.

Explicit PNG overrides report an error until the pre-normalization loader connection is validated. They must never reinterpret already normalized sRGB as though it were the original P3 samples. `auto` PNG keeps its existing loader CMS behavior. Explicit target definitions with `-d` dequantization are also rejected because that reconstruction needs its own color-space contract.

The new descriptor evaluator uses the builtin ICC PCS implementation. `--cms-engine` continues to select the legacy image-loader CMS; it does not change the descriptor evaluator. ICC input only needs a usable forward transform. An inverse table is not required. Slot selection does not add black-point compensation, absolute adaptation, or an external CMS profile-link operation.

### Evaluating Display P3 output

The encoder's `-Udisplay-p3` selects an output definition; the corresponding evaluator option is `-Tdisplay-p3`. For a PNG reference, leave `-Rauto` and let its embedded profile reach a CMS-capable loader. `-Rdisplay-p3` is for an original SIXEL reference, not for relabeling normalized PNG samples. For example, the builtin PNG loader can be selected explicitly for both reference loading and encoding:

```sh
input=tests/data/color-numeric/png-p3-16-rgb.png
converters/img2sixel "$input" --threading-policy=greedy:global_budget=1 \
    -L'builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!' \
    -Udisplay-p3 -_none > output.six
assessment/lsqa -L'builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!' \
    -Tdisplay-p3 "$input" output.six
```

The full JSON records each input's effective coordinate/format and the target definition. Its physical color metrics and [regional means](#per-pixel-and-regional-differences) complement MS-SSIM when studying destination rounding or a proposed destination-aware snap. The evaluator decodes SIXEL percentage values to RGB8 before interpretation; such measurements include that decoder's rounding, and do not model a receiver retaining exact `q/100` values. They also do not measure display calibration or operating-system color filters.

[Destination-aware snap](../encoder/palette-construction/snap-policy/README.md#output-conversion-and-destination-aware-snap) can be compared by changing only `-_none` to `-_nearest` or a selected timing/grid strategy. Existing sRGB snap results do not demonstrate a P3 benefit. Hold input interpretation and other encoding controls fixed; the [measurement policy](measurement-policy.md#controlled-comparisons) owns those controls.

## SIXEL drawing mode

`-D / --direct` selects direct rendering for **both** SIXEL inputs. Drawing mode is independent of `-R` and `-T`: `auto`, sRGB, P3, custom RGB and ICC definitions all accept it. With no `-D`, the existing indexed path remains the default, including final-palette interpretation after register redefinition. Choose the same drawing mode when comparing measurement tools or decoded reference images.

```sh
lsqa -D -Rdisplay-p3 -Tdisplay-p3 -m DELTA_E00 stable.six redefined.six
producer | lsqa --direct -Tdisplay-p3 reference.png -
```

Direct rendering executes the existing decoder's painting semantics. For ordinary SIXEL paint, a later register definition does not recolor already painted pixels; repainting replaces only the covered pixels. The adapter first retains original RGBA samples and unpainted alpha, then applies explicit input definitions into extended float32 linear sRGB. It does not use the RGB8 palette-transform callback for that conversion, which would clip extended colors. The decoder's existing OR-mode semantics remain as described in the [decoding pipeline](../decoder/pipeline/README.md).

For direct SIXEL, unpainted regions use `-B`, or black, in encoded sRGB independently of the input definition. Painted RGB endpoints remain exact before explicit color conversion. Other formats keep their normal loader behavior; this includes PNG from a file or either input's stdin. The opt-in format check reuses the loaded chunk, so a pipe is consumed only once and loader source metadata remains available. Explicit PNG overrides remain unsupported.

`-D -d METHOD` accepts the existing target reconstruction methods `k_undither`, `lso_undither:Vfs` and `selective_blur`, with their existing similarity, edge and threshold controls. It reuses the stateful decoder's CPU RGBA reconstruction and paint mask, including its existing bypass to direct paint-time samples when a palette register was redefined after paint. That bypass means the requested reconstruction does not run on those streams. The reference receives direct decoding without reconstruction. `-d none` leaves direct rendering enabled. An explicit target definition with non-`none` reconstruction is still rejected, including with `-D`; its color-space contract is not yet defined. Neither the converter's reconstruction policy nor its fallback implementation changes here.

Full JSON records `requested_sixel_drawing` as `direct` or `indexed`. Each input's `drawing` reports `direct`, explicit `indexed`, `loader-managed` for the unchanged auto loader route, or `not-applicable` when `-D` encounters another format. `original-sixel-srgb` identifies the direct auto adapter's origin; explicit definitions retain `explicit-original-sixel`. These fields describe drawing and interpretation, not a claim that requested reconstruction ran on a redefined-palette stream.

## Physical color and comparison signal

The evaluator reads a frame's **current coordinate**, not an original profile tag. Gamma sRGB, linear sRGB, Oklab, normalized CIELAB and DIN99d working coordinates describe the existing sRGB/D65 basis. Their inverse transforms recover physical color before Bradford adaptation to XYZ D50. Signed and above-one intermediate components remain available to the measurement.

Color difference uses XYZ with `Y=1` for white and ICC D50 `(Xn, Yn, Zn) = (0.9642, 1, 0.8249)`. CIELAB then uses conventional `L*`, `a*`, `b*` units; it is not the library's normalized Lab storage. CIEDE2000 uses unit parametric factors. This replaces the old sRGB-only, D65 color-difference interpretation, which incorrectly decoded linear and opponent components as encoded RGB.

Either input with zero chroma has hue zero by the CIEDE2000 convention. The evaluator applies that rule before calling `atan2`, so runtimes that return NaN for `atan2(0, 0)` cannot contaminate black, white, or neutral comparisons. The reference tests include zero chroma in both input positions, black versus white, and identical black samples.

`-W / --compare-colorspace` selects the signal for both MS-SSIM variants, PSNR-Y, GMSD and the other spatial/luma diagnostics:

| Selection | Signal |
| --- | --- |
| `reference` | Follow the reference's current coordinate; gamma uses encoded luma, other supported coordinates use linear Y. |
| `gamma` | Encoded sRGB luma, `0.2126 R' + 0.7152 G' + 0.0722 B'`. |
| `linear` | Linear sRGB Y with the same coefficients. |
| `oklab`, `cielab`, `din99d` | Recover linear sRGB and use Y. Opponent components are never weighted as RGB. |

Unchanged sRGB gamma/linear samples retain the existing luma coefficients and float32 representation. The coefficients are the historical metric definition; they are distinct from the full-precision primary matrix used for PCS.

`-P / --compare-precision=reference|8bit|float32` controls this signal. The reference default follows its storage. Eight-bit signals are clipped to 0..1 and rounded to 255 intervals; float32 signals retain extended values, with a nominal range of 0..1. PSNR-Y uses a unit peak, and spatial metric constants retain that nominal range. Clip-rate metrics describe this comparison signal. They cannot prove how much color an earlier loader or encoder clipped.

`-g` makes the comparison signal grayscale. Color difference, region color means and S-CIELAB still measure the physical input colors. Neither `-W`, `-P`, nor `-g` changes the XYZ used for those color measurements.

Explicit SIXEL inputs use linear float32 after interpretation. Unpainted pixels use the requested `-B` background, or black, interpreted as encoded sRGB independently of the palette definition. The paint mask distinguishes holes from painted pixels even when all 256 palette entries are used. For `auto` inputs without `-D`, legacy loader/frame alpha-composition behavior is retained; it is path-dependent and must be tested separately.

## Per-pixel and regional differences

`-m DELTA_E00` prints `Δ E00_mean`, the arithmetic mean of per-pixel CIEDE2000. `Δ Chroma_mean` averages absolute differences in CIELAB chroma magnitude. Both are lower-is-better and use the common D50 white.

`-m DE00_max` (alias `DELTA_E00_MAX`) reports the maximum per-pixel CIEDE2000 before any spatial averaging. In the full JSON report, `color_evaluation.pixel_delta_e00` gives the zero-based `maximum_xy` (first row-major occurrence for ties; `[0, 0]` when all errors are zero) and counts strictly above 1, 5, 10, 20 and 50. These fields are null when the maximum metric was not computed. Unlike a mean, the maximum cannot hide an isolated black or white fault among many correct pixels. Normal dithering can also produce large individual errors: compare the tail counts, locations and regional means under identical settings instead of imposing a universal maximum threshold.

The regional metrics first average XYZ within each tile, convert the two mean colors to Lab, then compute CIEDE2000:

- `-m DE00_region_mean`: area-weighted mean of tile color differences.
- `-m DE00_region_max`: maximum tile color difference.

`--region-window=8|16|32|64` (`-w`, default 16) selects fixed square tiles, anchored at the top-left corner. Right and bottom partial tiles are included; their actual pixel area supplies the weight. As with existing metrics, images of different dimensions are compared over their common top-left rectangle. Use the same window and alignment for all variants in a study.

A black/white dither with linear mean 0.5 can have zero regional error against a constant Y=0.5 patch while retaining substantial per-pixel error. Conversely, swapping black and white regions can preserve the whole-image mean while every local tile is wrong. Report both regional mean and maximum alongside spatial metrics: correct mean color does not imply acceptable grain, streaks, edges or banding. Natural images use the same fixed grid, not windows selected after seeing the result.

A single unique `-m` selection outputs one number; multiple selections use the [named metric JSON interface](metrics.md#selecting-metrics-and-gates). Full JSON includes `color_evaluation`: requested per-input definitions and their origin, selected SIXEL drawing mode and per-input route, actual current frame formats and coordinates, requested loader CMS, actual builtin evaluator, plain SIXEL and background definitions, signal precision/range, PCS white/units, and tile layout/count. `legacy-loader-managed` records a pipeline origin; it does not claim that metadata was successfully applied merely because it existed.

Use an explicit upper bound such as `-b DE00_region_max:max=VALUE` for color differences. Combine it with a structural lower bound in the same invocation; the [metric selection and gate reference](metrics.md#selecting-metrics-and-gates) defines the syntax, combined result, output and compatibility rules.

## Spatial color filtering

[S-CIELAB](metrics.md#spatial-color-difference-s-cielab) uses the same physical XYZ D50 inputs, with separate achromatic and chromatic spatial filters before Lab conversion. `-m S-CIELAB` reports mean CIE76 error and `-m S-CIELAB_max` reports its maximum; `-v` / `--scielab-ppd` sets the viewing geometry. The legacy `-b METRIC:VALUE` form uses an upper bound for these two metrics. Their owning metric reference specifies the kernels, default geometry, units, boundary extension, numeric tests and limitations. Full JSON adds the requested PPD and the implemented model, difference formula and boundary rule to `color_evaluation`.

## Independent validation

The numeric suite uses the production-independent fixtures in `tests/data/color-numeric/`, constructed float frames, and published [CIEDE2000 supplementary test pairs](https://hajim.rochester.edu/ece/sites/gsharma/ciede2000/). The published four-decimal ΔE values are checked within 0.00005. Analytical coordinate comparisons use 1e-6 XYZ tolerance for the inverse opponent path; the typed float32 P3-red frame tests use 0.00002 ΔE to cover storage rounding. These are test-specific bounds, not a universal zero-error bound for all float32 colors.

Tests distinguish same physical colors in different coordinates, identical RGB numbers with different primaries, explicit P3 clipping, physical averaging, spatial cancellation, and partial-edge weighting. P3 numeric fixture gray is exactly 0.5; the PNG fixture stores RGB8 128, which is 128/255. They are separate inputs and must not share a presumed exact expected value.

The `png-samples.json` and `png-samples.h` fixtures additionally separate source RGB/RGBA integers at 8 and 16 bits, ideal P3 coordinates, exact encoded ICC matrix/TRC coefficients, and sRGB override expectations. The 16 small PNGs include tagged and untagged samples, orientation 6, and a green file background. Alpha remains independent coverage (`integer / 255` or `integer / 65535`). Their midtones are 128/255 and 32768/65535, neither the analytic 0.5 fixture. These independent fixtures define the loader integration target; their existence does not establish that the current loader retains extended color.

See [color difference and color reproduction](color-difference.md) for global and multiscale color measures and tolerance grades, the [measurement policy](measurement-policy.md) for unchanged sRGB MS-SSIM gates and the [loader CMS guide](../loader/color-management/README.md) for the legacy normalization boundary.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| CE-01 | Published CIEDE2000 pairs, including zero chroma and hue wrap. | [tests/assessment/delta-e/0001_ciede2000_reference.t](../../tests/assessment/delta-e/0001_ciede2000_reference.t) |
| CE-02 | Same physical colors in gamma, linear and opponent coordinates. | [tests/assessment/delta-e/0002_same_physical_coordinates.t](../../tests/assessment/delta-e/0002_same_physical_coordinates.t) |
| CE-03 | Physical means at 8, 16, 32 and 64 pixels. | [tests/assessment/delta-e/0005_region_physical_average.t](../../tests/assessment/delta-e/0005_region_physical_average.t) |
| CE-04 | Local error is retained when global means match. | [tests/assessment/delta-e/0006_region_spatial_cancellation.t](../../tests/assessment/delta-e/0006_region_spatial_cancellation.t) |
| CE-05 | Partial tiles are included with pixel-area weights. | [tests/assessment/delta-e/0007_region_partial_edge.t](../../tests/assessment/delta-e/0007_region_partial_edge.t) |
| CE-06 | Current Lab frames are decoded before measurement. | [tests/assessment/delta-e/0008_frame_linear_lab.t](../../tests/assessment/delta-e/0008_frame_linear_lab.t) |
| CE-07 | Current Oklab frames are decoded before measurement. | [tests/assessment/delta-e/0009_frame_linear_oklab.t](../../tests/assessment/delta-e/0009_frame_linear_oklab.t) |
| CE-08 | Current DIN99d frames are decoded before measurement. | [tests/assessment/delta-e/0010_frame_linear_din99d.t](../../tests/assessment/delta-e/0010_frame_linear_din99d.t) |
| CE-09 | Target interpretation is independent. | [tests/assessment/cms/0011_target_definition_independent.t](../../tests/assessment/cms/0011_target_definition_independent.t) |
| CE-10 | Reference interpretation is independent. | [tests/assessment/cms/0012_reference_definition_independent.t](../../tests/assessment/cms/0012_reference_definition_independent.t) |
| CE-11 | Identical P3 interpretations have zero difference. | [tests/assessment/cms/0013_same_p3_definition.t](../../tests/assessment/cms/0013_same_p3_definition.t) |
| CE-12 | Explicit interpretation applies to stdin. | [tests/assessment/cms/0014_target_definition_stdin.t](../../tests/assessment/cms/0014_target_definition_stdin.t) |
| CE-13 | Forward-only ICC profiles are accepted as input. | [tests/assessment/cms/0015_forward_only_icc_input.t](../../tests/assessment/cms/0015_forward_only_icc_input.t) |
| CE-14 | Equivalent custom RGB and named P3 definitions agree. | [tests/assessment/cms/0016_simple_rgb_matches_p3.t](../../tests/assessment/cms/0016_simple_rgb_matches_p3.t) |
| CE-15 | Unknown definitions are rejected. | [tests/assessment/cms/0017_unknown_definition_error.t](../../tests/assessment/cms/0017_unknown_definition_error.t) |
| CE-16 | Unreadable ICC files are rejected. | [tests/assessment/cms/0018_unreadable_icc_error.t](../../tests/assessment/cms/0018_unreadable_icc_error.t) |
| CE-17 | PNG overrides cannot reinterpret normalized samples. | [tests/assessment/cms/0019_png_override_pending_error.t](../../tests/assessment/cms/0019_png_override_pending_error.t) |
| CE-18 | Explicit auto preserves default behavior. | [tests/assessment/cms/0020_auto_definition_compatibility.t](../../tests/assessment/cms/0020_auto_definition_compatibility.t) |
| CE-19 | JSON distinguishes requested CMS and effective evaluator. | [tests/assessment/cms/0021_color_context_json.t](../../tests/assessment/cms/0021_color_context_json.t) |
| CE-20 | Background definition is independent of the palette. | [tests/assessment/cms/0022_background_definition_independent.t](../../tests/assessment/cms/0022_background_definition_independent.t) |
| CE-21 | An 8-pixel window is accepted. | [tests/assessment/cms/0023_region_window_8.t](../../tests/assessment/cms/0023_region_window_8.t) |
| CE-22 | A 16-pixel window is accepted. | [tests/assessment/cms/0024_region_window_16.t](../../tests/assessment/cms/0024_region_window_16.t) |
| CE-23 | A 32-pixel window is accepted. | [tests/assessment/cms/0025_region_window_32.t](../../tests/assessment/cms/0025_region_window_32.t) |
| CE-24 | A 64-pixel window is accepted. | [tests/assessment/cms/0026_region_window_64.t](../../tests/assessment/cms/0026_region_window_64.t) |
| CE-25 | Signal quantization cannot clip color measurements. | [tests/assessment/cms/0027_signal_quantization_preserves_color.t](../../tests/assessment/cms/0027_signal_quantization_preserves_color.t) |
| CE-26 | Grayscale signal selection cannot discard physical color. | [tests/assessment/cms/0028_grayscale_signal_preserves_color.t](../../tests/assessment/cms/0028_grayscale_signal_preserves_color.t) |
| CE-27 | Invalid ICC bytes are rejected. | [tests/assessment/cms/0029_invalid_icc_error.t](../../tests/assessment/cms/0029_invalid_icc_error.t) |
| CE-28 | Two stdin readers are rejected. | [tests/assessment/cms/0030_dual_stdin_error.t](../../tests/assessment/cms/0030_dual_stdin_error.t) |
| CE-29 | Unsupported window sizes are rejected. | [tests/assessment/cms/0031_invalid_region_window_error.t](../../tests/assessment/cms/0031_invalid_region_window_error.t) |
| CE-30 | Explicit target definition and dequantization are rejected together. | [tests/assessment/cms/0032_explicit_dequantize_error.t](../../tests/assessment/cms/0032_explicit_dequantize_error.t) |
| CE-31 | Direct auto target retains paint-time colors. | [tests/assessment/direct/0001_auto_target.t](../../tests/assessment/direct/0001_auto_target.t) |
| CE-32 | Direct auto reference retains paint-time colors. | [tests/assessment/direct/0002_auto_reference.t](../../tests/assessment/direct/0002_auto_reference.t) |
| CE-33 | Direct explicit sRGB preserves palette redefinition. | [tests/assessment/direct/0003_srgb_target.t](../../tests/assessment/direct/0003_srgb_target.t) |
| CE-34 | Direct P3 target preserves palette redefinition. | [tests/assessment/direct/0004_p3_target.t](../../tests/assessment/direct/0004_p3_target.t) |
| CE-35 | Direct P3 reference preserves palette redefinition. | [tests/assessment/direct/0005_p3_reference.t](../../tests/assessment/direct/0005_p3_reference.t) |
| CE-36 | Direct custom RGB matches named P3. | [tests/assessment/direct/0006_simple_rgb.t](../../tests/assessment/direct/0006_simple_rgb.t) |
| CE-37 | Direct ICC interpretation retains paint history. | [tests/assessment/direct/0007_icc_redefinition.t](../../tests/assessment/direct/0007_icc_redefinition.t) |
| CE-38 | Direct ICC interpretation changes physical color. | [tests/assessment/direct/0008_icc_definition.t](../../tests/assessment/direct/0008_icc_definition.t) |
| CE-39 | Direct target definition remains independent. | [tests/assessment/direct/0009_target_definition.t](../../tests/assessment/direct/0009_target_definition.t) |
| CE-40 | Direct reference definition remains independent. | [tests/assessment/direct/0010_reference_definition.t](../../tests/assessment/direct/0010_reference_definition.t) |
| CE-41 | Direct auto reference accepts stdin. | [tests/assessment/direct/0011_reference_stdin.t](../../tests/assessment/direct/0011_reference_stdin.t) |
| CE-42 | Direct P3 target accepts stdin. | [tests/assessment/direct/0012_target_p3_stdin.t](../../tests/assessment/direct/0012_target_p3_stdin.t) |
| CE-43 | Repainting after redefinition changes only overwritten pixels. | [tests/assessment/direct/0013_repaint.t](../../tests/assessment/direct/0013_repaint.t) |
| CE-44 | Sparse P3 paint keeps earlier colors and unpainted background. | [tests/assessment/direct/0014_sparse_p3.t](../../tests/assessment/direct/0014_sparse_p3.t) |
| CE-45 | Reference holes use sRGB background independently of P3. | [tests/assessment/direct/0015_reference_background.t](../../tests/assessment/direct/0015_reference_background.t) |
| CE-46 | Target holes use sRGB background independently of P3. | [tests/assessment/direct/0016_target_background.t](../../tests/assessment/direct/0016_target_background.t) |
| CE-47 | Omitting -D retains auto indexed evaluation. | [tests/assessment/direct/0017_default_indexed.t](../../tests/assessment/direct/0017_default_indexed.t) |
| CE-48 | Omitting -D retains P3 indexed evaluation. | [tests/assessment/direct/0018_default_p3_indexed.t](../../tests/assessment/direct/0018_default_p3_indexed.t) |
| CE-49 | Direct selection leaves png file loader results unchanged. | [tests/assessment/direct/0019_png_file.t](../../tests/assessment/direct/0019_png_file.t) |
| CE-50 | Direct selection leaves png target stdin loader results unchanged. | [tests/assessment/direct/0020_png_target_stdin.t](../../tests/assessment/direct/0020_png_target_stdin.t) |
| CE-51 | Direct selection leaves png reference stdin loader results unchanged. | [tests/assessment/direct/0021_png_reference_stdin.t](../../tests/assessment/direct/0021_png_reference_stdin.t) |
| CE-52 | JSON reports direct drawing selection and both input routes. | [tests/assessment/direct/0022_direct_json.t](../../tests/assessment/direct/0022_direct_json.t) |
| CE-53 | JSON reports png drawing selection and both input routes. | [tests/assessment/direct/0023_png_json.t](../../tests/assessment/direct/0023_png_json.t) |
| CE-54 | JSON reports indexed drawing selection and both input routes. | [tests/assessment/direct/0024_indexed_json.t](../../tests/assessment/direct/0024_indexed_json.t) |
| CE-55 | Help advertises both direct option spellings. | [tests/assessment/direct/0025_help.t](../../tests/assessment/direct/0025_help.t) |
| CE-56 | Reject direct value. | [tests/assessment/direct/0026_direct_value_error.t](../../tests/assessment/direct/0026_direct_value_error.t) |
| CE-57 | Reject explicit dequantize. | [tests/assessment/direct/0027_explicit_dequantize_error.t](../../tests/assessment/direct/0027_explicit_dequantize_error.t) |
| CE-58 | Dequantize none does not disable direct drawing. | [tests/assessment/direct/0028_dequantize_none.t](../../tests/assessment/direct/0028_dequantize_none.t) |
| CE-59 | Direct kundither target reconstruction matches the established opaque result through stdin. | [tests/assessment/direct/0029_reconstruct_kundither.t](../../tests/assessment/direct/0029_reconstruct_kundither.t) |
| CE-60 | Direct fast4 target reconstruction matches the established opaque result through stdin. | [tests/assessment/direct/0030_reconstruct_fast4.t](../../tests/assessment/direct/0030_reconstruct_fast4.t) |
| CE-61 | Direct selective_blur target reconstruction matches the established opaque result through stdin. | [tests/assessment/direct/0031_reconstruct_selective_blur.t](../../tests/assessment/direct/0031_reconstruct_selective_blur.t) |
| CE-62 | Direct kundither uses the existing redefinition bypass. | [tests/assessment/direct/0032_redefined_kundither.t](../../tests/assessment/direct/0032_redefined_kundither.t) |
| CE-63 | Direct fast4 uses the existing redefinition bypass. | [tests/assessment/direct/0033_redefined_fast4.t](../../tests/assessment/direct/0033_redefined_fast4.t) |
| CE-64 | Direct selective_blur uses the existing redefinition bypass. | [tests/assessment/direct/0034_redefined_selective_blur.t](../../tests/assessment/direct/0034_redefined_selective_blur.t) |
| CE-65 | Direct kundither retains unpainted background. | [tests/assessment/direct/0035_background_kundither.t](../../tests/assessment/direct/0035_background_kundither.t) |
| CE-66 | Direct fast4 retains unpainted background. | [tests/assessment/direct/0036_background_fast4.t](../../tests/assessment/direct/0036_background_fast4.t) |
| CE-67 | Direct selective_blur retains unpainted background. | [tests/assessment/direct/0037_background_selective_blur.t](../../tests/assessment/direct/0037_background_selective_blur.t) |
| CE-68 | Direct auto target accepts stdin. | [tests/assessment/direct/0038_target_auto_stdin.t](../../tests/assessment/direct/0038_target_auto_stdin.t) |
| CE-69 | Direct P3 reference accepts stdin. | [tests/assessment/direct/0039_reference_p3_stdin.t](../../tests/assessment/direct/0039_reference_p3_stdin.t) |
| CE-70 | Maximum-only selection reports a sparse black/white error of 100, its position and tail counts. | [tests/assessment/delta-e/0013_pixel_delta_peak.t](../../tests/assessment/delta-e/0013_pixel_delta_peak.t) |
| CE-71 | A single corrupt pixel has mean 100/4096 but maximum 100. | [tests/assessment/delta-e/0014_pixel_delta_mean.t](../../tests/assessment/delta-e/0014_pixel_delta_mean.t) |
| CE-72 | Equal maxima select the first position; repeat analysis clears previous statistics. | [tests/assessment/delta-e/0015_pixel_delta_reset.t](../../tests/assessment/delta-e/0015_pixel_delta_reset.t) |
| CE-73 | CLI maximum-only selection. | [tests/assessment/cms/0035_pixel_delta_max.t](../../tests/assessment/cms/0035_pixel_delta_max.t) |
| CE-74 | The DELTA_E00_MAX alias selects the same metric. | [tests/assessment/cms/0036_pixel_delta_max_alias.t](../../tests/assessment/cms/0036_pixel_delta_max_alias.t) |
| CE-75 | Full JSON includes maximum, position and tail counts. | [tests/assessment/cms/0037_pixel_delta_json.t](../../tests/assessment/cms/0037_pixel_delta_json.t) |
| CE-76 | Neutral sRGB samples independently exercise each tail threshold. | [tests/assessment/delta-e/0016_pixel_delta_tails.t](../../tests/assessment/delta-e/0016_pixel_delta_tails.t) |

### Quality regression tests

The existing [sRGB quality gates](../../tests/assessment/ms-ssim/) and [legacy CMS comparisons](../../tests/assessment/cms/) retain their established thresholds. The color-coordinate tests above do not establish a perceptual quality floor for a newly encoded image.

### Defensive and malformed-input tests

The [measurement unit suite](../../tests/assessment) also exercises malformed numeric definitions, negative clipping controls, and the strict shared grammar. The source fixtures `red.six` and `hole.six` are hand-authored 16 by 16 standalone DCS streams: the first paints exact RGB endpoints; the second leaves its raster unpainted. They test adapter interpretation and background selection independently of the image encoder. The PNG integration boundary still requires separate loader validation.

The direct adapter fixtures use 16 by 18 rasters: `stable.six` and `redefined.six` paint the same red-left/green-right image using stable and reused registers; `repaint-reference.six` and `repaint.six` independently specify green-left/red-right output; `sparse.six` and `sparse-redefined.six` leave the right half unpainted. They are hand-authored endpoint streams, with no encoder or production color transform used to generate expected paint. Without `-D`, the first pair retains its archived six-decimal indexed differences (42.156315 for auto/sRGB and 44.478863 for P3); direct rendering yields 0.000000 in each interpretation.
