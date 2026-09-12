# Delta Encoding (6delta)

6delta reduces repeated SIXEL painting by remembering the colors previously sent to a persistent display surface. For each new pixel, the encoder can either paint a palette color or leave the existing pixel untouched. An untouched pixel is represented through ordinary SIXEL transparency with `P2=1`; the receiver does not need a separate 6delta command or decoder.

This is useful for animation and applications that repeatedly repaint a surface, including remote desktops. It requires the encoder's retained image and the receiver's actual image to remain synchronized. A delta frame by itself is an incomplete image: decoding it on a blank canvas cannot reconstruct the pixels it deliberately omits.

See the [SIXEL format](../sixel-format.md) for the wire syntax, [alpha policy](../loader/alpha-policy.md) for `P2=1`, and the [encoding pipeline](encoding-pipeline.md) for palette construction and application.

## Quick start

Enable 6delta with `-Z` or `--6delta-threshold`. Zero enables the feature with an exact early-match gate and the post-lookup comparison described below; it does **not** disable delta encoding.

```sh
img2sixel --6delta-threshold=0 --6delta-error=diffuse \
  --precision=8bit --working-colorspace=gamma --gpu-policy=off \
  --loop-control=disable animation.gif
```

Use an animated input supported by the selected loader and a SIXEL terminal that preserves unpainted pixels across frames at the same display position. `--loop-control=disable` plays the input once. The explicit precision, working-space, and GPU settings make this example use the CPU RGB888 path documented here. They are not additional enable flags for 6delta.

The first frame establishes history. Subsequent frames in the same encoder instance can reuse it. A fresh `img2sixel` process starts with no history, so invoking the command separately for each still image does not share delta state. A still image encoded once has no previous frame to reuse unless an application supplies an initial retained plane through the C API.

| Control | Meaning | Default |
| --- | --- | --- |
| `-Z DELTA`, `--6delta-threshold=DELTA` | Enable 6delta and set the early-keep tolerance, an integer from 0 through 255. | Disabled when neither the option nor a valid environment setting is supplied. |
| `-Y MODE`, `--6delta-error=MODE` | Choose `diffuse` or `skip` for error handling at kept pixels. This option alone does not enable 6delta. | `diffuse`. |
| `SIXEL_6DELTA_THRESHOLD` | Set the initial threshold and enable 6delta when valid. | Invalid values leave the built-in disabled state. |
| `SIXEL_6DELTA_ERROR` | Set the initial kept-pixel error mode. | Invalid values leave `diffuse`. |
| `--alpha-policy=auto` | Resolve to `keep` for 6delta, requesting `P2=1`. | `auto`. |

Explicit `-Z` and `-Y` settings override the corresponding environment defaults. The threshold and error mode persist on a C encoder object until changed. There is no documented `-Z` value meaning “off”; use a fresh encoder without a threshold setting for an independent non-delta baseline, and unset `SIXEL_6DELTA_THRESHOLD` for a CLI baseline. Clearing history only forces repainting until history is built again.

The canonical error-mode names are `diffuse` and `skip`; `carry` is rejected. Explicit `--alpha-policy=clear` or `--alpha-policy=composite` conflicts with 6delta and is rejected. A process-level alpha default does not prevent the feature from resolving to `keep`; see [alpha-policy precedence](../loader/alpha-policy.md).

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

Ties select keep. The retained color is an extra candidate specific to this display position; it need not belong to the current frame's palette. The comparison is against the entry that the configured lookup actually selected, not a separate exhaustive search over all palette entries. See [lookup policy](lookup-policy.md) for that selection boundary.

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

With `--6delta-error=diffuse`, a kept pixel's quantization error is calculated against retained RGB and propagated through the chosen diffusion kernel. The target is the color left on screen, not the transparency marker's RGB. With `skip`, propagation from a kept pixel is omitted; ordinary painted pixels still use the selected diffusion policy. Skipping can change following pixels and temporal appearance as well as execution time. Positional dithers and `none` have no spatial error diffusion for this switch to suppress.

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

A standalone decode of that second frame yields one painted blue pixel and unpainted positions. Reconstruct the display by applying its paint mask to the preceding canvas. Reusing decoder palette registers alone does not reconstruct the retained display surface. The [decoding pipeline](decoding-pipeline.md) separates palette state, paint masks, and image representations.

