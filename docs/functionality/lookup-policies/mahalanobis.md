# `mahalanobis` Lookup Policy

## Purpose and current status

The float32 `mahalanobis` policy prepares RBC-like clusters, then describes the
shape of each cluster with a mean and inverse covariance matrix. This metadata
could support anisotropic ordering or bounds. In the current query path,
however, the computed Mahalanobis value is not used to prune or order
clusters: every palette member is still scanned.

```text
palette
   |
   v
deterministic RBC clusters
   |
   +--> mean and covariance per cluster
   |
query --> evaluate Mahalanobis form --> value currently discarded
   |
   v
scan every member of every cluster with weighted Euclidean distance
```

The 8-bit implementation is also a direct scan and does not construct the
float32 covariance state.

## Mahalanobis mathematics

For cluster mean `mu[j]`, covariance matrix `Sigma[j]`, and query `x`, the
squared Mahalanobis distance is:

```text
M[j](x) = (x - mu[j])^T inverse(Sigma[j]) (x - mu[j])
```

Unlike an axis-isotropic Euclidean ball, this form measures displacement
relative to cluster variance and correlation:

```text
Euclidean contours             Mahalanobis contours

       *****                         ********
     **  mu **                    ***   /    ***
       *****                    **    mu       **
                                  ***    /   ***
                                     ******
```

A long, correlated color cluster can therefore regard motion along its major
axis as less surprising than the same raw motion across its narrow axis.

libsixel computes covariance in range-weighted coordinates and adds `1e-6` to
the diagonal before inversion. This regularization avoids a singular matrix for
flat or nearly collinear clusters. Single-member clusters retain an identity
matrix.

## Returned lookup result

It is essential not to confuse the policy's name with its returned distance.
The current query calculates `M[j](x)` and discards the value. It then compares
every member using:

```text
D_w(i, x) = sum_d w[d] (x[d] - c[i][d])^2
```

Thus the returned palette color minimizes normalized weighted Euclidean
distance, not Mahalanobis distance. The covariance preparation currently adds
work without reducing asymptotic query cost.

## Guarantee

The float32 path is nearest-distance exact in its normalized weighted-Euclidean
metric because the union of all cluster-member loops visits every palette
entry. Cluster order may change which tied index is retained. The 8-bit path is
a normal scan-index-exact unweighted scan.

This result can differ from float32 `none` in unequal-range color spaces because
`none` does not use the range-normalized weights.

## Cost

With `J = min(K,16)`:

```text
cluster assignment and statistics   Theta(K J)
one float32 query                    Theta(K)
whole float32 image                  Theta(K J + P K)
extra space                          Theta(K + J)
one 8-bit query                      Theta(K)
```

The 3-by-3 matrix operations are constant per cluster. They affect constants,
not the current linear dependence on palette size.

Any future pruning rule must prove a lower bound in the same metric as the
final palette comparison. A Mahalanobis score is not automatically a lower
bound on weighted Euclidean member distance.

## Name and sources

The name refers to P. C. Mahalanobis's generalized statistical distance,
introduced in
[“On the Generalised Distance in Statistics”](https://doi.org/10.1007/s13171-019-00164-5).
That source explains the statistical distance; it does not describe libsixel's
RBC clustering or current query loop.

The policy was introduced with RBC in the
[`mahalanobis` implementation commit](https://github.com/saitoha/libsixel/commit/ff38055039f1ac41aec32b81447f10b7c82fd691).
The implementation is
[`lookup-policy-mahalanobis.c`](../../../src/lookup-policy-mahalanobis.c).

Return to the [Lookup Policy index](../lookup-policy.md).
