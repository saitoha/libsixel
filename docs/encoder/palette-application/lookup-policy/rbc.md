# Lookup policy: `rbc`

This page describes the `rbc` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

Random Ball Cover (RBC) represents palette colors as clusters around a small set of pivots. A cluster can be skipped when the distance to its pivot, minus the cluster radius, is already no better than the best palette distance found.

```text
                    radius rho[j]
                 .----------------.
              .-'                  '-.
             /       palette          \
            |        members           |
             \           p[j]         /
              '-.                  .-'
                 '----------------'

query x -------- d(x,p[j]) --------> pivot
lower bound to every member = max(0, d(x,p[j]) - rho[j])
```

The current float32 implementation uses at most 16 clusters. The CPU 8-bit policy uses an exhaustive palette scan; Metal RGB8 uses deterministic clusters with integer cell and box bounds.

## Method

### Construction

Let:

```text
J = min(K, 16)
```

libsixel chooses deterministic palette positions:

```text
pivot[j] = palette[floor(j K / J)]
```

Every palette color is assigned to its closest pivot. For cluster `C[j]`, its radius is the farthest member distance:

```text
rho[j] = max_(c in C[j]) d(c, p[j])
```

This is RBC-shaped clustering, but it differs from the original algorithm's random representative selection. Deterministic positions make repeated runs stable but do not promise statistically representative pivots.

### Query mathematics

For any cluster member `c`, the triangle inequality gives:

```text
d(x,c) >= d(x,p[j]) - d(c,p[j])
       >= d(x,p[j]) - rho[j]
```

Distances cannot be negative, so the cluster lower bound is:

```text
L[j] = max(0, d(x,p[j]) - rho[j])
```

If `L[j]` is not smaller than the current best distance, no member of that cluster can win and the complete cluster is skipped. Otherwise its members are compared directly.

## Guarantee

The float32 RBC path is nearest-distance exact in its range-normalized metric. Every skipped cluster has a lower bound no better than the current result. Using `>=` may discard a tied entry, so palette tie index need not match an exhaustive first-index scan.

The CPU 8-bit implementation is scan-index exact through its direct unweighted RGB scan. The Metal RGB8 provider preserves the same index using conservative cell and cluster-box bounds.

## Cost

Let `V` be the total number of members in clusters whose bounds survive:

```text
construction              Theta(K J)
one float32 query         O(J + V)
worst float32 query       Theta(K)
extra space               Theta(K + J)
one 8-bit query           Theta(K)
```

`J` is capped at 16, but treating it as a named variable makes the design constraint visible. Good pivots and compact clusters make `V` small; broad or overlapping balls can leave nearly every member to scan.

## Metal RGB8 execution

The RGB888 gamma-space GPU provider is independent of the CPU float cluster implementation and of other lookup policies. On Vulkan, [`lookup-policy-rbc-gpu.glsl`](../../../../src/lookup-policy-rbc-gpu.glsl) owns the same three passes, table and both queries. It preserves the CPU byte scan's unweighted squared RGB distance and first-index tie rule. The injected program supports 1 through 256 paintable colors; shared PaletteApply admission controls eligible dithers and delta views as described in the [execution map](../../execution-map/README.md#4-lookup-per-pixel-application-and-output).

Three GPU passes prepare a private cluster table. The sequence separates each producer from its consumer before pixel lookup begins:

| Pass | Result |
| --- | --- |
| Assign | At most 16 deterministic pivots receive their nearest palette entries. |
| Pack | Each cluster gets a member row, a count and an axis-aligned RGB bounding box. |
| Seed and mask | Each of 32^3 integer RGB cells gets a valid initial color and a mask of potentially winning clusters. |

Each cell covers eight byte values per channel. The largest squared distance from its seed color to any cell corner bounds the nearest distance from above throughout the cell. The minimum squared distance between that cell and a cluster box bounds every member distance from below. A cluster is excluded only when its lower bound is strictly greater than the seed upper bound. Lookup then uses the actual query-to-box bound and scans surviving members four at a time. These integer bounds retain all potential winners, including equal-distance colors; a packed distance/index comparison chooses the lowest original index. Repeating the final member makes short vector tails safe, and empty clusters are ignored.

