# Lookup Policy

This reference is for CLI users who choose `-~` and for contributors who change a lookup backend. It owns the interface, the shared distance metric, the guarantee levels, the cost variables, and the test coverage of every lookup policy. Each policy has its own page, listed under [Variants](#variants); the measurements are in the [measurement companion](measurements/README.md). Palette construction is described in [Palette quantization](../../palette-construction/quantization/README.md) and error propagation in [Dithering](../dithering/README.md).

"Lookup policy" is libsixel terminology primarily for the strategy used to answer a repeated nearest-palette-color query:

```text
lookup(color candidate, completed palette) -> palette index
```

It is not a SIXEL wire-format term and does not mean reading a palette register whose index is already known.

## Pipeline position

Conceptually, lookup runs during palette application, after palette construction and before SIXEL byte generation:

```text
loader -> palette construction -> palette application -> SIXEL encoding
                                      ^
                                      |
                                lookup policy
```

The quantization model selected by `-Q` chooses the palette. The dithering method selected by `-d` changes each color candidate and propagates error. The lookup policy selected by `-~` maps that candidate to one entry in the completed palette. See [Encoding Pipeline](../../pipeline/README.md) for the complete flow.

## Options

`img2sixel` selects a policy with:

```text
-~ POLICY
--lookup-policy=POLICY
```

The short option is the two-character spelling `-~`; there is no `--~` long option. The encoder default is `auto`, which currently resolves to `vptree`. An explicit `auto` value is a dispatch request and is not another search algorithm.

An explicit lookup policy applies during palette application with generated palettes and with all three fixed-palette selectors: mapfiles selected by `-m`, built-in palettes selected by `-b`, and monochrome selected by `-e`. Outside the selected extended float path, a text mapfile retains its historical direct-scan behavior when no lookup policy is explicitly supplied; explicit lookup is the opt-in that enables the selected backend for that already completed palette.

RGB666 became the fast lookup baseline during development of the 1.11 line, when commit [`4f57a743c`](https://github.com/saitoha/libsixel/commit/4f57a743c11c4149656cdad6e3f8d59d76c58036) replaced the historical RGB555 table. It remains the selector fallback for an unrecognized policy value, but it is no longer what `auto` selects. Commit [`2e1cc0639`](https://github.com/saitoha/libsixel/commit/2e1cc06396009a452f25ecd9baf078e16e0d4d27) changed the built-in default from `eytzinger` to `certlut`; the default later became `auto`, resolved to `vptree`. Statements that call `6bit` "the 1.11 default" must therefore name the exact revision or release; this document describes the current source tree.

`-U display-p3` accepts `none`, `certlut`, `5bit`, `6bit`, `auto` (resolved to `vptree`), `vptree`, `delaunay`, `blockcull`, `gridlut`, `ordered`, `rbc`, `mahalanobis`, `eytzinger` and `fhedt`. On that path, and on the ordinary sRGB float path that uses the same preparation, lookup retains the unrounded float working palette; separately converted and rounded RGB101 codes serve only serialization; see [Output color space](../../output/output-colorspace.md#display-p3-selected-cpu-float-path).

## Contract

This documentation uses three levels of guarantee:

- **scan-index exact** returns the same index as an exhaustive scan using the policy's own distance and first-index tie rule;
- **nearest-distance exact** returns a minimizer of the policy's distance, but may choose another palette entry at a tie or floating-point boundary;
- **approximate** may return an entry whose policy distance is greater than the exhaustive minimum.

The labels apply to one already-adjusted color candidate. They do not imply that palette construction, dithering, perceptual quality, or SIXEL output size is globally optimal.

| Policy and representation | Default guarantee | Important qualification |
| --- | --- | --- |
| `none` | Scan-index exact on its direct path | Canonical two-entry black/white palettes select an internal threshold policy first |
| `5bit` / `6bit`, 8-bit | Approximate after a bucket is cached | The first completed query is exact; shared parallel writers can choose different valid representatives, so repeated runs need not be byte-reproducible |
| `5bit` / `6bit`, float32 | Scan-index exact | Exhaustive scan in the normalized policy metric |
| `certlut`, 8-bit | Approximate | The current cube test checks only the center's second-nearest competitor, not every possible boundary competitor |
| `certlut`, float32 | Nearest-distance exact | kd-tree pruning preserves the normalized policy minimum |
| `eytzinger`, 8-bit | Approximate | A fixed projected-neighbor window can omit the true nearest entry |
| `eytzinger`, float32 | Nearest-distance exact | Dual-metric projection bound with conservative rounding allowance |
| `fhedt`, 8-bit at `R < 256` | Approximate | Grid quantization and bounded refinement do not scan the full palette |
| `fhedt`, 8-bit at `R = 256` | Nearest-distance exact | Every byte-valued RGB tuple has a grid coordinate |
| `fhedt`, float32 | Approximate | Even `R = 256` discretizes a continuous working space |
| `delaunay` | Scan-index exact | Closed Voronoi graph walk with lowest-index ties and exhaustive float fallback outside its box |
| `gridlut` | Scan-index exact | Fixed RGB cells retain all possible winners; float32 delegates to blockcull |
| `ordered` | Scan-index exact | Distance-sorted byte candidates with exact radius pruning; float32 delegates to blockcull |
| `blockcull` | Scan-index exact | Colors that cannot be nearest anywhere in a run or pixel block are skipped before a palette-order scan |
| `vptree` | Nearest-distance exact | Tree pruning and the default safe cache preserve the policy minimum |
| `rbc`, float32 | Nearest-distance exact | Ball-radius lower bounds use the same normalized metric as final comparisons |
| `rbc`, 8-bit | Scan-index exact | The current implementation is a direct scan |
| `mahalanobis`, float32 | Nearest-distance exact | The current query still compares every cluster member in the normalized metric |
| `mahalanobis`, 8-bit | Scan-index exact | The current implementation is a direct scan |

`auto` inherits the concrete backend's guarantee. Configuration can matter as much as the base policy name. For example, an optional FHEDT cache remembers a voxel result, while the dense-bucket policies remember the result of the first actual color that entered a bucket.

### Extended float coordinates

The selected [CPU float palette path](../../output/output-colorspace.md#display-p3-selected-cpu-float-path) retains negative, above-nominal and fractional coordinates in `none`, `certlut`, `5bit`, `6bit`, `vptree`, `delaunay`, `blockcull`, `gridlut`, `ordered`, `rbc`, `mahalanobis`, `eytzinger` and `fhedt` lookup. The separately clipped RGB101 wire palette does not replace these working entries. FHEDT retains these coordinates separately from its approximate finite grid, as described in [FHEDT](fhedt.md).

`certlut`, `5bit`, `6bit`, `vptree`, `delaunay`, `blockcull`, `gridlut`, `ordered`, `rbc`, `mahalanobis`, `eytzinger` and `fhedt` derive their normalized metric from the declared channel ranges, rather than the observed palette extent. [FHEDT](fhedt.md#float-palette-domain-and-metric) additionally applies its configurable channel ratios, including L emphasis by default in perceptual spaces. Unlike `none`, this rescales the opponent axes in CIELAB and DIN99d. Float storage does not mean float arithmetic throughout: these classes subtract, square and accumulate distances in double. Construction metadata uses the same precision: `certlut` split-plane bounds, VP-tree radii and safe-cache thresholds, RBC ball radii, and Eytzinger projection keys and stopping bounds must not narrow distances back to float before pruning. This avoids overflow and underflow of squared distances for finite binary32 coordinates without clipping them. Working palette values and nominal metric coefficients retain float32 precision. Most classes store float32 search copies; `mahalanobis`, `eytzinger` and `fhedt` widen those exact values into a private double copy at preparation to avoid repeated conversion during candidate comparisons. It does not promise exact real arithmetic or identical index choice at numerically indistinguishable ties. General error-budget rules are in the [testing guide](../../../testing/guide.md#intermediate-floating-point-error).

Prepared float lookup objects own their palette copies. Repreparing one instance replaces its palette and search structure without changing another instance. For `certlut`, `5bit` and `6bit`, worker sharing is limited to immutable prepared float data. VP-tree, Delaunay, blockcull, gridlut, ordered, RBC, Mahalanobis and Eytzinger use worker-local instances. VP-tree and Delaunay keep previous-index hints in the caller, so their prepared search data retains no scan history. No query mutates the working palette or its wire cache.

## Precedence and interactions

There is one important historical compatibility exception. In the current 8-bit Heckbert path, `-~` also selects the resolution of the dense histogram used to construct the palette:

| Policy group | RGB histogram resolution |
| --- | --- |
| `5bit` | `32^3` cells, five address bits per channel |
| `6bit`, `certlut`, and the other accelerated policies | `64^3` cells, six address bits per channel |
| `none` | `256^3` cells, eight address bits per channel |

Changing `-~` can therefore change both the generated palette and its application when `-Q heckbert` is active. This coupling descends from the original RGB555 quantizer and is retained for compatibility; it is not a general requirement for lookup-policy implementations. Comparisons must state whether they hold the palette fixed or measure this end-to-end behavior.

## The shared mathematical problem

For candidate color `x`, palette color `c[i]`, and channel weights `w[d]`, most accelerated policies minimize a weighted squared Euclidean distance:

```text
D_w(i, x) = sum_d w[d] * (x[d] - c[i][d])^2
index(x) = argmin_i D_w(i, x)
```

There are three channels in accelerated RGB lookup. A direct scan evaluates the expression for all `K` palette entries. A tree or cluster index discards an entry only after a lower bound proves that it cannot improve the current best distance. A grid policy instead maps the continuous or byte-valued color space to finitely many cells and stores an answer per cell.

The distance itself is part of the policy contract:

- 8-bit policies use unweighted squared distance in byte coordinates.
- float32 `none` uses unweighted squared distance in the stored coordinates.
- other float32 policies normally use `w[d] = 1 / range[d]^2`, so every channel is measured relative to its declared numeric range.

Consequently, an exact float32 accelerated search is not necessarily index-equivalent to `none` in a colorspace whose channel ranges differ. The search can exactly minimize its normalized metric while `none` exactly minimizes a different metric. Tests and benchmarks must name both the working colorspace and the lookup policy.

## Comparison with other implementations

This comparison is mainly about applying an already-built palette. A data structure used while generating a palette belongs to the `-Q` stage and is a separate comparison. Netpbm is included because the separation between its palette builder and remapper is historically important to libsixel.

| Implementation | Palette-application strategy | Relation to libsixel |
| --- | --- | --- |
| `a-sixel` | Lab kd-tree for every builder except `PaletteBuilder::Bit`; that builder uses a bit-dilation LUT | Closest to tree-based exact lookup, with a dedicated fast path coupled to one palette builder |
| `go-sixel` | Eager RGB555 table built by scanning the palette at each cell center | Similar address space to `5bit`, but eager and center-representative rather than lazy and first-query-representative |
| `libsixel/libsixel` fork | Lazy RGB555 hash/LUT in its current `quant.c` | Shares the historical 5-bit bucket design lineage |
| `libimagequant` / pngquant | VP-tree palette remapping with a previous-result hint | Not a SIXEL encoder, but a direct influence on libsixel's VP-tree safe cache |
| Netpbm `pnmquant` / `pnmremap` | Exact-tuple hash memoization; each cache miss exhaustively scans all `K` palette entries with squared Cartesian distance | Like `none` on a cold 8-bit RGB tuple, but repeated identical tuples are cached without merging neighboring colors |

### Netpbm `pnmquant` and `pnmremap`

The phrase "closest color" hides the important part of the implementation. The [`pnmquant` wrapper at Netpbm SVN r4306](https://sourceforge.net/p/netpbm/code/4306/tree/trunk/editor/pnmquant#l267) runs `pnmcolormap` to construct a palette and then [`pnmremap` to apply it](https://sourceforge.net/p/netpbm/code/4306/tree/trunk/editor/pnmquant#l316). The remapper does not use a kd-tree, VP-tree, palette LUT, projection, or branch-and-bound pruning. Its lookup path at SVN r4732 is:

```text
normalized tuple x
        |
        v
exact tuple hash ---- hit ----> cached palette index
        |
       miss
        v
scan palette entries 0 ... K - 1
        |
        v
cache the complete tuple x and its selected index
```

On a cache miss, [`searchColormapClose`](https://sourceforge.net/p/netpbm/code/4732/tree/trunk/editor/pnmremap.c#l660) computes

```text
                         D - 1
d_q(x, c_i) =             sum  floor((x_j - c_i,j)^2 / q)
                          j=0
```

for every palette entry `i`. Here `D` is tuple depth and `q` is the `distanceDivider` selected to prevent `unsigned int` overflow. The loop keeps the first palette entry on a tie because it replaces the best index only for a strictly smaller distance. A cold lookup is therefore `Theta(K D)`, or `Theta(K)` for fixed-depth RGB.

[`lookupThroughHash`](https://sourceforge.net/p/netpbm/code/4732/tree/trunk/editor/pnmremap.c#l757) first probes an exact-tuple hash and inserts the result after a miss. The [hash implementation at SVN r5304](https://sourceforge.net/p/netpbm/code/5304/tree/trunk/lib/libpammap.c#l28) has 20,023 chained buckets. It computes a bucket from at most the first three planes, but [compares the complete tuple before declaring a hit](https://sourceforge.net/p/netpbm/code/5304/tree/trunk/lib/libpammap.c#l157). It is therefore exact memoization, not a reduced-bit approximation like libsixel's `5bit` and `6bit` policies. For `U` distinct lookup tuples, expected cache-hit time is `O(1)` subject to hash-chain length, worst-case hit time is `O(U)`, and additional space is `O(20023 + U D)`. Insertions continue until an allocation fails; 20,023 is the bucket count rather than an entry limit.

For an ordinary 8-bit RGB image, `maxval = 255`, `D = 3`, and `q = 1`, so the full scan preserves the exact minimum of squared Cartesian RGB distance and the hash preserves that result for the identical tuple. For sufficiently high `maxval` or depth, [`createColormapFinder` raises `q`](https://sourceforge.net/p/netpbm/code/4732/tree/trunk/editor/pnmremap.c#l621). Integer division is applied to each plane before summation, so candidates can collapse to a tie and the selected entry need not minimize the unscaled distance; the Netpbm source explicitly calls this case approximate. When Floyd--Steinberg dithering is enabled, the key is the normalized, dither-adjusted tuple, so equal source pixels need not produce hash hits after different propagated errors. Neither dithering nor the hash changes the cold lookup strategy: it remains an exhaustive palette scan.

The current `a-sixel` source constructs a [`KdTreeBucketer` for all non-`Bit` palette builders](https://github.com/Jesterhearts/a-sixel/blob/e2ffc9aa674aeb00779d2b8d5a5bd7ddb4615022/src/lib.rs#L285-L296) and performs [squared-Euclidean nearest-neighbor queries in Lab](https://github.com/Jesterhearts/a-sixel/blob/e2ffc9aa674aeb00779d2b8d5a5bd7ddb4615022/src/dither.rs#L76-L108). Its [`BitPaletteBucketer`](https://github.com/Jesterhearts/a-sixel/blob/e2ffc9aa674aeb00779d2b8d5a5bd7ddb4615022/src/bit.rs#L116-L190) is the exception. `a-sixel` also uses kd-trees inside some palette builders; that does not change which structure applies the completed palette.

`go-sixel` builds all `2^15` RGB555 entries eagerly. Each entry represents the center of an 8-by-8-by-8 byte cube, while source pixels select a cell by their top five bits; see [`newPaletteLUT` and `lutIndex`](https://github.com/mattn/go-sixel/blob/ceaaab1e2b5973d9ebc28b0a615760b921e90681/sixel.go#L676-L712). libsixel's `5bit` policy instead scans the actual first query color in a bucket and memoizes that result, making its approximation dependent on traversal history.

The `libsixel/libsixel` fork likewise computes an [RGB555 hash from the top five channel bits](https://github.com/libsixel/libsixel/blob/ee77c2f979dbccc86651a482e748fe7bf80bc89c/src/quant.c#L667-L675) and [fills an empty entry from the first palette scan that reaches it](https://github.com/libsixel/libsixel/blob/ee77c2f979dbccc86651a482e748fe7bf80bc89c/src/quant.c#L1091-L1139). Unlike that truncating hash, the current libsixel `5bit` policy rounds before packing its bucket coordinates.

The VP-tree chapter gives the exact libimagequant source links used to trace the previous-result optimization. These implementation comparisons are pinned to source commits or SVN revisions so later algorithm changes do not silently rewrite the meaning of this document. The Netpbm [`pnmquant`](https://netpbm.sourceforge.net/doc/pnmquant.html), [`pnmcolormap`](https://netpbm.sourceforge.net/doc/pnmcolormap.html), and [`pnmremap`](https://netpbm.sourceforge.net/doc/pnmremap.html) manuals remain useful for command behavior; the algorithmic claims above come from source.

## Variants

Each policy has its own page because the algorithms, correctness arguments, and dominant costs differ materially. The [contract](#contract) compares their guarantees and the [cost](#cost) section their preparation and query costs.

| Policy | Method |
| --- | --- |
| [`auto`](auto.md) | Selector behavior and fallbacks |
| [`none`](none.md) | Exhaustive palette scan |
| [`5bit`](5bit.md) | Lazy RGB555 bucket memoization |
| [`6bit`](6bit.md) | Lazy RGB666 bucket memoization |
| [`certlut`](certlut.md) | Intended cube certification and the float32 kd-tree |
| [`eytzinger`](eytzinger.md) | One-dimensional projection in an Eytzinger array layout |
| [`fhedt`](fhedt.md) | Separable three-dimensional Euclidean distance transform |
| [`delaunay`](delaunay.md) | Closed Voronoi adjacency with exact scalar and SIMD graph walks |
| [`gridlut`](gridlut.md) | Exact RGB cell candidate LUT, GPU construction and palette-identity reuse |
| [`ordered`](ordered.md) | Exact distance-sorted candidates, seed grids and CPU/Metal construction |
| [`blockcull`](blockcull.md) | Per-block candidate culling before an exact scan, with a Metal matrix-product kernel |
| [`vptree`](vptree.md) | Metric VP-tree with neighbor-list refinement from a caller hint |
| [`rbc`](rbc.md) | Random Ball Cover cluster pruning |
| [`mahalanobis`](mahalanobis.md) | Exhaustive normalized lookup in deterministic RBC cluster order |

## Cost

The policy chapters use these variables:

- `P`: non-transparent pixels mapped to the palette;
- `K`: completed palette entries;
- `R`: grid resolution on each of three axes;
- `G = R^3`: grid cells;
- `U`: distinct lazy buckets or cells first encountered;
- `J = min(K, 16)`: the current RBC pivot count;
- `C = 4096`: fixed byte RGB cells used by gridlut;
- `V`: candidates that survive a query's bounds.

The fixed three color dimensions are omitted. Preparation cost, one-query cost, and whole-image cost must be kept separate.

| Policy | Preparation | One query | Extra space |
| --- | --- | --- | --- |
| `none` | `O(1)` | `Theta(K)` | `O(1)` |
| `5bit` / `6bit`, 8-bit | `Theta(G)` initialization | hit `Theta(1)`; first-bucket miss `Theta(K)` | `Theta(G)` |
| `certlut`, 8-bit | `O(64^3 + K^2)` in the current builder | warm fixed-depth lookup `Theta(1)`; cold refinement worst `O(K)` | initial `Theta(64^3 + K)`; fully refined worst `O(256^3 + K)` |
| `certlut`, float32 | current kd-tree build conservatively `O(K^2)` | typical `O(log K)`; worst `Theta(K)` | `Theta(K)` |
| `eytzinger` | `Theta(K log K)` | 8-bit `Theta(log K)`; float32 `O(log K + V)`, worst `Theta(K)` | `Theta(K)` |
| `fhedt` | `Theta(R^3 + K)` | `Theta(1)` | `Theta(R^3 + K)` |
| `gridlut` (byte) | `Theta(C K)`; unchanged palette `Theta(K)` | `Theta(V)`; overflow `Theta(K)` | `Theta(C + K)` for fixed candidate capacity |
| `ordered` (byte CPU) | `Theta(K^2 + G K)` with radix sorting; unchanged palette `Theta(K)` | `Theta(V)`; worst `Theta(K)` | Fixed row capacity plus `G = 512` seeds |
| `blockcull` | `Theta(K)` | alone `Theta(K)`; a run of `S` samples `Theta(K + S V)` | `Theta(K)` |
| `vptree` | `Theta(K^2)` safe-radius preparation plus tree construction | typical balanced `O(log K)`; worst `Theta(K)` | `Theta(K)` |
| `rbc`, float32 | `Theta(K J)` | `O(J + V)`; worst `Theta(K)` | `Theta(K + J)` |
| `mahalanobis`, float32 | `Theta(K J)` | `Theta(K)` currently | `Theta(K + J)` |

Direct lookup over an image is `Theta(P K)`. Ignoring concurrent duplicate misses, the 8-bit `5bit` and `6bit` implementations instead have whole-image work:

```text
Theta(G + P + U K)
```

A shared parallel table can make more than one worker scan the same cold bucket before any store becomes visible, adding up to the number of active mapping workers to that bucket's miss cost without changing the cache-hit bound.

FHEDT has the clearest fixed-build-plus-linear-application form:

```text
Theta(R^3 + K + P)
```

A query being typically logarithmic does not establish a logarithmic worst case. VP-tree, kd-tree, projection, and RBC pruning can all visit most or all of the palette for unfavorable geometry. Likewise, lookup is linear in `P = W H` but quadratic in the side length of an `n`-by-`n` image; always name the independent variable.

## Measured trade-offs

Approximate lookup is not synonymous with lower end-to-end quality. Exact nearest lookup minimizes only the current candidate's chosen distance. It does not directly minimize spatial artifacts, temporal instability, or encoded size. In a smooth gradient, controlled reuse of a recent index can suppress index chatter, produce longer runs, and change error-diffusion feedback. That can improve speed, a perceptual metric, and SIXEL size simultaneously. It can also introduce banding or bias, so no such improvement is guaranteed.

The [measured quality and speed comparison](measurements/README.md) covers ten explicit policies, including `delaunay`, on one fixture across `K = 8` through `K = 256`, with native-float K-means and byte Heckbert lookup. It reports color error, MS-SSIM, and end-to-end runtime, keeping the different representation and palette conditions explicit. The separate [Delaunay comparison](measurements/README.md#delaunay-byte-and-native-float-comparison) measures four policies on three fixtures, with byte and native-float lookup and both diffusion settings.

With error diffusion, one changed index changes the error carried to later pixels. A small local lookup difference can therefore create a larger spatial difference in the final image. Evaluate lookup changes on at least these axes:

- palette-index agreement and distance regret against the appropriate exhaustive metric;
- decoded-image quality, including gradients and error-diffusion stability;
- encoded byte size and palette-index transition or run statistics;
- preparation time, steady-state query time, and total time;
- memory footprint and worker-sharing behavior.

## Implementation map

Selection is in [`lookup-policy.c`](../../../../src/lookup-policy.c). Concrete policy classes are the `src/lookup-policy-*.c` translation units, with some larger backends split into `src/lookup-*.c`. Their interface is declared in [`6cells.h`](../../../../include/6cells.h). With `SIXEL_TRACE_TOPIC=lookup_contract`, `LSXLUT2|phase=palette-apply` reports the class selected by the actual palette-application consumer; preparation-only `LSXLUT1` records are not sufficient evidence that the same class mapped pixels.

Direct and end-to-end coverage is under [`tests/lookup`](../../../../tests/lookup). For the caller of this interface, see [Dithering](../dithering/README.md).

### Design rules

- Derive a lookup instance from a completed palette; new policies must not add another palette-construction dependency. Treat the current Heckbert histogram-resolution coupling as a documented compatibility exception.
- Keep the per-pixel interface conceptually stateless. Explicit caches are policy state, while scan order and error propagation belong to dithering.
- Treat worker sharing as a lifecycle and thread-safety decision, not only a speed switch.
- Test lookup indexes and distances directly before relying on encoded bytes or perceptual scores.
- When a backend, metric, cache threshold, representation, or parallel path changes, update this guarantee table and the affected policy chapter.
- Keep policy names and suboptions synchronized through the central option registry.

## Test coverage

<!-- test-coverage: enforced -->

### Float monochrome specialization

These value tests exercise the named component and the legacy encoder consumer separately from the selected native-palette path. Literal RGB decisions and independently calculated two-pixel FS feedback protect sample representation and mask semantics; factory existence or an in-range index alone is insufficient.

| ID | Observation | Owning test |
| --- | --- | --- |
| ML-01 | Float dark-background queries preserve fractional RGB decisions and first-entry ties. | [tests/lookup/monochrome/0001_dark_values.t](../../../../tests/lookup/monochrome/0001_dark_values.t) |
| ML-02 | Float light-background queries reverse black/white ordering without changing the first-entry tie rule. | [tests/lookup/monochrome/0002_light_values.t](../../../../tests/lookup/monochrome/0002_light_values.t) |
| ML-03 | Float palette stride skips padding between the two entries. | [tests/lookup/monochrome/0003_padded_stride.t](../../../../tests/lookup/monochrome/0003_padded_stride.t) |
| ML-04 | Float queries without a float palette use byte-coordinate endpoints. | [tests/lookup/monochrome/0004_byte_palette.t](../../../../tests/lookup/monochrome/0004_byte_palette.t) |
| ML-05 | Repreparing one object replaces its decision plane without changing another instance. | [tests/lookup/monochrome/0005_reprepare_isolation.t](../../../../tests/lookup/monochrome/0005_reprepare_isolation.t) |
| ML-06 | Double query accumulation preserves extended finite samples whose float sums overflow. | [tests/lookup/monochrome/0006_extended_sum.t](../../../../tests/lookup/monochrome/0006_extended_sum.t) |
| ML-07 | The existing byte monochrome threshold remains unchanged. | [tests/lookup/monochrome/0007_byte_compatibility.t](../../../../tests/lookup/monochrome/0007_byte_compatibility.t) |
| ML-08 | The legacy encoder auto path maps two float gray samples to the expected palette-free dark-background mask. | [tests/lookup/monochrome/0008_encoder_dark.t](../../../../tests/lookup/monochrome/0008_encoder_dark.t) |
| ML-09 | The legacy encoder auto path maps the same samples to the reversed light-background mask. | [tests/lookup/monochrome/0009_encoder_light.t](../../../../tests/lookup/monochrome/0009_encoder_light.t) |
| ML-10 | Nonzero FS feedback changes the second legacy-auto float decision across the midpoint. | [tests/lookup/monochrome/0010_encoder_fs.t](../../../../tests/lookup/monochrome/0010_encoder_fs.t) |

### FHEDT weighting contracts

The R64 metric witnesses use independently derived nearest-color margins, rather than image similarity as a substitute for a distance contract. Grid witnesses tolerate float arithmetic through a strict winning margin, collision witnesses require native refinement, and exterior witnesses require the unclipped full scan. Reprepare tests warm the voxel cache and interleave two live policies with opposite metrics. These tests do not establish a universal perceptual-quality ordering.

| ID | Contract | Evidence |
| --- | --- | --- |
| FHW-01 | Default CIELAB/DIN99d weighting and explicit unweighted selection. | [tests/lookup/fhedt/0053_cielab_default.t](../../../../tests/lookup/fhedt/0053_cielab_default.t), [tests/lookup/fhedt/0054_cielab_unweighted.t](../../../../tests/lookup/fhedt/0054_cielab_unweighted.t), [tests/lookup/fhedt/0055_din99d_default.t](../../../../tests/lookup/fhedt/0055_din99d_default.t), [tests/lookup/fhedt/0056_din99d_unweighted.t](../../../../tests/lookup/fhedt/0056_din99d_unweighted.t) |
| FHW-02 | Collision refinement uses the selected native metric. | [tests/lookup/fhedt/0057_cielab_refine_weighted.t](../../../../tests/lookup/fhedt/0057_cielab_refine_weighted.t), [tests/lookup/fhedt/0058_cielab_refine_unweighted.t](../../../../tests/lookup/fhedt/0058_cielab_refine_unweighted.t), [tests/lookup/fhedt/0059_din99d_refine_weighted.t](../../../../tests/lookup/fhedt/0059_din99d_refine_weighted.t), [tests/lookup/fhedt/0060_din99d_refine_unweighted.t](../../../../tests/lookup/fhedt/0060_din99d_refine_unweighted.t) |
| FHW-03 | Exterior scans use the selected metric. | [tests/lookup/fhedt/0061_outside_weighted.t](../../../../tests/lookup/fhedt/0061_outside_weighted.t), [tests/lookup/fhedt/0062_outside_unweighted.t](../../../../tests/lookup/fhedt/0062_outside_unweighted.t) |
| FHW-04 | Grid expansion preserves the nominal distance coefficients. | [tests/lookup/fhedt/0063_expanded_weighted.t](../../../../tests/lookup/fhedt/0063_expanded_weighted.t), [tests/lookup/fhedt/0064_expanded_unweighted.t](../../../../tests/lookup/fhedt/0064_expanded_unweighted.t) |
| FHW-05 | Different modes remain isolated across shared-grid, cache, reuse and reprepare. | [tests/lookup/fhedt/0065_instance_metric_isolation.t](../../../../tests/lookup/fhedt/0065_instance_metric_isolation.t) |
| FHW-06 | Uniform ratios preserve unit-range RGB distance. | [tests/lookup/fhedt/0066_rgb_weighted.t](../../../../tests/lookup/fhedt/0066_rgb_weighted.t), [tests/lookup/fhedt/0067_rgb_unweighted.t](../../../../tests/lookup/fhedt/0067_rgb_unweighted.t) |
| FHW-07 | Compact and environment forms reach float lookup and produce the same decoded image. | [tests/cli/lookup-options/0038_lookup_fhedt_weights_image_regression.t](../../../../tests/cli/lookup-options/0038_lookup_fhedt_weights_image_regression.t) |
| FHW-08 | Long syntax, CLI/environment precedence, repeated-option reset, invalid values, policy scope, native float and byte dispatch. | [tests/cli/lookup-options/0001_long_unweighted.t](../../../../tests/cli/lookup-options/0001_long_unweighted.t), [tests/cli/lookup-options/0002_cli_over_environment.t](../../../../tests/cli/lookup-options/0002_cli_over_environment.t), [tests/cli/lookup-options/0003_repeated_reset.t](../../../../tests/cli/lookup-options/0003_repeated_reset.t), [tests/cli/lookup-options/0004_invalid_environment_default.t](../../../../tests/cli/lookup-options/0004_invalid_environment_default.t), [tests/cli/lookup-options/0005_reject_incomplete_tuple.t](../../../../tests/cli/lookup-options/0005_reject_incomplete_tuple.t), [tests/cli/lookup-options/0006_reject_other_policy.t](../../../../tests/cli/lookup-options/0006_reject_other_policy.t), [tests/cli/lookup-options/0007_byte_unweighted.t](../../../../tests/cli/lookup-options/0007_byte_unweighted.t) |
| FHW-09 | OKLab default, uniform override, explicit 3:1:1 and refinement. | [tests/lookup/fhedt/0068_oklab_default.t](../../../../tests/lookup/fhedt/0068_oklab_default.t), [tests/lookup/fhedt/0069_oklab_unweighted.t](../../../../tests/lookup/fhedt/0069_oklab_unweighted.t), [tests/lookup/fhedt/0070_oklab_explicit.t](../../../../tests/lookup/fhedt/0070_oklab_explicit.t), [tests/lookup/fhedt/0071_oklab_refine.t](../../../../tests/lookup/fhedt/0071_oklab_refine.t) |
| FHW-10 | Fractional L and independent a/b ratios select independently derived winners. | [tests/lookup/fhedt/0072_fractional_l_low.t](../../../../tests/lookup/fhedt/0072_fractional_l_low.t), [tests/lookup/fhedt/0073_fractional_l_high.t](../../../../tests/lookup/fhedt/0073_fractional_l_high.t), [tests/lookup/fhedt/0074_opponent_a.t](../../../../tests/lookup/fhedt/0074_opponent_a.t), [tests/lookup/fhedt/0075_opponent_b.t](../../../../tests/lookup/fhedt/0075_opponent_b.t) |
| FHW-11 | Changing only the common ratio scale preserves the winner, including magnitudes whose raw squares overflow or underflow double. | [tests/lookup/fhedt/0076_common_scale_large.t](../../../../tests/lookup/fhedt/0076_common_scale_large.t), [tests/lookup/fhedt/0077_common_scale_small.t](../../../../tests/lookup/fhedt/0077_common_scale_small.t) |
| FHW-12 | Malformed or unrepresentable ratios fail; byte lookup rejects nonuniform ratios; explicit OKLab auto overrides the environment. | [tests/cli/lookup-options/0008_reject_zero_axis.t](../../../../tests/cli/lookup-options/0008_reject_zero_axis.t), [tests/cli/lookup-options/0009_reject_negative_axis.t](../../../../tests/cli/lookup-options/0009_reject_negative_axis.t), [tests/cli/lookup-options/0010_reject_nan_axis.t](../../../../tests/cli/lookup-options/0010_reject_nan_axis.t), [tests/cli/lookup-options/0011_reject_infinite_axis.t](../../../../tests/cli/lookup-options/0011_reject_infinite_axis.t), [tests/cli/lookup-options/0012_reject_extra_axis.t](../../../../tests/cli/lookup-options/0012_reject_extra_axis.t), [tests/cli/lookup-options/0013_reject_empty_axis.t](../../../../tests/cli/lookup-options/0013_reject_empty_axis.t), [tests/cli/lookup-options/0014_reject_trailing_data.t](../../../../tests/cli/lookup-options/0014_reject_trailing_data.t), [tests/cli/lookup-options/0015_reject_unrepresentable_ratio.t](../../../../tests/cli/lookup-options/0015_reject_unrepresentable_ratio.t), [tests/cli/lookup-options/0016_reject_nonuniform_byte.t](../../../../tests/cli/lookup-options/0016_reject_nonuniform_byte.t), [tests/cli/lookup-options/0017_oklab_auto_over_environment.t](../../../../tests/cli/lookup-options/0017_oklab_auto_over_environment.t) |
| FHW-13 | Decimal tuple parsing and repeated-option restoration of the environment default. | [tests/cli/lookup-options/0018_fractional_tuple.t](../../../../tests/cli/lookup-options/0018_fractional_tuple.t), [tests/cli/lookup-options/0019_reset_to_environment.t](../../../../tests/cli/lookup-options/0019_reset_to_environment.t) |
| FHW-14 | Gamma and linear RGB apply explicit ratios to their R/G/B axes. | [tests/lookup/fhedt/0078_gamma_channel_ratio.t](../../../../tests/lookup/fhedt/0078_gamma_channel_ratio.t), [tests/lookup/fhedt/0079_linear_channel_ratio.t](../../../../tests/lookup/fhedt/0079_linear_channel_ratio.t) |

### Behavioral contract tests

The following focused tests cover the selected extended float lookup path. Range and fractional-coordinate cases assert exact indexes with strict nearest-distance margins, while checking that preparation and lookup preserve the source arrays. Overflow and underflow cases use normal finite float inputs with squared distances outside float range. Reprepare cases interleave two live lookup objects. Encoder cases inspect the retained palette, RGB101 output and actual consumer class; byte controls require the original byte path. The 41-color exhaustive oracle exercises nontrivial tree/ball pruning, repeated queries and cluster jumps in all five working spaces at binary exponents 0, 120 and -120; it accounts for float metric-weight rounding against independently computed double distances. Worker cases compare identical FS band geometries, including overlap, with private policies and, for the classes supporting it, shared policies. They do not assert equality with whole-image FS.

| ID | Contract | Owning tests |
| --- | --- | --- |
| LUTN-01 | `certlut`: Signed and above-nominal coordinates. | [tests/lookup/certlut/0001_certlut_gamma_range.t](../../../../tests/lookup/certlut/0001_certlut_gamma_range.t), [tests/lookup/certlut/0002_certlut_linear_range.t](../../../../tests/lookup/certlut/0002_certlut_linear_range.t), [tests/lookup/certlut/0003_certlut_oklab_range.t](../../../../tests/lookup/certlut/0003_certlut_oklab_range.t), [tests/lookup/certlut/0004_certlut_cielab_range.t](../../../../tests/lookup/certlut/0004_certlut_cielab_range.t), [tests/lookup/certlut/0005_certlut_din99d_range.t](../../../../tests/lookup/certlut/0005_certlut_din99d_range.t) |
| LUTN-02 | `certlut`: Sub-byte coordinate separation. | [tests/lookup/certlut/0006_certlut_gamma_fraction.t](../../../../tests/lookup/certlut/0006_certlut_gamma_fraction.t), [tests/lookup/certlut/0007_certlut_linear_fraction.t](../../../../tests/lookup/certlut/0007_certlut_linear_fraction.t), [tests/lookup/certlut/0008_certlut_oklab_fraction.t](../../../../tests/lookup/certlut/0008_certlut_oklab_fraction.t), [tests/lookup/certlut/0009_certlut_cielab_fraction.t](../../../../tests/lookup/certlut/0009_certlut_cielab_fraction.t), [tests/lookup/certlut/0010_certlut_din99d_fraction.t](../../../../tests/lookup/certlut/0010_certlut_din99d_fraction.t) |
| LUTN-03 | `certlut`: Declared normalized metric. | [tests/lookup/certlut/0011_certlut_cielab_normalized_metric.t](../../../../tests/lookup/certlut/0011_certlut_cielab_normalized_metric.t), [tests/lookup/certlut/0012_certlut_din99d_normalized_metric.t](../../../../tests/lookup/certlut/0012_certlut_din99d_normalized_metric.t) |
| LUTN-04 | `certlut`: Finite distances above float range. | [tests/lookup/certlut/0013_certlut_overflow.t](../../../../tests/lookup/certlut/0013_certlut_overflow.t) |
| LUTN-05 | `certlut`: Finite distances below float range. | [tests/lookup/certlut/0014_certlut_underflow.t](../../../../tests/lookup/certlut/0014_certlut_underflow.t) |
| LUTN-06 | `certlut`: Reprepare and two-instance isolation. | [tests/lookup/certlut/0015_certlut_isolation.t](../../../../tests/lookup/certlut/0015_certlut_isolation.t) |
| LUTN-07 | `5bit`: Signed and above-nominal coordinates. | [tests/lookup/5bit/0001_5bit_gamma_range.t](../../../../tests/lookup/5bit/0001_5bit_gamma_range.t), [tests/lookup/5bit/0002_5bit_linear_range.t](../../../../tests/lookup/5bit/0002_5bit_linear_range.t), [tests/lookup/5bit/0003_5bit_oklab_range.t](../../../../tests/lookup/5bit/0003_5bit_oklab_range.t), [tests/lookup/5bit/0004_5bit_cielab_range.t](../../../../tests/lookup/5bit/0004_5bit_cielab_range.t), [tests/lookup/5bit/0005_5bit_din99d_range.t](../../../../tests/lookup/5bit/0005_5bit_din99d_range.t) |
| LUTN-08 | `5bit`: Sub-byte coordinate separation. | [tests/lookup/5bit/0006_5bit_gamma_fraction.t](../../../../tests/lookup/5bit/0006_5bit_gamma_fraction.t), [tests/lookup/5bit/0007_5bit_linear_fraction.t](../../../../tests/lookup/5bit/0007_5bit_linear_fraction.t), [tests/lookup/5bit/0008_5bit_oklab_fraction.t](../../../../tests/lookup/5bit/0008_5bit_oklab_fraction.t), [tests/lookup/5bit/0009_5bit_cielab_fraction.t](../../../../tests/lookup/5bit/0009_5bit_cielab_fraction.t), [tests/lookup/5bit/0010_5bit_din99d_fraction.t](../../../../tests/lookup/5bit/0010_5bit_din99d_fraction.t) |
| LUTN-09 | `5bit`: Declared normalized metric. | [tests/lookup/5bit/0011_5bit_cielab_normalized_metric.t](../../../../tests/lookup/5bit/0011_5bit_cielab_normalized_metric.t), [tests/lookup/5bit/0012_5bit_din99d_normalized_metric.t](../../../../tests/lookup/5bit/0012_5bit_din99d_normalized_metric.t) |
| LUTN-10 | `5bit`: Finite distances above float range. | [tests/lookup/5bit/0013_5bit_overflow.t](../../../../tests/lookup/5bit/0013_5bit_overflow.t) |
| LUTN-11 | `5bit`: Finite distances below float range. | [tests/lookup/5bit/0014_5bit_underflow.t](../../../../tests/lookup/5bit/0014_5bit_underflow.t) |
| LUTN-12 | `5bit`: Reprepare and two-instance isolation. | [tests/lookup/5bit/0015_5bit_isolation.t](../../../../tests/lookup/5bit/0015_5bit_isolation.t) |
| LUTN-13 | `6bit`: Signed and above-nominal coordinates. | [tests/lookup/6bit/0001_6bit_gamma_range.t](../../../../tests/lookup/6bit/0001_6bit_gamma_range.t), [tests/lookup/6bit/0002_6bit_linear_range.t](../../../../tests/lookup/6bit/0002_6bit_linear_range.t), [tests/lookup/6bit/0003_6bit_oklab_range.t](../../../../tests/lookup/6bit/0003_6bit_oklab_range.t), [tests/lookup/6bit/0004_6bit_cielab_range.t](../../../../tests/lookup/6bit/0004_6bit_cielab_range.t), [tests/lookup/6bit/0005_6bit_din99d_range.t](../../../../tests/lookup/6bit/0005_6bit_din99d_range.t) |
| LUTN-14 | `6bit`: Sub-byte coordinate separation. | [tests/lookup/6bit/0006_6bit_gamma_fraction.t](../../../../tests/lookup/6bit/0006_6bit_gamma_fraction.t), [tests/lookup/6bit/0007_6bit_linear_fraction.t](../../../../tests/lookup/6bit/0007_6bit_linear_fraction.t), [tests/lookup/6bit/0008_6bit_oklab_fraction.t](../../../../tests/lookup/6bit/0008_6bit_oklab_fraction.t), [tests/lookup/6bit/0009_6bit_cielab_fraction.t](../../../../tests/lookup/6bit/0009_6bit_cielab_fraction.t), [tests/lookup/6bit/0010_6bit_din99d_fraction.t](../../../../tests/lookup/6bit/0010_6bit_din99d_fraction.t) |
| LUTN-15 | `6bit`: Declared normalized metric. | [tests/lookup/6bit/0011_6bit_cielab_normalized_metric.t](../../../../tests/lookup/6bit/0011_6bit_cielab_normalized_metric.t), [tests/lookup/6bit/0012_6bit_din99d_normalized_metric.t](../../../../tests/lookup/6bit/0012_6bit_din99d_normalized_metric.t) |
| LUTN-16 | `6bit`: Finite distances above float range. | [tests/lookup/6bit/0013_6bit_overflow.t](../../../../tests/lookup/6bit/0013_6bit_overflow.t) |
| LUTN-17 | `6bit`: Finite distances below float range. | [tests/lookup/6bit/0014_6bit_underflow.t](../../../../tests/lookup/6bit/0014_6bit_underflow.t) |
| LUTN-18 | `6bit`: Reprepare and two-instance isolation. | [tests/lookup/6bit/0015_6bit_isolation.t](../../../../tests/lookup/6bit/0015_6bit_isolation.t) |
| LUTE-01 | `certlut`: sRGB/P3 wire output, retained float palette and actual consumer. | [tests/lookup/certlut/0016_certlut_gamma_srgb.t](../../../../tests/lookup/certlut/0016_certlut_gamma_srgb.t), [tests/lookup/certlut/0017_certlut_gamma_p3.t](../../../../tests/lookup/certlut/0017_certlut_gamma_p3.t), [tests/lookup/certlut/0018_certlut_linear_srgb.t](../../../../tests/lookup/certlut/0018_certlut_linear_srgb.t), [tests/lookup/certlut/0019_certlut_linear_p3.t](../../../../tests/lookup/certlut/0019_certlut_linear_p3.t), [tests/lookup/certlut/0020_certlut_oklab_srgb.t](../../../../tests/lookup/certlut/0020_certlut_oklab_srgb.t), [tests/lookup/certlut/0021_certlut_oklab_p3.t](../../../../tests/lookup/certlut/0021_certlut_oklab_p3.t), [tests/lookup/certlut/0022_certlut_cielab_srgb.t](../../../../tests/lookup/certlut/0022_certlut_cielab_srgb.t), [tests/lookup/certlut/0023_certlut_cielab_p3.t](../../../../tests/lookup/certlut/0023_certlut_cielab_p3.t), [tests/lookup/certlut/0024_certlut_din99d_srgb.t](../../../../tests/lookup/certlut/0024_certlut_din99d_srgb.t), [tests/lookup/certlut/0025_certlut_din99d_p3.t](../../../../tests/lookup/certlut/0025_certlut_din99d_p3.t) |
| LUTE-02 | `certlut`: Byte fast-path eligibility. | [tests/lookup/certlut/0026_certlut_byte_eligibility.t](../../../../tests/lookup/certlut/0026_certlut_byte_eligibility.t) |
| LUTE-03 | `certlut`: Private/shared FS bands and stable wire cache. | [tests/lookup/certlut/0027_certlut_bands_private.t](../../../../tests/lookup/certlut/0027_certlut_bands_private.t), [tests/lookup/certlut/0028_certlut_bands_shared.t](../../../../tests/lookup/certlut/0028_certlut_bands_shared.t) |
| LUTE-04 | `5bit`: sRGB/P3 wire output, retained float palette and actual consumer. | [tests/lookup/5bit/0020_5bit_gamma_srgb.t](../../../../tests/lookup/5bit/0020_5bit_gamma_srgb.t), [tests/lookup/5bit/0021_5bit_gamma_p3.t](../../../../tests/lookup/5bit/0021_5bit_gamma_p3.t), [tests/lookup/5bit/0022_5bit_linear_srgb.t](../../../../tests/lookup/5bit/0022_5bit_linear_srgb.t), [tests/lookup/5bit/0023_5bit_linear_p3.t](../../../../tests/lookup/5bit/0023_5bit_linear_p3.t), [tests/lookup/5bit/0024_5bit_oklab_srgb.t](../../../../tests/lookup/5bit/0024_5bit_oklab_srgb.t), [tests/lookup/5bit/0025_5bit_oklab_p3.t](../../../../tests/lookup/5bit/0025_5bit_oklab_p3.t), [tests/lookup/5bit/0026_5bit_cielab_srgb.t](../../../../tests/lookup/5bit/0026_5bit_cielab_srgb.t), [tests/lookup/5bit/0027_5bit_cielab_p3.t](../../../../tests/lookup/5bit/0027_5bit_cielab_p3.t), [tests/lookup/5bit/0028_5bit_din99d_srgb.t](../../../../tests/lookup/5bit/0028_5bit_din99d_srgb.t), [tests/lookup/5bit/0029_5bit_din99d_p3.t](../../../../tests/lookup/5bit/0029_5bit_din99d_p3.t) |
| LUTE-05 | `5bit`: Byte fast-path eligibility. | [tests/lookup/5bit/0030_5bit_byte_eligibility.t](../../../../tests/lookup/5bit/0030_5bit_byte_eligibility.t) |
| LUTE-06 | `5bit`: Private/shared FS bands and stable wire cache. | [tests/lookup/5bit/0031_5bit_bands_private.t](../../../../tests/lookup/5bit/0031_5bit_bands_private.t), [tests/lookup/5bit/0032_5bit_bands_shared.t](../../../../tests/lookup/5bit/0032_5bit_bands_shared.t) |
| LUTE-07 | `6bit`: sRGB/P3 wire output, retained float palette and actual consumer. | [tests/lookup/6bit/0020_6bit_gamma_srgb.t](../../../../tests/lookup/6bit/0020_6bit_gamma_srgb.t), [tests/lookup/6bit/0021_6bit_gamma_p3.t](../../../../tests/lookup/6bit/0021_6bit_gamma_p3.t), [tests/lookup/6bit/0022_6bit_linear_srgb.t](../../../../tests/lookup/6bit/0022_6bit_linear_srgb.t), [tests/lookup/6bit/0023_6bit_linear_p3.t](../../../../tests/lookup/6bit/0023_6bit_linear_p3.t), [tests/lookup/6bit/0024_6bit_oklab_srgb.t](../../../../tests/lookup/6bit/0024_6bit_oklab_srgb.t), [tests/lookup/6bit/0025_6bit_oklab_p3.t](../../../../tests/lookup/6bit/0025_6bit_oklab_p3.t), [tests/lookup/6bit/0026_6bit_cielab_srgb.t](../../../../tests/lookup/6bit/0026_6bit_cielab_srgb.t), [tests/lookup/6bit/0027_6bit_cielab_p3.t](../../../../tests/lookup/6bit/0027_6bit_cielab_p3.t), [tests/lookup/6bit/0028_6bit_din99d_srgb.t](../../../../tests/lookup/6bit/0028_6bit_din99d_srgb.t), [tests/lookup/6bit/0029_6bit_din99d_p3.t](../../../../tests/lookup/6bit/0029_6bit_din99d_p3.t) |
| LUTE-08 | `6bit`: Byte fast-path eligibility. | [tests/lookup/6bit/0030_6bit_byte_eligibility.t](../../../../tests/lookup/6bit/0030_6bit_byte_eligibility.t) |
| LUTE-09 | `6bit`: Private/shared FS bands and stable wire cache. | [tests/lookup/6bit/0031_6bit_bands_private.t](../../../../tests/lookup/6bit/0031_6bit_bands_private.t), [tests/lookup/6bit/0032_6bit_bands_shared.t](../../../../tests/lookup/6bit/0032_6bit_bands_shared.t) |
| LUTE-10 | `auto`: sRGB/P3 wire output, retained float palette and actual consumer. | [tests/lookup/dispatch/0017_auto_gamma_srgb.t](../../../../tests/lookup/dispatch/0017_auto_gamma_srgb.t), [tests/lookup/dispatch/0018_auto_gamma_p3.t](../../../../tests/lookup/dispatch/0018_auto_gamma_p3.t) |
| LUTE-11 | `auto`: Byte fast-path eligibility. | [tests/lookup/dispatch/0019_auto_byte_eligibility.t](../../../../tests/lookup/dispatch/0019_auto_byte_eligibility.t) |
| LUTE-12 | `default`: sRGB/P3 wire output, retained float palette and actual consumer. | [tests/lookup/dispatch/0020_default_gamma_srgb.t](../../../../tests/lookup/dispatch/0020_default_gamma_srgb.t), [tests/lookup/dispatch/0021_default_gamma_p3.t](../../../../tests/lookup/dispatch/0021_default_gamma_p3.t) |
| LUTE-13 | `default`: Byte fast-path eligibility. | [tests/lookup/dispatch/0022_default_byte_eligibility.t](../../../../tests/lookup/dispatch/0022_default_byte_eligibility.t) |

| LUTN-19 | `vptree`: Signed and above-nominal coordinates. | [tests/lookup/vptree/0001_vptree_gamma_range.t](../../../../tests/lookup/vptree/0001_vptree_gamma_range.t), [tests/lookup/vptree/0002_vptree_linear_range.t](../../../../tests/lookup/vptree/0002_vptree_linear_range.t), [tests/lookup/vptree/0003_vptree_oklab_range.t](../../../../tests/lookup/vptree/0003_vptree_oklab_range.t), [tests/lookup/vptree/0004_vptree_cielab_range.t](../../../../tests/lookup/vptree/0004_vptree_cielab_range.t), [tests/lookup/vptree/0005_vptree_din99d_range.t](../../../../tests/lookup/vptree/0005_vptree_din99d_range.t) |
| LUTN-20 | `vptree`: Sub-byte coordinate separation. | [tests/lookup/vptree/0006_vptree_gamma_fraction.t](../../../../tests/lookup/vptree/0006_vptree_gamma_fraction.t), [tests/lookup/vptree/0007_vptree_linear_fraction.t](../../../../tests/lookup/vptree/0007_vptree_linear_fraction.t), [tests/lookup/vptree/0008_vptree_oklab_fraction.t](../../../../tests/lookup/vptree/0008_vptree_oklab_fraction.t), [tests/lookup/vptree/0009_vptree_cielab_fraction.t](../../../../tests/lookup/vptree/0009_vptree_cielab_fraction.t), [tests/lookup/vptree/0010_vptree_din99d_fraction.t](../../../../tests/lookup/vptree/0010_vptree_din99d_fraction.t) |
| LUTN-21 | `vptree`: Declared normalized metric. | [tests/lookup/vptree/0011_vptree_cielab_metric.t](../../../../tests/lookup/vptree/0011_vptree_cielab_metric.t), [tests/lookup/vptree/0012_vptree_din99d_metric.t](../../../../tests/lookup/vptree/0012_vptree_din99d_metric.t) |
| LUTN-22 | `vptree`: Finite distances above float range. | [tests/lookup/vptree/0013_vptree_overflow.t](../../../../tests/lookup/vptree/0013_vptree_overflow.t) |
| LUTN-23 | `vptree`: Finite distances below float range. | [tests/lookup/vptree/0014_vptree_underflow.t](../../../../tests/lookup/vptree/0014_vptree_underflow.t) |
| LUTN-24 | `vptree`: Reprepare and two-instance isolation. | [tests/lookup/vptree/0015_vptree_isolation.t](../../../../tests/lookup/vptree/0015_vptree_isolation.t) |
| LUTN-25 | `rbc`: Signed and above-nominal coordinates. | [tests/lookup/rbc/0001_rbc_gamma_range.t](../../../../tests/lookup/rbc/0001_rbc_gamma_range.t), [tests/lookup/rbc/0002_rbc_linear_range.t](../../../../tests/lookup/rbc/0002_rbc_linear_range.t), [tests/lookup/rbc/0003_rbc_oklab_range.t](../../../../tests/lookup/rbc/0003_rbc_oklab_range.t), [tests/lookup/rbc/0004_rbc_cielab_range.t](../../../../tests/lookup/rbc/0004_rbc_cielab_range.t), [tests/lookup/rbc/0005_rbc_din99d_range.t](../../../../tests/lookup/rbc/0005_rbc_din99d_range.t) |
| LUTN-26 | `rbc`: Sub-byte coordinate separation. | [tests/lookup/rbc/0006_rbc_gamma_fraction.t](../../../../tests/lookup/rbc/0006_rbc_gamma_fraction.t), [tests/lookup/rbc/0007_rbc_linear_fraction.t](../../../../tests/lookup/rbc/0007_rbc_linear_fraction.t), [tests/lookup/rbc/0008_rbc_oklab_fraction.t](../../../../tests/lookup/rbc/0008_rbc_oklab_fraction.t), [tests/lookup/rbc/0009_rbc_cielab_fraction.t](../../../../tests/lookup/rbc/0009_rbc_cielab_fraction.t), [tests/lookup/rbc/0010_rbc_din99d_fraction.t](../../../../tests/lookup/rbc/0010_rbc_din99d_fraction.t) |
| LUTN-27 | `rbc`: Declared normalized metric. | [tests/lookup/rbc/0011_rbc_cielab_metric.t](../../../../tests/lookup/rbc/0011_rbc_cielab_metric.t), [tests/lookup/rbc/0012_rbc_din99d_metric.t](../../../../tests/lookup/rbc/0012_rbc_din99d_metric.t) |
| LUTN-28 | `rbc`: Finite distances above float range. | [tests/lookup/rbc/0013_rbc_overflow.t](../../../../tests/lookup/rbc/0013_rbc_overflow.t) |
| LUTN-29 | `rbc`: Finite distances below float range. | [tests/lookup/rbc/0014_rbc_underflow.t](../../../../tests/lookup/rbc/0014_rbc_underflow.t) |
| LUTN-30 | `rbc`: Reprepare and two-instance isolation. | [tests/lookup/rbc/0015_rbc_isolation.t](../../../../tests/lookup/rbc/0015_rbc_isolation.t) |
| LUTN-31 | `vptree`: Exhaustive 41-color oracle at binary exponent 0. | [tests/lookup/vptree/0016_vptree_gamma_oracle.t](../../../../tests/lookup/vptree/0016_vptree_gamma_oracle.t), [tests/lookup/vptree/0017_vptree_linear_oracle.t](../../../../tests/lookup/vptree/0017_vptree_linear_oracle.t), [tests/lookup/vptree/0018_vptree_oklab_oracle.t](../../../../tests/lookup/vptree/0018_vptree_oklab_oracle.t), [tests/lookup/vptree/0019_vptree_cielab_oracle.t](../../../../tests/lookup/vptree/0019_vptree_cielab_oracle.t), [tests/lookup/vptree/0020_vptree_din99d_oracle.t](../../../../tests/lookup/vptree/0020_vptree_din99d_oracle.t) |
| LUTN-32 | `vptree`: Exhaustive 41-color oracle at binary exponent 120. | [tests/lookup/vptree/0021_vptree_gamma_oracle_large.t](../../../../tests/lookup/vptree/0021_vptree_gamma_oracle_large.t), [tests/lookup/vptree/0022_vptree_linear_oracle_large.t](../../../../tests/lookup/vptree/0022_vptree_linear_oracle_large.t), [tests/lookup/vptree/0023_vptree_oklab_oracle_large.t](../../../../tests/lookup/vptree/0023_vptree_oklab_oracle_large.t), [tests/lookup/vptree/0024_vptree_cielab_oracle_large.t](../../../../tests/lookup/vptree/0024_vptree_cielab_oracle_large.t), [tests/lookup/vptree/0025_vptree_din99d_oracle_large.t](../../../../tests/lookup/vptree/0025_vptree_din99d_oracle_large.t) |
| LUTN-33 | `vptree`: Exhaustive 41-color oracle at binary exponent -120. | [tests/lookup/vptree/0026_vptree_gamma_oracle_small.t](../../../../tests/lookup/vptree/0026_vptree_gamma_oracle_small.t), [tests/lookup/vptree/0027_vptree_linear_oracle_small.t](../../../../tests/lookup/vptree/0027_vptree_linear_oracle_small.t), [tests/lookup/vptree/0028_vptree_oklab_oracle_small.t](../../../../tests/lookup/vptree/0028_vptree_oklab_oracle_small.t), [tests/lookup/vptree/0029_vptree_cielab_oracle_small.t](../../../../tests/lookup/vptree/0029_vptree_cielab_oracle_small.t), [tests/lookup/vptree/0030_vptree_din99d_oracle_small.t](../../../../tests/lookup/vptree/0030_vptree_din99d_oracle_small.t) |
| LUTN-34 | `rbc`: Exhaustive 41-color oracle at binary exponent 0. | [tests/lookup/rbc/0016_rbc_gamma_oracle.t](../../../../tests/lookup/rbc/0016_rbc_gamma_oracle.t), [tests/lookup/rbc/0017_rbc_linear_oracle.t](../../../../tests/lookup/rbc/0017_rbc_linear_oracle.t), [tests/lookup/rbc/0018_rbc_oklab_oracle.t](../../../../tests/lookup/rbc/0018_rbc_oklab_oracle.t), [tests/lookup/rbc/0019_rbc_cielab_oracle.t](../../../../tests/lookup/rbc/0019_rbc_cielab_oracle.t), [tests/lookup/rbc/0020_rbc_din99d_oracle.t](../../../../tests/lookup/rbc/0020_rbc_din99d_oracle.t) |
| LUTN-35 | `rbc`: Exhaustive 41-color oracle at binary exponent 120. | [tests/lookup/rbc/0021_rbc_gamma_oracle_large.t](../../../../tests/lookup/rbc/0021_rbc_gamma_oracle_large.t), [tests/lookup/rbc/0022_rbc_linear_oracle_large.t](../../../../tests/lookup/rbc/0022_rbc_linear_oracle_large.t), [tests/lookup/rbc/0023_rbc_oklab_oracle_large.t](../../../../tests/lookup/rbc/0023_rbc_oklab_oracle_large.t), [tests/lookup/rbc/0024_rbc_cielab_oracle_large.t](../../../../tests/lookup/rbc/0024_rbc_cielab_oracle_large.t), [tests/lookup/rbc/0025_rbc_din99d_oracle_large.t](../../../../tests/lookup/rbc/0025_rbc_din99d_oracle_large.t) |
| LUTN-36 | `rbc`: Exhaustive 41-color oracle at binary exponent -120. | [tests/lookup/rbc/0026_rbc_gamma_oracle_small.t](../../../../tests/lookup/rbc/0026_rbc_gamma_oracle_small.t), [tests/lookup/rbc/0027_rbc_linear_oracle_small.t](../../../../tests/lookup/rbc/0027_rbc_linear_oracle_small.t), [tests/lookup/rbc/0028_rbc_oklab_oracle_small.t](../../../../tests/lookup/rbc/0028_rbc_oklab_oracle_small.t), [tests/lookup/rbc/0029_rbc_cielab_oracle_small.t](../../../../tests/lookup/rbc/0029_rbc_cielab_oracle_small.t), [tests/lookup/rbc/0030_rbc_din99d_oracle_small.t](../../../../tests/lookup/rbc/0030_rbc_din99d_oracle_small.t) |
| LUTE-14 | `vptree`: sRGB/P3 wire output, retained float palette and actual consumer. | [tests/lookup/vptree/0031_vptree_gamma_srgb.t](../../../../tests/lookup/vptree/0031_vptree_gamma_srgb.t), [tests/lookup/vptree/0032_vptree_gamma_p3.t](../../../../tests/lookup/vptree/0032_vptree_gamma_p3.t), [tests/lookup/vptree/0033_vptree_linear_srgb.t](../../../../tests/lookup/vptree/0033_vptree_linear_srgb.t), [tests/lookup/vptree/0034_vptree_linear_p3.t](../../../../tests/lookup/vptree/0034_vptree_linear_p3.t), [tests/lookup/vptree/0035_vptree_oklab_srgb.t](../../../../tests/lookup/vptree/0035_vptree_oklab_srgb.t), [tests/lookup/vptree/0036_vptree_oklab_p3.t](../../../../tests/lookup/vptree/0036_vptree_oklab_p3.t), [tests/lookup/vptree/0037_vptree_cielab_srgb.t](../../../../tests/lookup/vptree/0037_vptree_cielab_srgb.t), [tests/lookup/vptree/0038_vptree_cielab_p3.t](../../../../tests/lookup/vptree/0038_vptree_cielab_p3.t), [tests/lookup/vptree/0039_vptree_din99d_srgb.t](../../../../tests/lookup/vptree/0039_vptree_din99d_srgb.t), [tests/lookup/vptree/0040_vptree_din99d_p3.t](../../../../tests/lookup/vptree/0040_vptree_din99d_p3.t) |
| LUTE-15 | `vptree`: Byte fast-path eligibility. | [tests/lookup/vptree/0041_vptree_byte_eligibility.t](../../../../tests/lookup/vptree/0041_vptree_byte_eligibility.t) |
| LUTE-16 | `vptree`: Worker-local FS bands, overlap and stable wire cache. | [tests/lookup/vptree/0042_vptree_bands_private.t](../../../../tests/lookup/vptree/0042_vptree_bands_private.t) |
| LUTE-17 | `rbc`: sRGB/P3 wire output, retained float palette and actual consumer. | [tests/lookup/rbc/0031_rbc_gamma_srgb.t](../../../../tests/lookup/rbc/0031_rbc_gamma_srgb.t), [tests/lookup/rbc/0032_rbc_gamma_p3.t](../../../../tests/lookup/rbc/0032_rbc_gamma_p3.t), [tests/lookup/rbc/0033_rbc_linear_srgb.t](../../../../tests/lookup/rbc/0033_rbc_linear_srgb.t), [tests/lookup/rbc/0034_rbc_linear_p3.t](../../../../tests/lookup/rbc/0034_rbc_linear_p3.t), [tests/lookup/rbc/0035_rbc_oklab_srgb.t](../../../../tests/lookup/rbc/0035_rbc_oklab_srgb.t), [tests/lookup/rbc/0036_rbc_oklab_p3.t](../../../../tests/lookup/rbc/0036_rbc_oklab_p3.t), [tests/lookup/rbc/0037_rbc_cielab_srgb.t](../../../../tests/lookup/rbc/0037_rbc_cielab_srgb.t), [tests/lookup/rbc/0038_rbc_cielab_p3.t](../../../../tests/lookup/rbc/0038_rbc_cielab_p3.t), [tests/lookup/rbc/0039_rbc_din99d_srgb.t](../../../../tests/lookup/rbc/0039_rbc_din99d_srgb.t), [tests/lookup/rbc/0040_rbc_din99d_p3.t](../../../../tests/lookup/rbc/0040_rbc_din99d_p3.t) |
| LUTE-18 | `rbc`: Byte fast-path eligibility. | [tests/lookup/rbc/0041_rbc_byte_eligibility.t](../../../../tests/lookup/rbc/0041_rbc_byte_eligibility.t) |
| LUTE-19 | `rbc`: Worker-local FS bands, overlap and stable wire cache. | [tests/lookup/rbc/0042_rbc_bands_private.t](../../../../tests/lookup/rbc/0042_rbc_bands_private.t) |

| ID | Contract | Owning tests |
| --- | --- | --- |
| LUTM-01 | `mahalanobis`: Signed and above-nominal coordinates. | [tests/lookup/mahalanobis/0001_mah_gamma_range.t](../../../../tests/lookup/mahalanobis/0001_mah_gamma_range.t), [tests/lookup/mahalanobis/0002_mah_linear_range.t](../../../../tests/lookup/mahalanobis/0002_mah_linear_range.t), [tests/lookup/mahalanobis/0003_mah_oklab_range.t](../../../../tests/lookup/mahalanobis/0003_mah_oklab_range.t), [tests/lookup/mahalanobis/0004_mah_cielab_range.t](../../../../tests/lookup/mahalanobis/0004_mah_cielab_range.t), [tests/lookup/mahalanobis/0005_mah_din99d_range.t](../../../../tests/lookup/mahalanobis/0005_mah_din99d_range.t) |
| LUTM-02 | `mahalanobis`: Sub-byte separation. | [tests/lookup/mahalanobis/0006_mah_gamma_fraction.t](../../../../tests/lookup/mahalanobis/0006_mah_gamma_fraction.t), [tests/lookup/mahalanobis/0007_mah_linear_fraction.t](../../../../tests/lookup/mahalanobis/0007_mah_linear_fraction.t), [tests/lookup/mahalanobis/0008_mah_oklab_fraction.t](../../../../tests/lookup/mahalanobis/0008_mah_oklab_fraction.t), [tests/lookup/mahalanobis/0009_mah_cielab_fraction.t](../../../../tests/lookup/mahalanobis/0009_mah_cielab_fraction.t), [tests/lookup/mahalanobis/0010_mah_din99d_fraction.t](../../../../tests/lookup/mahalanobis/0010_mah_din99d_fraction.t) |
| LUTM-03 | `mahalanobis`: Declared normalized metric. | [tests/lookup/mahalanobis/0011_mah_cielab_metric.t](../../../../tests/lookup/mahalanobis/0011_mah_cielab_metric.t), [tests/lookup/mahalanobis/0012_mah_din99d_metric.t](../../../../tests/lookup/mahalanobis/0012_mah_din99d_metric.t) |
| LUTM-04 | `mahalanobis`: Finite distances above float range. | [tests/lookup/mahalanobis/0013_mah_overflow.t](../../../../tests/lookup/mahalanobis/0013_mah_overflow.t) |
| LUTM-05 | `mahalanobis`: Finite distances below float range. | [tests/lookup/mahalanobis/0014_mah_underflow.t](../../../../tests/lookup/mahalanobis/0014_mah_underflow.t) |
| LUTM-06 | `mahalanobis`: Reprepare and two-instance isolation. | [tests/lookup/mahalanobis/0015_mah_isolation.t](../../../../tests/lookup/mahalanobis/0015_mah_isolation.t) |
| LUTM-07 | `mahalanobis`: 41-color exhaustive oracle at exponent 0. | [tests/lookup/mahalanobis/0016_mah_gamma_oracle.t](../../../../tests/lookup/mahalanobis/0016_mah_gamma_oracle.t), [tests/lookup/mahalanobis/0017_mah_linear_oracle.t](../../../../tests/lookup/mahalanobis/0017_mah_linear_oracle.t), [tests/lookup/mahalanobis/0018_mah_oklab_oracle.t](../../../../tests/lookup/mahalanobis/0018_mah_oklab_oracle.t), [tests/lookup/mahalanobis/0019_mah_cielab_oracle.t](../../../../tests/lookup/mahalanobis/0019_mah_cielab_oracle.t), [tests/lookup/mahalanobis/0020_mah_din99d_oracle.t](../../../../tests/lookup/mahalanobis/0020_mah_din99d_oracle.t) |
| LUTM-08 | `mahalanobis`: 41-color exhaustive oracle at exponent 120. | [tests/lookup/mahalanobis/0021_mah_gamma_oracle_large.t](../../../../tests/lookup/mahalanobis/0021_mah_gamma_oracle_large.t), [tests/lookup/mahalanobis/0022_mah_linear_oracle_large.t](../../../../tests/lookup/mahalanobis/0022_mah_linear_oracle_large.t), [tests/lookup/mahalanobis/0023_mah_oklab_oracle_large.t](../../../../tests/lookup/mahalanobis/0023_mah_oklab_oracle_large.t), [tests/lookup/mahalanobis/0024_mah_cielab_oracle_large.t](../../../../tests/lookup/mahalanobis/0024_mah_cielab_oracle_large.t), [tests/lookup/mahalanobis/0025_mah_din99d_oracle_large.t](../../../../tests/lookup/mahalanobis/0025_mah_din99d_oracle_large.t) |
| LUTM-09 | `mahalanobis`: 41-color exhaustive oracle at exponent -120. | [tests/lookup/mahalanobis/0026_mah_gamma_oracle_small.t](../../../../tests/lookup/mahalanobis/0026_mah_gamma_oracle_small.t), [tests/lookup/mahalanobis/0027_mah_linear_oracle_small.t](../../../../tests/lookup/mahalanobis/0027_mah_linear_oracle_small.t), [tests/lookup/mahalanobis/0028_mah_oklab_oracle_small.t](../../../../tests/lookup/mahalanobis/0028_mah_oklab_oracle_small.t), [tests/lookup/mahalanobis/0029_mah_cielab_oracle_small.t](../../../../tests/lookup/mahalanobis/0029_mah_cielab_oracle_small.t), [tests/lookup/mahalanobis/0030_mah_din99d_oracle_small.t](../../../../tests/lookup/mahalanobis/0030_mah_din99d_oracle_small.t) |
| LUTM-10 | `mahalanobis`: sRGB/P3 wire output, retained native palette and actual consumer. | [tests/lookup/mahalanobis/0031_mah_gamma_srgb.t](../../../../tests/lookup/mahalanobis/0031_mah_gamma_srgb.t), [tests/lookup/mahalanobis/0032_mah_gamma_p3.t](../../../../tests/lookup/mahalanobis/0032_mah_gamma_p3.t), [tests/lookup/mahalanobis/0033_mah_linear_srgb.t](../../../../tests/lookup/mahalanobis/0033_mah_linear_srgb.t), [tests/lookup/mahalanobis/0034_mah_linear_p3.t](../../../../tests/lookup/mahalanobis/0034_mah_linear_p3.t), [tests/lookup/mahalanobis/0035_mah_oklab_srgb.t](../../../../tests/lookup/mahalanobis/0035_mah_oklab_srgb.t), [tests/lookup/mahalanobis/0036_mah_oklab_p3.t](../../../../tests/lookup/mahalanobis/0036_mah_oklab_p3.t), [tests/lookup/mahalanobis/0037_mah_cielab_srgb.t](../../../../tests/lookup/mahalanobis/0037_mah_cielab_srgb.t), [tests/lookup/mahalanobis/0038_mah_cielab_p3.t](../../../../tests/lookup/mahalanobis/0038_mah_cielab_p3.t), [tests/lookup/mahalanobis/0039_mah_din99d_srgb.t](../../../../tests/lookup/mahalanobis/0039_mah_din99d_srgb.t), [tests/lookup/mahalanobis/0040_mah_din99d_p3.t](../../../../tests/lookup/mahalanobis/0040_mah_din99d_p3.t) |
| LUTM-11 | `mahalanobis`: Byte fast-path eligibility. | [tests/lookup/mahalanobis/0041_mah_byte_eligibility.t](../../../../tests/lookup/mahalanobis/0041_mah_byte_eligibility.t) |
| LUTM-12 | `mahalanobis`: Worker-local FS bands, overlap and stable wire cache. | [tests/lookup/mahalanobis/0042_mah_bands_private.t](../../../../tests/lookup/mahalanobis/0042_mah_bands_private.t) |

| ID | Contract | Owning tests |
| --- | --- | --- |
| LUTY-01 | `eytzinger`: Signed and above-nominal coordinates. | [tests/lookup/eytzinger/0002_eyt_gamma_range.t](../../../../tests/lookup/eytzinger/0002_eyt_gamma_range.t), [tests/lookup/eytzinger/0003_eyt_linear_range.t](../../../../tests/lookup/eytzinger/0003_eyt_linear_range.t), [tests/lookup/eytzinger/0004_eyt_oklab_range.t](../../../../tests/lookup/eytzinger/0004_eyt_oklab_range.t), [tests/lookup/eytzinger/0005_eyt_cielab_range.t](../../../../tests/lookup/eytzinger/0005_eyt_cielab_range.t), [tests/lookup/eytzinger/0006_eyt_din99d_range.t](../../../../tests/lookup/eytzinger/0006_eyt_din99d_range.t) |
| LUTY-02 | `eytzinger`: Sub-byte separation. | [tests/lookup/eytzinger/0007_eyt_gamma_fraction.t](../../../../tests/lookup/eytzinger/0007_eyt_gamma_fraction.t), [tests/lookup/eytzinger/0008_eyt_linear_fraction.t](../../../../tests/lookup/eytzinger/0008_eyt_linear_fraction.t), [tests/lookup/eytzinger/0009_eyt_oklab_fraction.t](../../../../tests/lookup/eytzinger/0009_eyt_oklab_fraction.t), [tests/lookup/eytzinger/0010_eyt_cielab_fraction.t](../../../../tests/lookup/eytzinger/0010_eyt_cielab_fraction.t), [tests/lookup/eytzinger/0011_eyt_din99d_fraction.t](../../../../tests/lookup/eytzinger/0011_eyt_din99d_fraction.t) |
| LUTY-03 | `eytzinger`: Declared normalized metric. | [tests/lookup/eytzinger/0012_eyt_cielab_metric.t](../../../../tests/lookup/eytzinger/0012_eyt_cielab_metric.t), [tests/lookup/eytzinger/0013_eyt_din99d_metric.t](../../../../tests/lookup/eytzinger/0013_eyt_din99d_metric.t) |
| LUTY-04 | `eytzinger`: Finite distances above float range. | [tests/lookup/eytzinger/0014_eyt_overflow.t](../../../../tests/lookup/eytzinger/0014_eyt_overflow.t) |
| LUTY-05 | `eytzinger`: Finite distances below float range. | [tests/lookup/eytzinger/0015_eyt_underflow.t](../../../../tests/lookup/eytzinger/0015_eyt_underflow.t) |
| LUTY-06 | `eytzinger`: Reprepare and two-instance isolation. | [tests/lookup/eytzinger/0016_eyt_isolation.t](../../../../tests/lookup/eytzinger/0016_eyt_isolation.t) |
| LUTY-07 | `eytzinger`: 41-color exhaustive oracle at exponent 0. | [tests/lookup/eytzinger/0017_eyt_gamma_oracle.t](../../../../tests/lookup/eytzinger/0017_eyt_gamma_oracle.t), [tests/lookup/eytzinger/0018_eyt_linear_oracle.t](../../../../tests/lookup/eytzinger/0018_eyt_linear_oracle.t), [tests/lookup/eytzinger/0019_eyt_oklab_oracle.t](../../../../tests/lookup/eytzinger/0019_eyt_oklab_oracle.t), [tests/lookup/eytzinger/0020_eyt_cielab_oracle.t](../../../../tests/lookup/eytzinger/0020_eyt_cielab_oracle.t), [tests/lookup/eytzinger/0021_eyt_din99d_oracle.t](../../../../tests/lookup/eytzinger/0021_eyt_din99d_oracle.t) |
| LUTY-08 | `eytzinger`: 41-color exhaustive oracle at exponent 120. | [tests/lookup/eytzinger/0022_eyt_gamma_oracle_large.t](../../../../tests/lookup/eytzinger/0022_eyt_gamma_oracle_large.t), [tests/lookup/eytzinger/0023_eyt_linear_oracle_large.t](../../../../tests/lookup/eytzinger/0023_eyt_linear_oracle_large.t), [tests/lookup/eytzinger/0024_eyt_oklab_oracle_large.t](../../../../tests/lookup/eytzinger/0024_eyt_oklab_oracle_large.t), [tests/lookup/eytzinger/0025_eyt_cielab_oracle_large.t](../../../../tests/lookup/eytzinger/0025_eyt_cielab_oracle_large.t), [tests/lookup/eytzinger/0026_eyt_din99d_oracle_large.t](../../../../tests/lookup/eytzinger/0026_eyt_din99d_oracle_large.t) |
| LUTY-09 | `eytzinger`: 41-color exhaustive oracle at exponent -120. | [tests/lookup/eytzinger/0027_eyt_gamma_oracle_small.t](../../../../tests/lookup/eytzinger/0027_eyt_gamma_oracle_small.t), [tests/lookup/eytzinger/0028_eyt_linear_oracle_small.t](../../../../tests/lookup/eytzinger/0028_eyt_linear_oracle_small.t), [tests/lookup/eytzinger/0029_eyt_oklab_oracle_small.t](../../../../tests/lookup/eytzinger/0029_eyt_oklab_oracle_small.t), [tests/lookup/eytzinger/0030_eyt_cielab_oracle_small.t](../../../../tests/lookup/eytzinger/0030_eyt_cielab_oracle_small.t), [tests/lookup/eytzinger/0031_eyt_din99d_oracle_small.t](../../../../tests/lookup/eytzinger/0031_eyt_din99d_oracle_small.t) |
| LUTY-10 | `eytzinger`: Unequal-range dual-metric stopping bound. | [tests/lookup/eytzinger/0032_eyt_lab_bound.t](../../../../tests/lookup/eytzinger/0032_eyt_lab_bound.t), [tests/lookup/eytzinger/0033_eyt_din99d_bound.t](../../../../tests/lookup/eytzinger/0033_eyt_din99d_bound.t) |
| LUTY-11 | `eytzinger`: Finite coordinates with keys beyond float range. | [tests/lookup/eytzinger/0034_eyt_projection_overflow.t](../../../../tests/lookup/eytzinger/0034_eyt_projection_overflow.t) |
| LUTY-12 | `eytzinger`: Mixed-scale projection rounding cannot hide a nearest color. | [tests/lookup/eytzinger/0035_eyt_gamma_mixed.t](../../../../tests/lookup/eytzinger/0035_eyt_gamma_mixed.t), [tests/lookup/eytzinger/0036_eyt_linear_mixed.t](../../../../tests/lookup/eytzinger/0036_eyt_linear_mixed.t), [tests/lookup/eytzinger/0037_eyt_oklab_mixed.t](../../../../tests/lookup/eytzinger/0037_eyt_oklab_mixed.t), [tests/lookup/eytzinger/0038_eyt_cielab_mixed.t](../../../../tests/lookup/eytzinger/0038_eyt_cielab_mixed.t), [tests/lookup/eytzinger/0039_eyt_din99d_mixed.t](../../../../tests/lookup/eytzinger/0039_eyt_din99d_mixed.t) |
| LUTY-13 | `eytzinger`: sRGB/P3 wire output, retained native palette and actual consumer. | [tests/lookup/eytzinger/0040_eyt_gamma_srgb.t](../../../../tests/lookup/eytzinger/0040_eyt_gamma_srgb.t), [tests/lookup/eytzinger/0041_eyt_gamma_p3.t](../../../../tests/lookup/eytzinger/0041_eyt_gamma_p3.t), [tests/lookup/eytzinger/0042_eyt_linear_srgb.t](../../../../tests/lookup/eytzinger/0042_eyt_linear_srgb.t), [tests/lookup/eytzinger/0043_eyt_linear_p3.t](../../../../tests/lookup/eytzinger/0043_eyt_linear_p3.t), [tests/lookup/eytzinger/0044_eyt_oklab_srgb.t](../../../../tests/lookup/eytzinger/0044_eyt_oklab_srgb.t), [tests/lookup/eytzinger/0045_eyt_oklab_p3.t](../../../../tests/lookup/eytzinger/0045_eyt_oklab_p3.t), [tests/lookup/eytzinger/0046_eyt_cielab_srgb.t](../../../../tests/lookup/eytzinger/0046_eyt_cielab_srgb.t), [tests/lookup/eytzinger/0047_eyt_cielab_p3.t](../../../../tests/lookup/eytzinger/0047_eyt_cielab_p3.t), [tests/lookup/eytzinger/0048_eyt_din99d_srgb.t](../../../../tests/lookup/eytzinger/0048_eyt_din99d_srgb.t), [tests/lookup/eytzinger/0049_eyt_din99d_p3.t](../../../../tests/lookup/eytzinger/0049_eyt_din99d_p3.t) |
| LUTY-14 | `eytzinger`: Byte fast-path eligibility. | [tests/lookup/eytzinger/0050_eyt_byte_eligibility.t](../../../../tests/lookup/eytzinger/0050_eyt_byte_eligibility.t) |
| LUTY-15 | `eytzinger`: Worker-local FS bands, overlap and stable wire cache. | [tests/lookup/eytzinger/0051_eyt_bands_private.t](../../../../tests/lookup/eytzinger/0051_eyt_bands_private.t) |

### FHEDT refinement equivalence

The reference reproduces the original ordered, deduplicating corner comparisons using the actual unrefined grid labels. It checks every cell at `R=64` and deterministic random and edge queries at `R=128/256`. This establishes index compatibility with that approximate lookup, not an exhaustive nearest-palette guarantee. Existing native-float tests cover exterior scans and retained-distance gates.

| ID | Contract | Owning test |
| --- | --- | --- |
| FHR-01 | Byte refinement at `R=64` preserves the ordered deduplicating reference with duplicate labels, nonuniform weights and 8/16-bit labels. | [tests/lookup/fhedt/0047_byte_reference_r64.t](../../../../tests/lookup/fhedt/0047_byte_reference_r64.t) |
| FHR-02 | Byte refinement at `R=128` preserves the ordered deduplicating reference with duplicate labels, nonuniform weights and 8/16-bit labels. | [tests/lookup/fhedt/0048_byte_reference_r128.t](../../../../tests/lookup/fhedt/0048_byte_reference_r128.t) |
| FHR-03 | Byte refinement at `R=256` preserves the ordered deduplicating reference with duplicate labels, nonuniform weights and 8/16-bit labels. | [tests/lookup/fhedt/0049_byte_reference_r256.t](../../../../tests/lookup/fhedt/0049_byte_reference_r256.t) |
| FHR-04 | Float refinement at `R=64` preserves the ordered deduplicating reference with duplicate labels, nonuniform weights and 8/16-bit labels, native seed collisions and an expanded palette box. | [tests/lookup/fhedt/0050_float_reference_r64.t](../../../../tests/lookup/fhedt/0050_float_reference_r64.t) |
| FHR-05 | Float refinement at `R=128` preserves the ordered deduplicating reference with duplicate labels, nonuniform weights and 8/16-bit labels, native seed collisions and an expanded palette box. | [tests/lookup/fhedt/0051_float_reference_r128.t](../../../../tests/lookup/fhedt/0051_float_reference_r128.t) |
| FHR-06 | Float refinement at `R=256` preserves the ordered deduplicating reference with duplicate labels, nonuniform weights and 8/16-bit labels, native seed collisions and an expanded palette box. | [tests/lookup/fhedt/0052_float_reference_r256.t](../../../../tests/lookup/fhedt/0052_float_reference_r256.t) |

### FHEDT byte-grid coverage

The grid witnesses enumerate every site against all nine independent integer seed coordinates. Integer weights and squared distances are exact and below `2^20`; unequal distances differ by at least one, and equal-distance labels are all accepted. The two-color carried-label witnesses have squared byte distances `256` versus `208`. The byte backend is called directly, with the signature publication performed by its policy adapter reproduced before cache access. The threaded witness requests two workers; compiler/TLS restrictions and thread-creation fallback still follow the production threading policy, so this is not an exhaustive scheduling or race proof. R64 keeps the automated cases small; these witnesses do not establish every possible palette, subvoxel approximation or resolution/feature combination.

| ID | Contract | Evidence |
| --- | --- | --- |
| LUTB-01 | Y-axis propagation retains a source that loses at its own site. | [tests/lookup/fhedt/0039_y_labels.t](../../../../tests/lookup/fhedt/0039_y_labels.t) |
| LUTB-02 | Z-axis propagation retains the corresponding carried label. | [tests/lookup/fhedt/0040_z_labels.t](../../../../tests/lookup/fhedt/0040_z_labels.t) |
| LUTB-03 | Every R64 lattice site has a minimum-distance seed label under uniform weights. | [tests/lookup/fhedt/0041_grid.t](../../../../tests/lookup/fhedt/0041_grid.t) |
| LUTB-04 | Every R64 lattice site has a minimum-distance seed label under backend squared-distance coefficients 7:3:1. | [tests/lookup/fhedt/0042_weighted_grid.t](../../../../tests/lookup/fhedt/0042_weighted_grid.t) |
| LUTB-05 | Palette index 256 survives propagation, while duplicate seeds retain their last label. | [tests/lookup/fhedt/0043_u16_labels.t](../../../../tests/lookup/fhedt/0043_u16_labels.t) |
| LUTB-06 | The independent R64 grid oracle also holds with two construction workers requested. | [tests/lookup/fhedt/0044_threaded_grid.t](../../../../tests/lookup/fhedt/0044_threaded_grid.t) |
| LUTB-07 | Refinement preserves the strict winner in the carried-label witness. | [tests/lookup/fhedt/0045_refine_labels.t](../../../../tests/lookup/fhedt/0045_refine_labels.t) |
| LUTB-08 | Cached lookup and same-object reprepare use the newly published labels. | [tests/lookup/fhedt/0046_cache_reprepare.t](../../../../tests/lookup/fhedt/0046_cache_reprepare.t) |

### FHEDT native float coverage

| ID | Contract | Owning tests |
| --- | --- | --- |
| LUTF-01 | `fhedt`: Independent nearest-seed references for nominal and extended grids; refinement disabled. | [tests/lookup/fhedt/0011_fh_gamma_grid.t](../../../../tests/lookup/fhedt/0011_fh_gamma_grid.t), [tests/lookup/fhedt/0012_fh_linear_grid.t](../../../../tests/lookup/fhedt/0012_fh_linear_grid.t), [tests/lookup/fhedt/0013_fh_oklab_grid.t](../../../../tests/lookup/fhedt/0013_fh_oklab_grid.t), [tests/lookup/fhedt/0014_fh_cielab_grid.t](../../../../tests/lookup/fhedt/0014_fh_cielab_grid.t), [tests/lookup/fhedt/0015_fh_din99d_grid.t](../../../../tests/lookup/fhedt/0015_fh_din99d_grid.t), [tests/lookup/fhedt/0016_fh_gamma_expanded_grid.t](../../../../tests/lookup/fhedt/0016_fh_gamma_expanded_grid.t), [tests/lookup/fhedt/0017_fh_cielab_expanded_grid.t](../../../../tests/lookup/fhedt/0017_fh_cielab_expanded_grid.t), [tests/lookup/fhedt/0018_fh_din99d_expanded_grid.t](../../../../tests/lookup/fhedt/0018_fh_din99d_expanded_grid.t) |
| LUTF-02 | `fhedt`: Exterior native-distance scan bypasses edge-voxel cache, with dist2 off/on. | [tests/lookup/fhedt/0019_fh_outside_cache.t](../../../../tests/lookup/fhedt/0019_fh_outside_cache.t), [tests/lookup/fhedt/0020_fh_outside_dist2.t](../../../../tests/lookup/fhedt/0020_fh_outside_dist2.t) |
| LUTF-03 | `fhedt`: Unrounded palette coordinates decide boundary refinement, with dist2 off/on. | [tests/lookup/fhedt/0021_fh_native_refine.t](../../../../tests/lookup/fhedt/0021_fh_native_refine.t), [tests/lookup/fhedt/0022_fh_native_refine_dist2.t](../../../../tests/lookup/fhedt/0022_fh_native_refine_dist2.t) |
| LUTF-04 | `fhedt`: Quantized seed collision remains approximate. | [tests/lookup/fhedt/0023_fh_seed_collision.t](../../../../tests/lookup/fhedt/0023_fh_seed_collision.t) |
| LUTF-05 | `fhedt`: Opposite finite float extrema retain finite grid arithmetic. | [tests/lookup/fhedt/0024_fh_finite_extrema.t](../../../../tests/lookup/fhedt/0024_fh_finite_extrema.t) |
| LUTF-06 | `fhedt`: Cache signatures distinguish extended palettes. | [tests/lookup/fhedt/0025_fh_signature.t](../../../../tests/lookup/fhedt/0025_fh_signature.t) |
| LUTF-07 | `fhedt`: Rebuilding a palette invalidates cached indices. | [tests/lookup/fhedt/0026_fh_reprepare.t](../../../../tests/lookup/fhedt/0026_fh_reprepare.t) |
| LUTF-08 | `fhedt`: Nonfinite palettes are rejected before indexing. | [tests/lookup/fhedt/0027_fh_nonfinite.t](../../../../tests/lookup/fhedt/0027_fh_nonfinite.t) |
| LUTF-09 | `fhedt`: sRGB/P3 output, native palette and actual FHEDT consumer. | [tests/lookup/fhedt/0080_fh_gamma_srgb.t](../../../../tests/lookup/fhedt/0080_fh_gamma_srgb.t), [tests/lookup/fhedt/0081_fh_gamma_p3.t](../../../../tests/lookup/fhedt/0081_fh_gamma_p3.t), [tests/lookup/fhedt/0082_fh_linear_srgb.t](../../../../tests/lookup/fhedt/0082_fh_linear_srgb.t), [tests/lookup/fhedt/0083_fh_linear_p3.t](../../../../tests/lookup/fhedt/0083_fh_linear_p3.t), [tests/lookup/fhedt/0084_fh_oklab_srgb.t](../../../../tests/lookup/fhedt/0084_fh_oklab_srgb.t), [tests/lookup/fhedt/0085_fh_oklab_p3.t](../../../../tests/lookup/fhedt/0085_fh_oklab_p3.t), [tests/lookup/fhedt/0086_fh_cielab_srgb.t](../../../../tests/lookup/fhedt/0086_fh_cielab_srgb.t), [tests/lookup/fhedt/0087_fh_cielab_p3.t](../../../../tests/lookup/fhedt/0087_fh_cielab_p3.t), [tests/lookup/fhedt/0088_fh_din99d_srgb.t](../../../../tests/lookup/fhedt/0088_fh_din99d_srgb.t), [tests/lookup/fhedt/0089_fh_din99d_p3.t](../../../../tests/lookup/fhedt/0089_fh_din99d_p3.t) |
| LUTF-10 | `fhedt`: Eligible byte input stays on the byte path. | [tests/lookup/fhedt/0090_fh_byte_eligibility.t](../../../../tests/lookup/fhedt/0090_fh_byte_eligibility.t) |
| LUTF-11 | `fhedt`: Worker-local FS bands, overlap and stable wire cache. | [tests/lookup/fhedt/0091_fh_bands_private.t](../../../../tests/lookup/fhedt/0091_fh_bands_private.t) |

| LUTF-12 | `fhedt`: Exterior distances retain strict ordering above and below the float arithmetic range. | [tests/lookup/fhedt/0028_fh_outside_extreme.t](../../../../tests/lookup/fhedt/0028_fh_outside_extreme.t), [tests/lookup/fhedt/0029_fh_outside_tiny.t](../../../../tests/lookup/fhedt/0029_fh_outside_tiny.t) |

| LUTF-13 | `fhedt`: Enclosing-cell refinement, colliding native seeds and two-instance cache isolation. | [tests/lookup/fhedt/0030_fh_enclosing_cell.t](../../../../tests/lookup/fhedt/0030_fh_enclosing_cell.t), [tests/lookup/fhedt/0031_fh_collision_refine.t](../../../../tests/lookup/fhedt/0031_fh_collision_refine.t), [tests/lookup/fhedt/0032_fh_collision_dist2.t](../../../../tests/lookup/fhedt/0032_fh_collision_dist2.t), [tests/lookup/fhedt/0033_fh_instance_isolation.t](../../../../tests/lookup/fhedt/0033_fh_instance_isolation.t) |

### Quality regression tests

The existing [lookup quality suite](../../../../tests/lookup) checks perceptual output. Its image scores do not replace the native-coordinate and dispatch assertions above. Paired photo-dataset measurements are separate evidence for broadening the selected path; they are not additional per-test fixtures.

### FHEDT construction coverage

| ID | Contract | Owning test |
| --- | --- | --- |
| LUTC-01 | One byte grid is built for an encode with multiple active dither workers. | [tests/lookup/fhedt/0034_fh_build_once_byte.t](../../../../tests/lookup/fhedt/0034_fh_build_once_byte.t) |
| LUTC-02 | One native float grid is built for an encode with multiple active dither workers. | [tests/lookup/fhedt/0035_fh_build_once_float.t](../../../../tests/lookup/fhedt/0035_fh_build_once_float.t) |
| LUTC-03 | Under a bounded palette workload and larger full-frame conversion, the async palette thread begins construction before the main working-colorspace filter finishes. This verifies event order, not a latency threshold or an ordering promise for every workload. | [tests/lookup/fhedt/0036_fh_build_overlap.t](../../../../tests/lookup/fhedt/0036_fh_build_overlap.t) |
| LUTC-04 | Byte reuse preserves the source and isolates changed palettes, warm caches and owner lifetimes. | [tests/lookup/fhedt/0037_fh_reuse_byte.t](../../../../tests/lookup/fhedt/0037_fh_reuse_byte.t) |
| LUTC-05 | Float reuse preserves the source and isolates changed palettes, warm caches and owner lifetimes. | [tests/lookup/fhedt/0038_fh_reuse_float.t](../../../../tests/lookup/fhedt/0038_fh_reuse_float.t) |

### Bulk mapping and dither dispatch

The bulk/scalar contract test constructs two independent immutable FHEDT seed grids inside its timed process, then reprepares separate query handles from those owners for each traversal and tail length. Scalar and bulk cache identities remain distinct; immutable construction is reused without sharing mutable query state or removing cold/warm comparisons.

| ID | Contract | Owning test |
| --- | --- | --- |
| LUTBULK-01 | All byte and float lookup classes map runs, separated repeats, distinct queries, reverse rows, empty input and short tails identically to ordered scalar calls on independent cold state. | [tests/lookup/dispatch/0012_lookup_batch_contract.c](../../../../tests/lookup/dispatch/0012_lookup_batch_contract.c), [tests/lookup/dispatch/0012_lookup_batch_contract.t](../../../../tests/lookup/dispatch/0012_lookup_batch_contract.t) |
| LUTBULK-02 | AUTO and explicit vptree preserve the selected exact backend; NONE selects bulk mapping while causal and positional dithers select point mapping. CLI timeline witnesses verify actual NONE bulk entry and point execution for FS/LSO2. Each converter writes a separate regular log file, read after process exit without assuming shell stderr aliases are supported by the native C runtime. | [tests/lookup/dispatch/0013_lookup_auto_dither.c](../../../../tests/lookup/dispatch/0013_lookup_auto_dither.c), [tests/lookup/dispatch/0013_lookup_auto_dither.t](../../../../tests/lookup/dispatch/0013_lookup_auto_dither.t) |
| LUTBULK-03 | Byte and float NONE bulk rows match generic rows, preserve float work-buffer replacement and row notifications, and respect transparent pixels, output clipping and retained-plane keep. | [tests/lookup/none/0003_lookup_none_batch_fences.c](../../../../tests/lookup/none/0003_lookup_none_batch_fences.c), [tests/lookup/none/0003_lookup_none_batch_fences.t](../../../../tests/lookup/none/0003_lookup_none_batch_fences.t) |
| LUTBULK-04 | Float bulk and scalar indices agree for mixed grid, one-ulp, perceptual and extended coordinates, exact ties, duplicate entries, full-key collisions, nonfinite bypass, absent scratch, oversized palettes, reverse rows and every short tail. | [tests/lookup/dispatch/0014_lookup_float_batch_exact.c](../../../../tests/lookup/dispatch/0014_lookup_float_batch_exact.c), [tests/lookup/dispatch/0014_lookup_float_batch_exact.t](../../../../tests/lookup/dispatch/0014_lookup_float_batch_exact.t) |
| LUTBULK-05 | Double SIMD distances match scalar distance bits for all fixture palette entries and sampled grid, one-ulp and extreme finite queries; non-SIMD builds check the scalar formula. | [tests/lookup/dispatch/0015_lookup_float_simd_distance.c](../../../../tests/lookup/dispatch/0015_lookup_float_simd_distance.c), [tests/lookup/dispatch/0015_lookup_float_simd_distance.t](../../../../tests/lookup/dispatch/0015_lookup_float_simd_distance.t) |
| LUTBULK-06 | Refusing optional float band scratch does not fail dithering or change the scalar palette indices. | [tests/lookup/dispatch/0016_lookup_float_workspace_failure.c](../../../../tests/lookup/dispatch/0016_lookup_float_workspace_failure.c), [tests/lookup/dispatch/0016_lookup_float_workspace_failure.t](../../../../tests/lookup/dispatch/0016_lookup_float_workspace_failure.t) |
| LUTBULK-07 | Metal direct lookup returns the CPU exhaustive scan's index and keep mask for palette sizes that leave partial eight-entry tiles, pixel counts that leave partial simdgroups, equidistant and duplicate entries, and interleaved 6delta keeps; on Apple GPU family 7 and later this exercises the matrix-product kernel. | [tests/lookup/dispatch/0006_direct_exact.c](../../../../tests/lookup/dispatch/0006_direct_exact.c), [tests/lookup/dispatch/0006_direct_exact.t](../../../../tests/lookup/dispatch/0006_direct_exact.t) |
| LUTBULK-08 | 5bit filtered scan resolves float-score near ties and SIMD tails by the double oracle. | [tests/lookup/5bit/0016_5bit_near_tie_tail.t](../../../../tests/lookup/5bit/0016_5bit_near_tie_tail.t) |
| LUTBULK-09 | 5bit filtered scan retains the double winner at tiny coordinate scales. | [tests/lookup/5bit/0017_5bit_tiny_distances.t](../../../../tests/lookup/5bit/0017_5bit_tiny_distances.t) |
| LUTBULK-10 | 5bit filtered scan uses float-planar lookup through 2^40 and exact fallback beyond it. | [tests/lookup/5bit/0018_5bit_magnitude_boundary.t](../../../../tests/lookup/5bit/0018_5bit_magnitude_boundary.t) |
| LUTBULK-11 | 5bit filtered scan preserves exact results when optional planar allocations fail. | [tests/lookup/5bit/0019_5bit_allocation_fallback.t](../../../../tests/lookup/5bit/0019_5bit_allocation_fallback.t) |
| LUTBULK-12 | 6bit filtered scan resolves float-score near ties and SIMD tails by the double oracle. | [tests/lookup/6bit/0016_6bit_near_tie_tail.t](../../../../tests/lookup/6bit/0016_6bit_near_tie_tail.t) |
| LUTBULK-13 | 6bit filtered scan retains the double winner at tiny coordinate scales. | [tests/lookup/6bit/0017_6bit_tiny_distances.t](../../../../tests/lookup/6bit/0017_6bit_tiny_distances.t) |
| LUTBULK-14 | 6bit filtered scan uses float-planar lookup through 2^40 and exact fallback beyond it. | [tests/lookup/6bit/0018_6bit_magnitude_boundary.t](../../../../tests/lookup/6bit/0018_6bit_magnitude_boundary.t) |
| LUTBULK-15 | 6bit filtered scan preserves exact results when optional planar allocations fail. | [tests/lookup/6bit/0019_6bit_allocation_fallback.t](../../../../tests/lookup/6bit/0019_6bit_allocation_fallback.t) |

The filtered-scan boundary cases compile the private 5bit/6bit scans in separate test translation units. Stored float coordinates feed an independent double-distance oracle for ties, SIMD tails, tiny distances, the float-planar magnitude boundary and optional-allocation failures. Pointer-state assertions distinguish float-planar eligibility from exact double fallback; these tests do not measure SIMD speed.
