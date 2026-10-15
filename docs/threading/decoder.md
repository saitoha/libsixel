# Decoder Threading

## Why decoding is split by bytes

A SIXEL stream states vertical movement through control sequences in the input. Before parsing those sequences, an arbitrary byte offset cannot be mapped directly to a destination-row strip. The decoder therefore begins serially, establishes raster bounds and initial palette state, and passes the remaining payload to the parallel decoder as byte spans.

The serial path uses [ordinary band columns](../decoder/pipeline/README.md#ordinary-mode-band-columns). The following span design applies when a parallel request succeeds.

The default span lengths are nearly equal. `SIXEL_PARALLEL_SKEW` can bias the distribution by up to 20 percent so later workers receive systematically more or less input. Thread count is capped by payload length; a payload cannot have more non-empty initial spans than bytes.

## Parallel direct path

The ordinary parallel path parses every owned band once into private column words:

```text
serial parser establishes raster and palette
                 |
       split payload into N byte spans
                 |
       align starts to six-row band boundaries
                 |
       parallel parse into local columns
                 |
             join all N
                 |
       prefix sum of disjoint byte-span band counts
       validate extents / trusted clipping
                 |
       create all N gather workers
       all-ready release barrier
                 |
       parallel columns-to-pixels gather + join
                 |
             publish output
```

Each worker searches backward from its useful boundary for the nearest `#`, then reads that register number forward with the parser's numeric bounds. Without a preceding selector it inherits the serial anchor's register. It does not parse the complete prefix. Within its useful span, it interprets data, repeats and controls once, counts delimiters in its original disjoint byte span, and records relative painted-row bounds and the largest register. Consecutive literal data bytes update column words in one loop. Each word contains six byte indexes and six independent coverage bits, using the [serial column representation](../decoder/pipeline/README.md#ordinary-mode-band-columns).

An arbitrary split advances to the next `-` boundary unless it already follows one. A span whose aligned start exceeds its original end is empty. The preceding nonempty span owns the intervening complete band, so converging boundaries never duplicate band ownership. The controller sums original-span counts after all parsers join, adds the delimiter skipped for alignment, assigns absolute rows and validates extents. A row-overlap rejection pass is unnecessary for well-formed ordinary input: accepted spans own distinct bands by construction. Data after an embedded ST retains the legacy thread-budget-dependent behavior. An ignored tail is counted for absolute offsets; if starts also converge, the controller reconstructs the formerly duplicated first band of empty spans and retains the legacy overlap fallback before publication. This exceptional compatibility check does not reparse normal bodies.

Column storage grows locally and never retains relative rows beyond the raster height. Even a tiny trusted raster with a very large encoded paint extent keeps bounded storage; the parser still tracks outside paint. Absolute clipping is applied after prefix resolution. Gather uses the shared serial materializer with a worker-local column source, preserves initialized unpainted pixels, fills paint coverage and resolves RGBA through the established palette. It does not interpret the input again.

Palette definitions within the body, registers above 255, raster resets and unsafe numeric controls select serial fallback. Parse or allocation failure discards private columns before any shared write. All gather workers wait at a release barrier: partial worker creation aborts and joins the already-created workers while the output remains untouched. After release, gather performs no allocation or fallible parsing.

## Why a PNG phase appears

The threading timelines in this directory invoke `sixel2png`. The command first uses libsixel to decode the SIXEL stream and then serializes the returned raster as a PNG file. Its timeline therefore contains a downstream `png/io` interval in addition to the decoder interval. PNG wall time is retained to make the CLI measurement boundary explicit and to show end-to-end context; it is not decoder work and is not parallelized by the decoder's `--threads` budget.

The checked-in eight-worker timeline uses a 1920 by 1080 raster. Eight parse intervals begin together. All eight gather workers then become ready at a barrier before their intervals begin as a parallel block. PNG output follows and dominates the remaining process wall time; it is not part of the decoder worker budget. Local parse is shown in light blue and gather in dark blue.

![Eight-worker Full HD decoder timeline](measurements/decoder-thread8-timeline.png)

*Figure: Full HD decoding measured through `sixel2png`; the final PNG interval is CLI output serialization after libsixel decoding.*

The [OR-mode scaling study](../encoder/output/or-mode/scaling.md) additionally measures in-process indexed and RGBA decoder calls. Those API measurements exclude PNG serialization entirely and are reported separately from complete `sixel2png` timings.

## Worker-budget scaling

The following Full HD experiment varies `--threads` from one through sixteen. The horizontal axis is the configured decoder worker budget, which is the closest user-facing control to an available-core budget. It is not a count of reserved physical cores: workers have no affinity, the host is not isolated, and this arm64 macOS host reports fourteen logical CPUs.

Each point is the median of nine instrumented runs after two warm-ups. The shaded region is the inclusive interquartile range. Successive rounds traverse worker counts in alternating directions to reduce monotonic time-order bias. GPU processing is disabled. PNG serialization is excluded from the plotted lines because `--threads` does not parallelize it.

![Full HD decoder time by worker budget](measurements/decoder-thread-scaling.png)

*Figure: Full HD decoder intervals measured inside `sixel2png`. PNG serialization is measured separately but intentionally omitted from these worker-scaling lines.*

The one-worker serial path took 2.803 ms at the median; two workers took 16.125 ms. The fastest observed median was 2.803 ms with one worker. The serial path accumulates ordinary column words, whereas parallel workers still perform validation and per-pixel paint. Their timing curves therefore describe different implementations, and more workers need not be faster.

These numbers characterize this fixture, binary and host. No CPU affinity, exclusive host, frequency control or confidence-interval claim is implied. The retained [`decoder-thread-scaling.csv`](measurements/decoder-thread-scaling.csv) contains all 144 timed observations and 32 warm-ups with their schedule positions. Decoder-specific source, build and verified library provenance are recorded under `decoder_refresh` in [`threading-run.json`](measurements/threading-run.json); encoder and animation records retain their separate provenance.

## Raster-size comparison

The same source and encoder policy produce two dedicated decoder fixtures. These are single diagnostic observations, not repeated performance benchmarks. `PNG wall` measures the command's downstream output serialization separately from decoding.

| Raster | SIXEL bytes | Scan wall | Parallel paint wall | Decoder wall | PNG wall |
| --- | ---: | ---: | ---: | ---: | ---: |
| 900 x 675 | 295,821 | 0.690 ms | 0.784 ms | 1.805 ms | 18.054 ms |
| 1920 x 1080 | 960,950 | 1.896 ms | 2.425 ms | 4.803 ms | 64.712 ms |

These two points include scheduling and cache noise and must not be read as a scaling curve. [`decoder-size-comparison.csv`](measurements/decoder-size-comparison.csv) and both raw timelines retain the phase bounds and scan/paint fractions. Calling this only “band decoding” hides two facts: initial work units are encoded byte spans, and direct paint becomes row-parallel only after validation proves independence.

## In-process API scaling

The retained `snake-fullhd-p256-t1.0.six` fixture decodes to 1920 x 1080 pixels. These measurements compare develop `0a77c616180a2a77bdf93dfd81e14204021ce1fc` with ordinary band columns at `da729980f93894beadde90d0b60de643e1df1c22`. Cells contain the median of 21 timed API calls after two warm-ups, with the inclusive interquartile interval in brackets. Input loading, output freeing, PNG serialization and timeline logging are excluded. Build order alternates by condition; no builds or tests run alongside the experiment, while ordinary desktop activity remains uncontrolled.

### `sixel_decode_direct`

| Workers | Before, ms [Q1, Q3] | Band columns, ms [Q1, Q3] | Before / after |
| ---: | ---: | ---: | ---: |
| 1 | 16.549 [16.445, 16.716] | 5.399 [5.378, 5.496] | 3.07x |
| 2 | 16.261 [16.227, 16.312] | 16.243 [16.234, 16.283] | 1.00x |
| 4 | 8.999 [8.979, 9.040] | 9.005 [8.969, 9.035] | 1.00x |
| 8 | 5.165 [5.139, 5.194] | 5.151 [5.124, 5.179] | 1.00x |
| 12 | 4.498 [4.241, 4.610] | 4.327 [4.229, 4.376] | 1.04x |

### `sixel_decode_raw`

| Workers | Before, ms [Q1, Q3] | Band columns, ms [Q1, Q3] | Before / after |
| ---: | ---: | ---: | ---: |
| 1 | 15.930 [15.900, 15.983] | 5.331 [5.277, 5.385] | 2.99x |
| 2 | 15.510 [15.497, 15.526] | 15.518 [15.504, 15.557] | 1.00x |
| 4 | 8.507 [8.485, 8.532] | 8.545 [8.533, 8.559] | 1.00x |
| 8 | 4.894 [4.875, 4.899] | 4.890 [4.875, 4.911] | 1.00x |
| 12 | 4.239 [4.180, 4.380] | 4.195 [4.118, 4.357] | 1.01x |

The parallel worker algorithm is unchanged. These results do not imply that its validation and paint passes use band columns. The serial path can now outperform several parallel budgets; the API preserves the configured thread policy.

For 100 retained photographs, per-image direct-RGBA speed ratios are:

| Workers | Median speed ratio | Range |
| ---: | ---: | ---: |
| 1 | 2.23x | 1.54–3.13x |
| 4 | 1.00x | 0.99–1.02x |
| 12 | 1.00x | 0.96–1.10x |

[`decoder-api-scaling.json`](measurements/decoder-api-scaling.json) retains every sample, input hash, revision, configure arguments and verified library path, including the OR and high-color controls. The instrumented scaling chart uses a different fixture and timing boundary, so its absolute values should not be combined with this table.

## Local-buffer and undither path

When decoder-side undithering is prepared, the validation-only pass resolves row offsets and caches the start of the existing top halo and its inherited register. Local workers decode that halo and their body into private row chunks, then filter and publish only body rows. The halo is filter input, so its byte and newline semantics remain separate from the prefix sum. Converging spans retain the existing local filtering windows independently of ordinary column ownership. A local worker whose scheduled end falls inside its halo publishes no body rows, so direct-scan row bounds cannot be used as its publication bounds. Local buffers reduce direct write conflicts and permit more work to remain inside the parallel worker lifetime. This path has different allocation and copying costs from direct paint and must be measured separately.

Do not infer behavior for decoder undithering from the direct-path chart. A timeline should identify scan, decode, paint, local-copy, and output stages explicitly whenever those paths are compared.

## Fallback and correctness

Parallel decoding begins only after the serial parser has established an anchor. A worker can still encounter syntax that needs global parser context, including raster resets or palette redefinitions. Such a stream may fall back to serial handling. This is an intended correctness boundary, not proof that the configured thread option was ignored.

OR-mode streams (`P2=5`) never enter the parallel path. Their serial decoder accumulates band-plane words and is faster than the scan-and-paint workers at every measured budget, so the parallel entry point declines them before any buffer is shared. The [OR-mode decoder study](../encoder/output/or-mode/scaling.md#how-the-band-plane-decoder-works) explains that representation and the measurements.

Parallel work must preserve:

- control-sequence and palette semantics at each span boundary;
- disjoint row ownership, or clean fallback before shared-buffer writes;
- raster clipping and paint-mask behavior;
- identical malformed-input and allocation-failure handling; and
- the public decoder's output ownership contract.

The ordinary timeline records paired starts and finishes for both parse and gather. `tests/cli/sixel2png/0011_parallel_timeline_spans_paired.t` keeps that diagnostic contract from silently degrading back to zero-length markers. `0012_parallel_gather_barrier.t` verifies that every gather worker reaches the barrier before publication begins. Decoder tests 0025 and 0026 cover partial gather creation and disjoint publication from converging byte spans, including the clean-fallback guarantee.

## Measurement limits

The checked-in timeline and two-size comparison are single diagnostic observations from one generated image class. The worker-budget figure is a repeated decoder scaling curve, but remains one session on one host. A broader performance study should vary payload size, SIXEL command mix, destination dimensions, palette changes, output format, and host. Measure parser/paint time separately from PNG serialization when the question is decoder parallelism.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| DTH-01 | Successful parallel requests preserve initial and inherited registers and literal row placement in 8-bit indexed, 16-bit indexed and RGBA buffers. | [tests/processing/decoder/0091_parallel_inherited_register.t](../../tests/processing/decoder/0091_parallel_inherited_register.t) |
| DTH-02 | Prefix-derived absolute rows clip to a trusted raster and report out-of-raster paint. | [tests/processing/decoder/0092_parallel_prefix_clipping.t](../../tests/processing/decoder/0092_parallel_prefix_clipping.t) |
| DTH-03 | A large encoded paint extent remains safe when clipped to a small trusted canvas, including validation diagnostics. | [tests/processing/decoder/0093_parallel_large_clipped_scan.t](../../tests/processing/decoder/0093_parallel_large_clipped_scan.t) |
| DTH-04 | Worker-local column allocation failure discards private paint and leaves shared pixels and coverage untouched. | [tests/processing/decoder/0101_parallel_columns_allocation_failure.t](../../tests/processing/decoder/0101_parallel_columns_allocation_failure.t) |
| DTH-05 | Partial parse-worker creation joins started workers and returns a clean serial fallback. | [tests/processing/decoder/0102_parallel_parse_create_failure.t](../../tests/processing/decoder/0102_parallel_parse_create_failure.t) |
| DTH-06 | Embedded ST followed by trailing data preserves legacy absolute offsets and overlap fallback at each tested worker budget. | [tests/processing/decoder/0103_parallel_early_st_compatibility.t](../../tests/processing/decoder/0103_parallel_early_st_compatibility.t) |

The [decoder suite](../../tests/processing/decoder) also covers sparse bands, converging spans, palette redefinitions and partial worker creation; the [CLI suite](../../tests/cli/sixel2png) checks paired timeline spans and the all-ready gather barrier. Timing observations depend on the host and are not correctness assertions.
