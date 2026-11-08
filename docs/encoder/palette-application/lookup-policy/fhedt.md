# Lookup policy: `fhedt`

This page describes the `fhedt` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

`fhedt` builds a discrete Voronoi map of the palette over a three-dimensional color grid. Instead of searching the palette independently for every pixel, it propagates the nearest palette label through the grid once and then answers a query by indexing that grid.

```text
palette entries
      |
      v
seed labeled points in an R x R x R grid
      |
      v
1D distance transform along X
      |
      v
1D distance transform along Y
      |
      v
1D distance transform along Z
      |
      v
nearest-label Voronoi grid --> quantized pixel coordinate --> palette index
```

The current resolutions are `R = 64`, `128`, or `256`; the default is `64`.

Each pass preserves its input source labels until every output site has been evaluated, then publishes distances and labels together. After the first axis, a source has a nonzero carried cost: it can lose at its own coordinate but still win farther along the line. Overwriting that source label early would return the wrong palette entry even if the distance array remained correct. The byte implementation stages at most 256 integer labels on the active worker's stack; this adds no image-sized or grid-sized allocation.

## Method

### One-dimensional transform

For one line of grid sites, let `f(q)` be zero at a palette seed and infinity elsewhere. A weighted squared-Euclidean distance transform computes:

```text
g(i) = min_q (f(q) + w (i - q)^2)
```

Every source position `q` contributes a parabola. The algorithm maintains the lower envelope of those parabolas, producing the complete line in linear time.

```text
cost
 ^          parabola from q2
 |    __---''---__
 | __'            '--__       parabola from q3
 |'  \ lower envelope  \__---''---__
 +--------------------------------------> grid coordinate i
       q1       q2          q3
```

Squared Euclidean distance is separable. Applying the one-dimensional transform on X, then Y, then Z computes the three-dimensional minimum without enumerating every palette entry at every grid coordinate.

### Float palette domain and metric

The float grid covers the union of the nominal working-space box and all finite palette entries. It never clips the stored palette to nominal channel bounds. The grid index rounds each component after an affine transform into `0..R-1`; these grid coordinates are search metadata and do not replace the original float palette or the independently serialized RGB101 codes.

The `weights=auto|L/A/B` suboption (compact `Wauto` or, for example, `W3/1/1`) controls the coordinate scale ratio of the normalized axes. `auto` is the default: CIELAB and OKLab use `3:1:1`, DIN99d uses `2:1:1`, and RGB/linear RGB use `1:1:1`. `W1/1/1` disables the additional emphasis. Explicit ratios accept three positive finite numbers, including fractional values such as `W4.5/1/0.75`. In RGB spaces their positional meaning is R/G/B. A common scale cancels: `3/1/1`, `30/10/10` and `.03/.01/.01` define the same metric.

Ratios scale **coordinate differences before squaring**, after nominal-range normalization. For example, `W3/1/1` compares `(3 * delta_L)^2 + delta_a^2 + delta_b^2` in normalized coordinates, equivalent to squared-distance weights `9:1:1`. This changes lookup geometry without rescaling the stored source pixels, changing the working colorspace or altering gamut preservation. Let `s[d]` be the requested coordinate scale and `r[d]` the declared nominal channel range. The native coefficient is `w[d] = (s[d] / max(s))^2 / r[d]^2`. Thus default CIELAB coefficients are `(1, 1/81, 1/81)`, DIN99d `(1, 1/16, 1/16)`, and OKLab `(1, 1/9, 1/9)`. Uniform ratios give CIELAB `(1, 1/9, 1/9)`, DIN99d `(1, 1/4, 1/4)`, and OKLab `(1, 1, 1)`. Uniform ratios therefore do not mean unscaled Delta E76. Nominal ranges remain those defined by the working pixel format, even when a palette extends beyond them.

The common scale is normalized before squaring, so uniformly huge or tiny tuples do not overflow or underflow solely because of their shared magnitude. All three coefficients must remain positive normal float32 numbers after squaring and range normalization; an extreme ratio that would collapse an axis to zero/subnormal is rejected. The byte backend retains equal weights: `auto` or any uniform tuple preserves that fast path. Nonuniform byte requests fail with a diagnostic directing the caller to `--precision-policy=float32`.

