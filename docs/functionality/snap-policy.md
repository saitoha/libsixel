# Palette Snap Policy

`-_ POLICY` and `--snap-policy=POLICY` configure how the encoder maps palette channels onto the 101 RGB percentages that SIXEL can represent reversibly. This is a numeric stabilization transform, not gamut coverage repair: [`--cover-policy`](cover-policy.md) chooses or inserts palette anchors, while `--snap-policy` chooses the reversible target used when `-6` or `--6reversible` enables snapping.

Selecting a snap policy does not enable `-6`. This separation keeps two decisions independent: `-6` requests a reversible palette pipeline, and `--snap-policy` chooses how eligible colors approach that pipeline's safe tones.

## Why there are 101 safe tones

SIXEL RGB palette definitions carry integer percentages from 0 through 100. The decoder reconstructs an 8-bit channel from percentage $q$ as

$$
s_q = \left\lfloor\frac{255q + 50}{100}\right\rfloor, \qquad q \in \{0,\ldots,100\}.
$$

The resulting set $S=\{s_0,\ldots,s_{100}\}$ has 101 distinct byte values. The encoder converts a byte channel $x$ back to a SIXEL percentage as

$$
q(x) = \left\lfloor\frac{100x + 127}{255}\right\rfloor.
$$

For every $s_q \in S$, decoding the emitted percentage returns the same byte value. Arbitrary bytes outside $S$ generally move to a neighboring member of $S$ after one encode/decode cycle. Snapping the palette to $S^3$ before emission therefore removes that palette drift.

The 101 levels are per channel. This mechanism does not limit the palette to 101 colors; a palette entry is an RGB triplet whose three channels independently belong to the safe set.

## Pipeline position

The current fixed-palette path is:

```text
sampling -> palette-space transform -> binning -> quantizer (-Q)
                                                    |
                          optional early snap hooks selected by timing
                                                    |
                                                    v
                                  final palette merge (-F)
                                                    |
                                                    v
                              mandatory final snap when -6 is active
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

The final snap runs whenever `-6` is active, independently of `timing`. The timing policy only adds earlier solver hooks. The current byte-palette ordering places cover repair after the final snap; a merge-funded cover replacement or image-derived soft anchor can therefore introduce a value outside the safe set. Until this interaction is given an ordering fix and a dedicated regression, use `--cover-policy=off` when the fixed-point guarantee is more important than gamut coverage.

## Policy values

The default is `auto`, which currently has exactly the same behavior as `nearest`.

### `auto`

`auto` resolves directly to `nearest`; it does not currently inspect the quantizer, working color space, palette size, or image. The distinct spelling leaves room for a future measured resolver without changing the explicit `nearest` contract.

### `nearest`

For a float or perceptual solver hook, `nearest` finds the lower and upper safe sRGB tone around each channel, forms at most $2^3=8$ RGB candidates, transforms each candidate into the active working color space, and chooses the candidate with minimum distance there. This avoids choosing three independently nearest sRGB channels when a different corner of the local safe-tone cell is closer in the actual clustering geometry.

For gamma and linear RGB, the candidate distance is ordinary squared Euclidean distance in the active coordinates. For Oklab, CIELAB, and DIN99d, `channel_l` supplies the lightness weight $\lambda$:

$$
d^2 = \lambda (\Delta L)^2 + \frac{1-\lambda}{2}(\Delta a)^2 + \frac{1-\lambda}{2}(\Delta b)^2.
$$

The final byte-palette pass uses the precomputed per-channel safe-tone table. It does not repeat the eight-candidate working-space search because the stored result has already crossed the 8-bit RGB boundary.

### `reversible`

`reversible` selects the legacy independent-channel mapping. Each 8-bit sRGB channel is replaced by its precomputed fixed point without comparing the other corners of the surrounding safe-tone cell in the working color space. It is cheaper and historically direct, but it can be a worse geometric choice for a palette represented in a Lab-family space.

Both `nearest` and `reversible` target the same 101-level set. Their difference is how a target RGB triplet is selected, not whether the result is SIXEL-representable.

## Timing policy

The `timing` suboption forms an additive ladder:

| Value | Snap stages |
| --- | --- |
| `once` | final output only |
| `polish` | `once` plus the pre-final-merge polish stage |
| `merge` | `polish` plus final-merge iterations |
| `resolve` | `merge` plus quantizer iterations |
| `all` | `resolve` plus initial seeds |

Earlier snapping constrains more of the solver trajectory to the reversible lattice. It can reduce the amount of movement at final output, but it can also prevent a continuous-space solver from reaching a better unconstrained local optimum. `once` is therefore the default.

## Approach rate

The `rate` suboption applies

$$
x' = x + \rho(s-x), \qquad 0 \le \rho \le 1,
$$

where $x$ is the current component in the active representation, $s$ is the selected safe-tone target, and `rate` is $\rho$. `rate=1` reaches the target and preserves exact safe-tone membership. `rate=0` leaves the clamped input unchanged. Intermediate rates intentionally relax the fixed-point guarantee and are quality-tuning controls rather than reversible output settings.

## Configuration and precedence

The top-level environment variable is `SIXEL_PALETTE_SNAP_POLICY`; explicit `-_` or `--snap-policy` wins over it. Snap suboptions similarly override their environment defaults.

| Long suboption | Short form | Environment default | Built-in default |
| --- | --- | --- | --- |
| `timing=once|polish|merge|resolve|all` | `Ivalue` | `SIXEL_PALETTE_SNAP_TIMING_POLICY` | `once` |
| `rate=FACTOR` | `AFACTOR` | `SIXEL_PALETTE_SNAP_APPROACH_RATE` | `1.0` |
| `channel_l=FACTOR` | `LFACTOR` | `SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L` | `0.85` |

Examples:

```sh
# Use the default nearest-target policy and snap only final output.
img2sixel -6 --snap-policy=nearest image.png

