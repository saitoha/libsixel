# Pixel Formats and Alpha Representation

## Scope

libsixel handles pixels at three distinct layers. Keeping them separate avoids
confusing an input memory layout with a SIXEL protocol capability:

1. a source file or caller-owned pixel buffer;
2. a `sixel_frame_t` used between loading, processing, and encoding;
3. the paletted SIXEL byte stream sent to a terminal.

An alpha channel may exist at the first two layers. It does not exist at the
third layer.

## In-memory pixel formats

The `SIXEL_PIXELFORMAT_*` constants describe how samples are arranged in a
memory buffer. They include:

- packed RGB and BGR formats, including 15-, 16-, 24-, and 32-bit layouts;
- alpha-bearing RGBA, ARGB, BGRA, and ABGR layouts;
- opaque XRGB, RGBX, XBGR, and BGRX layouts, where `X` is padding rather than
  alpha;
- grayscale and gray-alpha layouts;
- `PAL1`, `PAL2`, `PAL4`, and `PAL8` indices with a separate palette;
- internal three-channel float formats used for RGB, linear RGB, Oklab,
  CIELAB, and DIN99d processing.

The pixel-format tag describes layout. Colorspace metadata describes how the
stored color values are interpreted. Neither property by itself states how
the terminal will treat an unpainted SIXEL position.

## Frame representation

A frame owns or borrows a pixel buffer and can also carry a palette,
dimensions, colorspace, animation timing, and transparency metadata. The
transparency metadata has three relevant forms:

- `transparent`: a transparent palette index for indexed pixels;
- `transparent_mask`: a binary per-pixel mask whose marked samples must not be
  painted;
- `alpha_zero_is_transparent`: a semantic marker that tells later stages to
  honor alpha-zero coverage in an alpha-bearing buffer or its separated mask.

Loader normalization does not require every frame to have exactly one storage
shape. Some paths convert packed RGBA immediately to three-channel color plus
a binary alpha-zero mask. Other backends can retain RGBA until a later
normalization boundary. Indexed input can retain a transparent palette index
until geometry processing requires promotion to color samples plus a mask.

The stable contract is semantic: fractional alpha must be resolved before
SIXEL encoding, while exact alpha-zero coverage can be carried separately from
color so that clipping, resizing, palette construction, and dithering do not
accidentally paint it. Geometry operations transform the mask with the pixel
plane.

## Alpha resolution

SIXEL cannot encode fractional alpha. When a background is resolved, loader
paths composite fractional samples into color values. Depending on the
[alpha policy](../loader/alpha-policy.md), exact alpha-zero samples are either
also composited or retained as holes. Background source selection and
colorspace are governed separately by the
[background policy](../loader/background-policy.md).

When no background can be resolved, an exact alpha-zero sample can still be
represented by an omitted SIXEL pixel. A nonzero fractional alpha value cannot
remain fractional at the wire boundary.

## SIXEL wire representation

A SIXEL stream defines palette colors and a one-bit paint decision for each
color plane. It does not carry RGB plus alpha, RGBA palette entries, or an
alpha plane. An omitted bit means only that the current color is not painted
at that position.

DCS `P2` supplies the terminal-side operation for omitted positions:

- `P2=0` requests background clearing;
- `P2=1` requests preservation of previous contents.

Those operations are not source-alpha blending and their visible result can
vary between terminal implementations. The
[SIXEL format guide](../sixel-format.md) defines the wire syntax; the
[alpha policy](../loader/alpha-policy.md) defines how libsixel maps frame
transparency to `P2` and omitted pixels.
