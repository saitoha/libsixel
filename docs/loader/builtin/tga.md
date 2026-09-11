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

The header's first-colormap-entry value is not implemented as a logical index base. Files using a nonzero origin are therefore outside the supported semantic subset even where the byte parser can continue. This is important because the field is not simply cosmetic; compliant indices are relative to that declared origin.

## RLE coding and orientation

TGA RLE divides the raster into packets of 1 through 128 pixels. A command with bit 7 clear is a raw packet followed by that many distinct samples; bit 7 set is a run packet followed by one sample repeated that many times. Packets can cross scanline boundaries because decode is over the linear pixel count. The decoder validates allocation arithmetic and source availability through the stb context, then performs vertical row reversal when the descriptor declares bottom-left origin.

Horizontal right-to-left order is left unchanged. TGA 2.0 scan-line tables are not used to seek rows; all accepted pixel data is decoded sequentially from the image-data offset.

## Alpha and palette fast-path details

Thirty-two-bit direct truecolor is the only non-indexed TGA form for which the builtin wrapper explicitly requests RGBA and retains source alpha. Common finalization then composites it against a resolved output background or converts alpha into the frame's transparency mask according to `-A`.

For an indexed 8-bit TGA with a 32-bit palette, the fast path chooses the first fully transparent palette entry as the single key-color index and remaps pixels using any other fully transparent entry to that key. Partial palette alpha cannot be represented in `PAL8`; each such palette color is flattened with integer gamma-space blending against the provided background, or black when no background is available. Thus the palette path preserves binary transparency but not arbitrary per-entry partial alpha.

If palette fusion is disabled, indexed input is expanded by the general path. That path is requested as three-component RGB, so palette alpha does not survive. This differs from 32-bit direct truecolor and is a current builtin boundary, not a property of the TGA format.

## TGA 2.0 and extension fields

The TGA 2.0 footer, extension-area offset, developer directory, author/comment fields, timestamp, job information, software ID, key color, pixel-aspect ratio, gamma, color-correction table, postage-stamp image, scan-line table, and attribute-type field are not interpreted. In particular, extension gamma/color-correction data does not establish a source colorspace, and the attribute-type field does not override the decoder's 15/16/32-bit decisions.

Trailing data can therefore coexist with a successfully decoded raster, but it is neither validated as a complete 2.0 object nor preserved for round trip.

## Builtin decode and output pipeline

1. Residual dispatch reaches the weak-signature TGA predicate only after strong signatures such as PNG, JPEG, GIF, PSD, HDR, BMP, PIC, and WebP have had their dedicated routing.
2. With palette fusion enabled, the builtin wrapper first attempts the narrow indexed-8 path and creates `PAL8` when it succeeds.
3. Otherwise stb decodes raw/RLE samples and converts them to a requested three-component buffer, except that recognized 32-bit direct truecolor requests four components.
4. BGR ordering and vertical origin are normalized. There is no TGA CMS or metadata-orientation stage.
5. Direct RGBA or a palette key enters common alpha/background finalization; opaque/general RGB is delivered directly as one frame.

The loader output is always eight-bit gamma RGB/PAL8 plus optional transparency state. Later resize, crop, colorspace conversion, palette initialization, and quantization are encoder stages.

### Implementation and test map

![A vertical implementation map of the builtin TGA loader. It follows the signature-less structural probe into an optional indexed-palette fast path or the adapted raw and packet-RLE raster decoder, vertical-origin normalization, and direct or palette alpha finalization. Every node carries a coverage ID used by the tables below.](pipeline-figures/tga.svg)

TGA is the one map here whose first node is necessarily a weak probe. The indexed fast path and general `stbi__tga_load()` path are alternatives; their alpha behavior is not equivalent, so the pipeline preserves that fork instead of presenting a fictional universal RGBA intermediate.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `TGA-01` | `stbi__tga_test()` and `stbi__tga_info()` in [`stb_image.h`](../../../src/stb_image.h) | Residual input bytes → plausible TGA header model | Image type, colormap flag, depths, and positive dimensions must be mutually plausible, while strong-signature formats retain routing priority. |
| `TGA-02` | `sixel_builtin_try_load_indexed_tga()` in [`loader-builtin.c`](../../../src/loader-builtin.c), calling `stbi__tga_load_palette()` | Indexed type with fusion enabled → `PAL8`, RGB palette, optional key | Only 8-bit indices and at most 256 entries qualify; multiple binary-transparent entries and partial alpha must be normalized as documented. |
| `TGA-03` | `stbi__tga_load()` and `stbi__tga_read_rgb16()` in [`stb_image.h`](../../../src/stb_image.h) | Raw or packet-RLE source pixels → expanded RGB/RGBA bytes | Raw/run packets, palette indices, grayscale, 5:5:5, BGR, and BGRA component order must consume exactly the raster pixel count. |
| `TGA-04` | Row-order branch inside `stbi__tga_load()` in [`stb_image.h`](../../../src/stb_image.h) | File-order decoded rows → canonical top-to-bottom rows | Bottom-origin rows must reverse exactly once; the unsupported right-to-left bit must not be documented as an implemented mirror. |
| `TGA-05` | `sixel_builtin_apply_tga_truecolor_alpha_policy()` and `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Direct RGBA or palette transparency → gamma RGB/PAL8 plus mask/key or composite | Direct 32-bit alpha must reach common policy; alpha discarded by the general indexed fallback must not be claimed as recoverable. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin! indexed.tga
img2sixel -Lbuiltin! -A keep -B '#202020' rgba32.tga
```

