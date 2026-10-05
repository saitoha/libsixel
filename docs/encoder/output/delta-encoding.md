# Delta Encoding (6delta)

6delta reduces repeated SIXEL painting by remembering the colors previously sent to a persistent display surface. For each new pixel, the encoder can either paint a palette color or leave the existing pixel untouched. An untouched pixel is represented through ordinary SIXEL transparency with `P2=1`; the receiver does not need a separate 6delta command or decoder.

This is useful for animation and applications that repeatedly repaint a surface, including remote desktops. It requires the encoder's retained image and the receiver's actual image to remain synchronized. A delta frame by itself is an incomplete image: decoding it on a blank canvas cannot reconstruct the pixels it deliberately omits.

See the [SIXEL format](../../sixel-format/README.md) for the wire syntax, [alpha policy](../../loader/alpha-policy/README.md) for `P2=1`, and the [encoding pipeline](../pipeline/README.md) for palette construction and application.

## Quick start

Select `--update-policy=delta` (short form `-Z delta`) to enable 6delta. The default policy is `full`. The delta policy defaults to `threshold=0:error=diffuse`; zero controls the exact early-match gate, while the policy itself controls enablement.

```sh
img2sixel --update-policy=delta:threshold=0:error=diffuse \
  --precision=8bit --working-colorspace=gamma --gpu-policy=off \
  --loop-control=disable animation.gif
```

Use an animated input supported by the selected loader and a SIXEL terminal that preserves unpainted pixels across frames at the same display position. `--loop-control=disable` plays the input once. The explicit precision, working-space, and GPU settings make this example use the CPU RGB888 path documented here. They are not additional enable flags for 6delta.

The first frame establishes history. Subsequent frames in the same encoder instance can reuse it. A fresh `img2sixel` process starts with no history, so invoking the command separately for each still image does not share delta state. A still image encoded once has no previous frame to reuse unless an application supplies an initial retained plane through the C API.

| Control | Meaning | Default |
| --- | --- | --- |
| `-Z POLICY`, `--update-policy=POLICY` | Select `full` or `delta[:threshold=N][:error=MODE]`. | `full`. |
| `delta:threshold=N` (short key `T`) | Set the early-keep tolerance, an integer from 0 through 255. | `0`. |
| `delta:error=MODE` (short key `E`) | Choose `diffuse` or `skip` for kept-pixel error handling. | `diffuse`. |
| `SIXEL_UPDATE_POLICY` | Select the initial base policy, `full` or `delta`, without suboptions. | Invalid values leave `full`. |
| `SIXEL_UPDATE_DELTA_THRESHOLD` | Set the delta threshold default without enabling delta. | Invalid values leave `0`. |
| `SIXEL_UPDATE_DELTA_ERROR` | Set the delta error-mode default without enabling delta. | Invalid values leave `diffuse`. |
| `--alpha-policy=auto` | Resolve to `keep` for delta, requesting `P2=1`. | `auto`. |

An explicit policy overrides `SIXEL_UPDATE_POLICY`. Within delta, explicit suboptions override their environment defaults. Each policy selection resets its parameters to built-in defaults, then applies applicable environment defaults and explicit suboptions. Thus `-Z delta:T8:Eskip` followed by `-Z delta` restores environment/default tuning. Parameters remain on the encoder until the next selection.

`full` accepts no suboptions, disables temporal keeps, and invalidates retained history. It preserves source-alpha handling: source-transparent pixels can still be omitted under `alpha-policy=keep`. Selecting delta again starts with no retained history. Malformed arguments are rejected before changing the policy, parameters, or retained plane. Clearing history without selecting full only forces repainting until history is built again.

The canonical error-mode names are `diffuse` and `skip`; `carry` is rejected. Explicit `--alpha-policy=clear` or `--alpha-policy=composite` conflicts with 6delta and is rejected. A process-level alpha default does not prevent the feature from resolving to `keep`; see [alpha-policy precedence](../../loader/alpha-policy/README.md).

## What is retained

The encoder maintains an RGB888 plane plus a validity mask. This plane models the palette colors painted by earlier frames, rather than their original, unquantized source RGB or their old palette indices. Palette indices can change meaning between frames, and the source may have a color that quantization never displayed.

For an encoded frame, the state advances as follows:

| Pixel outcome | SIXEL action | Retained-plane update |
| --- | --- | --- |
| A palette color is painted | Emit the selected color's SIXEL bits. | Store the encoder's emitted palette RGB and mark this position valid. |
| 6delta chooses keep | Leave this position unpainted. | Preserve its previous RGB and validity. |
| Source transparency leaves the pixel unpainted | Preserve the destination under `P2=1`. | Preserve its previous RGB and validity. |
| The pixel is outside this frame's rectangle | No update. | Preserve its previous RGB and validity. |
| There is no valid previous pixel | Do not perform a delta keep. | Establish history when an opaque pixel is painted. |

An initially invalid pixel is not assumed to be black merely because the backing allocation is zero-filled. Similarly, transparent input cannot establish knowledge of a destination color the encoder has never painted. A full repaint after invalidation means “no history-based keeps”; source transparency still applies.

The retained plane is an encoder-side model, not a readback from the terminal. Palette serialization, terminal color interpretation, placement, and external repainting can affect what the receiver actually shows. The library cannot detect those differences automatically.

## How a pixel is selected

### Early keep: a per-channel shortcut

Let `S` be the RGB888 sample used for this pixel's keep comparison, `R` its valid retained RGB, and `T` the threshold. The early gate keeps the pixel when all three conditions hold:

```text
abs(S.red   - R.red)   <= T
abs(S.green - R.green) <= T
abs(S.blue  - R.blue)  <= T
```

