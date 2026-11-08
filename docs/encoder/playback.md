# Sending a stream of frames with playback

This guide is for C programmers who want to send video or remote-desktop images through libsixel. It explains how to use `sixel_playback_t`, with small examples and no threading background required. For file playback in `img2sixel`, see [Animation playback](animation.md). The [public header](../../include/sixel.h.in) owns the exact API rules; [Animation threading](../threading/animation.md) explains the workers inside the library.

## Decide whether you need playback

A **frame** is one image in a changing picture. To **encode** a frame means to turn its pixels into SIXEL bytes that a terminal can draw. A **palette** is the small set of colors used to draw that frame. A video player produces many frames. A remote desktop may produce only the rectangle that changed.

Calling `sixel_encoder_encode_frame()` in a loop is like having one cook prepare a meal, hand it out, and then start the next meal. Playback adds a **queue**, a waiting line for frames. When threading and the selected options allow it, the library can prepare the next frame while it writes the current one.

This picture shows the possible overlap. Each box is an imaginary unit of time, not a speed measurement. Work inside one frame can also use several workers in either approach.

```text
Time -------------------------------------------------------------->

Direct calls, one after another:
caller    [prepare 1][write 1][prepare 2][write 2][prepare 3][write 3]

Playback, with preparation overlap:
prepare   [prepare 1][prepare 2][prepare 3]
write                [write 1]  [write 2]  [write 3]
                     output stays in frame order
```

Playback manages the handoff and limits how much unfinished work can pile up. It does not promise a higher frame rate. The terminal or the connection may still be the slowest part.

| Your task | A useful starting point | Why |
| --- | --- | --- |
| Show one picture | Direct encoding | There is no next frame to prepare. |
| Write a simple script or a small sequential C loop | Direct encoding | You may not need queue management. |
| Feed decoded video frames from a player such as 6mov | Playback | Preparation and output can overlap. |
| Feed changed rectangles from a remote desktop such as 6rdp | Playback with per-frame options | Each accepted rectangle keeps its own placement settings. |
| Play an animation file with `img2sixel` | The CLI's [animation controls](animation.md) | The loader already supplies the frames. |

## Set up the session

Configure the encoder before attaching playback. This includes its output, color choices, [threading policy](../cli/threading-policy.md), and any reserved colors. If using **delta** encoding, which can leave suitable existing pixels untouched, also set up its remembered canvas. While playback is attached, ordinary calls on that encoder fail with `SIXEL_BAD_ARGUMENT`. Detach playback before using the encoder directly again.

A **callback** is a function that you give the library to call later. Playback's optional `done` callback receives one completion result for each admitted frame. **Admission** means accepting a submitted frame for processing; it does not mean that the terminal has drawn it.

| Step | API | What to do |
| --- | --- | --- |
| Initialize configuration | `sixel_playback_config_init(&config, sizeof(config))` | Start with capacity 2, per-frame palettes, and no callback. Set `config.done` and `config.done_context` if needed. |
| Attach | `sixel_playback_new(&playback, encoder, output, &config, NULL)` | Playback retains the encoder and output. A null output uses the encoder's destination; a null allocator uses the default allocator. |
| Initialize per-frame choices | `sixel_playback_options_init(&options, sizeof(options))` | Set flags and their fields, or pass null options for defaults. `user_tag` can carry your frame identifier. |
| Feed frames | `sixel_playback_submit(...)` or `sixel_playback_reserve(...)` followed by `sixel_playback_submit_reserved(...)` | Choose the admission pattern below. |
| Finish pending work | `sixel_playback_drain(playback, -1)` | Wait for admitted frames and their callbacks. Inspect the returned status. |
| Detach | `sixel_playback_unref(playback)` | Drain, stop workers, and release playback's references. This call can wait. |

Use the initializers with the structure's `sizeof`, rather than filling only the fields you happen to need. Passing an explicit configuration selects its palette mode, including the default `PER_FRAME`. Passing null configuration instead inherits the encoder's palette policy. That choice stays fixed for the session.

Playback does not support attached progressive-repair contexts, DRCS terminal-font output, or paired presentation. Their details belong in the [header](../../include/sixel.h.in) and the respective output references.

## Keep the queue bounded

A **slot** is room for one unfinished frame or one reservation. Capacity is the total number of slots: 1 through 16, with a default of 2. A frame keeps its slot during preparation, encoding, writing, and its completion callback. Taking a frame out of the waiting line does not free its slot. Loader `load_depth` is a separate setting.

