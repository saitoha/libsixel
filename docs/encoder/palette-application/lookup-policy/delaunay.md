# Lookup policy: `delaunay`

This page describes `--lookup-policy=delaunay` for CLI users and library contributors. The [lookup policy reference](README.md) owns the shared interface, metric and guarantee terminology. The [measurement companion](measurements/README.md) includes ten-policy quality and speed sweeps plus a separate byte and native-float latency comparison with `vptree`, `5bit`, and `6bit`. `auto` continues to select `vptree`.

## Purpose and intuition

A Delaunay graph connects palette colors whose closed Voronoi cells meet inside the query domain. Lookup walks from a caller hint toward the closest color. Closed adjacency includes every pair meeting at a degenerate vertex, rather than choosing one triangulation of a cospherical set.

```text
previous index ──> representative ──> neighbor block ──> smaller (distance, ID)
                                            ^                    |
8 × 8 × 8 seed grid ──> initial color ────────┘                    |
                                            └────────────────────┘
                                      no improvement: return original index
```

The seed grid supplies only a starting color. It does not quantize the query or cache a cell's answer.

## Method

Preparation merges identical coordinates in first-occurrence order, retaining the lowest original palette index. The shared builder clips a bounding box by relaxed bisector half-spaces, then retains pairs whose bisectors approach the remaining polytope from both sides. Candidate lists remain in distance order. Radix keys contain a float distance rounded toward zero and a representative ID; this rounding is monotone. Capacity exhaustion retains every candidate for the affected color. Very close sites do not clip each other's candidate cells: their query distances can round to a double tie even when mathematical distance improves, so their other neighbors must remain reachable across that plateau.

| Representation | Geometry and query domain | Final distance |
| --- | --- | --- |
| 8bit | Integer coordinates, box `[0,255]^3` | Exact integer squared RGB distance |
| float32 | Coordinates scaled by `sqrt(weight)`; palette bounds expanded by one normalized channel width on each side | Original widened float coordinates, channel-ordered double weighted squared distance |

The clipping slack is `extent * 4e-7`; the candidate margin is `extent * 4e-6`, where extent is the longest box edge. These enlarge adjacency rather than approximate the final query distance. Invalid geometry uses exhaustive lookup. A float query outside the box, or containing a nonfinite coordinate, uses a palette-order exhaustive scan with the same distance expression.

For current color `c` and squared query distance `D`, only neighbors within squared palette distance `4D` can win. Float lookup uses the conservative bound `4D * (1 + 1e-9)`. Four-neighbor blocks store separate coordinate, ID and list-distance arrays; the last neighbor repeats in short blocks. A block stops the scan when its first list distance exceeds the bound.

If `c` is not nearest, the segment from `c` toward the query leaves its cell through a shared bisector. The difference between that neighbor's squared distance and `c`'s squared distance is affine along the segment: zero at the crossing and negative beyond it. There is therefore an improving neighbor. At a minimum, every tied cell contains the query and closed adjacency retains the tied colors. Each move decreases the lexicographic pair `(distance, representative ID)`.

For at most 256 byte representatives, SIMD compares packed keys `(distance << 8) | ID`, which fit signed 32 bits. Larger byte palettes and float palettes keep separate distance and ID lanes. AArch64 NEON, SSE2 and WebAssembly SIMD128 evaluate neighbor blocks; scalar lookup uses the same ordering. SSE2 squares signed 16-bit differences with `madd`, requiring neither SSE4.1 nor AVX. These paths follow compile-time capability probes.

