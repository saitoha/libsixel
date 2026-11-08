# Sending a stream of frames with playback

This guide is for C programmers who want to send video or remote-desktop images through libsixel. It explains how to use `sixel_playback_t`, with small examples and no threading background required. The [chafa comparison](#compare-with-chafa) shows which kinds of work can overlap. For file playback in `img2sixel`, see [Animation playback](animation.md). The [public header](../../include/sixel.h.in) owns the exact API rules; [Animation threading](../threading/animation.md) explains the workers inside the library.

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

## Tell frame overlap from band overlap

A **worker** is a thread: a line of work the program can run alongside other work. A **stage** is one step in processing an image. **Dithering** uses patterns or spreads color error to make a limited palette look smoother. A SIXEL **band** is a strip six pixels high.

libsixel already overlapped dithering, encoding, and writing bands before playback. This is **intra-frame** overlap: work inside one frame. All those workers share the same frame. Repeated direct `sixel_encoder_encode_frame()` calls finish frame A before starting frame B. The following picture leaves out palette preparation and shows three bands of A:

```text
Time -------------------------------------------------------->
Direct call: frame A only
dither     [A band 1][A band 2][A band 3]
encode               [A band 1][A band 2][A band 3]
write                          [A band 1][A band 2][A band 3]
                                                          |
                                              A returns --+--> start B

Playback can instead overlap DIFFERENT frames:
prepare    [frame A][frame B][frame C]
write                [frame A][frame B][frame C]
```

**Inter-frame** pipelining means that different frames occupy different stages at the same time. That is the frame-level meaning of pipeline in this guide. Band overlap alone does not provide it. The [direct entry point](../../src/encoder.c) and [band encoder](../../src/encoder-core-encode.c) keep these boundaries separate.

| Mechanism | What can overlap? | Where to learn more |
| --- | --- | --- |
| Band allocation `PIPELINE` | Dithering, encoding, and ordered writing of bands from one frame. | [Six-row jobs and ordered output](../threading/encoder.md#six-row-jobs-and-ordered-output) |
| Band allocation `STAGED` | Workers within a phase. All dither workers finish before encoding takes its worker allocation. | [Dither/encode budget policy](../threading/encoder.md#ditherencode-budget-policy) |
| Playback with preparation overlap | Preparation of a later frame and output of an earlier one. Eligible GPU playback separates more stages. | [Current frame pipeline](../threading/animation.md#current-frame-pipeline) |

The names `PIPELINE` and `STAGED` describe band allocation, not a promise of frame-level pipelining. File-loader read-ahead and playback add separate ways to overlap frames; the band mechanism by itself does not start the next frame.

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

A **GPU** is a processor that can do similar pixel calculations together. The **CPU** is the computer's general-purpose processor. **Lookup** chooses a palette color for a pixel. Lookup and dithering are parts of encoding that can use the GPU; writing bytes to a terminal still takes time.

Eligible RGB8 GPU sessions use the independent playback controller. RGB8 means one 8-bit value each for red, green, and blue. Here, gamma RGB means the usual nonlinear RGB values rather than linear-light values. This path needs native gamma RGB input/output, preparation overlap, at least two slots, a `PER_FRAME` or `LATEST` mode, and a supplied output object. Other restrictions and fallback paths are described in [Animation threading](../threading/animation.md#current-frame-pipeline).

| Work | Where the opportunity comes from |
| --- | --- |
| Prepare inputs and build palettes | CPU work for a later frame can overlap current output. |
| GPU diffusion and GPU palette lookup | Supported pixel calculations run on the GPU. Lookup support depends on the selected policy. |
| Encode SIXEL and write it | CPU encoding and ordered delivery can progress separately from other stages. |

GPU diffusion has one component for each `-d` method: `lso2`, `fs`, `atkinson`, `jajuni`, `stucki`, `burkes`, `sierra1`, `sierra2`, and `sierra3`. They are distinct methods, not names for one shared diffusion algorithm. See the [dithering reference and GPU method pages](palette-application/dithering/README.md#cli-surface) and [lookup policies](palette-application/lookup-policy/README.md) for supported combinations.

More overlap does not guarantee more frames per second. GPU commands may share the same hardware, and the connection can still dominate. Capacity bounds unfinished frames, not a number of GPU processors. Use the [threading explanation](../threading/animation.md) for internal stages rather than choosing a large queue on the assumption that it must be faster.

## Compare with chafa

chafa is an earlier example of SIXEL animation: its [1.4.0 release notes, dated April 1, 2020][c-news], announce SIXEL output and animation speed controls. The comparison below studies **chafa commit `c897fce60cc4e947a254770ea1c2e320d58e7dce`**. Every chafa source link is pinned to that commit. It covers normal CLI animation in SIXEL mode, with GIF, animated PNG, and WebP as decoder examples.

At this commit, the chafa CLI finishes writing each animation frame before rendering the next. It has file prefetch, parallel work inside a frame, and a writer thread. The animation loop calls `chafa_term_flush()` before advancing. This makes it an example of threaded SIXEL animation, but not of inter-frame rendering/output pipelining in this path. See [`tools/chafa/chafa.c`, `run_generic()`][c-loop].

### Follow a frame through chafa

**Prefetch** means getting input ready before it is needed. chafa's media pipeline has a ring of slots, sized from its resolved thread count. Workers open files and create loaders. The consumer takes results in input-file order. Normal animation requests loaders but disables rendering in those file jobs. Grid mode can request rendered images instead; that is a different path. See [`chicle-media-pipeline.c`, `thread_func()`, `wait_for_next()`, and `chicle_media_pipeline_new()`][c-files], [`chafa.c`, `run_vertical()`][c-vertical], and [`chicle-grid-layout.c`, `chicle_grid_layout_print_chunk()`][c-grid].

The upper line below is file prefetch. The lower loop handles animation for one of those files. A **FIFO** is a queue that removes items in the order they entered. Here the output FIFO holds bytes, not frame tickets. Sources: [`chicle-media-pipeline.c`, `wait_for_next()`][c-files], [`chafa.c`, `run_generic()`][c-loop], and [`chafa-stream-writer.c`, `chafa_stream_writer_write()` and `thread_main()`][c-writer].

```text
Input files --> [file-loader workers] --> [slots in file order]

Animation loop uses one loader from those slots:
  +--> get frame --> scale / palette / map / SIXEL string
  |                              |
  |                              v
  |                       byte FIFO --> writer
  |                                      |
  |                                      v
  +-- advance <-- wait if early <-- flush completes
```

Decoding turns a compressed file into pixels. The three loaders below keep decoder state between calls. Their work is requested **synchronously**: the animation loop waits for each request to finish. WebP's decoder may itself use threads. Source file paths in this table are under `tools/chafa/`.

| Input | What the loader keeps or prepares | Source at the studied commit |
| --- | --- | --- |
| GIF | Keeps the decoder and current bitmap. Moving forward clears the decoded flag; asking for pixels decodes the selected frame if needed. | [`chicle-gif-loader.c`, `maybe_decode_frame()`, `chicle_gif_loader_get_frame_data()`, `chicle_gif_loader_goto_next_frame()`][c-gif] |
| Animated PNG | Keeps an animation canvas. Moving forward decodes the next frame into it. | [`chicle-png-loader.c`, `chicle_png_loader_get_frame_data()`, `chicle_png_loader_goto_next_frame()`][c-png] |
| WebP | Keeps current and next pixel buffers. Asking for the current delay synchronously decodes one frame ahead to get its timestamp. Moving forward adopts that buffer. This happens before current-frame rendering, not alongside current-frame writing. | [`chicle-webp-loader.c`, `chicle_webp_loader_get_frame_delay()`, `chicle_webp_loader_goto_next_frame()`][c-webp] |

WebP enables its decoder's `use_threads` option in [`chicle_webp_loader_new_from_mapping()`][c-webp-new]. That is parallel work inside the decoder, not a separate chafa animation-rendering lane. A file-prefetch job may also decode initial pixels while creating a loader. These are useful forms of overlap, but their unit is not a queue of rendered animation frames. See the [GIF constructor][c-gif] and [`chicle-media-pipeline.c`, `thread_func()`][c-files].

For each displayed frame, the CLI creates a fresh canvas and places the pixels on it. In SIXEL mode, libchafa scales and maps those pixels, then builds one complete output string. The following steps describe this path, including its waiting points. A **batch** groups work to send to one worker. See [`chafa.c`, `build_canvas()` and `run_generic()`][c-canvas-cli], and [`chafa/chafa-canvas.c`, `chafa_canvas_print_rows()`][c-print].

| Step inside one frame | Work and waiting point | Source at the studied commit |
| --- | --- | --- |
| Scale | Smolscale resizes pixels into a shared image buffer. Row batches may run on workers; all finish before palette generation. | [`chafa/internal/chafa-indexed-image.c`, `draw_pixels_pass_1_worker()` and `draw_pixels()`][c-indexed] |
| Build a palette | For a generated palette, sample colors and repeatedly merge nearby color groups. The initial neighbor search can use workers. Fixed palettes skip generation. | [`chafa/internal/chafa-palette.c`, `find_initial_nearest()`, `pnn_palette()`, `chafa_palette_generate()`][c-palette] |
| Map and dither | Choose a palette entry for every pixel. None, ordered, and noise modes can split rows among workers. Floyd–Steinberg diffusion uses one batch, carrying color error from row to row. | [`chafa/internal/chafa-indexed-image.c`, `draw_pixels_pass_2_worker()`, `draw_pixels_pass_2_fs()`, `draw_pixels()`][c-indexed] |
| Build SIXEL | Workers encode batches aligned to six-row bands. All batches finish before their strings are appended in band order. The output includes palette definitions. | [`chafa/internal/chafa-sixel-renderer.c`, `build_sixel_row_worker()`, `build_sixel_palette()`, `chafa_sixel_renderer_build_ansi()`][c-sixel]; [`chafa/internal/chafa-batch.c`, `chafa_process_batches()`][c-batch] |
| Write | The CLI queues the completed string. A writer removes bytes in order, in chunks of up to 4096 bytes. Its FIFO defaults to 1 MiB and makes producers wait when full. The complete frame string is additional storage outside that FIFO. | [`chafa.c`, `write_image()`][c-write-image]; [`chafa/chafa-stream-writer.c`, `chafa_stream_writer_write()` and `thread_main()`][c-writer] |

chafa's six-row workers divide the work of building one frame. They do not stream completed bands to the terminal while later bands are still being rendered. `chafa_process_batches()` waits for every batch before appending results, and the CLI writes only after `chafa_canvas_print_rows()` returns. See [the batch helper][c-batch], [the canvas print path][c-print], and [`run_generic()`][c-loop].

Between animation frames, the CLI's `write_image_prologue()` moves the cursor back to the image location. `write_image()` sends the strings through `chafa_term_write()` to the byte writer. On Unix-like systems, `safe_write()` handles short writes and retries interrupted writes. See [`tools/chafa/chafa.c`, `write_image_prologue()`, `write_image()`, and `write_gstring_to_stdout()`][c-write-image], [`chafa/chafa-term.c`, `chafa_term_write()` and `chafa_term_flush()`][c-term], and [`chafa/chafa-stream-writer.c`, `safe_write()`][c-writer].

### Compare the timelines and carried state

For libsixel's eligible independent GPU controller, the stage names are P, S, R, and E. A **packet** here means one complete frame's SIXEL bytes. A **dispatcher** is the worker that delivers those packets and reports completion. The [animation threading reference](../threading/animation.md#current-frame-pipeline) owns the exact stage rules; their implementation is [encoder-playback-independent.inc.h](../../src/encoder-playback-independent.inc.h).

| Stage | Plain meaning |
| --- | --- |
| P: prepare | Prepare the input and construct its palette. |
| S: solve | Select a palette and prepare GPU work. Complete a solve without reading retained history when the GPU component supports it. |
| R: resolve | Finish palette application as needed. With delta enabled, compare with retained history and advance it in frame order. |
| E: encode | Turn the completed indexed image, whose pixels name palette entries, into a private SIXEL packet. |
| Dispatcher | Write packets and call `done` in admission order. No bytes for the next frame are sent until the current callback returns. |

The timelines below use imaginary time steps, not measured durations. On the left, B means build the current frame's output; W includes writing and flushing; a dot is an optional timing wait. On the right, numbers are frame numbers. Three slots can keep these three frames in flight. The right-hand schedule requires the eligible GPU controller; ordinary preparation overlap has fewer lanes. The left-hand schedule follows [`chafa.c`, `run_generic()`][c-loop].

```text
chafa CLI                         libsixel independent playback
time --> 1   2   3   4   5   6     time --> 1   2   3   4   5   6   7
main     B1  W1  .   B2  W2  .     P        1   2   3
                                  S            1   2   3
B2 begins after W1 and its wait.   R                1   2   3
File prefetch is a separate lane.  E                    1   2   3
                                  write/done               1   2   3
                                  At step 3: P3, S2, R1 coexist.
```

This is the same distinction as [band overlap versus frame overlap](#tell-frame-overlap-from-band-overlap). chafa's inspected path uses parallel batches inside a frame and a separate byte writer, with a frame boundary between rendering/output cycles. The libsixel controller can put different frames in different stages. This observation does not rank their speed. Sources: [chafa's batch helper][c-batch] and [CLI loop][c-loop]; [libsixel's controller](../../src/encoder-playback-independent.inc.h).

| Question | chafa's inspected CLI SIXEL path | libsixel playback |
| --- | --- | --- |
| What is bounded? | File-prefetch slots and an output byte FIFO. Neither is an application reservation for an animation frame. See [`chicle_media_pipeline_new()`][c-files] and [`chafa_stream_writer_write()`][c-writer]. | Capacity covers reservations and admitted frames through completion. `WAIT`, `NOWAIT`, and tickets let the producer control admission. |
| Are generated palettes reused across frames? | Each fresh canvas generates its own palette from scaled pixels. Fixed palette modes use predefined colors. Palette definitions are emitted again in each SIXEL image; there is no latest-completed-frame palette store in this path. See [`build_canvas()`][c-canvas-cli], [`setup_palette()`][c-canvas], [`chafa_palette_generate()`][c-palette], and [`build_sixel_palette()`][c-sixel]. | `PER_FRAME` uses the current palette; eligible `LATEST` can borrow a compatible earlier one; `AUTO` tests which policy helps. `palette_sequence` records the selection. |
| What picture is remembered? | Decoder state and pixel buffers survive between frames. The CLI replaces its rendering canvas each frame. Mapping caches and diffusion error rows are created for that draw, then released. See [the loader table](#follow-a-frame-through-chafa), [`run_generic()`][c-loop], and [`draw_pixels()` / `draw_pixels_pass_2_fs()`][c-indexed]. | Optional retained history survives to compare later updates, including damaged rectangles. R owns that history in the independent controller. |
| Does it skip unchanged output pixels? | The inspected renderer has no previous-frame comparison or damaged-rectangle input. It converts the current canvas to SIXEL. Transparent pixels can leave the display underneath, but this is not a computed delta. See [`draw_all_pixels()`][c-canvas] and [`chafa_sixel_renderer_build_ansi()`][c-sixel]. | [Delta encoding](output/delta-encoding.md) can retain suitable pixels. Per-frame origin and threshold limits describe partial updates. |
| What runs on a GPU? | The inspected scaling, palette, mapping, and SIXEL paths use CPU work and GLib worker batches. They contain no GPU solve/resolve stage. See [`draw_pixels()`][c-indexed], [`chafa_palette_generate()`][c-palette], and [`chafa_process_batches()`][c-batch]. | Eligible RGB8 sessions use the independent controller for GPU diffusion components and GPU lookup. Availability depends on the selected method and policy. |
| How is completion observed? | The CLI flushes a byte writer after each frame. There is no admitted-frame result callback in this loop. See [`run_generic()`][c-loop] and [`chafa_stream_writer_flush()`][c-writer-flush]. | Ordered `done` callbacks report per-frame status, palette sequence, and other results. `drain` waits for admitted frames and callbacks. |

chafa's CLI normally makes SIXEL animation pixels opaque unless transparency was explicitly configured. This helps cover the preceding image. It does not implement retained-history comparison. See [`tools/chafa/chafa.c`, `build_config()`][c-config], and [`chafa/chafa-canvas.c`, `draw_all_pixels()`][c-canvas].

### Timing, errors, and stopping

chafa measures the current frame's conversion and output time. It subtracts that time from the requested delay, adjusted by the speed or frames-per-second option. It can sleep for the remaining time; a late frame gets no extra sleep. There is no lateness-based frame dropping in this loop, so slow processing can stretch playback. It does not skip ahead to catch up. See [`tools/chafa/chafa.c`, `run_generic()`][c-loop].

| Event | chafa behavior at the studied commit | Comparison with playback |
| --- | --- | --- |
| Input file cannot be opened | The file pipeline returns a loader error. The CLI reports it, counts a failed file, and moves on. See [`thread_func()`][c-files] and [`run_vertical()`][c-vertical]. | Playback accepts already supplied frames; file decoding belongs to the caller. |
| Decoding stops partway through animation | Handling depends on the loader. Animated PNG stops at a failed next-frame decode and keeps the successful prefix for later loops. A null pixel pointer breaks the CLI frame loop without changing its result to `FILE_FAILED`. See [`chicle_png_loader_goto_next_frame()`][c-png] and [`run_generic()`][c-loop]. | An admitted frame has one ordered completion result; unstarted work after a session failure reports `SIXEL_INTERRUPTED`. |
| An I/O error is returned to the writer | The writer stops and wakes waiters. Its flush function also ends when the writer has stopped, and returns true without distinguishing that error from a normal drain. The CLI has no per-frame write result here. See [`thread_main()`][c-writer] and [`chafa_stream_writer_flush()`][c-writer-flush]. | `done` reports failures and `drain` returns the first session failure. Already written bytes cannot be undone. |
| User interrupts | On builds with signal handling, the CLI sets an interrupt flag. The frame loop and short sleep slices check it. A second signal forces exit; where interval timers exist, a 500 ms grace alarm also forces exit. This is process stopping, not frame cancellation with callbacks. See [`chafa.c`, `sigint_handler()`, `install_fast_exit_alarm()`, `fast_exit()`, and `interruptible_usleep()`][c-stop]. | `cancel` interrupts unclaimed admissions while claimed frames finish through delivery. `drain` waits for their callbacks. |

A successful byte drain means output has left the program, not that a terminal has shown the frame. chafa's writer marks itself drained after its pending writes; libsixel's results likewise describe host-side completion. See [`chafa/chafa-stream-writer.c`, `thread_main()`][c-writer] and the [libsixel header](../../include/sixel.h.in).

This comparison is source inspection, not a benchmark or a runtime test of chafa. It does not establish the behavior of every decoder backend, every platform's signal handling, or other libchafa callers. It also does not establish when each scheduling feature first appeared. The dated release notes credit chafa's earlier SIXEL animation support; the pinned code establishes the particular behavior described here.

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

The chafa comparison is backed by the pinned source references below, not by libsixel's tests. No chafa build or runtime result is claimed.

## References

All chafa links name files at commit `c897fce60cc4e947a254770ea1c2e320d58e7dce`. The comparison names the relevant functions beside each claim. The release-note reference supplies historical dates only.

[c-news]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/NEWS#L754-L783
[c-loop]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chafa.c#L675-L892
[c-vertical]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chafa.c#L943-L1014
[c-canvas-cli]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chafa.c#L577-L892
[c-config]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chafa.c#L533-L574
[c-write-image]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chafa.c#L274-L529
[c-stop]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chafa.c#L65-L194
[c-files]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chicle-media-pipeline.c#L86-L310
[c-grid]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chicle-grid-layout.c#L475-L507
[c-gif]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chicle-gif-loader.c#L79-L262
[c-png]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chicle-png-loader.c#L268-L345
[c-webp]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chicle-webp-loader.c#L293-L354
[c-webp-new]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/tools/chafa/chicle-webp-loader.c#L145-L232
[c-canvas]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/chafa-canvas.c#L221-L490
[c-print]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/chafa-canvas.c#L866-L978
[c-indexed]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/internal/chafa-indexed-image.c#L72-L553
[c-palette]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/internal/chafa-palette.c#L530-L1074
[c-sixel]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/internal/chafa-sixel-renderer.c#L416-L560
[c-batch]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/internal/chafa-batch.c#L71-L178
[c-term]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/chafa-term.c#L841-L909
[c-writer]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/chafa-stream-writer.c#L48-L522
[c-writer-flush]: https://github.com/hpjansson/chafa/blob/c897fce60cc4e947a254770ea1c2e320d58e7dce/chafa/chafa-stream-writer.c#L546-L558
