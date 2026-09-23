# CoreGraphics and ImageIO loader

The `coregraphics` loader uses Apple's ImageIO source APIs and CoreGraphics to obtain pixels, then performs libsixel's representation, orientation, alpha, and animation integration. Supported containers and codecs depend on the host framework; the name is not a fixed cross-version list of image formats.

```sh
img2sixel -L 'coregraphics!' image.heic
img2sixel -L 'coregraphics:orientation=0!' image.jpg
img2sixel -L 'coregraphics:cache_max_bytes=0!' -l disable animation.gif
```

## Pixels, metadata, and animation

The adapter has indexed, byte RGB, and float32 conversion paths. It can preserve indexed structure when eligible, while source depth, color interpretation, or alpha/background processing can select a different representation. Profile interpretation through Apple image/color APIs and libsixel's later working-space conversions are separate steps. A float frame is not a guarantee that every source format was decoded at its original precision.

`orientation=0|1` (`O`, enabled by default) controls application of source orientation. Its environment fallback is `SIXEL_LOADER_COREGRAPHICS_ORIENTATION`, then shared orientation policy. A rotation is applied before the shared crop/resize stages, so it can change which source coordinate a crop addresses.

The adapter recognizes framework animation dictionaries for GIF, APNG, WebP, and HEICS where the host exposes them. It reads delays and loop properties, prefers unclamped delays, and uses one pass when auto looping has no loop metadata. Other multi-image containers are treated as still-image sources. Static HEIC support is not evidence of HEICS animation support. The [animation guide](../functionality/animation.md) owns `-S`, `-T`, and looping details.

## Frame-cache budget

`cache_max_bytes=BYTES` (`M`) limits the adapter's decoded-frame cache; its default is 64 MiB and zero disables it. The environment fallback is `SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES`. The cache reuses finalized frame storage across loops and stamps per-emission frame/loop metadata separately. This avoids repeated decoding when the animation fits the cache budget.

The setting is not a total-process memory limit. ImageIO state, current frame buffers, palette work, and encoder queues remain additional allocations. Disabling the cache can trade memory for repeated decoding; it does not disable animation. For reproducible timing, record both cache policy and whether a measurement includes the initial decode or subsequent loops.

## Implementation and validation

[`loader-coregraphics.c`](../../src/loader-coregraphics.c) owns source properties, indexed and float routes, orientation, the frame cache, and animation emission. The [CoreGraphics suite](../../tests/loader/coregraphics/) checks representations, start-frame selection, alpha, orientation, metadata, caches, and failures under the available Apple frameworks. Host-specific format/provider checks may skip. Exact metadata tests, output comparisons, and quality thresholds answer different questions; none establishes universal support across macOS releases.

For document previews rather than ordinary ImageIO decoding, see [Quick Look](quicklook.md).

## PNG source precision

The CoreGraphics loader uses ImageIO to decode images and select their source color space. For supported static RGB/RGBA PNG inputs, it reads original straight integer samples from the resulting image provider and converts RGB directly to canonical linear sRGB float32. Alpha is independent coverage and never enters the color transform. See [color definitions and canonical coordinates](../concepts/colorspace.md) and [storage precision](../concepts/pixelformat-precision.md).

## Source and format boundaries

| Input or condition | Path and contract |
| --- | --- |
| Static nonindexed RGB/RGBA PNG, original eight- or sixteen-bit samples, supported straight RGB provider | Original samples are normalized once to float32, then passed to the explicit native linear transform. Negative and above-one canonical RGB remain available. |
| Eight-bit opaque static RGB PNG, adopted named sRGB, no background, no float preference | The existing native byte path remains available. Ordinary source bytes remain the compatibility contract. |
| The same ordinary source with the float preference | The loader request passes through the manager to the CoreGraphics component and selects canonical float storage. |
| Other formats, grayscale PNG, or PNG carrying `acTL` | These remain outside the static RGB/RGBA PNG precision contract, even when ImageIO returns only one frame. Images returned as indexed RGB by ImageIO follow the separate palette contract below. |
| A targeted source with an unsupported provider layout, premultiplied samples, a decode array, missing required profile, or failed native conversion | The component returns failure before emitting a frame. It does not retry its older byte path. The manager's existing selection of another loader is a separate mechanism. |