Float SIMD follows the [isolated noncontracting build gate](vptree.md#float-bulk-lookup). Every channel subtracts in double, squares, multiplies by the widened weight and accumulates in channel order. Split builds apply the gate to both float kernels; amalgamation retains native scalar arithmetic. WebAssembly SIMD additionally requires `-msimd128`; an explicit `-ffp-contract=off` establishes the float gate for that target.

The byte memo holds `2^18` relaxed atomic words with a complete RGB key and an 8-bit original index. The all-ones sentinel cannot cache white mapped to index 255. Float bulk calls optionally use a zeroed caller-owned table of 32,768 complete 96-bit keys. Positive and negative zero have distinct keys; nonfinite samples bypass the memo. Missing scratch changes only performance. Rows propagate the previous result as a hint, skip resolved adjacent identical keys and support negative byte strides without constructing a pointer before the buffer.

In the non-diffusion bulk path, eight independent cache misses share an exhaustive SIMD scan over first-occurrence representatives. Byte lookup batches palettes of at most 256 entries; eligible float SIMD batches finite queries for valid positive metrics and palettes of at most 64 entries. Larger float palettes keep graph walks after cache misses. Pending outputs never supply a repeat hint, and an incomplete batch uses ordinary point lookup.

## Guarantee

`delaunay` is scan-index exact in its representation's metric: equal computed distances select the smallest original palette index. Hints, SIMD selection, memo collisions and band sharing do not change that selection. Float weights and arithmetic follow the existing VP-tree float contract, including compiler arithmetic outside the SIMD gate. The geometric graph and seed data are immutable after preparation; only the complete-word byte memo is shared mutable state. Reuse migrates prepared ownership under the lookup interface's existing slot rules.

## Cost

For `U` distinct colors, graph construction sorts `U` lists with radix passes and clips candidate cells. Construction and temporary storage require at least quadratic pair work and space; clipping may dominate preparation. Degenerate or poorly conditioned palettes can produce a dense graph. The seed table adds `512U` distance evaluations. Persistent adjacency storage is proportional to the padded directed edge count, plus the palette maps and optional 1 MiB byte memo.

A query evaluates the neighbor blocks inside its current distance bound and may visit multiple colors. Sparse adjacency and coherent hints often reduce work, but no constant-time or logarithmic worst-case promise is made. Float queries outside the box scan all representatives. A bulk float workspace occupies 512 KiB per caller when the palette has at most 256 entries. A full cold batch scans `U` representatives for eight queries; warm keys avoid that scan. Its crossover with graph walking depends on palette size, spatial coherence and cache misses.

## Implementation map

The common builder is [`lookup-delaunay-graph.c`](../../../../src/lookup-delaunay-graph.c). [`lookup-delaunay-8bit.c`](../../../../src/lookup-delaunay-8bit.c) and [`lookup-delaunay-float32.c`](../../../../src/lookup-delaunay-float32.c) own query arithmetic, SIMD and memoization. [`lookup-policy-delaunay.c`](../../../../src/lookup-policy-delaunay.c) owns preparation, representation conversion and reuse through `lookup/delaunay.8bit` and `lookup/delaunay.float32`.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| DL-01 | Deterministic RGB samples match the integer oracle for a 1-color random palette, without a hint. | [tests/processing/lookup/0021_delaunay_byte_map_random_1.t](../../../../tests/processing/lookup/0021_delaunay_byte_map_random_1.t) |
| DL-02 | Deterministic RGB samples match the integer oracle for a 2-color random palette, without a hint. | [tests/processing/lookup/0022_delaunay_byte_map_random_2.t](../../../../tests/processing/lookup/0022_delaunay_byte_map_random_2.t) |
| DL-03 | Deterministic RGB samples match the integer oracle for a 3-color random palette, without a hint. | [tests/processing/lookup/0023_delaunay_byte_map_random_3.t](../../../../tests/processing/lookup/0023_delaunay_byte_map_random_3.t) |
| DL-04 | Deterministic RGB samples match the integer oracle for a 7-color random palette, without a hint. | [tests/processing/lookup/0024_delaunay_byte_map_random_7.t](../../../../tests/processing/lookup/0024_delaunay_byte_map_random_7.t) |
| DL-05 | Deterministic RGB samples match the integer oracle for a 40-color random palette, without a hint. | [tests/processing/lookup/0025_delaunay_byte_map_random_40.t](../../../../tests/processing/lookup/0025_delaunay_byte_map_random_40.t) |
| DL-06 | Deterministic RGB samples match the integer oracle for a 64-color random palette, without a hint. | [tests/processing/lookup/0026_delaunay_byte_map_random_64.t](../../../../tests/processing/lookup/0026_delaunay_byte_map_random_64.t) |
| DL-07 | Deterministic RGB samples match the integer oracle for a 256-color random palette, without a hint. | [tests/processing/lookup/0027_delaunay_byte_map_random_256.t](../../../../tests/processing/lookup/0027_delaunay_byte_map_random_256.t) |
| DL-08 | Deterministic RGB samples retain the lowest original index when duplicate colors collapse to representatives. | [tests/processing/lookup/0028_delaunay_byte_map_duplicates.t](../../../../tests/processing/lookup/0028_delaunay_byte_map_duplicates.t) |
| DL-09 | Deterministic RGB samples match the oracle for the cospherical grid over `{0,64,128,192}`. | [tests/processing/lookup/0029_delaunay_byte_map_grid.t](../../../../tests/processing/lookup/0029_delaunay_byte_map_grid.t) |
| DL-10 | Sampled native float queries and a near-coincident three-site regression match the double oracle regardless of hint. | [tests/processing/lookup/0030_delaunay_float_exact.t](../../../../tests/processing/lookup/0030_delaunay_float_exact.t) |
| DL-11 | Scalar and active SIMD distance expressions produce identical double bits. | [tests/processing/lookup/0031_delaunay_float_distance.t](../../../../tests/processing/lookup/0031_delaunay_float_distance.t) |
| DL-12 | Bulk results preserve scalar selection across memo collisions, reverse strides and short tails. | [tests/processing/lookup/0032_delaunay_float_bulk.t](../../../../tests/processing/lookup/0032_delaunay_float_bulk.t) |
| DL-13 | A 300-color palette uses separate distance and ID comparisons without truncating indices. | [tests/processing/lookup/0033_delaunay_byte_map_large.t](../../../../tests/processing/lookup/0033_delaunay_byte_map_large.t) |
| DL-20 | Each helper invocation compares one selected byte lookup path with a palette-order integer oracle. | [tests/processing/lookup/0021_delaunay_byte_exact.c](../../../../tests/processing/lookup/0021_delaunay_byte_exact.c) |
| DL-21 | Native float indices match an independent double oracle for duplicates, almost-coincident sites, grids, extended, extreme and nonfinite queries. | [tests/processing/lookup/0030_delaunay_float_exact.c](../../../../tests/processing/lookup/0030_delaunay_float_exact.c) |
| DL-22 | Neighbor and cold-query double SIMD lanes match scalar distance bits; builds without the gate check the scalar formula. | [tests/processing/lookup/0031_delaunay_float_distance.c](../../../../tests/processing/lookup/0031_delaunay_float_distance.c) |
| DL-23 | Float cold batches and memoization preserve complete keys, collisions, signed zero, duplicate ties, reverse rows, all short tails, absent scratch and palettes across the 64- and 256-entry boundaries. | [tests/processing/lookup/0032_delaunay_float_bulk.c](../../../../tests/processing/lookup/0032_delaunay_float_bulk.c) |
| DL-24 | Both representations preserve ordered scalar semantics in forward and reverse bulk rows. | [tests/processing/lookup/0015_lookup_batch_contract.c](../../../../tests/processing/lookup/0015_lookup_batch_contract.c) |
| DL-25 | Gamma sRGB palettes select the float consumer and preserve native palette coordinates. | [tests/quant/palette/lookup_float/0070_delaunay_gamma_srgb.t](../../../../tests/quant/palette/lookup_float/0070_delaunay_gamma_srgb.t) |
| DL-26 | Gamma Display-P3 palettes select the float consumer and preserve native palette coordinates. | [tests/quant/palette/lookup_float/0071_delaunay_gamma_p3.t](../../../../tests/quant/palette/lookup_float/0071_delaunay_gamma_p3.t) |
| DL-27 | Byte-only palettes select the byte consumer. | [tests/quant/palette/lookup_float/0072_delaunay_byte_eligibility.t](../../../../tests/quant/palette/lookup_float/0072_delaunay_byte_eligibility.t) |
| DL-28 | Private worker bands preserve the independently encoded band output. | [tests/quant/palette/lookup_float/0073_delaunay_bands_private.t](../../../../tests/quant/palette/lookup_float/0073_delaunay_bands_private.t) |
| DL-29 | Requesting shared bands preserves the VP-tree-compatible private-policy behavior and band output. | [tests/quant/palette/lookup_float/0074_delaunay_bands_shared.t](../../../../tests/quant/palette/lookup_float/0074_delaunay_bands_shared.t) |
| DL-30 | Byte cold batches and warm rows match an independent integer oracle across duplicate ties, full-key collisions, the white/index-255 sentinel, reverse padded strides and short tails. | [tests/processing/lookup/0034_delaunay_byte_bulk.c](../../../../tests/processing/lookup/0034_delaunay_byte_bulk.c), [tests/processing/lookup/0034_delaunay_byte_bulk.t](../../../../tests/processing/lookup/0034_delaunay_byte_bulk.t) |

| DL-31 | Scalar walks from varying palette centers match an integer oracle for a 1-color random palette. | [tests/processing/lookup/0041_delaunay_byte_scalar_random_1.t](../../../../tests/processing/lookup/0041_delaunay_byte_scalar_random_1.t) |
| DL-32 | Scalar walks from varying palette centers match an integer oracle for a 2-color random palette. | [tests/processing/lookup/0042_delaunay_byte_scalar_random_2.t](../../../../tests/processing/lookup/0042_delaunay_byte_scalar_random_2.t) |
| DL-33 | Scalar walks from varying palette centers match an integer oracle for a 3-color random palette. | [tests/processing/lookup/0043_delaunay_byte_scalar_random_3.t](../../../../tests/processing/lookup/0043_delaunay_byte_scalar_random_3.t) |
| DL-34 | Scalar walks from varying palette centers match an integer oracle for a 7-color random palette. | [tests/processing/lookup/0044_delaunay_byte_scalar_random_7.t](../../../../tests/processing/lookup/0044_delaunay_byte_scalar_random_7.t) |
| DL-35 | Scalar walks from varying palette centers match an integer oracle for a 40-color random palette. | [tests/processing/lookup/0045_delaunay_byte_scalar_random_40.t](../../../../tests/processing/lookup/0045_delaunay_byte_scalar_random_40.t) |
| DL-36 | Scalar walks from varying palette centers match an integer oracle for a 64-color random palette. | [tests/processing/lookup/0046_delaunay_byte_scalar_random_64.t](../../../../tests/processing/lookup/0046_delaunay_byte_scalar_random_64.t) |
| DL-37 | Scalar walks from varying palette centers match an integer oracle for a 256-color random palette. | [tests/processing/lookup/0047_delaunay_byte_scalar_random_256.t](../../../../tests/processing/lookup/0047_delaunay_byte_scalar_random_256.t) |
| DL-38 | Scalar walks from varying palette centers match an integer oracle for a palette with duplicate colors. | [tests/processing/lookup/0048_delaunay_byte_scalar_duplicates.t](../../../../tests/processing/lookup/0048_delaunay_byte_scalar_duplicates.t) |
| DL-39 | Scalar walks from varying palette centers match an integer oracle for the grid palette. | [tests/processing/lookup/0049_delaunay_byte_scalar_grid.t](../../../../tests/processing/lookup/0049_delaunay_byte_scalar_grid.t) |
| DL-40 | Scalar walks from varying palette centers match an integer oracle for a 300-color palette. | [tests/processing/lookup/0050_delaunay_byte_scalar_large.t](../../../../tests/processing/lookup/0050_delaunay_byte_scalar_large.t) |
| DL-41 | Active SIMD walks from varying palette centers match an integer oracle for a 1-color random palette. | [tests/processing/lookup/0051_delaunay_byte_simd_random_1.t](../../../../tests/processing/lookup/0051_delaunay_byte_simd_random_1.t) |
| DL-42 | Active SIMD walks from varying palette centers match an integer oracle for a 2-color random palette. | [tests/processing/lookup/0052_delaunay_byte_simd_random_2.t](../../../../tests/processing/lookup/0052_delaunay_byte_simd_random_2.t) |
| DL-43 | Active SIMD walks from varying palette centers match an integer oracle for a 3-color random palette. | [tests/processing/lookup/0053_delaunay_byte_simd_random_3.t](../../../../tests/processing/lookup/0053_delaunay_byte_simd_random_3.t) |
| DL-44 | Active SIMD walks from varying palette centers match an integer oracle for a 7-color random palette. | [tests/processing/lookup/0054_delaunay_byte_simd_random_7.t](../../../../tests/processing/lookup/0054_delaunay_byte_simd_random_7.t) |
| DL-45 | Active SIMD walks from varying palette centers match an integer oracle for a 40-color random palette. | [tests/processing/lookup/0055_delaunay_byte_simd_random_40.t](../../../../tests/processing/lookup/0055_delaunay_byte_simd_random_40.t) |
| DL-46 | Active SIMD walks from varying palette centers match an integer oracle for a 64-color random palette. | [tests/processing/lookup/0056_delaunay_byte_simd_random_64.t](../../../../tests/processing/lookup/0056_delaunay_byte_simd_random_64.t) |
| DL-47 | Active SIMD walks from varying palette centers match an integer oracle for a 256-color random palette. | [tests/processing/lookup/0057_delaunay_byte_simd_random_256.t](../../../../tests/processing/lookup/0057_delaunay_byte_simd_random_256.t) |
| DL-48 | Active SIMD walks from varying palette centers match an integer oracle for a palette with duplicate colors. | [tests/processing/lookup/0058_delaunay_byte_simd_duplicates.t](../../../../tests/processing/lookup/0058_delaunay_byte_simd_duplicates.t) |
| DL-49 | Active SIMD walks from varying palette centers match an integer oracle for the grid palette. | [tests/processing/lookup/0059_delaunay_byte_simd_grid.t](../../../../tests/processing/lookup/0059_delaunay_byte_simd_grid.t) |
| DL-50 | Active SIMD walks from varying palette centers match an integer oracle for a 300-color palette. | [tests/processing/lookup/0060_delaunay_byte_simd_large.t](../../../../tests/processing/lookup/0060_delaunay_byte_simd_large.t) |
| DL-51 | Lookup with an out-of-range hint matches an integer oracle for a 1-color random palette. | [tests/processing/lookup/0061_delaunay_byte_stale_random_1.t](../../../../tests/processing/lookup/0061_delaunay_byte_stale_random_1.t) |
| DL-52 | Lookup with an out-of-range hint matches an integer oracle for a 2-color random palette. | [tests/processing/lookup/0062_delaunay_byte_stale_random_2.t](../../../../tests/processing/lookup/0062_delaunay_byte_stale_random_2.t) |
| DL-53 | Lookup with an out-of-range hint matches an integer oracle for a 3-color random palette. | [tests/processing/lookup/0063_delaunay_byte_stale_random_3.t](../../../../tests/processing/lookup/0063_delaunay_byte_stale_random_3.t) |
| DL-54 | Lookup with an out-of-range hint matches an integer oracle for a 7-color random palette. | [tests/processing/lookup/0064_delaunay_byte_stale_random_7.t](../../../../tests/processing/lookup/0064_delaunay_byte_stale_random_7.t) |
| DL-55 | Lookup with an out-of-range hint matches an integer oracle for a 40-color random palette. | [tests/processing/lookup/0065_delaunay_byte_stale_random_40.t](../../../../tests/processing/lookup/0065_delaunay_byte_stale_random_40.t) |
| DL-56 | Lookup with an out-of-range hint matches an integer oracle for a 64-color random palette. | [tests/processing/lookup/0066_delaunay_byte_stale_random_64.t](../../../../tests/processing/lookup/0066_delaunay_byte_stale_random_64.t) |
| DL-57 | Lookup with an out-of-range hint matches an integer oracle for a 256-color random palette. | [tests/processing/lookup/0067_delaunay_byte_stale_random_256.t](../../../../tests/processing/lookup/0067_delaunay_byte_stale_random_256.t) |
| DL-58 | Lookup with an out-of-range hint matches an integer oracle for a palette with duplicate colors. | [tests/processing/lookup/0068_delaunay_byte_stale_duplicates.t](../../../../tests/processing/lookup/0068_delaunay_byte_stale_duplicates.t) |
| DL-59 | Lookup with an out-of-range hint matches an integer oracle for the grid palette. | [tests/processing/lookup/0069_delaunay_byte_stale_grid.t](../../../../tests/processing/lookup/0069_delaunay_byte_stale_grid.t) |
| DL-60 | Lookup with an out-of-range hint matches an integer oracle for a 300-color palette. | [tests/processing/lookup/0070_delaunay_byte_stale_large.t](../../../../tests/processing/lookup/0070_delaunay_byte_stale_large.t) |

### Coverage boundary

The byte fixtures use deterministic samples for random palettes of 1, 2, 3, 7, 40, 64, 256 and 300 colors, a duplicate palette and a grid over `{0,64,128,192}`. Each checks a 17-by-17-by-17 RGB lattice including channel endpoints, 4096 fixed-seed pseudorandom keys, 27 neighboring keys per palette entry, and seven neighboring keys around three pair midpoints per entry. These 9009 + 48N probes include palette hits, the white/index-255 sentinel and exact ties on the grid. Separate TAP files compare one selected lookup path with a palette-order integer oracle: ordinary lookup, scalar walk, active SIMD walk or lookup with an out-of-range hint. SIMD-only tests skip when that consumer is not compiled. Each path creates its own graph and memo. This samples the RGB domain rather than establishing correctness for every key. An explicit numeric third helper argument retains prefix enumeration for investigations: `tests/test_runner lookup/0021_delaunay_byte_exact 256 0 16777216 map` checks the entire RGB8 domain for that one palette.

Float tests sample native coordinates and arithmetic boundaries; they do not enumerate the float domain. SIMD evidence depends on the compiler target and arithmetic gate actually reported by the test; a scalar-only build does not validate another architecture's instructions. Performance measurements are separate from these functional contracts.
