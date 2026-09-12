# Terminal macros: DECDMAC and DECINVM

`img2sixel -u` can store animation frames in terminal macros and replay them by number. `img2sixel -n NUMBER` selects a single macro number and suppresses automatic invocation, so an application can preload an image and display it later. These options require a receiver that supports both SIXEL and terminal macros. Ordinary SIXEL support alone is insufficient.

## What is stored

A terminal macro stores a sequence of terminal input bytes. DECDMAC defines those bytes, and DECINVM asks the terminal to process them. In libsixel, the stored bytes are a complete SIXEL image, including its SIXEL DCS introducer, palette and bitmap commands, and string terminator. Invoking the macro replays that image through the terminal parser; it does not ask the terminal to copy a retained bitmap object. Cursor and palette effects of the replay remain in effect. See the DEC [DECDMAC](https://vt100.net/docs/vt510-rm/DECDMAC.html) and [DECINVM](https://vt100.net/docs/vt510-rm/DECINVM.html) definitions.

This is separate from the legacy SIXEL DCS `P1` parameter, also called a “macro parameter” in descriptions of pixel aspect ratio. `-n` does not select `P1`. It selects the ID of an outer terminal macro that contains a SIXEL stream. See the [SIXEL wire format](../sixel-format.md#dcs-envelope) for the inner format and [project history](../project-history.md#araki-kens-experiments-around-mlterm) for the origin of the libsixel macro path.

## Choosing an option

| Options | Definition | Automatic display |
| --- | --- | --- |
| Neither `-u` nor `-n` | No terminal macro; emit ordinary SIXEL. | The terminal receives the image directly. |
| `-u`, `--use-macro` | Define a static image, or define each animation frame on loop zero, using its frame number as the ID. | Invoke after defining; on later animation loops, invoke the same IDs without redefining them. |
| `-n NUMBER`, `--macro-number=NUMBER` | Define the static image at the selected ID; for animation, redefine that ID for each frame on loop zero. | No DECINVM is emitted. The caller decides when to invoke the macro. |
| `-u -n NUMBER`, in either order | Same explicit-ID behavior as `-n NUMBER`. | `-n` suppresses automatic invocation even when `-u` is also present. |

Automatic frame numbering normally starts at zero. `-n` is not a starting offset for the automatic IDs: it selects one fixed register. Repeating `-n` selects the last supplied number; repeating `-u` leaves macro playback enabled.

The CLI accepts a decimal integer from zero through the build's C `INT_MAX`. Negative values, overflow, and trailing nonnumeric characters are rejected. This is a parser range, not a promise about the terminal's macro capacity. The DEC VT510 definition specifies IDs `0` through `63`; receiver implementations can differ. Automatic playback also needs enough IDs for all frames and does not split an animation into batches to fit a receiver's register limit.

## Play an animation

```sh
# Use the source animation's loop policy.
img2sixel -u animation.gif

# Play one pass, defining and invoking each frame once.
img2sixel -u -ldisable animation.gif

# Repeat until interrupted with Ctrl-C.
img2sixel -u -lforce animation.gif
```

`-u` selects the output mechanism; `-l` selects the loop policy. It does not make the terminal run an autonomous animation. The host continues sending invocations and controlling delays. The macro output path is shared by decoded frames and is not inherently restricted to GIF; whether another format supplies an animation depends on the selected, compiled loader.

For a two-frame animation with IDs zero and one, the relevant output order is:

```text
First pass:  define 0 -> invoke 0 -> define 1 -> invoke 1
Next pass:              invoke 0 ->             invoke 1
```

Cursor positioning and animation timing surround these operations. A macro definition contains the encoded frame; it does not contain the application's complete playback loop. The current macro branch waits after an invocation when the frame has a delay, unless `-g` or `-S` disables that wait. See [threading and animation](../threading/README.md) for the surrounding host pipeline.

## Preload and invoke an image

These commands store a static PNG as macro seven, then invoke it at row two, column five. Choose an ID that the application owns on the receiving terminal.

```sh
# Preload without invoking the image.
img2sixel -n7 image.png

# Position the text cursor, then invoke macro 7.
printf '\033[2;5H\033[7*z'

# Replay the stored image at another position.
printf '\033[12;5H\033[7*z'
```

The cursor coordinates are terminal character cells, starting at one. Actual SIXEL placement and scrolling still depend on the receiver's graphics and cursor modes. DECINVM does not carry independent pixel coordinates, and replay does not save and restore the cursor or palette automatically.

For animated input, use `-S` to take one frame for a fixed ID:

```sh
img2sixel -S -n7 animation.gif
```

A static image is defined regardless of the loader's loop metadata. For an animation, definitions are emitted only on loop zero. Without `-S`, each frame on loop zero redefines the same ID; after the first pass, that ID contains the last frame. `-u -n7` also selects that fixed ID. Subsequent loops do not define additional images, so an indefinitely looping source without `-S` can keep the process running without useful macro output.

Definitions can also be saved as a terminal control stream:

```sh
img2sixel -n7 image.png -o frame7.mac
cat frame7.mac
printf '\033[7*z'
```

Writing the file does not preload the terminal. `cat` sends the definition to the receiver, and the final `printf` invokes it. The `.mac` suffix is only a descriptive filename choice; the content is a DECDMAC control stream, not a new image-file format. A SIXEL image decoder is not a terminal-macro interpreter; use ordinary SIXEL output for `sixel2png` workflows.

## Wire representation

libsixel emits these outer sequences, where `ID` is a decimal number and `HEX` is the hexadecimal encoding of a complete inner SIXEL stream. Spaces in the notation below separate fields and are not transmitted.

```text
Definition:  ESC P ID ; 0 ; 1 ! z HEX ESC \
Invocation:  ESC [ ID * z
```

The definition's `0` replaces only the selected ID, and `1` selects hexadecimal pairs. Each inner byte becomes two lowercase hexadecimal digits; for example, the inner `ESC P` becomes `1b50`. Encoding the inner terminator prevents it from ending the outer definition prematurely. libsixel does not use DECDMAC's optional hex-repeat syntax. These parameter meanings follow the DEC [DECDMAC reference](https://vt100.net/docs/vt510-rm/DECDMAC.html).

`-8` can select C1 bytes for the inner SIXEL DCS and ST, which then appear as hex pairs `90` and `9c`. The DECDMAC wrapper and DECINVM emitted by the current macro path remain 7-bit `ESC P`, `ESC \`, and `ESC [` sequences. Thus `-8` does not make the outer macro protocol use C1 bytes.

## Transfer cost and execution cost

Macro reuse trades a larger first transfer and terminal storage for shorter later transfers. If a frame's complete ordinary SIXEL stream has `S` bytes, its macro definition carries `2S` hex bytes plus definition overhead `H`. Each invocation costs `I` bytes. With a one-digit ID, `H` is 11 bytes and `I` is five bytes for the current emitter.

For `R` displays of the same encoded frame, excluding cursor controls and assuming an unchanged ordinary SIXEL payload:

| Method | Bytes sent |
| --- | --- |
| Ordinary retransmission | `R * S` |
| Define once and invoke on every display | `2 * S + H + R * I` |

Under those assumptions, macros save transfer bytes when `R * (S - I) > 2 * S + H`. A single display increases traffic, and even two displays do not recover the definition overhead. For a sufficiently large image, the third and later displays can make reuse worthwhile. This is a byte-count model, not a measured speed claim; changing frame encodings, palette reuse, delta output, or transport wrapping changes the comparison.

The terminal still processes the stored SIXEL on every invocation. On the host, later loops skip `sixel_encode()` in the macro output branch, but the surrounding loading, frame preparation, palette processing, and allocation work is not all bypassed. Do not interpret the smaller wire stream as a guarantee that either endpoint has no decoding or image-processing cost. The cached first-pass frame also freezes its encoded dithering and palette choices for later replay.

## Receiver state and option interactions

libsixel's macro emitter does not negotiate macro support, check available storage, or acknowledge successful storage before invoking an ID. A zero converter exit status establishes that the local conversion/output path succeeded; it does not establish that the receiver retained the definition. For diagnosis, first try ordinary SIXEL, then a small definition and explicit invocation on the same terminal connection.

The DEC VT510 reference gives 6 KiB of macro storage and says definitions that do not fit are ignored. Its hard reset clears definitions, whereas its soft reset preserves them. Treat those as that terminal's rules, not universal emulator limits or persistence guarantees. Applications must arrange ID ownership and redefine their images after receiver state is lost. libsixel emits no macro cleanup at process exit. See [DECDMAC](https://vt100.net/docs/vt510-rm/DECDMAC.html).

DEC also defines a macro-space query, `CSI ? 62 n`, and a response, `CSI Pn * {`, where `Pn` is free bytes divided by 16 and rounded down. libsixel's macro path does not use it. An application that implements a query must own terminal reply handling rather than leaving replies for an unrelated reader. See “DSR—Macro Space Report,” page 5–166 of the [VT510 programmer manual](https://vt100.net/mirror/mds-199909/cd3/term/vt510rmb.pdf), and the [DECMSR reference](https://vt100.net/docs/vt510-rm/DECMSR.html).

| Option or transport | Interaction |
| --- | --- |
| `-g`, `--ignore-delay` | Suppresses host-side frame delays; it does not store timing in the macro. |
| `-S`, `--static` | Selects a static frame from an animation and suppresses the animation delay path. For example, `-S -n7 animation.gif` preloads just one frame. |
| `-l auto`, `-l disable`, `-l force` | Controls playback looping independently of macro selection. Fixed-ID preloading is normally paired with `-S`. |
| `-n` with multiple frames or conversions | Reuses the same selected ID, replacing earlier definitions when a new definition is emitted. |
| `-8` | Changes the inner SIXEL control-byte representation; the outer macro controls remain 7-bit. |
| `-o PATH` | Writes definitions and any invocations into the destination stream. They take effect only when a terminal receives them. A captured file does not preserve host sleep timing. |
| `-@`, `--drcs` | The encoder rejects DRCS output combined with either macro option. |
| `-P`, `--penetrate`, and terminal multiplexers | The outer definition and invocation are written directly by the macro emitter, outside the inner SIXEL writer's multiplexer wrapping. Do not assume ordinary SIXEL passthrough also forwards DECDMAC/DECINVM correctly. Verify the entire connection. |

## Implementation and verification

[`src/encoder.c`](../../src/encoder.c) owns `sixel_encoder_apply_macro_number_option()`, the hexadecimal writer, macro definition/invocation, and the output dispatch. The public C setopt flags are `SIXEL_OPTFLAG_USE_MACRO` and `SIXEL_OPTFLAG_MACRO_NUMBER` in [`include/sixel.h.in`](../../include/sixel.h.in). These configure the encoder-object path; the low-level SIXEL encoding API does not itself allocate terminal macro IDs. The option spellings are listed in the [`img2sixel` manual](../../converters/img2sixel.1).

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

Each test compares the complete emitted control stream, including the hex-encoded SIXEL envelope, a fixed gray2 palette, pixel commands, definition IDs, cursor controls where applicable, and invocations. The static fixture is a 1-by-1 PNG. The animated fixture contains two distinct 2-by-1 frames and requests two loops; `-g` removes wall-clock waits without changing the expected control order.

| ID | Contract | Owning test |
| --- | --- | --- |
| TM-01 | A static PNG with `-n7` emits exactly one definition and no invocation, including when its loader reports loop one. | [tests/codec/macro/0001_static_png_define.t](../../tests/codec/macro/0001_static_png_define.t) |
| TM-02 | A static PNG with `-u` defines ID zero before invoking it. | [tests/codec/macro/0002_static_png_invoke.t](../../tests/codec/macro/0002_static_png_invoke.t) |
| TM-03 | The first animation pass defines each distinct frame at its own ID and invokes it in frame order. | [tests/codec/macro/0003_animation_first_pass.t](../../tests/codec/macro/0003_animation_first_pass.t) |
| TM-04 | A second animation pass invokes the existing IDs without emitting more definitions. | [tests/codec/macro/0004_animation_replay.t](../../tests/codec/macro/0004_animation_replay.t) |
| TM-05 | Animated fixed-ID mode redefines that ID with each first-pass frame, emits no invocation, and emits no definitions on the second pass. | [tests/codec/macro/0005_animation_fixed_id.t](../../tests/codec/macro/0005_animation_fixed_id.t) |
| TM-06 | `-S -n7` preloads only the first animation frame without playback controls or invocation. | [tests/codec/macro/0006_animation_static_preload.t](../../tests/codec/macro/0006_animation_static_preload.t) |
| TM-07 | `-u -n7` retains definition-only behavior for a static PNG. | [tests/codec/macro/0007_number_after_use.t](../../tests/codec/macro/0007_number_after_use.t) |
| TM-08 | `-n7 -u` also retains definition-only behavior for a static PNG. | [tests/codec/macro/0008_number_before_use.t](../../tests/codec/macro/0008_number_before_use.t) |

### Supplementary coverage and boundaries

The [builtin animation macro smoke test](../../tests/loader/builtin/0107_builtin_disable_update_mode.t) and the [optional libwebp macro smoke test](../../tests/loader/libwebp/0007_webp_use_macro.t) provide additional command-success coverage and discard output. They do not replace the exact stream assertions above.

The exact tests cover the static/animation definition boundary and the listed playback contracts. They do not establish a timing tolerance, other loaders' frame metadata, all numeric validation cases, repeated `-n` precedence, C1 output, DRCS rejection, or file/multiplexer handling. Receiver capacity, macro lifetime, visual placement, and rendering require terminal integration checks; a successful local test does not prove those receiver-dependent properties. No perceptual quality threshold or malformed-input inventory is substituted for these exact stream tests.
