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

Source transparency means “do not update this position,” not “erase what was painted here earlier.” A moving object cannot remove its old pixels merely by making them transparent in the next source frame. To restore the background, supply its opaque color or arrange an external repaint and invalidate the affected history. This limitation does not prevent delta from encoding an opaque image with omitted, unchanged pixels.

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

When the early gate fails, lookup selects a palette candidate `P`. For ordinary gamma RGB output with a byte-only palette, `P` is the emitted RGB101 value expanded to RGB888, as specified [below](#byte-candidate-comparison). Ordinary gamma HLS uses its decoded HLS candidate as described [below](#hls-retained-color-model). Other byte application contracts retain the supplied candidate value. The CPU RGB888 keep helper compares squared RGB distances:

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

The public declarations are in [sixel.h.in](../../../include/sixel.h.in). The feature macros `SIXEL_HAVE_ENCODER_6DELTA_PLANE`, `SIXEL_HAVE_ENCODER_6DELTA_REFINEMENT`, and `SIXEL_HAVE_ENCODER_ENCODE_FRAME` allow consumers to test header capabilities.

| API | Purpose and lifetime |
| --- | --- |
| `sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY, "delta:threshold=0:error=diffuse")` | Select delta and its parameters. |
| `sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY, "full")` | Disable temporal keeps and discard history. |
| `sixel_encoder_set_6delta_plane_size(encoder, width, height)` | Declare the persistent surface in output pixels. Changing the size discards history; repeating the same size preserves it. `(0, 0)` restores frame-sized history. |
| `sixel_encoder_set_6delta_plane_origin(encoder, x, y)` | Locate the next frame inside the declared plane. The nonnegative pixel origin persists until changed. |
| `sixel_encoder_set_accumulation_buffer(encoder, pixels, width, height, pixelformat)` | Copy a supplied previous image into RGB888 history, mark all positions valid, and declare its dimensions as the plane size. Passing `NULL` clears the history. |
| `sixel_encoder_invalidate_6delta_plane(encoder)` | Discard history while preserving the declared plane size and origin. |
| `sixel_encoder_encode_frame(encoder, frame, output)` | Encode an initialized frame and advance the retained state through the high-level encoder. |
| `sixel_encoder_set_6delta_threshold_limit(encoder, limit)` | Apply a persistent runtime ceiling without changing the configured threshold or history. |
| `sixel_encoder_get_6delta_painted_pixels(encoder)` | Count opaque pixels painted in the latest successful delta frame independently of protocol overhead; zero after invalidation or when delta is disabled. |
| `sixel_encoder_needs_6delta_refinement(encoder)` | Query the latest successful frame for approximate early keeps, including partially painted frames; scheduling and source lifetime belong to the caller. |

### Caller-driven idle refinement

A positive threshold is the maximum early-keep tolerance. Consumers compiled with `SIXEL_HAVE_ENCODER_6DELTA_REFINEMENT` can apply a temporary ceiling with `sixel_encoder_set_6delta_threshold_limit(encoder, limit)`. The effective threshold is `min(configured_threshold, limit)`. The limit accepts 0 through 255, defaults to 255, and persists until changed. It neither changes the configured maximum nor invalidates history. Restore 255 before ordinary encoding; setting zero keeps delta enabled and still permits the post-lookup keep comparison.

After a successful frame, `sixel_encoder_needs_6delta_refinement(encoder)` reports whether at least one pixel was kept by a nonexact, positive-threshold early match, even when other pixels were painted. Exact matches, source transparency, and post-lookup keeps alone do not request refinement. `sixel_encoder_get_6delta_painted_pixels(encoder)` reports the number of opaque painted pixels during the same retained-plane update, without another scan. `SIXEL_HAVE_ENCODER_6DELTA_REFINEMENT >= 2` identifies this accessor and the partially painted refinement result (revision 1 reported only zero-paint frames). The predicate does not measure output volume: SIXEL envelopes and palette definitions can still be emitted for an entirely omitted frame. The query describes only the latest frame of that encoder, is false after invalidation, and does not retain a source image or schedule work.

An interactive caller can retain this result across subsequent damage rectangles and arm one short timer when its primary encoded output falls below an activity budget. A zero painted-pixel count can bypass the byte budget so protocol overhead does not block idle repair. Repeated small updates must not extend that deadline; larger updates can defer the work without forgetting earlier approximate keeps. On expiry, set the ceiling to zero and re-evaluate the current source over the affected surface, retaining delta history. Re-evaluating the whole visible surface is a simple way to cover earlier omitted rectangles when damage moves. Use current source pixels, not a saved old frame that could overwrite later changes. Respect output backpressure and cancel obsolete work after geometry/history invalidation. A refinement pass must not arm itself again; a zero-threshold post-lookup keep may already be preferable to repainting. The library does not create threads, timers, or unsolicited writes, and `img2sixel` does not automatically replay idle frames.

Output budgets and timing belong to the caller. The query alone does not track outstanding error outside the latest frame rectangle; callers must retain that obligation until full-surface refinement or history invalidation. Continuous threshold decay, regional aging, and guaranteed convergence beyond the selected palette are separate policies.

### Progressive repair API

`SIXEL_HAVE_ENCODER_6DELTA_PROGRESSIVE >= 1` provides a bounded CPU repair API over the same retained plane. A repair context owns an immutable, preprocessed target and resumable work; the encoder continues to own displayed colors and validity. Repair can improve ordinary palette error after a single baseline, including at ordinary threshold zero. It does not require a positive result from the legacy refinement predicate.

| Function | Contract |
| --- | --- |
| `sixel_progressive_config_init(config, sizeof(*config))` | Initialize the supported configuration layout. Future structure tails remain untouched. |
| `sixel_progressive_new(&context, encoder, config, allocator)` | Copy configuration and retain the encoder and allocator. Effective delta must be enabled; one repair context may attach to an encoder. A null allocator selects the default allocator. |
| `sixel_progressive_bind_frame(context, frame, x, y, generation)` | Copy and preprocess an unprocessed source at the explicit retained-plane origin. Use nonzero, increasing generations. A failed bind preserves the previous binding. |
| `sixel_progressive_step(context, budget, output, stats)` | Resume bounded work and submit at most one complete sparse `P2=1` DCS. A null output uses the encoder's configured destination. |
| `sixel_progressive_unref(context)` | Release the context after its active/waiting calls finish. Compatible retained history remains available to ordinary delta. |

Preserve the original source before ordinary encoding can modify a working frame. Establish the baseline, bind that original source, and call `step` at caller-selected opportunities. Set `struct_size` on zero-initialized budget and statistics structures. All three budget limits must be positive: `max_packet_bytes` counts the complete DCS, while `max_pixel_visits` and `max_distance_evaluations` bound resumable computation. The public default-limit macros provide starting values; they do not impose a timer or a frame rate. A successful output callback must consume or copy the entire submitted span into an ordered, reliable path. Any nonnegative callback result, including zero, means acceptance; report an incomplete write as negative. Output failure invalidates retained history.

Check both the returned status and `stats.outcome`; `SIXEL_INTERRUPTED` is not a failure-class status. Successful outcomes are:

| Outcome | Caller action |
| --- | --- |
| `FRAME_EMITTED` | The packet and acc update completed. More repair may remain. |
| `YIELDED` | Resume later. `yield_reason` distinguishes work exhaustion, a pending ordinary update, and a busy encoder. |
| `CONVERGED` | A complete canonical audit found no source-domain residual above the configured threshold. |
| `STALLED` | Full-domain recovery found no improvement using the bounded palette and direct-source-projection strategy. This is not a proof of an output-color optimum. |
| `NEEDS_BASELINE` | Establish valid history for the unknown source-domain positions. |
| `NEEDS_SOURCE` | Bind a newer source after an ordinary frame or relevant configuration change. |
| `BUDGET_TOO_SMALL` | Increase the byte budget; `required_packet_bytes` reports the staged one-pixel packet size. |
| `UNSUPPORTED` | Select a supported configuration or output path. |

Residuals and acceptance use the effective `-W` coordinates. Each accepted pixel strictly reduces its unperturbed squared distance to the target after the actual RGB101/HLS output projection. The defaults use an upper-residual fraction of 0.25, alternating with full selection; a population-weighted retained-aware palette with at most 32 colors, 4096 samples and 128 candidates; and fixed positional blue noise of strength 0.055 with exhaustive lookup. Automatic stopping distances are `2/255` in gamma, `0.005` in linear, and `0.01` in normalized perceptual workspaces. Zero-noise application and direct-source-projection recovery distinguish stalled work from a converged audit. These defaults do not establish monotonic MS-SSIM or Delta E improvement.

Configuration is immutable for one context. `stop_distance_auto=0` selects the explicit nonnegative `stop_distance`, whose square must be finite; equality with that threshold is resolved. `selection_fraction` is in `(0, 1]`, `max_colors` in `1..255`, `sample_limit` in `1..65536`, and `candidate_limit` in `max_colors..4096`. `min_gain_ratio` is in `[0, 1)`, `swap_limit` in `0..16`, and `noise_strength` must be finite and nonnegative with safe squared arithmetic. Noise phases are `0..63`, and `noise_channel_rgb` is a boolean selecting shared versus separate channel noise. `gpu_policy` accepts `SIXEL_PROGRESSIVE_GPU_INHERIT` or an existing GPU policy constant. Invalid values are rejected before attaching a context.

Statistics describe the owned snapshot only when `state_snapshot_valid` is true. Full-domain residual totals additionally require `audit_complete`; a successful paint invalidates the pre-paint audit. Successful paint and packet byte counts describe this call only and are zero on yields and errors. Work totals are per binding, reset on successful rebinding and reject a quantum that could overflow; they never wrap. Color counts distinguish the constructed palette from the pruned, transmitted palette. Source/history/configuration revisions identify the reported state.

The caller owns placement, pacing and output order. Position each emitted packet at the bound rectangle, and account for cursor-prefix bytes separately from the DCS byte limit. A writer can prepend positioning only when it actually receives a packet; a yielded or completed analysis emits no bytes. Direct frame and byte APIs remain one ordinary frame per call. New ordinary work registered before repair's output admission has priority; an already admitted packet finishes its write and acc update before that ordinary operation proceeds. Thread-enabled builds serialize individual operations. The caller still protects compound origin/frame sequences and object lifetimes, and serializes all access in thread-disabled builds. A callback must not reenter its active encoder or repair context.

Residual selection, global sampling and palette application can use Metal in the canonical `-W` float coordinates. CPU audit data corrects float mask boundaries before sampling, preserving small residuals and protected candidates. Sampling uses the complete selected population across bounded dispatches, including video eligibility and luminance weights. Positional noise uses the shared absolute-coordinate CPU helper; palette construction, per-pixel acceptance and full-domain convergence audits remain on the CPU.

GPU `off` uses CPU selection and lookup. `auto` admits selection when the canonical unresolved population reaches the encoder's `palette_threshold` (`-G auto:Pvalue`); palette application separately uses the final selected population. Allocation or device failure permits CPU fallback. Admission uses each complete population, independently of the resumable work quantum. `force` bypasses that threshold and reports `UNSUPPORTED` when Metal is unavailable; other backend failures remain errors. Bounded batches and palette slices respect pixel and distance budgets, including a one-distance quantum. Failed GPU attempts are conservatively charged against work limits before CPU retry.

The context owns scratch device buffers and releases them on rebinding or destruction; a failed dispatch evicts reusable device state. A `METAL` backend statistic means that the current preparation has completed GPU selection or lookup, not that its audit or acceptance ran on the GPU. Float near-ties can choose a different candidate than CPU lookup, but cannot bypass canonical acceptance.

Fixed palettes, monochrome, high-color, OR mode, macros, DRCS, PNG output, multiplexer penetration, body/header suppression and transparent output offsets are unsupported by this repair path. Ordinary encoding retains its own supported paths and policies.

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
| Native float32 generated and fixed palettes | The selected CPU native path supports `-W gamma\|linear\|oklab\|cielab\|din99d -U gamma\|linear\|smpte-c\|display-p3 -t auto\|rgb\|hls`, at least two requested colors for generated palettes, ordinary SIXEL output, and `none`, the nine spatial diffusion policies or the three positional methods listed above (`auto` resolves as usual). Working samples and centroids retain negative, over-one and sub-byte components. Retained colors compete in the selected output RGB; see the comparison rule below. Genuine byte-eligible requests remain byte paths. |
| Palette-free monochrome `-e` | Conflicts with `-Z delta`, including environment-selected delta. It paints foreground bits and omits background bits; `P2=1` cannot erase previous foreground pixels when they become background. Rejection occurs before output, independently of precision or inversion. Select `-Z full` before `-e` to disable delta. Opaque black-and-white `-b gray1` paints both endpoints and is a distinct, supported fixed-palette case. |
| Other float32 combinations | RGB888 promoted for soft/none diffusion keeps its existing gamma-RGB history behavior. This is not a claim of equivalent delta behavior on every legacy float path. |
| GPU palette application | The Metal path supports RGB888 `none`, `a_dither`, `x_dither` and `bluenoise` with a compatible retained plane matching the frame geometry. Both direct and Eytzinger lookup exclude the reserved omission key from paintable candidates, as CPU lookup does. A keep must carry its omission mask; an ordinary palette selection must paint and advance history to the emitted color. The GPU implements both threshold and post-lookup decisions. CPU error-diffusion kernels are a separate path. |
| GPU with a larger or offset retained plane | The GPU request cannot express general origin-based plane addressing. `auto` declines that request so the CPU can apply the proper plane coordinates. Forced GPU use is not a portable fallback setting; prefer `off` or `auto` for damage rectangles. |
| Generated palette | An active retained plane requires a reserved transparency key. In the ordinary generated-palette path, one slot is taken from the requested budget; for `-p 256`, up to 255 entries remain for painted colors when that reservation is required. The key is excluded from ordinary nearest-color lookup. |
| Fixed palette and inter-frame palette reuse | Keeping display pixels and reusing palette construction are different operations. A stable palette does not establish a valid retained plane. Native gamma/linear sRGB, SMPTE-C or Display P3 `-b` and `-m` preserve all supplied entries, including 256-color tables, and omit by a separate mask as described below. Byte routes retain their earlier behavior; they do not gain native fixed-palette keeps solely by enabling delta. |
| OR output `-O` | Conflicts with `-Z delta`, including environment-selected delta. `P2=5` reconstructs palette-index bit planes and does not carry the `P2=1` operation that preserves omitted destination RGB. Zero bits are not delta keeps. Rejected option selection preserves prior options and history; the encoder also validates the combination before output. Select `-Z full` before enabling OR. Full OR encoding and low-level OR APIs retain their existing behavior. |
| DRCS `-@` | Conflicts with `-Z delta`, including environment-selected delta. Glyph definitions omit the ordinary SIXEL `P2` header and do not update the retained display plane. Selection rejects before changing DRCS settings or opening/truncating the character-output path; selecting delta after DRCS preserves the previous policy and history on rejection. Earlier successful DRCS selections may already have opened their output path; rejecting a later delta selection does not undo those earlier effects. Frame validation rejects before palette work and DRCS geometry lookup, as well as before definition bytes. Select `-Z full` before DRCS to disable delta. Full glyph output retains its existing behavior. |
| Terminal macros `-u` / `-n` | Conflict with `-Z delta`, including environment-selected delta and combined `-u -n` selection. A definition is not a display update; a cached invocation can occur against a different surface. The current emitter shares one display history and does not track or restore the base required by each stored delta. Either option order rejects before changing selectors, delta settings or retained history; frame validation also rejects before definition/invocation bytes. Select `-Z full` before the macro option. Full definitions, automatic invocations and animation replay retain their existing behavior. This is a current implementation limit, not a restriction of the macro wire format itself. |
| PNG snapshots | Conflict with `-Z delta`, including environment-selected delta, for `.png`, `png:<path>` and `png:-` targets. The snapshot decoder starts without the retained base and cannot reconstruct omitted pixels. Both option orders reject before changing destinations, policy settings or history. Frame validation rejects before palette work; file encoding rejects before input or staging access. Select `-Z full` before the PNG destination, or switch to ordinary SIXEL before selecting delta. Full snapshot pixels and color tagging retain their existing behavior. This is the current snapshot implementation boundary; composing a complete retained surface would require an explicit frame, geometry and validity contract. |
| High-color output `-I` | This has separate repeated-register dispatch. This guide does not claim `-Z delta -I` as a supported combination; use ordinary indexed output for the workflow above. |
| Missing emitted RGB result | If the encode path cannot supply trustworthy emitted colors, the high-level update discards history instead of seeding it from source RGB. |

### Wire-rounded history across byte and float paths

Ordinary gamma RGB palette output captures the same RGB101 percentages that the serializer sends, then stores each percentage `c` as the byte `(c*255+50)/100`. This applies to byte and native float palettes, in serial and pipelined encoding. A fixed-table encoder can switch between byte and native float application without discarding its retained surface: history keeps one wire-based representation across that precision change.

Capture projects a temporary palette of at most 768 bytes, only when delta RGB capture is enabled. It does not modify working entries, lookup, residuals, snap policy or mapfile export, and adds no image-sized allocation beyond the existing RGB capture. A legacy float-backed output palette must use its serialized float component, not its byte shadow; the component `.0051` emits 1% (retained byte 3), even though its byte shadow is 1. Its `.0001` margin from the `.005` percentage boundary is much larger than float storage error. The output and retained integer expectations are exact; preservation of the original palette is checked as a byte-for-byte copy contract.

The byte correction covers ordinary gamma RGB and HLS definitions; HLS uses the reconstruction [below](#hls-retained-color-model). Legacy output other than `-U gamma` and body-only output retain their separate migration boundaries. The high-level encoder rejects palette-free monochrome, OR composition, DRCS, macro output and PNG snapshots with delta. Candidate comparison for byte-only gamma palettes follows the rule below; legacy float-backed application remains separate.

### Byte candidate comparison

For an ordinary gamma RGB output call with gamma source coordinates and no float-backed palette, post-lookup byte comparison projects each selected byte `b` to `c=(b*100+127)/255`, then `P=(c*255+50)/100`, using integer division. A readonly 256-byte table performs this exact projection without repeated divisions in the per-pixel comparison. It compares that emitted candidate with the retained RGB888 color. The query remains the current byte sample; this projection never overwrites the palette used by lookup or by newly painted-pixel diffusion. Ties retain the previous color. The early threshold gate and kept-color diffuse/skip rules are unchanged.

For example, source 32, retained 33 and selected entry 32 must keep: the entry emits 13%, represented as 33 in history. Retained 34 instead must repaint. Source 34 with retained 35 and selected entry 32 is another tie (distance 1 to both 35 and emitted 33); rounding the source to 33 would incorrectly reject it. These integer witnesses need no floating-point tolerance. Repeated generated frames with literal grays 32/96/160 retain 33/97/161 and omit the second frame in both serial and producer/writer encoding; changing the source to black paints again.

The output owner binds this comparison hint to its dither object for the encode call, before serial application or worker use, and clears it on success and failure. There is no global switch or retained-frame reset. Standalone PaletteApply has no wire-format contract and keeps its supplied-candidate comparison. OR, body-only, transformed source/output and legacy float-backed palettes do not acquire this byte comparison hint. HLS uses a separate palette-sized projection with the same call lifetime. Fixed byte tables still require their existing omission mechanism; this change does not add the native fixed-table mask path to them. Native float comparison remains as described below.

Metal receives the same emitted-candidate comparison view for byte RGB and HLS. The RGB projection runs once per palette; HLS reuses the output owner's prepared bytes. At most 768 comparison bytes accompany the existing GPU parameters. Direct and Eytzinger lookup still select from the original palette, and blue noise still perturbs only that selection. The post-lookup comparison uses the unperturbed source and the selected entry's emitted RGB. Without an output-owned view, standalone GPU application compares the supplied candidate as before. An undersized explicit view is rejected before dispatch.

For a chromatic RGB witness, target `(13,13,8)` and retained `(10,10,8)` have squared error 18. Candidate `(11,11,6)` appears to cost 12 before serialization, but emits `(10,10,5)` with error 27, so it must keep. Likewise, repainting target 32 over retained 33 with entry 32 cannot reduce the error: that entry also emits 33. These are candidate-effect errors, not evidence of corrupt retained history.

### Native float comparison and retained precision

The retained surface remains RGB888 in the selected gamma/linear sRGB, SMPTE-C or Display P3 output coordinates; this migration does not add a per-pixel float history or a public API. Each RGB101 component is represented in history as `(code * 255 + 50) / 100` using integer division. Native keep decisions compare that retained value with the same projection of the selected entry's emitted code (RGB101 or the HLS model below), rather than with its unrounded working centroid. Otherwise, even an identical frame can repaint because its original centroid is closer to the input than the already displayed, rounded color. Ties keep the displayed pixel. This comparison is a decision about sending another color, not palette snapping: lookup and residuals for newly painted pixels still use the authoritative working float palette.

The corrected float sample is never clipped or rounded for the keep test. The per-channel threshold is normalized by 255, and squared distances are accumulated in double. The current native implementation performs lookup before this decision; it does not promise the byte path's early-lookup avoidance. For `-U gamma -W gamma`, a kept pixel supplies the retained RGB888 value normalized to float for `diffuse`. Other workspaces and output other than `-U gamma` convert the corrected query to the selected output RGB before the comparison, then convert the retained output color into the workspace for the residual. The threshold means output-RGB channel units divided by 255 (linear-light units for `-U linear`), independently of `-W`; it does not become a Lab lightness/chroma threshold. `skip` suppresses the residual. A failed query transform declines the keep without recording a mask bit; it never substitutes a clipped query. Source masks still take precedence. Plane origins, invalid holes and explicit invalidation follow the same lifecycle as byte encoding. The byte retained surface is an approximation of the receiver's RGB101 colors, not a measurement of its physical output; both competing wire colors use the same approximation.

For a non-gamma workspace or output other than `-U gamma`, a keep outside the explicit threshold must also have no greater squared error in normalized working coordinates than the selected emitted color. Both wire colors are transformed back to the workspace for this guard. It uses unweighted coordinate distance, independently of lookup-specific axis weights, and does not replace the selected palette entry. This additional conservative condition prevents a virtual working query whose inverse is far outside gamma RGB from favoring a bright retained color over a dark chosen color. The query remains unclamped. A keep inside the explicit output threshold still follows that user-selected approximation tolerance.

For `a_dither`, `x_dither` and `bluenoise`, the noisy query selects the palette entry, but the keep threshold and distance comparison use the original, unperturbed float sample. This matches the byte positional contract: noise is a palette-selection technique, not a change to the desired color. There is no residual to propagate, so `diffuse` and `skip` have no neighboring-error effect for these methods. Absolute image coordinates determine the noise and retained-plane address; warmup rows do not write results or keep-mask entries. Source-mask omissions do not count as retained-color keeps.

Positional keep tests use literal red endpoints 0 and .2, retained as bytes 0 and 51, and source red .09 or .11. At `(0,0)`, A/X shift red by `-32/255`; the fixed blue-noise red tap sum is `52/255` at strength 1 with RGB channel offsets and zero phase. The query and original source land on opposite sides of the .1 distance boundary. A `2e-6` absolute query tolerance covers float stores and bias arithmetic, while the original-source decision has a .01 margin. Positive-threshold controls use .02/.18 sources and threshold `50/255`, with more than .016 margin. Palette indices and keep masks therefore have exact independent expectations; these tests do not derive their expectations from the production keep or noise helpers.

The connected path adds no persistent image-sized buffer beyond the existing RGB888 surface and validity mask. The workspace conversion uses bounded local triplets and no history-sized cache. sRGB output with the gamma workspace keeps its existing direct path. Other workspaces and output other than `-U gamma` add a query transform for valid-history comparisons, a retained-color transform for tentative keeps, and a selected-wire-color transform for tentative keeps outside the threshold. P3 reuses immutable color definitions owned by the prepared palette; workers read them without mutation. No per-pixel profile construction or image-sized conversion cache is involved. These transforms increase encoding cost, so reduced painting does not imply lower CPU time. HLS uses the palette-sized projection described below.

Workspace value tests use independent neutral-axis sRGB, linear, Oklab, CIELAB and DIN99d equations, with `3e-6` absolute tolerance in normalized working units for matrix coefficient rounding and float32 storage. The gamma decision witnesses have at least `.011` threshold margin; their exact keep masks do not depend on floating-point ties. An FS linear witness retains byte 171, whose linear value is approximately `.40724021`: source `.32` sends a `7/16` residual of approximately `-.03816759`. A following `.51` becomes black while `.56` stays white. Both skip behavior and using gamma history directly produce a different observable result. Generated-color tests additionally preserve extended, chromatic centroids against independently evaluated literal references. The virtual-color guard uses DIN99d `(0,1,-1)`, black, and retained `(247,245,209)`: black has exactly 2 squared error in working coordinates, while the retained color exceeds 3. Its decision therefore has a large margin without relying on exact inverse-transform results. These are value contracts; the wider transform arithmetic is owned by the [color-space documentation](../../concepts/colorspace/README.md).

### HLS retained-color model

Native float delta also accepts `-t hls` with gamma, linear, SMPTE-C or Display P3 output and each of the five supported working spaces. HLS field generation follows [native HLS serialization](output-colorspace.md#native-hls-serialization): the destination float RGB is rounded directly to integer H/L/S, without an intermediate RGB101 store. To model the painted color, those fields pass through the decoder's established HLS-to-RGB percentage conversion, then each percentage becomes an RGB888 component via `(c*255+50)/100`. Candidate comparison and history capture use this same reconstruction. The working palette remains unsnapped; ordinary lookup and painted-pixel diffusion still use its original coordinates.

For example, gamma RGB `(.123,.456,.789)` produces HLS `(330,46,73)`. The decoder models this as percentages `(12,46,80)` and retained bytes `(31,117,204)`. Direct RGB serialization instead emits `(12,46,79)` and retains `(31,117,201)`. Mixing the latter candidate with the former history would repaint an identical HLS frame. Conversely, a source near blue `.80` must compare against 204 when considering that HLS candidate, rather than 201. Independent value witnesses protect both directions.

The palette owns at most 768 additional bytes for the reconstructed HLS percentages. It prepares this derived view before serial application or worker dispatch when native HLS delta is active, and invalidates it with the other output views. Workers borrow it read-only. Preparation requires no image-sized allocation or per-pixel HLS conversion. An encode-call hint selects this view and is cleared on success and failure. Standalone PaletteApply does not implicitly acquire a wire contract.

Ordinary byte HLS with gamma source/output coordinates also compares the selected candidate and captures painted history using this decoder model. The output owner prepares an immutable, stack-owned table of at most 768 bytes before lookup or worker dispatch. Each byte policy supplies the chosen palette index; comparison reads its reconstructed RGB888 entry without per-pixel HLS conversion. An explicit exact-snap setting is applied to this derived copy just as it is to the writer copy, without changing the lookup palette or enabling snap. The borrowed pointer and count are cleared on success and failure. Genuine byte work stays byte; HLS alone does not request float precision. This does not add the native fixed-table omission-mask contract to legacy fixed tables.

Byte HLS capture reconstructs the final writer palette, including a legacy float-backed palette when present. Comparison binding for a legacy float-backed application remains outside this byte-only scope. Native float comparison already uses its own prepared output view. Generated frames can switch byte→float→byte or float→byte→float while retaining compatible HLS history; source-alpha holes, partial-plane origins and invalidation follow the common plane rules.

Independent chromatic byte witnesses distinguish all three representations: `(31,116,201)` emits HLS `(330,45,73)` and retains `(31,115,199)`, while direct RGB percentages retain `(31,115,201)`. The three byte-frame fixtures have at least `1/102` field-unit distance from forward rounding boundaries and `.06` percentage-unit distance from inverse rounding boundaries. Float input representation and working arithmetic remain comfortably within the stated native fixture budgets. Candidate keep/repaint decisions use literal integer distances. The byte method tests isolate post-lookup ties with `error=skip`; they do not claim to revalidate each diffusion kernel's residual propagation.

Changing RGB/HLS syntax alone keeps compatible destination-RGB history: it does not relabel the RGB basis or require resetting the surface. The next candidate is evaluated using its new serialization. Changing the output color interpretation still follows the invalidation rule below. Fixed palettes retain all supplied indices, separate transparency masks, partial-plane origins and cached-palette reuse; generated palettes retain their omission-slot rule. Alternate delta transports remain outside this migration. Palette-free monochrome is incompatible with delta preservation rather than a pending color-model migration.

This is the library decoder's existing percentage approximation, not terminal readback. A receiver that reconstructs HLS at a different precision can display slightly different RGB values. The model does not establish receiver-specific HLS accuracy or universal perceptual improvement. Tests assert independent integer fields, reconstructed colors, keep/paint bits and stored history; they do not treat a round trip through shared production helpers as an independent expected value. Chromatic field witnesses have at least `.005` field-unit margin from a half-integer boundary; a `3e-6` working-coordinate budget and `5e-7` destination-component budget give a propagated HLS bound below `.002` field units for these fixtures (chroma exceeds `.3`), below that decision margin. Exact gray and endpoint cases exercise separate algebraic branches.

### Display P3 history interpretation

The retained plane records its output definition and transfer interpretation once for the whole surface. Changing `-U` makes old history incompatible; the next successful frame starts a new plane, with unpainted regions invalid. Repeating the same output setting, changing only working space or precision, or rejecting an invalid option does not change compatible history. A failed encode does not relabel old pixels as a new definition. The application must separately keep the receiver's interpretation and drawing location synchronized.

`sixel_encoder_set_accumulation_buffer()` interprets its normalized RGB values in the encoder's current output definition. Select that definition before supplying a seed. The API does not run source-image CMS over the seed. For exact agreement with encoder-generated history, use the emitted RGB101 colors expanded to RGB888 as described above. Identical numeric black is deliberately tested across sRGB/P3 switches: differing primary colors alone would not detect an incorrect history-identity check.

A P3 red `(1,0,0)` becomes approximately `(1.224940176,-.042056955,-.019637555)` in linear sRGB. Preserving those extended components is necessary when diffusing error from a retained pixel. P3 transform tests derive independent reference values from xy primaries, D65, signed sRGB transfer and the documented working equations; `3e-6` absolute tolerance covers matrix coefficient rounding and float32 representation. A two-pixel FS test fixes the following red sample at `.6`; the retained-red residual changes it to approximately `.492817735`, below the black/red boundary. Using raw P3 values or clipping retained red to 1 instead selects red. Threshold tests use a separate negative P3 witness with a margin from both integer threshold boundaries. Cached definitions are also tested for independent ownership, invalidation and failure without partial publication.

### Linear and SMPTE-C output history

The native ordinary-RGB delta path also supports `-U linear` and `-U smpte-c` with all five working spaces. It preserves the same RGB101-to-RGB888 history convention, output-channel threshold, conservative working-distance guard and separate omission mask. A linear code is interpreted as linear light; SMPTE-C uses the option's existing signed power-2.2 decoding and inverse primary matrix before conversion into the workspace. The inverse preserves negative and above-one sRGB components and validates results before publication. This does not add SMPTE-C as a clustering or working space, change byte-path eligibility, or add gamut mapping. Output interpretation changes invalidate compatibility even when both surfaces contain identical numeric black; a working-space change alone does not.

Inverse value tests evaluate the documented legacy matrix and transfer equations independently in Decimal and store literal results, including bounded green whose sRGB inverse has a negative blue component. Their `2e-12` absolute double tolerance allows intermediate arithmetic differences; it does not independently calibrate the legacy matrix. Frame tests reuse the independent [output-native fixtures](../../../tools/generate_output_native_fixtures.py) and assert exact paint masks, emitted RGB101 values expanded to bytes, and retained bytes across all five workspaces. FS tests use retained output red `128/255`, which becomes gamma red approximately `.73664694` for linear output or `.49152858` for SMPTE-C. The next samples cross the black/red decision boundary only when the retained-color residual is propagated. Threshold witnesses place destination red at `-.0067` against retained `1/255`, between `2/255` and `3/255`, and check the returned working color with `2e-8` absolute tolerance. These small value/state tests establish coordinate and lifecycle correctness, not a multi-image quality or performance improvement.

### Fixed-table omission and ownership

Native gamma/linear sRGB, SMPTE-C or Display P3 `-b` and `-m` use a separate omission mask, preserving palette order, duplicates, unused entries and the complete 256-entry capacity. Index zero is an internal placeholder for an omitted sample; it remains an ordinary lookup candidate and is painted wherever the mask is clear. The table has no reserved keycolor. Source-alpha holes and delta keeps are combined in the existing dither-owned delta result mask. Palette-free `-e` is rejected with delta because it cannot paint the background to erase an earlier foreground pixel. Its actual foreground RGB is also receiver-dependent; neither that value nor a background repaint may be inferred from the lookup table.

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
| D6-40 | P3 OR delta rejects before output; the same encoder can recover with ordinary RGB serialization. | [tests/quant/palette/delta_native/0016_delta_p3_or_recovery.t](../../../tests/quant/palette/delta_native/0016_delta_p3_or_recovery.t) |
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
| D6-68 | A byte-eligible fixed table keeps its byte route and repaint behavior while recording wire-rounded history. | [tests/quant/palette/delta_native/0044_delta_builtin_byte.t](../../../tests/quant/palette/delta_native/0044_delta_builtin_byte.t) |
| D6-69 | A byte-eligible map keeps its byte route and repaint behavior while recording wire-rounded history. | [tests/quant/palette/delta_native/0045_delta_map_byte.t](../../../tests/quant/palette/delta_native/0045_delta_map_byte.t) |
| D6-70 | Native/byte/native transitions preserve valid wire history without explicit invalidation. | [tests/quant/palette/delta_native/0046_delta_builtin_reuse.t](../../../tests/quant/palette/delta_native/0046_delta_builtin_reuse.t) |
| D6-71 | Mapfile application transitions retain independent source ownership and wire history without invalidation. | [tests/quant/palette/delta_native/0047_delta_map_reuse.t](../../../tests/quant/palette/delta_native/0047_delta_map_reuse.t) |
| D6-72 | Retaining a color outside the new fixed table diffuses its residual and paints black index zero next to an omission. | [tests/quant/palette/delta_native/0048_delta_fixed_fs_diffuse.t](../../../tests/quant/palette/delta_native/0048_delta_fixed_fs_diffuse.t) |
| D6-73 | Skipping the retained-color residual paints white at the following sample. | [tests/quant/palette/delta_native/0049_delta_fixed_fs_skip.t](../../../tests/quant/palette/delta_native/0049_delta_fixed_fs_skip.t) |
| D6-74 | Mask allocation failure, disabled capture and missing history expose no stale active mask; retry and same-size reuse succeed. | [tests/quant/palette/delta_native/0050_delta_fixed_mask_failure.t](../../../tests/quant/palette/delta_native/0050_delta_fixed_mask_failure.t) |
| D6-75 | Image-map construction retains all 256 grayscale colors independently of active display history. | [tests/quant/palette/delta_native/0051_delta_image_map_capacity.t](../../../tests/quant/palette/delta_native/0051_delta_image_map_capacity.t) |
| D6-76 | Byte built-in tables record exact emitted RGB across parallel bands. | [tests/quant/palette/delta_native/0052_delta_byte_workers.t](../../../tests/quant/palette/delta_native/0052_delta_byte_workers.t) |
| D6-77 | Byte mapfile tables record the same wire-rounded parallel history. | [tests/quant/palette/delta_native/0053_delta_byte_map_workers.t](../../../tests/quant/palette/delta_native/0053_delta_byte_map_workers.t) |
| D6-78 | Capture follows float-backed percentage serialization and preserves the original palette. | [tests/quant/palette/delta_native/0054_delta_wire_float_palette.t](../../../tests/quant/palette/delta_native/0054_delta_wire_float_palette.t) |
| D6-79 | Disabled RGB capture creates no retained-RGB allocation and does not alter wire output. | [tests/quant/palette/delta_native/0055_delta_wire_disabled.t](../../../tests/quant/palette/delta_native/0055_delta_wire_disabled.t) |
| D6-80 | Generated byte palettes record the independently specified RGB101 expansion. | [tests/quant/palette/delta_native/0056_delta_wire_generated.t](../../../tests/quant/palette/delta_native/0056_delta_wire_generated.t) |
| D6-81 | Generated byte palettes use that same capture rule in the producer/writer pipeline. | [tests/quant/palette/delta_native/0057_delta_wire_generated_workers.t](../../../tests/quant/palette/delta_native/0057_delta_wire_generated_workers.t) |
| D6-82 | Byte none retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0058_delta_byte_none_keep.t](../../../tests/quant/palette/delta_native/0058_delta_byte_none_keep.t) |
| D6-83 | Byte fs retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0059_delta_byte_fs_keep.t](../../../tests/quant/palette/delta_native/0059_delta_byte_fs_keep.t) |
| D6-84 | Byte atkinson retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0060_delta_byte_atkinson_keep.t](../../../tests/quant/palette/delta_native/0060_delta_byte_atkinson_keep.t) |
| D6-85 | Byte lso2 retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0061_delta_byte_lso2_keep.t](../../../tests/quant/palette/delta_native/0061_delta_byte_lso2_keep.t) |
| D6-86 | Byte jajuni retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0062_delta_byte_jajuni_keep.t](../../../tests/quant/palette/delta_native/0062_delta_byte_jajuni_keep.t) |
| D6-87 | Byte stucki retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0063_delta_byte_stucki_keep.t](../../../tests/quant/palette/delta_native/0063_delta_byte_stucki_keep.t) |
| D6-88 | Byte burkes retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0064_delta_byte_burkes_keep.t](../../../tests/quant/palette/delta_native/0064_delta_byte_burkes_keep.t) |
| D6-89 | Byte sierra1 retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0065_delta_byte_sierra1_keep.t](../../../tests/quant/palette/delta_native/0065_delta_byte_sierra1_keep.t) |
| D6-90 | Byte sierra2 retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0066_delta_byte_sierra2_keep.t](../../../tests/quant/palette/delta_native/0066_delta_byte_sierra2_keep.t) |
| D6-91 | Byte sierra3 retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0067_delta_byte_sierra3_keep.t](../../../tests/quant/palette/delta_native/0067_delta_byte_sierra3_keep.t) |
| D6-92 | Byte a-dither retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0068_delta_byte_a_dither_keep.t](../../../tests/quant/palette/delta_native/0068_delta_byte_a_dither_keep.t) |
| D6-93 | Byte x-dither retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0069_delta_byte_x_dither_keep.t](../../../tests/quant/palette/delta_native/0069_delta_byte_x_dither_keep.t) |
| D6-94 | Byte bluenoise-dither retains the selected wire-color tie without changing the working palette. | [tests/quant/palette/delta_native/0070_delta_byte_bluenoise_dither_keep.t](../../../tests/quant/palette/delta_native/0070_delta_byte_bluenoise_dither_keep.t) |
| D6-95 | Byte comparison uses exact emitted candidates, preserves the query, rejects worse history and leaves standalone comparison unchanged. | [tests/quant/palette/delta_native/0071_delta_byte_compare.t](../../../tests/quant/palette/delta_native/0071_delta_byte_compare.t) |
| D6-96 | RGB/HLS byte syntax reuse preserves projected keeps; failure recovery clears the call hints. | [tests/quant/palette/delta_native/0072_delta_byte_syntax_reuse.t](../../../tests/quant/palette/delta_native/0072_delta_byte_syntax_reuse.t) |
| D6-97 | The or boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/quant/palette/delta_native/0073_delta_byte_scope_or.t](../../../tests/quant/palette/delta_native/0073_delta_byte_scope_or.t) |
| D6-98 | The body boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/quant/palette/delta_native/0074_delta_byte_scope_body.t](../../../tests/quant/palette/delta_native/0074_delta_byte_scope_body.t) |
| D6-99 | The source boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/quant/palette/delta_native/0075_delta_byte_scope_source.t](../../../tests/quant/palette/delta_native/0075_delta_byte_scope_source.t) |
| D6-100 | The output boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/quant/palette/delta_native/0076_delta_byte_scope_output.t](../../../tests/quant/palette/delta_native/0076_delta_byte_scope_output.t) |
| D6-101 | The float boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/quant/palette/delta_native/0077_delta_byte_scope_float.t](../../../tests/quant/palette/delta_native/0077_delta_byte_scope_float.t) |
| D6-102 | Generated byte frames paint, omit an identical frame, and repaint changed black content in serial output. | [tests/quant/palette/delta_native/0078_delta_byte_repeat.t](../../../tests/quant/palette/delta_native/0078_delta_byte_repeat.t) |
| D6-103 | Generated byte frames paint, omit an identical frame, and repaint changed black content through actual parallel serialization. | [tests/quant/palette/delta_native/0079_delta_byte_repeat_workers.t](../../../tests/quant/palette/delta_native/0079_delta_byte_repeat_workers.t) |
| D6-104 | Every byte candidate maps to the exact RGB101 expansion, checked by an independent nearest-grid enumeration and both comparison endpoints. | [tests/quant/palette/delta_native/0080_delta_byte_grid.t](../../../tests/quant/palette/delta_native/0080_delta_byte_grid.t) |
| D6-105 | Linear workspace keeps extended centroids and the paint/keep/invalidate/change lifecycle. | [tests/quant/palette/delta_native/0081_delta_linear_frames.t](../../../tests/quant/palette/delta_native/0081_delta_linear_frames.t) |
| D6-106 | Oklab FS retains the same wire-history lifecycle and unrounded chromatic centroids. | [tests/quant/palette/delta_native/0082_delta_oklab_frames.t](../../../tests/quant/palette/delta_native/0082_delta_oklab_frames.t) |
| D6-107 | CIELAB LSO2 retains the same wire-history lifecycle and unrounded chromatic centroids. | [tests/quant/palette/delta_native/0083_delta_cielab_frames.t](../../../tests/quant/palette/delta_native/0083_delta_cielab_frames.t) |
| D6-108 | DIN99d positional delta preserves extended centroids and the paint/keep/invalidate/change lifecycle. | [tests/quant/palette/delta_native/0084_delta_din99d_frames.t](../../../tests/quant/palette/delta_native/0084_delta_din99d_frames.t) |
| D6-109 | Linear FS transforms retained colors before diffusing their residual. | [tests/quant/palette/delta_native/0085_delta_linear_diffuse.t](../../../tests/quant/palette/delta_native/0085_delta_linear_diffuse.t) |
| D6-110 | Linear FS skip suppresses the retained-color residual. | [tests/quant/palette/delta_native/0086_delta_linear_skip.t](../../../tests/quant/palette/delta_native/0086_delta_linear_skip.t) |
| D6-111 | Linear working values use gamma comparison and threshold units; retained residuals use linear coordinates. | [tests/quant/palette/delta_native/0087_delta_linear_compare.t](../../../tests/quant/palette/delta_native/0087_delta_linear_compare.t) |
| D6-112 | Oklab working values use gamma comparison and threshold units; retained residuals use Oklab coordinates. | [tests/quant/palette/delta_native/0088_delta_oklab_compare.t](../../../tests/quant/palette/delta_native/0088_delta_oklab_compare.t) |
| D6-113 | CIELAB working values use gamma comparison and threshold units; retained residuals use CIELAB coordinates. | [tests/quant/palette/delta_native/0089_delta_cielab_compare.t](../../../tests/quant/palette/delta_native/0089_delta_cielab_compare.t) |
| D6-114 | DIN99d working values use gamma comparison and threshold units; retained residuals use DIN99d coordinates. | [tests/quant/palette/delta_native/0090_delta_din99d_compare.t](../../../tests/quant/palette/delta_native/0090_delta_din99d_compare.t) |
| D6-115 | A negative linear query remains extended through the gamma threshold comparison. | [tests/quant/palette/delta_native/0091_delta_linear_extended.t](../../../tests/quant/palette/delta_native/0091_delta_linear_extended.t) |
| D6-116 | Fixed-palette workspace changes preserve history and distinguish painted black index zero from omission. | [tests/quant/palette/delta_native/0092_delta_workspace_fixed.t](../../../tests/quant/palette/delta_native/0092_delta_workspace_fixed.t) |
| D6-117 | The same fixed-palette workspace changes preserve omission through actual parallel output. | [tests/quant/palette/delta_native/0093_delta_workspace_workers.t](../../../tests/quant/palette/delta_native/0093_delta_workspace_workers.t) |
| D6-118 | CIELAB generated-palette delta preserves source-mask holes and history validity. | [tests/quant/palette/delta_native/0094_delta_workspace_mask.t](../../../tests/quant/palette/delta_native/0094_delta_workspace_mask.t) |
| D6-119 | DIN99d delta respects plane origins and explicit invalidation. | [tests/quant/palette/delta_native/0095_delta_workspace_origin.t](../../../tests/quant/palette/delta_native/0095_delta_workspace_origin.t) |
| D6-120 | Linear generated delta preserves partial bands through actual worker output. | [tests/quant/palette/delta_native/0096_delta_workspace_bands.t](../../../tests/quant/palette/delta_native/0096_delta_workspace_bands.t) |
| D6-121 | A nonrepresentable DIN99d inverse declines the keep without recording a result bit. | [tests/quant/palette/delta_native/0097_delta_workspace_domain.t](../../../tests/quant/palette/delta_native/0097_delta_workspace_domain.t) |
| D6-122 | Legacy promoted-byte delta does not acquire transformed-workspace semantics. | [tests/quant/palette/delta_native/0098_delta_workspace_legacy.t](../../../tests/quant/palette/delta_native/0098_delta_workspace_legacy.t) |
| D6-123 | A virtual DIN99d `(0,1,-1)` query rejects pale retained `(247,245,209)` in favor of black despite the inverse gamma distance. | [tests/quant/palette/delta_native/0099_delta_virtual_guard.t](../../../tests/quant/palette/delta_native/0099_delta_virtual_guard.t) |
| D6-124 | FS applies the same virtual-color guard and emits black instead of the keep index. | [tests/quant/palette/delta_native/0100_delta_virtual_fs.t](../../../tests/quant/palette/delta_native/0100_delta_virtual_fs.t) |
| D6-125 | Independently derived P3/sRGB gamma transforms preserve extended coordinates. | [tests/quant/palette/delta_native/0101_delta_p3_gamma_transform.t](../../../tests/quant/palette/delta_native/0101_delta_p3_gamma_transform.t) |
| D6-126 | Independent P3/linear transforms preserve negative and over-one components. | [tests/quant/palette/delta_native/0102_delta_p3_linear_transform.t](../../../tests/quant/palette/delta_native/0102_delta_p3_linear_transform.t) |
| D6-127 | Independent P3/Oklab transforms match normalized coordinates. | [tests/quant/palette/delta_native/0103_delta_p3_oklab_transform.t](../../../tests/quant/palette/delta_native/0103_delta_p3_oklab_transform.t) |
| D6-128 | Independent P3/CIELAB transforms match normalized coordinates. | [tests/quant/palette/delta_native/0104_delta_p3_cielab_transform.t](../../../tests/quant/palette/delta_native/0104_delta_p3_cielab_transform.t) |
| D6-129 | Independent P3/DIN99d transforms match normalized coordinates. | [tests/quant/palette/delta_native/0105_delta_p3_din99d_transform.t](../../../tests/quant/palette/delta_native/0105_delta_p3_din99d_transform.t) |
| D6-130 | P3 output threshold uses device coordinates without clipping the query. | [tests/quant/palette/delta_native/0106_delta_p3_threshold.t](../../../tests/quant/palette/delta_native/0106_delta_p3_threshold.t) |
| D6-131 | FS diffuses the retained P3 color after converting to linear working units. | [tests/quant/palette/delta_native/0107_delta_p3_diffuse.t](../../../tests/quant/palette/delta_native/0107_delta_p3_diffuse.t) |
| D6-132 | Skip omits the retained P3 residual while ordinary selection remains active. | [tests/quant/palette/delta_native/0108_delta_p3_skip.t](../../../tests/quant/palette/delta_native/0108_delta_p3_skip.t) |
| D6-133 | Prepared P3 definition ownership and invalidation are local to each palette. | [tests/quant/palette/delta_native/0109_delta_p3_cache_lifetime.t](../../../tests/quant/palette/delta_native/0109_delta_p3_cache_lifetime.t) |
| D6-134 | Invalid P3 transforms and rejected preparation preserve published output. | [tests/quant/palette/delta_native/0110_delta_p3_cache_failure.t](../../../tests/quant/palette/delta_native/0110_delta_p3_cache_failure.t) |
| D6-135 | Gamma P3 frames paint, retain, change and invalidate independent RGB values. | [tests/quant/palette/delta_native/0111_delta_p3_gamma_frames.t](../../../tests/quant/palette/delta_native/0111_delta_p3_gamma_frames.t) |
| D6-136 | Linear P3 frames paint, retain, change and invalidate independent RGB values. | [tests/quant/palette/delta_native/0112_delta_p3_linear_frames.t](../../../tests/quant/palette/delta_native/0112_delta_p3_linear_frames.t) |
| D6-137 | Oklab P3 frames paint, retain, change and invalidate independent RGB values. | [tests/quant/palette/delta_native/0113_delta_p3_oklab_frames.t](../../../tests/quant/palette/delta_native/0113_delta_p3_oklab_frames.t) |
| D6-138 | CIELAB P3 frames paint, retain, change and invalidate independent RGB values. | [tests/quant/palette/delta_native/0114_delta_p3_cielab_frames.t](../../../tests/quant/palette/delta_native/0114_delta_p3_cielab_frames.t) |
| D6-139 | DIN99d P3 frames paint, retain, change and invalidate independent RGB values. | [tests/quant/palette/delta_native/0115_delta_p3_din99d_frames.t](../../../tests/quant/palette/delta_native/0115_delta_p3_din99d_frames.t) |
| D6-140 | Actual parallel P3 body dispatch preserves wire colors and retained pixels. | [tests/quant/palette/delta_native/0116_delta_p3_workers.t](../../../tests/quant/palette/delta_native/0116_delta_p3_workers.t) |
| D6-141 | sRGB/P3 changes repaint identical numeric black; unchanged and invalid settings preserve history. | [tests/quant/palette/delta_native/0117_delta_p3_definition_switch.t](../../../tests/quant/palette/delta_native/0117_delta_p3_definition_switch.t) |
| D6-142 | An explicitly seeded P3 plane retains pixels and follows output-identity changes. | [tests/quant/palette/delta_native/0118_delta_p3_seed_definition.t](../../../tests/quant/palette/delta_native/0118_delta_p3_seed_definition.t) |
| D6-143 | Independent linear output inverse preserves extended coordinates. | [tests/quant/palette/delta_native/0119_delta_linear_output_inverse.t](../../../tests/quant/palette/delta_native/0119_delta_linear_output_inverse.t) |
| D6-144 | linear output thresholds use destination units and return working residuals. | [tests/quant/palette/delta_native/0120_delta_linear_output_threshold.t](../../../tests/quant/palette/delta_native/0120_delta_linear_output_threshold.t) |
| D6-145 | linear output FS propagates retained-color error in gamma working units. | [tests/quant/palette/delta_native/0121_delta_linear_output_diffuse.t](../../../tests/quant/palette/delta_native/0121_delta_linear_output_diffuse.t) |
| D6-146 | linear output FS skips retained-color error in gamma working units. | [tests/quant/palette/delta_native/0122_delta_linear_output_skip.t](../../../tests/quant/palette/delta_native/0122_delta_linear_output_skip.t) |
| D6-147 | linear output with gamma working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0123_delta_linear_output_gamma_frames.t](../../../tests/quant/palette/delta_native/0123_delta_linear_output_gamma_frames.t) |
| D6-148 | linear output with linear working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0124_delta_linear_output_linear_frames.t](../../../tests/quant/palette/delta_native/0124_delta_linear_output_linear_frames.t) |
| D6-149 | linear output with oklab working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0125_delta_linear_output_oklab_frames.t](../../../tests/quant/palette/delta_native/0125_delta_linear_output_oklab_frames.t) |
| D6-150 | linear output with cielab working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0126_delta_linear_output_cielab_frames.t](../../../tests/quant/palette/delta_native/0126_delta_linear_output_cielab_frames.t) |
| D6-151 | linear output with din99d working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0127_delta_linear_output_din99d_frames.t](../../../tests/quant/palette/delta_native/0127_delta_linear_output_din99d_frames.t) |
| D6-152 | linear output preserves retained colors through actual worker dispatch. | [tests/quant/palette/delta_native/0128_delta_linear_output_workers.t](../../../tests/quant/palette/delta_native/0128_delta_linear_output_workers.t) |
| D6-153 | A seeded linear surface follows transfer/definition identity across fixed-palette frames. | [tests/quant/palette/delta_native/0129_delta_linear_output_identity.t](../../../tests/quant/palette/delta_native/0129_delta_linear_output_identity.t) |
| D6-154 | Independent smptec output inverse preserves extended coordinates. | [tests/quant/palette/delta_native/0130_delta_smptec_output_inverse.t](../../../tests/quant/palette/delta_native/0130_delta_smptec_output_inverse.t) |
| D6-155 | smptec output thresholds use destination units and return working residuals. | [tests/quant/palette/delta_native/0131_delta_smptec_output_threshold.t](../../../tests/quant/palette/delta_native/0131_delta_smptec_output_threshold.t) |
| D6-156 | smptec output FS propagates retained-color error in gamma working units. | [tests/quant/palette/delta_native/0132_delta_smptec_output_diffuse.t](../../../tests/quant/palette/delta_native/0132_delta_smptec_output_diffuse.t) |
| D6-157 | smptec output FS skips retained-color error in gamma working units. | [tests/quant/palette/delta_native/0133_delta_smptec_output_skip.t](../../../tests/quant/palette/delta_native/0133_delta_smptec_output_skip.t) |
| D6-158 | smptec output with gamma working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0134_delta_smptec_output_gamma_frames.t](../../../tests/quant/palette/delta_native/0134_delta_smptec_output_gamma_frames.t) |
| D6-159 | smptec output with linear working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0135_delta_smptec_output_linear_frames.t](../../../tests/quant/palette/delta_native/0135_delta_smptec_output_linear_frames.t) |
| D6-160 | smptec output with oklab working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0136_delta_smptec_output_oklab_frames.t](../../../tests/quant/palette/delta_native/0136_delta_smptec_output_oklab_frames.t) |
| D6-161 | smptec output with cielab working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0137_delta_smptec_output_cielab_frames.t](../../../tests/quant/palette/delta_native/0137_delta_smptec_output_cielab_frames.t) |
| D6-162 | smptec output with din99d working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/quant/palette/delta_native/0138_delta_smptec_output_din99d_frames.t](../../../tests/quant/palette/delta_native/0138_delta_smptec_output_din99d_frames.t) |
| D6-163 | smptec output preserves retained colors through actual worker dispatch. | [tests/quant/palette/delta_native/0139_delta_smptec_output_workers.t](../../../tests/quant/palette/delta_native/0139_delta_smptec_output_workers.t) |
| D6-164 | A seeded smptec surface follows transfer/definition identity across fixed-palette frames. | [tests/quant/palette/delta_native/0140_delta_smptec_output_identity.t](../../../tests/quant/palette/delta_native/0140_delta_smptec_output_identity.t) |
| D6-165 | Nonfinite/overflow and unsupported output inverses leave the destination unchanged. | [tests/quant/palette/delta_native/0141_delta_output_inverse_failure.t](../../../tests/quant/palette/delta_native/0141_delta_output_inverse_failure.t) |
| D6-166 | HLS gamma output in gamma workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0142_delta_hls_gamma_gamma_frames.t](../../../tests/quant/palette/delta_native/0142_delta_hls_gamma_gamma_frames.t) |
| D6-167 | HLS gamma output in linear workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0143_delta_hls_gamma_linear_frames.t](../../../tests/quant/palette/delta_native/0143_delta_hls_gamma_linear_frames.t) |
| D6-168 | HLS gamma output in oklab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0144_delta_hls_gamma_oklab_frames.t](../../../tests/quant/palette/delta_native/0144_delta_hls_gamma_oklab_frames.t) |
| D6-169 | HLS gamma output in cielab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0145_delta_hls_gamma_cielab_frames.t](../../../tests/quant/palette/delta_native/0145_delta_hls_gamma_cielab_frames.t) |
| D6-170 | HLS gamma output in din99d workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0146_delta_hls_gamma_din99d_frames.t](../../../tests/quant/palette/delta_native/0146_delta_hls_gamma_din99d_frames.t) |
| D6-171 | HLS linear output in gamma workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0147_delta_hls_linear_gamma_frames.t](../../../tests/quant/palette/delta_native/0147_delta_hls_linear_gamma_frames.t) |
| D6-172 | HLS linear output in linear workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0148_delta_hls_linear_linear_frames.t](../../../tests/quant/palette/delta_native/0148_delta_hls_linear_linear_frames.t) |
| D6-173 | HLS linear output in oklab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0149_delta_hls_linear_oklab_frames.t](../../../tests/quant/palette/delta_native/0149_delta_hls_linear_oklab_frames.t) |
| D6-174 | HLS linear output in cielab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0150_delta_hls_linear_cielab_frames.t](../../../tests/quant/palette/delta_native/0150_delta_hls_linear_cielab_frames.t) |
| D6-175 | HLS linear output in din99d workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0151_delta_hls_linear_din99d_frames.t](../../../tests/quant/palette/delta_native/0151_delta_hls_linear_din99d_frames.t) |
| D6-176 | HLS smptec output in gamma workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0152_delta_hls_smptec_gamma_frames.t](../../../tests/quant/palette/delta_native/0152_delta_hls_smptec_gamma_frames.t) |
| D6-177 | HLS smptec output in linear workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0153_delta_hls_smptec_linear_frames.t](../../../tests/quant/palette/delta_native/0153_delta_hls_smptec_linear_frames.t) |
| D6-178 | HLS smptec output in oklab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0154_delta_hls_smptec_oklab_frames.t](../../../tests/quant/palette/delta_native/0154_delta_hls_smptec_oklab_frames.t) |
| D6-179 | HLS smptec output in cielab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0155_delta_hls_smptec_cielab_frames.t](../../../tests/quant/palette/delta_native/0155_delta_hls_smptec_cielab_frames.t) |
| D6-180 | HLS smptec output in din99d workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0156_delta_hls_smptec_din99d_frames.t](../../../tests/quant/palette/delta_native/0156_delta_hls_smptec_din99d_frames.t) |
| D6-181 | HLS p3 output in gamma workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0157_delta_hls_p3_gamma_frames.t](../../../tests/quant/palette/delta_native/0157_delta_hls_p3_gamma_frames.t) |
| D6-182 | HLS p3 output in linear workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0158_delta_hls_p3_linear_frames.t](../../../tests/quant/palette/delta_native/0158_delta_hls_p3_linear_frames.t) |
| D6-183 | HLS p3 output in oklab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0159_delta_hls_p3_oklab_frames.t](../../../tests/quant/palette/delta_native/0159_delta_hls_p3_oklab_frames.t) |
| D6-184 | HLS p3 output in cielab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0160_delta_hls_p3_cielab_frames.t](../../../tests/quant/palette/delta_native/0160_delta_hls_p3_cielab_frames.t) |
| D6-185 | HLS p3 output in din99d workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/quant/palette/delta_native/0161_delta_hls_p3_din99d_frames.t](../../../tests/quant/palette/delta_native/0161_delta_hls_p3_din99d_frames.t) |
| D6-186 | HLS P3 delta preserves emitted colors through actual worker dispatch. | [tests/quant/palette/delta_native/0162_delta_hls_workers.t](../../../tests/quant/palette/delta_native/0162_delta_hls_workers.t) |
| D6-187 | HLS candidate keep uses decoded HLS RGB rather than directly rounded RGB percentages. | [tests/quant/palette/delta_native/0163_delta_hls_candidate_keep.t](../../../tests/quant/palette/delta_native/0163_delta_hls_candidate_keep.t) |
| D6-188 | HLS candidate repaint uses decoded HLS RGB rather than directly rounded RGB percentages. | [tests/quant/palette/delta_native/0164_delta_hls_candidate_repaint.t](../../../tests/quant/palette/delta_native/0164_delta_hls_candidate_repaint.t) |
| D6-189 | HLS derived palette cache isolation preserves publication and ownership boundaries. | [tests/quant/palette/delta_native/0165_delta_hls_cache_isolation.t](../../../tests/quant/palette/delta_native/0165_delta_hls_cache_isolation.t) |
| D6-190 | HLS derived palette cache invalidation preserves publication and ownership boundaries. | [tests/quant/palette/delta_native/0166_delta_hls_cache_invalidation.t](../../../tests/quant/palette/delta_native/0166_delta_hls_cache_invalidation.t) |
| D6-191 | HLS fixed palettes preserve all 256 slots and mask state across frames. | [tests/quant/palette/delta_native/0167_delta_hls_fixed_mask.t](../../../tests/quant/palette/delta_native/0167_delta_hls_fixed_mask.t) |
| D6-192 | HLS fixed palettes preserve all 256 slots and origin state across frames. | [tests/quant/palette/delta_native/0168_delta_hls_fixed_origin.t](../../../tests/quant/palette/delta_native/0168_delta_hls_fixed_origin.t) |
| D6-193 | HLS fixed palettes preserve all 256 slots and reuse state across frames. | [tests/quant/palette/delta_native/0169_delta_hls_fixed_reuse.t](../../../tests/quant/palette/delta_native/0169_delta_hls_fixed_reuse.t) |
| D6-194 | HLS kept-color diffuse uses the retained displayed gray in FS diffusion. | [tests/quant/palette/delta_native/0170_delta_hls_residual_diffuse.t](../../../tests/quant/palette/delta_native/0170_delta_hls_residual_diffuse.t) |
| D6-195 | HLS kept-color skip uses the retained displayed gray in FS diffusion. | [tests/quant/palette/delta_native/0171_delta_hls_residual_skip.t](../../../tests/quant/palette/delta_native/0171_delta_hls_residual_skip.t) |
| D6-196 | RGB and HLS syntax changes preserve compatible history and compare their distinct emitted colors. | [tests/quant/palette/delta_native/0172_delta_hls_syntax_reuse.t](../../../tests/quant/palette/delta_native/0172_delta_hls_syntax_reuse.t) |
| D6-197 | HLS encode failure and successful reuse clear the call-scoped comparison hint. | [tests/quant/palette/delta_native/0173_delta_hls_failure_recovery.t](../../../tests/quant/palette/delta_native/0173_delta_hls_failure_recovery.t) |
| D6-198 | Six chromatic sectors, gray and a double-rounding witness match independent HLS fields and decoded percentages. | [tests/quant/palette/delta_native/0174_delta_hls_field_model.t](../../../tests/quant/palette/delta_native/0174_delta_hls_field_model.t) |
| D6-199 | Byte HLS none captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0175_delta_byte_hls_none.t](../../../tests/quant/palette/delta_native/0175_delta_byte_hls_none.t) |
| D6-200 | Byte HLS fs captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0176_delta_byte_hls_fs.t](../../../tests/quant/palette/delta_native/0176_delta_byte_hls_fs.t) |
| D6-201 | Byte HLS atkinson captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0177_delta_byte_hls_atkinson.t](../../../tests/quant/palette/delta_native/0177_delta_byte_hls_atkinson.t) |
| D6-202 | Byte HLS jajuni captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0178_delta_byte_hls_jajuni.t](../../../tests/quant/palette/delta_native/0178_delta_byte_hls_jajuni.t) |
| D6-203 | Byte HLS stucki captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0179_delta_byte_hls_stucki.t](../../../tests/quant/palette/delta_native/0179_delta_byte_hls_stucki.t) |
| D6-204 | Byte HLS burkes captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0180_delta_byte_hls_burkes.t](../../../tests/quant/palette/delta_native/0180_delta_byte_hls_burkes.t) |
| D6-205 | Byte HLS sierra1 captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0181_delta_byte_hls_sierra1.t](../../../tests/quant/palette/delta_native/0181_delta_byte_hls_sierra1.t) |
| D6-206 | Byte HLS sierra2 captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0182_delta_byte_hls_sierra2.t](../../../tests/quant/palette/delta_native/0182_delta_byte_hls_sierra2.t) |
| D6-207 | Byte HLS sierra3 captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0183_delta_byte_hls_sierra3.t](../../../tests/quant/palette/delta_native/0183_delta_byte_hls_sierra3.t) |
| D6-208 | Byte HLS lso2 captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0184_delta_byte_hls_lso2.t](../../../tests/quant/palette/delta_native/0184_delta_byte_hls_lso2.t) |
| D6-209 | Byte HLS a-dither captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0185_delta_byte_hls_a_dither.t](../../../tests/quant/palette/delta_native/0185_delta_byte_hls_a_dither.t) |
| D6-210 | Byte HLS x-dither captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0186_delta_byte_hls_x_dither.t](../../../tests/quant/palette/delta_native/0186_delta_byte_hls_x_dither.t) |
| D6-211 | Byte HLS bluenoise captures and compares independent emitted colors without float promotion. | [tests/quant/palette/delta_native/0187_delta_byte_hls_bluenoise.t](../../../tests/quant/palette/delta_native/0187_delta_byte_hls_bluenoise.t) |
| D6-212 | Byte HLS candidate keep compares the indexed HLS reconstruction rather than pre-wire RGB. | [tests/quant/palette/delta_native/0188_delta_byte_hls_candidate_keep.t](../../../tests/quant/palette/delta_native/0188_delta_byte_hls_candidate_keep.t) |
| D6-213 | Byte HLS candidate repaint compares the indexed HLS reconstruction rather than pre-wire RGB. | [tests/quant/palette/delta_native/0189_delta_byte_hls_candidate_repaint.t](../../../tests/quant/palette/delta_native/0189_delta_byte_hls_candidate_repaint.t) |
| D6-214 | Generated HLS frames preserves exact painted history, compatible keeps and byte eligibility. | [tests/quant/palette/delta_native/0190_delta_byte_hls_frames.t](../../../tests/quant/palette/delta_native/0190_delta_byte_hls_frames.t) |
| D6-215 | Generated HLS workers preserves exact painted history, compatible keeps and byte eligibility. | [tests/quant/palette/delta_native/0191_delta_byte_hls_workers.t](../../../tests/quant/palette/delta_native/0191_delta_byte_hls_workers.t) |
| D6-216 | Generated HLS byte-float preserves exact painted history, compatible keeps and byte eligibility. | [tests/quant/palette/delta_native/0192_delta_byte_hls_byte_float.t](../../../tests/quant/palette/delta_native/0192_delta_byte_hls_byte_float.t) |
| D6-217 | Generated HLS float-byte preserves exact painted history, compatible keeps and byte eligibility. | [tests/quant/palette/delta_native/0193_delta_byte_hls_float_byte.t](../../../tests/quant/palette/delta_native/0193_delta_byte_hls_float_byte.t) |
| D6-218 | Generated HLS mask preserves exact painted history, compatible keeps and byte eligibility. | [tests/quant/palette/delta_native/0194_delta_byte_hls_mask.t](../../../tests/quant/palette/delta_native/0194_delta_byte_hls_mask.t) |
| D6-219 | Generated HLS origin preserves exact painted history, compatible keeps and byte eligibility. | [tests/quant/palette/delta_native/0195_delta_byte_hls_origin.t](../../../tests/quant/palette/delta_native/0195_delta_byte_hls_origin.t) |
| D6-220 | Generated HLS invalidation preserves exact painted history, compatible keeps and byte eligibility. | [tests/quant/palette/delta_native/0196_delta_byte_hls_invalidation.t](../../../tests/quant/palette/delta_native/0196_delta_byte_hls_invalidation.t) |
| D6-221 | Byte HLS failure clears the borrowed comparison view before reuse. | [tests/quant/palette/delta_native/0197_delta_byte_hls_failure.t](../../../tests/quant/palette/delta_native/0197_delta_byte_hls_failure.t) |
| D6-222 | Independent byte HLS contexts rebuild comparison views after palette changes. | [tests/quant/palette/delta_native/0198_delta_byte_hls_instances.t](../../../tests/quant/palette/delta_native/0198_delta_byte_hls_instances.t) |
| D6-223 | Explicit exact snap is reflected in byte HLS candidate comparison. | [tests/quant/palette/delta_native/0199_delta_byte_hls_snap.t](../../../tests/quant/palette/delta_native/0199_delta_byte_hls_snap.t) |
| D6-224 | Legacy indexed HLS history follows the final float palette rather than its byte shadow. | [tests/quant/palette/delta_native/0200_delta_byte_hls_float_palette.t](../../../tests/quant/palette/delta_native/0200_delta_byte_hls_float_palette.t) |
| D6-225 | Selecting delta after monochrome rejects without changing options or retained history. | [tests/quant/palette/delta_native/0201_delta_mono_then_delta.t](../../../tests/quant/palette/delta_native/0201_delta_mono_then_delta.t) |
| D6-226 | Selecting monochrome after delta rejects without changing options or retained history. | [tests/quant/palette/delta_native/0202_delta_delta_then_mono.t](../../../tests/quant/palette/delta_native/0202_delta_delta_then_mono.t) |
| D6-227 | Environment-selected delta also rejects monochrome. | [tests/quant/palette/delta_native/0203_delta_mono_environment.t](../../../tests/quant/palette/delta_native/0203_delta_mono_environment.t) |
| D6-228 | Selecting full before monochrome clears delta history and allows monochrome. | [tests/quant/palette/delta_native/0204_delta_mono_full_override.t](../../../tests/quant/palette/delta_native/0204_delta_mono_full_override.t) |
| D6-229 | Final output validation rejects monochrome delta before writing and permits full-output reuse. | [tests/quant/palette/delta_native/0205_delta_mono_output_reject.t](../../../tests/quant/palette/delta_native/0205_delta_mono_output_reject.t) |
| D6-230 | Opaque black and white overwrite each other in delta output; a repeated frame preserves both pixels. | [tests/quant/palette/delta_native/0206_delta_opaque_bw_frames.t](../../../tests/quant/palette/delta_native/0206_delta_opaque_bw_frames.t) |
| D6-231 | Selecting delta after OR rejects without changing options or retained history. | [tests/quant/palette/delta_native/0207_delta_or_then_delta.t](../../../tests/quant/palette/delta_native/0207_delta_or_then_delta.t) |
| D6-232 | Selecting OR after delta rejects without changing options or retained history. | [tests/quant/palette/delta_native/0208_delta_delta_then_or.t](../../../tests/quant/palette/delta_native/0208_delta_delta_then_or.t) |
| D6-233 | Environment-selected delta also rejects OR output. | [tests/quant/palette/delta_native/0209_delta_or_environment.t](../../../tests/quant/palette/delta_native/0209_delta_or_environment.t) |
| D6-234 | Selecting full before OR clears delta history and permits OR output. | [tests/quant/palette/delta_native/0210_delta_or_full_override.t](../../../tests/quant/palette/delta_native/0210_delta_or_full_override.t) |
| D6-235 | Byte OR delta rejects before output; full-output reuse paints both black and white endpoints. | [tests/quant/palette/delta_native/0211_delta_or_byte_recovery.t](../../../tests/quant/palette/delta_native/0211_delta_or_byte_recovery.t) |
| D6-236 | Float OR delta rejects before output; full-output reuse preserves native float application and opaque endpoints. | [tests/quant/palette/delta_native/0212_delta_or_float_recovery.t](../../../tests/quant/palette/delta_native/0212_delta_or_float_recovery.t) |
| D6-237 | P3 OR delta rejects before output; full-output reuse preserves native P3 application and opaque endpoints. | [tests/quant/palette/delta_native/0213_delta_or_p3_recovery.t](../../../tests/quant/palette/delta_native/0213_delta_or_p3_recovery.t) |
| D6-238 | Selecting delta after DRCS rejects without changing options or retained history. | [tests/quant/palette/delta_native/0214_delta_drcs_then_delta.t](../../../tests/quant/palette/delta_native/0214_delta_drcs_then_delta.t) |
| D6-239 | Selecting DRCS after delta rejects before glyph settings or tile routing change. | [tests/quant/palette/delta_native/0215_delta_delta_then_drcs.t](../../../tests/quant/palette/delta_native/0215_delta_delta_then_drcs.t) |
| D6-240 | Environment-selected delta also rejects DRCS without changing options. | [tests/quant/palette/delta_native/0216_delta_drcs_environment.t](../../../tests/quant/palette/delta_native/0216_delta_drcs_environment.t) |
| D6-241 | Selecting full before DRCS clears delta history and permits glyph definitions. | [tests/quant/palette/delta_native/0217_delta_drcs_full_override.t](../../../tests/quant/palette/delta_native/0217_delta_drcs_full_override.t) |
| D6-242 | A rejected DRCS selection leaves the existing character-output file intact. | [tests/quant/palette/delta_native/0218_delta_drcs_tile_untouched.t](../../../tests/quant/palette/delta_native/0218_delta_drcs_tile_untouched.t) |
| D6-243 | Byte DRCS delta rejects before palette work and output; full glyph reuse preserves opaque endpoint values. | [tests/quant/palette/delta_native/0219_delta_drcs_byte_recovery.t](../../../tests/quant/palette/delta_native/0219_delta_drcs_byte_recovery.t) |
| D6-244 | Float DRCS delta rejects before palette work and output; full reuse retains native float application. | [tests/quant/palette/delta_native/0220_delta_drcs_float_recovery.t](../../../tests/quant/palette/delta_native/0220_delta_drcs_float_recovery.t) |
| D6-245 | P3 DRCS delta rejects before output; full reuse retains native P3 application and endpoint values. | [tests/quant/palette/delta_native/0221_delta_drcs_p3_recovery.t](../../../tests/quant/palette/delta_native/0221_delta_drcs_p3_recovery.t) |
| D6-246 | Selecting delta after automatic macro output preserves options and history on rejection. | [tests/quant/palette/delta_native/0222_delta_macro_use_then_delta.t](../../../tests/quant/palette/delta_native/0222_delta_macro_use_then_delta.t) |
| D6-247 | Selecting automatic macro output after delta rejects before changing selection. | [tests/quant/palette/delta_native/0223_delta_delta_then_macro_use.t](../../../tests/quant/palette/delta_native/0223_delta_delta_then_macro_use.t) |
| D6-248 | Selecting delta after a fixed-ID macro preserves options and history on rejection. | [tests/quant/palette/delta_native/0224_delta_macro_number_then_delta.t](../../../tests/quant/palette/delta_native/0224_delta_macro_number_then_delta.t) |
| D6-249 | Selecting a fixed macro ID after delta rejects without installing the ID. | [tests/quant/palette/delta_native/0225_delta_delta_then_macro_number.t](../../../tests/quant/palette/delta_native/0225_delta_delta_then_macro_number.t) |
| D6-250 | Environment-selected delta rejects automatic macro output. | [tests/quant/palette/delta_native/0226_delta_macro_use_environment.t](../../../tests/quant/palette/delta_native/0226_delta_macro_use_environment.t) |
| D6-251 | Environment-selected delta rejects fixed-ID macro output. | [tests/quant/palette/delta_native/0227_delta_macro_number_environment.t](../../../tests/quant/palette/delta_native/0227_delta_macro_number_environment.t) |
| D6-252 | Explicit full output clears delta history and allows automatic macros. | [tests/quant/palette/delta_native/0228_delta_macro_use_full_override.t](../../../tests/quant/palette/delta_native/0228_delta_macro_use_full_override.t) |
| D6-253 | Explicit full output clears delta history and allows fixed-ID macros. | [tests/quant/palette/delta_native/0229_delta_macro_number_full_override.t](../../../tests/quant/palette/delta_native/0229_delta_macro_number_full_override.t) |
| D6-254 | Byte macro delta rejects with zero output; full recovery defines and invokes the exact white pixel. | [tests/quant/palette/delta_native/0230_delta_macro_byte_recovery.t](../../../tests/quant/palette/delta_native/0230_delta_macro_byte_recovery.t) |
| D6-255 | Float macro delta rejects with zero output; full recovery retains native application and defines the exact white pixel. | [tests/quant/palette/delta_native/0231_delta_macro_float_recovery.t](../../../tests/quant/palette/delta_native/0231_delta_macro_float_recovery.t) |
| D6-256 | P3 macro delta rejects with zero output; full recovery retains native application and fixed-ID precedence over automatic invocation. | [tests/quant/palette/delta_native/0232_delta_macro_p3_recovery.t](../../../tests/quant/palette/delta_native/0232_delta_macro_p3_recovery.t) |
| D6-257 | Delta rejects a combined automatic and fixed-ID macro selection without changing either selector. | [tests/quant/palette/delta_native/0233_delta_macro_combined_selection.t](../../../tests/quant/palette/delta_native/0233_delta_macro_combined_selection.t) |
| D6-258 | Selecting delta after a PNG stdout target preserves destination and history on rejection. | [tests/codec/png_snapshot/0010_delta_stdout_then_delta.t](../../../tests/codec/png_snapshot/0010_delta_stdout_then_delta.t) |
| D6-259 | Selecting a PNG stdout target after delta rejects before changing output state. | [tests/codec/png_snapshot/0011_delta_delta_then_stdout.t](../../../tests/codec/png_snapshot/0011_delta_delta_then_stdout.t) |
| D6-260 | Selecting delta after a prefixed PNG path preserves the configured path and history. | [tests/codec/png_snapshot/0012_delta_prefix_then_delta.t](../../../tests/codec/png_snapshot/0012_delta_prefix_then_delta.t) |
| D6-261 | Selecting a prefixed PNG path after delta leaves destination ownership unchanged. | [tests/codec/png_snapshot/0013_delta_delta_then_prefix.t](../../../tests/codec/png_snapshot/0013_delta_delta_then_prefix.t) |
| D6-262 | Selecting delta after a PNG filename suffix preserves the configured path and history. | [tests/codec/png_snapshot/0014_delta_suffix_then_delta.t](../../../tests/codec/png_snapshot/0014_delta_suffix_then_delta.t) |
| D6-263 | Selecting a PNG filename suffix after delta leaves output state unchanged. | [tests/codec/png_snapshot/0015_delta_delta_then_suffix.t](../../../tests/codec/png_snapshot/0015_delta_delta_then_suffix.t) |
| D6-264 | Environment-selected delta rejects PNG snapshot selection. | [tests/codec/png_snapshot/0016_delta_environment.t](../../../tests/codec/png_snapshot/0016_delta_environment.t) |
| D6-265 | Explicit full clears delta history and permits PNG snapshots. | [tests/codec/png_snapshot/0017_delta_full_override.t](../../../tests/codec/png_snapshot/0017_delta_full_override.t) |
| D6-266 | Switching a PNG destination back to ordinary SIXEL permits delta. | [tests/codec/png_snapshot/0018_delta_sixel_switch.t](../../../tests/codec/png_snapshot/0018_delta_sixel_switch.t) |
| D6-267 | PNG-selected byte frames reject delta before palette work; full reuse paints exact white and black endpoints. | [tests/codec/png_snapshot/0019_delta_byte_recovery.t](../../../tests/codec/png_snapshot/0019_delta_byte_recovery.t) |
| D6-268 | PNG-selected float frames reject delta before palette work; full reuse preserves native application and exact endpoints. | [tests/codec/png_snapshot/0020_delta_float_recovery.t](../../../tests/codec/png_snapshot/0020_delta_float_recovery.t) |
| D6-269 | PNG-selected P3 frames reject delta before palette work; full reuse preserves native P3 application and exact endpoints. | [tests/codec/png_snapshot/0021_delta_p3_recovery.t](../../../tests/codec/png_snapshot/0021_delta_p3_recovery.t) |
| D6-270 | The PNG file frontend rejects delta before input/staging access and preserves its destination and history. | [tests/codec/png_snapshot/0022_delta_file_guard.t](../../../tests/codec/png_snapshot/0022_delta_file_guard.t) |
| D6-271 | The PNG stdout frontend rejects delta before input/staging access and preserves its descriptor and history. | [tests/codec/png_snapshot/0023_delta_stdout_guard.t](../../../tests/codec/png_snapshot/0023_delta_stdout_guard.t) |
| D6-272 | Forced GPU direct and Eytzinger lookup with `none` or `bluenoise` cannot select the appended omission key as a color; a black source chooses the real dark entry and leaves the keep mask clear. | [tests/processing/gpu-palette/0002_reserved_key.t](../../../tests/processing/gpu-palette/0002_reserved_key.t) |
| D6-273 | A forced GPU blue-noise white/dark-ramp/black/repeated-black sequence at threshold 8 satisfies independent per-pixel color bounds, and the retained RGB/validity exactly matches the decoded, composed canvas after every frame. The ramp requires lossy quantization, so a uniform-image shortcut cannot satisfy this check. | [tests/processing/gpu-palette/0003_fade_history.t](../../../tests/processing/gpu-palette/0003_fade_history.t) |
| D6-274 | CPU byte early keeps request refinement with zero painted pixels; a zero ceiling paints the full fixture black without changing the maximum or another encoder. | [tests/quant/palette/delta_native/0234_delta_refine_byte.t](../../../tests/quant/palette/delta_native/0234_delta_refine_byte.t) |
| D6-275 | CPU float early keeps support the same retained-history refinement contract. | [tests/quant/palette/delta_native/0235_delta_refine_float.t](../../../tests/quant/palette/delta_native/0235_delta_refine_float.t) |
| D6-276 | Forced GPU blue-noise early keeps support the same refinement contract when Metal is available. | [tests/quant/palette/delta_native/0236_delta_refine_gpu.t](../../../tests/quant/palette/delta_native/0236_delta_refine_gpu.t) |
| D6-277 | An exactly matching frame does not request refinement. | [tests/quant/palette/delta_native/0237_delta_refine_exact.t](../../../tests/quant/palette/delta_native/0237_delta_refine_exact.t) |
| D6-278 | Approximate early keeps request refinement even when another opaque pixel is painted; the painted count is one. | [tests/quant/palette/delta_native/0238_delta_refine_partial.t](../../../tests/quant/palette/delta_native/0238_delta_refine_partial.t) |
| D6-279 | Nonzero source transparency masks do not impersonate approximate delta keeps. | [tests/quant/palette/delta_native/0239_delta_refine_source_mask.t](../../../tests/quant/palette/delta_native/0239_delta_refine_source_mask.t) |

