# `5bit` Lookup Policy

## Purpose and intuition

`5bit` partitions byte-valued RGB space into `32^3` buckets. The first serial
query entering an empty bucket performs a full palette scan. The resulting
index is then reused for later colors in the same bucket.

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

The name describes address precision, not palette precision. Palette entries
and the first candidate remain 8-bit values.

## Bucket mathematics

For a byte channel `v`, libsixel rounds and saturates:

```text
q5(v) = min(31, floor((v + 4) / 8))
bucket(x) = pack(q5(x.r), q5(x.g), q5(x.b))
```

There are:

```text
G = 2^(5 * 3) = 32,768 buckets
```

The table stores one signed 32-bit palette index per bucket, approximately
128 KiB before allocator overhead.

The documented `SIXEL_LOOKUP_PACKING` values are `linear` and `morton`.
Linear packing concatenates channel fields. Morton packing interleaves their
bits to improve spatial locality for some access patterns. The implementation
also recognizes `hilbert`, but that value is not currently part of the public
manual contract. Packing changes memory order, not which colors share a bucket.

## Guarantee

On an empty serial bucket, the policy scans the actual candidate against all
palette entries, so that first answer is scan-index exact in byte RGB distance.
Afterwards every candidate with the same `bucket(x)` receives the stored index:

```text
lookup(x_first) = argmin_i D(i, x_first)
lookup(x_later) = lookup(x_first), if bucket(x_later) = bucket(x_first)
```

The later answer can be approximate, and it depends on traversal history. The
maximum component separation inside one rounded bucket is small, but a palette
Voronoi boundary can still cross the bucket.

The current parallel-dither path does not write the shared dense table. It
therefore performs a full scan for each query and is scan-index exact, at the
cost of losing memoized hits. The float32 backend also performs an exhaustive
scan, using the normalized weighted metric described in the index.

## Cost

Let `U` be the number of distinct buckets first encountered:

```text
table initialization       Theta(32^3)
serial cache hit           Theta(1)
serial first-bucket miss   Theta(K)
serial whole image         Theta(32^3 + P + U K)
parallel or float32 query  Theta(K)
```

The `shared_instance` suboption controls whether workers receive a shared
instance; its default is enabled for `5bit`. Sharing is a lifecycle and race
policy as well as a memory choice.

## Name, lineage, and comparisons

`5bit` is a libsixel descriptive name derived from the five address bits per
channel. It is not named after a paper. The same basic lazy RGB555 design is
present in the current
[`libsixel/libsixel` fork](https://github.com/libsixel/libsixel/blob/ee77c2f979dbccc86651a482e748fe7bf80bc89c/src/quant.c#L1091-L1139)
and comes from shared project lineage. That implementation truncates to the
top five bits, whereas the current policy documented here rounds and
saturates.

`go-sixel` provides a useful contrast: it also addresses an RGB555 table, but
it [fills all cells eagerly using each cell center](https://github.com/mattn/go-sixel/blob/ceaaab1e2b5973d9ebc28b0a615760b921e90681/sixel.go#L676-L712).
That makes its representative deterministic for a palette, while libsixel's
representative is the first actual query observed in each bucket.

The implementation is
[`lookup-policy-5bit.c`](../../../src/lookup-policy-5bit.c). CLI and environment
spelling is documented in the [`img2sixel(1)` manual](../../../converters/img2sixel.1).

Return to the [Lookup Policy index](../lookup-policy.md).
