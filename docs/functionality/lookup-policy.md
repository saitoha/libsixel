# Lookup Policy

## Definition

"Lookup policy" is libsixel terminology for the strategy used to answer a
repeated nearest-palette-color query:

```text
lookup(color candidate, completed palette) -> palette index
```

It is not a SIXEL wire-format term and does not mean reading a palette register
by an already known index. It is the mapping from a source or dither-adjusted
color to the index that will represent that color.

Lookup runs during palette application, after palette construction. A policy
may build an index or cache from the palette and then answer one query for each
non-transparent pixel. Its design trades among preparation time, per-pixel
cost, memory, sharing between workers, supported colorspaces, and exact or
discretized mapping behavior.

## CLI surface

`img2sixel` selects a policy with:

```text
-~ POLICY
--lookup-policy=POLICY
```

The short option is the two-character spelling `-~`; there is no `--~` long
option. The base policies are:

| Policy | Strategy |
| --- | --- |
| `auto` | Lets the encoder select a backend for the active pixel representation |
| `none` | Builds no lookup cache and scans the palette directly |
| `5bit` | Uses a dense table with five address bits per RGB channel |
| `6bit` | Uses a finer dense table with six bits per RGB channel for RGB input |
| `certlut` | Uses a hierarchical LUT whose refinement certifies the nearest color |
| `eytzinger` | Searches an implicit binary-tree layout with a small neighbor scan |
| `fhedt` | Builds a Voronoi grid with a three-pass 3D distance transform |
| `vptree` | Uses a metric VP-tree with safe-distance pruning |
| `rbc` | Uses Random Ball Cover cluster pruning |
| `mahalanobis` | Uses RBC clusters with Mahalanobis lower bounds |

Some policies expose suboptions and environment tuning for table layout,
resolution, refinement, or sharing. Use `img2sixel -H` or the
[`img2sixel(1)` manual](../../converters/img2sixel.1) for the current detailed
surface.

## What `none` means

`--lookup-policy=none` is the reference mental model: compare the candidate
with palette entries directly and return the best index. It disables lookup
acceleration, not color reduction, palette application, or dithering.

This differs from `-d none`, which disables dither adjustment and error
propagation but still needs a lookup for each pixel. For example:

```text
-d none --lookup-policy=none
```

means "map each original pixel by a direct palette scan." Either option may be
changed independently.

## Quality and performance

Lookup preparation can dominate a small image, while query throughput can
dominate a large image. Benchmark those phases separately and include palette
size, image size, colorspace, precision, thread count, and cache-sharing policy
in the result.

Dense bucket policies reduce the query color before indexing a finite table and
can therefore select a different entry from a direct scan. Other algorithms
have their own refinement and pruning contracts. Do not classify a policy as a
drop-in speed improvement until tests establish the required index equivalence
or an explicit image-quality threshold.

With error diffusion, one changed index also changes the error carried to later
pixels. A small local lookup difference can therefore create a larger spatial
difference in the final image.

## Design rules

- Keep policy instances derived from a completed palette; they must not choose
  or mutate the palette.
- Keep the per-pixel interface conceptually stateless. Image traversal and
  error propagation belong to the dither policy.
- Treat worker sharing as a lifecycle and thread-safety decision, not merely a
  performance flag.
- Test index behavior directly before relying only on encoded bytes or
  perceptual scores.
- Keep policy names and suboptions synchronized through the central option
  registry.

## Implementation and tests

Selection is in [`lookup-policy.c`](../../src/lookup-policy.c), and individual
backends are implemented by the `src/lookup-policy-*.c` translation units.
Their interface explicitly owns mapping pixels to palette indexes and forbids
image or output ownership; see [`6cells.h`](../../include/6cells.h).

Direct and end-to-end coverage is under
[`tests/quant/palette/usage/`](../../tests/quant/palette/usage/). For the caller
of this interface, see [Dithering](dithering.md); for the complete data flow,
see [Encoding Pipeline](encoding-pipeline.md).
