# Palette Cover Policy

`-a POLICY` and `--cover-policy=POLICY` control a final palette-repair pass that runs after palette construction. The pass is independent of the quantizer selected by `-Q`: the quantizer chooses representative colors, while cover repair tries to keep colors near important parts of the source gamut reachable during palette application and dithering. Reversible 101-level tone mapping is a different transformation configured by [`--snap-policy`](snap-policy.md).

## Pipeline position

The normal fixed-palette path is:

```text
sampling -> palette-space transform -> binning -> quantizer (-Q)
                                                    |
                                                    v
                  solver refinement and final merge (-F)
                                                    |
                                                    v
                         reversible snapping when -6 is active
                                                    |
                                                    v
                              palette cover repair (-a)
                                                    |
                                                    v
                     lookup preparation (-~) -> palette application (-d)
                                                    |
                                                    v
                                            SIXEL encoding
```

Cover repair therefore changes the palette consumed by lookup and dithering, but it does not select a quantizer, clustering color space, lookup data structure, or diffusion kernel. Use `--cover-policy=off` when benchmarking one of those stages in isolation.

The implementation currently applies cover repair only when the completed palette is stored as byte RGB entries. A native float32 palette bypasses the pass. If a byte palette was produced from an input format that the soft candidate reader cannot inspect, soft mode falls back to hard anchors. These are implementation boundaries, not a promise that float32 or every input layout needs no coverage repair.

## Why palette coverage matters

Let the final palette be a set of colors $P = \{p_1, \ldots, p_K\}$. If dithering represents a source color by spatially mixing palette entries, the average reconstructed color can only lie in the convex hull of the entries that lookup actually selects:

$$
\bar{p} = \sum_{i=1}^{K} w_i p_i, \qquad w_i \ge 0, \qquad \sum_i w_i = 1.
$$

A source color outside that reachable hull cannot be recovered by changing the diffusion kernel. Gamut-boundary colors are especially fragile because an accumulated error that points outside the RGB cube is clipped instead of creating a useful compensating lookup query. A small saturated UI element can consequently lock onto one nearby palette entry even when a global quantization metric judged the element too rare to preserve.

Convex-hull inclusion is necessary but not sufficient. Lookup is discrete and stateful under error diffusion, so the corrected samples must also visit entries that form the intended mixture. Cover repair does not compute an exact hull and does not guarantee exact reproduction. It adds a bounded set of anchors chosen as practical reachability proxies, then leaves lookup and diffusion to use them.

## Policy values

The default is `auto`.

| Policy | Hard-mode anchor set | Soft-mode budget | Meaning |
| --- | ---: | ---: | --- |
| `off` | 0 | 0 | Skip cover repair. |
| `corners` | 8 cube corners | up to 8 image-derived anchors | First coverage rung. |
| `faces` | corners plus 6 face centers, 14 total | up to 14 image-derived anchors | Adds partners for colors with one channel pinned at a gamut boundary. |
| `edges` | previous set plus 12 edge midpoints, 26 total | up to 26 image-derived anchors | Largest implemented coverage rung. |
| `all` | same as `edges` | same as `edges` | Exact alias of `edges`; it does not request a denser lattice. |
| `auto` | selected from actual palette size | selected from actual palette size | Resolves through the size ladder below. |

The registry also accepts `0` as a compatibility spelling for `off` and `1` as a compatibility spelling for `auto`. New configurations should use the named values.

### `auto`

`auto` resolves after the quantizer has reported the actual palette entry count, before optional cover growth:

| Actual entries | Effective policy | Maximum anchors |
| ---: | --- | ---: |
| 1-31 | `off` | 0 |
| 32-63 | `corners` | 8 |
| 64-255 | `faces` | 14 |
| 256 | `edges` | 26 |

The lowest rung is disabled below 32 colors because eight anchors would consume at least one quarter of the palette budget. An explicit policy is never rewritten by this size resolver.

### `off`

`off` is the control arm for quantizer, merge, lookup, and diffusion experiments. It is also appropriate when a caller supplies a deliberately constrained palette and any post-quantizer mutation would violate that external contract.

`off` affects only cover repair. Reversible safe-tone snapping still runs when `-6` requests it and is configured independently by `--snap-policy`.

### `corners`

In hard mode, `corners` targets the eight points in $\{0,255\}^3$. These anchors expand reach toward fully saturated combinations and black and white. In soft mode, the name denotes an eight-anchor budget rather than cube geometry.

### `faces`

In hard mode, `faces` contains the corners and adds `(0|255,128,128)`, `(128,0|255,128)`, and `(128,128,0|255)`. Face centers precede edge midpoints in the policy ladder because measured error-diffusion reachability improved more from partners that preserve one pinned channel than from a geometrically more obvious edge-first ordering. In soft mode, the name denotes a fourteen-anchor budget.

### `edges` and `all`

In hard mode, `edges` contains the previous fourteen anchors and adds the twelve permutations of `(128,0|255,0|255)`, for twenty-six anchors in total. `all` resolves to exactly the same internal policy. In soft mode, either spelling provides a maximum budget of twenty-six image-derived anchors.

