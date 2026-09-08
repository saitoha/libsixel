# Pixel Formats and Alpha Representation

## Scope

libsixel handles pixels at three distinct layers. Keeping them separate avoids confusing an input memory layout with a SIXEL protocol capability:

1. a source file or caller-owned pixel buffer;
2. a `sixel_frame_t` used between loading, processing, and encoding;
3. the paletted SIXEL byte stream sent to a terminal.

An alpha channel may exist at the first two layers. It does not exist at the third layer.

## In-memory pixel formats

The `SIXEL_PIXELFORMAT_*` constants describe how samples are arranged in a memory buffer. They include:

- packed RGB and BGR formats, including 15-, 16-, 24-, and 32-bit layouts;
- alpha-bearing RGBA, ARGB, BGRA, and ABGR layouts;
- opaque XRGB, RGBX, XBGR, and BGRX layouts, where `X` is padding rather than alpha;
- grayscale and gray-alpha layouts;
- `PAL1`, `PAL2`, `PAL4`, and `PAL8` indices with a separate palette;
- internal three-channel float formats used for RGB, linear RGB, Oklab, CIELAB, and DIN99d processing.

The pixel-format tag describes layout. Colorspace metadata describes how the stored color values are interpreted. Neither property by itself states how the terminal will treat an unpainted SIXEL position.

The distinction between three-channel byte storage and typed float32 storage, including implicit promotion by resize, CMS, and color-space conversion, is documented in [Pixel-format precision](pixelformat-precision.md).

## Frame representation

A frame owns or borrows a pixel buffer and can also carry a palette, dimensions, colorspace, animation timing, and transparency metadata. The transparency metadata has three relevant forms:

- `transparent`: a transparent palette index for indexed pixels;
- `transparent_mask`: a binary per-pixel mask whose marked samples must not be painted;
- `alpha_zero_is_transparent`: a semantic marker that tells later stages to honor alpha-zero coverage in an alpha-bearing buffer or its separated mask.

Loader normalization does not require every frame to have exactly one storage shape. Some paths convert packed RGBA immediately to three-channel color plus a binary alpha-zero mask. Other backends can retain RGBA until a later normalization boundary. Indexed input can retain a transparent palette index until geometry processing requires promotion to color samples plus a mask.

The stable contract is semantic: fractional alpha must be resolved before SIXEL encoding, while exact alpha-zero coverage can be carried separately from color so that clipping, resizing, palette construction, and dithering do not accidentally paint it. Geometry operations transform the mask with the pixel plane.

## Alpha resolution

SIXEL cannot encode fractional alpha. When a background is resolved, loader paths composite fractional samples into color values. Depending on the [alpha policy](../loader/alpha-policy.md), exact alpha-zero samples are either also composited or retained as holes. Background source selection and colorspace are governed separately by the [background policy](../loader/background-policy.md).

When no background can be resolved, an exact alpha-zero sample can still be represented by an omitted SIXEL pixel. A nonzero fractional alpha value cannot remain fractional at the wire boundary.

## SIXEL wire representation

A SIXEL stream defines palette colors and a one-bit paint decision for each color plane. It does not carry RGB plus alpha, RGBA palette entries, or an alpha plane. An omitted bit means only that the current color is not painted at that position.

DCS `P2` supplies the terminal-side operation for omitted positions:

- `P2=0` requests background clearing;
- `P2=1` requests preservation of previous contents.

Those operations are not source-alpha blending and their visible result can vary between terminal implementations. The [SIXEL format guide](../sixel-format.md) defines the wire syntax; the [alpha policy](../loader/alpha-policy.md) defines how libsixel maps frame transparency to `P2` and omitted pixels.

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and an owning test. The reciprocal `Policy:` reference in each test is checked by `staticcheck-doc-test-links`.

