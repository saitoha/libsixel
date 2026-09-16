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
| High-precision scanlines | Selected contiguous RGB/grayscale 16-bit integer or float32 layouts, and selected 16-bit/float32 CIELAB layouts, without extra alpha samples. Supported orientation and scanline-layout checks also apply. |
| `TIFFReadRGBAImageOriented` | The general libtiff conversion fallback for combinations outside the direct path. Its RGBA result is an 8-bit representation; later promotion to float32 does not recover discarded source precision. |
| Loader-manager fallback | A different backend may be tried only according to the shared failure/callback rules. This is separate from choosing libtiff's internal RGBA route. |

The direct CIELAB path retains a typed Lab representation where applicable, rather than presenting Lab channel bytes as sRGB. RGB/gray paths and profile conversion have their own normalization. Inspect emitted `pixelformat` and `colorspace`; a `.tif` extension is not sufficient to infer either.

TIFF orientation is handled in the decode paths. The direct path has narrower layout/orientation eligibility than the convenience reader, so changing tags can change both geometry handling and precision route. This backend does not expose the JPEG-style `orientation` suboption in the current registry.

## Color and alpha

`cms_engine=...` (`E`) selects the TIFF CMS engine; the environment fallback is `SIXEL_LOADER_LIBTIFF_CMS_ENGINE`, then shared CMS policy. Embedded ICC use depends on photometric interpretation and source/profile compatibility. The adapter contains separate handling for grayscale, RGB-like RGBA data, and high-precision/Lab paths. Do not assume all TIFF photometric models share an identical transform.

Alpha-bearing layouts commonly take the RGBA path. Alpha normalization and explicit background processing still determine the final frame representation; request [alpha/background policy](alpha-policy.md) explicitly when comparing decoders. TIFF reader support for a compressed image does not imply support for preserving its original alpha association or sample depth end to end.

## Implementation and validation

[`loader-libtiff.c`](../../src/loader-libtiff.c) owns memory-backed TIFF access, direct-path eligibility, the RGBA fallback, ICC processing, and frame handoff. The [libtiff test category](../../tests/loader/libtiff/) contains RGB/indexed/gray/alpha and compression fixtures, high-depth and Lab cases, CMS comparisons, and malformed-input checks. Perceptual tests are quality evidence, not proof that a particular precision path or metadata transform ran. Exact frame/metadata checks and diagnostics are needed for that distinction.

For the common downstream representation and resize promotion rules, see [pixel-format precision](../concepts/pixelformat-precision.md).
