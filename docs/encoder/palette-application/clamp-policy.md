# Dither clamp policy

`-0` / `--clamp-policy` controls byte and float spatial error-diffusion feedback during palette application. Byte carry candidates and float working coordinates can leave the byte cube with negative or over-range components. It does not change loader samples, CMS, resizing, clustering, or color-space conversion. See [working color space](working-colorspace/README.md) for the coordinate system in which diffusion runs and [loader precision](../../loader/precision-policy.md) for source preservation.

## Options

| Policy | Behavior |
| --- | --- |
| `auto` | Select `soft`. This is the default. |
| `soft` | Scale the added correction vector to keep the corrected sample within source-containing bounds with headroom. |
| `hard` | Clamp corrected channels independently to source-containing bounds without headroom. |
| `none` | Do not limit the correction. Finite-value validation still applies. |

`threshold` (`T`) is a finite factor from 1 through 16, default 1.01. `-0 soft:threshold=1.1` and `-0 soft:T1.1` are equivalent. The threshold is accepted but inactive for `hard` and `none`. `SIXEL_DITHER_CLAMP_POLICY` and `SIXEL_DITHER_CLAMP_THRESHOLD` provide defaults; explicit arguments win. Repeating `-0` resets the previous suboptions before reapplying the environment and the new argument. Invalid explicit values are errors; invalid environment values are ignored.

## Library configuration

`sixel_dither_set_clamp_policy(dither, policy, threshold)` configures the low-level dither API with `SIXEL_DITHER_CLAMP_AUTO`, `SOFT`, `HARD`, or `NONE`. The initial settings are auto and 1.01, independently of environment variables. A null context, unknown policy, or non-finite/out-of-range threshold returns `SIXEL_BAD_ARGUMENT` without changing either setting. Configure before applying a palette. Each dither and encoder owns independent settings; changing or destroying one instance does not change another.

## Numeric contract

### Byte carry arithmetic

The eight byte carry kernels use denominator D (4096 for perturbed FS), source components s in 0–255 and integer carries c in units of 1/D. Once per band, compute `(T - 1) * 255 * D` in double and round to the nearest integer M, with exact half ties to even. For each source component, the inclusive carry bounds are `L = -M - s*D` and `U = (255-s)*D + M`. Thus headroom is quantized in carry units; default T=1.01 has slightly different coordinate bounds for different denominators.

After the deadzone decision, soft leaves an in-bound correction unchanged. For each component outside its bounds, form the exact ratio b/c using the crossed bound b. Normalize both signs to make the denominator positive, compare ratios by int64 cross multiplication, and select the smallest ratio. Equal ratios retain the first component and give identical rational candidates. Apply that one ratio to every component: the candidate is `(s*D*c_k + c*b_k)/(D*c_k)`, with signs normalized to a positive denominator. Integer quotient and remainder round this rational to the nearest integer, with half ties to even. No floating-point arithmetic is used to select the scale or round the soft candidate. The correction direction is preserved before rounding, with at most half a level of component rounding error.

Hard clamps each numerator `s*D + c` to `[0,255*D]`, then uses the same nearest-even integer division. Its results match the double candidate arithmetic for all supported denominators. None retains its checked double candidate calculation and int32 carry overflow detection.

