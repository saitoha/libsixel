# Lookup Policy

## Definition

"Lookup policy" is libsixel terminology for the strategy used to answer a
repeated nearest-palette-color query:

```text
lookup(color candidate, completed palette) -> palette index
```

It is not a SIXEL wire-format term and does not mean reading a palette register
by an already known index. It is the mapping from a source or dither-adjusted
color to the index that will represent that color.

Lookup runs during palette application, after palette construction. A policy
may build an index or cache from the palette and then answer one query for each
non-transparent pixel. Its design trades among preparation time, per-pixel
cost, memory, sharing between workers, supported colorspaces, and exact or
discretized mapping behavior.

## CLI surface

`img2sixel` selects a policy with:

```text
-~ POLICY
--lookup-policy=POLICY
```

The short option is the two-character spelling `-~`; there is no `--~` long
option. The base policies are:

| Policy | Strategy |
| --- | --- |
| `auto` | Lets the encoder select a backend for the active pixel representation |
| `none` | Builds no lookup cache and scans the palette directly |
| `5bit` | Uses a dense table with five address bits per RGB channel |
| `6bit` | Uses a finer dense table with six bits per RGB channel for RGB input |
| `certlut` | Uses a hierarchical LUT whose refinement certifies the nearest color |
| `eytzinger` | Searches an implicit binary-tree layout and projected neighbors |
| `fhedt` | Builds a Voronoi grid with a three-pass 3D distance transform |
| `vptree` | Uses a metric VP-tree with safe-distance pruning |
| `rbc` | Uses Random Ball Cover cluster pruning |
| `mahalanobis` | Builds RBC covariance metadata; the current query path still scans all members |

Some policies expose suboptions and environment tuning for table layout,
resolution, refinement, or sharing. Use `img2sixel -H` or the
[`img2sixel(1)` manual](../../converters/img2sixel.1) for the current detailed
surface.

## What `none` means

`--lookup-policy=none` is the reference mental model: compare the candidate
with palette entries directly and return the best index. It disables lookup
acceleration, not color reduction, palette application, or dithering.

This differs from `-d none`, which disables dither adjustment and error
propagation but still needs a lookup for each pixel. For example:

```text
-d none --lookup-policy=none
```

means "map each original pixel by a direct palette scan." Either option may be
changed independently.

## Mathematical problem

For candidate color `x`, palette color `c[i]`, and channel weights `w[d]`,
the reference weighted squared distance is:

```text
D(i, x) = sum_d w[d] * (x[d] - c[i][d])^2
index(x) = argmin_i D(i, x)
```

RGB dimensionality is fixed at three. A direct scan evaluates this expression
for every palette entry. Search trees try to prove that some entries cannot
beat the current best distance. Projection search uses the distance along one
axis as a lower bound; metric trees use the triangle inequality; RBC uses a
pivot distance and cluster radius. Those proofs can make a query short on
well-separated data, but a lower average count does not imply a logarithmic
worst case.

Dense grid policies solve a related discretized problem. They quantize `x` to
a cell and cache or precompute a palette answer for that cell. Lookup can then
be constant-time, at the cost of table construction, memory, and possible
differences at cell boundaries.

## Cost model and asymptotic order

Use these variables when discussing lookup cost:

- `P`: non-transparent pixels mapped to the palette;
- `K`: palette entries;
- `R`: lookup-grid resolution on each RGB axis;
- `G = R^3`: cells in a dense RGB grid;
- `U`: distinct lazy cells first encountered during an image;
- `J = min(K, 16)`: current RBC pivot count;
- `V`: projected or tree candidates actually visited by a query.

The fixed three color dimensions are omitted. Preparation, one query, and the
whole-image total must be kept separate:

| Policy and representation | Preparation | One query | Extra space |
| --- | --- | --- | --- |
| `none` | `O(1)` beyond retaining the palette | `Theta(K)` | `O(1)` beyond the palette |
| `5bit` / `6bit`, 8-bit RGB | `Theta(G)` table initialization | cache hit `Theta(1)`; first-cell miss `Theta(K)` | `Theta(G)` |
| `certlut`, 8-bit RGB | `O(64^3 + K^2)` top-level initialization and current kd-tree build | warmed fixed-depth cell `Theta(1)`; cold refinement worst `O(K)` | initial `Theta(64^3 + K)`; fully refined worst `O(256^3 + K)` |
| `eytzinger`, 8-bit | `Theta(K log K)` projection sort and layout | `Theta(log K)` with the fixed neighbor window | `Theta(K)` |
| `eytzinger`, float32 | `Theta(K log K)` projection sort and layout | `O(log K + V)`; worst `Theta(K)` | `Theta(K)` |
| `fhedt` | `Theta(R^3 + K)` three-pass distance transform | `Theta(1)` | `Theta(R^3 + K)` |
| `vptree` | `Theta(K^2)` safe-distance preparation, followed by tree construction | typical balanced search `O(log K)`; worst `Theta(K)` | `Theta(K)` |
| `rbc`, float32 | `Theta(K J)` pivot assignment and cluster data | `O(J + V)`; worst `Theta(K)` | `Theta(K + J)` |
| `mahalanobis`, float32 | `Theta(K J)` clusters, means, and 3-by-3 inverse covariances | `Theta(K)` in the current implementation | `Theta(K + J)` |

