# Builtin BMP Loader

## Format lineage

The BMP file format wraps a device-independent bitmap (DIB) header, optional masks or palette, and raster data. It evolved through OS/2 Presentation Manager and successive Windows DIB generations rather than through one immutable specification. Newer Windows headers added explicit channel masks, calibrated color endpoints, alpha, ICC profiles, and embedded JPEG/PNG payloads; OS/2 assigned different meanings to some of the same numeric fields. Microsoft's [Bitmap Storage](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-storage) and [BITMAPV5HEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv5header) references describe the Windows branch.

libsixel originally used stb_image's compact Windows-oriented BMP decoder. Commit [`379c90cf5`](https://github.com/saitoha/libsixel/commit/379c90cf5) replaced it with [`frombmp.c`](../../../src/frombmp.c) on 2026-04-10 so the OS/2 ambiguity, compression families, masks, CMYK, nested codecs, alpha, and color profiles could be validated explicitly. `STBI_NO_BMP` prevents silent fallback to the old stock path.

## File container and DIB families

Recognition requires the two-byte `BM` signature. The builtin component does not accept the other historical OS/2 bitmap-file signatures (`BA`, `CI`, `CP`, `IC`, or `PT`) and does not parse an ICO/CUR directory. The 14-byte bitmap file header gives the declared file size and pixel-data offset; the following little-endian DIB size chooses a layout.

| DIB size | Builtin family and interpretation |
| --- | --- |
| 12 | OS/2 1.x `BITMAPCOREHEADER`; 16-bit unsigned dimensions and three-byte palette entries. |
| 16, 24, 32 | OS/2 short headers accepted with only fields present in the selected size. |
| 40 | Windows `BITMAPINFOHEADER` or an OS/2-compatible 40-byte dialect selected by `bmp_info40_mode`. |
| 52 | Windows V2 with RGB masks. |
| 56 | Windows V3 with RGBA masks. |
| 64 | OS/2 2.x header. |
| 108 | Windows V4 with color-space endpoints and gamma. |
| 124 | Windows V5 with intent and embedded/linked profile fields. |

Exactly one plane is required. Width must be positive. A positive height is bottom-up; a negative height is top-down and is accepted only with Windows `BI_RGB` or `BI_BITFIELDS`. Uncompressed rows are padded to a four-byte boundary. CORE palettes store BGR triples; INFO-family palettes use four-byte entries whose reserved byte is not automatically an alpha channel.

For the ambiguous 40-byte DIB, `auto` chooses Windows unless the bpp/compression pair is one of the OS/2-only signatures used here: compression 3 with 1 bpp, or compression 4 with 24 bpp. `windows` and `os2` force the field namespace and therefore determine whether the same numeric compression value means Windows bitfields/embedded JPEG or OS/2 Huffman/RLE24.

## Depth and compression matrix

The parser accepts 1, 2, 4, 8, 16, 24, and 32 bits per pixel only in combinations valid for the selected family and compression. Palette indices are unpacked and checked against the available palette.

| Compression value | Family | Accepted content and decode behavior |
| --- | --- | --- |
| 0 `BI_RGB` | Windows/OS/2 | Palette 1/2/4/8 bpp, packed 16 bpp, BGR24, or 32-bit storage. Rows are DWORD-aligned. |
| 1 `BI_RLE8` | Windows/OS/2 | 8-bit indexed encoded/absolute runs, end-of-line, end-of-bitmap, and delta escapes. |
| 2 `BI_RLE4` | Windows/OS/2 | 4-bit indexed alternating-nibble runs and padded absolute blocks plus the same escapes. |
| 3 `BI_BITFIELDS` | Windows | 16- or 32-bit packed pixels using validated non-overlapping RGB masks and an optional alpha mask where the header supplies one. |
| 6 `BI_ALPHABITFIELDS` | Windows | 16- or 32-bit packed pixels with an explicit valid alpha mask; headers lacking alpha-mask storage are rejected. |
| 4 `BI_JPEG` | Windows | The validated payload slice is delegated to the builtin JPEG decoder. |
| 5 `BI_PNG` | Windows | The validated payload slice is delegated to the builtin PNG decoder. |
| 11 `BI_CMYK` | Windows | Uncompressed 32-bit CMYK. |
| 12 `BI_CMYKRLE8` | Windows | CMYK palette/index coding through the RLE8 grammar. |
| 13 `BI_CMYKRLE4` | Windows | CMYK palette/index coding through the RLE4 grammar. |
| 3 Huffman 1D | OS/2 | 1-bit rows using CCITT T.4 one-dimensional modified-Huffman runs. |
| 4 RLE24 | OS/2 | 24-bit BGR run/raw packets and OS/2 escapes. |

