# Builtin SIXEL Input Loader

## Format lineage

Digital Equipment Corporation introduced SIXEL as a six-pixel-high bitmap language for printers and later used it in graphics terminals including the VT240, VT330, and VT340. It is a terminal Device Control String (DCS), not a conventional file container: paint commands select mutable color registers and place vertical six-bit masks onto terminal state. The DEC [VT330/VT340 Sixel Graphics chapter](https://manx-docs.org/mirror/vt100.net/docs/vt3xx-gp/chapter14.html) describes the historical model, and libsixel's broader [SIXEL Format](../../sixel-format.md) guide covers wire semantics and encoder examples.

The generic image helper gained SIXEL input in commit [`98b7aee47`](https://github.com/saitoha/libsixel/commit/98b7aee47) in 2014. The current builtin route calls `sixel_decode_raw()` in [`fromsixel.c`](../../../src/fromsixel.c). Parallel decoding was introduced and replaced with the current anchored design in commits [`55ec28ad7`](https://github.com/saitoha/libsixel/commit/55ec28ad7) and [`71722f608`](https://github.com/saitoha/libsixel/commit/71722f608) in 2026; OR-mode paint support followed in [`68214059e`](https://github.com/saitoha/libsixel/commit/68214059e) and [`dd8a8366e`](https://github.com/saitoha/libsixel/commit/dd8a8366e).

## DCS envelope and recognition

The ordinary seven-bit envelope is:

```text
ESC P [P1 [; P2 [; P3]]] q SIXEL-DATA ESC \
```

The eight-bit C1 form uses DCS `0x90` and may use ST `0x9c`. The builtin predicate requires either `ESC P` or `0x90` at the beginning and scans DCS parameter/intermediate bytes until it finds final byte `q`; CAN/SUB before `q` rejects recognition. Filename suffix is irrelevant.

This is recognition of one leading SIXEL DCS, not a terminal-stream parser. Bytes before the DCS, cursor movement around it, palette state inherited from earlier terminal traffic, several DCS images, and surrounding text/control sequences are not replayed as a terminal session.

## Header parameters and raster attributes

| Field | Wire meaning | Builtin raw-loader behavior |
| --- | --- | --- |
| `P1` | Legacy pixel-aspect macro | Maps to internal `Pan`/`Pad` defaults. The raw loader does not resample the returned image merely because the pixels have a non-square aspect. |
| `P2=0`, omitted, `1`, or `2` | Historical erase/transparent-background behavior on a terminal surface | There is no existing terminal surface to retain or erase. Unpainted positions remain raw decoder background/sentinel state rather than receiving a queried terminal background. |
| `P2=5` | libsixel nonstandard OR-mode extension | Enables OR-mode bit-plane accumulation so overlapping paints combine register bits. |
| `P3` | Horizontal grid-size parameter | Scales internal aspect values as the implementation's compatibility model, but does not by itself resize returned geometry. Zero is treated as 10. |
| `" Pan;Pad;Ph;Pv` | Raster aspect and declared extent | `Pan`/`Pad` update aspect state; positive `Ph`/`Pv` establish at least that canvas size. Actual paint outside the declaration is preserved by the raw loader rather than clipped. |

Without positive `Ph`/`Pv`, width and height are inferred from cursor movement and the furthest painted/reached position. Declared extents can add blank trailing area. Later paint can expand beyond them because the builtin loader calls the default raw API without the separate `TRUST_RASTER_SIZE` option available to newer decoder APIs.

## Paint language and compression

A data byte from `?` through `~` represents one column. Subtracting `0x3f` gives a six-bit vertical mask; bit 0 is the top pixel. Set bits paint with the current register, and every data byte advances the horizontal position even when its mask is zero.

| Control | Builtin action |
| --- | --- |
| `!Pn<char>` | Repeats the following SIXEL data character. Missing/zero count becomes 1; counts above 65535 are rejected. Cursor and allocation arithmetic are overflow-checked before expansion. |
| `#Pc` | Selects a color register. Values are clamped to the decoder's internal range. |
| `#Pc;1;H;L;S` | Defines HLS with hue clamped to 360 and lightness/saturation clamped to 100, then selects the register. |
| `#Pc;2;R;G;B` | Defines RGB percentages clamped to 100, then selects the register. |
| `$` | Returns horizontal position to the beginning of the current six-row band. |
| `-` | Returns horizontal position and advances vertical position by six pixels. |
| `ESC \\` / ST | Ends the DCS. |

SIXEL compression is therefore command-level run repetition, not a compressed pixel file with an independent entropy stream. The decoder expands paint into a canvas while parsing; malformed numeric overflow, impossible dimensions, and allocation failure stop the load.

## Palette state, high color, and OR-mode dialect

DEC defined up to 256 register numbers, although physical terminals often exposed fewer. The parser has a wider internal register table for extensions, while `sixel_decode_raw()` deliberately returns one-byte indexes plus the final palette. That representation is faithful only when each painted byte can name a stable final palette entry.

A high-color stream can redefine and reuse a register after earlier pixels were painted. The direct/stateful decoder APIs detect that history and can preserve paint-time RGB, but the builtin image loader does not call them: it calls `sixel_decode_raw()` and receives only the final register table. Earlier pixels can therefore be recolored by the last definition. Register numbers above the byte-index range likewise do not form a faithful builtin-loader contract. Ordinary stable-palette streams using at most 256 registers are the supported representation.

`P2=5` OR mode is a libsixel extension in which overlapping paint accumulates register bits. The parser keeps auxiliary wide OR state while painting and resolves it into the raw result. It is not DEC's ordinary overwrite model and should not be expected on unrelated terminals. The builtin loader accepts it because it shares the library decoder.

The decoder starts with the DEC default palette and color register 15. Definitions update this local table; it does not import palette state from a live terminal or persist definitions into another input image.

## Transparency and unpainted pixels

SIXEL has no RGBA sample. In transparent-background semantics, the information is “this canvas position was not painted,” which requires a paint mask or a sentinel distinct from a valid index. The public raw API defines any returned byte greater than or equal to `ncolors` as transparent/unpainted.

The builtin loader does not request the optional paint mask. When it keeps `PAL8`, out-of-range sentinel bytes remain in the index plane and consumers must not index the palette with them as ordinary colors. When palette fusion is disabled or the color count exceeds `-p`, the wrapper expands every byte through the returned palette table; that RGB expansion cannot preserve the unpainted distinction. `-A` cannot recreate a mask that this route did not request.

This limitation is specific to the builtin image-loader adapter. `sixel_decode_raw_with_options_mask()`, `sixel_decode_direct()`, and `sixel_decode_pixels()` have richer transparency/result contracts and are documented in [SIXEL Decoding Pipeline](../../functionality/decoding-pipeline.md).

## Serial and parallel decode pipeline

1. The builtin dispatcher recognizes the leading DCS and creates a one-frame object.
2. `sixel_decode_raw()` initializes a byte-index canvas and local default palette, then a serial parser validates the DCS header, raster attributes, and palette state.
3. Once a reliable raster extent and palette anchor exist, a threaded build may ask the parallel decoder to validate spans and prove disjoint row ownership. Only then are paint workers released. Any unsafe split or worker-creation failure falls back to a clean serial paint path.
4. All workers are joined before `sixel_decode_raw()` returns. Parallel work is internal concurrency, not an asynchronous loader callback or a separately scheduled encoder-DAG node.
5. The loader keeps gamma `PAL8` when palette fusion is allowed and the reported color count fits the request; otherwise it expands indexes through the final palette to gamma `RGB888`.
6. The one completed frame enters the ordinary encoder pipeline.

The raw loader does not perform CMS, Exif orientation, resize, crop, sampling, quantizer initialization, final palette merge, lookup-policy LUT construction, or per-pixel dithering. Those are later stages even when timeline logging makes them appear in one end-to-end run.

### Implementation and test map

![A vertical implementation map of the builtin SIXEL input path. It follows leading-DCS recognition into the raw decoder state machine, register and paint processing, the conditional parallel span validation and serial fallback, and the final PAL8-or-RGB frame decision. Every node carries a coverage ID used by the tables below.](pipeline-figures/sixel.svg)

The arrows are control and data dependencies, not asynchronous task boundaries. `sixel_decode_raw()` may use worker threads internally, but it joins them before returning to `sixel_builtin_load_with_builtin_impl()`. The function names are search anchors in the current implementation; the coverage IDs are the stable documentation anchors.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `SIX-01` | `sixel_builtin_detect_decode_path()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Input chunk → dedicated SIXEL path | Only one leading DCS is recognized; other terminal-stream bytes are outside this adapter. |
| `SIX-02` | `sixel_decode_raw()` → `sixel_decode_raw_impl()` → `parser_context_enter_decsixel()` in [`fromsixel.c`](../../../src/fromsixel.c) | DCS bytes → checked parser/canvas state | Header parameters, raster extent, repeat arithmetic, and cursor growth must establish a bounded byte-index canvas. |
| `SIX-03` | `hls_to_rgb()`, `image_buffer_store_pixel()`, and `image_buffer_ormode_store_sixel()` in [`fromsixel.c`](../../../src/fromsixel.c) | Register definitions and paint commands → index canvas plus final palette | RGB/HLS definitions, overwrite paint, repeats, and the `P2=5` OR-mode extension must agree on register semantics. |
| `SIX-04` | `sixel_decoder_parallel_request_start()` and `sixel_decoder_parallel_direct_rows_disjoint()` in [`decoder-parallel.c`](../../../src/decoder-parallel.c) | Anchored serial prefix → proven-disjoint worker spans or serial fallback | Workers must not paint until span ownership is safe; failed or unsafe parallel work must leave a clean serial restart. |
| `SIX-05` | `sixel_builtin_load_with_builtin_impl()` and `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Index canvas/final palette → gamma `PAL8` or expanded `RGB888` frame | Palette fusion and later geometry constraints must select the documented representation without turning internal concurrency into an async callback. |

The committed SVG and its machine-readable stage/test manifest are generated by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py). `python3 tools/plot_builtin_loader_format_figures.py --check` verifies exact regeneration, every named implementation symbol, coverage ID presence, and both directions of the document/test links.

## Options and observable differences

```console
img2sixel -Lbuiltin! input.six
SIXEL_THREADS=1 img2sixel -Lbuiltin! input.six
```

| Control | Exact effect on SIXEL input |
| --- | --- |
| `-p COLORS` | If the decoded `ncolors` exceeds the request, the loader expands through the final palette to `RGB888`; otherwise palette fusion may keep `PAL8`. It does not requantize inside the loader. |
| Resize options and non-default encoder color modes | Disable loader palette fusion, so raw indexes are expanded to `RGB888` before later processing. This can lose the unpainted sentinel distinction. |
| `SIXEL_THREADS=COUNT|auto` | Supplies the internal decoder worker budget in a threaded build. One selects serial behavior; larger resolved values permit anchored scan/paint workers. The load still returns synchronously after joins. |
| `-A auto|composite|clear|keep`, `-B`, `background_policy`, `background_colorspace` | Do not reconstruct a missing paint mask or terminal backing surface in this adapter. They have no reliable format-specific effect on raw SIXEL input. |
| `cms_engine`, `cms_target`, `cms_intent`, `prefer_8bit` | No effect. Register colors are decoded as gamma RGB bytes with no embedded profile. |
| `builtin:orientation` | No effect. SIXEL has no Exif orientation; raster position is consumed by the paint parser. |
| `-S`, `-T`, `-l`, `-g` | No effect. One recognized DCS produces one still frame. Several DCS strings are not animation. |
| `trns_keycolor`, HDR, PNM, BMP, and PSD-specific suboptions | No effect. |

## Unsupported behavior and security boundary

The builtin adapter does not replay a terminal, inherit or persist palette registers, compose against existing screen pixels, return several DCS images, preserve byte-level paint order for round trip, faithfully represent palette redefinition after paint, guarantee registers above 255, expose a separate paint mask, apply pixel-aspect resampling, or perform dequantization/k-undither.

Numeric parameters, repeats, raster declarations, register numbers, cursor motion, and canvas growth are attacker-controlled. Internal parallel validation is a performance optimization and correctness guard, not isolation. `-Lbuiltin!` narrows fallback but does not sandbox the decoder.

## Implementation landmarks

- Loader recognition, raw adapter, palette-fusion decision, and frame delivery: [`loader-builtin.c`](../../../src/loader-builtin.c)
- DCS state machine, palette, paint, OR mode, and raw/direct APIs: [`fromsixel.c`](../../../src/fromsixel.c)
- Parallel span validation and paint: [`decoder-parallel.c`](../../../src/decoder-parallel.c)
- Full wire-language reference: [SIXEL Format](../../sixel-format.md)
- Richer decode/output policies: [SIXEL Decoding Pipeline](../../functionality/decoding-pipeline.md)
- Parallel execution model: [Decoder Threading](../../threading/decoder.md)
- Broader non-owning decoder suite: [`tests/processing/decoder`](../../../tests/processing/decoder)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than a complete decoder inventory. The [related SIXEL suites](../../testing/builtin-loader-coverage.md#related-sixel-suites) section explains the boundary between the builtin adapter, reusable decoder, and `sixel2png` coverage.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| SIX-01 | `img2sixel` recognizes SIXEL input through the image-loader path and can pass the decoded frame into later resize and palette-limit work. | [tests/processing/geometry/0009_sixel_resize_palette_limit.t](../../../tests/processing/geometry/0009_sixel_resize_palette_limit.t) |
| SIX-02 | The raw parser accepts the `P2=5` dialect and resolves overlapping selectors into the expected byte index and palette entry. | [tests/processing/decoder/0002_decoder_ormode_raw_overlay.t](../../../tests/processing/decoder/0002_decoder_ormode_raw_overlay.t) |
| SIX-03 | Repeat expansion and OR-mode paint use the same register-bit accumulation contract. | [tests/processing/decoder/0005_decoder_ormode_repeat_overlay.t](../../../tests/processing/decoder/0005_decoder_ormode_repeat_overlay.t) |
| SIX-04 | A valid anchored split produces byte-identical serial and parallel output across a sixel newline boundary. | [tests/processing/decoder/0001_decoder_parallel_split_after_newline.t](../../../tests/processing/decoder/0001_decoder_parallel_split_after_newline.t) |
| SIX-05 | The adapter preserves the raw unpainted sentinel in `PAL8`, expands it through the backing palette in `RGB888`, and exposes the documented final-palette collapse after register redefinition. | [tests/loader/builtin/1986_loader_builtin_sixel_unpainted_numeric.t](../../../tests/loader/builtin/1986_loader_builtin_sixel_unpainted_numeric.t), [tests/loader/builtin/1987_loader_builtin_sixel_high_color_numeric.t](../../../tests/loader/builtin/1987_loader_builtin_sixel_high_color_numeric.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| SIX-90 | A failed parallel attempt does not leak partially painted worker state into the serial fallback image. | [tests/processing/decoder/0016_decoder_parallel_fallback_keeps_image_clean.t](../../../tests/processing/decoder/0016_decoder_parallel_fallback_keeps_image_clean.t) |

Coverage audit note: these owners cover routing, representative raw/OR paint, one safe parallel split, dirty-worker fallback, both adapter representations of an unpainted pixel, and the final-palette limitation after register redefinition. They still do not make the builtin adapter a terminal replay model or add a separate paint mask.
