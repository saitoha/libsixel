# Lookup policy: `ordered`

This page describes `--lookup-policy=ordered`, abbreviated `--lookup-policy=o`, for CLI users and contributors. The [lookup policy reference](README.md) owns the interface, metric and guarantee terminology. `auto` continues to select `vptree`.

## Purpose and intuition

`ordered` starts from a nearby palette color selected by a small RGB seed grid, then scans that color's complete palette row in increasing color-to-color distance. The triangle inequality excludes candidates beyond an exact radius. The grid selects a starting color, not a quantized answer; final distances use the original post-dither byte query.

```text
palette --> distance-sorted rows + RGB seed grid
                        |
query --> seed color --> exact radius --> four candidates per comparison
                                             |
                               lowest-index nearest palette color
```

Unlike [`gridlut`](gridlut.md), the seed cells do not own candidate lists. Each palette color owns a sorted row. The method needs no Voronoi adjacency construction and no per-frame error buffer.

## Method

Let `q` be the query and `c` the seed. Any color `p` that improves or ties the seed satisfies `|p-c| <= |p-q| + |q-c| <= 2|q-c|`. Scan the row until its squared color-to-seed distance is strictly greater than `4|q-c|^2`. Equality remains eligible, including a tie at the radius boundary. Because the row contains every other palette color, one scan suffices.

Rows are sorted by `(squared distance, original palette index)`. Final candidates use the same packed ordering, so equal query distances choose the lowest original index even when row order differs from palette order. Duplicated coordinates retain their distinct indexes. Four-entry tails repeat the final valid color; they never introduce a new candidate or truncate a row. A single-color palette has an empty row.

The byte instance owns its palette and tables. Re-preparation reuses them only when palette size and every byte match. A changed palette replaces the tables even at unchanged size. Queries are read-only. A caller's previous-index hint may replace the seed when closer, but cannot remove a possible winner. Byte palettes above 256 colors are rejected.

## CPU and Metal construction

| Path | Preparation | Query data |
| --- | --- | --- |
| CPU byte | Stable radix sorting of exact integer distances; 8x8x8 seed grid | Packed distance and RGB rows; four-color NEON comparisons on AArch64, portable scalar comparisons elsewhere |
| Metal byte | Parallel bitonic row sorting directly emits packed rows; 32x32x32 seed grid | Four-color integer comparisons without palette-indirection loads |
| CPU float32 | Delegate to the existing exact blockcull backend | Native float coordinates and its normalized metric; no byte rounding or seed grid |

Seed centers are exact rational coordinates: scale them and palette bytes by sixteen on CPU or sixty-four on Metal before comparison. All squared distances fit in signed 32-bit arithmetic; packed query keys fit in 26 bits. The differing seed resolutions affect work, not the chosen index.

Metal constructors and palette application run in separate compute encoders within one command buffer. Tracked buffers establish visibility without an intermediate CPU wait. A lock protects the shared cache; its palette identity is independent of ordinary palette uploads and is published only after successful execution. Failed construction invalidates that identity. Unchanged palettes reuse the table; alternating palettes rebuild it.

Lookup follows the existing transparency, positional or blue-noise dither, and delta-keep decisions. A kept pixel performs no lookup. The retained-plane result update stays shared with other GPU policies. GPU admission otherwise follows the byte RGB palette-application rules, including CPU fallback for unsupported shapes under GPU `auto`. `SIXEL_TRACE_TOPIC=gpu_contract` reports `kernel=ordered` and `lookup=ordered|table=rebuild` or `lookup=ordered|table=reuse`.

## Guarantee