This is a byte-component tolerance, not a percentage, a Euclidean distance, or a perceptual Delta E. At `T=0`, it succeeds for an exact RGB match. If it succeeds, the palette lookup can be skipped. If it fails, the pixel still has another opportunity to be kept after lookup.

A positive threshold admits approximate matches before checking whether the palette could represent the sample more accurately. At `T=255`, every valid retained RGB888 pixel satisfies the gate, even after a large content change. This can leave old content indefinitely; it is not merely a more aggressive lossless compression level. Newly invalid positions and source transparency follow their own rules.

### Post-lookup keep: compare with the selected palette color

When the early gate fails, lookup selects a palette candidate `P`. The CPU RGB888 keep helper compares squared RGB distances:

```text
distance(A, B) = (A.red   - B.red)^2
               + (A.green - B.green)^2
               + (A.blue  - B.blue)^2

keep if distance(S, R) <= distance(S, P)
```

Ties select keep. The retained color is an extra candidate specific to this display position; it need not belong to the current frame's palette. The comparison is against the entry that the configured lookup actually selected, not a separate exhaustive search over all palette entries. See [lookup policy](../palette-application/lookup-policy/README.md) for that selection boundary.

For example, consider gray samples, written as one value repeated in all three RGB components:

| Sample `S` | Retained `R` | Selected `P` | Early keep at `T=0` | Post-lookup result |
| ---: | ---: | ---: | --- | --- |
| 130 | 130 | Not needed | Yes | Lookup is skipped. |
| 130 | 140 | 160 | No | Keep: `3 × 10²` is less than `3 × 30²`. |
| 130 | 120 | 140 | No | Keep: equal distances. |
| 130 | 100 | 128 | No | Paint 128: it is closer. |

In the last row, `T=30` would keep 100 before discovering the better palette color 128. This illustrates the extra accuracy tradeoff introduced by a positive threshold.

At zero, a post-lookup keep is no farther from this sample under the helper's RGB metric than the selected palette candidate. That local statement is not a guarantee of identical output, global perceptual quality, or a smaller complete SIXEL stream. Palette reservation, diffusion, future frames, and serialization can all change the result.

### Dither samples and kept-pixel error

Error-diffusion policies make their comparison using the current error-corrected sample. Positional policies such as A-dither, X-dither, and blue noise use the source sample for the keep comparison while their positional perturbation affects the palette choice. The selected candidate and the meaning of the sample therefore depend on the policy; the threshold is not universally a comparison against untouched input-file bytes.

With `--update-policy=delta:error=diffuse`, a kept pixel's quantization error is calculated against retained RGB and propagated through the chosen diffusion kernel. The target is the color left on screen, not the transparency marker's RGB. With `skip`, propagation from a kept pixel is omitted; ordinary painted pixels still use the selected diffusion policy. Skipping can change following pixels and temporal appearance as well as execution time. Positional dithers and `none` have no spatial error diffusion for this switch to suppress.

## What goes on the wire

6delta uses ordinary palette definitions, six-row SIXEL masks, and a `P2=1` DCS request. The internal transparency key is a marker for omitted pixels; there is no instruction meaning “paint with transparent RGB,” no motion vector, and no rectangle-copy command. Copying or scrolling a display region is an application or terminal operation outside this encoder feature.

As a small wire example, suppose a 3-column by 6-row red rectangle has already been painted. A second frame changes only column 1, row 2 to blue, using zero-based coordinates. The escaped streams below are hand-written illustrations of valid drawing operations, not canonical bytes promised by the optimizer:

```text
Initial red frame:
ESC P 0;1 q "1;1;3;6 #0;2;100;0;0 !3~ ESC \

One blue pixel, preserving the other 17 pixels:
ESC P 0;1 q "1;1;3;6 #1;2;0;0;100 ?C ESC \
```

Spaces separate explanatory tokens and are not transmitted. Both DCS images must be placed at the same display origin. `~` paints all six vertical positions, `!3` repeats it across three columns, `?` advances one column without painting, and `C` paints the third vertical position because its SIXEL value is `67 - 63 = 4`. The second frame does not resend red pixels or define their color again.

A standalone decode of that second frame yields one painted blue pixel and unpainted positions. Reconstruct the display by applying its paint mask to the preceding canvas. Reusing decoder palette registers alone does not reconstruct the retained display surface. The [decoding pipeline](../../decoder/pipeline/README.md) separates palette state, paint masks, and image representations.

Omitting pixels can reduce drawing work, but palette definitions, framing, horizontal skips, and band changes still cost bytes. Scattered keeps can also break repeated patterns that ordinary SIXEL compresses efficiently. The percentage of kept pixels is therefore not a compression ratio.

## C API and partial updates

The public declarations are in [sixel.h.in](../../../include/sixel.h.in). The feature macros `SIXEL_HAVE_ENCODER_6DELTA_PLANE` and `SIXEL_HAVE_ENCODER_ENCODE_FRAME` allow consumers to test header capabilities.

| API | Purpose and lifetime |
| --- | --- |
| `sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY, "delta:threshold=0:error=diffuse")` | Select delta and its parameters. |
| `sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY, "full")` | Disable temporal keeps and discard history. |
| `sixel_encoder_set_6delta_plane_size(encoder, width, height)` | Declare the persistent surface in output pixels. Changing the size discards history; repeating the same size preserves it. `(0, 0)` restores frame-sized history. |
| `sixel_encoder_set_6delta_plane_origin(encoder, x, y)` | Locate the next frame inside the declared plane. The nonnegative pixel origin persists until changed. |
| `sixel_encoder_set_accumulation_buffer(encoder, pixels, width, height, pixelformat)` | Copy a supplied previous image into RGB888 history, mark all positions valid, and declare its dimensions as the plane size. Passing `NULL` clears the history. |
| `sixel_encoder_invalidate_6delta_plane(encoder)` | Discard history while preserving the declared plane size and origin. |
| `sixel_encoder_encode_frame(encoder, frame, output)` | Encode an initialized frame and advance the retained state through the high-level encoder. |

