# Palette Quantization

## Purpose

Palette quantization constructs a small set of colors that represents a much
larger input color population. In the encoding pipeline it runs after loading
and normalization and before palette application:

```text
normalized pixels -> quantization -> palette entries
```

It answers **which colors are available to the encoder**. It does not assign a
palette index to every pixel; dithering and lookup perform that later step.

## CLI surface

`img2sixel` selects the palette solver with:

```text
-Q MODEL
--quantize-model=MODEL
```

`MODEL` may include model-specific suboptions. Use `img2sixel -H` or the
[`img2sixel(1)` manual](../../converters/img2sixel.1) for the current accepted
suboptions. `-p COLORS` sets the requested palette size and normally defaults
to 256 colors.

The base models are:

| Model | Primary objective and behavior |
| --- | --- |
| `auto` | Preserves the historical Heckbert median-cut selection |
| `heckbert` | Recursively splits color boxes and chooses representatives |
| `kmeans` | Places centroids to reduce weighted squared assignment error |
| `medoids` | Restricts centers to observed samples and reduces assignment cost |
| `center` | Restricts centers to samples and minimizes the maximum assignment radius |

Model-specific initialization, sampling, iteration, merge, cover, and polishing
settings change how a solver reaches or repairs its result. They remain part of
palette construction even when their names refer to assignment or lookup
inside the solver.

## Mathematical objectives

Let the solver receive color samples `x[i]` with non-negative weights `w[i]`
and produce at most `K` palette colors `c[j]`. Color is three-dimensional in
the selected clustering space. The distance written below is Euclidean in
that space; channel normalization and a nonlinear colorspace change the
geometry before the solver sees it.

K-means minimizes a weighted sum of squared errors:

```text
SSE(C) = sum_i w[i] * min_j ||x[i] - c[j]||^2
```

Lloyd iteration alternates between assigning every sample to its nearest
center and replacing each center by the weighted mean of its assigned
samples. The objective cannot increase in exact arithmetic, but the method
finds a local optimum rather than a guaranteed global optimum. Restarts and
initialization policy exist because the result depends on the initial centers.
Hamerly, Elkan, and Yinyang pruning use triangle-inequality bounds to avoid
some distance evaluations; they do not change the objective.

K-medoids uses the same nearest-center assignment form but requires every
`c[j]` to be one of the observed samples. This makes centers robust and
directly representable, but turns an inexpensive mean update into a discrete
swap search. The `pam`, `sample`, `random`, and `bandit` algorithms are,
respectively, FastPAM-style exhaustive swaps, CLARA sampling, CLARANS random
neighbor search, and a budgeted BanditPAM-style candidate search.

K-center instead minimizes the worst represented distance:

```text
radius(C) = max_i min_j ||x[i] - c[j]||
```

The `fft` spelling means farthest-first traversal, not a Fourier transform.
Starting from one sample, it repeatedly selects the sample farthest from its
nearest existing center. In a metric space this greedy construction is a
two-approximation for the discrete k-center radius. `swap` searches local
center replacements, while `hybrid` starts with farthest-first centers and
then applies guarded swaps. libsixel uses weighted SSE as a tie-breaker only
after preserving the radius objective.

Heckbert median cut is a partition heuristic rather than an optimizer for one
of those objectives. It builds a color histogram, repeatedly divides a box at
a weighted median along a selected axis or PCA direction, and represents each
leaf by a population-weighted color. Balanced division allocates palette
capacity across occupied color-space regions without performing repeated
global nearest-center assignment.

## Cost model and asymptotic order

Use these variables when discussing cost:

- `N`: pixels supplied to palette construction after the encoder's sampling;
- `S`: weighted samples or occupied bins retained by the selected solver;
- `K`: requested palette size;
- `B`: addressable cells in a preprocessing histogram;
- `I`: solver iteration limit;
- `A`: restart or trial count;
- `M`: sample size used by CLARA, where `M <= S`;
- `C`: swap candidates or random neighbors examined per iteration;
- `Q`: number of oversplit clusters supplied to optional final merging.

RGB dimensionality is fixed at three and is omitted from the formulas. These
are bounds for the current implementation structure, not claims that the
underlying NP-hard clustering problems are solved exactly.