Omitting pixels can reduce drawing work, but palette definitions, framing, horizontal skips, and band changes still cost bytes. Scattered keeps can also break repeated patterns that ordinary SIXEL compresses efficiently. The percentage of kept pixels is therefore not a compression ratio.

## C API and partial updates

The public declarations are in [sixel.h.in](../../include/sixel.h.in). The feature macros `SIXEL_HAVE_ENCODER_6DELTA_PLANE` and `SIXEL_HAVE_ENCODER_ENCODE_FRAME` allow consumers to test header capabilities.

| API | Purpose and lifetime |
| --- | --- |
| `sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_6DELTA_THRESHOLD, "0")` | Enable delta selection on this encoder. |
| `sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_6DELTA_ERROR, "diffuse")` | Set kept-pixel error handling. |
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
                                  SIXEL_OPTFLAG_6DELTA_THRESHOLD, "0");
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_6DELTA_ERROR, "diffuse");
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
| Float32 and non-gamma working spaces | The described keep helpers are in the RGB888 policy implementations. Do not assume that selecting `-Z` adds the same keep behavior to float32 paths or changes the metric to the `-W` space. Explicit `--precision=8bit --working-colorspace=gamma` is the baseline for this guide; see [precision](precision.md). |
| GPU palette application | The Metal path supports RGB888 `none` and `bluenoise` with a compatible retained plane matching the frame geometry. It implements both threshold and post-lookup decisions. CPU error-diffusion kernels are a separate path. |
| GPU with a larger or offset retained plane | The GPU request cannot express general origin-based plane addressing. `auto` declines that request so the CPU can apply the proper plane coordinates. Forced GPU use is not a portable fallback setting; prefer `off` or `auto` for damage rectangles. |
| Generated palette | An active retained plane requires a reserved transparency key. In the ordinary generated-palette path, one slot is taken from the requested budget; for `-p 256`, up to 255 entries remain for painted colors when that reservation is required. The key is excluded from ordinary nearest-color lookup. |
| Fixed palette and inter-frame palette reuse | Keeping display pixels and reusing palette construction are different operations. A stable palette does not establish a valid retained plane. Do not assume identical palette capacity or selection across every fixed-palette path. |
| High-color and OR output | These have separate dispatch and wire semantics. In particular, OR output uses `P2=5`, which is not the `P2=1` preservation contract. This guide does not claim `-Z -I` or `-Z -O` as supported combinations; use ordinary indexed output for the workflow above. |
| Missing emitted RGB result | If the encode path cannot supply trustworthy emitted colors, the high-level update discards history instead of seeding it from source RGB. |

The support table describes the current implementation, rather than a guarantee that every selectable combination has end-to-end coverage. Source transparency takes precedence over history-based color selection. Terminal preservation, color-register behavior, and cursor placement still require verification on the intended receiver.

## Quality and performance evaluation

Start with threshold zero and `diffuse`, then compare a fixed sequence of frames at the same geometry, palette settings, dither policy, precision, GPU policy, and thread budget. Measure both the first repaint and steady-state updates. A positive threshold may avoid more lookups; `skip` may avoid more diffusion work. Neither promises lower total latency or acceptable visual error.

Useful observations are complementary:

- Reconstruct the persistent canvas after every frame, using emitted paint masks and the correct origins. Compare this canvas with the intended source and with a non-delta baseline. Comparing isolated transparent delta PNGs with opaque input images measures the wrong object.
- Measure complete stream bytes, including palette definitions and framing. Also record kept-pixel counts, but do not use them as a substitute for bytes.
- Separate palette construction, lookup/dither, SIXEL serialization, transport, and receiver rendering. The retained plane and validity checks add work even when few pixels can be kept; palette construction is not automatically removed.
- Include unchanged frames, moving damage rectangles, changing text, flat light/dark areas, gradients, abrupt scene changes, and recovery after invalidation. Small residual errors in nearly uniform areas and stale content after scene changes deserve particular attention with positive thresholds.

For a plane of `W × H` pixels, retained RGB and one byte of validity per pixel account for approximately `4 × W × H` bytes. Per-frame masks, emitted-RGB capture, source pixels, palette state, and allocator overhead are additional. This is not the total encoder memory budget.

No universal speedup or quality threshold is claimed here. Per-pixel RGB selection, whole-canvas perceptual error, temporal artifacts, and transmitted size are different measurements; follow the [quality measurement policy](../quality/measurement-policy.md) when publishing comparisons.

