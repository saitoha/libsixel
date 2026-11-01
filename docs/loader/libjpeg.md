# libjpeg image loader

The `libjpeg` loader delegates JPEG decompression to the linked JPEG library and performs frame normalization, embedded-profile handling, and Exif orientation in libsixel. It is a still-image adapter. Support for a JPEG coding process or sample depth depends on the library and the APIs detected at build time; the loader name alone does not promise every JPEG variant.

```sh
img2sixel -L 'libjpeg!' photo.jpg
img2sixel -L 'libjpeg:orientation=0!' photo.jpg
```

The trailing `!` prevents fallback and is useful when comparing this adapter with [builtin JPEG](builtin/jpeg.md). Without it, `-L libjpeg` changes priority and the [loader manager](README.md) can try another eligible loader before any frame has been delivered.

## Decode and metadata boundaries

| Concern | Current adapter behavior |
| --- | --- |
| Ordinary JPEG | Uses libjpeg scanline decoding for library-supported baseline/progressive input. Grayscale is normalized for the downstream RGB pipeline. |
| Higher sample precision | The selected 12/16-bit scanline APIs preserve supported higher-depth data in float32 RGB work buffers. If those APIs are absent, naming `libjpeg` does not enable the missing precision path. Gray16 lossless input is decoded as native grayscale, then expanded locally to equal RGB float components; libjpeg lossless decoding does not offer native colorspace conversion. |
| CMYK/YCCK | Decodes through a four-component path. Adobe APP14 CMYK (transform 0) and YCCK (transform 2) are complemented before ICC conversion at every supported precision. Marker-absent input retains its decoded device coordinates. Embedded CMYK profile handling is distinct from the unprofiled fallback. |
| ICC | Collects the embedded profile and routes eligible conversion through the selected CMS integration. Source profile kind and pixel layout must match; CMS availability does not mean every profile can be applied. |
| Exif orientation | Enabled by default. `orientation=0` retains the stored raster orientation; applied rotations can exchange width and height before shared crop/resize. |
| Alpha and animation | JPEG has no source alpha in this adapter and produces one frame. `-S` does not expose another JPEG frame sequence. |

