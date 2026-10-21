# Lookup policy: `6bit`

This page describes the `6bit` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

`6bit` is the finer sibling of `5bit`. It partitions byte RGB space into `64^3` buckets, performs an exhaustive scan for a query that finds an empty bucket, and memoizes that index.

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

One signed 32-bit value per bucket requires approximately 1 MiB before allocator overhead. This is eight times the number of entries used by `5bit`.

The finer grid lowers the chance that two visibly different candidates share a cached answer, but it increases initialization work, memory footprint, and the number of cold buckets an image may encounter. It is not unconditionally faster or better than `5bit`.

As with `5bit`, `linear` and `morton` are the documented packing values. The implementation recognizes an additional non-public `hilbert` value. Packing only changes address layout.

## Guarantee

The 8-bit guarantee has two phases:

```text
first candidate in bucket   scan-index exact
later candidate in bucket   approximate if its nearest entry differs
```

The approximation is query-history dependent. Smaller buckets reduce, but do not eliminate, the possibility that a nearest-color boundary crosses a bucket.

Serial, worker-local parallel (`shared_instance=0`), and shared parallel (`shared_instance=1`) 8-bit paths all populate their dense tables. With a shared instance, racing first writers can select different valid representatives for one bucket, making later hits and the resulting output schedule-dependent. Relaxed atomic accesses prevent a C data race without imposing ordering on the surrounding lookup. The float32 backend remains a normalized-distance exhaustive scan; the `6bit` name does not describe a float32 grid in that path.

## Cost

With `U` distinct first-use buckets:

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
