# Lookup policy: `5bit`

This page describes the `5bit` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

On the CPU, `5bit` partitions byte-valued RGB space into `32^3` buckets. The first serial query entering an empty bucket performs a full palette scan. The resulting index is then reused for later colors in the same bucket.

```text
8-bit RGB candidate
       |
       v
round each channel to 5 address bits
       |
       v
32 x 32 x 32 bucket table
       |
       +-- filled --> return cached index
       |
       +-- empty  --> scan palette --> store and return index
```

The name describes address precision, not palette precision. Palette entries and the first candidate remain 8-bit values.

## Method

For a byte channel `v`, libsixel rounds and saturates:

```text
q5(v) = min(31, floor((v + 4) / 8))
bucket(x) = pack(q5(x.r), q5(x.g), q5(x.b))
```

There are:

```text
G = 2^(5 * 3) = 32,768 buckets
```

The CPU table stores one signed 32-bit palette index per bucket, approximately 128 KiB before allocator overhead.

Reducing one byte channel from 256 values to 32 address levels groups eight adjacent values together. Across RGB this reduces `256^3 = 16,777,216` possible tuples to `32^3 = 32,768` cells. A bucket boundary can cross a palette Voronoi boundary, and a histogram cell can erase distinctions among colors that should influence different median-cut boxes. The error is therefore not only a memory-capacity question.

The documented `SIXEL_LOOKUP_PACKING` values are `linear` and `morton`. Linear packing concatenates channel fields. Morton packing interleaves their bits to improve spatial locality for some access patterns. The implementation also recognizes `hilbert`, but that value is not currently part of the public manual contract. Packing changes memory order, not which colors share a bucket.

## Guarantee

On an empty bucket, the CPU policy scans the actual candidate against all palette entries, so that answer is scan-index exact in byte RGB distance. Afterwards every candidate with the same `bucket(x)` receives the stored index:

```text
lookup(x_first) = argmin_i D(i, x_first)
lookup(x_later) = lookup(x_first), if bucket(x_later) = bucket(x_first)
```

The later answer can be approximate, and it depends on traversal history. The maximum component separation inside one rounded bucket is small, but a palette Voronoi boundary can still cross the bucket.

Serial, worker-local parallel (`shared_instance=0`), and shared parallel (`shared_instance=1`) 8-bit paths all populate their dense tables. With a shared instance, multiple workers can observe the same empty bucket, scan different candidate colors, and store different valid palette indices; whichever write becomes visible determines the cached representative, so repeated runs need not be byte-reproducible. Relaxed atomic loads and stores make those accesses well-defined without ordering surrounding work. The float32 backend still performs an exhaustive scan using the normalized weighted metric described in the index.

## GPU application

The Metal and Vulkan backends eagerly build all `32^3` entries at palette application. Each cell stores the nearest actual palette entry to the center of the byte values mapped to that cell by `q5`. The palette itself is not snapped to the grid. Distances use doubled integer coordinates so half-byte centers are represented exactly; equal distances choose the lowest palette index. Edge cells use their actual clipped byte intervals.

| Backend | Cell representative | Population | Query |
| --- | --- | --- | --- |
| CPU | First observed query | Lazy palette scan | Cached index, possibly traversal-dependent |
| Metal / Vulkan | Center of the cell's integer byte interval | Complete table before application | One byte-table read, independent of traversal |

Both are approximate for an arbitrary query inside a cell. CPU and GPU output can differ even with the same palette and dither settings; this path does not promise index equivalence to CPU memoization or exhaustive `none` lookup.

For Metal byte-query dithers, `5bit` owns a process-global table guarded by the Metal dispatch mutex. Its builder, query shader and cache are independent of `6bit`; alternating policies preserves both completed tables. GPU packing is linear; the CPU `shared_instance` and `packing` suboptions do not change this behavior. The table contains 32 KiB of entries, with palette index 255 available as an ordinary value for 256-color palettes. Reuse requires identical palette count and ordered palette bytes. Geometry and dither parameters do not affect table identity.

Construction and application use the same command buffer with an encoder boundary. Cache identity is published only after successful completion. A failed rebuild invalidates the table so a subsequent call rebuilds it; failed application does not publish indexes. A cold application costs `Theta(G K + P)` and a reusable application costs `Theta(P + K)`, where `G = 32^3`. Shader compilation is a separate first-use cost.