The provider allocates 279,744 bytes for member rows, assignments, counts, boxes, seeds and masks. Construction costs `O(K J + G (K + J))`, with `J <= 16` clusters and `G = 32^3` cells; query cost is `O(J + V)` in the worst case, where `V` counts scanned members. Cell masks reduce the clusters visited without changing this worst-case bound. Palette count, RGB bytes and device identity determine reuse. Each plan snapshots its palette, so a replacement plan cannot alter an older live plan.

The provider owns its shader, buffer layout and completion protocol. A binding exclusively leases the plan's workspace; a completed successful construction enables reuse, while a failed construction remains unbuilt. GPU preparation does not build a CPU lookup. No new CLI suboptions or interface changes are required. GPU memory and construction cost are policy costs, not a guarantee that RBC is faster than another GPU policy.

For [GPU diffusion](../dithering/README.md), fractional plans cover each cell through `min(low + 8, 255)` instead of the integer upper endpoint `low + 7`. Closing that upper face conservatively includes every fractional point selected by the cell index. Both the seed-distance upper bound and cluster-box lower bound use this expanded box. Runtime cluster tests and member distances retain the fractional query; a one-unit squared-distance slack only makes runtime pruning more conservative. Byte plans retain their original cell bounds and vector search. The [parent reference](README.md) owns fractional precision and cache identity.

## Name and sources

