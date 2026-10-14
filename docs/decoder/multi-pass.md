# Multi-Pass Still-Image Decoding

## Status and purpose

The stateful decoder, `lsqa` and `sixel2png` accept an explicitly selected still-image capture containing a baseline SIXEL image followed by zero or more quality-repair images. They share one CPU composition path. This reference defines the accepted capture profile and its tool behavior. A capture may be produced independently of the encoder, including hand-authored streams.

The existing [decoding pipeline](pipeline/README.md) owns individual SIXEL parsing, paint-time color, raster, alpha and output-format semantics. Its [parser implementation](../../src/fromsixel.c) finalizes at the first string terminator. Passing a concatenated progressive stream to that single-image path therefore does not establish the quality or appearance of the final composed image. A final palette plus index plane is also insufficient: different passes may redefine the same register, and the final canvas may contain more than 256 distinct colors.

The multi-pass adapter explicitly frames every complete SIXEL image, executes its paint operations in order on one logical canvas, and exposes the canvas only after successful end-of-input validation. It adds no SIXEL command and cannot recognize the encoder's convergence state from the stream.

## Explicit mode selection

Both tools accept the following argument:

```text
-M MODE, --input-mode=MODE
MODE = single | 6delta-progressive
default = single
```

`single` preserves current behavior. `6delta-progressive` declares that all SIXEL images in the selected input refine one still image at the same position. It accepts a baseline alone as a valid one-pass capture. There is no `auto` value, content-based progressive detection, or Unicode alias. Ordinary animation and progressive repair use the same SIXEL envelopes; multiple DCS sequences are not evidence of one or the other. The caller supplies that interpretation explicitly.

The accepted capture profile uses explicit input-mode selection and contains no in-stream interpretation annotations.

`lsqa` also provides a separate [`-M animation` operation](../quality/video-evaluation.md) for per-frame assessment. The shared decoder modes described here retain their still-image meaning; the animation operation is not a new public decoder option or a `sixel2png` mode.

Both tools resolve this argument through the decoder's shared option schema and the [choice prefix-matching policy](../cli/prefix-matching.md). Unique case-sensitive prefixes are accepted: `-M6` selects `6delta-progressive`, and `-Ms` selects `single`. The long option accepts the same values, such as `--input-mode=6`.

For this initial mode, activation requires the argument; there is no environment-based enablement. This is a scoped exception to the general [CLI environment-default policy](../cli/design-policy.md), because ambient activation could silently collapse an animation into one still image. Explicit library callers select the mode through decoder configuration. Other CLI precedence and diagnostic rules remain unchanged. Repeating the mode argument uses the last valid selection; an unknown, missing or empty value is a usage error before decoding or output.

| Consumer | Scope of the mode | Result |
| --- | --- | --- |
| `sixel2png` | Its one SIXEL input, including stdin. | One PNG containing the final composed canvas. |
| `lsqa` | The target/output input only; the reference retains its existing loading contract. | One assessment of the reference against the final composed target. |

The short spelling is available in both tools and does not change `img2sixel -M` palette export, which belongs to a different command context. Usage is:

```sh
sixel2png --input-mode=6delta-progressive -i image.six -o image.png
lsqa --input-mode=6delta-progressive reference.png image.six
lsqa --input-mode=6delta-progressive -m MS-SSIM reference.png image.six
```

A file containing an animation is not an accepted substitute for the declared still-image capture: the decoder has no source-frame boundary metadata with which to undo a false declaration. Extract a self-contained capture for one source image before assessing it. Animation decoding, source-frame selection and APNG/video output are separate work.

## Accepted capture profile

A capture contains one complete baseline DCS and its complete repair DCS packets for the same source generation and repair rectangle. The decoder accepts either raw packet bytes or a fixed-anchor presentation wrapper. Both forms require fixed dimensions and origin; neither provides general cursor-position inference.

