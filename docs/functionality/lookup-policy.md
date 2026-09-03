# Lookup Policy

## Definition

"Lookup policy" is libsixel terminology for the strategy used to answer a
repeated nearest-palette-color query:

```text
lookup(color candidate, completed palette) -> palette index
```

It is not a SIXEL wire-format term and does not mean reading a palette register
whose index is already known. Lookup runs during palette application, after
palette construction and before SIXEL byte generation:

```text
loader -> palette construction -> palette application -> SIXEL encoding
                                      ^
                                      |
                                lookup policy
```

The quantization model selected by `-Q` chooses the palette. The dithering
method selected by `-d` changes each color candidate and propagates error. The
lookup policy selected by `-~` maps that candidate to one entry in the completed
palette. See [Encoding Pipeline](encoding-pipeline.md) for the complete flow.

## CLI surface

`img2sixel` selects a policy with:

```text
-~ POLICY
--lookup-policy=POLICY
```

The short option is the two-character spelling `-~`; there is no `--~` long
option. The encoder default is currently `certlut`. An explicit `auto` value is
a dispatch request and is not another search algorithm.

## Policy chapters

Each policy has a separate chapter because the algorithms, correctness
arguments, and dominant costs differ materially:

- [`auto`](lookup-policies/auto.md): selector behavior and fallbacks.
- [`none`](lookup-policies/none.md): exhaustive palette scan.
- [`5bit`](lookup-policies/5bit.md): lazy RGB555 bucket memoization.
- [`6bit`](lookup-policies/6bit.md): lazy RGB666 bucket memoization.
- [`certlut`](lookup-policies/certlut.md): intended cube certification and the
  float32 kd-tree.
- [`eytzinger`](lookup-policies/eytzinger.md): one-dimensional projection in
  an Eytzinger array layout.
- [`fhedt`](lookup-policies/fhedt.md): separable three-dimensional Euclidean
  distance transform.
- [`vptree`](lookup-policies/vptree.md): metric VP-tree and safe previous-color
  cache.
- [`rbc`](lookup-policies/rbc.md): Random Ball Cover cluster pruning.
- [`mahalanobis`](lookup-policies/mahalanobis.md): covariance metadata over RBC
  clusters and the current exhaustive query.

## The shared mathematical problem

For candidate color `x`, palette color `c[i]`, and channel weights `w[d]`, most
accelerated policies minimize a weighted squared Euclidean distance:

```text
D_w(i, x) = sum_d w[d] * (x[d] - c[i][d])^2
index(x) = argmin_i D_w(i, x)
```

There are three channels in accelerated RGB lookup. A direct scan evaluates the
expression for all `K` palette entries. A tree or cluster index discards an
entry only after a lower bound proves that it cannot improve the current best
distance. A grid policy instead maps the continuous or byte-valued color space
to finitely many cells and stores an answer per cell.

The distance itself is part of the policy contract:

- 8-bit policies use unweighted squared distance in byte coordinates.
- float32 `none` uses unweighted squared distance in the stored coordinates.
- other float32 policies normally use `w[d] = 1 / range[d]^2`, so every channel
  is measured relative to its declared numeric range.

Consequently, an exact float32 accelerated search is not necessarily
index-equivalent to `none` in a colorspace whose channel ranges differ. The
search can exactly minimize its normalized metric while `none` exactly
minimizes a different metric. Tests and benchmarks must name both the working
colorspace and the lookup policy.

## Result guarantees

This documentation uses three levels of guarantee:

- **scan-index exact** returns the same index as an exhaustive scan using the
  policy's own distance and first-index tie rule;
- **nearest-distance exact** returns a minimizer of the policy's distance, but
  may choose another palette entry at a tie or floating-point boundary;
- **approximate** may return an entry whose policy distance is greater than the
  exhaustive minimum.

The labels apply to one already-adjusted color candidate. They do not imply
that palette construction, dithering, perceptual quality, or SIXEL output size
is globally optimal.