| Model or phase | Dominant current work | Time order | Extra space |
| --- | --- | --- | --- |
| Histogram preprocessing | Clear bins, ingest pixels, collect occupied bins | `Theta(N + B)` | `Theta(B + S)` |
| Heckbert splitting | Partition occupied bins through up to `log K` balanced levels | usual `O(S log K)`; imbalanced worst case `O(S K)` | `O(S + K)` beyond the histogram |
| K-means, one restart | Lloyd assignments plus center-bound maintenance | worst case `O(I * (S K + K^2))` | `O(S + K)` for plain/Hamerly; up to `O(S K + K^2)` for Elkan-style bounds |
| K-center `fft` | Farthest sample scan and distance-cache update for each center | `Theta(S K)` | `Theta(S + K)` |
| K-center swaps | Test `C` replacements using nearest/second-nearest caches | `O(I C S)` after initialization | `O(S + K + C)` |

For K-means, `A` independent restarts multiply the one-restart time in the
table by `A`. Bounds can make Hamerly, Elkan, and Yinyang substantially faster
on separated data, but their worst case still evaluates every sample-center
pair.

The PCA median-cut path normally uses linear-time weighted selection, but an
allocation or convergence fallback may sort a box. Consequently, `O(S log K)`
describes the balanced normal path, not a universal hard bound for every
profile and fallback.

K-medoids deserves a separate breakdown because its sub-algorithms expose
different controlling variables:

| Medoids algorithm | Time order |
| --- | --- |
| `pam` | FastPAM-style iteration is `O(S^2 + S K)`; with `I` accepted-search iterations, `O(I * (S^2 + S K))` |
| `sample` | With `A` CLARA trials of `M` samples, `O(A * (I M^2 + S K))` |
| `random` | With `A` local searches and `C` neighbor probes, `O(A C S + S K)` using cached assignments |
| `bandit` | Data- and budget-dependent; batches and confidence pruning reduce evaluated point/candidate pairs, while an unhelpful-pruning worst case remains PAM-class |

For small and medium medoids problems the implementation may allocate an
`S`-by-`S` distance cache, changing extra space from linear to `Theta(S^2)` in
exchange for avoiding repeated distance arithmetic.

Optional final processing has its own cost. Ward merging starts with `Q`
oversplit clusters, stores a dense `Q`-by-`Q` cost matrix, and therefore uses
`Theta(Q^2)` space. Row-minimum invalidation makes `O(Q^3)` a conservative
worst-case time bound, although normal runs reuse most pair costs. Additional
Lloyd refinement contributes `O(I S K)`.

Every model must inspect its input, so palette construction is at least
linear in `N`. A run may look linear when `K`, iteration limits, and table
sizes are capped, but those fixed CLI limits must not be mistaken for a
generally logarithmic algorithm. Likewise, the word "squared" in the K-means
objective describes the loss function; it does not by itself imply quadratic
running time.

## Relationship to colorspaces and precision

Distance has meaning only in a particular representation. Clustering
colorspace and arithmetic precision can therefore alter the palette produced
from the same decoded pixels. Output palette conversion is a later concern and
must not be confused with the space in which the solver compares samples.

Tests for a quantizer change should state the colorspace, precision, palette
size, seed, and other non-default inputs that make the result reproducible.
When exact palette bytes are not the contract, assess the decoded result with
the [Quality Measurement Policy](../quality/measurement-policy.md).

## Bypassing construction

Monochrome, built-in palette, and palette-map modes supply palette entries
instead of deriving them from the source image. They therefore bypass or
replace normal `-Q` processing. The supplied palette is still applied to each
pixel by the dithering and lookup stage.

High-color mode is different again: it can redefine palette registers during
encoding and is not a fixed-palette quantizer model.

## Design rules

- Keep the quantizer responsible for palette entries and build telemetry, not
  final indexed pixels or SIXEL byte ordering.
- Keep accepted model and suboption names synchronized through the option
  registry rather than duplicating parser-only lists.
- Make fallback behavior explicit. The palette dispatcher currently falls
  back to the Heckbert path when a requested newer solver cannot complete.
- Evaluate both aggregate quality and failure cases such as rare saturated
  colors; one metric does not describe every palette defect.
- Separate palette-build cost from palette-application cost in benchmarks.

## Implementation and tests

Palette orchestration is in [`palette.c`](../../src/palette.c). The individual
models are implemented by [`palette-heckbert.c`](../../src/palette-heckbert.c),
[`palette-kmeans.c`](../../src/palette-kmeans.c),
[`palette-kmedoids.c`](../../src/palette-kmedoids.c), and
[`palette-kcenter.c`](../../src/palette-kcenter.c). Focused solver tests and CLI
quality cases are under [`tests/quant/palette/`](../../tests/quant/palette/).

For the surrounding data flow, start with
[Encoding Pipeline](encoding-pipeline.md).
