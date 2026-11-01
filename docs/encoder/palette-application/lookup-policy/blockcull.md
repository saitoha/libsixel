# Lookup policy: `blockcull`

This page describes `--lookup-policy=blockcull` for CLI users and library contributors. The [lookup policy reference](README.md) owns the shared interface, metric and guarantee terminology. `auto` continues to select `vptree`.

## Purpose and intuition

Neighboring pixels usually have similar colors. A group of samples therefore occupies a small axis-aligned box in color space, and most palette colors cannot be the nearest color of any point in that box. `blockcull` removes those colors once per group and scans only the survivors for each sample.

```text
group of samples ──> bounding box [low, high]
                            │
palette ──> far(c): largest distance from c to the box ──> U = min far(c)
        └─> near(c): smallest distance from c to the box ──> keep c if near(c) <= U
                            │
each sample ──> palette-order scan over the kept colors ──> first minimum
```

The method relies on spatial coherence in the image, not on a search structure built from the palette. Its main use is the Metal palette-application path, where a block of pixels shares one culling pass; the CPU implementation is a reference for the same result.

## Method

For a box with per-channel bounds `low[d]` and `high[d]` and the policy distance `D(x, c) = sum_d w[d] * (x[d] - c[d])^2`:

```text
far(c)  = sum_d w[d] * max(c[d] - low[d], high[d] - c[d])^2
near(c) = sum_d w[d] * max(low[d] - c[d], c[d] - high[d], 0)^2
U       = min_c far(c)
```

Every sample `x` in the box satisfies `D(x, c) <= far(c)` and `D(x, c) >= near(c)`. Let `u` be a color with `far(u) = U`. If `near(c) > U`, then `D(x, c) > U >= D(x, u)` for every sample in the box, so `c` is strictly farther than `u` and can neither win nor tie the minimum. Survivors keep palette order, so the strict first-minimum scan over them returns the exhaustive scan's index, including the lowest index among equal distances. The color attaining `U` always survives, so the survivor list is never empty.

The CPU bulk path forms groups of up to 32 consecutive samples in the order the caller supplies them, in either scan direction. A single-sample lookup has no group to cull over and scans the whole palette. Error diffusion changes each sample after the previous decision, so diffusion paths use single-sample lookup; `blockcull` then performs the same palette-order scan as an exhaustive search.

| Representation | Distance | Culling test |
| --- | --- | --- |
| 8bit | Unweighted integer squared RGB distance | Exact integer comparison `near(c) <= U` |
| float32 | The VP-tree float metric: `w[d] = 1 / range[d]^2`, double differences, channel order, accumulated from zero | `near(c) <= U * (1 + 1e-9)` |

The float slack exceeds the relative rounding error of the computed distances, so a skipped color is strictly farther in the computed metric as well. A group containing a nonfinite sample, a nonfinite bound, or a palette or weight that is not finite skips culling and scans the whole palette. Palettes larger than 256 entries are rejected at preparation.

## Metal palette application

On the Metal palette-application path, one simdgroup handles an 8-by-8 pixel block. Each lane forms the queries of two block pixels after positional dither noise, transparency and 6delta keeps; finished pixels do not widen the box. The lanes then compute `U` and the survivor list cooperatively and place the survivors, in palette order, into threadgroup matrix tiles of the same `|c|^2 - 2 q·c` form that the [`none` GPU kernel](none.md) uses. Byte coordinates keep every score an integer below `2^24`, so float matrix products are exact.

| Condition | Kernel |
| --- | --- |
| More than 32 colors, survivors within the 128-color capacity | Block culling, then matrix products over the survivor tiles |
| More than 128 survivors in a block | The same kernel scans the full palette tiles |
| At most 32 colors | The exhaustive matrix-product kernel, which costs less than culling at that size |
| Device without simdgroup matrices | The exhaustive per-pixel scan |