### GPU emitted-candidate comparison tests

| ID | GPU comparison contract | Test |
| --- | --- | --- |
| GW-01 | RGB direct lookup compares emitted colors, with ties, repaint and invalid history. | [tests/processing/gpu-palette/0005_wire_rgb_none.t](../../../tests/processing/gpu-palette/0005_wire_rgb_none.t) |
| GW-02 | RGB blue noise preserves the emitted-color keep comparison. | [tests/processing/gpu-palette/0006_wire_rgb_bluenoise.t](../../../tests/processing/gpu-palette/0006_wire_rgb_bluenoise.t) |
| GW-03 | Eytzinger selects from working entries and compares their emitted colors. | [tests/processing/gpu-palette/0007_wire_rgb_eytzinger.t](../../../tests/processing/gpu-palette/0007_wire_rgb_eytzinger.t) |
| GW-04 | HLS direct lookup uses the output owner's reconstructed RGB view. | [tests/processing/gpu-palette/0008_wire_hls_none.t](../../../tests/processing/gpu-palette/0008_wire_hls_none.t) |
| GW-05 | HLS blue noise uses the same reconstructed view. | [tests/processing/gpu-palette/0009_wire_hls_bluenoise.t](../../../tests/processing/gpu-palette/0009_wire_hls_bluenoise.t) |
| GW-06 | Standalone GPU application retains supplied-candidate semantics. | [tests/processing/gpu-palette/0010_comparison_default.t](../../../tests/processing/gpu-palette/0010_comparison_default.t) |
| GW-07 | A short comparison view is rejected before any GPU access. | [tests/processing/gpu-palette/0011_comparison_short.t](../../../tests/processing/gpu-palette/0011_comparison_short.t) |
| GW-08 | CPU/Metal A dither uses unperturbed targets and emitted RGB/HLS colors for post-lookup acceptance with direct/Eytzinger lookup. | [tests/processing/gpu-palette/0018_wire_a.t](../../../tests/processing/gpu-palette/0018_wire_a.t) |
| GW-09 | CPU/Metal X dither uses unperturbed targets and emitted RGB/HLS colors for post-lookup acceptance with direct/Eytzinger lookup. | [tests/processing/gpu-palette/0019_wire_x.t](../../../tests/processing/gpu-palette/0019_wire_x.t) |

