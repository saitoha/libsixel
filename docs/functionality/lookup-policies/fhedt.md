# `fhedt` Lookup Policy

## Purpose and intuition

`fhedt` builds a discrete Voronoi map of the palette over a three-dimensional
color grid. Instead of searching the palette independently for every pixel, it
propagates the nearest palette label through the grid once and then answers a
query by indexing that grid.

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

## One-dimensional transform

For one line of grid sites, let `f(q)` be zero at a palette seed and infinity
elsewhere. A weighted squared-Euclidean distance transform computes:

```text
g(i) = min_q (f(q) + w (i - q)^2)
```

Every source position `q` contributes a parabola. The algorithm maintains the
lower envelope of those parabolas, producing the complete line in linear time.

```text
cost
 ^          parabola from q2
 |    __---''---__
 | __'            '--__       parabola from q3
 |'  \ lower envelope  \__---''---__
 +--------------------------------------> grid coordinate i
       q1       q2          q3
```

Squared Euclidean distance is separable. Applying the one-dimensional
transform on X, then Y, then Z computes the three-dimensional minimum without
enumerating every palette entry at every grid coordinate.

## Query and boundary refinement

A query converts each color channel to a grid coordinate and reads the stored
source label. At `R < 256`, many byte colors share a grid cell. An optional
boundary bit marks cells where neighboring Voronoi labels disagree; the query
can then compare the source labels from at most eight surrounding lattice
vertices.

```text
one quantized cell

      o---------o       o = surrounding lattice label
     /|        /|
    o---------o |
    | |   x   | |       x = actual query color
    | o-------|-o
    |/        |/
    o---------o

refinement compares no more than the eight labels
```

This improves a boundary answer but is not an exhaustive palette scan. A true
nearest color whose label is absent from those corners can still be omitted.

## Guarantee

For 8-bit RGB at `R = 256`, each possible byte tuple has its own grid
coordinate. The transformed label is therefore nearest-distance exact; duplicate
or equidistant palette entries may still differ in tie index.

At `R = 64` or `128`, the query discretizes byte RGB and is approximate, even
with boundary refinement. Float32 input remains continuous, so all supported
finite resolutions are approximate.

The optional `SIXEL_LOOKUP_FHEDT_USE_CACHE=1` setting adds a per-thread cache
keyed by voxel. It can make repeated colors in one voxel reuse the first refined
answer. The default is off; when enabled at an approximate resolution it adds
history dependence.

## Cost

Each one-dimensional pass is linear in the number of grid cells. With three
fixed dimensions:

```text
preparation       Theta(R^3 + K)
one query         Theta(1)
whole image       Theta(R^3 + K + P)
extra space       Theta(R^3 + K)
```

Thus application is linear in pixel count after a fixed grid-build cost. The
constant is substantial: increasing `R` from 64 to 256 multiplies cell count by
64. Small images may not amortize construction, and memory bandwidth can
dominate the transform.

Tiling, first-touch, thread pinning, retained distances, and shared-grid knobs
change constants and NUMA behavior, not the asymptotic order. Their current
names and defaults are in the [`img2sixel(1)` manual](../../../converters/img2sixel.1).

## Name and sources

`fhedt` means Felzenszwalb-Huttenlocher Euclidean Distance Transform. The core
linear-time lower-envelope algorithm comes from Pedro F. Felzenszwalb and
Daniel P. Huttenlocher,
[“Distance Transforms of Sampled Functions”](https://cs.brown.edu/people/pfelzens/papers/dt-final.pdf).

The libsixel option was originally named `vpte` and was renamed to `fhedt` in
the [attribution-oriented rename commit](https://github.com/saitoha/libsixel/commit/ac25344a8d49e9f8c2545dfad1b1d00598e48343).
The three-dimensional palette-label construction and bounded corner refinement
are libsixel adaptations of the separable transform.

The policy wrapper is
[`lookup-policy-fhedt.c`](../../../src/lookup-policy-fhedt.c). The representation
backends are [`lookup-fhedt-8bit.c`](../../../src/lookup-fhedt-8bit.c) and
[`lookup-fhedt-float32.c`](../../../src/lookup-fhedt-float32.c).

Return to the [Lookup Policy index](../lookup-policy.md).
