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
| Native float32 generated and fixed palettes | The selected CPU native path supports `-W gamma\|linear\|oklab\|cielab\|din99d -U gamma\|linear\|smpte-c\|display-p3 -t auto\|rgb\|hls`, at least two requested colors for generated palettes, ordinary SIXEL output, and `none`, the nine spatial diffusion policies or the three positional methods listed above (`auto` resolves as usual). Working samples and centroids retain negative, over-one and sub-byte components. Retained colors compete in the selected output RGB; see the comparison rule below. Genuine byte-eligible requests remain byte paths. |
| Palette-free monochrome `-e` | Conflicts with `-Z delta`, including environment-selected delta. It paints foreground bits and omits background bits; `P2=1` cannot erase previous foreground pixels when they become background. Rejection occurs before output, independently of precision or inversion. Select `-Z full` before `-e` to disable delta. Opaque black-and-white `-b gray1` paints both endpoints and is a distinct, supported fixed-palette case. |
| Other float32 combinations | PNG snapshots and macro output have not migrated to native delta. Selecting delta retains their earlier dispatch; P3 with an unsupported delta transport rejects before output. RGB888 promoted for soft/none diffusion keeps its existing gamma-RGB history behavior. This is not a claim of equivalent delta behavior on every legacy float path. |
| GPU palette application | The Metal path supports RGB888 `none` and `bluenoise` with a compatible retained plane matching the frame geometry. It implements both threshold and post-lookup decisions. CPU error-diffusion kernels are a separate path. |
| GPU with a larger or offset retained plane | The GPU request cannot express general origin-based plane addressing. `auto` declines that request so the CPU can apply the proper plane coordinates. Forced GPU use is not a portable fallback setting; prefer `off` or `auto` for damage rectangles. |
| Generated palette | An active retained plane requires a reserved transparency key. In the ordinary generated-palette path, one slot is taken from the requested budget; for `-p 256`, up to 255 entries remain for painted colors when that reservation is required. The key is excluded from ordinary nearest-color lookup. |
| Fixed palette and inter-frame palette reuse | Keeping display pixels and reusing palette construction are different operations. A stable palette does not establish a valid retained plane. Native gamma/linear sRGB, SMPTE-C or Display P3 `-b` and `-m` preserve all supplied entries, including 256-color tables, and omit by a separate mask as described below. Byte routes retain their earlier behavior; they do not gain native fixed-palette keeps solely by enabling delta. |
| OR output `-O` | Conflicts with `-Z delta`, including environment-selected delta. `P2=5` reconstructs palette-index bit planes and does not carry the `P2=1` operation that preserves omitted destination RGB. Zero bits are not delta keeps. Rejected option selection preserves prior options and history; the encoder also validates the combination before output. Select `-Z full` before enabling OR. Full OR encoding and low-level OR APIs retain their existing behavior. |
| DRCS `-@` | Conflicts with `-Z delta`, including environment-selected delta. Glyph definitions omit the ordinary SIXEL `P2` header and do not update the retained display plane. Selection rejects before changing DRCS settings or opening/truncating the character-output path; selecting delta after DRCS preserves the previous policy and history on rejection. Earlier successful DRCS selections may already have opened their output path; rejecting a later delta selection does not undo those earlier effects. Frame validation rejects before palette work and DRCS geometry lookup, as well as before definition bytes. Select `-Z full` before DRCS to disable delta. Full glyph output retains its existing behavior. |
| High-color output `-I` | This has separate repeated-register dispatch. This guide does not claim `-Z delta -I` as a supported combination; use ordinary indexed output for the workflow above. |
| Missing emitted RGB result | If the encode path cannot supply trustworthy emitted colors, the high-level update discards history instead of seeding it from source RGB. |

### Wire-rounded history across byte and float paths

Ordinary gamma RGB palette output captures the same RGB101 percentages that the serializer sends, then stores each percentage `c` as the byte `(c*255+50)/100`. This applies to byte and native float palettes, in serial and pipelined encoding. A fixed-table encoder can switch between byte and native float application without discarding its retained surface: history keeps one wire-based representation across that precision change.