## Implementation references

- [High-level encoder](../../src/encoder.c): option/default handling, `sixel_encoder_bind_transparent_mask`, geometry resolution, palette-key reservation, public plane APIs, and `sixel_encoder_update_accumulation_from_frame`.
- [Dither state and keep helpers](../../src/dither.c): `sixel_dither_rgb888_delta_within`, origin/validity checks, `sixel_dither_pipeline_6delta_try_keep_rgb888`, `sixel_dither_pipeline_6delta_try_keep_after_lookup`, result masks, and RGB capture.
- [FS](../../src/dither-policy-fs.c), [LSO2](../../src/dither-policy-lso2.c), and [blue noise](../../src/dither-policy-bluenoise.c): sample selection and policy-specific handling of kept pixels.
- [Indexed encoder core](../../src/encoder-core-encode.c): emitted palette/index capture and SIXEL serialization.
- [GPU request checks](../../src/gpu-palette.c) and [Metal implementation](../../src/gpu-palette-metal.m): retained-plane capability and per-pixel GPU decisions.
- [Option registry](../../src/options-registry.c), [CLI help](../../converters/img2sixel.c), and [manual](../../converters/img2sixel.1): accepted controls and environment defaults.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The IDs below map to existing assertions. A representative fixture establishes its stated observation, not exhaustive coverage of every RGB value, threshold, backend, or terminal.

| ID | Contract and assertion boundary | Owning test |
| --- | --- | --- |
| D6-01 | No history keeps on the first opaque frame; repeats keep pixels; a moving rectangle keeps some overlap without keeping the entire new rectangle; invalidation forces no history keeps. | [tests/processing/filter/0012_filter_encode_6delta_plane.t](../../tests/processing/filter/0012_filter_encode_6delta_plane.t) |
| D6-02 | Repeated content can produce keeps at threshold zero. | [tests/processing/filter/0013_filter_encode_6delta_keycolor.t](../../tests/processing/filter/0013_filter_encode_6delta_keycolor.t) |
| D6-03 | CLI threshold 8 is accepted; 256, -1, and nonnumeric input are rejected; `skip` is accepted. | [tests/quant/palette/usage/0168_6delta_threshold_cli_range.t](../../tests/quant/palette/usage/0168_6delta_threshold_cli_range.t) |
| D6-04 | `diffuse` and `skip` are accepted; `carry` is rejected. | [tests/cli/options/matching/0149_option_matching_6delta_error_choice.t](../../tests/cli/options/matching/0149_option_matching_6delta_error_choice.t) |
| D6-05 | Threshold 32 supplied through the environment produces the same animation bytes as `-Z 32` for the fixture. | [tests/cli/options/migration/0001_6delta_threshold_environment_cli_equivalence.t](../../tests/cli/options/migration/0001_6delta_threshold_environment_cli_equivalence.t) |
| D6-06 | Environment `skip` produces the same animation bytes as `-Y skip` for the fixture. | [tests/cli/options/migration/0002_6delta_error_environment_cli_equivalence.t](../../tests/cli/options/migration/0002_6delta_error_environment_cli_equivalence.t) |
| D6-07 | Default `auto` requests `P2=1` with 6delta. | [tests/quant/palette/usage/0186_6delta_auto_policy_selects_keep.t](../../tests/quant/palette/usage/0186_6delta_auto_policy_selects_keep.t) |
| D6-08 | Explicit `clear` conflicts with 6delta and reports the alpha-policy requirement. | [tests/quant/palette/usage/0187_6delta_explicit_alpha_policy_conflict.t](../../tests/quant/palette/usage/0187_6delta_explicit_alpha_policy_conflict.t) |
| D6-09 | Explicit `composite` conflicts with 6delta and reports the alpha-policy requirement. | [tests/quant/palette/usage/0190_6delta_composite_policy_conflict.t](../../tests/quant/palette/usage/0190_6delta_composite_policy_conflict.t) |
| D6-10 | LSO2 selects the keep index at threshold zero and propagates retained-color error to the next sample. | [tests/processing/filter/0015_filter_dither_6delta_lso2.t](../../tests/processing/filter/0015_filter_dither_6delta_lso2.t) |
| D6-11 | Jarvis, Judice & Ninke applies the same keep/error observation. | [tests/processing/filter/0016_filter_dither_6delta_jajuni.t](../../tests/processing/filter/0016_filter_dither_6delta_jajuni.t) |
| D6-12 | Stucki applies the same keep/error observation. | [tests/processing/filter/0017_filter_dither_6delta_stucki.t](../../tests/processing/filter/0017_filter_dither_6delta_stucki.t) |
| D6-13 | Burkes applies the same keep/error observation. | [tests/processing/filter/0018_filter_dither_6delta_burkes.t](../../tests/processing/filter/0018_filter_dither_6delta_burkes.t) |
| D6-14 | Sierra Lite applies the same keep/error observation. | [tests/processing/filter/0019_filter_dither_6delta_sierra1.t](../../tests/processing/filter/0019_filter_dither_6delta_sierra1.t) |
| D6-15 | Sierra Two-row applies the same keep/error observation. | [tests/processing/filter/0020_filter_dither_6delta_sierra2.t](../../tests/processing/filter/0020_filter_dither_6delta_sierra2.t) |
| D6-16 | Sierra-3 applies the same keep/error observation. | [tests/processing/filter/0021_filter_dither_6delta_sierra3.t](../../tests/processing/filter/0021_filter_dither_6delta_sierra3.t) |
| D6-17 | A-dither records a keep index and mask when retained RGB beats its perturbed palette choice. | [tests/processing/filter/0022_filter_dither_6delta_a_dither.t](../../tests/processing/filter/0022_filter_dither_6delta_a_dither.t) |
| D6-18 | X-dither records the corresponding keep index and mask. | [tests/processing/filter/0023_filter_dither_6delta_x_dither.t](../../tests/processing/filter/0023_filter_dither_6delta_x_dither.t) |
| D6-19 | CPU blue noise records the corresponding keep index and mask. | [tests/processing/filter/0024_filter_dither_6delta_bluenoise.t](../../tests/processing/filter/0024_filter_dither_6delta_bluenoise.t) |
| D6-20 | `skip` leaves the following sample unchanged by a kept pixel for Jajuni, Stucki, Burkes, and the three Sierra policies. | [tests/processing/filter/0026_filter_dither_6delta_diffusion_skip.t](../../tests/processing/filter/0026_filter_dither_6delta_diffusion_skip.t) |
| D6-21 | High-level forced GPU blue noise attaches an exact-size retained plane and returns the expected keep index/mask; requires a GPU-capable build and device. | [tests/processing/filter/0028_filter_dither_6delta_bluenoise_gpu_exact_plane.t](../../tests/processing/filter/0028_filter_dither_6delta_bluenoise_gpu_exact_plane.t) |
| D6-22 | GPU `auto` permits origin-aware CPU fallback for an offset retained plane. | [tests/processing/filter/0029_filter_dither_6delta_bluenoise_gpu_auto_fallback.t](../../tests/processing/filter/0029_filter_dither_6delta_bluenoise_gpu_auto_fallback.t) |

