# Builtin TGA Loader

## Format lineage

Truevision introduced TGA for TARGA graphics hardware and later published the TGA 2.0 extension/footer model. The format is also called TARGA and conventionally uses `.tga`, but it has no mandatory leading magic. The [Truevision TGA specification archive](https://github.com/DEAKSoftware/Truevision-TGA) preserves the 2.0 specification and developer material.

TGA is a small raster container rather than one pixel coding. It can store colormapped indices, direct BGR(A), or grayscale, either raw or packet-RLE, with selectable vertical/horizontal origin and optional trailing extension/developer areas. Many old writers use underspecified 16-bit alpha and colormap conventions, so “TGA support” needs a narrower compatibility statement.

libsixel retains TGA in the adapted stb-derived core imported by commit [`81375f0bd`](https://github.com/saitoha/libsixel/commit/81375f0bd). Commit [`6d57f075d`](https://github.com/saitoha/libsixel/commit/6d57f075d) added the palette-preserving path on 2026-01-22, and [`ba59c4607`](https://github.com/saitoha/libsixel/commit/ba59c4607) added explicit 32-bit truecolor alpha finalization on 2026-03-29. Unlike PNG, GIF, HDR, BMP, and PSD, this family has not been extracted into a complete format-owned decoder.

## Header, recognition, and image types

The 18-byte little-endian header contains ID length, colormap flag, image type, colormap origin/length/entry size, x/y origin, width, height, pixel depth, and descriptor. Because there is no signature, the predicate validates compatible combinations and can only make a structural guess. Closing the loader chain with `-Lbuiltin!` prevents another component from accepting a rejected guess but does not make TGA recognition collision-free.

| Image type | Source model | Compression |
| --- | --- | --- |
| 1 | Colormapped indices | Raw |
| 2 | Direct truecolor | Raw |
| 3 | Grayscale | Raw |
| 9 | Colormapped indices | TGA packet RLE |
| 10 | Direct truecolor | TGA packet RLE |
| 11 | Grayscale | TGA packet RLE |

The image ID field is skipped. Width and height must be positive and within stb's configured dimension limit. The descriptor's vertical-origin bit is honored by row reversal. The horizontal right-to-left bit is not implemented. X/y origin values do not place the raster on a larger canvas.

## Pixel and palette representations

The byte representations below describe the untagged or CMS-disabled path. With an active TGA 2.0 color definition, original decoded components enter the [managed float path](#tga-20-and-extension-fields) before palette-alpha folding or background composition.

| Source representation | Parser support | Builtin frame behavior |
| --- | --- | --- |
| 8-bit grayscale | Yes | Expanded to gamma `RGB888` because the residual wrapper requests three components. |
| 16-bit grayscale plus alpha | The stb parser recognizes two 8-bit components | The residual wrapper requests `RGB`, so grayscale is expanded and alpha is discarded. This is not an alpha-preserving end-to-end variant. |
| 15/16-bit direct truecolor | Yes | Interpreted as opaque 5:5:5 RGB and expanded to gamma bytes. The spare bit is not treated as alpha. |
| 24-bit direct truecolor | Yes | BGR is reordered to gamma `RGB888`. |
| 32-bit direct truecolor | Yes | BGRA is reordered to `RGBA8888`, then common alpha/background finalization runs. |
| 8-bit colormap entry | Yes | Interpreted as grayscale and expanded to RGB. |
| 15/16-bit colormap entry | Yes | Interpreted as opaque 5:5:5 and expanded to RGB. |
| 24-bit colormap entry | Yes | BGR palette entry becomes RGB. |
| 32-bit colormap entry | Yes | BGRA palette entry supplies palette alpha as described below. |

The general stb path accepts 8- or 16-bit colormap indices and expands them immediately. The palette-preserving path is narrower: it requires an indexed type, 8-bit indices, and enabled palette fusion, then returns gamma `PAL8` plus at most 256 RGB entries.

The header's First Entry Index is the logical index of the first stored palette entry. It does not add padding before the palette: the file stores exactly Color Map Length entries immediately after the optional image ID. For example, origin 342 and length 72 describe file indices 342 through 413, which map to stored palette slots 0 through 71. Both raw and RLE decoding subtract the origin before lookup; the PAL8 fast path stores those zero-based slots, so palette alpha and the transparent key remain aligned. Eight- and sixteen-bit source indices retain their full unsigned range before subtraction. An invalid index below the origin or beyond the stored entries retains the existing general-path compatibility behavior of choosing the first stored color; a failed palette-preserving attempt releases its temporary buffers before taking that RGB fallback.

## RLE coding and orientation

TGA RLE divides the raster into packets of 1 through 128 pixels. A command with bit 7 clear is a raw packet followed by that many distinct samples; bit 7 set is a run packet followed by one sample repeated that many times. Packets can cross scanline boundaries because decode is over the linear pixel count. The decoder validates allocation arithmetic and source availability through the stb context, then performs vertical row reversal when the descriptor declares bottom-left origin.

Horizontal right-to-left order is left unchanged. TGA 2.0 scan-line tables are not used to seek rows; all accepted pixel data is decoded sequentially from the image-data offset.

## Alpha and palette fast-path details

On the legacy byte path, thirty-two-bit direct truecolor is the only non-indexed TGA form for which the builtin wrapper explicitly requests RGBA and retains source alpha. Common finalization then composites it against a resolved output background or converts alpha into the frame's transparency mask according to `-A`.

For an indexed 8-bit TGA with a 32-bit palette, the fast path chooses the first fully transparent palette entry as the single key-color index and remaps pixels using any other fully transparent entry to that key. Partial palette alpha cannot be represented in `PAL8`; each such palette color is flattened with integer gamma-space blending against the provided background, or black when no background is available. Thus the palette path preserves binary transparency but not arbitrary per-entry partial alpha.

If palette fusion is disabled, indexed input is expanded by the general path. That path is requested as three-component RGB, so palette alpha does not survive. This differs from 32-bit direct truecolor and is a current builtin boundary, not a property of the TGA format.

## TGA 2.0 and extension fields

The [Truevision TGA 2.0 specification](https://www.ludorg.net/amnesia/TGA_File_Format_Spec.html) defines gamma and color-correction data, but no standard ICC-profile field. Its developer area can carry application-defined data; that extensibility does not itself establish an interoperable ICC embedding convention. Thus absence of a standard ICC field does not mean that TGA has no source color information worth interpreting.

The managed path recognizes the exact `TRUEVISION-XFILE.` footer signature and terminator, validates the extension offset and declared size (at least 495 bytes), and reads gamma and the optional 2048-byte color-correction table. All values are little-endian. The extension and table may appear in either order after the raster, but must fit before the footer without overlapping one another or the encoded raster. A malformed active declaration fails without publishing an unmanaged replacement. The bounded raster view also prevents truncated raw/RLE data from borrowing bytes from metadata.

| Source declaration | Adopted interpretation |
| --- | --- |
| Color-correction table present | Each decoded component independently indexes its column in the 256-row table. Words are stored in **A, R, G, B** order and normalized by 65535, without an intermediate byte conversion. The table takes precedence over scalar gamma. |
| Gamma numerator/denominator, no table | The ratio is the source **decoding exponent**: `linear = sample^gamma`, assuming sRGB primaries and D65. Thus `1/1` means linear samples, not unspecified sRGB. A nonzero denominator requires `0 < gamma <= 10`; fractional ratios retain their precision. |
| Gamma denominator zero, no table | No color definition: retain the existing byte/palette path. A footer alone likewise does not require float conversion. |
| CMS disabled | Ignore color metadata and retain the existing unmanaged behavior. |

The original specification describes the table as values for image remapping or hardware LUT driving; it does not provide a calibrated destination color space or a complete rule for combining the table and gamma. **libsixel's explicit compatibility convention** interprets corrected RGB table values as sRGB and does not apply the scalar gamma again. This convention is not a claim that every historic hardware LUT was calibrated to sRGB. TGA provides no standard primaries/white-point or embedded ICC field from which to recover a different device definition.

Metadata reading and component-table lookup live in [`fromtga.c`](../../../src/fromtga.c), without a CMS dependency. The loader adapter passes the resulting source definition to the common CMS and keeps canonical linear RGB in float32 before final target negotiation. `cms_target` and explicit `prefer_8bit` apply at that final boundary; encoder `-.8bit` does not force an earlier narrowing.

Managed raw/RLE truecolor, grayscale, and indexed input share this route. The existing decoder still expands packed 5:5:5 into byte channel codes, and the correction table indexes those expanded codes. Palette fusion requests cannot force corrected colors back into a byte palette. Requesting RGBA from the decoder preserves indexed and grayscale alpha on this managed route, including when the unmanaged RGB fallback would discard it. Real source alpha uses the table's A column when present, but never the RGB gamma/profile transform. Decoder-added opaque alpha is not remapped into artificial transparency. Background composition is linear and follows the existing alpha policy; hidden RGB is preserved where that policy preserves zero coverage.

Premultiplied alpha (`Attributes Type = 4`) is rejected on the managed path because it requires a separate source-association contract. No-metadata and CMS-disabled alpha behavior is unchanged. Other extension attributes, developer records, author/comments, timestamps, key color, pixel aspect, postage stamps, and scan-line indexing remain uninterpreted and are not round-tripped.

## Builtin decode and output pipeline

1. Residual dispatch reaches the weak-signature TGA predicate only after strong signatures such as PNG, JPEG, GIF, PSD, HDR, BMP, PIC, and WebP have had their dedicated routing.
2. An active TGA 2.0 color definition selects bounded RGBA decoding followed by table/gamma interpretation, common CMS, float composition, and final target negotiation. Without it, palette fusion first attempts the narrow indexed-8 path and creates `PAL8` when it succeeds.
3. On the unmanaged fallback, stb decodes raw/RLE samples and converts them to a requested three-component buffer, except that recognized 32-bit direct truecolor requests four components.
4. BGR ordering and vertical origin are normalized by the raster reader. The managed path performs its color conversion before alpha composition; metadata does not add an orientation stage.
5. On the unmanaged route, direct RGBA or a palette key enters common alpha/background finalization; opaque/general RGB is delivered directly as one frame.

Unmanaged output remains eight-bit gamma RGB/PAL8 plus optional transparency state. Managed output preserves canonical float RGB until an explicit CMS target requests another representation. Later resize, crop, colorspace conversion, palette initialization, and quantization are encoder stages.

### Implementation and test map

![A vertical implementation map of the builtin TGA loader. It follows the signature-less structural probe into managed float color normalization or the legacy indexed-palette and byte-alpha paths, using the adapted raw and packet-RLE raster decoder and vertical-origin normalization. Every node carries a coverage ID used by the tables below.](figures/tga.svg)

TGA is the one map here whose first node is necessarily a weak probe. Managed color input bypasses the legacy palette and byte-alpha finalizers. The indexed fast path and general `stbi__tga_load()` path are alternatives; their alpha behavior is not equivalent, so the map names their distinct entry points and conditions.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `TGA-01` | `stbi__tga_test()` and `stbi__tga_info()` in [`stb_image.h`](../../../src/stb_image.h) | Residual input bytes → plausible TGA header model | Image type, colormap flag, depths, and positive dimensions must be mutually plausible, while strong-signature formats retain routing priority. |
| `TGA-02` | `sixel_builtin_try_load_indexed_tga()` in [`loader-builtin.c`](../../../src/loader-builtin.c), calling `stbi__tga_load_palette()` | Indexed type with fusion enabled → `PAL8`, RGB palette, optional key | Only 8-bit indices and at most 256 entries qualify; multiple binary-transparent entries and partial alpha must be normalized as documented. |
| `TGA-03` | `stbi__tga_load()` and `stbi__tga_read_rgb16()` in [`stb_image.h`](../../../src/stb_image.h) | Raw or packet-RLE source pixels → expanded RGB/RGBA bytes | Raw/run packets, palette indices, grayscale, 5:5:5, BGR, and BGRA component order must consume exactly the raster pixel count. |
| `TGA-04` | Row-order branch inside `stbi__tga_load()` in [`stb_image.h`](../../../src/stb_image.h) | File-order decoded rows → canonical top-to-bottom rows | Bottom-origin rows must reverse exactly once; the unsupported right-to-left bit must not be documented as an implemented mirror. |
| `TGA-06` | `sixel_builtin_load_tga_color()` and [`fromtga.c`](../../../src/fromtga.c) | Extension metadata plus decoded RGBA → canonical float RGB and coverage | Validate bounds, preserve table words, choose one source interpretation, and compose before final target narrowing. |
| `TGA-05` | `sixel_builtin_apply_tga_truecolor_alpha_policy()` and `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Direct RGBA or palette transparency → gamma RGB/PAL8 plus mask/key or composite | Direct 32-bit alpha must reach common policy; alpha discarded by the general indexed fallback must not be claimed as recoverable. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin! indexed.tga
img2sixel -Lbuiltin! -A keep -B '#202020' rgba32.tga
```

| Control | Exact effect on TGA |
| --- | --- |
| `-A auto|composite|clear|keep` | Applies to real decoded alpha on the managed path, and to 32-bit direct truecolor or binary transparent-key output on the unmanaged path. It cannot recover alpha discarded by the RGB fallback. |
| `-B COLOR`, `background_colorspace` | Supplies the output background for linear managed composition or legacy direct-alpha finalization. On the indexed fast path, partial palette alpha is flattened against the supplied byte color; absent background means black. |
| `background_policy` | TGA has no interpreted file-background candidate, so it does not choose between two TGA background sources. |
| `-p COLORS` | Does not itself disable palette fusion. The TGA fast path can return the complete source palette even when later encoder policy requests fewer output colors; later palette processing must reduce it. This differs from builtin PNG's loader-side `palette_colors <= reqcolors` guard. |
| Resize options and non-default encoder color modes | Disable loader palette fusion, forcing indexed TGA through RGB expansion. On the unmanaged RGB fallback, 32-bit palette alpha is not retained; managed decoding retains it. |
| `cms_engine`, `cms_target`, `cms_intent`, `prefer_8bit` | Control common-CMS interpretation and final target negotiation for active gamma/table metadata. CMS disabled or absent metadata retains the byte path. |
| `builtin:orientation` | No effect. Vertical origin is always handled by the TGA raster parser; Exif/XMP orientation is absent. Horizontal origin remains unsupported. |
| `-S`, `-T`, `-l`, `-g`, `trns_keycolor`, HDR, PNM, BMP, and PSD-specific suboptions | No effect. TGA is emitted as one frame. |

## Unsupported behavior and security boundary

Horizontal right-to-left storage, extension/developer semantics other than the color fields above, high-depth source samples, premultiplied managed alpha, embedded ICC interpretation, and animation are not supported as end-to-end contracts. Gray-alpha and indexed alpha preservation remain unsupported on the unmanaged general RGB fallback. The descriptor alpha-bit count is not used to turn 15/16-bit direct pixels into alpha.

Weak recognition, dimensions, palette sizes, index values, and RLE commands are attacker-controlled. Because TGA lacks a strong magic, fallback order can affect which parser sees a malformed input. `-Lbuiltin!` narrows the component set but does not sandbox the adapted stb decoder.

## Implementation landmarks

- TGA predicate, indexed parser, and general decode: [`stb_image.h`](../../../src/stb_image.h)
- Palette fusion, palette-alpha folding, direct-alpha selection, and frame delivery: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Alpha/background contract: [Alpha Policy](../alpha-policy/README.md) and [Background Policy](../background-policy.md)
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)
- Forced raw/RLE and indexed/truecolor/grayscale fuzz entry points: [`fuzz-loader-builtin-libfuzzer.c`](../../../fuzz/fuzz-loader-builtin-libfuzzer.c)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [TGA coverage inventory](../../testing/builtin-loader-coverage.md#tga) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| TGA-01 | A type-2 24-bit direct-color stream expands exact BGR pixels into RGB component lanes. | [tests/loader/builtin/1968_loader_builtin_tga_truecolor24_numeric.t](../../../tests/loader/builtin/1968_loader_builtin_tga_truecolor24_numeric.t) |
| TGA-02 | An eligible indexed TGA selects the palette-preserving builtin path. | [tests/loader/builtin/0050_loader_builtin_palette_tga_path.t](../../../tests/loader/builtin/0050_loader_builtin_palette_tga_path.t) |
| TGA-03 | A type-10 packet-RLE truecolor stream expands raw and repeated packets to exact RGB bytes. | [tests/loader/builtin/1953_loader_builtin_tga_rle_packet_modes_numeric.t](../../../tests/loader/builtin/1953_loader_builtin_tga_rle_packet_modes_numeric.t) |
| TGA-04 | A type-3 grayscale stream expands each sample to exact equal RGB components. | [tests/loader/builtin/1969_loader_builtin_tga_grayscale8_numeric.t](../../../tests/loader/builtin/1969_loader_builtin_tga_grayscale8_numeric.t) |
| TGA-05 | Direct BGRA alpha composites against an explicit background with the expected numeric result. | [tests/loader/builtin/0710_loader_builtin_tga_rgba_background_numeric.t](../../../tests/loader/builtin/0710_loader_builtin_tga_rgba_background_numeric.t) |
| TGA-06 | Packed 5:5:5 direct color expands to exact RGB bytes. | [tests/loader/builtin/1948_loader_builtin_tga_truecolor16_numeric.t](../../../tests/loader/builtin/1948_loader_builtin_tga_truecolor16_numeric.t) |
| TGA-07 | Eight-bit grayscale and 5:5:5 palette entries expand through their distinct component paths. | [tests/loader/builtin/1949_loader_builtin_tga_palette8_numeric.t](../../../tests/loader/builtin/1949_loader_builtin_tga_palette8_numeric.t), [tests/loader/builtin/1950_loader_builtin_tga_palette16_numeric.t](../../../tests/loader/builtin/1950_loader_builtin_tga_palette16_numeric.t) |
| TGA-08 | Sixteen-bit palette indices select the exact declared entry. | [tests/loader/builtin/1951_loader_builtin_tga_index16_numeric.t](../../../tests/loader/builtin/1951_loader_builtin_tga_index16_numeric.t) |
| TGA-09 | Bottom-origin storage is vertically normalized to top-to-bottom frame order. | [tests/loader/builtin/1952_loader_builtin_tga_bottom_origin_numeric.t](../../../tests/loader/builtin/1952_loader_builtin_tga_bottom_origin_numeric.t) |
| TGA-10 | Raw and repeated type-10 RLE packets preserve exact pixel order. | [tests/loader/builtin/1953_loader_builtin_tga_rle_packet_modes_numeric.t](../../../tests/loader/builtin/1953_loader_builtin_tga_rle_packet_modes_numeric.t) |
| TGA-11 | The general RGB fallback maps an out-of-range palette index to the first stored entry instead of reading beyond the palette. | [tests/loader/builtin/1955_loader_builtin_tga_oob_index_fallback_numeric.t](../../../tests/loader/builtin/1955_loader_builtin_tga_oob_index_fallback_numeric.t) |
| TGA-12 | Expand raw eight-bit indices relative to a nonzero colormap origin. | [tests/loader/builtin/2244_tga_origin_rgb_raw_numeric.t](../../../tests/loader/builtin/2244_tga_origin_rgb_raw_numeric.t) |
| TGA-13 | Rebase raw eight-bit file indices to exact zero-based PAL8 slots. | [tests/loader/builtin/2245_tga_origin_pal_raw_numeric.t](../../../tests/loader/builtin/2245_tga_origin_pal_raw_numeric.t) |
| TGA-14 | Expand raw and run RLE packets relative to the colormap origin. | [tests/loader/builtin/2246_tga_origin_rgb_rle_numeric.t](../../../tests/loader/builtin/2246_tga_origin_rgb_rle_numeric.t) |
| TGA-15 | Rebase a repeated PAL8 index once when reading its RLE packet. | [tests/loader/builtin/2247_tga_origin_pal_rle_numeric.t](../../../tests/loader/builtin/2247_tga_origin_pal_rle_numeric.t) |
| TGA-16 | Use the full sixteen-bit colormap origin for raw source indices. | [tests/loader/builtin/2248_tga_origin_index16_raw_numeric.t](../../../tests/loader/builtin/2248_tga_origin_index16_raw_numeric.t) |
| TGA-17 | Use the full sixteen-bit colormap origin in RLE packets. | [tests/loader/builtin/2249_tga_origin_index16_rle_numeric.t](../../../tests/loader/builtin/2249_tga_origin_index16_rle_numeric.t) |
| TGA-18 | Rebase source index 255 without overflowing eight-bit PAL8 slots. | [tests/loader/builtin/2250_tga_origin_byte_max_numeric.t](../../../tests/loader/builtin/2250_tga_origin_byte_max_numeric.t) |
| TGA-19 | Resolve source index 65535 without wrapping the colormap range. | [tests/loader/builtin/2251_tga_origin_word_max_numeric.t](../../../tests/loader/builtin/2251_tga_origin_word_max_numeric.t) |
| TGA-20 | Preserve the zero-based transparent palette key after index rebasing. | [tests/loader/builtin/2252_tga_origin_alpha_numeric.t](../../../tests/loader/builtin/2252_tga_origin_alpha_numeric.t) |
| TGA-21 | Map an index below the declared origin to the first stored color. | [tests/loader/builtin/2253_tga_origin_below_numeric.t](../../../tests/loader/builtin/2253_tga_origin_below_numeric.t) |
| TGA-22 | Map an index above the declared palette range to its first color. | [tests/loader/builtin/2254_tga_origin_above_numeric.t](../../../tests/loader/builtin/2254_tga_origin_above_numeric.t) |
| TGA-23 | Preserve the RGB index fallback after an invalid PAL8 index. | [tests/loader/builtin/2255_tga_origin_pal_fallback_numeric.t](../../../tests/loader/builtin/2255_tga_origin_pal_fallback_numeric.t) |
| TGA-24 | Read grayscale palette entries directly after the image ID. | [tests/loader/builtin/2256_tga_origin_gray_numeric.t](../../../tests/loader/builtin/2256_tga_origin_gray_numeric.t) |
| TGA-25 | Read packed RGB16 palette entries directly after the image ID. | [tests/loader/builtin/2257_tga_origin_rgb16_numeric.t](../../../tests/loader/builtin/2257_tga_origin_rgb16_numeric.t) |

### Quality regression tests

| ID | Quality floor protected | Owning test |
| --- | --- | --- |
| TGAQ-01 | A type-2 direct-color image retains its end-to-end visual floor. | [tests/loader/builtin/0010_lsqa_format_tga_type2_rgb.t](../../../tests/loader/builtin/0010_lsqa_format_tga_type2_rgb.t) |
| TGAQ-02 | A type-10 RLE truecolor image retains its end-to-end visual floor. | [tests/loader/builtin/0014_lsqa_format_tga_type10_rgb.t](../../../tests/loader/builtin/0014_lsqa_format_tga_type10_rgb.t) |
| TGAQ-03 | A type-3 grayscale image retains its end-to-end visual floor. | [tests/loader/builtin/0011_lsqa_format_tga_type3_gray.t](../../../tests/loader/builtin/0011_lsqa_format_tga_type3_gray.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| TGA-90 | A truncated declared palette is rejected without emitting a frame. | [tests/loader/builtin/1954_loader_builtin_tga_truncated_palette_reject.t](../../../tests/loader/builtin/1954_loader_builtin_tga_truncated_palette_reject.t) |
| TGA-91 | A short direct-color raster is rejected instead of accepting zero-filled tail pixels. | [tests/loader/builtin/1990_loader_builtin_tga_truncated_direct_reject.t](../../../tests/loader/builtin/1990_loader_builtin_tga_truncated_direct_reject.t) |
| TGA-92 | A packet-RLE count that exceeds the remaining raster is rejected before pixel expansion. | [tests/loader/builtin/1991_loader_builtin_tga_rle_overrun_reject.t](../../../tests/loader/builtin/1991_loader_builtin_tga_rle_overrun_reject.t) |
| TGA-93 | A plausible direct-color header carrying the conflicting color-map flag is rejected rather than colliding with the TGA recognizer. | [tests/loader/builtin/2039_loader_builtin_tga_header_collision_reject.t](../../../tests/loader/builtin/2039_loader_builtin_tga_header_collision_reject.t) |

Malformed TGA header collision, RLE overrun, and truncated direct-pixel input now have focused rejection owners rather than relying on an LSQA floor or unrelated format test. Horizontal right-to-left origin remains explicitly unsupported and is therefore not locked in as a successful compatibility behavior. The builtin fuzz matrix forces all six accepted image types with structurally valid minimal headers, so mutations reach raw/RLE and indexed/truecolor/grayscale dispatch instead of depending on accidental TGA recognition.

### Extension color behavioral tests

The two-by-two fixtures are constructed in memory. RGB expectations use original component codes, explicit power/sRGB equations, and 16-bit table words, independently of the parser and CMS. Builtin values use a `2e-6` absolute budget; native CMS uses `3e-4` to include matrix/TRC profile quantization. These are value contracts, not perceptual quality floors. All managed cases request byte eligibility from the component to verify that color conversion still promotes precision; native cases also require a trace of the selected backend's successful apply.

| ID | Contract protected | Owning test |
| --- | --- | --- |
| TGAC-01 | Decode gamma 2 as the square of normalized source codes. | [tests/loader/builtin/tga_color/0001_gamma2_numeric.t](../../../tests/loader/builtin/tga_color/0001_gamma2_numeric.t) |
| TGAC-02 | Treat gamma 1 as linear source values. | [tests/loader/builtin/tga_color/0002_gamma1_numeric.t](../../../tests/loader/builtin/tga_color/0002_gamma1_numeric.t) |
| TGAC-03 | Retain a fractional gamma ratio (22/10). | [tests/loader/builtin/tga_color/0003_gamma_fraction_numeric.t](../../../tests/loader/builtin/tga_color/0003_gamma_fraction_numeric.t) |
| TGAC-04 | Apply gamma after both run and raw RLE packets. | [tests/loader/builtin/tga_color/0004_gamma_rle_numeric.t](../../../tests/loader/builtin/tga_color/0004_gamma_rle_numeric.t) |
| TGAC-05 | Normalize grayscale through the same gamma definition. | [tests/loader/builtin/tga_color/0005_gamma_gray_numeric.t](../../../tests/loader/builtin/tga_color/0005_gamma_gray_numeric.t) |
| TGAC-06 | Interpret expanded packed 5:5:5 codes before gamma. | [tests/loader/builtin/tga_color/0006_gamma_rgb16_numeric.t](../../../tests/loader/builtin/tga_color/0006_gamma_rgb16_numeric.t) |
| TGAC-07 | Expand an origin-relative eight-bit palette before conversion. | [tests/loader/builtin/tga_color/0007_gamma_index8_numeric.t](../../../tests/loader/builtin/tga_color/0007_gamma_index8_numeric.t) |
| TGAC-08 | Retain sixteen-bit palette indices before conversion. | [tests/loader/builtin/tga_color/0008_gamma_index16_numeric.t](../../../tests/loader/builtin/tga_color/0008_gamma_index16_numeric.t) |
| TGAC-09 | Keep hidden RGB and an exact zero-alpha mask. | [tests/loader/builtin/tga_color/0009_gamma_hidden_alpha_numeric.t](../../../tests/loader/builtin/tga_color/0009_gamma_hidden_alpha_numeric.t) |
| TGAC-10 | Composite gamma-tagged RGB against a linearized background. | [tests/loader/builtin/tga_color/0010_gamma_background_numeric.t](../../../tests/loader/builtin/tga_color/0010_gamma_background_numeric.t) |
| TGAC-11 | Apply independent ARGB table columns without byte rounding. | [tests/loader/builtin/tga_color/0011_table_rgb_numeric.t](../../../tests/loader/builtin/tga_color/0011_table_rgb_numeric.t) |
| TGAC-12 | Prefer the table over a simultaneous scalar gamma. | [tests/loader/builtin/tga_color/0012_table_precedence_numeric.t](../../../tests/loader/builtin/tga_color/0012_table_precedence_numeric.t) |
| TGAC-13 | Read a table preceding the extension area. | [tests/loader/builtin/tga_color/0013_table_before_extension_numeric.t](../../../tests/loader/builtin/tga_color/0013_table_before_extension_numeric.t) |
| TGAC-14 | Apply table values after both run and raw RLE packets. | [tests/loader/builtin/tga_color/0014_table_rle_numeric.t](../../../tests/loader/builtin/tga_color/0014_table_rle_numeric.t) |
| TGAC-15 | Keep indexed alpha until table mapping and float finalization. | [tests/loader/builtin/tga_color/0015_table_index_alpha_numeric.t](../../../tests/loader/builtin/tga_color/0015_table_index_alpha_numeric.t) |
| TGAC-16 | Map grayscale through each independent RGB table column. | [tests/loader/builtin/tga_color/0016_table_gray_numeric.t](../../../tests/loader/builtin/tga_color/0016_table_gray_numeric.t) |
| TGAC-17 | Use 16-bit corrected alpha in linear background composition. | [tests/loader/builtin/tga_color/0017_table_alpha_background_numeric.t](../../../tests/loader/builtin/tga_color/0017_table_alpha_background_numeric.t) |
| TGAC-18 | Keep CMS-disabled input on the unchanged byte route. | [tests/loader/builtin/tga_color/0018_cms_none_numeric.t](../../../tests/loader/builtin/tga_color/0018_cms_none_numeric.t) |
| TGAC-19 | Keep original-format input on the byte route. | [tests/loader/builtin/tga_color/0019_legacy_byte_numeric.t](../../../tests/loader/builtin/tga_color/0019_legacy_byte_numeric.t) |
| TGAC-20 | Treat a zero gamma denominator as no color definition. | [tests/loader/builtin/tga_color/0020_gamma_unused_numeric.t](../../../tests/loader/builtin/tga_color/0020_gamma_unused_numeric.t) |
| TGAC-21 | Keep a footer without an extension on the byte route. | [tests/loader/builtin/tga_color/0021_footer_only_numeric.t](../../../tests/loader/builtin/tga_color/0021_footer_only_numeric.t) |
| TGAC-31 | Reject managed premultiplied-alpha input without publishing a frame. | [tests/loader/builtin/tga_color/0031_premultiplied_reject.t](../../../tests/loader/builtin/tga_color/0031_premultiplied_reject.t) |
| TGAC-32 | Apply gamma through the explicitly selected lcms2 engine. | [tests/loader/builtin/tga_color/0032_gamma_lcms2_numeric.t](../../../tests/loader/builtin/tga_color/0032_gamma_lcms2_numeric.t) |
| TGAC-33 | Apply gamma through the explicitly selected ColorSync engine. | [tests/loader/builtin/tga_color/0033_gamma_colorsync_numeric.t](../../../tests/loader/builtin/tga_color/0033_gamma_colorsync_numeric.t) |
| TGAC-34 | Apply table values through the explicitly selected lcms2 engine. | [tests/loader/builtin/tga_color/0034_table_lcms2_numeric.t](../../../tests/loader/builtin/tga_color/0034_table_lcms2_numeric.t) |
| TGAC-35 | Apply table values through the explicitly selected ColorSync engine. | [tests/loader/builtin/tga_color/0035_table_colorsync_numeric.t](../../../tests/loader/builtin/tga_color/0035_table_colorsync_numeric.t) |
| TGAC-36 | Honor the canonical gamma-float target after normalization. | [tests/loader/builtin/tga_color/0036_gamma_target_numeric.t](../../../tests/loader/builtin/tga_color/0036_gamma_target_numeric.t) |
| TGAC-37 | Preserve grayscale-alpha coverage on the managed route. | [tests/loader/builtin/tga_color/0037_gamma_gray_alpha_numeric.t](../../../tests/loader/builtin/tga_color/0037_gamma_gray_alpha_numeric.t) |
| TGAC-38 | Do not map synthesized opaque alpha through the table. | [tests/loader/builtin/tga_color/0038_table_opaque_alpha_numeric.t](../../../tests/loader/builtin/tga_color/0038_table_opaque_alpha_numeric.t) |

### Extension color defensive coverage

The [extension tests](../../../tests/loader/builtin/tga_color) additionally reject zero/out-of-range declared gamma, invalid extension/table offsets and sizes, table overlap, and truncated raw/RLE data adjacent to valid metadata. They assert failure and zero callbacks. Their full inventory belongs to the [builtin loader test plan](../../testing/builtin-loader-coverage.md#tga).