### Surface coordinates

By default, history has the current frame's dimensions and origin `(0, 0)`. It has no knowledge that two same-sized input rectangles might represent different screen positions. A dimension change makes old history incompatible; a same-sized rectangle moved by the application must not reuse frame-local history as though it were still at the old location.

Applications sending damage rectangles should declare the full persistent surface once and set each frame's origin. A frame-local coordinate `(x, y)` then addresses retained position `(origin_x + x, origin_y + y)`. Unvisited parts of the declared surface remain invalid until painted, while overlapping updates can keep known pixels.

For example, on a 48 × 24 plane, a 24 × 12 frame at `(0, 0)` followed by another at `(12, 6)` overlaps by 12 × 6 pixels. Only that overlap has history from the first frame; the other positions in the new rectangle still need painting. The retained-plane regression exercises this moving-rectangle case.

The plane origin selects history coordinates. It does **not** move the terminal cursor, add SIXEL padding, or choose a terminal drawing location. The application must place output at the matching destination. `--transparent-offset` is a separate wire-layout feature that adds left/top padding; it is not an alias for the plane-origin API. Coordinates and extents must describe the pixels actually encoded after preprocessing.

If a frame does not fit at its requested origin, it is encoded without delta keeps and the retained plane is discarded. Use `(0, 0)` for both extents to restore the default geometry; a single zero extent or a negative extent is rejected. A negative origin is rejected. The size and origin setters do not themselves enable 6delta.

### Integration sequence

Keep one encoder per independently retained surface. The following helper assumes that the caller has already created the encoder, configured its RGB888 processing path, and owns initialized frames and an output context. It enables the feature and declares the surface with checked return values:

```c
static SIXELSTATUS
prepare_delta_surface(sixel_encoder_t *encoder, int width, int height)
{
    SIXELSTATUS status;

    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_ALPHA_POLICY, "keep");
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_UPDATE_POLICY,
                                  "delta:threshold=0:error=diffuse");
    if (SIXEL_FAILED(status)) {
        return status;
    }
    return sixel_encoder_set_6delta_plane_size(encoder, width, height);
}

static SIXELSTATUS
encode_delta_rectangle(sixel_encoder_t *encoder, sixel_frame_t *frame,
                       sixel_output_t *output, int x, int y)
{
    SIXELSTATUS status;

    status = sixel_encoder_set_6delta_plane_origin(encoder, x, y);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    /* The caller places this output at the matching terminal position. */
    return sixel_encoder_encode_frame(encoder, frame, output);
}
```

Call preparation once for a new surface, then encode each rectangle in delivery order. Establish an opaque initial repaint when the whole surface must be known. Use normal frame/output ownership rules and release the encoder when finished. The low-level `sixel_encode` function by itself does not own this high-level retained-plane lifecycle.

If seeding history, provide the colors already displayed, not the original pre-quantized source. The copied buffer is treated as entirely known; it is not an API for importing a sparse validity mask. Do not call it with every incoming source frame as a shortcut for updating history.

### Invalidation and delivery

Invalidate whenever the encoder's history no longer describes the destination: terminal clear/reset, external scrolling, resize, overlays, reconnect, a dropped delta frame, or a failed/partial delivery. Then repaint the affected surface before relying on old pixels again. The available invalidation call discards the whole retained plane, even when the external damage was local.

A successful encode does not acknowledge terminal presentation. If an application queues output and later drops it, the encoder may already have advanced its history. Serialize encoding and delivery for each surface, or invalidate and send a repaint when the queue is abandoned. This also matters when replaying saved streams: begin from their required base image and preserve order and placement.

## Execution paths and interactions

| Path or feature | Current scope |
| --- | --- |
| CPU RGB888 | Early and post-lookup keep helpers are used by `none`, `fs`, `atkinson`, `lso2`, `jajuni`, `stucki`, `burkes`, `sierra1`, `sierra2`, `sierra3`, `a_dither`, `x_dither`, and `bluenoise`. `auto` resolves to a concrete dither policy. |
| Native float32 generated and fixed palettes | The selected CPU native path supports `-W gamma -U gamma -t auto\|rgb`, at least two requested colors for generated palettes, ordinary SIXEL output, and `none`, the nine spatial diffusion policies or the three positional methods listed above (`auto` resolves as usual). Working samples and centroids retain negative, over-one and sub-byte components. Retained colors compete in gamma RGB; see the comparison rule below. Genuine byte-eligible requests remain byte paths. |
| Other float32 combinations | Non-gamma working spaces, P3/linear/SMPTE-C output, HLS, monochrome `-e`, PNG snapshots and alternate output transports have not migrated to native delta. Selecting delta retains their earlier dispatch; explicit P3 rejects before output. RGB888 promoted for soft/none diffusion keeps its existing gamma-RGB history behavior. This is not a claim of equivalent delta behavior on every legacy float path. |
| GPU palette application | The Metal path supports RGB888 `none` and `bluenoise` with a compatible retained plane matching the frame geometry. It implements both threshold and post-lookup decisions. CPU error-diffusion kernels are a separate path. |
| GPU with a larger or offset retained plane | The GPU request cannot express general origin-based plane addressing. `auto` declines that request so the CPU can apply the proper plane coordinates. Forced GPU use is not a portable fallback setting; prefer `off` or `auto` for damage rectangles. |
| Generated palette | An active retained plane requires a reserved transparency key. In the ordinary generated-palette path, one slot is taken from the requested budget; for `-p 256`, up to 255 entries remain for painted colors when that reservation is required. The key is excluded from ordinary nearest-color lookup. |
| Fixed palette and inter-frame palette reuse | Keeping display pixels and reusing palette construction are different operations. A stable palette does not establish a valid retained plane. Native sRGB/gamma `-b` and `-m` preserve all supplied entries, including 256-color tables, and omit by a separate mask as described below. Byte routes retain their earlier behavior; they do not gain native fixed-palette keeps solely by enabling delta. |
| High-color and OR output | These have separate dispatch and wire semantics. In particular, OR output uses `P2=5`, which is not the `P2=1` preservation contract. This guide does not claim `-Z -I` or `-Z -O` as supported combinations; use ordinary indexed output for the workflow above. |
| Missing emitted RGB result | If the encode path cannot supply trustworthy emitted colors, the high-level update discards history instead of seeding it from source RGB. |