The encode witnesses assert literal keep masks and decoded paints independently on CPU and forced GPU, preserve the working palette, and verify that comparison hints are cleared after the call. They establish byte RGB/HLS comparison behavior, not perceptual improvement for every video or support for legacy float-backed comparison.

### Quality regression tests

| ID | Observation and limit | Owning test |
| --- | --- | --- |
| D6-23 | Alternating black/white regions are composed onto a retained canvas and checked against brightness bounds, detecting accidental ordinary-color selection of the transparency key. This is a targeted stale-color regression, not an MS-SSIM guarantee or exact equality of every output pixel. | [tests/processing/filter/0013_filter_encode_6delta_keycolor.t](../../../tests/processing/filter/0013_filter_encode_6delta_keycolor.t) |


### Defensive and malformed-input tests

The [filter test directory](../../../tests/processing/filter/) also checks contradictory internal GPU enable/capability flags and exercises direct GPU request wiring. General malformed SIXEL and allocation coverage belongs to the [testing guide](../../testing/guide.md); it does not prove retained-screen correctness on a terminal.

### Coverage limits and manual validation

The exact early-match and threshold-255 boundaries, plane-size rejection/reset details, seeded-buffer lifecycle, broader float32/output-space scope, and missing-emitted-RGB fallback are audited against owning code. The table does not provide a separate focused automated assertion for every one of those dimensions. Likewise, the listed LSO2 test asserts `diffuse`, while D6-20's `skip` test covers only its six named policies. GPU tests may skip when the required device or backend is unavailable.