If the native squared-distance coefficient is `w[d]` and the grid box spans `span[d]`, the EDT uses weights proportional to `w[d] * span[d]^2`. The common factor `1/(R-1)^2` and a common normalization cancel in nearest-label comparisons. Palette bounds can expand the grid but do not redefine the nominal metric. Grid construction, native refinement and exterior scans all use the same selected coefficients. Range subtraction, EDT costs and native refinement differences are evaluated in double; opposite finite float extrema do not overflow an intermediate float subtraction.

`SIXEL_LOOKUP_FHEDT_WEIGHTS=auto|L/A/B` supplies the environment default; an explicit suboption, including `auto`, takes precedence. A malformed environment tuple falls back to `auto`. A later bare `-~fhedt` removes the previous CLI override and resolves the environment or built-in default again. The setting is carried per encoder/dither/policy object and its coefficients participate in the shared-grid signature. A reuse hint prepared with different coefficients cannot supply the result. Language bindings using encoder `setopt` accept the same lookup-policy string; no new top-level option constant is needed.

Default CIELAB/DIN99d ratios preserve the former grid metric while retaining the corrected EDT label propagation and extended palette coordinates. They do not promise byte-identical historical output: older refinement used a different metric from the grid, and exterior queries were clipped. OKLab now also defaults to explicit L emphasis. No ratio universally maximizes MS-SSIM or color accuracy; assess the chosen distance together with the diffusion method and resolution.

A finite query outside this box takes a full native-palette distance scan and bypasses the voxel cache. This preserves diffusion overshoot instead of mapping all exterior colors onto a grid face. In-box queries retain the approximate grid algorithm, including seed collisions: when several palette entries round to the same seed coordinate, the last one supplies that seed label. With refinement enabled, a per-seed chain retains the other entries for native-distance comparison, even if that seed has no Voronoi boundary. The optional distance shortcut does not skip this collision refinement. Expanding the box can make cells coarser; retaining float coordinates does not remove this approximation. Nonfinite palette entries are rejected before indexing.

The EDT skips seedless parabolas while retaining the distance expression and tie order for real sources. Y and Z passes gather and scatter up to eight neighboring X columns together, reducing repeated strided memory traffic without changing the independent one-dimensional transforms. Line-level timeline diagnostics retain scalar per-line event ordering. The byte X pass uses a small immutable bitmap of seeded columns to bypass empty columns; their initialized distances and source sentinels are already the transform output. Its per-line timeline start/end markers remain available. Byte grids allocate boundary bits and retained distances only when refinement can use them, at resolutions below R256. Float boundary construction remains controlled by refinement; its distance-retention settings are independent.

### Query and boundary refinement

A query converts each color channel to a grid coordinate and reads the stored source label. Float refinement examines the eight corners of the cell enclosing the actual query, using its floor coordinate as the lower corner. At `R < 256`, many byte colors share a grid cell. An optional boundary bit marks cells where neighboring Voronoi labels disagree; the query can then compare the source labels from at most eight surrounding lattice vertices. Float refinement also visits any native entries sharing those seed coordinates.

```text
one quantized cell

      o---------o       o = surrounding lattice label
     /|        /|
    o---------o |
    | |   x   | |       x = actual query color
    | o-------|-o
    |/        |/
    o---------o

refinement compares up to eight seed labels
(float also visits native entries sharing those seeds)
```

Corner visits retain Z/Y/X order (X changes fastest), and the winner changes only for a strictly smaller weighted squared distance. Byte refinement compares repeated labels again instead of searching a per-query deduplication list: an identical repeated distance cannot replace the first minimum. Float refinement retains its ordered deduplication of all corner labels before following collision chains, avoiding redundant native-distance calculations and chain traversal. The distance expression and its double-precision arithmetic remain unchanged. The byte query optimization preserves the palette index, including tie choices, and therefore preserves the encoded SIXEL bytes for the same input and settings. Boundary gates, retained-distance gates, exterior scans and voxel-cache behavior retain their existing semantics.

This improves a boundary answer but is not an exhaustive palette scan. A true nearest color whose label is absent from those corners can still be omitted.

## Guarantee