The provider check validates RGB color model, component depth/count, byte order, alpha position, row stride, data length and overflow before reading samples. Recognized straight first/last alpha and explicit skip channels preserve their positions; premultiplied data cannot guarantee hidden RGB recovery. This is a boundary for decoded provider layouts, not a promise that every OS release supplies the same layout for every PNG.

Missing embedded ICC does not identify an sRGB source: ImageIO can adopt other color metadata. A source is treated as native named sRGB only when its copied name and equality with the native named space both match. That path uses the named sRGB definition. Other RGB sources use the ICC bytes of the color space adopted by ImageIO. A serialized native sRGB profile may contain sampled curves that differ from the named definition; the two are distinct numerical references.

## Indexed palette color management

An indexed image's color table contains coordinates in its **base color space**. Copying those bytes into a palette tagged as sRGB changes the meaning of Display P3 and other non-sRGB palettes. For images ImageIO returns with an indexed RGB color space, the adapter converts each color-table entry once through the explicit native ColorSync transform, then expands indices into canonical linear sRGB float32 pixels. Extended negative and above-one components are retained. The frame has no byte palette after conversion.

| Indexed condition | Representation and interpretation |
| --- | --- |
| Native named sRGB base, no float preference, eligible palette request | Original PAL8 indices and RGB palette bytes; no CMS apply or full-image float allocation. |
| Non-sRGB RGB base | ICC from the **base** color space supplies the source definition; canonical float output is required even with an 8bit preference. |
| Native named sRGB base with a float preference | Native named sRGB to canonical linear float conversion. |
| Managed palette with palette output disabled or fewer requested colors than source entries | The same canonical float output; palette requests do not disable source color interpretation. |
| One-, two-, four-, or eight-bit indices | Provider row stride is respected and packed samples are unpacked before palette expansion. |
| Non-RGB base model or partial palette alpha | Native generic drawing handles these inputs; the new indexed RGB precision guarantee does not cover them. |

The global CMS engine setting, including `none` and `builtin`, does not override this native conversion. Missing required base-profile data or failed conversion returns failure; converted samples are not retried as unmanaged palette bytes. Expansion validates every index before publishing the frame. Allocation failure likewise emits no partial frame.

For binary PNG palette transparency retained in an indexed provider, source-zero coverage stays independent of RGB: hidden colors remain available without a background and under `keep`/`clear`; `composite` uses the declared linear or gamma background. ImageIO may expand indexed inputs into RGB/RGBA itself; this was observed for both transparent and Adam7 PNGs on the validation host. Such expanded providers use the existing generic path and are outside this direct palette contract.

The indexed fixtures use independently defined `png-samples.h` palette entries and the existing P3 ICC matrix/TRC reference, with a five-pixel row to expose packed-row mistakes. Their generator is [generate_cg_indexed_fixtures.py](../../tools/generate_cg_indexed_fixtures.py). Native color values use the same absolute `1e-4` profile budget as the RGB PNG tests; palette indices, byte values, frame tags and masks use exact assertions. These value tests are distinct from the existing MS-SSIM quality regressions.

## Native conversion and failure

The internal conversion entry explicitly creates both source and linear destination profiles with ColorSync. It does not read or change the requested global CMS engine. Source and destination creation cannot fall back to builtin CMS. Native transform creation retains the existing rendering-intent order and native tag attempts; once a transform exists, application is attempted once. A creation or application failure, nonfinite result, or allocation failure leaves the original input buffer unchanged.

This preserves CoreGraphics' automatic native color interpretation, including when the global engine preference is `none`. The global preference has never disabled ImageIO/CoreGraphics color management on this backend. It does not become an unmanaged control for these tests. Generic CMS factories and other loaders retain their existing fallback policies.

The original provider remains alive until color conversion and alpha composition finish. New RGB and mask buffers remain private until the complete result succeeds. The resulting frame owns its pixels, declares linear float32 coordinates, and carries exact source-zero coverage where required.

## Alpha and background

