# `none` Lookup Policy

## Purpose and intuition

`none` is the exhaustive-search mental model: compare the candidate with every
palette color and keep the best index. It disables lookup acceleration, not
palette construction, dithering, or palette application.

```text
candidate x
    |
    +--> distance(x, c[0]) --+
    +--> distance(x, c[1]) --+--> smallest distance --> palette index
    |          ...           |
    +--> distance(x, c[K-1]) +
```

`-d none` is independent. This command disables both dither adjustment and
lookup acceleration:

```text
img2sixel -d none --lookup-policy=none input.png
```

## Mathematics

For an 8-bit or float32 candidate `x` with stored palette entries `c[i]`, the
current direct scan uses:

```text
D(i, x) = sum_d (x[d] - c[i][d])^2
```

The scan updates its result only for a strict improvement. If two entries have
equal distance, the lower palette index wins because it was visited first.

Unlike the accelerated float32 policies, `none` does not divide each channel
by its declared range. It is therefore the exhaustive reference for its own
stored-coordinate metric, not for every other policy's normalized metric.

## Guarantee

The ordinary direct path is scan-index exact by definition. It evaluates every
entry and preserves the first-index tie rule.

There is one selector-level exception: a completed palette containing exactly
black then white, or white then black, activates an internal monochrome
threshold policy before the selector checks `none`. Tests that specifically
need the exhaustive implementation must avoid that canonical two-entry shape
or instantiate the policy class directly.

## Cost

For palette size `K`:

```text
preparation       O(1)
one query         Theta(K)
P image queries   Theta(P K)
extra storage     O(1)
```

For a SIXEL palette, `K` is bounded, but the factor remains important because
the query runs once per non-transparent pixel.

## Name and source

`none` is a libsixel descriptive name: no lookup acceleration. It is not the
claim that palette application does no work.

The implementation is
[`lookup-policy-none.c`](../../../src/lookup-policy-none.c), and selector-level
monochrome handling is in
[`lookup-policy.c`](../../../src/lookup-policy.c).

Return to the [Lookup Policy index](../lookup-policy.md).
