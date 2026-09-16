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
| CMYK/YCCK | Decodes through the CMYK path and handles JPEG/Adobe polarity before RGB conversion. Embedded CMYK profile handling is distinct from the unprofiled fallback. |
| ICC | Collects the embedded profile and routes eligible conversion through the selected CMS integration. Source profile kind and pixel layout must match; CMS availability does not mean every profile can be applied. |
| Exif orientation | Enabled by default. `orientation=0` retains the stored raster orientation; applied rotations can exchange width and height before shared crop/resize. |
| Alpha and animation | JPEG has no source alpha in this adapter and produces one frame. `-S` does not expose another JPEG frame sequence. |

Backend-specific suboptions are `orientation=0|1` (`O`) and `cms_engine=auto|lcms2|colorsync|builtin|none` (`E`, subject to build availability). Their environment fallbacks are `SIXEL_LOADER_LIBJPEG_ORIENTATION` and `SIXEL_LOADER_LIBJPEG_CMS_ENGINE`, followed by the corresponding shared loader settings. Common loader CMS target, rendering-intent, and `prefer_8bit` controls are described in [color management](color-management.md) and [color spaces](../concepts/colorspace.md).

Float storage preserves the values supplied by the decode/conversion route; it does not reverse JPEG loss or establish that an 8-bit JPEG contains higher-depth source information. Likewise, disabling CMS does not disable Exif orientation. Keep those controls fixed when comparing the external and builtin paths.

## Implementation and validation

[`loader-libjpeg.c`](../../src/loader-libjpeg.c) owns scanline decode, CMYK conversion, ICC extraction, orientation, and the component callback. libjpeg errors are translated into loader status and cleaned up; allocator or callback failures must not be treated as an invitation to append frames from another backend.

The [libjpeg test category](../../tests/loader/libjpeg/) covers progressive decode, forced selection, signatures, malformed JPEG, output representations, CMYK, and metadata/precision cases. Higher-depth checks are conditional on the linked APIs. This inventory does not guarantee every library release's coding-process coverage or that every profile takes a transform path. Compare effective pixel format and profile diagnostics, not just successful conversion.

For source-format behavior implemented without libjpeg, see [builtin JPEG](builtin/jpeg.md); its accepted precision/coding modes are a separate contract.