| Capture form | Ordered units |
| --- | --- |
| Raw | One baseline DCS followed by zero or more repair DCS packets. |
| Anchored | One DECSC (`ESC 7`) immediately followed by the baseline DCS, then zero or more DECRC (`ESC 8`) plus repair-DCS pairs. |

Select the form from the first non-whitespace unit after the caller has explicitly enabled multi-pass input mode. This is framing recognition, not automatic recognition of progressive semantics. Save/restore prefixes must be adjacent to their corresponding DCS; permitted whitespace can occur only before, between or after complete units. The anchored form has exactly one save and requires a restore before every subsequent DCS. A second save, unmatched restore, missing restore or trailing prefix without a complete DCS is an error. No `CSI s/u` aliases, absolute cursor moves, scrolls, terminal queries or cursor-visibility controls are accepted.

The decoder maps the saved anchor to its canvas origin and each paired restore to that same origin. It does not execute these commands on the host terminal or infer a physical row/column. All other surrounding controls remain unsupported. An anchored capture can therefore replay in place on a compatible terminal and compose through the offline tools without terminal emulation.

| Property | Required contract |
| --- | --- |
| Framing | Complete 7-bit `ESC P ... q` or 8-bit DCS images, each closed by a supported 7-bit or 8-bit ST. Validate the entire input through EOF. |
| Inter-packet bytes | ASCII space, tab, CR and LF may separate complete units. Only the anchored form's specified save/restore prefixes are accepted; other text, CSI/OSC, cursor movement, scrolling, non-SIXEL DCS and transport wrappers are rejected. |
| Image placement | Every packet paints at local `(0, 0)` on the same capture canvas. A nonzero physical terminal origin is irrelevant to this cropped local image and is not encoded in the file. |
| Raster | Every packet declares the same positive width and height and unit pixel aspect, within the packed-pixel decoder's existing 6000-by-6000 limit. Validate this limit before allocating the canvas. The first declaration fixes the canvas. Do not infer missing dimensions from sparse paint. |
| Paint extent | Actual painted pixels must fit the declared canvas. Reject contradictory paint; do not silently grow or clip it. Cursor skips with no paint do not themselves paint outside the canvas. |
| Preservation | Every image uses explicit `P2=1`. Clear/default-background and OR composition are unsupported in this profile. |
| Color registers | Preserve register definitions across passes, with paint-time color capture. Every register used for painting must have been explicitly defined in this capture before that paint. Do not depend on a terminal's preexisting palette. |
| External display state | None is imported. The baseline starts on a transparent canvas; positions never painted by any pass remain transparent. |
| Input completeness | Empty input, missing baseline, canceled DCS, malformed packet or unfinished final DCS is an error. A valid prefix does not authorize a successful final result. |

The baseline must represent the intended initial image independently of a previous terminal surface. When only a repair delta has been captured, unknown background pixels cannot be recovered from palette state. Do not substitute the reference image as the missing baseline in `lsqa`, because that would conceal omitted or incorrect output. A source-transparent area may legitimately remain transparent; no terminal background is inferred.

Each selected input is one capture. Do not mix baseline and repair packets from different source generations, dimensions, output color interpretations or origins. There is no generation tag on the wire, so this is a producer/caller obligation; geometry and framing checks can detect only some violations. RGB and HLS definitions may coexist using their established decode rules, but the caller supplies one consistent color interpretation for the entire capture.

This mode intentionally does not emulate a terminal. Apart from the validated fixed-anchor wrapper, general session logs containing cursor moves, different-sized damage rectangles, external clears or multiple display surfaces require a terminal compositor and are not accepted by silently ignoring those controls. Terminal queries and initial scrolling used for live display are outside the saved capture profile.

## Framing and composition algorithm