RGB888 application supports `none`, arithmetic `a`/`x`, and `bluenoise` dithering, transparency, and eligible partial-rectangle delta comparison. Native float lookup remains CPU-only. The public feature macro `SIXEL_HAVE_GPU_PREBUILT_BIT_LOOKUP` lets clients admit this policy without relying on older libraries that could reinterpret forced unsupported lookup as direct scanning. GPU-only preparation needs the existing lookup kind and builds no CPU memo table; an automatic GPU rejection can still prepare the CPU policy before output.

The Metal implementation is [lookup-policy-5bit-gpu-metal.inc.h](../../../../src/lookup-policy-5bit-gpu-metal.inc.h). The [execution map](../../execution-map/README.md) describes component ownership.

GPU diffusion uses an independent fractional program in [`lookup-policy-5bit-gpu.c`](../../../../src/lookup-policy-5bit-gpu.c) and [`lookup-policy-5bit-fractional-gpu.metal`](../../../../src/lookup-policy-5bit-fractional-gpu.metal). Vulkan byte and fractional queries use the policy-owned [`lookup-policy-5bit-gpu.glsl`](../../../../src/lookup-policy-5bit-gpu.glsl) program. The continuous query selects the rounded `5bit` address directly, without an intermediate RGB8 rounding. Representatives remain the centers of the integer byte intervals, preserving the declared approximation. The table is completely prebuilt; there is no first-query memoization.

A process-wide idle slot belonging only to `5bit` shares completed workspace across preparers for Metal fractional queries and Vulkan byte/fractional queries, independent of CPU `shared_instance` and packing. Active bindings keep exclusive allocations and cannot overwrite another palette's table. Reuse requires matching device, palette count, ordered palette and successful completion. Completed solves return workspace to the slot while a deferred solution may still retain its frozen plan. The slot holds at most one idle 32 KiB allocation; in-flight plans may temporarily hold additional tables. The Metal byte executor retains its own cache. Neither cache can be borrowed, resized or invalidated by `6bit`.

## Cost

For the CPU backend, let `U` be the number of distinct buckets first encountered:

```text
table initialization        Theta(32^3)
cache hit                   Theta(1)
first-bucket miss           Theta(K)
8-bit whole image           Theta(32^3 + P + U K)
float32 query               Theta(K)
```

The `shared_instance` suboption controls whether workers receive a shared instance; its default is enabled for `5bit`. Worker-local instances spend additional initialization and memory per active instance. A shared instance aggregates memoized buckets across workers and can repeat an exhaustive scan when workers miss the same bucket concurrently, while racing first writers make its output schedule-dependent. Sharing is therefore a lifecycle, race, memory, output, and speed policy.

## Name and sources