# Constrain every eligible Oklab solver stage to exact safe tones.
img2sixel -6 -Woklab -_nearest:Iall:A1:L0.85 image.png

# Use the legacy independent-channel mapping and disable cover mutation.
img2sixel -6 --snap-policy=reversible --cover-policy=off image.png
```

## Cost and measurement policy

The safe-tone table has 256 entries and fixed construction cost. A final byte-palette snap is $O(K)$ for $K \le 256$ palette entries. A `nearest` float/perceptual hook evaluates at most eight candidates per palette entry, so it is also $O(K)$ with a larger colorspace-conversion constant. Earlier timing levels multiply that bounded work by the number of solver and merge iterations at which the hook runs; they do not add a per-image-pixel lookup stage.

Snap quality is not monotonic. Exact snapping removes palette round-trip drift but perturbs solver output, and an intermediate `rate` can improve a one-generation metric while losing reversibility. A reproducible comparison should report palette fixed-point failures after encode/decode/re-encode, Delta E or MSE for channel perturbation, MS-SSIM for spatial impact, palette-build and end-to-end time, and exact SIXEL byte size. It should hold `-Q`, `-F`, `-a`, `-X`, precision, lookup, diffusion, and thread count fixed while sweeping policy, timing, rate, and palette size.

The source tree does not yet contain a one-command snap-policy quality, speed, size, and reversibility measurement suite. Until such a suite records commands, fixtures, host/build provenance, raw data, and plots, no policy comparison should be treated as a refreshed default-selection result.

## Implementation references

The top-level schema and environment bindings are declared in [`options-registry.c`](../../src/options-registry.c). [`palette-common-snap.c`](../../src/palette-common-snap.c) owns safe-tone selection, working-space candidate comparison, timing, and approach rate; [`palette-common-snap.h`](../../src/palette-common-snap.h) owns the precomputed table and internal policy types. Quantizers call the shared snap hooks, while [`palette.c`](../../src/palette.c) owns their final composition with cover repair.

For neighboring stages, see [Palette Construction Pipeline](palette-pipeline.md), [Palette Quantization](quantization.md), [Palette Cover Policy](cover-policy.md), [Encoder Working Precision](precision.md), [Dithering](dithering.md), and [Lookup Policy](lookup-policy.md).

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and an owning test. The reciprocal `Policy:` reference in each test is checked by `staticcheck-doc-test-links`.

| ID | Contract | Owning test |
| --- | --- | --- |
| SP-01 | `reversible` has equivalent top-level short-option and environment behavior and reaches the snap consumer independently of cover policy. | [tests/cli/options/regression/0024_snap_policy_reversible_image_regression.t](../../tests/cli/options/regression/0024_snap_policy_reversible_image_regression.t) |
| SP-02 | `timing=all` has equivalent short-suboption and environment behavior and reaches the snap consumer. | [tests/cli/options/regression/0137_snap_policy_timing_image_regression.t](../../tests/cli/options/regression/0137_snap_policy_timing_image_regression.t) |
| SP-03 | `rate` has equivalent short-suboption and environment behavior, reaches the snap consumer, clamps the environment value at the upper bound, and preserves the image-quality floor. | [tests/cli/options/regression/0138_snap_policy_rate_image_regression.t](../../tests/cli/options/regression/0138_snap_policy_rate_image_regression.t) |
| SP-04 | `channel_l` has equivalent short-suboption and environment behavior, reaches the snap consumer, clamps the environment value at the lower bound, and preserves the image-quality floor. | [tests/cli/options/regression/0139_snap_policy_channel_l_image_regression.t](../../tests/cli/options/regression/0139_snap_policy_channel_l_image_regression.t) |
| SP-05 | `--cover-policy` rejects the removed snap suboption namespace. | [tests/cli/options/matching/0279_option_matching_cover_snap_suboption_rejected.t](../../tests/cli/options/matching/0279_option_matching_cover_snap_suboption_rejected.t) |

### Coverage audit boundary

The focused suite covers the new top-level policy path and every registered suboption consumer. It does not yet directly test every policy spelling, invalid or empty top-level values, repeated-option last-wins behavior, multi-frame state, exact membership of all 101 tones, byte-for-byte re-encoding, or the ordering interaction with cover repair. The image tests enforce a broad MS-SSIM floor on one small fixture but do not replace the reproducible measurement suite described above.
