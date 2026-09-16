# Animation playback in img2sixel

`img2sixel` plays an animation by loading successive composed images, converting each image to SIXEL, positioning the cursor for replacement, and pacing output on the host. Animation support therefore depends on both the input format and the selected loader. Being able to open a GIF, PNG, or WebP file does not by itself establish support for all of its frames, delays, disposal operations, or finite playback count.

The principal controls are `-l` for repetition, `-S` for a still frame, `-T` for the starting frame, and `-g` for skipping presentation waits. Some CLI help descriptions still name GIF, but the controls also participate in the supported APNG and WebP paths. Terminal macros, temporal dithering, delta output, and alpha policy affect later stages independently.

## Typical commands

```sh
# Play one pass, including all frames, through a known animation loader.
img2sixel -Lbuiltin! -ldisable animation.gif
img2sixel -Lbuiltin! -ldisable animation.png
img2sixel -Llibwebp! -ldisable animation.webp

# Use the file's playback count, including an indefinite count if present.
img2sixel -Lbuiltin! -lauto animation.gif

# Extract the last composed animation frame as one SIXEL image.
img2sixel -Lbuiltin! -S -T-1 animation.png -o last-frame.six

# Convert a single pass without presentation waits.
img2sixel -Lbuiltin! -g -ldisable animation.webp -o frames.six
```

Optional loader names require the corresponding build dependency. Check `img2sixel --version` for the available loaders. `-Lbuiltin!` is useful when a reproducible in-tree GIF/APNG/WebP path is needed. The final `!` prevents another loader from being appended; `-Lbuiltin` only changes priority. The [loader architecture](../loader/README.md) explains the registry and fallback rules.

A saved `frames.six` contains terminal control bytes, not a container with a portable animation timeline. SIXEL does not carry the source frame durations or loop count. Redirecting output can still incur host-side waits unless `-g` is supplied, but those waits are not stored in the file: `cat frames.six` cannot reconstruct the original cadence. `-g` also leaves repetition enabled, so pair it with `-ldisable` when a finite conversion is required.

## Loader and format support

This table describes libsixel's adapters, rather than the maximum capabilities advertised by their underlying libraries or operating systems.

| Loader | Animated inputs handled by this adapter | Timing and repetition | Frame selection and boundary |
| --- | --- | --- | --- |
| `builtin` | GIF, APNG, animated WebP with VP8/VP8L frame payloads. | Parses format-specific delay, loop, blend, and disposal metadata. Supports `-l`, with the GIF exception below. | `-S` and `-T`; earlier dependent rectangles are composed before the selected frame is emitted. |
| `libpng` | APNG as well as static PNG. | libsixel parses APNG control chunks, reconstructs rectangle PNGs for libpng, composes canvases, and applies the play count. An APNG-patched libpng API is not required. | `-S` and `-T`; a separate non-animation default image is excluded from animation numbering. |
| `libwebp` | Animated WebP. | `WebPAnimDecoder` supplies composed frames and timestamps; libsixel applies the loop count and an output-frame limit. | `-S` and `-T`; selecting a one-frame first-pass tail currently prevents further loops. |
| `coregraphics` | ImageIO sources exposing GIF, APNG, WebP, or HEICS animation dictionaries. Actual decode support depends on the host framework. | Reads framework delay and loop properties; prefers unclamped delay. Missing loop metadata means one pass in `auto`. | `-S` and `-T` on recognized animations. Other multi-image containers are treated as still-image sources. A static HEIC file is not evidence of HEICS animation support. |
| `gdk-pixbuf2` | Animations exposed by installed GdkPixbuf modules, commonly GIF. APNG/WebP animation cannot be inferred from static-image support. | Time-based iterator stepping; no generic API for the source's finite loop count. `auto` and `force` share the adapter's restart behavior. | Iterator-based `-T`, including an extra counting traversal for negative indices. See the limitations below before relying on exact frame or loop counts. |
| `wic` | One selected frame from a WIC decoder. | No timed animation playback: the adapter returns after one frame and does not implement the GIF canvas/disposal or loop/delay protocol. | `-T` selects a frame in a non-ICO multi-frame container; this is frame extraction. `-S` and `-l` do not turn it into a player. ICO has separate size-selection rules. |
| `gd`, `libtiff`, `libjpeg`, `librsvg` | Still-image/rasterization paths in these adapters. | No animation timeline or repetition. | In particular, a multipage TIFF is not played as an animation by `libtiff`. |
| `quicklook`, `gnome-thumbnailer` | Preview/thumbnail output. | No source animation timeline. | Suitable for a representative still, not preservation of the original animation. |