Backend-specific suboptions are `orientation=0|1` (`O`) and `cms_engine=auto|lcms2|colorsync|builtin|none` (`E`, subject to build availability). Their environment fallbacks are `SIXEL_LOADER_LIBJPEG_ORIENTATION` and `SIXEL_LOADER_LIBJPEG_CMS_ENGINE`, followed by the corresponding shared loader settings. Common loader CMS target, rendering-intent, and `prefer_8bit` controls are described in [color management](color-management/README.md) and [color-space handling](../encoder/colorspace-handling/README.md#loader-color-management).

Float storage preserves the values supplied by the decode/conversion route; it does not reverse JPEG loss or establish that an 8-bit JPEG contains higher-depth source information. Likewise, disabling CMS does not disable Exif orientation. Keep those controls fixed when comparing the external and builtin paths.

Eight-bit RGB and grayscale decode directly to gamma RGB888 when no ICC payload needs interpretation and the encoder has not requested float decoding. This also applies with CMS enabled but no embedded profile. `-.float32` and independent float-work requirements reach the loader through its precision preference. CMYK/YCCK conversion and high-depth decoding retain their existing float paths.

## Grayscale ICC source coordinates

Gray8/12/16 samples retain their source transfer until profile application. Although the decoder presents equal RGB components to the frame pipeline, the common CMS facade packs one original normalized sample per pixel and applies a Gray ICC profile with `GRAY_F32` input and RGB float output. The same facade is used with and without Little CMS compiled in; selecting builtin or ColorSync does not inherit a different source layout from build-time Little CMS availability. Gray ICC conversion uses the shared linear destination, including its ColorSync power-curve correction and failure-safe publication. Unconverted samples retain the existing sRGB fallback interpretation.

CMS-disabled and untagged eight-bit sources retain the RGB888 fast path unless float decoding is requested. Invalid profiles retain the original decoded samples for the existing fallback interpretation. High-depth grayscale and applied profiles do not acquire an intermediate byte boundary. This adapter currently publishes managed output as linear float; shared `cms_target` and `prefer_8bit` settings are not yet reflected in its publication format.

## CMYK and YCCK ICC source coordinates

libjpeg preserves inverted Adobe CMYK device coordinates, including the CMYK scanlines decoded from Adobe YCCK. ICC CMYK input expresses ink coverage. The adapter complements all four components only for matching APP14 markers: CMYK/transform 0 or YCCK/transform 2. Marker-absent input retains its source values. This follows the [libjpeg decoder](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/jdcolor.c) and [Little CMS jpgicc input flavor selection](https://github.com/mm2/Little-CMS/blob/master/utils/jpgicc/jpgicc.c).

Eight-bit coordinates use `255 - sample`. The 12-bit scanline path first expands each integer to the full uint16 range with `(sample * 65535 + 2047) / 4095`; sixteen-bit input already has that range. Both higher-precision paths then use `65535 - sample`. The shared CMS receives CMYK_8 or CMYK_16 original device coordinates, independently of the output RGB float representation. The build probes verify the sample typedef, MAX constant and linked scanline symbol; a typedef is not a preprocessor macro.

All these profile conversions use the selected shared CMS engine and rendering intent, including builds without Little CMS. Normalization surrounds the ICC attempt on an owned buffer. If profile creation or application fails, the original device samples are restored before the established unprofiled conversion. A destination prefix modified before failure is overwritten by that complete fallback. Disabling CMS or omitting a profile retains the historical `c*k`, `m*k`, `y*k` behavior; it is a compatibility interpretation, not a printer model. The fallback RGB result is never submitted to the source CMYK ICC profile as a second attempt.

The numeric controls reuse the coding bytes of the existing 32-by-32 CMYK JPEG. Only ICC APP2 data and the Adobe APP14 marker differ. [`generate_jpeg_cmyk_tests.py`](../../tools/generate_jpeg_cmyk_tests.py) derives a complete affine CMYK profile's PCS values with 80-digit Decimal arithmetic and an independently derived inverse D50 sRGB matrix. The source raster is frozen before any CMM; [`verify_jpeg_cmyk_source.c`](../../tools/verify_jpeg_cmyk_source.c) can reproduce it directly with libjpeg. The executable source check compares all 4,096 device bytes for the original and eight metadata variants. Runtime value checks fix geometry, format, colorspace and callback count, then check all 3,072 finite RGB components against the independent intervals. Builtin uses the existing 64u32 relative float/libm budget; external engines additionally receive the established two-bin u1.15 PCS budget propagated through the inverse matrix. [`jpeg-cmyk.json`](../../tests/data/color-numeric/jpeg-cmyk.json) records the equations, hashes and bounds.

The historical 320-byte profile in `cmyk_mft2_valid.jpg` is incomplete ICC metadata despite its filename. Its builtin default-intent compatibility path, strict-relative apply failure and external-engine rejection are separate observations. The default-intent reference is computed directly from its stored A2B0, complemented device samples and extended float RGB coordinates; it does not certify that the profile is standards-compliant. Only the serialized reference copy is clipped to the destination range before independent RGB101 rounding. Reference SIXEL uses an exact palette, without a production CMM or palette reducer; frame values follow the common [float conversion contract](../concepts/pixelformat-precision/README.md#float-conversion-arithmetic). The original JPEG and profile bytes are retained. `python3 tools/generate_color_fixtures.py --check` verifies these numeric fixtures and the historical reference together; regeneration also writes that reference under the sibling `tests/data/colormgmt/reference/custom/` directory.

The YCCK8, CMYK12, YCCK12 and CMYK16 controls reuse the original coding data of the existing 64-by-64 JPEG fixtures. [`generate_jpeg_precision_tests.py`](../../tools/generate_jpeg_precision_tests.py) retains compressed independent decoder records and full source/coding/raster hashes, then derives affine ICC and fallback RGB with 80-digit Decimal arithmetic. Runtime checks cover every component for finiteness and compare 32 fixed pixel witnesses (96 color components), including the prefix, its neighbors, row boundaries, interior colors and the last pixel. A separate include-seam check verifies all 128 selected CMYK input integers exactly after 12-bit expansion and Adobe normalization, qualifies a successful real transform, then injects a prefix-write failure and checks restored fallback values. [`jpeg-precision.json`](../../tests/data/color-numeric/jpeg-precision.json) records the derivation and selected indices. The same scalar derivation generates full reference images for independent MS-SSIM tests at 0.98; those image tests do not replace source/engine/failure observations.

## Implementation and validation

[`loader-libjpeg.c`](../../src/loader-libjpeg.c) owns scanline decode, CMYK conversion, ICC extraction, orientation, and the component callback. libjpeg errors are translated into loader status and cleaned up; allocator or callback failures must not be treated as an invitation to append frames from another backend.

The [libjpeg test category](../../tests/loader-jpeg) covers progressive decode, forced selection, signatures, malformed JPEG, output representations, CMYK, and metadata/precision cases. Higher-depth checks are conditional on the linked APIs. This inventory does not guarantee every library release's coding-process coverage or that every profile takes a transform path. Compare effective pixel format and profile diagnostics, not just successful conversion.

For source-format behavior implemented without libjpeg, see [builtin JPEG](builtin/jpeg.md); its accepted precision/coding modes are a separate contract.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

#### Grayscale ICC

The in-memory fixtures encode constant 8/12-bit DCT blocks with unit quantization and native 16-bit lossless predictor samples. Expected linear values follow the existing `gray-gamma2.icc` profile analytically: `(sample / maximum)^2`. Original sample and fallback checks are separate from profile-application checks; no CMM-generated image is an oracle. Native CMS tolerances cover their existing numerical error, while CMS-disabled Gray8/16 checks require exact float source values and Gray12 allows only the two float roundings of its reciprocal normalization. Existing MS-SSIM tests below remain a separate quality observation.

| ID | Contract | Owning test |
| --- | --- | --- |
| JGRAY-01 | Exact source-derived RGB values and frame representation: gray8 builtin; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0022_gray8_builtin.t](../../tests/loader-jpeg/gray/0022_gray8_builtin.t) |
| JGRAY-02 | Exact source-derived RGB values and frame representation: gray8 lcms2; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0023_gray8_lcms2.t](../../tests/loader-jpeg/gray/0023_gray8_lcms2.t) |
| JGRAY-03 | Exact source-derived RGB values and frame representation: gray8 colorsync; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0024_gray8_colorsync.t](../../tests/loader-jpeg/gray/0024_gray8_colorsync.t) |
| JGRAY-04 | Exact source-derived RGB values and frame representation: gray12 builtin; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0025_gray12_builtin.t](../../tests/loader-jpeg/gray/0025_gray12_builtin.t) |
| JGRAY-05 | Exact source-derived RGB values and frame representation: gray12 lcms2; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0026_gray12_lcms2.t](../../tests/loader-jpeg/gray/0026_gray12_lcms2.t) |
| JGRAY-06 | Exact source-derived RGB values and frame representation: gray12 colorsync; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0027_gray12_colorsync.t](../../tests/loader-jpeg/gray/0027_gray12_colorsync.t) |
| JGRAY-07 | Exact source-derived RGB values and frame representation: gray16 builtin; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0028_gray16_builtin.t](../../tests/loader-jpeg/gray/0028_gray16_builtin.t) |
| JGRAY-08 | Exact source-derived RGB values and frame representation: gray16 lcms2; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0029_gray16_lcms2.t](../../tests/loader-jpeg/gray/0029_gray16_lcms2.t) |
| JGRAY-09 | Exact source-derived RGB values and frame representation: gray16 colorsync; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0030_gray16_colorsync.t](../../tests/loader-jpeg/gray/0030_gray16_colorsync.t) |
| JGRAY-10 | Exact source-derived RGB values and frame representation: cms none byte; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0031_cms_none_byte.t](../../tests/loader-jpeg/gray/0031_cms_none_byte.t) |
| JGRAY-11 | Exact source-derived RGB values and frame representation: untagged byte; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0032_untagged_byte.t](../../tests/loader-jpeg/gray/0032_untagged_byte.t) |
| JGRAY-13 | Exact source-derived RGB values and frame representation: gray12 cms none; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0033_gray12_cms_none.t](../../tests/loader-jpeg/gray/0033_gray12_cms_none.t) |
| JGRAY-14 | Exact source-derived RGB values and frame representation: untagged float; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0034_untagged_float.t](../../tests/loader-jpeg/gray/0034_untagged_float.t) |
| JGRAY-15 | Exact source-derived RGB values and frame representation: cms none float; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0035_cms_none_float.t](../../tests/loader-jpeg/gray/0035_cms_none_float.t) |
| JGRAY-16 | Exact source-derived RGB values and frame representation: gray16 cms none; managed cases require actual Gray float apply by the selected engine. | [tests/loader-jpeg/gray/0036_gray16_cms_none.t](../../tests/loader-jpeg/gray/0036_gray16_cms_none.t) |

