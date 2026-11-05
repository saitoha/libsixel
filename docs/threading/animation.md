# Animation Threading

For input formats, loader support, playback controls, timing, and output-state interactions, see [Animation playback in img2sixel](../encoder/animation.md). This document covers scheduling and frame ownership. Playback and prepare-ahead workers inherit the owning encoder’s resolved policy snapshot, including its frame keys. Serial playback submission enters the same snapshot. The [palette policy](../cli/threading-policy.md#precedence-and-interactions) defines playback overrides and inheritance; frame mode waits for each frame’s own palette, while eligible latest sessions select a completed palette at application start.

## Current frame pipeline

Supported multi-frame loaders hand frames to the encoder through a bounded read-ahead queue. Its depth is `load_depth`, default 4 and maximum 16. With `load=ahead`, loading overlaps downstream work; `load=serial` keeps the loader callback synchronous. The [threading-policy reference](../cli/threading-policy.md) owns these controls and their precedence.

With `prepare=ahead`, frame n+1 can be prepared while frame n is emitted. The two lanes overlap independent preparation and output work; output and retained delta history are still committed one frame at a time in order. GPU palette application also permits this overlap: the helper performs CPU frame preparation and palette construction, while ordered emission acquires the GPU lookup plan and backend lease. The helper does not apply a palette or write output. Helper availability follows the shared [encoder capability](encoder.md#test-coverage); builds without threading, PCC and TinyCC keep serial preparation. Tests that gate on preparation callbacks skip on those builds instead of waiting for a worker that cannot start. `prepare=serial` disables this helper; a GPU auto-policy cutoff does not disable CPU preparation overlap.

```text
loader / callback       preparation                 ordered emission
-----------------       -----------                 ----------------
load frame 0 ---------> prepare frame 0 -----------> emit frame 0
load frame 1 ---------> prepare frame 1 (overlap) --> emit frame 1
load frame 2 ---------> prepare frame 2 (overlap) --> emit frame 2
wait when queue full                                commit in frame order
```

The public `sixel_playback_*` API has a separate admission bound: up to its configured `CAPACITY`, independently of loader `load_depth`. Reservations and submitted frames count toward that capacity. The API supplies its own frames and does not turn loader read-ahead depth into an admission limit. Its sequences start at one in submission order, independently of reservations. RGB inputs need no animation flag. The [public API comments](../../include/sixel.h.in) describe callbacks and `palette_sequence`; the [policy reference](../cli/threading-policy.md#precedence-and-interactions) owns explicit-config precedence, including zero, and NULL-config inheritance.

The retained two-frame timeline below records a specific measurement revision in [threading-run.json](measurements/threading-run.json). It shows serial frame-level intervals with internal dither and encode workers. It is measurement evidence for that run, not a representation of every permitted prepare/emit overlap.

![Four-worker animation timeline](measurements/animation-thread4-timeline.png)

The fixture intentionally has two frames and no loop extension. An infinitely looping animation is unsuitable for a reproducible timeline command because neither the JSONL record nor the SIXEL output has a natural completion point.

## What is and is not shared

Preparation owns private frame and palette state; emission owns ordered output and temporal history. Caller-local budget scopes limit preprocessing, and ordinary next-frame preparation does not reduce emission allocation in either palette mode. Latest preparation returns after starting an owned build; emission waits for display time, selects a completed snapshot and creates a private application dither. Frame completion joins its own build before reporting the result, even when another palette was applied. An own-build error therefore fails the frame even after bytes have been written. The store retains at most two own jobs, two future records and one eligible completed record. A selection retains its snapshot through application, independently of later store replacement. Source masks, origins, diffusion state and history belong to the current frame rather than the snapshot. The [mode explanation](policy-modes.md#frame-overlap-and-ordered-publication) places these stages in the complete scheduling model.

Ahead preparation requires a generated palette without a stateful dither cache. Presentation pairs, DRCS, palette capture and progressive contexts retain serial preparation. A change of the AUTO working colorspace also defers the next frame to the ordered owner, so it cannot change coordinates underneath the current emission. The current frame owns diagnostic dither publication; the helper owns its frame job, pristine recovery copy and palette. GPU latest/auto palette-store eligibility is unchanged by frame preparation overlap.

Each component owns its independent pools; encoder_core can retain drained workers and scratch under the [encoder reuse conditions](encoder.md#six-row-jobs-and-ordered-output). The budget is not a process-wide scheduler: control, loader-handoff and writer threads exist outside planned work slots, and separate API clients are not jointly admitted against one root quota. [Threading overview](overview.md#worker-budget-is-not-a-process-thread-limit) defines that distinction.

Within a latest session, application/encoding M receives the whole encoder budget N. Its fixed geometry and stage limits come from `sixel_dither_encode_allocate`, exactly as in frame mode. Ordinary next preparation retains its independent `prepare_workers` cap. Palette construction P has a bounded pool lease and borrows live CPU slots separately: its controller and child callbacks acquire loans only when emission leaves capacity. Pool submission and join release the caller’s CPU claim, allowing unused stage capacity, draining bands and output waits to admit background work. Emission resumes without waiting for borrowers; already admitted background jobs finish before trying to renew their loans. Latest-only preparation Q, the owned full-frame input copy, acquires a loan for its copy job; common sampling remains independent. P and Q together stay within the preparation cap at admission. Pool callbacks and serial solver iterations or box splits are cooperative boundaries; unsplit setup and finalization remain serial jobs. Thus reclamation is bounded by work granularity, not elapsed time, and a running job can temporarily overlap renewed emission demand. Publication and thread join remain distinct lifetime boundaries. The first frame and incompatible-palette recovery wait for the own build outside emission work accounting; final cleanup also releases emission demand before joining.

The palette scheduler initializes mutexes and condition variables only when thread support is enabled. A build without threads still initializes a disabled scheduler, retains the resolved policy budget and grants no asynchronous lane reservations; synchronization stubs must not turn serial frame setup into an initialization error. The serial test also covers a one-slot budget and explicit frame mode in thread-enabled builds.

A process-wide coordinator would need to account for both prepare and emit lanes, their internal pools and other concurrent clients. Dividing a budget eagerly at animation start would not model loader stalls, temporarily empty queues or completion-driven reclamation.

## Temporal state constrains parallelism

Animation is more than an ordered list of independent still images. Delta output retains the colors left on the display plane across frame boundaries. Emission commits that state in display order. Ahead preparation can predict whether delta output needs an alpha/keep palette entry. If committed history requires a different reservation, the worker discards the prepared result and prepares again from a pristine source copy before emission. Latest first checks the selected snapshot’s reservation and color profile; an incompatible candidate waits for the current frame’s own palette. A stale own prediction is joined and rebuilt from the pristine source. Epoch changes invalidate both published future records and outstanding attempts, so an obsolete completion cannot replace a repaired palette. Loader loops keep sequence continuity; input boundaries, detach and bypass behavior follow the [palette contract](../cli/threading-policy.md#contract). This correctness rule applies where that reservation affects the palette; it is not a general reason to redo every prepared frame.

Frame ownership also matters. The handoff can retain a shareable frame by reference, or clone it when later preprocessing could conflict with loader reuse. Performance changes must preserve that lifetime contract as well as visual ordering. Packed-alpha separation also counts as preprocessing that mutates storage, even if no explicit float precision, crop, resize or workspace conversion requested a copy. The existing planner marks packed-to-RGB normalization as a colorspace operation, so handoff already requires a copy; see the [generated animation contract](../encoder/output/output-colorspace.md#generated-palettes-in-animation).

Cancellation records the last admitted sequence under the queue mutex. A frame already claimed by the ordered owner may finish; any earlier admitted frame still held by the preparation helper is cancelled even though it has left the queue. Later submissions exceed that watermark and remain eligible. Cancellation does not interrupt a running preparation task. The owner waits for it, releases its job and palette workers, and only then reports its completion callback, which may recycle borrowed pixels. Output failure follows the same drain-before-release rule for the next prepared frame.

## Future budget-coordinator requirements

A process-wide budget design should be attempted as a separate architecture change. At minimum it needs:

1. one explicit root budget for the operation;
2. incremental leases for only the stages that are runnable now;
3. reclamation when palette, dither, decode, or encode stages finish;
4. no hidden worker creation outside the accounted control threads;
5. ordered frame publication and temporal-state commit; and
6. timeline fields that record requested, granted, active, and returned workers.

The retained timelines establish host-specific measurement baselines. They do not prove process-wide accounting or simultaneous publication of frames.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| AT-01 | Serial palette scheduling initializes successfully, grants no lanes and preserves the requested budget, including builds without threads. | [tests/encoder/planning/0045_palette_scheduler_serial.t](../../tests/encoder/planning/0045_palette_scheduler_serial.t) |
| AT-02 | With preparation overlap available, cancellation includes a prepared frame outside the queue. | [tests/encoder/planning/0189_prepare_ahead_cancel_prepared.t](../../tests/encoder/planning/0189_prepare_ahead_cancel_prepared.t) |
| AT-03 | With preparation overlap available, submissions after cancellation remain eligible. | [tests/encoder/planning/0190_prepare_ahead_cancel_new_admission.t](../../tests/encoder/planning/0190_prepare_ahead_cancel_new_admission.t) |
| AT-04 | With preparation overlap available, output failure drains preparation before the next completion releases ownership. | [tests/encoder/planning/0191_prepare_ahead_write_failure_drain.t](../../tests/encoder/planning/0191_prepare_ahead_write_failure_drain.t) |
| AT-05 | Explicit serial preparation does not enter the helper lane. | [tests/encoder/planning/0192_gpu_ahead_explicit_serial.t](../../tests/encoder/planning/0192_gpu_ahead_explicit_serial.t) |

The gated cancellation and failure cases select the CPU backend, so their ownership evidence does not depend on Metal initialization or availability. GPU dispatch and output equivalence are covered by the [encoder execution tests](encoder.md#test-coverage). These tests establish ownership and output behavior, not a scheduling deadline.