1. Parse and validate the explicit mode and conflicting options before producing output. Begin with no canvas, no defined-register bits and a pass count of zero.
2. Validate raw or anchored unit framing and locate its next complete SIXEL DCS with a bounded parser that recognizes the envelope and termination. Consume a paired save/restore only in its permitted position and require the following DCS to complete. Do not split on every ESC byte or search for the next `q` without parsing the header. Reject cancellation, nesting and unsupported surrounding controls. Check all byte-offset arithmetic.
3. Validate the raster, `P2` and aspect contract. Allocate the transparent RGBA8888 canvas on the first pass and verify identical geometry thereafter. Reuse the existing dimension and allocation limits.
4. Decode the packet through the existing direct paint implementation, preserving register values and their defined bits from preceding passes. Reset packet-local cursor, raster/parser state and paint mask for each DCS. Palette selection starts from register zero for the packet; its value is usable only if defined in this capture before painting.
5. Produce a packet-local direct RGBA view and paint mask, or an equivalent shared direct-painter artifact. Store each pixel's RGB when it is painted, even if that register is redefined later in the same packet. A mask bit, not RGB zero or a final palette index, determines whether the packet writes the pixel.
6. Scatter painted pixels into the persistent canvas. Keep every unpainted pixel's previous RGB and alpha. A painted pixel becomes opaque; the sparse repair does not alpha-blend with, clear or replace unpainted pixels. Preserve the register state for the next packet independently of canvas colors.
7. Increment the checked pass count and continue to EOF. Trailing allowed whitespace is accepted; an incomplete or invalid suffix fails the entire operation. A complete no-paint packet can update registers without changing the canvas.
8. On successful EOF, expose the final direct image and composition metadata to the existing assessment or PNG-output stage. Do not claim that EOF proves convergence: a deliberately short capture is still a valid intermediate-quality result.

In pixel form, with `M_k` denoting a pass's actual paint mask:

```text
canvas_0[p] = transparent
canvas_k[p] = painted_rgba_k[p] if M_k[p] else canvas_(k-1)[p]
```

Register redefinition affects later paint only. For example, redefining register 0 from red to blue while painting a single repair pixel must leave all omitted red pixels red. Even if every pass has at most 256 palette entries, the composed direct image is not restricted to 256 colors. No quantization to a union palette is permitted for assessment or PNG materialization.

The independent paint mask is authoritative even for opaque black, palette index zero, all-transparent passes and partial six-row bands. Packed RGB conversion or background flattening happens after the final composition. Flattening each sparse patch before composition would turn omitted pixels into background paint.

## Shared library boundary

The decoder owns the shared multi-pass path. [`assessment/lsqa.c`](../../assessment/lsqa.c) and [`converters/sixel2png.c`](../../converters/sixel2png.c) route input to that implementation.

Decoder optflag `SIXEL_OPTFLAG_DECODE_INPUT_MODE`, mapped to `M`, accepts exactly `single` and `6delta-progressive`. `sixel_decoder_setopt()` rejects invalid mode values without changing the selected mode. `SIXEL_HAVE_DECODER_6DELTA_PROGRESSIVE` has value 1. Mode is per decoder and defaults to `single`. Encoder optflag meanings and decoder public options/result structure layouts retain their existing contracts.

`sixel_decoder_decode_pixels()` and the stateful file-to-PNG decode path honor that mode through a shared internal sequence decoder. Stateless `sixel_decode_raw()`, `sixel_decode_direct()` and `sixel_decode_pixels()` retain their existing single-image behavior. The body-only API consumes one already-framed body. Sequence composition retains a direct-color canvas and palette state while resetting packet-local parser state for each DCS.

All state used for one capture is call-scoped: canvas, palette/register-defined bits, framing cursor and pass counters start fresh at each decode call. Reusing a configured decoder for a second file does not reuse the previous file as an implicit baseline. Returned pixels retain existing allocator and caller-ownership rules. Keep borrowed parser/GPU worker data alive until each pass finishes.

The shared decoder returns a private pass count used by `lsqa` verbose diagnostics. There is no public pass-statistics accessor. Public pixel-result flags retain their meanings and describe the final canvas, including its alpha and palette-redefinition observations.

