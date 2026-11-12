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

## GPU byte path

For byte RGB palettes of at most 256 entries, Metal and Vulkan build a conservative superset of closed Voronoi adjacency. Neither backend transfers the CPU graph or tetrahedralizes the palette. The existing byte palette-application admission rules select this path for `none`, `a_dither`, `x_dither` and `bluenoise` dithering; unsupported inputs retain CPU fallback under GPU `auto`. Native float lookup keeps its CPU implementation.

| Stage | GPU operation | Exactness boundary |
| --- | --- | --- |
| Cell bounds | One parallel interval contraction of `[0,255]^3` per palette color | Signed integer division rounds outward, retaining every point of the continuous closed cell |
| Adjacency | Intersect each pair of boxes and retain pairs whose bisector crosses the intersection; bitonic sorting compacts and packs the rows | Necessary neighbors survive; extra edges only increase query work |
| Seeds | Find the nearest color to each exact rational center of a 16x16x16 grid | The seed chooses a starting color, not a quantized answer |
| Lookup | Walk four neighbors at a time, ordered by squared color distance, with exact radius pruning and strict packed-key descent | Original post-dither byte queries and lowest-original-index ties match exhaustive lookup |

All bounds, bisector tests, distances and tie keys use integer arithmetic. Duplicate coordinates retain separate indexes. Closed-cell adjacency includes shared faces, edges and vertices, including equal-distance ties. A nonempty vector tail repeats its final valid neighbor; a single-color palette has an empty row. Conservative boxes may retain a dense graph but cannot exclude a necessary improving edge. The legacy Metal byte builders require 32-lane SIMD groups; Vulkan reduces complete workgroups through shared memory and does not assume a hardware subgroup width. Pipeline unavailability detected during execution fails an admitted indexed plan without retrying the frame on CPU.

The indexed adapter selects the GPU-only preparation plan before constructing a CPU graph; [execution-map](../../execution-map/README.md) owns that admission and fallback contract. Bounds, compact sorted rows, seeds and palette application run in ordered GPU passes. Metal encoder boundaries and Vulkan compute barriers establish dependencies without intermediate CPU waits. One lock protects the cache. Palette size and all palette bytes identify it independently of ordinary uploads and other lookup caches. Identity is published only after successful command completion; a failed rebuild invalidates it. Unchanged palettes reuse the table, while changed or alternating palettes rebuild it.

The cache occupies 537 KiB: two fixed-stride 256x256 uint32 row arrays, 256 degrees, 4096 uint32 seeds and 512 int4 bounds. Construction performs quadratic pair work and `O(K^2 log^2 K)` bitonic work, plus `4096K` seed distance evaluations. Lookup may visit several rows; no universal speed advantage over `ordered` is promised. Include changed-palette construction, uploads and synchronization when measuring application cost.

Transparency, positional dithering, blue-noise configuration and delta keep decisions use the shared GPU path. Kept pixels do no lookup. The retained result mask and history publication follow the same rules as other GPU policies. `SIXEL_TRACE_TOPIC=gpu_contract` reports `kernel=delaunay` and `lookup=delaunay|table=rebuild` or `lookup=delaunay|table=reuse`.

For [GPU diffusion](../dithering/README.md), the policy builds the same conservative adjacency for a scalar fractional graph walk. The outward-rounded boxes already contain continuous Voronoi cells, including fractional points. A seed selects the initial vertex; all query distances retain fractional coordinates. Each move decreases the computed distance/index pair. The row radius test uses `4 * distance_squared + 1`: the extra squared code-value unit conservatively covers float rounding in the pruning bound without changing candidate distances. The plan and builders are owned by [`lookup-policy-delaunay-gpu.c`](../../../../src/lookup-policy-delaunay-gpu.c) and its [Metal fractional shader](../../../../src/lookup-policy-delaunay-fractional-gpu.metal) or [Vulkan program](../../../../src/lookup-policy-delaunay-gpu.glsl). The fractional builder reduces bounds across its complete 32-thread group, using shared memory when hardware SIMD groups are narrower. Adjacency counts use the actual SIMD lane, group index and group count rather than dividing thread indexes by 32; otherwise valid neighbors can disappear from the graph. Vulkan retains the separate four-neighbor integer byte walk and scalar fractional walk, with the same bounds, sorted rows and rational seeds. Its bound builder explicitly derives the signed remainder to preserve Metal's outward rounding without using GLSL remainder on negative operands. The [parent reference](README.md) defines fractional precision and cache identity.

## Implementation map

The common builder is [`lookup-delaunay-graph.c`](../../../../src/lookup-delaunay-graph.c). [`lookup-delaunay-8bit.c`](../../../../src/lookup-delaunay-8bit.c) and [`lookup-delaunay-float32.c`](../../../../src/lookup-delaunay-float32.c) own query arithmetic, SIMD and memoization. [`lookup-policy-delaunay.c`](../../../../src/lookup-policy-delaunay.c) owns preparation, representation conversion and reuse through `lookup/delaunay.8bit` and `lookup/delaunay.float32`.

