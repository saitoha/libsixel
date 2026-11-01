# Lookup policy: `gridlut`

This page describes `--lookup-policy=gridlut`, abbreviated `--lookup-policy=g`, for CLI users and contributors. The [lookup policy reference](README.md) owns the interface, metric and guarantee terminology. `auto` continues to select `vptree`.

## Purpose and intuition

`gridlut` partitions the byte RGB cube into 4096 fixed cells. Each cell stores a conservative list of palette colors that could be nearest to a query anywhere inside it. Spatially distant pixels with similar colors share that list. This differs from [`blockcull`](blockcull.md), which derives a box from each group of neighboring image pixels.

```text
palette + fixed 16x16x16 RGB cells --> conservative candidate lists
                                                |
source --> transparency / delta keep --> dither --> cell address
                                                |
                         exact distances to candidates, in palette order
                                                |
                                    lowest-index nearest color
```

The cell address selects candidates, not a quantized query or a cached answer. The final distances use the original post-dither byte coordinates.

## Method

Each channel's cell coordinate is `q >> 4`. Its closed integer bounds are `low = coordinate * 16` and `high = low + 15`. For each palette color, the constructor computes the nearest and farthest squared distances to that box using the [block-culling bounds](blockcull.md#method). It retains every color whose nearest distance is no greater than the minimum farthest distance over all colors. Strictly excluded colors cannot win or tie anywhere in the cell.

| Cell result | Query operation |
| --- | --- |
| One candidate | Return its index |
| Two to 64 candidates | Scan their exact integer squared RGB distances in palette order |
| More than 64 candidates | Scan the whole palette; no candidates are truncated |

The byte CPU instance owns its palette and candidate table. Preparation reuses the table only when the palette size and every channel value match; a changed palette replaces it even when its size is unchanged. Queries are read-only and require no per-frame error buffer. Palettes above 256 colors are rejected.

