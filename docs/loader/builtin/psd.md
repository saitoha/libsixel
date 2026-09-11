# Builtin PSD and PSB Loader

## Format lineage

Adobe Photoshop stores an editable document graph as well as a flattened composite. PSD version 1 uses mostly 32-bit section lengths and conventional dimensions; PSB version 2 widens selected lengths and raises format limits for large documents. Adobe publishes the [Photoshop file formats specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/), but real files also contain versioned descriptor objects, legacy effect records, plugin keys, and application-specific extensions accumulated over decades.

The builtin family accepts Adobe's `8BPS` signature and the implementation's PSB-compatible `8BPB` alias. Commit [`2a743d4b2`](https://github.com/saitoha/libsixel/commit/2a743d4b2) extracted PSD decoding from stb_image on 2026-03-26; [`e0d479e64`](https://github.com/saitoha/libsixel/commit/e0d479e64) removed the old eight-bit-RGB-only fallback. Subsequent work added PSB, high-depth grayscale/RGB/CMYK/Lab, ZIP, ICC resources, transparency masks, missing-composite reconstruction, blend modes, vector/fill/effect descriptors, and stable diagnostics. `STBI_NO_PSD` disables the former stock flattened decoder.

## Document container and structural limits

The fixed 26-byte header contains signature, version, six reserved bytes, channel count, height, width, depth, and color mode. `8BPS` accepts versions 1 and 2; `8BPB` is accepted only with version 2. Reserved bytes must be valid, channel count is 1 through 56, and each dimension is 1 through 300000.

The remaining stream is a sequence of big-endian sections:

1. color-mode data, including the planar 768-byte palette required by Indexed mode;
2. image resources, each carrying a signature, numeric resource ID, Pascal name, padded length, and payload;
3. layer and mask information, containing layer records, per-channel payloads, masks, blending ranges, and additional-layer blocks;
4. merged/composite image compression and planar sample data.

PSD uses 32-bit section and channel lengths in the relevant structures. PSB uses 64-bit lengths for the layer/mask section and selected layer-channel data, and wider RLE row-length tables. The decoder follows version-specific widths and rejects truncation, overflow, section overlap, implausible record counts, and payloads that escape their enclosing section.

## Composite color, depth, and output matrix

PSD samples are planar: all samples for one channel are stored before the next channel. The meaning of those channels depends on the document color mode. The builtin accepts exactly this merged-image matrix:

| Color mode | Accepted depth and channel condition | Decode/output interpretation |
| --- | --- | --- |
| 0 Bitmap | 1 bit; at least one channel | Packed monochrome expanded to RGB. Compression 3 is rejected because ZIP prediction is undefined for this path. |
| 1 Grayscale | 8, 16, or 32 bits; at least one channel | Eight-bit gamma RGB, or precision-preserving float RGB for high depth. |
| 2 Indexed | 8 bits; at least one channel | Indices expanded through the required 256-entry planar RGB table to gamma `RGB888`. Source indices are not returned as `PAL8`. |
| 3 RGB | 8, 16, or 32 bits; at least three channels | Gamma `RGB888` at 8 bit or `RGBFLOAT32` on high-depth paths before applicable CMS. |
| 4 CMYK | 8, 16, or 32 bits; at least four channels | CMYK is converted while its components are available; high-depth work remains float. |
| 7 Multichannel | Exactly three channels as RGB or exactly four as CMYK; 8, 16, or 32 bits | Reuses the matching RGB/CMYK path. Other channel counts are rejected rather than guessed. |
| 8 Duotone | 8, 16, or 32 bits; at least one channel | Uses the grayscale sample path. Duotone ink definitions and transfer curves are not rendered. |
| 9 Lab | 8, 16, or 32 bits; at least three channels | `CIELABFLOAT32` before applicable Lab-to-target CMS conversion. |

Only numeric mode 8 enters the Duotone path. HSB/HSL, RGBE, and undocumented color modes are unsupported. A caller must use the matrix, not infer support from the `.psd` suffix.

The first usable extra channel after the minimum color channels may supply merged alpha. That alpha is carried through composition or extracted into the frame transparency representation according to policy; the stable frame is not universally interleaved RGBA. Additional spot channels are not exposed as independent output planes.

## Composite compression

| Compression | Merged-image behavior |
| --- | --- |
| 0 Raw | Reads each planar row without compression, using packed rows for Bitmap and big-endian words/floats for high depth. |
| 1 RLE | Uses PackBits independently per row and channel. PSD row byte counts are 16-bit; PSB counts are 32-bit. |
| 2 ZIP | Inflates the concatenated planar data and validates the exact decoded size. |
| 3 ZIP with prediction | Inflates, reverses byte-plane/delta prediction appropriate to depth, and reconstructs samples. Bitmap 1-bit rejects this mode. |

Eight-bit values are normalized to byte RGB where possible. Sixteen-bit unsigned samples are normalized without first truncating to eight bits. Thirty-two-bit channels are read as big-endian IEEE floats and bounded before color conversion. Malformed PackBits packets, incorrect row tables, invalid zlib output size, and prediction arithmetic overflow fail.

## ICC and image-resource treatment

The ICC profile image resource has ID `0x040f`. The loader applies it only when the profile's input domain matches the document path: RGB profiles to RGB, CMYK profiles while CMYK samples still exist, and Lab profiles to Lab. If CMS is disabled, the profile is skipped. If an applicable profile cannot be opened or transformed, format-specific device/fallback conversion remains rather than relabeling unconverted values as the requested target.

Other resources are bounded and skipped unless a decoder feature explicitly needs them. Thumbnails, guides, print settings, slices, XMP, URL lists, and arbitrary plugin resource payloads are not returned as public metadata. PSD does not pass through the common JPEG/PNG Exif scanner, so `builtin:orientation` has no effect on PSD/PSB.

## Missing-composite layer reconstruction

A normal PSD/PSB contains a merged image after the layer section. If that image is absent, or guarded structure/comparison logic selects the layer path, the builtin decoder can reconstruct a canvas from layer records. It parses layer rectangles, channel IDs and lengths, visibility, opacity, clipping, fill opacity, masks, vector masks, blending ranges, knockout flags, and per-layer raw/RLE/ZIP/ZIP-prediction channel payloads.

The compositor recognizes these 27 blend keys: Normal, Dissolve, Darken, Multiply, Color Burn, Linear Burn, Darker Color, Lighten, Screen, Color Dodge, Linear Dodge, Lighter Color, Overlay, Soft Light, Hard Light, Vivid Light, Linear Light, Pin Light, Hard Mix, Difference, Exclusion, Subtract, Divide, Hue, Saturation, Color, and Luminosity. The implementation owns its numerical formulas; recognition of a key does not promise pixel identity with every Photoshop version, color setting, or legacy blending option.

Selected additional-layer keys contribute reconstructed content:

| Key family | Implemented use |
| --- | --- |
| `SoCo`, `GdFl`, `PtFl` | Solid, gradient, and pattern-fill descriptor parsing for covered layer forms. |
| `TySh` | Extracts covered text/fill color information; it is not a general Photoshop font/layout engine. |
| `vmsk`, `vsms`, `vogk` | Vector-mask path records and covered geometry. |
| `vscg`, `vstk` | Vector content/stroke color, opacity, width, position, join, cap, miter, and blend information. |
| `lfx2`, `lrFX` | Covered object/legacy effects including selected solid/gradient overlay, stroke, glow, shadow, and bevel approximations. |
| `iOpa`, `clbl`, `infx`, `knko` | Fill opacity, clipped/interior blending, and knockout-related flags used by the covered compositor paths. |

Unknown additional-layer keys and unsupported descriptor classes are skipped after structural validation where the enclosing format permits that. The fallback does not rasterize arbitrary smart objects, reproduce every text run, execute adjustment layers or filters, resolve linked resources, implement every pattern/gradient/effect dialect, or guarantee Adobe-compositor parity. A usable merged composite remains authoritative where reconstruction lacks semantics.

## Builtin decode and output pipeline

1. [`frompsd-header.c`](../../../src/frompsd-header.c) validates signature, version, dimensions, color/depth/compression compatibility, section boundaries, and whether layer fallback is structurally possible.
2. The decoder extracts the applicable ICC resource and color-mode data before consuming planar samples.
3. It decodes the merged image with raw, PackBits, ZIP, or predicted ZIP, or enters the guarded layer reconstruction path when necessary.
4. Grayscale/RGB, CMYK, or Lab samples are converted at their native available precision. Applicable ICC work occurs before source components are discarded.
5. An extra alpha channel or reconstructed canvas alpha is normalized into background composition or the separate transparency representation.
6. One typed frame is delivered; there is no Photoshop timeline/animation playback.

Depending on mode and depth, the initial frame can be gamma `RGB888`, `RGBFLOAT32`, `LINEARRGBFLOAT32`, or `CIELABFLOAT32`, followed by a configured typed target after successful CMS. Even an alpha-bearing document is conceptually three color components plus transparency state at the stable boundary, not an unconditional four-component RGBA image.

## Options and observable differences

```console
img2sixel -Lbuiltin:cms_engine=auto:cms_target=linear:prefer_8bit=0! document.psd
img2sixel -x code:psd_trace=1:psd_header_only=1 -Lbuiltin! document.psb
```

| Control | Exact effect on PSD/PSB |
| --- | --- |
| `cms_engine=none|auto|builtin|lcms2|colorsync` | `none` disables ICC conversion. Other values select an available engine for domain-compatible RGB, CMYK, or Lab profiles. Device/fallback conversion remains for uncovered or failed transforms. |
| `cms_target=gamma|linear|cielab|oklab|din99d` | Selects the typed destination after a successful transform. It does not alter the document color-mode number. |
| `cms_intent=...` | Controls ordered rendering-intent attempts for applicable ICC transforms. |
| `prefer_8bit=0|1` | Requests byte storage where the CMS target/path permits. It does not force 16/32-bit source decode through the old eight-bit PSD path. |
| `-B`, `background_colorspace`, `-A auto|composite|clear|keep` | Control final handling of merged or reconstructed alpha. PSD does not provide the shared PNG/GIF file-background candidate, so `background_policy` has no competing source. |
| `-x ...:psd_trace=0|1` (`D0`/`D1`) | Enables PSD/PSB decode diagnostics. With tracing enabled for a PSD/PSB input, `img2sixel` can complete the diagnostic decode without generating SIXEL image output. `SIXEL_PSD_TRACE_ONLY` is the environment form; the explicit diagnostic suboption wins. |
| `-x ...:psd_header_only=0|1` (`E0`/`E1`) | When tracing, suppresses verbose PSD trace lines while preserving the stable `LSXPSD1` summary. `SIXEL_PSD_TRACE_HEADER_ONLY` is the environment form; the explicit diagnostic suboption wins. |
| `-x ...:trace_topic=psd_decode` or `SIXEL_TRACE_TOPIC=psd_decode` | Selects topic-scoped decode detail; this is diagnostic output, not a change to accepted pixels. |
| `builtin:orientation`, `-S`, `-T`, `-l`, `-g` | No effect. PSD is emitted as one image and Exif orientation is not interpreted by this path. |
| `-p COLORS`, resize, crop, sampling, and encoder color modes | Run after the typed PSD frame is returned. PSD Indexed mode is already expanded, so these controls do not recover its original indices. |
| PNG, HDR, PNM, BMP, and WebP-specific suboptions | No effect. |

## Unsupported behavior and security boundary

Unsupported color modes and depth combinations fail rather than being guessed. Duotone colorants are ignored, Multichannel counts other than exactly three or four are rejected, Bitmap plus ZIP prediction is rejected, spot channels are not exported, unknown resources are not preserved, linked content is not fetched, and document timelines/video are not decoded.

PSD/PSB is the builtin family's broadest attacker-controlled object graph: nested section lengths, layer counts, rectangles, channel compression, descriptor recursion, masks, vector records, effects, and large-document widths all interact. The reconstruction features improve compatibility but expand software attack surface far beyond a flattened stb decoder. `-Lbuiltin!` narrows fallback; it does not sandbox this parser or establish Photoshop-level conformance.

## Implementation landmarks

- Header, section graph, validation, and summary: [`frompsd-header.c`](../../../src/frompsd-header.c)
- Composite decode, color modes, layer reconstruction, blend/effect handling: [`frompsd.c`](../../../src/frompsd.c)
- Stable diagnostic buffering and codes: [`frompsd-trace.c`](../../../src/frompsd-trace.c)
- Builtin routing, CMS handoff, alpha, and frame delivery: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Alpha and typed-frame concepts: [Pixel Formats and Alpha Representation](../../concepts/pixelformat.md) and [Alpha Policy](../alpha-policy.md)
- Regression coverage: [`tests/loader/builtin`](../../../tests/loader/builtin)