The legacy Metal byte implementation and its cache live in [`gpu-palette-metal.m`](../../../../src/gpu-palette-metal.m). The Vulkan builders and both query entries belong to [`lookup-policy-delaunay-gpu.glsl`](../../../../src/lookup-policy-delaunay-gpu.glsl), with storage and completion owned by the policy plan; admission is shared through [`gpu-palette.c`](../../../../src/gpu-palette.c) and [`palette-apply.c`](../../../../src/palette-apply.c).

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| DL-01 | Deterministic RGB samples match the integer oracle for a 1-color random palette, without a hint. | [tests/lookup/delaunay/0001_delaunay_byte_map_random_1.t](../../../../tests/lookup/delaunay/0001_delaunay_byte_map_random_1.t) |
| DL-02 | Deterministic RGB samples match the integer oracle for a 2-color random palette, without a hint. | [tests/lookup/delaunay/0002_delaunay_byte_map_random_2.t](../../../../tests/lookup/delaunay/0002_delaunay_byte_map_random_2.t) |
| DL-03 | Deterministic RGB samples match the integer oracle for a 3-color random palette, without a hint. | [tests/lookup/delaunay/0003_delaunay_byte_map_random_3.t](../../../../tests/lookup/delaunay/0003_delaunay_byte_map_random_3.t) |
| DL-04 | Deterministic RGB samples match the integer oracle for a 7-color random palette, without a hint. | [tests/lookup/delaunay/0004_delaunay_byte_map_random_7.t](../../../../tests/lookup/delaunay/0004_delaunay_byte_map_random_7.t) |
| DL-05 | Deterministic RGB samples match the integer oracle for a 40-color random palette, without a hint. | [tests/lookup/delaunay/0005_delaunay_byte_map_random_40.t](../../../../tests/lookup/delaunay/0005_delaunay_byte_map_random_40.t) |
| DL-06 | Deterministic RGB samples match the integer oracle for a 64-color random palette, without a hint. | [tests/lookup/delaunay/0006_delaunay_byte_map_random_64.t](../../../../tests/lookup/delaunay/0006_delaunay_byte_map_random_64.t) |
| DL-07 | Deterministic RGB samples match the integer oracle for a 256-color random palette, without a hint. | [tests/lookup/delaunay/0007_delaunay_byte_map_random_256.t](../../../../tests/lookup/delaunay/0007_delaunay_byte_map_random_256.t) |
| DL-08 | Deterministic RGB samples retain the lowest original index when duplicate colors collapse to representatives. | [tests/lookup/delaunay/0008_delaunay_byte_map_duplicates.t](../../../../tests/lookup/delaunay/0008_delaunay_byte_map_duplicates.t) |
| DL-09 | Deterministic RGB samples match the oracle for the cospherical grid over `{0,64,128,192}`. | [tests/lookup/delaunay/0009_delaunay_byte_map_grid.t](../../../../tests/lookup/delaunay/0009_delaunay_byte_map_grid.t) |
| DL-10 | Sampled native float queries and a near-coincident three-site regression match the double oracle regardless of hint. | [tests/lookup/delaunay/0010_delaunay_float_exact.t](../../../../tests/lookup/delaunay/0010_delaunay_float_exact.t) |
| DL-11 | Scalar and active SIMD distance expressions produce identical double bits. | [tests/lookup/delaunay/0011_delaunay_float_distance.t](../../../../tests/lookup/delaunay/0011_delaunay_float_distance.t) |
| DL-12 | Bulk results preserve scalar selection across memo collisions, reverse strides and short tails. | [tests/lookup/delaunay/0012_delaunay_float_bulk.t](../../../../tests/lookup/delaunay/0012_delaunay_float_bulk.t) |
| DL-13 | A 300-color palette uses separate distance and ID comparisons without truncating indices. | [tests/lookup/delaunay/0013_delaunay_byte_map_large.t](../../../../tests/lookup/delaunay/0013_delaunay_byte_map_large.t) |
| DL-20 | Each helper invocation compares one selected byte lookup path with a palette-order integer oracle. | [tests/_support/processing/lookup/0021_delaunay_byte_exact.c](../../../../tests/_support/processing/lookup/0021_delaunay_byte_exact.c) |
| DL-21 | Native float indices match an independent double oracle for duplicates, almost-coincident sites, grids, extended, extreme and nonfinite queries. | [tests/lookup/delaunay/0010_delaunay_float_exact.c](../../../../tests/lookup/delaunay/0010_delaunay_float_exact.c) |
| DL-22 | Neighbor and cold-query double SIMD lanes match scalar distance bits; builds without the gate check the scalar formula. | [tests/lookup/delaunay/0011_delaunay_float_distance.c](../../../../tests/lookup/delaunay/0011_delaunay_float_distance.c) |
| DL-23 | Float cold batches and memoization preserve complete keys, collisions, signed zero, duplicate ties, reverse rows, all short tails, absent scratch and palettes across the 64- and 256-entry boundaries. | [tests/lookup/delaunay/0012_delaunay_float_bulk.c](../../../../tests/lookup/delaunay/0012_delaunay_float_bulk.c) |
| DL-24 | Both representations preserve ordered scalar semantics in forward and reverse bulk rows. | [tests/lookup/dispatch/0012_lookup_batch_contract.c](../../../../tests/lookup/dispatch/0012_lookup_batch_contract.c) |
| DL-25 | Gamma sRGB palettes select the float consumer and preserve native palette coordinates. | [tests/lookup/delaunay/0045_delaunay_gamma_srgb.t](../../../../tests/lookup/delaunay/0045_delaunay_gamma_srgb.t) |
| DL-26 | Gamma Display-P3 palettes select the float consumer and preserve native palette coordinates. | [tests/lookup/delaunay/0046_delaunay_gamma_p3.t](../../../../tests/lookup/delaunay/0046_delaunay_gamma_p3.t) |
| DL-27 | Byte-only palettes select the byte consumer. | [tests/lookup/delaunay/0047_delaunay_byte_eligibility.t](../../../../tests/lookup/delaunay/0047_delaunay_byte_eligibility.t) |
| DL-28 | Private worker bands preserve the independently encoded band output. | [tests/lookup/delaunay/0048_delaunay_bands_private.t](../../../../tests/lookup/delaunay/0048_delaunay_bands_private.t) |
| DL-29 | Requesting shared bands preserves the VP-tree-compatible private-policy behavior and band output. | [tests/lookup/delaunay/0049_delaunay_bands_shared.t](../../../../tests/lookup/delaunay/0049_delaunay_bands_shared.t) |
| DL-30 | Byte cold batches and warm rows match an independent integer oracle across duplicate ties, full-key collisions, the white/index-255 sentinel, reverse padded strides and short tails. | [tests/lookup/delaunay/0014_delaunay_byte_bulk.c](../../../../tests/lookup/delaunay/0014_delaunay_byte_bulk.c), [tests/lookup/delaunay/0014_delaunay_byte_bulk.t](../../../../tests/lookup/delaunay/0014_delaunay_byte_bulk.t) |

