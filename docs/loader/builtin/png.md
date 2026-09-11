# Builtin PNG and APNG Loader

## Format lineage

PNG was designed in the mid-1990s as a patent-free, lossless network raster format with indexed, grayscale, truecolor, alpha, color-management metadata, streaming, and integrity checks. It became a W3C Recommendation in 1996. APNG later extended the chunk language with animation while preserving a useful static fallback, and animation is now included in the [PNG Third Edition specification](https://www.w3.org/TR/png-3/).

libsixel's current builtin path is intentionally hybrid. [`frompng.c`](../../../src/frompng.c) owns high-depth decode, color metadata, source background policy, and reusable PNG helpers; [`loader-builtin.c`](../../../src/loader-builtin.c) owns APNG sequencing and selected indexed fast paths; adapted PNG and inflate machinery remains in [`stb_image.h`](../../../src/stb_image.h). Commit [`51bfc7f69`](https://github.com/saitoha/libsixel/commit/51bfc7f69) added builtin APNG in 2026. Commit [`ca38e5e4a`](https://github.com/saitoha/libsixel/commit/ca38e5e4a) introduced a precision-preserving 16-bit path, and [`44045c67b`](https://github.com/saitoha/libsixel/commit/44045c67b) extracted the dedicated module.

## Container, image types, and compression

A PNG begins with the fixed eight-byte signature. It then stores typed chunks as a big-endian length, four-byte type, payload, and CRC. `IHDR` defines width, height, bit depth, color type, compression, filter, and interlace; `IDAT` chunks form one zlib stream; `IEND` terminates the image.

| Color type | Source model | Accepted bit depths |
| --- | --- | --- |
| 0 | Grayscale | 1, 2, 4, 8, 16 |
| 2 | RGB truecolor | 8, 16 |
| 3 | Indexed through `PLTE` | 1, 2, 4, 8 |
| 4 | Grayscale plus alpha | 8, 16 |
| 6 | RGB plus alpha | 8, 16 |

Compression method 0 is supported: scanlines are filtered and compressed by zlib/DEFLATE. The decoder reverses filter types None, Sub, Up, Average, and Paeth, then reconstructs either ordinary rows or all seven Adam7 passes. Indexed samples are unpacked most-significant bits first and checked against `PLTE`.

The adapted path also recognizes Apple's private `CgBI` form and performs its channel-order and unpremultiplication compatibility work. That support is narrow: it does not turn every Apple-private chunk or malformed iPhone PNG variant into part of the PNG contract.

## Chunk semantics and extensions

| Chunk | Treatment |
| --- | --- |
| `IHDR`, `PLTE`, `IDAT`, `IEND` | Parsed as core image structure with ordering and size checks. |
| `tRNS` | Palette alpha or grayscale/RGB transparent key according to color type. |
| `bKGD` | File-supplied background candidate, resolved against `-B` by background policy. |
| `iCCP` | Embedded ICC source profile; compressed profile data is bounded and inflated before CMS use. |
| `sRGB` | Declares the standard sRGB interpretation and takes precedence over lower-level gamma/chromaticity fallback. |
| `cHRM` and `gAMA` | Supply chromaticities and transfer information when a higher-priority usable declaration is absent. |
| `eXIf` | Scanned for Exif orientation when builtin orientation is enabled. Other Exif tags are not exposed. |
| `acTL`, `fcTL`, `fdAT` | Select and describe APNG animation as detailed below. |
| Unknown ancillary chunks | Bounds-checked and skipped. |
| Unknown critical chunks | Rejected. |

The color interpretation order is usable `iCCP`, then `sRGB`, then the applicable `cHRM`/`gAMA` combination. Malformed or non-applicable metadata follows the format path's fallback rules rather than becoming a generic metadata object. Text chunks, `pHYs`, `tIME`, and HDR signaling such as `cICP`, `mDCv`, and `cLLi` have no semantic effect in the current decoder.

PNG specifies CRC validation, but these in-memory builtin paths currently read past stored CRC fields without verifying them. Successful decode therefore proves structural acceptance by this parser, not checksum authentication.

## Static decode pipeline and output forms

The dispatcher recognizes the PNG signature, probes for `acTL`, and chooses animation or static decode. Static decode first inspects depth, color, `bKGD`, `tRNS`, and color metadata, then chooses one of several paths instead of forcing every source through RGBA8.

| Condition | Possible initial frame |
| --- | --- |
| Indexed source, palette fusion enabled, palette fits the requested count, and no policy requires a different representation | Gamma `PAL8` plus an RGB palette and optional single transparent key. Multiple fully transparent palette entries are remapped to that one key. |
| Opaque eight-bit grayscale/RGB | Gamma `RGB888`. |
| Eight-bit alpha or transparent-key source not retained as indexed | RGBA is decoded and then normalized to RGB plus the frame's transparency representation or background composition. |
| Sixteen-bit opaque source | `RGBFLOAT32`, retaining normalized 16-bit sample precision. |
| Sixteen-bit alpha or background composition | Float processing; composition is performed in linear light and may produce `LINEARRGBFLOAT32`. |
| Successful color-management path | The configured gamma, linear, CIELAB, OKLab, or DIN99d typed target, with byte output only where the target/path permits and `prefer_8bit=1`. |

Indexed palette conversion happens before later resize and quantization. If the source palette exceeds `-p COLORS`, or scaling/non-default color mode disables palette fusion, the decoder expands it. `trns_keycolor=1` is a separate optimization for non-indexed `tRNS`: it is used only when there is no resolved background and no CMS operation that requires true color samples; otherwise the ordinary alpha/float path wins.

## APNG pipeline

`acTL` declares frame count and play count. Each `fcTL` gives a canvas sub-rectangle, delay numerator/denominator, disposal, and blend operator. `fdAT` carries frame zlib bytes after a sequence number; the default image can instead use `IDAT`. The implementation checks sequence numbers, declared frame count, chunk order, rectangle overflow, and canvas bounds.

Each frame is reconstructed as a synthetic PNG using shared chunks, decoded through `stbi__load_and_postprocess_8bit()`, and composited onto an RGBA8 canvas. This is a distinct path from `sixel_frompng_load_nonindexed()` and its precision-preserving static pipeline. Shared metadata being copied into the synthetic PNG does not imply that static PNG's float/CMS processing runs before animation blending:

| APNG field | Behavior |
| --- | --- |
| Blend 0 (`SOURCE`) | Replace the frame rectangle with decoded source pixels. |
| Blend 1 (`OVER`) | Alpha-composite source over the existing canvas. |
| Dispose 0 (`NONE`) | Keep the composed canvas for the next frame. |
| Dispose 1 (`BACKGROUND`) | Clear the frame rectangle after emission. |
| Dispose 2 (`PREVIOUS`) | Restore the saved pre-frame canvas after emission. |

The delay denominator defaults to 100 when encoded as zero. Delay is converted to libsixel's centisecond frame unit, so finer fractions are truncated by that boundary. A default image not participating in the animation remains the static fallback defined by APNG structure; invalid attempts to mix frame controls and data are rejected.

### Implementation and test map

![A vertical implementation map of the builtin PNG and APNG loader. It follows static-versus-animation classification into indexed and non-indexed raster paths, profile and transfer conversion, APNG frame reconstruction and canvas composition, then common alpha, orientation, and typed-frame finalization. Every node carries a coverage ID used by the tables below.](pipeline-figures/png.svg)

The static branches are alternatives, not a forced trip through each box: an indexed image may finish through `sixel_builtin_try_load_indexed_png()`, while non-indexed and high-depth images enter `sixel_frompng_load_nonindexed()`. APNG takes its own RGBA8 raster/canvas branch. Its integer `OVER` operation blends the stored component values; it is not switched to a float linear-light canvas by `background_colorspace`. Background flattening at emission is a separate operation.

| ID | Implementation boundary | State entering → state leaving | Correctness obligation |
| --- | --- | --- | --- |
| `PNG-01` | `sixel_builtin_load_stbi_png_path()` and `sixel_builtin_chunk_has_apng_control()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | PNG chunk → static path or APNG state machine | `acTL` controls animation routing; `-S` may request one emitted frame without changing the structural classification. |
| `PNG-02` | `sixel_builtin_try_load_indexed_png()` and `sixel_builtin_load_png_keycolor_or_rgba()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | PLTE/tRNS raster → `PAL8`, key-color, or RGBA branch | Palette size, fusion policy, CMS/background needs, and multiple transparent entries must select a representable output. |
| `PNG-03` | `sixel_frompng_load_nonindexed()` in [`frompng.c`](../../../src/frompng.c), backed by the adapted inflate/filter code in [`stb_image.h`](../../../src/stb_image.h) | Compressed scanlines → canonical byte or float RGB(A) samples | DEFLATE, PNG filters, Adam7 ordering, source depth, and alpha expansion must preserve numeric sample precision. |
| `PNG-04` | `sixel_frompng_build_profile_from_chunks()` and `sixel_frompng_apply_colorspace_fallback_internal()` in [`frompng.c`](../../../src/frompng.c) | Decoded samples plus iCCP/sRGB/cHRM/gAMA/bKGD → transformed pixels/background | Metadata precedence and source transfer must be applied in the same precision and colorspace as alpha/background composition. |
| `PNG-05` | `sixel_builtin_apng_process_chunk()`, `sixel_builtin_apng_blend_rect()`, and `sixel_builtin_apng_emit_pending_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | acTL/fcTL/IDAT/fdAT stream → completed animation canvases | Sequence numbers, frame rectangles, shared chunks, blend/dispose, timing, loop count, and pre-roll must remain coherent. |
| `PNG-06` | `sixel_builtin_finalize_frame_callback()` and `sixel_builtin_finalize_loaded_frame()` in [`loader-builtin.c`](../../../src/loader-builtin.c) | Format-owned temporary frame → public typed frame callback | Alpha must become composition or a separate transparency representation, and Exif orientation must rotate pixels and mask together. |

The generated SVG and stage/test manifest are maintained by [`plot_builtin_loader_format_figures.py`](../../../tools/plot_builtin_loader_format_figures.py); its `--check` mode verifies regeneration and all named source/document/test anchors.

## Options and observable differences

```console
img2sixel -Lbuiltin:trns_keycolor=1! image.png
img2sixel -Lbuiltin:cms_engine=auto:cms_target=linear:prefer_8bit=0! image.png
```

| Control | Exact effect on PNG/APNG |
| --- | --- |
| `builtin:cms_engine=none|auto|builtin|lcms2|colorsync` | `none` disables embedded/profile-derived conversion. Other values select an available engine; `auto` follows the project CMS priority. |
| `cms_target=gamma|linear|cielab|oklab|din99d` | Selects the semantic destination only when a source color transform runs. It does not relabel an unprofiled byte PNG by itself. |
| `prefer_8bit=0|1` | Requests byte output from loader CMS where that target/path supports it. It does not reduce a 16-bit untransformed source merely because the value is 1. |
| `cms_intent=...` | Sets the ordered rendering-intent attempts for applicable ICC transforms; its inner `!` closes only intent fallback. |
| `builtin:orientation=0|1` | Enables or disables `eXIf` orientation. It runs after decode, so width/height and masks rotate together. |
| `trns_keycolor=0|1` (`K0`/`K1`) | Enables the guarded non-indexed `tRNS` keycolor path described above. Default is `1`; it does not override background or CMS requirements. |
| `-B`, `background_policy`, `background_colorspace` | Resolve `bKGD` versus explicit background and define the explicit color's transfer interpretation before composition. |
| `-A auto|composite|clear|keep` | Controls final treatment of zero-alpha pixels after PNG/APNG decode. A resolved background may already have flattened partial alpha. |
| `-p COLORS`, resize options, and non-default encoder color modes | Affect whether indexed `PAL8` can be fused through the loader. Resize disables that fast path because resampling requires component pixels. |
| `-S`, `-T`, `-l`, `-g` | Apply only to APNG. Their start-frame, loop, static, and presentation-delay meanings match the GIF description; prior frames may be decoded for canvas state. |
| HDR, PNM, and BMP-specific suboptions | No effect. |

## Comparison with the libpng loader

The comparison is between libsixel's `builtin` and `libpng` loader components, including their adapters and color/alpha processing, not between bare stb_image and bare libpng. Both components support static PNG, precision-preserving static 16-bit paths, and APNG. In the libpng component, libsixel implements APNG chunk sequencing, reconstruction, blend/dispose, and looping around ordinary libpng raster decoding; an APNG-patched libpng is not required. See [`loader-libpng.c`](../../../src/loader-libpng.c), especially `load_png()`, `load_apng_frames()`, `apng_blend_rect()`, and `load_with_libpng()`.

### Precision and representation are conditional

| Input and effective path | Observed relationship / limit |
| --- | --- |
| Opaque RGB8, CMS off, no palette | Both return `RGB888`; the measured samples are identical. PNG's lossless decompression does not itself create a quality advantage for either loader. |
| Opaque RGB16, CMS off | Both retain normalized 16-bit values in `RGBFLOAT32`; the measured float samples are identical. A 16-bit source is not automatically truncated by the libpng component. |
| Non-indexed RGBA8/16 with the same resolved background | Both can return `LINEARRGBFLOAT32` after linear-light composition. The measured RGBA16 samples are identical; RGBA8 differs by at most `2.98e-8` in normalized linear RGB. This is floating-point rounding, not a measured perceptual advantage. |
| RGB16 with `gAMA=1`, builtin CMS, gamma target | Both measured paths return identical `RGBFLOAT32`. This single fallback-transfer case does not establish parity for all ICC profiles, CMS engines, rendering intents, or targets. |
| RGB16 with `tRNS`, CMS off, no background, `trns_keycolor=1` | Both key-color paths reduce to eight-bit component values. In the probe, source `(32768,32769,32770)` becomes `(128,128,128)`. Builtin exposes RGB plus a mask; libpng exposes RGBA8 at this callback boundary. |
| Same `tRNS` input with `trns_keycolor=0` | Both retain the distinct visible component values in linear float. In the measured default-composite/no-background case, builtin preserves the transparent pixel's mask while libpng flattens it to black without a mask. This observed difference is not a compatibility promise. |
| APNG | Both animation raster/canvas paths are eight-bit. Neither should be described as a precision-preserving 16-bit animation compositor merely because its static PNG path supports float32. |
| Integrity checking | Builtin skips stored PNG CRCs. The libpng adapter retains libpng's default CRC processing. Equal decoded pixels on valid files do not imply equivalent validation work or acceptance of damaged input. |

The static timings below deliberately avoid the key-color path and palette fusion. `prefer_8bit=0` does not restore low bits already removed by an earlier key-color path. Conversely, enabling `prefer_8bit=1` is not a universal instruction to truncate every untransformed 16-bit source. Inspect the returned frame representation before drawing precision conclusions.

### Background source, interpretation, and blend arithmetic

These are three separate decisions. `background_policy=file_first|explicit_first` selects a source; `background_colorspace=gamma|linear` controls how background values are interpreted on the selected path; the path itself determines where alpha arithmetic takes place. For the static non-indexed float paths, foreground samples are decoded into linear light and composited there under either background setting. With `-B#808080`, `gamma` supplies approximately `0.21586` linear intensity, whereas `linear` supplies `128/255`, approximately `0.50196`. Thus changing this option changes the color denoted by the background argument, not just rounding or speed. PNG's [reference compositing procedure](https://www.w3.org/TR/png-3/#13Alpha-channel-processing) likewise distinguishes transfer decoding from alpha arithmetic.

| Boundary | Builtin | libpng component |
| --- | --- | --- |
| Explicit background and `bKGD` both present | `sixel_frompng_resolve_background()` applies `background_policy`; default `file_first` can select the file color. | `png_resolve_background_unit()` selects a supplied explicit background immediately. Setting the common `background_policy=file_first` suboption does not change that branch in the measured revision. |
| Non-indexed RGBA8/16 static composition | `sixel_frompng_convert_rgba8_to_linearrgbfloat32()` / `sixel_frompng_convert_rgba16_to_linearrgbfloat32()` transform source and background and blend in linear float. | `load_png()` has corresponding alpha/float branches; matching source selection and background interpretation can produce matching samples. |
| Indexed `PAL8` fast path, partial `tRNS`, explicit background, no CMS or `bKGD` | The retained palette is composited with byte arithmetic on gamma component values. The probe remains unchanged when `background_colorspace` changes to `linear`. | `read_palette()` uses gamma byte arithmetic for `gamma`, but transfer-decodes the foreground and uses linear arithmetic for `linear`, then encodes back into the byte palette. |
| Indexed input with `bKGD` | Dispatch bypasses both indexed fast paths and enters `frompng`, so background policy and float/alpha handling are resolved together. | Has its own palette/alpha dispatch and background resolver; do not infer builtin path selection from a shared filename or option. |
| APNG `OVER` and subsequent background flattening | Integer RGBA8 inter-frame blend; emission/background normalization is a later step. | Integer RGBA8 inter-frame blend with a separate emission/background path. |

For a concrete source `(240,32,16,128)`, file background `(16,128,240)`, and explicit background `#808080`, the measured non-indexed linear output is `(0.439973,0.114757,0.436576)` under builtin `file_first:background_colorspace=gamma`, and `(0.544899,0.114757,0.110108)` under builtin `explicit_first:background_colorspace=gamma`. libpng produces the latter with either priority setting. For the separate palette-retaining specimen without `bKGD`, `background_colorspace=linear` gives byte RGB `(183,79,71)` in builtin and `(216,139,138)` in libpng. These are visibly different policy/path outcomes, not DEFLATE precision losses.

To judge which result is better, first fix the intended contract. Under `file_first`, builtin selects the requested file background; the libpng adapter's explicit-background choice is a policy mismatch. After encoding the non-indexed outputs for display, the colors round to `(177,95,176)` versus `(195,95,93)`: the blue channel differs by 83 byte levels. Under `explicit_first`, both agree, so the distinction is source selection rather than a universally better color algorithm.

For the indexed `background_colorspace=linear` specimen, the analytic reference is `decode_sRGB(source) * alpha + (128/255) * (1-alpha)`, because the explicit background value is specified as linear. Its linear RGB is approximately `(0.687388,0.257246,0.252597)`. The builtin palette output is darker than this reference by up to `0.213857` in normalized linear RGB, while the libpng byte palette's maximum error is `0.001555` after its final eight-bit rounding. In display RGB the two results differ by `(33,60,67)` levels. Thus libpng is more accurate for this particular linear-composition request; builtin's retained-palette branch does not implement the requested arithmetic. Changing the background interpretation while comparing colors would otherwise conflate two different target colors.

For `tRNS`, precision loss and coverage loss must be assessed independently. With key-color enabled, both loaders map the visible 16-bit triplet `(32768,32769,32770)` to byte `(128,128,128)`, erasing the one-step 16-bit distinctions; this is roughly half an eight-bit step of error for this specimen, not a large visible color shift by itself. Disabling key-color preserves the visible float samples in both loaders. However, with default composite policy and no resolved background, the builtin mask keeps the second pixel transparent while libpng returns an opaque black pixel with no transparency mask. A direct `img2sixel -L<backend>:cms_engine=none:trns_keycolor=0! -dnone` check also shows the builtin SIXEL omitting that pixel and the libpng SIXEL painting it near black. This is a coverage error relative to preserving transparency, not merely a low-bit precision difference. These are current cross-backend observations, not a claim about when a historical regression or improvement was introduced.

For a builtin PNG requiring explicit-background linear-light processing, specify the source priority and verify that the effective route expands to component pixels. Merely requesting `background_colorspace=linear` while retaining an indexed palette is insufficient in the measured implementation. The [background policy](../background-policy.md) and [alpha policy](../alpha-policy.md) describe the common controls; their availability does not imply identical handling in every loader.

### Measured static PNG cost

**Interpretation after profiling:** the following graph measures the complete libsixel loader adapters. In the recorded revision, the libpng adapter performs avoidable APNG preparation even for static PNG: `load_apng_frames()` allocates and clears two RGBA canvases at `IHDR`, copies `IDAT` through `append_chunk()`, and recomputes its CRC with the adapter's bit-at-a-time `crc32_update()`. Only after the chunk scan does absence of `acTL` return `SIXEL_FALSE` and trigger ordinary `load_png()`. Builtin first classifies static versus animated PNG. Consequently, these bars do not establish that builtin's PNG codec beats libpng's codec. The overhead is in [`loader-libpng.c`](../../../src/loader-libpng.c), not in zlib's CRC implementation. Both ordinary RGB8 paths transfer ownership of the final pixel buffer through `init_pixels()`; that handoff does not add a whole-image copy on just the libpng side.

An independent timer reproduced the complete-loader ordering. A separate memory-to-RGB8 control, with no libsixel adapter or file I/O and exact decoded-pixel agreement, reversed the ranking on the tested photographs: for the same 512 × 512 RGB8 specimens, the adapted stb decoder took approximately 3.34 ms (None filter) and 3.37 ms (Paeth), while libpng took 1.92 ms and 2.57 ms. Disabling libpng's own CRC checking only changed those controls to 1.89 ms and 2.55 ms. The actual library paths were verified as Homebrew libpng 1.6.58 and macOS `/usr/lib/libz.1.dylib`; a runtime sample showed `png_read_filter_row_paeth3_neon()` executing. A 1024 × 1024 incompressible-noise probe attributed 2369 of 2534 CPU samples (93.5%) to the adapter's inlined CRC work inside `append_chunk()`, versus five samples in its `memcpy`. This identifies the adapter prepass as the dominant anomaly; it does not establish a general Apple Silicon or zlib hardware-acceleration limitation. The decoder-only control includes decoder allocation and release, excludes its preflight comparison, and uses six alternating batches of 30 decodes for the 512 × 512 specimens.

![Two grouped horizontal bar charts compare builtin and libpng loader time. Builtin is faster in all six measured conditions. Bars begin at zero and show milliseconds per call; whiskers show the interquartile range. The opaque-decode panel and composition/CMS panel use different scales.](measurements/png-loaders.svg)

| 512 × 512 input / operation | Builtin median (ms) | libpng median (ms) | Returned representation (both) | Maximum linear RGB difference |
| --- | ---: | ---: | --- | ---: |
| RGB8, None filter | 3.232 | 5.761 | `RGB888` | 0 |
| RGB8, Paeth filter | 3.289 | 4.942 | `RGB888` | 0 |
| RGB16, Paeth filter | 2.257 | 3.392 | `RGBFLOAT32` | 0 |
| RGBA8, explicit background | 10.141 | 12.841 | `LINEARRGBFLOAT32` | `2.98e-8` |
| RGBA16, explicit background | 8.489 | 9.671 | `LINEARRGBFLOAT32` | 0 |
| RGB16, `gAMA=1`, builtin CMS | 14.330 | 18.173 | `RGBFLOAT32` | 0 |

Measured on Apple M3 Max, arm64 macOS, libpng 1.6.58, libsixel revision `34d1a6bad`, Clang `-O3 -DNDEBUG`, no lcms2, one calling thread. Each entry is the median of nine alternating paired batches of eight calls. Each batch has an untimed pixel export and two warmup calls; whiskers show the first and third quartiles of batch averages. Each timed call includes cached file reading, dispatch, decode, normalization, frame callback, and release. It excludes process startup, loader construction, pixel export, resize, quantization, dithering, SIXEL generation, and terminal I/O. These are warm-loader timings, not cold-start or whole-encoder timings.

RGB8 uses `images/snake.png` resized to 512 × 512 before measurement; RGB16 uses a deterministic three-channel ramp with nonzero low bits. RGBA variants add fixed alpha (`128/255` or `32768/65535`) and explicit `#808080`. The writer uses zlib level 6 and known None/Paeth row filters without interlace or incidental color metadata. The gamma fixture alone carries `gAMA=100000`. All timed runs disable palette fusion, use `prefer_8bit=0`, gamma background interpretation, and a terminal `!` on the selected loader; CMS is off except for the marked transfer case. The two RGB8 files encode identical pixels but different filtered streams. Different depths use different content/compressibility, so the faster RGB16 row is not evidence that 16-bit decoding is inherently cheaper.

Builtin took approximately 12–44% less time in these cases. This does not establish a universal speed ranking. The [official libpng source](https://github.com/pnggroup/libpng) includes architecture-specific filter optimizations such as ARM NEON, while builtin's adapted [`stb_image.h`](../../../src/stb_image.h) PNG unfilter/inflate path is largely scalar C (its JPEG SIMD should not be mistaken for PNG SIMD). This measurement does not isolate or disable libpng SIMD. Filter mix, DEFLATE implementation, compressed size, allocation, metadata, CMS, and alpha processing all contribute; counting SIMD kernels alone does not predict the complete-loader result.

The [raw measurement record](measurements/png-loaders.json) includes all timing samples, native pixel hashes, output formats, maximum/RMS error against independently computed linear reference samples, precision/background probes, versions, configure arguments, and binary/input hashes. Exact native comparisons precede canonical linear comparisons. The independent reference error is below `1e-7` for the six timed cases. This is a loader-boundary sample/precision experiment, not an MS-SSIM comparison of final SIXEL images; downstream quantization can amplify even small differences. Existing end-to-end quality owners are listed below.

### Reproduce the measurements

Use a clean checkout of the recorded revision with libpng enabled, `--with-lcms2=no`, and `CFLAGS='-O3 -DNDEBUG'`; the JSON records the full configuration. Build `src`, then compile the public loader-API probe against that build. Use an isolated build when another task is changing the working tree. The Python runner requires NumPy, Matplotlib, ImageMagick, and pkg-config:

```sh
cc -O3 -DNDEBUG -I"$PNG_BUILD/include" tools/bench/png-loader.c \
    -L"$PNG_BUILD/src/.libs" -lsixel -o "$PNG_BUILD/png-loader"
python3 tools/measure_png_loaders.py \
    --build "$PNG_BUILD" --probe "$PNG_BUILD/png-loader" \
    --revision 34d1a6bad --output docs/loader/builtin/measurements/png-loaders.json
python3 tools/measure_png_loaders.py --plot-only \
    --output docs/loader/builtin/measurements/png-loaders.json
python3 tools/measure_png_loaders.py --check \
    --output docs/loader/builtin/measurements/png-loaders.json
```

`PNG_BUILD` is the absolute configured build directory. The runner pins its library search path to that build and removes inherited `SIXEL_*`/`LSQA_*` policy variables. The generated fixtures are temporary; their construction and hashes are retained. `--plot-only` regenerates the static SVG from saved observations without rerunning timing. `--check` verifies saved medians/quartiles and byte-identical SVG regeneration; it does not rerun timing or assert a hardware-independent speed threshold. Use the recorded package versions for byte-identical regeneration. The graph is a single Markdown-embedded SVG, not a separately selected mobile page.

## Unsupported behavior and security boundary

MNG and JNG are separate formats and are not decoded. Unknown critical chunks, invalid standard depth/type combinations, inconsistent APNG sequence graphs, and out-of-canvas frames fail. Arbitrary per-frame color profiles and general ancillary metadata preservation are not implemented.

PNG combines attacker-controlled chunk lengths, DEFLATE expansion, filters, interlace passes, profiles, and animation canvases. CRC skipping makes bounds validation especially important and means another integrity layer is required when checksums are part of the trust model. `-Lbuiltin!` prevents a fallback decoder from accepting a rejected dialect but does not isolate this parser.

## Implementation landmarks

- High-depth/static decode and color metadata: [`frompng.c`](../../../src/frompng.c)
- APNG, indexed routing, orientation, and finalization: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Adapted inflate and CgBI helpers: [`stb_image.h`](../../../src/stb_image.h)
- Alpha/background semantics: [Alpha Policy](../alpha-policy.md) and [Background Policy](../background-policy.md)
- Broader non-owning regression suite: [`tests/loader/builtin`](../../../tests/loader/builtin)

## Test coverage

<!-- test-coverage: enforced -->

The rows below are pipeline landmarks rather than the complete suite. The [PNG/APNG coverage inventory](../../testing/builtin-loader-coverage.md#png-and-apng) lists every format-classified test, including supplementary and defensive cases.

### Behavioral contract tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| PNG-01 | `-S` selects the APNG static behavior without treating the file as an unrelated residual format. | [tests/loader/builtin/0017_apng_builtin_static_option.t](../../../tests/loader/builtin/0017_apng_builtin_static_option.t) |
| PNG-02 | Indexed tRNS uses the documented default key-color path when its palette representation is valid. | [tests/loader/builtin/0143_loader_builtin_png_trns_keycolor_default_enabled_for_palette.t](../../../tests/loader/builtin/0143_loader_builtin_png_trns_keycolor_default_enabled_for_palette.t) |
| PNG-03 | A 16-bit RGBA source follows the high-depth alpha/key-color decision rather than silently collapsing into the ordinary eight-bit path. | [tests/loader/builtin/0166_loader_builtin_trns_keycolor_optin_changes_rgba16_output.t](../../../tests/loader/builtin/0166_loader_builtin_trns_keycolor_optin_changes_rgba16_output.t) |
| PNG-04 | Builtin ICC conversion produces a linear `LINEARRGBFLOAT32` frame with fixed representative samples. | [tests/loader/builtin/1977_loader_builtin_png_icc_numeric.t](../../../tests/loader/builtin/1977_loader_builtin_png_icc_numeric.t) |
| PNG-05 | APNG disposal `PREVIOUS` restores the saved canvas before a third frame is composed; all three decoded RGB buffers are compared exactly. | [tests/loader/builtin/1962_loader_builtin_png_apng_dispose_previous_numeric.t](../../../tests/loader/builtin/1962_loader_builtin_png_apng_dispose_previous_numeric.t) |
| PNG-06 | Enabling and disabling eXIf orientation changes geometry/pixels at common frame finalization as documented. | [tests/loader/builtin/1668_loader_builtin_png_orientation_toggle.t](../../../tests/loader/builtin/1668_loader_builtin_png_orientation_toggle.t) |
| PNG-07 | Builtin's file-first default, explicit-first selection, and invalid-policy fallback determine competing background priority. | [tests/loader/builtin/1497_loader_builtin_background_policy_png_priority.t](../../../tests/loader/builtin/1497_loader_builtin_background_policy_png_priority.t) |
| PNG-08 | The builtin non-indexed linear-background option agrees with its reference image, and CLI/suboption forms agree. | [tests/cli/options/regression/0098_loader_background_colorspace_image_regression.t](../../../tests/cli/options/regression/0098_loader_background_colorspace_image_regression.t) |
| PNG-09 | The libpng component accepts APNG and can emit its static selection. | [tests/loader/libpng/0001_apng_libpng_static_option.t](../../../tests/loader/libpng/0001_apng_libpng_static_option.t) |
| PNG-10 | APNG maps a zero delay denominator to 100 and emits exact delay, frame, loop, and multiframe metadata. | [tests/loader/builtin/1994_loader_builtin_apng_animation_metadata_numeric.t](../../../tests/loader/builtin/1994_loader_builtin_apng_animation_metadata_numeric.t) |
| PNG-11 | Scanline filters None, Sub, Up, Average, and Paeth reconstruct one fixed RGB image exactly. | [tests/loader/builtin/2001_loader_builtin_png_filter_modes_numeric.t](../../../tests/loader/builtin/2001_loader_builtin_png_filter_modes_numeric.t) |
| PNG-12 | The seven Adam7 passes reconstruct a four-bit indexed image into the fixed complete RGB buffer. | [tests/loader/builtin/2002_loader_builtin_png_adam7_indexed4_digest.t](../../../tests/loader/builtin/2002_loader_builtin_png_adam7_indexed4_digest.t) |
| PNG-13 | Gray16 values that cannot be represented as eight-bit increments remain exact in a gamma `RGBFLOAT32` frame. | [tests/loader/builtin/2003_loader_builtin_png_gray16_sub8bit_numeric.t](../../../tests/loader/builtin/2003_loader_builtin_png_gray16_sub8bit_numeric.t) |
| PNG-14 | APNG `SOURCE` and `OVER` blending produces two fixed complete RGB canvases. | [tests/loader/builtin/2004_loader_builtin_apng_blend_over_digest.t](../../../tests/loader/builtin/2004_loader_builtin_apng_blend_over_digest.t) |
| PNG-15 | APNG `BACKGROUND` disposal clears the affected rectangle before the next fixed canvas is composed. | [tests/loader/builtin/2005_loader_builtin_apng_dispose_background_digest.t](../../../tests/loader/builtin/2005_loader_builtin_apng_dispose_background_digest.t) |

### Quality regression tests

| ID | Quality floor protected | Owning test |
| --- | --- | --- |
| PNGQ-01 | Embedded ICC conversion remains visually close to the fixed converted PNM after the full encode path. | [tests/loader/builtin/0055_builtin_png_embedded_icc_matches_reference_pnm.t](../../../tests/loader/builtin/0055_builtin_png_embedded_icc_matches_reference_pnm.t) |
| PNGQ-02 | The libpng RGBA8 and RGBA16 paths retain their end-to-end PNGSuite MS-SSIM floors. | [tests/loader/libpng/0039_pngsuite_basic_default_basn6a08_msssim.t](../../../tests/loader/libpng/0039_pngsuite_basic_default_basn6a08_msssim.t), [tests/loader/libpng/0040_pngsuite_basic_default_basn6a16_msssim.t](../../../tests/loader/libpng/0040_pngsuite_basic_default_basn6a16_msssim.t) |

### Defensive and malformed-input tests

| ID | Contract protected | Owning test |
| --- | --- | --- |
| PNG-90 | An `fcTL` placed after image data in an invalid structural position is rejected. | [tests/loader/builtin/0035_apng_builtin_invalid_fctl_after_idat.t](../../../tests/loader/builtin/0035_apng_builtin_invalid_fctl_after_idat.t) |

Coverage audit note: direct owners now fix all five scanline filters in one image, all seven Adam7 passes for one indexed/depth combination, one sub-eight-bit Gray16 boundary, and representative `SOURCE`/`OVER`/`BACKGROUND`/`PREVIOUS` animation canvases. They do not establish the full filter × pass × depth × color-type product or every APNG ordering, blend, disposal, depth, and alpha combination. CRC validation is not an uncovered promised behavior: the implementation deliberately does not validate stored CRCs, as documented above.

The performance and sample-comparison records are reproducible observations, not a timing CI gate or exhaustive backend-equivalence test. In particular, the measured libpng background-priority difference, indexed linear-composition difference, and no-background `tRNS` mask difference are documented limitations; the existing behavioral owners do not guarantee those differences remain unchanged. APNG frame count/disposal tests do not prove linear-light inter-frame blending or 16-bit preservation.