#### CMYK and YCCK

| ID | Contract | Owning test |
| --- | --- | --- |
| JCMYK-01 | Independent full-frame values: affine adobe builtin. | [tests/loader-jpeg/cmyk/0061_affine_adobe_builtin.t](../../tests/loader-jpeg/cmyk/0061_affine_adobe_builtin.t) |
| JCMYK-02 | Independent full-frame values: affine plain builtin. | [tests/loader-jpeg/cmyk/0062_affine_plain_builtin.t](../../tests/loader-jpeg/cmyk/0062_affine_plain_builtin.t) |
| JCMYK-03 | Independent full-frame values: affine adobe lcms2. | [tests/loader-jpeg/cmyk/0063_affine_adobe_lcms2.t](../../tests/loader-jpeg/cmyk/0063_affine_adobe_lcms2.t) |
| JCMYK-04 | Independent full-frame values: affine plain lcms2. | [tests/loader-jpeg/cmyk/0064_affine_plain_lcms2.t](../../tests/loader-jpeg/cmyk/0064_affine_plain_lcms2.t) |
| JCMYK-05 | Independent full-frame values: affine adobe colorsync. | [tests/loader-jpeg/cmyk/0065_affine_adobe_colorsync.t](../../tests/loader-jpeg/cmyk/0065_affine_adobe_colorsync.t) |
| JCMYK-06 | Independent full-frame values: affine plain colorsync. | [tests/loader-jpeg/cmyk/0066_affine_plain_colorsync.t](../../tests/loader-jpeg/cmyk/0066_affine_plain_colorsync.t) |
| JCMYK-07 | Independent full-frame values: cms disabled adobe. | [tests/loader-jpeg/cmyk/0067_cms_disabled_adobe.t](../../tests/loader-jpeg/cmyk/0067_cms_disabled_adobe.t) |
| JCMYK-08 | Independent full-frame values: cms disabled plain. | [tests/loader-jpeg/cmyk/0068_cms_disabled_plain.t](../../tests/loader-jpeg/cmyk/0068_cms_disabled_plain.t) |
| JCMYK-09 | Independent full-frame values: profile absent adobe. | [tests/loader-jpeg/cmyk/0069_profile_absent_adobe.t](../../tests/loader-jpeg/cmyk/0069_profile_absent_adobe.t) |
| JCMYK-10 | Independent full-frame values: profile absent plain. | [tests/loader-jpeg/cmyk/0070_profile_absent_plain.t](../../tests/loader-jpeg/cmyk/0070_profile_absent_plain.t) |
| JCMYK-11 | Independent full-frame values: profile rejected adobe. | [tests/loader-jpeg/codestream/0123_profile_rejected_adobe.t](../../tests/loader-jpeg/codestream/0123_profile_rejected_adobe.t) |
| JCMYK-12 | Independent full-frame values: profile rejected plain. | [tests/loader-jpeg/codestream/0124_profile_rejected_plain.t](../../tests/loader-jpeg/codestream/0124_profile_rejected_plain.t) |
| JPREC-01 | Independent RGB witness values and actual selected-engine apply: 8bit-ycck-seq444 builtin. | [tests/loader-jpeg/cmyk/0082_8bit_ycck_seq444_builtin.t](../../tests/loader-jpeg/cmyk/0082_8bit_ycck_seq444_builtin.t) |
| JPREC-02 | Independent RGB witness values and actual selected-engine apply: 8bit-ycck-seq444 lcms2. | [tests/loader-jpeg/cmyk/0083_8bit_ycck_seq444_lcms2.t](../../tests/loader-jpeg/cmyk/0083_8bit_ycck_seq444_lcms2.t) |
| JPREC-03 | Independent RGB witness values and actual selected-engine apply: 8bit-ycck-seq444 colorsync. | [tests/loader-jpeg/cmyk/0084_8bit_ycck_seq444_colorsync.t](../../tests/loader-jpeg/cmyk/0084_8bit_ycck_seq444_colorsync.t) |
| JPREC-04 | CMS disabled retains independent original-source fallback values without apply: 8bit-ycck-seq444 cms none. | [tests/loader-jpeg/cmyk/0085_8bit_ycck_seq444_cms_none.t](../../tests/loader-jpeg/cmyk/0085_8bit_ycck_seq444_cms_none.t) |
| JPREC-05 | Absent ICC retains independent original-source fallback values without apply: 8bit-ycck-seq444 profile absent. | [tests/loader-jpeg/cmyk/0086_8bit_ycck_seq444_profile_absent.t](../../tests/loader-jpeg/cmyk/0086_8bit_ycck_seq444_profile_absent.t) |
| JPREC-06 | Rejected ICC retains independent original-source fallback values without apply: 8bit-ycck-seq444 profile rejected. | [tests/loader-jpeg/codestream/0127_8bit_ycck_seq444_profile_rejected.t](../../tests/loader-jpeg/codestream/0127_8bit_ycck_seq444_profile_rejected.t) |
| JPREC-07 | Strict-intent apply failure after successful qualification retains independent fallback values: 8bit-ycck-seq444 strict apply failure. | [tests/loader-jpeg/cmyk/0087_8bit_ycck_seq444_strict_apply_failure.t](../../tests/loader-jpeg/cmyk/0087_8bit_ycck_seq444_strict_apply_failure.t) |
| JPREC-08 | Exact packed CMYK source integers and complete fallback after a qualified prefix-write failure: 8bit-ycck-seq444 prefix failure. | [tests/loader-jpeg/cmyk/0088_8bit_ycck_seq444_prefix_failure.t](../../tests/loader-jpeg/cmyk/0088_8bit_ycck_seq444_prefix_failure.t) |
| JPREC-09 | Independent RGB witness values and actual selected-engine apply: 12bit-cmyk-seq444 builtin. | [tests/loader-jpeg/cmyk/0089_12bit_cmyk_seq444_builtin.t](../../tests/loader-jpeg/cmyk/0089_12bit_cmyk_seq444_builtin.t) |
| JPREC-10 | Independent RGB witness values and actual selected-engine apply: 12bit-cmyk-seq444 lcms2. | [tests/loader-jpeg/cmyk/0090_12bit_cmyk_seq444_lcms2.t](../../tests/loader-jpeg/cmyk/0090_12bit_cmyk_seq444_lcms2.t) |
| JPREC-11 | Independent RGB witness values and actual selected-engine apply: 12bit-cmyk-seq444 colorsync. | [tests/loader-jpeg/cmyk/0091_12bit_cmyk_seq444_colorsync.t](../../tests/loader-jpeg/cmyk/0091_12bit_cmyk_seq444_colorsync.t) |
| JPREC-12 | CMS disabled retains independent original-source fallback values without apply: 12bit-cmyk-seq444 cms none. | [tests/loader-jpeg/cmyk/0092_12bit_cmyk_seq444_cms_none.t](../../tests/loader-jpeg/cmyk/0092_12bit_cmyk_seq444_cms_none.t) |
| JPREC-13 | Absent ICC retains independent original-source fallback values without apply: 12bit-cmyk-seq444 profile absent. | [tests/loader-jpeg/cmyk/0093_12bit_cmyk_seq444_profile_absent.t](../../tests/loader-jpeg/cmyk/0093_12bit_cmyk_seq444_profile_absent.t) |
| JPREC-14 | Rejected ICC retains independent original-source fallback values without apply: 12bit-cmyk-seq444 profile rejected. | [tests/loader-jpeg/codestream/0128_12bit_cmyk_seq444_profile_rejected.t](../../tests/loader-jpeg/codestream/0128_12bit_cmyk_seq444_profile_rejected.t) |
| JPREC-15 | Strict-intent apply failure after successful qualification retains independent fallback values: 12bit-cmyk-seq444 strict apply failure. | [tests/loader-jpeg/cmyk/0094_12bit_cmyk_seq444_strict_apply_failure.t](../../tests/loader-jpeg/cmyk/0094_12bit_cmyk_seq444_strict_apply_failure.t) |
| JPREC-16 | Exact packed CMYK source integers and complete fallback after a qualified prefix-write failure: 12bit-cmyk-seq444 prefix failure. | [tests/loader-jpeg/cmyk/0095_12bit_cmyk_seq444_prefix_failure.t](../../tests/loader-jpeg/cmyk/0095_12bit_cmyk_seq444_prefix_failure.t) |
| JPREC-17 | Independent RGB witness values and actual selected-engine apply: 12bit-ycck-seq444 builtin. | [tests/loader-jpeg/cmyk/0096_12bit_ycck_seq444_builtin.t](../../tests/loader-jpeg/cmyk/0096_12bit_ycck_seq444_builtin.t) |
| JPREC-18 | Independent RGB witness values and actual selected-engine apply: 12bit-ycck-seq444 lcms2. | [tests/loader-jpeg/cmyk/0097_12bit_ycck_seq444_lcms2.t](../../tests/loader-jpeg/cmyk/0097_12bit_ycck_seq444_lcms2.t) |
| JPREC-19 | Independent RGB witness values and actual selected-engine apply: 12bit-ycck-seq444 colorsync. | [tests/loader-jpeg/cmyk/0098_12bit_ycck_seq444_colorsync.t](../../tests/loader-jpeg/cmyk/0098_12bit_ycck_seq444_colorsync.t) |
| JPREC-20 | CMS disabled retains independent original-source fallback values without apply: 12bit-ycck-seq444 cms none. | [tests/loader-jpeg/cmyk/0099_12bit_ycck_seq444_cms_none.t](../../tests/loader-jpeg/cmyk/0099_12bit_ycck_seq444_cms_none.t) |
| JPREC-21 | Absent ICC retains independent original-source fallback values without apply: 12bit-ycck-seq444 profile absent. | [tests/loader-jpeg/cmyk/0100_12bit_ycck_seq444_profile_absent.t](../../tests/loader-jpeg/cmyk/0100_12bit_ycck_seq444_profile_absent.t) |
| JPREC-22 | Rejected ICC retains independent original-source fallback values without apply: 12bit-ycck-seq444 profile rejected. | [tests/loader-jpeg/codestream/0129_12bit_ycck_seq444_profile_rejected.t](../../tests/loader-jpeg/codestream/0129_12bit_ycck_seq444_profile_rejected.t) |
| JPREC-23 | Strict-intent apply failure after successful qualification retains independent fallback values: 12bit-ycck-seq444 strict apply failure. | [tests/loader-jpeg/cmyk/0101_12bit_ycck_seq444_strict_apply_failure.t](../../tests/loader-jpeg/cmyk/0101_12bit_ycck_seq444_strict_apply_failure.t) |
| JPREC-24 | Exact packed CMYK source integers and complete fallback after a qualified prefix-write failure: 12bit-ycck-seq444 prefix failure. | [tests/loader-jpeg/cmyk/0102_12bit_ycck_seq444_prefix_failure.t](../../tests/loader-jpeg/cmyk/0102_12bit_ycck_seq444_prefix_failure.t) |
| JPREC-25 | Independent RGB witness values and actual selected-engine apply: 16bit-cmyk-lossless builtin. | [tests/loader-jpeg/cmyk/0103_16bit_cmyk_lossless_builtin.t](../../tests/loader-jpeg/cmyk/0103_16bit_cmyk_lossless_builtin.t) |
| JPREC-26 | Independent RGB witness values and actual selected-engine apply: 16bit-cmyk-lossless lcms2. | [tests/loader-jpeg/cmyk/0104_16bit_cmyk_lossless_lcms2.t](../../tests/loader-jpeg/cmyk/0104_16bit_cmyk_lossless_lcms2.t) |
| JPREC-27 | Independent RGB witness values and actual selected-engine apply: 16bit-cmyk-lossless colorsync. | [tests/loader-jpeg/cmyk/0105_16bit_cmyk_lossless_colorsync.t](../../tests/loader-jpeg/cmyk/0105_16bit_cmyk_lossless_colorsync.t) |
| JPREC-28 | CMS disabled retains independent original-source fallback values without apply: 16bit-cmyk-lossless cms none. | [tests/loader-jpeg/cmyk/0106_16bit_cmyk_lossless_cms_none.t](../../tests/loader-jpeg/cmyk/0106_16bit_cmyk_lossless_cms_none.t) |
| JPREC-29 | Absent ICC retains independent original-source fallback values without apply: 16bit-cmyk-lossless profile absent. | [tests/loader-jpeg/cmyk/0107_16bit_cmyk_lossless_profile_absent.t](../../tests/loader-jpeg/cmyk/0107_16bit_cmyk_lossless_profile_absent.t) |
| JPREC-30 | Rejected ICC retains independent original-source fallback values without apply: 16bit-cmyk-lossless profile rejected. | [tests/loader-jpeg/codestream/0130_16bit_cmyk_lossless_profile_rejected.t](../../tests/loader-jpeg/codestream/0130_16bit_cmyk_lossless_profile_rejected.t) |
| JPREC-31 | Strict-intent apply failure after successful qualification retains independent fallback values: 16bit-cmyk-lossless strict apply failure. | [tests/loader-jpeg/cmyk/0108_16bit_cmyk_lossless_strict_apply_failure.t](../../tests/loader-jpeg/cmyk/0108_16bit_cmyk_lossless_strict_apply_failure.t) |
| JPREC-32 | Exact packed CMYK source integers and complete fallback after a qualified prefix-write failure: 16bit-cmyk-lossless prefix failure. | [tests/loader-jpeg/cmyk/0109_16bit_cmyk_lossless_prefix_failure.t](../../tests/loader-jpeg/cmyk/0109_16bit_cmyk_lossless_prefix_failure.t) |
| JGRAY16-01 | Original lossless Gray16 integers expand to equal, exact normalized RGB float values. | [tests/loader-jpeg/gray/0021_loader_libjpeg_gray16_original_samples.t](../../tests/loader-jpeg/gray/0021_loader_libjpeg_gray16_original_samples.t) |