| DL-31 | Scalar walks from varying palette centers match an integer oracle for a 1-color random palette. | [tests/lookup/delaunay/0015_delaunay_byte_scalar_random_1.t](../../../../tests/lookup/delaunay/0015_delaunay_byte_scalar_random_1.t) |
| DL-32 | Scalar walks from varying palette centers match an integer oracle for a 2-color random palette. | [tests/lookup/delaunay/0016_delaunay_byte_scalar_random_2.t](../../../../tests/lookup/delaunay/0016_delaunay_byte_scalar_random_2.t) |
| DL-33 | Scalar walks from varying palette centers match an integer oracle for a 3-color random palette. | [tests/lookup/delaunay/0017_delaunay_byte_scalar_random_3.t](../../../../tests/lookup/delaunay/0017_delaunay_byte_scalar_random_3.t) |
| DL-34 | Scalar walks from varying palette centers match an integer oracle for a 7-color random palette. | [tests/lookup/delaunay/0018_delaunay_byte_scalar_random_7.t](../../../../tests/lookup/delaunay/0018_delaunay_byte_scalar_random_7.t) |
| DL-35 | Scalar walks from varying palette centers match an integer oracle for a 40-color random palette. | [tests/lookup/delaunay/0019_delaunay_byte_scalar_random_40.t](../../../../tests/lookup/delaunay/0019_delaunay_byte_scalar_random_40.t) |
| DL-36 | Scalar walks from varying palette centers match an integer oracle for a 64-color random palette. | [tests/lookup/delaunay/0020_delaunay_byte_scalar_random_64.t](../../../../tests/lookup/delaunay/0020_delaunay_byte_scalar_random_64.t) |
| DL-37 | Scalar walks from varying palette centers match an integer oracle for a 256-color random palette. | [tests/lookup/delaunay/0021_delaunay_byte_scalar_random_256.t](../../../../tests/lookup/delaunay/0021_delaunay_byte_scalar_random_256.t) |
| DL-38 | Scalar walks from varying palette centers match an integer oracle for a palette with duplicate colors. | [tests/lookup/delaunay/0022_delaunay_byte_scalar_duplicates.t](../../../../tests/lookup/delaunay/0022_delaunay_byte_scalar_duplicates.t) |
| DL-39 | Scalar walks from varying palette centers match an integer oracle for the grid palette. | [tests/lookup/delaunay/0023_delaunay_byte_scalar_grid.t](../../../../tests/lookup/delaunay/0023_delaunay_byte_scalar_grid.t) |
| DL-40 | Scalar walks from varying palette centers match an integer oracle for a 300-color palette. | [tests/lookup/delaunay/0024_delaunay_byte_scalar_large.t](../../../../tests/lookup/delaunay/0024_delaunay_byte_scalar_large.t) |
| DL-41 | Active SIMD walks from varying palette centers match an integer oracle for a 1-color random palette. | [tests/lookup/delaunay/0025_delaunay_byte_simd_random_1.t](../../../../tests/lookup/delaunay/0025_delaunay_byte_simd_random_1.t) |
| DL-42 | Active SIMD walks from varying palette centers match an integer oracle for a 2-color random palette. | [tests/lookup/delaunay/0026_delaunay_byte_simd_random_2.t](../../../../tests/lookup/delaunay/0026_delaunay_byte_simd_random_2.t) |
| DL-43 | Active SIMD walks from varying palette centers match an integer oracle for a 3-color random palette. | [tests/lookup/delaunay/0027_delaunay_byte_simd_random_3.t](../../../../tests/lookup/delaunay/0027_delaunay_byte_simd_random_3.t) |
| DL-44 | Active SIMD walks from varying palette centers match an integer oracle for a 7-color random palette. | [tests/lookup/delaunay/0028_delaunay_byte_simd_random_7.t](../../../../tests/lookup/delaunay/0028_delaunay_byte_simd_random_7.t) |
| DL-45 | Active SIMD walks from varying palette centers match an integer oracle for a 40-color random palette. | [tests/lookup/delaunay/0029_delaunay_byte_simd_random_40.t](../../../../tests/lookup/delaunay/0029_delaunay_byte_simd_random_40.t) |
| DL-46 | Active SIMD walks from varying palette centers match an integer oracle for a 64-color random palette. | [tests/lookup/delaunay/0030_delaunay_byte_simd_random_64.t](../../../../tests/lookup/delaunay/0030_delaunay_byte_simd_random_64.t) |
| DL-47 | Active SIMD walks from varying palette centers match an integer oracle for a 256-color random palette. | [tests/lookup/delaunay/0031_delaunay_byte_simd_random_256.t](../../../../tests/lookup/delaunay/0031_delaunay_byte_simd_random_256.t) |
| DL-48 | Active SIMD walks from varying palette centers match an integer oracle for a palette with duplicate colors. | [tests/lookup/delaunay/0032_delaunay_byte_simd_duplicates.t](../../../../tests/lookup/delaunay/0032_delaunay_byte_simd_duplicates.t) |
| DL-49 | Active SIMD walks from varying palette centers match an integer oracle for the grid palette. | [tests/lookup/delaunay/0033_delaunay_byte_simd_grid.t](../../../../tests/lookup/delaunay/0033_delaunay_byte_simd_grid.t) |
| DL-50 | Active SIMD walks from varying palette centers match an integer oracle for a 300-color palette. | [tests/lookup/delaunay/0034_delaunay_byte_simd_large.t](../../../../tests/lookup/delaunay/0034_delaunay_byte_simd_large.t) |
| DL-51 | Lookup with an out-of-range hint matches an integer oracle for a 1-color random palette. | [tests/lookup/delaunay/0035_delaunay_byte_stale_random_1.t](../../../../tests/lookup/delaunay/0035_delaunay_byte_stale_random_1.t) |
| DL-52 | Lookup with an out-of-range hint matches an integer oracle for a 2-color random palette. | [tests/lookup/delaunay/0036_delaunay_byte_stale_random_2.t](../../../../tests/lookup/delaunay/0036_delaunay_byte_stale_random_2.t) |
| DL-53 | Lookup with an out-of-range hint matches an integer oracle for a 3-color random palette. | [tests/lookup/delaunay/0037_delaunay_byte_stale_random_3.t](../../../../tests/lookup/delaunay/0037_delaunay_byte_stale_random_3.t) |
| DL-54 | Lookup with an out-of-range hint matches an integer oracle for a 7-color random palette. | [tests/lookup/delaunay/0038_delaunay_byte_stale_random_7.t](../../../../tests/lookup/delaunay/0038_delaunay_byte_stale_random_7.t) |
| DL-55 | Lookup with an out-of-range hint matches an integer oracle for a 40-color random palette. | [tests/lookup/delaunay/0039_delaunay_byte_stale_random_40.t](../../../../tests/lookup/delaunay/0039_delaunay_byte_stale_random_40.t) |
| DL-56 | Lookup with an out-of-range hint matches an integer oracle for a 64-color random palette. | [tests/lookup/delaunay/0040_delaunay_byte_stale_random_64.t](../../../../tests/lookup/delaunay/0040_delaunay_byte_stale_random_64.t) |
| DL-57 | Lookup with an out-of-range hint matches an integer oracle for a 256-color random palette. | [tests/lookup/delaunay/0041_delaunay_byte_stale_random_256.t](../../../../tests/lookup/delaunay/0041_delaunay_byte_stale_random_256.t) |
| DL-58 | Lookup with an out-of-range hint matches an integer oracle for a palette with duplicate colors. | [tests/lookup/delaunay/0042_delaunay_byte_stale_duplicates.t](../../../../tests/lookup/delaunay/0042_delaunay_byte_stale_duplicates.t) |
| DL-59 | Lookup with an out-of-range hint matches an integer oracle for the grid palette. | [tests/lookup/delaunay/0043_delaunay_byte_stale_grid.t](../../../../tests/lookup/delaunay/0043_delaunay_byte_stale_grid.t) |
| DL-60 | Lookup with an out-of-range hint matches an integer oracle for a 300-color palette. | [tests/lookup/delaunay/0044_delaunay_byte_stale_large.t](../../../../tests/lookup/delaunay/0044_delaunay_byte_stale_large.t) |

