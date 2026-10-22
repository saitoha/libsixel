# Lookup policy: `fhedt`

This page describes the `fhedt` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

`fhedt` builds a discrete Voronoi map of the palette over a three-dimensional color grid. Instead of searching the palette independently for every pixel, it propagates the nearest palette label through the grid once and then answers a query by indexing that grid.

```text
palette entries
      |
      v
seed labeled points in an R x R x R grid
      |
      v
1D distance transform along X
      |
      v
1D distance transform along Y
      |
      v
1D distance transform along Z
      |
      v
nearest-label Voronoi grid --> quantized pixel coordinate --> palette index
```

The current resolutions are `R = 64`, `128`, or `256`; the default is `64`.

Each pass preserves its input source labels until every output site has been evaluated, then publishes distances and labels together. After the first axis, a source has a nonzero carried cost: it can lose at its own coordinate but still win farther along the line. Overwriting that source label early would return the wrong palette entry even if the distance array remained correct. The byte implementation stages at most 256 integer labels on the active worker's stack; this adds no image-sized or grid-sized allocation.

## Method

### One-dimensional transform

For one line of grid sites, let `f(q)` be zero at a palette seed and infinity elsewhere. A weighted squared-Euclidean distance transform computes:

```text
g(i) = min_q (f(q) + w (i - q)^2)
```

Every source position `q` contributes a parabola. The algorithm maintains the lower envelope of those parabolas, producing the complete line in linear time.

```text
cost
 ^          parabola from q2
 |    __---''---__
 | __'            '--__       parabola from q3
 |'  \ lower envelope  \__---''---__
 +--------------------------------------> grid coordinate i
       q1       q2          q3
```

Squared Euclidean distance is separable. Applying the one-dimensional transform on X, then Y, then Z computes the three-dimensional minimum without enumerating every palette entry at every grid coordinate.

### Float palette domain and metric

The float grid covers the union of the nominal working-space box and all finite palette entries. It never clips the stored palette to nominal channel bounds. The grid index rounds each component after an affine transform into `0..R-1`; these grid coordinates are search metadata and do not replace the original float palette or the independently serialized RGB101 codes.

The `weights=auto|L/A/B` suboption (compact `Wauto` or, for example, `W3/1/1`) controls the coordinate scale ratio of the normalized axes. `auto` is the default: CIELAB and OKLab use `3:1:1`, DIN99d uses `2:1:1`, and RGB/linear RGB use `1:1:1`. `W1/1/1` disables the additional emphasis. Explicit ratios accept three positive finite numbers, including fractional values such as `W4.5/1/0.75`. In RGB spaces their positional meaning is R/G/B. A common scale cancels: `3/1/1`, `30/10/10` and `.03/.01/.01` define the same metric.

Ratios scale **coordinate differences before squaring**, after nominal-range normalization. For example, `W3/1/1` compares `(3 * delta_L)^2 + delta_a^2 + delta_b^2` in normalized coordinates, equivalent to squared-distance weights `9:1:1`. This changes lookup geometry without rescaling the stored source pixels, changing the working colorspace or altering gamut preservation. Let `s[d]` be the requested coordinate scale and `r[d]` the declared nominal channel range. The native coefficient is `w[d] = (s[d] / max(s))^2 / r[d]^2`. Thus default CIELAB coefficients are `(1, 1/81, 1/81)`, DIN99d `(1, 1/16, 1/16)`, and OKLab `(1, 1/9, 1/9)`. Uniform ratios give CIELAB `(1, 1/9, 1/9)`, DIN99d `(1, 1/4, 1/4)`, and OKLab `(1, 1, 1)`. Uniform ratios therefore do not mean unscaled Delta E76. Nominal ranges remain those defined by the working pixel format, even when a palette extends beyond them.

