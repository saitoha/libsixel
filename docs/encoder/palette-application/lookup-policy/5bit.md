# Lookup policy: `5bit`

This page describes the `5bit` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

`5bit` partitions byte-valued RGB space into `32^3` buckets. The first serial query entering an empty bucket performs a full palette scan. The resulting index is then reused for later colors in the same bucket.

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

The table stores one signed 32-bit palette index per bucket, approximately 128 KiB before allocator overhead.

Reducing one byte channel from 256 values to 32 address levels groups eight adjacent values together. Across RGB this reduces `256^3 = 16,777,216` possible tuples to `32^3 = 32,768` cells. A bucket boundary can cross a palette Voronoi boundary, and a histogram cell can erase distinctions among colors that should influence different median-cut boxes. The error is therefore not only a memory-capacity question.

The documented `SIXEL_LOOKUP_PACKING` values are `linear` and `morton`. Linear packing concatenates channel fields. Morton packing interleaves their bits to improve spatial locality for some access patterns. The implementation also recognizes `hilbert`, but that value is not currently part of the public manual contract. Packing changes memory order, not which colors share a bucket.

## Guarantee

On an empty bucket, the policy scans the actual candidate against all palette entries, so that answer is scan-index exact in byte RGB distance. Afterwards every candidate with the same `bucket(x)` receives the stored index:

```text
lookup(x_first) = argmin_i D(i, x_first)
lookup(x_later) = lookup(x_first), if bucket(x_later) = bucket(x_first)
```

The later answer can be approximate, and it depends on traversal history. The maximum component separation inside one rounded bucket is small, but a palette Voronoi boundary can still cross the bucket.

Serial, worker-local parallel (`shared_instance=0`), and shared parallel (`shared_instance=1`) 8-bit paths all populate their dense tables. With a shared instance, multiple workers can observe the same empty bucket, scan different candidate colors, and store different valid palette indices; whichever write becomes visible determines the cached representative, so repeated runs need not be byte-reproducible. Relaxed atomic loads and stores make those accesses well-defined without ordering surrounding work. The float32 backend still performs an exhaustive scan using the normalized weighted metric described in the index.

## Cost

Let `U` be the number of distinct buckets first encountered:

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

`go-sixel` provides a useful contrast: it also addresses an RGB555 table, but it [fills all cells eagerly using each cell center](https://github.com/mattn/go-sixel/blob/ceaaab1e2b5973d9ebc28b0a615760b921e90681/sixel.go#L676-L712). That makes its representative deterministic for a palette, while libsixel's representative is the first actual query observed in each bucket.

## History

The quantizer used through the 1.8.7 era applied the same RGB555 address to two different jobs. Palette construction counted input colors in a `32^3` histogram and reconstructed every occupied cell from the truncated five-bit coordinates. Palette application then lazily cached the nearest palette index for the first pixel observed in each RGB555 cell. The historical channel map was simply:

```text
q_old(v) = floor(v / 8)
r_old(q) = 8q
```

Consequently, as many as eight input values on each axis became one histogram value, with a per-channel truncation of up to seven byte values before median cut even started. The lazy application cache could then add a second source of error around palette decision boundaries.

The current named `5bit` policy rounds and saturates its lookup address rather than reproducing that old truncating hash exactly. Its Heckbert configuration still selects a five-bit-per-channel histogram, however, so requesting a larger palette cannot recover color distinctions that the histogram already merged. This is why higher `K` does not guarantee that the quality gap will close.

The [move from RGB555 to RGB666 in 2025](https://github.com/saitoha/libsixel/commit/4f57a743c11c4149656cdad6e3f8d59d76c58036), followed by [`none` as a full-precision control](https://github.com/saitoha/libsixel/commit/fafc097994dfcd1af8f17c09a94628ea158281a4) and by several structurally different search policies, grew out of the need to make this speed, memory, and quality tradeoff explicit. Later policies also address float colorspaces, parallelism, and different preparation/query costs; they are not merely cosmetic renamings of the old cache.

The [measured quality and speed comparison](README.md#measured-trade-offs) compares the current controlled K-means command with the historical Heckbert coupling. On the checked fixture, `6bit` is materially closer to `none` than `5bit` is, and is faster than `none`, but neither difference alone determines the best default.

The implementation is [`lookup-policy-5bit.c`](../../../../src/lookup-policy-5bit.c). CLI and environment spelling is documented in the [`img2sixel(1)` manual](../../../../converters/img2sixel.1).