`5bit` is a libsixel descriptive name derived from the five address bits per channel. It is not named after a paper. The same basic lazy RGB555 design is present in the current [`libsixel/libsixel` fork](https://github.com/libsixel/libsixel/blob/ee77c2f979dbccc86651a482e748fe7bf80bc89c/src/quant.c#L1091-L1139) and comes from shared project lineage. That implementation truncates to the top five bits, whereas the current policy documented here rounds and saturates.

`go-sixel` provides a useful contrast: it also addresses an RGB555 table, but it [fills all cells eagerly using each cell center](https://github.com/mattn/go-sixel/blob/ceaaab1e2b5973d9ebc28b0a615760b921e90681/sixel.go#L676-L712). That makes its representative deterministic for a palette, while the libsixel CPU representative is the first actual query observed in each bucket.

## History

The quantizer used through the 1.8.7 era applied the same RGB555 address to two different jobs. Palette construction counted input colors in a `32^3` histogram and reconstructed every occupied cell from the truncated five-bit coordinates. Palette application then lazily cached the nearest palette index for the first pixel observed in each RGB555 cell. The historical channel map was simply:

```text
q_old(v) = floor(v / 8)
r_old(q) = 8q
```

Consequently, as many as eight input values on each axis became one histogram value, with a per-channel truncation of up to seven byte values before median cut even started. The lazy application cache could then add a second source of error around palette decision boundaries.

The current named `5bit` policy rounds and saturates its lookup address rather than reproducing that old truncating hash exactly. Its Heckbert configuration still selects a five-bit-per-channel histogram, however, so requesting a larger palette cannot recover color distinctions that the histogram already merged. This is why higher `K` does not guarantee that the quality gap will close.

The [move from RGB555 to RGB666 in 2025](https://github.com/saitoha/libsixel/commit/4f57a743c11c4149656cdad6e3f8d59d76c58036), followed by [`none` as a full-precision control](https://github.com/saitoha/libsixel/commit/fafc097994dfcd1af8f17c09a94628ea158281a4) and by several structurally different search policies, grew out of the need to make this speed, memory, and quality tradeoff explicit. Later policies also address float colorspaces, parallelism, and different preparation/query costs; they are not merely cosmetic renamings of the old cache.

The [measured quality and speed comparison](README.md#measured-trade-offs) compares the current controlled K-means command with the historical Heckbert coupling. In its byte Heckbert quality comparison at `K = 256`, `6bit` is closer to `none` than `5bit` is. The controlled K-means curves instead use native-float lookup and therefore do not measure the byte-cache quality or speed tradeoff. Neither experiment alone determines the best default.

The implementation is [`lookup-policy-5bit.c`](../../../../src/lookup-policy-5bit.c). CLI and environment spelling is documented in the [`img2sixel(1)` manual](../../../../converters/img2sixel.1).

## Test coverage

<!-- test-coverage: enforced -->

The bounded byte-ramp oracle independently enumerates each channel's bucket preimage and checks its center against actual palette colors. Cases isolate center values, tie order, the byte-index limit, reuse keys, failure recovery and each supported integration path. Ownership checks use normal indexed encoding; reuse checks use the GPU observation hooks. These tests establish behavior, not perceptual superiority or throughput, and do not exhaust the RGB cube.

| ID | Contract | Test |
| --- | --- | --- |
| GPU-01 | Rounded-cell centers match an independent byte-preimage oracle. | [tests/lookup/5bit/0034_5bit_gpu_centers.t](../../../../tests/lookup/5bit/0034_5bit_gpu_centers.t) |
| GPU-02 | Equal center distances choose the lowest palette index. | [tests/lookup/5bit/0035_5bit_gpu_tie.t](../../../../tests/lookup/5bit/0035_5bit_gpu_tie.t) |
| GPU-03 | Index 255 remains usable with 256 paintable colors. | [tests/lookup/5bit/0036_5bit_gpu_index255.t](../../../../tests/lookup/5bit/0036_5bit_gpu_index255.t) |
| GPU-04 | Active table size is one byte per grid cell. | [tests/lookup/5bit/0037_5bit_gpu_table_bytes.t](../../../../tests/lookup/5bit/0037_5bit_gpu_table_bytes.t) |
| GPU-05 | Unchanged palette reuses the table. | [tests/lookup/5bit/0038_5bit_gpu_reuse.t](../../../../tests/lookup/5bit/0038_5bit_gpu_reuse.t) |
| GPU-06 | Changed palette bytes rebuild the table. | [tests/lookup/5bit/0039_5bit_gpu_palette_rebuild.t](../../../../tests/lookup/5bit/0039_5bit_gpu_palette_rebuild.t) |
| GPU-07 | Changed color count rebuilds the table. | [tests/lookup/5bit/0040_5bit_gpu_count_rebuild.t](../../../../tests/lookup/5bit/0040_5bit_gpu_count_rebuild.t) |
| GPU-08 | Running `6bit` between two `5bit` byte applications preserves the original table and indexes without rebuilding. | [tests/lookup/5bit/0041_5bit_gpu_policy_isolation.t](../../../../tests/lookup/5bit/0041_5bit_gpu_policy_isolation.t) |
| GPU-09 | Allocation failure invalidates a rebuild and allows recovery. | [tests/lookup/5bit/0042_5bit_gpu_allocation_recovery.t](../../../../tests/lookup/5bit/0042_5bit_gpu_allocation_recovery.t) |
| GPU-10 | Encoder failure invalidates a rebuild and allows recovery. | [tests/lookup/5bit/0043_5bit_gpu_encoder_recovery.t](../../../../tests/lookup/5bit/0043_5bit_gpu_encoder_recovery.t) |
| GPU-11 | Completion failure invalidates a rebuild and allows recovery. | [tests/lookup/5bit/0044_5bit_gpu_completion_recovery.t](../../../../tests/lookup/5bit/0044_5bit_gpu_completion_recovery.t) |
| GPU-12 | Reversing query traversal preserves per-color results. | [tests/lookup/5bit/0045_5bit_gpu_traversal.t](../../../../tests/lookup/5bit/0045_5bit_gpu_traversal.t) |
| GPU-13 | Bluenoise queries use the center table. | [tests/lookup/5bit/0046_5bit_gpu_bluenoise.t](../../../../tests/lookup/5bit/0046_5bit_gpu_bluenoise.t) |
| GPU-14 | Arithmetic a queries use the center table. | [tests/lookup/5bit/0047_5bit_gpu_arithmetic_a.t](../../../../tests/lookup/5bit/0047_5bit_gpu_arithmetic_a.t) |
| GPU-15 | Arithmetic x queries use the center table. | [tests/lookup/5bit/0048_5bit_gpu_arithmetic_x.t](../../../../tests/lookup/5bit/0048_5bit_gpu_arithmetic_x.t) |
| GPU-16 | Partial-rectangle delta indexes match the oracle. | [tests/lookup/5bit/0049_5bit_gpu_delta_indexes.t](../../../../tests/lookup/5bit/0049_5bit_gpu_delta_indexes.t) |
| GPU-17 | Partial-rectangle delta coverage matches the oracle. | [tests/lookup/5bit/0050_5bit_gpu_delta_mask.t](../../../../tests/lookup/5bit/0050_5bit_gpu_delta_mask.t) |
| GPU-18 | Bluenoise delta indexes match the oracle. | [tests/lookup/5bit/0051_5bit_gpu_delta_blue_indexes.t](../../../../tests/lookup/5bit/0051_5bit_gpu_delta_blue_indexes.t) |
| GPU-19 | Bluenoise delta coverage matches the oracle. | [tests/lookup/5bit/0052_5bit_gpu_delta_blue_mask.t](../../../../tests/lookup/5bit/0052_5bit_gpu_delta_blue_mask.t) |
| GPU-20 | Transparent source pixels retain the transparent index. | [tests/lookup/5bit/0053_5bit_gpu_transparent.t](../../../../tests/lookup/5bit/0053_5bit_gpu_transparent.t) |
| GPU-21 | Automatic policy admits the GPU table. | [tests/lookup/5bit/0054_5bit_gpu_auto.t](../../../../tests/lookup/5bit/0054_5bit_gpu_auto.t) |
| GPU-22 | Automatic policy declines below the size threshold. | [tests/lookup/5bit/0055_5bit_gpu_threshold.t](../../../../tests/lookup/5bit/0055_5bit_gpu_threshold.t) |
| GPU-23 | Forced indexed preparation omits the CPU memo. | [tests/lookup/5bit/0056_5bit_gpu_force_no_cpu.t](../../../../tests/lookup/5bit/0056_5bit_gpu_force_no_cpu.t) |
| GPU-24 | Automatic indexed preparation omits the CPU memo. | [tests/lookup/5bit/0057_5bit_gpu_auto_no_cpu.t](../../../../tests/lookup/5bit/0057_5bit_gpu_auto_no_cpu.t) |
| GPU-25 | GPU table reuse ignores CPU shared-instance overrides. | [tests/lookup/5bit/0058_5bit_gpu_shared_override.t](../../../../tests/lookup/5bit/0058_5bit_gpu_shared_override.t) |
| GPU-26 | GPU table reuse ignores CPU packing overrides. | [tests/lookup/5bit/0059_5bit_gpu_packing_override.t](../../../../tests/lookup/5bit/0059_5bit_gpu_packing_override.t) |
| GPU-27 | Declined GPU planning constructs the CPU memo. | [tests/lookup/5bit/0060_5bit_gpu_cpu_fallback.t](../../../../tests/lookup/5bit/0060_5bit_gpu_cpu_fallback.t) |

The fractional cases use bounded fixtures at fixed settings and the [fractional GPU oracle and verification limits](README.md#fractional-gpu-lookup-verification). These checks do not rank perceived quality or establish throughput.

| ID | Contract | Owning test |
| --- | --- | --- |
| FRAC-01 | 5bit fractional reference. | [tests/lookup/5bit/0061_5bit_fractional_reference.t](../../../../tests/lookup/5bit/0061_5bit_fractional_reference.t) |
| FRAC-02 | 5bit fractional ties. | [tests/lookup/5bit/0062_5bit_fractional_ties.t](../../../../tests/lookup/5bit/0062_5bit_fractional_ties.t) |
| FRAC-03 | 5bit fractional cache. | [tests/lookup/5bit/0063_5bit_fractional_cache.t](../../../../tests/lookup/5bit/0063_5bit_fractional_cache.t) |
| FRAC-04 | A completed fractional solve returns its shared table for reuse without rebuilding on the next solve. | [tests/lookup/5bit/0064_5bit_fractional_reuse.t](../../../../tests/lookup/5bit/0064_5bit_fractional_reuse.t) |
| FRAC-05 | Interleaving `6bit` and `5bit` fractional solves preserves each completed table and output without rebuilding. | [tests/lookup/5bit/0065_5bit_fractional_policy_isolation.t](../../../../tests/lookup/5bit/0065_5bit_fractional_policy_isolation.t) |