### Native float comparison and retained precision

The retained surface remains RGB888 in emitted sRGB coordinates; this migration does not add a per-pixel float history or a public API. Each RGB101 component is represented in history as `(code * 255 + 50) / 100` using integer division. Native keep decisions compare that retained value with the same projection of the selected entry's cached RGB101 code, rather than with its unrounded working centroid. Otherwise, even an identical frame can repaint because its original centroid is closer to the input than the already displayed, rounded color. Ties keep the displayed pixel. This comparison is a decision about sending another color, not palette snapping: lookup and residuals for newly painted pixels still use the authoritative working float palette.

The corrected float sample is never clipped or rounded for the keep test. The per-channel threshold is normalized by 255, and squared distances are accumulated in double. The current native implementation performs lookup before this decision; it does not promise the byte path's early-lookup avoidance. A kept pixel supplies the retained RGB888 value, normalized to float, for `diffuse`; `skip` suppresses that residual. Source masks still take precedence. Plane origins, invalid holes and explicit invalidation follow the same lifecycle as byte encoding. The byte retained surface is an approximation of the receiver's RGB101 colors, not a measurement of its physical output; both competing wire colors use the same approximation.

For `a_dither`, `x_dither` and `bluenoise`, the noisy query selects the palette entry, but the keep threshold and distance comparison use the original, unperturbed float sample. This matches the byte positional contract: noise is a palette-selection technique, not a change to the desired color. There is no residual to propagate, so `diffuse` and `skip` have no neighboring-error effect for these methods. Absolute image coordinates determine the noise and retained-plane address; warmup rows do not write results or keep-mask entries. Source-mask omissions do not count as retained-color keeps.

Positional keep tests use literal red endpoints 0 and .2, retained as bytes 0 and 51, and source red .09 or .11. At `(0,0)`, A/X shift red by `-32/255`; the fixed blue-noise red tap sum is `52/255` at strength 1 with RGB channel offsets and zero phase. The query and original source land on opposite sides of the .1 distance boundary. A `2e-6` absolute query tolerance covers float stores and bias arithmetic, while the original-source decision has a .01 margin. Positive-threshold controls use .02/.18 sources and threshold `50/255`, with more than .016 margin. Palette indices and keep masks therefore have exact independent expectations; these tests do not derive their expectations from the production keep or noise helpers.

The connected path adds no persistent image-sized buffer beyond the existing RGB888 surface and validity mask. Broader working/output spaces require a defined transform of retained display values into the comparison space; HLS requires capturing its own emitted rounding. These remain separate migration steps.

### Fixed-table omission and ownership

Native sRGB/gamma `-b` and `-m` use a separate omission mask, preserving palette order, duplicates, unused entries and the complete 256-entry capacity. Index zero is an internal placeholder for an omitted sample; it remains an ordinary lookup candidate and is painted wherever the mask is clear. The table has no reserved keycolor. Source-alpha holes and delta keeps are combined in the existing dither-owned delta result mask. Palette-free `-e` remains outside this native delta scope because its actual foreground color is receiver-dependent.

The output layer prepares the mask before parallel workers copy their output views. Palette application reinitializes the same allocation for the same geometry, then publishes each row after writing both its indices and mask bits. Warmup rows do not publish keep bits. The pointer remains owned by the dither and the output borrow is cleared after success or failure. This connects fixed tables to the existing one-byte-per-pixel delta result mask; it does not introduce another dedicated omission plane. Source-mask preparation remains distinct from output consumption.

Image mapfiles construct their palette independently of the display history: an active retained plane must not reserve a color while quantizing the map image. Application caches may reuse a fixed source, while working/output/precision changes rebuild the appropriate view. Invalidation, full/delta transitions, origins and source masks follow the same retained-plane lifecycle as generated palettes. A genuinely byte-eligible job retains its existing route; use an explicit float precision request when the native fixed-table delta contract is required.

Fixed-table tests compare all working gray fractions against the independent sRGB8 formula `entry/255` with absolute tolerance `2e-7`; the RGB101 code `(entry*100+127)/255` has at least `1/510` code unit of rounding margin. Emitted RGB, the preservation header (`P2=1`) and paint masks have exact expectations, including simultaneously omitted and painted pixels with placeholder/index zero. A gray2-to-gray1 FS control retains emitted 67% gray (byte 171): the residual from desired .6 makes the following .51 sample black in `diffuse`, while `skip` leaves it white. Its decision margin exceeds .01, far above float arithmetic uncertainty.

The support table describes the current implementation, rather than a guarantee that every selectable combination has end-to-end coverage. Source transparency takes precedence over history-based color selection. Terminal preservation, color-register behavior, and cursor placement still require verification on the intended receiver.

## Quality and performance evaluation

Start with threshold zero and `diffuse`, then compare a fixed sequence of frames at the same geometry, palette settings, dither policy, precision, GPU policy, and thread budget. Measure both the first repaint and steady-state updates. A positive threshold may avoid more lookups; `skip` may avoid more diffusion work. Neither promises lower total latency or acceptable visual error.