Without a resolved external background, RGB stays straight, including hidden RGB at alpha zero. With a background, only RGB is transformed and the original integer alpha ratio is used for linear-light composition. `keep` and `clear` retain hidden RGB and the zero mask even with a background; `composite` fills alpha-zero pixels only when a background exists.

External background bytes use their declared gamma or linear encoding independently of the source profile. The existing request handling forces an OSC 11 reply to gamma interpretation. CoreGraphics does not select PNG file `bKGD` colors on this path: its background pointer comes from the caller, environment, or terminal query. Adding file-background selection and source-profile interpretation is a separate migration step. See [background policy](background-policy.md) and [alpha policy](alpha-policy.md).

## Why a float destination is insufficient

A local native control showed that drawing eight-bit sources into a float bitmap could use coarser intermediate precision, and that premultiplied drawing discarded hidden RGB. Equivalent sixteen-bit or float source views avoided the observed eight-bit grid. These are observations of the tested native path, not universal OS guarantees. Reading original samples and separating alpha removes dependence on that drawing intermediate.

The former fallback from extended linear sRGB to bounded linear sRGB could clip wide colors. Its further fallback to gamma sRGB could also mismatch the frame's linear tag. The new source path uses the explicit linear CMS destination and returns failure when it cannot be created.

## Numerical verification

Display-P3 fixture expectations come from original PNG integers and stored ICC values, evaluated independently with high-precision arithmetic. Verified native linear results use the existing absolute `1e-4` budget for this profile and explicit transform path; named sRGB has an independent analytic reference. That budget does not establish accuracy for arbitrary ICC profiles or every native implementation.