For 8-bit RGB at `R = 256`, each possible byte tuple has its own grid coordinate. The transformed label is therefore nearest-distance exact; duplicate or equidistant palette entries may still differ from other lookup policies in tie index. Corner refinement visits this grid label first and replaces it only for a strictly smaller distance, so it cannot change an R256 byte result. CPU and GPU omit that redundant refinement and its auxiliary data while retaining the requested configuration and cache identity. Float queries remain continuous and do not use this shortcut.

At `R = 64` or `128`, the query discretizes byte RGB and is approximate, even with boundary refinement. Float32 input remains continuous, so all supported finite resolutions are approximate.

The optional `SIXEL_LOOKUP_FHEDT_USE_CACHE=1` setting adds a per-thread cache keyed by voxel. For in-box queries it can make repeated colors in one voxel reuse the first refined answer. Exterior scans do not read or populate this cache. The default is off; when enabled at an approximate resolution it adds history dependence.

## Cost

Each one-dimensional pass is linear in the number of grid cells. With three fixed dimensions:

```text
preparation       Theta(R^3 + K)
byte query        Theta(1)
float query       O(K) worst case; usually a grid read and few candidates
whole byte image  Theta(R^3 + K + P)
whole float image O(R^3 + K + P K) worst case
extra space       Theta(R^3 + K)
```

Byte refinement performs at most eight distance comparisons without a growing duplicate-label search. Float refinement uses at most eight local integers to deduplicate corner labels before native-distance comparisons. Neither path needs a precomputed candidate table or additional grid storage.

Float exterior scans and native entries colliding at a seed account for the `O(K)` query bound. Collision chains add one integer per palette entry; they do not add an image-sized buffer. The fixed grid-build cost is substantial: increasing `R` from 64 to 256 multiplies cell count by 64. Small images may not amortize construction, and memory bandwidth can dominate the transform.

On the CPU path, with `SIXEL_LOOKUP_FHEDT_SHARED=1` (the default), the encoder builds one immutable grid immediately after finalizing the working palette. An asynchronous palette job constructs it inside its existing thread-budget scope, independently of the full-frame working-colorspace conversion. Serial and parallel palette application retain references to that grid; preparing a consumer never consumes the source policy. Each handle owns one grid reference and its cache settings; cache entries remain thread-local, and the non-TLS parallel fallback disables the cache. Mapping needs no grid lock.

Reuse requires the same representation, pixel format, allocator, palette signature, resolution, refinement, metric weights and retained-distance setting. A mismatch builds a new grid while existing consumers retain the original. Tiling, first-touch and pinning govern construction only. `SIXEL_LOOKUP_FHEDT_SHARED=0` deliberately disables reference reuse, retaining independent construction for diagnostics and memory/performance comparisons; it does not promise one build per encode. This opt-out preserves the ability to compare sharing against independent grids without changing lookup results.

Tiling, first-touch, thread pinning, retained distances, and shared-grid knobs change constants and NUMA behavior, not the asymptotic order. Their current names and defaults are in the [`img2sixel(1)` manual](../../../../converters/img2sixel.1).

The Metal backend supports RGB888 with uniform positive weights, `R64`, `R128` or `R256`, either `refine` and `dist2` setting, and `cache=0`. It preserves byte FHEDT's quantized seed coordinates, last-index seed collisions, axis tie order and strict corner-refinement comparisons. This is CPU/GPU equivalence at identical settings; R64/R128 remain approximate even with refinement. Positional `none`, `a_dither`, `x_dither` and `bluenoise` application use the existing transparency and delta gates, including offset retained-plane rectangles.

GPU construction uses separate tracked compute encoders within one command buffer. The initial stages clear R² column heads and link palette seeds by their (Y,Z) column. The X stage evaluates only the seeds in that column and writes consecutive X cells, including explicit sentinels for empty columns. Lower X wins an equal-distance tie; the last palette index wins a duplicate seed, independently of atomic list insertion order. The remaining Y/Z transforms use packed 32-bit distance/label scratch cells. Without effective refinement, including R256 with F1 requested, the final axis writes a one-byte LUT directly. Otherwise, a packing pass produces a two-byte LUT carrying the label, boundary flag and zero-distance flag; the last flag implements the uniform-metric `dist2` shortcut. Construction and application complete before index publication.