## Tool-specific behavior and interactions

### lsqa

The target loader routes `--input-mode=6delta-progressive` to the common decoder rather than to its first-frame loader callback or single-DCS direct shortcut. This dispatch precedes the existing first-two-byte SIXEL probe: an anchored capture begins with DECSC, not DCS. A non-capture target is an input error under this explicit mode; do not fall back to a PNG or animation loader. The reference can remain an ordinary PNG or another currently supported reference format.

Assess exactly once after all packets have composed and input validation has succeeded. Use the existing metric, dimension, background, colorspace and comparison-precision policies. Preserve direct alpha until the existing comparison background policy is applied once. `-R` and `-T` retain their reference/target color meanings; the mode does not imply sRGB when the caller explicitly selects another target interpretation. Palette color interpretation is stable for the entire capture and must not be inferred from the final pass alone.

The mode inherently uses direct paint-time colors for the target. `-D` is redundant for that target and retains its current meaning for a SIXEL reference; omitting `-D` must not send the composed target through a lossy indexed representation. Record the selected target mode and pass count in the appropriate verbose diagnostics. Single-metric output such as `-m MS-SSIM` retains its existing stdout format; composition metadata must not contaminate it.

`lsqa` reports final-canvas quality only. To measure a per-pass curve, create complete-DCS prefixes of the same capture and assess each prefix against the same reference, accounting for its cumulative bytes. An arbitrary byte truncation inside a DCS is invalid, not a lower-quality sample. Neither a good final score nor a low average error proves every accepted encoder paint improved its target; independent encoder acceptance tests remain necessary.

### sixel2png

The stateful output path waits for successful whole-capture composition before invoking the PNG writer. It produces one final image, not one PNG per DCS, APNG or concatenated PNGs. Invalid late packets must not produce an apparently successful PNG of an earlier prefix. Defer opening/truncating the destination until input and composition validation completes, following the existing output-destination rules.

Keep direct RGB/RGBA through PNG writing, using the existing opaque optimization when valid. A palette redefinition or a union of more than 256 colors must not trigger final-palette reinterpretation or re-quantization. `-D` is redundant for composition. Apply an explicitly requested `-s` resize once to the final canvas; resizing individual delta patches would change their masks and alignment. Existing `-T` input-color interpretation and PNG tagging apply to that final canvas and retain their documented supported spaces.

### Shared restrictions

Multi-pass composition requires `dequantize=none`; an effective non-none `-d` policy is rejected before output. Per-pass dequantization would modify the repair masks/values, and palette-based reconstruction after composition cannot assume one source palette. This restriction includes decoder environment defaults that would otherwise enable reconstruction, so reproducible commands may explicitly include `-d none`.

Passes compose serially in stream order on the CPU. The sequence path uses the direct painter and does not dispatch GPU or band-parallel composition. Forced GPU decoding is rejected; `auto` and `off` use the CPU implementation.

The common decoder uses existing size/resource limits and checked arithmetic for dimensions, strides, packet offsets, cumulative bytes and pass counters. It need not retain every decoded pass: one canvas, one current patch and bounded parser/palette state suffice, in addition to any whole-input byte buffer already owned by the frontend. Do not allocate image-sized storage multiplied by pass count. EOF on stdin completes the capture; no timeout or progressive-quality heuristic substitutes for EOF.

## Test coverage

<!-- test-coverage: enforced -->

### Implemented decoder witnesses

The following focused tests exercise stream composition, capture validation and option behavior. Tool integration and a deliberately incorrect baseline control are covered separately below.