| Control | Exact effect on TGA |
| --- | --- |
| `-A auto|composite|clear|keep` | Applies to 32-bit direct truecolor and to binary transparent-key output from the indexed fast path. It cannot recover alpha discarded by the RGB fallback. |
| `-B COLOR`, `background_colorspace` | Supplies the output background for direct alpha finalization. On the indexed fast path, partial palette alpha is flattened against the supplied byte color; absent background means black. |
| `background_policy` | TGA has no interpreted file-background candidate, so it does not choose between two TGA background sources. |
| `-p COLORS` | Does not itself disable palette fusion. The TGA fast path can return the complete source palette even when later encoder policy requests fewer output colors; later palette processing must reduce it. This differs from builtin PNG's loader-side `palette_colors <= reqcolors` guard. |
| Resize options and non-default encoder color modes | Disable loader palette fusion, forcing indexed TGA through RGB expansion. In that path 32-bit palette alpha is not retained. |
| `cms_engine`, `cms_target`, `cms_intent`, `prefer_8bit` | No effect on TGA decode because no TGA color metadata is interpreted. Later encoder conversions remain possible. |
| `builtin:orientation` | No effect. Vertical origin is always handled by the TGA raster parser; Exif/XMP orientation is absent. Horizontal origin remains unsupported. |
| `-S`, `-T`, `-l`, `-g`, `trns_keycolor`, HDR, PNM, BMP, and PSD-specific suboptions | No effect. TGA is emitted as one frame. |

## Unsupported behavior and security boundary

Nonzero colormap origins, horizontal right-to-left storage, extension/developer semantics, high-depth samples, alpha-preserving gray-alpha, alpha-preserving indexed fallback, ICC/profile interpretation, and animation are not supported as end-to-end contracts. The descriptor alpha-bit count is not used to turn 15/16-bit direct pixels into alpha.

Weak recognition, dimensions, palette sizes, index values, and RLE commands are attacker-controlled. Because TGA lacks a strong magic, fallback order can affect which parser sees a malformed input. `-Lbuiltin!` narrows the component set but does not sandbox the adapted stb decoder.

## Implementation landmarks

- TGA predicate, indexed parser, and general decode: [`stb_image.h`](../../../src/stb_image.h)
- Palette fusion, palette-alpha folding, direct-alpha selection, and frame delivery: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Alpha/background contract: [Alpha Policy](../alpha-policy.md) and [Background Policy](../background-policy.md)
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [TGA coverage inventory](../../testing/builtin-loader-coverage.md#tga) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| TGA-01 | A structurally valid type-2 direct-color TGA is recognized and meets the expected quality floor. | [tests/loader/builtin/0010_lsqa_format_tga_type2_rgb.t](../../../tests/loader/builtin/0010_lsqa_format_tga_type2_rgb.t) |
| TGA-02 | An eligible indexed TGA selects the palette-preserving builtin path. | [tests/loader/builtin/0050_loader_builtin_palette_tga_path.t](../../../tests/loader/builtin/0050_loader_builtin_palette_tga_path.t) |
| TGA-03 | A type-10 packet-RLE truecolor TGA expands to the expected image. | [tests/loader/builtin/0014_lsqa_format_tga_type10_rgb.t](../../../tests/loader/builtin/0014_lsqa_format_tga_type10_rgb.t) |
| TGA-04 | A grayscale TGA with the fixture's declared origin reaches the canonical expected image. | [tests/loader/builtin/0011_lsqa_format_tga_type3_gray.t](../../../tests/loader/builtin/0011_lsqa_format_tga_type3_gray.t) |
| TGA-05 | Direct BGRA alpha composites against an explicit background with the expected numeric result. | [tests/loader/builtin/0710_loader_builtin_tga_rgba_background_numeric.t](../../../tests/loader/builtin/0710_loader_builtin_tga_rgba_background_numeric.t) |

### Defensive and malformed-input tests

There is currently no dedicated owning test that isolates malformed TGA header collision, RLE overrun, or horizontal-origin rejection. The broad loader suite exercises successful TGA variants, but this remains an explicit coverage gap rather than evidence supplied by an unrelated format test.
