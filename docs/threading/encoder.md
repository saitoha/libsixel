# Encoder Threading

This explanation is for users and contributors inspecting encoder scheduling, allocator details and measurements. The [threading-policy reference](../cli/threading-policy.md) owns options and defaults; [policy modes](policy-modes.md) compares mode behavior across stages.

## Execution stages

A queued frame carries its own timeline snapshot. Workers may copy that snapshot back only into private work frames; a shared replay frame retains the loader-owned timeline, which can already describe a later loop. This preserves the zero-copy pixel handoff. The ownership regression blocks the first writer, queues a shared frame, advances its loop number, then releases the writer and checks that completion leaves the source timeline unchanged.

With `palette=frame`, the encoder has several opportunities for overlap. The complete pipeline is broader than the final dither/encode window. [Animation threading](animation.md#what-is-and-is-not-shared) describes the separate construction and application lifetimes of latest sessions:

```text
load and normalize frame
          |
          +---- optional palette-build worker ----+
          |                                        |
          +---- independent main/preplan work -----+
                                                   v
                                    completed palette
                                                   |
                         +-------------------------+
                         v
          palette application / dithering work bands
                         |
                  completed index rows
                         |
             six-row encode-job queue
                         |
                 encode worker pool
                         |
                  ordered writer thread
```

In balanced and greedy modes, the planner splits eligible asynchronous palette work and the main pre-plan into `floor(N/2)` and `ceil(N/2)` execution slots respectively. Each side includes its calling thread. The split is the initial overlap reservation. A large float working-space conversion polls for palette completion at chunk boundaries, joins the completed palette thread, and expands its pool to use the returned slots. Palette application and final encoding use the configured budget after collection. With two slots, each overlapping side is serial. With four or more, both the sample conversion for `-X` and the full-frame conversion for `-W` can run in parallel. [Policy modes](policy-modes.md#independent-work-and-data-fan-out) explains lazy branch reservations.

With eligible GPU playback, the next frame can run CPU sampling and palette construction while the current frame applies its palette on the GPU and encodes completed rows on the CPU. The helper owns a separate frame job and planner; GPU lookup construction and lease acquisition stay on the ordered emission path. Each lookup component retains its implementation and cache ownership. [Animation threading](animation.md#what-is-and-is-not-shared) defines admission, ownership and stop behavior.

K-means, K-medoids and K-center setting getters parse registry defaults into caller-local storage, after applying the existing override precedence. Their first resolved defaults remain cached for later calls. The shared environment mutex protects both the ready flag and cached value through `sixel_palette_option_cache()`: concurrent first callers publish once and receive the same winning value. Parsing occurs outside the lock; a lock failure leaves the local result uncached without accessing shared storage. Build setup retains the selected settings outside pixel loops. The concurrent palette-store test exercises publication and first-use K-means resolution under ThreadSanitizer; a separate test changes the environment between calls to verify first-value retention.

The indexed EncoderCore groups one, two or four contiguous six-row bands into a pool job. It starts with one and learns from completed worker time per band across compatible frames. A 200-microsecond estimated job budget, a smoothed cost and two consecutive light observations limit expansion; a sudden expensive frame reduces the next frame's grouping immediately. This is a scheduling heuristic, independent of video frame rate. Failed frames or unavailable timing discard the estimate.

Only complete, already published bands may be grouped. The controller never waits for future rows to fill a group, limits groups by the queue's band capacity, and reduces grouping near the tail to leave work for the reserved workers. Queue credits still count bands, rather than job descriptors. Each band publishes its own output fragment; the ordered writer, cancellation and pool drain retain their existing responsibilities. Timeline `controller/encode/job_bands` events report the admitted group size; per-band dispatch events remain available.

## Float colorspace transactions

The frame API and the float path of `sixel_helper_convert_colorspace()` share the same driver in [src/colorspace.c](../../src/colorspace.c). Above `SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS`, it resolves the available budget through `sixel_threads_resolve_fanout()`, starts at most `N-1` pool workers, and includes the caller in conversion. Standalone conversions use one chunk per slot; an overlapping conversion larger than 16,384 pixels uses 16,384-pixel chunks and a bounded queue, with one caller chunk per submission batch. Every chunk converts into disjoint temporary RGB float32 storage using the same double-intermediate arithmetic as serial conversion. Only after all jobs have joined and succeeded does the controller copy the result into the original image. A nonfinite input, domain error, or float32 overflow leaves all original pixels and frame format tags unchanged; the caller receives the serial diagnostic for the earliest failing chunk.

Scratch storage costs 12 bytes per pixel, approximately 24.9 MB for 1920×1080, plus one status per chunk. This trades memory and a final copy for one arithmetic pass instead of separate validation and write passes. Small conversions and initial scratch or pool-creation failures use the serial two-pass path without full-frame scratch storage. Failure to grow an existing pool retains its workers and completes every queued chunk; a partially successful growth is charged its entire requested reservation and is not retried. Palette elements and individual colors use the explicitly serial `sixel_color_convert_rgbf32()` utility. Frame-sized callers use `sixel_color_convert_frame_rgbf32()`.

Each pool worker owns a small zero-initialized workspace record and lazily allocates an optional 16384-entry float memo at its first substantial chunk. The pool also allocates a fresh workspace for each worker added by `grow()`. A worker reuses successful exact conversions across its chunks; the controller owns another memo for its own chunks. Memos are private to one transaction and source/target pair. Pool cleanup releases worker tables after joining, and the controller releases its table before returning. Allocation failure disables that owner's memo without failing conversion or repeatedly retrying. Serial calls own a memo through validation and writeback. The [float arithmetic policy](../concepts/pixelformat-precision/README.md#float-conversion-arithmetic) owns the exact-key and numeric contracts.

The encoder registers stack-owned, caller-local budget scopes around the palette worker and the main pre-plan. Scope registration and lookup use the thread-configuration mutex and live thread identities, without compiler-specific TLS. Each scope carries the resolved object policy snapshot; nested stage scopes compose their limits by minimum. Public work entries and the palette, preparation and frame-pipeline workers enter that snapshot, while threadpool jobs inherit the policy with one reserved slot. A reclaimable scope raises its own limit only after the donor has exited, and still respects enclosing limits. The callback runs on the owner outside the configuration mutex. Palette child pools do not submit to or wait for the parent palette job, so the parent can participate in conversion and join its children without a pool dependency cycle. Scope removal restores the previous budget even when preprocessing fails. These scopes partition a single encoder's overlapping stages; independent concurrent API clients resolve their own policies and have no shared admission scheduler.

Palette `budget-return` records follow palette completion and native-thread join; colorspace `grow` records show the newly available reservation (thread creation can still fail). Existing workers may drain the queue before newly created workers are scheduled, so distinct worker identities do not measure the returned reservation. This completion-driven handoff keeps the palette critical path independent of conversion and requires no estimate of either stage's runtime.

When the main pre-plan places a float colorspace filter immediately before gamut mapping, a two-element queue can overlap 131,072-pixel tiles. Admission requires at least 262,144 pixels and two available fan-out slots. The caller retains `N-1` slots for ordinary colorspace conversion; one threadpool worker owns the gamut filter. The pipeline uses its enclosing preparation reservation, including a concurrent palette reservation, and neither branch reclaims the other's live slot. One private float plane isolates publication until all tiles finish; queue descriptors are bounded and disjoint pixel ranges remain alive through join. Unsupported bindings, progress callbacks, fractional alpha, small frames, lazy fan-out and resource-admission failures keep sequential filters. A runtime failure drains and joins before replaying the original filters to preserve their diagnostic and publication order. This mechanism is separate from loop fusion and does not promise a speedup over full-budget sequential stages.

Timeline `controller/filter-pipeline` produce-start/finish and `worker/filter-pipeline` consume-start/finish records identify tiles and native execution lanes. The controller's start/finish `job` field records the admitted total tile budget; it is split into `N-1` producer slots and one consumer slot. The planner's colorspace-to-gamut pipeline edge denotes eligibility; these runtime events establish actual admission and overlap. Existing frame preparation and dither/encode pipelines keep their own boundaries.

Timeline `controller/colorspace` start/finish records enclose the scratch transaction, including validation and publication. `worker/colorspace` events identify each chunk, including the caller's chunk. Both working-space and asynchronous sample-space conversions have a `filter/worker` colorspace-start/finish envelope. Events establish execution and ordering; durations remain benchmark observations rather than test assertions.

## Adaptive sample extraction

Sampling finishes before palette construction and preprocessing can overlap. Its grid and solid-region phases resolve the caller's budget with `sixel_threads_resolve_fanout()`, including any enclosing `sixel_thread_budget_enter()` scope. Each phase includes the caller in its budget and joins all children before returning. Thresholds, format eligibility, deterministic histogram reduction and the byte-equivalence contract belong to [parallel adaptive extraction](../encoder/palette-construction/pipeline/README.md#parallel-adaptive-extraction).

Within the existing `filter/worker` sample-start/sample-finish envelope, each parallel phase emits `controller/sample` start/finish and nested `worker/sample` start/finish records. Worker records include the caller's partition and identify the solid or grid phase. These events prove participation and ordering without imposing a duration or a scheduling order between independent workers.

## Dither/encode budget policy

In frame mode, verbose `workers: total/main/palette` describes the earlier palette/preprocessing overlap, and dither and encoding receive the full `total` budget after palette collection. In latest sessions the dump is deferred until emission: `total` and `prepare_workers` retain the session limits N and W, while `latest leases: palette/prepare/emit` reports pool lifetime leases at application allocation. M always receives N, independently of P and ordinary preparation. Live CPU loans are separate from those leases; `SIXEL_TRACE_TOPIC=palette_latest` emits `LSXLOAN1` admission, return and foreground-demand events. Dither and encode limits apply within N. The shared allocator in [src/planner.c](../../src/planner.c) is called by both the early planner and [src/encoder-core-encode.c](../../src/encoder-core-encode.c). The verbose `pipeline` summary is labeled `resolution=pre-palette-estimate`: it uses requested palette and diffusion settings. Execution resolves the allocation again from the completed palette and effective diffusion method, which can become `none` when all original colors fit. `SIXEL_TRACE_TOPIC=dither_contract` reports that resolved allocation, including `allocation_mode`, worker reservations and band geometry.

The allocator distinguishes three execution modes. In `serial` mode, one caller applies the palette before encoding uses its available budget. In `pipeline` mode, dither and encode reservations coexist and their sum is bounded by the total budget. In `staged` mode, parallel work bands finish and every dither worker joins before encoding takes its own reservation. Equal full-budget D and E counts in staged mode therefore describe successive phases. Ordered writer and controller threads remain outside these work-slot counts, as described in the [threading overview](overview.md#worker-budget-is-not-a-process-thread-limit).

In balanced and greedy modes, for CPU spatial diffusion and positional dither with budgets two through four, the automatic mode is staged: `D = N` followed by `E = N`. There is no simultaneous row pipeline during these phases. A budget of one remains serial. Effective `none`, frame-global or unknown policies, and budgets above four retain the split starting at `D0 = ceil(0.7 N)` and `E0 = N - D0`, with at least two encode slots when `N > 2`. A one-slot encode side selects serial application followed by full-budget encoding. The low-budget rule avoids leaving most of the encode reservation waiting for rows; the cutoff retains row overlap at larger budgets instead of assuming that staged execution always wins. No additional pixel-count or requested-color-count threshold is introduced. The table applies when band application is eligible: indexed inputs that bypass application, a raster with at most one six-row output band, and OR-mode output with transparent offsets retain their existing direct paths. PCC, TinyCC and builds without threading retain their existing application restrictions.

| Budget | Spatial diffusion / positional dither | Effective `none` |
| ---: | --- | --- |
| 1 | Serial D1, then E1 | Serial D1, then E1 |
| 2 | Staged D2, then E2 | Serial D1, then E2 |
| 3 | Staged D3, then E3 | Pipeline D1 + E2 |
| 4 | Staged D4, then E4 | Pipeline D2 + E2 |
| 5 | Pipeline D3 + E2 | Pipeline D3 + E2 |
| 6 | Pipeline D4 + E2 | Pipeline D4 + E2 |
| 8 | Pipeline D6 + E2 | Pipeline D6 + E2 |
| 10 | Pipeline D7 + E3 | Pipeline D7 + E3 |
| 14 | Pipeline D10 + E4 | Pipeline D10 + E4 |

The [mode explanation](policy-modes.md#independent-work-and-data-fan-out) describes lazy's single producer and the effect of explicit stage limits. In the allocator, a `dither` limit alone retains the minimum encoder reservation. An `encode` limit alone leaves the remainder for dither in balanced and greedy modes. When both requests exceed the budget, both shrink proportionally until they fit. The [policy reference](../cli/threading-policy.md#precedence-and-interactions) owns configuration precedence; diffusion geometry belongs to the [dithering reference](../encoder/palette-application/dithering/README.md).


Geometry overrides alone do not disable staging; a work-band count can differ from its worker reservation. Default band height is the image height divided by D, rounded upward and aligned to six rows. Spatial diffusion retains six warm-up rows when the completed palette has at most 32 colors, capped at half a band, and zero above 32 colors. Stateless `none`, A/X positional dither and blue noise use zero automatic overlap. Explicit overlap, including zero, remains meaningful for every method. A GPU application claim reserves no CPU dither slots and assigns the full CPU budget to encode with six-row geometry. Both palette modes use N as the available emission budget.

Changing the dither reservation changes the independent error histories and can change decoded pixels even at the same total budget. Staging preserves the existing band algorithm; it does not promise serial-equivalent diffusion. The [band-seam quality guidance](measurement.md#band-seam-quality-protocol) describes the required comparison. Stateless methods retain exact pixels across these automatic geometry choices.

Queue depth is three times the initial encode reservation, capped by the number of six-row bands. A row pipeline grows its encode pool only after palette application returns and its dither workers have joined. A staged path admits encode jobs after that same join boundary; the indexed component can reserve an idle pool earlier during prepare. Neither path requires scheduler fairness or guarantees that every reserved worker executes a job.

### Historical allocation measurements

The allocation table and chart below retain the 2026-09-06 measurement at revision `0d8c1807eeccf109afd7c8de63ff038919fcb61b`, recorded in [threading-run.json](measurements/threading-run.json). They describe that revision's policies and diagnostic fields. The encoder timing curves on this page likewise describe their recorded historical configurations, not the current allocator.

The measured automatic allocation for `N = 1..12` is shown below. These runs
use `img2sixel` to resize the controlled 900 by 675 source to a 1920 by 1080
processed raster before encoding:

| `global_budget` | Planned dither | Planned encode | Runtime interpretation |
| ---: | ---: | ---: | --- |
| 1 | 0 | 0 | Serial palette application and encoding |
| 2 | 1 | 1 | Pipeline gate rejects one encode worker; stages serialize, then two encode workers run |
| 3 | 1 | 2 | First dither/encode overlap; dither is the caller-side producer |
| 4 | 2 | 2 | First overlap with a multi-worker dither pool |
| 5 | 3 | 2 | Parallel dither and encode |
| 6 | 4 | 2 | Parallel dither and encode |
| 7 | 5 | 2 | Parallel dither and encode |
| 8 | 6 | 2 | Parallel dither and encode |
| 9 | 7 | 2 | Parallel dither and encode |
| 10 | 7 | 3 | Parallel dither and encode |
| 11 | 8 | 3 | Parallel dither and encode |
| 12 | 9 | 3 | Parallel dither and encode |

In this historical measurement, the two-worker planner reported `1 + 1`, but runtime applied the palette serially and then used two encode workers. Actual dither/encode overlap began at three workers, with the caller acting as the single dither producer. A parallel dither pool began at four workers. The current allocator reports the resolved mode directly; the historical table must not be used to infer its low-budget staged rule.

![Measured encoder thread budget](measurements/thread-budget.png)

*Figure: Full HD encoder allocation measured through `img2sixel`. This chart
shows planned and observed worker counts, not elapsed time.*

The top panel shows planner allocation. The lower panel distinguishes workers
that actually executed jobs from post-dither encode capacity. After palette
application finishes, the encode pool asks to grow by the retired dither
budget. This makes the full worker budget available to a remaining encode
tail, but it does not guarantee that every added worker receives a job. In the
recorded run, the initial encode workers drained the queue before newly
available capacity performed work.

## Six-row jobs and ordered output

Palette application writes indexes into a shared image buffer. Each completed
six-row range becomes eligible for an encode job. Encode workers classify and
compose separate SIXEL body fragments, so they can finish out of order. A
dedicated writer waits for increasing band indexes and appends fragments to the
output callback in deterministic order.

The queue is bounded. Backpressure prevents palette application from getting
arbitrarily far ahead of encoding and bounds the number of buffered output
fragments. Source rows that are completely outside a transparent output extent
can be submitted as empty bands so the ordered writer still advances.

The writer and the thread performing palette application are not counted as
encode workers. Therefore a two-worker encode can show a caller/controller,
two encode workers, and one writer in the operating-system thread timeline.

Native Win32 workers inherit the creating thread's floating-point environment before entering their callback. MSVCRT thread startup can reset x87 precision to 53 bits while a MinGW main thread uses 64 bits; allowing that difference changes analytic conversion results merely by selecting a parallel budget. The environment is captured at native thread creation, as with pthread inheritance; this does not promise renewal of an existing pool after the caller changes its arithmetic environment. The [extended float equivalence test](../../tests/color/space/0037_float_memo_extended_exact.t) compares scalar-reference and worker results without a tolerance.

Each encode worker owns its maps, occupancy tables, column scratch and drawing-node storage. Successful indexed encoding drains worker callbacks and joins the ordered writer, then retains that capacity and the per-band byte buffers inside encoder_core. It releases image, palette, mask, sink and frame-policy references before returning. The next prepare can reuse storage only when encoded dimensions, color count, actual worker count, worker capacity, queue capacity, allocator and OR mode match. Changing source offsets, palette values, masks or output flags rebinds frame fields. Serial transitions, incompatible capacity, explicit abort, errors and final core destruction discard the retained resources. Cleanup resets the worker inventory and capacity together with the released array, and releasing an active context clears its cached-state flag before any parking decision. Pools without drain/resume methods retain terminal per-frame behavior. [Pool lifecycle](overview.md#worker-budget-is-not-a-process-thread-limit) owns admission and policy renewal.

Retained node capacity is the largest band plan handled by each worker; per-band byte capacity is the largest fragment for that band at compatible geometry. Maps remain sized to the current encoded width and color count. These are private high-water allocations, not a global cache. Idle native threads retain their stacks but consume no reserved work slots. Serial and legacy body paths retain their within-frame scratch lifetime. The ordered writer starts and joins for every frame.

Unemitted nodes belong to their start-column buckets; completed nodes belong to the worker's reusable list. Reusing a node overwrites its color, span, map pointer and link before publication. Failed composition discards partial buckets, maps and reusable nodes before another job; terminal teardown joins callbacks before freeing their storage. The [encoding policy](../encoder/output/encode-policy/README.md#implementation-and-existing-coverage) owns bucket ordering and fill semantics.

### Typed row publication and drain

In the indexed component path, IndexedFrame owns index and final skip storage. PaletteApply publishes only a completed contiguous source-row prefix, using release/acquire synchronization. encoder_core translates source readiness through the encoded top offset and schedules complete six-row output bands; it accepts the final partial band only after every source row is published. CPU policy scan order and warm-up overlap remain part of the selected policy. GPU application can publish completed row groups through the same boundary. The fused RGB8 consumer used by ordered, vptree, rbc, mahalanobis and certlut keeps lookup construction before the first group, then submits disjoint pixel ranges with original image coordinates. Small images, an absent row consumer and legacy GPU adapters keep one whole-image dispatch.

For eligible rasters of at least 262,144 pixels and twelve rows, the producer divides the image into approximately six groups, rounded up to complete six-row bands (168 rows at 1792 by 1008); the last group may be short. At most three command buffers are submitted but not consumed. Each completed group has its own index-validation flag; a failure in a later group cannot invalidate an earlier valid prefix. Only after completion and validation are its index and skip bytes exposed to CPU workers. Published rows are immutable. The callback runs serially on the caller, so CPU queue backpressure bounds submission without a Metal completion thread entering encoder code.

Cancellation is checked before submission and before publication. Command creation failure, GPU failure, invalid indexes or consumer rejection stops further publication and drains every submitted command before releasing image storage, the lookup binding or shared backend buffers. The partial image becomes FAILED and cannot commit retained history. The existing no-reentry rule for the PaletteApply callback also prevents nested GPU use while that binding is held; independent callers remain serialized by the backend lease.

The stream filter retains the image through producer completion and body-worker drain. Active resource users also pin its retained update, so abandoning a caller reference cannot free an in-flight transaction. A failed producer seals the image FAILED; abort drains body workers before releasing storage. Commit waits until baseline readers and body users are gone and the writer has accepted a complete stream. This is an encode-order boundary, independent of physical display timing. See the [component responsibilities](../encoder/execution-map/README.md#indexed-encode-components) and [commit coverage](../encoder/output/delta-encoding.md#indexed-commit-and-writer-coverage).

The parallel indexed writer exclusively owns `body_status` until its thread is joined. Producer-side submission uses the queue-synchronized status; it must not inspect the writer field while the writer is active. The serial path may read it directly. Existing parallel sampling output cases exercise this boundary under ThreadSanitizer; ordinary successful execution alone cannot establish absence of a race.

On the CPU path, RetainedPlane admits its caller-inclusive `worker_threads` ceiling in `begin_update`: zero and one mean serial execution, and negative values are rejected. The session passes the current frame's encode allocation. Small rectangles, hardware limits or optional worker-admission failure can reduce participation. Preparation freezes the row kernel and grows a component-owned persistent executor before output; commit neither allocates storage nor creates threads. The actual participation count is returned in commit statistics and the `retained_commit/workers` timeline event.

Each CPU commit captures the current execution policy, partitions disjoint complete rows and joins all participants before publishing the new sequence. Reused workers enter and leave that batch's policy and CPU-work callbacks, retaining no old frame callback context while idle. The caller releases its CPU loan while waiting. Lower later budgets use fewer cached workers; teardown joins the idle executor. A build without thread support keeps the same serial transaction semantics. See [retained-state transactions](../encoder/output/delta-encoding.md#indexed-retained-state-transactions) for byte and receipt semantics.

With coherent GPU history, the stream invokes RetainedPlane `stage` after PaletteApply has completed and before joining the final body workers. GPU completion waits outside the plane gate. Read leases and body consumers still drain before receipt-bound commit; the GPU candidate then publishes without a CPU pixel traversal. `retained_stage/start` and `finish` include its input copies, command wait and statistics reduction. These intervals can overlap body encoding and must not be added as disjoint pipeline stages. `retained_commit/workers = 0` identifies GPU publication; CPU commits still include their calling thread.

## Timeline interpretation

The encode controller records `context_begin` for cold preparation and `context_reuse` for a compatible retained context. These events establish the selected allocation path; their duration is not a speed guarantee.

The `loader:builtin` spans in these figures decode the 900 by 675 source PNG;
they do not decode a Full HD PNG. The complete preprocessing sequence for the
controlled `-Wgamma -Xoklab` command is:

```text
builtin PNG decode: 900x675 gamma RGB888
              |
              v
colorspace/pre: gamma RGB888 -> linear RGB float32
              |
              v
resize: 900x675 -> 1920x1080 in linear RGB float32
              |
              v
colorspace/post: linear RGB float32 -> gamma RGB888 (-Wgamma)
              |
              v
palette input: gamma RGB888 -> Oklab float32 (-Xoklab)
              |
              v
K-means palette build
```

The first two conversions bracket the `scale` interval rather than belonging
to it. In the retained eight-worker run, linearization took 3.888 ms, the
linear-RGB resize took 26.164 ms, conversion back to gamma RGB took 6.412 ms,
and the later Oklab conversion took 8.926 ms. Thus the scale bar alone omits
19.226 ms of required color conversion before palette construction. The
repeated bars on each `colorspace #N` row belong to these distinct conversion
stages; the row identifies a reused operating-system thread, not one semantic
colorspace operation.

Across the retained two-, four-, and eight-worker runs, chunk creation took
0.541 to 1.015 ms, builtin pixel decode took 0.333 to 0.415 ms, and the
linear-RGB resize itself took 25.373 to 26.164 ms. The 1,823,391-byte source is
almost the size of its 1,822,500-byte unpacked RGB payload and was read from a
warmed filesystem cache. Its unusually short decode interval is therefore real
for this fixture, but is not representative of decoding a compressed Full HD
PNG.

At two configured workers, palette application completes before the band
workers begin useful encode work. The long `encode` interval is the frame-level
operation; the short worker intervals at its end are the six-row jobs.

![Two-worker encoder timeline](measurements/encoder-thread2-timeline.png)

*Figure: Single `img2sixel --threading-policy=greedy:global_budget=2` diagnostic run. The builtin loader decodes the 900 by 675 PNG; colorspace rows then show linearization, conversion back to gamma after the Full HD resize, and Oklab palette-input conversion.*

In the retained historical four-worker run, two dither work bands overlap two initial encode workers. The
solid dither spans are work-band lifetimes; the thin encode marks are many
short six-row jobs. The frame-level interval and writer appear in addition to
the configured worker budget.

![Four-worker encoder timeline](measurements/encoder-thread4-timeline.png)

*Figure: Single `img2sixel --threading-policy=greedy:global_budget=4` diagnostic run. The builtin loader decodes the 900 by 675 PNG; colorspace rows then show linearization, conversion back to gamma after the Full HD resize, and Oklab palette-input conversion.*

At eight workers, six dither work bands overlap two initial encode workers.
The pool then requests the complete encode capacity after dithering ends. The
larger processed raster makes the relative span lengths easier to see than the
previous 900 by 675 timeline.

![Eight-worker encoder timeline](measurements/encoder-thread8-timeline.png)

*Figure: Single `img2sixel --threading-policy=greedy:global_budget=8` diagnostic run. The builtin loader decodes the 900 by 675 PNG; colorspace rows then show linearization, conversion back to gamma after the Full HD resize, and Oklab palette-input conversion.*

These charts use `tools/timeline.py --sort-order start`. Row numbering is local
to the rendered worker group and must not be interpreted as a stable thread
identity across separate runs.

## Worker-budget scaling

The following experiment varies `img2sixel --threading-policy=greedy:global_budget=N` from one through sixteen while keeping the Full HD processed raster and all non-threading policies fixed. The horizontal axis is a configured worker budget, not a reservation of physical cores. Workers are unpinned, the host is not isolated, and this arm64 macOS host reports fourteen logical CPUs.

This is specifically a 256-color experiment. Every point passes `-p 256`;
palette size is neither automatic nor varied in this chart. Here `256` is the
requested palette size, not a measurement of the number of distinct entries
that survived palette construction. The allocation chart and the two-, four-,
and eight-worker timelines above use the same fixed request. At 256 requested
colors the measured automatic dither work-band overlap was zero, so these results
do not characterize the lower-color overlap path.

Each point is the median of nine instrumented runs after two warm-ups. The
shaded region is the inclusive interquartile range. Successive rounds traverse
worker counts in alternating directions. The complete encoder interval begins
before image loading and ends after the frame has been encoded. The palette
line isolates palette construction, while the dither/encode tail begins at
palette-application preparation and ends with the complete encoder interval.
These stage lines expose bottlenecks but are not an additive partition.

![Full HD encoder time by worker budget](measurements/encoder-thread-scaling.png)

*Figure: Full HD encoder intervals measured through `img2sixel`; nine timed
runs follow two warm-ups at each configured worker budget. Lines show medians
and shading shows inclusive IQR.*

The one-worker encoder median was 859.297 ms. Two and three workers reached
only 1.09 and 1.12 times speedup because palette construction remained serial
and the dither/encode tail still used the serial or single-producer shape. Four
workers were the first point with a multi-worker dither pool and reduced the
median to 452.423 ms, a 1.90 times speedup. Eight workers reached 231.792 ms,
or 3.71 times faster than one worker.

Palette construction stayed close to 69 ms across all budgets. The complete
encoder curve flattened from 188.541 ms at thirteen workers to 185.138 ms at
sixteen; fourteen workers measured 188.888 ms, or 4.55 times faster than one
worker. The small ordering changes in that plateau are within this
non-isolated experiment and are not evidence that budgets beyond the host's
logical CPU count improve throughput.

The output remained deterministic across all eleven executions at each fixed
worker count, but it was not identical across worker counts. The retained
SIXEL sizes ranged from 1,106,783 to 1,114,189 bytes, a spread of about 0.7
percent. Parallel work-band boundaries alter error-diffusion history, so this
is a controlled threading comparison rather than an identical-output
microbenchmark. The raw
[`encoder-thread-scaling.csv`](measurements/encoder-thread-scaling.csv)
retains all 144 timed observations, 32 warm-up executions, phase intervals,
planner observations, output size and hash, and exact schedule positions.

## Palette-size scaling at selected worker budgets

The next experiment keeps the Full HD fixture and controlled encoder policies above, but varies the requested palette size with `-p K`. The twelve requested values are 2, 4, 8, 16, 24, 32, 48, 64, 96, 128, 192, and 256. Each panel fixes `global_budget` at 2, 4, 6, or 8. These are configured worker budgets, not reserved physical cores. The shared horizontal axis uses a log2 scale so the low-color region and the 32-color policy boundary remain legible.

![Full HD encoder time by requested palette size](measurements/encoder-color-scaling.png)

*Figure: Full HD encoder intervals measured through `img2sixel` while varying
the requested `-p K` palette size. Each panel fixes a 2, 4, 6, or 8 worker
budget. Nine timed runs follow two warm-ups per condition; lines show medians
and shading shows inclusive IQR.*

The complete interval grows with requested palette size at every budget, but
the worker budget changes the magnitude. The measured endpoints were:

| `global_budget` | 2 colors | 256 colors | 256/2 time ratio |
| ---: | ---: | ---: | ---: |
| 2 | 213.542 ms | 801.259 ms | 3.75 |
| 4 | 150.663 ms | 463.076 ms | 3.07 |
| 6 | 123.963 ms | 292.751 ms | 2.36 |
| 8 | 112.511 ms | 235.688 ms | 2.09 |

Palette construction increased comparatively gently, from about 45 ms at two
requested colors to about 71 ms at 256. Most of the observed color-count
dependence was in the dither/encode tail: its median rose from 70.286 to
633.478 ms at two workers, and from 16.501 to 112.525 ms at eight workers. The
retained SIXEL output also grew from roughly 273 KiB at two colors to about
1.06 MiB at 256 colors. These measurements identify the stage and output-size
covariation; they do not by themselves assign the cost to lookup, diffusion,
band encoding, or output writing.

The dotted line marks a second variable in the current automatic policy. Six
overlap rows are used through 32 requested colors, while requests above 32 use
zero overlap rows. The chart therefore describes the complete current policy,
but it does not isolate the cost of overlap from the cost of changing palette
size. A causal overlap comparison must hold `K` and every other policy fixed.

The raw
[`encoder-color-scaling.csv`](measurements/encoder-color-scaling.csv) contains
432 timed observations and 96 warm-ups. It records the requested color count,
worker budget, phase intervals, overlap decision, output size and hash, and
alternating schedule position for every execution.

### Quantizer-profile caveat

This palette-size sweep is not a default-policy benchmark. It explicitly uses
K-means with hard binning, a fixed seed, Ward merging, and Oklab clustering.
The top-level automatic palette policies normally retain the Heckbert
compatibility path. Merely deleting `--quantize-model=kmeans:seed=1` from this
command would not restore that path, because an explicit hard-binning request
causes automatic quantizer resolution to choose a weighted-point-capable
K-means implementation.

A focused check on the same Full HD processed fixture at 256 requested colors and `--threading-policy=greedy:global_budget=8` illustrates why the distinction matters. After two warm-ups, nine runs gave a 71.009 ms median palette-build interval for the measured K-means/hard profile. Explicit `heckbert:profile=compat` with compatible automatic binning gave 17.315 ms, about 4.10 times faster; the corresponding complete encoder medians were 232.411 and 179.358 ms. This is a profile comparison, not a one-variable quantizer comparison: Heckbert cannot consume the hard-binned weighted points used by the K-means condition. The spot check is evidence that the chart must not be extrapolated to default encoder latency, not a replacement for a repeated Heckbert palette-size sweep.

## Image-quality boundary

The six-row encoder jobs consume already chosen palette indexes and do not
change their colors. The image-quality risk in this pipeline is the preceding
split of causal dithering into larger horizontal work bands. Overlap rows warm
local error state but cannot exactly reproduce an unlimited full-frame scan.
The detailed mechanism and default overlap are in the
[threading overview](overview.md#work-band-seams-and-image-quality).

Keep performance and quality conclusions separate:

- a timing timeline establishes which stages overlap;
- a serial-versus-banded decoded-image comparison establishes visual error;
- a byte-size comparison establishes whether changed palette-index patterns
  affect SIXEL output size; and
- none of those alone establishes application-level latency on a terminal.

## Conditions that change the shape

The measured shape is not universal. It can change when:

- the image is too short to provide multiple SIXEL or dither work bands;
- the selected dither policy does not support parallel work bands;
- GPU policy claims palette application;
- explicit `band_width`, `band_overwrap`, or dither thread limits apply;
- transparency offsets make the pipelined OR-mode path inapplicable;
- palette construction overlaps other planning work; or
- a lookup policy uses shared or per-worker prepared state.

Record all such settings when comparing results.

## Test coverage

<!-- test-coverage: enforced -->

The fixed-budget colorspace tests select `greedy` explicitly so their worker counts, nested limits and pool-growth assertions remain the same on small machines. They test those configured reservations rather than the default mode; [policy modes](policy-modes.md#performance-cpus-and-size-thresholds) owns the `balanced` CPU cap.

| ID | Contract | Owning test |
| --- | --- | --- |
| TE-01 | Frame conversions at one, two, four and eight threads preserve identical bits for all pairs among gamma, linear, CIELAB, OKLab and DIN99d, including extended coordinates. | [tests/color/space/0039_frame_bits.t](../../tests/color/space/0039_frame_bits.t) |
| TE-02 | The public float helper has the same serial/parallel bit consistency. | [tests/color/space/0040_helper_bits.t](../../tests/color/space/0040_helper_bits.t) |
| TE-03 | A nonfinite final chunk leaves frame pixels and format tags intact and returns the serial diagnostic. | [tests/color/space/0041_frame_atomic.t](../../tests/color/space/0041_frame_atomic.t) |
| TE-04 | A nonfinite final chunk leaves public-helper pixels intact and returns the serial diagnostic. | [tests/color/space/0042_helper_atomic.t](../../tests/color/space/0042_helper_atomic.t) |
| TE-05 | API encoding with CIELAB working space executes a colorspace controller and at least two worker thread identities inside the filter interval. | [tests/color/space/0043_working_pipeline.t](../../tests/color/space/0043_working_pipeline.t) |
| TE-06 | API encoding with CIELAB clustering executes the asynchronous sample conversion with the same parallel event contract. | [tests/color/space/0044_clustering_pipeline.t](../../tests/color/space/0044_clustering_pipeline.t) |
| TE-07 | Caller-local scopes compose by minimum, restore on exit and do not change another thread's budget. | [tests/color/space/0045_budget_scope.t](../../tests/color/space/0045_budget_scope.t) |
| TE-08 | At eight threads with automatic sampling, the verbose post-palette geometry matches the fixed raster and all six dither jobs start and finish, and observed worker identities stay within the planned dither/encode budgets. Encode identities are counted before all dither jobs finish, excluding subsequent pool growth. A pool slot may remain idle while another worker consumes multiple queued jobs; the test does not require scheduler fairness. | [tests/encoder/planning/0017_pipeline_auto_worker_counts.t](../../tests/encoder/planning/0017_pipeline_auto_worker_counts.t) |
| TE-09 | At eight threads with full-frame sampling, the verbose post-palette geometry matches the fixed raster and all six dither jobs start and finish, and observed worker identities stay within the planned dither/encode budgets. Encode identities are counted before all dither jobs finish, excluding subsequent pool growth. A pool slot may remain idle while another worker consumes multiple queued jobs; the test does not require scheduler fairness. | [tests/encoder/planning/0018_pipeline_full_frame_worker_counts.t](../../tests/encoder/planning/0018_pipeline_full_frame_worker_counts.t) |
| TE-10 | Public encode-frame sampling encloses controller intervals and workers on at least two distinct threads within sample-start/sample-finish when four threads and a low threshold are requested. | [tests/frame/timeline/0008_timeline_parallel_sample.t](../../tests/frame/timeline/0008_timeline_parallel_sample.t) |
| TE-11 | At eight threads with K-means, Ward, CIELAB working and clustering spaces and LSO2, a completed palette reservation is returned during working conversion, the conversion requests pool expansion, and chunks complete afterward. Test hooks synchronize palette completion before conversion begins; assertions require palette completion, budget return, growth and subsequent work in that order, without requiring the scheduler to assign work to a minimum number of distinct threads. | [tests/color/space/0046_overlap_reclaim.t](../../tests/color/space/0046_overlap_reclaim.t) |
| TE-12 | Completion-driven donation respects nested limits and restores the configured budget. | [tests/color/space/0047_reclaim_budget.t](../../tests/color/space/0047_reclaim_budget.t) |
| TE-13 | Donation after the first conversion batch preserves serial float bits. | [tests/color/space/0048_reclaim_bits.t](../../tests/color/space/0048_reclaim_bits.t) |
| TE-14 | Failures in multiple donation-sized chunks preserve every input byte and the serial diagnostic. | [tests/color/space/0049_reclaim_atomic.t](../../tests/color/space/0049_reclaim_atomic.t) |
| TE-15 | Repeated colors in separated chunks, colliding memo keys and both zero signs retain uncached scalar result bits when the real conversion pool grows from four to eight execution slots. | [tests/color/space/0050_worker_memo_exact.t](../../tests/color/space/0050_worker_memo_exact.t) |
| TE-16 | Across seventeen chunks and pool growth, each of eight execution slots attempts at most one memo allocation and releases it at transaction end; rejecting every optional allocation preserves uncached output and does not retry per chunk. | [tests/color/space/0051_worker_memo_lifetime.t](../../tests/color/space/0051_worker_memo_lifetime.t) |
| TE-17 | The shared allocation mechanism gives the pipeline disjoint D/E reservations and staged execution full, successive reservations at budgets 1, 2, 3, 4, 6, 8, 10 and 14. Band heights are rounded to six rows; PCC and TinyCC use one dither worker and whole-image automatic band geometry while preserving the encode reservation. | [tests/encoder/allocation/0001_allocation_slots.t](../../tests/encoder/allocation/0001_allocation_slots.t) |
| TE-18 | An explicit dither cap keeps the existing minimum encode reservation and takes precedence over automatic staged selection. Explicit band height is rounded, explicit zero overlap remains zero, and overlap is capped at half a work band. | [tests/encoder/allocation/0002_allocation_controls.t](../../tests/encoder/allocation/0002_allocation_controls.t) |
| TE-19 | A GPU application claim reserves no CPU dither workers and keeps the entire CPU budget for encode with six-row geometry, independently of CPU dither controls. | [tests/encoder/allocation/0003_allocation_gpu.t](../../tests/encoder/allocation/0003_allocation_gpu.t) |
| TE-20 | Unknown application policies retain the existing overlap reservation rather than selecting independent staged work bands. | [tests/encoder/allocation/0004_allocation_serial_policy.t](../../tests/encoder/allocation/0004_allocation_serial_policy.t) |
| TE-21 | Staged low budgets and higher-budget pipelines execute their expected work bands within the total occupied-callback budget; staged encoding begins after application has joined, including explicit geometry with more jobs than slots. | [tests/encoder/allocation/0005_allocation_runtime_phases.t](../../tests/encoder/allocation/0005_allocation_runtime_phases.t) |
| TE-22 | A requested FS method that resolves to none after exact-fit palette construction keeps the baseline reservation and zero overlap at every tested budget. | [tests/encoder/allocation/0006_allocation_effective_none.t](../../tests/encoder/allocation/0006_allocation_effective_none.t) |
| TE-23 | CLI dither cap and geometry override conflicting environment settings before automatic allocation is selected. | [tests/encoder/allocation/0007_allocation_override_precedence.t](../../tests/encoder/allocation/0007_allocation_override_precedence.t) |
| TE-24 | None, A/X positional dither and blue noise preserve decoded pixels at one and four slots, in byte and CIELAB float paths, with zero automatic overlap or explicit six-row overlap. | [tests/encoder/allocation/0008_allocation_stateless_overlap.t](../../tests/encoder/allocation/0008_allocation_stateless_overlap.t) |
| TE-25 | Automatic staging applies only to supported spatial methods at budgets two through four, including the five-slot exclusion; stateless and low/high-color diffusion geometry remain distinct. | [tests/encoder/allocation/0009_allocation_automatic_policy.t](../../tests/encoder/allocation/0009_allocation_automatic_policy.t) |
| TE-26 | Animation playback that prepares the next frame while emitting the current one produces the same output as serial preparation, with and without 6delta, whose keep-key reservation the prepared palette predicts from the previous frame. | [tests/encoder/planning/0019_pipeline_prepare_ahead_bits.t](../../tests/encoder/planning/0019_pipeline_prepare_ahead_bits.t) |
| TE-27 | Frames submitted through `sixel_playback_t` produce the same bytes as successive `sixel_encoder_encode_frame()` calls, with and without 6delta, at one and eight threads and capacities one, two and four, and complete in submission order. | [tests/encoder/planning/0020_playback_direct_t1_d0_q1.t](../../tests/encoder/planning/0020_playback_direct_t1_d0_q1.t), [tests/encoder/planning/0123_playback_direct_t1_d0_q2.t](../../tests/encoder/planning/0123_playback_direct_t1_d0_q2.t), [tests/encoder/planning/0124_playback_direct_t1_d0_q4.t](../../tests/encoder/planning/0124_playback_direct_t1_d0_q4.t), [tests/encoder/planning/0125_playback_direct_t1_d1_q1.t](../../tests/encoder/planning/0125_playback_direct_t1_d1_q1.t), [tests/encoder/planning/0126_playback_direct_t1_d1_q2.t](../../tests/encoder/planning/0126_playback_direct_t1_d1_q2.t), [tests/encoder/planning/0127_playback_direct_t1_d1_q4.t](../../tests/encoder/planning/0127_playback_direct_t1_d1_q4.t), [tests/encoder/planning/0128_playback_direct_t8_d0_q1.t](../../tests/encoder/planning/0128_playback_direct_t8_d0_q1.t), [tests/encoder/planning/0129_playback_direct_t8_d0_q2.t](../../tests/encoder/planning/0129_playback_direct_t8_d0_q2.t), [tests/encoder/planning/0130_playback_direct_t8_d0_q4.t](../../tests/encoder/planning/0130_playback_direct_t8_d0_q4.t), [tests/encoder/planning/0131_playback_direct_t8_d1_q1.t](../../tests/encoder/planning/0131_playback_direct_t8_d1_q1.t), [tests/encoder/planning/0132_playback_direct_t8_d1_q2.t](../../tests/encoder/planning/0132_playback_direct_t8_d1_q2.t), [tests/encoder/planning/0133_playback_direct_t8_d1_q4.t](../../tests/encoder/planning/0133_playback_direct_t8_d1_q4.t), [tests/encoder/planning/0160_playback_direct_callbacks_t1_d0_q1.t](../../tests/encoder/planning/0160_playback_direct_callbacks_t1_d0_q1.t), [tests/encoder/planning/0161_playback_direct_callbacks_t1_d0_q2.t](../../tests/encoder/planning/0161_playback_direct_callbacks_t1_d0_q2.t), [tests/encoder/planning/0162_playback_direct_callbacks_t1_d0_q4.t](../../tests/encoder/planning/0162_playback_direct_callbacks_t1_d0_q4.t), [tests/encoder/planning/0163_playback_direct_callbacks_t1_d1_q1.t](../../tests/encoder/planning/0163_playback_direct_callbacks_t1_d1_q1.t), [tests/encoder/planning/0164_playback_direct_callbacks_t1_d1_q2.t](../../tests/encoder/planning/0164_playback_direct_callbacks_t1_d1_q2.t), [tests/encoder/planning/0165_playback_direct_callbacks_t1_d1_q4.t](../../tests/encoder/planning/0165_playback_direct_callbacks_t1_d1_q4.t), [tests/encoder/planning/0166_playback_direct_callbacks_t8_d0_q1.t](../../tests/encoder/planning/0166_playback_direct_callbacks_t8_d0_q1.t), [tests/encoder/planning/0167_playback_direct_callbacks_t8_d0_q2.t](../../tests/encoder/planning/0167_playback_direct_callbacks_t8_d0_q2.t), [tests/encoder/planning/0168_playback_direct_callbacks_t8_d0_q4.t](../../tests/encoder/planning/0168_playback_direct_callbacks_t8_d0_q4.t), [tests/encoder/planning/0169_playback_direct_callbacks_t8_d1_q1.t](../../tests/encoder/planning/0169_playback_direct_callbacks_t8_d1_q1.t), [tests/encoder/planning/0170_playback_direct_callbacks_t8_d1_q2.t](../../tests/encoder/planning/0170_playback_direct_callbacks_t8_d1_q2.t), [tests/encoder/planning/0171_playback_direct_callbacks_t8_d1_q4.t](../../tests/encoder/planning/0171_playback_direct_callbacks_t8_d1_q4.t) |
| TE-28 | Playback admission refuses a full queue without waiting, times out a bounded wait, counts both refusals, frees a released reservation and accepts a reserved submission without waiting. | [tests/encoder/planning/0021_playback_admission.t](../../tests/encoder/planning/0021_playback_admission.t) |
| TE-29 | Cancelled and unstarted frames complete in order with `SIXEL_INTERRUPTED`; successful frame fallback reports its admission sequence as `palette_sequence`, while failed/cancelled work reports zero. Release waits for admitted frames, the first failure is returned by later admissions and drains, and an attached encoder refuses ordinary calls until playback is released. | [tests/encoder/planning/0022_playback_cancel_failure.t](../../tests/encoder/planning/0022_playback_cancel_failure.t) |
| TE-30 | A completed own palette is joined before latest application fixes its geometry, retaining the full emission budget at four and eight slots. | [tests/encoder/planning/0074_latest_completed_build_reclaims_budget.t](../../tests/encoder/planning/0074_latest_completed_build_reclaims_budget.t) |
| TE-31 | A producer/consumer rendezvous under `greedy:global_budget=4` proves tile-stage overlap on distinct threads, an `N-1`/one slot split, an incomplete final tile and restoration of the caller budget. The explicit greedy policy keeps four slots on hosts with fewer performance CPUs. | [tests/color/gamut/0065_pipeline_overlap.t](../../tests/color/gamut/0065_pipeline_overlap.t) ([tests/color/gamut/0065_pipeline_overlap.c](../../tests/color/gamut/0065_pipeline_overlap.c)) |
| TE-32 | Reservation release wakes a full-queue waiter. | [tests/encoder/planning/0105_playback_wait_release.t](../../tests/encoder/planning/0105_playback_wait_release.t) |
| TE-33 | Writer completion wakes a full-queue waiter. | [tests/encoder/planning/0106_playback_wait_completion.t](../../tests/encoder/planning/0106_playback_wait_completion.t) |
| TE-34 | Writer failure wakes a waiter with the retained failure. | [tests/encoder/planning/0107_playback_wait_failure.t](../../tests/encoder/planning/0107_playback_wait_failure.t) |
| TE-35 | A blocked writer times out drain then drains after release. | [tests/encoder/planning/0108_playback_drain_timeout.t](../../tests/encoder/planning/0108_playback_drain_timeout.t) |
| TE-36 | B1 reservations remain bounded without a worker. | [tests/encoder/planning/0109_playback_serial_reservation.t](../../tests/encoder/planning/0109_playback_serial_reservation.t) |
| TE-37 | Parallel sampling retains exact serial-equivalent output; running this case under ThreadSanitizer also observes indexed writer status ownership. | [tests/encoder/sampling/0004_rgb888_plain_auto_t8_output.t](../../tests/encoder/sampling/0004_rgb888_plain_auto_t8_output.t) |

| TE-38 | Two participants produce the exact retained RGB for an offset rectangle with unequal row partitions. | [tests/encoder/indexed/0105_retained_workers_two_rgb.t](../../tests/encoder/indexed/0105_retained_workers_two_rgb.t) |
| TE-39 | Four participants produce the exact retained RGB for an offset rectangle with unequal row partitions. | [tests/encoder/indexed/0106_retained_workers_four_rgb.t](../../tests/encoder/indexed/0106_retained_workers_four_rgb.t) |
| TE-40 | Four participants preserve kept validity and mark only painted pixels valid. | [tests/encoder/indexed/0107_retained_workers_valid.t](../../tests/encoder/indexed/0107_retained_workers_valid.t) |
| TE-41 | Four participants preserve frame-local presentation marks outside painted lanes. | [tests/encoder/indexed/0108_retained_workers_paint.t](../../tests/encoder/indexed/0108_retained_workers_paint.t) |
| TE-42 | Four participants reduce paint counts exactly. | [tests/encoder/indexed/0109_retained_workers_count.t](../../tests/encoder/indexed/0109_retained_workers_count.t) |
| TE-43 | Four participants propagate approximate-keep refinement. | [tests/encoder/indexed/0110_retained_workers_refinement.t](../../tests/encoder/indexed/0110_retained_workers_refinement.t) |
| TE-44 | A reused executor applies the second batch callback context without reusing the first context. | [tests/encoder/indexed/0111_retained_workers_current_policy.t](../../tests/encoder/indexed/0111_retained_workers_current_policy.t) |
| TE-45 | An admitted executor runs with malloc, calloc and realloc rejected and makes no allocator calls. | [tests/encoder/indexed/0112_retained_workers_no_allocation.t](../../tests/encoder/indexed/0112_retained_workers_no_allocation.t) |
| TE-46 | A four-participant executor reuses only two participants when the next batch requests two. | [tests/encoder/indexed/0113_retained_workers_reuse_smaller.t](../../tests/encoder/indexed/0113_retained_workers_reuse_smaller.t) |
| TE-47 | A zero update budget commits with one participant. | [tests/encoder/indexed/0114_retained_default_serial.t](../../tests/encoder/indexed/0114_retained_default_serial.t) |
| TE-48 | An explicit one-participant update commits serially. | [tests/encoder/indexed/0115_retained_explicit_serial.t](../../tests/encoder/indexed/0115_retained_explicit_serial.t) |
| TE-49 | A small rectangle stays serial despite a larger requested ceiling. | [tests/encoder/indexed/0116_retained_small_serial.t](../../tests/encoder/indexed/0116_retained_small_serial.t) |
| TE-50 | A large update uses its two-participant ceiling. | [tests/encoder/indexed/0117_retained_budget_two.t](../../tests/encoder/indexed/0117_retained_budget_two.t) |
| TE-51 | A large update uses its four-participant ceiling. | [tests/encoder/indexed/0118_retained_budget_four.t](../../tests/encoder/indexed/0118_retained_budget_four.t) |
| TE-52 | The next update obeys a budget reduced from four to one. | [tests/encoder/indexed/0119_retained_budget_shrink.t](../../tests/encoder/indexed/0119_retained_budget_shrink.t) |
| TE-53 | The next update admits a budget increased from two to four. | [tests/encoder/indexed/0120_retained_budget_grow.t](../../tests/encoder/indexed/0120_retained_budget_grow.t) |
| TE-54 | Failure to allocate optional worker storage preserves successful serial commit. | [tests/encoder/indexed/0121_retained_admission_fallback.t](../../tests/encoder/indexed/0121_retained_admission_fallback.t) |
| TE-55 | An admitted component commit makes no malloc, calloc or realloc calls. | [tests/encoder/indexed/0122_retained_commit_no_allocation.t](../../tests/encoder/indexed/0122_retained_commit_no_allocation.t) |
| TE-56 | A negative update budget is rejected without creating an update. | [tests/encoder/indexed/0123_retained_negative_budget.t](../../tests/encoder/indexed/0123_retained_negative_budget.t) |
| TE-57 | Aborting an update with prepared workers preserves committed RGB. | [tests/encoder/indexed/0124_retained_abort_prepared.t](../../tests/encoder/indexed/0124_retained_abort_prepared.t) |
| TE-58 | A foreign receipt is rejected before prepared workers can change committed RGB. | [tests/encoder/indexed/0125_retained_wrong_receipt_parallel.t](../../tests/encoder/indexed/0125_retained_wrong_receipt_parallel.t) |
| TE-59 | Each participating background worker balances its CPU-work begin and end callbacks. | [tests/encoder/indexed/0126_retained_workers_balanced.t](../../tests/encoder/indexed/0126_retained_workers_balanced.t) |
| TE-60 | A thread-disabled build commits a large rectangle serially despite a larger ceiling. | [tests/encoder/indexed/0127_retained_threads_disabled.t](../../tests/encoder/indexed/0127_retained_threads_disabled.t) |
| TE-61 | Forty-one single-node bands allocate at most one node per participating worker with a two-worker ceiling. | [tests/encoder/pipeline/0057_parallel_node_reuse_bound.t](../../tests/encoder/pipeline/0057_parallel_node_reuse_bound.t) |
| TE-62 | Successful two-worker frame drain releases all caller-allocator storage. | [tests/encoder/pipeline/0058_parallel_node_drain_release.t](../../tests/encoder/pipeline/0058_parallel_node_drain_release.t) |
| TE-63 | A serial node-allocation failure returns `SIXEL_BAD_ALLOCATION`. | [tests/encoder/pipeline/0059_serial_node_failure_status.t](../../tests/encoder/pipeline/0059_serial_node_failure_status.t) |
| TE-64 | Serial node-allocation failure releases pending and reusable storage. | [tests/encoder/pipeline/0060_serial_node_failure_release.t](../../tests/encoder/pipeline/0060_serial_node_failure_release.t) |
| TE-65 | Parallel node-allocation failure releases pending and reusable storage. | [tests/encoder/pipeline/0061_parallel_node_failure_release.t](../../tests/encoder/pipeline/0061_parallel_node_failure_release.t) |
| TE-66 | Reusing a serial output after node-allocation failure produces the expected body. | [tests/encoder/pipeline/0062_serial_node_failure_retry.t](../../tests/encoder/pipeline/0062_serial_node_failure_retry.t) |
| TE-67 | A rejected typed sink releases all caller-allocator storage after parallel drain. | [tests/encoder/pipeline/0063_parallel_node_writer_release.t](../../tests/encoder/pipeline/0063_parallel_node_writer_release.t) |
| TE-68 | A serial failure while building a same-column chain releases its pending nodes. | [tests/encoder/pipeline/0079_bucket_chain_failure_release.t](../../tests/encoder/pipeline/0079_bucket_chain_failure_release.t) |
| TE-69 | A queued job cannot overwrite a later loader-owned loop number on a shared frame; writer gating determines the order without sleeps. The wrapper discards helper stdout so encoder cursor controls cannot corrupt TAP; the observation uses shared state and the helper's status, with stderr retained for diagnostics. | [tests/encoder/planning/0184_shared_timeline_ownership.t](../../tests/encoder/planning/0184_shared_timeline_ownership.t) |
| TE-70 | Streamed ordered indexes match literal palette choices. | [tests/encoder/indexed/0129_gpu_rows_ordered_indexes.t](../../tests/encoder/indexed/0129_gpu_rows_ordered_indexes.t) ([tests/encoder/indexed/0129_gpu_rows_ordered_indexes.c](../../tests/encoder/indexed/0129_gpu_rows_ordered_indexes.c)) |
| TE-71 | Streamed rbc indexes match literal palette choices. | [tests/encoder/indexed/0130_gpu_rows_rbc_indexes.t](../../tests/encoder/indexed/0130_gpu_rows_rbc_indexes.t) ([tests/encoder/indexed/0130_gpu_rows_rbc_indexes.c](../../tests/encoder/indexed/0130_gpu_rows_rbc_indexes.c)) |
| TE-72 | Streamed mahalanobis indexes match literal palette choices. | [tests/encoder/indexed/0131_gpu_rows_mahalanobis_indexes.t](../../tests/encoder/indexed/0131_gpu_rows_mahalanobis_indexes.t) ([tests/encoder/indexed/0131_gpu_rows_mahalanobis_indexes.c](../../tests/encoder/indexed/0131_gpu_rows_mahalanobis_indexes.c)) |
| TE-73 | Streamed certlut indexes match literal palette choices. | [tests/encoder/indexed/0132_gpu_rows_certlut_indexes.t](../../tests/encoder/indexed/0132_gpu_rows_certlut_indexes.t) ([tests/encoder/indexed/0132_gpu_rows_certlut_indexes.c](../../tests/encoder/indexed/0132_gpu_rows_certlut_indexes.c)) |
| TE-74 | Completed prefixes include the short final band. | [tests/encoder/indexed/0133_gpu_rows_prefix_order.t](../../tests/encoder/indexed/0133_gpu_rows_prefix_order.t) ([tests/encoder/indexed/0133_gpu_rows_prefix_order.c](../../tests/encoder/indexed/0133_gpu_rows_prefix_order.c)) |
| TE-75 | Later commands preserve every published prefix. | [tests/encoder/indexed/0134_gpu_rows_prefix_immutable.t](../../tests/encoder/indexed/0134_gpu_rows_prefix_immutable.t) ([tests/encoder/indexed/0134_gpu_rows_prefix_immutable.c](../../tests/encoder/indexed/0134_gpu_rows_prefix_immutable.c)) |
| TE-76 | Eight chunks reuse a window of at most three commands. | [tests/encoder/indexed/0135_gpu_rows_window_bound.t](../../tests/encoder/indexed/0135_gpu_rows_window_bound.t) ([tests/encoder/indexed/0135_gpu_rows_window_bound.c](../../tests/encoder/indexed/0135_gpu_rows_window_bound.c)) |
| TE-77 | A twelve-row group preserves the index oracle. | [tests/encoder/indexed/0136_gpu_rows_twelve.t](../../tests/encoder/indexed/0136_gpu_rows_twelve.t) ([tests/encoder/indexed/0136_gpu_rows_twelve.c](../../tests/encoder/indexed/0136_gpu_rows_twelve.c)) |
| TE-78 | Row grouping preserves global blue-noise coordinates. | [tests/encoder/indexed/0137_gpu_rows_bluenoise_equivalence.t](../../tests/encoder/indexed/0137_gpu_rows_bluenoise_equivalence.t) ([tests/encoder/indexed/0137_gpu_rows_bluenoise_equivalence.c](../../tests/encoder/indexed/0137_gpu_rows_bluenoise_equivalence.c)) |
| TE-79 | Row publication preserves exact delta keeps. | [tests/encoder/indexed/0138_gpu_rows_delta_mask.t](../../tests/encoder/indexed/0138_gpu_rows_delta_mask.t) ([tests/encoder/indexed/0138_gpu_rows_delta_mask.c](../../tests/encoder/indexed/0138_gpu_rows_delta_mask.c)) |
| TE-80 | Row publication retains input transparency. | [tests/encoder/indexed/0139_gpu_rows_transparency.t](../../tests/encoder/indexed/0139_gpu_rows_transparency.t) ([tests/encoder/indexed/0139_gpu_rows_transparency.c](../../tests/encoder/indexed/0139_gpu_rows_transparency.c)) |
| TE-81 | Cancellation before submission launches no GPU commands. | [tests/encoder/indexed/0140_gpu_rows_cancel_before.t](../../tests/encoder/indexed/0140_gpu_rows_cancel_before.t) ([tests/encoder/indexed/0140_gpu_rows_cancel_before.c](../../tests/encoder/indexed/0140_gpu_rows_cancel_before.c)) |
| TE-82 | Cancellation after one prefix drains every submitted command. | [tests/encoder/indexed/0141_gpu_rows_cancel_drain.t](../../tests/encoder/indexed/0141_gpu_rows_cancel_drain.t) ([tests/encoder/indexed/0141_gpu_rows_cancel_drain.c](../../tests/encoder/indexed/0141_gpu_rows_cancel_drain.c)) |
| TE-83 | A rejected prefix propagates the consumer error after drain. | [tests/encoder/indexed/0142_gpu_rows_callback_failure.t](../../tests/encoder/indexed/0142_gpu_rows_callback_failure.t) ([tests/encoder/indexed/0142_gpu_rows_callback_failure.c](../../tests/encoder/indexed/0142_gpu_rows_callback_failure.c)) |
| TE-84 | The second failed completion publishes no later prefix. | [tests/encoder/indexed/0143_gpu_rows_completion_failure.t](../../tests/encoder/indexed/0143_gpu_rows_completion_failure.t) ([tests/encoder/indexed/0143_gpu_rows_completion_failure.c](../../tests/encoder/indexed/0143_gpu_rows_completion_failure.c)) |
| TE-85 | Failure creating the second command drains the first. | [tests/encoder/indexed/0144_gpu_rows_submission_failure.t](../../tests/encoder/indexed/0144_gpu_rows_submission_failure.t) ([tests/encoder/indexed/0144_gpu_rows_submission_failure.c](../../tests/encoder/indexed/0144_gpu_rows_submission_failure.c)) |
| TE-86 | A failed request releases its lookup lease for the next run. | [tests/encoder/indexed/0145_gpu_rows_failed_reuse.t](../../tests/encoder/indexed/0145_gpu_rows_failed_reuse.t) ([tests/encoder/indexed/0145_gpu_rows_failed_reuse.c](../../tests/encoder/indexed/0145_gpu_rows_failed_reuse.c)) |
| TE-87 | An absent consumer retains one whole-image dispatch. | [tests/encoder/indexed/0146_gpu_rows_no_callback.t](../../tests/encoder/indexed/0146_gpu_rows_no_callback.t) ([tests/encoder/indexed/0146_gpu_rows_no_callback.c](../../tests/encoder/indexed/0146_gpu_rows_no_callback.c)) |
| TE-88 | Zero chunk height retains whole-image dispatch. | [tests/encoder/indexed/0147_gpu_rows_zero_chunk.t](../../tests/encoder/indexed/0147_gpu_rows_zero_chunk.t) ([tests/encoder/indexed/0147_gpu_rows_zero_chunk.c](../../tests/encoder/indexed/0147_gpu_rows_zero_chunk.c)) |
| TE-89 | An oversized chunk retains whole-image dispatch. | [tests/encoder/indexed/0148_gpu_rows_large_chunk.t](../../tests/encoder/indexed/0148_gpu_rows_large_chunk.t) ([tests/encoder/indexed/0148_gpu_rows_large_chunk.c](../../tests/encoder/indexed/0148_gpu_rows_large_chunk.c)) |
| TE-90 | The legacy adapter retains whole-image dispatch. | [tests/encoder/indexed/0149_gpu_rows_legacy_whole.t](../../tests/encoder/indexed/0149_gpu_rows_legacy_whole.t) ([tests/encoder/indexed/0149_gpu_rows_legacy_whole.c](../../tests/encoder/indexed/0149_gpu_rows_legacy_whole.c)) |
| TE-91 | Independent sessions retain their own output while sharing the backend. | [tests/encoder/indexed/0150_gpu_rows_concurrent.t](../../tests/encoder/indexed/0150_gpu_rows_concurrent.t) ([tests/encoder/indexed/0150_gpu_rows_concurrent.c](../../tests/encoder/indexed/0150_gpu_rows_concurrent.c)) |
| TE-92 | Only published rows are available to the consumer. | [tests/encoder/indexed/0151_gpu_publish_prefix.t](../../tests/encoder/indexed/0151_gpu_publish_prefix.t) ([tests/encoder/indexed/0151_gpu_publish_prefix.c](../../tests/encoder/indexed/0151_gpu_publish_prefix.c)) |
| TE-93 | A completed GPU prefix becomes readable. | [tests/encoder/indexed/0152_gpu_publish_final.t](../../tests/encoder/indexed/0152_gpu_publish_final.t) ([tests/encoder/indexed/0152_gpu_publish_final.c](../../tests/encoder/indexed/0152_gpu_publish_final.c)) |
| TE-94 | A GPU producer cannot retract a published prefix. | [tests/encoder/indexed/0153_gpu_publish_backwards.t](../../tests/encoder/indexed/0153_gpu_publish_backwards.t) ([tests/encoder/indexed/0153_gpu_publish_backwards.c](../../tests/encoder/indexed/0153_gpu_publish_backwards.c)) |
| TE-95 | A GPU prefix cannot exceed the image height. | [tests/encoder/indexed/0154_gpu_publish_bounds.t](../../tests/encoder/indexed/0154_gpu_publish_bounds.t) ([tests/encoder/indexed/0154_gpu_publish_bounds.c](../../tests/encoder/indexed/0154_gpu_publish_bounds.c)) |
| TE-96 | A failed image rejects subsequent GPU publication. | [tests/encoder/indexed/0155_gpu_publish_failed.t](../../tests/encoder/indexed/0155_gpu_publish_failed.t) ([tests/encoder/indexed/0155_gpu_publish_failed.c](../../tests/encoder/indexed/0155_gpu_publish_failed.c)) |
| TE-97 | GPU chunk geometry for small raster. | [tests/encoder/indexed/0156_gpu_row_geometry_small.t](../../tests/encoder/indexed/0156_gpu_row_geometry_small.t) ([tests/encoder/indexed/0156_gpu_row_geometry_small.c](../../tests/encoder/indexed/0156_gpu_row_geometry_small.c)) |
| TE-98 | GPU chunk geometry for short raster. | [tests/encoder/indexed/0157_gpu_row_geometry_short.t](../../tests/encoder/indexed/0157_gpu_row_geometry_short.t) ([tests/encoder/indexed/0157_gpu_row_geometry_short.c](../../tests/encoder/indexed/0157_gpu_row_geometry_short.c)) |
| TE-99 | GPU chunk geometry for zoom raster. | [tests/encoder/indexed/0158_gpu_row_geometry_zoom.t](../../tests/encoder/indexed/0158_gpu_row_geometry_zoom.t) ([tests/encoder/indexed/0158_gpu_row_geometry_zoom.c](../../tests/encoder/indexed/0158_gpu_row_geometry_zoom.c)) |
| TE-100 | GPU chunk geometry for odd raster. | [tests/encoder/indexed/0159_gpu_row_geometry_odd.t](../../tests/encoder/indexed/0159_gpu_row_geometry_odd.t) ([tests/encoder/indexed/0159_gpu_row_geometry_odd.c](../../tests/encoder/indexed/0159_gpu_row_geometry_odd.c)) |
| TE-101 | A later invalid index cannot invalidate a completed prefix. | [tests/encoder/indexed/0160_gpu_rows_invalid_later.t](../../tests/encoder/indexed/0160_gpu_rows_invalid_later.t) ([tests/encoder/indexed/0160_gpu_rows_invalid_later.c](../../tests/encoder/indexed/0160_gpu_rows_invalid_later.c)) |
| TE-102 | PaletteApply publishes readable rows before completion. | [tests/encoder/indexed/0161_gpu_rows_apply_visible.t](../../tests/encoder/indexed/0161_gpu_rows_apply_visible.t) ([tests/encoder/indexed/0161_gpu_rows_apply_visible.c](../../tests/encoder/indexed/0161_gpu_rows_apply_visible.c)) |
| TE-103 | Consumer rejection seals a partially published image FAILED. | [tests/encoder/indexed/0162_gpu_rows_apply_failed.t](../../tests/encoder/indexed/0162_gpu_rows_apply_failed.t) ([tests/encoder/indexed/0162_gpu_rows_apply_failed.c](../../tests/encoder/indexed/0162_gpu_rows_apply_failed.c)) |
| TE-104 | Streamed vptree indexes match literal palette choices. | [tests/encoder/indexed/0128_gpu_rows_vptree_indexes.t](../../tests/encoder/indexed/0128_gpu_rows_vptree_indexes.t) ([tests/encoder/indexed/0128_gpu_rows_vptree_indexes.c](../../tests/encoder/indexed/0128_gpu_rows_vptree_indexes.c)) |
| TE-105 | GPU output overlaps CPU preparation of the next frame. | [tests/encoder/planning/0185_gpu_ahead_overlap.t](../../tests/encoder/planning/0185_gpu_ahead_overlap.t) |
| TE-106 | GPU ahead and serial schedules emit identical bytes with T0 and a fixed refresh timeline. | [tests/encoder/planning/0186_gpu_ahead_t0_bytes.t](../../tests/encoder/planning/0186_gpu_ahead_t0_bytes.t) |
| TE-107 | GPU ahead and serial schedules emit identical bytes with T48 and a fixed refresh timeline. | [tests/encoder/planning/0187_gpu_ahead_t48_bytes.t](../../tests/encoder/planning/0187_gpu_ahead_t48_bytes.t) |
| TE-108 | A GPU auto-policy cutoff retains preparation overlap through the CPU fallback. | [tests/encoder/planning/0188_gpu_ahead_cpu_fallback.t](../../tests/encoder/planning/0188_gpu_ahead_cpu_fallback.t) |

| TE-109 | Changed indexes match a fresh core. | [tests/encoder/indexed/0163_reuse_pixels.t](../../tests/encoder/indexed/0163_reuse_pixels.t) |
| TE-110 | Changed palette definitions match a fresh core. | [tests/encoder/indexed/0164_reuse_palette.t](../../tests/encoder/indexed/0164_reuse_palette.t) |
| TE-111 | Changed keep masks match a fresh core. | [tests/encoder/indexed/0165_reuse_mask.t](../../tests/encoder/indexed/0165_reuse_mask.t) |
| TE-112 | A new sink receives output without changing the prior sink. | [tests/encoder/indexed/0166_reuse_sink.t](../../tests/encoder/indexed/0166_reuse_sink.t) |
| TE-113 | Changed encoded width matches a fresh core. | [tests/encoder/indexed/0167_reuse_width_change.t](../../tests/encoder/indexed/0167_reuse_width_change.t) |
| TE-114 | A changed height with a partial final band matches a fresh core. | [tests/encoder/indexed/0168_reuse_height_change.t](../../tests/encoder/indexed/0168_reuse_height_change.t) |
| TE-115 | Changed palette count matches a fresh core. | [tests/encoder/indexed/0169_reuse_colors_change.t](../../tests/encoder/indexed/0169_reuse_colors_change.t) |
| TE-116 | A parallel-to-serial transition matches a fresh core. | [tests/encoder/indexed/0170_reuse_threads_change.t](../../tests/encoder/indexed/0170_reuse_threads_change.t) |
| TE-117 | Changed queue depth matches a fresh core. | [tests/encoder/indexed/0171_reuse_queue_change.t](../../tests/encoder/indexed/0171_reuse_queue_change.t) |
| TE-118 | An OR-mode transition matches a fresh core. | [tests/encoder/indexed/0172_reuse_ormode_change.t](../../tests/encoder/indexed/0172_reuse_ormode_change.t) |
| TE-119 | Changed encode policy matches a fresh core. | [tests/encoder/indexed/0173_reuse_encode_policy.t](../../tests/encoder/indexed/0173_reuse_encode_policy.t) |
| TE-120 | Changed source offset matches a fresh core. | [tests/encoder/indexed/0174_reuse_offset_change.t](../../tests/encoder/indexed/0174_reuse_offset_change.t) |
| TE-121 | Encoding after abort matches a fresh core. | [tests/encoder/indexed/0175_reuse_abort_restart.t](../../tests/encoder/indexed/0175_reuse_abort_restart.t) |
| TE-122 | Encoding after palette-sink rejection matches a fresh core. | [tests/encoder/indexed/0176_reuse_palette_failure.t](../../tests/encoder/indexed/0176_reuse_palette_failure.t) |
| TE-123 | Encoding after body-sink rejection matches a fresh core. | [tests/encoder/indexed/0177_reuse_body_failure.t](../../tests/encoder/indexed/0177_reuse_body_failure.t) |
| TE-124 | Compatible frames select one cold and one reuse preparation. | [tests/encoder/indexed/0178_reuse_path.t](../../tests/encoder/indexed/0178_reuse_path.t) |
| TE-125 | Changed geometry selects cold preparation. | [tests/encoder/indexed/0179_reuse_geometry_rebuild.t](../../tests/encoder/indexed/0179_reuse_geometry_rebuild.t) |
| TE-126 | Final core release frees retained allocator-owned storage. | [tests/encoder/indexed/0180_reuse_release_storage.t](../../tests/encoder/indexed/0180_reuse_release_storage.t) |
| TE-127 | Explicit abort frees parked allocator-owned storage. | [tests/encoder/indexed/0181_reuse_abort_storage.t](../../tests/encoder/indexed/0181_reuse_abort_storage.t) |
| TE-128 | Encoding after injected allocation failure matches a fresh core. | [tests/encoder/indexed/0182_reuse_allocation_failure.t](../../tests/encoder/indexed/0182_reuse_allocation_failure.t) |
| TE-129 | Identical warm frames perform no allocator calls for worker scratch or nodes after both workspaces have executed. | [tests/encoder/indexed/0183_reuse_scratch_allocations.t](../../tests/encoder/indexed/0183_reuse_scratch_allocations.t) |
| TE-130 | Preparation helper tests and production share the compiler/thread capability; the source-only check rejects thread-only guards around helper waits or ahead comparisons. | [tests/_static/sh/staticcheck-prepare-ahead-capability.sh](../../tests/_static/sh/staticcheck-prepare-ahead-capability.sh) |
| TE-131 | Win32 thread creation preserves the creator's arithmetic environment, retaining exact extended float conversion results across worker budgets. | [tests/color/space/0037_float_memo_extended_exact.t](../../tests/color/space/0037_float_memo_extended_exact.t) |
| TE-132 | Concurrent palette-store jobs publish ordered selection events; ThreadSanitizer also checks their first-use K-means option resolution. | [tests/encoder/planning/0039_palette_store_concurrent.t](../../tests/encoder/planning/0039_palette_store_concurrent.t) |
| TE-133 | Quantizer getters access their shared first-value cache only through the synchronized helper. | [tests/_static/sh/staticcheck-palette-option-storage.sh](../../tests/_static/sh/staticcheck-palette-option-storage.sh) |
| TE-134 | A cached K-means initialization choice survives subsequent environment changes. | [tests/palette/initialization/0005_kmeans_init_cache.t](../../tests/palette/initialization/0005_kmeans_init_cache.t) |
| TE-135 | Cold admission uses one band. | [tests/encoder/scheduling/0001_cold_single.t](../../tests/encoder/scheduling/0001_cold_single.t) |
| TE-136 | One light frame does not expand admission. | [tests/encoder/scheduling/0002_light_once.t](../../tests/encoder/scheduling/0002_light_once.t) |
| TE-137 | Two light frames expand to two bands. | [tests/encoder/scheduling/0003_light_two.t](../../tests/encoder/scheduling/0003_light_two.t) |
| TE-138 | Four light frames expand to four bands. | [tests/encoder/scheduling/0004_light_four.t](../../tests/encoder/scheduling/0004_light_four.t) |
| TE-139 | An expensive frame shrinks four bands to one. | [tests/encoder/scheduling/0005_heavy_shrink.t](../../tests/encoder/scheduling/0005_heavy_shrink.t) |
| TE-140 | A moderate frame shrinks four bands to two. | [tests/encoder/scheduling/0006_moderate_shrink.t](../../tests/encoder/scheduling/0006_moderate_shrink.t) |
| TE-141 | Interrupted light observations do not expand. | [tests/encoder/scheduling/0007_interrupted_light.t](../../tests/encoder/scheduling/0007_interrupted_light.t) |
| TE-142 | A missing clock sample resets admission. | [tests/encoder/scheduling/0008_missing_clock.t](../../tests/encoder/scheduling/0008_missing_clock.t) |
| TE-143 | A backward clock sample resets admission. | [tests/encoder/scheduling/0009_backward_clock.t](../../tests/encoder/scheduling/0009_backward_clock.t) |
| TE-144 | A single published band is submitted immediately. | [tests/encoder/scheduling/0010_published_one.t](../../tests/encoder/scheduling/0010_published_one.t) |
| TE-145 | Three published bands admit only two together. | [tests/encoder/scheduling/0011_published_three.t](../../tests/encoder/scheduling/0011_published_three.t) |
| TE-146 | One-band queue capacity limits admission to one. | [tests/encoder/scheduling/0012_queue_one.t](../../tests/encoder/scheduling/0012_queue_one.t) |
| TE-147 | Two-band queue capacity limits admission to two. | [tests/encoder/scheduling/0013_queue_two.t](../../tests/encoder/scheduling/0013_queue_two.t) |
| TE-148 | Seven remaining bands retain work for two workers. | [tests/encoder/scheduling/0014_tail_two.t](../../tests/encoder/scheduling/0014_tail_two.t) |
| TE-149 | Three remaining bands use single-band jobs. | [tests/encoder/scheduling/0015_tail_one.t](../../tests/encoder/scheduling/0015_tail_one.t) |
| TE-150 | Four published bands can form a four-band job. | [tests/encoder/scheduling/0016_full_four.t](../../tests/encoder/scheduling/0016_full_four.t) |
| TE-151 | Two-band jobs preserve exact ordered bytes. | [tests/encoder/scheduling/0017_batch_two_bytes.t](../../tests/encoder/scheduling/0017_batch_two_bytes.t) |
| TE-152 | Four-band jobs preserve exact ordered bytes. | [tests/encoder/scheduling/0018_batch_four_bytes.t](../../tests/encoder/scheduling/0018_batch_four_bytes.t) |
| TE-153 | A batched stream submits no unpublished rows. | [tests/encoder/scheduling/0019_batch_partial_prefix.t](../../tests/encoder/scheduling/0019_batch_partial_prefix.t) |
| TE-154 | Batched virtual offsets preserve exact bytes. | [tests/encoder/scheduling/0020_batch_offset_bytes.t](../../tests/encoder/scheduling/0020_batch_offset_bytes.t) |
| TE-155 | Batched keep masks preserve exact bytes. | [tests/encoder/scheduling/0021_batch_keep_bytes.t](../../tests/encoder/scheduling/0021_batch_keep_bytes.t) |
| TE-156 | A one-band queue stays within its band limit. | [tests/encoder/scheduling/0022_batch_queue_one.t](../../tests/encoder/scheduling/0022_batch_queue_one.t) |
| TE-157 | A two-band queue stays within its band limit. | [tests/encoder/scheduling/0023_batch_queue_two.t](../../tests/encoder/scheduling/0023_batch_queue_two.t) |
| TE-158 | A batched abort drains before a fresh frame. | [tests/encoder/scheduling/0024_batch_abort_restart.t](../../tests/encoder/scheduling/0024_batch_abort_restart.t) |
| TE-159 | A rejected batched sink drains before a fresh frame. | [tests/encoder/scheduling/0025_batch_sink_restart.t](../../tests/encoder/scheduling/0025_batch_sink_restart.t) |
| TE-160 | A missing worker clock resets the next frame. | [tests/encoder/scheduling/0026_batch_clock_reset.t](../../tests/encoder/scheduling/0026_batch_clock_reset.t) |
| TE-161 | A partial final band preserves exact bytes. | [tests/encoder/scheduling/0027_batch_tail_bytes.t](../../tests/encoder/scheduling/0027_batch_tail_bytes.t) |
| TE-162 | GPU publication reports zero CPU commit participants and counts all 289 painted pixels across a full threadgroup and a short tail. | [tests/encoder/retained/0009_gpu_commit_count.t](../../tests/encoder/retained/0009_gpu_commit_count.t) |

Helper-lane tests use `SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED`, the same internal capability as production: threading is required, and PCC and TinyCC retain serial preparation. They skip before installing gates when that capability is absent; the explicit-serial test still runs. The GPU overlap and byte-comparison cases require an observed palette-apply kernel dispatch. Their small fixed-time streams compare scheduling consistency, not independent color accuracy.

The Metal cases skip without a usable device. Injected command failures exercise host drain and lease reuse; they do not simulate a device reset. The small range fixtures establish byte and publication behavior, not GPU scheduling speed.

The bit comparisons establish scheduling consistency, not independent color accuracy; the [color arithmetic tests](../../tests/color/space/) own independent numeric references and pool-fault coverage. Actual scaling depends on the image, concurrent work and memory bandwidth.

Cancellation and wait tests use synchronization gates: one writer is held while the queue and waiter state are observed. They require interrupted queued results, ordered single completion, source release and retained writer failure. The finite waits bound a hung test; they do not assert scheduling latency.

The reuse byte comparisons isolate state rebinding against a fresh core; they do not independently validate pixel accuracy. The warm allocation case gates two workers to initialize both workspaces, then counts calls through the core allocator. It does not assert zero native-thread or per-band `realloc` activity. Resident memory, thread stacks, performance and OS-specific scheduling require separate measurement.

The scheduling unit cases use explicit costs; integration cases inject a monotonic clock and assert the selected group size before comparing against a fresh single-band parallel core. They establish admission, prefix and recovery behavior, not speed or a perceptual quality change. The queue-size cases verify the maximum submitted group and completion with the selected capacity; native scheduling fairness remains platform-dependent.