Useful observations are complementary:

- Reconstruct the persistent canvas after every frame, using emitted paint masks and the correct origins. Compare this canvas with the intended source and with a non-delta baseline. Comparing isolated transparent delta PNGs with opaque input images measures the wrong object.
- Measure complete stream bytes, including palette definitions and framing. Also record kept-pixel counts, but do not use them as a substitute for bytes.
- Separate palette construction, lookup/dither, SIXEL serialization, transport, and receiver rendering. The retained plane and validity checks add work even when few pixels can be kept; palette construction is not automatically removed.
- Include unchanged frames, moving damage rectangles, changing text, flat light/dark areas, gradients, abrupt scene changes, and recovery after invalidation. Small residual errors in nearly uniform areas and stale content after scene changes deserve particular attention with positive thresholds.

For a plane of `W × H` pixels, retained RGB and one byte of validity per pixel account for approximately `4 × W × H` bytes. Per-frame masks, emitted-RGB capture, source pixels, palette state, and allocator overhead are additional. This is not the total encoder memory budget.

No universal speedup or quality threshold is claimed here. Per-pixel RGB selection, whole-canvas perceptual error, temporal artifacts, and transmitted size are different measurements; follow the [quality measurement policy](../../quality/measurement-policy.md) when publishing comparisons.

## Implementation references

- [High-level encoder](../../../src/encoder.c): option/default handling, `sixel_encoder_bind_transparent_mask`, geometry resolution, palette-key reservation, public plane APIs, and `sixel_encoder_update_accumulation_from_frame`.
- [Dither state and keep helpers](../../../src/dither.c): `sixel_dither_rgb888_delta_within`, origin/validity checks, `sixel_dither_pipeline_6delta_try_keep_rgb888`, `sixel_dither_pipeline_6delta_try_keep_after_lookup`, result masks, and RGB capture.
- [FS](../../../src/dither-policy-fs.c), [LSO2](../../../src/dither-policy-lso2.c), and [blue noise](../../../src/dither-policy-bluenoise.c): sample selection and policy-specific handling of kept pixels.
- [Indexed encoder core](../../../src/encoder-core-encode.c): emitted palette/index capture and SIXEL serialization.
- [GPU request checks](../../../src/gpu-palette.c) and [Metal implementation](../../../src/gpu-palette-metal.m): retained-plane capability and per-pixel GPU decisions.
- [Option registry](../../../src/options-registry.c), [CLI help](../../../converters/img2sixel.c), and [manual](../../../converters/img2sixel.1): accepted controls and environment defaults.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The IDs below map to existing assertions. A representative fixture establishes its stated observation, not exhaustive coverage of every RGB value, threshold, backend, or terminal.