Receiver preservation, palette interpretation, external scroll/overlay recovery, delivery loss, and temporal appearance are application/terminal integration observations. Validate them with the actual receiver and a composed frame sequence; a passing single-frame decoder test cannot establish them.

### Progressive API contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| DP-01 | Bounded public calls report per-call and cumulative work, commit counts, complete audits and stale-source outcomes. | [tests/processing/progressive/0095_public_statistics_resume.t](../../../tests/processing/progressive/0095_public_statistics_resume.t) |
| DP-02 | A null output writes the complete repair to the configured destination. | [tests/processing/progressive/0096_public_default_destination.t](../../../tests/processing/progressive/0096_public_default_destination.t) |
| DP-03 | Invalid layout sizes are rejected and future structure tails remain untouched. | [tests/processing/progressive/0097_public_layout_validation.t](../../../tests/processing/progressive/0097_public_layout_validation.t) |
| DP-04 | Work counters reject overflow without output, and successful rebinding resets them. | [tests/processing/progressive/0098_public_counter_exhaustion.t](../../../tests/processing/progressive/0098_public_counter_exhaustion.t) |
| DP-05 | Envelope suppression reports unsupported repair without output or history changes. | [tests/processing/progressive/0099_public_unsupported_controls.t](../../../tests/processing/progressive/0099_public_unsupported_controls.t) |
| DP-06 | Busy admission and pending-update admission return distinct yield reasons without reading unowned image state. | [tests/processing/progressive/0100_public_admission_snapshot.t](../../../tests/processing/progressive/0100_public_admission_snapshot.t) |
| DP-07 | A normal update registered before output admission prevents repair emission. | [tests/processing/progressive/0093_step_pre_admission_priority.t](../../../tests/processing/progressive/0093_step_pre_admission_priority.t) |
| DP-08 | An admitted repair commits before a later normal update uses its history. | [tests/processing/progressive/0094_step_admitted_packet_order.t](../../../tests/processing/progressive/0094_step_admitted_packet_order.t) |
| DP-09 | A failed output callback invalidates retained history. | [tests/processing/progressive/0082_step_failed_output.t](../../../tests/processing/progressive/0082_step_failed_output.t) |