All four kernels return the same indexes. GPU admission follows the existing Metal palette-application rules for `none`: 8-bit RGB samples, no error diffusion, and positional or blue-noise dithering. Other requests use the CPU policy. With `SIXEL_TRACE_TOPIC=gpu_contract`, `LSXGPU2|consumer=palette-apply|kernel=...` names the kernel that ran.

## Guarantee

`blockcull` is scan-index exact in its representation's metric. The 8-bit class returns the same index as the `none` 8-bit scan, and the float32 class returns the same index as an exhaustive scan in the VP-tree float metric. Float arithmetic follows the [isolated noncontracting build gate](vptree.md#float-bulk-lookup) in split builds; amalgamation retains native scalar arithmetic.

Equal lookup results do not make end-to-end output identical to `--lookup-policy=none`. `none` also changes stages outside lookup, such as the [Heckbert histogram resolution](README.md#precedence-and-interactions), while `blockcull` behaves there like the other accelerated policies.

Prepared state is an immutable palette copy. Preparation does not consume reuse hints, and each worker owns its instance.

## Cost

Preparation copies the palette: `Theta(K)` time and space. A single lookup is `Theta(K)`. A group of `S` samples costs `Theta(K)` for the bounds plus `Theta(S V)` for the scans, where `V` is the number of survivors. `V` depends on the group's color spread: smooth regions keep few colors, while noise can keep the whole palette, in which case culling adds work to an exhaustive scan.

The CPU bulk path groups samples along a row only, which limits how small the boxes become. Exact tree and graph policies such as [`vptree`](vptree.md) and [`delaunay`](delaunay.md) are generally faster on the CPU. The square 8-by-8 blocks of the Metal kernel are more coherent, and culling there reduces the matrix-product work that dominates large palettes. No checked-in measurement covers `blockcull` yet.

## Implementation map

[`lookup-policy-blockcull.c`](../../../../src/lookup-policy-blockcull.c) owns preparation and the 8-bit class through `lookup/blockcull.8bit` and `lookup/blockcull.float32`. [`lookup-blockcull-float32.c`](../../../../src/lookup-blockcull-float32.c) owns the float bounds and scans inside the float lookup build gate. The Metal kernel `sixel_gpu_palette_apply_blockcull` and its dispatch are in [`gpu-palette-metal.m`](../../../../src/gpu-palette-metal.m).

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| BC-01 | Byte bulk lookup equals an independent palette-order integer scan for palettes of 1 to 256 colors with duplicates and equidistant midpoints, every run length up to 70 and a long run, in both scan directions. | [tests/processing/lookup/0035_blockcull_byte_exact.t](../../../../tests/processing/lookup/0035_blockcull_byte_exact.t), [tests/processing/lookup/0035_blockcull_exact.c](../../../../tests/processing/lookup/0035_blockcull_exact.c) |
| BC-02 | Float bulk lookup equals an independent double-distance scan, including runs with NaN, infinite and far out-of-range samples, and equals the VP-tree float metric's index for finite samples. | [tests/processing/lookup/0036_blockcull_float_exact.t](../../../../tests/processing/lookup/0036_blockcull_float_exact.t) |
| BC-03 | Both classes satisfy the shared bulk contract: ordered results equal independent point lookups for cold and repeated calls, partial runs and reverse traversal. | [tests/processing/lookup/0015_lookup_batch_contract.c](../../../../tests/processing/lookup/0015_lookup_batch_contract.c) |
| BC-04 | Metal blockcull returns the exhaustive index and the CPU keep mask for partial 8-by-8 blocks, smooth and noisy rows that exceed the survivor capacity, duplicate and equidistant colors and interleaved 6delta keeps, and requires the matrix kernel when the device supports simdgroup matrices, otherwise the direct kernel. | [tests/processing/gpu-palette/0021_blockcull_exact.t](../../../../tests/processing/gpu-palette/0021_blockcull_exact.t), [tests/processing/gpu-palette/0021_blockcull_exact.c](../../../../tests/processing/gpu-palette/0021_blockcull_exact.c) |
| BC-05 | Native gamma float lookup retains extended coordinates. | [tests/processing/lookup/blockcull/0001_blockcull_gamma_range.t](../../../../tests/processing/lookup/blockcull/0001_blockcull_gamma_range.t) |
| BC-06 | Native linear float lookup retains extended coordinates. | [tests/processing/lookup/blockcull/0002_blockcull_linear_range.t](../../../../tests/processing/lookup/blockcull/0002_blockcull_linear_range.t) |
| BC-07 | Native oklab float lookup retains extended coordinates. | [tests/processing/lookup/blockcull/0003_blockcull_oklab_range.t](../../../../tests/processing/lookup/blockcull/0003_blockcull_oklab_range.t) |
| BC-08 | Native cielab float lookup retains extended coordinates. | [tests/processing/lookup/blockcull/0004_blockcull_cielab_range.t](../../../../tests/processing/lookup/blockcull/0004_blockcull_cielab_range.t) |
| BC-09 | Native din99d float lookup retains extended coordinates. | [tests/processing/lookup/blockcull/0005_blockcull_din99d_range.t](../../../../tests/processing/lookup/blockcull/0005_blockcull_din99d_range.t) |
| BC-10 | Native gamma float lookup distinguishes fractional samples. | [tests/processing/lookup/blockcull/0006_blockcull_gamma_fraction.t](../../../../tests/processing/lookup/blockcull/0006_blockcull_gamma_fraction.t) |
| BC-11 | Native linear float lookup distinguishes fractional samples. | [tests/processing/lookup/blockcull/0007_blockcull_linear_fraction.t](../../../../tests/processing/lookup/blockcull/0007_blockcull_linear_fraction.t) |
| BC-12 | Native oklab float lookup distinguishes fractional samples. | [tests/processing/lookup/blockcull/0008_blockcull_oklab_fraction.t](../../../../tests/processing/lookup/blockcull/0008_blockcull_oklab_fraction.t) |
| BC-13 | Native cielab float lookup distinguishes fractional samples. | [tests/processing/lookup/blockcull/0009_blockcull_cielab_fraction.t](../../../../tests/processing/lookup/blockcull/0009_blockcull_cielab_fraction.t) |
| BC-14 | Native din99d float lookup distinguishes fractional samples. | [tests/processing/lookup/blockcull/0010_blockcull_din99d_fraction.t](../../../../tests/processing/lookup/blockcull/0010_blockcull_din99d_fraction.t) |
| BC-15 | Native gamma float lookup matches the independent metric oracle. | [tests/processing/lookup/blockcull/0011_blockcull_gamma_oracle.t](../../../../tests/processing/lookup/blockcull/0011_blockcull_gamma_oracle.t) |
| BC-16 | Native linear float lookup matches the independent metric oracle. | [tests/processing/lookup/blockcull/0012_blockcull_linear_oracle.t](../../../../tests/processing/lookup/blockcull/0012_blockcull_linear_oracle.t) |
| BC-17 | Native oklab float lookup matches the independent metric oracle. | [tests/processing/lookup/blockcull/0013_blockcull_oklab_oracle.t](../../../../tests/processing/lookup/blockcull/0013_blockcull_oklab_oracle.t) |
| BC-18 | Native cielab float lookup matches the independent metric oracle. | [tests/processing/lookup/blockcull/0014_blockcull_cielab_oracle.t](../../../../tests/processing/lookup/blockcull/0014_blockcull_cielab_oracle.t) |
| BC-19 | Native din99d float lookup matches the independent metric oracle. | [tests/processing/lookup/blockcull/0015_blockcull_din99d_oracle.t](../../../../tests/processing/lookup/blockcull/0015_blockcull_din99d_oracle.t) |
| BC-20 | Large gamma palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0016_blockcull_gamma_oracle_large.t](../../../../tests/processing/lookup/blockcull/0016_blockcull_gamma_oracle_large.t) |
| BC-21 | Large linear palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0017_blockcull_linear_oracle_large.t](../../../../tests/processing/lookup/blockcull/0017_blockcull_linear_oracle_large.t) |
| BC-22 | Large oklab palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0018_blockcull_oklab_oracle_large.t](../../../../tests/processing/lookup/blockcull/0018_blockcull_oklab_oracle_large.t) |
| BC-23 | Large cielab palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0019_blockcull_cielab_oracle_large.t](../../../../tests/processing/lookup/blockcull/0019_blockcull_cielab_oracle_large.t) |
| BC-24 | Large din99d palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0020_blockcull_din99d_oracle_large.t](../../../../tests/processing/lookup/blockcull/0020_blockcull_din99d_oracle_large.t) |
| BC-25 | Small gamma palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0021_blockcull_gamma_oracle_small.t](../../../../tests/processing/lookup/blockcull/0021_blockcull_gamma_oracle_small.t) |
| BC-26 | Small linear palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0022_blockcull_linear_oracle_small.t](../../../../tests/processing/lookup/blockcull/0022_blockcull_linear_oracle_small.t) |
| BC-27 | Small oklab palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0023_blockcull_oklab_oracle_small.t](../../../../tests/processing/lookup/blockcull/0023_blockcull_oklab_oracle_small.t) |
| BC-28 | Small cielab palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0024_blockcull_cielab_oracle_small.t](../../../../tests/processing/lookup/blockcull/0024_blockcull_cielab_oracle_small.t) |
| BC-29 | Small din99d palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0025_blockcull_din99d_oracle_small.t](../../../../tests/processing/lookup/blockcull/0025_blockcull_din99d_oracle_small.t) |
| BC-30 | Mixed-range gamma palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0026_blockcull_gamma_oracle_mixed.t](../../../../tests/processing/lookup/blockcull/0026_blockcull_gamma_oracle_mixed.t) |
| BC-31 | Mixed-range linear palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0027_blockcull_linear_oracle_mixed.t](../../../../tests/processing/lookup/blockcull/0027_blockcull_linear_oracle_mixed.t) |
| BC-32 | Mixed-range oklab palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0028_blockcull_oklab_oracle_mixed.t](../../../../tests/processing/lookup/blockcull/0028_blockcull_oklab_oracle_mixed.t) |
| BC-33 | Mixed-range cielab palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0029_blockcull_cielab_oracle_mixed.t](../../../../tests/processing/lookup/blockcull/0029_blockcull_cielab_oracle_mixed.t) |
| BC-34 | Mixed-range din99d palettes match the independent metric oracle. | [tests/processing/lookup/blockcull/0030_blockcull_din99d_oracle_mixed.t](../../../../tests/processing/lookup/blockcull/0030_blockcull_din99d_oracle_mixed.t) |
| BC-35 | cielab decisions follow the normalized channel metric. | [tests/processing/lookup/blockcull/0031_blockcull_cielab_metric.t](../../../../tests/processing/lookup/blockcull/0031_blockcull_cielab_metric.t) |
| BC-36 | din99d decisions follow the normalized channel metric. | [tests/processing/lookup/blockcull/0032_blockcull_din99d_metric.t](../../../../tests/processing/lookup/blockcull/0032_blockcull_din99d_metric.t) |
| BC-37 | Preparing one float lookup instance does not change another. | [tests/processing/lookup/blockcull/0033_blockcull_instance_isolation.t](../../../../tests/processing/lookup/blockcull/0033_blockcull_instance_isolation.t) |

BC-04 skips when the Metal palette backend is unavailable. The tests establish index equality, not speed; they do not cover the per-call cost or the crossover between culling and exhaustive scans.

The native-float cases below the byte contracts cover five colorspaces, fractional and extended coordinates, several palette sizes, independent metric decisions and instance isolation. GPU selection assertions use device capability independently of pipeline compilation; unavailable devices skip rather than count as matrix-kernel evidence.