Default packed 16-bit `BI_RGB` is interpreted as 5:5:5. Explicit bitfields may represent other contiguous component widths; masks are normalized to eight-bit components. Invalid overlap, out-of-range masks, illegal escape motion, row overrun, truncated padding, palette overrun, or inconsistent wrapper dimensions fail.

## Alpha and color semantics

Palette and opaque direct rasters become gamma `RGB888`. A valid explicit alpha mask creates gamma `RGBA8888` and then enters the shared alpha finalizer. Thirty-two-bit `BI_RGB` is not blindly treated as BGRA: files frequently leave the fourth byte reserved or all zero, so the parser distinguishes explicit alpha contracts and applies its all-zero/meaningful-alpha rules instead of making ordinary images transparent.

CMYK data stays CMYK until conversion. If CMS is enabled and a bounded embedded profile is applicable to CMYK input, it converts to RGB through that profile; otherwise the builtin device conversion is used. The output is gamma `RGB888`, or the configured target pixelformat after a successful CMS path.

V4 `LCS_CALIBRATED_RGB` endpoints and per-channel gamma values can synthesize a source profile. V5 `PROFILE_EMBEDDED` bytes are bounds-checked and used only when their profile domain matches the decoded source. V5 `PROFILE_LINKED` is deliberately ignored: the builtin parser never opens a path supplied by an untrusted image. Rendering intent and other header metadata are not exposed as a generic metadata object.

## Embedded JPEG and PNG

For `BI_JPEG` and `BI_PNG`, `frombmp` validates the wrapper and returns the exact nested payload. The builtin loader constructs an in-memory chunk and calls its JPEG or PNG family rather than treating the bytes as ordinary BMP pixels. Wrapper width and height must agree with the nested result.

This delegation matters for precision and dialect support. A 16-bit embedded PNG can retain the PNG float path, PNG transparency/background/CMS rules remain PNG rules, and a supported high-precision JPEG can use the JPEG float path. Conversely, a PNG/JPEG variant rejected by the nested builtin component is not made valid by placing it in BMP. APNG and animated semantics are not promoted into BMP animation; the wrapper produces one image.

## Builtin decode pipeline and output forms

1. The dispatcher recognizes `BM`, probes DIB family, masks, compression, profile, and embedded-payload bounds, and applies `bmp_info40_mode`.
2. Embedded JPEG/PNG is delegated as described above. Native rasters enter the format-specific uncompressed, RLE, Huffman, RLE24, bitfield, or CMYK decoder.
3. Indexed and packed values are expanded, rows are placed according to bottom-up/top-down orientation, and explicit alpha is retained only where the format contract supports it.
4. Applicable calibrated RGB or embedded ICC conversion runs while the relevant source components still exist. CMYK otherwise uses the device fallback.
5. Shared alpha/background finalization converts RGBA into the frame's color-plus-mask/key representation or composites it. The result is delivered as one frame.

Possible initial results include gamma `RGB888`, gamma `RGBA8888` before finalization, a configured typed CMS target, or any higher-precision representation returned by an embedded PNG/JPEG path. BMP does not preserve its source index plane as loader `PAL8` in the current dedicated path.

### Implementation and test map

![A vertical implementation map of the builtin BMP loader. It follows BMP and DIB probing into native raster, RLE or OS/2 compressed, and embedded JPEG or PNG branches, then calibrated or embedded-profile color conversion and explicit-alpha finalization. Every node carries a coverage ID used by the tables below.](pipeline-figures/bmp.svg)