Additional [progressive tests](../../../tests/processing/progressive/) exercise canonical output projection, selection and recovery, budgeted packet construction, allocator failures, cancellation, callback reentry and ordinary-update ordering. These establish numeric and stream behavior; they do not measure perceptual pass curves, terminal presentation or GPU repair performance.

## Progressive presentation

`img2sixel -Z progressive` displays one composed still and overlays repairs until the working-space residual meets the stop threshold, the bounded repair strategy cannot improve it, or a presentation limit is reached. Animation input defaults to source frame zero, excluding an APNG poster. `-T` takes precedence over `SIXEL_LOADER_ANIMATION_START_FRAME_NO`. The implicit still selection belongs to this policy selection; selecting `-Z delta` or `-Z full` restores ordinary loading unless `-S` was explicitly requested.

`-Z delta:progressive=1` (or `-Z delta:P1`) keeps animation playback and pairs each ordinary image with at most one non-overlapping repair image in the same source interval. Each image uses an independently generated palette; the repair palette is not restricted to the ordinary `-p` colors. Both images preserve omitted pixels, and the repair restores the same image origin without adding another source-frame delay. The default is `0`; `SIXEL_UPDATE_DELTA_PROGRESSIVE=1|0` supplies the delta default without enabling delta itself.

The progressive switch follows the shared strict boolean contract: CLI long and short forms and the environment accept only exact `0` and `1`. Use `:progressive=0` or `:P0` to disable it. Textual values such as `on` and `off` are rejected.