- [tests/processing/decoder/0054_multi_pass_raw_overlay.t](../../tests/processing/decoder/0054_multi_pass_raw_overlay.t): raw overlay.
- [tests/processing/decoder/0055_multi_pass_anchored_overlay.t](../../tests/processing/decoder/0055_multi_pass_anchored_overlay.t): anchored overlay.
- [tests/processing/decoder/0056_multi_pass_palette_carry.t](../../tests/processing/decoder/0056_multi_pass_palette_carry.t): palette carry.
- [tests/processing/decoder/0057_multi_pass_paint_time_colors.t](../../tests/processing/decoder/0057_multi_pass_paint_time_colors.t): paint time colors.
- [tests/processing/decoder/0058_multi_pass_transparent_and_black.t](../../tests/processing/decoder/0058_multi_pass_transparent_and_black.t): transparent and black.
- [tests/processing/decoder/0059_multi_pass_mixed_framing.t](../../tests/processing/decoder/0059_multi_pass_mixed_framing.t): mixed framing.
- [tests/processing/decoder/0060_multi_pass_transparent_baseline.t](../../tests/processing/decoder/0060_multi_pass_transparent_baseline.t): transparent baseline.
- [tests/processing/decoder/0061_multi_pass_single_default.t](../../tests/processing/decoder/0061_multi_pass_single_default.t): single default.
- [tests/processing/decoder/0062_multi_pass_missing_raster.t](../../tests/processing/decoder/0062_multi_pass_missing_raster.t): missing raster.
- [tests/processing/decoder/0063_multi_pass_mismatched_raster.t](../../tests/processing/decoder/0063_multi_pass_mismatched_raster.t): mismatched raster.
- [tests/processing/decoder/0064_multi_pass_nonunit_aspect.t](../../tests/processing/decoder/0064_multi_pass_nonunit_aspect.t): nonunit aspect.
- [tests/processing/decoder/0065_multi_pass_paint_width.t](../../tests/processing/decoder/0065_multi_pass_paint_width.t): paint width.
- [tests/processing/decoder/0066_multi_pass_paint_height.t](../../tests/processing/decoder/0066_multi_pass_paint_height.t): paint height.
- [tests/processing/decoder/0067_multi_pass_clear_p2.t](../../tests/processing/decoder/0067_multi_pass_clear_p2.t): clear p2.
- [tests/processing/decoder/0068_multi_pass_undefined_color.t](../../tests/processing/decoder/0068_multi_pass_undefined_color.t): undefined color.
- [tests/processing/decoder/0069_multi_pass_late_truncation.t](../../tests/processing/decoder/0069_multi_pass_late_truncation.t): late truncation.
- [tests/processing/decoder/0070_multi_pass_trailing_text.t](../../tests/processing/decoder/0070_multi_pass_trailing_text.t): trailing text.
- [tests/processing/decoder/0071_multi_pass_repeated_save.t](../../tests/processing/decoder/0071_multi_pass_repeated_save.t): repeated save.
- [tests/processing/decoder/0072_multi_pass_unmatched_restore.t](../../tests/processing/decoder/0072_multi_pass_unmatched_restore.t): unmatched restore.
- [tests/processing/decoder/0073_multi_pass_missing_restore.t](../../tests/processing/decoder/0073_multi_pass_missing_restore.t): missing restore.
- [tests/processing/decoder/0074_multi_pass_prefix_whitespace.t](../../tests/processing/decoder/0074_multi_pass_prefix_whitespace.t): prefix whitespace.
- [tests/processing/decoder/0075_multi_pass_dangling_prefix.t](../../tests/processing/decoder/0075_multi_pass_dangling_prefix.t): dangling prefix.
- [tests/processing/decoder/0076_multi_pass_terminal_control.t](../../tests/processing/decoder/0076_multi_pass_terminal_control.t): terminal control.
- [tests/processing/decoder/0077_multi_pass_cancelled_dcs.t](../../tests/processing/decoder/0077_multi_pass_cancelled_dcs.t): cancelled dcs.
- [tests/processing/decoder/0078_multi_pass_empty_capture.t](../../tests/processing/decoder/0078_multi_pass_empty_capture.t): empty capture.
- [tests/processing/decoder/0079_multi_pass_dequant_conflict.t](../../tests/processing/decoder/0079_multi_pass_dequant_conflict.t): dequant conflict.
- [tests/processing/decoder/0080_multi_pass_force_gpu.t](../../tests/processing/decoder/0080_multi_pass_force_gpu.t): force gpu.
- [tests/processing/decoder/0081_multi_pass_skip_outside.t](../../tests/processing/decoder/0081_multi_pass_skip_outside.t): skip outside.
- [tests/processing/decoder/0082_multi_pass_hls_color.t](../../tests/processing/decoder/0082_multi_pass_hls_color.t): hls color.
- [tests/processing/decoder/0083_multi_pass_call_isolation.t](../../tests/processing/decoder/0083_multi_pass_call_isolation.t): call isolation.
- [tests/processing/decoder/0084_multi_pass_color_union.t](../../tests/processing/decoder/0084_multi_pass_color_union.t): color union.
- [tests/processing/decoder/0085_multi_pass_option_atomicity.t](../../tests/processing/decoder/0085_multi_pass_option_atomicity.t): option atomicity.
- [tests/processing/decoder/0086_multi_pass_allocation_failure.t](../../tests/processing/decoder/0086_multi_pass_allocation_failure.t): allocation failure.

