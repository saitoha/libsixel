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
| Higher sample precision | The selected 12/16-bit scanline APIs preserve supported higher-depth data in float32 RGB work buffers. If those APIs are absent, naming `libjpeg` does not enable the missing precision path. |
| CMYK/YCCK | Decodes through a four-component path. Ordinary 8-bit `JCS_CMYK` with an Adobe APP14 marker and transform 0 is complemented before ICC conversion. Marker-absent, YCCK and higher-precision paths retain their own existing interpretation. Embedded CMYK profile handling is distinct from the unprofiled fallback. |
| ICC | Collects the embedded profile and routes eligible conversion through the selected CMS integration. Source profile kind and pixel layout must match; CMS availability does not mean every profile can be applied. |
| Exif orientation | Enabled by default. `orientation=0` retains the stored raster orientation; applied rotations can exchange width and height before shared crop/resize. |
| Alpha and animation | JPEG has no source alpha in this adapter and produces one frame. `-S` does not expose another JPEG frame sequence. |

Backend-specific suboptions are `orientation=0|1` (`O`) and `cms_engine=auto|lcms2|colorsync|builtin|none` (`E`, subject to build availability). Their environment fallbacks are `SIXEL_LOADER_LIBJPEG_ORIENTATION` and `SIXEL_LOADER_LIBJPEG_CMS_ENGINE`, followed by the corresponding shared loader settings. Common loader CMS target, rendering-intent, and `prefer_8bit` controls are described in [color management](color-management.md) and [color spaces](../concepts/colorspace.md).

Float storage preserves the values supplied by the decode/conversion route; it does not reverse JPEG loss or establish that an 8-bit JPEG contains higher-depth source information. Likewise, disabling CMS does not disable Exif orientation. Keep those controls fixed when comparing the external and builtin paths.

## Ordinary CMYK ICC polarity