The pair captures the ordinary image's final paint mask and excludes those positions from repair. Approximate delta keeps remain eligible, but only against the latest composed source and current retained history; an omission is not proof of an unchanged source. Source transparency without a known target remains excluded. Compatible unchanged target pixels retain a bounded residual age across source updates. Changed targets reset their age, and all candidate colors are evaluated against the newly bound target. The final still pass removes the ordinary-paint restriction so the last source can be refined over its full known domain.

Video repair selects and weights residuals using local luminance-mean and gradient error normalized by target brightness and variance, with bounded age weighting. Brightness and contrast normalization retains sensitivity to dark, flat defects; bounded terms prevent one extreme edge from outweighing an entire region. Gamma uses encoded Rec.709 luma; other workspaces convert to linear Y. Palette construction still evaluates retained-aware color costs in `-W`, with a bounded 128-sample pool, at most the configured repair color count of candidates, no swap iterations and no repair dither. A spatial luminance-error guard evaluates the actual packet after byte fitting, including unchanged neighbors, and rejects a worsening proposal. This inexpensive guard does not establish monotonic MS-SSIM, perceptual optimality or correction of every defect within a fixed number of frames. Per-pixel accepted color changes retain the canonical `-W` improvement rule.

The scheduler immediately reserves up to half a source interval for bounded repair work, constrained by the remaining absolute deadline and measured drain margin. Whole-frame write observations estimate packet throughput without treating one-byte flush overhead as a bulk transfer rate. Both DCS envelopes, palettes and placement consume the available output budget. An unavailable budget or exhausted work slot emits no repair and never certifies convergence. Source prefetch does not cancel the reserved slot. A bounded ordered adapter accepts at most one complete repair ahead of the host sink in threaded builds; thread-disabled builds use ordered synchronous writes. An admitted packet drains before a later ordinary frame. Host acceptance is not a terminal-display acknowledgement, and a blocking sink can exceed the estimated deadline. `-g` and zero-duration frames skip intermediate repairs. Finite playback refines its final frame.

