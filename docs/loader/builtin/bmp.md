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

Default packed 16-bit `BI_RGB` is interpreted as 5:5:5. Explicit bitfields may represent other contiguous component widths; each mask must be contiguous, disjoint from the other masks, and contained in the pixel word. Components wider than eight bits are normalized directly from the original integer ratio into float32, including alpha. Ordinary narrow components retain the legacy byte expansion when no precision-requiring work is needed; CMS, explicit float precision, or actual background composition also selects original-ratio unpacking for narrow bitfields. Invalid overlap, out-of-range masks, illegal escape motion, row overrun, truncated padding, palette overrun, or inconsistent wrapper dimensions fail.

## Alpha and color semantics

Native BMP follows the [loader precision, gamut and fast-path policy](../precision-policy.md). Untagged/named-sRGB byte RGB, palette expansion, opaque images with an unused background, and binary-alpha images without composition retain `RGB888` plus an optional transparency mask. An explicit float request promotes eligible byte inputs. BMP does not retain a palette/index plane as `PAL8`.

When calibrated RGB or a matching embedded ICC is selected, the adapter transforms source RGB or CMYK directly to canonical linear-sRGB float32. Negative values and values above one are retained. It then handles alpha/background in linear light and converts once to `cms_target`; `prefer_8bit=1` can explicitly narrow the final frame. There is no intermediate RGB888 conversion. The format decoder returns typed source samples and does not acquire profiles or apply loader policies.

V4 `LCS_CALIBRATED_RGB` endpoints and the three channel gamma values synthesize a source profile without averaging the channel responses. V5 `PROFILE_EMBEDDED` takes precedence. Its value is the little-endian DWORD `0x4d424544` (stored as `DEBM`); the historical ASCII `MBED` spelling is also accepted for compatibility. Calibrated primaries with zero Z, including the Display P3 red primary, are valid. An embedded profile with a different input domain retains the historical unprofiled interpretation; once an applicable profile is selected, profile creation or conversion failure fails the load instead of returning unmanaged pixels. CMYK without an applicable profile uses the existing byte device conversion. V5 `PROFILE_LINKED` is deliberately ignored: the parser never opens a path supplied by an image. Header rendering intent and unknown metadata are not exposed as a generic metadata object.

Explicit alpha retains its original ratio until linear background composition. Zero coverage can remain a separate mask according to the [alpha policy](../alpha-policy/README.md), including hidden RGB values transformed by CMS. Only implicit 32-bit `BI_RGB` all-zero alpha receives the historical opaque rescue; explicit all-zero alpha remains transparent. Fully opaque source alpha is discarded. Background and CMS allocation/conversion failures are reported before any frame callback.

## Embedded JPEG and PNG

For `BI_JPEG` and `BI_PNG`, `frombmp` validates the wrapper and returns the exact nested payload. The builtin loader constructs an in-memory chunk and calls its JPEG or PNG family rather than treating the bytes as ordinary BMP pixels. Wrapper width and height must agree with the nested result.

This delegation matters for precision and dialect support. A 16-bit embedded PNG can retain the PNG float path, PNG transparency/background/CMS rules remain PNG rules, and a supported high-precision JPEG can use the JPEG float path. Conversely, a PNG/JPEG variant rejected by the nested builtin component is not made valid by placing it in BMP. APNG and animated semantics are not promoted into BMP animation; the wrapper produces one image.

## Builtin decode pipeline and output forms

1. The dispatcher recognizes `BM`, probes DIB family, masks, compression, profile, and embedded-payload bounds, and applies `bmp_info40_mode`.
2. Embedded JPEG/PNG is delegated as described above. Native rasters enter the format-specific uncompressed, RLE, Huffman, RLE24, bitfield, or CMYK decoder.
3. Indexed values expand to bytes; packed values expand to bytes or normalized float source samples according to their precision requirements. Rows are placed according to bottom-up/top-down orientation, and explicit alpha is retained.
4. The adapter applies calibrated RGB or embedded ICC to canonical linear float while the original source components still exist. Unprofiled CMYK uses the device byte conversion.
5. The adapter composites the float path in linear light, retains zero coverage as a mask when requested, and selects the final target representation. Eligible byte inputs use the shared byte alpha finalizer. One complete frame is delivered.