Background tests first validate the uncomposed native result against that independent reference. They then compare composition against the same validated native result and independent original-alpha/background arithmetic at `5e-7`, so native profile error cannot hide alpha quantization. Source-zero masks, format tags, dimensions, ordinary byte outputs and failure-buffer identity use exact assertions.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| CG-01 | P3 8 rgb original samples with float preference 0. | [tests/loader/coregraphics/0098_loader_coregraphics_p3_8_rgb_prefer_0.t](../../tests/loader/coregraphics/0098_loader_coregraphics_p3_8_rgb_prefer_0.t) |
| CG-02 | P3 8 rgb original samples with float preference 1. | [tests/loader/coregraphics/0099_loader_coregraphics_p3_8_rgb_prefer_1.t](../../tests/loader/coregraphics/0099_loader_coregraphics_p3_8_rgb_prefer_1.t) |
| CG-03 | P3 8 rgba original samples with float preference 0. | [tests/loader/coregraphics/0100_loader_coregraphics_p3_8_rgba_prefer_0.t](../../tests/loader/coregraphics/0100_loader_coregraphics_p3_8_rgba_prefer_0.t) |
| CG-04 | P3 8 rgba original samples with float preference 1. | [tests/loader/coregraphics/0101_loader_coregraphics_p3_8_rgba_prefer_1.t](../../tests/loader/coregraphics/0101_loader_coregraphics_p3_8_rgba_prefer_1.t) |
| CG-05 | P3 16 rgb original samples with float preference 0. | [tests/loader/coregraphics/0102_loader_coregraphics_p3_16_rgb_prefer_0.t](../../tests/loader/coregraphics/0102_loader_coregraphics_p3_16_rgb_prefer_0.t) |
| CG-06 | P3 16 rgb original samples with float preference 1. | [tests/loader/coregraphics/0103_loader_coregraphics_p3_16_rgb_prefer_1.t](../../tests/loader/coregraphics/0103_loader_coregraphics_p3_16_rgb_prefer_1.t) |
| CG-07 | P3 16 rgba original samples with float preference 0. | [tests/loader/coregraphics/0104_loader_coregraphics_p3_16_rgba_prefer_0.t](../../tests/loader/coregraphics/0104_loader_coregraphics_p3_16_rgba_prefer_0.t) |
| CG-08 | P3 16 rgba original samples with float preference 1. | [tests/loader/coregraphics/0105_loader_coregraphics_p3_16_rgba_prefer_1.t](../../tests/loader/coregraphics/0105_loader_coregraphics_p3_16_rgba_prefer_1.t) |
| CG-09 | Raw 8 rgb original samples with float preference 0. | [tests/loader/coregraphics/0106_loader_coregraphics_raw_8_rgb_prefer_0.t](../../tests/loader/coregraphics/0106_loader_coregraphics_raw_8_rgb_prefer_0.t) |
| CG-10 | Raw 8 rgb original samples with float preference 1. | [tests/loader/coregraphics/0107_loader_coregraphics_raw_8_rgb_prefer_1.t](../../tests/loader/coregraphics/0107_loader_coregraphics_raw_8_rgb_prefer_1.t) |
| CG-11 | Raw 8 rgba original samples with float preference 0. | [tests/loader/coregraphics/0108_loader_coregraphics_raw_8_rgba_prefer_0.t](../../tests/loader/coregraphics/0108_loader_coregraphics_raw_8_rgba_prefer_0.t) |
| CG-12 | Raw 8 rgba original samples with float preference 1. | [tests/loader/coregraphics/0109_loader_coregraphics_raw_8_rgba_prefer_1.t](../../tests/loader/coregraphics/0109_loader_coregraphics_raw_8_rgba_prefer_1.t) |
| CG-13 | Raw 16 rgb original samples with float preference 0. | [tests/loader/coregraphics/0110_loader_coregraphics_raw_16_rgb_prefer_0.t](../../tests/loader/coregraphics/0110_loader_coregraphics_raw_16_rgb_prefer_0.t) |
| CG-14 | Raw 16 rgb original samples with float preference 1. | [tests/loader/coregraphics/0111_loader_coregraphics_raw_16_rgb_prefer_1.t](../../tests/loader/coregraphics/0111_loader_coregraphics_raw_16_rgb_prefer_1.t) |
| CG-15 | Raw 16 rgba original samples with float preference 0. | [tests/loader/coregraphics/0112_loader_coregraphics_raw_16_rgba_prefer_0.t](../../tests/loader/coregraphics/0112_loader_coregraphics_raw_16_rgba_prefer_0.t) |
| CG-16 | Raw 16 rgba original samples with float preference 1. | [tests/loader/coregraphics/0113_loader_coregraphics_raw_16_rgba_prefer_1.t](../../tests/loader/coregraphics/0113_loader_coregraphics_raw_16_rgba_prefer_1.t) |
| CG-17 | P3 8 original alpha over gamma background with composite. | [tests/loader/coregraphics/0114_loader_coregraphics_p3_8_gamma_background_composite.t](../../tests/loader/coregraphics/0114_loader_coregraphics_p3_8_gamma_background_composite.t) |
| CG-18 | P3 8 original alpha over gamma background with clear. | [tests/loader/coregraphics/0115_loader_coregraphics_p3_8_gamma_background_clear.t](../../tests/loader/coregraphics/0115_loader_coregraphics_p3_8_gamma_background_clear.t) |
| CG-19 | P3 8 original alpha over gamma background with keep. | [tests/loader/coregraphics/0116_loader_coregraphics_p3_8_gamma_background_keep.t](../../tests/loader/coregraphics/0116_loader_coregraphics_p3_8_gamma_background_keep.t) |
| CG-20 | P3 8 original alpha over linear background with composite. | [tests/loader/coregraphics/0117_loader_coregraphics_p3_8_linear_background_composite.t](../../tests/loader/coregraphics/0117_loader_coregraphics_p3_8_linear_background_composite.t) |
| CG-21 | P3 8 original alpha over linear background with clear. | [tests/loader/coregraphics/0118_loader_coregraphics_p3_8_linear_background_clear.t](../../tests/loader/coregraphics/0118_loader_coregraphics_p3_8_linear_background_clear.t) |
| CG-22 | P3 8 original alpha over linear background with keep. | [tests/loader/coregraphics/0119_loader_coregraphics_p3_8_linear_background_keep.t](../../tests/loader/coregraphics/0119_loader_coregraphics_p3_8_linear_background_keep.t) |
| CG-23 | P3 16 original alpha over gamma background with composite. | [tests/loader/coregraphics/0120_loader_coregraphics_p3_16_gamma_background_composite.t](../../tests/loader/coregraphics/0120_loader_coregraphics_p3_16_gamma_background_composite.t) |
| CG-24 | P3 16 original alpha over gamma background with clear. | [tests/loader/coregraphics/0121_loader_coregraphics_p3_16_gamma_background_clear.t](../../tests/loader/coregraphics/0121_loader_coregraphics_p3_16_gamma_background_clear.t) |
| CG-25 | P3 16 original alpha over gamma background with keep. | [tests/loader/coregraphics/0122_loader_coregraphics_p3_16_gamma_background_keep.t](../../tests/loader/coregraphics/0122_loader_coregraphics_p3_16_gamma_background_keep.t) |
| CG-26 | P3 16 original alpha over linear background with composite. | [tests/loader/coregraphics/0123_loader_coregraphics_p3_16_linear_background_composite.t](../../tests/loader/coregraphics/0123_loader_coregraphics_p3_16_linear_background_composite.t) |
| CG-27 | P3 16 original alpha over linear background with clear. | [tests/loader/coregraphics/0124_loader_coregraphics_p3_16_linear_background_clear.t](../../tests/loader/coregraphics/0124_loader_coregraphics_p3_16_linear_background_clear.t) |
| CG-28 | P3 16 original alpha over linear background with keep. | [tests/loader/coregraphics/0125_loader_coregraphics_p3_16_linear_background_keep.t](../../tests/loader/coregraphics/0125_loader_coregraphics_p3_16_linear_background_keep.t) |
| CG-29 | Raw 8 original alpha over gamma background with composite. | [tests/loader/coregraphics/0126_loader_coregraphics_raw_8_gamma_background_composite.t](../../tests/loader/coregraphics/0126_loader_coregraphics_raw_8_gamma_background_composite.t) |
| CG-30 | Raw 8 original alpha over gamma background with clear. | [tests/loader/coregraphics/0127_loader_coregraphics_raw_8_gamma_background_clear.t](../../tests/loader/coregraphics/0127_loader_coregraphics_raw_8_gamma_background_clear.t) |
| CG-31 | Raw 8 original alpha over gamma background with keep. | [tests/loader/coregraphics/0128_loader_coregraphics_raw_8_gamma_background_keep.t](../../tests/loader/coregraphics/0128_loader_coregraphics_raw_8_gamma_background_keep.t) |
| CG-32 | Raw 8 original alpha over linear background with composite. | [tests/loader/coregraphics/0129_loader_coregraphics_raw_8_linear_background_composite.t](../../tests/loader/coregraphics/0129_loader_coregraphics_raw_8_linear_background_composite.t) |
| CG-33 | Raw 8 original alpha over linear background with clear. | [tests/loader/coregraphics/0130_loader_coregraphics_raw_8_linear_background_clear.t](../../tests/loader/coregraphics/0130_loader_coregraphics_raw_8_linear_background_clear.t) |
| CG-34 | Raw 8 original alpha over linear background with keep. | [tests/loader/coregraphics/0131_loader_coregraphics_raw_8_linear_background_keep.t](../../tests/loader/coregraphics/0131_loader_coregraphics_raw_8_linear_background_keep.t) |
| CG-35 | Raw 16 original alpha over gamma background with composite. | [tests/loader/coregraphics/0132_loader_coregraphics_raw_16_gamma_background_composite.t](../../tests/loader/coregraphics/0132_loader_coregraphics_raw_16_gamma_background_composite.t) |
| CG-36 | Raw 16 original alpha over gamma background with clear. | [tests/loader/coregraphics/0133_loader_coregraphics_raw_16_gamma_background_clear.t](../../tests/loader/coregraphics/0133_loader_coregraphics_raw_16_gamma_background_clear.t) |
| CG-37 | Raw 16 original alpha over gamma background with keep. | [tests/loader/coregraphics/0134_loader_coregraphics_raw_16_gamma_background_keep.t](../../tests/loader/coregraphics/0134_loader_coregraphics_raw_16_gamma_background_keep.t) |
| CG-38 | Raw 16 original alpha over linear background with composite. | [tests/loader/coregraphics/0135_loader_coregraphics_raw_16_linear_background_composite.t](../../tests/loader/coregraphics/0135_loader_coregraphics_raw_16_linear_background_composite.t) |
| CG-39 | Raw 16 original alpha over linear background with clear. | [tests/loader/coregraphics/0136_loader_coregraphics_raw_16_linear_background_clear.t](../../tests/loader/coregraphics/0136_loader_coregraphics_raw_16_linear_background_clear.t) |
| CG-40 | Raw 16 original alpha over linear background with keep. | [tests/loader/coregraphics/0137_loader_coregraphics_raw_16_linear_background_keep.t](../../tests/loader/coregraphics/0137_loader_coregraphics_raw_16_linear_background_keep.t) |
| CG-41 | P3 native float source with global engine none. | [tests/loader/coregraphics/0138_loader_coregraphics_p3_8_float_engine_none.t](../../tests/loader/coregraphics/0138_loader_coregraphics_p3_8_float_engine_none.t) |
| CG-42 | Raw native float source with global engine none. | [tests/loader/coregraphics/0139_loader_coregraphics_raw_8_float_engine_none.t](../../tests/loader/coregraphics/0139_loader_coregraphics_raw_8_float_engine_none.t) |
| CG-43 | P3 native float source with global engine auto. | [tests/loader/coregraphics/0140_loader_coregraphics_p3_8_float_engine_auto.t](../../tests/loader/coregraphics/0140_loader_coregraphics_p3_8_float_engine_auto.t) |
| CG-44 | Raw native float source with global engine auto. | [tests/loader/coregraphics/0141_loader_coregraphics_raw_8_float_engine_auto.t](../../tests/loader/coregraphics/0141_loader_coregraphics_raw_8_float_engine_auto.t) |
| CG-45 | P3 native float source with global engine builtin. | [tests/loader/coregraphics/0142_loader_coregraphics_p3_8_float_engine_builtin.t](../../tests/loader/coregraphics/0142_loader_coregraphics_p3_8_float_engine_builtin.t) |
| CG-46 | Raw native float source with global engine builtin. | [tests/loader/coregraphics/0143_loader_coregraphics_raw_8_float_engine_builtin.t](../../tests/loader/coregraphics/0143_loader_coregraphics_raw_8_float_engine_builtin.t) |
| CG-47 | Explicit native ICC transform with global engine none. | [tests/loader/coregraphics/0144_loader_coregraphics_strict_icc_engine_none.t](../../tests/loader/coregraphics/0144_loader_coregraphics_strict_icc_engine_none.t) |
| CG-48 | Explicit native ICC transform with global engine auto. | [tests/loader/coregraphics/0145_loader_coregraphics_strict_icc_engine_auto.t](../../tests/loader/coregraphics/0145_loader_coregraphics_strict_icc_engine_auto.t) |
| CG-49 | Explicit native ICC transform with global engine builtin. | [tests/loader/coregraphics/0146_loader_coregraphics_strict_icc_engine_builtin.t](../../tests/loader/coregraphics/0146_loader_coregraphics_strict_icc_engine_builtin.t) |
| CG-50 | Explicit native ICC transform with global engine colorsync. | [tests/loader/coregraphics/0147_loader_coregraphics_strict_icc_engine_colorsync.t](../../tests/loader/coregraphics/0147_loader_coregraphics_strict_icc_engine_colorsync.t) |
| CG-51 | Explicit native named transform with global engine none. | [tests/loader/coregraphics/0148_loader_coregraphics_strict_named_engine_none.t](../../tests/loader/coregraphics/0148_loader_coregraphics_strict_named_engine_none.t) |
| CG-52 | Explicit native named transform with global engine auto. | [tests/loader/coregraphics/0149_loader_coregraphics_strict_named_engine_auto.t](../../tests/loader/coregraphics/0149_loader_coregraphics_strict_named_engine_auto.t) |
| CG-53 | Explicit native named transform with global engine builtin. | [tests/loader/coregraphics/0150_loader_coregraphics_strict_named_engine_builtin.t](../../tests/loader/coregraphics/0150_loader_coregraphics_strict_named_engine_builtin.t) |
| CG-54 | Explicit native named transform with global engine colorsync. | [tests/loader/coregraphics/0151_loader_coregraphics_strict_named_engine_colorsync.t](../../tests/loader/coregraphics/0151_loader_coregraphics_strict_named_engine_colorsync.t) |
| CG-55 | Explicit native ICC rejects NaN atomically. | [tests/loader/coregraphics/0152_loader_coregraphics_strict_icc_nan_atomic.t](../../tests/loader/coregraphics/0152_loader_coregraphics_strict_icc_nan_atomic.t) |
| CG-56 | Explicit native ICC rejects infinity atomically. | [tests/loader/coregraphics/0153_loader_coregraphics_strict_icc_infinity_atomic.t](../../tests/loader/coregraphics/0153_loader_coregraphics_strict_icc_infinity_atomic.t) |
| CG-57 | Explicit native ICC rejects dimensions atomically. | [tests/loader/coregraphics/0154_loader_coregraphics_strict_icc_dimensions_atomic.t](../../tests/loader/coregraphics/0154_loader_coregraphics_strict_icc_dimensions_atomic.t) |
| CG-58 | Explicit native ICC rejects profile arguments atomically. | [tests/loader/coregraphics/0155_loader_coregraphics_strict_icc_profile_arguments_atomic.t](../../tests/loader/coregraphics/0155_loader_coregraphics_strict_icc_profile_arguments_atomic.t) |
| CG-59 | Explicit native named rejects NaN atomically. | [tests/loader/coregraphics/0156_loader_coregraphics_strict_named_nan_atomic.t](../../tests/loader/coregraphics/0156_loader_coregraphics_strict_named_nan_atomic.t) |
| CG-60 | Explicit native named rejects infinity atomically. | [tests/loader/coregraphics/0157_loader_coregraphics_strict_named_infinity_atomic.t](../../tests/loader/coregraphics/0157_loader_coregraphics_strict_named_infinity_atomic.t) |
| CG-61 | Explicit native named rejects dimensions atomically. | [tests/loader/coregraphics/0158_loader_coregraphics_strict_named_dimensions_atomic.t](../../tests/loader/coregraphics/0158_loader_coregraphics_strict_named_dimensions_atomic.t) |
| CG-62 | Explicit native named rejects profile arguments atomically. | [tests/loader/coregraphics/0159_loader_coregraphics_strict_named_profile_arguments_atomic.t](../../tests/loader/coregraphics/0159_loader_coregraphics_strict_named_profile_arguments_atomic.t) |
| CG-63 | CoreGraphics ignores file bKGD for 8 rgb with no external background. | [tests/loader/coregraphics/0160_loader_coregraphics_bkgd_8_rgb_no_external_background.t](../../tests/loader/coregraphics/0160_loader_coregraphics_bkgd_8_rgb_no_external_background.t) |
| CG-64 | CoreGraphics ignores file bKGD for 8 rgb with gamma external background. | [tests/loader/coregraphics/0161_loader_coregraphics_bkgd_8_rgb_gamma_external_background.t](../../tests/loader/coregraphics/0161_loader_coregraphics_bkgd_8_rgb_gamma_external_background.t) |
| CG-65 | CoreGraphics ignores file bKGD for 8 rgba with no external background. | [tests/loader/coregraphics/0162_loader_coregraphics_bkgd_8_rgba_no_external_background.t](../../tests/loader/coregraphics/0162_loader_coregraphics_bkgd_8_rgba_no_external_background.t) |
| CG-66 | CoreGraphics ignores file bKGD for 8 rgba with gamma external background. | [tests/loader/coregraphics/0163_loader_coregraphics_bkgd_8_rgba_gamma_external_background.t](../../tests/loader/coregraphics/0163_loader_coregraphics_bkgd_8_rgba_gamma_external_background.t) |
| CG-67 | CoreGraphics ignores file bKGD for 8 rgba with linear external background. | [tests/loader/coregraphics/0164_loader_coregraphics_bkgd_8_rgba_linear_external_background.t](../../tests/loader/coregraphics/0164_loader_coregraphics_bkgd_8_rgba_linear_external_background.t) |
| CG-68 | CoreGraphics ignores file bKGD for 16 rgb with no external background. | [tests/loader/coregraphics/0165_loader_coregraphics_bkgd_16_rgb_no_external_background.t](../../tests/loader/coregraphics/0165_loader_coregraphics_bkgd_16_rgb_no_external_background.t) |
| CG-69 | CoreGraphics ignores file bKGD for 16 rgb with gamma external background. | [tests/loader/coregraphics/0166_loader_coregraphics_bkgd_16_rgb_gamma_external_background.t](../../tests/loader/coregraphics/0166_loader_coregraphics_bkgd_16_rgb_gamma_external_background.t) |
| CG-70 | CoreGraphics ignores file bKGD for 16 rgba with no external background. | [tests/loader/coregraphics/0167_loader_coregraphics_bkgd_16_rgba_no_external_background.t](../../tests/loader/coregraphics/0167_loader_coregraphics_bkgd_16_rgba_no_external_background.t) |
| CG-71 | CoreGraphics ignores file bKGD for 16 rgba with gamma external background. | [tests/loader/coregraphics/0168_loader_coregraphics_bkgd_16_rgba_gamma_external_background.t](../../tests/loader/coregraphics/0168_loader_coregraphics_bkgd_16_rgba_gamma_external_background.t) |
| CG-72 | CoreGraphics ignores file bKGD for 16 rgba with linear external background. | [tests/loader/coregraphics/0169_loader_coregraphics_bkgd_16_rgba_linear_external_background.t](../../tests/loader/coregraphics/0169_loader_coregraphics_bkgd_16_rgba_linear_external_background.t) |
| CGI-01 | P3 eight-bit indices expand into extended canonical float values. | [tests/loader/coregraphics/indexed/0001_p3_8.t](../../tests/loader/coregraphics/indexed/0001_p3_8.t) |
| CGI-02 | P3 one-bit source indices preserve the pixel map across odd-width rows. | [tests/loader/coregraphics/indexed/0002_p3_1.t](../../tests/loader/coregraphics/indexed/0002_p3_1.t) |
| CGI-03 | P3 two-bit source indices preserve the pixel map across odd-width rows. | [tests/loader/coregraphics/indexed/0003_p3_2.t](../../tests/loader/coregraphics/indexed/0003_p3_2.t) |
| CGI-04 | P3 four-bit source indices preserve the pixel map across odd-width rows. | [tests/loader/coregraphics/indexed/0004_p3_4.t](../../tests/loader/coregraphics/indexed/0004_p3_4.t) |
| CGI-05 | Disabling palette output still applies the P3 base profile. | [tests/loader/coregraphics/indexed/0005_p3_no_palette.t](../../tests/loader/coregraphics/indexed/0005_p3_no_palette.t) |
| CGI-06 | A request for two colors does not bypass the eight-entry P3 source palette transform. | [tests/loader/coregraphics/indexed/0006_p3_reqcolors.t](../../tests/loader/coregraphics/indexed/0006_p3_reqcolors.t) |
| CGI-07 | Named sRGB retains exact PAL8 indices/bytes without CMS apply or image-sized float allocation. | [tests/loader/coregraphics/indexed/0007_srgb_byte.t](../../tests/loader/coregraphics/indexed/0007_srgb_byte.t) |
| CGI-08 | A float preference converts named sRGB palette entries to canonical linear coordinates. | [tests/loader/coregraphics/indexed/0008_srgb_float.t](../../tests/loader/coregraphics/indexed/0008_srgb_float.t) |
| CGI-09 | Global engine none preserves native P3 palette conversion. | [tests/loader/coregraphics/indexed/0009_p3_engine_none.t](../../tests/loader/coregraphics/indexed/0009_p3_engine_none.t) |
| CGI-10 | Global engine builtin preserves native P3 palette conversion. | [tests/loader/coregraphics/indexed/0010_p3_engine_builtin.t](../../tests/loader/coregraphics/indexed/0010_p3_engine_builtin.t) |

### Defensive and malformed-input tests

The explicit native helper rejects nonfinite inputs, invalid dimensions and inconsistent profile arguments atomically. Native stage failures and unsupported provider layouts also have focused external fault-injection controls; these supplement the permanent numerical and rejection tests without implying native API conformance on every platform.

The [indexed tests](../../tests/loader/coregraphics/indexed/) also exercise expansion-allocation failure, callback suppression, allocator balance and reuse of the same loader after failure.