### GPU behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| DLG-01 | Gpu none plain p1. | [tests/lookup/delaunay/0050_gpu_none_plain_p1.t](../../../../tests/lookup/delaunay/0050_gpu_none_plain_p1.t) |
| DLG-02 | Gpu none plain p2. | [tests/lookup/delaunay/0051_gpu_none_plain_p2.t](../../../../tests/lookup/delaunay/0051_gpu_none_plain_p2.t) |
| DLG-03 | Gpu none plain p4. | [tests/lookup/delaunay/0052_gpu_none_plain_p4.t](../../../../tests/lookup/delaunay/0052_gpu_none_plain_p4.t) |
| DLG-04 | Gpu none plain p33. | [tests/lookup/delaunay/0053_gpu_none_plain_p33.t](../../../../tests/lookup/delaunay/0053_gpu_none_plain_p33.t) |
| DLG-05 | Gpu none plain p129. | [tests/lookup/delaunay/0054_gpu_none_plain_p129.t](../../../../tests/lookup/delaunay/0054_gpu_none_plain_p129.t) |
| DLG-06 | Gpu none plain p255. | [tests/lookup/delaunay/0055_gpu_none_plain_p255.t](../../../../tests/lookup/delaunay/0055_gpu_none_plain_p255.t) |
| DLG-07 | Gpu none plain p256. | [tests/lookup/delaunay/0056_gpu_none_plain_p256.t](../../../../tests/lookup/delaunay/0056_gpu_none_plain_p256.t) |
| DLG-08 | Gpu none keep p1. | [tests/lookup/delaunay/0057_gpu_none_keep_p1.t](../../../../tests/lookup/delaunay/0057_gpu_none_keep_p1.t) |
| DLG-09 | Gpu none keep p2. | [tests/lookup/delaunay/0058_gpu_none_keep_p2.t](../../../../tests/lookup/delaunay/0058_gpu_none_keep_p2.t) |
| DLG-10 | Gpu none keep p4. | [tests/lookup/delaunay/0059_gpu_none_keep_p4.t](../../../../tests/lookup/delaunay/0059_gpu_none_keep_p4.t) |
| DLG-11 | Gpu none keep p33. | [tests/lookup/delaunay/0060_gpu_none_keep_p33.t](../../../../tests/lookup/delaunay/0060_gpu_none_keep_p33.t) |
| DLG-12 | Gpu none keep p129. | [tests/lookup/delaunay/0061_gpu_none_keep_p129.t](../../../../tests/lookup/delaunay/0061_gpu_none_keep_p129.t) |
| DLG-13 | Gpu none keep p255. | [tests/lookup/delaunay/0062_gpu_none_keep_p255.t](../../../../tests/lookup/delaunay/0062_gpu_none_keep_p255.t) |
| DLG-14 | Gpu bluenoise plain p1. | [tests/lookup/delaunay/0063_gpu_bluenoise_plain_p1.t](../../../../tests/lookup/delaunay/0063_gpu_bluenoise_plain_p1.t) |
| DLG-15 | Gpu bluenoise plain p2. | [tests/lookup/delaunay/0064_gpu_bluenoise_plain_p2.t](../../../../tests/lookup/delaunay/0064_gpu_bluenoise_plain_p2.t) |
| DLG-16 | Gpu bluenoise plain p4. | [tests/lookup/delaunay/0065_gpu_bluenoise_plain_p4.t](../../../../tests/lookup/delaunay/0065_gpu_bluenoise_plain_p4.t) |
| DLG-17 | Gpu bluenoise plain p33. | [tests/lookup/delaunay/0066_gpu_bluenoise_plain_p33.t](../../../../tests/lookup/delaunay/0066_gpu_bluenoise_plain_p33.t) |
| DLG-18 | Gpu bluenoise plain p129. | [tests/lookup/delaunay/0067_gpu_bluenoise_plain_p129.t](../../../../tests/lookup/delaunay/0067_gpu_bluenoise_plain_p129.t) |
| DLG-19 | Gpu bluenoise plain p255. | [tests/lookup/delaunay/0068_gpu_bluenoise_plain_p255.t](../../../../tests/lookup/delaunay/0068_gpu_bluenoise_plain_p255.t) |
| DLG-20 | Gpu bluenoise plain p256. | [tests/lookup/delaunay/0069_gpu_bluenoise_plain_p256.t](../../../../tests/lookup/delaunay/0069_gpu_bluenoise_plain_p256.t) |
| DLG-21 | Gpu bluenoise keep p1. | [tests/lookup/delaunay/0070_gpu_bluenoise_keep_p1.t](../../../../tests/lookup/delaunay/0070_gpu_bluenoise_keep_p1.t) |
| DLG-22 | Gpu bluenoise keep p2. | [tests/lookup/delaunay/0071_gpu_bluenoise_keep_p2.t](../../../../tests/lookup/delaunay/0071_gpu_bluenoise_keep_p2.t) |
| DLG-23 | Gpu bluenoise keep p4. | [tests/lookup/delaunay/0072_gpu_bluenoise_keep_p4.t](../../../../tests/lookup/delaunay/0072_gpu_bluenoise_keep_p4.t) |
| DLG-24 | Gpu bluenoise keep p33. | [tests/lookup/delaunay/0073_gpu_bluenoise_keep_p33.t](../../../../tests/lookup/delaunay/0073_gpu_bluenoise_keep_p33.t) |
| DLG-25 | Gpu bluenoise keep p129. | [tests/lookup/delaunay/0074_gpu_bluenoise_keep_p129.t](../../../../tests/lookup/delaunay/0074_gpu_bluenoise_keep_p129.t) |
| DLG-26 | Gpu bluenoise keep p255. | [tests/lookup/delaunay/0075_gpu_bluenoise_keep_p255.t](../../../../tests/lookup/delaunay/0075_gpu_bluenoise_keep_p255.t) |
| DLG-27 | Gpu x plain p1. | [tests/lookup/delaunay/0076_gpu_x_plain_p1.t](../../../../tests/lookup/delaunay/0076_gpu_x_plain_p1.t) |
| DLG-28 | Gpu x plain p2. | [tests/lookup/delaunay/0077_gpu_x_plain_p2.t](../../../../tests/lookup/delaunay/0077_gpu_x_plain_p2.t) |
| DLG-29 | Gpu x plain p4. | [tests/lookup/delaunay/0078_gpu_x_plain_p4.t](../../../../tests/lookup/delaunay/0078_gpu_x_plain_p4.t) |
| DLG-30 | Gpu x plain p33. | [tests/lookup/delaunay/0079_gpu_x_plain_p33.t](../../../../tests/lookup/delaunay/0079_gpu_x_plain_p33.t) |
| DLG-31 | Gpu x plain p129. | [tests/lookup/delaunay/0080_gpu_x_plain_p129.t](../../../../tests/lookup/delaunay/0080_gpu_x_plain_p129.t) |
| DLG-32 | Gpu x plain p255. | [tests/lookup/delaunay/0081_gpu_x_plain_p255.t](../../../../tests/lookup/delaunay/0081_gpu_x_plain_p255.t) |
| DLG-33 | Gpu x plain p256. | [tests/lookup/delaunay/0082_gpu_x_plain_p256.t](../../../../tests/lookup/delaunay/0082_gpu_x_plain_p256.t) |
| DLG-34 | Gpu x keep p1. | [tests/lookup/delaunay/0083_gpu_x_keep_p1.t](../../../../tests/lookup/delaunay/0083_gpu_x_keep_p1.t) |
| DLG-35 | Gpu x keep p2. | [tests/lookup/delaunay/0084_gpu_x_keep_p2.t](../../../../tests/lookup/delaunay/0084_gpu_x_keep_p2.t) |
| DLG-36 | Gpu x keep p4. | [tests/lookup/delaunay/0085_gpu_x_keep_p4.t](../../../../tests/lookup/delaunay/0085_gpu_x_keep_p4.t) |
| DLG-37 | Gpu x keep p33. | [tests/lookup/delaunay/0086_gpu_x_keep_p33.t](../../../../tests/lookup/delaunay/0086_gpu_x_keep_p33.t) |
| DLG-38 | Gpu x keep p129. | [tests/lookup/delaunay/0087_gpu_x_keep_p129.t](../../../../tests/lookup/delaunay/0087_gpu_x_keep_p129.t) |
| DLG-39 | Gpu x keep p255. | [tests/lookup/delaunay/0088_gpu_x_keep_p255.t](../../../../tests/lookup/delaunay/0088_gpu_x_keep_p255.t) |
| DLG-40 | Gpu a plain p1. | [tests/lookup/delaunay/0089_gpu_a_plain_p1.t](../../../../tests/lookup/delaunay/0089_gpu_a_plain_p1.t) |
| DLG-41 | Gpu a plain p2. | [tests/lookup/delaunay/0090_gpu_a_plain_p2.t](../../../../tests/lookup/delaunay/0090_gpu_a_plain_p2.t) |
| DLG-42 | Gpu a plain p4. | [tests/lookup/delaunay/0091_gpu_a_plain_p4.t](../../../../tests/lookup/delaunay/0091_gpu_a_plain_p4.t) |
| DLG-43 | Gpu a plain p33. | [tests/lookup/delaunay/0092_gpu_a_plain_p33.t](../../../../tests/lookup/delaunay/0092_gpu_a_plain_p33.t) |
| DLG-44 | Gpu a plain p129. | [tests/lookup/delaunay/0093_gpu_a_plain_p129.t](../../../../tests/lookup/delaunay/0093_gpu_a_plain_p129.t) |
| DLG-45 | Gpu a plain p255. | [tests/lookup/delaunay/0094_gpu_a_plain_p255.t](../../../../tests/lookup/delaunay/0094_gpu_a_plain_p255.t) |
| DLG-46 | Gpu a plain p256. | [tests/lookup/delaunay/0095_gpu_a_plain_p256.t](../../../../tests/lookup/delaunay/0095_gpu_a_plain_p256.t) |
| DLG-47 | Gpu a keep p1. | [tests/lookup/delaunay/0096_gpu_a_keep_p1.t](../../../../tests/lookup/delaunay/0096_gpu_a_keep_p1.t) |
| DLG-48 | Gpu a keep p2. | [tests/lookup/delaunay/0097_gpu_a_keep_p2.t](../../../../tests/lookup/delaunay/0097_gpu_a_keep_p2.t) |
| DLG-49 | Gpu a keep p4. | [tests/lookup/delaunay/0098_gpu_a_keep_p4.t](../../../../tests/lookup/delaunay/0098_gpu_a_keep_p4.t) |
| DLG-50 | Gpu a keep p33. | [tests/lookup/delaunay/0099_gpu_a_keep_p33.t](../../../../tests/lookup/delaunay/0099_gpu_a_keep_p33.t) |
| DLG-51 | Gpu a keep p129. | [tests/lookup/delaunay/0100_gpu_a_keep_p129.t](../../../../tests/lookup/delaunay/0100_gpu_a_keep_p129.t) |
| DLG-52 | Gpu a keep p255. | [tests/lookup/delaunay/0101_gpu_a_keep_p255.t](../../../../tests/lookup/delaunay/0101_gpu_a_keep_p255.t) |
| DLG-53 | Gpu allocation failure. | [tests/lookup/delaunay/0102_gpu_allocation_failure.t](../../../../tests/lookup/delaunay/0102_gpu_allocation_failure.t) |
| DLG-54 | Gpu encoder failure. | [tests/lookup/delaunay/0103_gpu_encoder_failure.t](../../../../tests/lookup/delaunay/0103_gpu_encoder_failure.t) |
| DLG-55 | Gpu completion failure. | [tests/lookup/delaunay/0104_gpu_completion_failure.t](../../../../tests/lookup/delaunay/0104_gpu_completion_failure.t) |
| DLG-56 | Gpu geometry random. | [tests/lookup/delaunay/0105_gpu_geometry_random.t](../../../../tests/lookup/delaunay/0105_gpu_geometry_random.t) |
| DLG-57 | Gpu geometry duplicate. | [tests/lookup/delaunay/0106_gpu_geometry_duplicate.t](../../../../tests/lookup/delaunay/0106_gpu_geometry_duplicate.t) |
| DLG-58 | Gpu geometry grid. | [tests/lookup/delaunay/0107_gpu_geometry_grid.t](../../../../tests/lookup/delaunay/0107_gpu_geometry_grid.t) |
| DLG-59 | Gpu geometry collinear. | [tests/lookup/delaunay/0108_gpu_geometry_collinear.t](../../../../tests/lookup/delaunay/0108_gpu_geometry_collinear.t) |
| DLG-60 | Gpu geometry coplanar. | [tests/lookup/delaunay/0109_gpu_geometry_coplanar.t](../../../../tests/lookup/delaunay/0109_gpu_geometry_coplanar.t) |
| DLG-61 | Gpu geometry cospherical. | [tests/lookup/delaunay/0110_gpu_geometry_cospherical.t](../../../../tests/lookup/delaunay/0110_gpu_geometry_cospherical.t) |
| DLG-62 | Gpu cache reuse. | [tests/lookup/delaunay/0111_gpu_cache_reuse.t](../../../../tests/lookup/delaunay/0111_gpu_cache_reuse.t) |
| DLG-63 | Gpu cache byte change. | [tests/lookup/delaunay/0112_gpu_cache_byte_change.t](../../../../tests/lookup/delaunay/0112_gpu_cache_byte_change.t) |
| DLG-64 | Gpu cache size change. | [tests/lookup/delaunay/0113_gpu_cache_size_change.t](../../../../tests/lookup/delaunay/0113_gpu_cache_size_change.t) |
| DLG-65 | Gpu cache after direct. | [tests/lookup/delaunay/0114_gpu_cache_after_direct.t](../../../../tests/lookup/delaunay/0114_gpu_cache_after_direct.t) |
| DLG-66 | Gpu cache after gridlut. | [tests/lookup/delaunay/0115_gpu_cache_after_gridlut.t](../../../../tests/lookup/delaunay/0115_gpu_cache_after_gridlut.t) |
| DLG-67 | Gpu cache after ordered. | [tests/lookup/delaunay/0116_gpu_cache_after_ordered.t](../../../../tests/lookup/delaunay/0116_gpu_cache_after_ordered.t) |
| DLG-68 | Gpu cache alternating. | [tests/lookup/delaunay/0117_gpu_cache_alternating.t](../../../../tests/lookup/delaunay/0117_gpu_cache_alternating.t) |

