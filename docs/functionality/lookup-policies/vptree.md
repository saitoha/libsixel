# `vptree` Lookup Policy

## Purpose and intuition

A vantage-point tree organizes a metric space by distance from a chosen pivot.
Each node stores one palette color and a median radius. Colors inside that
radius go to one subtree; colors outside it go to the other.

```text
                         pivot p
                    median radius mu
                       /          \
              d(c,p) < mu      d(c,p) >= mu
                 near              far
               subtree           subtree
```

A query visits the side containing its distance from the pivot first. The
triangle inequality determines whether the current best-radius ball intersects
the other side. If not, that entire subtree is skipped.

## Tree construction

For the current set of palette indices, libsixel chooses the last index as the
pivot, computes every remaining distance to it, sorts those distances, and
splits at the median:

```text
distances from pivot:  2  4  5 | 8  9  13
                                 ^
                           median radius
```

Recursive median splits tend to balance the number of nodes, although search
work still depends on metric geometry. The current builder allocates and sorts
each recursive subset rather than using a globally optimal construction.

## Branch-and-bound mathematics

Let `t = d(x,p)` be query-to-pivot distance, `mu` the node radius, and `tau` the
best distance found so far.

If `t < mu`, the near side is searched first. The far side can contain a better
point only if:

```text
mu - t <= tau
```

If `t >= mu`, the far side is searched first and the near side is necessary
only if:

```text
t - mu <= tau
```

These are consequences of the triangle inequality. The implementation stores
squared full distances but takes square roots where it compares them with the
node radius.

## Safe previous-color cache

Serial traversal offers the previous result as a likely palette index. Let
`c[i]` be that index and let:

```text
s[i] = min_(j != i) d(c[i], c[j])
```

If the new candidate satisfies:

```text
d(x, c[i]) <= s[i] / 2
```

then for every other `c[j]`:

```text
d(x, c[j]) >= d(c[i], c[j]) - d(x, c[i])
             >= s[i] - s[i]/2
             >= d(x, c[i])
```

So `c[i]` is still a nearest color. In squared-distance storage the threshold
is:

```text
safe_dist2[i] = nearest_palette_dist2[i] / 4
```

This test is also applied to pivots encountered during traversal. It is a proof
radius, not an approximation tolerance. The cache is disabled during parallel
dithering because a single previous-color history is not meaningful across
independent worker traversal.

## Guarantee

The current VP-tree is nearest-distance exact in its policy metric, both with
and without the safe cache. The cache and tree pruning follow metric bounds.
Tie index can differ from the first-index order of an exhaustive scan.

If the cache threshold were enlarged beyond `s[i]/2`, the proof would no
longer hold and the result would become approximate. That could still improve
an image: in a smooth gradient, deliberate index stability can reduce palette
chatter, extend SIXEL runs, and alter error-diffusion feedback beneficially.
It can also add banding. Exactness, perceptual quality, speed, and encoded size
must therefore be measured separately.

For float32 unequal-range formats, this exactness is relative to the
range-normalized metric, not the unnormalized metric used by `none`.

## Cost

Computing `s[i]` compares every palette pair and dominates current preparation:

```text
safe-radius preparation     Theta(K^2)
tree construction           at least Theta(K log K), with recursive allocation
typical balanced query      O(log K)
worst query                 Theta(K)
safe-cache hit              Theta(1)
extra space                 Theta(K)
```

Calling the lookup logarithmic without qualification would be misleading. A
query ball can intersect both sides repeatedly, especially when the metric has
weak separation.

## Name and sources

`vptree` means vantage-point tree. Peter N. Yianilos introduced the name and
algorithms in
[“Data Structures and Algorithms for Nearest Neighbor Search in General Metric Spaces”](https://citeseerx.ist.psu.edu/document?doi=ef5f36cb8ed5a1bdd2f564d65b87ce88e0536c1a&repid=rep1&type=pdf).

The previous-index design was informed by libimagequant's
[VP-tree nearest-color search and nearest-other-color radius](https://github.com/ImageOptim/libimagequant/blob/2883dbd955fe9007bf33d7b64f6eafaf9fec4d6a/nearest.c#L128-L225)
and its [remapping loop, which passes the previous result as a guess](https://github.com/ImageOptim/libimagequant/blob/2883dbd955fe9007bf33d7b64f6eafaf9fec4d6a/remap.c#L165-L220).
libsixel's exact threshold and lifecycle remain its own implementation
contract.

The policy entered libsixel in the
[`vp-tree` implementation commit](https://github.com/saitoha/libsixel/commit/e980610a9ee86ae7638d0375d37c0871ce6e1ab7).
The wrapper is
[`lookup-policy-vptree.c`](../../../src/lookup-policy-vptree.c), with 8-bit and
float32 backends in the corresponding `src/lookup-vptree-*.c` files.

Return to the [Lookup Policy index](../lookup-policy.md).
