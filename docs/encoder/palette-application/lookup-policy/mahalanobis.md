# Lookup policy: `mahalanobis`

This page describes the `mahalanobis` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

The float32 `mahalanobis` policy retains its historical name and deterministic RBC-like cluster traversal, but selects colors by normalized weighted Euclidean distance. It scans every palette member. It does not build means or inverse covariance matrices or calculate a Mahalanobis score, because nothing would consume them for ordering, pruning or selection, and extended finite coordinates should not pass through unused, overflow-prone covariance arithmetic.

```text
palette --> deterministic RBC clusters
                         |
query -------------------+
                         v
scan every member with normalized weighted Euclidean distance
```

Active distance subtraction, squaring and accumulation use double. The working palette remains float32; preparation widens its exact values and the existing binary32 metric coefficients into a private double search copy, then widens each query once. The search palette uses 12 additional bytes per color compared with a float32 copy, before accounting for removed covariance state. Clusters preserve member traversal and the existing nearest-distance contract, including freedom at ties. No query mutates the palette. The 8-bit implementation remains a direct unweighted scan.

## Method

### Mahalanobis mathematics

For cluster mean `mu[j]`, covariance matrix `Sigma[j]`, and query `x`, the squared Mahalanobis distance is:

```text
M[j](x) = (x - mu[j])^T inverse(Sigma[j]) (x - mu[j])
```

Unlike an axis-isotropic Euclidean ball, this form measures displacement relative to cluster variance and correlation:

```text
Euclidean contours             Mahalanobis contours

       *****                         ********
     **  mu **                    ***   /    ***
       *****                    **    mu       **
                                  ***    /   ***
                                     ******
```

A long, correlated color cluster can therefore regard motion along its major axis as less surprising than the same raw motion across its narrow axis.

The historical implementation computed covariance in range-weighted coordinates with a `1e-6` diagonal regularizer. This described its unused metadata, not the selection metric. The mathematical form above explains the policy's name; current queries do not evaluate it.

### Returned lookup result

Every member is compared using:

```text
D_w(i, x) = sum_d w[d] (x[d] - c[i][d])^2
```

The returned color minimizes normalized weighted Euclidean distance, not Mahalanobis distance. Removing the unused covariance calculation does not introduce anisotropic selection or pruning.

## Guarantee

The float32 path is nearest-distance exact in its normalized weighted-Euclidean metric because the union of all cluster-member loops visits every palette entry. Cluster order may change which tied index is retained. The 8-bit path is a normal scan-index-exact unweighted scan.

This result can differ from float32 `none` in unequal-range color spaces because `none` does not use the range-normalized weights.

## Cost

With `J = min(K,16)`:

```text
cluster assignment                  Theta(K J)
one float32 query                    Theta(K)
whole float32 image                  Theta(K J + P K)
extra space                          Theta(K + J)
one 8-bit query                      Theta(K)
```

Any future pruning rule must prove a lower bound in the same metric as the final palette comparison. A Mahalanobis score is not automatically a lower bound on weighted Euclidean member distance.

## Metal RGB8 execution