For byte-query dithers, GPU pipelines and buffers are process-wide under the backend's execution lock, with one reusable table slot. The key contains palette bytes, palette count, resolution, refinement and dist2. It is published only after successful command completion; a failed rebuild invalidates it. CPU-only `shared`, tiling, first-touch and pinning settings do not partition or disable GPU sharing. Uniform weight scaling does not change byte geometry. Scratch buffers and the LUT grow to the largest requested resolution and remain allocated for reuse: at R256 they occupy up to 128 MiB of scratch plus 16 MiB of LUT storage. The seed heads and links temporarily occupy the first scratch buffer and need no additional allocation.

A GPU candidate resolves query settings without building a CPU grid. Unsupported settings, including the history-dependent voxel cache and nonuniform/float metrics, decline GPU admission before output; AUTO can prepare a compatible CPU plan, while FORCE reports an error rather than replacing FHEDT with direct lookup. This does not make otherwise invalid CPU settings valid. The component's copied configuration and fallback boundary are described in the [execution map](../../execution-map/README.md).

GPU diffusion uses an independent fractional provider with the same resolution, uniform weights, `cache=0`, refine and dist2 settings. Construction preserves seed collisions and axis tie order. F0 returns the grid label. F1 tests the original unrounded query against the selected corners' quantized palette coordinates and retains corner traversal tie order. R256 still discretizes continuous queries: F1 retains refinement and its two-byte boundary table. The dist2 zero-cost shortcut additionally requires the actual query to equal that grid seed; a zero grid cost alone does not certify neighboring fractional queries.

[`lookup-policy-fhedt-gpu.c`](../../../../src/lookup-policy-fhedt-gpu.c) owns the fractional plan and [`lookup-policy-fhedt-fractional-gpu.metal`](../../../../src/lookup-policy-fhedt-fractional-gpu.metal) owns its independent builders and query. One leased allocation contains column heads, two packed grids and the output table: at R256/F1 it is about 160.3 MiB. A process-wide idle cache reuses completed workspace and table identity across preparers. Active bindings keep exclusive allocations; concurrent plans cannot overwrite each other's tables. A completed solve releases the workspace to that slot even while its deferred solution retains the frozen plan. The idle slot is always shared, independent of CPU `shared`; its retained capacity is bounded to one allocation. No source-frame residual enters that cache.

## Name and sources