| ID | Contract | Owning test |
| --- | --- | --- |
| PF-01 | The builtin loader separates RGBA alpha-zero coverage into an RGB frame plus a transparent mask when no background is resolved. | [tests/loader/0063_loader_builtin_rgba_alpha_mask.c](../../tests/loader/0063_loader_builtin_rgba_alpha_mask.c), [tests/loader/builtin/1932_loader_builtin_rgba_alpha_mask.t](../../tests/loader/builtin/1932_loader_builtin_rgba_alpha_mask.t) |
| PF-02 | Indexed transparent input remains `PAL8` without geometry processing; clipping promotes it to RGB work storage while preserving omitted transparent coverage. | [tests/loader/builtin/1935_loader_builtin_pal8_trns_stays_indexed.t](../../tests/loader/builtin/1935_loader_builtin_pal8_trns_stays_indexed.t), [tests/loader/builtin/1510_loader_builtin_pal8_trns_clip_promotes_rgb_mask.t](../../tests/loader/builtin/1510_loader_builtin_pal8_trns_clip_promotes_rgb_mask.t), [tests/loader/builtin/1936_loader_builtin_pal8_trns_clip_omits_pixel.t](../../tests/loader/builtin/1936_loader_builtin_pal8_trns_clip_omits_pixel.t) |
| PF-03 | Frame clipping applies the same coordinates to the transparent mask and retains alpha-zero metadata. | [tests/processing/frame/0003_frame_transparent_mask_clip.c](../../tests/processing/frame/0003_frame_transparent_mask_clip.c), [tests/processing/frame/0003_frame_transparent_mask_clip.t](../../tests/processing/frame/0003_frame_transparent_mask_clip.t) |
| PF-04 | Nearest-neighbor and bilinear frame mask resizing produce their exact expected binary masks and retain alpha-zero metadata. | [tests/processing/frame/0004_frame_transparent_mask_resize.c](../../tests/processing/frame/0004_frame_transparent_mask_resize.c), [tests/processing/frame/0004_frame_transparent_mask_resize.t](../../tests/processing/frame/0004_frame_transparent_mask_resize.t), [tests/processing/frame/0005_frame_transparent_mask_resize_bilinear.c](../../tests/processing/frame/0005_frame_transparent_mask_resize_bilinear.c), [tests/processing/frame/0005_frame_transparent_mask_resize_bilinear.t](../../tests/processing/frame/0005_frame_transparent_mask_resize_bilinear.t) |
| PF-05 | A loader backend may retain RGBA without a background and report RGB storage when a background is provided. | [tests/loader/librsvg/0023_loader_librsvg_pixelformat.c](../../tests/loader/librsvg/0023_loader_librsvg_pixelformat.c), [tests/loader/librsvg/0010_loader_librsvg_pixelformat.t](../../tests/loader/librsvg/0010_loader_librsvg_pixelformat.t), [tests/loader/librsvg/0033_loader_librsvg_bgcolor_pixelformat.c](../../tests/loader/librsvg/0033_loader_librsvg_bgcolor_pixelformat.c), [tests/loader/librsvg/0033_loader_librsvg_bgcolor_pixelformat.t](../../tests/loader/librsvg/0033_loader_librsvg_bgcolor_pixelformat.t) |
| PF-06 | Palette sampling excludes samples selected by an explicit transparent mask. | [tests/processing/filter/0036_filter_binning_stream_transparency.c](../../tests/processing/filter/0036_filter_binning_stream_transparency.c), [tests/processing/filter/0036_filter_binning_stream_transparency.t](../../tests/processing/filter/0036_filter_binning_stream_transparency.t) |
| PF-07 | A transparent mask fences serial dithering so masked samples are not painted or diffused into visible neighbors. | [tests/processing/filter/0043_filter_dither_transparent_mask_fence_serial.c](../../tests/processing/filter/0043_filter_dither_transparent_mask_fence_serial.c), [tests/processing/filter/0043_filter_dither_transparent_mask_fence_serial.t](../../tests/processing/filter/0043_filter_dither_transparent_mask_fence_serial.t) |
| PF-08 | Alpha-zero input produces no SIXEL body paint while `clear` selects DCS `P2=0` and `keep` selects DCS `P2=1`. | [tests/quant/palette/usage/0182_clear_policy_omits_alpha_zero.t](../../tests/quant/palette/usage/0182_clear_policy_omits_alpha_zero.t), [tests/quant/palette/usage/0167_keep_policy_omits_alpha_zero.t](../../tests/quant/palette/usage/0167_keep_policy_omits_alpha_zero.t) |
| PF-09 | A transparent mask applies the same fence to the parallel dither path. | [tests/processing/filter/0044_filter_dither_transparent_mask_fence_parallel.c](../../tests/processing/filter/0044_filter_dither_transparent_mask_fence_parallel.c), [tests/processing/filter/0044_filter_dither_transparent_mask_fence_parallel.t](../../tests/processing/filter/0044_filter_dither_transparent_mask_fence_parallel.t) |
| PF-10 | Palette sampling excludes exact alpha-zero samples only when the frame enables alpha-zero interpretation. | [tests/processing/filter/0042_filter_binning_stream_alpha_zero.c](../../tests/processing/filter/0042_filter_binning_stream_alpha_zero.c), [tests/processing/filter/0042_filter_binning_stream_alpha_zero.t](../../tests/processing/filter/0042_filter_binning_stream_alpha_zero.t), [tests/processing/filter/0045_filter_binning_stream_alpha_zero_disabled.c](../../tests/processing/filter/0045_filter_binning_stream_alpha_zero_disabled.c), [tests/processing/filter/0045_filter_binning_stream_alpha_zero_disabled.t](../../tests/processing/filter/0045_filter_binning_stream_alpha_zero_disabled.t) |

### Coverage boundary

The automated inventory covers representative packed, indexed, and mask representations; the separation of explicit masks from alpha-bearing storage; mask-preserving geometry; palette and dither consumers; and the final `P2` request. It does not claim that every loader backend returns the same storage shape, because backend-specific normalization boundaries intentionally differ. The serial and parallel dither observations are skipped by their TAP wrappers on Windows. The parallel observation is also skipped on builds without thread support; serial mask fencing remains covered on non-Windows builds without threads. Terminal rendering after the emitted DCS request is external to libsixel and remains covered by the alpha-policy portability boundary.
