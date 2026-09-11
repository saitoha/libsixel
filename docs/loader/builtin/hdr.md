# Builtin Radiance HDR Loader

## Identity and history

[`fromhdr.c`](../../../src/fromhdr.c) recognizes a case-insensitive `#?RADIANCE` or `#?RGBE` first line. HDR originally used stb-derived code, but the format's RGBE/XYZE values and header policy required a float-native boundary. Commit [`8517810cc`](https://github.com/saitoha/libsixel/commit/8517810cc) routed HDR through `fromhdr` on 2026-03-27 and [`4210a8a7e`](https://github.com/saitoha/libsixel/commit/4210a8a7e) completed the custom path on 2026-04-01. `STBI_NO_HDR` prevents fallback to the old stb decoder.

## Accepted stream forms

The header must contain either `FORMAT=32-bit_rle_rgbe` or `FORMAT=32-bit_rle_xyze`, followed by a blank line and a two-axis resolution/orientation line. All sign and axis-order combinations of the `X` and `Y` resolution notation are mapped into the output orientation. The raster decoder accepts the modern per-component scanline RLE form for widths 8 through 32767 and the legacy flat/old-RLE form for other valid streams.

RGBE samples become linear RGB radiance. XYZE samples are converted from CIE XYZ to linear sRGB coordinates, with non-finite and negative results clamped at the decode boundary. Output begins as `LINEARRGBFLOAT32`; an enabled CMS path can convert it to the configured typed float target.

## Header metadata

| Field | Treatment |
| --- | --- |
| `FORMAT` | Required and selects RGBE or XYZE interpretation. |
| `GAMMA` | Parsed as the source transfer/profile hint. |
| `PRIMARIES` | Parses white, red, green, and blue xy coordinates for source-profile synthesis. |
| Repeated `EXPOSURE` | Positive values are multiplied and optionally applied according to `hdr_header_exposure`. |
| Repeated `COLORCORR` | Per-channel positive multipliers are accumulated and applied. |
| `PIXASPECT` | Parsed and validated but does not resample or alter geometry. |
| `VIEW` | Checked as a nonempty header field but not interpreted as a camera transform. |
| Unknown line | Ignored. |

`hdr_fallback_profile` chooses linear-sRGB or sRGB interpretation when usable profile metadata is absent. `hdr_exposure` applies finite EV stops, and `hdr_tonemap` selects no tonemap or Reinhard. These controls operate on float radiance before palette construction; their complete CLI and environment spelling is in [`img2sixel(1)`](../../../converters/img2sixel.1).

The loader does not decode OpenEXR, PFM, TIFF LogLuv, arbitrary Radiance `FORMAT` values, alpha channels, or animations. It does not load an embedded ICC profile because the Radiance path has no interpreted ICC container. `VIEW` and `PIXASPECT` are not rendering instructions in this implementation.