Native output includes gamma `RGB888`, canonical `LINEARRGBFLOAT32`, or the configured typed CMS target. Embedded PNG/JPEG can supply its own higher-precision representation. BMP does not preserve its source index plane as loader `PAL8` in the current dedicated path.

### Implementation and test map

![A vertical implementation map of the builtin BMP loader. It follows BMP and DIB probing into native raster, RLE or OS/2 compressed, and embedded JPEG or PNG branches, then calibrated or embedded-profile color conversion and explicit-alpha finalization. Every node carries a coverage ID used by the tables below.](figures/bmp.svg)

The compressed and nested-payload boxes are alternatives. Native BMP compression stays in `frombmp.c`; `BI_JPEG` and `BI_PNG` slice a child chunk and deliberately re-enter the JPEG/PNG decoders, so their precision, CMS, and alpha rules are inherited from the child format before BMP finalization resumes.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `BMP-01` | `sixel_frombmp_probe()` in [`frombmp-parser.c`](../../../src/frombmp-parser.c) and `sixel_bmp_parse_header()` in [`frombmp.c`](../../../src/frombmp.c) | File/DIB bytes → validated dialect, raster window, masks, palette, profile, and compression model | OS/2 versus Windows field namespaces, signed geometry, offsets, bpp/compression pairs, and `bmp_info40_mode` must be settled before payload dispatch. |
| `BMP-02` | `sixel_frombmp_load()`, `sixel_bmp_decode_indexed_uncompressed()`, and `sixel_bmp_decode_truecolor()` in [`frombmp.c`](../../../src/frombmp.c) | Native palette/packed rows → typed RGB/RGBA source samples | Row stride/origin, palette bounds, bit masks, scaling, and explicit-versus-legacy alpha must produce canonical row-major pixels. |
| `BMP-03` | `sixel_bmp_decode_rle8_rgb()`, `sixel_bmp_decode_rle4_rgb()`, `sixel_bmp_decode_huffman1d_rgb()`, and `sixel_bmp_decode_rle24_rgb()` in [`frombmp.c`](../../../src/frombmp.c) | Compressed native payload → RGB/CMYK canvas | Encoded/absolute runs, delta/EOL/EOB motion, OS/2 Huffman codes, top-down exclusions, and row/canvas bounds must be enforced by dialect. |
| `BMP-04` | `sixel_builtin_load_nonpng_rgb8_fallback()` in [`loader-builtin.c`](../../../src/loader-builtin.c), calling `sixel_builtin_load_jpeg_frame()` or `sixel_frompng_load_nonindexed()` | Bounded BI_JPEG/BI_PNG child chunk → child-format typed pixels | Child data must not escape the BMP payload window; PNG/JPEG source depth, alpha, background, and CMS semantics must survive delegation. |
| `BMP-05` | `sixel_builtin_bmp_source()` and `sixel_builtin_load_bmp_native()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Typed RGB/CMYK source → canonical linear float → requested target | CMS must preserve gamut and original packed ratios; selected-profile failures must not fall through to unmanaged pixels. |
| `BMP-06` | `sixel_builtin_load_bmp_native()` and `sixel_builtin_apply_bmp_alpha_policy()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Typed RGB/alpha → color plus coverage mask | Only implicit all-zero alpha receives the opaque rescue; actual composition uses original alpha ratios in linear light. |

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
| `cms_target=gamma|linear|cielab|oklab|din99d`, `cms_intent=...`, `prefer_8bit=0|1` | Select the final profile-converted representation and intent. `prefer_8bit=1` is an explicit final narrowing, separate from the encoder byte preference. Wide bitfields and actual composition require float work even without CMS. Nested JPEG/PNG retains that component's precision rules. |
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
- Alpha/background semantics: [Alpha Policy](../alpha-policy/README.md) and [Background Policy](../background-policy.md)
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

### Native precision and gamut contracts

These independent value tests cover the native precision boundary. The P3 oracle is an analytic primary matrix and transfer curve; calibrated endpoints use a tighter 2.30/float budget, and CMYK uses the existing 60-digit Decimal fixture oracle. Engine-specific cases require a successful trace from the requested engine. They are separate from image-quality regression tests.