The default registry puts `libpng` and `libwebp` ahead of `builtin`, and puts `builtin` ahead of WIC, CoreGraphics, and GdkPixbuf. Consequently, installing a dependency or explicitly changing `-L` can change the animation path. A successful still-image decode stops the loader chain: the manager does not reject it merely because another backend could have supplied an animation.

### GdkPixbuf: frame access is not loop-count access

The public [`GdkPixbufAnimation`](https://docs.gtk.org/gdk-pixbuf/class.PixbufAnimation.html) interface exposes a static-image probe and an iterator, but no numeric source loop-count or total-frame-count accessor. [`GdkPixbufAnimationIter`](https://docs.gtk.org/gdk-pixbuf/class.PixbufAnimationIter.html) supplies the image and delay at a point in time. [`gdk_pixbuf_simple_anim_get_loop()`](https://docs.gtk.org/gdk-pixbuf/method.PixbufSimpleAnim.get_loop.html) is a boolean for the separate `GdkPixbufSimpleAnim` class; it is neither a finite repeat count nor a generic query applicable to every decoder's animation object. These animation APIs are deprecated since GdkPixbuf 2.44.

The current [`gdkpixbuf_emit_animation_frames()`](../../src/loader-gdk-pixbuf2.c) implementation advances a synthetic clock by the reported delay, copies the current pixbuf, and restarts the iterator after a traversal. Only `disable`, an empty traversal, or its single-traversed-frame guard prevents that restart; there is no source-count check distinguishing `auto` from `force`. A finite-loop GIF can therefore repeat beyond its declared count. This is a limitation of the current integration, not proof that GdkPixbuf's decoder ignores the count internally.

Traversal itself has a weaker contract than explicit container frame parsing. The adapter uses `on_currently_loading_frame()` and a false return from `advance()` as stopping conditions. The upstream documentation defines these as a [loading/last-frame predicate](https://docs.gtk.org/gdk-pixbuf/method.PixbufAnimationIter.on_currently_loading_frame.html) and a [display-update indication](https://docs.gtk.org/gdk-pixbuf/method.PixbufAnimationIter.advance.html), respectively. They are not a generic indexed-frame enumeration API. Exact traversal length, the final frame, and negative-index resolution must therefore be checked with the actual module; `-ldisable` stops after one adapter traversal, not a proven portable enumeration of every container frame.

There is also a current `-S` interaction: it stops the inner traversal after a selected callback, but the outer restart condition does not test `fstatic`. With a nonzero start index, that can produce an additional callback on the next traversal unless `-ldisable` is also selected. Use `-Lbuiltin! -S -T N` for exact GIF/APNG/WebP still extraction, or explicitly combine `-S -T N -ldisable` when evaluating GdkPixbuf. These iterator limitations are code-inspected gaps; the existing GdkPixbuf smoke tests do not establish exact finite-loop or last-frame behavior across modules.

## Playback controls and interactions

| Option | Meaning | Important interaction |
| --- | --- | --- |
| `-l auto`, `--loop-control=auto` | Default: use the loader's interpretation of source repetition. | Zero normally means indefinite playback for GIF/APNG/WebP; missing GIF loop metadata means one pass. GdkPixbuf cannot provide the same finite-count contract. |
| `-l disable` | Stop after one traversal. | This still emits successive animation frames; it is not the same as `-S`. With `-T`, only the selected first-pass tail is emitted. |
| `-l force` | Ignore a finite count and request repetition. | Does not animate static input. Single-frame guards, missing GIF loop metadata, and libwebp's output limit still apply. |
| `-S`, `--static` | On full animation paths, stop after the first selected frame. | `-T` selects that frame; decoding/composition of earlier dependencies may still be necessary. Encoder waits and animated cursor handling are disabled. GdkPixbuf has the restart caveat above. |
| `-T N`, `--start-frame=N` | Zero-based source index; negative values count from the end (`-1` is last). | Overrides `SIXEL_LOADER_ANIMATION_START_FRAME_NO`. On animation paths, out-of-range indices fail; static inputs can ignore the setting. WIC uses it for extraction instead of playback. |
| `-g`, `--ignore-delay` | Skip encoder presentation waits. | Does not change frame selection, loop count, composition, or parsed delay metadata. It is not a frame-rate setting. |
| `-L CHAIN`, `--loader-order=CHAIN` | Choose the loader route. | Close the chain with `!` to compare actual backend behavior without fallback. |
| `-u`, `--use-macro` | Define terminal macros on the first loop and invoke them for playback. | Requires a macro-capable receiver; the host still controls repetition and timing. |
| `-n ID`, `--macro-number=ID` | Define an image at one fixed macro ID without invoking it. | Overrides automatic invocation even with `-u`; use `-S` to preload one selected animation frame. |
| `-z 0|1`, `--terminal-policy=0|1` | Control hiding of the cursor during animated TTY output. | `img2sixel` defaults to 1 when neither this option nor `SIXEL_ANIMATION_HIDE_CURSOR` is configured; the library default is 0. This does not select a loader or enable looping. |

### What a loop count means here

The source playback count and `sixel_frame_t`'s `loop_count` are different quantities. The latter is exposed through `sixel_frame_get_loop_no()` and records the current zero-based playback pass. It is not the number of passes requested by the file. `frame_no` likewise identifies an emitted frame within a pass; it can differ from the original source index after `-T`.

For a two-frame source played twice, callback metadata normally proceeds as follows:

| Callback | Source frame | `frame_no` | `loop_no` |
| --- | --- | --- | --- |
| 1 | 0 | 0 | 0 |
| 2 | 1 | 1 | 0 |
| 3 | 0 | 0 | 1 |
| 4 | 1 | 1 | 1 |

The builtin GIF implementation interprets a positive `NETSCAPE2.0` value `N` as `N` total passes, including the first; it does not add an initial pass to that number. A value of 2 therefore yields four callbacks for a two-frame file. Zero allows indefinite playback. Without a recognized Netscape extension, `gif_advance_loop_and_should_stop()` ends after one pass even under `force`. This is the current libsixel behavior, not a claim that every GIF viewer interprets the extension identically.

APNG's [`acTL.num_plays`](https://www.w3.org/TR/png-3/#11acTL) and WebP's [`ANIM` loop count](https://developers.google.com/speed/webp/docs/riff_container#animation) specify playback counts, with zero indicating indefinite repetition. The corresponding in-tree/libpng/libwebp paths consume those fields rather than a generic image-framework loop query. ImageIO paths consume the values supplied by their framework dictionaries.

`-T` normally applies only to the first pass; subsequent passes start at source frame zero. The first emitted frame is renumbered to zero so cursor setup and output state start correctly. It does not imply random-access decoding: disposal and blending may depend on every preceding frame. Negative indices can also require an initial counting scan. Attach a negative value to the option (`-T-1` or `--start-frame=-1`); a separate `-1` token is parsed as an option by the current CLI.

The libwebp adapter has a narrower stopping condition than the builtin WebP path: after a pass it stops if only one frame was emitted. Consequently, `-T-1` can terminate after that selected frame even with `-lauto` or `-lforce`, without `-S`. Also, libwebp caps cumulative emitted frames at 262144 by default; `-Llibwebp:max_output_frames=N!` and `SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES` configure that backend's guard. This is not a universal `img2sixel` frame limit or a substitute for `-l`.

## Format-specific timing and composition

The frame handoff stores delay in integer centiseconds (10 ms). Conversion to that common representation already loses some source timing precision before the encoder waits.

| Path | Source delay | Handoff conversion |
| --- | --- | --- |
| Builtin GIF | Graphic Control Extension centiseconds. | Preserved directly; a missing control block uses a 1 cs default, while an explicit zero remains zero. |
| Builtin/libpng APNG | `fcTL.delay_num / delay_den` seconds. | Zero denominator becomes 100; multiply by 100 and truncate, then raise a positive fraction that became zero to 1 cs. |
| Builtin/libwebp WebP | Milliseconds; libwebp exposes cumulative timestamps. | Integer division by 10; libwebp first takes the difference between successive timestamps. A duration below 10 ms can become zero. |
| GdkPixbuf | Iterator delay in milliseconds. | Negative delay becomes zero in this adapter, then integer division by 10. Upstream `-1` means display indefinitely, so the conversion does not preserve that meaning by itself. |
| CoreGraphics | Framework delay in seconds, preferring the unclamped property. | Multiply by 100 and truncate; a positive value that became zero is raised to 1 cs. |

Ordinary output checks the delay before encoding/writing the frame, accounting for elapsed host time. Macro output checks after invocation. Neither path is a video presentation clock: decode/quantization cost, queueing, terminal transport, and receiver rendering affect cadence. The encoder does not attach timestamps to SIXEL for the receiver to schedule. `-g` removes its deliberate waiting but cannot remove those processing costs.

GIF, APNG, and WebP can encode subrectangles whose meaning depends on earlier frames. A full animation loader applies blend and disposal rules to a canvas before passing the image to the encoder. GIF has binary transparency and local/global palettes; APNG has `SOURCE`/`OVER` and `NONE`/`BACKGROUND`/`PREVIOUS`; WebP has replace/blend and keep/background disposal. See the [GIF](../loader/builtin/gif.md), [PNG/APNG](../loader/builtin/png.md), [libpng APNG](../loader/libpng.md#apng-semantics), and [WebP](../loader/builtin/webp.md) references for composition, precision, and metadata details.

For APNG, the PNG default image belongs to the animation only if `fcTL` precedes its `IDAT`. Otherwise an animation-aware path omits that image from frame numbering, whereas a static-only PNG reader can show it as the file's still image. Thus two loaders may both successfully decode the same PNG yet display different first images.

Input disposal, output alpha, and temporal delta output are separate stages. `-A clear` requests clearing of omitted SIXEL pixels; `-A keep` preserves the receiver's existing pixels, which can leave trails where an object disappears. An explicit background with `-A composite` can make the emitted canvas opaque. `-A auto` normally resolves to clear, but delta output uses preservation. SIXEL cannot represent fractional alpha; partial coverage must be resolved through the [alpha](../loader/alpha-policy.md) and [background](../loader/background-policy.md) policies.

## Output reuse, quality, and threading

| Feature | State reused | Consequence for animation |
| --- | --- | --- |
| Loader replay cache | Decoded/composed frames retained on the host. | Reduces repeated decoding where available; does not eliminate conversion or transfer by itself. Cache availability and budgets are backend-specific. |
| `-u` terminal macros | Encoded frame bytes stored by the receiver. | Later passes invoke first-pass definitions instead of sending new image definitions. Transfer savings depend on repeated playback and receiver capacity. |
| `-d interframe` / `-d stbn` | Temporal quantization/error state. | Can change temporal appearance and per-frame output; inspect flicker and motion, not only a still-image quality score. |
| `-Z delta`, `--update-policy=delta` | Encoder history of colors retained on the display plane. | Omits suitable pixels; requires ordered delivery, stable placement, and a matching receiver image. The default `full` does not use temporal keeps. |
| `-m` or `-b` fixed palette | Palette choice across frames. | Can stabilize palette selection, but does not itself implement timing, disposal, or delta updates. |

Macro replay freezes the first-pass encoded images: it cannot display newly calculated temporal-dither results on later passes. Combining macros with `-T` also requires care because first-pass frame IDs describe the truncated first pass, while later passes restart at source frame zero. Do not assume that all later source frames have matching stored definitions; use `-ldisable`, or avoid combining a nonzero start with repeated macro playback. Macro and delta reuse likewise need their own ordering/state analysis rather than being assumed additive. See [terminal macros](terminal-macros.md), [dithering](dithering.md#interframe-residual-feedback-across-frames), and [delta encoding](delta-encoding.md).

Loading can overlap encoding through a bounded four-frame queue, but one frame-encoder worker consumes frames in order. `--threads=N` does not mean that `N` complete animation frames are encoded concurrently, nor does it promise an overall process thread cap. [Animation threading](../threading/animation.md) describes the queue, frame lifetime, and temporal-state constraints.

## Diagnosing an unexpected result

First identify the selected backend, then inspect callback sequence and timing separately from terminal presentation:

```sh
img2sixel --version
img2sixel -x human:trace_topic=loader,encode_handoff,lifecycle \
  -Lbuiltin! -g -ldisable animation.gif -o /dev/null
```

`loader` shows routing; `encode_handoff` exposes frame/loop callback order; `lifecycle` exposes delay handling when waits are enabled. Remove `-g` to examine that timing path. For APNG, `apng_decode` adds chunk/frame tracing. A trace or a successful conversion alone does not prove the number of displayed frames, exact composition, or receiver cadence.

If an animation runs forever, check `auto` with a zero source count, `force`, and the GdkPixbuf restart limitation. If it shows a still, check the selected adapter, `-S`, the APNG default-image distinction, and the libwebp one-emitted-frame guard. If images leave trails or scroll, inspect output alpha/cursor behavior and receiver state rather than changing the input disposal parser on that evidence alone.

## Implementation landmarks

| Boundary | Owning implementation |
| --- | --- |
| GIF parsing, composition, loop exception, and replay | [`fromgif.c`](../../src/fromgif.c), especially `gif_advance_loop_and_should_stop()`. |
| APNG classification, composition, and frame delivery | [`loader-builtin.c`](../../src/loader-builtin.c) and [`loader-libpng.c`](../../src/loader-libpng.c). |
| WebP animation and loop guards | [`fromwebp.c`](../../src/fromwebp.c) and `webp_decode_and_emit_multiframe_animation()` in [`loader-libwebp.c`](../../src/loader-libwebp.c). |
| Framework adapters | [`loader-gdk-pixbuf2.c`](../../src/loader-gdk-pixbuf2.c), [`loader-coregraphics.c`](../../src/loader-coregraphics.c), and [`loader-wic.c`](../../src/loader-wic.c). |
| Frame delay, cursor setup, macros, and ordered handoff | [`encoder.c`](../../src/encoder.c), [`encoder-core.c`](../../src/encoder-core.c), and [`tty.c`](../../src/tty.c). |

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation fixed by the test | Owning test |
| --- | --- | --- |
| AN-01 | Builtin GIF emits two callbacks with exact 7/23 cs delays, frame indices, loop indices, and multiframe flags. | [tests/loader/builtin/1993_loader_builtin_gif_animation_metadata_numeric.t](../../tests/loader/builtin/1993_loader_builtin_gif_animation_metadata_numeric.t) |
| AN-02 | Builtin APNG's zero-denominator fixture emits two callbacks with exact 1 cs delays and sequence metadata. | [tests/loader/builtin/1994_loader_builtin_apng_animation_metadata_numeric.t](../../tests/loader/builtin/1994_loader_builtin_apng_animation_metadata_numeric.t) |
| AN-03 | Builtin WebP converts the fixture's millisecond durations to exact 1 cs delays and retains frame/loop metadata. | [tests/loader/builtin/1995_loader_builtin_webp_animation_metadata_numeric.t](../../tests/loader/builtin/1995_loader_builtin_webp_animation_metadata_numeric.t) |
| AN-04 | Builtin GIF `disable` produces one pass even when the source loop count is zero. | [tests/loader/builtin/0257_loader_builtin_gif_loop_disable_loop0_once.t](../../tests/loader/builtin/0257_loader_builtin_gif_loop_disable_loop0_once.t) |
| AN-05 | Builtin GIF `auto` interprets a source count of 2 as two total passes. | [tests/loader/builtin/0265_loader_builtin_gif_loop_auto_loop2.t](../../tests/loader/builtin/0265_loader_builtin_gif_loop_auto_loop2.t) |
| AN-06 | Builtin GIF `force` continues beyond a source count of 1 until the test cancels callback delivery. | [tests/loader/builtin/0268_loader_builtin_gif_loop_force_loop1_unbounded.t](../../tests/loader/builtin/0268_loader_builtin_gif_loop_force_loop1_unbounded.t) |
| AN-07 | Builtin and libpng APNG emit the exact two-pass sequence `0:0, 0:1, 1:0, 1:1`. | [tests/loader/builtin/0140_apng_builtin_frame_no_sequence_per_loop.t](../../tests/loader/builtin/0140_apng_builtin_frame_no_sequence_per_loop.t), [tests/loader/libpng/0178_apng_libpng_frame_no_sequence_per_loop.t](../../tests/loader/libpng/0178_apng_libpng_frame_no_sequence_per_loop.t) |
| AN-08 | libpng excludes a non-animation default image from delivered APNG frames. | [tests/loader/libpng/0223_libpng_apng_default_image_excluded_numeric.t](../../tests/loader/libpng/0223_libpng_apng_default_image_excluded_numeric.t) |
| AN-09 | `-T 1` and the corresponding environment setting produce identical bytes for builtin static APNG extraction. | [tests/cli/options/migration/0008_start_frame_environment_cli_equivalence.t](../../tests/cli/options/migration/0008_start_frame_environment_cli_equivalence.t) |
| AN-10 | Macro replay emits exact first-pass definitions/invocations and later invocations; `-S -n7` emits exactly one definition. | [tests/codec/macro/0004_animation_replay.t](../../tests/codec/macro/0004_animation_replay.t), [tests/codec/macro/0006_animation_static_preload.t](../../tests/codec/macro/0006_animation_static_preload.t) |
| AN-11 | CoreGraphics APNG preserves the tested delays/indices and distinguishes one-pass, automatic finite, and forced playback. | [tests/loader/coregraphics/0071_loader_coregraphics_apng_animation_metadata.t](../../tests/loader/coregraphics/0071_loader_coregraphics_apng_animation_metadata.t), [tests/loader/coregraphics/0073_loader_coregraphics_apng_loop_control_behavior.t](../../tests/loader/coregraphics/0073_loader_coregraphics_apng_loop_control_behavior.t) |

### Quality regression tests

The format-specific [GIF](../loader/builtin/gif.md#test-coverage), [APNG](../loader/builtin/png.md#test-coverage), and [WebP](../loader/builtin/webp.md#test-coverage) documents identify composition and image-quality coverage. Still-frame MS-SSIM thresholds do not establish correct repetition, timing, temporal stability, or absence of trails on a terminal.

### Defensive and malformed-input tests

The [builtin loader inventory](../testing/builtin-loader-coverage.md) and [libpng suite](../loader/libpng.md#test-coverage) cover malformed animation chunks, rectangles, sequence numbers, cancellation, and allocation failures. These defensive checks complement the exact callback/stream observations above; they do not demonstrate support for every loader/format combination.

### Coverage boundary and current gaps

The table is a representative cross-layer inventory, not a claim that every interaction has an exact regression test. The [GdkPixbuf suite](../../tests/loader/gdk-pixbuf2), [libwebp suite](../../tests/loader/libwebp), and [WIC suite](../../tests/loader/wic) include smoke and selection checks with narrower assertions. GdkPixbuf finite repetition, iterator termination, and static-start replay; GIF `force` without a Netscape extension; libwebp's one-emitted-frame stopping condition; and truncated-first-pass macro replay remain implementation observations here rather than fully covered compatibility promises. Framework tests require the corresponding compiled backend and host codecs; a skip is not validation. Terminal placement, actual playback cadence, and macro capacity require checking the intended receiver.