`fhedt` means Felzenszwalb-Huttenlocher Euclidean Distance Transform. The core linear-time lower-envelope algorithm comes from Pedro F. Felzenszwalb and Daniel P. Huttenlocher, [“Distance Transforms of Sampled Functions”](https://cs.brown.edu/people/pfelzens/papers/dt-final.pdf).

The libsixel option was originally named `vpte` and was renamed to `fhedt` in the [attribution-oriented rename commit](https://github.com/saitoha/libsixel/commit/ac25344a8d49e9f8c2545dfad1b1d00598e48343). The three-dimensional palette-label construction and bounded corner refinement are libsixel adaptations of the separable transform.

The policy wrapper is [`lookup-policy-fhedt.c`](../../../../src/lookup-policy-fhedt.c). The representation backends are [`lookup-fhedt-8bit.c`](../../../../src/lookup-fhedt-8bit.c) and [`lookup-fhedt-float32.c`](../../../../src/lookup-fhedt-float32.c).

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The finite GPU references scan palette sites only at queried voxels and refinement neighbors; they do not construct a second dense grid. Separate CPU/GPU application cases cover real dithering, omission indexes and keep masks. Finite sparse fixtures check seed collisions, tie order and propagation through empty columns against an integer palette-site scan. Allocation accounting bounds CPU auxiliary storage, and a GPU execution observation checks the effective R256 table format; output equality alone does not establish these work-elimination properties. Each parameter combination has its own TAP file. These tests do not establish perceptual equivalence between different FHEDT settings or end-to-end terminal throughput. GPU cases skip when Metal execution is unavailable.

| ID | Contract | Owning test |
| --- | --- | --- |
| FHG-01 | Gpu reference r64 f0 d0. | [tests/lookup/fhedt/0099_gpu_reference_r64_f0_d0.t](../../../../tests/lookup/fhedt/0099_gpu_reference_r64_f0_d0.t) |
| FHG-02 | Gpu reference r64 f0 d1. | [tests/lookup/fhedt/0100_gpu_reference_r64_f0_d1.t](../../../../tests/lookup/fhedt/0100_gpu_reference_r64_f0_d1.t) |
| FHG-03 | Gpu reference r64 f1 d0. | [tests/lookup/fhedt/0101_gpu_reference_r64_f1_d0.t](../../../../tests/lookup/fhedt/0101_gpu_reference_r64_f1_d0.t) |
| FHG-04 | Gpu reference r64 f1 d1. | [tests/lookup/fhedt/0102_gpu_reference_r64_f1_d1.t](../../../../tests/lookup/fhedt/0102_gpu_reference_r64_f1_d1.t) |
| FHG-05 | Gpu reference r128 f0 d0. | [tests/lookup/fhedt/0103_gpu_reference_r128_f0_d0.t](../../../../tests/lookup/fhedt/0103_gpu_reference_r128_f0_d0.t) |
| FHG-06 | Gpu reference r128 f0 d1. | [tests/lookup/fhedt/0104_gpu_reference_r128_f0_d1.t](../../../../tests/lookup/fhedt/0104_gpu_reference_r128_f0_d1.t) |
| FHG-07 | Gpu reference r128 f1 d0. | [tests/lookup/fhedt/0105_gpu_reference_r128_f1_d0.t](../../../../tests/lookup/fhedt/0105_gpu_reference_r128_f1_d0.t) |
| FHG-08 | Gpu reference r128 f1 d1. | [tests/lookup/fhedt/0106_gpu_reference_r128_f1_d1.t](../../../../tests/lookup/fhedt/0106_gpu_reference_r128_f1_d1.t) |
| FHG-09 | Gpu reference r256 f0 d0. | [tests/lookup/fhedt/0107_gpu_reference_r256_f0_d0.t](../../../../tests/lookup/fhedt/0107_gpu_reference_r256_f0_d0.t) |
| FHG-10 | Gpu reference r256 f0 d1. | [tests/lookup/fhedt/0108_gpu_reference_r256_f0_d1.t](../../../../tests/lookup/fhedt/0108_gpu_reference_r256_f0_d1.t) |
| FHG-11 | Gpu reference r256 f1 d0. | [tests/lookup/fhedt/0109_gpu_reference_r256_f1_d0.t](../../../../tests/lookup/fhedt/0109_gpu_reference_r256_f1_d0.t) |
| FHG-12 | Gpu reference r256 f1 d1. | [tests/lookup/fhedt/0110_gpu_reference_r256_f1_d1.t](../../../../tests/lookup/fhedt/0110_gpu_reference_r256_f1_d1.t) |
| FHG-13 | Gpu none keep0. | [tests/lookup/fhedt/0111_gpu_none_keep0.t](../../../../tests/lookup/fhedt/0111_gpu_none_keep0.t) |
| FHG-14 | Gpu none keep1. | [tests/lookup/fhedt/0112_gpu_none_keep1.t](../../../../tests/lookup/fhedt/0112_gpu_none_keep1.t) |
| FHG-15 | Gpu bluenoise keep0. | [tests/lookup/fhedt/0113_gpu_bluenoise_keep0.t](../../../../tests/lookup/fhedt/0113_gpu_bluenoise_keep0.t) |
| FHG-16 | Gpu bluenoise keep1. | [tests/lookup/fhedt/0114_gpu_bluenoise_keep1.t](../../../../tests/lookup/fhedt/0114_gpu_bluenoise_keep1.t) |
| FHG-17 | Gpu x keep0. | [tests/lookup/fhedt/0115_gpu_x_keep0.t](../../../../tests/lookup/fhedt/0115_gpu_x_keep0.t) |
| FHG-18 | Gpu x keep1. | [tests/lookup/fhedt/0116_gpu_x_keep1.t](../../../../tests/lookup/fhedt/0116_gpu_x_keep1.t) |
| FHG-19 | Gpu a keep0. | [tests/lookup/fhedt/0117_gpu_a_keep0.t](../../../../tests/lookup/fhedt/0117_gpu_a_keep0.t) |
| FHG-20 | Gpu a keep1. | [tests/lookup/fhedt/0118_gpu_a_keep1.t](../../../../tests/lookup/fhedt/0118_gpu_a_keep1.t) |
| FHG-21 | Gpu palette p1. | [tests/lookup/fhedt/0119_gpu_palette_p1.t](../../../../tests/lookup/fhedt/0119_gpu_palette_p1.t) |
| FHG-22 | Gpu palette p2. | [tests/lookup/fhedt/0120_gpu_palette_p2.t](../../../../tests/lookup/fhedt/0120_gpu_palette_p2.t) |
| FHG-23 | Gpu palette p129. | [tests/lookup/fhedt/0121_gpu_palette_p129.t](../../../../tests/lookup/fhedt/0121_gpu_palette_p129.t) |
| FHG-24 | Gpu palette p256. | [tests/lookup/fhedt/0122_gpu_palette_p256.t](../../../../tests/lookup/fhedt/0122_gpu_palette_p256.t) |
| FHG-25 | Gpu cache reuse. | [tests/lookup/fhedt/0123_gpu_cache_reuse.t](../../../../tests/lookup/fhedt/0123_gpu_cache_reuse.t) |
| FHG-26 | Gpu cache palette. | [tests/lookup/fhedt/0124_gpu_cache_palette.t](../../../../tests/lookup/fhedt/0124_gpu_cache_palette.t) |
| FHG-27 | Gpu cache count. | [tests/lookup/fhedt/0125_gpu_cache_count.t](../../../../tests/lookup/fhedt/0125_gpu_cache_count.t) |
| FHG-28 | Gpu cache resolution. | [tests/lookup/fhedt/0126_gpu_cache_resolution.t](../../../../tests/lookup/fhedt/0126_gpu_cache_resolution.t) |
| FHG-29 | Gpu cache refine. | [tests/lookup/fhedt/0127_gpu_cache_refine.t](../../../../tests/lookup/fhedt/0127_gpu_cache_refine.t) |
| FHG-30 | Gpu cache dist2. | [tests/lookup/fhedt/0128_gpu_cache_dist2.t](../../../../tests/lookup/fhedt/0128_gpu_cache_dist2.t) |
| FHG-31 | Gpu cache after ordered. | [tests/lookup/fhedt/0129_gpu_cache_after_ordered.t](../../../../tests/lookup/fhedt/0129_gpu_cache_after_ordered.t) |
| FHG-32 | Gpu cache alternate. | [tests/lookup/fhedt/0130_gpu_cache_alternate.t](../../../../tests/lookup/fhedt/0130_gpu_cache_alternate.t) |
| FHG-33 | Gpu cache allocation failure. | [tests/lookup/fhedt/0131_gpu_cache_allocation_failure.t](../../../../tests/lookup/fhedt/0131_gpu_cache_allocation_failure.t) |
| FHG-34 | Gpu cache encoder failure. | [tests/lookup/fhedt/0132_gpu_cache_encoder_failure.t](../../../../tests/lookup/fhedt/0132_gpu_cache_encoder_failure.t) |
| FHG-35 | Gpu cache completion failure. | [tests/lookup/fhedt/0133_gpu_cache_completion_failure.t](../../../../tests/lookup/fhedt/0133_gpu_cache_completion_failure.t) |
| FHG-36 | Gpu force no cpu grid. | [tests/lookup/fhedt/0134_gpu_force_no_cpu_grid.t](../../../../tests/lookup/fhedt/0134_gpu_force_no_cpu_grid.t) |
| FHG-37 | Gpu auto no cpu grid. | [tests/lookup/fhedt/0135_gpu_auto_no_cpu_grid.t](../../../../tests/lookup/fhedt/0135_gpu_auto_no_cpu_grid.t) |
| FHG-38 | Gpu threshold cpu grid. | [tests/lookup/fhedt/0136_gpu_threshold_cpu_grid.t](../../../../tests/lookup/fhedt/0136_gpu_threshold_cpu_grid.t) |
| FHG-39 | Gpu off cpu grid. | [tests/lookup/fhedt/0137_gpu_off_cpu_grid.t](../../../../tests/lookup/fhedt/0137_gpu_off_cpu_grid.t) |
| FHG-40 | Gpu diffusion cpu grid. | [tests/lookup/fhedt/0138_gpu_diffusion_cpu_grid.t](../../../../tests/lookup/fhedt/0138_gpu_diffusion_cpu_grid.t) |
| FHG-41 | Gpu cache cpu grid. | [tests/lookup/fhedt/0139_gpu_cache_cpu_grid.t](../../../../tests/lookup/fhedt/0139_gpu_cache_cpu_grid.t) |
| FHG-42 | Snapshot f0. | [tests/lookup/fhedt/0140_snapshot_f0.t](../../../../tests/lookup/fhedt/0140_snapshot_f0.t) |
| FHG-43 | Snapshot f1. | [tests/lookup/fhedt/0141_snapshot_f1.t](../../../../tests/lookup/fhedt/0141_snapshot_f1.t) |
| FHG-44 | Unprepared configuration. | [tests/lookup/fhedt/0142_unprepared_configuration.t](../../../../tests/lookup/fhedt/0142_unprepared_configuration.t) |
| FHG-45 | GPU rectangle none indexes. | [tests/lookup/fhedt/0143_gpu_rectangle_none_indexes.t](../../../../tests/lookup/fhedt/0143_gpu_rectangle_none_indexes.t) |
| FHG-46 | GPU rectangle none mask. | [tests/lookup/fhedt/0144_gpu_rectangle_none_mask.t](../../../../tests/lookup/fhedt/0144_gpu_rectangle_none_mask.t) |
| FHG-47 | GPU rectangle bluenoise indexes. | [tests/lookup/fhedt/0145_gpu_rectangle_bluenoise_indexes.t](../../../../tests/lookup/fhedt/0145_gpu_rectangle_bluenoise_indexes.t) |
| FHG-48 | GPU rectangle bluenoise mask. | [tests/lookup/fhedt/0146_gpu_rectangle_bluenoise_mask.t](../../../../tests/lookup/fhedt/0146_gpu_rectangle_bluenoise_mask.t) |
| FHO-01 | GPU R256 refinement uses a byte table. | [tests/lookup/fhedt/0147_gpu_r256_compact_table.t](../../../../tests/lookup/fhedt/0147_gpu_r256_compact_table.t) |
| FHO-02 | GPU R64 sparse and colliding seed labels. | [tests/lookup/fhedt/0148_gpu_sparse_r64.t](../../../../tests/lookup/fhedt/0148_gpu_sparse_r64.t) |
| FHO-03 | GPU R128 sparse and colliding seed labels. | [tests/lookup/fhedt/0149_gpu_sparse_r128.t](../../../../tests/lookup/fhedt/0149_gpu_sparse_r128.t) |
| FHO-04 | GPU R256 sparse and colliding seed labels. | [tests/lookup/fhedt/0150_gpu_sparse_r256.t](../../../../tests/lookup/fhedt/0150_gpu_sparse_r256.t) |
| FHO-05 | CPU R64 sparse and colliding seed labels. | [tests/lookup/fhedt/0151_cpu_sparse_r64.t](../../../../tests/lookup/fhedt/0151_cpu_sparse_r64.t) |
| FHO-06 | CPU R128 sparse and colliding seed labels. | [tests/lookup/fhedt/0152_cpu_sparse_r128.t](../../../../tests/lookup/fhedt/0152_cpu_sparse_r128.t) |
| FHO-07 | CPU R256 refined exact labels and tie order. | [tests/lookup/fhedt/0153_cpu_sparse_r256_refine.t](../../../../tests/lookup/fhedt/0153_cpu_sparse_r256_refine.t) |
| FHO-08 | CPU R256 omits refinement auxiliary storage. | [tests/lookup/fhedt/0154_cpu_r256_aux_memory.t](../../../../tests/lookup/fhedt/0154_cpu_r256_aux_memory.t) |
| FHO-09 | CPU unrefined grids omit auxiliary storage. | [tests/lookup/fhedt/0155_cpu_unrefined_aux_memory.t](../../../../tests/lookup/fhedt/0155_cpu_unrefined_aux_memory.t) |
| FHO-10 | CPU weighted sparse seed labels. | [tests/lookup/fhedt/0156_cpu_weighted_sparse_r64.t](../../../../tests/lookup/fhedt/0156_cpu_weighted_sparse_r64.t) |
| FHO-11 | GPU dense column collision and tie labels. | [tests/lookup/fhedt/0157_gpu_dense_column_r64.t](../../../../tests/lookup/fhedt/0157_gpu_dense_column_r64.t) |
| FHO-12 | CPU weighted R256 refined exact labels. | [tests/lookup/fhedt/0158_cpu_weighted_sparse_r256_refine.t](../../../../tests/lookup/fhedt/0158_cpu_weighted_sparse_r256_refine.t) |

The fractional cases use bounded fixtures at fixed settings. Approximate-policy references check their documented rule rather than exhaustive-nearest equivalence. Device-dependent cases skip without Metal. These checks do not rank perceived quality or establish throughput.

| ID | Contract | Owning test |
| --- | --- | --- |
| FRAC-01 | Fhedt fractional r64 f0 d0. | [tests/lookup/fhedt/0159_fhedt_fractional_r64_f0_d0.t](../../../../tests/lookup/fhedt/0159_fhedt_fractional_r64_f0_d0.t) |
| FRAC-02 | Fhedt fractional r64 f0 d1. | [tests/lookup/fhedt/0160_fhedt_fractional_r64_f0_d1.t](../../../../tests/lookup/fhedt/0160_fhedt_fractional_r64_f0_d1.t) |
| FRAC-03 | Fhedt fractional r64 f1 d0. | [tests/lookup/fhedt/0161_fhedt_fractional_r64_f1_d0.t](../../../../tests/lookup/fhedt/0161_fhedt_fractional_r64_f1_d0.t) |
| FRAC-04 | Fhedt fractional r64 f1 d1. | [tests/lookup/fhedt/0162_fhedt_fractional_r64_f1_d1.t](../../../../tests/lookup/fhedt/0162_fhedt_fractional_r64_f1_d1.t) |
| FRAC-05 | Fhedt fractional r128 f0 d0. | [tests/lookup/fhedt/0163_fhedt_fractional_r128_f0_d0.t](../../../../tests/lookup/fhedt/0163_fhedt_fractional_r128_f0_d0.t) |
| FRAC-06 | Fhedt fractional r128 f0 d1. | [tests/lookup/fhedt/0164_fhedt_fractional_r128_f0_d1.t](../../../../tests/lookup/fhedt/0164_fhedt_fractional_r128_f0_d1.t) |
| FRAC-07 | Fhedt fractional r128 f1 d0. | [tests/lookup/fhedt/0165_fhedt_fractional_r128_f1_d0.t](../../../../tests/lookup/fhedt/0165_fhedt_fractional_r128_f1_d0.t) |
| FRAC-08 | Fhedt fractional r128 f1 d1. | [tests/lookup/fhedt/0166_fhedt_fractional_r128_f1_d1.t](../../../../tests/lookup/fhedt/0166_fhedt_fractional_r128_f1_d1.t) |
| FRAC-09 | Fhedt fractional r256 f0 d0. | [tests/lookup/fhedt/0167_fhedt_fractional_r256_f0_d0.t](../../../../tests/lookup/fhedt/0167_fhedt_fractional_r256_f0_d0.t) |
| FRAC-10 | Fhedt fractional r256 f0 d1. | [tests/lookup/fhedt/0168_fhedt_fractional_r256_f0_d1.t](../../../../tests/lookup/fhedt/0168_fhedt_fractional_r256_f0_d1.t) |
| FRAC-11 | Fhedt fractional r256 f1 d0. | [tests/lookup/fhedt/0169_fhedt_fractional_r256_f1_d0.t](../../../../tests/lookup/fhedt/0169_fhedt_fractional_r256_f1_d0.t) |
| FRAC-12 | Fhedt fractional r256 f1 d1. | [tests/lookup/fhedt/0170_fhedt_fractional_r256_f1_d1.t](../../../../tests/lookup/fhedt/0170_fhedt_fractional_r256_f1_d1.t) |
| FRAC-13 | Fhedt fractional cache. | [tests/lookup/fhedt/0171_fhedt_fractional_cache.t](../../../../tests/lookup/fhedt/0171_fhedt_fractional_cache.t) |
| FRAC-14 | A completed fractional solve returns its shared table for reuse without rebuilding on the next solve. | [tests/lookup/fhedt/0172_fhedt_fractional_reuse.t](../../../../tests/lookup/fhedt/0172_fhedt_fractional_reuse.t) |