### Quality regression tests

These tests compare the final SIXEL with independently generated images at the unchanged MS-SSIM floor of 0.98. Numeric and route checks remain separate from this perceptual gate. In particular, JPQ cases record the requested CLI engine and qualify the numeric helper separately; their final-image assertion is the MS-SSIM floor, not an additional observation of the CLI engine trace.

| ID | Contract | Owning test |
| --- | --- | --- |
| JQ-01 | Affine CMYK, adobe, builtin. | [tests/loader-jpeg/cmyk/0076_quality_adobe_builtin.t](../../tests/loader-jpeg/cmyk/0076_quality_adobe_builtin.t) |
| JQ-02 | Affine CMYK, plain, builtin. | [tests/loader-jpeg/cmyk/0077_quality_plain_builtin.t](../../tests/loader-jpeg/cmyk/0077_quality_plain_builtin.t) |
| JQ-03 | Affine CMYK, adobe, lcms2. | [tests/loader-jpeg/cmyk/0078_quality_adobe_lcms2.t](../../tests/loader-jpeg/cmyk/0078_quality_adobe_lcms2.t) |
| JQ-04 | Affine CMYK, plain, lcms2. | [tests/loader-jpeg/cmyk/0079_quality_plain_lcms2.t](../../tests/loader-jpeg/cmyk/0079_quality_plain_lcms2.t) |
| JQ-05 | Affine CMYK, adobe, colorsync. | [tests/loader-jpeg/cmyk/0080_quality_adobe_colorsync.t](../../tests/loader-jpeg/cmyk/0080_quality_adobe_colorsync.t) |
| JQ-06 | Affine CMYK, plain, colorsync. | [tests/loader-jpeg/cmyk/0081_quality_plain_colorsync.t](../../tests/loader-jpeg/cmyk/0081_quality_plain_colorsync.t) |
| JQ-07 | Original incomplete-profile compatibility image against the independently corrected reference. | [tests/loader-jpeg/cmyk/0060_loader_libjpeg_jpeg_cmyk_mft2_valid_builtin_lsqa.t](../../tests/loader-jpeg/cmyk/0060_loader_libjpeg_jpeg_cmyk_mft2_valid_builtin_lsqa.t) |
| JPQ-01 | Independent final-image quality floor, requested 8bit-ycck-seq444, builtin, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0110_quality_8bit_ycck_seq444_builtin.t](../../tests/loader-jpeg/cmyk/0110_quality_8bit_ycck_seq444_builtin.t) |
| JPQ-02 | Independent final-image quality floor, requested 8bit-ycck-seq444, lcms2, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0111_quality_8bit_ycck_seq444_lcms2.t](../../tests/loader-jpeg/cmyk/0111_quality_8bit_ycck_seq444_lcms2.t) |
| JPQ-03 | Independent final-image quality floor, requested 8bit-ycck-seq444, colorsync, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0112_quality_8bit_ycck_seq444_colorsync.t](../../tests/loader-jpeg/cmyk/0112_quality_8bit_ycck_seq444_colorsync.t) |
| JPQ-04 | Independent final-image quality floor, requested 12bit-cmyk-seq444, builtin, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0113_quality_12bit_cmyk_seq444_builtin.t](../../tests/loader-jpeg/cmyk/0113_quality_12bit_cmyk_seq444_builtin.t) |
| JPQ-05 | Independent final-image quality floor, requested 12bit-cmyk-seq444, lcms2, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0114_quality_12bit_cmyk_seq444_lcms2.t](../../tests/loader-jpeg/cmyk/0114_quality_12bit_cmyk_seq444_lcms2.t) |
| JPQ-06 | Independent final-image quality floor, requested 12bit-cmyk-seq444, colorsync, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0115_quality_12bit_cmyk_seq444_colorsync.t](../../tests/loader-jpeg/cmyk/0115_quality_12bit_cmyk_seq444_colorsync.t) |
| JPQ-07 | Independent final-image quality floor, requested 12bit-ycck-seq444, builtin, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0116_quality_12bit_ycck_seq444_builtin.t](../../tests/loader-jpeg/cmyk/0116_quality_12bit_ycck_seq444_builtin.t) |
| JPQ-08 | Independent final-image quality floor, requested 12bit-ycck-seq444, lcms2, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0117_quality_12bit_ycck_seq444_lcms2.t](../../tests/loader-jpeg/cmyk/0117_quality_12bit_ycck_seq444_lcms2.t) |
| JPQ-09 | Independent final-image quality floor, requested 12bit-ycck-seq444, colorsync, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0118_quality_12bit_ycck_seq444_colorsync.t](../../tests/loader-jpeg/cmyk/0118_quality_12bit_ycck_seq444_colorsync.t) |
| JPQ-10 | Independent final-image quality floor, requested 16bit-cmyk-lossless, builtin, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0119_quality_16bit_cmyk_lossless_builtin.t](../../tests/loader-jpeg/cmyk/0119_quality_16bit_cmyk_lossless_builtin.t) |
| JPQ-11 | Independent final-image quality floor, requested 16bit-cmyk-lossless, lcms2, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0120_quality_16bit_cmyk_lossless_lcms2.t](../../tests/loader-jpeg/cmyk/0120_quality_16bit_cmyk_lossless_lcms2.t) |
| JPQ-12 | Independent final-image quality floor, requested 16bit-cmyk-lossless, colorsync, MS-SSIM at least 0.98. | [tests/loader-jpeg/cmyk/0121_quality_16bit_cmyk_lossless_colorsync.t](../../tests/loader-jpeg/cmyk/0121_quality_16bit_cmyk_lossless_colorsync.t) |
| JGRAY16Q-01 | Existing Gray16 lossless restart JPEG retains the independent image-quality floor. | [tests/loader-jpeg/gray/0020_loader_libjpeg_jpeg_gray_16bit_lossless_r64_expected_lsqa.t](../../tests/loader-jpeg/gray/0020_loader_libjpeg_jpeg_gray_16bit_lossless_r64_expected_lsqa.t) |

### Defensive and malformed-input tests

The [Gray ICC tests](../../tests/loader-jpeg) also verify that a rejected ICC profile preserves the original samples under the fallback transfer, without a successful CMS apply.


The [CMYK controls](../../tests/loader-jpeg) separately check source-byte invariance, successful-transform qualification followed by an actual missing-intent apply failure for both marker states, and the historical incomplete profile's default acceptance, strict-relative failure and external rejection. Available CMS engines are qualified at runtime, and unavailable engines are reported as skips.