The compressed and nested-payload boxes are alternatives. Native BMP compression stays in `frombmp.c`; `BI_JPEG` and `BI_PNG` slice a child chunk and deliberately re-enter the JPEG/PNG decoders, so their precision, CMS, and alpha rules are inherited from the child format before BMP finalization resumes.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `BMP-01` | `sixel_frombmp_probe()` in [`frombmp-parser.c`](../../../src/frombmp-parser.c) and `sixel_bmp_parse_header()` in [`frombmp.c`](../../../src/frombmp.c) | File/DIB bytes → validated dialect, raster window, masks, palette, profile, and compression model | OS/2 versus Windows field namespaces, signed geometry, offsets, bpp/compression pairs, and `bmp_info40_mode` must be settled before payload dispatch. |
| `BMP-02` | `sixel_frombmp_load()`, `sixel_bmp_decode_indexed_uncompressed()`, and `sixel_bmp_decode_truecolor()` in [`frombmp.c`](../../../src/frombmp.c) | Native palette/packed rows → RGB or RGBA canvas | Row stride/origin, palette bounds, bit masks, scaling, and explicit-versus-legacy alpha must produce canonical row-major pixels. |
| `BMP-03` | `sixel_bmp_decode_rle8_rgb()`, `sixel_bmp_decode_rle4_rgb()`, `sixel_bmp_decode_huffman1d_rgb()`, and `sixel_bmp_decode_rle24_rgb()` in [`frombmp.c`](../../../src/frombmp.c) | Compressed native payload → RGB/CMYK canvas | Encoded/absolute runs, delta/EOL/EOB motion, OS/2 Huffman codes, top-down exclusions, and row/canvas bounds must be enforced by dialect. |
| `BMP-04` | `sixel_builtin_load_nonpng_rgb8_fallback()` in [`loader-builtin.c`](../../../src/loader-builtin.c), calling `sixel_builtin_load_jpeg_frame()` or `sixel_frompng_load_nonindexed()` | Bounded BI_JPEG/BI_PNG child chunk → child-format typed pixels | Child data must not escape the BMP payload window; PNG/JPEG source depth, alpha, background, and CMS semantics must survive delegation. |
| `BMP-05` | `sixel_builtin_apply_bmp_icc_to_rgba_channels()` and `sixel_builtin_apply_bmp_calibrated_to_rgba_channels()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Source RGB(A) plus V4/V5 metadata → transformed RGB(A) | Embedded ICC takes its documented precedence over calibrated endpoints/gamma, and color conversion must leave alpha unchanged. |
| `BMP-06` | `sixel_builtin_apply_bmp_alpha_policy()` and `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Native or delegated pixels → one typed frame | Only implicit all-zero legacy alpha receives the opaque rescue; explicit alpha must be composited or converted to the frame transparency representation. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin:bmp_info40_mode=auto! image.bmp
img2sixel -Lbuiltin:bmp_info40_mode=os2:cms_engine=none! ambiguous.bmp
```

| Control | Exact effect on BMP |
| --- | --- |
| `bmp_info40_mode=auto|windows|os2` (`BMODE`) | Selects the namespace for a 40-byte DIB. Default `auto` uses the OS/2-only bpp/compression heuristic above. It has no effect on other DIB sizes. Forcing the wrong family normally rejects the compression/layout rather than silently converting it. |
| `cms_engine=none|auto|builtin|lcms2|colorsync` | `none` disables V4/V5 profile application and makes CMYK use device conversion. Other values select an available engine for applicable calibrated/embedded profiles. |
| `cms_target=gamma|linear|cielab|oklab|din99d`, `cms_intent=...`, `prefer_8bit=0|1` | Affect successful profile conversion and target storage. They do not change mask unpacking, RLE, or an unprofiled ordinary BMP. Nested JPEG/PNG retains that component's precision rules. |
| `-B`, `background_policy`, `background_colorspace`, `-A auto|composite|clear|keep` | Affect an explicit-alpha native BMP and alpha-bearing nested PNG/JPEG. BMP has no interpreted file-background field, so `background_policy` has no competing native BMP source. |
| `builtin:orientation=0|1` | No effect on native BMP. Top-down/bottom-up is part of raster layout, not Exif. A nested payload follows the nested component only where that call path implements the metadata. |
| `-p COLORS`, resize, crop, sampling, and encoder color modes | Run after native BMP expansion. They do not enable a palette-preserving BMP path. |
| `trns_keycolor` | Relevant only inside a nested PNG payload. It has no native BMP meaning. |
| HDR, PNM, and PSD-specific suboptions, `-S`, `-T`, `-l`, `-g` | No effect on native BMP. The builtin BMP component emits one frame. |

## Unsupported behavior and security boundary

Other DIB sizes, arbitrary plane counts, invalid bpp/compression pairings, top-down RLE/CMYK/alpha-bitfield forms, OS/2 bitmap arrays, ICO/CUR selection, embedded external profile paths, and unsupported nested-codec dialects are rejected or ignored as stated above. The component does not preserve unknown DIB fields, palette indices, or application metadata for round trip.

BMP is not intrinsically simple: header-size dialects, signed dimensions, palette arithmetic, bit masks, RLE escape motion, Huffman runs, nested compressed images, and profile offsets are all attacker-controlled. `-Lbuiltin!` narrows fallback but is not a sandbox. Linked-profile suppression prevents the image from expanding the attack surface into filesystem or network profile resolution.

## Implementation landmarks

- Header, masks, color metadata, compression, and native raster decode: [`frombmp.c`](../../../src/frombmp.c)
- Probe and payload boundary used by loader routing: [`frombmp-parser.c`](../../../src/frombmp-parser.c)
- Nested JPEG/PNG, CMS, alpha, and frame delivery: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Alpha/background semantics: [Alpha Policy](../alpha-policy.md) and [Background Policy](../background-policy.md)
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [BMP/DIB coverage inventory](../../testing/builtin-loader-coverage.md#bmp-and-dib) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| BMP-01 | An OS/2 DIB is selected and decoded according to its RGB24 field namespace. | [tests/loader/builtin/1414_loader_builtin_bmp_os2_rgb24_decode_numeric.t](../../../tests/loader/builtin/1414_loader_builtin_bmp_os2_rgb24_decode_numeric.t) |
| BMP-02 | Explicit RGBA without a background becomes the documented RGB-plus-mask frame. | [tests/loader/builtin/1332_loader_builtin_bmp_rgba_mask_no_bg_numeric.t](../../../tests/loader/builtin/1332_loader_builtin_bmp_rgba_mask_no_bg_numeric.t) |
| BMP-03 | Mixed encoded, absolute, and control records reconstruct an RLE4 raster correctly. | [tests/loader/builtin/1345_loader_builtin_bmp_rle4_mixed_numeric.t](../../../tests/loader/builtin/1345_loader_builtin_bmp_rle4_mixed_numeric.t) |
| BMP-04 | A 16-bit alpha-bearing BI_PNG child retains PNG CMS/background precision through the BMP wrapper. | [tests/loader/builtin/1456_loader_builtin_bmp_bi_png16_alpha_bgcolor_cms_on_numeric.t](../../../tests/loader/builtin/1456_loader_builtin_bmp_bi_png16_alpha_bgcolor_cms_on_numeric.t) |
| BMP-05 | A V5 embedded RGB ICC profile transforms native BMP pixels when CMS is enabled. | [tests/loader/builtin/1372_loader_builtin_bmp_v5_embedded_icc_rgb_cms_on_numeric.t](../../../tests/loader/builtin/1372_loader_builtin_bmp_v5_embedded_icc_rgb_cms_on_numeric.t) |
| BMP-06 | RGBA composition over an explicit background preserves the expected float color result. | [tests/loader/builtin/1331_loader_builtin_bmp_rgba_bgcolor_float32_numeric.t](../../../tests/loader/builtin/1331_loader_builtin_bmp_rgba_bgcolor_float32_numeric.t) |
| BMP-07 | Mixed encoded, absolute, delta, and end records reconstruct an RLE8 raster exactly. | [tests/loader/builtin/1334_loader_builtin_bmp_rle8_decode_numeric.t](../../../tests/loader/builtin/1334_loader_builtin_bmp_rle8_decode_numeric.t) |
| BMP-08 | The OS/2 Huffman1D path decodes its monochrome scanline to exact samples. | [tests/loader/builtin/1417_loader_builtin_bmp_os2_huffman1d_decode_numeric.t](../../../tests/loader/builtin/1417_loader_builtin_bmp_os2_huffman1d_decode_numeric.t) |
| BMP-09 | The OS/2 RLE24 path reconstructs exact RGB samples. | [tests/loader/builtin/1418_loader_builtin_bmp_os2_rle24_decode_numeric.t](../../../tests/loader/builtin/1418_loader_builtin_bmp_os2_rle24_decode_numeric.t) |
| BMP-10 | Native CMYK32 conversion without CMS produces the documented numeric RGB result. | [tests/loader/builtin/1396_loader_builtin_bmp_cmyk_cms_off_numeric.t](../../../tests/loader/builtin/1396_loader_builtin_bmp_cmyk_cms_off_numeric.t) |
| BMP-11 | A nested BI_JPEG child produces exact pixels through the BMP wrapper with CMS disabled. | [tests/loader/builtin/1382_loader_builtin_bmp_bi_jpeg_cms_off_numeric.t](../../../tests/loader/builtin/1382_loader_builtin_bmp_bi_jpeg_cms_off_numeric.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| BMP-90 | Overlapping or otherwise invalid color masks are rejected during header/model validation. | [tests/loader/builtin/1360_loader_builtin_bmp_fail_invalid_color_masks_numeric.t](../../../tests/loader/builtin/1360_loader_builtin_bmp_fail_invalid_color_masks_numeric.t) |

Coverage audit note: direct numeric owners cover OS/2 RGB24, Huffman1D and RLE24, native alpha, RLE4 and RLE8, native CMYK32, nested JPEG and PNG, V5 ICC, and mask rejection. This still does not constitute the full DIB-size × bpp × compression matrix; the generated inventory must be consulted for narrower cases, and unlisted cross-products remain gaps rather than implications of these representatives.