Lookup receives the rounded candidate projected to 0–255; diffusion propagates the rounded candidate before projection minus the selected palette color. Transparent pixels and retained 6delta colors follow the [byte diffusion contract](dithering/README.md#byte-spatial-diffusion). The float definition below does not quantize headroom and does not promise identical output to byte arithmetic. The deadzone comparison remains a separate operation; excess-precision targets retain its original double expression rather than changing its strict boundary.

At T=16 and D<=4096, the absolute carry boundary is at most 16*255*4096. Rounded residuals are bounded by 4080, and perturbed FS has incoming coefficient sum at most 7168, hence each carry and partial sum is bounded by 29,245,440. Ratio products and candidate numerators fit int64: `abs(N) <= (255*4096 + 16*255*4096)*29,245,440 < 2^49`, and `Q < 2^37`. The implementation also supports arbitrary int32 numeric-test carries without overflowing these products.

### Float working arithmetic

Let `s` be the original working-space source vector, `e` the accumulated correction, and `u = s + e` the candidate passed to nearest-palette lookup. Each channel has nominal working-coordinate limits `a` and `b`. The bounds are expanded to include the original source: `lo = min(a, s)` and `hi = max(b, s)`.

For `soft`, expand both ends by `(threshold - 1) * (b - a)`. Choose the largest common factor `r` in `[0, 1]` such that every channel of `s + r * e` fits those bounds. Scaling with one factor preserves the direction of the correction in the working coordinates. For `hard`, limit each candidate channel independently to `[lo, hi]`.

For normalized RGB and a source inside `[0, 1]`, threshold 1.1 gives bounds `[-0.1, 1.1]`. An original component of 1.5 instead has an upper soft bound of 1.6. An unchanged source remains unchanged even when it contains negative or over-range components. These are coordinate bounds, not a physical gamut boundary, perceptual color difference, or output-device clipping rule. Lab-family channels use their own nominal ranges.

The limiter runs immediately before palette lookup, after incoming spatial contributions and, where applicable, temporal correction have been combined. The residual sent to later pixels is the limited candidate minus the chosen output color. Limiting can therefore discard error and change the local average. It does not guarantee that a palette's convex hull contains the source color or that all tones remain exactly reproducible.

A bound on corrected coordinates differs from a fixed cap on correction magnitude. With a black/white palette and source 0.25, a correction cap below 0.25 prevents the candidate from ever crossing the 0.5 decision boundary. That can remove white dots entirely. Headroom limiting permits that boundary crossing; low-color gradients still require separate quality evaluation.

## Scope and precision

The policy applies to FS, Atkinson, Jarvis--Judice--Ninke, Stucki, Burkes, the Sierra variants, LSO2, and the interframe policy's corrected working samples. Positional noise and no-dither paths do not propagate quantization residuals, so this policy does not apply to them. [Positional candidate precision](dithering/README.md#positional-candidate-precision) defines their float bias and byte saturation boundaries. LSO2's existing lightness-only diffusion in Lab-family spaces is unchanged. Its correction has the form `(delta_L, 0, 0)`, so only L can constrain the soft scale factor; unchanged source chroma cannot suppress lightness compensation even outside nominal chroma bounds. No extra L weighting is needed. RGB working spaces still diffuse all three components. LSO2's error-dependent coefficient table continues to control both spatial distribution and total propagated error separately from this limiter.

Byte FS, Atkinson, Jarvis--Judice--Ninke, Stucki, Burkes and Sierra use integer carry rows. The limiter runs once on the combined correction before candidate rounding and byte lookup projection. The propagated residual uses the rounded candidate before projection. `none` can accumulate error indefinitely for colors outside the palette hull; exceeding int32 carry capacity returns an error. Byte LSO2, high-color and interframe/STBN retain their separate arithmetic. Float diffusion retains its existing limiter and defaults.

A float source, a non-gamma working space, an authoritative float palette, or an explicit float precision request selects float work. The clamp policy never selects arithmetic precision. Byte and float arithmetic are not promised to produce identical indexes. The [byte-application test](../../../tests/processing/precision/0016_clamp_keeps_byte_application.t) traps float promotion allocations, and the [planner check](../../../tests/_static/sh/staticcheck-dither-clamp-precision.sh) prevents the palette-application planner from using the clamp setting to select precision.

On the selected Display P3 and ordinary sRGB float paths, sources can hold extended coordinates outside the sRGB cube and the residual is computed against the unrounded working palette; the limiter bounds only the feedback and keeps those original coordinates, while `-0none` propagates the unrestricted residual. See [Output color space](../output/output-colorspace.md#display-p3-selected-cpu-float-path).

The interframe implementation retains its existing quantized history representation. This option controls the corrected sample supplied to lookup; it does not increase temporal-history precision or remove its representation limits.

Each hard/soft float diffusion request owns an immutable source snapshot. Unlimited feedback (`none`) needs no source-dependent bound and allocates no snapshot. Parallel bands do not share mutable limiter state. Band boundaries can still change incoming errors, so use a fixed worker count, scan order, and palette when comparing policies.

### Snapshot storage and process memory

For an RGB float request covering `W` by `H` pixels, hard and soft each allocate one `W * H * 3 * sizeof(float)` snapshot. With binary32 components this is 12 bytes per pixel. The snapshot preserves the original coordinates exactly and owns independent storage; allocation failure is reported and does not select another policy. Finishing a failed or already finished snapshot does not free source pixels. The original source is needed by hard as well as soft because both expand their bounds to contain extended source coordinates. Byte carry diffusion limits feedback without a source snapshot.

| Full-image RGB request | Hard or soft snapshot | None snapshot |
| --- | ---: | ---: |
| 640 × 480 | 3.516 MiB | 0 |
| 1920 × 1080 | 23.730 MiB | 0 |
| 3840 × 2160 | 94.922 MiB | 0 |

These are derived allocation sizes, not measured whole-process peak RSS. Input decoding, format conversion, working copies, palette construction, output indexes, and queues can coexist or peak at other times. Consequently, subtracting two process RSS peaks does not directly measure the snapshot. A parallel request uses its effective work-band height including warm-up rows; sum the sizes of simultaneously active snapshots rather than multiplying the full-image size by the requested worker count. [Work-band geometry](../../threading/overview.md#work-band-seams-and-image-quality) defines that execution boundary. Keep format and concurrency fixed when comparing hard, soft and none; these snapshot sizes apply only to float paths. Byte diffusion uses the carry rows described in the [dithering reference](dithering/README.md#byte-spatial-diffusion), with no source snapshot.

## Quality and threshold selection

The default threshold 1.01 retains a small correction margin. It is a compromise, not a universal optimum: reducing headroom can improve photographic similarity and isolated-pixel error while worsening regional average error or low-color gradients. More headroom permits stronger compensation but can also extend runs of poor color choices when the palette cannot represent a region. Less headroom can suppress isolated artifacts while increasing bias or visible bands. Compare per-image results as well as dataset means; [measurement policy](../../quality/measurement-policy.md) owns the shared evaluation procedure. Maximum per-pixel color error and spatially averaged error answer different questions and do not replace a low-color gradient check.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| DC-01 | soft vector | [tests/processing/dither/clamp/0001_soft_vector.t](../../../tests/processing/dither/clamp/0001_soft_vector.t) |
| DC-02 | auto soft | [tests/processing/dither/clamp/0002_auto_soft.t](../../../tests/processing/dither/clamp/0002_auto_soft.t) |
| DC-03 | hard channels | [tests/processing/dither/clamp/0003_hard_channels.t](../../../tests/processing/dither/clamp/0003_hard_channels.t) |
| DC-04 | none extended | [tests/processing/dither/clamp/0004_none_extended.t](../../../tests/processing/dither/clamp/0004_none_extended.t) |
| DC-05 | source fixed point | [tests/processing/dither/clamp/0005_source_fixed_point.t](../../../tests/processing/dither/clamp/0005_source_fixed_point.t) |
| DC-06 | negative headroom | [tests/processing/dither/clamp/0006_negative_headroom.t](../../../tests/processing/dither/clamp/0006_negative_headroom.t) |
| DC-07 | zero headroom | [tests/processing/dither/clamp/0007_zero_headroom.t](../../../tests/processing/dither/clamp/0007_zero_headroom.t) |
| DC-08 | kernel fs | [tests/processing/dither/clamp/0008_kernel_fs.t](../../../tests/processing/dither/clamp/0008_kernel_fs.t) |
| DC-09 | kernel atkinson | [tests/processing/dither/clamp/0009_kernel_atkinson.t](../../../tests/processing/dither/clamp/0009_kernel_atkinson.t) |
| DC-10 | kernel jajuni | [tests/processing/dither/clamp/0010_kernel_jajuni.t](../../../tests/processing/dither/clamp/0010_kernel_jajuni.t) |
| DC-11 | kernel stucki | [tests/processing/dither/clamp/0011_kernel_stucki.t](../../../tests/processing/dither/clamp/0011_kernel_stucki.t) |
| DC-12 | kernel burkes | [tests/processing/dither/clamp/0012_kernel_burkes.t](../../../tests/processing/dither/clamp/0012_kernel_burkes.t) |
| DC-13 | kernel sierra1 | [tests/processing/dither/clamp/0013_kernel_sierra1.t](../../../tests/processing/dither/clamp/0013_kernel_sierra1.t) |
| DC-14 | kernel sierra2 | [tests/processing/dither/clamp/0014_kernel_sierra2.t](../../../tests/processing/dither/clamp/0014_kernel_sierra2.t) |
| DC-15 | kernel sierra3 | [tests/processing/dither/clamp/0015_kernel_sierra3.t](../../../tests/processing/dither/clamp/0015_kernel_sierra3.t) |
| DC-16 | kernel lso2 | [tests/processing/dither/clamp/0016_kernel_lso2.t](../../../tests/processing/dither/clamp/0016_kernel_lso2.t) |
| DC-17 | keep fs | [tests/processing/dither/clamp/0017_keep_fs.t](../../../tests/processing/dither/clamp/0017_keep_fs.t) |
| DC-18 | keep atkinson | [tests/processing/dither/clamp/0018_keep_atkinson.t](../../../tests/processing/dither/clamp/0018_keep_atkinson.t) |
| DC-19 | keep jajuni | [tests/processing/dither/clamp/0019_keep_jajuni.t](../../../tests/processing/dither/clamp/0019_keep_jajuni.t) |
| DC-20 | keep stucki | [tests/processing/dither/clamp/0020_keep_stucki.t](../../../tests/processing/dither/clamp/0020_keep_stucki.t) |
| DC-21 | keep burkes | [tests/processing/dither/clamp/0021_keep_burkes.t](../../../tests/processing/dither/clamp/0021_keep_burkes.t) |
| DC-22 | keep sierra1 | [tests/processing/dither/clamp/0022_keep_sierra1.t](../../../tests/processing/dither/clamp/0022_keep_sierra1.t) |
| DC-23 | keep sierra2 | [tests/processing/dither/clamp/0023_keep_sierra2.t](../../../tests/processing/dither/clamp/0023_keep_sierra2.t) |
| DC-24 | keep sierra3 | [tests/processing/dither/clamp/0024_keep_sierra3.t](../../../tests/processing/dither/clamp/0024_keep_sierra3.t) |
| DC-25 | keep lso2 | [tests/processing/dither/clamp/0025_keep_lso2.t](../../../tests/processing/dither/clamp/0025_keep_lso2.t) |
| DC-26 | option default | [tests/processing/dither/clamp/0026_option_default.t](../../../tests/processing/dither/clamp/0026_option_default.t) |
| DC-27 | option short | [tests/processing/dither/clamp/0027_option_short.t](../../../tests/processing/dither/clamp/0027_option_short.t) |
| DC-28 | option repeat | [tests/processing/dither/clamp/0028_option_repeat.t](../../../tests/processing/dither/clamp/0028_option_repeat.t) |
| DC-29 | option nan | [tests/processing/dither/clamp/0029_option_nan.t](../../../tests/processing/dither/clamp/0029_option_nan.t) |
| DC-30 | option minimum | [tests/processing/dither/clamp/0030_option_minimum.t](../../../tests/processing/dither/clamp/0030_option_minimum.t) |
| DC-31 | option maximum | [tests/processing/dither/clamp/0031_option_maximum.t](../../../tests/processing/dither/clamp/0031_option_maximum.t) |
| DC-32 | Interframe spatial correction reaches the common limiter without temporal history. | [tests/processing/dither/clamp/0032_kernel_interframe.t](../../../tests/processing/dither/clamp/0032_kernel_interframe.t) |
| DC-33 | LSO2 oklab limits L correction without changing extended source chroma or letting it suppress lightness compensation. | [tests/processing/dither/clamp/0033_lso2_oklab_lightness.t](../../../tests/processing/dither/clamp/0033_lso2_oklab_lightness.t) |
| DC-34 | LSO2 cielab limits L correction without changing extended source chroma or letting it suppress lightness compensation. | [tests/processing/dither/clamp/0034_lso2_cielab_lightness.t](../../../tests/processing/dither/clamp/0034_lso2_cielab_lightness.t) |
| DC-35 | LSO2 din99d limits L correction without changing extended source chroma or letting it suppress lightness compensation. | [tests/processing/dither/clamp/0035_lso2_din99d_lightness.t](../../../tests/processing/dither/clamp/0035_lso2_din99d_lightness.t) |
| DC-36 | Invalid low-level updates preserve both settings. | [tests/processing/dither/clamp/0036_api_invalid.t](../../../tests/processing/dither/clamp/0036_api_invalid.t) |
| DC-37 | Low-level dither settings survive another instance changing and being destroyed. | [tests/processing/dither/clamp/0037_dither_instances.t](../../../tests/processing/dither/clamp/0037_dither_instances.t) |
| DC-38 | Encoder clamp settings survive another instance changing and being destroyed. | [tests/processing/dither/clamp/0038_encoder_instances.t](../../../tests/processing/dither/clamp/0038_encoder_instances.t) |
| DC-39 | Accumulated temporal correction reaches the limiter and stores its bounded residual. | [tests/processing/dither/clamp/0039_temporal_history.t](../../../tests/processing/dither/clamp/0039_temporal_history.t) |
| DC-40 | Clearing temporal history removes prior correction before the next apply. | [tests/processing/dither/clamp/0040_temporal_reset.t](../../../tests/processing/dither/clamp/0040_temporal_reset.t) |
| DC-41 | FS vertical taps propagate the limited residual in raster order. | [tests/processing/dither/clamp/0041_fs_vertical_residual.t](../../../tests/processing/dither/clamp/0041_fs_vertical_residual.t) |
| DC-42 | FS reverses horizontal taps while propagating limited residuals in serpentine order. | [tests/processing/dither/clamp/0042_fs_serpentine_residual.t](../../../tests/processing/dither/clamp/0042_fs_serpentine_residual.t) |
| DC-43 | Native float soft-clamp FS bands match independent raster indexes after overlap warm-up, with a short final band and two/three workers. | [tests/processing/dither/clamp/0043_parallel_overlap.t](../../../tests/processing/dither/clamp/0043_parallel_overlap.t) |
| DC-44 | FS with odd overlap preserves absolute-row serpentine parity while each worker rebuilds private limiter history. | [tests/processing/dither/clamp/0044_parallel_serpentine_overlap.t](../../../tests/processing/dither/clamp/0044_parallel_serpentine_overlap.t) |
| DC-45 | FS with a requested two-row work band uses the effective six-row geometry for soft-clamp feedback. | [tests/processing/dither/clamp/0045_parallel_minimum_band.t](../../../tests/processing/dither/clamp/0045_parallel_minimum_band.t) |
| DC-46 | FS with a requested seven-row work band rounds to twelve rows and excessive overlap is capped at six before soft-clamp application. | [tests/processing/dither/clamp/0046_parallel_overlap_cap.t](../../../tests/processing/dither/clamp/0046_parallel_overlap_cap.t) |
| DC-47 | Soft allocates one independent exact-size snapshot and frees it once. | [tests/processing/dither/clamp/0047_soft_snapshot.t](../../../tests/processing/dither/clamp/0047_soft_snapshot.t) |
| DC-48 | Source-containing hard has the same snapshot size and ownership contract. | [tests/processing/dither/clamp/0048_hard_snapshot.t](../../../tests/processing/dither/clamp/0048_hard_snapshot.t) |
| DC-49 | None allocates no source snapshot. | [tests/processing/dither/clamp/0049_none_no_snapshot.t](../../../tests/processing/dither/clamp/0049_none_no_snapshot.t) |
| DC-50 | Snapshot allocation failure permits safe finish and a successful retry. | [tests/processing/dither/clamp/0050_snapshot_failure.t](../../../tests/processing/dither/clamp/0050_snapshot_failure.t) |
| DC-51 | No clamp policy promotes byte diffusion to float arithmetic, for any error-diffusion kernel. | [tests/processing/precision/0016_clamp_keeps_byte_application.t](../../../tests/processing/precision/0016_clamp_keeps_byte_application.t) |
| DC-52 | The palette-application planner does not use the clamp policy to select arithmetic precision. | [tests/_static/sh/staticcheck-dither-clamp-precision.sh](../../../tests/_static/sh/staticcheck-dither-clamp-precision.sh) |

| DC-53 | Byte rational rounding covers all supported denominators and half ties. | [tests/processing/dither/carry/0023_carry_rational_rounding.t](../../../tests/processing/dither/carry/0023_carry_rational_rounding.t) |
| DC-54 | Byte integer headroom and deadzone boundaries follow the numeric contract. | [tests/processing/dither/carry/0024_carry_rational_bounds.t](../../../tests/processing/dither/carry/0024_carry_rational_bounds.t) |
| DC-55 | Specialized and generic byte scans preserve candidate, mask and keep semantics. | [tests/processing/dither/carry/0025_carry_rational_stencil.t](../../../tests/processing/dither/carry/0025_carry_rational_stencil.t) |
| DC-56 | Hard candidates match the frozen double reference. | [tests/processing/dither/carry/0026_carry_hard_double.t](../../../tests/processing/dither/carry/0026_carry_hard_double.t) |
| DC-57 | Rational soft scaling preserves correction direction within rounding. | [tests/processing/dither/carry/0027_carry_soft_direction.t](../../../tests/processing/dither/carry/0027_carry_soft_direction.t) |
| DC-58 | The limiting component reaches its rounded integer boundary. | [tests/processing/dither/carry/0028_carry_soft_boundary.t](../../../tests/processing/dither/carry/0028_carry_soft_boundary.t) |
| DC-59 | Equal positive and negative ratios preserve nearest-even half ties. | [tests/processing/dither/carry/0029_carry_soft_tied_ratios.t](../../../tests/processing/dither/carry/0029_carry_soft_tied_ratios.t) |

### Quality regression tests

| ID | Contract | Owning test |
| --- | --- | --- |
| DCQ-01 | Short and environment thresholds reach the consumer, produce identical SIXEL, and retain the fixture quality floor. | [tests/cli/options/regression/0168_clamp_threshold_image_regression.t](../../../tests/cli/options/regression/0168_clamp_threshold_image_regression.t) |

### Coverage boundary

The parallel-band fixtures use an independent 50-digit FS recurrence with source-containing soft bounds, and verify that rounding each stored operation to binary32 produces the same indexes. Their minimum nearest-color squared-distance gap exceeds `0.0017`; exact index assertions do not require bit-identical intermediate floating calculations. The source and palette coordinates are binary-exact. Regenerate or verify the fixtures with `tools/generate_clamp_band_fixtures.py [--check]`. Each fixture differs from whole-frame serial rendering, zero overlap, omitted limiting and byte-projected palettes. These tests cover spatial work-band isolation; they do not establish temporal-history precision or all combinations of scan order and diffusion kernel. [Threading overview](../../threading/overview.md#work-band-seams-and-image-quality) owns the work-band/overlap behavior.

The value tests establish analytic limiter behavior and per-kernel dispatch. Retained-color tests compare propagated and skipped residuals in float diffusion. Dataset scores are supplementary measurements, not substitutes for those contracts.
