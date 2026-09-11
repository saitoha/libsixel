# Builtin GIF Loader

## Format lineage

CompuServe published GIF87a in 1987 and GIF89a in 1990. The latter retained the indexed LZW image core and formalized control and application extensions used for transparency and animation. The wire contract is preserved in the [GIF89a specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt).

libsixel originally obtained single-image GIF support through stb_image. That API could not express the complete animation sequence needed by `img2sixel`, so commits [`30b80e140`](https://github.com/saitoha/libsixel/commit/30b80e140) and [`1d92429d7`](https://github.com/saitoha/libsixel/commit/1d92429d7) added and selected [`fromgif.c`](../../../src/fromgif.c) in 2015. The current decoder still has stb ancestry, but owns logical-canvas composition, frame callbacks, disposal, timing, looping, palette preservation, cancellation, and replay caching. `STBI_NO_GIF` disables the old residual stb path.

## Container, color model, and compression

A GIF data stream begins with `GIF87a` or `GIF89a`, followed by a Logical Screen Descriptor, an optional Global Color Table, a sequence of extensions and image descriptors, and a trailer. Each image descriptor selects a rectangle on the logical screen and may provide a Local Color Table that overrides the global table for that image.

GIF is always indexed. A color table has 2 through 256 RGB entries with eight bits per component. Transparency is binary and is attached by a Graphic Control Extension to one palette index; GIF has no partial-alpha sample. Because successive images may use unrelated local palettes, the decoder composes into a full RGB logical-screen canvas rather than treating the source index value as stable across frames.

Raster indices are compressed with GIF LZW. The stream supplies a minimum code size, clear and end codes, grows the dictionary code width up to 12 bits, and stores codes in length-delimited sub-blocks. The decoder validates code growth, dictionary references, sub-block termination, and rectangle bounds. Interlaced images are reconstructed in the four GIF passes beginning at rows 0, 4, 2, and 1 with strides 8, 8, 4, and 2.

## Extension and dialect handling

| Block | Builtin treatment |
| --- | --- |
| Graphic Control Extension (`0xf9`) | Applies delay, transparent index, and disposal to the following image. A missing control block gives the frame the libsixel default delay of 1 centisecond. |
| `NETSCAPE2.0` Application Extension | Sub-block type 1 sets the playback count. Zero means indefinite looping. |
| Other Application Extensions | Their sub-block chains are validated and skipped. Private protocols, XMP, and vendor loop conventions are not interpreted. |
| Comment Extension (`0xfe`) | Validated and skipped; text is not exposed. |
| Plain Text Extension (`0x01`) | Validated and skipped; glyphs are not rendered onto the canvas. |
| Unknown extension label | Its sub-block chain is consumed and ignored. |

The Logical Screen pixel-aspect byte and color-table sort flags do not affect output. `GIF87a` is accepted even though animation control normally relies on 89a extensions. Unknown or skipped extensions are not preserved for re-encoding.

## Frame-composition pipeline

The builtin dispatcher recognizes either GIF version signature, counts frames when a start index must be resolved, and enters `load_gif()`. A lightweight stream scan determines whether transparency and multiframe buffers are needed; a scan failure is advisory and switches to conservative buffers rather than accepting malformed decode data.

For every image, the decoder expands its LZW indices through the active color table and composites the rectangle at its left/top offset into the full logical canvas. The frame callback therefore always receives logical-screen dimensions. Before the next image it applies the previous image's disposal:

| Disposal value | Result |
| --- | --- |
| 0 or 1 | Keep the composed canvas. |
| 2 | Restore the previous image rectangle to the resolved background. |
| 3 | Restore the saved pre-image canvas and alpha state. |
| 4–7 | Reserved values are not assigned new semantics. |

When no explicit output background is selected and the stream uses transparency, a parallel alpha state preserves zero-alpha pixels. With a background, composition resolves those pixels according to background policy. A file background is the Logical Screen background index in the Global Color Table; it is unavailable when that table/index is unusable.

After composition, the decoder attempts `PAL8` promotion only when palette use is enabled, the complete composed canvas can be represented by no more than the requested number of colors, and its binary transparency can be expressed by one key index. Otherwise it returns gamma RGB plus transparency state for common finalization. Palette preservation is therefore an output optimization and fidelity path, not a promise to return the source's local indices.

Frame delay remains in centiseconds. `fromgif` records the stream loop count and libsixel loop number separately. A replay cache may retain first-loop frames when repeated playback is needed; its default data cap is 64 MiB, and failure to cache falls back to decoding the stream again.

### Implementation and test map

![A vertical implementation map of the builtin GIF loader. It follows GIF signature routing into logical-screen and extension scanning, LZW raster expansion, persistent-canvas composition and disposal, then frame selection, loop handling, and PAL8-or-RGB emission. Every node carries a coverage ID used by the tables below.](pipeline-figures/gif.svg)

The map separates source indices from the emitted frame. Local-table indices are consumed by `gif_process_raster()` and painted into a logical-screen canvas; `gif_export_pal8_frame()` may build a new palette only after composition proves that the complete canvas is representable. This distinction is central to interpreting the palette and disposal tests.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `GIF-01` | `sixel_builtin_load_gif_frames()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | GIF chunk and loader controls → `load_gif()` callback run | Start-frame, static, loop, background, palette, and cancellation policy must reach the format decoder without creating asynchronous callbacks. |
| `GIF-02` | `gif_load_header()`, `gif_scan_stream_info()`, and `gif_skip_subblocks()` in [`fromgif.c`](../../../src/fromgif.c) | Header/block stream → screen metadata and buffer plan | Global table/background, GCE, Netscape looping, and unknown-extension skipping must leave the next block boundary exact. |
| `GIF-03` | `gif_process_raster()` and `gif_out_code()` in [`fromgif.c`](../../../src/fromgif.c) | LZW sub-block stream plus active table → decoded rectangle pixels | Code-table reset/growth, interlace order, local/global table selection, and transparent-index normalization must agree. |
| `GIF-04` | `gif_decode_one_frame()`, `gif_history_mark()`, and `gif_reset_canvas_for_loop()` in [`fromgif.c`](../../../src/fromgif.c) | Decoded rectangle plus prior canvas → completed logical-screen canvas | Offsets, transparency, disposal 0–3, saved history, dirty regions, and loop reset must reconstruct the visible frame. |
| `GIF-05` | `gif_should_emit_decoded_frame()`, `gif_export_pal8_frame()`, and `gif_export_nonpal_frame()` in [`fromgif.c`](../../../src/fromgif.c) | Completed canvas → selected callback frame | Start-frame and loop decisions must not skip prerequisite composition; PAL8 is permitted only when the final canvas and binary alpha fit. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin! animation.gif
img2sixel -Lbuiltin:background_policy=explicit_first! -B '#202020' animation.gif
```

| Control | Exact effect on GIF |
| --- | --- |
| `-S`, `--static` | Emits the first frame selected by `-T` and stops. Earlier frames are still decoded when required to build the selected composited canvas. |
| `-T N`, `--start-frame=N` | On the first playback loop, skips emission before absolute zero-based `N`; negative values count from the end and require a preliminary frame count. Out-of-range values fail. Later loops start at frame 0. |
| `-l auto|force|disable` | `disable` stops after one pass. `auto` follows a positive `NETSCAPE2.0` count and otherwise follows stream semantics. `force` keeps replaying a multiframe stream until cancellation even when the file declares a finite count. A one-frame stream is never replayed as animation. |
| `-g`, `--ignore-delay` | Leaves parsed delays intact but makes the encoder skip presentation sleeps. It does not alter GIF parsing or disposal. |
| `-B COLOR` | Supplies an explicit composition/disposal background. Without one, transparent pixels can remain transparent and the file background can participate according to policy. |
| `background_policy=file_first|explicit_first` | Chooses whether a valid Logical Screen background or `-B` wins where GIF requires a background. |
| `background_colorspace=gamma|linear` | Selects how an explicit background is interpreted when alpha composition is performed. |
| `-A auto|composite|clear|keep` | Controls whether zero-alpha output remains transparent or is flattened after GIF composition. It cannot create partial alpha because GIF has none. |
| `-p COLORS` and operations that disable palette fusion | The requested palette limit can keep or prevent the composed `PAL8` fast path. Scaling and non-default encoder color modes disable loader palette fusion and force RGB expansion. |
| CMS, orientation, PNG, HDR, PNM, and BMP suboptions | No effect: this GIF path has no interpreted ICC/Exif source and none of those grammars apply. |

`NETSCAPE2.0` counts and `-l` interact at playback time; `-S` wins earlier by stopping after one emitted frame. Cancellation is checked during decode and replay, including forced indefinite looping.

## Unsupported behavior and security boundary

The loader does not render Plain Text, expose comments, interpret private application payloads, honor pixel-aspect resampling, or retain original sub-block and code ordering. It has no support for partial alpha, per-frame ICC profiles, or animation conventions outside the supported Graphic Control plus Netscape model.

The LZW dictionary, sub-block lengths, image rectangles, frame count, and disposal history are all attacker-controlled. Closing the chain with `-Lbuiltin!` makes failure deterministic but does not sandbox the parser. Large or indefinite animations should also be treated as CPU and memory workloads; the cache cap limits retained replay data, not total decode work under `-l force`.

## Implementation landmarks

- GIF parser, LZW decode, canvas, frame selection, and replay: [`fromgif.c`](../../../src/fromgif.c)
- Builtin dispatch and common frame finalization: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Alpha and background semantics: [Alpha Policy](../alpha-policy.md) and [Background Policy](../background-policy.md)
- Broader non-owning format and malformed-stream suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [GIF coverage inventory](../../testing/builtin-loader-coverage.md#gif) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| GIF-01 | A positive start-frame selection emits the requested composited frame rather than renumbering the source stream. | [tests/loader/builtin/0044_builtin_gif_start_frame_positive.t](../../../tests/loader/builtin/0044_builtin_gif_start_frame_positive.t) |
| GIF-02 | A structurally valid unknown extension is consumed without corrupting the following block boundary. | [tests/loader/builtin/0261_loader_builtin_gif_unknown_extension_ignored.t](../../../tests/loader/builtin/0261_loader_builtin_gif_unknown_extension_ignored.t) |
| GIF-03 | A later frame's local color table replaces global-table colors for that frame. | [tests/loader/builtin/1972_loader_builtin_gif_local_palette_numeric.t](../../../tests/loader/builtin/1972_loader_builtin_gif_local_palette_numeric.t) |
| GIF-04 | Disposal method 3 restores the saved pre-frame canvas before the next composition. | [tests/loader/builtin/1971_loader_builtin_gif_disposal3_numeric.t](../../../tests/loader/builtin/1971_loader_builtin_gif_disposal3_numeric.t) |
| GIF-05 | Automatic looping honors a finite Netscape loop count for a multi-frame stream. | [tests/loader/builtin/0265_loader_builtin_gif_loop_auto_loop2.t](../../../tests/loader/builtin/0265_loader_builtin_gif_loop_auto_loop2.t) |
| GIF-06 | The GIF87a signature reaches the same exact-pixel decode path as the later GIF89a dialect. | [tests/loader/builtin/1939_loader_builtin_gif87a_numeric.t](../../../tests/loader/builtin/1939_loader_builtin_gif87a_numeric.t) |
| GIF-07 | Four-pass interlacing maps encoded rows to their exact logical-screen positions. | [tests/loader/builtin/1940_loader_builtin_gif_interlace_four_pass_numeric.t](../../../tests/loader/builtin/1940_loader_builtin_gif_interlace_four_pass_numeric.t) |
| GIF-08 | A nonzero image-descriptor offset composites into the full logical-screen canvas. | [tests/loader/builtin/1941_loader_builtin_gif_rectangle_offset_numeric.t](../../../tests/loader/builtin/1941_loader_builtin_gif_rectangle_offset_numeric.t) |
| GIF-09 | Comment, Plain Text, and unknown Application extension sub-blocks preserve the following image boundary. | [tests/loader/builtin/1943_loader_builtin_gif_extension_subblocks_numeric.t](../../../tests/loader/builtin/1943_loader_builtin_gif_extension_subblocks_numeric.t) |
| GIF-10 | Disposal method 2 clears the previous dirty rectangle before the next frame is composed. | [tests/loader/builtin/1946_loader_builtin_gif_disposal2_numeric.t](../../../tests/loader/builtin/1946_loader_builtin_gif_disposal2_numeric.t) |
| GIF-11 | Literal LZW input remains exact while the dictionary crosses each code-width boundary through 12 bits. | [tests/loader/builtin/1947_loader_builtin_gif_lzw_12bit_width_numeric.t](../../../tests/loader/builtin/1947_loader_builtin_gif_lzw_12bit_width_numeric.t) |

### Quality regression tests

| ID | Quality floor protected | Owning test |
| --- | --- | --- |
| GIFQ-01 | The later-frame local/global palette switch remains visually close to its reference after the full encode path. | [tests/loader/builtin/0259_loader_builtin_gif_lct_gct_switch_frame2_lsqa.t](../../../tests/loader/builtin/0259_loader_builtin_gif_lct_gct_switch_frame2_lsqa.t) |
| GIFQ-02 | Transparency plus disposal 3 remains visually close to the expected second frame after the full encode path. | [tests/loader/builtin/0234_loader_builtin_gif_transparency_dispose3_frame2_lsqa.t](../../../tests/loader/builtin/0234_loader_builtin_gif_transparency_dispose3_frame2_lsqa.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| GIF-90 | Repeated disposal/history transitions complete within the watchdog rather than hanging or growing without bound. | [tests/loader/builtin/0260_loader_builtin_gif_disposal_stress_watchdog.t](../../../tests/loader/builtin/0260_loader_builtin_gif_disposal_stress_watchdog.t) |
| GIF-91 | An image rectangle extending beyond the logical screen is rejected before raster writes. | [tests/loader/builtin/1942_loader_builtin_gif_rectangle_oob_reject.t](../../../tests/loader/builtin/1942_loader_builtin_gif_rectangle_oob_reject.t) |
| GIF-92 | A raster sub-block shorter than its declared length is rejected without emitting a frame. | [tests/loader/builtin/1944_loader_builtin_gif_truncated_raster_reject.t](../../../tests/loader/builtin/1944_loader_builtin_gif_truncated_raster_reject.t) |
| GIF-93 | An LZW dictionary reference beyond the next available code is rejected without emitting a frame. | [tests/loader/builtin/1945_loader_builtin_gif_illegal_lzw_code_reject.t](../../../tests/loader/builtin/1945_loader_builtin_gif_illegal_lzw_code_reject.t) |

Coverage audit note: exact owners isolate both format signatures, local/global palettes, four-pass interlacing, descriptor offsets and bounds, known extension families, LZW width growth and illegal references, and disposal methods 2 and 3. The LSQA cases remain complementary end-to-end quality floors. The owners do not form a complete disposal × background-policy × palette-fusion × start-frame/loop matrix.
