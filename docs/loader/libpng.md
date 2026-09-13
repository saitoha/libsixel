# libpng Image Loader

The `libpng` loader decodes PNG scanlines with the external libpng library and implements APNG playback in [`loader-libpng.c`](../../src/loader-libpng.c). libsixel owns frame representation, color management, alpha/background normalization, orientation, and delivery. An APNG-patched libpng is not required.

## Selection and supported input

Enable the optional dependency with Autotools `--with-png` or Meson `-Dpng=enabled`. `HAVE_LIBPNG` controls registration, and libpng is first in the compiled default loader order. Use `-Llibpng!` to close the chain and inspect this component alone; `-Llibpng` permits other configured loaders after an eligible failure. Check the running binary's help and loader trace rather than inferring selection from a `.png` filename. The build dependency also enables the [PNG writer](../writers/png.md), which is a separate component.

```sh
img2sixel -Llibpng! input.png
img2sixel -Llibpng:cms_engine=builtin:prefer_8bit=0! input.png
img2sixel -Llibpng! -S --start-frame=1 animation.png
```

Quote `!` in shells with interactive history expansion. Build tools should use the repository's `.local/bin` first in `PATH`.

The predicate recognizes the eight-byte PNG signature. libpng handles DEFLATE, row filters, Adam7 interlace, and standard PNG samples: grayscale at 1/2/4/8/16 bits, indexed at 1/2/4/8 bits, and RGB, grayscale-alpha, and RGBA at 8/16 bits. The effective decoder feature set depends on the linked libpng build. This adapter does not implement Apple `CgBI`, HDR tone mapping, or dedicated interpretation of `cICP`, `mDCv`, and `cLLi`. See the [PNG specification](https://www.w3.org/TR/png-3/) and [libpng manual](https://github.com/pnggroup/libpng/blob/libpng16/libpng-manual.txt) for codec-level details.

## Processing boundaries

| Stage | Responsibility |
| --- | --- |
| Original chunk classification | Validate chunk boundaries and animation structure without allocating a canvas or copying image data. Recognize actual chunk types, not matching bytes inside payloads. |
| Static decode | Read the original stream through `png_read_image()` and `png_read_end()`. Retain eligible packed palettes and grayscale formats, or expand to component samples. |
| APNG rectangle decode | Reconstruct a PNG for each controlled rectangle, carrying shared metadata including `PLTE`, `tRNS`, color chunks, and `bKGD`; use the same sample/CMS processing as static PNG. |
| Color interpretation | Convert validated supported source metadata to linear sRGB while keeping alpha separate. |
| Composition | Resolve the requested background and alpha policy. APNG first updates its linear RGB/alpha canvas, then prepares the emitted frame. |
| Delivery | Attach zero-alpha coverage, apply orientation, and invoke the callback. Once a callback has been invoked, an error is terminal for the loader chain. |

`SIXEL_TRACE_TOPIC=loader` shows candidate selection; `apng_decode` shows animation parsing and replay. Static inputs bypass animation decoding and ignore animation start-frame settings, including malformed or out-of-range environment values.

## Color management and precision

The CLI defaults to `cms_engine=auto`; a directly constructed component starts with CMS disabled until configured. Decoder choice and CMS-engine choice are independent. `png_source_to_linear()` supplies the same source interpretation to static images and animation rectangles. The [shared CMS policy](color-management.md) owns engine selection, rendering-intent fallback, target colorspace, and unsupported-profile behavior.

Only ICC bytes exposed by libpng after its validation are eligible. A usable supported ICC transform has priority over sRGB and cHRM/gAMA. If the selected engine cannot use that profile, the adapter reports the fallback in loader tracing and uses supported lower-priority metadata. It does not revive raw ICC bytes rejected by libpng. Simultaneous iCCP and sRGB violate PNG's requirements; there is no special `iCCP+sRGB+cHRM` rule that disables a usable profile. This is best-effort recovery for contradictory input, not a claim that such input is conforming.

Without a usable ICC transform, valid sRGB selects the sRGB transfer function and primaries. Otherwise valid gAMA supplies the source transfer function, and valid cHRM supplies the source-to-sRGB matrix. With CMS disabled or no applicable metadata, source color samples use the adapter's sRGB assumption. Matrix singularity or unavailable metadata must not authorize an invented color transform. Alpha is linear coverage and never enters the CMS transform.

Eight-bit samples are normalized before alpha composition. Sixteen-bit samples retain their low bits in float32 storage; a transparency optimization does not truncate them. Grayscale ICC conversion also has an internal float gray format in [`cms.c`](../../src/cms.c), including the builtin, Little CMS, and ColorSync format mappings. This is an internal conversion format, not a new public PNG or frame format.

Static opaque images may retain byte RGB, packed indices, or grayscale storage. Unmanaged eight-bit alpha with no resolved background can retain RGBA8. Other alpha paths emit linear RGB float32 plus a zero-alpha mask. APNG uses float32 linear RGB and fractional alpha internally and emits linear RGB float32 frames plus masks. Subsequent encoder precision and quantization choices remain separate, explicit boundaries; see [pixel-format precision](../concepts/pixelformat-precision.md).

The `SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR` compatibility switch may select an eligible representation optimization. It does not grant permission to discard alpha, change background priority, or reduce sixteen-bit precision. CMS or indexed expansion must preserve coverage regardless of that switch.

## Alpha and background

The [alpha policy](alpha-policy.md) and [background policy](background-policy.md) apply to static PNG and APNG alike. `file_first` selects valid bKGD before the external candidate; `explicit_first` reverses that order. If the preferred source is absent, use the other available source. No source means no background, not implicit black.

File background samples use the image's source interpretation, including indexed palette lookup and applicable CMS. An external background uses the requested `background_colorspace`: `gamma` decodes it as sRGB and `linear` uses its values directly. This setting describes external sample encoding; composition itself uses linear light in both cases. An OSC 11 reply is a gamma-encoded external candidate as specified by the shared policy.

For foreground color `F`, resolved background `B`, and alpha `a`, composition computes `a * F + (1 - a) * B` in linear RGB. `composite` fills alpha-zero pixels when a background exists and clears their mask. `clear` and `keep` preserve zero-alpha coverage, while partial alpha can still use an available background. Unresolved `composite` preserves coverage so the encoder can request `P2=1`. Without a background, partially covered static pixels retain their source color because SIXEL cannot encode fractional alpha.

Expanding an indexed image because of requested colors, disabled palette fusion, CMS, or composition carries tRNS into the expanded samples. Retained indexed images can use a transparent index, remapping multiple zero-alpha entries to one index; expanded images use component alpha or a mask. The encoder consumes this coverage metadata and selects the wire request. A fully opaque emitted APNG canvas does not need a transparency header merely because its source color type has an alpha channel.

To compare against an independently generated opaque reference, make the intended composition explicit:

```sh
img2sixel -Llibpng:cms_engine=none! -Acomposite -B#000 -Nexplicit_first input.png
```

## APNG semantics

An IDAT default image belongs to the animation only when fcTL precedes it. Otherwise all of its IDAT chunks are excluded from the declared animation count, output callbacks, and frame numbering. Animation sequence numbers, frame counts, rectangles, operation values, and chunk ordering are checked before delivery. Applicable pre-IDAT metadata, including tRNS, is included in each reconstructed rectangle PNG. Color chunks appearing after the original IDAT are not moved ahead of a reconstructed rectangle to make them usable.

The canvas stores straight alpha and linear sRGB. `SOURCE` replaces the controlled rectangle. `OVER` combines foreground and destination with their fractional alpha and normalizes by the resulting coverage. `BACKGROUND` disposal clears the rectangle to transparent; it does not paint bKGD. `PREVIOUS` restores the saved canvas. The selected background is applied to an emitted copy, leaving fractional canvas alpha available for subsequent frames.

Delay fractions are converted to integer centiseconds; a zero denominator uses 100 and a positive fraction that truncates to zero becomes one centisecond. `-l disable` emits one pass; `-l auto` follows the play count, where zero permits indefinite replay. A one-frame animation stops after one pass. `-g` controls later presentation waiting.

`--start-frame`/`-T` overrides `SIXEL_LOADER_ANIMATION_START_FRAME_NO`. Negative indices count from the declared end. Earlier frames still update the canvas, but their first-loop callbacks are skipped. Subsequent loops start from the beginning, and emitted frame numbers are local to each loop. `-S` stops after the selected callback. The original chunk structure and relevant CRCs have already been checked, but compressed pixels of later frames need not have been decoded; successful extraction is not exhaustive raster validation of the whole animation.

## Integrity, errors, and resources

The classifier checks chunk boundaries, IHDR placement, contiguous IDAT, IEND, and the animation grammar. IEND CRC is checked explicitly because libpng can treat its CRC failure as a warning. Before APNG reconstruction, the adapter validates original critical and animation-control/data CRCs. Replacing fdAT with newly checksummed IDAT cannot conceal an original CRC error. Optional ancillary chunks retain libpng's recoverable warning/skip behavior; success does not mean every ancillary checksum was accepted as valid.

Each libpng reader uses its own jump target. Cleanup retains its owned pointers across error unwinding. A failed decode or a callback error returns its status; the [loader manager](README.md#candidate-selection-and-fallback) may try a different backend only before the first callback. It never appends replacement frames after delivery has begun.

APNG storage includes two float RGBA canvases, the reconstructed compressed frame, decoded color/alpha buffers, emitted frames, and an optional replay cache. The cache budgets retained frame storage at 64 MiB. Allocation failure or budget exhaustion disables retention and permits later loops to decode again, with the same pixels, masks, timing, and order. This is not a total-memory limit or a public cache-size suboption. Dimension and reconstruction-size arithmetic are checked before allocation.

Static classification does not allocate either animation canvas or rebuild IDAT chunks. Performance comparisons should still separate libpng raster work from adapter parsing, CRC, allocation, CMS, and encoder work. The [recorded static measurements](builtin/png.md#measured-static-png-cost) describe an earlier adapter and must not be presented as measurements of this implementation or as a codec ranking.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

Direct probes inspect frame samples, masks, timing, and callback counts rather than inferring these properties from compressed SIXEL equality.

| ID | Observation protected | Owning test |
| --- | --- | --- |
| LP-01 | Direct component callbacks report RGBA8 for the unmanaged alpha fixture and float RGB for the gray16 fixture. | [tests/loader/libpng/0009_loader_libpng_pixelformat.t](../../tests/loader/libpng/0009_loader_libpng_pixelformat.t) |
| LP-02 | A direct indexed loader probe expands to RGB when the requested budget is smaller than `PLTE`. | [tests/loader/libpng/0143_loader_libpng_indexed_png_reqcolors_fallback.t](../../tests/loader/libpng/0143_loader_libpng_indexed_png_reqcolors_fallback.t) |
| LP-03 | The unmanaged RGB16/sRGB fixture reaches the encoder planner as `rgb-f32` and keeps float working storage. | [tests/loader/libpng/0142_libpng_rgb16_srgb_only_float32_pipeline.t](../../tests/loader/libpng/0142_libpng_rgb16_srgb_only_float32_pipeline.t) |
| LP-04 | The key-color compatibility switch preserves decoded linear samples and zero-alpha coverage across RGB+tRNS, grayscale+tRNS, indexed+tRNS, RGBA, GA, 8-bit, 16-bit, static, and APNG paths; APNG timing and frame identity are also unchanged. Storage representation and later SIXEL palette construction are outside this loader contract. | [tests/loader/libpng/0147_loader_libpng_trns_keycolor_optimization_preserves_output.t](../../tests/loader/libpng/0147_loader_libpng_trns_keycolor_optimization_preserves_output.t), [tests/loader/libpng/0148_loader_libpng_trns_keycolor_optimization_preserves_gray_output.t](../../tests/loader/libpng/0148_loader_libpng_trns_keycolor_optimization_preserves_gray_output.t), [tests/loader/libpng/0149_loader_libpng_trns_keycolor_optimization_preserves_gray16_output.t](../../tests/loader/libpng/0149_loader_libpng_trns_keycolor_optimization_preserves_gray16_output.t), [tests/loader/libpng/0179_apng_libpng_trns_keycolor_optimization_preserves_output.t](../../tests/loader/libpng/0179_apng_libpng_trns_keycolor_optimization_preserves_output.t), [tests/loader/libpng/0195_loader_libpng_trns_keycolor_palette_optimization_invariance.t](../../tests/loader/libpng/0195_loader_libpng_trns_keycolor_palette_optimization_invariance.t), [tests/loader/libpng/0202_loader_libpng_trns_keycolor_optimization_preserves_rgba16_output.t](../../tests/loader/libpng/0202_loader_libpng_trns_keycolor_optimization_preserves_rgba16_output.t), [tests/loader/libpng/0203_loader_libpng_trns_keycolor_optimization_preserves_ga16_output.t](../../tests/loader/libpng/0203_loader_libpng_trns_keycolor_optimization_preserves_ga16_output.t) |
| LP-05 | Under `cms_engine=auto`, toggling key-color produces identical SIXEL for the gray-key, RGBA16, and APNG fixtures. This proves optimization invariance; direct probes below establish CMS and coverage. | [tests/loader/libpng/0156_loader_libpng_trns_keycolor_gating_conditions.t](../../tests/loader/libpng/0156_loader_libpng_trns_keycolor_gating_conditions.t), [tests/loader/libpng/0204_loader_libpng_trns_keycolor_rgba16_cms_auto_gating.t](../../tests/loader/libpng/0204_loader_libpng_trns_keycolor_rgba16_cms_auto_gating.t), [tests/loader/libpng/0198_apng_libpng_trns_keycolor_cms_gating.t](../../tests/loader/libpng/0198_apng_libpng_trns_keycolor_cms_gating.t) |
| LP-06 | Invalid and empty key-color environment values retain the enabled default's encoded output. | [tests/loader/libpng/0154_loader_libpng_trns_keycolor_env_invalid_defaults_enabled.t](../../tests/loader/libpng/0154_loader_libpng_trns_keycolor_env_invalid_defaults_enabled.t), [tests/loader/libpng/0211_loader_libpng_trns_keycolor_env_empty_matches_default.t](../../tests/loader/libpng/0211_loader_libpng_trns_keycolor_env_empty_matches_default.t) |
| LP-07 | A CLI-injected environment setting retains the requested keep header. | [tests/loader/libpng/0161_loader_libpng_trns_keycolor_cli_env_override.t](../../tests/loader/libpng/0161_loader_libpng_trns_keycolor_cli_env_override.t) |
| LP-08 | Omitted background colorspace matches explicit gamma, while explicit linear changes the indexed transparency fixture. | [tests/loader/libpng/0145_loader_libpng_background_colorspace_default_equals_gamma.t](../../tests/loader/libpng/0145_loader_libpng_background_colorspace_default_equals_gamma.t), [tests/loader/libpng/0146_loader_libpng_background_colorspace_linear_changes_indexed_trns.t](../../tests/loader/libpng/0146_loader_libpng_background_colorspace_linear_changes_indexed_trns.t) |
| LP-09 | Orientation 6 changes static/APNG output; the static default matches enabled, and an untagged static image is unchanged by the switch. | [tests/loader/libpng/0213_loader_libpng_exif_orientation_static_and_apng.t](../../tests/loader/libpng/0213_loader_libpng_exif_orientation_static_and_apng.t) |
| LP-10 | Static PNG output is unchanged by invalid/out-of-range animation environment values or a CLI start-frame request. | [tests/loader/libpng/0216_loader_libpng_static_ignores_invalid_start_frame_env.t](../../tests/loader/libpng/0216_loader_libpng_static_ignores_invalid_start_frame_env.t), [tests/loader/libpng/0217_loader_libpng_static_ignores_start_frame_cli.t](../../tests/loader/libpng/0217_loader_libpng_static_ignores_start_frame_cli.t), [tests/loader/libpng/0218_loader_libpng_static_ignores_out_of_range_start_frame_env.t](../../tests/loader/libpng/0218_loader_libpng_static_ignores_out_of_range_start_frame_env.t) |
| LP-11 | Trace output reports the exact loop-local frame-number sequence for normal playback and first-loop start-frame selection. | [tests/loader/libpng/0178_apng_libpng_frame_no_sequence_per_loop.t](../../tests/loader/libpng/0178_apng_libpng_frame_no_sequence_per_loop.t), [tests/loader/libpng/0197_apng_libpng_start_frame_keycolor_sequence.t](../../tests/loader/libpng/0197_apng_libpng_start_frame_keycolor_sequence.t) |
| LP-12 | Switching builtin CMS rendering intent changes encoded output for an ICC fixture with distinct A2B intent tables. | [tests/loader/libpng/0215_loader_libpng_png_rgb_mab_a2b012_intent_switch_builtin.t](../../tests/loader/libpng/0215_loader_libpng_png_rgb_mab_a2b012_intent_switch_builtin.t) |
| LP-13 | APNG excludes an uncontrolled default image from callbacks, timing, and numbering. | [tests/loader/libpng/0223_libpng_apng_default_image_excluded_numeric.t](../../tests/loader/libpng/0223_libpng_apng_default_image_excluded_numeric.t) |
| LP-14 | APNG tRNS retains the exact zero-alpha mask. | [tests/loader/libpng/0224_libpng_apng_trns_mask_numeric.t](../../tests/loader/libpng/0224_libpng_apng_trns_mask_numeric.t) |
| LP-15 | CMS preserves zero-alpha coverage. | [tests/loader/libpng/0227_libpng_cms_mask_numeric.t](../../tests/loader/libpng/0227_libpng_cms_mask_numeric.t) |
| LP-16 | Indexed expansion below the palette budget preserves zero-alpha coverage. | [tests/loader/libpng/0228_libpng_indexed_mask_numeric.t](../../tests/loader/libpng/0228_libpng_indexed_mask_numeric.t) |
| LP-17 | file_first selects bKGD over a competing external background in static PNG and APNG. | [tests/loader/libpng/0229_libpng_background_numeric.t](../../tests/loader/libpng/0229_libpng_background_numeric.t), [tests/loader/libpng/0230_libpng_apng_background_numeric.t](../../tests/loader/libpng/0230_libpng_apng_background_numeric.t) |
| LP-18 | Static and animated sixteen-bit alpha samples retain visible low bits. | [tests/loader/libpng/0231_libpng_lowbits_numeric.t](../../tests/loader/libpng/0231_libpng_lowbits_numeric.t), [tests/loader/libpng/0232_libpng_apng_lowbits_numeric.t](../../tests/loader/libpng/0232_libpng_apng_lowbits_numeric.t) |
| LP-19 | APNG applies pre-IDAT gAMA numerically in linear RGB and ignores misplaced later gAMA. | [tests/loader/libpng/0233_libpng_apng_gamma_numeric.t](../../tests/loader/libpng/0233_libpng_apng_gamma_numeric.t) |
| LP-20 | Grayscale ICC transforms retain sixteen-bit distinctions in static and animated input while keeping neutral gray. | [tests/loader/libpng/0236_libpng_gray_icc.t](../../tests/loader/libpng/0236_libpng_gray_icc.t), [tests/loader/libpng/0237_libpng_apng_gray_icc.t](../../tests/loader/libpng/0237_libpng_apng_gray_icc.t) |
| LP-21 | OVER uses linear-light arithmetic; BACKGROUND and PREVIOUS restore transparent coverage. | [tests/loader/libpng/0006_apng_libpng_blend_over.t](../../tests/loader/libpng/0006_apng_libpng_blend_over.t), [tests/loader/libpng/0007_apng_libpng_dispose_background.t](../../tests/loader/libpng/0007_apng_libpng_dispose_background.t), [tests/loader/libpng/0243_libpng_previous.t](../../tests/loader/libpng/0243_libpng_previous.t) |
| LP-22 | Indexed fractional alpha composes in linear RGB. | [tests/loader/libpng/0238_libpng_indexed_partial.t](../../tests/loader/libpng/0238_libpng_indexed_partial.t) |
| LP-23 | keep retains a mask with an explicit background, and unresolved composite retains it without one. | [tests/loader/libpng/0239_libpng_keep_background.t](../../tests/loader/libpng/0239_libpng_keep_background.t), [tests/loader/libpng/0240_libpng_composite_missing.t](../../tests/loader/libpng/0240_libpng_composite_missing.t) |
| LP-24 | Cached and allocation-fallback replay retain pixels, masks, delays, frame numbers, and loop numbers. | [tests/loader/libpng/0241_libpng_cache.t](../../tests/loader/libpng/0241_libpng_cache.t), [tests/loader/libpng/0242_libpng_cache_failure.t](../../tests/loader/libpng/0242_libpng_cache_failure.t) |
| LP-25 | Static PNG performs no animation-canvas allocation, handles split IDAT, and ignores literal acTL payload bytes. | [tests/loader/libpng/0245_libpng_static_alloc.t](../../tests/loader/libpng/0245_libpng_static_alloc.t), [tests/loader/libpng/0246_libpng_literal_actl.t](../../tests/loader/libpng/0246_libpng_literal_actl.t), [tests/loader/libpng/0247_libpng_split_idat.t](../../tests/loader/libpng/0247_libpng_split_idat.t) |
| LP-26 | A rejected ICC profile is not revived; supported remaining metadata determines the samples. | [tests/loader/libpng/0214_loader_libpng_rejected_icc_metadata_fallback.t](../../tests/loader/libpng/0214_loader_libpng_rejected_icc_metadata_fallback.t) |
| LP-27 | Original APNG IDAT CRC errors, malformed IEND CRC, orphan controls, and missing IEND are rejected before callbacks. | [tests/loader/libpng/0225_libpng_apng_original_idat_crc_reject.t](../../tests/loader/libpng/0225_libpng_apng_original_idat_crc_reject.t), [tests/loader/libpng/0226_libpng_png_iend_crc_reject.t](../../tests/loader/libpng/0226_libpng_png_iend_crc_reject.t), [tests/loader/libpng/0248_libpng_orphan.t](../../tests/loader/libpng/0248_libpng_orphan.t), [tests/loader/libpng/0250_libpng_truncated.t](../../tests/loader/libpng/0250_libpng_truncated.t) |
| LP-28 | A recoverable ancillary CRC error does not invalidate decoded pixels. | [tests/loader/libpng/0249_libpng_ancillary_crc.t](../../tests/loader/libpng/0249_libpng_ancillary_crc.t) |
| LP-29 | Callback errors, including APNG SIXEL_FALSE, and later frame-decode failures do not emit replacement frames through static or backend fallback. | [tests/loader/libpng/0235_libpng_fallback.t](../../tests/loader/libpng/0235_libpng_fallback.t), [tests/loader/libpng/0244_libpng_late_error.t](../../tests/loader/libpng/0244_libpng_late_error.t) |
| LP-30 | APNG tRNS keeps its P2=1 request with the default or disabled compatibility optimization. | [tests/loader/libpng/0208_apng_libpng_trns_keycolor_default_has_header.t](../../tests/loader/libpng/0208_apng_libpng_trns_keycolor_default_has_header.t), [tests/loader/libpng/0209_apng_libpng_trns_keycolor_env0_preserves_header.t](../../tests/loader/libpng/0209_apng_libpng_trns_keycolor_env0_preserves_header.t) |

### Quality regression tests

| ID | Quality observation | Owning test |
| --- | --- | --- |
| LPQ-01 | PNGSuite RGBA8 and RGBA16 composed over explicit black preserve their MS-SSIM floors. | [tests/loader/libpng/0039_pngsuite_basic_default_basn6a08_msssim.t](../../tests/loader/libpng/0039_pngsuite_basic_default_basn6a08_msssim.t), [tests/loader/libpng/0040_pngsuite_basic_default_basn6a16_msssim.t](../../tests/loader/libpng/0040_pngsuite_basic_default_basn6a16_msssim.t) |
| LPQ-03 | Multiple zero-alpha palette entries and a normalized single-zero fixture produce matching MS-SSIM 1.0 output with CMS disabled; this is not a direct index-map assertion. | [tests/loader/libpng/0172_loader_libpng_palette_trns_multi_zero_remap_icc.t](../../tests/loader/libpng/0172_loader_libpng_palette_trns_multi_zero_remap_icc.t) |


### Defensive and malformed-input tests

The [libpng test category](../../tests/loader/libpng) additionally covers invalid APNG counts, geometry, operation values, sequence gaps, and ordering. PNGSuite and color-metadata matrices exercise more input variants and quality floors. LP-27 through LP-29 explicitly elevate selected rejection and recovery outcomes to behavioral contracts; other malformed specimens remain defensive coverage.

### Coverage boundary

Run the category with `HAVE_LIBPNG=1` and with both Little-CMS-enabled and disabled builds. Builtin-only skips do not validate this adapter. The numerical probes deliberately use small independently constructed inputs, including low-bit pairs, fractional blends, and a valid gray ICC profile. Broader encoded-output and MS-SSIM comparisons do not replace those exact observations.

The suite does not exhaust every color-type/depth/interlace combination, every ICC dialect or engine-specific rounding rule, all eight orientations, or the exact 64 MiB cache boundary. Cache allocation fallback has direct equivalence coverage. External terminal rendering and OSC 11 round trips remain outside these in-memory tests. The former byte-composition reference observation LPQ-02 is superseded by LP-21's numerical contract.