For example, a capacity of 3 can be full even when only one frame is waiting:

```text
Capacity = 3
+------------------+------------------+------------------+
| slot: writing A  | slot: waiting B  | slot: reserved   |
+------------------+------------------+------------------+
       occupied           occupied           occupied

New submit/reserve --> no free slot
    NOWAIT          --> SIXEL_QUEUE_FULL; nothing admitted
    WAIT            --> wait for a slot, or SIXEL_TIMEOUT

A's done returns   --> one free slot
release reservation --> one free slot, without encoding a frame
```

| Admission mode | When space is available | When the queue is full |
| --- | --- | --- |
| `SIXEL_PLAYBACK_NOWAIT` | Accept immediately | Return `SIXEL_QUEUE_FULL`. The caller decides whether to retry or skip producing that update. |
| `SIXEL_PLAYBACK_WAIT` | Accept immediately | Wait up to `timeout_ms`; return `SIXEL_TIMEOUT` if time runs out. A negative timeout waits indefinitely. |

The timeout here is for obtaining space, not for encoding or displaying the frame. A successful asynchronous submission means acceptance, so check the callback or `drain` for the final encoding result. A refused frame is never queued. Playback does not make room by silently dropping an accepted frame.

If capture or decoding is expensive, reserve first. A successful `sixel_playback_reserve()` gives you a ticket and holds a slot. Produce the frame, then pass that ticket to `sixel_playback_submit_reserved()`. This call does not wait for space, but it can still fail. It consumes a valid ticket even on failure. If you abandon production, return the ticket with `sixel_playback_release_reservation()`.

Use each ticket once; do not copy it to make extra reservations. Reserving a slot does not assign a frame sequence number. Submission does, so two producers can reserve in one order and submit in another. Release unused tickets before destroying playback: `drain` does not wait for reservations.

In a build without threads, submission encodes the frame before returning, and the callback can run inside that call. Only unused reservations can then fill the queue. A `WAIT` on that full queue returns `SIXEL_TIMEOUT` immediately, even with a negative timeout, because no worker could free a slot.

## Handle completion and stopping

Playback holds a reference to each admitted frame. Encoding may change the frame, so do not read, edit, or resubmit it while it is in flight. You may drop your own frame reference after submission; that is not permission to reuse its pixels. Any borrowed pixel storage must remain alive until completion. Without a callback, wait for a completed drain before reusing storage.

Completion callbacks arrive in admission order, with sequence numbers starting at 1. They normally run on a playback worker; synchronous playback and failure cleanup can call them from the caller's thread. Keep them short, and do not call the playback object or its encoder from a callback. Copy any result fields you need to keep: the result pointer is valid only during the call. Keep `done_context` alive until all callbacks finish.

The callback receives a `sixel_playback_result_t`:

| Field | Meaning |
| --- | --- |
| `sequence` | The frame's admission number, starting at 1. |
| `user_tag` | The identifier copied from your submission options. |
| `status` | Success or the reason this frame failed or was interrupted. Use `SIXEL_FAILED(status)` to check it. |
| `palette_sequence` | The frame whose palette was selected; see the next section. Zero means no selection was recorded. |
| `painted_pixels` | Opaque pixels painted by a successful delta frame; it is not a byte count. |
| `needs_6delta_refinement` | A successful delta frame kept some pixels using a nonzero threshold. Your application may choose to submit a stricter update. |
| `queued_seconds` / `processing_seconds` | Time before encoding starts / time from that start to completion. These are host-side intervals, not terminal display times. |

For a failed frame, call `sixel_helper_get_additional_message()` inside the callback to obtain its diagnostic text. A nonzero `palette_sequence` does not prove success: a latest-palette frame can fail after selecting a palette or even after writing output.

For cancellation, a frame is **claimed** when a worker takes responsibility for its final, ordered output. Preparation alone does not reach that point.