For direct lookup, whole-image work is `Theta(P K)`. For the serial 8-bit
`5bit` and `6bit` implementations, lazy memoization gives:

```text
Theta(G + P + U K)
```

where `R` is 32 or 64 respectively. The table is allocated and initialized
up front, but a bucket's nearest palette entry is computed only on its first
use. When parallel dithering is active, the current implementation does not
write those memoized answers, so repeated queries cannot assume the same
amortized bound. The float32 implementations of these two policies currently
fall back to a direct `Theta(K)` scan.

The 8-bit `certlut` similarly combines a 64-by-64-by-64 top-level table with
lazy hierarchical refinement and cube certification. Its warmed lookup has a
fixed depth, while first use of an uncertified region can require a
palette-wide nearest search. The float32 `certlut` path instead uses a
kd-tree: a balanced query is typically `O(log K)` but has `Theta(K)` worst
case, and the current recursive construction has a conservative `O(K^2)`
bound.

`eytzinger` sorts palette colors by a weighted one-dimensional projection,
stores them in implicit binary-tree order, and finds the projected insertion
point in `Theta(log K)`. The 8-bit path examines a fixed window around that
point, so it stays logarithmic in `K` but is an approximate candidate search.
The float32 path instead scans outward while the squared projection difference
can still beat the best full distance. That stopping proof preserves an exact
weighted-Euclidean answer, but `V` can grow to `K` when the projection cannot
separate the palette.

`fhedt` is the clearest fixed-precomputation case. It computes a three-
dimensional squared-Euclidean Voronoi grid with three separable one-dimensional
distance-transform passes. At a selected `R` of 64, 128, or 256, total
application cost is:

```text
Theta(R^3 + K + P)
```

For fixed `R` and `K` this is linear in the number of image pixels after a
fixed table-build cost. Optional boundary refinement examines at most eight
cell-corner candidates, which remains constant per query.

The VP-tree and RBC paths are data-dependent pruning structures. A balanced,
well-separated palette often produces near-logarithmic VP-tree traversal, but
the metric worst case visits every entry. RBC compares at most `J` pivots and
then searches clusters whose radius bound can still win; if every bound
survives, it also scans all `K` entries. The 8-bit RBC path currently uses the
direct linear scan.

The float32 `mahalanobis` path prepares per-cluster means and inverse
covariances and evaluates a Mahalanobis quadratic form. In the current code
that value is not used to reject a cluster: every member of every cluster is
still compared with the reference weighted-Euclidean distance. Its present
query order is therefore `Theta(K)`, not logarithmic. The 8-bit path is also a
direct scan.

`auto` has no single complexity class because it resolves to one of the
concrete backends. Published benchmark results must name the resolved policy,
pixel representation, table resolution, and whether lazy state was shared.

## Quality and performance

Lookup preparation can dominate a small image, while query throughput can
dominate a large image. Benchmark those phases separately and include palette
size, image size, colorspace, precision, thread count, and cache-sharing policy
in the result.

As with dithering, the independent variable matters. A lookup that is linear
in `P = W H` is quadratic in side length `n` for an `n`-by-`n` image. Calling
that a "quadratic lookup" would obscure the fact that it still performs
constant work per pixel.

Dense bucket policies reduce the query color before indexing a finite table and
can therefore select a different entry from a direct scan. Other algorithms
have their own refinement and pruning contracts. Do not classify a policy as a
drop-in speed improvement until tests establish the required index equivalence
or an explicit image-quality threshold.

With error diffusion, one changed index also changes the error carried to later
pixels. A small local lookup difference can therefore create a larger spatial
difference in the final image.

## Design rules

- Keep policy instances derived from a completed palette; they must not choose
  or mutate the palette.
- Keep the per-pixel interface conceptually stateless. Image traversal and
  error propagation belong to the dither policy.
- Treat worker sharing as a lifecycle and thread-safety decision, not merely a
  performance flag.
- Test index behavior directly before relying only on encoded bytes or
  perceptual scores.
- Keep policy names and suboptions synchronized through the central option
  registry.

## Implementation and tests

Selection is in [`lookup-policy.c`](../../src/lookup-policy.c), and individual
backends are implemented by the `src/lookup-policy-*.c` translation units.
Their interface explicitly owns mapping pixels to palette indexes and forbids
image or output ownership; see [`6cells.h`](../../include/6cells.h).

Direct and end-to-end coverage is under
[`tests/quant/palette/usage/`](../../tests/quant/palette/usage/). For the caller
of this interface, see [Dithering](dithering.md); for the complete data flow,
see [Encoding Pipeline](encoding-pipeline.md).
