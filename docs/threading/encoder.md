# Encoder Threading

## Execution stages

The fixed-palette encoder has several opportunities for overlap. The complete
pipeline is broader than the final dither/encode window:

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

The planner can temporarily reserve one worker for asynchronous palette work
and assign the remainder to `main_threads`. That early split is distinct from
the later dither/encode split. Once palette work has joined, the final encoder
resolves the configured thread count again, so a timeline can contain more
stage transitions than one static allocation table suggests.

## Dither/encode budget policy

For a CPU palette-application path with `N > 1`, the planner starts with

```text
D0 = ceil(0.7 N)
E0 = N - D0
```

and clamps the active pipeline to retain at least two encode workers when
`N > 2`. The remaining workers are assigned to dither. Explicit dither thread
limits can override the automatic split. GPU palette application is different:
the GPU produces the complete index plane, so all CPU workers are placed on
the encode side after the command buffer becomes visible.

The measured automatic allocation for `N = 1..12` is:

| `--threads` | Planned dither | Planned encode | Runtime interpretation |
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

The two-worker exception is important. Although the planner reports `1 + 1`,
the runtime requires more than one encode worker before enabling row-by-row
dither/encode overlap. The first actual overlap is therefore at three workers,
not two. A true parallel dither pool starts at four workers; at three workers,
the single dither budget is consumed by the producer executing palette
application.

![Measured encoder thread budget](measurements/thread-budget.png)

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

## Timeline interpretation

At two configured workers, palette application completes before the band
workers begin useful encode work. The long `encode` interval is the frame-level
operation; the short worker intervals at its end are the six-row jobs.

![Two-worker encoder timeline](measurements/encoder-thread2-timeline.png)

At four workers, two dither work bands overlap two initial encode workers. The
solid dither spans are work-band lifetimes; the thin encode marks are many
short six-row jobs. The frame-level interval and writer appear in addition to
the configured worker budget.

![Four-worker encoder timeline](measurements/encoder-thread4-timeline.png)

The [eight-worker timeline](measurements/encoder-thread8-timeline.png) shows
the same shape at a wider allocation: six planned dither workers and two
planned encode workers, followed by a request to expose the complete encode
capacity after dithering ends.

These charts use `tools/timeline.py --sort-order start`. Row numbering is local
to the rendered worker group and must not be interpreted as a stable thread
identity across separate runs.

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
