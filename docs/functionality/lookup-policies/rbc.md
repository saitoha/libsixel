# `rbc` Lookup Policy

## Purpose and intuition

Random Ball Cover (RBC) represents palette colors as clusters around a small
set of pivots. A cluster can be skipped when the distance to its pivot, minus
the cluster radius, is already no better than the best palette distance found.

```text
                    radius rho[j]
                 .----------------.
              .-'                  '-.
             /       palette          \
            |        members           |
             \           p[j]         /
              '-.                  .-'
                 '----------------'

query x -------- d(x,p[j]) --------> pivot
lower bound to every member = max(0, d(x,p[j]) - rho[j])
```

The current float32 implementation uses at most 16 clusters. The 8-bit policy
currently falls back to an exhaustive palette scan.

## Construction

Let:

```text
J = min(K, 16)
```

libsixel chooses deterministic palette positions:

```text
pivot[j] = palette[floor(j K / J)]
```

Every palette color is assigned to its closest pivot. For cluster `C[j]`, its
radius is the farthest member distance:

```text
rho[j] = max_(c in C[j]) d(c, p[j])
```

This is RBC-shaped clustering, but it differs from the original algorithm's
random representative selection. Deterministic positions make repeated runs
stable but do not promise statistically representative pivots.

## Query mathematics

For any cluster member `c`, the triangle inequality gives:

```text
d(x,c) >= d(x,p[j]) - d(c,p[j])
       >= d(x,p[j]) - rho[j]
```

Distances cannot be negative, so the cluster lower bound is:

```text
L[j] = max(0, d(x,p[j]) - rho[j])
```

If `L[j]` is not smaller than the current best distance, no member of that
cluster can win and the complete cluster is skipped. Otherwise its members are
compared directly.

## Guarantee

The float32 RBC path is nearest-distance exact in its range-normalized metric.
Every skipped cluster has a lower bound no better than the current result.
Using `>=` may discard a tied entry, so palette tie index need not match an
exhaustive first-index scan.

The 8-bit implementation is scan-index exact because it currently performs the
direct unweighted RGB scan; it does not build RBC clusters.

## Cost

Let `V` be the total number of members in clusters whose bounds survive:

```text
construction              Theta(K J)
one float32 query         O(J + V)
worst float32 query       Theta(K)
extra space               Theta(K + J)
one 8-bit query           Theta(K)
```

`J` is capped at 16, but treating it as a named variable makes the design
constraint visible. Good pivots and compact clusters make `V` small; broad or
overlapping balls can leave nearly every member to scan.

## Name and sources

RBC means Random Ball Cover. Lawrence Cayton introduced the data structure for
parallel nearest-neighbor search in
[“A Nearest Neighbor Data Structure for Graphics Hardware”](https://www.lcayton.com/rbc-adms.pdf),
with an accompanying
[reference implementation](https://github.com/lcayton/RandomBallCover-GPU).

libsixel borrows the representative, assignment, radius, and triangle-bound
shape. Because its current representatives are deterministic and it performs
an exact bounded search, it should not be described as a verbatim
implementation of Cayton's construction or GPU algorithm.

The policy was added with the related Mahalanobis policy in the
[`rbc` implementation commit](https://github.com/saitoha/libsixel/commit/ff38055039f1ac41aec32b81447f10b7c82fd691).
Its current implementation is
[`lookup-policy-rbc.c`](../../../src/lookup-policy-rbc.c).

Return to the [Lookup Policy index](../lookup-policy.md).