| Operation or event | What happens |
| --- | --- |
| `sixel_playback_drain(playback, timeout_ms)` | Waits for admitted frames and callbacks, excluding unused reservations. A negative timeout waits indefinitely. A timeout returns `SIXEL_TIMEOUT`; it does not cancel the work or make its buffers reusable. |
| `sixel_playback_cancel(playback)` | Marks admitted frames not yet claimed for ordered output as interrupted. Already claimed frames finish through delivery; there can be several in the GPU pipeline. Use `drain` afterward to wait for completion. |
| A frame fails | Frames not yet started complete with `SIXEL_INTERRUPTED`. Later admissions and `drain` report the first failure. Failure cleanup joins workers before releasing their inputs. |
| `sixel_playback_get_stats(playback, &stats)` | Returns a snapshot. Set `stats.struct_size = sizeof(stats)` first. `depth` includes reservations and active frames; `reserved` and `free_slots` explain its occupancy. Submission, completion, rejection, and timing fields help monitor it. |
| `sixel_playback_unref(playback)` | Finishes draining and detaches. It returns no status, so call `drain` explicitly when you need the outcome. |

Cancellation is not an undo button. A cancelled, unclaimed frame reports `SIXEL_INTERRUPTED` even if preparation had begun. Cancellation alone does not permanently close a healthy queue; later submissions remain eligible. A free slot reported by `get_stats` can be taken immediately afterward, so use `reserve`, not a statistics check, to secure space.

**Written bytes cannot be rolled back.** A writer may have accepted part of a frame before an error. With `LATEST`, the frame's own palette build can also fail after output using an earlier palette. Completion waits for that build before releasing the frame. See the [header contract](../../include/sixel.h.in) for the full failure rules.

## Choose how palettes are made

Building a palette takes time. Playback can either wait for each frame's own palette or, where supported, reuse a suitable completed palette from an earlier frame.

Set `config.palette_mode` to one of these constants:

| Mode | Plain meaning | What to expect |
| --- | --- | --- |
| `SIXEL_PLAYBACK_PALETTE_PER_FRAME` | Use each frame's own palette. | The default. Output matches repeated direct frame calls with the same options. |
| `SIXEL_PLAYBACK_PALETTE_LATEST` | Use the newest completed, compatible palette from this frame or an earlier one. | The first frame waits for its own palette. A completed compatible own palette wins. Timing can change the output, even between repeated runs. |
| `SIXEL_PLAYBACK_PALETTE_AUTO` | Start with per-frame behavior and try latest behavior when palette waiting makes it worth testing. | The library compares completion intervals and may switch at frame boundaries. Trials cost time; faster playback is not guaranteed. |

`LATEST` never borrows a palette from a later frame. For example, result `sequence = 8, palette_sequence = 6` means frame 8 used frame 6's palette. It does not mean frame 8 was dropped. The choice is made once for that frame.

Palette reuse requires compatible colors and transparency/delta reservations. An incompatible choice waits for the current frame's own palette. Supplied palettes, preserved indexed images, high-color output, and generated macros retain per-frame behavior. Explicit `LATEST` also supports eligible native gamma RGB8 GPU playback. Without usable asynchronous execution, reuse falls back to per-frame behavior. The [threading-policy reference](../cli/threading-policy.md#contract) and [header](../../include/sixel.h.in) give the exact eligibility and AUTO rules.

Sometimes the displayed image includes controls or text that should not influence the image palette. Set `options.palette_source` to a separate, unchanging RGB frame to sample colors from it. Null uses the submitted image. The source can have different dimensions, but its pixel format and colorspace must match. Output crop and resize do not apply to it. Keep its borrowed pixels alive and unchanged through the callback, including cancellation.

