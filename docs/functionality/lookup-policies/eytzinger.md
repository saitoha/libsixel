# `eytzinger` Lookup Policy

## Purpose and intuition

`eytzinger` reduces three-dimensional palette search to an ordered
one-dimensional candidate search. It projects each palette color to a scalar,
sorts those keys, stores the binary search tree in breadth-first array order,
and then checks full color distances near the projected insertion point.

```text
palette color c              candidate x
      |                           |
      v                           v
 scalar key h(c)              scalar key h(x)
      |                           |
      +--> sort and Eytzinger <--+
             lower_bound
                  |
                  v
      inspect projected neighbors
                  |
                  v
       best full 3D color distance
```

The layout improves the memory-access pattern of binary search; it does not by
itself make a one-dimensional projection preserve three-dimensional nearest
neighbors.

## Projection mathematics

The policy computes a linear projection:

```text
h(x) = sum_d a[d] x[d]
```

After finding the position of `h(x)` among the sorted palette keys, the policy
evaluates the full metric:

```text
D_w(i, x) = sum_d w[d] (x[d] - c[i][d])^2
```

A projection can support an exact stopping proof when its coefficient vector is
a unit vector in the transformed metric space. Then Cauchy-Schwarz gives:

```text
(h(x) - h(c[i]))^2 <= D_w(i, x)
```

Once the projected difference alone exceeds the best full distance, all
farther keys on that side can be ignored.

## Eytzinger array layout

A sorted sequence is arranged as an implicit breadth-first binary tree. For
seven keys the array positions are:

```text
sorted rank:       0   1   2   3   4   5   6

tree:                      3
                        /     \
                       1       5
                      / \     / \
                     0   2   4   6

array index:        [3, 1, 5, 0, 2, 4, 6]
                      1  2  3  4  5  6  7
```

With one-based array index `i`, the children are `2i` and `2i + 1`. The search
therefore needs no pointers and can prefetch predictable child locations.

## 8-bit query and guarantee

The 8-bit implementation projects with `h(r,g,b) = r + g + b`, finds the lower
bound, and examines at most six sorted neighbors on each side. Full RGB
distance chooses the best candidate in that window.

```text
... omitted keys ... [six left] [insertion] [six right] ... omitted keys ...
                         <------ candidates ------>
```

This is `Theta(log K)` plus a fixed amount of work, but it is approximate. Two
colors can have distant projection keys yet be close in RGB, and a fixed window
can omit the true nearest palette entry. The unnormalized sum projection is not
itself a general lower bound for squared RGB distance.

## Float32 query and guarantee

The float32 implementation scans outward from the insertion point until the
squared projected difference exceeds the best full distance. This is a valid
nearest-distance proof for current unit-range RGB and OKLab float formats,
where the coefficients are `1 / sqrt(3)` and the policy metric has unit channel
weights.

For CIELAB and DIN99d float formats, channel ranges differ. The current code
sets metric weights to inverse squared ranges but normalizes the projection
coefficients by the sum of those weights. That coefficient vector is not, in
general, a unit vector in the transformed metric. The stopping test is
therefore not a proof for those formats, and this document classifies those
queries as approximate.

This distinction is easy to miss: continuing until a bound is exceeded is exact
only when the value being compared really is a lower bound.

## Cost

```text
projection, sort, and layout       Theta(K log K)
8-bit query                        Theta(log K) + fixed window
float32 query                      O(log K + V)
float32 worst query                Theta(K)
extra space                        Theta(K)
```

`V` is the number of projected neighbors whose bound does not terminate the
scan. A palette concentrated along a direction poorly represented by the
projection can make `V` large.

## Name and sources

The Eytzinger layout name is established terminology for this breadth-first
implicit tree arrangement. Paul-Virak Khuong and Pat Morin analyze the layout,
prefetching, and comparison search in
[“Array Layouts for Comparison-Based Searching”](https://arxiv.org/abs/1509.05053).

libsixel applies that array layout to projected palette keys; the surrounding
three-dimensional candidate strategy is libsixel-specific and should not be
attributed to the array-layout paper. The policy entered libsixel in the
[`1d-eytzinger` implementation commit](https://github.com/saitoha/libsixel/commit/acbde38716ecedaf8a1092386c6779a802504902)
and was later renamed `eytzinger`.

The CPU implementation is
[`lookup-policy-eytzinger.c`](../../../src/lookup-policy-eytzinger.c). A Metal
palette-application path also supports an Eytzinger mode in
[`gpu-palette-metal.m`](../../../src/gpu-palette-metal.m).

Return to the [Lookup Policy index](../lookup-policy.md).
