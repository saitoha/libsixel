# Threading Measurement and Reproduction

## Scope

The checked-in threading artifacts answer architectural questions:

- how the encoder divides a configured worker budget;
- at which count dither and encode first overlap;
- which pools actually execute jobs;
- how Full HD encoder phases scale with the configured worker budget;
- how Full HD encoder phases vary with requested palette size at selected
  worker budgets;
- whether decoder scan joins before the paint barrier and paint spans overlap;
- how Full HD decoder phases scale with the configured worker budget; and
- whether animation frames are encoded concurrently.

Most timeline configurations are recorded once and are deliberately not
advertised as throughput results. The encoder and decoder worker sweeps are
repeated, stage-specific characterizations, but their diagnostic timestamps
still include instrumentation overhead. A portable performance benchmark
additionally needs an isolated host, controlled power state, CPU affinity where
available, several image classes and sizes, and uncertainty across independent
sessions.

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
4. resizes the controlled static source to a Full HD processed raster and
   sweeps encoder budgets from one through twelve;
5. records representative Full HD `img2sixel` timelines at two, four, and
   eight workers;
6. sweeps the Full HD encoder from one through sixteen workers, using two
   warm-ups and nine timed runs at each count;
7. sweeps twelve requested palette sizes at two, four, six, and eight encoder
   workers, again using two warm-ups and nine timed runs per condition;
8. records eight-worker decoder timelines at 900 by 675 and 1920 by 1080,
   using the Full HD result for the main figure;
9. sweeps the Full HD decoder from one through sixteen workers, using two
   warm-ups and nine timed runs at each count;
10. proves which build-tree libsixel both converter payloads load and rejects
   any input, tool, converter, or library hash change during measurement;
11. records a four-worker, finite two-frame animation timeline;
12. renders every timeline with `tools/timeline.py --sort-order start`; and
13. validates artifact completeness and current synchronization invariants.

An alternate output directory may be passed as the first argument. The second
and third arguments replace the static and animation fixtures respectively.
Changing a fixture or protocol produces a different experiment and must be
reported as such.

## Controlled encoder configuration

The encoder experiments use a controlled 900 by 675 smooth-gradient source.
`img2sixel -w 1920 -h 1080` resizes it to a Full HD processed raster before
palette construction and encoding. The source file is therefore not a native
Full HD fixture; the measured command and the raster processed by the encoder
are Full HD. The worker-budget sweep, allocation sweep, and representative
timelines fix the following non-threading choices:

```text
--precision=8bit
--quality=full
--quantize-model=kmeans:seed=1:sampling_policy=full-frame:binning_policy=hard
--merge-policy=ward
-Xoklab
-Wgamma
--diffusion=fs:scan=raster
--gpu-policy=off
--lookup-policy=6bit:shared_instance=1
-p 256
```

The requested 8-bit mode is verified as an effective `work=rgb888` path. The
Linear RGB and OKLab transformations used by resizing and palette construction
still use their required float32 intermediate coordinates; this does not turn
the shared gamma working raster into `rgb-f32`. Precision remains an
independent experiment across the whole encoder. The current scaling curves
describe only the true 8-bit profile above; the controlled single-thread
cross-section in [Encoder Working Precision](../functionality/precision.md)
shows the corresponding policy-wide float32 cost and output sensitivity.

At 256 colors the current automatic dither work-band overlap is zero. This is
useful for observing allocation but is not a neutral seam-quality experiment.
In particular, every point in the encoder worker-budget chart passes the
literal argument `-p 256`. The value is a requested palette size; the protocol
does not reinterpret it as a measured count of realized distinct entries.

The encoder scaling sweep records the complete `main/encoder` interval,
palette construction, and the dither/encode tail. Timed rounds alternate
between ascending and descending worker-count order. The plot reports medians
and inclusive interquartile ranges, and the CSV retains output SIXEL size and
hash for every execution. `--threads` is a worker budget rather than a
physical-core reservation because no affinity or exclusive-host control is
applied.

The encoder color-count sweep changes only `-p K` from that controlled encoder
configuration. It uses K = 2, 4, 8, 16, 24, 32, 48, 64, 96, 128, 192, and 256
at `--threads=2`, 4, 6, and 8. These values remain worker budgets rather than
physical-core reservations. Each of the 48 conditions receives two warm-ups
and nine timed runs. Whole rounds traverse the Cartesian condition list in
alternating directions. The chart shares both axes across its four panels and
uses a log2 color-count axis. It also marks the current automatic transition
from six overlap rows at K <= 32 to zero rows at K > 32. Because K and overlap
change together at that boundary, this experiment must not be cited as an
isolated overlap-cost measurement.