The RGB888 gamma-space GPU provider preserves the CPU byte implementation's unweighted exhaustive RGB metric and first-index tie rule. It does not compute covariance, change the selected distance or call another policy's search. Its shader, preparation, workspace and cache are independently owned. On Vulkan, [`lookup-policy-mahalanobis-gpu.glsl`](../../../../src/lookup-policy-mahalanobis-gpu.glsl) implements the same palette packing and byte and fractional queries. The injected program supports 1 through 256 paintable colors; shared PaletteApply admission controls eligible dithers and delta views as described in the [execution map](../../execution-map/README.md#4-lookup-per-pixel-application-and-output).

One GPU pass packs the palette into 256 four-byte entries containing RGB and original index. A query scans four entries at a time and minimizes a packed distance/index key. Short vector tails repeat the last valid entry, retaining the first-index tie rule without reading outside the palette. The private workspace uses 1,024 bytes. One GPU pass writes a fixed 256 packed entries; palette identity checks and copies take `O(K)` host work, and each query takes `Theta(K)` work.

Palette count, RGB bytes and device identity determine reuse. Plans snapshot their palette; preparing a replacement does not change an older live plan. A binding exclusively leases its plan's workspace, and only completed successful construction enables table reuse. A failed construction remains unbuilt. GPU preparation does not build a CPU lookup. This byte provider adds no CLI suboptions or interface changes.

For [GPU diffusion](../dithering/README.md), a separate scalar entry scans the same packed palette using the unrounded fractional query and direct float32 squared differences. It keeps the unweighted RGB metric; no covariance transform or candidate approximation is introduced. Byte vector queries remain unchanged. The [parent reference](README.md) owns fractional precision and cache identity.

## Name and sources

The name refers to P. C. Mahalanobis's generalized statistical distance, introduced in [“On the Generalised Distance in Statistics”](https://doi.org/10.1007/s13171-019-00164-5). That source explains the statistical distance; it does not describe libsixel's RBC clustering or current query loop.

The policy was introduced with RBC in the [`mahalanobis` implementation commit](https://github.com/saitoha/libsixel/commit/ff38055039f1ac41aec32b81447f10b7c82fd691). The implementation is [`lookup-policy-mahalanobis.c`](../../../../src/lookup-policy-mahalanobis.c).

## Test coverage

<!-- test-coverage: enforced -->

The GPU cases fix one policy, input configuration and observation per file. Byte shader cases require a GPU device that carries the mahalanobis program, Metal or Vulkan; plan lifetime and admission cases use an opaque test device and run without GPU hardware. Exact no-dither cases use an independent integer exhaustive oracle; positional-dither and rectangle cases compare with the CPU path. These bounded fixtures do not enumerate the full RGB cube or establish performance on every GPU.

| ID | Contract | Owning test |
| --- | --- | --- |
| MAHGPU-01 | Palette snapshot. | [tests/lookup/mahalanobis/0045_gpu_palette_snapshot.t](../../../../tests/lookup/mahalanobis/0045_gpu_palette_snapshot.t), [tests/lookup/mahalanobis/0045_gpu_palette_snapshot.c](../../../../tests/lookup/mahalanobis/0045_gpu_palette_snapshot.c) |
| MAHGPU-02 | Cache reuse. | [tests/lookup/mahalanobis/0046_gpu_cache_reuse.t](../../../../tests/lookup/mahalanobis/0046_gpu_cache_reuse.t), [tests/lookup/mahalanobis/0046_gpu_cache_reuse.c](../../../../tests/lookup/mahalanobis/0046_gpu_cache_reuse.c) |
| MAHGPU-03 | Device identity. | [tests/lookup/mahalanobis/0047_gpu_device_identity.t](../../../../tests/lookup/mahalanobis/0047_gpu_device_identity.t), [tests/lookup/mahalanobis/0047_gpu_device_identity.c](../../../../tests/lookup/mahalanobis/0047_gpu_device_identity.c) |
| MAHGPU-04 | Live plan isolation. | [tests/lookup/mahalanobis/0048_gpu_live_plan_isolation.t](../../../../tests/lookup/mahalanobis/0048_gpu_live_plan_isolation.t), [tests/lookup/mahalanobis/0048_gpu_live_plan_isolation.c](../../../../tests/lookup/mahalanobis/0048_gpu_live_plan_isolation.c) |
| MAHGPU-05 | Busy lease. | [tests/lookup/mahalanobis/0049_gpu_busy_lease.t](../../../../tests/lookup/mahalanobis/0049_gpu_busy_lease.t), [tests/lookup/mahalanobis/0049_gpu_busy_lease.c](../../../../tests/lookup/mahalanobis/0049_gpu_busy_lease.c) |
| MAHGPU-06 | Completed reuse. | [tests/lookup/mahalanobis/0050_gpu_completed_reuse.t](../../../../tests/lookup/mahalanobis/0050_gpu_completed_reuse.t), [tests/lookup/mahalanobis/0050_gpu_completed_reuse.c](../../../../tests/lookup/mahalanobis/0050_gpu_completed_reuse.c) |
| MAHGPU-07 | Aborted rebuild. | [tests/lookup/mahalanobis/0051_gpu_aborted_rebuild.t](../../../../tests/lookup/mahalanobis/0051_gpu_aborted_rebuild.t), [tests/lookup/mahalanobis/0051_gpu_aborted_rebuild.c](../../../../tests/lookup/mahalanobis/0051_gpu_aborted_rebuild.c) |
| MAHGPU-08 | Double finish. | [tests/lookup/mahalanobis/0052_gpu_double_finish.t](../../../../tests/lookup/mahalanobis/0052_gpu_double_finish.t), [tests/lookup/mahalanobis/0052_gpu_double_finish.c](../../../../tests/lookup/mahalanobis/0052_gpu_double_finish.c) |
| MAHGPU-09 | Finished view. | [tests/lookup/mahalanobis/0053_gpu_finished_view.t](../../../../tests/lookup/mahalanobis/0053_gpu_finished_view.t), [tests/lookup/mahalanobis/0053_gpu_finished_view.c](../../../../tests/lookup/mahalanobis/0053_gpu_finished_view.c) |
| MAHGPU-10 | Provider lifetime. | [tests/lookup/mahalanobis/0054_gpu_provider_lifetime.t](../../../../tests/lookup/mahalanobis/0054_gpu_provider_lifetime.t), [tests/lookup/mahalanobis/0054_gpu_provider_lifetime.c](../../../../tests/lookup/mahalanobis/0054_gpu_provider_lifetime.c) |
| MAHGPU-11 | Binding lifetime. | [tests/lookup/mahalanobis/0055_gpu_binding_lifetime.t](../../../../tests/lookup/mahalanobis/0055_gpu_binding_lifetime.t), [tests/lookup/mahalanobis/0055_gpu_binding_lifetime.c](../../../../tests/lookup/mahalanobis/0055_gpu_binding_lifetime.c) |
| MAHGPU-12 | Float decline. | [tests/lookup/mahalanobis/0056_gpu_float_decline.t](../../../../tests/lookup/mahalanobis/0056_gpu_float_decline.t), [tests/lookup/mahalanobis/0056_gpu_float_decline.c](../../../../tests/lookup/mahalanobis/0056_gpu_float_decline.c) |
| MAHGPU-13 | No-dither indexes match the integer first-index oracle at 1 colors. | [tests/lookup/mahalanobis/0057_gpu_exact_0_0_1.t](../../../../tests/lookup/mahalanobis/0057_gpu_exact_0_0_1.t), [tests/lookup/mahalanobis/0057_gpu_exact_0_0_1.c](../../../../tests/lookup/mahalanobis/0057_gpu_exact_0_0_1.c) |
| MAHGPU-14 | No-dither indexes match the integer first-index oracle at 2 colors. | [tests/lookup/mahalanobis/0058_gpu_exact_0_0_2.t](../../../../tests/lookup/mahalanobis/0058_gpu_exact_0_0_2.t), [tests/lookup/mahalanobis/0058_gpu_exact_0_0_2.c](../../../../tests/lookup/mahalanobis/0058_gpu_exact_0_0_2.c) |
| MAHGPU-15 | No-dither indexes match the integer first-index oracle at 7 colors. | [tests/lookup/mahalanobis/0059_gpu_exact_0_0_7.t](../../../../tests/lookup/mahalanobis/0059_gpu_exact_0_0_7.t), [tests/lookup/mahalanobis/0059_gpu_exact_0_0_7.c](../../../../tests/lookup/mahalanobis/0059_gpu_exact_0_0_7.c) |
| MAHGPU-16 | No-dither indexes match the integer first-index oracle at 16 colors. | [tests/lookup/mahalanobis/0060_gpu_exact_0_0_16.t](../../../../tests/lookup/mahalanobis/0060_gpu_exact_0_0_16.t), [tests/lookup/mahalanobis/0060_gpu_exact_0_0_16.c](../../../../tests/lookup/mahalanobis/0060_gpu_exact_0_0_16.c) |
| MAHGPU-17 | No-dither indexes match the integer first-index oracle at 17 colors. | [tests/lookup/mahalanobis/0061_gpu_exact_0_0_17.t](../../../../tests/lookup/mahalanobis/0061_gpu_exact_0_0_17.t), [tests/lookup/mahalanobis/0061_gpu_exact_0_0_17.c](../../../../tests/lookup/mahalanobis/0061_gpu_exact_0_0_17.c) |
| MAHGPU-18 | No-dither indexes match the integer first-index oracle at 33 colors. | [tests/lookup/mahalanobis/0062_gpu_exact_0_0_33.t](../../../../tests/lookup/mahalanobis/0062_gpu_exact_0_0_33.t), [tests/lookup/mahalanobis/0062_gpu_exact_0_0_33.c](../../../../tests/lookup/mahalanobis/0062_gpu_exact_0_0_33.c) |
| MAHGPU-19 | No-dither indexes match the integer first-index oracle at 64 colors. | [tests/lookup/mahalanobis/0063_gpu_exact_0_0_64.t](../../../../tests/lookup/mahalanobis/0063_gpu_exact_0_0_64.t), [tests/lookup/mahalanobis/0063_gpu_exact_0_0_64.c](../../../../tests/lookup/mahalanobis/0063_gpu_exact_0_0_64.c) |
| MAHGPU-20 | No-dither indexes match the integer first-index oracle at 129 colors. | [tests/lookup/mahalanobis/0064_gpu_exact_0_0_129.t](../../../../tests/lookup/mahalanobis/0064_gpu_exact_0_0_129.t), [tests/lookup/mahalanobis/0064_gpu_exact_0_0_129.c](../../../../tests/lookup/mahalanobis/0064_gpu_exact_0_0_129.c) |
| MAHGPU-21 | No-dither indexes match the integer first-index oracle at 256 colors. | [tests/lookup/mahalanobis/0065_gpu_exact_0_0_256.t](../../../../tests/lookup/mahalanobis/0065_gpu_exact_0_0_256.t), [tests/lookup/mahalanobis/0065_gpu_exact_0_0_256.c](../../../../tests/lookup/mahalanobis/0065_gpu_exact_0_0_256.c) |
| MAHGPU-22 | Blue-noise indexes match CPU palette application at 64 colors. | [tests/lookup/mahalanobis/0066_gpu_exact_1_0_64.t](../../../../tests/lookup/mahalanobis/0066_gpu_exact_1_0_64.t), [tests/lookup/mahalanobis/0066_gpu_exact_1_0_64.c](../../../../tests/lookup/mahalanobis/0066_gpu_exact_1_0_64.c) |
| MAHGPU-23 | X-dither indexes match CPU palette application at 33 colors. | [tests/lookup/mahalanobis/0067_gpu_exact_2_0_33.t](../../../../tests/lookup/mahalanobis/0067_gpu_exact_2_0_33.t), [tests/lookup/mahalanobis/0067_gpu_exact_2_0_33.c](../../../../tests/lookup/mahalanobis/0067_gpu_exact_2_0_33.c) |
| MAHGPU-24 | A-dither indexes match CPU palette application at 33 colors. | [tests/lookup/mahalanobis/0068_gpu_exact_4_0_33.t](../../../../tests/lookup/mahalanobis/0068_gpu_exact_4_0_33.t), [tests/lookup/mahalanobis/0068_gpu_exact_4_0_33.c](../../../../tests/lookup/mahalanobis/0068_gpu_exact_4_0_33.c) |
| MAHGPU-25 | Force no cpu. | [tests/lookup/mahalanobis/0069_gpu_force_no_cpu.t](../../../../tests/lookup/mahalanobis/0069_gpu_force_no_cpu.t), [tests/lookup/mahalanobis/0069_gpu_force_no_cpu.c](../../../../tests/lookup/mahalanobis/0069_gpu_force_no_cpu.c) |
| MAHGPU-26 | Auto no cpu. | [tests/lookup/mahalanobis/0070_gpu_auto_no_cpu.t](../../../../tests/lookup/mahalanobis/0070_gpu_auto_no_cpu.t), [tests/lookup/mahalanobis/0070_gpu_auto_no_cpu.c](../../../../tests/lookup/mahalanobis/0070_gpu_auto_no_cpu.c) |
| MAHGPU-27 | Rectangle indices. | [tests/lookup/mahalanobis/0071_gpu_rectangle_indices.t](../../../../tests/lookup/mahalanobis/0071_gpu_rectangle_indices.t), [tests/lookup/mahalanobis/0071_gpu_rectangle_indices.c](../../../../tests/lookup/mahalanobis/0071_gpu_rectangle_indices.c) |
| MAHGPU-28 | Rectangle mask. | [tests/lookup/mahalanobis/0072_gpu_rectangle_mask.t](../../../../tests/lookup/mahalanobis/0072_gpu_rectangle_mask.t), [tests/lookup/mahalanobis/0072_gpu_rectangle_mask.c](../../../../tests/lookup/mahalanobis/0072_gpu_rectangle_mask.c) |
| MAHGPU-29 | Blue rectangle indices. | [tests/lookup/mahalanobis/0073_gpu_blue_rectangle_indices.t](../../../../tests/lookup/mahalanobis/0073_gpu_blue_rectangle_indices.t), [tests/lookup/mahalanobis/0073_gpu_blue_rectangle_indices.c](../../../../tests/lookup/mahalanobis/0073_gpu_blue_rectangle_indices.c) |
| MAHGPU-30 | Blue rectangle mask. | [tests/lookup/mahalanobis/0074_gpu_blue_rectangle_mask.t](../../../../tests/lookup/mahalanobis/0074_gpu_blue_rectangle_mask.t), [tests/lookup/mahalanobis/0074_gpu_blue_rectangle_mask.c](../../../../tests/lookup/mahalanobis/0074_gpu_blue_rectangle_mask.c) |
| MAHGPU-31 | Table reuse. | [tests/lookup/mahalanobis/0075_gpu_table_reuse.t](../../../../tests/lookup/mahalanobis/0075_gpu_table_reuse.t), [tests/lookup/mahalanobis/0075_gpu_table_reuse.c](../../../../tests/lookup/mahalanobis/0075_gpu_table_reuse.c) |
| MAHGPU-32 | Palette rebuild. | [tests/lookup/mahalanobis/0076_gpu_palette_rebuild.t](../../../../tests/lookup/mahalanobis/0076_gpu_palette_rebuild.t), [tests/lookup/mahalanobis/0076_gpu_palette_rebuild.c](../../../../tests/lookup/mahalanobis/0076_gpu_palette_rebuild.c) |
| MAHGPU-33 | Count rebuild. | [tests/lookup/mahalanobis/0077_gpu_count_rebuild.t](../../../../tests/lookup/mahalanobis/0077_gpu_count_rebuild.t), [tests/lookup/mahalanobis/0077_gpu_count_rebuild.c](../../../../tests/lookup/mahalanobis/0077_gpu_count_rebuild.c) |
| MAHGPU-34 | Failed build recovery. | [tests/lookup/mahalanobis/0078_gpu_failed_build_recovery.t](../../../../tests/lookup/mahalanobis/0078_gpu_failed_build_recovery.t), [tests/lookup/mahalanobis/0078_gpu_failed_build_recovery.c](../../../../tests/lookup/mahalanobis/0078_gpu_failed_build_recovery.c) |
| MAHGPU-35 | Index255. | [tests/lookup/mahalanobis/0079_gpu_index255.t](../../../../tests/lookup/mahalanobis/0079_gpu_index255.t), [tests/lookup/mahalanobis/0079_gpu_index255.c](../../../../tests/lookup/mahalanobis/0079_gpu_index255.c) |
| MAHGPU-36 | First index tie. | [tests/lookup/mahalanobis/0080_gpu_first_index_tie.t](../../../../tests/lookup/mahalanobis/0080_gpu_first_index_tie.t), [tests/lookup/mahalanobis/0080_gpu_first_index_tie.c](../../../../tests/lookup/mahalanobis/0080_gpu_first_index_tie.c) |
| MAHGPU-37 | Cell boundaries. | [tests/lookup/mahalanobis/0081_gpu_cell_boundaries.t](../../../../tests/lookup/mahalanobis/0081_gpu_cell_boundaries.t), [tests/lookup/mahalanobis/0081_gpu_cell_boundaries.c](../../../../tests/lookup/mahalanobis/0081_gpu_cell_boundaries.c) |

### Fractional GPU queries

These cases use the [fractional GPU oracle and verification limits](README.md#fractional-gpu-lookup-verification).

| ID | Contract | Owning test |
| --- | --- | --- |
| MAHALANOBISF-01 | Fractional candidate selection agrees with the independent nearest-color oracle. | [tests/lookup/mahalanobis/0082_gpu_mahalanobis_fractional_exact.t](../../../../tests/lookup/mahalanobis/0082_gpu_mahalanobis_fractional_exact.t) |
| MAHALANOBISF-02 | Equal-distance queries and duplicate colors retain the lowest index. | [tests/lookup/mahalanobis/0083_gpu_mahalanobis_fractional_ties.t](../../../../tests/lookup/mahalanobis/0083_gpu_mahalanobis_fractional_ties.t) |
| MAHALANOBISF-03 | Changing query precision creates a separate plan without mutating the held byte plan. | [tests/lookup/mahalanobis/0084_gpu_mahalanobis_fractional_cache.t](../../../../tests/lookup/mahalanobis/0084_gpu_mahalanobis_fractional_cache.t) |
