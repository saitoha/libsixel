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

### Implementation and test map

![A vertical implementation map of the builtin PSD and PSB loader. It follows document-header and section validation into native-depth planar decompression, color-mode dispatch, the guarded missing-composite layer fallback, embedded ICC conversion, and alpha or mask aware typed-frame finalization. Every node carries a coverage ID used by the tables below.](pipeline-figures/psd.svg)

The composite and layer-reconstruction routes converge only after they have independently decoded native-depth channels and interpreted the document color mode. The layer fallback box represents a substantial subgraph—records, masks, fills, supported effects, clipping, blend, and deferred effects—not a call to a general Photoshop renderer. The table names the stable top-level anchors from which those format-specific helpers can be followed.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `PSD-01` | `sixel_builtin_parse_psd_info()` and `sixel_builtin_validate_psd_info()` in [`frompsd-header.c`](../../../src/frompsd-header.c) | PSD/PSB bytes → validated section windows and document model | Version-dependent lengths, dimensions, depth/mode/channel count, resources, composite bounds, and layer-fallback eligibility must be checked before decode. |
| `PSD-02` | `sixel_builtin_decode_psd_8bit_channel()`, `sixel_builtin_decode_psd_16bit_channel()`, and `sixel_builtin_decode_psd_32bit_channel()` in [`frompsd.c`](../../../src/frompsd.c) | Raw/RLE/ZIP/predicted-ZIP planes → native-precision component buffers | Row tables, PackBits runs, inflate size, prediction reversal, and endian float/sample conversion must agree for PSD and PSB. |
| `PSD-03` | `sixel_builtin_psd_lookup_basic_decode_fn()` and `sixel_builtin_psd_decode_cmyk_by_mode()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Mode/depth-specific component planes → RGB, linear RGB, or CIELAB pixels | Bitmap, Gray/Duotone, Indexed, RGB, CMYK/Multichannel, and Lab must retain the documented precision and fallback semantics. |
| `PSD-04` | `sixel_builtin_decode_psd_multilayer_missing_composite()` and `sixel_builtin_psd_composite_layer_over()` in [`frompsd.c`](../../../src/frompsd.c) | Layer/mask/resource graph → reconstructed linear canvas | Only supported pixel/fill/effect semantics may enter the compositor; ordering, clipping, masks, alpha, blend, and deferred effects must remain explicit. |
| `PSD-05` | `sixel_builtin_psd_apply_embedded_icc()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Applicable resource ICC plus decoded mode components → configured target color | Profile colorspace must match the source model; conversion must occur before CMYK/Lab components are irreversibly collapsed to fallback RGB. |
| `PSD-06` | `sixel_builtin_load_psd_single_frame()` and `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Decoded pixels plus alpha/mask → one typed frame | Pixel storage kind, colorspace, background composition, separate transparency mask, and ownership must match the selected decode/CMS route. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

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
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [PSD/PSB coverage inventory](../../testing/builtin-loader-coverage.md#psd-and-psb) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| PSD-02 | The 16-bit ZIP-prediction and 32-bit PackBits paths both produce gamma `RGBFLOAT32` frames with fixed representative samples. | [tests/loader/builtin/1984_loader_builtin_psd_rgb16_zip_pred_numeric.t](../../../tests/loader/builtin/1984_loader_builtin_psd_rgb16_zip_pred_numeric.t), [tests/loader/builtin/1985_loader_builtin_psd_rgb32_rle_numeric.t](../../../tests/loader/builtin/1985_loader_builtin_psd_rgb32_rle_numeric.t) |
| PSD-04 | A missing merged composite is reconstructed from offset RGB8 layers with normal blend, producing the fixed complete RGB buffer. | [tests/loader/builtin/1964_loader_builtin_psd_missing_composite_rgb8_digest.t](../../../tests/loader/builtin/1964_loader_builtin_psd_missing_composite_rgb8_digest.t) |
| PSD-05 | Builtin ICC conversion for an RGB8 document produces the fixed complete RGB buffer. | [tests/loader/builtin/1979_loader_builtin_psd_icc_digest.t](../../../tests/loader/builtin/1979_loader_builtin_psd_icc_digest.t) |

### Quality regression tests

| ID | Regression observation protected | Owning test |
| --- | --- | --- |
| PSDQ-01 | A 16-bit Multichannel/RGB-mode ZIP-prediction stream retains its end-to-end visual floor. | [tests/loader/builtin/0597_loader_builtin_psd_mode7_rgb16_zip_pred_decode.t](../../../tests/loader/builtin/0597_loader_builtin_psd_mode7_rgb16_zip_pred_decode.t) |
| PSDQ-02 | A 32-bit mode-7 RGB RLE stream retains its end-to-end visual floor. | [tests/loader/builtin/0580_loader_builtin_psd_mode7_rgb32_rle_decode.t](../../../tests/loader/builtin/0580_loader_builtin_psd_mode7_rgb32_rle_decode.t) |
| PSDQ-03 | Missing-composite RGB8 reconstruction remains visually close to the external reference after encoding. | [tests/loader/builtin/0647_loader_builtin_psd_missing_composite_rgb8_multilayer_normal_decode.t](../../../tests/loader/builtin/0647_loader_builtin_psd_missing_composite_rgb8_multilayer_normal_decode.t) |
| PSDQ-04 | Embedded ICC conversion remains visually close to the fixed converted PNM after encoding. | [tests/loader/builtin/0057_builtin_psd_embedded_icc_matches_reference_pnm.t](../../../tests/loader/builtin/0057_builtin_psd_embedded_icc_matches_reference_pnm.t) |
| PSDQ-05 | A 16-bit Gray/Duotone alpha document produces observably different encoded images over black and white backgrounds. | [tests/loader/builtin/0173_loader_builtin_psd_gray_duotone_16bit_alpha_bgcolor_composite.t](../../../tests/loader/builtin/0173_loader_builtin_psd_gray_duotone_16bit_alpha_bgcolor_composite.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| PSD-01 | A document whose mode/depth contract has the wrong channel count is rejected by structural validation. | [tests/loader/builtin/0177_loader_builtin_psd_spec_wrong_channel_count_reject.t](../../../tests/loader/builtin/0177_loader_builtin_psd_spec_wrong_channel_count_reject.t) |
| PSD-90 | A signature/version mismatch between PSD and PSB is rejected rather than changing length-field interpretation. | [tests/loader/builtin/0778_loader_builtin_psd_signature8bpb_version1_reject_trace.t](../../../tests/loader/builtin/0778_loader_builtin_psd_signature8bpb_version1_reject_trace.t) |

Coverage audit note: direct owners now fix representative typed samples for 16-bit ZIP prediction and 32-bit RLE, complete RGB buffers for builtin ICC and one RGB8 layer reconstruction, and selected structural failures. Exact alpha composition remains covered only relationally, alongside an incomplete mode × depth × compression × PSD/PSB matrix and non-exhaustive masks, fills, text, vector data, blend modes, and effects coverage.
