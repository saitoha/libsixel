# Threading Measurement and Reproduction

## Scope

The checked-in threading artifacts answer architectural questions:

- how the encoder divides a configured worker budget;
- at which count dither and encode first overlap;
- which pools actually execute jobs;
- whether decoder scan and paint spans overlap; and
- whether animation frames are encoded concurrently.

They are deliberately not advertised as throughput or scalability results.
Each configuration is recorded once with diagnostic logging enabled, and the
timestamps include instrumentation overhead. A performance benchmark needs
warmups, repeated samples, uncertainty, controlled power state, several image
sizes, and stage-specific reporting.

## One-command reproduction

From a configured Autotools tree, install Python Matplotlib, commit the source
being measured, and run:

```sh
PYTHON=.venv/bin/python tools/reproduce_threading_measurements.sh
```

The Python path is only an example. The runner:

1. refuses to record a dirty tracked worktree;
2. rebuilds the configured tree with `$TOP_SRCDIR/.local/bin` first in `PATH`;
3. removes inherited `SIXEL_*` policy variables from measured commands;
4. sweeps encoder budgets from one through twelve;
5. records representative encoder timelines at two, four, and eight workers;
6. records an eight-worker decoder timeline;
7. records a four-worker, finite two-frame animation timeline;
8. renders every timeline with `tools/timeline.py --sort-order start`; and
9. validates artifact completeness and current ordering invariants.

An alternate output directory may be passed as the first argument. The second
and third arguments replace the static and animation fixtures respectively.
Changing a fixture or protocol produces a different experiment and must be
reported as such.

## Controlled encoder configuration

The allocation sweep uses a 900 by 675 smooth-gradient fixture and fixes the
following non-threading choices:

```text
--precision=8bit
--quality=full
--sampling-policy=full-frame
--binning-policy=hard
--quantize-model=kmeans:seed=1
--merge-policy=ward
-Xoklab
-Wgamma
--diffusion=fs:scan=raster
--gpu-policy=off
--lookup-policy=6bit:shared_instance=1
-p 256
```

At 256 colors the current automatic dither work-band overlap is zero. This is
useful for observing allocation but is not a neutral seam-quality experiment.
The static input is encoded once at one worker and that SIXEL stream is used as
the decoder input, avoiding a thread-dependent decoder fixture.

The animation case fixes the same policies but uses 16 colors and scales a
finite two-frame GIF to width 1200. Six overlap rows are selected automatically
at this palette size.

## Artifacts and validation

[`measurements/threading-run.json`](measurements/threading-run.json) records
the source revision, clean-state assertion, host, compiler, configure flags,
program hashes, input hashes, and templated commands.

[`measurements/thread-budget.csv`](measurements/thread-budget.csv) records the
planned split and observed worker participation for every configured budget.
The accompanying [budget chart](measurements/thread-budget.png) separates
planned stage allocation, observed job execution, and post-dither capacity.

Representative raw JSONL records and rendered charts are retained for:

- [encoder, two workers](measurements/encoder-thread2.jsonl);
- [encoder, four workers](measurements/encoder-thread4.jsonl);
- [encoder, eight workers](measurements/encoder-thread8.jsonl);
- [decoder, eight workers](measurements/decoder-thread8.jsonl); and
- [animation, four workers](measurements/animation-thread4.jsonl).

Run the artifact checker independently with:

```sh
PYTHON=.venv/bin/python \
  tools/check_threading_measurements.py docs/threading/measurements
```

The checker verifies the recorded revision relationship, complete budget
sweep, two-worker pipeline exception, first useful overlap, dither worker
participation, requested tail growth, paired timeline spans, serialized direct
decoder paint, serialized animation frame encoding, and PNG integrity. It does
not bless the measured elapsed seconds as performance thresholds.

## Band-seam quality protocol