| ID | Contract and assertion boundary | Owning test |
| --- | --- | --- |
| D6-01 | No history keeps on the first opaque frame; repeats keep pixels; a moving rectangle keeps some overlap without keeping the entire new rectangle; invalidation and a full/delta transition force no history keeps; rejected full suboptions preserve state. | [tests/processing/filter/0012_filter_encode_6delta_plane.t](../../../tests/processing/filter/0012_filter_encode_6delta_plane.t) |
| D6-02 | Repeated content can produce keeps at threshold zero. | [tests/processing/filter/0013_filter_encode_6delta_keycolor.t](../../../tests/processing/filter/0013_filter_encode_6delta_keycolor.t) |
| D6-03 | CLI threshold 8 is accepted; 256, -1, and nonnumeric input are rejected; `skip` is accepted. | [tests/quant/palette/usage/0168_update_delta_threshold_cli_range.t](../../../tests/quant/palette/usage/0168_update_delta_threshold_cli_range.t) |
| D6-04 | `diffuse` and `skip` are accepted; `carry` is rejected. | [tests/cli/options/matching/0149_option_matching_update_delta_error_choice.t](../../../tests/cli/options/matching/0149_option_matching_update_delta_error_choice.t) |
| D6-05 | Threshold 32 supplied through the environment produces the same animation bytes as `-Z delta:threshold=32` for the fixture. | [tests/cli/options/migration/0001_update_delta_threshold_environment_cli_equivalence.t](../../../tests/cli/options/migration/0001_update_delta_threshold_environment_cli_equivalence.t) |
| D6-06 | Environment `skip` produces the same animation bytes as `-Z delta:error=skip` for the fixture. | [tests/cli/options/migration/0002_update_delta_error_environment_cli_equivalence.t](../../../tests/cli/options/migration/0002_update_delta_error_environment_cli_equivalence.t) |
| D6-07 | Default `auto` requests `P2=1` with 6delta. | [tests/quant/palette/usage/0186_6delta_auto_policy_selects_keep.t](../../../tests/quant/palette/usage/0186_6delta_auto_policy_selects_keep.t) |
| D6-08 | Explicit `clear` conflicts with 6delta and reports the alpha-policy requirement. | [tests/quant/palette/usage/0187_6delta_explicit_alpha_policy_conflict.t](../../../tests/quant/palette/usage/0187_6delta_explicit_alpha_policy_conflict.t) |
| D6-09 | Explicit `composite` conflicts with 6delta and reports the alpha-policy requirement. | [tests/quant/palette/usage/0190_6delta_composite_policy_conflict.t](../../../tests/quant/palette/usage/0190_6delta_composite_policy_conflict.t) |
| D6-10 | LSO2 selects the keep index at threshold zero and propagates retained-color error to the next sample. | [tests/processing/filter/0015_filter_dither_6delta_lso2.t](../../../tests/processing/filter/0015_filter_dither_6delta_lso2.t) |
| D6-11 | Jarvis, Judice & Ninke applies the same keep/error observation. | [tests/processing/filter/0016_filter_dither_6delta_jajuni.t](../../../tests/processing/filter/0016_filter_dither_6delta_jajuni.t) |
| D6-12 | Stucki applies the same keep/error observation. | [tests/processing/filter/0017_filter_dither_6delta_stucki.t](../../../tests/processing/filter/0017_filter_dither_6delta_stucki.t) |
| D6-13 | Burkes applies the same keep/error observation. | [tests/processing/filter/0018_filter_dither_6delta_burkes.t](../../../tests/processing/filter/0018_filter_dither_6delta_burkes.t) |
| D6-14 | Sierra Lite applies the same keep/error observation. | [tests/processing/filter/0019_filter_dither_6delta_sierra1.t](../../../tests/processing/filter/0019_filter_dither_6delta_sierra1.t) |
| D6-15 | Sierra Two-row applies the same keep/error observation. | [tests/processing/filter/0020_filter_dither_6delta_sierra2.t](../../../tests/processing/filter/0020_filter_dither_6delta_sierra2.t) |
| D6-16 | Sierra-3 applies the same keep/error observation. | [tests/processing/filter/0021_filter_dither_6delta_sierra3.t](../../../tests/processing/filter/0021_filter_dither_6delta_sierra3.t) |
| D6-17 | A-dither records a keep index and mask when retained RGB beats its perturbed palette choice. | [tests/processing/filter/0022_filter_dither_6delta_a_dither.t](../../../tests/processing/filter/0022_filter_dither_6delta_a_dither.t) |
| D6-18 | X-dither records the corresponding keep index and mask. | [tests/processing/filter/0023_filter_dither_6delta_x_dither.t](../../../tests/processing/filter/0023_filter_dither_6delta_x_dither.t) |
| D6-19 | CPU blue noise records the corresponding keep index and mask. | [tests/processing/filter/0024_filter_dither_6delta_bluenoise.t](../../../tests/processing/filter/0024_filter_dither_6delta_bluenoise.t) |
| D6-20 | `skip` leaves the following sample unchanged by a kept pixel for Jajuni, Stucki, Burkes, and the three Sierra policies. | [tests/processing/filter/0026_filter_dither_6delta_diffusion_skip.t](../../../tests/processing/filter/0026_filter_dither_6delta_diffusion_skip.t) |
| D6-21 | High-level forced GPU blue noise attaches an exact-size retained plane and returns the expected keep index/mask; requires a GPU-capable build and device. | [tests/processing/filter/0028_filter_dither_6delta_bluenoise_gpu_exact_plane.t](../../../tests/processing/filter/0028_filter_dither_6delta_bluenoise_gpu_exact_plane.t) |
| D6-22 | GPU `auto` permits origin-aware CPU fallback for an offset retained plane. | [tests/processing/filter/0029_filter_dither_6delta_bluenoise_gpu_auto_fallback.t](../../../tests/processing/filter/0029_filter_dither_6delta_bluenoise_gpu_auto_fallback.t) |
| D6-24 | Environment tuning alone does not enable delta; explicit suffixes override it; repeated delta resets tuning; invalid arguments preserve parameters; invalid base environment falls back to full. | [tests/processing/filter/0011_filter_encode_accumulation_buffer.t](../../../tests/processing/filter/0011_filter_encode_accumulation_buffer.t) |
| D6-25 | Extended float centroids remain unrounded; first/repeated/invalidated frames respectively paint, keep and repaint; changed content then repaints, with retained RGB matching decoded RGB101. | [tests/quant/palette/delta_native/0001_delta_float_centroid.t](../../../tests/quant/palette/delta_native/0001_delta_float_centroid.t) |
| D6-26 | A genuinely byte-eligible no-dither request retains its byte route and the same retained-plane lifecycle. | [tests/quant/palette/delta_native/0002_delta_byte_control.t](../../../tests/quant/palette/delta_native/0002_delta_byte_control.t) |
| D6-27 | Moving four-pixel updates on a six-pixel plane keep only the two-pixel overlap; invalidation leaves untouched positions invalid. | [tests/quant/palette/delta_native/0003_delta_plane_origin.t](../../../tests/quant/palette/delta_native/0003_delta_plane_origin.t) |
| D6-28 | Source-mask holes remain unpainted and invalid while visible pixels participate in native delta. | [tests/quant/palette/delta_native/0004_delta_source_mask.t](../../../tests/quant/palette/delta_native/0004_delta_source_mask.t) |
| D6-29 | Observed multiworker dispatch encodes a 25-row image with a partial final band; repeated pixels keep and invalidation repaints. | [tests/quant/palette/delta_native/0005_delta_parallel_rows.t](../../../tests/quant/palette/delta_native/0005_delta_parallel_rows.t) |
| D6-30 | FS native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0006_delta_residual_fs.t](../../../tests/quant/palette/delta_native/0006_delta_residual_fs.t) |
| D6-31 | ATKINSON native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0007_delta_residual_atkinson.t](../../../tests/quant/palette/delta_native/0007_delta_residual_atkinson.t) |
| D6-32 | JAJUNI native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0008_delta_residual_jajuni.t](../../../tests/quant/palette/delta_native/0008_delta_residual_jajuni.t) |
| D6-33 | STUCKI native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0009_delta_residual_stucki.t](../../../tests/quant/palette/delta_native/0009_delta_residual_stucki.t) |
| D6-34 | BURKES native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0010_delta_residual_burkes.t](../../../tests/quant/palette/delta_native/0010_delta_residual_burkes.t) |
| D6-35 | SIERRA1 native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0011_delta_residual_sierra1.t](../../../tests/quant/palette/delta_native/0011_delta_residual_sierra1.t) |
| D6-36 | SIERRA2 native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0012_delta_residual_sierra2.t](../../../tests/quant/palette/delta_native/0012_delta_residual_sierra2.t) |
| D6-37 | SIERRA3 native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0013_delta_residual_sierra3.t](../../../tests/quant/palette/delta_native/0013_delta_residual_sierra3.t) |
| D6-38 | LSO2 native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/quant/palette/delta_native/0014_delta_residual_lso2.t](../../../tests/quant/palette/delta_native/0014_delta_residual_lso2.t) |
| D6-39 | A signed sample distinguishes thresholds 1 and 2 without intermediate clipping; the retained normalized value has an explicit tolerance. | [tests/quant/palette/delta_native/0015_delta_signed_threshold.t](../../../tests/quant/palette/delta_native/0015_delta_signed_threshold.t) |
| D6-40 | Unconnected P3 delta rejects before output; the same encoder can recover with sRGB gamma output. | [tests/quant/palette/delta_native/0016_delta_p3_rejection_recovery.t](../../../tests/quant/palette/delta_native/0016_delta_p3_rejection_recovery.t) |
| D6-41 | AUTO and every admitted spatial method reach native delta through the high-level encoder, retaining the working centroid and repeated-frame wire colors. | [tests/quant/palette/delta_native/0017_delta_spatial_dispatch.t](../../../tests/quant/palette/delta_native/0017_delta_spatial_dispatch.t) |
| D6-42 | a: Original-source fidelity and threshold decisions differ from the biased query; invalid history and disabled delta paint normally, and the source is unchanged. | [tests/quant/palette/delta_native/0018_delta_a_source.t](../../../tests/quant/palette/delta_native/0018_delta_a_source.t) |
| D6-43 | x: Original-source fidelity and threshold decisions differ from the biased query; invalid history and disabled delta paint normally, and the source is unchanged. | [tests/quant/palette/delta_native/0019_delta_x_source.t](../../../tests/quant/palette/delta_native/0019_delta_x_source.t) |
| D6-44 | blue: Original-source fidelity and threshold decisions differ from the biased query; invalid history and disabled delta paint normally, and the source is unchanged. | [tests/quant/palette/delta_native/0020_delta_blue_source.t](../../../tests/quant/palette/delta_native/0020_delta_blue_source.t) |
| D6-45 | a: Native float first/repeated/invalidated/changed frames preserve exact emitted colors, with shifted origins and source-mask holes. | [tests/quant/palette/delta_native/0021_delta_a_frames.t](../../../tests/quant/palette/delta_native/0021_delta_a_frames.t) |
| D6-46 | x: Native float first/repeated/invalidated/changed frames preserve exact emitted colors, with shifted origins and source-mask holes. | [tests/quant/palette/delta_native/0022_delta_x_frames.t](../../../tests/quant/palette/delta_native/0022_delta_x_frames.t) |
| D6-47 | blue: Native float first/repeated/invalidated/changed frames preserve exact emitted colors, with shifted origins and source-mask holes. | [tests/quant/palette/delta_native/0023_delta_blue_frames.t](../../../tests/quant/palette/delta_native/0023_delta_blue_frames.t) |
| D6-48 | a: Multiworker serialization preserves the native centroid and retained-plane lifecycle across full and partial bands. | [tests/quant/palette/delta_native/0024_delta_a_workers.t](../../../tests/quant/palette/delta_native/0024_delta_a_workers.t) |
| D6-49 | x: Multiworker serialization preserves the native centroid and retained-plane lifecycle across full and partial bands. | [tests/quant/palette/delta_native/0025_delta_x_workers.t](../../../tests/quant/palette/delta_native/0025_delta_x_workers.t) |
| D6-50 | blue: Multiworker serialization preserves the native centroid and retained-plane lifecycle across full and partial bands. | [tests/quant/palette/delta_native/0026_delta_blue_workers.t](../../../tests/quant/palette/delta_native/0026_delta_blue_workers.t) |
| D6-51 | a: A byte-eligible positional request retains its byte route and emitted-color lifecycle. | [tests/quant/palette/delta_native/0027_delta_a_byte.t](../../../tests/quant/palette/delta_native/0027_delta_a_byte.t) |
| D6-52 | x: A byte-eligible positional request retains its byte route and emitted-color lifecycle. | [tests/quant/palette/delta_native/0028_delta_x_byte.t](../../../tests/quant/palette/delta_native/0028_delta_x_byte.t) |
| D6-53 | blue: A byte-eligible positional request retains its byte route and emitted-color lifecycle. | [tests/quant/palette/delta_native/0029_delta_blue_byte.t](../../../tests/quant/palette/delta_native/0029_delta_blue_byte.t) |
| D6-54 | a: Absolute band and plane origins, warmup/result-mask fences, invalid history and source-mask priority agree in raster and serpentine scans. | [tests/quant/palette/delta_native/0030_delta_a_band.t](../../../tests/quant/palette/delta_native/0030_delta_a_band.t) |
| D6-55 | x: Absolute band and plane origins, warmup/result-mask fences, invalid history and source-mask priority agree in raster and serpentine scans. | [tests/quant/palette/delta_native/0031_delta_x_band.t](../../../tests/quant/palette/delta_native/0031_delta_x_band.t) |
| D6-56 | blue: Absolute band and plane origins, warmup/result-mask fences, invalid history and source-mask priority agree in raster and serpentine scans. | [tests/quant/palette/delta_native/0032_delta_blue_band.t](../../../tests/quant/palette/delta_native/0032_delta_blue_band.t) |
| D6-57 | Built-in gray8 retains all 256 working entries and exact wire colors through repeat, invalidation, changed content and full/delta transitions. | [tests/quant/palette/delta_native/0033_delta_builtin_slots.t](../../../tests/quant/palette/delta_native/0033_delta_builtin_slots.t) |
| D6-58 | A 256-entry PAL retains every entry through the same native lifecycle. | [tests/quant/palette/delta_native/0034_delta_map_slots.t](../../../tests/quant/palette/delta_native/0034_delta_map_slots.t) |
| D6-59 | Duplicate and unused PAL entries retain their positions and values. | [tests/quant/palette/delta_native/0035_delta_map_duplicates.t](../../../tests/quant/palette/delta_native/0035_delta_map_duplicates.t) |
| D6-60 | Source-mask holes move independently of keeps; real index zero remains paintable. | [tests/quant/palette/delta_native/0036_delta_builtin_mask.t](../../../tests/quant/palette/delta_native/0036_delta_builtin_mask.t) |
| D6-61 | Full-size fixed maps combine source holes and delta keeps without taking a color slot. | [tests/quant/palette/delta_native/0037_delta_map_mask.t](../../../tests/quant/palette/delta_native/0037_delta_map_mask.t) |
| D6-62 | Overlapping updates use retained-plane coordinates and repaint invalid regions. | [tests/quant/palette/delta_native/0038_delta_builtin_origin.t](../../../tests/quant/palette/delta_native/0038_delta_builtin_origin.t) |
| D6-63 | Mapfile updates preserve the same shifted-plane lifecycle. | [tests/quant/palette/delta_native/0039_delta_map_origin.t](../../../tests/quant/palette/delta_native/0039_delta_map_origin.t) |
| D6-64 | Observed multiworker encoding preserves full-table keeps across six-row bands and a partial final band. | [tests/quant/palette/delta_native/0040_delta_builtin_workers.t](../../../tests/quant/palette/delta_native/0040_delta_builtin_workers.t) |
| D6-65 | Parallel mapfile delta consumes the shared result mask. | [tests/quant/palette/delta_native/0041_delta_map_workers.t](../../../tests/quant/palette/delta_native/0041_delta_map_workers.t) |
| D6-66 | Animation reuses fixed source/application state while retaining valid delta history. | [tests/quant/palette/delta_native/0042_delta_builtin_animation.t](../../../tests/quant/palette/delta_native/0042_delta_builtin_animation.t) |
| D6-67 | Animation reuses the mapfile application and its unchanged palette. | [tests/quant/palette/delta_native/0043_delta_map_animation.t](../../../tests/quant/palette/delta_native/0043_delta_map_animation.t) |
| D6-68 | A byte-eligible fixed table keeps its previous route and repaint behavior. | [tests/quant/palette/delta_native/0044_delta_builtin_byte.t](../../../tests/quant/palette/delta_native/0044_delta_builtin_byte.t) |
| D6-69 | A byte-eligible map keeps its previous route and repaint behavior. | [tests/quant/palette/delta_native/0045_delta_map_byte.t](../../../tests/quant/palette/delta_native/0045_delta_map_byte.t) |
| D6-70 | Native/byte/native transitions on one animated encoder refresh the application and mask. | [tests/quant/palette/delta_native/0046_delta_builtin_reuse.t](../../../tests/quant/palette/delta_native/0046_delta_builtin_reuse.t) |
| D6-71 | Mapfile application transitions retain independent fixed source ownership. | [tests/quant/palette/delta_native/0047_delta_map_reuse.t](../../../tests/quant/palette/delta_native/0047_delta_map_reuse.t) |
| D6-72 | Retaining a color outside the new fixed table diffuses its residual and paints black index zero next to an omission. | [tests/quant/palette/delta_native/0048_delta_fixed_fs_diffuse.t](../../../tests/quant/palette/delta_native/0048_delta_fixed_fs_diffuse.t) |
| D6-73 | Skipping the retained-color residual paints white at the following sample. | [tests/quant/palette/delta_native/0049_delta_fixed_fs_skip.t](../../../tests/quant/palette/delta_native/0049_delta_fixed_fs_skip.t) |
| D6-74 | Mask allocation failure, disabled capture and missing history expose no stale active mask; retry and same-size reuse succeed. | [tests/quant/palette/delta_native/0050_delta_fixed_mask_failure.t](../../../tests/quant/palette/delta_native/0050_delta_fixed_mask_failure.t) |
| D6-75 | Image-map construction retains all 256 grayscale colors independently of active display history. | [tests/quant/palette/delta_native/0051_delta_image_map_capacity.t](../../../tests/quant/palette/delta_native/0051_delta_image_map_capacity.t) |

