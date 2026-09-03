# `6bit` Lookup Policy

## Purpose and intuition

`6bit` is the finer sibling of `5bit`. It partitions byte RGB space into
`64^3` buckets, performs an exhaustive scan for the first serial query in a
bucket, and memoizes that index.

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

## Bucket mathematics

For byte channel `v`:

```text
q6(v) = min(63, floor((v + 2) / 4))
bucket(x) = pack(q6(x.r), q6(x.g), q6(x.b))
G = 2^(6 * 3) = 262,144
```

One signed 32-bit value per bucket requires approximately 1 MiB before
allocator overhead. This is eight times the number of entries used by `5bit`.

The finer grid lowers the chance that two visibly different candidates share a
cached answer, but it increases initialization work, memory footprint, and the
number of cold buckets an image may encounter. It is not unconditionally
faster or better than `5bit`.

As with `5bit`, `linear` and `morton` are the documented packing values. The
implementation recognizes an additional non-public `hilbert` value. Packing
only changes address layout.

## Guarantee

The serial 8-bit guarantee has two phases:

```text
first candidate in bucket   scan-index exact
later candidate in bucket   approximate if its nearest entry differs
```

The approximation is query-history dependent. Smaller buckets reduce, but do
not eliminate, the possibility that a nearest-color boundary crosses a bucket.

The current parallel-dither path suppresses writes to the table and scans the
palette for every candidate, preserving scan-index exactness. The float32
backend is also a normalized-distance exhaustive scan; the `6bit` name does not
describe a float32 grid in that path.

## Cost

With `U` distinct first-use buckets:

```text
table initialization       Theta(64^3)
serial cache hit           Theta(1)
serial first-bucket miss   Theta(K)
serial whole image         Theta(64^3 + P + U K)
parallel or float32 query  Theta(K)
```

The `shared_instance` suboption defaults to enabled. The larger table makes
worker-local duplication especially relevant to memory and cache pressure.

## Name and source

`6bit` is a libsixel descriptive name derived from six address bits per RGB
channel, not a literature algorithm name. It was introduced as a denser
alternative to the historical 5-bit table.

The implementation is
[`lookup-policy-6bit.c`](../../../src/lookup-policy-6bit.c). Selection and the
non-RGB fallback are in
[`lookup-policy.c`](../../../src/lookup-policy.c).

Return to the [Lookup Policy index](../lookup-policy.md).