libjpeg preserves inverted Adobe CMYK device bytes on its `JCS_CMYK` scanline path. ICC CMYK input expresses ink coverage instead: the adapter supplies `255 - sample` only for ordinary 8-bit `JCS_CMYK` with an Adobe marker and transform 0. Marker-absent input supplies its original device values. See the [libjpeg application documentation](https://raw.githubusercontent.com/libjpeg-turbo/libjpeg-turbo/main/doc/libjpeg.txt) and [Little CMS jpgicc input flavor selection](https://github.com/mm2/Little-CMS/blob/master/utils/jpgicc/jpgicc.c).

Ordinary 8-bit `JCS_CMYK` uses the existing shared CMS facade with or without Little CMS support, so the selected CMS engine and strict rendering intent govern conversion in both builds. The legacy direct ICC connection is retained for YCCK in builds without Little CMS.

This normalization surrounds the existing ICC attempt on an owned temporary buffer. If the profile is rejected or the transform cannot apply, the adapter restores the original bytes before the established unprofiled conversion. Disabling CMS or omitting the profile also retains that fallback. Its historical `c*k`, `m*k`, `y*k` calculation is a compatibility behavior, not a physical printer model. The change does not alter YCCK, 12-bit or 16-bit policy.

The numeric controls reuse the coding bytes of the existing 32-by-32 CMYK JPEG. Only ICC APP2 data and the Adobe APP14 marker differ. [`generate_jpeg_cmyk_tests.py`](../../tools/generate_jpeg_cmyk_tests.py) derives a complete affine CMYK profile's PCS values with 80-digit Decimal arithmetic and an independently derived inverse D50 sRGB matrix. The source raster is frozen before any CMM; [`verify_jpeg_cmyk_source.c`](../../tools/verify_jpeg_cmyk_source.c) can reproduce it directly with libjpeg. The executable source check compares all 4,096 device bytes for the original and eight metadata variants. Runtime value checks fix geometry, format, colorspace and callback count, then check all 3,072 finite RGB components against the independent intervals. Builtin uses the existing 64u32 relative float/libm budget; external engines additionally receive the established two-bin u1.15 PCS budget propagated through the inverse matrix. [`jpeg-cmyk.json`](../../tests/data/color-numeric/jpeg-cmyk.json) records the equations, hashes and bounds.

The historical 320-byte profile in `cmyk_mft2_valid.jpg` is incomplete ICC metadata despite its filename. Its builtin default-intent compatibility path, strict-relative apply failure and external-engine rejection are separate observations. The default-intent reference is computed directly from its stored A2B0, complemented device samples and existing bounded output contract; it does not certify that the profile is standards-compliant. Reference SIXEL is encoded directly from independently rounded RGB101 pixels with an exact palette, without a production CMM or palette reducer. The original JPEG and profile bytes are retained. `python3 tools/generate_color_fixtures.py --check` verifies these numeric fixtures and the historical reference together; regeneration also writes that reference under the sibling `tests/data/colormgmt/reference/custom/` directory.

## Implementation and validation

[`loader-libjpeg.c`](../../src/loader-libjpeg.c) owns scanline decode, CMYK conversion, ICC extraction, orientation, and the component callback. libjpeg errors are translated into loader status and cleaned up; allocator or callback failures must not be treated as an invitation to append frames from another backend.

The [libjpeg test category](../../tests/loader/libjpeg/) covers progressive decode, forced selection, signatures, malformed JPEG, output representations, CMYK, and metadata/precision cases. Higher-depth checks are conditional on the linked APIs. This inventory does not guarantee every library release's coding-process coverage or that every profile takes a transform path. Compare effective pixel format and profile diagnostics, not just successful conversion.

For source-format behavior implemented without libjpeg, see [builtin JPEG](builtin/jpeg.md); its accepted precision/coding modes are a separate contract.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| JCMYK-01 | Independent full-frame values: affine adobe builtin. | [tests/loader/libjpeg/cmyk/0001_affine_adobe_builtin.t](../../tests/loader/libjpeg/cmyk/0001_affine_adobe_builtin.t) |
| JCMYK-02 | Independent full-frame values: affine plain builtin. | [tests/loader/libjpeg/cmyk/0002_affine_plain_builtin.t](../../tests/loader/libjpeg/cmyk/0002_affine_plain_builtin.t) |
| JCMYK-03 | Independent full-frame values: affine adobe lcms2. | [tests/loader/libjpeg/cmyk/0003_affine_adobe_lcms2.t](../../tests/loader/libjpeg/cmyk/0003_affine_adobe_lcms2.t) |
| JCMYK-04 | Independent full-frame values: affine plain lcms2. | [tests/loader/libjpeg/cmyk/0004_affine_plain_lcms2.t](../../tests/loader/libjpeg/cmyk/0004_affine_plain_lcms2.t) |
| JCMYK-05 | Independent full-frame values: affine adobe colorsync. | [tests/loader/libjpeg/cmyk/0005_affine_adobe_colorsync.t](../../tests/loader/libjpeg/cmyk/0005_affine_adobe_colorsync.t) |
| JCMYK-06 | Independent full-frame values: affine plain colorsync. | [tests/loader/libjpeg/cmyk/0006_affine_plain_colorsync.t](../../tests/loader/libjpeg/cmyk/0006_affine_plain_colorsync.t) |
| JCMYK-07 | Independent full-frame values: cms disabled adobe. | [tests/loader/libjpeg/cmyk/0007_cms_disabled_adobe.t](../../tests/loader/libjpeg/cmyk/0007_cms_disabled_adobe.t) |
| JCMYK-08 | Independent full-frame values: cms disabled plain. | [tests/loader/libjpeg/cmyk/0008_cms_disabled_plain.t](../../tests/loader/libjpeg/cmyk/0008_cms_disabled_plain.t) |
| JCMYK-09 | Independent full-frame values: profile absent adobe. | [tests/loader/libjpeg/cmyk/0009_profile_absent_adobe.t](../../tests/loader/libjpeg/cmyk/0009_profile_absent_adobe.t) |
| JCMYK-10 | Independent full-frame values: profile absent plain. | [tests/loader/libjpeg/cmyk/0010_profile_absent_plain.t](../../tests/loader/libjpeg/cmyk/0010_profile_absent_plain.t) |
| JCMYK-11 | Independent full-frame values: profile rejected adobe. | [tests/loader/libjpeg/cmyk/0011_profile_rejected_adobe.t](../../tests/loader/libjpeg/cmyk/0011_profile_rejected_adobe.t) |
| JCMYK-12 | Independent full-frame values: profile rejected plain. | [tests/loader/libjpeg/cmyk/0012_profile_rejected_plain.t](../../tests/loader/libjpeg/cmyk/0012_profile_rejected_plain.t) |

### Quality regression tests

These tests compare the final SIXEL with independently generated images at the unchanged MS-SSIM floor of 0.98. Numeric and route checks remain separate from this perceptual gate.

| ID | Contract | Owning test |
| --- | --- | --- |
| JQ-01 | Affine CMYK, adobe, builtin. | [tests/loader/libjpeg/cmyk/0020_quality_adobe_builtin.t](../../tests/loader/libjpeg/cmyk/0020_quality_adobe_builtin.t) |
| JQ-02 | Affine CMYK, plain, builtin. | [tests/loader/libjpeg/cmyk/0021_quality_plain_builtin.t](../../tests/loader/libjpeg/cmyk/0021_quality_plain_builtin.t) |
| JQ-03 | Affine CMYK, adobe, lcms2. | [tests/loader/libjpeg/cmyk/0022_quality_adobe_lcms2.t](../../tests/loader/libjpeg/cmyk/0022_quality_adobe_lcms2.t) |
| JQ-04 | Affine CMYK, plain, lcms2. | [tests/loader/libjpeg/cmyk/0023_quality_plain_lcms2.t](../../tests/loader/libjpeg/cmyk/0023_quality_plain_lcms2.t) |
| JQ-05 | Affine CMYK, adobe, colorsync. | [tests/loader/libjpeg/cmyk/0024_quality_adobe_colorsync.t](../../tests/loader/libjpeg/cmyk/0024_quality_adobe_colorsync.t) |
| JQ-06 | Affine CMYK, plain, colorsync. | [tests/loader/libjpeg/cmyk/0025_quality_plain_colorsync.t](../../tests/loader/libjpeg/cmyk/0025_quality_plain_colorsync.t) |
| JQ-07 | Original incomplete-profile compatibility image against the independently corrected reference. | [tests/loader/libjpeg/0041_loader_libjpeg_jpeg_cmyk_mft2_valid_builtin_lsqa.t](../../tests/loader/libjpeg/0041_loader_libjpeg_jpeg_cmyk_mft2_valid_builtin_lsqa.t) |

### Defensive and malformed-input tests

The [CMYK controls](../../tests/loader/libjpeg/cmyk/) separately check source-byte invariance, successful-transform qualification followed by an actual missing-intent apply failure for both marker states, and the historical incomplete profile's default acceptance, strict-relative failure and external rejection. Available CMS engines are qualified at runtime, and unavailable engines are reported as skips.