| ID | Contract protected | Owning test |
| --- | --- | --- |
| BMP-101 | V5 P3 RGB24 reaches linear float with negative and above-one components. | [tests/loader/builtin/bmp_precision/0001_bmp_p3_rgb24_numeric.t](../../../tests/loader/builtin/bmp_precision/0001_bmp_p3_rgb24_numeric.t) |
| BMP-102 | CMS preserves hidden RGB and the zero-alpha mask. | [tests/loader/builtin/bmp_precision/0002_bmp_p3_alpha_numeric.t](../../../tests/loader/builtin/bmp_precision/0002_bmp_p3_alpha_numeric.t) |
| BMP-103 | P3 partial alpha composites against the explicit background in linear light. | [tests/loader/builtin/bmp_precision/0003_bmp_p3_background_numeric.t](../../../tests/loader/builtin/bmp_precision/0003_bmp_p3_background_numeric.t) |
| BMP-104 | Gamma target encoding follows linear background composition. | [tests/loader/builtin/bmp_precision/0004_bmp_gamma_background_numeric.t](../../../tests/loader/builtin/bmp_precision/0004_bmp_gamma_background_numeric.t) |
| BMP-105 | The keep policy retains zero coverage while composing partial alpha. | [tests/loader/builtin/bmp_precision/0005_bmp_keep_background_numeric.t](../../../tests/loader/builtin/bmp_precision/0005_bmp_keep_background_numeric.t) |
| BMP-106 | Equal calibrated channel gammas retain P3 gamut, including a zero-Z red primary. | [tests/loader/builtin/bmp_precision/0006_bmp_calibrated_common_numeric.t](../../../tests/loader/builtin/bmp_precision/0006_bmp_calibrated_common_numeric.t) |
| BMP-107 | Distinct calibrated channel gammas remain distinct through float conversion. | [tests/loader/builtin/bmp_precision/0007_bmp_calibrated_split_numeric.t](../../../tests/loader/builtin/bmp_precision/0007_bmp_calibrated_split_numeric.t) |
| BMP-108 | Indexed RGB expands into the same managed float values as direct pixels. | [tests/loader/builtin/bmp_precision/0008_bmp_indexed_p3_numeric.t](../../../tests/loader/builtin/bmp_precision/0008_bmp_indexed_p3_numeric.t) |
| BMP-109 | RLE8 reconstruction precedes float RGB CMS. | [tests/loader/builtin/bmp_precision/0009_bmp_rle8_p3_numeric.t](../../../tests/loader/builtin/bmp_precision/0009_bmp_rle8_p3_numeric.t) |
| BMP-110 | RLE4 reconstruction precedes float RGB CMS. | [tests/loader/builtin/bmp_precision/0010_bmp_rle4_p3_numeric.t](../../../tests/loader/builtin/bmp_precision/0010_bmp_rle4_p3_numeric.t) |
| BMP-111 | Unmanaged 10-bit components retain their integer ratios in canonical float. | [tests/loader/builtin/bmp_precision/0011_bmp_rgb10_numeric.t](../../../tests/loader/builtin/bmp_precision/0011_bmp_rgb10_numeric.t) |
| BMP-112 | Managed 10-bit components enter CMS before any byte expansion. | [tests/loader/builtin/bmp_precision/0012_bmp_p3_rgb10_numeric.t](../../../tests/loader/builtin/bmp_precision/0012_bmp_p3_rgb10_numeric.t) |
| BMP-113 | 11-bit alpha retains its original ratio through background composition. | [tests/loader/builtin/bmp_precision/0013_bmp_alpha11_numeric.t](../../../tests/loader/builtin/bmp_precision/0013_bmp_alpha11_numeric.t) |
| BMP-114 | CMS uses original 5:6:5 ratios instead of replicated byte values. | [tests/loader/builtin/bmp_precision/0014_bmp_p3_rgb565_numeric.t](../../../tests/loader/builtin/bmp_precision/0014_bmp_p3_rgb565_numeric.t) |
| BMP-115 | Explicit float precision uses original unprofiled 5:6:5 ratios. | [tests/loader/builtin/bmp_precision/0015_bmp_forced_rgb565_numeric.t](../../../tests/loader/builtin/bmp_precision/0015_bmp_forced_rgb565_numeric.t) |
| BMP-116 | Top-down 10-bit rows retain placement and values. | [tests/loader/builtin/bmp_precision/0016_bmp_topdown_rgb10_numeric.t](../../../tests/loader/builtin/bmp_precision/0016_bmp_topdown_rgb10_numeric.t) |
| BMP-117 | CMS disabling ignores embedded color interpretation but retains wide source precision. | [tests/loader/builtin/bmp_precision/0017_bmp_none_rgb10_numeric.t](../../../tests/loader/builtin/bmp_precision/0017_bmp_none_rgb10_numeric.t) |
| BMP-118 | An unmanaged RGB24 image retains exact byte samples. | [tests/loader/builtin/bmp_precision/0018_bmp_rgb24_byte_numeric.t](../../../tests/loader/builtin/bmp_precision/0018_bmp_rgb24_byte_numeric.t) |
| BMP-119 | Binary alpha without composition retains exact byte RGB and a coverage mask. | [tests/loader/builtin/bmp_precision/0019_bmp_binary_alpha_byte_numeric.t](../../../tests/loader/builtin/bmp_precision/0019_bmp_binary_alpha_byte_numeric.t) |
| BMP-120 | An unused background does not promote fully opaque byte pixels. | [tests/loader/builtin/bmp_precision/0020_bmp_opaque_background_byte_numeric.t](../../../tests/loader/builtin/bmp_precision/0020_bmp_opaque_background_byte_numeric.t) |
| BMP-121 | Actual unmanaged partial-alpha composition requires linear float. | [tests/loader/builtin/bmp_precision/0021_bmp_partial_background_numeric.t](../../../tests/loader/builtin/bmp_precision/0021_bmp_partial_background_numeric.t) |
| BMP-122 | Unprofiled CMYK retains the existing exact byte device conversion. | [tests/loader/builtin/bmp_precision/0022_bmp_device_cmyk_byte_numeric.t](../../../tests/loader/builtin/bmp_precision/0022_bmp_device_cmyk_byte_numeric.t) |
| BMP-123 | Managed CMYK32 transforms original ink samples directly to float RGB. | [tests/loader/builtin/bmp_precision/0023_bmp_cmyk32_numeric.t](../../../tests/loader/builtin/bmp_precision/0023_bmp_cmyk32_numeric.t) |
| BMP-124 | CMYK RLE8 retains its original ink samples through float CMS. | [tests/loader/builtin/bmp_precision/0024_bmp_cmyk_rle8_numeric.t](../../../tests/loader/builtin/bmp_precision/0024_bmp_cmyk_rle8_numeric.t) |
| BMP-125 | CMYK RLE4 retains its original ink samples through float CMS. | [tests/loader/builtin/bmp_precision/0025_bmp_cmyk_rle4_numeric.t](../../../tests/loader/builtin/bmp_precision/0025_bmp_cmyk_rle4_numeric.t) |
| BMP-126 | Gamma target retains out-of-sRGB components after float CMS. | [tests/loader/builtin/bmp_precision/0026_bmp_gamma_target_numeric.t](../../../tests/loader/builtin/bmp_precision/0026_bmp_gamma_target_numeric.t) |
| BMP-127 | Explicit final byte narrowing occurs after float CMS. | [tests/loader/builtin/bmp_precision/0027_bmp_prefer8bit_numeric.t](../../../tests/loader/builtin/bmp_precision/0027_bmp_prefer8bit_numeric.t) |
| BMP-128 | Actual lcms2 RGB conversion satisfies the independent P3 bound. | [tests/loader/builtin/bmp_precision/0028_bmp_lcms2_numeric.t](../../../tests/loader/builtin/bmp_precision/0028_bmp_lcms2_numeric.t) |
| BMP-129 | Actual ColorSync RGB conversion satisfies the independent P3 bound. | [tests/loader/builtin/bmp_precision/0029_bmp_colorsync_numeric.t](../../../tests/loader/builtin/bmp_precision/0029_bmp_colorsync_numeric.t) |
| BMP-130 | An incompatible RGB-source profile retains the historical unmanaged byte interpretation. | [tests/loader/builtin/bmp_precision/0030_bmp_wrong_rgb_domain_byte_numeric.t](../../../tests/loader/builtin/bmp_precision/0030_bmp_wrong_rgb_domain_byte_numeric.t) |
| BMP-131 | An incompatible CMYK-source profile retains the historical device byte conversion. | [tests/loader/builtin/bmp_precision/0031_bmp_wrong_cmyk_domain_byte_numeric.t](../../../tests/loader/builtin/bmp_precision/0031_bmp_wrong_cmyk_domain_byte_numeric.t) |
| BMP-132 | A malformed selected RGB profile fails without a frame callback. | [tests/loader/builtin/bmp_precision/0032_bmp_invalid_profile_reject.t](../../../tests/loader/builtin/bmp_precision/0032_bmp_invalid_profile_reject.t) |
| BMP-133 | A selected CMYK transform failure does not retry unmanaged conversion. | [tests/loader/builtin/bmp_precision/0033_bmp_transform_fail.t](../../../tests/loader/builtin/bmp_precision/0033_bmp_transform_fail.t) |
| BMP-134 | Float allocation failure emits no frame and a later load can recover. | [tests/loader/builtin/bmp_precision/0034_bmp_float_alloc_fail.t](../../../tests/loader/builtin/bmp_precision/0034_bmp_float_alloc_fail.t) |
| BMP-135 | Mask allocation failure emits no frame and a later load can recover. | [tests/loader/builtin/bmp_precision/0035_bmp_mask_alloc_fail.t](../../../tests/loader/builtin/bmp_precision/0035_bmp_mask_alloc_fail.t) |
| BMP-139 | A 30-bit component is normalized without integer overflow. | [tests/loader/builtin/bmp_precision/0039_bmp_rgb30_numeric.t](../../../tests/loader/builtin/bmp_precision/0039_bmp_rgb30_numeric.t) |
| BMP-140 | Explicit all-zero alpha remains transparent after CMS. | [tests/loader/builtin/bmp_precision/0040_bmp_explicit_zero_alpha_numeric.t](../../../tests/loader/builtin/bmp_precision/0040_bmp_explicit_zero_alpha_numeric.t) |
| BMP-141 | Implicit BI_RGB all-zero alpha receives the opaque rescue after CMS. | [tests/loader/builtin/bmp_precision/0041_bmp_implicit_zero_alpha_numeric.t](../../../tests/loader/builtin/bmp_precision/0041_bmp_implicit_zero_alpha_numeric.t) |
| BMP-142 | Actual lcms2 CMYK conversion satisfies the independent affine-LUT bound. | [tests/loader/builtin/bmp_precision/0042_bmp_cmyk_lcms2_numeric.t](../../../tests/loader/builtin/bmp_precision/0042_bmp_cmyk_lcms2_numeric.t) |
| BMP-143 | Actual ColorSync CMYK conversion satisfies the independent affine-LUT bound. | [tests/loader/builtin/bmp_precision/0043_bmp_cmyk_colorsync_numeric.t](../../../tests/loader/builtin/bmp_precision/0043_bmp_cmyk_colorsync_numeric.t) |
| BMP-144 | Legacy ASCII profile identifiers also preserve RGB float precision. | [tests/loader/builtin/bmp_precision/0044_bmp_legacy_icc_rgb_numeric.t](../../../tests/loader/builtin/bmp_precision/0044_bmp_legacy_icc_rgb_numeric.t) |
| BMP-145 | Legacy ASCII profile identifiers also preserve CMYK float precision. | [tests/loader/builtin/bmp_precision/0045_bmp_legacy_icc_cmyk_numeric.t](../../../tests/loader/builtin/bmp_precision/0045_bmp_legacy_icc_cmyk_numeric.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| BMP-90 | Overlapping or otherwise invalid color masks are rejected during header/model validation. | [tests/loader/builtin/1360_loader_builtin_bmp_fail_invalid_color_masks_numeric.t](../../../tests/loader/builtin/1360_loader_builtin_bmp_fail_invalid_color_masks_numeric.t) |

Coverage audit note: direct numeric owners cover OS/2 RGB24, Huffman1D and RLE24, native alpha, RLE4 and RLE8, native CMYK32, nested JPEG and PNG, V5 ICC, and mask rejection. This still does not constitute the full DIB-size × bpp × compression matrix; the generated inventory must be consulted for narrower cases, and unlisted cross-products remain gaps rather than implications of these representatives.

The [native precision suite](../../../tests/loader/builtin/bmp_precision) also rejects noncontiguous, overlapping, and out-of-word masks. These numeric and defensive tests do not establish an image-quality score or exhaust the full header/depth/compression cross-product.
