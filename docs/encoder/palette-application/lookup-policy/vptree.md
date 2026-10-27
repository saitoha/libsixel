# Lookup policy: `vptree`

This page describes the `vptree` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

A vantage-point tree organizes a metric space by distance from a chosen pivot. Each node stores one palette color and a median radius. Colors inside that radius go to one subtree; colors outside it go to the other.

```text
                         pivot p
                    median radius mu
                       /          \
              d(c,p) < mu      d(c,p) >= mu
                 near              far
               subtree           subtree
```

A query visits the side containing its distance from the pivot first. The triangle inequality determines whether the current best-radius ball intersects the other side. If not, that entire subtree is skipped.

## Method

### Float bulk lookup

The float32 bulk memo uses all 96 bits of the three native float coordinates as its key. A hash only selects a slot; a hit compares the entire key. Positive and negative zero may occupy different entries. Nonfinite queries bypass the memo and SIMD. No input lattice is assumed: promoted byte values, one-ulp offsets, transformed perceptual coordinates, 16-bit inputs and extended negative or above-one values may occur in the same row. The working palette remains float32. Palettes larger than 256 entries and missing scratch retain scalar search.

Each dither band owns a zeroed, direct-mapped table of 32,768 entries (512 KiB), allocated through the dither allocator. Collisions replace complete entries locally, so workers need no locks, atomic multiword publication or shared mutable history. Allocation failure is an optional acceleration miss. The lookup interface owns the scratch-size contract; dither code does not depend on a backend-specific table layout.

When the build establishes noncontracting scalar arithmetic, AArch64 NEON and SSE2 map eight cold finite queries against palettes of at most 64 colors using double lanes. Larger palettes keep hinted neighbor refinement after memo misses: exhaustive double scans become more expensive as the palette grows, while spatial hints often need only a few candidates. Each channel first subtracts the widened palette value and squares the double difference, then separately multiplies by the widened weight and accumulates in channel order. No reassociation, float-distance narrowing or palette rounding is permitted. SIMD and scalar distances must be bit-identical within each build. Strict improvement in palette order retains the lowest index at ties, including duplicate colors. Incomplete batches and platforms without eligible double SIMD use the exact scalar search.

The scalar distance uses its native C expression, without explicit `fma()` or file-level contraction pragmas. Compiler names and C dialects do not establish whether that expression contracts. Both build systems probe `-ffp-contract=off`; they enable the isolated SIMD kernel only if the existing compiler arguments explicitly select that mode, or the target is x86-64 without FMA, FMA4 or AVX512F. Fast-math builds and targets with excess intermediate precision do not enable the kernel. Eligible split builds apply `-ffp-contract=off` and `SIXEL_FLOAT_LOOKUP_EXACT_SIMD=1` only to `lookup-vptree-float32.c` and `lookup-delaunay-float32.c` (and their private kernel tests), never to the whole library. Other builds keep the original compiler arithmetic and exact memo plus scalar search. In particular, default AArch64 builds conservatively retain scalar misses even when the compiler accepts the flag: silently disabling native contraction could change palette decisions at boundaries. Amalgamation never defines the gate or adds the per-file flag, so it cannot leak an arithmetic setting into subsequent sources. Existing user-supplied global compiler flags remain the user's build policy.

### Tree construction

For the current set of palette indices, libsixel chooses the last index as the pivot, computes every remaining distance to it, sorts those distances, and splits at the median:

```text
distances from pivot:  2  4  5 | 8  9  13
                                 ^
                           median radius
```

Recursive median splits tend to balance the number of nodes, although search work still depends on metric geometry. The current builder allocates and sorts each recursive subset rather than using a globally optimal construction.

### Branch-and-bound mathematics

Let `t = d(x,p)` be query-to-pivot distance, `mu` the node radius, and `tau` the best distance found so far.

If `t < mu`, the near side is searched first. The far side can contain a better point only if:

```text
mu - t <= tau
```

If `t >= mu`, the far side is searched first and the near side is necessary only if:

```text
t - mu <= tau
```

These are consequences of the triangle inequality. The implementation stores squared full distances but takes square roots where it compares them with the node radius.

### Safe radius

Let `c[i]` be a palette color and let:

```text
s[i] = min_(j != i) d(c[i], c[j])
```

If a query satisfies:

```text
d(x, c[i]) <= s[i] / 2
```

then for every other `c[j]`:

```text
d(x, c[j]) >= d(c[i], c[j]) - d(x, c[i])
             >= s[i] - s[i]/2
             >= d(x, c[i])
```

So `c[i]` is a nearest color. In squared-distance storage the threshold is:

```text
safe_dist2[i] = nearest_palette_dist2[i] / 4
```

The tree search applies this test to the pivots it visits. It is a proof radius, not an approximation tolerance.

### Previous-color refinement