The common scale is normalized before squaring, so uniformly huge or tiny tuples do not overflow or underflow solely because of their shared magnitude. All three coefficients must remain positive normal float32 numbers after squaring and range normalization; an extreme ratio that would collapse an axis to zero/subnormal is rejected. The byte backend retains equal weights: `auto` or any uniform tuple preserves that fast path. Nonuniform byte requests fail with a diagnostic directing the caller to `--precision-policy=float32`.

If the native squared-distance coefficient is `w[d]` and the grid box spans `span[d]`, the EDT uses weights proportional to `w[d] * span[d]^2`. The common factor `1/(R-1)^2` and a common normalization cancel in nearest-label comparisons. Palette bounds can expand the grid but do not redefine the nominal metric. Grid construction, native refinement and exterior scans all use the same selected coefficients. Range subtraction, EDT costs and native refinement differences are evaluated in double; opposite finite float extrema do not overflow an intermediate float subtraction.

`SIXEL_LOOKUP_FHEDT_WEIGHTS=auto|L/A/B` supplies the environment default; an explicit suboption, including `auto`, takes precedence. A malformed environment tuple falls back to `auto`. A later bare `-~fhedt` removes the previous CLI override and resolves the environment or built-in default again. The setting is carried per encoder/dither/policy object and its coefficients participate in the shared-grid signature. A reuse hint prepared with different coefficients cannot supply the result. Language bindings using encoder `setopt` accept the same lookup-policy string; no new top-level option constant is needed.

Default CIELAB/DIN99d ratios preserve the former grid metric while retaining the corrected EDT label propagation and extended palette coordinates. They do not promise byte-identical historical output: older refinement used a different metric from the grid, and exterior queries were clipped. OKLab now also defaults to explicit L emphasis. No ratio universally maximizes MS-SSIM or color accuracy; assess the chosen distance together with the diffusion method and resolution.

A finite query outside this box takes a full native-palette distance scan and bypasses the voxel cache. This preserves diffusion overshoot instead of mapping all exterior colors onto a grid face. In-box queries retain the approximate grid algorithm, including seed collisions: when several palette entries round to the same seed coordinate, the last one supplies that seed label. With refinement enabled, a per-seed chain retains the other entries for native-distance comparison, even if that seed has no Voronoi boundary. The optional distance shortcut does not skip this collision refinement. Expanding the box can make cells coarser; retaining float coordinates does not remove this approximation. Nonfinite palette entries are rejected before indexing.

The EDT skips seedless parabolas while retaining the distance expression and tie order for real sources. Y and Z passes gather and scatter up to eight neighboring X columns together, reducing repeated strided memory traffic without changing the independent one-dimensional transforms. Line-level timeline diagnostics retain scalar per-line event ordering. Boundary bits are allocated and constructed only when refinement is enabled; disabling refinement leaves the grid labels and optional retained distances unchanged.

### Query and boundary refinement

A query converts each color channel to a grid coordinate and reads the stored source label. Float refinement examines the eight corners of the cell enclosing the actual query, using its floor coordinate as the lower corner. At `R < 256`, many byte colors share a grid cell. An optional boundary bit marks cells where neighboring Voronoi labels disagree; the query can then compare the source labels from at most eight surrounding lattice vertices. Float refinement also visits any native entries sharing those seed coordinates.

```text
one quantized cell

      o---------o       o = surrounding lattice label
     /|        /|
    o---------o |
    | |   x   | |       x = actual query color
    | o-------|-o
    |/        |/
    o---------o

refinement compares up to eight seed labels
(float also visits native entries sharing those seeds)
```

Corner visits retain Z/Y/X order (X changes fastest), and the winner changes only for a strictly smaller weighted squared distance. Byte refinement compares repeated labels again instead of searching a per-query deduplication list: an identical repeated distance cannot replace the first minimum. Float refinement retains its ordered deduplication of all corner labels before following collision chains, avoiding redundant native-distance calculations and chain traversal. The distance expression and its double-precision arithmetic remain unchanged. The byte query optimization preserves the palette index, including tie choices, and therefore preserves the encoded SIXEL bytes for the same input and settings. Boundary gates, retained-distance gates, exterior scans and voxel-cache behavior retain their existing semantics.

