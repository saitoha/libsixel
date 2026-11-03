# Lookup policy: `6bit`

This page describes the `6bit` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

`6bit` is the finer sibling of `5bit`. Its CPU backend partitions byte RGB space into `64^3` buckets, performs an exhaustive scan for a query that finds an empty bucket, and memoizes that index.

```text
8-bit RGB candidate
       |
       v
round each channel to 6 address bits
       |
       v
64 x 64 x 64 bucket table
       |
       +-- filled --> return cached index
       |
       +-- empty  --> scan palette --> store and return index
```

## Method

For byte channel `v`:

```text
q6(v) = min(63, floor((v + 2) / 4))
bucket(x) = pack(q6(x.r), q6(x.g), q6(x.b))
G = 2^(6 * 3) = 262,144
```

The CPU table stores one signed 32-bit value per bucket requires approximately 1 MiB before allocator overhead. This is eight times the number of entries used by `5bit`.

The finer grid lowers the chance that two visibly different candidates share a cached answer, but it increases initialization work, memory footprint, and the number of cold buckets an image may encounter. It is not unconditionally faster or better than `5bit`.

As with `5bit`, `linear` and `morton` are the documented packing values. The implementation recognizes an additional non-public `hilbert` value. Packing only changes address layout.

## Guarantee

The CPU 8-bit guarantee has two phases:

```text
first candidate in bucket   scan-index exact
later candidate in bucket   approximate if its nearest entry differs
```

The approximation is query-history dependent. Smaller buckets reduce, but do not eliminate, the possibility that a nearest-color boundary crosses a bucket.

Serial, worker-local parallel (`shared_instance=0`), and shared parallel (`shared_instance=1`) 8-bit paths all populate their dense tables. With a shared instance, racing first writers can select different valid representatives for one bucket, making later hits and the resulting output schedule-dependent. Relaxed atomic accesses prevent a C data race without imposing ordering on the surrounding lookup. The float32 backend remains a normalized-distance exhaustive scan; the `6bit` name does not describe a float32 grid in that path.

## Metal application

The Metal backend eagerly builds all `64^3` entries at palette application. Each cell stores the nearest actual palette entry to the center of the byte values mapped to that cell by `q6`. The palette itself is not snapped to the grid. Distances use doubled integer coordinates so half-byte centers are represented exactly; equal distances choose the lowest palette index. Edge cells use their actual clipped byte intervals.

| Backend | Cell representative | Population | Query |
| --- | --- | --- | --- |
| CPU | First observed query | Lazy palette scan | Cached index, possibly traversal-dependent |
| Metal | Center of the cell's integer byte interval | Complete table before application | One byte-table read, independent of traversal |

Both are approximate for an arbitrary query inside a cell. CPU and GPU output can differ even with the same palette and dither settings; this path does not promise index equivalence to CPU memoization or exhaustive `none` lookup.

The table is always process-global and shared under the Metal dispatch mutex. GPU packing is linear; the CPU `shared_instance` and `packing` suboptions do not change this behavior. The active table contains 256 KiB of entries, with palette index 255 available as an ordinary value for 256-color palettes. Reuse requires identical bit width, palette count and ordered palette bytes. Geometry and dither parameters do not affect table identity. The shared allocation grows to the largest requested table and retains that capacity, so using `6bit` can leave a 256 KiB allocation resident during later `5bit` calls.

Construction and application use the same command buffer with an encoder boundary. Cache identity is published only after successful completion. A failed rebuild invalidates the table so a subsequent call rebuilds it; failed application does not publish indexes. A cold application costs `Theta(G K + P)` and a reusable application costs `Theta(P + K)`, where `G = 64^3`. Shader compilation is a separate first-use cost.

RGB888 application supports `none`, arithmetic `a`/`x`, and `bluenoise` dithering, transparency, and eligible partial-rectangle delta comparison. Native float lookup remains CPU-only. The public feature macro `SIXEL_HAVE_GPU_PREBUILT_BIT_LOOKUP` lets clients admit this policy without relying on older libraries that could reinterpret forced unsupported lookup as direct scanning. GPU-only preparation needs the existing lookup kind and builds no CPU memo table; an automatic GPU rejection can still prepare the CPU policy before output.

