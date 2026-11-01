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
| `ICCRGBG1` / `012` Application Extension | With CMS enabled, extract one stream-wide RGB ICC profile and normalize source palettes through shared CMS. See [embedded color profiles](#embedded-color-profiles). |
| Other Application Extensions | Their sub-block chains are validated and skipped. Private protocols, XMP, and vendor loop conventions are not interpreted. |
| Comment Extension (`0xfe`) | Validated and skipped; text is not exposed. |
| Plain Text Extension (`0x01`) | Validated and skipped; glyphs are not rendered onto the canvas. |
| Unknown extension label | Its sub-block chain is consumed and ignored. |

The Logical Screen pixel-aspect byte and color-table sort flags do not affect output. `GIF87a` is accepted even though animation control normally relies on 89a extensions. Unknown or skipped extensions are not preserved for re-encoding.

## Embedded color profiles

[ICC.1:2010 Annex B.5](https://www.color.org/specification/ICC1v43_2010-12.pdf#page=102) defines the 11-byte application identifier/authentication pair `ICCRGBG1012`. The profile is the concatenation of the following ordinary GIF sub-blocks, each with up to 255 data bytes, ending in a zero-length block. There are no JPEG-style sequence/count bytes. Unknown application identifiers remain uninterpreted.

libsixel adopts one RGB profile for the entire logical-screen stream, including both global and local palettes and the file background. The structural pre-scan can discover it before, between, or after image descriptors, before the trailer; selecting a static or later frame does not change its scope. This is a stream-wide interpretation policy, not support for per-frame profile switching. More than one recognized ICC extension is rejected instead of silently choosing one.

Extraction in `fromgif.c` validates the sub-block chain and caps the reassembled payload at 16 MiB, inclusive. Empty, truncated, oversized and duplicate recognized declarations fail with CMS enabled. The loader adapter opens the complete profile through shared CMS and requires an RGB source domain; invalid/unsupported profiles or failures to create/apply the selected transform fail loading. A failed palette is never drawn, and a failed float allocation never substitutes byte output. Frames already emitted before a later decode/transform error are not retracted. CMS-disabled input retains ordinary extension skipping, without ICC validation or float promotion; the GIF structure must still be decodable.

The adapter supplies a palette-conversion hook; the GIF parser, LZW decoder and canvas have no CMS object dependency. Each source color table is converted from its original RGB bytes to canonical linear sRGB float before painting. Negative and above-one components remain intact in the canvas, disposal backup, frame selection and replay cache. Transparency is a separate binary coverage plane and never enters the transform. The Logical Screen background inherits the source profile. An explicit output background is already in the configured background color space, so it is linearized independently and never interpreted through the GIF profile.

ICC-enabled output bypasses the retained byte-palette path, even when palette fusion or encoder byte precision is requested. The final `cms_target` / CMS `prefer_8bit` boundary remains available after composition. Untagged and CMS-disabled GIFs retain their existing RGB888/PAL8 canvas and fast paths. Profile conversion changes color interpretation, not source indices, frame delays, disposal, start-frame or loop policy.

## Frame-composition pipeline

The builtin dispatcher recognizes either GIF version signature, counts frames when a start index must be resolved, and enters the shared GIF decode loop (`load_gif()` or its optional color-hook entrypoint). A lightweight stream scan determines whether transparency and multiframe buffers are needed; a scan failure is advisory and switches to conservative buffers rather than accepting malformed decode data.

For every image, the decoder expands its LZW indices through the active color table and composites the rectangle at its left/top offset into the full logical canvas. The frame callback therefore always receives logical-screen dimensions. Before the next image it applies the previous image's disposal:

| Disposal value | Result |
| --- | --- |
| 0 or 1 | Keep the composed canvas. |
| 2 | Restore the previous image rectangle to the resolved background. |
| 3 | Restore the saved pre-image canvas and alpha state. |
| 4–7 | Reserved values are not assigned new semantics. |

When no explicit output background is selected and the stream uses transparency, a parallel alpha state preserves zero-alpha pixels. With a background, composition resolves those pixels according to background policy. A file background is the Logical Screen background index in the Global Color Table; it is unavailable when that table/index is unusable.

On the untagged or CMS-disabled byte route, the decoder attempts `PAL8` promotion only when palette use is enabled, the complete composed canvas can be represented by no more than the requested number of colors, and its binary transparency can be expressed by one key index. Otherwise it returns gamma RGB plus transparency state for common finalization. Palette preservation is therefore an output optimization and fidelity path, not a promise to return the source's local indices.

Frame delay remains in centiseconds. `fromgif` records the stream loop count and libsixel loop number separately. A replay cache may retain first-loop frames when repeated playback is needed; its default data cap is 64 MiB, and failure to cache falls back to decoding the stream again.

### Implementation and test map

![A vertical implementation map of the builtin GIF loader. It follows GIF signature routing into logical-screen and extension scanning, LZW raster expansion, persistent-canvas composition and disposal, optional ICC palette conversion, then frame selection, loop handling, and float-or-byte emission. Every node carries a coverage ID used by the tables below.](figures/gif.svg)

The map separates source indices from the emitted frame. Local-table indices are consumed by `gif_process_raster()` and painted into a logical-screen canvas; `gif_export_pal8_frame()` may build a new palette only after composition proves that the complete canvas is representable. This distinction is central to interpreting the palette and disposal tests.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `GIF-01` | `sixel_builtin_load_gif_frames()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | GIF chunk and loader controls → `load_gif()` callback run | Start-frame, static, loop, background, palette, and cancellation policy must reach the format decoder without creating asynchronous callbacks. |
| `GIF-02` | `gif_load_header()`, `gif_scan_stream_info()`, and `gif_skip_subblocks()` in [`fromgif.c`](../../../src/fromgif.c) | Header/block stream → screen metadata and buffer plan | Global table/background, GCE, Netscape looping, and unknown-extension skipping must leave the next block boundary exact. |
| `GIF-03` | `gif_process_raster()` and `gif_out_code()` in [`fromgif.c`](../../../src/fromgif.c) | LZW sub-block stream plus active table → decoded rectangle pixels | Code-table reset/growth, interlace order, local/global table selection, and transparent-index normalization must agree. |
| `GIF-04` | `gif_decode_one_frame()`, `gif_history_mark()`, and `gif_reset_canvas_for_loop()` in [`fromgif.c`](../../../src/fromgif.c) | Decoded rectangle plus prior canvas → completed logical-screen canvas | Offsets, transparency, disposal 0–3, saved history, dirty regions, and loop reset must reconstruct the visible frame. |
| `GIF-05` | `gif_should_emit_decoded_frame()`, `gif_export_pal8_frame()`, and `gif_export_nonpal_frame()` in [`fromgif.c`](../../../src/fromgif.c) | Completed canvas → selected callback frame | Start-frame and loop decisions must not skip prerequisite composition; PAL8 is permitted only when the final canvas and binary alpha fit. |
| `GIF-21` | `sixel_gif_read_icc()` and `gif_convert_colortable()` in [`fromgif.c`](../../../src/fromgif.c), adapter hook in [`loader-builtin.c`](../../../src/loader-builtin.c) | Source ICC and palette bytes → canonical float palette/canvas | Keep CMS ownership in the adapter; preserve extended values and independent background/coverage semantics. |

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
| `-l auto|force|disable` | `disable` stops after one pass. `auto` treats a positive `NETSCAPE2.0` count as that many total passes and zero as indefinite playback. `force` overrides a finite count when the extension is present. Without a recognized loop extension, all three modes currently stop after one pass. A one-frame stream is never replayed as animation. |
| `-g`, `--ignore-delay` | Leaves parsed delays intact but makes the encoder skip presentation sleeps. It does not alter GIF parsing or disposal. |
| `-B COLOR` | Supplies an explicit composition/disposal background. Without one, transparent pixels can remain transparent and the file background can participate according to policy. |
| `background_policy=file_first|explicit_first` | Chooses whether a valid Logical Screen background or `-B` wins where GIF requires a background. |
| `background_colorspace=gamma|linear` | Selects how an explicit background is interpreted when alpha composition is performed. |
| `-A auto|composite|clear|keep` | Controls whether zero-alpha output remains transparent or is flattened after GIF composition. It cannot create partial alpha because GIF has none. |
| `-p COLORS` and operations that disable palette fusion | The requested palette limit can keep or prevent the composed `PAL8` fast path. Scaling and non-default encoder color modes disable loader palette fusion and force RGB expansion. |
| `cms_engine`, `cms_target`, `prefer_8bit` | Select embedded ICC interpretation and final target negotiation. `cms_engine=none` retains the unmanaged byte route. |
| Orientation, PNG, HDR, PNM, and BMP suboptions | No effect: those metadata/format grammars do not apply to this GIF path. |

`NETSCAPE2.0` counts and `-l` interact at playback time; `-S` wins earlier by stopping after one emitted frame. Cancellation is checked during decode and replay, including forced indefinite looping.

See [Animation playback](../../encoder/animation.md) for cross-loader differences, timing, and output-state interactions. In particular, the builtin parser's finite-count behavior is not available through the generic GdkPixbuf animation API.

## Unsupported behavior and security boundary

The loader does not render Plain Text, expose comments, interpret application payloads other than the supported ICC/Netscape identifiers, honor pixel-aspect resampling, or retain original sub-block and code ordering. It has no support for partial alpha, per-frame ICC profiles, or animation conventions outside the supported Graphic Control plus Netscape model.

The LZW dictionary, sub-block lengths, image rectangles, frame count, and disposal history are all attacker-controlled. Closing the chain with `-Lbuiltin!` makes failure deterministic but does not sandbox the parser. Large or indefinite animations should also be treated as CPU and memory workloads; the cache cap limits retained replay data, not total decode work under `-l force`.

## Implementation landmarks

- GIF parser, LZW decode, canvas, frame selection, and replay: [`fromgif.c`](../../../src/fromgif.c)
- Builtin dispatch and common frame finalization: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Alpha and background semantics: [Alpha Policy](../alpha-policy/README.md) and [Background Policy](../background-policy.md)
- Broader non-owning format and malformed-stream suite: [`tests/loader/gif`](../../../tests/loader/gif)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [GIF coverage inventory](../../testing/builtin-loader-coverage.md#gif) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| GIF-01 | A positive start-frame selection emits the requested composited frame rather than renumbering the source stream. | [tests/loader/gif/0001_builtin_gif_start_frame_positive.t](../../../tests/loader/gif/0001_builtin_gif_start_frame_positive.t) |
| GIF-02 | A structurally valid unknown extension is consumed without corrupting the following block boundary. | [tests/loader/gif/0014_loader_builtin_gif_unknown_extension_ignored.t](../../../tests/loader/gif/0014_loader_builtin_gif_unknown_extension_ignored.t) |
| GIF-03 | A later frame's local color table replaces global-table colors for that frame. | [tests/loader/gif/0036_loader_builtin_gif_local_palette_numeric.t](../../../tests/loader/gif/0036_loader_builtin_gif_local_palette_numeric.t) |
| GIF-04 | Disposal method 3 restores the saved pre-frame canvas before the next composition. | [tests/loader/gif/0035_loader_builtin_gif_disposal3_numeric.t](../../../tests/loader/gif/0035_loader_builtin_gif_disposal3_numeric.t) |
| GIF-05 | Automatic looping honors a finite Netscape loop count for a multi-frame stream. | [tests/loader/gif/0018_loader_builtin_gif_loop_auto_loop2.t](../../../tests/loader/gif/0018_loader_builtin_gif_loop_auto_loop2.t) |
| GIF-06 | The GIF87a signature reaches the same exact-pixel decode path as the later GIF89a dialect. | [tests/loader/gif/0026_loader_builtin_gif87a_numeric.t](../../../tests/loader/gif/0026_loader_builtin_gif87a_numeric.t) |
| GIF-07 | Four-pass interlacing maps encoded rows to their exact logical-screen positions. | [tests/loader/gif/0027_loader_builtin_gif_interlace_four_pass_numeric.t](../../../tests/loader/gif/0027_loader_builtin_gif_interlace_four_pass_numeric.t) |
| GIF-08 | A nonzero image-descriptor offset composites into the full logical-screen canvas. | [tests/loader/gif/0028_loader_builtin_gif_rectangle_offset_numeric.t](../../../tests/loader/gif/0028_loader_builtin_gif_rectangle_offset_numeric.t) |
| GIF-09 | Comment, Plain Text, and unknown Application extension sub-blocks preserve the following image boundary. | [tests/loader/gif/0030_loader_builtin_gif_extension_subblocks_numeric.t](../../../tests/loader/gif/0030_loader_builtin_gif_extension_subblocks_numeric.t) |
| GIF-10 | Disposal method 2 clears the previous dirty rectangle before the next frame is composed. | [tests/loader/gif/0033_loader_builtin_gif_disposal2_numeric.t](../../../tests/loader/gif/0033_loader_builtin_gif_disposal2_numeric.t) |
| GIF-11 | Literal LZW input remains exact while the dictionary crosses each code-width boundary through 12 bits. | [tests/loader/gif/0034_loader_builtin_gif_lzw_12bit_width_numeric.t](../../../tests/loader/gif/0034_loader_builtin_gif_lzw_12bit_width_numeric.t) |
| GIF-12 | Emitted frames preserve exact GCE delays and report the expected frame, loop, and multiframe metadata. | [tests/loader/gif/0037_loader_builtin_gif_animation_metadata_numeric.t](../../../tests/loader/gif/0037_loader_builtin_gif_animation_metadata_numeric.t) |

### Decoded frame value tests

| ID | Frame value contract | Owning test |
| --- | --- | --- |
| GIFQ-01 | The later-frame local/global palette switch preserves every expected decoded RGB component exactly after encoding. | [tests/loader/gif/0012_loader_builtin_gif_lct_gct_switch_frame2_samples.t](../../../tests/loader/gif/0012_loader_builtin_gif_lct_gct_switch_frame2_samples.t) |
| GIFQ-02 | Transparency plus disposal 3 produces the expected frame, with at most two byte levels of endpoint-palette error plus float roundoff in every component. | [tests/loader/gif/0007_loader_builtin_gif_transparency_dispose3_frame2_samples.t](../../../tests/loader/gif/0007_loader_builtin_gif_transparency_dispose3_frame2_samples.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| GIF-90 | Repeated disposal/history transitions complete within the watchdog rather than hanging or growing without bound. | [tests/loader/gif/0013_loader_builtin_gif_disposal_stress_watchdog.t](../../../tests/loader/gif/0013_loader_builtin_gif_disposal_stress_watchdog.t) |
| GIF-91 | An image rectangle extending beyond the logical screen is rejected before raster writes. | [tests/loader/gif/0029_loader_builtin_gif_rectangle_oob_reject.t](../../../tests/loader/gif/0029_loader_builtin_gif_rectangle_oob_reject.t) |
| GIF-92 | A raster sub-block shorter than its declared length is rejected without emitting a frame. | [tests/loader/gif/0031_loader_builtin_gif_truncated_raster_reject.t](../../../tests/loader/gif/0031_loader_builtin_gif_truncated_raster_reject.t) |
| GIF-93 | An LZW dictionary reference beyond the next available code is rejected without emitting a frame. | [tests/loader/gif/0032_loader_builtin_gif_illegal_lzw_code_reject.t](../../../tests/loader/gif/0032_loader_builtin_gif_illegal_lzw_code_reject.t) |

Coverage audit note: exact owners isolate both format signatures, local/global palettes, four-pass interlacing, descriptor offsets and bounds, known extension families, LZW width growth and illegal references, and disposal methods 2 and 3. The LSQA cases remain complementary end-to-end quality floors. The owners do not form a complete disposal × background-policy × palette-fusion × start-frame/loop matrix.

### Embedded ICC behavioral and numeric contracts

The fixtures are two-by-two GIFs assembled in memory. Input ICCs are serialized from named definitions or explicit primaries/gamma; expected output is independently calculated from fixed P3-to-sRGB matrix coefficients and transfer equations. The `3e-4` absolute tolerance includes ICC s15Fixed16 colorant rounding. Byte controls, coverage, dimensions, frame numbers, delays and loop numbers use exact assertions. These tests do not derive expectations from decoded output or use a perceptual score as a numerical oracle. Native-engine tests require a successful apply trace from that engine.

| ID | Contract protected | Owning test |
| --- | --- | --- |
| GICC-01 | Global P3 palette retains extended linear RGB. | [tests/loader/gif/0038_gif_icc_global_numeric.t](../../../tests/loader/gif/0038_gif_icc_global_numeric.t) |
| GICC-02 | Local P3 palette overrides the global table without byte narrowing. | [tests/loader/gif/0039_gif_icc_local_numeric.t](../../../tests/loader/gif/0039_gif_icc_local_numeric.t) |
| GICC-03 | An sRGB profile yields the independently decoded transfer values. | [tests/loader/gif/0040_gif_icc_srgb_numeric.t](../../../tests/loader/gif/0040_gif_icc_srgb_numeric.t) |
| GICC-04 | An RGB gamma-2 profile yields squared source samples. | [tests/loader/gif/0041_gif_icc_gamma2_numeric.t](../../../tests/loader/gif/0041_gif_icc_gamma2_numeric.t) |
| GICC-05 | One-byte ICC sub-blocks reassemble to byte-identical profile data. | [tests/loader/gif/0042_gif_icc_split_bytes_numeric.t](../../../tests/loader/gif/0042_gif_icc_split_bytes_numeric.t) |
| GICC-06 | A profile after the image is adopted before the first callback. | [tests/loader/gif/0043_gif_icc_late_profile_numeric.t](../../../tests/loader/gif/0043_gif_icc_late_profile_numeric.t) |
| GICC-07 | Untagged input keeps the byte palette without CMS application. | [tests/loader/gif/0044_gif_icc_untagged_byte_numeric.t](../../../tests/loader/gif/0044_gif_icc_untagged_byte_numeric.t) |
| GICC-08 | CMS-disabled ICC input keeps the byte palette without CMS application. | [tests/loader/gif/0045_gif_icc_cms_none_byte_numeric.t](../../../tests/loader/gif/0045_gif_icc_cms_none_byte_numeric.t) |
| GICC-09 | Binary transparency keeps exact zero coverage outside the color transform. | [tests/loader/gif/0046_gif_icc_alpha_numeric.t](../../../tests/loader/gif/0046_gif_icc_alpha_numeric.t) |
| GICC-10 | The logical-screen background uses the source profile. | [tests/loader/gif/0047_gif_icc_file_background_numeric.t](../../../tests/loader/gif/0047_gif_icc_file_background_numeric.t) |
| GICC-11 | Explicit-first background is normalized independently of the source ICC. | [tests/loader/gif/0048_gif_icc_explicit_background_numeric.t](../../../tests/loader/gif/0048_gif_icc_explicit_background_numeric.t) |
| GICC-12 | File-first policy retains the profiled file background over an explicit value. | [tests/loader/gif/0049_gif_icc_file_first_background_numeric.t](../../../tests/loader/gif/0049_gif_icc_file_first_background_numeric.t) |
| GICC-13 | Disposal 2 restores the profiled background after a local-table frame. | [tests/loader/gif/0050_gif_icc_disposal2_numeric.t](../../../tests/loader/gif/0050_gif_icc_disposal2_numeric.t) |
| GICC-14 | Disposal 3 restores the previous float canvas rather than the background. | [tests/loader/gif/0051_gif_icc_disposal3_numeric.t](../../../tests/loader/gif/0051_gif_icc_disposal3_numeric.t) |
| GICC-15 | Interlaced source rows retain the same interpreted palette values. | [tests/loader/gif/0052_gif_icc_interlace_numeric.t](../../../tests/loader/gif/0052_gif_icc_interlace_numeric.t) |
| GICC-16 | Positive start-frame selection preserves prior composition and frame numbering. | [tests/loader/gif/0053_gif_icc_start_positive_numeric.t](../../../tests/loader/gif/0053_gif_icc_start_positive_numeric.t) |
| GICC-17 | Negative start-frame selection resolves against the source frame count. | [tests/loader/gif/0054_gif_icc_start_negative_numeric.t](../../../tests/loader/gif/0054_gif_icc_start_negative_numeric.t) |
| GICC-18 | Cached replay retains extended colors, source delays and loop numbers. | [tests/loader/gif/0055_gif_icc_replay_numeric.t](../../../tests/loader/gif/0055_gif_icc_replay_numeric.t) |
| GICC-19 | An unknown application identifier does not trigger CMS or float promotion. | [tests/loader/gif/0056_gif_icc_unknown_app_byte_numeric.t](../../../tests/loader/gif/0056_gif_icc_unknown_app_byte_numeric.t) |
| GICC-20 | An invalid recognized ICC profile fails before a frame is emitted. | [tests/loader/gif/0057_gif_icc_invalid_profile.t](../../../tests/loader/gif/0057_gif_icc_invalid_profile.t) |
| GICC-21 | A non-RGB profile fails instead of interpreting RGB codes in another domain. | [tests/loader/gif/0058_gif_icc_wrong_domain_reject.t](../../../tests/loader/gif/0058_gif_icc_wrong_domain_reject.t) |
| GICC-22 | Duplicate ICC declarations fail without choosing either profile. | [tests/loader/gif/0059_gif_icc_duplicate_reject.t](../../../tests/loader/gif/0059_gif_icc_duplicate_reject.t) |
| GICC-23 | An empty recognized ICC declaration fails before output. | [tests/loader/gif/0060_gif_icc_empty_reject.t](../../../tests/loader/gif/0060_gif_icc_empty_reject.t) |
| GICC-24 | A truncated recognized ICC chain fails before output. | [tests/loader/gif/0061_gif_icc_truncated_blocks.t](../../../tests/loader/gif/0061_gif_icc_truncated_blocks.t) |
| GICC-25 | A recognized ICC stream without a closing trailer fails before output. | [tests/loader/gif/0062_gif_icc_missing_trailer_reject.t](../../../tests/loader/gif/0062_gif_icc_missing_trailer_reject.t) |
| GICC-26 | CMS-disabled input skips duplicate ICC extensions as ordinary application data. | [tests/loader/gif/0063_gif_icc_cms_none_duplicate_numeric.t](../../../tests/loader/gif/0063_gif_icc_cms_none_duplicate_numeric.t) |
| GICC-27 | Actual lcms2 palette conversion preserves the numerical color contract. | [tests/loader/gif/0064_gif_icc_lcms2_numeric.t](../../../tests/loader/gif/0064_gif_icc_lcms2_numeric.t) |
| GICC-28 | Actual ColorSync palette conversion preserves the numerical color contract. | [tests/loader/gif/0065_gif_icc_colorsync_numeric.t](../../../tests/loader/gif/0065_gif_icc_colorsync_numeric.t) |
| GICC-29 | The gamma float target uses signed extended sRGB encoding after composition. | [tests/loader/gif/0066_gif_icc_gamma_target_numeric.t](../../../tests/loader/gif/0066_gif_icc_gamma_target_numeric.t) |
| GICC-30 | A failed initial palette hook publishes no frame. | [tests/loader/gif/0067_gif_icc_fail_global.t](../../../tests/loader/gif/0067_gif_icc_fail_global.t) |
| GICC-31 | A failed later palette hook preserves the earlier callback count and publishes no incomplete frame. | [tests/loader/gif/0068_gif_icc_fail_local.t](../../../tests/loader/gif/0068_gif_icc_fail_local.t) |
| GICC-32 | A failed float canvas allocation does not fall back to byte output. | [tests/loader/gif/0069_gif_icc_fail_canvas.t](../../../tests/loader/gif/0069_gif_icc_fail_canvas.t) |
| GICC-33 | ICC extraction rejects a payload above 16 MiB without allocating profile output. | [tests/loader/gif/0070_gif_icc_over_limit_reject.t](../../../tests/loader/gif/0070_gif_icc_over_limit_reject.t) |
| GICC-34 | ICC extraction accepts and preserves all bytes at the 16 MiB limit. | [tests/loader/gif/0071_gif_icc_limit_exact_numeric.t](../../../tests/loader/gif/0071_gif_icc_limit_exact_numeric.t) |
| GICC-35 | A local-only table works without any global color table. | [tests/loader/gif/0072_gif_icc_local_only_numeric.t](../../../tests/loader/gif/0072_gif_icc_local_only_numeric.t) |
| GICC-36 | Explicit background retains its own interpretation without a global table. | [tests/loader/gif/0073_gif_icc_explicit_no_global_numeric.t](../../../tests/loader/gif/0073_gif_icc_explicit_no_global_numeric.t) |
| GICC-37 | Gamma-target replay does not apply a source ICC or transfer twice. | [tests/loader/gif/0074_gif_icc_gamma_replay_numeric.t](../../../tests/loader/gif/0074_gif_icc_gamma_replay_numeric.t) |

These ICC tests are independent value/behavior contracts. The existing GIF MS-SSIM regressions above continue to protect untagged decoding; they are not ICC reference images. ICC sub-block/profile failures are covered separately from LZW malformed-input tests in the exhaustive [builtin test inventory](../../testing/builtin-loader-coverage.md#gif).