### Quality regression tests

| ID | Observation and limit | Owning test |
| --- | --- | --- |
| D6-23 | Alternating black/white regions are composed onto a retained canvas and checked against brightness bounds, detecting accidental ordinary-color selection of the transparency key. This is a targeted stale-color regression, not an MS-SSIM guarantee or exact equality of every output pixel. | [tests/processing/filter/0013_filter_encode_6delta_keycolor.t](../../tests/processing/filter/0013_filter_encode_6delta_keycolor.t) |

### Defensive and malformed-input tests

The [filter test directory](../../tests/processing/filter/) also checks contradictory internal GPU enable/capability flags and exercises direct GPU request wiring. General malformed SIXEL and allocation coverage belongs to the [testing guide](../testing/guide.md); it does not prove retained-screen correctness on a terminal.

### Coverage limits and manual validation

The documented constructor defaults, environment precedence and invalid-value fallback, exact early-match and threshold-255 boundaries, plane-size rejection/reset details, seeded-buffer lifecycle, float32 scope, and emitted-RGB fallback are audited against owning code. The table does not provide a separate focused automated assertion for every one of those dimensions. Likewise, the listed LSO2 test asserts `diffuse`, while D6-20's `skip` test covers only its six named policies. GPU tests may skip when the required device or backend is unavailable.

Receiver preservation, palette interpretation, external scroll/overlay recovery, delivery loss, and temporal appearance are application/terminal integration observations. Validate them with the actual receiver and a composed frame sequence; a passing single-frame decoder test cannot establish them.