RBC means Random Ball Cover. Lawrence Cayton introduced the data structure for parallel nearest-neighbor search in [“A Nearest Neighbor Data Structure for Graphics Hardware”](https://www.lcayton.com/rbc-adms.pdf), with an accompanying [reference implementation](https://github.com/lcayton/RandomBallCover-GPU).

libsixel borrows the representative, assignment, radius, and triangle-bound shape. Because its current representatives are deterministic and it performs an exact bounded search, it should not be described as a verbatim implementation of Cayton's construction or GPU algorithm.

The policy was added with the related Mahalanobis policy in the [`rbc` implementation commit](https://github.com/saitoha/libsixel/commit/ff38055039f1ac41aec32b81447f10b7c82fd691). Its current implementation is [`lookup-policy-rbc.c`](../../../../src/lookup-policy-rbc.c).

## Test coverage

<!-- test-coverage: enforced -->

The GPU cases fix one policy, input configuration and observation per file. Shader cases require a GPU device that carries the rbc program, Metal or Vulkan; plan lifetime and admission cases use an opaque test device and run without GPU hardware. Exact no-dither cases use an independent integer exhaustive oracle; positional-dither and rectangle cases compare with the CPU path. These bounded fixtures do not enumerate the full RGB cube or establish performance on every GPU.

| ID | Contract | Owning test |
| --- | --- | --- |
| RBCGPU-01 | Palette snapshot. | [tests/lookup/rbc/0045_gpu_palette_snapshot.t](../../../../tests/lookup/rbc/0045_gpu_palette_snapshot.t), [tests/lookup/rbc/0045_gpu_palette_snapshot.c](../../../../tests/lookup/rbc/0045_gpu_palette_snapshot.c) |
| RBCGPU-02 | Cache reuse. | [tests/lookup/rbc/0046_gpu_cache_reuse.t](../../../../tests/lookup/rbc/0046_gpu_cache_reuse.t), [tests/lookup/rbc/0046_gpu_cache_reuse.c](../../../../tests/lookup/rbc/0046_gpu_cache_reuse.c) |
| RBCGPU-03 | Device identity. | [tests/lookup/rbc/0047_gpu_device_identity.t](../../../../tests/lookup/rbc/0047_gpu_device_identity.t), [tests/lookup/rbc/0047_gpu_device_identity.c](../../../../tests/lookup/rbc/0047_gpu_device_identity.c) |
| RBCGPU-04 | Live plan isolation. | [tests/lookup/rbc/0048_gpu_live_plan_isolation.t](../../../../tests/lookup/rbc/0048_gpu_live_plan_isolation.t), [tests/lookup/rbc/0048_gpu_live_plan_isolation.c](../../../../tests/lookup/rbc/0048_gpu_live_plan_isolation.c) |
| RBCGPU-05 | Busy lease. | [tests/lookup/rbc/0049_gpu_busy_lease.t](../../../../tests/lookup/rbc/0049_gpu_busy_lease.t), [tests/lookup/rbc/0049_gpu_busy_lease.c](../../../../tests/lookup/rbc/0049_gpu_busy_lease.c) |
| RBCGPU-06 | Completed reuse. | [tests/lookup/rbc/0050_gpu_completed_reuse.t](../../../../tests/lookup/rbc/0050_gpu_completed_reuse.t), [tests/lookup/rbc/0050_gpu_completed_reuse.c](../../../../tests/lookup/rbc/0050_gpu_completed_reuse.c) |
| RBCGPU-07 | Aborted rebuild. | [tests/lookup/rbc/0051_gpu_aborted_rebuild.t](../../../../tests/lookup/rbc/0051_gpu_aborted_rebuild.t), [tests/lookup/rbc/0051_gpu_aborted_rebuild.c](../../../../tests/lookup/rbc/0051_gpu_aborted_rebuild.c) |
| RBCGPU-08 | Double finish. | [tests/lookup/rbc/0052_gpu_double_finish.t](../../../../tests/lookup/rbc/0052_gpu_double_finish.t), [tests/lookup/rbc/0052_gpu_double_finish.c](../../../../tests/lookup/rbc/0052_gpu_double_finish.c) |
| RBCGPU-09 | Finished view. | [tests/lookup/rbc/0053_gpu_finished_view.t](../../../../tests/lookup/rbc/0053_gpu_finished_view.t), [tests/lookup/rbc/0053_gpu_finished_view.c](../../../../tests/lookup/rbc/0053_gpu_finished_view.c) |
| RBCGPU-10 | Provider lifetime. | [tests/lookup/rbc/0054_gpu_provider_lifetime.t](../../../../tests/lookup/rbc/0054_gpu_provider_lifetime.t), [tests/lookup/rbc/0054_gpu_provider_lifetime.c](../../../../tests/lookup/rbc/0054_gpu_provider_lifetime.c) |
| RBCGPU-11 | Binding lifetime. | [tests/lookup/rbc/0055_gpu_binding_lifetime.t](../../../../tests/lookup/rbc/0055_gpu_binding_lifetime.t), [tests/lookup/rbc/0055_gpu_binding_lifetime.c](../../../../tests/lookup/rbc/0055_gpu_binding_lifetime.c) |
| RBCGPU-12 | Float decline. | [tests/lookup/rbc/0056_gpu_float_decline.t](../../../../tests/lookup/rbc/0056_gpu_float_decline.t), [tests/lookup/rbc/0056_gpu_float_decline.c](../../../../tests/lookup/rbc/0056_gpu_float_decline.c) |
| RBCGPU-13 | No-dither indexes match the integer first-index oracle at 1 colors. | [tests/lookup/rbc/0057_gpu_exact_0_0_1.t](../../../../tests/lookup/rbc/0057_gpu_exact_0_0_1.t), [tests/lookup/rbc/0057_gpu_exact_0_0_1.c](../../../../tests/lookup/rbc/0057_gpu_exact_0_0_1.c) |
| RBCGPU-14 | No-dither indexes match the integer first-index oracle at 2 colors. | [tests/lookup/rbc/0058_gpu_exact_0_0_2.t](../../../../tests/lookup/rbc/0058_gpu_exact_0_0_2.t), [tests/lookup/rbc/0058_gpu_exact_0_0_2.c](../../../../tests/lookup/rbc/0058_gpu_exact_0_0_2.c) |
| RBCGPU-15 | No-dither indexes match the integer first-index oracle at 7 colors. | [tests/lookup/rbc/0059_gpu_exact_0_0_7.t](../../../../tests/lookup/rbc/0059_gpu_exact_0_0_7.t), [tests/lookup/rbc/0059_gpu_exact_0_0_7.c](../../../../tests/lookup/rbc/0059_gpu_exact_0_0_7.c) |
| RBCGPU-16 | No-dither indexes match the integer first-index oracle at 16 colors. | [tests/lookup/rbc/0060_gpu_exact_0_0_16.t](../../../../tests/lookup/rbc/0060_gpu_exact_0_0_16.t), [tests/lookup/rbc/0060_gpu_exact_0_0_16.c](../../../../tests/lookup/rbc/0060_gpu_exact_0_0_16.c) |
| RBCGPU-17 | No-dither indexes match the integer first-index oracle at 17 colors. | [tests/lookup/rbc/0061_gpu_exact_0_0_17.t](../../../../tests/lookup/rbc/0061_gpu_exact_0_0_17.t), [tests/lookup/rbc/0061_gpu_exact_0_0_17.c](../../../../tests/lookup/rbc/0061_gpu_exact_0_0_17.c) |
| RBCGPU-18 | No-dither indexes match the integer first-index oracle at 33 colors. | [tests/lookup/rbc/0062_gpu_exact_0_0_33.t](../../../../tests/lookup/rbc/0062_gpu_exact_0_0_33.t), [tests/lookup/rbc/0062_gpu_exact_0_0_33.c](../../../../tests/lookup/rbc/0062_gpu_exact_0_0_33.c) |
| RBCGPU-19 | No-dither indexes match the integer first-index oracle at 64 colors. | [tests/lookup/rbc/0063_gpu_exact_0_0_64.t](../../../../tests/lookup/rbc/0063_gpu_exact_0_0_64.t), [tests/lookup/rbc/0063_gpu_exact_0_0_64.c](../../../../tests/lookup/rbc/0063_gpu_exact_0_0_64.c) |
| RBCGPU-20 | No-dither indexes match the integer first-index oracle at 129 colors. | [tests/lookup/rbc/0064_gpu_exact_0_0_129.t](../../../../tests/lookup/rbc/0064_gpu_exact_0_0_129.t), [tests/lookup/rbc/0064_gpu_exact_0_0_129.c](../../../../tests/lookup/rbc/0064_gpu_exact_0_0_129.c) |
| RBCGPU-21 | No-dither indexes match the integer first-index oracle at 256 colors. | [tests/lookup/rbc/0065_gpu_exact_0_0_256.t](../../../../tests/lookup/rbc/0065_gpu_exact_0_0_256.t), [tests/lookup/rbc/0065_gpu_exact_0_0_256.c](../../../../tests/lookup/rbc/0065_gpu_exact_0_0_256.c) |
| RBCGPU-22 | Blue-noise indexes match CPU palette application at 64 colors. | [tests/lookup/rbc/0066_gpu_exact_1_0_64.t](../../../../tests/lookup/rbc/0066_gpu_exact_1_0_64.t), [tests/lookup/rbc/0066_gpu_exact_1_0_64.c](../../../../tests/lookup/rbc/0066_gpu_exact_1_0_64.c) |
| RBCGPU-23 | X-dither indexes match CPU palette application at 33 colors. | [tests/lookup/rbc/0067_gpu_exact_2_0_33.t](../../../../tests/lookup/rbc/0067_gpu_exact_2_0_33.t), [tests/lookup/rbc/0067_gpu_exact_2_0_33.c](../../../../tests/lookup/rbc/0067_gpu_exact_2_0_33.c) |
| RBCGPU-24 | A-dither indexes match CPU palette application at 33 colors. | [tests/lookup/rbc/0068_gpu_exact_4_0_33.t](../../../../tests/lookup/rbc/0068_gpu_exact_4_0_33.t), [tests/lookup/rbc/0068_gpu_exact_4_0_33.c](../../../../tests/lookup/rbc/0068_gpu_exact_4_0_33.c) |
| RBCGPU-25 | Force no cpu. | [tests/lookup/rbc/0069_gpu_force_no_cpu.t](../../../../tests/lookup/rbc/0069_gpu_force_no_cpu.t), [tests/lookup/rbc/0069_gpu_force_no_cpu.c](../../../../tests/lookup/rbc/0069_gpu_force_no_cpu.c) |
| RBCGPU-26 | Auto no cpu. | [tests/lookup/rbc/0070_gpu_auto_no_cpu.t](../../../../tests/lookup/rbc/0070_gpu_auto_no_cpu.t), [tests/lookup/rbc/0070_gpu_auto_no_cpu.c](../../../../tests/lookup/rbc/0070_gpu_auto_no_cpu.c) |
| RBCGPU-27 | Rectangle indices. | [tests/lookup/rbc/0071_gpu_rectangle_indices.t](../../../../tests/lookup/rbc/0071_gpu_rectangle_indices.t), [tests/lookup/rbc/0071_gpu_rectangle_indices.c](../../../../tests/lookup/rbc/0071_gpu_rectangle_indices.c) |
| RBCGPU-28 | Rectangle mask. | [tests/lookup/rbc/0072_gpu_rectangle_mask.t](../../../../tests/lookup/rbc/0072_gpu_rectangle_mask.t), [tests/lookup/rbc/0072_gpu_rectangle_mask.c](../../../../tests/lookup/rbc/0072_gpu_rectangle_mask.c) |
| RBCGPU-29 | Blue rectangle indices. | [tests/lookup/rbc/0073_gpu_blue_rectangle_indices.t](../../../../tests/lookup/rbc/0073_gpu_blue_rectangle_indices.t), [tests/lookup/rbc/0073_gpu_blue_rectangle_indices.c](../../../../tests/lookup/rbc/0073_gpu_blue_rectangle_indices.c) |
| RBCGPU-30 | Blue rectangle mask. | [tests/lookup/rbc/0074_gpu_blue_rectangle_mask.t](../../../../tests/lookup/rbc/0074_gpu_blue_rectangle_mask.t), [tests/lookup/rbc/0074_gpu_blue_rectangle_mask.c](../../../../tests/lookup/rbc/0074_gpu_blue_rectangle_mask.c) |
| RBCGPU-31 | Table reuse. | [tests/lookup/rbc/0075_gpu_table_reuse.t](../../../../tests/lookup/rbc/0075_gpu_table_reuse.t), [tests/lookup/rbc/0075_gpu_table_reuse.c](../../../../tests/lookup/rbc/0075_gpu_table_reuse.c) |
| RBCGPU-32 | Palette rebuild. | [tests/lookup/rbc/0076_gpu_palette_rebuild.t](../../../../tests/lookup/rbc/0076_gpu_palette_rebuild.t), [tests/lookup/rbc/0076_gpu_palette_rebuild.c](../../../../tests/lookup/rbc/0076_gpu_palette_rebuild.c) |
| RBCGPU-33 | Count rebuild. | [tests/lookup/rbc/0077_gpu_count_rebuild.t](../../../../tests/lookup/rbc/0077_gpu_count_rebuild.t), [tests/lookup/rbc/0077_gpu_count_rebuild.c](../../../../tests/lookup/rbc/0077_gpu_count_rebuild.c) |
| RBCGPU-34 | Failed build recovery. | [tests/lookup/rbc/0078_gpu_failed_build_recovery.t](../../../../tests/lookup/rbc/0078_gpu_failed_build_recovery.t), [tests/lookup/rbc/0078_gpu_failed_build_recovery.c](../../../../tests/lookup/rbc/0078_gpu_failed_build_recovery.c) |
| RBCGPU-35 | Index255. | [tests/lookup/rbc/0079_gpu_index255.t](../../../../tests/lookup/rbc/0079_gpu_index255.t), [tests/lookup/rbc/0079_gpu_index255.c](../../../../tests/lookup/rbc/0079_gpu_index255.c) |
| RBCGPU-36 | First index tie. | [tests/lookup/rbc/0080_gpu_first_index_tie.t](../../../../tests/lookup/rbc/0080_gpu_first_index_tie.t), [tests/lookup/rbc/0080_gpu_first_index_tie.c](../../../../tests/lookup/rbc/0080_gpu_first_index_tie.c) |
| RBCGPU-37 | Cell boundaries. | [tests/lookup/rbc/0081_gpu_cell_boundaries.t](../../../../tests/lookup/rbc/0081_gpu_cell_boundaries.t), [tests/lookup/rbc/0081_gpu_cell_boundaries.c](../../../../tests/lookup/rbc/0081_gpu_cell_boundaries.c) |

### Fractional GPU queries

These cases use the [fractional GPU oracle and verification limits](README.md#fractional-gpu-lookup-verification).

| ID | Contract | Owning test |
| --- | --- | --- |
| RBCF-01 | Fractional candidate selection agrees with the independent nearest-color oracle. | [tests/lookup/rbc/0082_gpu_rbc_fractional_exact.t](../../../../tests/lookup/rbc/0082_gpu_rbc_fractional_exact.t) |
| RBCF-02 | Equal-distance queries and duplicate colors retain the lowest index. | [tests/lookup/rbc/0083_gpu_rbc_fractional_ties.t](../../../../tests/lookup/rbc/0083_gpu_rbc_fractional_ties.t) |
| RBCF-03 | Changing query precision creates a separate plan without mutating the held byte plan. | [tests/lookup/rbc/0084_gpu_rbc_fractional_cache.t](../../../../tests/lookup/rbc/0084_gpu_rbc_fractional_cache.t) |