| Policy and representation | Default guarantee | Important qualification |
| --- | --- | --- |
| `none` | Scan-index exact on its direct path | Canonical two-entry black/white palettes select an internal threshold policy first |
| `5bit` / `6bit`, serial 8-bit | Approximate after a bucket is cached | The first query is exact; later colors in the bucket reuse its index |
| `5bit` / `6bit`, parallel 8-bit | Scan-index exact | The current parallel path does not populate the shared table |
| `5bit` / `6bit`, float32 | Scan-index exact | Exhaustive scan in the normalized policy metric |
| `certlut`, 8-bit | Approximate | The current cube test checks only the center's second-nearest competitor, not every possible boundary competitor |
| `certlut`, float32 | Nearest-distance exact | kd-tree pruning preserves the normalized policy minimum |
| `eytzinger`, 8-bit | Approximate | A fixed projected-neighbor window can omit the true nearest entry |
| `eytzinger`, float32 unit-range RGB/OKLab | Nearest-distance exact | Projection distance is a valid lower bound in these formats |
| `eytzinger`, float32 unequal-range Lab/DIN99d | Approximate | The current projection normalization does not prove a lower bound for the weighted metric |
| `fhedt`, 8-bit at `R < 256` | Approximate | Grid quantization and bounded refinement do not scan the full palette |
| `fhedt`, 8-bit at `R = 256` | Nearest-distance exact | Every byte-valued RGB tuple has a grid coordinate |
| `fhedt`, float32 | Approximate | Even `R = 256` discretizes a continuous working space |
| `vptree` | Nearest-distance exact | Tree pruning and the default safe cache preserve the policy minimum |
| `rbc`, float32 | Nearest-distance exact | Ball-radius lower bounds use the same normalized metric as final comparisons |
| `rbc`, 8-bit | Scan-index exact | The current implementation is a direct scan |
| `mahalanobis`, float32 | Nearest-distance exact | The current query still compares every cluster member in the normalized metric |
| `mahalanobis`, 8-bit | Scan-index exact | The current implementation is a direct scan |

`auto` inherits the concrete backend's guarantee. Configuration can matter as
much as the base policy name. For example, an optional FHEDT cache remembers a
voxel result, while the serial dense-bucket policies remember the result of the
first actual color that entered a bucket.

## Cost model

The policy chapters use these variables:

- `P`: non-transparent pixels mapped to the palette;
- `K`: completed palette entries;
- `R`: grid resolution on each of three axes;
- `G = R^3`: grid cells;
- `U`: distinct lazy buckets or cells first encountered;
- `J = min(K, 16)`: the current RBC pivot count;
- `V`: candidates that survive a query's bounds.

The fixed three color dimensions are omitted. Preparation cost, one-query cost,
and whole-image cost must be kept separate.

| Policy | Preparation | One query | Extra space |
| --- | --- | --- | --- |
| `none` | `O(1)` | `Theta(K)` | `O(1)` |
| `5bit` / `6bit`, serial 8-bit | `Theta(G)` initialization | hit `Theta(1)`; first-bucket miss `Theta(K)` | `Theta(G)` |
| `certlut`, 8-bit | `O(64^3 + K^2)` in the current builder | warm fixed-depth lookup `Theta(1)`; cold refinement worst `O(K)` | initial `Theta(64^3 + K)`; fully refined worst `O(256^3 + K)` |
| `certlut`, float32 | current kd-tree build conservatively `O(K^2)` | typical `O(log K)`; worst `Theta(K)` | `Theta(K)` |
| `eytzinger` | `Theta(K log K)` | 8-bit `Theta(log K)`; float32 `O(log K + V)`, worst `Theta(K)` | `Theta(K)` |
| `fhedt` | `Theta(R^3 + K)` | `Theta(1)` | `Theta(R^3 + K)` |
| `vptree` | `Theta(K^2)` safe-radius preparation plus tree construction | typical balanced `O(log K)`; worst `Theta(K)` | `Theta(K)` |
| `rbc`, float32 | `Theta(K J)` | `O(J + V)`; worst `Theta(K)` | `Theta(K + J)` |
| `mahalanobis`, float32 | `Theta(K J)` | `Theta(K)` currently | `Theta(K + J)` |

Direct lookup over an image is `Theta(P K)`. The serial `5bit` and `6bit`
implementations instead have whole-image work:

```text
Theta(G + P + U K)
```

FHEDT has the clearest fixed-build-plus-linear-application form:

```text
Theta(R^3 + K + P)
```

A query being typically logarithmic does not establish a logarithmic worst
case. VP-tree, kd-tree, projection, and RBC pruning can all visit most or all of
the palette for unfavorable geometry. Likewise, lookup is linear in `P = W H`
but quadratic in the side length of an `n`-by-`n` image; always name the
independent variable.

## Comparison with other implementations

This comparison is about applying an already-built palette. A data structure
used while generating a palette belongs to the `-Q` stage and is a separate
comparison.

