# Delta Encoding (6delta)

6delta reduces repeated SIXEL painting by remembering the colors previously sent to a persistent display surface. For each new pixel, the encoder can either paint a palette color or leave the existing pixel untouched. An untouched pixel is represented through ordinary SIXEL transparency with `P2=1`; the receiver does not need a separate 6delta command or decoder.

This is useful for animation and applications that repeatedly repaint a surface, including remote desktops. It requires the encoder's retained image and the receiver's actual image to remain synchronized. A delta frame by itself is an incomplete image: decoding it on a blank canvas cannot reconstruct the pixels it deliberately omits.

See the [SIXEL format](../../sixel-format/README.md) for the wire syntax, [alpha policy](../../loader/alpha-policy/README.md) for `P2=1`, and the [encoding pipeline](../pipeline/README.md) for palette construction and application.

## Quick start

Select `--update-policy=delta` (short form `-Z delta`) to enable 6delta. The default policy is `full`. The delta policy defaults to `threshold=0:error=diffuse`; zero controls the exact early-match gate, while the policy itself controls enablement.

```sh
img2sixel --update-policy=delta:threshold=0:error=diffuse \
  --precision-policy=8bit --working-colorspace=gamma --gpu-policy=off \
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

## Persistent residual threshold control

Use `img2sixel input.apng -p64 -dbluenoise -Zdelta:R1:T8` to reduce stale low-frequency color errors during ordinary delta updates. The encoder analyzes the difference between the current target and the displayed-color model, then lowers T locally. It uses the ordinary palette and emits one ordinary delta image per input; it does not generate extra repair packets, perform palette feedback, or continue working after the final input. The separate `progressive` option keeps its own meaning.

| Delta suboption | Compact key | Accepted values | Default |
| --- | --- | --- | --- |
| `residual` | R | 0 or 1 | 0 |
| `threshold_metric` | D | auto, max, rgb241 (value prefixes accepted) | auto |
| `residual_threshold` | H | finite number 0..255 | 1.5 |
| `residual_keep_threshold` | K | integer 0..255 | 0 |
| `residual_frames` | F | integer 1..255 | 3 |

The explicit profile is `-Zdelta:R1:T8:H1.5:K0:F3`. T retains its default zero: R1 alone does not silently increase it. H measures signed Gaussian-filtered residual with the same normalized 2:4:1 metric as the early gate. It is not Delta E, S-CIELAB, or a portable threshold for other working spaces. Filter each signed component before taking its magnitude, so fine alternating errors can cancel. The filter has sigma 4 pixels and radius 12, with support clipped and renormalized over valid history. Up to 64 by 64 screen cells are sampled at their centers. Strictly exceeding H (with a 1e-10 roundoff guard) for F consecutive observations selects the whole cell. This is screen-coordinate persistence, without motion compensation.

F counts successful encode observations, including zero-paint updates, rather than source indices, seconds or repair passes. Clients need not set frame numbers. Inputs dropped before encoding are not observations. Failed output cannot publish speculative age; potentially partial output invalidates history and age. Seeding/invalidation, geometry changes and effective residual-policy changes reset age; reapplying the same policy preserves it. Turning residual off clears age without discarding valid displayed colors.

The effective outside threshold is `min(T, runtime_limit)` and the inside threshold is `min(T, runtime_limit, K)`. A mask cannot relax the outside gate. Runtime limit zero remains authoritative and clears age after a successful observation; T0 still permits post-lookup keeps and is not a forced repaint.

R1 currently requires an opaque, full-plane gamma RGB8 target at origin zero, native sRGB RGB output, and none, a_dither, x_dither or bluenoise application. Alpha holes, partial updates, float targets and other working/output spaces are rejected before image output. Explicit rgb241 without a mask also requires gamma RGB8 and none/a_dither/x_dither/bluenoise, but does not require a full plane. R1 with max metric, progressive=1 or an attached progressive context is rejected. R0 retains the ordinary delta support boundaries. CPU analysis produces one immutable grid used by CPU and Metal application; AUTO/OFF provide the same residual semantics on CPU, while FORCE follows the GPU admission contract. CPU analysis is bounded by the grid rather than a full-resolution residual image. Verbose output reports the resolved residual profile and selected area; existing GPU diagnostics identify the application backend.

Configure the C API with `sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY, "delta:residual=1:threshold=8")`, check its status, then use the existing encode/frame/byte calls. `SIXEL_HAVE_ENCODER_6DELTA_RESIDUAL` is 1. No new context or caller-owned mask is required: the encoder owns persistence and accumulation, and the client owns frame timing, ordered output, backpressure and external-paint invalidation. Existing painted-pixel and refinement queries retain their meanings; they do not certify convergence or an empty residual mask.

The associated environment variables are `SIXEL_UPDATE_DELTA_RESIDUAL`, `SIXEL_UPDATE_DELTA_THRESHOLD_METRIC`, `SIXEL_UPDATE_DELTA_RESIDUAL_THRESHOLD`, `SIXEL_UPDATE_DELTA_RESIDUAL_KEEP_THRESHOLD`, and `SIXEL_UPDATE_DELTA_RESIDUAL_FRAMES`. They do not select the delta base by themselves. Each update-policy selection resolves defaults, environment and explicit suffixes in that order; explicit values win. Invalid individual environment values preserve defaults, whereas invalid explicit values fail. Unsupported resolved combinations fail atomically, including later changes to dependent options.

An experimental periodic alternative is `-Zdelta:T8:refresh_ms=80` (`:J80`). `refresh_ms` accepts integer milliseconds 0..10000, defaults to 0 (disabled), and has environment default `SIXEL_UPDATE_DELTA_REFRESH_MS`. It leaves T and the threshold metric unchanged and cannot combine with R1 or progressive repair. The same opaque, full-plane gamma RGB8 and positional/none application restrictions apply. This is a scheduling experiment, not a perceptual quality preset; 80 ms is an example rather than a visibility guarantee.

Periodic refresh distributes 64 deadline phases over individual pixels using a repeating 64x64 blue-noise tile. Each pixel lowers its early-keep threshold to zero on the first encoded frame crossing its deadline. The tile stays at pixel scale regardless of frame dimensions and is translated relative to the color-dither tile. Fixed phases avoid the starvation possible with independent per-frame random selection. Pixel-scale selection avoids coherent rectangular refresh boundaries on smooth fades, but can increase SIXEL output size by breaking runs. Every deadline crossed since the previous successful output is considered: skipped frames cannot skip a phase, and a gap of one period selects the whole frame. Equal timestamps do not age the schedule. Backward time, history invalidation, geometry changes and period changes restart it; restarting against valid history first selects the entire frame. No source-image scan or additional repair packet is needed. The ordinary post-lookup keep still applies, so a deadline guarantees re-evaluation, not repainting, convergence, or disappearance of visible error. Work occurs only when the caller submits a frame; processing, transport and receiver delays can extend actual on-screen residence beyond the requested period.

The default clock is monotonic time at encoding. Playback callers can instead set `SIXEL_PLAYBACK_OPTION_REFRESH_TIME` and `refresh_time_us` in [sixel_playback_options_t](../../../include/sixel.h.in), for example to use media timestamps in an unpaced benchmark. `SIXEL_HAVE_PLAYBACK_REFRESH_TIME` identifies this appended field; both older option-struct sizes remain accepted without the new flag. The timestamp is copied at submission and does not pace playback. The clock and mask are committed only after successful encoding; output failure invalidates them with history. CPU and Metal consume the same mask. `SIXEL_TRACE_TOPIC=delta_refresh` reports the supplied time, period, selected area and frame area. This selected area measures re-evaluation work, not painted pixels or output bytes.

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

### Early keep: a configurable shortcut

Let `S` be the RGB888 sample used for this pixel's keep comparison, `R` its valid retained RGB, and `T` the threshold. With the default `threshold_metric=max`, the early gate keeps the pixel when all three conditions hold:

```text
abs(S.red   - R.red)   <= T
abs(S.green - R.green) <= T
abs(S.blue  - R.blue)  <= T
```

The default max metric is a byte-component tolerance, not a percentage or a perceptual Delta E. `threshold_metric=rgb241` instead compares `(2*dR*dR + 4*dG*dG + dB*dB) <= 7*T*T`, a normalized weighted Euclidean distance in gamma RGB8 units. It changes neither palette lookup nor post-lookup acceptance. `threshold_metric=auto` resolves to max with residual disabled and rgb241 with residual enabled. At `T=0`, it succeeds for an exact RGB match. If it succeeds, the palette lookup can be skipped. If it fails, the pixel still has another opportunity to be kept after lookup.

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
| Other float32 combinations | Float paths without the RGB101 wire palette keep their existing gamma-RGB history behavior. This is not a claim of equivalent delta behavior on every legacy float path. |
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

Reaching a resource cap returns success with the last complete output image. It does not certify that the quality target was met. Verbose output (`-v`) distinguishes `converged`, `stalled`, `max_passes`, and `max_seconds`, and reports the `max_seconds` value held by the presentation consumer. A positive time cap shorter than the platform clock resolution does not guarantee a one-pass stop: the clock may return the same value across multiple observations. Deadline tests use an injected clock; CLI route-equivalence tests use a one-pass cap and inspect the consumed time budget so their output does not depend on host clock granularity. For example, `img2sixel input.png -Z progressive:stop_threshold=auto:max_passes=10:max_seconds=2` limits the display to ten total passes and two seconds after the initial image. `-Z progressive:max_seconds=0` removes the default time cap. Environment defaults are `SIXEL_UPDATE_PROGRESSIVE_STOP_THRESHOLD`, `SIXEL_UPDATE_PROGRESSIVE_MAX_PASSES`, and `SIXEL_UPDATE_PROGRESSIVE_MAX_SECONDS`; explicit suffixes take precedence, and a new `-Z` selection starts again from defaults and the selected base's environment.

File presentation uses fixed initial residual quantiles to move toward smaller residuals, allowing at most two accepted packets per nonfinal threshold stage. Empty or non-improving masks advance immediately. The final full-domain stage permanently disables noise for that source and retains the direct-projection rescue before reporting a strategy stall. The final quality target itself never changes. This policy bounds coarse-stage residence; it does not promise a universal optimal bandwidth/quality tradeoff or a pass bound at the final stage. Direct callers of the progressive step API retain their own configuration and scheduling; CLI caps do not change the public step or stats layouts.

A still capture contains one initial `ESC 7` followed by its complete P2=1 baseline, then `ESC 8` before each actual repair. No cursor restoration is emitted for an audit or a no-output yield. Cursor controls and image bytes use the same selected destination; live-terminal space reservation occurs before the initial save. Decode a still capture with `sixel2png -M 6delta-progressive`, or assess its final composition with `lsqa -M 6delta-progressive`. No IDCS attributes are emitted.

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

## Indexed retained-state transactions

The ordinary byte indexed path stores retained RGB and validity in the `image/retained-plane` component. A read lease pins one stream, epoch and committed sequence. `begin_update` binds an unpublished indexed image to that baseline and reserves optional paint-mark storage before output. Read leases and producer/body users must drain before commit. The result is immutable after publication; source pixels and a predicted palette are not evidence of the image that was sent.

A successful writer receipt identifies the stream, epoch, base sequence and new sequence accepted by the callback. Commit requires a complete indexed result and a matching receipt; it rejects a foreign, stale, duplicate or busy transaction. In one allocation-free pass it preserves skipped pixels, expands painted indexes through the exact wire palette, sets validity and reports paint statistics. Skipped sentinel indexes are never palette addresses. Origin coordinates address the retained plane; SIXEL padding belongs only to serialization.

Aborting an update preserves the prior committed plane. A partial external write instead makes that plane unknown: the session drains workers, aborts framing and invalidates history before another dependent delta frame. Seek, geometry or interpretation changes and discarded queued packets likewise require admission to stop, dependent work to drain and history to be invalidated. Receipt acceptance means ordered packet construction or callback acceptance, not physical terminal display. Transport owners must preserve packet order or discard the dependent suffix and request a fresh baseline.

Legacy highcolor and specialized repair adapters borrow the same component-owned storage exclusively; they do not own a second retained RGB plane. Their existing wire projection and recovery contracts remain in effect. The indexed component protocol itself does not acknowledge a downstream terminal queue or detect a later transport failure.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The IDs below map to existing assertions. A representative fixture establishes its stated observation, not exhaustive coverage of every RGB value, threshold, backend, or terminal.

| ID | Contract and assertion boundary | Owning test |
| --- | --- | --- |
| D6-290 | Compact and environment refresh-period routes store 80 ms and produce identical nonempty initial-image bytes. | [tests/cli/output-options/0024_update_delta_refresh_ms_image_regression.t](../../../tests/cli/output-options/0024_update_delta_refresh_ms_image_regression.t) |
| D6-280 | Disabled default, long/compact period options, bounds and conflicting policy changes are atomic. | [tests/delta/refresh/0001_option_atomicity.t](../../../tests/delta/refresh/0001_option_atomicity.t) |
| D6-281 | Four quarter-period observations cover all blue-noise tile positions exactly once. | [tests/delta/refresh/0002_staggered_coverage.t](../../../tests/delta/refresh/0002_staggered_coverage.t) |
| D6-282 | An observation after a missed full period selects all pixels. | [tests/delta/refresh/0003_skipped_deadlines.t](../../../tests/delta/refresh/0003_skipped_deadlines.t) |
| D6-283 | Repeated timestamps select no additional pixels. | [tests/delta/refresh/0004_repeated_clock.t](../../../tests/delta/refresh/0004_repeated_clock.t) |
| D6-284 | Backward timestamps restart with full re-evaluation, then resume staggered deadlines. | [tests/delta/refresh/0005_backward_clock.t](../../../tests/delta/refresh/0005_backward_clock.t) |
| D6-285 | CPU playback updates the exact blue-noise pixel positions on an uneven 130x77 frame, with literal paint counts and expected emitted RGB history. | [tests/delta/refresh/0006_cpu_playback.t](../../../tests/delta/refresh/0006_cpu_playback.t) |
| D6-286 | Metal playback preserves the same pixel positions, uneven-edge paint counts and emitted RGB history as CPU playback. | [tests/delta/refresh/0007_gpu_playback.t](../../../tests/delta/refresh/0007_gpu_playback.t) |
| D6-287 | The previous option-struct size preserves the appended-field canary and rejects the new flag. | [tests/delta/refresh/0008_legacy_options.t](../../../tests/delta/refresh/0008_legacy_options.t) |
| D6-288 | Failed output invalidates retained pixels and the tentative refresh clock. | [tests/delta/refresh/0009_failed_output.t](../../../tests/delta/refresh/0009_failed_output.t) |
| D6-289 | Latest-palette playback retains the same timestamp and emitted-history assertions. | [tests/delta/refresh/0010_latest_playback.t](../../../tests/delta/refresh/0010_latest_playback.t) |
| D6-01 | No history keeps on the first opaque frame; repeats keep pixels; a moving rectangle keeps some overlap without keeping the entire new rectangle; invalidation and a full/delta transition force no history keeps; rejected full suboptions preserve state. | [tests/delta/comparison/0001_filter_encode_6delta_plane.t](../../../tests/delta/comparison/0001_filter_encode_6delta_plane.t) |
| D6-02 | Repeated content can produce keeps at threshold zero. | [tests/delta/comparison/0002_filter_encode_6delta_keycolor.t](../../../tests/delta/comparison/0002_filter_encode_6delta_keycolor.t) |
| D6-03 | CLI threshold 8 is accepted; 256, -1, and nonnumeric input are rejected; `skip` is accepted. | [tests/delta/comparison/0018_update_delta_threshold_cli_range.t](../../../tests/delta/comparison/0018_update_delta_threshold_cli_range.t) |
| D6-04 | `diffuse` and `skip` are accepted; `carry` is rejected. | [tests/cli/output-options/0009_option_matching_update_delta_error_choice.t](../../../tests/cli/output-options/0009_option_matching_update_delta_error_choice.t) |
| D6-05 | Threshold 32 supplied through the environment produces the same animation bytes as `-Z delta:threshold=32` for the fixture. | [tests/cli/output-options/0011_update_delta_threshold_environment_cli_equivalence.t](../../../tests/cli/output-options/0011_update_delta_threshold_environment_cli_equivalence.t) |
| D6-06 | Environment `skip` produces the same animation bytes as `-Z delta:error=skip` for the fixture. | [tests/cli/output-options/0012_update_delta_error_environment_cli_equivalence.t](../../../tests/cli/output-options/0012_update_delta_error_environment_cli_equivalence.t) |
| D6-07 | Default `auto` requests `P2=1` with 6delta. | [tests/delta/comparison/0019_6delta_auto_policy_selects_keep.t](../../../tests/delta/comparison/0019_6delta_auto_policy_selects_keep.t) |
| D6-08 | Explicit `clear` conflicts with 6delta and reports the alpha-policy requirement. | [tests/delta/comparison/0020_6delta_explicit_alpha_policy_conflict.t](../../../tests/delta/comparison/0020_6delta_explicit_alpha_policy_conflict.t) |
| D6-09 | Explicit `composite` conflicts with 6delta and reports the alpha-policy requirement. | [tests/delta/comparison/0021_6delta_composite_policy_conflict.t](../../../tests/delta/comparison/0021_6delta_composite_policy_conflict.t) |
| D6-10 | LSO2 selects the keep index at threshold zero and propagates retained-color error to the next sample. | [tests/delta/comparison/0003_filter_dither_6delta_lso2.t](../../../tests/delta/comparison/0003_filter_dither_6delta_lso2.t) |
| D6-11 | Jarvis, Judice & Ninke applies the same keep/error observation. | [tests/delta/comparison/0004_filter_dither_6delta_jajuni.t](../../../tests/delta/comparison/0004_filter_dither_6delta_jajuni.t) |
| D6-12 | Stucki applies the same keep/error observation. | [tests/delta/comparison/0005_filter_dither_6delta_stucki.t](../../../tests/delta/comparison/0005_filter_dither_6delta_stucki.t) |
| D6-13 | Burkes applies the same keep/error observation. | [tests/delta/comparison/0006_filter_dither_6delta_burkes.t](../../../tests/delta/comparison/0006_filter_dither_6delta_burkes.t) |
| D6-14 | Sierra Lite applies the same keep/error observation. | [tests/delta/comparison/0007_filter_dither_6delta_sierra1.t](../../../tests/delta/comparison/0007_filter_dither_6delta_sierra1.t) |
| D6-15 | Sierra Two-row applies the same keep/error observation. | [tests/delta/comparison/0008_filter_dither_6delta_sierra2.t](../../../tests/delta/comparison/0008_filter_dither_6delta_sierra2.t) |
| D6-16 | Sierra-3 applies the same keep/error observation. | [tests/delta/comparison/0009_filter_dither_6delta_sierra3.t](../../../tests/delta/comparison/0009_filter_dither_6delta_sierra3.t) |
| D6-17 | A-dither records a keep index and mask when retained RGB beats its perturbed palette choice. | [tests/delta/comparison/0010_filter_dither_6delta_a_dither.t](../../../tests/delta/comparison/0010_filter_dither_6delta_a_dither.t) |
| D6-18 | X-dither records the corresponding keep index and mask. | [tests/delta/comparison/0011_filter_dither_6delta_x_dither.t](../../../tests/delta/comparison/0011_filter_dither_6delta_x_dither.t) |
| D6-19 | CPU blue noise records the corresponding keep index and mask. | [tests/delta/comparison/0012_filter_dither_6delta_bluenoise.t](../../../tests/delta/comparison/0012_filter_dither_6delta_bluenoise.t) |
| D6-20 | `skip` leaves the following sample unchanged by a kept pixel for Jajuni, Stucki, Burkes, and the three Sierra policies. | [tests/delta/comparison/0014_filter_dither_6delta_diffusion_skip.t](../../../tests/delta/comparison/0014_filter_dither_6delta_diffusion_skip.t) |
| D6-21 | High-level forced GPU blue noise attaches an exact-size retained plane and returns the expected keep index/mask; requires a GPU-capable build and device. | [tests/delta/comparison/0016_filter_dither_6delta_bluenoise_gpu_exact_plane.t](../../../tests/delta/comparison/0016_filter_dither_6delta_bluenoise_gpu_exact_plane.t) |
| D6-22 | GPU `auto` permits origin-aware CPU fallback for an offset retained plane. | [tests/delta/comparison/0017_filter_dither_6delta_bluenoise_gpu_auto_fallback.t](../../../tests/delta/comparison/0017_filter_dither_6delta_bluenoise_gpu_auto_fallback.t) |
| D6-24 | Environment tuning alone does not enable delta; explicit suffixes override it; repeated delta resets tuning; invalid arguments preserve parameters; invalid base environment falls back to full. | [tests/encoder/pipeline/0039_filter_encode_accumulation_buffer.t](../../../tests/encoder/pipeline/0039_filter_encode_accumulation_buffer.t) |
| D6-25 | Extended float centroids remain unrounded; first/repeated/invalidated frames respectively paint, keep and repaint; changed content then repaints, with retained RGB matching decoded RGB101. | [tests/delta/palette/0001_delta_float_centroid.t](../../../tests/delta/palette/0001_delta_float_centroid.t) |
| D6-26 | A genuinely byte-eligible no-dither request retains its byte route and the same retained-plane lifecycle. | [tests/delta/palette/0002_delta_byte_control.t](../../../tests/delta/palette/0002_delta_byte_control.t) |
| D6-27 | Moving four-pixel updates on a six-pixel plane keep only the two-pixel overlap; invalidation leaves untouched positions invalid. | [tests/delta/output-representation/0001_delta_plane_origin.t](../../../tests/delta/output-representation/0001_delta_plane_origin.t) |
| D6-28 | Source-mask holes remain unpainted and invalid while visible pixels participate in native delta. | [tests/delta/output-representation/0002_delta_source_mask.t](../../../tests/delta/output-representation/0002_delta_source_mask.t) |
| D6-29 | Observed multiworker dispatch encodes a 25-row image with a partial final band; repeated pixels keep and invalidation repaints. | [tests/delta/palette/0003_delta_parallel_rows.t](../../../tests/delta/palette/0003_delta_parallel_rows.t) |
| D6-30 | FS native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0031_delta_residual_fs.t](../../../tests/delta/residual/0031_delta_residual_fs.t) |
| D6-31 | ATKINSON native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0032_delta_residual_atkinson.t](../../../tests/delta/residual/0032_delta_residual_atkinson.t) |
| D6-32 | JAJUNI native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0033_delta_residual_jajuni.t](../../../tests/delta/residual/0033_delta_residual_jajuni.t) |
| D6-33 | STUCKI native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0034_delta_residual_stucki.t](../../../tests/delta/residual/0034_delta_residual_stucki.t) |
| D6-34 | BURKES native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0035_delta_residual_burkes.t](../../../tests/delta/residual/0035_delta_residual_burkes.t) |
| D6-35 | SIERRA1 native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0036_delta_residual_sierra1.t](../../../tests/delta/residual/0036_delta_residual_sierra1.t) |
| D6-36 | SIERRA2 native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0037_delta_residual_sierra2.t](../../../tests/delta/residual/0037_delta_residual_sierra2.t) |
| D6-37 | SIERRA3 native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0038_delta_residual_sierra3.t](../../../tests/delta/residual/0038_delta_residual_sierra3.t) |
| D6-38 | LSO2 native float diffusion uses the retained color residual; `skip` leaves the following sample unchanged. | [tests/delta/residual/0039_delta_residual_lso2.t](../../../tests/delta/residual/0039_delta_residual_lso2.t) |
| D6-39 | A signed sample distinguishes thresholds 1 and 2 without intermediate clipping; the retained normalized value has an explicit tolerance. | [tests/delta/palette/0004_delta_signed_threshold.t](../../../tests/delta/palette/0004_delta_signed_threshold.t) |
| D6-40 | P3 OR delta rejects before output; the same encoder can recover with ordinary RGB serialization. | [tests/delta/output-representation/0003_delta_p3_or_recovery.t](../../../tests/delta/output-representation/0003_delta_p3_or_recovery.t) |
| D6-41 | AUTO and every admitted spatial method reach native delta through the high-level encoder, retaining the working centroid and repeated-frame wire colors. | [tests/delta/palette/0005_delta_spatial_dispatch.t](../../../tests/delta/palette/0005_delta_spatial_dispatch.t) |
| D6-42 | a: Original-source fidelity and threshold decisions differ from the biased query; invalid history and disabled delta paint normally, and the source is unchanged. | [tests/delta/palette/0006_delta_a_source.t](../../../tests/delta/palette/0006_delta_a_source.t) |
| D6-43 | x: Original-source fidelity and threshold decisions differ from the biased query; invalid history and disabled delta paint normally, and the source is unchanged. | [tests/delta/palette/0007_delta_x_source.t](../../../tests/delta/palette/0007_delta_x_source.t) |
| D6-44 | blue: Original-source fidelity and threshold decisions differ from the biased query; invalid history and disabled delta paint normally, and the source is unchanged. | [tests/delta/palette/0008_delta_blue_source.t](../../../tests/delta/palette/0008_delta_blue_source.t) |
| D6-45 | a: Native float first/repeated/invalidated/changed frames preserve exact emitted colors, with shifted origins and source-mask holes. | [tests/delta/palette/0009_delta_a_frames.t](../../../tests/delta/palette/0009_delta_a_frames.t) |
| D6-46 | x: Native float first/repeated/invalidated/changed frames preserve exact emitted colors, with shifted origins and source-mask holes. | [tests/delta/palette/0010_delta_x_frames.t](../../../tests/delta/palette/0010_delta_x_frames.t) |
| D6-47 | blue: Native float first/repeated/invalidated/changed frames preserve exact emitted colors, with shifted origins and source-mask holes. | [tests/delta/palette/0011_delta_blue_frames.t](../../../tests/delta/palette/0011_delta_blue_frames.t) |
| D6-48 | a: Multiworker serialization preserves the native centroid and retained-plane lifecycle across full and partial bands. | [tests/delta/palette/0012_delta_a_workers.t](../../../tests/delta/palette/0012_delta_a_workers.t) |
| D6-49 | x: Multiworker serialization preserves the native centroid and retained-plane lifecycle across full and partial bands. | [tests/delta/palette/0013_delta_x_workers.t](../../../tests/delta/palette/0013_delta_x_workers.t) |
| D6-50 | blue: Multiworker serialization preserves the native centroid and retained-plane lifecycle across full and partial bands. | [tests/delta/palette/0014_delta_blue_workers.t](../../../tests/delta/palette/0014_delta_blue_workers.t) |
| D6-51 | a: A byte-eligible positional request retains its byte route and emitted-color lifecycle. | [tests/delta/palette/0015_delta_a_byte.t](../../../tests/delta/palette/0015_delta_a_byte.t) |
| D6-52 | x: A byte-eligible positional request retains its byte route and emitted-color lifecycle. | [tests/delta/palette/0016_delta_x_byte.t](../../../tests/delta/palette/0016_delta_x_byte.t) |
| D6-53 | blue: A byte-eligible positional request retains its byte route and emitted-color lifecycle. | [tests/delta/palette/0017_delta_blue_byte.t](../../../tests/delta/palette/0017_delta_blue_byte.t) |
| D6-54 | a: Absolute band and plane origins, warmup/result-mask fences, invalid history and source-mask priority agree in raster and serpentine scans. | [tests/delta/palette/0018_delta_a_band.t](../../../tests/delta/palette/0018_delta_a_band.t) |
| D6-55 | x: Absolute band and plane origins, warmup/result-mask fences, invalid history and source-mask priority agree in raster and serpentine scans. | [tests/delta/palette/0019_delta_x_band.t](../../../tests/delta/palette/0019_delta_x_band.t) |
| D6-56 | blue: Absolute band and plane origins, warmup/result-mask fences, invalid history and source-mask priority agree in raster and serpentine scans. | [tests/delta/palette/0020_delta_blue_band.t](../../../tests/delta/palette/0020_delta_blue_band.t) |
| D6-57 | Built-in gray8 retains all 256 working entries and exact wire colors through repeat, invalidation, changed content and full/delta transitions. | [tests/delta/palette/0021_delta_builtin_slots.t](../../../tests/delta/palette/0021_delta_builtin_slots.t) |
| D6-58 | A 256-entry PAL retains every entry through the same native lifecycle. | [tests/delta/palette/0022_delta_map_slots.t](../../../tests/delta/palette/0022_delta_map_slots.t) |
| D6-59 | Duplicate and unused PAL entries retain their positions and values. | [tests/delta/palette/0023_delta_map_duplicates.t](../../../tests/delta/palette/0023_delta_map_duplicates.t) |
| D6-60 | Source-mask holes move independently of keeps; real index zero remains paintable. | [tests/delta/palette/0024_delta_builtin_mask.t](../../../tests/delta/palette/0024_delta_builtin_mask.t) |
| D6-61 | Full-size fixed maps combine source holes and delta keeps without taking a color slot. | [tests/delta/palette/0025_delta_map_mask.t](../../../tests/delta/palette/0025_delta_map_mask.t) |
| D6-62 | Overlapping updates use retained-plane coordinates and repaint invalid regions. | [tests/delta/palette/0026_delta_builtin_origin.t](../../../tests/delta/palette/0026_delta_builtin_origin.t) |
| D6-63 | Mapfile updates preserve the same shifted-plane lifecycle. | [tests/delta/palette/0027_delta_map_origin.t](../../../tests/delta/palette/0027_delta_map_origin.t) |
| D6-64 | Observed multiworker encoding preserves full-table keeps across six-row bands and a partial final band. | [tests/delta/palette/0028_delta_builtin_workers.t](../../../tests/delta/palette/0028_delta_builtin_workers.t) |
| D6-65 | Parallel mapfile delta consumes the shared result mask. | [tests/delta/palette/0029_delta_map_workers.t](../../../tests/delta/palette/0029_delta_map_workers.t) |
| D6-66 | Animation reuses fixed source/application state while retaining valid delta history. | [tests/delta/palette/0030_delta_builtin_animation.t](../../../tests/delta/palette/0030_delta_builtin_animation.t) |
| D6-67 | Animation reuses the mapfile application and its unchanged palette. | [tests/delta/palette/0031_delta_map_animation.t](../../../tests/delta/palette/0031_delta_map_animation.t) |
| D6-68 | A byte-eligible fixed table keeps its byte route and repaint behavior while recording wire-rounded history. | [tests/delta/palette/0032_delta_builtin_byte.t](../../../tests/delta/palette/0032_delta_builtin_byte.t) |
| D6-69 | A byte-eligible map keeps its byte route and repaint behavior while recording wire-rounded history. | [tests/delta/palette/0033_delta_map_byte.t](../../../tests/delta/palette/0033_delta_map_byte.t) |
| D6-70 | Native/byte/native transitions preserve valid wire history without explicit invalidation. | [tests/delta/palette/0034_delta_builtin_reuse.t](../../../tests/delta/palette/0034_delta_builtin_reuse.t) |
| D6-71 | Mapfile application transitions retain independent source ownership and wire history without invalidation. | [tests/delta/palette/0035_delta_map_reuse.t](../../../tests/delta/palette/0035_delta_map_reuse.t) |
| D6-72 | Retaining a color outside the new fixed table diffuses its residual and paints black index zero next to an omission. | [tests/delta/palette/0036_delta_fixed_fs_diffuse.t](../../../tests/delta/palette/0036_delta_fixed_fs_diffuse.t) |
| D6-73 | Skipping the retained-color residual paints white at the following sample. | [tests/delta/palette/0037_delta_fixed_fs_skip.t](../../../tests/delta/palette/0037_delta_fixed_fs_skip.t) |
| D6-74 | Mask allocation failure, disabled capture and missing history expose no stale active mask; retry and same-size reuse succeed. | [tests/delta/commit/0001_delta_fixed_mask_failure.t](../../../tests/delta/commit/0001_delta_fixed_mask_failure.t) |
| D6-75 | Image-map construction retains all 256 grayscale colors independently of active display history. | [tests/delta/palette/0038_delta_image_map_capacity.t](../../../tests/delta/palette/0038_delta_image_map_capacity.t) |
| D6-76 | Byte built-in tables record exact emitted RGB across parallel bands. | [tests/delta/palette/0039_delta_byte_workers.t](../../../tests/delta/palette/0039_delta_byte_workers.t) |
| D6-77 | Byte mapfile tables record the same wire-rounded parallel history. | [tests/delta/palette/0040_delta_byte_map_workers.t](../../../tests/delta/palette/0040_delta_byte_map_workers.t) |
| D6-78 | Capture follows float-backed percentage serialization and preserves the original palette. | [tests/delta/output-representation/0004_delta_wire_float_palette.t](../../../tests/delta/output-representation/0004_delta_wire_float_palette.t) |
| D6-79 | Disabled RGB capture creates no retained-RGB allocation and does not alter wire output. | [tests/delta/output-representation/0005_delta_wire_disabled.t](../../../tests/delta/output-representation/0005_delta_wire_disabled.t) |
| D6-80 | Generated byte palettes record the independently specified RGB101 expansion. | [tests/delta/output-representation/0006_delta_wire_generated.t](../../../tests/delta/output-representation/0006_delta_wire_generated.t) |
| D6-81 | Generated byte palettes use that same capture rule in the producer/writer pipeline. | [tests/delta/output-representation/0007_delta_wire_generated_workers.t](../../../tests/delta/output-representation/0007_delta_wire_generated_workers.t) |
| D6-82 | Byte none retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0041_delta_byte_none_keep.t](../../../tests/delta/palette/0041_delta_byte_none_keep.t) |
| D6-83 | Byte fs retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0042_delta_byte_fs_keep.t](../../../tests/delta/palette/0042_delta_byte_fs_keep.t) |
| D6-84 | Byte atkinson retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0043_delta_byte_atkinson_keep.t](../../../tests/delta/palette/0043_delta_byte_atkinson_keep.t) |
| D6-85 | Byte lso2 retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0044_delta_byte_lso2_keep.t](../../../tests/delta/palette/0044_delta_byte_lso2_keep.t) |
| D6-86 | Byte jajuni retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0045_delta_byte_jajuni_keep.t](../../../tests/delta/palette/0045_delta_byte_jajuni_keep.t) |
| D6-87 | Byte stucki retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0046_delta_byte_stucki_keep.t](../../../tests/delta/palette/0046_delta_byte_stucki_keep.t) |
| D6-88 | Byte burkes retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0047_delta_byte_burkes_keep.t](../../../tests/delta/palette/0047_delta_byte_burkes_keep.t) |
| D6-89 | Byte sierra1 retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0048_delta_byte_sierra1_keep.t](../../../tests/delta/palette/0048_delta_byte_sierra1_keep.t) |
| D6-90 | Byte sierra2 retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0049_delta_byte_sierra2_keep.t](../../../tests/delta/palette/0049_delta_byte_sierra2_keep.t) |
| D6-91 | Byte sierra3 retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0050_delta_byte_sierra3_keep.t](../../../tests/delta/palette/0050_delta_byte_sierra3_keep.t) |
| D6-92 | Byte a-dither retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0051_delta_byte_a_dither_keep.t](../../../tests/delta/palette/0051_delta_byte_a_dither_keep.t) |
| D6-93 | Byte x-dither retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0052_delta_byte_x_dither_keep.t](../../../tests/delta/palette/0052_delta_byte_x_dither_keep.t) |
| D6-94 | Byte bluenoise-dither retains the selected wire-color tie without changing the working palette. | [tests/delta/palette/0053_delta_byte_bluenoise_dither_keep.t](../../../tests/delta/palette/0053_delta_byte_bluenoise_dither_keep.t) |
| D6-95 | Byte comparison uses exact emitted candidates, preserves the query, rejects worse history and leaves standalone comparison unchanged. | [tests/delta/palette/0054_delta_byte_compare.t](../../../tests/delta/palette/0054_delta_byte_compare.t) |
| D6-96 | RGB/HLS byte syntax reuse preserves projected keeps; failure recovery clears the call hints. | [tests/delta/palette/0055_delta_byte_syntax_reuse.t](../../../tests/delta/palette/0055_delta_byte_syntax_reuse.t) |
| D6-97 | The or boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/delta/palette/0056_delta_byte_scope_or.t](../../../tests/delta/palette/0056_delta_byte_scope_or.t) |
| D6-98 | The body boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/delta/palette/0057_delta_byte_scope_body.t](../../../tests/delta/palette/0057_delta_byte_scope_body.t) |
| D6-99 | The source boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/delta/palette/0058_delta_byte_scope_source.t](../../../tests/delta/palette/0058_delta_byte_scope_source.t) |
| D6-100 | The output boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/delta/palette/0059_delta_byte_scope_output.t](../../../tests/delta/palette/0059_delta_byte_scope_output.t) |
| D6-101 | The float boundary does not acquire byte wire comparison; dither reuse and failure recovery clear the call hint. | [tests/delta/palette/0060_delta_byte_scope_float.t](../../../tests/delta/palette/0060_delta_byte_scope_float.t) |
| D6-102 | Generated byte frames paint, omit an identical frame, and repaint changed black content in serial output. | [tests/delta/palette/0061_delta_byte_repeat.t](../../../tests/delta/palette/0061_delta_byte_repeat.t) |
| D6-103 | Generated byte frames paint, omit an identical frame, and repaint changed black content through actual parallel serialization. | [tests/delta/palette/0062_delta_byte_repeat_workers.t](../../../tests/delta/palette/0062_delta_byte_repeat_workers.t) |
| D6-104 | Every byte candidate maps to the exact RGB101 expansion, checked by an independent nearest-grid enumeration and both comparison endpoints. | [tests/delta/palette/0063_delta_byte_grid.t](../../../tests/delta/palette/0063_delta_byte_grid.t) |
| D6-105 | Linear workspace keeps extended centroids and the paint/keep/invalidate/change lifecycle. | [tests/delta/palette/0064_delta_linear_frames.t](../../../tests/delta/palette/0064_delta_linear_frames.t) |
| D6-106 | Oklab FS retains the same wire-history lifecycle and unrounded chromatic centroids. | [tests/delta/palette/0065_delta_oklab_frames.t](../../../tests/delta/palette/0065_delta_oklab_frames.t) |
| D6-107 | CIELAB LSO2 retains the same wire-history lifecycle and unrounded chromatic centroids. | [tests/delta/palette/0066_delta_cielab_frames.t](../../../tests/delta/palette/0066_delta_cielab_frames.t) |
| D6-108 | DIN99d positional delta preserves extended centroids and the paint/keep/invalidate/change lifecycle. | [tests/delta/palette/0067_delta_din99d_frames.t](../../../tests/delta/palette/0067_delta_din99d_frames.t) |
| D6-109 | Linear FS transforms retained colors before diffusing their residual. | [tests/delta/palette/0068_delta_linear_diffuse.t](../../../tests/delta/palette/0068_delta_linear_diffuse.t) |
| D6-110 | Linear FS skip suppresses the retained-color residual. | [tests/delta/palette/0069_delta_linear_skip.t](../../../tests/delta/palette/0069_delta_linear_skip.t) |
| D6-111 | Linear working values use gamma comparison and threshold units; retained residuals use linear coordinates. | [tests/delta/palette/0070_delta_linear_compare.t](../../../tests/delta/palette/0070_delta_linear_compare.t) |
| D6-112 | Oklab working values use gamma comparison and threshold units; retained residuals use Oklab coordinates. | [tests/delta/palette/0071_delta_oklab_compare.t](../../../tests/delta/palette/0071_delta_oklab_compare.t) |
| D6-113 | CIELAB working values use gamma comparison and threshold units; retained residuals use CIELAB coordinates. | [tests/delta/palette/0072_delta_cielab_compare.t](../../../tests/delta/palette/0072_delta_cielab_compare.t) |
| D6-114 | DIN99d working values use gamma comparison and threshold units; retained residuals use DIN99d coordinates. | [tests/delta/palette/0073_delta_din99d_compare.t](../../../tests/delta/palette/0073_delta_din99d_compare.t) |
| D6-115 | A negative linear query remains extended through the gamma threshold comparison. | [tests/delta/palette/0074_delta_linear_extended.t](../../../tests/delta/palette/0074_delta_linear_extended.t) |
| D6-116 | Fixed-palette workspace changes preserve history and distinguish painted black index zero from omission. | [tests/delta/commit/0002_delta_workspace_fixed.t](../../../tests/delta/commit/0002_delta_workspace_fixed.t) |
| D6-117 | The same fixed-palette workspace changes preserve omission through actual parallel output. | [tests/delta/commit/0003_delta_workspace_workers.t](../../../tests/delta/commit/0003_delta_workspace_workers.t) |
| D6-118 | CIELAB generated-palette delta preserves source-mask holes and history validity. | [tests/delta/commit/0004_delta_workspace_mask.t](../../../tests/delta/commit/0004_delta_workspace_mask.t) |
| D6-119 | DIN99d delta respects plane origins and explicit invalidation. | [tests/delta/commit/0005_delta_workspace_origin.t](../../../tests/delta/commit/0005_delta_workspace_origin.t) |
| D6-120 | Linear generated delta preserves partial bands through actual worker output. | [tests/delta/commit/0006_delta_workspace_bands.t](../../../tests/delta/commit/0006_delta_workspace_bands.t) |
| D6-121 | A nonrepresentable DIN99d inverse declines the keep without recording a result bit. | [tests/delta/commit/0007_delta_workspace_domain.t](../../../tests/delta/commit/0007_delta_workspace_domain.t) |
| D6-122 | Delta without the RGB101 wire palette never takes the float keep path or acquires transformed-workspace semantics. | [tests/delta/commit/0008_delta_workspace_legacy.t](../../../tests/delta/commit/0008_delta_workspace_legacy.t) |
| D6-123 | A virtual DIN99d `(0,1,-1)` query rejects pale retained `(247,245,209)` in favor of black despite the inverse gamma distance. | [tests/delta/output-representation/0008_delta_virtual_guard.t](../../../tests/delta/output-representation/0008_delta_virtual_guard.t) |
| D6-124 | FS applies the same virtual-color guard and emits black instead of the keep index. | [tests/delta/output-representation/0009_delta_virtual_fs.t](../../../tests/delta/output-representation/0009_delta_virtual_fs.t) |
| D6-125 | Independently derived P3/sRGB gamma transforms preserve extended coordinates. | [tests/delta/palette/0075_delta_p3_gamma_transform.t](../../../tests/delta/palette/0075_delta_p3_gamma_transform.t) |
| D6-126 | Independent P3/linear transforms preserve negative and over-one components. | [tests/delta/palette/0076_delta_p3_linear_transform.t](../../../tests/delta/palette/0076_delta_p3_linear_transform.t) |
| D6-127 | Independent P3/Oklab transforms match normalized coordinates. | [tests/delta/palette/0077_delta_p3_oklab_transform.t](../../../tests/delta/palette/0077_delta_p3_oklab_transform.t) |
| D6-128 | Independent P3/CIELAB transforms match normalized coordinates. | [tests/delta/palette/0078_delta_p3_cielab_transform.t](../../../tests/delta/palette/0078_delta_p3_cielab_transform.t) |
| D6-129 | Independent P3/DIN99d transforms match normalized coordinates. | [tests/delta/palette/0079_delta_p3_din99d_transform.t](../../../tests/delta/palette/0079_delta_p3_din99d_transform.t) |
| D6-130 | P3 output threshold uses device coordinates without clipping the query. | [tests/delta/palette/0080_delta_p3_threshold.t](../../../tests/delta/palette/0080_delta_p3_threshold.t) |
| D6-131 | FS diffuses the retained P3 color after converting to linear working units. | [tests/delta/palette/0081_delta_p3_diffuse.t](../../../tests/delta/palette/0081_delta_p3_diffuse.t) |
| D6-132 | Skip omits the retained P3 residual while ordinary selection remains active. | [tests/delta/palette/0082_delta_p3_skip.t](../../../tests/delta/palette/0082_delta_p3_skip.t) |
| D6-133 | Prepared P3 definition ownership and invalidation are local to each palette. | [tests/delta/palette/0083_delta_p3_cache_lifetime.t](../../../tests/delta/palette/0083_delta_p3_cache_lifetime.t) |
| D6-134 | Invalid P3 transforms and rejected preparation preserve published output. | [tests/delta/commit/0009_delta_p3_cache_failure.t](../../../tests/delta/commit/0009_delta_p3_cache_failure.t) |
| D6-135 | Gamma P3 frames paint, retain, change and invalidate independent RGB values. | [tests/delta/palette/0084_delta_p3_gamma_frames.t](../../../tests/delta/palette/0084_delta_p3_gamma_frames.t) |
| D6-136 | Linear P3 frames paint, retain, change and invalidate independent RGB values. | [tests/delta/palette/0085_delta_p3_linear_frames.t](../../../tests/delta/palette/0085_delta_p3_linear_frames.t) |
| D6-137 | Oklab P3 frames paint, retain, change and invalidate independent RGB values. | [tests/delta/palette/0086_delta_p3_oklab_frames.t](../../../tests/delta/palette/0086_delta_p3_oklab_frames.t) |
| D6-138 | CIELAB P3 frames paint, retain, change and invalidate independent RGB values. | [tests/delta/palette/0087_delta_p3_cielab_frames.t](../../../tests/delta/palette/0087_delta_p3_cielab_frames.t) |
| D6-139 | DIN99d P3 frames paint, retain, change and invalidate independent RGB values. | [tests/delta/palette/0088_delta_p3_din99d_frames.t](../../../tests/delta/palette/0088_delta_p3_din99d_frames.t) |
| D6-140 | Actual parallel P3 body dispatch preserves wire colors and retained pixels. | [tests/delta/palette/0089_delta_p3_workers.t](../../../tests/delta/palette/0089_delta_p3_workers.t) |
| D6-141 | sRGB/P3 changes repaint identical numeric black; unchanged and invalid settings preserve history. | [tests/delta/palette/0090_delta_p3_definition_switch.t](../../../tests/delta/palette/0090_delta_p3_definition_switch.t) |
| D6-142 | An explicitly seeded P3 plane retains pixels and follows output-identity changes. | [tests/delta/palette/0091_delta_p3_seed_definition.t](../../../tests/delta/palette/0091_delta_p3_seed_definition.t) |
| D6-143 | Independent linear output inverse preserves extended coordinates. | [tests/delta/palette/0092_delta_linear_output_inverse.t](../../../tests/delta/palette/0092_delta_linear_output_inverse.t) |
| D6-144 | linear output thresholds use destination units and return working residuals. | [tests/delta/palette/0093_delta_linear_output_threshold.t](../../../tests/delta/palette/0093_delta_linear_output_threshold.t) |
| D6-145 | linear output FS propagates retained-color error in gamma working units. | [tests/delta/palette/0094_delta_linear_output_diffuse.t](../../../tests/delta/palette/0094_delta_linear_output_diffuse.t) |
| D6-146 | linear output FS skips retained-color error in gamma working units. | [tests/delta/palette/0095_delta_linear_output_skip.t](../../../tests/delta/palette/0095_delta_linear_output_skip.t) |
| D6-147 | linear output with gamma working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0096_delta_linear_output_gamma_frames.t](../../../tests/delta/palette/0096_delta_linear_output_gamma_frames.t) |
| D6-148 | linear output with linear working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0097_delta_linear_output_linear_frames.t](../../../tests/delta/palette/0097_delta_linear_output_linear_frames.t) |
| D6-149 | linear output with oklab working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0098_delta_linear_output_oklab_frames.t](../../../tests/delta/palette/0098_delta_linear_output_oklab_frames.t) |
| D6-150 | linear output with cielab working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0099_delta_linear_output_cielab_frames.t](../../../tests/delta/palette/0099_delta_linear_output_cielab_frames.t) |
| D6-151 | linear output with din99d working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0100_delta_linear_output_din99d_frames.t](../../../tests/delta/palette/0100_delta_linear_output_din99d_frames.t) |
| D6-152 | linear output preserves retained colors through actual worker dispatch. | [tests/delta/palette/0101_delta_linear_output_workers.t](../../../tests/delta/palette/0101_delta_linear_output_workers.t) |
| D6-153 | A seeded linear surface follows transfer/definition identity across fixed-palette frames. | [tests/delta/palette/0102_delta_linear_output_identity.t](../../../tests/delta/palette/0102_delta_linear_output_identity.t) |
| D6-154 | Independent smptec output inverse preserves extended coordinates. | [tests/delta/palette/0103_delta_smptec_output_inverse.t](../../../tests/delta/palette/0103_delta_smptec_output_inverse.t) |
| D6-155 | smptec output thresholds use destination units and return working residuals. | [tests/delta/palette/0104_delta_smptec_output_threshold.t](../../../tests/delta/palette/0104_delta_smptec_output_threshold.t) |
| D6-156 | smptec output FS propagates retained-color error in gamma working units. | [tests/delta/palette/0105_delta_smptec_output_diffuse.t](../../../tests/delta/palette/0105_delta_smptec_output_diffuse.t) |
| D6-157 | smptec output FS skips retained-color error in gamma working units. | [tests/delta/palette/0106_delta_smptec_output_skip.t](../../../tests/delta/palette/0106_delta_smptec_output_skip.t) |
| D6-158 | smptec output with gamma working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0107_delta_smptec_output_gamma_frames.t](../../../tests/delta/palette/0107_delta_smptec_output_gamma_frames.t) |
| D6-159 | smptec output with linear working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0108_delta_smptec_output_linear_frames.t](../../../tests/delta/palette/0108_delta_smptec_output_linear_frames.t) |
| D6-160 | smptec output with oklab working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0109_delta_smptec_output_oklab_frames.t](../../../tests/delta/palette/0109_delta_smptec_output_oklab_frames.t) |
| D6-161 | smptec output with cielab working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0110_delta_smptec_output_cielab_frames.t](../../../tests/delta/palette/0110_delta_smptec_output_cielab_frames.t) |
| D6-162 | smptec output with din99d working coordinates preserves wire values and paint/keep/change/invalidate state. | [tests/delta/palette/0111_delta_smptec_output_din99d_frames.t](../../../tests/delta/palette/0111_delta_smptec_output_din99d_frames.t) |
| D6-163 | smptec output preserves retained colors through actual worker dispatch. | [tests/delta/palette/0112_delta_smptec_output_workers.t](../../../tests/delta/palette/0112_delta_smptec_output_workers.t) |
| D6-164 | A seeded smptec surface follows transfer/definition identity across fixed-palette frames. | [tests/delta/palette/0113_delta_smptec_output_identity.t](../../../tests/delta/palette/0113_delta_smptec_output_identity.t) |
| D6-165 | Nonfinite/overflow and unsupported output inverses leave the destination unchanged. | [tests/delta/commit/0010_delta_output_inverse_failure.t](../../../tests/delta/commit/0010_delta_output_inverse_failure.t) |
| D6-166 | HLS gamma output in gamma workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0010_delta_hls_gamma_gamma_frames.t](../../../tests/delta/output-representation/0010_delta_hls_gamma_gamma_frames.t) |
| D6-167 | HLS gamma output in linear workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0011_delta_hls_gamma_linear_frames.t](../../../tests/delta/output-representation/0011_delta_hls_gamma_linear_frames.t) |
| D6-168 | HLS gamma output in oklab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0012_delta_hls_gamma_oklab_frames.t](../../../tests/delta/output-representation/0012_delta_hls_gamma_oklab_frames.t) |
| D6-169 | HLS gamma output in cielab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0013_delta_hls_gamma_cielab_frames.t](../../../tests/delta/output-representation/0013_delta_hls_gamma_cielab_frames.t) |
| D6-170 | HLS gamma output in din99d workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0014_delta_hls_gamma_din99d_frames.t](../../../tests/delta/output-representation/0014_delta_hls_gamma_din99d_frames.t) |
| D6-171 | HLS linear output in gamma workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0015_delta_hls_linear_gamma_frames.t](../../../tests/delta/output-representation/0015_delta_hls_linear_gamma_frames.t) |
| D6-172 | HLS linear output in linear workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0016_delta_hls_linear_linear_frames.t](../../../tests/delta/output-representation/0016_delta_hls_linear_linear_frames.t) |
| D6-173 | HLS linear output in oklab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0017_delta_hls_linear_oklab_frames.t](../../../tests/delta/output-representation/0017_delta_hls_linear_oklab_frames.t) |
| D6-174 | HLS linear output in cielab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0018_delta_hls_linear_cielab_frames.t](../../../tests/delta/output-representation/0018_delta_hls_linear_cielab_frames.t) |
| D6-175 | HLS linear output in din99d workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0019_delta_hls_linear_din99d_frames.t](../../../tests/delta/output-representation/0019_delta_hls_linear_din99d_frames.t) |
| D6-176 | HLS smptec output in gamma workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0020_delta_hls_smptec_gamma_frames.t](../../../tests/delta/output-representation/0020_delta_hls_smptec_gamma_frames.t) |
| D6-177 | HLS smptec output in linear workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0021_delta_hls_smptec_linear_frames.t](../../../tests/delta/output-representation/0021_delta_hls_smptec_linear_frames.t) |
| D6-178 | HLS smptec output in oklab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0022_delta_hls_smptec_oklab_frames.t](../../../tests/delta/output-representation/0022_delta_hls_smptec_oklab_frames.t) |
| D6-179 | HLS smptec output in cielab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0023_delta_hls_smptec_cielab_frames.t](../../../tests/delta/output-representation/0023_delta_hls_smptec_cielab_frames.t) |
| D6-180 | HLS smptec output in din99d workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0024_delta_hls_smptec_din99d_frames.t](../../../tests/delta/output-representation/0024_delta_hls_smptec_din99d_frames.t) |
| D6-181 | HLS p3 output in gamma workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0025_delta_hls_p3_gamma_frames.t](../../../tests/delta/output-representation/0025_delta_hls_p3_gamma_frames.t) |
| D6-182 | HLS p3 output in linear workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0026_delta_hls_p3_linear_frames.t](../../../tests/delta/output-representation/0026_delta_hls_p3_linear_frames.t) |
| D6-183 | HLS p3 output in oklab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0027_delta_hls_p3_oklab_frames.t](../../../tests/delta/output-representation/0027_delta_hls_p3_oklab_frames.t) |
| D6-184 | HLS p3 output in cielab workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0028_delta_hls_p3_cielab_frames.t](../../../tests/delta/output-representation/0028_delta_hls_p3_cielab_frames.t) |
| D6-185 | HLS p3 output in din99d workspace retains independent wire colors across paint, keep, change and invalidation. | [tests/delta/output-representation/0029_delta_hls_p3_din99d_frames.t](../../../tests/delta/output-representation/0029_delta_hls_p3_din99d_frames.t) |
| D6-186 | HLS P3 delta preserves emitted colors through actual worker dispatch. | [tests/delta/output-representation/0030_delta_hls_workers.t](../../../tests/delta/output-representation/0030_delta_hls_workers.t) |
| D6-187 | HLS candidate keep uses decoded HLS RGB rather than directly rounded RGB percentages. | [tests/delta/output-representation/0031_delta_hls_candidate_keep.t](../../../tests/delta/output-representation/0031_delta_hls_candidate_keep.t) |
| D6-188 | HLS candidate repaint uses decoded HLS RGB rather than directly rounded RGB percentages. | [tests/delta/output-representation/0032_delta_hls_candidate_repaint.t](../../../tests/delta/output-representation/0032_delta_hls_candidate_repaint.t) |
| D6-189 | HLS derived palette cache isolation preserves publication and ownership boundaries. | [tests/delta/output-representation/0033_delta_hls_cache_isolation.t](../../../tests/delta/output-representation/0033_delta_hls_cache_isolation.t) |
| D6-190 | HLS derived palette cache invalidation preserves publication and ownership boundaries. | [tests/delta/output-representation/0034_delta_hls_cache_invalidation.t](../../../tests/delta/output-representation/0034_delta_hls_cache_invalidation.t) |
| D6-191 | HLS fixed palettes preserve all 256 slots and mask state across frames. | [tests/delta/output-representation/0035_delta_hls_fixed_mask.t](../../../tests/delta/output-representation/0035_delta_hls_fixed_mask.t) |
| D6-192 | HLS fixed palettes preserve all 256 slots and origin state across frames. | [tests/delta/output-representation/0036_delta_hls_fixed_origin.t](../../../tests/delta/output-representation/0036_delta_hls_fixed_origin.t) |
| D6-193 | HLS fixed palettes preserve all 256 slots and reuse state across frames. | [tests/delta/output-representation/0037_delta_hls_fixed_reuse.t](../../../tests/delta/output-representation/0037_delta_hls_fixed_reuse.t) |
| D6-194 | HLS kept-color diffuse uses the retained displayed gray in FS diffusion. | [tests/delta/residual/0040_delta_hls_residual_diffuse.t](../../../tests/delta/residual/0040_delta_hls_residual_diffuse.t) |
| D6-195 | HLS kept-color skip uses the retained displayed gray in FS diffusion. | [tests/delta/residual/0041_delta_hls_residual_skip.t](../../../tests/delta/residual/0041_delta_hls_residual_skip.t) |
| D6-196 | RGB and HLS syntax changes preserve compatible history and compare their distinct emitted colors. | [tests/delta/output-representation/0038_delta_hls_syntax_reuse.t](../../../tests/delta/output-representation/0038_delta_hls_syntax_reuse.t) |
| D6-197 | HLS encode failure and successful reuse clear the call-scoped comparison hint. | [tests/delta/commit/0011_delta_hls_failure_recovery.t](../../../tests/delta/commit/0011_delta_hls_failure_recovery.t) |
| D6-198 | Six chromatic sectors, gray and a double-rounding witness match independent HLS fields and decoded percentages. | [tests/delta/output-representation/0039_delta_hls_field_model.t](../../../tests/delta/output-representation/0039_delta_hls_field_model.t) |
| D6-199 | Byte HLS none captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0040_delta_byte_hls_none.t](../../../tests/delta/output-representation/0040_delta_byte_hls_none.t) |
| D6-200 | Byte HLS fs captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0041_delta_byte_hls_fs.t](../../../tests/delta/output-representation/0041_delta_byte_hls_fs.t) |
| D6-201 | Byte HLS atkinson captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0042_delta_byte_hls_atkinson.t](../../../tests/delta/output-representation/0042_delta_byte_hls_atkinson.t) |
| D6-202 | Byte HLS jajuni captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0043_delta_byte_hls_jajuni.t](../../../tests/delta/output-representation/0043_delta_byte_hls_jajuni.t) |
| D6-203 | Byte HLS stucki captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0044_delta_byte_hls_stucki.t](../../../tests/delta/output-representation/0044_delta_byte_hls_stucki.t) |
| D6-204 | Byte HLS burkes captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0045_delta_byte_hls_burkes.t](../../../tests/delta/output-representation/0045_delta_byte_hls_burkes.t) |
| D6-205 | Byte HLS sierra1 captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0046_delta_byte_hls_sierra1.t](../../../tests/delta/output-representation/0046_delta_byte_hls_sierra1.t) |
| D6-206 | Byte HLS sierra2 captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0047_delta_byte_hls_sierra2.t](../../../tests/delta/output-representation/0047_delta_byte_hls_sierra2.t) |
| D6-207 | Byte HLS sierra3 captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0048_delta_byte_hls_sierra3.t](../../../tests/delta/output-representation/0048_delta_byte_hls_sierra3.t) |
| D6-208 | Byte HLS lso2 captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0049_delta_byte_hls_lso2.t](../../../tests/delta/output-representation/0049_delta_byte_hls_lso2.t) |
| D6-209 | Byte HLS a-dither captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0050_delta_byte_hls_a_dither.t](../../../tests/delta/output-representation/0050_delta_byte_hls_a_dither.t) |
| D6-210 | Byte HLS x-dither captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0051_delta_byte_hls_x_dither.t](../../../tests/delta/output-representation/0051_delta_byte_hls_x_dither.t) |
| D6-211 | Byte HLS bluenoise captures and compares independent emitted colors without float promotion. | [tests/delta/output-representation/0052_delta_byte_hls_bluenoise.t](../../../tests/delta/output-representation/0052_delta_byte_hls_bluenoise.t) |
| D6-212 | Byte HLS candidate keep compares the indexed HLS reconstruction rather than pre-wire RGB. | [tests/delta/output-representation/0053_delta_byte_hls_candidate_keep.t](../../../tests/delta/output-representation/0053_delta_byte_hls_candidate_keep.t) |
| D6-213 | Byte HLS candidate repaint compares the indexed HLS reconstruction rather than pre-wire RGB. | [tests/delta/output-representation/0054_delta_byte_hls_candidate_repaint.t](../../../tests/delta/output-representation/0054_delta_byte_hls_candidate_repaint.t) |
| D6-214 | Generated HLS frames preserves exact painted history, compatible keeps and byte eligibility. | [tests/delta/output-representation/0055_delta_byte_hls_frames.t](../../../tests/delta/output-representation/0055_delta_byte_hls_frames.t) |
| D6-215 | Generated HLS workers preserves exact painted history, compatible keeps and byte eligibility. | [tests/delta/output-representation/0056_delta_byte_hls_workers.t](../../../tests/delta/output-representation/0056_delta_byte_hls_workers.t) |
| D6-216 | Generated HLS byte-float preserves exact painted history, compatible keeps and byte eligibility. | [tests/delta/output-representation/0057_delta_byte_hls_byte_float.t](../../../tests/delta/output-representation/0057_delta_byte_hls_byte_float.t) |
| D6-217 | Generated HLS float-byte preserves exact painted history, compatible keeps and byte eligibility. | [tests/delta/output-representation/0058_delta_byte_hls_float_byte.t](../../../tests/delta/output-representation/0058_delta_byte_hls_float_byte.t) |
| D6-218 | Generated HLS mask preserves exact painted history, compatible keeps and byte eligibility. | [tests/delta/output-representation/0059_delta_byte_hls_mask.t](../../../tests/delta/output-representation/0059_delta_byte_hls_mask.t) |
| D6-219 | Generated HLS origin preserves exact painted history, compatible keeps and byte eligibility. | [tests/delta/output-representation/0060_delta_byte_hls_origin.t](../../../tests/delta/output-representation/0060_delta_byte_hls_origin.t) |
| D6-220 | Generated HLS invalidation preserves exact painted history, compatible keeps and byte eligibility. | [tests/delta/output-representation/0061_delta_byte_hls_invalidation.t](../../../tests/delta/output-representation/0061_delta_byte_hls_invalidation.t) |
| D6-221 | Byte HLS failure clears the borrowed comparison view before reuse. | [tests/delta/commit/0012_delta_byte_hls_failure.t](../../../tests/delta/commit/0012_delta_byte_hls_failure.t) |
| D6-222 | Independent byte HLS contexts rebuild comparison views after palette changes. | [tests/delta/output-representation/0062_delta_byte_hls_instances.t](../../../tests/delta/output-representation/0062_delta_byte_hls_instances.t) |
| D6-223 | Explicit exact snap is reflected in byte HLS candidate comparison. | [tests/delta/output-representation/0063_delta_byte_hls_snap.t](../../../tests/delta/output-representation/0063_delta_byte_hls_snap.t) |
| D6-224 | Legacy indexed HLS history follows the final float palette rather than its byte shadow. | [tests/delta/output-representation/0064_delta_byte_hls_float_palette.t](../../../tests/delta/output-representation/0064_delta_byte_hls_float_palette.t) |
| D6-225 | Selecting delta after monochrome rejects without changing options or retained history. | [tests/delta/palette/0114_delta_mono_then_delta.t](../../../tests/delta/palette/0114_delta_mono_then_delta.t) |
| D6-226 | Selecting monochrome after delta rejects without changing options or retained history. | [tests/delta/palette/0115_delta_delta_then_mono.t](../../../tests/delta/palette/0115_delta_delta_then_mono.t) |
| D6-227 | Environment-selected delta also rejects monochrome. | [tests/delta/palette/0116_delta_mono_environment.t](../../../tests/delta/palette/0116_delta_mono_environment.t) |
| D6-228 | Selecting full before monochrome clears delta history and allows monochrome. | [tests/delta/palette/0117_delta_mono_full_override.t](../../../tests/delta/palette/0117_delta_mono_full_override.t) |
| D6-229 | Final output validation rejects monochrome delta before writing and permits full-output reuse. | [tests/delta/palette/0118_delta_mono_output_reject.t](../../../tests/delta/palette/0118_delta_mono_output_reject.t) |
| D6-230 | Opaque black and white overwrite each other in delta output; a repeated frame preserves both pixels. | [tests/delta/palette/0119_delta_opaque_bw_frames.t](../../../tests/delta/palette/0119_delta_opaque_bw_frames.t) |
| D6-231 | Selecting delta after OR rejects without changing options or retained history. | [tests/delta/output-representation/0065_delta_or_then_delta.t](../../../tests/delta/output-representation/0065_delta_or_then_delta.t) |
| D6-232 | Selecting OR after delta rejects without changing options or retained history. | [tests/delta/palette/0120_delta_delta_then_or.t](../../../tests/delta/palette/0120_delta_delta_then_or.t) |
| D6-233 | Environment-selected delta also rejects OR output. | [tests/delta/output-representation/0066_delta_or_environment.t](../../../tests/delta/output-representation/0066_delta_or_environment.t) |
| D6-234 | Selecting full before OR clears delta history and permits OR output. | [tests/delta/output-representation/0067_delta_or_full_override.t](../../../tests/delta/output-representation/0067_delta_or_full_override.t) |
| D6-235 | Byte OR delta rejects before output; full-output reuse paints both black and white endpoints. | [tests/delta/output-representation/0068_delta_or_byte_recovery.t](../../../tests/delta/output-representation/0068_delta_or_byte_recovery.t) |
| D6-236 | Float OR delta rejects before output; full-output reuse preserves native float application and opaque endpoints. | [tests/delta/output-representation/0069_delta_or_float_recovery.t](../../../tests/delta/output-representation/0069_delta_or_float_recovery.t) |
| D6-237 | P3 OR delta rejects before output; full-output reuse preserves native P3 application and opaque endpoints. | [tests/delta/output-representation/0070_delta_or_p3_recovery.t](../../../tests/delta/output-representation/0070_delta_or_p3_recovery.t) |
| D6-238 | Selecting delta after DRCS rejects without changing options or retained history. | [tests/delta/output-representation/0071_delta_drcs_then_delta.t](../../../tests/delta/output-representation/0071_delta_drcs_then_delta.t) |
| D6-239 | Selecting DRCS after delta rejects before glyph settings or tile routing change. | [tests/delta/output-representation/0072_delta_delta_then_drcs.t](../../../tests/delta/output-representation/0072_delta_delta_then_drcs.t) |
| D6-240 | Environment-selected delta also rejects DRCS without changing options. | [tests/delta/output-representation/0073_delta_drcs_environment.t](../../../tests/delta/output-representation/0073_delta_drcs_environment.t) |
| D6-241 | Selecting full before DRCS clears delta history and permits glyph definitions. | [tests/delta/output-representation/0074_delta_drcs_full_override.t](../../../tests/delta/output-representation/0074_delta_drcs_full_override.t) |
| D6-242 | A rejected DRCS selection leaves the existing character-output file intact. | [tests/delta/output-representation/0075_delta_drcs_tile_untouched.t](../../../tests/delta/output-representation/0075_delta_drcs_tile_untouched.t) |
| D6-243 | Byte DRCS delta rejects before palette work and output; full glyph reuse preserves opaque endpoint values. | [tests/delta/output-representation/0076_delta_drcs_byte_recovery.t](../../../tests/delta/output-representation/0076_delta_drcs_byte_recovery.t) |
| D6-244 | Float DRCS delta rejects before palette work and output; full reuse retains native float application. | [tests/delta/output-representation/0077_delta_drcs_float_recovery.t](../../../tests/delta/output-representation/0077_delta_drcs_float_recovery.t) |
| D6-245 | P3 DRCS delta rejects before output; full reuse retains native P3 application and endpoint values. | [tests/delta/output-representation/0078_delta_drcs_p3_recovery.t](../../../tests/delta/output-representation/0078_delta_drcs_p3_recovery.t) |
| D6-246 | Selecting delta after automatic macro output preserves options and history on rejection. | [tests/delta/output-representation/0079_delta_macro_use_then_delta.t](../../../tests/delta/output-representation/0079_delta_macro_use_then_delta.t) |
| D6-247 | Selecting automatic macro output after delta rejects before changing selection. | [tests/delta/output-representation/0080_delta_delta_then_macro_use.t](../../../tests/delta/output-representation/0080_delta_delta_then_macro_use.t) |
| D6-248 | Selecting delta after a fixed-ID macro preserves options and history on rejection. | [tests/delta/output-representation/0081_delta_macro_number_then_delta.t](../../../tests/delta/output-representation/0081_delta_macro_number_then_delta.t) |
| D6-249 | Selecting a fixed macro ID after delta rejects without installing the ID. | [tests/delta/output-representation/0082_delta_delta_then_macro_number.t](../../../tests/delta/output-representation/0082_delta_delta_then_macro_number.t) |
| D6-250 | Environment-selected delta rejects automatic macro output. | [tests/delta/output-representation/0083_delta_macro_use_environment.t](../../../tests/delta/output-representation/0083_delta_macro_use_environment.t) |
| D6-251 | Environment-selected delta rejects fixed-ID macro output. | [tests/delta/output-representation/0084_delta_macro_number_environment.t](../../../tests/delta/output-representation/0084_delta_macro_number_environment.t) |
| D6-252 | Explicit full output clears delta history and allows automatic macros. | [tests/delta/output-representation/0085_delta_macro_use_full_override.t](../../../tests/delta/output-representation/0085_delta_macro_use_full_override.t) |
| D6-253 | Explicit full output clears delta history and allows fixed-ID macros. | [tests/delta/output-representation/0086_delta_macro_number_full_override.t](../../../tests/delta/output-representation/0086_delta_macro_number_full_override.t) |
| D6-254 | Byte macro delta rejects with zero output; full recovery defines and invokes the exact white pixel. | [tests/delta/output-representation/0087_delta_macro_byte_recovery.t](../../../tests/delta/output-representation/0087_delta_macro_byte_recovery.t) |
| D6-255 | Float macro delta rejects with zero output; full recovery retains native application and defines the exact white pixel. | [tests/delta/output-representation/0088_delta_macro_float_recovery.t](../../../tests/delta/output-representation/0088_delta_macro_float_recovery.t) |
| D6-256 | P3 macro delta rejects with zero output; full recovery retains native application and fixed-ID precedence over automatic invocation. | [tests/delta/output-representation/0089_delta_macro_p3_recovery.t](../../../tests/delta/output-representation/0089_delta_macro_p3_recovery.t) |
| D6-257 | Delta rejects a combined automatic and fixed-ID macro selection without changing either selector. | [tests/delta/output-representation/0090_delta_macro_combined_selection.t](../../../tests/delta/output-representation/0090_delta_macro_combined_selection.t) |
| D6-258 | Selecting delta after a PNG stdout target preserves destination and history on rejection. | [tests/writer/snapshot/0010_delta_stdout_then_delta.t](../../../tests/writer/snapshot/0010_delta_stdout_then_delta.t) |
| D6-259 | Selecting a PNG stdout target after delta rejects before changing output state. | [tests/writer/snapshot/0011_delta_delta_then_stdout.t](../../../tests/writer/snapshot/0011_delta_delta_then_stdout.t) |
| D6-260 | Selecting delta after a prefixed PNG path preserves the configured path and history. | [tests/writer/snapshot/0012_delta_prefix_then_delta.t](../../../tests/writer/snapshot/0012_delta_prefix_then_delta.t) |
| D6-261 | Selecting a prefixed PNG path after delta leaves destination ownership unchanged. | [tests/writer/snapshot/0013_delta_delta_then_prefix.t](../../../tests/writer/snapshot/0013_delta_delta_then_prefix.t) |
| D6-262 | Selecting delta after a PNG filename suffix preserves the configured path and history. | [tests/writer/snapshot/0014_delta_suffix_then_delta.t](../../../tests/writer/snapshot/0014_delta_suffix_then_delta.t) |
| D6-263 | Selecting a PNG filename suffix after delta leaves output state unchanged. | [tests/writer/snapshot/0015_delta_delta_then_suffix.t](../../../tests/writer/snapshot/0015_delta_delta_then_suffix.t) |
| D6-264 | Environment-selected delta rejects PNG snapshot selection. | [tests/writer/snapshot/0016_delta_environment.t](../../../tests/writer/snapshot/0016_delta_environment.t) |
| D6-265 | Explicit full clears delta history and permits PNG snapshots. | [tests/writer/snapshot/0017_delta_full_override.t](../../../tests/writer/snapshot/0017_delta_full_override.t) |
| D6-266 | Switching a PNG destination back to ordinary SIXEL permits delta. | [tests/writer/snapshot/0018_delta_sixel_switch.t](../../../tests/writer/snapshot/0018_delta_sixel_switch.t) |
| D6-267 | PNG-selected byte frames reject delta before palette work; full reuse paints exact white and black endpoints. | [tests/writer/snapshot/0019_delta_byte_recovery.t](../../../tests/writer/snapshot/0019_delta_byte_recovery.t) |
| D6-268 | PNG-selected float frames reject delta before palette work; full reuse preserves native application and exact endpoints. | [tests/writer/snapshot/0020_delta_float_recovery.t](../../../tests/writer/snapshot/0020_delta_float_recovery.t) |
| D6-269 | PNG-selected P3 frames reject delta before palette work; full reuse preserves native P3 application and exact endpoints. | [tests/writer/snapshot/0021_delta_p3_recovery.t](../../../tests/writer/snapshot/0021_delta_p3_recovery.t) |
| D6-270 | The PNG file frontend rejects delta before input/staging access and preserves its destination and history. | [tests/writer/snapshot/0022_delta_file_guard.t](../../../tests/writer/snapshot/0022_delta_file_guard.t) |
| D6-271 | The PNG stdout frontend rejects delta before input/staging access and preserves its descriptor and history. | [tests/writer/snapshot/0023_delta_stdout_guard.t](../../../tests/writer/snapshot/0023_delta_stdout_guard.t) |
| D6-272 | Forced GPU direct and Eytzinger lookup with `none` or `bluenoise` cannot select the appended omission key as a color; a black source chooses the real dark entry and leaves the keep mask clear. | [tests/lookup/dispatch/0003_reserved_key.t](../../../tests/lookup/dispatch/0003_reserved_key.t) |
| D6-273 | A forced GPU blue-noise white/dark-ramp/black/repeated-black sequence at threshold 8 satisfies independent per-pixel color bounds, and the retained RGB/validity exactly matches the decoded, composed canvas after every frame. The ramp requires lossy quantization, so a uniform-image shortcut cannot satisfy this check. | [tests/dither/bluenoise/0049_fade_history.t](../../../tests/dither/bluenoise/0049_fade_history.t) |
| D6-274 | CPU byte early keeps request refinement with zero painted pixels; a zero ceiling paints the full fixture black without changing the maximum or another encoder. | [tests/delta/palette/0121_delta_refine_byte.t](../../../tests/delta/palette/0121_delta_refine_byte.t) |
| D6-275 | CPU float early keeps support the same retained-history refinement contract. | [tests/delta/palette/0122_delta_refine_float.t](../../../tests/delta/palette/0122_delta_refine_float.t) |
| D6-276 | Forced GPU blue-noise early keeps support the same refinement contract when Metal is available. | [tests/delta/palette/0123_delta_refine_gpu.t](../../../tests/delta/palette/0123_delta_refine_gpu.t) |
| D6-277 | An exactly matching frame does not request refinement. | [tests/delta/palette/0124_delta_refine_exact.t](../../../tests/delta/palette/0124_delta_refine_exact.t) |
| D6-278 | Approximate early keeps request refinement even when another opaque pixel is painted; the painted count is one. | [tests/delta/palette/0125_delta_refine_partial.t](../../../tests/delta/palette/0125_delta_refine_partial.t) |
| D6-279 | Nonzero source transparency masks do not impersonate approximate delta keeps. | [tests/delta/output-representation/0091_delta_refine_source_mask.t](../../../tests/delta/output-representation/0091_delta_refine_source_mask.t) |

### GPU emitted-candidate comparison tests

| ID | GPU comparison contract | Test |
| --- | --- | --- |
| GW-01 | RGB direct lookup compares emitted colors, with ties, repaint and invalid history. | [tests/lookup/none/0001_wire_rgb_none.t](../../../tests/lookup/none/0001_wire_rgb_none.t) |
| GW-02 | RGB blue noise preserves the emitted-color keep comparison. | [tests/dither/bluenoise/0051_wire_rgb_bluenoise.t](../../../tests/dither/bluenoise/0051_wire_rgb_bluenoise.t) |
| GW-03 | Eytzinger selects from working entries and compares their emitted colors. | [tests/lookup/eytzinger/0001_wire_rgb_eytzinger.t](../../../tests/lookup/eytzinger/0001_wire_rgb_eytzinger.t) |
| GW-04 | HLS direct lookup uses the output owner's reconstructed RGB view. | [tests/lookup/none/0002_wire_hls_none.t](../../../tests/lookup/none/0002_wire_hls_none.t) |
| GW-05 | HLS blue noise uses the same reconstructed view. | [tests/dither/bluenoise/0052_wire_hls_bluenoise.t](../../../tests/dither/bluenoise/0052_wire_hls_bluenoise.t) |
| GW-06 | Standalone GPU application retains supplied-candidate semantics. | [tests/lookup/dispatch/0004_comparison_default.t](../../../tests/lookup/dispatch/0004_comparison_default.t) |
| GW-07 | A short comparison view is rejected before any GPU access. | [tests/lookup/dispatch/0005_comparison_short.t](../../../tests/lookup/dispatch/0005_comparison_short.t) |

The encode witnesses assert literal keep masks and decoded paints independently on CPU and forced GPU, preserve the working palette, and verify that comparison hints are cleared after the call. They establish byte RGB/HLS comparison behavior, not perceptual improvement for every video or support for legacy float-backed comparison.

### Quality regression tests

| ID | Observation and limit | Owning test |
| --- | --- | --- |
| D6-23 | Alternating black/white regions are composed onto a retained canvas and checked against brightness bounds, detecting accidental ordinary-color selection of the transparency key. This is a targeted stale-color regression, not an MS-SSIM guarantee or exact equality of every output pixel. | [tests/delta/comparison/0002_filter_encode_6delta_keycolor.t](../../../tests/delta/comparison/0002_filter_encode_6delta_keycolor.t) |


### Defensive and malformed-input tests

The [filter test directory](../../../tests/delta) also checks contradictory internal GPU enable/capability flags and exercises direct GPU request wiring. General malformed SIXEL and allocation coverage belongs to the [testing guide](../../testing/guide.md); it does not prove retained-screen correctness on a terminal.

### Coverage limits and manual validation

The exact early-match and threshold-255 boundaries, plane-size rejection/reset details, seeded-buffer lifecycle, broader float32/output-space scope, and missing-emitted-RGB fallback are audited against owning code. The table does not provide a separate focused automated assertion for every one of those dimensions. Likewise, the listed LSO2 test asserts `diffuse`, while D6-20's `skip` test covers only its six named policies. GPU tests may skip when the required device or backend is unavailable.

Receiver preservation, palette interpretation, external scroll/overlay recovery, delivery loss, and temporal appearance are application/terminal integration observations. Validate them with the actual receiver and a composed frame sequence; a passing single-frame decoder test cannot establish them.

### Progressive API contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| DP-01 | Bounded public calls report per-call and cumulative work, commit counts, complete audits and stale-source outcomes. | [tests/encoder/progressive-refinement/0069_public_statistics_resume.t](../../../tests/encoder/progressive-refinement/0069_public_statistics_resume.t) |
| DP-02 | A null output writes the complete repair to the configured destination. | [tests/encoder/progressive-refinement/0070_public_default_destination.t](../../../tests/encoder/progressive-refinement/0070_public_default_destination.t) |
| DP-03 | Invalid layout sizes are rejected and future structure tails remain untouched. | [tests/encoder/progressive-refinement/0071_public_layout_validation.t](../../../tests/encoder/progressive-refinement/0071_public_layout_validation.t) |
| DP-04 | Work counters reject overflow without output, and successful rebinding resets them. | [tests/encoder/progressive-refinement/0072_public_counter_exhaustion.t](../../../tests/encoder/progressive-refinement/0072_public_counter_exhaustion.t) |
| DP-05 | Envelope suppression reports unsupported repair without output or history changes. | [tests/encoder/progressive-refinement/0073_public_unsupported_controls.t](../../../tests/encoder/progressive-refinement/0073_public_unsupported_controls.t) |
| DP-06 | Busy admission and pending-update admission return distinct yield reasons without reading unowned image state. | [tests/encoder/progressive-transport/0027_public_admission_snapshot.t](../../../tests/encoder/progressive-transport/0027_public_admission_snapshot.t) |
| DP-07 | A normal update registered before output admission prevents repair emission. | [tests/encoder/progressive-transport/0025_step_pre_admission_priority.t](../../../tests/encoder/progressive-transport/0025_step_pre_admission_priority.t) |
| DP-08 | An admitted repair commits before a later normal update uses its history. | [tests/encoder/progressive-transport/0026_step_admitted_packet_order.t](../../../tests/encoder/progressive-transport/0026_step_admitted_packet_order.t) |
| DP-09 | A failed output callback invalidates retained history. | [tests/encoder/progressive-transport/0014_step_failed_output.t](../../../tests/encoder/progressive-transport/0014_step_failed_output.t) |

Additional [progressive tests](../../../tests/encoder) exercise canonical output projection, selection and recovery, budgeted packet construction, allocator failures, cancellation, callback reentry and ordinary-update ordering. These establish numeric and stream behavior; they do not measure perceptual pass curves, terminal presentation or GPU repair performance.

### Progressive presentation coverage

Presentation contracts are covered by:

- [tests/encoder/progressive-refinement/0074_still_capture_history.t](../../../tests/encoder/progressive-refinement/0074_still_capture_history.t): still capture history.
- [tests/encoder/progressive-refinement/0075_gif_default_frame.t](../../../tests/encoder/progressive-refinement/0075_gif_default_frame.t): gif default frame.
- [tests/encoder/progressive-refinement/0076_gif_composed_selection.t](../../../tests/encoder/progressive-refinement/0076_gif_composed_selection.t): gif composed selection.
- [tests/encoder/progressive-refinement/0077_gif_negative_selection.t](../../../tests/encoder/progressive-refinement/0077_gif_negative_selection.t): gif negative selection.
- [tests/encoder/progressive-refinement/0078_apng_poster_exclusion.t](../../../tests/encoder/progressive-refinement/0078_apng_poster_exclusion.t): apng poster exclusion.
- [tests/encoder/progressive-refinement/0079_apng_invalid_selection.t](../../../tests/encoder/progressive-refinement/0079_apng_invalid_selection.t): apng invalid selection.
- [tests/encoder/progressive-refinement/0080_still_8bit_capture.t](../../../tests/encoder/progressive-refinement/0080_still_8bit_capture.t): still 8bit capture.
- [tests/encoder/progressive-transport/0028_presentation_option_atomicity.t](../../../tests/encoder/progressive-transport/0028_presentation_option_atomicity.t): presentation option atomicity.

Scheduler and ordered output contracts are covered by:

- [tests/encoder/progressive-transport/0029_transport_capacity_admission.t](../../../tests/encoder/progressive-transport/0029_transport_capacity_admission.t): transport capacity admission.
- [tests/encoder/progressive-transport/0030_deadline_admission.t](../../../tests/encoder/progressive-transport/0030_deadline_admission.t): deadline admission.
- [tests/encoder/progressive-refinement/0081_source_clock_preserved.t](../../../tests/encoder/progressive-refinement/0081_source_clock_preserved.t): source clock preserved.
- [tests/encoder/progressive-refinement/0082_zero_and_ignored_delay.t](../../../tests/encoder/progressive-refinement/0082_zero_and_ignored_delay.t): zero and ignored delay.
- [tests/encoder/progressive-refinement/0083_delay_cancellation.t](../../../tests/encoder/progressive-refinement/0083_delay_cancellation.t): delay cancellation.
- [tests/encoder/progressive-transport/0031_ordered_transport.t](../../../tests/encoder/progressive-transport/0031_ordered_transport.t): ordered transport.
- [tests/encoder/progressive-transport/0032_transport_write_failure.t](../../../tests/encoder/progressive-transport/0032_transport_write_failure.t): transport write failure.
- [tests/encoder/progressive-refinement/0084_ignored_delay_final_repair.t](../../../tests/encoder/progressive-refinement/0084_ignored_delay_final_repair.t): ignored delay final repair.

CLI selection and precedence are covered by:

- [tests/cli/progressive/0001_environment_base.t](../../../tests/cli/progressive/0001_environment_base.t): environment base selection.
- [tests/cli/progressive/0002_source_environment_precedence.t](../../../tests/cli/progressive/0002_source_environment_precedence.t): explicit source index precedence.
- [tests/cli/progressive/0003_policy_reset_animation.t](../../../tests/cli/progressive/0003_policy_reset_animation.t): policy reset restores animation loading.
- [tests/cli/progressive/0004_explicit_static_preserved.t](../../../tests/cli/progressive/0004_explicit_static_preserved.t): explicit static survives policy reset.
- [tests/cli/progressive/0005_progressive_disable_precedence.t](../../../tests/cli/progressive/0005_progressive_disable_precedence.t): explicit progressive off overrides environment.
- [tests/cli/progressive/0006_source_environment_selection.t](../../../tests/cli/progressive/0006_source_environment_selection.t): source environment selects the composed frame.


### Presentation limit coverage

- [tests/encoder/progressive-transport/0034_limit_option_values.t](../../../tests/encoder/progressive-transport/0034_limit_option_values.t): limit option values.
- [tests/encoder/progressive-transport/0035_limit_option_atomicity.t](../../../tests/encoder/progressive-transport/0035_limit_option_atomicity.t): limit option atomicity.
- [tests/encoder/progressive-transport/0036_presentation_pass_limit.t](../../../tests/encoder/progressive-transport/0036_presentation_pass_limit.t): presentation pass limit.
- [tests/cli/output-options/0018_update_progressive_max_seconds_image_regression.t](../../../tests/cli/output-options/0018_update_progressive_max_seconds_image_regression.t): short and environment time-budget routes reach both presentation consumers with deterministic one-pass output.
- [tests/encoder/progressive-transport/0037_presentation_time_limit.t](../../../tests/encoder/progressive-transport/0037_presentation_time_limit.t): presentation time limit.
- [tests/encoder/progressive-transport/0038_presentation_admitted_deadline.t](../../../tests/encoder/progressive-transport/0038_presentation_admitted_deadline.t): presentation admitted deadline.
- [tests/encoder/progressive-transport/0039_presentation_zero_limits.t](../../../tests/encoder/progressive-transport/0039_presentation_zero_limits.t): presentation zero limits.
- [tests/encoder/progressive-transport/0040_presentation_rebind_limits.t](../../../tests/encoder/progressive-transport/0040_presentation_rebind_limits.t): presentation rebind limits.
- [tests/encoder/progressive-transport/0041_animation_pass_limit_clock.t](../../../tests/encoder/progressive-transport/0041_animation_pass_limit_clock.t): animation pass limit clock.
- [tests/encoder/progressive-refinement/0107_animation_tail_time_limit.t](../../../tests/encoder/progressive-refinement/0107_animation_tail_time_limit.t): animation tail time limit.
- [tests/encoder/progressive-transport/0042_presentation_final_no_noise.t](../../../tests/encoder/progressive-transport/0042_presentation_final_no_noise.t): presentation final no noise.
- [tests/encoder/progressive-transport/0043_video_pair_disjoint_palettes.t](../../../tests/encoder/progressive-transport/0043_video_pair_disjoint_palettes.t): disjoint paired images retain independent palette colors and one source interval.
- [tests/encoder/progressive-transport/0044_video_stable_residual_age.t](../../../tests/encoder/progressive-transport/0044_video_stable_residual_age.t): unchanged targets carry residual age while changed targets reset it.
- [tests/encoder/progressive-transport/0045_video_latest_source_revalidation.t](../../../tests/encoder/progressive-transport/0045_video_latest_source_revalidation.t): the next repair uses the latest target rather than a stale source.
- [tests/encoder/progressive-transport/0046_video_structure_guard.t](../../../tests/encoder/progressive-transport/0046_video_structure_guard.t): reject a pixel improvement that worsens the declared spatial luminance loss.
- [tests/encoder/progressive-transport/0047_video_pair_deadline.t](../../../tests/encoder/progressive-transport/0047_video_pair_deadline.t): an exhausted pair slot does not emit or claim full convergence.
- [tests/encoder/progressive-transport/0048_video_pair_write_failure.t](../../../tests/encoder/progressive-transport/0048_video_pair_write_failure.t): repair write failure invalidates retained history.
- [tests/encoder/progressive-transport/0049_video_final_full_domain.t](../../../tests/encoder/progressive-transport/0049_video_final_full_domain.t): final refinement includes positions excluded from the paired repair.
- [tests/encoder/progressive-refinement/0108_progressive_boolean_syntax.t](../../../tests/encoder/progressive-refinement/0108_progressive_boolean_syntax.t): accept exact boolean long/short syntax and atomically reject textual or malformed values.

CLI precedence and selection reset are verified by [tests/cli/progressive/0007_limit_cli_environment_precedence.t](../../../tests/cli/progressive/0007_limit_cli_environment_precedence.t) and [tests/cli/progressive/0008_limit_selection_reset.t](../../../tests/cli/progressive/0008_limit_selection_reset.t).

### Progressive GPU application coverage

These behavioral tests exercise deliberate-margin fixtures and canonical acceptance; they do not calibrate performance, automatic admission thresholds or perceptual quality. Metal-dependent cases skip when no device is available. Residual scan and global sampling are covered separately below; GPU summaries are never used to certify convergence.

| ID | Contract | Owning test |
| --- | --- | --- |
| PG-01 | Forced preparation uses Metal when available and reports unsupported otherwise. | [tests/encoder/progressive-refinement/0067_prepare_force_policy.t](../../../tests/encoder/progressive-refinement/0067_prepare_force_policy.t) |
| PG-02 | Reject malformed lookup requests without publishing matches. | [tests/encoder/progressive-refinement/0118_gpu_lookup_validation.t](../../../tests/encoder/progressive-refinement/0118_gpu_lookup_validation.t) |
| PG-03 | Search ordered signed float palettes with first-slot ties. | [tests/encoder/progressive-refinement/0119_gpu_lookup_float_palette.t](../../../tests/encoder/progressive-refinement/0119_gpu_lookup_float_palette.t) |
| PG-04 | Isolate owners and refresh reused query and palette addresses. | [tests/encoder/progressive-refinement/0120_gpu_lookup_cache_isolation.t](../../../tests/encoder/progressive-refinement/0120_gpu_lookup_cache_isolation.t) |
| PG-05 | Preserve output on failure, allow AUTO fallback and recover. | [tests/encoder/progressive-refinement/0121_gpu_lookup_failure_fallback.t](../../../tests/encoder/progressive-refinement/0121_gpu_lookup_failure_fallback.t) |
| PG-06 | Match CPU zero-noise masks on sparse offset input. | [tests/encoder/progressive-refinement/0122_gpu_application_none.t](../../../tests/encoder/progressive-refinement/0122_gpu_application_none.t) |
| PG-07 | Preserve absolute noise phase, protected pixels and keep masks. | [tests/encoder/progressive-refinement/0123_gpu_application_bluenoise.t](../../../tests/encoder/progressive-refinement/0123_gpu_application_bluenoise.t) |
| PG-08 | Resume a forced GPU palette search with one-distance budgets. | [tests/encoder/progressive-refinement/0124_gpu_application_budget_resume.t](../../../tests/encoder/progressive-refinement/0124_gpu_application_budget_resume.t) |
| PG-09 | Reject equal canonical error even when GPU float distance rounds down. | [tests/encoder/progressive-refinement/0125_gpu_application_canonical_guard.t](../../../tests/encoder/progressive-refinement/0125_gpu_application_canonical_guard.t) |
| PG-10 | Commit decoded GPU repair colors only at the bound plane origin. | [tests/encoder/progressive-refinement/0126_gpu_public_commit.t](../../../tests/encoder/progressive-refinement/0126_gpu_public_commit.t) |
| PG-11 | Rebind to a new source and switch from GPU to CPU without stale data. | [tests/encoder/progressive-refinement/0127_gpu_public_rebind_transition.t](../../../tests/encoder/progressive-refinement/0127_gpu_public_rebind_transition.t) |
| PG-12 | Admit AUTO by selected population independently of dispatch size. | [tests/encoder/progressive-transport/0051_gpu_public_auto_admission.t](../../../tests/encoder/progressive-transport/0051_gpu_public_auto_admission.t) |
| PG-13 | Invalidate retained history after a GPU repair writer failure. | [tests/encoder/progressive-refinement/0128_gpu_public_write_failure.t](../../../tests/encoder/progressive-refinement/0128_gpu_public_write_failure.t) |
| PG-14 | Cancel after GPU selection and after GPU lookup, resuming each without early output. | [tests/encoder/progressive-refinement/0129_gpu_public_cancel_resume.t](../../../tests/encoder/progressive-refinement/0129_gpu_public_cancel_resume.t) |
| PG-15 | Retry a failed GPU batch on CPU while accounting for its work budget. | [tests/encoder/progressive-refinement/0130_gpu_application_auto_fallback.t](../../../tests/encoder/progressive-refinement/0130_gpu_application_auto_fallback.t) |

### Progressive GPU selection coverage

GPU residual masks are corrected against the canonical audit before global sampling. Sample strata and video weights do not depend on dispatch size. These tests verify functional contracts, not performance or perceptual calibration.

| ID | Contract | Owning test |
| --- | --- | --- |
| PS-01 | Preserve global sample positions and populations across filtered batches. | [tests/encoder/progressive-refinement/0131_gpu_global_strata.t](../../../tests/encoder/progressive-refinement/0131_gpu_global_strata.t) |
| PS-02 | Reject malformed global sampling requests before GPU submission. | [tests/encoder/progressive-refinement/0132_gpu_global_validation.t](../../../tests/encoder/progressive-refinement/0132_gpu_global_validation.t) |
| PS-03 | Preserve more than 128 global samples across bounded GPU dispatches. | [tests/encoder/progressive-refinement/0133_gpu_selection_global_samples.t](../../../tests/encoder/progressive-refinement/0133_gpu_selection_global_samples.t) |
| PS-04 | Resume GPU selection and sampling with one-unit work budgets. | [tests/encoder/progressive-refinement/0134_gpu_selection_budget_resume.t](../../../tests/encoder/progressive-refinement/0134_gpu_selection_budget_resume.t) |
| PS-05 | Restore canonical residuals omitted by a rounded float threshold. | [tests/encoder/progressive-refinement/0135_gpu_selection_threshold_guard.t](../../../tests/encoder/progressive-refinement/0135_gpu_selection_threshold_guard.t) |
| PS-06 | Preserve luminance priority masks and weighted sample populations. | [tests/encoder/progressive-transport/0052_gpu_selection_video_priority.t](../../../tests/encoder/progressive-transport/0052_gpu_selection_video_priority.t) |
| PS-07 | Account failed GPU selection work before AUTO resumes on CPU. | [tests/encoder/progressive-refinement/0136_gpu_selection_auto_fallback.t](../../../tests/encoder/progressive-refinement/0136_gpu_selection_auto_fallback.t) |
| PS-08 | Keep frozen mask thresholds and protected candidates on the GPU path. | [tests/encoder/progressive-refinement/0137_gpu_selection_ladder.t](../../../tests/encoder/progressive-refinement/0137_gpu_selection_ladder.t) |
| PS-09 | Reject stale GPU scan work after an ordinary update and rebind safely. | [tests/encoder/progressive-refinement/0138_gpu_scan_update_rebind.t](../../../tests/encoder/progressive-refinement/0138_gpu_scan_update_rebind.t) |
| PS-10 | Use GPU scan and lookup for disjoint paired images with independent palettes. | [tests/encoder/progressive-transport/0053_gpu_video_pair.t](../../../tests/encoder/progressive-transport/0053_gpu_video_pair.t) |

### Progressive GPU operation ordering coverage

Metal batches complete synchronously while the encoder operation gate owns the context. A waiting ordinary update or rebind receives the gate at the next preparation checkpoint; after output admission, the packet and retained-history commit finish first. Context teardown also waits for that gate, keeping source snapshots, host work and device caches alive through the active operation. These tests register an actual waiting thread at a completed GPU batch or the output-admission boundary. They do not inject a driver failure or suspend an in-flight Metal command.

| ID | Contract | Owning test |
| --- | --- | --- |
| PO-01 | Yield to a registered ordinary update after a real GPU scan. | [tests/encoder/progressive-refinement/0139_gpu_scan_pending_update.t](../../../tests/encoder/progressive-refinement/0139_gpu_scan_pending_update.t) |
| PO-02 | Yield to a registered ordinary update after a real GPU lookup. | [tests/encoder/progressive-refinement/0140_gpu_lookup_pending_update.t](../../../tests/encoder/progressive-refinement/0140_gpu_lookup_pending_update.t) |
| PO-03 | Suppress a prepared GPU repair when an update precedes admission. | [tests/encoder/progressive-transport/0054_gpu_pending_before_admission.t](../../../tests/encoder/progressive-transport/0054_gpu_pending_before_admission.t) |
| PO-04 | Commit an admitted GPU repair before a waiting ordinary update. | [tests/encoder/progressive-transport/0055_gpu_pending_after_admission.t](../../../tests/encoder/progressive-transport/0055_gpu_pending_after_admission.t) |
| PO-05 | Keep GPU binding storage alive until a waiting rebind owns the gate. | [tests/encoder/progressive-refinement/0141_gpu_pending_rebind.t](../../../tests/encoder/progressive-refinement/0141_gpu_pending_rebind.t) |
| PO-06 | Finish an admitted GPU repair before a waiting context destruction. | [tests/encoder/progressive-refinement/0142_gpu_pending_destroy.t](../../../tests/encoder/progressive-refinement/0142_gpu_pending_destroy.t) |

### Progressive GPU color interpretation coverage

The matrix below exercises the public repair API with all five working spaces, the four supported output interpretations and both RGB and HLS wire definitions. Each case uses an offset source rectangle with transparent holes and runs with zero noise and positional blue noise. Both GPU selection and lookup must actually execute under FORCE and admitted AUTO; OFF and AUTO below its population threshold must remain on CPU. The AUTO cases inherit encoder policy and distinguish the four eligible pixels from the six-pixel rectangle. The packet is independently decoded in the selected output coordinates and compared exactly with retained history; omitted pixels and the outside halo must remain unchanged. A fresh canonical audit must show strictly reduced error at every painted position in the selected working space.

Repair source preparation normalizes packed alpha before producing typed float samples, independently of the ordinary encoder's palette, lookup and GPU restrictions. Ordinary Display P3 encoding retains its own GPU rejection; support in this repair API does not expand the ordinary encoder or CLI baseline path.

The CPU comparison establishes backend consistency, not an independent reference for color-space conversion or a perceptual quality score. Exact packet/history agreement is checked only for fixtures whose nearest and runner-up lookup distances differ by more than `32 * FLT_EPSILON * max(1, runner_up_distance)`, bounding float subtraction, multiplication and accumulation error for both candidates (including fused evaluation). Query coordinates are the shared float positional queries; the oracle computes their distances in double. This margin is asserted at runtime before accepting exact agreement. General conversion correctness and numerical methods remain owned by [color-space handling](../colorspace-handling/README.md) and the [testing guide](../../testing/guide.md#intermediate-floating-point-error). Output interpretation is checked through API metadata; the SIXEL packet does not announce Display P3 or another wide-gamut interpretation.

| ID | Working space | Output coordinates | Wire definition | Owning test |
| --- | --- | --- | --- | --- |
| PC-01 | `gamma` | `gamma` | RGB | [tests/encoder/progressive-refinement/0143_gpu_gamma_gamma_rgb.t](../../../tests/encoder/progressive-refinement/0143_gpu_gamma_gamma_rgb.t) |
| PC-02 | `gamma` | `gamma` | HLS | [tests/encoder/progressive-refinement/0144_gpu_gamma_gamma_hls.t](../../../tests/encoder/progressive-refinement/0144_gpu_gamma_gamma_hls.t) |
| PC-03 | `gamma` | `linear` | RGB | [tests/encoder/progressive-refinement/0145_gpu_gamma_linear_rgb.t](../../../tests/encoder/progressive-refinement/0145_gpu_gamma_linear_rgb.t) |
| PC-04 | `gamma` | `linear` | HLS | [tests/encoder/progressive-refinement/0146_gpu_gamma_linear_hls.t](../../../tests/encoder/progressive-refinement/0146_gpu_gamma_linear_hls.t) |
| PC-05 | `gamma` | `smpte-c` | RGB | [tests/encoder/progressive-refinement/0147_gpu_gamma_smptec_rgb.t](../../../tests/encoder/progressive-refinement/0147_gpu_gamma_smptec_rgb.t) |
| PC-06 | `gamma` | `smpte-c` | HLS | [tests/encoder/progressive-refinement/0148_gpu_gamma_smptec_hls.t](../../../tests/encoder/progressive-refinement/0148_gpu_gamma_smptec_hls.t) |
| PC-07 | `gamma` | `display-p3` | RGB | [tests/encoder/progressive-refinement/0149_gpu_gamma_display_p3_rgb.t](../../../tests/encoder/progressive-refinement/0149_gpu_gamma_display_p3_rgb.t) |
| PC-08 | `gamma` | `display-p3` | HLS | [tests/encoder/progressive-refinement/0150_gpu_gamma_display_p3_hls.t](../../../tests/encoder/progressive-refinement/0150_gpu_gamma_display_p3_hls.t) |
| PC-09 | `linear` | `gamma` | RGB | [tests/encoder/progressive-refinement/0151_gpu_linear_gamma_rgb.t](../../../tests/encoder/progressive-refinement/0151_gpu_linear_gamma_rgb.t) |
| PC-10 | `linear` | `gamma` | HLS | [tests/encoder/progressive-refinement/0152_gpu_linear_gamma_hls.t](../../../tests/encoder/progressive-refinement/0152_gpu_linear_gamma_hls.t) |
| PC-11 | `linear` | `linear` | RGB | [tests/encoder/progressive-refinement/0153_gpu_linear_linear_rgb.t](../../../tests/encoder/progressive-refinement/0153_gpu_linear_linear_rgb.t) |
| PC-12 | `linear` | `linear` | HLS | [tests/encoder/progressive-refinement/0154_gpu_linear_linear_hls.t](../../../tests/encoder/progressive-refinement/0154_gpu_linear_linear_hls.t) |
| PC-13 | `linear` | `smpte-c` | RGB | [tests/encoder/progressive-refinement/0155_gpu_linear_smptec_rgb.t](../../../tests/encoder/progressive-refinement/0155_gpu_linear_smptec_rgb.t) |
| PC-14 | `linear` | `smpte-c` | HLS | [tests/encoder/progressive-refinement/0156_gpu_linear_smptec_hls.t](../../../tests/encoder/progressive-refinement/0156_gpu_linear_smptec_hls.t) |
| PC-15 | `linear` | `display-p3` | RGB | [tests/encoder/progressive-refinement/0157_gpu_linear_display_p3_rgb.t](../../../tests/encoder/progressive-refinement/0157_gpu_linear_display_p3_rgb.t) |
| PC-16 | `linear` | `display-p3` | HLS | [tests/encoder/progressive-refinement/0158_gpu_linear_display_p3_hls.t](../../../tests/encoder/progressive-refinement/0158_gpu_linear_display_p3_hls.t) |
| PC-17 | `oklab` | `gamma` | RGB | [tests/encoder/progressive-refinement/0159_gpu_oklab_gamma_rgb.t](../../../tests/encoder/progressive-refinement/0159_gpu_oklab_gamma_rgb.t) |
| PC-18 | `oklab` | `gamma` | HLS | [tests/encoder/progressive-refinement/0160_gpu_oklab_gamma_hls.t](../../../tests/encoder/progressive-refinement/0160_gpu_oklab_gamma_hls.t) |
| PC-19 | `oklab` | `linear` | RGB | [tests/encoder/progressive-refinement/0161_gpu_oklab_linear_rgb.t](../../../tests/encoder/progressive-refinement/0161_gpu_oklab_linear_rgb.t) |
| PC-20 | `oklab` | `linear` | HLS | [tests/encoder/progressive-refinement/0162_gpu_oklab_linear_hls.t](../../../tests/encoder/progressive-refinement/0162_gpu_oklab_linear_hls.t) |
| PC-21 | `oklab` | `smpte-c` | RGB | [tests/encoder/progressive-refinement/0163_gpu_oklab_smptec_rgb.t](../../../tests/encoder/progressive-refinement/0163_gpu_oklab_smptec_rgb.t) |
| PC-22 | `oklab` | `smpte-c` | HLS | [tests/encoder/progressive-refinement/0164_gpu_oklab_smptec_hls.t](../../../tests/encoder/progressive-refinement/0164_gpu_oklab_smptec_hls.t) |
| PC-23 | `oklab` | `display-p3` | RGB | [tests/encoder/progressive-refinement/0165_gpu_oklab_display_p3_rgb.t](../../../tests/encoder/progressive-refinement/0165_gpu_oklab_display_p3_rgb.t) |
| PC-24 | `oklab` | `display-p3` | HLS | [tests/encoder/progressive-refinement/0166_gpu_oklab_display_p3_hls.t](../../../tests/encoder/progressive-refinement/0166_gpu_oklab_display_p3_hls.t) |
| PC-25 | `cielab` | `gamma` | RGB | [tests/encoder/progressive-refinement/0167_gpu_cielab_gamma_rgb.t](../../../tests/encoder/progressive-refinement/0167_gpu_cielab_gamma_rgb.t) |
| PC-26 | `cielab` | `gamma` | HLS | [tests/encoder/progressive-refinement/0168_gpu_cielab_gamma_hls.t](../../../tests/encoder/progressive-refinement/0168_gpu_cielab_gamma_hls.t) |
| PC-27 | `cielab` | `linear` | RGB | [tests/encoder/progressive-refinement/0169_gpu_cielab_linear_rgb.t](../../../tests/encoder/progressive-refinement/0169_gpu_cielab_linear_rgb.t) |
| PC-28 | `cielab` | `linear` | HLS | [tests/encoder/progressive-refinement/0170_gpu_cielab_linear_hls.t](../../../tests/encoder/progressive-refinement/0170_gpu_cielab_linear_hls.t) |
| PC-29 | `cielab` | `smpte-c` | RGB | [tests/encoder/progressive-refinement/0171_gpu_cielab_smptec_rgb.t](../../../tests/encoder/progressive-refinement/0171_gpu_cielab_smptec_rgb.t) |
| PC-30 | `cielab` | `smpte-c` | HLS | [tests/encoder/progressive-refinement/0172_gpu_cielab_smptec_hls.t](../../../tests/encoder/progressive-refinement/0172_gpu_cielab_smptec_hls.t) |
| PC-31 | `cielab` | `display-p3` | RGB | [tests/encoder/progressive-refinement/0173_gpu_cielab_display_p3_rgb.t](../../../tests/encoder/progressive-refinement/0173_gpu_cielab_display_p3_rgb.t) |
| PC-32 | `cielab` | `display-p3` | HLS | [tests/encoder/progressive-refinement/0174_gpu_cielab_display_p3_hls.t](../../../tests/encoder/progressive-refinement/0174_gpu_cielab_display_p3_hls.t) |
| PC-33 | `din99d` | `gamma` | RGB | [tests/encoder/progressive-refinement/0175_gpu_din99d_gamma_rgb.t](../../../tests/encoder/progressive-refinement/0175_gpu_din99d_gamma_rgb.t) |
| PC-34 | `din99d` | `gamma` | HLS | [tests/encoder/progressive-refinement/0176_gpu_din99d_gamma_hls.t](../../../tests/encoder/progressive-refinement/0176_gpu_din99d_gamma_hls.t) |
| PC-35 | `din99d` | `linear` | RGB | [tests/encoder/progressive-refinement/0177_gpu_din99d_linear_rgb.t](../../../tests/encoder/progressive-refinement/0177_gpu_din99d_linear_rgb.t) |
| PC-36 | `din99d` | `linear` | HLS | [tests/encoder/progressive-refinement/0178_gpu_din99d_linear_hls.t](../../../tests/encoder/progressive-refinement/0178_gpu_din99d_linear_hls.t) |
| PC-37 | `din99d` | `smpte-c` | RGB | [tests/encoder/progressive-refinement/0179_gpu_din99d_smptec_rgb.t](../../../tests/encoder/progressive-refinement/0179_gpu_din99d_smptec_rgb.t) |
| PC-38 | `din99d` | `smpte-c` | HLS | [tests/encoder/progressive-refinement/0180_gpu_din99d_smptec_hls.t](../../../tests/encoder/progressive-refinement/0180_gpu_din99d_smptec_hls.t) |
| PC-39 | `din99d` | `display-p3` | RGB | [tests/encoder/progressive-refinement/0181_gpu_din99d_display_p3_rgb.t](../../../tests/encoder/progressive-refinement/0181_gpu_din99d_display_p3_rgb.t) |
| PC-40 | `din99d` | `display-p3` | HLS | [tests/encoder/progressive-refinement/0182_gpu_din99d_display_p3_hls.t](../../../tests/encoder/progressive-refinement/0182_gpu_din99d_display_p3_hls.t) |
| PC-41 | Ordinary encoding | `display-p3` | GPU admission still rejects before output | [tests/encoder/progressive-refinement/0183_gpu_p3_ordinary_boundary.t](../../../tests/encoder/progressive-refinement/0183_gpu_p3_ordinary_boundary.t) |

### Residual threshold contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| DR-0001 | Defaults. | [tests/delta/residual/0001_defaults.t](../../../tests/delta/residual/0001_defaults.t) |
| DR-0002 | Option routes. | [tests/delta/residual/0002_option_routes.t](../../../tests/delta/residual/0002_option_routes.t) |
| DR-0003 | Invalid option atomicity. | [tests/delta/residual/0003_invalid_option_atomicity.t](../../../tests/delta/residual/0003_invalid_option_atomicity.t) |
| DR-0004 | Dependent option atomicity. | [tests/delta/residual/0004_dependent_option_atomicity.t](../../../tests/delta/residual/0004_dependent_option_atomicity.t) |
| DR-0005 | Policy age reset. | [tests/delta/residual/0005_policy_age_reset.t](../../../tests/delta/residual/0005_policy_age_reset.t) |
| DR-0006 | Metric boundaries. | [tests/delta/residual/0006_metric_boundaries.t](../../../tests/delta/residual/0006_metric_boundaries.t) |
| DR-0007 | Signed filter. | [tests/delta/residual/0007_signed_filter.t](../../../tests/delta/residual/0007_signed_filter.t) |
| DR-0008 | Filter threshold boundary. | [tests/delta/residual/0008_filter_threshold_boundary.t](../../../tests/delta/residual/0008_filter_threshold_boundary.t) |
| DR-0009 | Cell boundaries. | [tests/delta/residual/0009_cell_boundaries.t](../../../tests/delta/residual/0009_cell_boundaries.t) |
| DR-0010 | Cpu three observations. | [tests/delta/residual/0010_cpu_three_observations.t](../../../tests/delta/residual/0010_cpu_three_observations.t) |
| DR-0011 | Gpu three observations. | [tests/delta/residual/0011_gpu_three_observations.t](../../../tests/delta/residual/0011_gpu_three_observations.t) |
| DR-0012 | Gpu bluenoise observations. | [tests/delta/residual/0012_gpu_bluenoise_observations.t](../../../tests/delta/residual/0012_gpu_bluenoise_observations.t) |
| DR-0013 | Runtime ceiling. | [tests/delta/residual/0013_runtime_ceiling.t](../../../tests/delta/residual/0013_runtime_ceiling.t) |
| DR-0014 | History reset. | [tests/delta/residual/0014_history_reset.t](../../../tests/delta/residual/0014_history_reset.t) |
| DR-0015 | Output failure. | [tests/delta/residual/0015_output_failure.t](../../../tests/delta/residual/0015_output_failure.t) |
| DR-0016 | Partial frame rejection. | [tests/delta/residual/0016_partial_frame_rejection.t](../../../tests/delta/residual/0016_partial_frame_rejection.t) |
| DR-0017 | Progressive conflict. | [tests/delta/residual/0017_progressive_conflict.t](../../../tests/delta/residual/0017_progressive_conflict.t) |
| DR-0018 | Metric without mask. | [tests/delta/residual/0018_metric_without_mask.t](../../../tests/delta/residual/0018_metric_without_mask.t) |
| DR-0019 | gpu metric none. | [tests/delta/residual/0019_gpu_metric_none.t](../../../tests/delta/residual/0019_gpu_metric_none.t) |
| DR-0020 | gpu metric bluenoise. | [tests/delta/residual/0020_gpu_metric_bluenoise.t](../../../tests/delta/residual/0020_gpu_metric_bluenoise.t) |
| DR-0021 | gpu cell edges none. | [tests/delta/residual/0021_gpu_cell_edges_none.t](../../../tests/delta/residual/0021_gpu_cell_edges_none.t) |
| DR-0022 | gpu cell edges bluenoise. | [tests/delta/residual/0022_gpu_cell_edges_bluenoise.t](../../../tests/delta/residual/0022_gpu_cell_edges_bluenoise.t) |
| DR-0023 | cpu a observations with the existing observation/keep contract. | [tests/delta/residual/0023_cpu_a_observations.t](../../../tests/delta/residual/0023_cpu_a_observations.t) |
| DR-0024 | cpu x observations with the existing observation/keep contract. | [tests/delta/residual/0024_cpu_x_observations.t](../../../tests/delta/residual/0024_cpu_x_observations.t) |
| DR-0025 | gpu a observations with the existing observation/keep contract. | [tests/delta/residual/0025_gpu_a_observations.t](../../../tests/delta/residual/0025_gpu_a_observations.t) |
| DR-0026 | gpu x observations with the existing observation/keep contract. | [tests/delta/residual/0026_gpu_x_observations.t](../../../tests/delta/residual/0026_gpu_x_observations.t) |
| DR-0027 | gpu metric a with the existing observation/keep contract. | [tests/delta/residual/0027_gpu_metric_a.t](../../../tests/delta/residual/0027_gpu_metric_a.t) |
| DR-0028 | gpu metric x with the existing observation/keep contract. | [tests/delta/residual/0028_gpu_metric_x.t](../../../tests/delta/residual/0028_gpu_metric_x.t) |
| DR-0029 | gpu cells a with the existing observation/keep contract. | [tests/delta/residual/0029_gpu_cells_a.t](../../../tests/delta/residual/0029_gpu_cells_a.t) |
| DR-0030 | gpu cells x with the existing observation/keep contract. | [tests/delta/residual/0030_gpu_cells_x.t](../../../tests/delta/residual/0030_gpu_cells_x.t) |
| DR-0031 | CPU/Metal A dither uses unperturbed targets and emitted RGB/HLS colors for post-lookup acceptance with direct/Eytzinger lookup. | [tests/dither/a-dither/0039_wire_a.t](../../../tests/dither/a-dither/0039_wire_a.t) |
| DR-0032 | CPU/Metal X dither uses unperturbed targets and emitted RGB/HLS colors for post-lookup acceptance with direct/Eytzinger lookup. | [tests/dither/x-dither/0045_wire_x.t](../../../tests/dither/x-dither/0045_wire_x.t) |

These numeric and decoded-output witnesses cover configured mask semantics, not a general perceptual quality gain. GPU tests require an available Metal application backend. Filter scale is in pixels; gains measured at one resolution do not establish gains at another.

### Indexed commit and writer coverage

| ID | Contract | Owning test |
| --- | --- | --- |
| IC-01 | A painted index updates only its declared plane origin. | [tests/encoder/indexed/0001_retained_origin.t](../../../tests/encoder/indexed/0001_retained_origin.t) |
| IC-02 | An approximate keep requests refinement. | [tests/encoder/indexed/0002_retained_approximate.t](../../../tests/encoder/indexed/0002_retained_approximate.t) |
| IC-03 | Paint count excludes kept pixels. | [tests/encoder/indexed/0003_retained_paint_count.t](../../../tests/encoder/indexed/0003_retained_paint_count.t) |
| IC-04 | Presentation marks identify accepted paint only. | [tests/encoder/indexed/0004_retained_paint_marks.t](../../../tests/encoder/indexed/0004_retained_paint_marks.t) |
| IC-05 | Accepted packets advance the baseline without display waits. | [tests/encoder/indexed/0005_retained_sequence.t](../../../tests/encoder/indexed/0005_retained_sequence.t) |
| IC-06 | A duplicate receipt cannot commit the update twice. | [tests/encoder/indexed/0006_retained_duplicate.t](../../../tests/encoder/indexed/0006_retained_duplicate.t) |
| IC-07 | A foreign receipt cannot change retained history. | [tests/encoder/indexed/0007_retained_wrong_receipt.t](../../../tests/encoder/indexed/0007_retained_wrong_receipt.t) |
| IC-08 | A live baseline reader blocks commit. | [tests/encoder/indexed/0008_retained_reader_pin.t](../../../tests/encoder/indexed/0008_retained_reader_pin.t) |
| IC-09 | An active body consumer blocks commit. | [tests/encoder/indexed/0009_retained_user_pin.t](../../../tests/encoder/indexed/0009_retained_user_pin.t) |
| IC-10 | A failed producer cannot commit an accepted stream. | [tests/encoder/indexed/0010_retained_failed_image.t](../../../tests/encoder/indexed/0010_retained_failed_image.t) |
| IC-11 | Abort leaves the previous committed bytes intact. | [tests/encoder/indexed/0011_retained_abort.t](../../../tests/encoder/indexed/0011_retained_abort.t) |
| IC-12 | Invalidation clears validity without inventing replacement RGB. | [tests/encoder/indexed/0012_retained_invalidate.t](../../../tests/encoder/indexed/0012_retained_invalidate.t) |
| IC-13 | Invalidation cannot race a prepared update. | [tests/encoder/indexed/0013_retained_busy_invalidate.t](../../../tests/encoder/indexed/0013_retained_busy_invalidate.t) |
| IC-14 | Unpublished rows cannot be consumed. | [tests/encoder/indexed/0014_indexed_unpublished.t](../../../tests/encoder/indexed/0014_indexed_unpublished.t) |
| IC-15 | A skipped sentinel is never interpreted as a palette index. | [tests/encoder/indexed/0015_indexed_kept_sentinel.t](../../../tests/encoder/indexed/0015_indexed_kept_sentinel.t) |
| IC-16 | An out-of-range painted index seals the producer as failed. | [tests/encoder/indexed/0016_indexed_invalid_paint.t](../../../tests/encoder/indexed/0016_indexed_invalid_paint.t) |
| IC-17 | Completion requires every source row to be published. | [tests/encoder/indexed/0017_indexed_early_complete.t](../../../tests/encoder/indexed/0017_indexed_early_complete.t) |
| IC-18 | Published rows survive release of the producer reference. | [tests/encoder/indexed/0018_indexed_published_lifetime.t](../../../tests/encoder/indexed/0018_indexed_published_lifetime.t) |
| IC-19 | A rejected header never issues an acceptance receipt. | [tests/encoder/indexed/0019_writer_failed_header.t](../../../tests/encoder/indexed/0019_writer_failed_header.t) |
| IC-20 | A rejected body remains sticky through end_image. | [tests/encoder/indexed/0020_writer_failed_body.t](../../../tests/encoder/indexed/0020_writer_failed_body.t) |
| IC-21 | Abort closes framing without an acceptance receipt. | [tests/encoder/indexed/0021_writer_abort.t](../../../tests/encoder/indexed/0021_writer_abort.t) |
| IC-22 | A failed header drains writer state for the next image. | [tests/encoder/indexed/0022_writer_reuse_after_failure.t](../../../tests/encoder/indexed/0022_writer_reuse_after_failure.t) |
| IC-23 | Keeping an uninitialized hole leaves it invalid. | [tests/encoder/indexed/0023_retained_invalid_keep.t](../../../tests/encoder/indexed/0023_retained_invalid_keep.t) |
| IC-24 | An exact keep preserves the committed wire color. | [tests/encoder/indexed/0024_retained_exact_keep.t](../../../tests/encoder/indexed/0024_retained_exact_keep.t) |
| IC-25 | The last resource user safely drains an abandoned update. | [tests/encoder/indexed/0025_retained_deferred_abort.t](../../../tests/encoder/indexed/0025_retained_deferred_abort.t) |