Capture projects a temporary palette of at most 768 bytes, only when delta RGB capture is enabled. It does not modify working entries, lookup, residuals, snap policy or mapfile export, and adds no image-sized allocation beyond the existing RGB capture. A legacy float-backed output palette must use its serialized float component, not its byte shadow; the component `.0051` emits 1% (retained byte 3), even though its byte shadow is 1. Its `.0001` margin from the `.005` percentage boundary is much larger than float storage error. The output and retained integer expectations are exact; preservation of the original palette is checked as a byte-for-byte copy contract.

The byte correction covers ordinary gamma RGB and HLS definitions; HLS uses the reconstruction [below](#hls-retained-color-model). Legacy output other than `-U gamma` and body-only output retain their separate migration boundaries. The high-level encoder rejects palette-free monochrome, OR composition and DRCS with delta. Candidate comparison for byte-only gamma palettes follows the rule below; legacy float-backed application remains separate.

### Byte candidate comparison

For an ordinary gamma RGB output call with gamma source coordinates and no float-backed palette, post-lookup byte comparison projects each selected byte `b` to `c=(b*100+127)/255`, then `P=(c*255+50)/100`, using integer division. A readonly 256-byte table performs this exact projection without repeated divisions in the per-pixel comparison. It compares that emitted candidate with the retained RGB888 color. The query remains the current byte sample; this projection never overwrites the palette used by lookup or by newly painted-pixel diffusion. Ties retain the previous color. The early threshold gate and kept-color diffuse/skip rules are unchanged.

For example, source 32, retained 33 and selected entry 32 must keep: the entry emits 13%, represented as 33 in history. Retained 34 instead must repaint. Source 34 with retained 35 and selected entry 32 is another tie (distance 1 to both 35 and emitted 33); rounding the source to 33 would incorrectly reject it. These integer witnesses need no floating-point tolerance. Repeated generated frames with literal grays 32/96/160 retain 33/97/161 and omit the second frame in both serial and producer/writer encoding; changing the source to black paints again.

The output owner binds this comparison hint to its dither object for the encode call, before serial application or worker use, and clears it on success and failure. There is no global switch or retained-frame reset. Standalone PaletteApply has no wire-format contract and keeps its supplied-candidate comparison. OR, body-only, transformed source/output and legacy float-backed palettes do not acquire this byte comparison hint. HLS uses a separate palette-sized projection with the same call lifetime. Fixed byte tables still require their existing omission mechanism; this change does not add the native fixed-table mask path to them. Native float comparison remains as described below.

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

### Quality regression tests

| ID | Observation and limit | Owning test |
| --- | --- | --- |
| D6-23 | Alternating black/white regions are composed onto a retained canvas and checked against brightness bounds, detecting accidental ordinary-color selection of the transparency key. This is a targeted stale-color regression, not an MS-SSIM guarantee or exact equality of every output pixel. | [tests/processing/filter/0013_filter_encode_6delta_keycolor.t](../../../tests/processing/filter/0013_filter_encode_6delta_keycolor.t) |


### Defensive and malformed-input tests

The [filter test directory](../../../tests/processing/filter/) also checks contradictory internal GPU enable/capability flags and exercises direct GPU request wiring. General malformed SIXEL and allocation coverage belongs to the [testing guide](../../testing/guide.md); it does not prove retained-screen correctness on a terminal.

### Coverage limits and manual validation

The exact early-match and threshold-255 boundaries, plane-size rejection/reset details, seeded-buffer lifecycle, broader float32/output-space scope, and missing-emitted-RGB fallback are audited against owning code. The table does not provide a separate focused automated assertion for every one of those dimensions. Likewise, the listed LSO2 test asserts `diffuse`, while D6-20's `skip` test covers only its six named policies. GPU tests may skip when the required device or backend is unavailable.

Receiver preservation, palette interpretation, external scroll/overlay recovery, delivery loss, and temporal appearance are application/terminal integration observations. Validate them with the actual receiver and a composed frame sequence; a passing single-frame decoder test cannot establish them.