Byte lookup is scan-index exact for the same working palette and query as exhaustive `none`, including the lowest-index tie rule. Pruning uses integer bounds and no floating-point geometry. Float selection inherits [blockcull's exactness and nonfinite handling](blockcull.md#guarantee).

This does not promise identical complete encodes to `none`: its [Heckbert histogram resolution](README.md#precedence-and-interactions) differs from accelerated policies. Hold the palette fixed when checking lookup equality. Measure complete pipeline timing and output bytes when assessing playback.

## Cost

For `K` colors and `G` seeds, CPU construction costs `Theta(K^2 + G K)` with fixed-width radix passes. Metal bitonic sorting performs `O(K^2 log^2 K)` work plus `Theta(G K)` seed work, in parallel. Comparing an unchanged palette costs `Theta(K)`. Each query scans `V` radius-eligible candidates, with worst-case `Theta(K)` work. A coarse seed or a query far from the palette can admit most colors.

Both byte paths reserve two 256x256 arrays of 32-bit values. CPU adds 512 byte seeds and its owned palette; Metal adds 32768 32-bit seeds, for a 640 KiB table. The fixed row stride simplifies short rows and vector tails. Float delegation has blockcull's storage and preparation costs.

Table construction is paid on a palette change even when delta keeps most pixels. Dense seed grids are more expensive on CPU, and lookup-stage improvements alone do not establish a full-frame speedup. Report changed-palette construction separately from reuse, and include copying and synchronization in application timings.

## Implementation map

[`lookup-policy-ordered.c`](../../../../src/lookup-policy-ordered.c) owns CPU construction, exact byte lookup and float delegation. [`gpu-palette-metal.m`](../../../../src/gpu-palette-metal.m) owns Metal construction, cache identity and query integration. [`options-registry.c`](../../../../src/options-registry.c) registers the unique `o` prefix.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| OR-01 | Byte point, hinted and bulk queries match an independent exhaustive scan at seed-cell boundaries, random and palette-hit queries, duplicate colors, radius-boundary ties, reverse strides, tails and equal-size palette changes. | [tests/lookup/ordered/0008_ordered_byte_exact.t](../../../../tests/lookup/ordered/0008_ordered_byte_exact.t), [tests/_support/processing/lookup/0039_ordered_exact.c](../../../../tests/_support/processing/lookup/0039_ordered_exact.c) |
| OR-02 | Float point and bulk results match an independent double scan, including nonfinite queries, and finite VP-tree metric results. | [tests/lookup/ordered/0009_ordered_float_exact.t](../../../../tests/lookup/ordered/0009_ordered_float_exact.t) |
| OR-03 | Metal indexes and keep masks match CPU exhaustive lookup with none, bluenoise and positional dithering, repeated and changed palettes, duplicate colors and short rows; kernel selection and table reuse are observed. | [tests/lookup/ordered/0001_ordered_exact.t](../../../../tests/lookup/ordered/0001_ordered_exact.t), [tests/lookup/ordered/0001_ordered_exact.c](../../../../tests/lookup/ordered/0001_ordered_exact.c) |
| OR-04 | Policy alternation, palette resizing and rejected requests preserve exact results. | [tests/lookup/ordered/0002_ordered_cache_transitions.t](../../../../tests/lookup/ordered/0002_ordered_cache_transitions.t), [tests/lookup/ordered/0002_ordered_cache_transitions.c](../../../../tests/lookup/ordered/0002_ordered_cache_transitions.c) |
| OR-05 | Concurrent callers with independent palettes preserve exact results. | [tests/lookup/ordered/0003_ordered_parallel_cache.t](../../../../tests/lookup/ordered/0003_ordered_parallel_cache.t), [tests/lookup/ordered/0003_ordered_parallel_cache.c](../../../../tests/lookup/ordered/0003_ordered_parallel_cache.c) |
| OR-06 | Sequential delta frames preserve indexes, keep masks and retained RGB across threshold and palette changes. | [tests/lookup/ordered/0004_ordered_delta_history.t](../../../../tests/lookup/ordered/0004_ordered_delta_history.t), [tests/lookup/ordered/0004_ordered_delta_history.c](../../../../tests/lookup/ordered/0004_ordered_delta_history.c) |
| OR-07 | Valid, negative and out-of-range hints preserve point results. | [tests/lookup/dispatch/0011_lookup_hint_contract.c](../../../../tests/lookup/dispatch/0011_lookup_hint_contract.c) |
| OR-08 | Empty, cold and warm bulk calls preserve independent point results. | [tests/lookup/dispatch/0012_lookup_batch_contract.c](../../../../tests/lookup/dispatch/0012_lookup_batch_contract.c) |
| ORF-01 | Native float gamma range. | [tests/lookup/ordered/0010_ord_gamma_range.t](../../../../tests/lookup/ordered/0010_ord_gamma_range.t) |
| ORF-02 | Native float linear range. | [tests/lookup/ordered/0011_ord_linear_range.t](../../../../tests/lookup/ordered/0011_ord_linear_range.t) |
| ORF-03 | Native float oklab range. | [tests/lookup/ordered/0012_ord_oklab_range.t](../../../../tests/lookup/ordered/0012_ord_oklab_range.t) |
| ORF-04 | Native float cielab range. | [tests/lookup/ordered/0013_ord_cielab_range.t](../../../../tests/lookup/ordered/0013_ord_cielab_range.t) |
| ORF-05 | Native float din99d range. | [tests/lookup/ordered/0014_ord_din99d_range.t](../../../../tests/lookup/ordered/0014_ord_din99d_range.t) |
| ORF-06 | Native float gamma fraction. | [tests/lookup/ordered/0015_ord_gamma_fraction.t](../../../../tests/lookup/ordered/0015_ord_gamma_fraction.t) |
| ORF-07 | Native float linear fraction. | [tests/lookup/ordered/0016_ord_linear_fraction.t](../../../../tests/lookup/ordered/0016_ord_linear_fraction.t) |
| ORF-08 | Native float oklab fraction. | [tests/lookup/ordered/0017_ord_oklab_fraction.t](../../../../tests/lookup/ordered/0017_ord_oklab_fraction.t) |
| ORF-09 | Native float cielab fraction. | [tests/lookup/ordered/0018_ord_cielab_fraction.t](../../../../tests/lookup/ordered/0018_ord_cielab_fraction.t) |
| ORF-10 | Native float din99d fraction. | [tests/lookup/ordered/0019_ord_din99d_fraction.t](../../../../tests/lookup/ordered/0019_ord_din99d_fraction.t) |
| ORF-11 | Native float gamma oracle. | [tests/lookup/ordered/0020_ord_gamma_oracle.t](../../../../tests/lookup/ordered/0020_ord_gamma_oracle.t) |
| ORF-12 | Native float linear oracle. | [tests/lookup/ordered/0021_ord_linear_oracle.t](../../../../tests/lookup/ordered/0021_ord_linear_oracle.t) |
| ORF-13 | Native float oklab oracle. | [tests/lookup/ordered/0022_ord_oklab_oracle.t](../../../../tests/lookup/ordered/0022_ord_oklab_oracle.t) |
| ORF-14 | Native float cielab oracle. | [tests/lookup/ordered/0023_ord_cielab_oracle.t](../../../../tests/lookup/ordered/0023_ord_cielab_oracle.t) |
| ORF-15 | Native float din99d oracle. | [tests/lookup/ordered/0024_ord_din99d_oracle.t](../../../../tests/lookup/ordered/0024_ord_din99d_oracle.t) |
| ORF-16 | Native float gamma oracle large. | [tests/lookup/ordered/0025_ord_gamma_oracle_large.t](../../../../tests/lookup/ordered/0025_ord_gamma_oracle_large.t) |
| ORF-17 | Native float linear oracle large. | [tests/lookup/ordered/0026_ord_linear_oracle_large.t](../../../../tests/lookup/ordered/0026_ord_linear_oracle_large.t) |
| ORF-18 | Native float oklab oracle large. | [tests/lookup/ordered/0027_ord_oklab_oracle_large.t](../../../../tests/lookup/ordered/0027_ord_oklab_oracle_large.t) |
| ORF-19 | Native float cielab oracle large. | [tests/lookup/ordered/0028_ord_cielab_oracle_large.t](../../../../tests/lookup/ordered/0028_ord_cielab_oracle_large.t) |
| ORF-20 | Native float din99d oracle large. | [tests/lookup/ordered/0029_ord_din99d_oracle_large.t](../../../../tests/lookup/ordered/0029_ord_din99d_oracle_large.t) |
| ORF-21 | Native float gamma oracle small. | [tests/lookup/ordered/0030_ord_gamma_oracle_small.t](../../../../tests/lookup/ordered/0030_ord_gamma_oracle_small.t) |
| ORF-22 | Native float linear oracle small. | [tests/lookup/ordered/0031_ord_linear_oracle_small.t](../../../../tests/lookup/ordered/0031_ord_linear_oracle_small.t) |
| ORF-23 | Native float oklab oracle small. | [tests/lookup/ordered/0032_ord_oklab_oracle_small.t](../../../../tests/lookup/ordered/0032_ord_oklab_oracle_small.t) |
| ORF-24 | Native float cielab oracle small. | [tests/lookup/ordered/0033_ord_cielab_oracle_small.t](../../../../tests/lookup/ordered/0033_ord_cielab_oracle_small.t) |
| ORF-25 | Native float din99d oracle small. | [tests/lookup/ordered/0034_ord_din99d_oracle_small.t](../../../../tests/lookup/ordered/0034_ord_din99d_oracle_small.t) |
| ORF-26 | Native float gamma oracle mixed. | [tests/lookup/ordered/0035_ord_gamma_oracle_mixed.t](../../../../tests/lookup/ordered/0035_ord_gamma_oracle_mixed.t) |
| ORF-27 | Native float linear oracle mixed. | [tests/lookup/ordered/0036_ord_linear_oracle_mixed.t](../../../../tests/lookup/ordered/0036_ord_linear_oracle_mixed.t) |
| ORF-28 | Native float oklab oracle mixed. | [tests/lookup/ordered/0037_ord_oklab_oracle_mixed.t](../../../../tests/lookup/ordered/0037_ord_oklab_oracle_mixed.t) |
| ORF-29 | Native float cielab oracle mixed. | [tests/lookup/ordered/0038_ord_cielab_oracle_mixed.t](../../../../tests/lookup/ordered/0038_ord_cielab_oracle_mixed.t) |
| ORF-30 | Native float din99d oracle mixed. | [tests/lookup/ordered/0039_ord_din99d_oracle_mixed.t](../../../../tests/lookup/ordered/0039_ord_din99d_oracle_mixed.t) |
| ORF-31 | Native float cielab metric. | [tests/lookup/ordered/0040_ord_cielab_metric.t](../../../../tests/lookup/ordered/0040_ord_cielab_metric.t) |
| ORF-32 | Native float din99d metric. | [tests/lookup/ordered/0041_ord_din99d_metric.t](../../../../tests/lookup/ordered/0041_ord_din99d_metric.t) |
| ORF-33 | Native float instance isolation. | [tests/lookup/ordered/0042_ord_instance_isolation.t](../../../../tests/lookup/ordered/0042_ord_instance_isolation.t) |
| ORF-34 | ordered allocation failure falls back and rebuilds. | [tests/lookup/ordered/0005_ordered_allocation_failure.t](../../../../tests/lookup/ordered/0005_ordered_allocation_failure.t) |
| ORF-35 | ordered encoder failure falls back and rebuilds. | [tests/lookup/ordered/0006_ordered_encoder_failure.t](../../../../tests/lookup/ordered/0006_ordered_encoder_failure.t) |
| ORF-36 | ordered completion failure falls back and rebuilds. | [tests/lookup/ordered/0007_ordered_completion_failure.t](../../../../tests/lookup/ordered/0007_ordered_completion_failure.t) |

OR-03 through OR-06 skip without a Metal palette backend. OR-04 exercises request rejection before Metal dispatch; it does not inject Metal allocation or command-buffer failures. The shared [CLI prefix contract](../../../cli/prefix-matching.md) checks unique initials across registered policies. These tests establish numeric results and path selection, not a performance guarantee.

The failure tests inject allocation, encoder-creation and post-completion errors while rebuilding an ordered table. They verify actual dither AUTO fallback, immutable retained input, index and keep-mask equality, and recovery followed by table reuse. These are backend fault-boundary tests, not device-reset or driver-failure tests.
