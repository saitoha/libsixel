# libtiff image loader

The `libtiff` adapter reads a TIFF image through libtiff, with a precision-preserving scanline path for selected layouts and an RGBA convenience path for other supported combinations. TIFF is a family of sample organizations and codecs: successfully reading one TIFF does not establish support for all compression, photometric, alpha, or planar combinations.

```sh
img2sixel -L 'libtiff!' image.tif
img2sixel -L 'libtiff:cms_engine=builtin!' image.tif
```

`!` fixes the backend for inspection. Compression availability depends on the linked libtiff build. This adapter processes the current image directory and does not enumerate pages into an animation; `-T` is not a TIFF page selector here.

## Precision and fallback inside the adapter

| Route | Scope |
| --- | --- |
| High-precision scanlines | Selected contiguous RGB/grayscale 8/16-bit integer or float32 layouts, and selected 16-bit/float32 CIELAB layouts, without extra alpha samples. Supported orientation and scanline-layout checks also apply. |
| `TIFFReadRGBAImageOriented` | The general libtiff conversion fallback for combinations outside the direct path. Its RGBA result is an 8-bit representation; later promotion to float32 does not recover discarded source precision. |
| Loader-manager fallback | A different backend may be tried only according to the shared failure/callback rules. This is separate from choosing libtiff's internal RGBA route. |

The direct CIELAB path keeps typed Lab as an internal source representation until CMS is attempted. Successful CMS conversion publishes `LINEARRGBFLOAT32` / `LINEAR`; when ICC conversion is inapplicable or fails, the existing Lab-to-gamma-RGB fallback publishes `RGBFLOAT32` / `GAMMA` if that fallback succeeds. The emitted frame does not retain the internal Lab representation. RGB/gray paths and profile conversion have their own normalization. Inspect emitted `pixelformat` and `colorspace`; a `.tif` extension is not sufficient to infer either.

TIFF orientation is handled in the decode paths. The direct path has narrower layout/orientation eligibility than the convenience reader, so changing tags can change both geometry handling and precision route. This backend does not expose the JPEG-style `orientation` suboption in the current registry.

## Color and alpha

`cms_engine=...` (`E`) selects the TIFF CMS engine; the environment fallback is `SIXEL_LOADER_LIBTIFF_CMS_ENGINE`, then shared CMS policy. Embedded ICC use depends on photometric interpretation and source/profile compatibility. The adapter contains separate handling for grayscale, RGB-like RGBA data, and high-precision/Lab paths. Do not assume all TIFF photometric models share an identical transform.

Alpha-bearing layouts commonly take the RGBA path. Alpha normalization and explicit background processing still determine the final frame representation; request [alpha/background policy](alpha-policy.md) explicitly when comparing decoders. TIFF reader support for a compressed image does not imply support for preserving its original alpha association or sample depth end to end.

The `libtiff` loader reads TIFF through the linked libtiff library. Its adapter implements selected high-precision scanline paths and uses libtiff's RGBA8 decoder for other accepted inputs. Successful TIFF decoding does not establish that an embedded ICC profile was applied or that the original precision and gamut survived.

This page records the adapter's implemented and missing paths. The detailed numerical and failure contracts remain in [Loader Color Management](color-management.md). Loader selection and fallback between components are described in [Image loader architecture](README.md).

## Format and color-management support

“Implemented” below identifies an adapter path, subject to its stated conditions and the selected CMS engine's profile support. “Not implemented” identifies a missing adapter path even when libtiff can decode the image. “Unverified” means that the table does not establish acceptance or numerical correctness for that condition. These statuses distinguish an implementation omission from an unavailable optional library or an explicitly disabled CMS.

The **high-precision layout** is contiguous strips, orientation 1–4, no extra samples, and exactly three RGB/Lab samples or one grayscale sample per pixel. Compression support is supplied by the linked libtiff codec. `SAMPLEFORMAT_VOID` retains the adapter's unsigned-integer interpretation. The LogLuv row has its own codec and sample conditions.

