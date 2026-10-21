# Lookup policy: `mahalanobis`

This page describes the `mahalanobis` value of `-~` / `--lookup-policy`. It is part of the [lookup policy](README.md) reference, which owns the shared contract, distance metric, guarantee levels, cost variables, and test coverage that this page relies on.

## Purpose and intuition

The float32 `mahalanobis` policy retains its historical name and deterministic RBC-like cluster traversal, but selects colors by normalized weighted Euclidean distance. It scans every palette member. It does not build means or inverse covariance matrices or calculate a Mahalanobis score, because nothing would consume them for ordering, pruning or selection, and extended finite coordinates should not pass through unused, overflow-prone covariance arithmetic.

```text
palette --> deterministic RBC clusters
                         |
query -------------------+
                         v
scan every member with normalized weighted Euclidean distance
```

Active distance subtraction, squaring and accumulation use double. The working palette remains float32; preparation widens its exact values and the existing binary32 metric coefficients into a private double search copy, then widens each query once. The search palette uses 12 additional bytes per color compared with a float32 copy, before accounting for removed covariance state. Clusters preserve member traversal and the existing nearest-distance contract, including freedom at ties. No query mutates the palette. The 8-bit implementation remains a direct unweighted scan.

## Method

### Mahalanobis mathematics

For cluster mean `mu[j]`, covariance matrix `Sigma[j]`, and query `x`, the squared Mahalanobis distance is:

```text
M[j](x) = (x - mu[j])^T inverse(Sigma[j]) (x - mu[j])
```

Unlike an axis-isotropic Euclidean ball, this form measures displacement relative to cluster variance and correlation:

```text
Euclidean contours             Mahalanobis contours

       *****                         ********
     **  mu **                    ***   /    ***
       *****                    **    mu       **
                                  ***    /   ***
                                     ******
```

A long, correlated color cluster can therefore regard motion along its major axis as less surprising than the same raw motion across its narrow axis.

The historical implementation computed covariance in range-weighted coordinates with a `1e-6` diagonal regularizer. This described its unused metadata, not the selection metric. The mathematical form above explains the policy's name; current queries do not evaluate it.

### Returned lookup result

Every member is compared using:

```text
D_w(i, x) = sum_d w[d] (x[d] - c[i][d])^2
```

The returned color minimizes normalized weighted Euclidean distance, not Mahalanobis distance. Removing the unused covariance calculation does not introduce anisotropic selection or pruning.

## Guarantee

The float32 path is nearest-distance exact in its normalized weighted-Euclidean metric because the union of all cluster-member loops visits every palette entry. Cluster order may change which tied index is retained. The 8-bit path is a normal scan-index-exact unweighted scan.

This result can differ from float32 `none` in unequal-range color spaces because `none` does not use the range-normalized weights.

## Cost

With `J = min(K,16)`:

```text
cluster assignment                  Theta(K J)
one float32 query                    Theta(K)
whole float32 image                  Theta(K J + P K)
extra space                          Theta(K + J)
one 8-bit query                      Theta(K)
```

Any future pruning rule must prove a lower bound in the same metric as the final palette comparison. A Mahalanobis score is not automatically a lower bound on weighted Euclidean member distance.

## Name and sources

The name refers to P. C. Mahalanobis's generalized statistical distance, introduced in [“On the Generalised Distance in Statistics”](https://doi.org/10.1007/s13171-019-00164-5). That source explains the statistical distance; it does not describe libsixel's RBC clustering or current query loop.

The policy was introduced with RBC in the [`mahalanobis` implementation commit](https://github.com/saitoha/libsixel/commit/ff38055039f1ac41aec32b81447f10b7c82fd691). The implementation is [`lookup-policy-mahalanobis.c`](../../../../src/lookup-policy-mahalanobis.c).
