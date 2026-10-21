# Lookup policy: `eytzinger`

This page describes the `eytzinger` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

`eytzinger` reduces three-dimensional palette search to an ordered one-dimensional candidate search. It projects each palette color to a scalar, sorts those keys, stores the binary search tree in breadth-first array order, and then checks full color distances near the projected insertion point.

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

The layout improves the memory-access pattern of binary search; it does not by itself make a one-dimensional projection preserve three-dimensional nearest neighbors.

## Method

### Projection mathematics

The policy computes a linear projection:

```text
h(x) = sum_d a[d] x[d]
```

After finding the position of `h(x)` among the sorted palette keys, the policy evaluates the full metric:

```text
D_w(i, x) = sum_d w[d] (x[d] - c[i][d])^2
```

Cauchy-Schwarz in the declared weighted metric gives the general bound:

```text
S = sum_d a[d]^2 / w[d]
(h(x) - h(c[i]))^2 <= S * D_w(i, x)
```

The float path retains its nominal metric and projection coefficients. It computes `S` from those stored coefficients and compares a conservative squared key gap with `S` times the best distance. Assuming `S = 1` is only justified for a unit projection in the transformed metric; unequal-range Lab and DIN99d require the explicit factor. Once this lower bound exceeds the best full distance, all farther sorted keys on that side can be ignored.

### Eytzinger array layout

A sorted sequence is arranged as an implicit breadth-first binary tree. For seven keys the array positions are:

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

With one-based array index `i`, the children are `2i` and `2i + 1`. The search therefore needs no pointers and can prefetch predictable child locations.

## Guarantee

### 8-bit query and guarantee

The 8-bit implementation projects with `h(r,g,b) = r + g + b`, finds the lower bound, and examines at most six sorted neighbors on each side. Full RGB distance chooses the best candidate in that window.

```text
... omitted keys ... [six left] [insertion] [six right] ... omitted keys ...
                         <------ candidates ------>
```

This is `Theta(log K)` plus a fixed amount of work, but it is approximate. Two colors can have distant projection keys yet be close in RGB, and a fixed window can omit the true nearest palette entry. The unnormalized sum projection is not itself a general lower bound for squared RGB distance.

### Float32 query and guarantee

The float32 implementation scans outward from the insertion point across the complete palette, using the dual-metric bound above to stop. It finds the nearest normalized weighted distance in gamma RGB, linear RGB, OKLab, CIELAB and DIN99d, subject to finite arithmetic and indistinguishable ties. The byte implementation retains its fixed-window approximation.

Palette and query coordinates may be negative, exceed nominal channel ranges or contain sub-byte differences. Their authoritative working representation remains float32. Preparation copies the exact palette values into private double search storage, and each query is widened once. Distances, projection keys and bounds are evaluated and stored in double, avoiding overflow and underflow for finite binary32 inputs. The projection coefficients retain their existing binary32 values; the dual-metric norm accounts for that coefficient rounding.

A sorted key can lose small components when its terms have very different magnitudes or cancel. Before squaring a key gap, the stopping test subtracts an absolute error allowance: eight double epsilons times the absolute projection-term sum for the query, plus the maximum such allowance over all palette entries. A nonpositive corrected gap cannot stop the scan. The norm also includes a 32-double-epsilon allowance for its three-term calculation, distance accumulation and stopping comparison. These guards may inspect more candidates; they do not clamp pixels, palette entries or dither compensation. Independent mixed-scale oracles cover this case in all five working spaces. General floating-point test expectations are in the [testing guide](../../../testing/guide.md#intermediate-floating-point-error).

Earlier float implementations compared the raw squared key gap with distance, without the dual-metric factor. This could skip a nearer color in unequal-range Lab or DIN99d. The current bound corrects that limitation without changing the range-normalized selection metric. DIN99d's [common coordinate divisor](../../../concepts/din99d/README.md#coordinates-and-normalization) and its declared storage ranges remain separate concepts.

Private search storage adds 12 bytes per palette entry compared with a float copy, plus eight bytes per color for the two widened key arrays and four bytes for their sentinel. No image-sized buffer is introduced. Reprepare replaces one object's data; worker-local ownership prevents concurrent encoders from sharing mutable search state.

## Cost

```text
projection, sort, and layout       Theta(K log K)
8-bit query                        Theta(log K) + fixed window
float32 query                      O(log K + V)
float32 worst query                Theta(K)
extra space                        Theta(K)
```

`V` is the number of projected neighbors whose bound does not terminate the scan. A palette concentrated along a direction poorly represented by the projection can make `V` large.

## Name and sources

The Eytzinger layout name is established terminology for this breadth-first implicit tree arrangement. Paul-Virak Khuong and Pat Morin analyze the layout, prefetching, and comparison search in [“Array Layouts for Comparison-Based Searching”](https://arxiv.org/abs/1509.05053).

libsixel applies that array layout to projected palette keys; the surrounding three-dimensional candidate strategy is libsixel-specific and should not be attributed to the array-layout paper. The policy entered libsixel in the [`1d-eytzinger` implementation commit](https://github.com/saitoha/libsixel/commit/acbde38716ecedaf8a1092386c6779a802504902) and was later renamed `eytzinger`.

The CPU implementation is [`lookup-policy-eytzinger.c`](../../../../src/lookup-policy-eytzinger.c). A Metal palette-application path also supports an Eytzinger mode in [`gpu-palette-metal.m`](../../../../src/gpu-palette-metal.m).