| Source samples / photometric interpretation | Decode path | Embedded ICC integration | Precision and implementation boundary |
| --- | --- | --- | --- |
| RGB unsigned 16-bit, high-precision layout | Implemented: original words normalized into RGB float32 | Implemented: applicable RGB/Gray profile through the shared direct-linear helper | A complete finite CMS result retains extended linear RGB. No applicable conversion leaves normalized RGB float32. See [RGB16](color-management.md#tiff-unsigned-rgb16-input). |
| MINISBLACK / MINISWHITE unsigned 16-bit, high-precision layout | Implemented: original word divided by 65535; MINISWHITE uses `1-x` | Implemented: shared direct-linear helper | The original normalized, polarity-adjusted values survive failed conversion. See [Gray16](color-management.md#tiff-gray16-samples). |
| RGB / MINISBLACK / MINISWHITE IEEE float32, high-precision layout | Implemented: finite source values retained; MINISWHITE uses `1-x` | Implemented: shared direct-linear helper | Negative and above-one input survives the adapter boundary; the CMS engine still owns the profile's device-domain rules. NaN/Inf input is rejected. See [floating-point samples](color-management.md#tiff-ieee-floating-point-samples). |
| CIELAB unsigned 16-bit, high-precision layout | Implemented: unsigned L and signed a/b words decoded into the internal Lab representation | Implemented: applicable Lab profile through the shared direct-linear helper | The signed-word convention and fallback are defined by [CIELAB16](color-management.md#tiff-cielab16-signed-input-contract) and [Lab publication](color-management.md#tiff-lab-direct-linear-publication-and-recovery). |
| CIELAB IEEE float32, high-precision layout | Implemented: existing Lab sample normalization | Implemented: applicable Lab profile through the shared direct-linear helper | This retains the existing input normalization convention; it does not promise arbitrary extended Lab input. See [Lab publication](color-management.md#tiff-lab-direct-linear-publication-and-recovery). |
| LogLuv with SGILOG / SGILOG24, contiguous strips, three channels, no extras, orientation 1–4 | Implemented: libtiff decoded XYZ float32 converted to canonical linear RGB | Not applied by design: this path interprets native XYZ, not device samples through ICC | Stored-coordinate scale and finite extended RGB are retained. See [LogLuv policy](color-management.md#logluv-stored-coordinate-and-exposure-policy). |
| RGB / MINISBLACK / MINISWHITE UINT8, high-precision layout | Implemented from original scanlines | Implemented with a matching RGB/Gray profile through the shared direct-linear helper | Successful interpretation retains extended linear float values before final target negotiation. Unmanaged or failed interpretation restores exact RGB888/GAMMA source bytes. |
| RGB / MINISBLACK / MINISWHITE on the RGBA8 path, including accepted low-bit inputs and byte layouts outside the direct path | Delegated: requires successful libtiff RGBA8 decoding | Implemented after RGBA8 decoding, subject to profile compatibility | Original high-depth values and extended range cannot be restored after this byte boundary. This is CMS-connected, but not a precision-preserving path. |
| Palette TIFF on the RGBA8 path | Delegated: libtiff expands the palette to RGBA8 | Implemented for an applicable RGB profile after expansion | The palette and source precision are no longer retained as the original indexed representation. |
| YCbCr TIFF on the RGBA8 path | Delegated: libtiff converts components to RGBA8 | Implemented for an applicable RGB profile after component conversion | This does not provide an original-YCbCr-domain ICC input path or recover source precision lost during decoding. |
| **CIELAB 8-bit**, or CIELAB outside the high-precision layout | Delegated: requires successful libtiff RGBA8 decoding | **Not implemented:** the byte-path photometric gates skip the embedded Lab ICC | A readable image can be returned without applying its embedded profile. Correct integration needs original Lab samples before RGB conversion. |
| **SEPARATED / CMYK** | Delegated: requires successful libtiff RGBA8 decoding | **Not implemented:** no original-CMYK CMS input path in this adapter; byte-path gates skip its ICC | libtiff's RGB conversion can produce a readable image, but the embedded CMYK profile is not applied. Applying that profile to the already converted RGB bytes would not repair the omission. |
| ICCLAB / ITULAB / LOGL | No high-precision adapter path; libtiff fallback acceptance is unverified here | No embedded-ICC connection in the byte helper | Accepted layouts, original-domain/profile compatibility and numerical output still need verification. These entries are not claims of complete format support. |
| Other sample depths, sample formats, planar/tiled layouts, extra samples, or orientation 5–8 | No matching high-precision path; the adapter attempts RGBA8 fallback where eligible | Depends on the photometric rows above | Do not infer high-precision support from successful decoding. libtiff may reject an unsupported combination. |

A failed high-precision operation is distinct from an unsupported combination: after a supported decode has begun, errors can terminate the load instead of trying RGBA8 again. In particular, native LogLuv checks layout eligibility before requesting its floating-point codec interface and does not retry RGBA8 after that state change.

## Missing CMS integration and fallback

CIELAB8 and CMYK are implementation omissions inside a loader that already has CMS integration. They are not loader-wide CMS support and must not be described as fully color-managed TIFF merely because another TIFF path applies a profile. The missing connections are established by the adapter's source branches; this inventory does not claim measured color-error bounds for these unmanaged results.

Both byte-helper build variants omit those photometric cases: `tiff_convert_embedded_icc_to_srgb()` does not select a Lab/CMYK transform, and `tiff_photometric_allows_embedded_icc(..., 0)` rejects them for the other variant. Selecting a different CMS engine cannot add the missing original-sample connection.

For the implemented 16-bit and float32 RGB/Gray paths, successful ICC conversion publishes `LINEARRGBFLOAT32` / `LINEAR`. With no applicable conversion, they publish their original normalized integer or polarity-adjusted floating-point samples as `RGBFLOAT32` / `GAMMA`. Lab has its own format-specific fallback. The fallback `GAMMA` marker does not establish calibrated sRGB primaries for a device-specific source. The shared helper publishes only a complete finite result and preserves original samples on failure; it does not make every embedded profile acceptable.

`cms_target` and `prefer_8bit` are not evidence that a particular TIFF branch performed a transform. Native LogLuv and successfully managed eligible RGB/Gray8 have an explicit final target-negotiation step; the other paths retain their own publication contracts. Alpha handling after RGBA8 decoding likewise cannot recover the samples lost at the byte boundary.

## Original byte samples and final publication

The direct byte route accepts unsigned 8-bit RGB3 or MINISBLACK/MINISWHITE1 in contiguous strips, without extra samples, in orientations 1–4. Existing VOID-as-UINT interpretation remains supported. Even an all-255 alpha extra is outside this route. Palette, YCbCr, low-bit, planar/tiled, orientation 5–8 and alpha paths retain their separate limitations; this contract does not close them.

Each original byte is normalized once into float32. MINISWHITE polarity is applied before its Gray ICC curve. An RGB source requires an RGB profile and a grayscale source requires a Gray profile, consistently with and without external CMMs. A complete, finite ICC result establishes LINEARRGBFLOAT32/LINEAR; the existing frame converter then honors `cms_target` or the deliberate final byte boundary requested by `prefer_8bit`. Negative and above-one managed values survive until an explicitly selected byte conversion.

With CMS `none`, absent, invalid or mismatched ICC, or failed optional CMS work, the complete normalized original source survives. The adapter reconstructs those original bytes exactly and publishes RGB888/GAMMA. It does not publish unmanaged float merely because its decoder used a float workspace, nor does it negotiate a requested target after failed interpretation. This preserves the existing unmanaged downstream precision/lookup selection. The GAMMA marker does not assert calibrated sRGB source primaries. Mandatory decoder allocation/read errors terminate the selected decode without callback or RGBA retry.

A resolved background leaves eligible opaque samples unchanged. Bypassing the RGBA convenience path intentionally removes its former alpha-255 right-shift attenuation, which could darken a component by one byte level even for an opaque input. Alpha-bearing inputs remain outside this guarantee.

The decoder allocates 12 bytes per pixel plus a scanline. CMS retains its separate transactional float result and, for Gray, its packed source. Although unmanaged fallback has a 3-byte-per-pixel payload, the compacted allocation retains its original 12-byte-per-pixel capacity until release. This is not a memory reduction and adds no arbitrary-precision runtime library. The [numeric and final-output distinction](color-management.md#tiff-original-byte-rgb-and-gray-input) also applies: more accurate intermediate samples do not guarantee a visible improvement after palette quantization.

## Implementation and verification references

[`loader-libtiff.c`](../../src/loader-libtiff.c) owns memory-backed TIFF access, direct-path eligibility, the RGBA fallback, ICC processing, and frame handoff. The [libtiff test category](../../tests/loader/libtiff/) contains RGB/indexed/gray/alpha and compression fixtures, high-depth and Lab cases, CMS comparisons, and malformed-input checks. Perceptual tests are quality evidence, not proof that a particular precision path or metadata transform ran. Exact frame/metadata checks and diagnostics are needed for that distinction.

For the common downstream representation and resize promotion rules, see [pixel-format precision](../concepts/pixelformat-precision.md).

The owning adapter is [`src/loader-libtiff.c`](../../src/loader-libtiff.c): `tiff_try_load_high_precision()` selects the native scanline paths, `load_tiff()` selects RGBA8 fallback, and the two byte ICC helpers enforce their photometric gates. The direct-linear conversion and publication helper is in [`src/cms.c`](../../src/cms.c).

The [TIFF test directory](../../tests/loader/libtiff/) contains numerical, metadata, recovery and quality tests. Their documented contracts and reciprocal test links are maintained in [Loader Color Management: test coverage](color-management.md#test-coverage). Independent sample-value tests protect arithmetic and source retention; image-similarity tests cover their stated perceptual floor. Neither the directory's existence nor a CMS engine name proves coverage for an unimplemented or unverified row above.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| CMS-339 | RGB original bytes produce independently bounded canonical values through actual builtin, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0285_byte_rgb_builtin.t](../../tests/loader/libtiff/0285_byte_rgb_builtin.t) |
| CMS-340 | RGB original bytes produce independently bounded canonical values through actual LCMS, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0286_byte_rgb_lcms2.t](../../tests/loader/libtiff/0286_byte_rgb_lcms2.t) |
| CMS-341 | RGB original bytes produce independently bounded canonical values through actual ColorSync, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0287_byte_rgb_colorsync.t](../../tests/loader/libtiff/0287_byte_rgb_colorsync.t) |
| CMS-342 | MINISBLACK original bytes produce independently bounded canonical values through actual builtin, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0288_byte_black_builtin.t](../../tests/loader/libtiff/0288_byte_black_builtin.t) |
| CMS-343 | MINISBLACK original bytes produce independently bounded canonical values through actual LCMS, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0289_byte_black_lcms2.t](../../tests/loader/libtiff/0289_byte_black_lcms2.t) |
| CMS-344 | MINISBLACK original bytes produce independently bounded canonical values through actual ColorSync, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0290_byte_black_colorsync.t](../../tests/loader/libtiff/0290_byte_black_colorsync.t) |
| CMS-345 | MINISWHITE original bytes produce independently bounded canonical values through actual builtin, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0291_byte_white_builtin.t](../../tests/loader/libtiff/0291_byte_white_builtin.t) |
| CMS-346 | MINISWHITE original bytes produce independently bounded canonical values through actual LCMS, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0292_byte_white_lcms2.t](../../tests/loader/libtiff/0292_byte_white_lcms2.t) |
| CMS-347 | MINISWHITE original bytes produce independently bounded canonical values through actual ColorSync, with exact frame metadata and no opaque-background attenuation. | [tests/loader/libtiff/0293_byte_white_colorsync.t](../../tests/loader/libtiff/0293_byte_white_colorsync.t) |
| CMS-348 | RGB retains every original byte across all 256 levels for none/absent/invalid/mismatched ICC, without CMS apply or target negotiation. | [tests/loader/libtiff/0294_byte_rgb_fallback.t](../../tests/loader/libtiff/0294_byte_rgb_fallback.t) |
| CMS-349 | MINISBLACK retains every original byte across all 256 levels for none/absent/invalid/mismatched ICC, without CMS apply or target negotiation. | [tests/loader/libtiff/0295_byte_black_fallback.t](../../tests/loader/libtiff/0295_byte_black_fallback.t) |
| CMS-350 | MINISWHITE retains every original byte across all 256 levels for none/absent/invalid/mismatched ICC, without CMS apply or target negotiation. | [tests/loader/libtiff/0296_byte_white_fallback.t](../../tests/loader/libtiff/0296_byte_white_fallback.t) |
| CMS-351 | Orientations 1–4 map original RGB/Gray samples independently; UINT and VOID select the same direct route. | [tests/loader/libtiff/0297_byte_orientation_void.t](../../tests/loader/libtiff/0297_byte_orientation_void.t) |
| CMS-352 | Managed gamma float publication follows the independently encoded canonical values. | [tests/loader/libtiff/0298_byte_gamma_target.t](../../tests/loader/libtiff/0298_byte_gamma_target.t) |
| CMS-353 | Explicit final byte preference clamps and rounds only after successful canonical ICC interpretation. | [tests/loader/libtiff/0299_byte_final_byte_target.t](../../tests/loader/libtiff/0299_byte_final_byte_target.t) |
| CMS-354 | RGB discards qualified partial/nonfinite CMS output, preserves all caller source bits before compaction, restores exact source bytes, and permits recovery. | [tests/loader/libtiff/0300_byte_rgb_late_failure.t](../../tests/loader/libtiff/0300_byte_rgb_late_failure.t) |
| CMS-355 | MINISBLACK discards qualified partial/nonfinite CMS output, preserves all caller source bits before compaction, restores exact source bytes, and permits recovery. | [tests/loader/libtiff/0301_byte_black_late_failure.t](../../tests/loader/libtiff/0301_byte_black_late_failure.t) |
| CMS-356 | MINISWHITE discards qualified partial/nonfinite CMS output, preserves all caller source bits before compaction, restores exact source bytes, and permits recovery. | [tests/loader/libtiff/0302_byte_white_late_failure.t](../../tests/loader/libtiff/0302_byte_white_late_failure.t) |
| CMS-357 | Mandatory selected byte-image allocation failure has no callback or RGBA retry and releases decoder allocations. | [tests/loader/libtiff/0303_byte_decode_allocation_failure.t](../../tests/loader/libtiff/0303_byte_decode_allocation_failure.t) |
| CMS-358 | A real first scanline precedes the injected read failure; the selected decode has no callback or RGBA retry and releases allocations. | [tests/loader/libtiff/0304_byte_decode_read_failure.t](../../tests/loader/libtiff/0304_byte_decode_read_failure.t) |

### Quality regression and defensive coverage

The existing perceptual tests retain their references and thresholds; their separate inventory is in [Loader Color Management](color-management.md#test-coverage). Malformed and unsupported-layout cases remain in the [libtiff test category](../../tests/loader/libtiff/). The twenty contracts above cover only their stated direct byte route, not all TIFF combinations.