Each dither, keep mode and palette size has its own invocation against CPU exhaustive lookup. Geometry cases separately sample random, duplicate, grid, collinear, coplanar and cospherical palettes using palette hits, pair midpoints, seed boundaries and deterministic random queries; they observe actual GPU `auto` admission and the Delaunay kernel. Cache cases observe one transition per invocation. Fault cases exercise allocation, command-encoder and post-completion failure, actual CPU fallback and subsequent rebuild/reuse. These tests run on Metal and Vulkan, skip when the Delaunay provider is unavailable, and do not enumerate the RGB domain or prove performance on another device.

### Coverage boundary

The byte fixtures use deterministic samples for random palettes of 1, 2, 3, 7, 40, 64, 256 and 300 colors, a duplicate palette and a grid over `{0,64,128,192}`. Each checks a 17-by-17-by-17 RGB lattice including channel endpoints, 4096 fixed-seed pseudorandom keys, 27 neighboring keys per palette entry, and seven neighboring keys around three pair midpoints per entry. These 9009 + 48N probes include palette hits, the white/index-255 sentinel and exact ties on the grid. Separate TAP files compare one selected lookup path with a palette-order integer oracle: ordinary lookup, scalar walk, active SIMD walk or lookup with an out-of-range hint. SIMD-only tests skip when that consumer is not compiled. Each path creates its own graph and memo. This samples the RGB domain rather than establishing correctness for every key. An explicit numeric third helper argument retains prefix enumeration for investigations: `tests/test_runner lookup/0021_delaunay_byte_exact 256 0 16777216 map` checks the entire RGB8 domain for that one palette.