The `progressive-snake-192.six` quality fixture starts with a deliberately black baseline and ends with an ordinary quantized version of `tests/data/inputs/snake_192.png`, wrapped at one anchor with explicit `P2=1`. Its 192 by 192 canvas supports all five MS-SSIM scales; the companion baseline-only capture is a negative quality control. Regenerate both captures with `python3 tests/data/inputs/generate_progressive_quality_fixture.py` after building `img2sixel`. The independent pre-encoding PNG remains the quality reference. These fixtures test composition and tool routing independently of a progressive encoder. The 16 by 16 capture remains available for dimension and protocol-value tests.

- [tests/cli/sixel2png/0014_multi_pass_png.t](../../tests/cli/sixel2png/0014_multi_pass_png.t): Verify the final PNG includes the correcting pass.
- [tests/cli/sixel2png/0015_multi_pass_lsqa.t](../../tests/cli/sixel2png/0015_multi_pass_lsqa.t): Verify explicit multi-pass target assessment and numeric output.
- [tests/cli/sixel2png/0016_multi_pass_baseline_control.t](../../tests/cli/sixel2png/0016_multi_pass_baseline_control.t): Verify the deliberately wrong baseline fails the quality gate.
- [tests/cli/sixel2png/0017_multi_pass_late_error.t](../../tests/cli/sixel2png/0017_multi_pass_late_error.t): Verify malformed final input produces no prefix PNG.
- [tests/cli/sixel2png/0018_multi_pass_resize.t](../../tests/cli/sixel2png/0018_multi_pass_resize.t): Verify resize applies to the final composed image.
- [tests/cli/sixel2png/0019_multi_pass_non_capture.t](../../tests/cli/sixel2png/0019_multi_pass_non_capture.t): Verify explicit target mode rejects PNG without loader fallback.
- [tests/cli/sixel2png/0020_multi_pass_png_prefix.t](../../tests/cli/sixel2png/0020_multi_pass_png_prefix.t): resolve unique prefixes through the shared decoder schema and verify final-canvas quality.
- [tests/cli/sixel2png/0021_multi_pass_lsqa_prefix.t](../../tests/cli/sixel2png/0021_multi_pass_lsqa_prefix.t): resolve unique prefixes through the shared decoder schema and verify final-canvas quality.
- [tests/cli/sixel2png/0022_sixel2png_input_mode_invalid.t](../../tests/cli/sixel2png/0022_sixel2png_input_mode_invalid.t): reject empty, case-mismatched and overlong input modes.
- [tests/cli/sixel2png/0023_lsqa_input_mode_invalid.t](../../tests/cli/sixel2png/0023_lsqa_input_mode_invalid.t): reject empty, case-mismatched and overlong input modes.
