# Lookup policy: `none`

This page describes the `none` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

`none` is the exhaustive-search mental model: compare the candidate with every palette color and keep the best index. It disables lookup acceleration, not palette construction, dithering, or palette application.

```text
candidate x
    |
    +--> distance(x, c[0]) --+
    +--> distance(x, c[1]) --+--> smallest distance --> palette index
    |          ...           |
    +--> distance(x, c[K-1]) +
```

`-d none` is independent. This command disables both dither adjustment and lookup acceleration:

```text
img2sixel -d none --lookup-policy=none input.png
```

## Method

For an 8-bit or float32 candidate `x` with stored palette entries `c[i]`, the current direct scan uses:

```text
D(i, x) = sum_d (x[d] - c[i][d])^2
```

The scan updates its result only for a strict improvement. If two entries have equal distance, the lower palette index wins because it was visited first.

The Metal palette-application path evaluates the same scan as a matrix product. Dropping the query term common to every entry leaves the ranking unchanged:

```text
D(i, x) - |x|^2 = |c[i]|^2 - 2 x . c[i]
```

Each simdgroup multiplies 32 byte queries, as rows `(r, g, b, 1)`, by tiles of eight palette entries, as columns `(-2r, -2g, -2b, |c|^2)`, and keeps the first minimum in palette order. With byte coordinates every product and partial sum is an integer below `2^24`, so float32 matrix arithmetic is exact in any summation order and the kernel returns the scan's index, including the first-index tie rule. Apple GPU family 7 and later use this kernel; other devices, or a failed kernel build, keep the per-pixel scan. Both variants exclude a reserved omission key and apply 6delta keeps exactly as the scan does.

Unlike the accelerated float32 policies, `none` does not divide each channel by its declared range. It is therefore the exhaustive reference for its own stored-coordinate metric, not for every other policy's normalized metric.

## Guarantee

The ordinary direct path is scan-index exact by definition. It evaluates every entry and preserves the first-index tie rule.

When lookup selection is automatic, a completed palette containing exactly black then white, or white then black, activates an internal monochrome threshold policy. An explicit `none` request is honored before this automatic specialization and therefore selects the exhaustive implementation even for that canonical two-entry shape.

The float monochrome classes consume float samples in the same coordinates as the prepared palette. They copy the midpoint `m = (c[0] + c[1]) / 2` and direction `v = c[1] - c[0]`, then choose entry one only when `dot(x - m, v) > 0`; a tie keeps entry zero. This is unweighted two-entry nearest-color lookup, independent of black/white ordering. Palette padding is respected. A request without a float palette uses the byte palette values as coordinates, matching the direct float scan's fallback; it does not assume a normalized `0..1` scale. Midpoint, direction and query arithmetic use double so finite float excursions do not overflow solely through binary32 accumulation. The copied state belongs to each prepared object and is immutable during queries.

The byte monochrome classes retain their historical threshold `sum(channels) >= 128 * color_count` (with reversed polarity for light background); that legacy threshold is not the two-entry Euclidean midpoint. The float correction does not change this byte compatibility behavior. On the selected extended float path, the specialization remains bypassed as described under [`auto`](auto.md).

## Cost

For palette size `K`:

```text
preparation       O(1)
one query         Theta(K)
P image queries   Theta(P K)
extra storage     O(1)
```

For a SIXEL palette, `K` is bounded, but the factor remains important because the query runs once per non-transparent pixel.

## Name and sources

`none` is a libsixel descriptive name: no lookup acceleration. It is not the claim that palette application does no work.

The implementation is [`lookup-policy-none.c`](../../../../src/lookup-policy-none.c), and selector-level monochrome handling is in [`lookup-policy.c`](../../../../src/lookup-policy.c). The Metal palette-application path, including its matrix-product direct lookup, is in [`gpu-palette-metal.m`](../../../../src/gpu-palette-metal.m).