Both paths use the repair API and generated palettes, including its optional Metal selection and palette application. The ordinary baseline retains its own GPU input and dither restrictions. Fixed palettes, high color, multiplexer wrapping and transparent offsets are unsupported, as are the ordinary delta conflicts above. Direct frame/byte calls remain caller-paced and do not run this presentation loop.

Both presentation policies accept `stop_threshold=auto|distance` (`:Svalue`), `max_passes=N` (`:Nvalue`) and `max_seconds=S` (`:Mvalue`). Defaults are `auto`, `0`, and `5`, respectively. The stop distance is in `-W`, with explicit values in `0..1e154`; it is not a Delta E00 grade. A zero stop distance requests exact agreement. Pass counts are integers in `0..4294967295`; time is any nonnegative finite number of seconds. Zero disables the respective resource cap. Passes count the initial ordinary image plus accepted repair packets, not resumable work calls. Limits on `delta` do not enable progressive repair by themselves. `full` rejects these suffixes.

Each source's elapsed time starts after its ordinary image is accepted, before source-binding work. Animation resets limits only for a new source; final-frame refinement retains that source's count and start time. The controller checks elapsed time during bounded work and immediately before output admission. An already admitted packet completes and remains in the retained history; a blocked host write can exceed the time allowance. Source-frame deadlines continue to take precedence over optional repair and are never extended by a limit or by repair count.

Reaching a resource cap returns success with the last complete output image. It does not certify that the quality target was met. Verbose output (`-v`) distinguishes `converged`, `stalled`, `max_passes`, and `max_seconds`. For example, `img2sixel input.png -Z progressive:stop_threshold=auto:max_passes=10:max_seconds=2` limits the display to ten total passes and two seconds after the initial image. `-Z progressive:max_seconds=0` removes the default time cap. Environment defaults are `SIXEL_UPDATE_PROGRESSIVE_STOP_THRESHOLD`, `SIXEL_UPDATE_PROGRESSIVE_MAX_PASSES`, and `SIXEL_UPDATE_PROGRESSIVE_MAX_SECONDS`; explicit suffixes take precedence, and a new `-Z` selection starts again from defaults and the selected base's environment.

File presentation uses fixed initial residual quantiles to move toward smaller residuals, allowing at most two accepted packets per nonfinal threshold stage. Empty or non-improving masks advance immediately. The final full-domain stage permanently disables noise for that source and retains the direct-projection rescue before reporting a strategy stall. The final quality target itself never changes. This policy bounds coarse-stage residence; it does not promise a universal optimal bandwidth/quality tradeoff or a pass bound at the final stage. Direct callers of the progressive step API retain their own configuration and scheduling; CLI caps do not change the public step or stats layouts.

A still capture contains one initial `ESC 7` followed by its complete P2=1 baseline, then `ESC 8` before each actual repair. No cursor restoration is emitted for an audit or a no-output yield. Cursor controls and image bytes use the same selected destination; live-terminal space reservation occurs before the initial save. Decode a still capture with `sixel2png -M 6delta-progressive`, or assess its final composition with `lsqa -M 6delta-progressive`. No IDCS attributes are emitted.

Presentation contracts are covered by:

- [tests/processing/progressive/0101_still_capture_history.t](../../../tests/processing/progressive/0101_still_capture_history.t): still capture history.
- [tests/processing/progressive/0102_gif_default_frame.t](../../../tests/processing/progressive/0102_gif_default_frame.t): gif default frame.
- [tests/processing/progressive/0103_gif_composed_selection.t](../../../tests/processing/progressive/0103_gif_composed_selection.t): gif composed selection.
- [tests/processing/progressive/0104_gif_negative_selection.t](../../../tests/processing/progressive/0104_gif_negative_selection.t): gif negative selection.
- [tests/processing/progressive/0105_apng_poster_exclusion.t](../../../tests/processing/progressive/0105_apng_poster_exclusion.t): apng poster exclusion.
- [tests/processing/progressive/0106_apng_invalid_selection.t](../../../tests/processing/progressive/0106_apng_invalid_selection.t): apng invalid selection.
- [tests/processing/progressive/0107_still_8bit_capture.t](../../../tests/processing/progressive/0107_still_8bit_capture.t): still 8bit capture.
- [tests/processing/progressive/0108_presentation_option_atomicity.t](../../../tests/processing/progressive/0108_presentation_option_atomicity.t): presentation option atomicity.

Scheduler and ordered output contracts are covered by:

- [tests/processing/progressive/0109_transport_capacity_admission.t](../../../tests/processing/progressive/0109_transport_capacity_admission.t): transport capacity admission.
- [tests/processing/progressive/0110_deadline_admission.t](../../../tests/processing/progressive/0110_deadline_admission.t): deadline admission.
- [tests/processing/progressive/0111_source_clock_preserved.t](../../../tests/processing/progressive/0111_source_clock_preserved.t): source clock preserved.
- [tests/processing/progressive/0112_zero_and_ignored_delay.t](../../../tests/processing/progressive/0112_zero_and_ignored_delay.t): zero and ignored delay.
- [tests/processing/progressive/0113_delay_cancellation.t](../../../tests/processing/progressive/0113_delay_cancellation.t): delay cancellation.
- [tests/processing/progressive/0114_ordered_transport.t](../../../tests/processing/progressive/0114_ordered_transport.t): ordered transport.
- [tests/processing/progressive/0115_transport_write_failure.t](../../../tests/processing/progressive/0115_transport_write_failure.t): transport write failure.
- [tests/processing/progressive/0116_ignored_delay_final_repair.t](../../../tests/processing/progressive/0116_ignored_delay_final_repair.t): ignored delay final repair.

CLI selection and precedence are covered by:

- [tests/cli/progressive/0001_environment_base.t](../../../tests/cli/progressive/0001_environment_base.t): environment base selection.
- [tests/cli/progressive/0002_source_environment_precedence.t](../../../tests/cli/progressive/0002_source_environment_precedence.t): explicit source index precedence.
- [tests/cli/progressive/0003_policy_reset_animation.t](../../../tests/cli/progressive/0003_policy_reset_animation.t): policy reset restores animation loading.
- [tests/cli/progressive/0004_explicit_static_preserved.t](../../../tests/cli/progressive/0004_explicit_static_preserved.t): explicit static survives policy reset.
- [tests/cli/progressive/0005_progressive_disable_precedence.t](../../../tests/cli/progressive/0005_progressive_disable_precedence.t): explicit progressive off overrides environment.
- [tests/cli/progressive/0006_source_environment_selection.t](../../../tests/cli/progressive/0006_source_environment_selection.t): source environment selects the composed frame.


### Presentation limit coverage

- [tests/processing/progressive/0140_limit_option_values.t](../../../tests/processing/progressive/0140_limit_option_values.t): limit option values.
- [tests/processing/progressive/0141_limit_option_atomicity.t](../../../tests/processing/progressive/0141_limit_option_atomicity.t): limit option atomicity.
- [tests/processing/progressive/0142_presentation_pass_limit.t](../../../tests/processing/progressive/0142_presentation_pass_limit.t): presentation pass limit.
- [tests/processing/progressive/0143_presentation_time_limit.t](../../../tests/processing/progressive/0143_presentation_time_limit.t): presentation time limit.
- [tests/processing/progressive/0144_presentation_admitted_deadline.t](../../../tests/processing/progressive/0144_presentation_admitted_deadline.t): presentation admitted deadline.
- [tests/processing/progressive/0145_presentation_zero_limits.t](../../../tests/processing/progressive/0145_presentation_zero_limits.t): presentation zero limits.
- [tests/processing/progressive/0146_presentation_rebind_limits.t](../../../tests/processing/progressive/0146_presentation_rebind_limits.t): presentation rebind limits.
- [tests/processing/progressive/0147_animation_pass_limit_clock.t](../../../tests/processing/progressive/0147_animation_pass_limit_clock.t): animation pass limit clock.
- [tests/processing/progressive/0148_animation_tail_time_limit.t](../../../tests/processing/progressive/0148_animation_tail_time_limit.t): animation tail time limit.
- [tests/processing/progressive/0149_presentation_final_no_noise.t](../../../tests/processing/progressive/0149_presentation_final_no_noise.t): presentation final no noise.
- [tests/processing/progressive/0150_video_pair_disjoint_palettes.t](../../../tests/processing/progressive/0150_video_pair_disjoint_palettes.t): disjoint paired images retain independent palette colors and one source interval.
- [tests/processing/progressive/0151_video_stable_residual_age.t](../../../tests/processing/progressive/0151_video_stable_residual_age.t): unchanged targets carry residual age while changed targets reset it.
- [tests/processing/progressive/0152_video_latest_source_revalidation.t](../../../tests/processing/progressive/0152_video_latest_source_revalidation.t): the next repair uses the latest target rather than a stale source.
- [tests/processing/progressive/0153_video_structure_guard.t](../../../tests/processing/progressive/0153_video_structure_guard.t): reject a pixel improvement that worsens the declared spatial luminance loss.
- [tests/processing/progressive/0154_video_pair_deadline.t](../../../tests/processing/progressive/0154_video_pair_deadline.t): an exhausted pair slot does not emit or claim full convergence.
- [tests/processing/progressive/0155_video_pair_write_failure.t](../../../tests/processing/progressive/0155_video_pair_write_failure.t): repair write failure invalidates retained history.
- [tests/processing/progressive/0156_video_final_full_domain.t](../../../tests/processing/progressive/0156_video_final_full_domain.t): final refinement includes positions excluded from the paired repair.
- [tests/processing/progressive/0157_progressive_boolean_syntax.t](../../../tests/processing/progressive/0157_progressive_boolean_syntax.t): accept exact boolean long/short syntax and atomically reject textual or malformed values.

CLI precedence and selection reset are verified by [tests/cli/progressive/0007_limit_cli_environment_precedence.t](../../../tests/cli/progressive/0007_limit_cli_environment_precedence.t) and [tests/cli/progressive/0008_limit_selection_reset.t](../../../tests/cli/progressive/0008_limit_selection_reset.t).

### Progressive GPU application coverage

These behavioral tests exercise deliberate-margin fixtures and canonical acceptance; they do not calibrate performance, automatic admission thresholds or perceptual quality. Metal-dependent cases skip when no device is available. Residual scan and global sampling are covered separately below; GPU summaries are never used to certify convergence.

| ID | Contract | Owning test |
| --- | --- | --- |
| PG-01 | Forced preparation uses Metal when available and reports unsupported otherwise. | [tests/processing/progressive/0067_prepare_force_policy.t](../../../tests/processing/progressive/0067_prepare_force_policy.t) |
| PG-02 | Reject malformed lookup requests without publishing matches. | [tests/processing/progressive/0168_gpu_lookup_validation.t](../../../tests/processing/progressive/0168_gpu_lookup_validation.t) |
| PG-03 | Search ordered signed float palettes with first-slot ties. | [tests/processing/progressive/0169_gpu_lookup_float_palette.t](../../../tests/processing/progressive/0169_gpu_lookup_float_palette.t) |
| PG-04 | Isolate owners and refresh reused query and palette addresses. | [tests/processing/progressive/0170_gpu_lookup_cache_isolation.t](../../../tests/processing/progressive/0170_gpu_lookup_cache_isolation.t) |
| PG-05 | Preserve output on failure, allow AUTO fallback and recover. | [tests/processing/progressive/0171_gpu_lookup_failure_fallback.t](../../../tests/processing/progressive/0171_gpu_lookup_failure_fallback.t) |
| PG-06 | Match CPU zero-noise masks on sparse offset input. | [tests/processing/progressive/0172_gpu_application_none.t](../../../tests/processing/progressive/0172_gpu_application_none.t) |
| PG-07 | Preserve absolute noise phase, protected pixels and keep masks. | [tests/processing/progressive/0173_gpu_application_bluenoise.t](../../../tests/processing/progressive/0173_gpu_application_bluenoise.t) |
| PG-08 | Resume a forced GPU palette search with one-distance budgets. | [tests/processing/progressive/0174_gpu_application_budget_resume.t](../../../tests/processing/progressive/0174_gpu_application_budget_resume.t) |
| PG-09 | Reject equal canonical error even when GPU float distance rounds down. | [tests/processing/progressive/0175_gpu_application_canonical_guard.t](../../../tests/processing/progressive/0175_gpu_application_canonical_guard.t) |
| PG-10 | Commit decoded GPU repair colors only at the bound plane origin. | [tests/processing/progressive/0176_gpu_public_commit.t](../../../tests/processing/progressive/0176_gpu_public_commit.t) |
| PG-11 | Rebind to a new source and switch from GPU to CPU without stale data. | [tests/processing/progressive/0177_gpu_public_rebind_transition.t](../../../tests/processing/progressive/0177_gpu_public_rebind_transition.t) |
| PG-12 | Admit AUTO by selected population independently of dispatch size. | [tests/processing/progressive/0178_gpu_public_auto_admission.t](../../../tests/processing/progressive/0178_gpu_public_auto_admission.t) |
| PG-13 | Invalidate retained history after a GPU repair writer failure. | [tests/processing/progressive/0179_gpu_public_write_failure.t](../../../tests/processing/progressive/0179_gpu_public_write_failure.t) |
| PG-14 | Cancel after GPU selection and after GPU lookup, resuming each without early output. | [tests/processing/progressive/0180_gpu_public_cancel_resume.t](../../../tests/processing/progressive/0180_gpu_public_cancel_resume.t) |
| PG-15 | Retry a failed GPU batch on CPU while accounting for its work budget. | [tests/processing/progressive/0181_gpu_application_auto_fallback.t](../../../tests/processing/progressive/0181_gpu_application_auto_fallback.t) |

