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
| OR-01 | Byte point, hinted and bulk queries match an independent exhaustive scan at seed-cell boundaries, random and palette-hit queries, duplicate colors, radius-boundary ties, reverse strides, tails and equal-size palette changes. | [tests/processing/lookup/0039_ordered_byte_exact.t](../../../../tests/processing/lookup/0039_ordered_byte_exact.t), [tests/processing/lookup/0039_ordered_exact.c](../../../../tests/processing/lookup/0039_ordered_exact.c) |
| OR-02 | Float delegation preserves scalar results, nonfinite handling and finite VP-tree metric results. | [tests/processing/lookup/0040_ordered_float_exact.t](../../../../tests/processing/lookup/0040_ordered_float_exact.t) |
| OR-03 | Metal indexes and keep masks match CPU exhaustive lookup with none, bluenoise and positional dithering, repeated and changed palettes, duplicate colors and short rows; kernel selection and table reuse are observed. | [tests/processing/gpu-palette/0023_ordered_exact.t](../../../../tests/processing/gpu-palette/0023_ordered_exact.t), [tests/processing/gpu-palette/0023_ordered_exact.c](../../../../tests/processing/gpu-palette/0023_ordered_exact.c) |

OR-03 skips without a Metal palette backend. The shared [CLI prefix contract](../../../cli/prefix-matching.md) checks unique initials across registered policies. These tests establish numeric results and path selection, not a performance guarantee.