## Cover modes

`cover_mode` selects how anchors are nominated. The default is `soft`.

### Soft mode

Soft mode protects colors supported by the current image instead of reserving fixed RGB-cube locations. It is intended to avoid spending entries on unoccupied colors that can appear as isolated full-intensity dots when diffusion uses them to discharge residual error.

The current algorithm uses a 4-bit-per-channel histogram with $16^3 = 4096$ cells. For each populated cell it stores a count and channel sums, and represents the cell by the mean of its samples rather than by the geometric cell center. A cell is eligible only when its population is at least

$$
t = \max\left(2, \left\lfloor\frac{N}{1024}\right\rfloor\right),
$$

where $N$ is the inspected pixel count. This threshold prevents a single hot pixel, cursor pixel, or compression artifact from nominating an anchor.

From the eligible cells, the algorithm repeatedly chooses the representative whose nearest squared RGB distance from both the completed palette and every already selected anchor is greatest. Selection stops at the policy budget or when no candidate is farther than the reach threshold. This is a farthest-first coverage heuristic over populous image colors, not another quantizer and not soft binning from the palette-construction pipeline.

Soft mode intentionally owns an empty candidate result. If the readable image is already covered according to the threshold, the pass does nothing; it must not fall back to hard anchors merely because it found no candidate. Hard fallback is reserved for an unsupported candidate input format.

Earlier support-point and bounding-box designs were rejected. A sparse set of support points inscribes the sampled hull and leaves regions between sampled directions uncovered. A componentwise bounding box encloses the hull but constructs colors that the image may never contain, and one outlier can expand most box corners into empty space. The populous-histogram rule preserves content support while remaining bounded.

### Hard mode

Hard mode uses the fixed RGB lattice described by the policy names. It is the only mode that can reserve coverage for colors absent from the current frame, which can be useful when a palette is intended for later content. The tradeoff is that every inserted anchor may spend capacity on a color that never appears in this image.

Hard cover distance is unweighted squared Euclidean distance in 8-bit RGB code values:

$$
d^2(x,p) = (x_R-p_R)^2 + (x_G-p_G)^2 + (x_B-p_B)^2.
$$

It does not inherit the clustering geometry selected by `-X`.

## Reach threshold and funding anchors

An anchor is treated as already reached when its nearest palette entry satisfies

$$
d^2 \le 24^2 \cdot 3 = 1728,
$$

which is an RGB Euclidean distance of approximately 41.6 code values. This is a proximity criterion, not an exact gamut-containment test.

With the default `cover_grow=0`, the requested palette size remains fixed. To place a missing anchor, the algorithm finds the globally closest pair of palette entries, replaces one entry with their componentwise midpoint, and reuses the other slot for the anchor. The merge is accepted only when the closest-pair squared distance is no greater than half the squared gap closed by the anchor. This data-dependent budget avoids spending a well-separated palette entry merely to place an anchor.

Placement repeats for at most four sweeps because merging a pair can move an entry away from an anchor that an earlier check considered reached. A sweep that places nothing terminates immediately. Failure to find a cheap enough pair skips that anchor and continues with the remaining candidates.

`cover_grow=1` first appends missing anchors while entries remain below the SIXEL encoder limit of 256. It may therefore return more entries than requested by `-p`, by at most the policy budget and never beyond 256. If only some anchors fit near the ceiling, the remaining anchors use the same merge-funded placement as fixed-size mode.

## Cost model

Let $N$ be the inspected pixel count, $K \le 256$ the completed palette size, $A \le 26$ the policy budget, $H=4096$ the soft histogram size, and $R \le 4$ the placement sweep limit.

- Hard candidate construction is constant-sized, and fixed-size placement is $O(RAK^2)$ in the worst case because each funded anchor searches all palette pairs.
- Soft candidate collection is $O(N + AH(K+A))$: one linear histogram pass followed by bounded farthest-first scans. Its histogram currently occupies approximately 64 KiB before allocator metadata.
- Soft fixed-size placement adds the same $O(RAK^2)$ bound as hard mode.
- Growth can avoid pair searches for anchors that fit in newly appended slots; any remainder still uses merge-funded placement.

Because $A$, $H$, $R$, and $K$ are capped by the format-facing implementation, soft mode is linear in image pixels plus bounded palette work. It is a final palette-build cost, not a per-pixel lookup cost. The changed palette can still affect later dithering time and SIXEL encoding size indirectly.

## Quality, speed, and encoded size

Cover repair is deliberately not monotonic under a global image metric. Preserving a rare saturated region can improve its local error while the merge used to fund an anchor slightly worsens common colors. Soft mode reduces this risk by choosing populated source regions; hard mode trades more image-specific accuracy for predictable gamut coverage. `cover_grow=1` avoids an existing-entry merge when room is available, but spends additional palette registers.

The following design measurement used a frozen 64-entry palette and equal merge funding for each hard anchor set. A probe was counted as stuck when diffusion failed to form the intended mixture and remained at essentially the full nearest-entry residual. The table explains the non-geometric `corners -> faces -> edges` ladder; it is not a broad image-quality benchmark.