The Metal implementation is [gpu-palette-metal-bitlut.inc.h](../../../../src/gpu-palette-metal-bitlut.inc.h). The [execution map](../../execution-map/README.md) describes component ownership.

## Cost

For the CPU backend, with `U` distinct first-use buckets:

```text
table initialization        Theta(64^3)
cache hit                   Theta(1)
first-bucket miss           Theta(K)
8-bit whole image           Theta(64^3 + P + U K)
float32 query               Theta(K)
```

The `shared_instance` suboption defaults to enabled. The larger table makes worker-local duplication especially relevant to memory and cache pressure; sharing avoids that duplication and lets workers reuse one memoized table at the cost of schedule-dependent representatives when first writes race.

## Name and sources

`6bit` is a libsixel descriptive name derived from six address bits per RGB channel, not a literature algorithm name. It was introduced as a denser alternative to the historical 5-bit table.

The implementation is [`lookup-policy-6bit.c`](../../../../src/lookup-policy-6bit.c). Selection and the non-RGB fallback are in [`lookup-policy.c`](../../../../src/lookup-policy.c).

## Test coverage

<!-- test-coverage: enforced -->

The bounded byte-ramp oracle independently enumerates each channel's bucket preimage and checks its center against actual palette colors. Cases isolate center values, tie order, the byte-index limit, reuse keys, failure recovery and each supported integration path. Ownership checks use normal indexed encoding; reuse checks use the GPU observation hooks. These tests establish behavior, not perceptual superiority or throughput, and do not exhaust the RGB cube.

