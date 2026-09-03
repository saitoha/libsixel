# `certlut` Lookup Policy

## Purpose and intuition

The 8-bit `certlut` policy combines a lookup table with an intended dominance
test. Large RGB cubes are cached when the test accepts the center's winner.
Ambiguous cubes split until the test accepts them or they reach a single
byte-valued coordinate.

```text
one 4 x 4 x 4 RGB cube
          |
          v
nearest and second-nearest at cube center
          |
          v
 dominance test accepts the center winner?
          | yes                         | no
          v                             v
 cache one palette index       split into 2 x 2 x 2 children
                                      |
                                      v
                              repeat down to unit cells
```

The top-level table has `64^3` entries because each initial cube spans four
values on each byte channel.

The float32 implementation uses a kd-tree instead of the hierarchical table.
The common policy name denotes the exact-search role, not identical storage in
both representations.

## Certification mathematics

Let `a` be the nearest palette color at cube center `m`, and `b` the
second-nearest. Write the weighted-coordinate squared distances as `D_a(x)` and
`D_b(x)`. Their difference is affine in `x`:

```text
D_b(x) - D_a(x)
  = D_b(m) - D_a(m) + 2 (x - m) dot (a - b)
```

Let:

```text
gap = D_b(m) - D_a(m)
```

For a cube with side length `s`, the center-to-corner norm is at most
`sqrt(3) s / 2`. Cauchy-Schwarz therefore bounds how far the affine difference
can fall anywhere in the cube:

```text
|2 (x - m) dot (a - b)| <= sqrt(3) s ||a - b||
```

For this particular pair, `a` is certified against `b` if:

```text
gap > sqrt(3) s ||a - b||
```

The integer implementation avoids square roots by testing the squared form:

```text
gap^2 > 3 s^2 ||a - b||^2
```

All terms are measured in the same weighted coordinates. If the inequality
holds, even the strongest possible movement toward `b` cannot close the gap.

A complete cube proof must apply the inequality to every competing palette
entry `c[j]`:

```text
for every j != a:
    (D_j(m) - D_a(m))^2 > 3 s^2 ||a - c[j]||^2
```

The current implementation checks only `b`, the second-nearest entry at the
center. Being second-nearest by center distance does not prove that `b` has the
nearest Voronoi boundary: a more distant center color can have a larger
distance gradient and cross `a` inside the cube. The present 8-bit test is
therefore a strong heuristic, not a universal certificate.

A one-axis counterexample can be embedded in an RGB cube by keeping the other
two channels at 102. At center `m = 102`, use palette coordinates `a = 112`,
`b = 113`, and `c = 90` on the varying axis:

```text
center distances: D_a = 100, D_b = 121, D_c = 144
current pair test: (121 - 100)^2 = 441 > 3 * 4^2 * (113 - 112)^2 = 48
```

The current test accepts `a` for the size-four cell `[100,103]`. At coordinate
100, however, `D_a = 144` and `D_c = 100`, so the center's third-nearest entry
wins inside that cell. Testing every competitor would reject this cell.

## Float32 kd-tree

The float32 palette is recursively split by coordinate axes. A query visits the
near child first. It visits the far child only when the squared distance to the
splitting plane is smaller than the current best palette distance:

```text
                       split plane
near subtree <-------------|-------------> far subtree
          query x -------->|<-- plane distance -->

visit far subtree only if plane_distance^2 < best_distance^2
```

This is the standard kd-tree branch-and-bound idea. The policy uses the same
range-normalized channel weights for full distances and plane bounds, so the
pruning preserves the minimum of that metric.

## Guarantee

The float32 representation is nearest-distance exact in its normalized policy
metric: its kd-tree discards a subtree only through a valid axis-plane lower
bound.

The 8-bit representation is approximate because a non-unit cube can be cached
after checking only the center's nearest and second-nearest entries. Unit cells
are exact for their individual byte-valued RGB coordinates, and many larger
cubes will also have one true winner, but the current test does not prove that
for every accepted cube.

Tie order in the float32 tree can differ from an exhaustive scan, so its
guarantee is nearest-distance exact rather than scan-index exact. For
unequal-range float colorspaces, the normalized metric can also choose a
different index from float32 `none`, which uses unnormalized stored
coordinates.

## Cost

The current implementation has these conservative bounds:

```text
8-bit top-level initialization       Theta(64^3)
8-bit current search-index build     O(K^2)
8-bit warm query                     Theta(1), fixed maximum depth
8-bit cold ambiguous query           O(K)
8-bit fully refined space            O(256^3 + K)

float32 current kd-tree build        O(K^2)
float32 typical balanced query       O(log K)
float32 worst query                  Theta(K)
float32 space                        Theta(K)
```

Certification makes preparation and first-touch behavior palette dependent.
A small image may not amortize the index, while a large image can reuse large
certified regions. The `shared_instance` suboption controls reuse across
workers; its current default is disabled for `certlut`.

## Name and sources

`certlut` is a libsixel descriptive contraction of "certified lookup table."
It is not the established name of a published algorithm. The cube test is a
libsixel design built from squared-distance algebra and Cauchy-Schwarz. The
name describes the design intent; it must not be treated as evidence that the
current second-nearest-only test is a complete proof.

The float32 index follows the kd-tree introduced by Jon Louis Bentley in
[“Multidimensional Binary Search Trees Used for Associative Searching”](https://doi.org/10.1145/361002.361007).
The libsixel policy was introduced by the
[`certlut` implementation commit](https://github.com/saitoha/libsixel/commit/17f3deea418e2d947d72995a0ae00fd9ce1c8d11).

The current implementation is
[`lookup-policy-certlut.c`](../../../src/lookup-policy-certlut.c).

Return to the [Lookup Policy index](../lookup-policy.md).