This controlled profile is intentionally distinct from the top-level automatic
palette path, which normally retains Heckbert compatibility behavior. Its
palette-build intervals include the cost of K-means, hard binning, Ward
merging, and Oklab clustering. The focused Heckbert comparison documented in
[Encoder threading](encoder.md#quantizer-profile-caveat) also changes binning
to a compatible policy; do not interpret it as a quantizer-only benchmark.

The decoder comparison encodes the same static source into dedicated 900 by
675 and 1920 by 1080 streams with one encoder worker and the controlled policy
above. Both are decoded with eight workers and GPU acceleration disabled. The
Full HD raw timeline supplies the main decoder figure. This keeps decoder
fixture construction independent of the encoder allocation sweep and avoids a
thread-dependent decoder input.

The decoder scaling sweep reuses the dedicated Full HD SIXEL stream, disables
GPU processing, and records decoder, validation-scan, direct-paint, PNG, and
complete process intervals. Timed rounds alternate between ascending and
descending worker-count order. The plot reports medians and inclusive
interquartile ranges; raw samples remain available for alternative summaries.
The worker budget is not a physical-core reservation because no affinity or
exclusive-host control is applied.

Decoder commands are run through `sixel2png`, so their timeline continues
after libsixel returns: the CLI serializes the decoded raster into PNG format.
This is why decoder CSV files include `png_wall_seconds`. Keeping that field
makes the measured command boundary explicit and shows the downstream cost,
but it is not part of `decoder_wall_seconds` and is not affected by the decoder
worker budget.

The animation case fixes the same policies but uses 16 colors and scales a
finite two-frame GIF to width 1200. Six overlap rows are selected automatically
at this palette size.

## Artifacts and validation

[`measurements/threading-run.json`](measurements/threading-run.json) records
the source revision, clean-state assertion, host, compiler, configure flags,
program hashes, input hashes, the dynamically loaded build-tree libsixel and
its hash, platform load probes for both `img2sixel` and `sixel2png`, and
templated commands. Collection aborts unless relevant inputs, tools, converter
payloads, and that library have the same hashes before and after the
measurement session.

[`measurements/thread-budget.csv`](measurements/thread-budget.csv) records the
planned split and observed worker participation for every configured budget.
The accompanying [budget chart](measurements/thread-budget.png) separates
planned stage allocation, observed job execution, and post-dither capacity.
Both were recorded through `img2sixel` with a Full HD processed raster.

Representative raw JSONL records and rendered charts are retained for:

- [encoder, two workers](measurements/encoder-thread2.jsonl);
- [encoder, four workers](measurements/encoder-thread4.jsonl);
- [encoder, eight workers](measurements/encoder-thread8.jsonl);
- [decoder, eight workers, 900 by 675](measurements/decoder-thread8-900x675.jsonl);
- [decoder, eight workers, 1920 by 1080](measurements/decoder-thread8.jsonl);
- [animation, four workers](measurements/animation-thread4.jsonl).

The encoder timeline loader spans cover the 900 by 675 source PNG. Pre-resize
linearization, linear-RGB resizing, conversion back to the `-Wgamma` working
space, and the later `-Xoklab` palette-input conversion appear as separate
filter and colorspace activity. “Full HD encoder timeline” refers to the
processed raster, not the loader input dimensions or the loader span alone.

[`measurements/encoder-thread-scaling.csv`](measurements/encoder-thread-scaling.csv)
retains two warm-ups and nine timed Full HD `img2sixel` samples for every
worker budget from one through sixteen. It records the exact execution order,
validated planner and worker observations, phase intervals, and output SIXEL
size and hash. The accompanying
[scaling chart](measurements/encoder-thread-scaling.png) plots complete
encoder, palette-build, and dither/encode-tail medians with IQR shading.

[`measurements/encoder-color-scaling.csv`](measurements/encoder-color-scaling.csv)
retains 96 warm-ups and 432 timed Full HD `img2sixel` observations across the
twelve requested palette sizes and four worker budgets. It records palette
size, exact execution order, phase intervals, automatic overlap, output size
and hash, and validated planner observations. The accompanying
[palette-size chart](measurements/encoder-color-scaling.png) uses one panel per
worker budget and plots complete encoder, palette-build, and dither/encode-tail
medians with IQR shading.

[`measurements/decoder-size-comparison.csv`](measurements/decoder-size-comparison.csv)
records raster dimensions, SIXEL payload size and hash, scan, parallel-paint,
decoder and PNG wall intervals, and scan/paint fractions. These values describe
one instrumented `sixel2png` run and are not throughput thresholds. PNG wall
time is the CLI's post-decode output serialization.

[`measurements/decoder-thread-scaling.csv`](measurements/decoder-thread-scaling.csv)
retains two warm-ups and nine timed Full HD samples for every worker budget
from one through sixteen, including exact execution order and observed path.
The accompanying [scaling chart](measurements/decoder-thread-scaling.png)
plots `sixel2png` decoder, validation-scan, and direct-paint medians with IQR
shading; it deliberately omits the separately recorded PNG serialization.

Run the artifact checker independently with:

```sh
PYTHON=.venv/bin/python \
  tools/check_threading_measurements.py docs/threading/measurements
```

The checker verifies the recorded revision relationship, complete budget
sweep, Full HD encoder command boundary, two-worker pipeline exception, first
useful overlap, dither worker participation, requested tail growth, paired
timeline spans, deterministic output within each encoder worker count, the
decoder all-ready paint barrier, overlapping direct paint intervals,
serialized animation frame encoding, every warm-up and timed schedule
position, exact per-run scan/ready/paint/abort counts, absence of fallback,
both converter load probes, command provenance, and PNG integrity. It does not
bless the measured elapsed seconds as performance thresholds.

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

- It is one measurement session on one arm64 macOS host.
- Diagnostic logging perturbs scheduling and short job duration. The scaling
  sweep repeats samples, but does not reserve the host or pin workers to cores.
- The encoder and decoder scaling charts characterize wall intervals, not CPU
  utilization or portable throughput. The decoder size comparison remains two
  diagnostic points.
- It does not exercise GPU palette application.
- It does not compare decoder direct and local-buffer paths.
- It does not contain band-seam quality metrics yet; the protocol above is the
  contract for that separate characterization.
- It demonstrates serialized frame encoding but does not model an eventual
  process-wide budget coordinator.