### Quality regression tests

| ID | Observation and limit | Owning test |
| --- | --- | --- |
| D6-23 | Alternating black/white regions are composed onto a retained canvas and checked against brightness bounds, detecting accidental ordinary-color selection of the transparency key. This is a targeted stale-color regression, not an MS-SSIM guarantee or exact equality of every output pixel. | [tests/processing/filter/0013_filter_encode_6delta_keycolor.t](../../../tests/processing/filter/0013_filter_encode_6delta_keycolor.t) |

### Defensive and malformed-input tests

The [filter test directory](../../../tests/processing/filter/) also checks contradictory internal GPU enable/capability flags and exercises direct GPU request wiring. General malformed SIXEL and allocation coverage belongs to the [testing guide](../../testing/guide.md); it does not prove retained-screen correctness on a terminal.

### Coverage limits and manual validation

The exact early-match and threshold-255 boundaries, plane-size rejection/reset details, seeded-buffer lifecycle, broader float32/output-space scope, and missing-emitted-RGB fallback are audited against owning code. The table does not provide a separate focused automated assertion for every one of those dimensions. Likewise, the listed LSO2 test asserts `diffuse`, while D6-20's `skip` test covers only its six named policies. GPU tests may skip when the required device or backend is unavailable.

Receiver preservation, palette interpretation, external scroll/overlay recovery, delivery loss, and temporal appearance are application/terminal integration observations. Validate them with the actual receiver and a composed frame sequence; a passing single-frame decoder test cannot establish them.