### Progressive GPU selection coverage

GPU residual masks are corrected against the canonical audit before global sampling. Sample strata and video weights do not depend on dispatch size. These tests verify functional contracts, not performance or perceptual calibration.

| ID | Contract | Owning test |
| --- | --- | --- |
| PS-01 | Preserve global sample positions and populations across filtered batches. | [tests/processing/progressive/0182_gpu_global_strata.t](../../../tests/processing/progressive/0182_gpu_global_strata.t) |
| PS-02 | Reject malformed global sampling requests before GPU submission. | [tests/processing/progressive/0183_gpu_global_validation.t](../../../tests/processing/progressive/0183_gpu_global_validation.t) |
| PS-03 | Preserve more than 128 global samples across bounded GPU dispatches. | [tests/processing/progressive/0184_gpu_selection_global_samples.t](../../../tests/processing/progressive/0184_gpu_selection_global_samples.t) |
| PS-04 | Resume GPU selection and sampling with one-unit work budgets. | [tests/processing/progressive/0185_gpu_selection_budget_resume.t](../../../tests/processing/progressive/0185_gpu_selection_budget_resume.t) |
| PS-05 | Restore canonical residuals omitted by a rounded float threshold. | [tests/processing/progressive/0186_gpu_selection_threshold_guard.t](../../../tests/processing/progressive/0186_gpu_selection_threshold_guard.t) |
| PS-06 | Preserve luminance priority masks and weighted sample populations. | [tests/processing/progressive/0187_gpu_selection_video_priority.t](../../../tests/processing/progressive/0187_gpu_selection_video_priority.t) |
| PS-07 | Account failed GPU selection work before AUTO resumes on CPU. | [tests/processing/progressive/0188_gpu_selection_auto_fallback.t](../../../tests/processing/progressive/0188_gpu_selection_auto_fallback.t) |
| PS-08 | Keep frozen mask thresholds and protected candidates on the GPU path. | [tests/processing/progressive/0189_gpu_selection_ladder.t](../../../tests/processing/progressive/0189_gpu_selection_ladder.t) |
| PS-09 | Reject stale GPU scan work after an ordinary update and rebind safely. | [tests/processing/progressive/0190_gpu_scan_update_rebind.t](../../../tests/processing/progressive/0190_gpu_scan_update_rebind.t) |
| PS-10 | Use GPU scan and lookup for disjoint paired images with independent palettes. | [tests/processing/progressive/0191_gpu_video_pair.t](../../../tests/processing/progressive/0191_gpu_video_pair.t) |

### Progressive GPU operation ordering coverage

Metal batches complete synchronously while the encoder operation gate owns the context. A waiting ordinary update or rebind receives the gate at the next preparation checkpoint; after output admission, the packet and retained-history commit finish first. Context teardown also waits for that gate, keeping source snapshots, host work and device caches alive through the active operation. These tests register an actual waiting thread at a completed GPU batch or the output-admission boundary. They do not inject a driver failure or suspend an in-flight Metal command.

| ID | Contract | Owning test |
| --- | --- | --- |
| PO-01 | Yield to a registered ordinary update after a real GPU scan. | [tests/processing/progressive/0192_gpu_scan_pending_update.t](../../../tests/processing/progressive/0192_gpu_scan_pending_update.t) |
| PO-02 | Yield to a registered ordinary update after a real GPU lookup. | [tests/processing/progressive/0193_gpu_lookup_pending_update.t](../../../tests/processing/progressive/0193_gpu_lookup_pending_update.t) |
| PO-03 | Suppress a prepared GPU repair when an update precedes admission. | [tests/processing/progressive/0194_gpu_pending_before_admission.t](../../../tests/processing/progressive/0194_gpu_pending_before_admission.t) |
| PO-04 | Commit an admitted GPU repair before a waiting ordinary update. | [tests/processing/progressive/0195_gpu_pending_after_admission.t](../../../tests/processing/progressive/0195_gpu_pending_after_admission.t) |
| PO-05 | Keep GPU binding storage alive until a waiting rebind owns the gate. | [tests/processing/progressive/0196_gpu_pending_rebind.t](../../../tests/processing/progressive/0196_gpu_pending_rebind.t) |
| PO-06 | Finish an admitted GPU repair before a waiting context destruction. | [tests/processing/progressive/0197_gpu_pending_destroy.t](../../../tests/processing/progressive/0197_gpu_pending_destroy.t) |

### Progressive GPU color interpretation coverage

The matrix below exercises the public repair API with all five working spaces, the four supported output interpretations and both RGB and HLS wire definitions. Each case uses an offset source rectangle with transparent holes and runs with zero noise and positional blue noise. Both GPU selection and lookup must actually execute under FORCE and admitted AUTO; OFF and AUTO below its population threshold must remain on CPU. The AUTO cases inherit encoder policy and distinguish the four eligible pixels from the six-pixel rectangle. The packet is independently decoded in the selected output coordinates and compared exactly with retained history; omitted pixels and the outside halo must remain unchanged. A fresh canonical audit must show strictly reduced error at every painted position in the selected working space.

Repair source preparation normalizes packed alpha before producing typed float samples, independently of the ordinary encoder's palette, lookup and GPU restrictions. Ordinary Display P3 encoding retains its own GPU rejection; support in this repair API does not expand the ordinary encoder or CLI baseline path.

The CPU comparison establishes backend consistency, not an independent reference for color-space conversion or a perceptual quality score. Exact packet/history agreement is checked only for fixtures whose nearest and runner-up lookup distances differ by more than `32 * FLT_EPSILON * max(1, runner_up_distance)`, bounding float subtraction, multiplication and accumulation error for both candidates (including fused evaluation). Query coordinates are the shared float positional queries; the oracle computes their distances in double. This margin is asserted at runtime before accepting exact agreement. General conversion correctness and numerical methods remain owned by [color-space handling](../colorspace-handling/README.md) and the [testing guide](../../testing/guide.md#intermediate-floating-point-error). Output interpretation is checked through API metadata; the SIXEL packet does not announce Display P3 or another wide-gamut interpretation.

| ID | Working space | Output coordinates | Wire definition | Owning test |
| --- | --- | --- | --- | --- |
| PC-01 | `gamma` | `gamma` | RGB | [tests/processing/progressive/0198_gpu_gamma_gamma_rgb.t](../../../tests/processing/progressive/0198_gpu_gamma_gamma_rgb.t) |
| PC-02 | `gamma` | `gamma` | HLS | [tests/processing/progressive/0199_gpu_gamma_gamma_hls.t](../../../tests/processing/progressive/0199_gpu_gamma_gamma_hls.t) |
| PC-03 | `gamma` | `linear` | RGB | [tests/processing/progressive/0200_gpu_gamma_linear_rgb.t](../../../tests/processing/progressive/0200_gpu_gamma_linear_rgb.t) |
| PC-04 | `gamma` | `linear` | HLS | [tests/processing/progressive/0201_gpu_gamma_linear_hls.t](../../../tests/processing/progressive/0201_gpu_gamma_linear_hls.t) |
| PC-05 | `gamma` | `smpte-c` | RGB | [tests/processing/progressive/0202_gpu_gamma_smptec_rgb.t](../../../tests/processing/progressive/0202_gpu_gamma_smptec_rgb.t) |
| PC-06 | `gamma` | `smpte-c` | HLS | [tests/processing/progressive/0203_gpu_gamma_smptec_hls.t](../../../tests/processing/progressive/0203_gpu_gamma_smptec_hls.t) |
| PC-07 | `gamma` | `display-p3` | RGB | [tests/processing/progressive/0204_gpu_gamma_display_p3_rgb.t](../../../tests/processing/progressive/0204_gpu_gamma_display_p3_rgb.t) |
| PC-08 | `gamma` | `display-p3` | HLS | [tests/processing/progressive/0205_gpu_gamma_display_p3_hls.t](../../../tests/processing/progressive/0205_gpu_gamma_display_p3_hls.t) |
| PC-09 | `linear` | `gamma` | RGB | [tests/processing/progressive/0206_gpu_linear_gamma_rgb.t](../../../tests/processing/progressive/0206_gpu_linear_gamma_rgb.t) |
| PC-10 | `linear` | `gamma` | HLS | [tests/processing/progressive/0207_gpu_linear_gamma_hls.t](../../../tests/processing/progressive/0207_gpu_linear_gamma_hls.t) |
| PC-11 | `linear` | `linear` | RGB | [tests/processing/progressive/0208_gpu_linear_linear_rgb.t](../../../tests/processing/progressive/0208_gpu_linear_linear_rgb.t) |
| PC-12 | `linear` | `linear` | HLS | [tests/processing/progressive/0209_gpu_linear_linear_hls.t](../../../tests/processing/progressive/0209_gpu_linear_linear_hls.t) |
| PC-13 | `linear` | `smpte-c` | RGB | [tests/processing/progressive/0210_gpu_linear_smptec_rgb.t](../../../tests/processing/progressive/0210_gpu_linear_smptec_rgb.t) |
| PC-14 | `linear` | `smpte-c` | HLS | [tests/processing/progressive/0211_gpu_linear_smptec_hls.t](../../../tests/processing/progressive/0211_gpu_linear_smptec_hls.t) |
| PC-15 | `linear` | `display-p3` | RGB | [tests/processing/progressive/0212_gpu_linear_display_p3_rgb.t](../../../tests/processing/progressive/0212_gpu_linear_display_p3_rgb.t) |
| PC-16 | `linear` | `display-p3` | HLS | [tests/processing/progressive/0213_gpu_linear_display_p3_hls.t](../../../tests/processing/progressive/0213_gpu_linear_display_p3_hls.t) |
| PC-17 | `oklab` | `gamma` | RGB | [tests/processing/progressive/0214_gpu_oklab_gamma_rgb.t](../../../tests/processing/progressive/0214_gpu_oklab_gamma_rgb.t) |
| PC-18 | `oklab` | `gamma` | HLS | [tests/processing/progressive/0215_gpu_oklab_gamma_hls.t](../../../tests/processing/progressive/0215_gpu_oklab_gamma_hls.t) |
| PC-19 | `oklab` | `linear` | RGB | [tests/processing/progressive/0216_gpu_oklab_linear_rgb.t](../../../tests/processing/progressive/0216_gpu_oklab_linear_rgb.t) |
| PC-20 | `oklab` | `linear` | HLS | [tests/processing/progressive/0217_gpu_oklab_linear_hls.t](../../../tests/processing/progressive/0217_gpu_oklab_linear_hls.t) |
| PC-21 | `oklab` | `smpte-c` | RGB | [tests/processing/progressive/0218_gpu_oklab_smptec_rgb.t](../../../tests/processing/progressive/0218_gpu_oklab_smptec_rgb.t) |
| PC-22 | `oklab` | `smpte-c` | HLS | [tests/processing/progressive/0219_gpu_oklab_smptec_hls.t](../../../tests/processing/progressive/0219_gpu_oklab_smptec_hls.t) |
| PC-23 | `oklab` | `display-p3` | RGB | [tests/processing/progressive/0220_gpu_oklab_display_p3_rgb.t](../../../tests/processing/progressive/0220_gpu_oklab_display_p3_rgb.t) |
| PC-24 | `oklab` | `display-p3` | HLS | [tests/processing/progressive/0221_gpu_oklab_display_p3_hls.t](../../../tests/processing/progressive/0221_gpu_oklab_display_p3_hls.t) |
| PC-25 | `cielab` | `gamma` | RGB | [tests/processing/progressive/0222_gpu_cielab_gamma_rgb.t](../../../tests/processing/progressive/0222_gpu_cielab_gamma_rgb.t) |
| PC-26 | `cielab` | `gamma` | HLS | [tests/processing/progressive/0223_gpu_cielab_gamma_hls.t](../../../tests/processing/progressive/0223_gpu_cielab_gamma_hls.t) |
| PC-27 | `cielab` | `linear` | RGB | [tests/processing/progressive/0224_gpu_cielab_linear_rgb.t](../../../tests/processing/progressive/0224_gpu_cielab_linear_rgb.t) |
| PC-28 | `cielab` | `linear` | HLS | [tests/processing/progressive/0225_gpu_cielab_linear_hls.t](../../../tests/processing/progressive/0225_gpu_cielab_linear_hls.t) |
| PC-29 | `cielab` | `smpte-c` | RGB | [tests/processing/progressive/0226_gpu_cielab_smptec_rgb.t](../../../tests/processing/progressive/0226_gpu_cielab_smptec_rgb.t) |
| PC-30 | `cielab` | `smpte-c` | HLS | [tests/processing/progressive/0227_gpu_cielab_smptec_hls.t](../../../tests/processing/progressive/0227_gpu_cielab_smptec_hls.t) |
| PC-31 | `cielab` | `display-p3` | RGB | [tests/processing/progressive/0228_gpu_cielab_display_p3_rgb.t](../../../tests/processing/progressive/0228_gpu_cielab_display_p3_rgb.t) |
| PC-32 | `cielab` | `display-p3` | HLS | [tests/processing/progressive/0229_gpu_cielab_display_p3_hls.t](../../../tests/processing/progressive/0229_gpu_cielab_display_p3_hls.t) |
| PC-33 | `din99d` | `gamma` | RGB | [tests/processing/progressive/0230_gpu_din99d_gamma_rgb.t](../../../tests/processing/progressive/0230_gpu_din99d_gamma_rgb.t) |
| PC-34 | `din99d` | `gamma` | HLS | [tests/processing/progressive/0231_gpu_din99d_gamma_hls.t](../../../tests/processing/progressive/0231_gpu_din99d_gamma_hls.t) |
| PC-35 | `din99d` | `linear` | RGB | [tests/processing/progressive/0232_gpu_din99d_linear_rgb.t](../../../tests/processing/progressive/0232_gpu_din99d_linear_rgb.t) |
| PC-36 | `din99d` | `linear` | HLS | [tests/processing/progressive/0233_gpu_din99d_linear_hls.t](../../../tests/processing/progressive/0233_gpu_din99d_linear_hls.t) |
| PC-37 | `din99d` | `smpte-c` | RGB | [tests/processing/progressive/0234_gpu_din99d_smptec_rgb.t](../../../tests/processing/progressive/0234_gpu_din99d_smptec_rgb.t) |
| PC-38 | `din99d` | `smpte-c` | HLS | [tests/processing/progressive/0235_gpu_din99d_smptec_hls.t](../../../tests/processing/progressive/0235_gpu_din99d_smptec_hls.t) |
| PC-39 | `din99d` | `display-p3` | RGB | [tests/processing/progressive/0236_gpu_din99d_display_p3_rgb.t](../../../tests/processing/progressive/0236_gpu_din99d_display_p3_rgb.t) |
| PC-40 | `din99d` | `display-p3` | HLS | [tests/processing/progressive/0237_gpu_din99d_display_p3_hls.t](../../../tests/processing/progressive/0237_gpu_din99d_display_p3_hls.t) |
| PC-41 | Ordinary encoding | `display-p3` | GPU admission still rejects before output | [tests/processing/progressive/0238_gpu_p3_ordinary_boundary.t](../../../tests/processing/progressive/0238_gpu_p3_ordinary_boundary.t) |