| Hard anchors | Interior probes stuck | Face probes stuck | Edge probes stuck | Corner probes stuck | All probes stuck |
| --- | ---: | ---: | ---: | ---: | ---: |
| none | 86% | 100% | 100% | 100% | 96% |
| corners, 8 | 34% | 74% | 15% | 12% | 47% |
| corners plus face centers, 14 | 0% | 12% | 15% | 12% | 9% |
| previous plus edge midpoints, 26 | 0% | 4% | 10% | 12% | 5% |

Global mean Delta E or MSE alone can hide the defect this option targets. A durable comparison should report region-specific and tail color error for rare saturated patches, MS-SSIM for spatial impact, palette-build and end-to-end time, actual palette entry count, and exact SIXEL byte size. It should sweep palette sizes across the `auto` boundaries, compare `soft` and `hard`, keep `-Q`, `-F`, `-X`, `--precision`, lookup, and thread count fixed, and show both `-d none` and an error-diffusion control such as `-d fs`.

The source tree does not yet contain a one-command cover-policy quality, speed, and size benchmark comparable to the quantizer and lookup-policy measurement suites. Until such a suite records commands, fixtures, host/build provenance, raw CSV, and plots, the table above should be treated as implementation-design evidence rather than a continuously refreshed default-selection result.

## Configuration and precedence

The top-level environment variable is `SIXEL_PALETTE_COVER`; explicit `-a` or `--cover-policy` wins over it. Cover suboptions similarly override their environment defaults.

| Long suboption | Short form | Environment default | Built-in default |
| --- | --- | --- | --- |
| `cover_grow=0|1` | `V0` or `V1` | `SIXEL_PALETTE_COVER_GROW` | `0` |
| `cover_mode=soft|hard` | `Wsoft` or `Whard` | `SIXEL_PALETTE_COVER_MODE` | `soft` |

Examples:

```sh
# Isolate quantizer behavior by disabling cover repair.
img2sixel --cover-policy=off --quantize-model=kmeans image.png

# Keep 64 entries while reserving hard corners and face centers when affordable.
img2sixel -p 64 --cover-policy=faces:cover_mode=hard:cover_grow=0 image.png

# Let image-derived anchors grow the palette up to the 256-entry limit.
img2sixel -p 128 -a edges:Wsoft:V1 image.png

```

## Implementation references

The option schema and environment bindings are defined in [`options-registry.c`](../../src/options-registry.c). Cover policy resolution, fixed anchors, soft candidate selection, thresholds, and placement are implemented in [`palette-common-cover.c`](../../src/palette-common-cover.c) with shared constants and rationale in [`palette-common-cover.h`](../../src/palette-common-cover.h). [`palette.c`](../../src/palette.c) owns the final ordering and applies cover repair after every successful byte-palette solver. Safe-tone behavior is owned by the separate [Palette Snap Policy](snap-policy.md).

For the stages before and after this pass, see [Palette Construction Pipeline](palette-pipeline.md), [Palette Quantization](quantization.md), [Encoder Working Precision](precision.md), [Dithering](dithering.md), and [Lookup Policy](lookup-policy.md).

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and an owning test. The reciprocal `Policy:` reference in each test is checked by `staticcheck-doc-test-links`.

| ID | Contract | Owning test |
| --- | --- | --- |
| CP-01 | Hard anchor rungs, `auto` thresholds, explicit-policy preservation, environment/override precedence, fixed-size and growing placement, soft content support, and post-quantizer repair across every solver remain stable. | [tests/quant/palette/0005_cover_anchor.c](../../tests/quant/palette/0005_cover_anchor.c) |
| CP-02 | The top-level `corners` CLI and `SIXEL_PALETTE_COVER` environment forms produce equivalent output. | [tests/cli/options/migration/0018_cover_policy_environment_cli_equivalence.t](../../tests/cli/options/migration/0018_cover_policy_environment_cli_equivalence.t) |
| CP-03 | `cover_grow=1` has equivalent short-suboption and environment behavior, reaches the cover consumer, and preserves the image-quality floor. | [tests/cli/options/regression/0025_cover_policy_grow_image_regression.t](../../tests/cli/options/regression/0025_cover_policy_grow_image_regression.t) |
| CP-04 | `cover_mode=hard` has equivalent short-suboption and environment behavior, reaches the cover consumer, and preserves the image-quality floor. | [tests/cli/options/regression/0026_cover_policy_mode_image_regression.t](../../tests/cli/options/regression/0026_cover_policy_mode_image_regression.t) |

### Coverage audit boundary

The focused suite covers the core policy ladder, `auto` boundaries, explicit override precedence, hard and soft modes, fixed and growing palettes, every quantizer, and each registered suboption consumer. It does not yet directly test every top-level spelling (`off`, `faces`, `edges`, `all`, `0`, and `1`), invalid or empty top-level values, repeated-option last-wins behavior, float32 bypass and unsupported-soft fallback, multi-frame state, or the ordering interaction with `--6reversible`. Quality tests enforce a broad MS-SSIM floor on one small fixture but do not replace the missing reproducible cover-policy quality, speed, and size measurement suite described above.
