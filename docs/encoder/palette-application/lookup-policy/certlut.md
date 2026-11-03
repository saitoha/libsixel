# Lookup policy: `certlut`

This page describes the `certlut` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

The CPU 8-bit `certlut` policy combines a lookup table with an intended dominance test. Large RGB cubes are cached when the test accepts the center's winner. Ambiguous cubes split until the test accepts them or they reach a single byte-valued coordinate.

```text
one 4 x 4 x 4 RGB cube
          |
          v
nearest and second-nearest at cube center
          |
          v
 dominance test accepts the center winner?
          | yes                         | no
          v                             v
 cache one palette index       split into 2 x 2 x 2 children
                                      |
                                      v
                              repeat down to unit cells
```

The top-level table has `64^3` entries because each initial cube spans four values on each byte channel.

The Metal RGB8 implementation uses exact whole-cell certificates and boundary candidate lists, described under [Metal RGB8 execution](#metal-rgb8-execution). The float32 implementation uses a kd-tree instead of the hierarchical table. The common policy name does not imply identical algorithms or guarantees across these representations.

## Method

### Certification mathematics

Let `a` be the nearest palette color at cube center `m`, and `b` the second-nearest. Write the weighted-coordinate squared distances as `D_a(x)` and `D_b(x)`. Their difference is affine in `x`:

```text
D_b(x) - D_a(x)
  = D_b(m) - D_a(m) + 2 (x - m) dot (a - b)
```

Let:

```text
gap = D_b(m) - D_a(m)
```

For a cube with side length `s`, the center-to-corner norm is at most `sqrt(3) s / 2`. Cauchy-Schwarz therefore bounds how far the affine difference can fall anywhere in the cube:

```text
|2 (x - m) dot (a - b)| <= sqrt(3) s ||a - b||
```

For this particular pair, `a` is certified against `b` if:

```text
gap > sqrt(3) s ||a - b||
```

The integer implementation avoids square roots by testing the squared form:

```text
gap^2 > 3 s^2 ||a - b||^2
```

All terms are measured in the same weighted coordinates. If the inequality holds, even the strongest possible movement toward `b` cannot close the gap.

A complete cube proof must apply the inequality to every competing palette entry `c[j]`:

```text
for every j != a:
    (D_j(m) - D_a(m))^2 > 3 s^2 ||a - c[j]||^2
```

The current CPU implementation checks only `b`, the second-nearest entry at the center. Being second-nearest by center distance does not prove that `b` has the nearest Voronoi boundary: a more distant center color can have a larger distance gradient and cross `a` inside the cube. The present CPU 8-bit test is therefore a strong heuristic, not a universal certificate.

A one-axis counterexample can be embedded in an RGB cube by keeping the other two channels at 102. At center `m = 102`, use palette coordinates `a = 112`, `b = 113`, and `c = 90` on the varying axis:

```text
center distances: D_a = 100, D_b = 121, D_c = 144
current pair test: (121 - 100)^2 = 441 > 3 * 4^2 * (113 - 112)^2 = 48
```

The current test accepts `a` for the size-four cell `[100,103]`. At coordinate 100, however, `D_a = 144` and `D_c = 100`, so the center's third-nearest entry wins inside that cell. Testing every competitor would reject this cell.

### Float32 kd-tree

The float32 palette is recursively split by coordinate axes. A query visits the near child first. It visits the far child only when the squared distance to the splitting plane is smaller than the current best palette distance:

```text
                       split plane
near subtree <-------------|-------------> far subtree
          query x -------->|<-- plane distance -->

visit far subtree only if plane_distance^2 < best_distance^2
```

This is the standard kd-tree branch-and-bound idea. The policy uses the same range-normalized channel weights for full distances and plane bounds, so the pruning preserves the minimum of that metric.

## Guarantee

The float32 representation is nearest-distance exact in its normalized policy metric: its kd-tree discards a subtree only through a valid axis-plane lower bound.

The CPU 8-bit representation is approximate because a non-unit cube can be cached after checking only the center's nearest and second-nearest entries. Unit cells are exact for their individual byte-valued RGB coordinates, and many larger cubes will also have one true winner, but the current test does not prove that for every accepted cube.

Tie order in the float32 tree can differ from an exhaustive scan, so its guarantee is nearest-distance exact rather than scan-index exact. For unequal-range float colorspaces, the normalized metric can also choose a different index from float32 `none`, which uses unnormalized stored coordinates.

## Cost

The current implementation has these conservative bounds:

```text
CPU 8-bit top-level initialization       Theta(64^3)
CPU 8-bit current search-index build     O(K^2)
CPU 8-bit warm query                     Theta(1), fixed maximum depth
CPU 8-bit cold ambiguous query           O(K)
CPU 8-bit fully refined space            O(256^3 + K)

float32 current kd-tree build        O(K^2)
float32 typical balanced query       O(log K)
float32 worst query                  Theta(K)
float32 space                        Theta(K)
```

CPU certification makes preparation and first-touch behavior palette dependent. A small image may not amortize the index, while a large image can reuse large certified regions. The `shared_instance` suboption controls reuse across workers; its current default is disabled for `certlut`.

## Metal RGB8 execution

The RGB888 gamma-space GPU provider checks every competitor and is scan-index exact: for a fixed working palette and post-dither query, it returns the exhaustive unweighted squared-RGB minimum and the lowest original index at a tie. It supports 1 through 256 paintable colors. This differs intentionally from the CPU byte heuristic and its kd-tree visitation tie order. It introduces no new lookup name, CLI suboption or interface.

The GPU partitions RGB into `16^3` cells. Each cell covers 16 byte values per channel, with closed bounds `low = cell_coordinate * 16` and `high = low + 15`. The final query is not rounded. A cell stores a certified single answer or up to 64 packed candidate colors:

```text
palette --> GPU palette packing --> GPU cell certification
                                              |
post-dither RGB --> cell address --> one candidate: certified answer
                                --> 2..64: exact candidate scan
                                --> overflow: exact full-palette scan
```

The constructor selects the nearest color `a` at the integer cell center and compares every other color `b` against it. The squared-distance difference is affine:

```text
D_b(x) - D_a(x) = 2 x dot (a - b) + |b|^2 - |a|^2
```

Its exact minimum over the cell occurs at the corner chosen independently on each axis: use `low` where `a >= b`, otherwise `high`. A competitor is excluded only when that minimum is positive, or zero and `a` has the lower original index. The center winner is always retained. A remaining single candidate therefore wins throughout the cell; retained boundary candidates preserve every possible winner. Unlike [gridlut](gridlut.md#method), this bound evaluates a pairwise distance difference rather than comparing separate nearest and farthest box distances.

All distances and signed differences fit in 32-bit integer arithmetic. The candidate scan compares packed distance/index keys four colors at a time, so ties retain the original first index. Short vector tails repeat the final candidate. More than 64 candidates selects a full scan rather than truncating the list.

The provider owns two ordered GPU construction passes and its immutable query table. It uses 1,065,984 bytes: 256 packed palette entries, 4096 rows of 64 packed colors and one count per cell. Construction costs `Theta(C K)` for `C = 4096`; a query costs `Theta(V)` for its retained candidates or `Theta(K)` on overflow. These costs do not establish a speed advantage for every palette or image.

Palette count, RGB bytes and device identity determine reuse. Each plan snapshots its palette; replacing it leaves older live plans intact. A binding exclusively leases the plan's workspace. Only successful completion publishes a rebuilt table as reusable; failed construction leaves it unbuilt. The CPU `shared_instance` control does not change this GPU ownership or the table layout.

The injected provider owns its shader, construction, search and cache without depending on another lookup implementation. Shared [PaletteApply execution](../../execution-map/README.md#4-lookup-per-pixel-application-and-output) owns dither admission, transparency, delta and output publication. GPU-only preparation builds no CPU lookup and remains eligible when CPU instance sharing is disabled. Parallel CPU fallback with worker-local lookup uses the legacy adapter. When the caller selects or falls back to CPU, the existing CPU certlut guarantee applies; exact GPU results are not a promise that both backends produce identical indexes. The legacy direct application fallback and indexed execution failure rules remain those of their callers.

The implementation is [`lookup-policy-certlut-gpu.c`](../../../../src/lookup-policy-certlut-gpu.c), with private kernels in [`lookup-policy-certlut-gpu-shader.inc.h`](../../../../src/lookup-policy-certlut-gpu-shader.inc.h).

## Name and sources

`certlut` is a libsixel descriptive contraction of "certified lookup table." It is not the established name of a published algorithm. The cube test is a libsixel design built from squared-distance algebra and Cauchy-Schwarz. The name describes the design intent; it must not be treated as evidence that the CPU second-nearest-only test is a complete proof.

The float32 index follows the kd-tree introduced by Jon Louis Bentley in [“Multidimensional Binary Search Trees Used for Associative Searching”](https://doi.org/10.1145/361002.361007). The libsixel policy was introduced by the [`certlut` implementation commit](https://github.com/saitoha/libsixel/commit/17f3deea418e2d947d72995a0ae00fd9ce1c8d11).

The CPU implementation is [`lookup-policy-certlut.c`](../../../../src/lookup-policy-certlut.c).

## Test coverage

<!-- test-coverage: enforced -->

The GPU cases fix one input configuration and observation per wrapper. The no-dither oracle is an independent integer palette scan; dithered and rectangle results use CPU exhaustive lookup with the same explicit palette and dither configuration. They do not use CPU certlut as the exact reference.

| ID | Contract | Owning test |
| --- | --- | --- |
| CERTGPU-01 | Palette snapshot. | [tests/lookup/certlut/0038_gpu_palette_snapshot.t](../../../../tests/lookup/certlut/0038_gpu_palette_snapshot.t), [tests/lookup/certlut/0038_gpu_palette_snapshot.c](../../../../tests/lookup/certlut/0038_gpu_palette_snapshot.c) |
| CERTGPU-02 | Cache reuse. | [tests/lookup/certlut/0039_gpu_cache_reuse.t](../../../../tests/lookup/certlut/0039_gpu_cache_reuse.t), [tests/lookup/certlut/0039_gpu_cache_reuse.c](../../../../tests/lookup/certlut/0039_gpu_cache_reuse.c) |
| CERTGPU-03 | Device identity. | [tests/lookup/certlut/0040_gpu_device_identity.t](../../../../tests/lookup/certlut/0040_gpu_device_identity.t), [tests/lookup/certlut/0040_gpu_device_identity.c](../../../../tests/lookup/certlut/0040_gpu_device_identity.c) |
| CERTGPU-04 | Live plan isolation. | [tests/lookup/certlut/0041_gpu_live_plan_isolation.t](../../../../tests/lookup/certlut/0041_gpu_live_plan_isolation.t), [tests/lookup/certlut/0041_gpu_live_plan_isolation.c](../../../../tests/lookup/certlut/0041_gpu_live_plan_isolation.c) |
| CERTGPU-05 | Busy lease. | [tests/lookup/certlut/0042_gpu_busy_lease.t](../../../../tests/lookup/certlut/0042_gpu_busy_lease.t), [tests/lookup/certlut/0042_gpu_busy_lease.c](../../../../tests/lookup/certlut/0042_gpu_busy_lease.c) |
| CERTGPU-06 | Completed reuse. | [tests/lookup/certlut/0043_gpu_completed_reuse.t](../../../../tests/lookup/certlut/0043_gpu_completed_reuse.t), [tests/lookup/certlut/0043_gpu_completed_reuse.c](../../../../tests/lookup/certlut/0043_gpu_completed_reuse.c) |
| CERTGPU-07 | Aborted rebuild. | [tests/lookup/certlut/0044_gpu_aborted_rebuild.t](../../../../tests/lookup/certlut/0044_gpu_aborted_rebuild.t), [tests/lookup/certlut/0044_gpu_aborted_rebuild.c](../../../../tests/lookup/certlut/0044_gpu_aborted_rebuild.c) |
| CERTGPU-08 | Double finish. | [tests/lookup/certlut/0045_gpu_double_finish.t](../../../../tests/lookup/certlut/0045_gpu_double_finish.t), [tests/lookup/certlut/0045_gpu_double_finish.c](../../../../tests/lookup/certlut/0045_gpu_double_finish.c) |
| CERTGPU-09 | Finished view. | [tests/lookup/certlut/0046_gpu_finished_view.t](../../../../tests/lookup/certlut/0046_gpu_finished_view.t), [tests/lookup/certlut/0046_gpu_finished_view.c](../../../../tests/lookup/certlut/0046_gpu_finished_view.c) |
| CERTGPU-10 | Provider lifetime. | [tests/lookup/certlut/0047_gpu_provider_lifetime.t](../../../../tests/lookup/certlut/0047_gpu_provider_lifetime.t), [tests/lookup/certlut/0047_gpu_provider_lifetime.c](../../../../tests/lookup/certlut/0047_gpu_provider_lifetime.c) |
| CERTGPU-11 | Binding lifetime. | [tests/lookup/certlut/0048_gpu_binding_lifetime.t](../../../../tests/lookup/certlut/0048_gpu_binding_lifetime.t), [tests/lookup/certlut/0048_gpu_binding_lifetime.c](../../../../tests/lookup/certlut/0048_gpu_binding_lifetime.c) |
| CERTGPU-12 | Float decline. | [tests/lookup/certlut/0049_gpu_float_decline.t](../../../../tests/lookup/certlut/0049_gpu_float_decline.t), [tests/lookup/certlut/0049_gpu_float_decline.c](../../../../tests/lookup/certlut/0049_gpu_float_decline.c) |
| CERTGPU-13 | No-dither indexes match the integer first-index oracle at 1 colors. | [tests/lookup/certlut/0050_gpu_exact_0_0_1.t](../../../../tests/lookup/certlut/0050_gpu_exact_0_0_1.t), [tests/lookup/certlut/0050_gpu_exact_0_0_1.c](../../../../tests/lookup/certlut/0050_gpu_exact_0_0_1.c) |
| CERTGPU-14 | No-dither indexes match the integer first-index oracle at 2 colors. | [tests/lookup/certlut/0051_gpu_exact_0_0_2.t](../../../../tests/lookup/certlut/0051_gpu_exact_0_0_2.t), [tests/lookup/certlut/0051_gpu_exact_0_0_2.c](../../../../tests/lookup/certlut/0051_gpu_exact_0_0_2.c) |
| CERTGPU-15 | No-dither indexes match the integer first-index oracle at 7 colors. | [tests/lookup/certlut/0052_gpu_exact_0_0_7.t](../../../../tests/lookup/certlut/0052_gpu_exact_0_0_7.t), [tests/lookup/certlut/0052_gpu_exact_0_0_7.c](../../../../tests/lookup/certlut/0052_gpu_exact_0_0_7.c) |
| CERTGPU-16 | No-dither indexes match the integer first-index oracle at 16 colors. | [tests/lookup/certlut/0053_gpu_exact_0_0_16.t](../../../../tests/lookup/certlut/0053_gpu_exact_0_0_16.t), [tests/lookup/certlut/0053_gpu_exact_0_0_16.c](../../../../tests/lookup/certlut/0053_gpu_exact_0_0_16.c) |
| CERTGPU-17 | No-dither indexes match the integer first-index oracle at 17 colors. | [tests/lookup/certlut/0054_gpu_exact_0_0_17.t](../../../../tests/lookup/certlut/0054_gpu_exact_0_0_17.t), [tests/lookup/certlut/0054_gpu_exact_0_0_17.c](../../../../tests/lookup/certlut/0054_gpu_exact_0_0_17.c) |
| CERTGPU-18 | No-dither indexes match the integer first-index oracle at 33 colors. | [tests/lookup/certlut/0055_gpu_exact_0_0_33.t](../../../../tests/lookup/certlut/0055_gpu_exact_0_0_33.t), [tests/lookup/certlut/0055_gpu_exact_0_0_33.c](../../../../tests/lookup/certlut/0055_gpu_exact_0_0_33.c) |
| CERTGPU-19 | No-dither indexes match the integer first-index oracle at 64 colors. | [tests/lookup/certlut/0056_gpu_exact_0_0_64.t](../../../../tests/lookup/certlut/0056_gpu_exact_0_0_64.t), [tests/lookup/certlut/0056_gpu_exact_0_0_64.c](../../../../tests/lookup/certlut/0056_gpu_exact_0_0_64.c) |
| CERTGPU-20 | No-dither indexes match the integer first-index oracle at 129 colors. | [tests/lookup/certlut/0057_gpu_exact_0_0_129.t](../../../../tests/lookup/certlut/0057_gpu_exact_0_0_129.t), [tests/lookup/certlut/0057_gpu_exact_0_0_129.c](../../../../tests/lookup/certlut/0057_gpu_exact_0_0_129.c) |
| CERTGPU-21 | No-dither indexes match the integer first-index oracle at 256 colors. | [tests/lookup/certlut/0058_gpu_exact_0_0_256.t](../../../../tests/lookup/certlut/0058_gpu_exact_0_0_256.t), [tests/lookup/certlut/0058_gpu_exact_0_0_256.c](../../../../tests/lookup/certlut/0058_gpu_exact_0_0_256.c) |
| CERTGPU-22 | Blue-noise indexes match CPU exhaustive palette application at 64 colors. | [tests/lookup/certlut/0059_gpu_exact_1_0_64.t](../../../../tests/lookup/certlut/0059_gpu_exact_1_0_64.t), [tests/lookup/certlut/0059_gpu_exact_1_0_64.c](../../../../tests/lookup/certlut/0059_gpu_exact_1_0_64.c) |
| CERTGPU-23 | X-dither indexes match CPU exhaustive palette application at 33 colors. | [tests/lookup/certlut/0060_gpu_exact_2_0_33.t](../../../../tests/lookup/certlut/0060_gpu_exact_2_0_33.t), [tests/lookup/certlut/0060_gpu_exact_2_0_33.c](../../../../tests/lookup/certlut/0060_gpu_exact_2_0_33.c) |
| CERTGPU-24 | A-dither indexes match CPU exhaustive palette application at 33 colors. | [tests/lookup/certlut/0061_gpu_exact_4_0_33.t](../../../../tests/lookup/certlut/0061_gpu_exact_4_0_33.t), [tests/lookup/certlut/0061_gpu_exact_4_0_33.c](../../../../tests/lookup/certlut/0061_gpu_exact_4_0_33.c) |
| CERTGPU-25 | GPU force omits CPU lookup and matches an exact-reference encoded stream. | [tests/lookup/certlut/0062_gpu_force_no_cpu.t](../../../../tests/lookup/certlut/0062_gpu_force_no_cpu.t), [tests/lookup/certlut/0062_gpu_force_no_cpu.c](../../../../tests/lookup/certlut/0062_gpu_force_no_cpu.c) |
| CERTGPU-26 | GPU auto omits CPU lookup and matches an exact-reference encoded stream. | [tests/lookup/certlut/0063_gpu_auto_no_cpu.t](../../../../tests/lookup/certlut/0063_gpu_auto_no_cpu.t), [tests/lookup/certlut/0063_gpu_auto_no_cpu.c](../../../../tests/lookup/certlut/0063_gpu_auto_no_cpu.c) |
| CERTGPU-27 | Offset rectangle indexes match exact CPU application. | [tests/lookup/certlut/0064_gpu_rectangle_indices.t](../../../../tests/lookup/certlut/0064_gpu_rectangle_indices.t), [tests/lookup/certlut/0064_gpu_rectangle_indices.c](../../../../tests/lookup/certlut/0064_gpu_rectangle_indices.c) |
| CERTGPU-28 | Offset rectangle delta masks match CPU application. | [tests/lookup/certlut/0065_gpu_rectangle_mask.t](../../../../tests/lookup/certlut/0065_gpu_rectangle_mask.t), [tests/lookup/certlut/0065_gpu_rectangle_mask.c](../../../../tests/lookup/certlut/0065_gpu_rectangle_mask.c) |
| CERTGPU-29 | Blue-noise offset rectangle indexes match exact CPU application. | [tests/lookup/certlut/0066_gpu_blue_rectangle_indices.t](../../../../tests/lookup/certlut/0066_gpu_blue_rectangle_indices.t), [tests/lookup/certlut/0066_gpu_blue_rectangle_indices.c](../../../../tests/lookup/certlut/0066_gpu_blue_rectangle_indices.c) |
| CERTGPU-30 | Blue-noise offset rectangle delta masks match CPU application. | [tests/lookup/certlut/0067_gpu_blue_rectangle_mask.t](../../../../tests/lookup/certlut/0067_gpu_blue_rectangle_mask.t), [tests/lookup/certlut/0067_gpu_blue_rectangle_mask.c](../../../../tests/lookup/certlut/0067_gpu_blue_rectangle_mask.c) |
| CERTGPU-31 | Table reuse. | [tests/lookup/certlut/0068_gpu_table_reuse.t](../../../../tests/lookup/certlut/0068_gpu_table_reuse.t), [tests/lookup/certlut/0068_gpu_table_reuse.c](../../../../tests/lookup/certlut/0068_gpu_table_reuse.c) |
| CERTGPU-32 | Palette rebuild. | [tests/lookup/certlut/0069_gpu_palette_rebuild.t](../../../../tests/lookup/certlut/0069_gpu_palette_rebuild.t), [tests/lookup/certlut/0069_gpu_palette_rebuild.c](../../../../tests/lookup/certlut/0069_gpu_palette_rebuild.c) |
| CERTGPU-33 | Count rebuild. | [tests/lookup/certlut/0070_gpu_count_rebuild.t](../../../../tests/lookup/certlut/0070_gpu_count_rebuild.t), [tests/lookup/certlut/0070_gpu_count_rebuild.c](../../../../tests/lookup/certlut/0070_gpu_count_rebuild.c) |
| CERTGPU-34 | Failed build recovery. | [tests/lookup/certlut/0071_gpu_failed_build_recovery.t](../../../../tests/lookup/certlut/0071_gpu_failed_build_recovery.t), [tests/lookup/certlut/0071_gpu_failed_build_recovery.c](../../../../tests/lookup/certlut/0071_gpu_failed_build_recovery.c) |
| CERTGPU-35 | Index255. | [tests/lookup/certlut/0072_gpu_index255.t](../../../../tests/lookup/certlut/0072_gpu_index255.t), [tests/lookup/certlut/0072_gpu_index255.c](../../../../tests/lookup/certlut/0072_gpu_index255.c) |
| CERTGPU-36 | First index tie. | [tests/lookup/certlut/0073_gpu_first_index_tie.t](../../../../tests/lookup/certlut/0073_gpu_first_index_tie.t), [tests/lookup/certlut/0073_gpu_first_index_tie.c](../../../../tests/lookup/certlut/0073_gpu_first_index_tie.c) |
| CERTGPU-37 | Queries at cell boundaries and byte endpoints match an independent scan. | [tests/lookup/certlut/0074_gpu_cell_boundaries.t](../../../../tests/lookup/certlut/0074_gpu_cell_boundaries.t), [tests/lookup/certlut/0074_gpu_cell_boundaries.c](../../../../tests/lookup/certlut/0074_gpu_cell_boundaries.c) |
| CERTGPU-38 | A third-center-neighbor winner is retained in the CPU heuristic counterexample. | [tests/lookup/certlut/0075_gpu_counterexample.t](../../../../tests/lookup/certlut/0075_gpu_counterexample.t), [tests/lookup/certlut/0075_gpu_counterexample.c](../../../../tests/lookup/certlut/0075_gpu_counterexample.c) |
| CERTGPU-39 | A cell containing 256 distinct winning colors falls back without truncation. | [tests/lookup/certlut/0076_gpu_candidate_overflow.t](../../../../tests/lookup/certlut/0076_gpu_candidate_overflow.t), [tests/lookup/certlut/0076_gpu_candidate_overflow.c](../../../../tests/lookup/certlut/0076_gpu_candidate_overflow.c) |
| CERTGPU-40 | A lower-index competitor tying only at the far cell face is retained. | [tests/lookup/certlut/0077_gpu_boundary_lower_tie.t](../../../../tests/lookup/certlut/0077_gpu_boundary_lower_tie.t), [tests/lookup/certlut/0077_gpu_boundary_lower_tie.c](../../../../tests/lookup/certlut/0077_gpu_boundary_lower_tie.c) |
| CERTGPU-41 | A lower-index competitor tying only at the far cell corner is retained. | [tests/lookup/certlut/0078_gpu_corner_lower_tie.t](../../../../tests/lookup/certlut/0078_gpu_corner_lower_tie.t), [tests/lookup/certlut/0078_gpu_corner_lower_tie.c](../../../../tests/lookup/certlut/0078_gpu_corner_lower_tie.c) |
| CERTGPU-42 | A failed allocation publishes no output and the next invocation rebuilds. | [tests/lookup/certlut/0079_gpu_allocation_recovery.t](../../../../tests/lookup/certlut/0079_gpu_allocation_recovery.t), [tests/lookup/certlut/0079_gpu_allocation_recovery.c](../../../../tests/lookup/certlut/0079_gpu_allocation_recovery.c) |
| CERTGPU-43 | A failed command encoder publishes no output and the next invocation rebuilds. | [tests/lookup/certlut/0080_gpu_encoder_recovery.t](../../../../tests/lookup/certlut/0080_gpu_encoder_recovery.t), [tests/lookup/certlut/0080_gpu_encoder_recovery.c](../../../../tests/lookup/certlut/0080_gpu_encoder_recovery.c) |
| CERTGPU-44 | Legacy direct AUTO failure falls back to CPU certlut and its CPU result. | [tests/lookup/certlut/0081_gpu_auto_failure_cpu_lookup.t](../../../../tests/lookup/certlut/0081_gpu_auto_failure_cpu_lookup.t), [tests/lookup/certlut/0081_gpu_auto_failure_cpu_lookup.c](../../../../tests/lookup/certlut/0081_gpu_auto_failure_cpu_lookup.c) |
| CERTGPU-45 | GPU off retains CPU certlut preparation and output. | [tests/lookup/certlut/0082_gpu_off_cpu_lookup.t](../../../../tests/lookup/certlut/0082_gpu_off_cpu_lookup.t), [tests/lookup/certlut/0082_gpu_off_cpu_lookup.c](../../../../tests/lookup/certlut/0082_gpu_off_cpu_lookup.c) |
| CERTGPU-46 | An unsupported diffusion mode retains CPU certlut preparation and output. | [tests/lookup/certlut/0083_gpu_diffusion_cpu_lookup.t](../../../../tests/lookup/certlut/0083_gpu_diffusion_cpu_lookup.t), [tests/lookup/certlut/0083_gpu_diffusion_cpu_lookup.c](../../../../tests/lookup/certlut/0083_gpu_diffusion_cpu_lookup.c) |

Lifecycle and admission tests use a fake device and run without Metal. Actual GPU cases skip only when the Metal palette backend is unavailable. The state and certificate fixtures verify the injected kernel, so a legacy direct GPU scan cannot satisfy them. Tests cover bounded deterministic samples, duplicate colors, short vectors, overflow and failed construction; they do not enumerate the full RGB cube, establish perceptual quality, or simulate operating-system device resets. CPU float and legacy byte behavior retain the existing coverage in the [parent reference](README.md).