Float tests sample native coordinates and arithmetic boundaries; they do not enumerate the float domain. SIMD evidence depends on the compiler target and arithmetic gate actually reported by the test; a scalar-only build does not validate another architecture's instructions. Performance measurements are separate from these functional contracts.

### Fractional GPU queries

These cases use the [fractional GPU oracle and verification limits](README.md#fractional-gpu-lookup-verification). The two shader-injection precision probes run on Metal and Vulkan; Vulkan requires libshaderc to compile the decorated program. Each checks all 128 queries independently of the dither input, then flips only the last expected index and requires rejection without publishing a complete frame. Restoring the correct expectations must succeed again, exercising the source-keyed pipeline cache as well as the lookup. The diffusion and palette-change fixtures also exercise the Vulkan fractional program.

| ID | Contract | Owning test |
| --- | --- | --- |
| DELAUNAYF-01 | Fractional candidate selection agrees with the independent nearest-color oracle, including construction on narrow-SIMD GPUs. | [tests/lookup/delaunay/0118_gpu_delaunay_fractional_exact.t](../../../../tests/lookup/delaunay/0118_gpu_delaunay_fractional_exact.t) |
| DELAUNAYF-02 | Equal-distance queries and duplicate colors retain the lowest index. | [tests/lookup/delaunay/0119_gpu_delaunay_fractional_ties.t](../../../../tests/lookup/delaunay/0119_gpu_delaunay_fractional_ties.t) |
| DELAUNAYF-03 | Changing query precision creates a separate plan without mutating the held byte plan. | [tests/lookup/delaunay/0120_gpu_delaunay_fractional_cache.t](../../../../tests/lookup/delaunay/0120_gpu_delaunay_fractional_cache.t) |