**Reserved palette colors** are colors set aside, for example for controls. They are separate from queue reservations. Configure them before attaching playback with `sixel_encoder_set_reserved_palette()`. The [palette construction pipeline](palette-construction/pipeline/README.md#parallel-adaptive-extraction) owns the rules for reserved colors and `palette_source`; use it for budgets, sampling, and color conversion details.

## Send only the changed rectangle

A **damaged rectangle** is an area that needs redrawing. A **retained canvas** is the encoder's remembered picture of the larger surface. Delta encoding compares an update with that picture. Playback supplies ordered frames; delta decides which pixels need output. You can use playback without delta.

Imagine a 640 by 480 desktop where only a 120 by 60 rectangle changed. The coordinates below locate that rectangle in the remembered canvas:

```text
Retained canvas: 640 x 480
(0,0) +--------------------------------------------------+
      |                                                  |
      |          (100,80) +-------------------+          |
      |                   | damaged rectangle |          |
      |                   |     120 x 60      |          |
      |                   +-------------------+          |
      |         pixels outside stay in history           |
      +--------------------------------------------------+

submitted frame: 120 x 60
options: ORIGIN flag, origin_x = 100, origin_y = 80
```

Before attaching playback, enable delta and declare the whole surface with `sixel_encoder_set_6delta_plane_size(encoder, 640, 480)`. For each rectangle, initialize options, set the relevant flags, and submit the rectangle's frame with those options.

| Flag in `options.flags` | Fields to set | Meaning |
| --- | --- | --- |
| `SIXEL_PLAYBACK_OPTION_ORIGIN` | `origin_x`, `origin_y` | Place this frame in the retained canvas's coordinates. |
| `SIXEL_PLAYBACK_OPTION_THRESHOLD_LIMIT` | `threshold_limit` | Cap the effective delta threshold. Zero requests exact-threshold refinement; 255 removes the runtime cap. Values above 255 are rejected. |

Combine flags with `|` when both are needed. Setting a field without its flag does not apply that field. Applied origin and threshold limits persist until changed; set them explicitly for updates that need different values. The queue copies these choices at submission, so later changes to your options variable do not move an earlier frame.

The origin does **not** move the terminal cursor or add SIXEL padding. Your application must place the output at the matching terminal location, in the same order as the queued frames. Declare coordinates for the pixels after any crop or resize. If something else changes the displayed surface, its remembered history may no longer be valid. Follow [C API and partial updates](output/delta-encoding.md#c-api-and-partial-updates) for placement, invalidation, and recovery.

The [delta guide](output/delta-encoding.md#persistent-residual-threshold-control) also describes periodic `refresh`. Its period counts encoded frames, not seconds. Inputs skipped before encoding do not advance that count. Refresh rechecks pixels; it does not promise that the terminal has repainted them by a wall-clock deadline.

## Understand where a GPU helps

A **GPU** is a processor that can do similar pixel calculations together. **Lookup** chooses a palette color for a pixel. **Dithering** uses patterns or spreads color error to make a limited palette look smoother. These are parts of encoding that can use the GPU; writing bytes to a terminal still takes time.

Eligible RGB8 GPU sessions use the independent playback controller. RGB8 means one 8-bit value each for red, green, and blue. Here, gamma RGB means the usual nonlinear RGB values rather than linear-light values. This path needs native gamma RGB input/output, preparation overlap, at least two slots, a `PER_FRAME` or `LATEST` mode, and a supplied output object. Other restrictions and fallback paths are described in [Animation threading](../threading/animation.md#current-frame-pipeline).

| Work | Where the opportunity comes from |
| --- | --- |
| Prepare inputs and build palettes | CPU work for a later frame can overlap current output. |
| GPU diffusion and GPU palette lookup | Supported pixel calculations run on the GPU. Lookup support depends on the selected policy. |
| Encode SIXEL and write it | CPU encoding and ordered delivery can progress separately from other stages. |

GPU diffusion has one component for each `-d` method: `lso2`, `fs`, `atkinson`, `jajuni`, `stucki`, `burkes`, `sierra1`, `sierra2`, and `sierra3`. They are distinct methods, not names for one shared diffusion algorithm. See the [dithering reference and GPU method pages](palette-application/dithering/README.md#cli-surface) and [lookup policies](palette-application/lookup-policy/README.md) for supported combinations.

More overlap does not guarantee more frames per second. GPU commands may share the same hardware, and the connection can still dominate. Capacity bounds unfinished frames, not a number of GPU processors. Use the [threading explanation](../threading/animation.md) for internal stages rather than choosing a large queue on the assumption that it must be faster.

## Example: a simple WAIT loop

This helper takes an already configured encoder, an output object, and distinct initialized frames. The caller keeps the frames and their storage alive until it returns, then releases them. It uses no completion callback and waits for all accepted work before returning. It is a complete function, not a complete application.

```c
#include <sixel.h>

SIXELSTATUS
send_frames(sixel_encoder_t *encoder, sixel_output_t *output,
            sixel_frame_t **frames, size_t count)
{
    sixel_playback_config_t config;
    sixel_playback_t *playback;
    SIXELSTATUS status;
    SIXELSTATUS drained;
    size_t i;

    status = sixel_playback_config_init(&config, sizeof(config));
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_playback_new(&playback, encoder, output, &config, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    for (i = 0; i < count; ++i) {
        status = sixel_playback_submit(playback, frames[i],
                                      SIXEL_PLAYBACK_WAIT, -1, NULL);
        if (SIXEL_FAILED(status)) {
            break;
        }
    }
    /* Drain even when submission failed: earlier frames may be active. */
    drained = sixel_playback_drain(playback, -1);
    sixel_playback_unref(playback);
    return SIXEL_FAILED(status) ? status : drained;
}
```

The wait in `submit` is for a slot. The final `drain` is for completion. Keeping those two waits separate lets the next frame get ready while earlier output is still in progress.

## Example: reserve before producing a frame

Here the application supplies `produce_frame()`. On success it returns one initialized frame with its own pixel storage and one caller-owned reference. On failure it cleans up its partial work. A video player can decode there; a remote desktop can capture a changed rectangle there. These application-specific steps are outside this example.

The callback logs to standard error, so its text does not enter the SIXEL output. Production applications can record results or return completed buffers to their own pool, with suitable synchronization.

```c
#include <stdio.h>
#include <sixel.h>

/* Application function; returns owned storage, not a reused live buffer. */
SIXELSTATUS produce_frame(sixel_frame_t **frame);

static void
frame_done(sixel_playback_result_t const *result, void *context)
{
    (void)context;
    fprintf(stderr, "frame %llu: status 0x%x\n",
            (unsigned long long)result->user_tag,
            (unsigned int)result->status);
}

SIXELSTATUS
produce_and_send(sixel_encoder_t *encoder, sixel_output_t *output,
                 size_t count)
{
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_playback_ticket_t ticket;
    sixel_playback_t *playback;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    SIXELSTATUS drained;
    size_t i;

    status = sixel_playback_config_init(&config, sizeof(config));
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_playback_options_init(&options, sizeof(options));
    if (SIXEL_FAILED(status)) {
        return status;
    }
    config.done = frame_done;
    status = sixel_playback_new(&playback, encoder, output, &config, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    for (i = 0; i < count; ++i) {
        status = sixel_playback_reserve(playback, SIXEL_PLAYBACK_WAIT,
                                       -1, &ticket);
        if (SIXEL_FAILED(status)) {
            break;
        }
        status = produce_frame(&frame);
        if (SIXEL_FAILED(status)) {
            sixel_playback_release_reservation(playback, &ticket);
            break;
        }
        options.user_tag = (uint64_t)i + 1;
        status = sixel_playback_submit_reserved(playback, &ticket,
                                                frame, &options);
        /* Drop our reference; accepted work keeps the frame alive. */
        sixel_frame_unref(frame);
        if (SIXEL_FAILED(status)) {
            break;
        }
    }
    drained = sixel_playback_drain(playback, -1);
    sixel_playback_unref(playback);
    return SIXEL_FAILED(status) ? status : drained;
}
```

To avoid waiting for capacity, use `NOWAIT` in the reservation call and handle `SIXEL_QUEUE_FULL` before calling the producer. This saves work on an update you cannot yet submit. `submit_reserved` still consumes the ticket if submission fails; the release call is only for an unused reservation.

## Implementation map

- [Public API](../../include/sixel.h.in): playback structures, lifetime rules, admission, and palette selection.
- [Encoder implementation](../../src/encoder.c): attachment, reservations, callbacks, statistics, and completion.
- [Independent controller](../../src/encoder-playback-independent.inc.h) and [deferred controller](../../src/encoder-playback-deferred.inc.h): ordered GPU playback and fallback execution.

## Test coverage

This guide explains existing contracts rather than defining a separate policy. Their owning references keep the detailed test mappings:

| Topic | Existing evidence |
| --- | --- |
| Direct/playback output equivalence, admission, waits, and callbacks | [Encoder threading coverage](../threading/encoder.md#test-coverage) |
| Cancellation, preparation lifetime, and ordered GPU execution | [Animation threading coverage](../threading/animation.md#test-coverage) |
| Palette modes and inheritance | [Threading-policy coverage](../cli/threading-policy.md#test-coverage) |
| Separate palette sources and reserved colors | [Palette construction coverage](palette-construction/pipeline/README.md#test-coverage) |
| Partial updates and frame-count refresh | [Delta coverage](output/delta-encoding.md#test-coverage) |

Those tests establish the stated software behavior for their fixtures. They do not establish terminal presentation timing, a particular frame rate, or end-to-end behavior in a video player or remote desktop. The two C helpers also need application-provided frames and an output destination; syntax checks alone do not prove a working terminal integration.
