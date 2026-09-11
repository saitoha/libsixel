# Builtin PNG and APNG Loader

## Format lineage

PNG was designed in the mid-1990s as a patent-free, lossless network raster format with indexed, grayscale, truecolor, alpha, color-management metadata, streaming, and integrity checks. It became a W3C Recommendation in 1996. APNG later extended the chunk language with animation while preserving a useful static fallback, and animation is now included in the [PNG Third Edition specification](https://www.w3.org/TR/png-3/).

libsixel's current builtin path is intentionally hybrid. [`frompng.c`](../../../src/frompng.c) owns high-depth decode, color metadata, source background policy, and reusable PNG helpers; [`loader-builtin.c`](../../../src/loader-builtin.c) owns APNG sequencing and selected indexed fast paths; adapted PNG and inflate machinery remains in [`stb_image.h`](../../../src/stb_image.h). Commit [`51bfc7f69`](https://github.com/saitoha/libsixel/commit/51bfc7f69) added builtin APNG in 2026. Commit [`ca38e5e4a`](https://github.com/saitoha/libsixel/commit/ca38e5e4a) introduced a precision-preserving 16-bit path, and [`44045c67b`](https://github.com/saitoha/libsixel/commit/44045c67b) extracted the dedicated module.

## Container, image types, and compression

A PNG begins with the fixed eight-byte signature. It then stores typed chunks as a big-endian length, four-byte type, payload, and CRC. `IHDR` defines width, height, bit depth, color type, compression, filter, and interlace; `IDAT` chunks form one zlib stream; `IEND` terminates the image.

| Color type | Source model | Accepted bit depths |
| --- | --- | --- |
| 0 | Grayscale | 1, 2, 4, 8, 16 |
| 2 | RGB truecolor | 8, 16 |
| 3 | Indexed through `PLTE` | 1, 2, 4, 8 |
| 4 | Grayscale plus alpha | 8, 16 |
| 6 | RGB plus alpha | 8, 16 |

Compression method 0 is supported: scanlines are filtered and compressed by zlib/DEFLATE. The decoder reverses filter types None, Sub, Up, Average, and Paeth, then reconstructs either ordinary rows or all seven Adam7 passes. Indexed samples are unpacked most-significant bits first and checked against `PLTE`.

The adapted path also recognizes Apple's private `CgBI` form and performs its channel-order and unpremultiplication compatibility work. That support is narrow: it does not turn every Apple-private chunk or malformed iPhone PNG variant into part of the PNG contract.

## Chunk semantics and extensions

| Chunk | Treatment |
| --- | --- |
| `IHDR`, `PLTE`, `IDAT`, `IEND` | Parsed as core image structure with ordering and size checks. |
| `tRNS` | Palette alpha or grayscale/RGB transparent key according to color type. |
| `bKGD` | File-supplied background candidate, resolved against `-B` by background policy. |
| `iCCP` | Embedded ICC source profile; compressed profile data is bounded and inflated before CMS use. |
| `sRGB` | Declares the standard sRGB interpretation and takes precedence over lower-level gamma/chromaticity fallback. |
| `cHRM` and `gAMA` | Supply chromaticities and transfer information when a higher-priority usable declaration is absent. |
| `eXIf` | Scanned for Exif orientation when builtin orientation is enabled. Other Exif tags are not exposed. |
| `acTL`, `fcTL`, `fdAT` | Select and describe APNG animation as detailed below. |
| Unknown ancillary chunks | Bounds-checked and skipped. |
| Unknown critical chunks | Rejected. |

The color interpretation order is usable `iCCP`, then `sRGB`, then the applicable `cHRM`/`gAMA` combination. Malformed or non-applicable metadata follows the format path's fallback rules rather than becoming a generic metadata object. Text chunks, `pHYs`, `tIME`, and HDR signaling such as `cICP`, `mDCv`, and `cLLi` have no semantic effect in the current decoder.

PNG specifies CRC validation, but these in-memory builtin paths currently read past stored CRC fields without verifying them. Successful decode therefore proves structural acceptance by this parser, not checksum authentication.

## Static decode pipeline and output forms

The dispatcher recognizes the PNG signature, probes for `acTL`, and chooses animation or static decode. Static decode first inspects depth, color, `bKGD`, `tRNS`, and color metadata, then chooses one of several paths instead of forcing every source through RGBA8.

| Condition | Possible initial frame |
| --- | --- |
| Indexed source, palette fusion enabled, palette fits the requested count, and no policy requires a different representation | Gamma `PAL8` plus an RGB palette and optional single transparent key. Multiple fully transparent palette entries are remapped to that one key. |
| Opaque eight-bit grayscale/RGB | Gamma `RGB888`. |
| Eight-bit alpha or transparent-key source not retained as indexed | RGBA is decoded and then normalized to RGB plus the frame's transparency representation or background composition. |
| Sixteen-bit opaque source | `RGBFLOAT32`, retaining normalized 16-bit sample precision. |
| Sixteen-bit alpha or background composition | Float processing; composition is performed in linear light and may produce `LINEARRGBFLOAT32`. |
| Successful color-management path | The configured gamma, linear, CIELAB, OKLab, or DIN99d typed target, with byte output only where the target/path permits and `prefer_8bit=1`. |

Indexed palette conversion happens before later resize and quantization. If the source palette exceeds `-p COLORS`, or scaling/non-default color mode disables palette fusion, the decoder expands it. `trns_keycolor=1` is a separate optimization for non-indexed `tRNS`: it is used only when there is no resolved background and no CMS operation that requires true color samples; otherwise the ordinary alpha/float path wins.

## APNG pipeline

`acTL` declares frame count and play count. Each `fcTL` gives a canvas sub-rectangle, delay numerator/denominator, disposal, and blend operator. `fdAT` carries frame zlib bytes after a sequence number; the default image can instead use `IDAT`. The implementation checks sequence numbers, declared frame count, chunk order, rectangle overflow, and canvas bounds.

Each frame is reconstructed as a synthetic PNG using the shared `IHDR`, palette, transparency, background, and color metadata, then composited onto an RGBA canvas:

| APNG field | Behavior |
| --- | --- |
| Blend 0 (`SOURCE`) | Replace the frame rectangle with decoded source pixels. |
| Blend 1 (`OVER`) | Alpha-composite source over the existing canvas. |
| Dispose 0 (`NONE`) | Keep the composed canvas for the next frame. |
| Dispose 1 (`BACKGROUND`) | Clear the frame rectangle after emission. |
| Dispose 2 (`PREVIOUS`) | Restore the saved pre-frame canvas after emission. |

The delay denominator defaults to 100 when encoded as zero. Delay is converted to libsixel's centisecond frame unit, so finer fractions are truncated by that boundary. A default image not participating in the animation remains the static fallback defined by APNG structure; invalid attempts to mix frame controls and data are rejected.

### Implementation and test map

![A vertical implementation map of the builtin PNG and APNG loader. It follows static-versus-animation classification into indexed and non-indexed raster paths, profile and transfer conversion, APNG frame reconstruction and canvas composition, then common alpha, orientation, and typed-frame finalization. Every node carries a coverage ID used by the tables below.](pipeline-figures/png.svg)

The static branches are alternatives, not a forced trip through each box: an indexed image may finish through `sixel_builtin_try_load_indexed_png()`, while non-indexed and high-depth images enter `sixel_frompng_load_nonindexed()`. APNG reconstructs a legal frame PNG and reuses those decode/color paths before it updates the animation canvas.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `PNG-01` | `sixel_builtin_load_stbi_png_path()` and `sixel_builtin_chunk_has_apng_control()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | PNG chunk → static path or APNG state machine | `acTL` controls animation routing; `-S` may request one emitted frame without changing the structural classification. |
| `PNG-02` | `sixel_builtin_try_load_indexed_png()` and `sixel_builtin_load_png_keycolor_or_rgba()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | PLTE/tRNS raster → `PAL8`, key-color, or RGBA branch | Palette size, fusion policy, CMS/background needs, and multiple transparent entries must select a representable output. |
| `PNG-03` | `sixel_frompng_load_nonindexed()` in [`frompng.c`](../../../src/frompng.c), backed by the adapted inflate/filter code in [`stb_image.h`](../../../src/stb_image.h) | Compressed scanlines → canonical byte or float RGB(A) samples | DEFLATE, PNG filters, Adam7 ordering, source depth, and alpha expansion must preserve numeric sample precision. |
| `PNG-04` | `sixel_frompng_build_profile_from_chunks()` and `sixel_frompng_apply_colorspace_fallback_internal()` in [`frompng.c`](../../../src/frompng.c) | Decoded samples plus iCCP/sRGB/cHRM/gAMA/bKGD → transformed pixels/background | Metadata precedence and source transfer must be applied in the same precision and colorspace as alpha/background composition. |
| `PNG-05` | `sixel_builtin_apng_process_chunk()`, `sixel_builtin_apng_blend_rect()`, and `sixel_builtin_apng_emit_pending_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | acTL/fcTL/IDAT/fdAT stream → completed animation canvases | Sequence numbers, frame rectangles, shared chunks, blend/dispose, timing, loop count, and pre-roll must remain coherent. |
| `PNG-06` | `sixel_builtin_finalize_frame_callback()` and `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Format-owned temporary frame → public typed frame callback | Alpha must become composition or a separate transparency representation, and Exif orientation must rotate pixels and mask together. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin:trns_keycolor=1! image.png
img2sixel -Lbuiltin:cms_engine=auto:cms_target=linear:prefer_8bit=0! image.png
```

| Control | Exact effect on PNG/APNG |
| --- | --- |
| `builtin:cms_engine=none|auto|builtin|lcms2|colorsync` | `none` disables embedded/profile-derived conversion. Other values select an available engine; `auto` follows the project CMS priority. |
| `cms_target=gamma|linear|cielab|oklab|din99d` | Selects the semantic destination only when a source color transform runs. It does not relabel an unprofiled byte PNG by itself. |
| `prefer_8bit=0|1` | Requests byte output from loader CMS where that target/path supports it. It does not reduce a 16-bit untransformed source merely because the value is 1. |
| `cms_intent=...` | Sets the ordered rendering-intent attempts for applicable ICC transforms; its inner `!` closes only intent fallback. |
| `builtin:orientation=0|1` | Enables or disables `eXIf` orientation. It runs after decode, so width/height and masks rotate together. |
| `trns_keycolor=0|1` (`K0`/`K1`) | Enables the guarded non-indexed `tRNS` keycolor path described above. Default is `1`; it does not override background or CMS requirements. |
| `-B`, `background_policy`, `background_colorspace` | Resolve `bKGD` versus explicit background and define the explicit color's transfer interpretation before composition. |
| `-A auto|composite|clear|keep` | Controls final treatment of zero-alpha pixels after PNG/APNG decode. A resolved background may already have flattened partial alpha. |
| `-p COLORS`, resize options, and non-default encoder color modes | Affect whether indexed `PAL8` can be fused through the loader. Resize disables that fast path because resampling requires component pixels. |
| `-S`, `-T`, `-l`, `-g` | Apply only to APNG. Their start-frame, loop, static, and presentation-delay meanings match the GIF description; prior frames may be decoded for canvas state. |
| HDR, PNM, and BMP-specific suboptions | No effect. |

## Unsupported behavior and security boundary

MNG and JNG are separate formats and are not decoded. Unknown critical chunks, invalid standard depth/type combinations, inconsistent APNG sequence graphs, and out-of-canvas frames fail. Arbitrary per-frame color profiles and general ancillary metadata preservation are not implemented.

PNG combines attacker-controlled chunk lengths, DEFLATE expansion, filters, interlace passes, profiles, and animation canvases. CRC skipping makes bounds validation especially important and means another integrity layer is required when checksums are part of the trust model. `-Lbuiltin!` prevents a fallback decoder from accepting a rejected dialect but does not isolate this parser.

## Implementation landmarks

- High-depth/static decode and color metadata: [`frompng.c`](../../../src/frompng.c)
- APNG, indexed routing, orientation, and finalization: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Adapted inflate and CgBI helpers: [`stb_image.h`](../../../src/stb_image.h)
- Alpha/background semantics: [Alpha Policy](../alpha-policy.md) and [Background Policy](../background-policy.md)
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| PNG-01 | `-S` selects the APNG static behavior without treating the file as an unrelated residual format. | [tests/loader/builtin/0017_apng_builtin_static_option.t](../../../tests/loader/builtin/0017_apng_builtin_static_option.t) |
| PNG-02 | Indexed tRNS uses the documented default key-color path when its palette representation is valid. | [tests/loader/builtin/0143_loader_builtin_png_trns_keycolor_default_enabled_for_palette.t](../../../tests/loader/builtin/0143_loader_builtin_png_trns_keycolor_default_enabled_for_palette.t) |
| PNG-03 | A 16-bit RGBA source follows the high-depth alpha/key-color decision rather than silently collapsing into the ordinary eight-bit path. | [tests/loader/builtin/0166_loader_builtin_trns_keycolor_optin_changes_rgba16_output.t](../../../tests/loader/builtin/0166_loader_builtin_trns_keycolor_optin_changes_rgba16_output.t) |
| PNG-04 | Embedded ICC conversion matches a reference PNM through the format color-management path. | [tests/loader/builtin/0055_builtin_png_embedded_icc_matches_reference_pnm.t](../../../tests/loader/builtin/0055_builtin_png_embedded_icc_matches_reference_pnm.t) |
| PNG-05 | APNG disposal `PREVIOUS` restores the saved canvas before the next emitted frame. | [tests/loader/builtin/0033_apng_builtin_dispose_previous.t](../../../tests/loader/builtin/0033_apng_builtin_dispose_previous.t) |
| PNG-06 | Enabling and disabling eXIf orientation changes geometry/pixels at common frame finalization as documented. | [tests/loader/builtin/1668_loader_builtin_png_orientation_toggle.t](../../../tests/loader/builtin/1668_loader_builtin_png_orientation_toggle.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| PNG-90 | An `fcTL` placed after image data in an invalid structural position is rejected. | [tests/loader/builtin/0035_apng_builtin_invalid_fctl_after_idat.t](../../../tests/loader/builtin/0035_apng_builtin_invalid_fctl_after_idat.t) |

Coverage audit note: these owners cover the major representation, CMS, orientation, and APNG-disposal boundaries, not every PNG filter × Adam7 pass × depth/color-type combination or every APNG ordering failure. CRC validation is not an uncovered promised behavior: the implementation deliberately does not validate stored CRCs, as documented above.