| Implementation | Palette-application strategy | Relation to libsixel |
| --- | --- | --- |
| `a-sixel` | Lab kd-tree for every builder except `PaletteBuilder::Bit`; that builder uses a bit-dilation LUT | Closest to tree-based exact lookup, with a dedicated fast path coupled to one palette builder |
| `go-sixel` | Eager RGB555 table built by scanning the palette at each cell center | Similar address space to `5bit`, but eager and center-representative rather than lazy and first-query-representative |
| `libsixel/libsixel` fork | Lazy RGB555 hash/LUT in its current `quant.c` | Shares the historical 5-bit bucket design lineage |
| `libimagequant` / pngquant | VP-tree palette remapping with a previous-result hint | Not a SIXEL encoder, but a direct influence on libsixel's VP-tree safe cache |

The current `a-sixel` source constructs a
[`KdTreeBucketer` for all non-`Bit` palette builders](https://github.com/Jesterhearts/a-sixel/blob/e2ffc9aa674aeb00779d2b8d5a5bd7ddb4615022/src/lib.rs#L285-L296)
and performs [squared-Euclidean nearest-neighbor queries in Lab](https://github.com/Jesterhearts/a-sixel/blob/e2ffc9aa674aeb00779d2b8d5a5bd7ddb4615022/src/dither.rs#L76-L108).
Its [`BitPaletteBucketer`](https://github.com/Jesterhearts/a-sixel/blob/e2ffc9aa674aeb00779d2b8d5a5bd7ddb4615022/src/bit.rs#L116-L190)
is the exception. `a-sixel` also uses kd-trees inside some palette builders;
that does not change which structure applies the completed palette.

`go-sixel` builds all `2^15` RGB555 entries eagerly. Each entry represents the
center of an 8-by-8-by-8 byte cube, while source pixels select a cell by their
top five bits; see [`newPaletteLUT` and `lutIndex`](https://github.com/mattn/go-sixel/blob/ceaaab1e2b5973d9ebc28b0a615760b921e90681/sixel.go#L676-L712).
libsixel's `5bit` policy instead scans the actual first query color in a bucket
and memoizes that result, making its approximation dependent on traversal
history.

The `libsixel/libsixel` fork likewise computes an
[RGB555 hash from the top five channel bits](https://github.com/libsixel/libsixel/blob/ee77c2f979dbccc86651a482e748fe7bf80bc89c/src/quant.c#L667-L675)
and [fills an empty entry from the first palette scan that reaches it](https://github.com/libsixel/libsixel/blob/ee77c2f979dbccc86651a482e748fe7bf80bc89c/src/quant.c#L1091-L1139).
Unlike that truncating hash, the current libsixel `5bit` policy rounds before
packing its bucket coordinates.

The VP-tree chapter gives the exact libimagequant source links used to trace
the previous-result optimization. These implementation comparisons are pinned
to source commits so later algorithm changes do not silently rewrite the
meaning of this document.

## Quality and performance

Approximate lookup is not synonymous with lower end-to-end quality. Exact
nearest lookup minimizes only the current candidate's chosen distance. It does
not directly minimize spatial artifacts, temporal instability, or encoded
size. In a smooth gradient, controlled reuse of a recent index can suppress
index chatter, produce longer runs, and change error-diffusion feedback. That
can improve speed, a perceptual metric, and SIXEL size simultaneously. It can
also introduce banding or bias, so no such improvement is guaranteed.

With error diffusion, one changed index changes the error carried to later
pixels. A small local lookup difference can therefore create a larger spatial
difference in the final image. Evaluate lookup changes on at least these axes:

- palette-index agreement and distance regret against the appropriate
  exhaustive metric;
- decoded-image quality, including gradients and error-diffusion stability;
- encoded byte size and palette-index transition or run statistics;
- preparation time, steady-state query time, and total time;
- memory footprint and worker-sharing behavior.

## Design rules

- Derive a lookup instance from a completed palette; do not choose or mutate
  the palette in this stage.
- Keep the per-pixel interface conceptually stateless. Explicit caches are
  policy state, while scan order and error propagation belong to dithering.
- Treat worker sharing as a lifecycle and thread-safety decision, not only a
  speed switch.
- Test lookup indexes and distances directly before relying on encoded bytes or
  perceptual scores.
- When a backend, metric, cache threshold, representation, or parallel path
  changes, update this guarantee table and the affected policy chapter.
- Keep policy names and suboptions synchronized through the central option
  registry.

## Implementation and tests

Selection is in [`lookup-policy.c`](../../src/lookup-policy.c). Concrete policy
classes are the `src/lookup-policy-*.c` translation units, with some larger
backends split into `src/lookup-*.c`. Their interface is declared in
[`6cells.h`](../../include/6cells.h).

Direct and end-to-end coverage is under
[`tests/quant/palette/usage/`](../../tests/quant/palette/usage/). For the caller
of this interface, see [Dithering](dithering.md).