Dither queries arrive in scan order, and adjacent queries usually map to the same or a neighboring color. The dither loops therefore pass the index they mapped for the previous pixel as the `hint` of the lookup contract's `map_pixel_hint()` ([`include/6cells.idl`](../../../../include/6cells.idl)), and `vptree` refines that start with Orchard's neighbor-list method. Without a usable hint, as for the first pixel of a band or a plain `map_pixel()` call, the tree search supplies the start.

Preparation stores, for every palette color, all palette colors sorted by distance from it. For a query `x` and current center `c`, any color `p` at least as close to `x` as `c` satisfies:

```text
d(c, p) <= d(c, x) + d(x, p) <= 2 d(c, x)
```

The scan of `c`'s list therefore stops at the first neighbor with `d(c,p)^2 > 4 d(c,x)^2`. When it finds a color strictly closer than `c`, that color becomes the center and the scan restarts from its list. Each restart strictly decreases the center distance, so the walk terminates. When a scan completes without a move, `c` is at the minimum distance and every color at that distance has been examined.

Among equally near colors the refinement returns the lowest index. The result is thus a function of the query alone: the start, and hence the previous pixel, affects only the work, never the answer. The hint is owned by the caller's scan loop, so the prepared tree keeps no scan history and one instance can serve concurrent bands.

The float32 neighbor distances are stored rounded toward zero and the float32 scan bound carries a relative slack far above the rounding error of the double query distances. Both adjustments can only lengthen a scan, never end it before a color that could win. The 8-bit backend uses exact integer distances.

## Guarantee

`vptree` is nearest-distance exact in its policy metric. Among equally near colors it returns the lowest palette index, the same choice as a first-index exhaustive scan in the same metric, independent of scan order, previous results, and band partitioning.

Earlier revisions kept the previous result inside the lookup object, returned whichever equally near color the tree traversal or that cache reached first, and disabled the cache in parallel dithering. Outputs can therefore differ from those revisions only at exact distance ties.

For float32 unequal-range formats, this exactness is relative to the range-normalized metric, not the unnormalized metric used by `none`.

## Cost

Computing `s[i]` and sorting the neighbor lists compare every palette pair and dominate preparation:

```text
safe-radius preparation     Theta(K^2)
neighbor-list preparation   Theta(K^2 log K)
tree construction           at least Theta(K log K), with recursive allocation
first query (tree)          O(log K) typical, Theta(K) worst
refined query               proportional to the neighbors inside 2 d(c,x),
                            plus one list scan per center move
extra space                 Theta(K^2)
```

With `K = 256` the neighbor lists occupy 512 KiB per prepared instance. The 8-bit backend also keeps an exact-result memo of 2^18 one-word entries (1 MiB per prepared instance) keyed by the full RGB value: dithered byte queries repeat often, and a hit skips the search. Each entry is a single relaxed atomic word holding the query's nearest index, so the memo is safe for concurrent callers and never changes a result. A refined query is cheap when consecutive pixels are close in color, which error diffusion and natural images usually provide; a query far from its hint pays for the moves toward its nearest color. Calling the lookup logarithmic or constant without qualification would be misleading.

The memo is initialized while the new table is private, before configuration returns and readers can use it. [`sixel_atomic_init_i32()`](../../../../src/sixel_atomic.h) initializes each word without a synchronization operation; the C11 backend uses `atomic_init`. This avoids 262,144 mutex operations per palette on compilers using the atomic fallback. Published memo reads and writes still use relaxed atomic operations. The [OpenVMS compatibility coverage](../../../misc/platforms/openvms.md#test-coverage) guards this construction boundary and exercises complete playback histories with the default lookup.

## Name and sources

`vptree` means vantage-point tree. Peter N. Yianilos introduced the name and algorithms in [“Data Structures and Algorithms for Nearest Neighbor Search in General Metric Spaces”](https://citeseerx.ist.psu.edu/document?doi=ef5f36cb8ed5a1bdd2f564d65b87ce88e0536c1a&repid=rep1&type=pdf).

The neighbor-list refinement follows Michael T. Orchard's “A fast nearest-neighbor search algorithm” (ICASSP 1991). The previous-index design was informed by libimagequant's [VP-tree nearest-color search and nearest-other-color radius](https://github.com/ImageOptim/libimagequant/blob/2883dbd955fe9007bf33d7b64f6eafaf9fec4d6a/nearest.c#L128-L225) and its [remapping loop, which passes the previous result as a guess](https://github.com/ImageOptim/libimagequant/blob/2883dbd955fe9007bf33d7b64f6eafaf9fec4d6a/remap.c#L165-L220). libsixel's exact threshold and lifecycle remain its own implementation contract.

The policy entered libsixel in the [`vp-tree` implementation commit](https://github.com/saitoha/libsixel/commit/e980610a9ee86ae7638d0375d37c0871ce6e1ab7). The wrapper is [`lookup-policy-vptree.c`](../../../../src/lookup-policy-vptree.c), with 8-bit and float32 backends in the corresponding `src/lookup-vptree-*.c` files.
