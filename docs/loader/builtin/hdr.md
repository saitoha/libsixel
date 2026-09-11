# Builtin Radiance HDR Loader

## Format lineage

Radiance RGBE was developed for the Radiance lighting-simulation system to store scene-referred radiance with substantially more dynamic range than integer RGB image formats. The format is commonly called Radiance HDR and conventionally uses `.hdr` or `.pic`, although recognition must use the stream rather than the suffix. Its canonical design notes are the Radiance [original picture format](https://radsite.lbl.gov/radiance/refer/Notes/picture_format.html).

Each color sample uses three mantissa bytes and one shared exponent byte. This compact representation gives a large dynamic range but much less precision than three independent floating-point channels, especially when one component is far smaller than the largest component. The later XYZE variant applies the same representation to CIE XYZ tristimulus values.

libsixel originally reached HDR through stb_image, which converted RGBE into a generic float buffer but did not provide the metadata, XYZE, orientation, or exposure policy needed by the loader architecture. Commit [`8517810cc`](https://github.com/saitoha/libsixel/commit/8517810cc) routed HDR through [`fromhdr.c`](../../../src/fromhdr.c) on 2026-03-27 and [`4210a8a7e`](https://github.com/saitoha/libsixel/commit/4210a8a7e) completed the custom float-native path on 2026-04-01. `STBI_NO_HDR` prevents fallback to the old stb decoder.

## Container and accepted encodings

The stream is an ASCII header followed by a binary raster. The first line is matched case-insensitively as `#?RADIANCE` or `#?RGBE`. Header variables continue until a blank line, after which a resolution/orientation line names two signed axes and their lengths.

Exactly these `FORMAT` values select a pixel interpretation:

| `FORMAT` value | Stored channels | Decode interpretation |
| --- | --- | --- |
| `32-bit_rle_rgbe` | R, G, B mantissas plus a shared exponent | Scene-linear RGB radiance. |
| `32-bit_rle_xyze` | X, Y, Z mantissas plus a shared exponent | CIE XYZ, converted to linear sRGB coordinates before later CMS work. |

The decoder accepts all valid `+X`, `-X`, `+Y`, and `-Y` axis directions and either axis order. It writes the returned raster in conventional top-to-bottom, left-to-right memory order; orientation is therefore part of raster decode, not Exif finalization.

Radiance has two raster coding families. The modern form stores four independently run-length-coded component streams per scanline and is used only when the scanline width is 8 through 32767. The builtin decoder also accepts flat RGBE/XYZE bytes and the legacy repeat marker used by old Radiance streams. It rejects malformed runs, scanline width mismatches, truncated components, invalid dimensions, and allocation-size overflow.

RGBE/XYZE has no alpha channel, palette, integer high-bit-depth variant, or built-in frame directory. A Radiance command can concatenate several pictures operationally, but this loader consumes one picture and emits one frame; it does not treat concatenated pictures as animation.

## Header variables and dialect treatment

| Header item | Builtin treatment |
| --- | --- |
| `FORMAT` | Required for the dedicated path and must be one of the two values above. |
| `GAMMA=value` | A positive finite value contributes the source transfer function used for profile synthesis. Repeated usable values replace the earlier value. |
| `PRIMARIES=xr yr xg yg xb yb xw yw` | Supplies RGB chromaticities and white point for source-profile synthesis. Every coordinate set is validated before use. |
| `EXPOSURE=value` | Positive finite values are multiplied together. The default loader policy divides decoded samples by that cumulative scale. |
| `COLORCORR=r g b` | Positive finite triples are multiplied component-wise. Decode always divides the corresponding output channels by the cumulative correction. |
| `PIXASPECT=value` | Parsed and validated for diagnostics, but does not resample or change geometry. |
| `VIEW=...` | Presence and basic validity are traced, but no camera/view transform is rendered. |
| Comment or unknown assignment | Skipped and not preserved. |

If only one of `GAMMA` and `PRIMARIES` is usable, profile synthesis fills the missing half with the corresponding sRGB default. The header is not an ICC container, and arbitrary Radiance variables do not become a general metadata object.

## Decode, color, and dynamic-range pipeline

The format predicate selects the HDR path before residual stb dispatch. `fromhdr` parses the header and resolution, decodes the raster directly into three float components, and normalizes invalid results. RGBE becomes linear RGB. XYZE becomes XYZ and is then matrix-converted to linear sRGB coordinates. Negative or non-finite components are clamped to zero.

When CMS is disabled, that linear decode result remains `LINEARRGBFLOAT32`. When CMS is enabled, the loader builds a source interpretation in this order:

1. usable header `GAMMA` and/or `PRIMARIES`, with missing pieces filled from sRGB;
2. `hdr_fallback_profile`, defaulting to `linear-srgb`, when usable header profile data is absent;
3. the decode-linear result if profile construction or application fails.

An applicable transform writes the configured typed CMS target: gamma RGB, linear RGB, CIELAB, OKLab, or DIN99d. HDR starts as float32 and is not reduced to an eight-bit source decoder merely because another path prefers byte storage.

After color interpretation, each channel is scaled as follows:

```text
output[c] = decoded[c] * 2^hdr_exposure / header_EXPOSURE / COLORCORR[c]
```

The `header_EXPOSURE` divisor is present only when `hdr_header_exposure=1`. `COLORCORR` compensation is independent of that switch. Overflowing or non-finite controls are ignored in favor of a safe unit scale. With `hdr_tonemap=none`, finite nonnegative values above 1.0 remain available to later float processing, up to float range. With `hdr_tonemap=reinhard`, each scaled component becomes `x / (1 + x)`.

This work occurs before encoder sampling, resize, crop, palette initialization, and quantization. A later encoder colorspace conversion or quantizer preprocessing is a separate stage and must not be mistaken for HDR decode.

### Implementation and test map

![A vertical implementation map of the builtin Radiance HDR loader. It follows signature and metadata parsing into resolution and orientation mapping, new-RLE, old-RLE, or legacy scanline decode, RGBE or XYZE expansion to linear RGB float, profile and dynamic-range postprocessing, and typed-frame output. Every node carries a coverage ID used by the tables below.](pipeline-figures/hdr.svg)

HDR is decoded directly to a three-component float buffer; there is no RGBA intermediate. The profile and dynamic-range passes are ordered operations on that same buffer, and `sixel_frame_set_pixelformat()` may then convert it to the configured CMS target. Tests therefore need to state both numeric values and the expected semantic pixel format.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `HDR-01` | `sixel_builtin_load_hdr_frame()` and `sixel_builtin_parse_hdr_profile_hint()` in [`fromhdr.c`](../../../src/fromhdr.c) | Radiance chunk → format/profile hint plus decode request | `FORMAT`, `GAMMA`, `PRIMARIES`, `EXPOSURE`, and `COLORCORR` parsing must preserve documented precedence and malformed-field fallback. |
| `HDR-02` | `sixel_builtin_hdr_parse_resolution_line()` and `sixel_builtin_hdr_map_scan_position()` in [`fromhdr.c`](../../../src/fromhdr.c) | Resolution axis declaration → canonical destination coordinates | Both axis orders and signs must map scan samples into width/height without silently rotating, mirroring, or overrunning the canvas. |
| `HDR-03` | `sixel_builtin_hdr_decode_new_rle_scanline()`, `sixel_builtin_hdr_decode_old_scanline()`, or `sixel_builtin_hdr_decode_legacy_stream()` in [`fromhdr.c`](../../../src/fromhdr.c) | Encoded four-channel scan data → RGBE/XYZE tuples | Width guards, run/literal counts, old repeat markers, truncation, and scanline selection must be validated before exponent expansion. |
| `HDR-04` | `sixel_builtin_hdr_rgbe_to_float()` and `sixel_builtin_hdr_xyz_to_linearrgb()` in [`fromhdr.c`](../../../src/fromhdr.c) | Exponent-coded tuple → finite nonnegative linear RGB float | Shared exponent reconstruction and the XYZE matrix must not be confused with a gamma RGB byte conversion. |
| `HDR-05` | `sixel_builtin_hdr_apply_source_profile()`, `sixel_builtin_hdr_apply_dynamic_range()`, and `sixel_builtin_hdr_apply_postprocess()` in [`fromhdr.c`](../../../src/fromhdr.c) | Linear RGB plus metadata/options → color-interpreted and range-mapped float | Header/fallback profile, exposure divisors, `COLORCORR`, EV, and optional Reinhard mapping must run in the documented order. |
| `HDR-06` | `sixel_builtin_hdr_assign_decoded_frame()` in [`fromhdr.c`](../../../src/fromhdr.c) | Float buffer plus target policy → typed one-frame object | CMS-off output stays `LINEARRGBFLOAT32`; an enabled target conversion must update pixel format/colorspace without losing frame ownership. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin:hdr_fallback_profile=linear-srgb:hdr_exposure=1.5:hdr_tonemap=reinhard! image.hdr
img2sixel -Lbuiltin:cms_engine=auto:cms_target=oklab:prefer_8bit=0! image.hdr
```

| Control | Exact effect on Radiance HDR |
| --- | --- |
| `hdr_fallback_profile=linear-srgb|srgb` (`Fvalue`) | Chooses the assumed source profile only when usable header gamma/primaries are absent. Default is `linear-srgb`. The `srgb` value interprets decoded numeric components through the sRGB transfer curve and can materially change midtones. |
| `hdr_exposure=EV` (`Xvalue`) | Multiplies all channels by `2^EV`. Default is `0`. The parser requires a finite number; a scale that overflows is replaced by 1. |
| `hdr_header_exposure=0|1` (`U0`/`U1`) | Default `1` divides by the product of valid header `EXPOSURE` values. `0` leaves those values informational. It does not disable `COLORCORR` compensation. |
| `hdr_tonemap=none|reinhard` (`Hvalue`) | Default `none` preserves HDR magnitude. `reinhard` compresses each channel independently after exposure and correction. |
| `cms_engine=none|auto|builtin|lcms2|colorsync` | `none` bypasses header/fallback profile conversion and retains linear RGB float. Other values select an available engine; failure falls back to decoded linear values. |
| `cms_target=gamma|linear|cielab|oklab|din99d` | Selects the destination representation when CMS application succeeds. |
| `cms_intent=...` | Controls ordered rendering-intent attempts for the applicable transform; it does not affect header parsing or exposure math. |
| `prefer_8bit=0|1` | Does not choose an eight-bit HDR decoder. The source and dynamic-range path remain float32; target storage follows the HDR/CMS implementation. |
| `-p COLORS`, resize, crop, sampling, and encoder colorspace options | Operate after the HDR loader has returned its float frame. They can cause later colorspace conversions but do not change RGBE/XYZE parsing. |
| `-B`, `background_policy`, `background_colorspace`, `-A` | No effect because Radiance HDR has no alpha or interpreted file background. |
| `builtin:orientation`, `-S`, `-T`, `-l`, `-g`, `trns_keycolor`, `pnm_compatibility`, `bmp_info40_mode`, PSD diagnostics | No effect. Axis orientation is already consumed by the HDR raster parser and the format is single-frame. |

Loader suboptions may also be supplied through their documented environment bindings; an explicit `-Lbuiltin:...` assignment wins. See [`img2sixel(1)`](../../../converters/img2sixel.1) for spelling and precedence.

## Unsupported behavior and security boundary

OpenEXR, PFM, TIFF LogLuv, arbitrary Radiance `FORMAT` values, alpha, embedded ICC profiles, and concatenated-picture animation are not supported. `VIEW` is not a request to render or reproject the scene, and `PIXASPECT` is not an automatic resize instruction. The shared-exponent representation cannot recreate precision that the source encoding already lost.

Header lengths, dimensions, scanline markers, and RLE counts are attacker-controlled. `-Lbuiltin!` prevents another loader from accepting a rejected stream but does not isolate this parser. Very large finite radiance values can also amplify later resize and quantization cost even when the byte stream itself is compact.

## Implementation landmarks

- Header, orientation, RGBE/XYZE, RLE, profile synthesis, and controls: [`fromhdr.c`](../../../src/fromhdr.c)
- Builtin recognition and frame delivery: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Shared colorspace model: [Color Spaces and Loader Color Management](../../concepts/colorspace.md)
- Precision behavior: [Pixel-format Precision](../../concepts/pixelformat-precision.md)
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| HDR-01 | Usable header profile metadata takes precedence over the configured fallback interpretation. | [tests/loader/builtin/0278_loader_builtin_hdr_cms_header_priority_numeric.t](../../../tests/loader/builtin/0278_loader_builtin_hdr_cms_header_priority_numeric.t) |
| HDR-02 | Every accepted Radiance axis-order/sign combination maps to the same canonical image. | [tests/loader/builtin/0671_loader_builtin_hdr_orientation_all_numeric.t](../../../tests/loader/builtin/0671_loader_builtin_hdr_orientation_all_numeric.t) |
| HDR-03 | An XYZE stream passes the native scanline decoder and reaches the expected numeric output. | [tests/loader/builtin/0670_loader_builtin_hdr_xyze_numeric.t](../../../tests/loader/builtin/0670_loader_builtin_hdr_xyze_numeric.t) |
| HDR-04 | The no-CMS, no-tonemap baseline exposes the expected linear float values rather than gamma bytes. | [tests/loader/builtin/0308_loader_builtin_hdr_numeric_target_linear_gamma_none_primaries_none_ev_0_tonemap_none_cms_none_fallback_linear_srgb.t](../../../tests/loader/builtin/0308_loader_builtin_hdr_numeric_target_linear_gamma_none_primaries_none_ev_0_tonemap_none_cms_none_fallback_linear_srgb.t) |
| HDR-05 | Multiple header `EXPOSURE` values participate in the documented dynamic-range scale. | [tests/loader/builtin/0534_loader_builtin_hdr_header_exposure_multi_numeric.t](../../../tests/loader/builtin/0534_loader_builtin_hdr_header_exposure_multi_numeric.t) |
| HDR-06 | A non-RGB CMS target changes the typed frame to DIN99d as requested. | [tests/loader/builtin/0276_loader_builtin_hdr_cms_target_din99d.t](../../../tests/loader/builtin/0276_loader_builtin_hdr_cms_target_din99d.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| HDR-90 | A malformed header exposure is ignored in favor of the safe unit default. | [tests/loader/builtin/0538_loader_builtin_hdr_invalid_header_exposure_defaults_to_unity_numeric.t](../../../tests/loader/builtin/0538_loader_builtin_hdr_invalid_header_exposure_defaults_to_unity_numeric.t) |

Coverage audit note: profile priority, orientation, XYZE, baseline linear values, exposure, and a typed target are owned. New scanline RLE, old scanline RLE, and legacy-stream decode are not yet separated into three reciprocal owners here, so a regression confined to one coding dialect could require the broader suite or a new focused test to localize.