| ID | Contract | Test |
| --- | --- | --- |
| GPU-01 | Rounded-cell centers match an independent byte-preimage oracle. | [tests/lookup/6bit/0034_6bit_gpu_centers.t](../../../../tests/lookup/6bit/0034_6bit_gpu_centers.t) |
| GPU-02 | Equal center distances choose the lowest palette index. | [tests/lookup/6bit/0035_6bit_gpu_tie.t](../../../../tests/lookup/6bit/0035_6bit_gpu_tie.t) |
| GPU-03 | Index 255 remains usable with 256 paintable colors. | [tests/lookup/6bit/0036_6bit_gpu_index255.t](../../../../tests/lookup/6bit/0036_6bit_gpu_index255.t) |
| GPU-04 | Active table size is one byte per grid cell. | [tests/lookup/6bit/0037_6bit_gpu_table_bytes.t](../../../../tests/lookup/6bit/0037_6bit_gpu_table_bytes.t) |
| GPU-05 | Unchanged palette reuses the table. | [tests/lookup/6bit/0038_6bit_gpu_reuse.t](../../../../tests/lookup/6bit/0038_6bit_gpu_reuse.t) |
| GPU-06 | Changed palette bytes rebuild the table. | [tests/lookup/6bit/0039_6bit_gpu_palette_rebuild.t](../../../../tests/lookup/6bit/0039_6bit_gpu_palette_rebuild.t) |
| GPU-07 | Changed color count rebuilds the table. | [tests/lookup/6bit/0040_6bit_gpu_count_rebuild.t](../../../../tests/lookup/6bit/0040_6bit_gpu_count_rebuild.t) |
| GPU-08 | Switching bit width invalidates the table identity. | [tests/lookup/6bit/0041_6bit_gpu_bits_rebuild.t](../../../../tests/lookup/6bit/0041_6bit_gpu_bits_rebuild.t) |
| GPU-09 | Allocation failure invalidates a rebuild and allows recovery. | [tests/lookup/6bit/0042_6bit_gpu_allocation_recovery.t](../../../../tests/lookup/6bit/0042_6bit_gpu_allocation_recovery.t) |
| GPU-10 | Encoder failure invalidates a rebuild and allows recovery. | [tests/lookup/6bit/0043_6bit_gpu_encoder_recovery.t](../../../../tests/lookup/6bit/0043_6bit_gpu_encoder_recovery.t) |
| GPU-11 | Completion failure invalidates a rebuild and allows recovery. | [tests/lookup/6bit/0044_6bit_gpu_completion_recovery.t](../../../../tests/lookup/6bit/0044_6bit_gpu_completion_recovery.t) |
| GPU-12 | Reversing query traversal preserves per-color results. | [tests/lookup/6bit/0045_6bit_gpu_traversal.t](../../../../tests/lookup/6bit/0045_6bit_gpu_traversal.t) |
| GPU-13 | Bluenoise queries use the center table. | [tests/lookup/6bit/0046_6bit_gpu_bluenoise.t](../../../../tests/lookup/6bit/0046_6bit_gpu_bluenoise.t) |
| GPU-14 | Arithmetic a queries use the center table. | [tests/lookup/6bit/0047_6bit_gpu_arithmetic_a.t](../../../../tests/lookup/6bit/0047_6bit_gpu_arithmetic_a.t) |
| GPU-15 | Arithmetic x queries use the center table. | [tests/lookup/6bit/0048_6bit_gpu_arithmetic_x.t](../../../../tests/lookup/6bit/0048_6bit_gpu_arithmetic_x.t) |
| GPU-16 | Partial-rectangle delta indexes match the oracle. | [tests/lookup/6bit/0049_6bit_gpu_delta_indexes.t](../../../../tests/lookup/6bit/0049_6bit_gpu_delta_indexes.t) |
| GPU-17 | Partial-rectangle delta coverage matches the oracle. | [tests/lookup/6bit/0050_6bit_gpu_delta_mask.t](../../../../tests/lookup/6bit/0050_6bit_gpu_delta_mask.t) |
| GPU-18 | Bluenoise delta indexes match the oracle. | [tests/lookup/6bit/0051_6bit_gpu_delta_blue_indexes.t](../../../../tests/lookup/6bit/0051_6bit_gpu_delta_blue_indexes.t) |
| GPU-19 | Bluenoise delta coverage matches the oracle. | [tests/lookup/6bit/0052_6bit_gpu_delta_blue_mask.t](../../../../tests/lookup/6bit/0052_6bit_gpu_delta_blue_mask.t) |
| GPU-20 | Transparent source pixels retain the transparent index. | [tests/lookup/6bit/0053_6bit_gpu_transparent.t](../../../../tests/lookup/6bit/0053_6bit_gpu_transparent.t) |
| GPU-21 | Automatic policy admits the GPU table. | [tests/lookup/6bit/0054_6bit_gpu_auto.t](../../../../tests/lookup/6bit/0054_6bit_gpu_auto.t) |
| GPU-22 | Automatic policy declines below the size threshold. | [tests/lookup/6bit/0055_6bit_gpu_threshold.t](../../../../tests/lookup/6bit/0055_6bit_gpu_threshold.t) |
| GPU-23 | Forced indexed preparation omits the CPU memo. | [tests/lookup/6bit/0056_6bit_gpu_force_no_cpu.t](../../../../tests/lookup/6bit/0056_6bit_gpu_force_no_cpu.t) |
| GPU-24 | Automatic indexed preparation omits the CPU memo. | [tests/lookup/6bit/0057_6bit_gpu_auto_no_cpu.t](../../../../tests/lookup/6bit/0057_6bit_gpu_auto_no_cpu.t) |
| GPU-25 | GPU table reuse ignores CPU shared-instance overrides. | [tests/lookup/6bit/0058_6bit_gpu_shared_override.t](../../../../tests/lookup/6bit/0058_6bit_gpu_shared_override.t) |
| GPU-26 | GPU table reuse ignores CPU packing overrides. | [tests/lookup/6bit/0059_6bit_gpu_packing_override.t](../../../../tests/lookup/6bit/0059_6bit_gpu_packing_override.t) |
| GPU-27 | Declined GPU planning constructs the CPU memo. | [tests/lookup/6bit/0060_6bit_gpu_cpu_fallback.t](../../../../tests/lookup/6bit/0060_6bit_gpu_cpu_fallback.t) |