The fixed byte-domain grid does not cover arbitrary float working coordinates. `lookup/gridlut.float32` therefore delegates to the existing exact [`blockcull` float backend](blockcull.md#method), preserving its normalized metric, nonfinite handling and build arithmetic. It does not construct a byte grid or round float queries.

## Metal palette application

Metal builds the candidate table on the GPU before applying a changed palette. Separate compute encoders in the same command buffer establish the table's write/read dependency without a CPU wait between construction and lookup. The cache compares palette bytes and size independently of the ordinary palette-upload cache and publishes its identity only after successful execution. A failed rebuild invalidates that identity.

The table occupies `4096 * 65 = 266240` bytes: one count and 64 byte indexes per cell. A zero count denotes the exhaustive overflow path. The cache is shared under the existing Metal dispatch lock. Unchanged palettes reuse it; alternating palettes rebuild it. No simdgroup matrix support is required.

Lookup runs after positional or blue-noise dither, using the same transparency, delta keep and retained-plane update stages as other GPU policies. A kept pixel performs no candidate lookup. GPU admission otherwise follows the byte RGB palette-application rules; unsupported shapes use the CPU path under GPU `auto`. With `SIXEL_TRACE_TOPIC=gpu_contract`, the selected path reports `LSXGPU2|consumer=palette-apply|kernel=gridlut`.

## Guarantee

Byte lookup is scan-index exact: it returns the same palette index as `none` for the same working palette and query, including the lowest index at a tie. Integer bounds preserve every possible winner, and overflow returns to an exhaustive scan. Float selection retains the exactness contract of its delegated backend.

This does not promise identical complete encodes to `none`: `none` also selects a different [Heckbert histogram resolution](README.md#precedence-and-interactions). `gridlut` uses the ordinary accelerated-policy histogram, like `blockcull`. Comparisons of lookup exactness must hold the palette fixed; comparisons of complete pipelines must also account for palette construction, encoded bytes and presentation timing.

## Cost

For `K` colors and `C = 4096` cells, a changed palette costs `Theta(C K)` to prepare. Comparing an unchanged palette costs `Theta(K)`. A lookup costs `Theta(V)` for the cell's retained candidates, or `Theta(K)` on overflow. CPU storage is the fixed candidate table plus palette arrays; the float fallback has the existing blockcull costs.

The GPU construction cost is paid once per palette change, even if delta keeps most pixels. Candidate-list loads and divergent list lengths can outweigh saved distances. Small images, tiny palettes, frequently changing palettes or many overflow cells can favor another policy. A shorter lookup stage alone does not establish a playback improvement: measure the complete pipeline and output size as well.

## Implementation map

[`lookup-policy-gridlut.c`](../../../../src/lookup-policy-gridlut.c) owns CPU preparation, exact byte lookup and float delegation. [`gpu-palette-metal.m`](../../../../src/gpu-palette-metal.m) owns GPU construction, cache identity, lookup and dispatch. [`options-registry.c`](../../../../src/options-registry.c) registers the unique `g` prefix.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| GL-01 | Byte queries equal an independent exhaustive scan at every grid-cell corner, on random and palette-hit queries, duplicate colors, overflow, reverse traversal and re-preparation with a changed palette of equal size. | [tests/lookup/gridlut/0005_gridlut_byte_exact.t](../../../../tests/lookup/gridlut/0005_gridlut_byte_exact.t), [tests/_support/processing/lookup/0037_gridlut_exact.c](../../../../tests/_support/processing/lookup/0037_gridlut_exact.c) |
| GL-02 | Float delegation matches an independent double-distance scan, nonfinite handling and finite VP-tree metric results. | [tests/lookup/gridlut/0006_gridlut_float_exact.t](../../../../tests/lookup/gridlut/0006_gridlut_float_exact.t) |
| GL-03 | Metal indexes and delta keep masks equal CPU exhaustive lookup with none, bluenoise and positional dithering, candidate overflow, repeated palettes and changed palettes of equal size; the gridlut kernel is selected. | [tests/lookup/gridlut/0001_gridlut_exact.t](../../../../tests/lookup/gridlut/0001_gridlut_exact.t), [tests/lookup/gridlut/0001_gridlut_exact.c](../../../../tests/lookup/gridlut/0001_gridlut_exact.c) |
| GL-04 | gridlut allocation failure falls back and rebuilds. | [tests/lookup/gridlut/0002_gridlut_allocation_failure.t](../../../../tests/lookup/gridlut/0002_gridlut_allocation_failure.t) |
| GL-05 | gridlut encoder failure falls back and rebuilds. | [tests/lookup/gridlut/0003_gridlut_encoder_failure.t](../../../../tests/lookup/gridlut/0003_gridlut_encoder_failure.t) |
| GL-06 | gridlut completion failure falls back and rebuilds. | [tests/lookup/gridlut/0004_gridlut_completion_failure.t](../../../../tests/lookup/gridlut/0004_gridlut_completion_failure.t) |
| GL-07 | Native gamma float lookup retains extended coordinates. | [tests/lookup/gridlut/0007_gridlut_gamma_range.t](../../../../tests/lookup/gridlut/0007_gridlut_gamma_range.t) |
| GL-08 | Native linear float lookup retains extended coordinates. | [tests/lookup/gridlut/0008_gridlut_linear_range.t](../../../../tests/lookup/gridlut/0008_gridlut_linear_range.t) |
| GL-09 | Native oklab float lookup retains extended coordinates. | [tests/lookup/gridlut/0009_gridlut_oklab_range.t](../../../../tests/lookup/gridlut/0009_gridlut_oklab_range.t) |
| GL-10 | Native cielab float lookup retains extended coordinates. | [tests/lookup/gridlut/0010_gridlut_cielab_range.t](../../../../tests/lookup/gridlut/0010_gridlut_cielab_range.t) |
| GL-11 | Native din99d float lookup retains extended coordinates. | [tests/lookup/gridlut/0011_gridlut_din99d_range.t](../../../../tests/lookup/gridlut/0011_gridlut_din99d_range.t) |
| GL-12 | Native gamma float lookup distinguishes fractional samples. | [tests/lookup/gridlut/0012_gridlut_gamma_fraction.t](../../../../tests/lookup/gridlut/0012_gridlut_gamma_fraction.t) |
| GL-13 | Native linear float lookup distinguishes fractional samples. | [tests/lookup/gridlut/0013_gridlut_linear_fraction.t](../../../../tests/lookup/gridlut/0013_gridlut_linear_fraction.t) |
| GL-14 | Native oklab float lookup distinguishes fractional samples. | [tests/lookup/gridlut/0014_gridlut_oklab_fraction.t](../../../../tests/lookup/gridlut/0014_gridlut_oklab_fraction.t) |
| GL-15 | Native cielab float lookup distinguishes fractional samples. | [tests/lookup/gridlut/0015_gridlut_cielab_fraction.t](../../../../tests/lookup/gridlut/0015_gridlut_cielab_fraction.t) |
| GL-16 | Native din99d float lookup distinguishes fractional samples. | [tests/lookup/gridlut/0016_gridlut_din99d_fraction.t](../../../../tests/lookup/gridlut/0016_gridlut_din99d_fraction.t) |
| GL-17 | Native gamma float lookup matches the independent metric oracle. | [tests/lookup/gridlut/0017_gridlut_gamma_oracle.t](../../../../tests/lookup/gridlut/0017_gridlut_gamma_oracle.t) |
| GL-18 | Native linear float lookup matches the independent metric oracle. | [tests/lookup/gridlut/0018_gridlut_linear_oracle.t](../../../../tests/lookup/gridlut/0018_gridlut_linear_oracle.t) |
| GL-19 | Native oklab float lookup matches the independent metric oracle. | [tests/lookup/gridlut/0019_gridlut_oklab_oracle.t](../../../../tests/lookup/gridlut/0019_gridlut_oklab_oracle.t) |
| GL-20 | Native cielab float lookup matches the independent metric oracle. | [tests/lookup/gridlut/0020_gridlut_cielab_oracle.t](../../../../tests/lookup/gridlut/0020_gridlut_cielab_oracle.t) |
| GL-21 | Native din99d float lookup matches the independent metric oracle. | [tests/lookup/gridlut/0021_gridlut_din99d_oracle.t](../../../../tests/lookup/gridlut/0021_gridlut_din99d_oracle.t) |
| GL-22 | Large gamma palettes match the independent metric oracle. | [tests/lookup/gridlut/0022_gridlut_gamma_oracle_large.t](../../../../tests/lookup/gridlut/0022_gridlut_gamma_oracle_large.t) |
| GL-23 | Large linear palettes match the independent metric oracle. | [tests/lookup/gridlut/0023_gridlut_linear_oracle_large.t](../../../../tests/lookup/gridlut/0023_gridlut_linear_oracle_large.t) |
| GL-24 | Large oklab palettes match the independent metric oracle. | [tests/lookup/gridlut/0024_gridlut_oklab_oracle_large.t](../../../../tests/lookup/gridlut/0024_gridlut_oklab_oracle_large.t) |
| GL-25 | Large cielab palettes match the independent metric oracle. | [tests/lookup/gridlut/0025_gridlut_cielab_oracle_large.t](../../../../tests/lookup/gridlut/0025_gridlut_cielab_oracle_large.t) |
| GL-26 | Large din99d palettes match the independent metric oracle. | [tests/lookup/gridlut/0026_gridlut_din99d_oracle_large.t](../../../../tests/lookup/gridlut/0026_gridlut_din99d_oracle_large.t) |
| GL-27 | Small gamma palettes match the independent metric oracle. | [tests/lookup/gridlut/0027_gridlut_gamma_oracle_small.t](../../../../tests/lookup/gridlut/0027_gridlut_gamma_oracle_small.t) |
| GL-28 | Small linear palettes match the independent metric oracle. | [tests/lookup/gridlut/0028_gridlut_linear_oracle_small.t](../../../../tests/lookup/gridlut/0028_gridlut_linear_oracle_small.t) |
| GL-29 | Small oklab palettes match the independent metric oracle. | [tests/lookup/gridlut/0029_gridlut_oklab_oracle_small.t](../../../../tests/lookup/gridlut/0029_gridlut_oklab_oracle_small.t) |
| GL-30 | Small cielab palettes match the independent metric oracle. | [tests/lookup/gridlut/0030_gridlut_cielab_oracle_small.t](../../../../tests/lookup/gridlut/0030_gridlut_cielab_oracle_small.t) |
| GL-31 | Small din99d palettes match the independent metric oracle. | [tests/lookup/gridlut/0031_gridlut_din99d_oracle_small.t](../../../../tests/lookup/gridlut/0031_gridlut_din99d_oracle_small.t) |
| GL-32 | Mixed-range gamma palettes match the independent metric oracle. | [tests/lookup/gridlut/0032_gridlut_gamma_oracle_mixed.t](../../../../tests/lookup/gridlut/0032_gridlut_gamma_oracle_mixed.t) |
| GL-33 | Mixed-range linear palettes match the independent metric oracle. | [tests/lookup/gridlut/0033_gridlut_linear_oracle_mixed.t](../../../../tests/lookup/gridlut/0033_gridlut_linear_oracle_mixed.t) |
| GL-34 | Mixed-range oklab palettes match the independent metric oracle. | [tests/lookup/gridlut/0034_gridlut_oklab_oracle_mixed.t](../../../../tests/lookup/gridlut/0034_gridlut_oklab_oracle_mixed.t) |
| GL-35 | Mixed-range cielab palettes match the independent metric oracle. | [tests/lookup/gridlut/0035_gridlut_cielab_oracle_mixed.t](../../../../tests/lookup/gridlut/0035_gridlut_cielab_oracle_mixed.t) |
| GL-36 | Mixed-range din99d palettes match the independent metric oracle. | [tests/lookup/gridlut/0036_gridlut_din99d_oracle_mixed.t](../../../../tests/lookup/gridlut/0036_gridlut_din99d_oracle_mixed.t) |
| GL-37 | cielab decisions follow the normalized channel metric. | [tests/lookup/gridlut/0037_gridlut_cielab_metric.t](../../../../tests/lookup/gridlut/0037_gridlut_cielab_metric.t) |
| GL-38 | din99d decisions follow the normalized channel metric. | [tests/lookup/gridlut/0038_gridlut_din99d_metric.t](../../../../tests/lookup/gridlut/0038_gridlut_din99d_metric.t) |
| GL-39 | Preparing one float lookup instance does not change another. | [tests/lookup/gridlut/0039_gridlut_instance_isolation.t](../../../../tests/lookup/gridlut/0039_gridlut_instance_isolation.t) |

GL-03 skips without a Metal palette backend. The shared [CLI prefix contract](../../../cli/prefix-matching.md) checks that every registered lookup policy has a unique initial. These tests establish selection and exactness, not performance or perceived quality.

The native-float matrix covers five colorspaces, fractional and extended coordinates, palette sizes and instance isolation. GPU failure cases inject allocation, encoder-creation and post-completion errors around real backend execution, then require AUTO CPU fallback, cache rebuilding and later reuse. They do not simulate a device reset or an operating-system allocation failure.