This improves a boundary answer but is not an exhaustive palette scan. A true nearest color whose label is absent from those corners can still be omitted.

## Guarantee

For 8-bit RGB at `R = 256`, each possible byte tuple has its own grid coordinate. The transformed label is therefore nearest-distance exact; duplicate or equidistant palette entries may still differ in tie index.

At `R = 64` or `128`, the query discretizes byte RGB and is approximate, even with boundary refinement. Float32 input remains continuous, so all supported finite resolutions are approximate.

The optional `SIXEL_LOOKUP_FHEDT_USE_CACHE=1` setting adds a per-thread cache keyed by voxel. For in-box queries it can make repeated colors in one voxel reuse the first refined answer. Exterior scans do not read or populate this cache. The default is off; when enabled at an approximate resolution it adds history dependence.

## Cost

Each one-dimensional pass is linear in the number of grid cells. With three fixed dimensions:

```text
preparation       Theta(R^3 + K)
byte query        Theta(1)
float query       O(K) worst case; usually a grid read and few candidates
whole byte image  Theta(R^3 + K + P)
whole float image O(R^3 + K + P K) worst case
extra space       Theta(R^3 + K)
```

Byte refinement performs at most eight distance comparisons without a growing duplicate-label search. Float refinement uses at most eight local integers to deduplicate corner labels before native-distance comparisons. Neither path needs a precomputed candidate table or additional grid storage.

Float exterior scans and native entries colliding at a seed account for the `O(K)` query bound. Collision chains add one integer per palette entry; they do not add an image-sized buffer. The fixed grid-build cost is substantial: increasing `R` from 64 to 256 multiplies cell count by 64. Small images may not amortize construction, and memory bandwidth can dominate the transform.

With `SIXEL_LOOKUP_FHEDT_SHARED=1` (the default), the encoder builds one immutable grid immediately after finalizing the working palette. An asynchronous palette job constructs it inside its existing thread-budget scope, independently of the full-frame working-colorspace conversion. Serial and parallel palette application retain references to that grid; preparing a consumer never consumes the source policy. Each handle owns one grid reference and its cache settings; cache entries remain thread-local, and the non-TLS parallel fallback disables the cache. Mapping needs no grid lock.

Reuse requires the same representation, pixel format, allocator, palette signature, resolution, refinement, metric weights and retained-distance setting. A mismatch builds a new grid while existing consumers retain the original. Tiling, first-touch and pinning govern construction only. `SIXEL_LOOKUP_FHEDT_SHARED=0` deliberately disables reference reuse, retaining independent construction for diagnostics and memory/performance comparisons; it does not promise one build per encode. This opt-out preserves the ability to compare sharing against independent grids without changing lookup results.

Tiling, first-touch, thread pinning, retained distances, and shared-grid knobs change constants and NUMA behavior, not the asymptotic order. Their current names and defaults are in the [`img2sixel(1)` manual](../../../../converters/img2sixel.1).

## Name and sources

`fhedt` means Felzenszwalb-Huttenlocher Euclidean Distance Transform. The core linear-time lower-envelope algorithm comes from Pedro F. Felzenszwalb and Daniel P. Huttenlocher, [“Distance Transforms of Sampled Functions”](https://cs.brown.edu/people/pfelzens/papers/dt-final.pdf).

The libsixel option was originally named `vpte` and was renamed to `fhedt` in the [attribution-oriented rename commit](https://github.com/saitoha/libsixel/commit/ac25344a8d49e9f8c2545dfad1b1d00598e48343). The three-dimensional palette-label construction and bounded corner refinement are libsixel adaptations of the separable transform.

The policy wrapper is [`lookup-policy-fhedt.c`](../../../../src/lookup-policy-fhedt.c). The representation backends are [`lookup-fhedt-8bit.c`](../../../../src/lookup-fhedt-8bit.c) and [`lookup-fhedt-float32.c`](../../../../src/lookup-fhedt-float32.c).