Thread allocation timelines do not measure image quality. Because causal
error diffusion is initialized independently for each palette-application work
band, a separate serial-versus-banded protocol is required. Use the following
design when that durable measurement is added or rerun:

### Independent variables

- worker count: one worker as the full-frame reference and representative
  parallel budgets such as 4, 8, and 12;
- dither method: at least `fs`, one wider fixed stencil, one positional method,
  and `none`;
- scan order: raster and serpentine where supported;
- work-band height: automatic plus explicit small, medium, and full-height
  values, all resolved to multiples of six;
- overlap: zero, the kernel's short practical context, six rows, and a larger
  sensitivity point; and
- palette size: include both sides of the current 32-color automatic-overlap
  boundary.

### Controls

Generate the palette once or otherwise prove that every run receives identical
palette entries. Pin sampling, binning, quantizer seed, merge policy,
clustering space, palette-application space, lookup policy, GPU policy, crop,
resize, background, and transparency behavior. Do not let thread count change
sampling or palette construction while evaluating seams.

Decode every SIXEL result through the same decoder configuration before
comparison. Retain the encoded byte count from the same run; changed error
history can change palette-index sequences and therefore SIXEL size even when
the visual delta is negligible.

### Outputs

Report at least:

- MS-SSIM for spatially pooled impact;
- mean and a high percentile of Delta E00;
- luma and chroma error separately;
- encoded SIXEL bytes;
- a false-color per-pixel error map; and
- seam-local profiles in windows centered on every work-band boundary.

The seam-local view is essential. A faint horizontal discontinuity can occupy
too few pixels to move a global mean while remaining visually structured. A
useful plot places signed or absolute error against row offset from the nearest
boundary and aggregates corresponding offsets across all seams and fixtures.

### Fixtures

Use more than one image class:

- smooth vertical and diagonal gradients to reveal residual resets;
- flat fields near palette decision boundaries;
- natural images with texture and edges;
- alpha or background transitions when those paths are in scope; and
- heights that are not exact multiples of either work-band or SIXEL-band
  dimensions.

Routine regression tests should stay small, but characterization fixtures must
be large enough to contain several work-band boundaries.

### Interpretation

A non-zero serial-versus-banded delta is not automatically a quality failure.
Error diffusion is heuristic, and a changed local pattern can occasionally
improve a perceptual score or encoded size. Treat the serial run as a defined
execution reference, not a mathematical optimum. The acceptance decision must
consider seam visibility, aggregate quality, output size, and speed together.

Do not attribute a measured delta to the six-row SIXEL encoder jobs until the
indexed pixel planes have been compared. If the indexed planes match, visual
output must match and any byte difference belongs to encode ordering or
representation. If they differ near work-band boundaries, the palette-
application seam policy is the relevant variable.

## When to regenerate

Rerun the allocation and timeline command after changes to:

- the planner's worker split, band height, overlap, or queue depth;
- palette application, dither work-band scheduling, or row-ready notification;
- encode-pool growth, six-row jobs, or ordered writing;
- decoder span creation, scan, paint, local buffers, or fallback;
- loader/encoder handoff, frame queueing, or temporal policy;
- timeline event naming, pairing, grouping, or rendering; or
- the thread-pool service and its growth or affinity behavior.

Rerun the separate seam-quality protocol after changes to any dither kernel,
scan order, work-band boundary state, overlap default, lookup sharing, or GPU
palette-application path. A change to only the chart renderer does not require
re-encoding, but it does require rerendering and visual inspection.

## Known limits of the current dataset

- It is one run on one arm64 macOS host.
- Diagnostic logging perturbs scheduling and short job duration.
- It records allocation and ordering, not speedup or CPU utilization.
- It does not exercise GPU palette application.
- It does not compare decoder direct and local-buffer paths.
- It does not contain band-seam quality metrics yet; the protocol above is the
  contract for that separate characterization.
- It demonstrates serialized frame encoding but does not model an eventual
  process-wide budget coordinator.
