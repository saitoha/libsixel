# PNG cHRM arithmetic tests

This inventory covers the libpng-only arithmetic selector and its fixed-capacity rational evaluator. The [libpng policy](../loader/libpng.md#chrm-arithmetic-selection) defines the user-facing modes. The separate [cHRM boundary plan](png-chrm-boundaries.md) continues to own original-grid validation and metadata selection.

## Independent expectations and distinct changes

[`generate_png_arithmetic_tests.py`](../../tools/generate_png_arithmetic_tests.py) derives RGB-to-XYZ from literal PNG grid coordinates by rational Gaussian elimination, independently inverts the standard sRGB matrix, optionally applies rational Bradford adaptation, and rounds the resulting rational value directly to binary32. `--check` verifies checked-in bytes without changing fixtures; the common color fixture generator also includes these outputs. No production matrix or pixel evaluator supplies expected values or an error tolerance.

Three observations remain separate: mode selection uses threshold cases; target definition uses the exact rational sRGB identity, before the pixel shortcut; and CAT uses D50-adapted vectors plus a source-only control of the same grid. Ordinary double uses a fixed absolute budget of `2e-7` against the independently rounded references. Those component comparisons do not establish perceptual quality. The CLI registry image test uses the existing 16-by-16 snake and checks final encoded quality separately.

## Ordinary double value certificates

The original ordinary P3, D50, and permutation vectors retain their fixed absolute budget of `2e-7`. Additional nonneutral tiny vectors use an independently proved singleton float32 rounding cell, so their positive, negative, and subnormal components are compared bit for bit in `auto`, `double`, and `512bit`. The generator records exact results and forward bounds in [`png-arithmetic-certificates.json`](../../tests/data/color-numeric/png-arithmetic-certificates.json). Erasing colored components below `1e-10` must fail these tests even though such a mutation fits the older absolute budget.

The certificate expands the exact matrices with absolute values before multiplication: without CAT it is `abs(T) * abs(S)`; with CAT it is `abs(T) * abs(inverse(B)) * abs(diag(ratios)) * abs(B) * abs(S)`. For each output it multiplies that positive expression by the absolute input and `gamma(80) = 80u/(1-80u)`, where `u = 2^-53`. All original integer factors converted to double are exactly representable; each division and each multiplication or addition contributes at most one rounding factor. Eighty exceeds the number of factors on any expanded path through construction and the final dot product. Under the ordinary nearest-even environment, this bounds the error before float32 rounding without reading implementation coefficients or observed errors. The generator proves that both endpoints round to the same float32 value for each certified vector. This is a guarantee for the named finite vector set, not a universal correct-rounding contract for double.

Every dispatch boundary also checks the actual result for one nonneutral vector certified this way. Signed area and signed cone thresholds cover below, equal, and above cases; the forced-double ill-conditioned case uses the same independent certificate, not a tolerance fitted to its implementation output.

The two added RGB16 fixtures use P3/D65 or P3/D50, `gAMA=10000`, and literal samples near 64, whose transfer-decoded values are about `2^-100`. The generator derives normalization and the power-10 reference with rational arithmetic. Real loader callbacks inspect the complete float32 frame before clipping or quantization. Long and short loader suboptions and environment precedence traverse the production loader's shared typed resolver; these callback tests do not claim to invoke the complete `img2sixel` main/encoding path. Existing converter CLI trace and final-image tests remain separate integration gates, because tiny colors can quantize to black and disappear from a final SIXEL comparison.

## Capacity and arithmetic contract

The supported domain is PNG's `100000` xy grid with positive y, nonnegative XYZ numerators, nonzero primary area, nonzero required Bradford cones, and finite binary32 RGB inputs in `[0,1]`. The generator derives conservative bounds from that domain: standard-target coefficient numerators need at most 228 bits and denominators 225 bits; an input-weighted sum needs at most 378 bits, and the midpoint comparison at most 448 bits. The fixed 512-bit storage therefore has headroom for every supported case, including severe cancellation. The bound also keeps output magnitudes below `2^72`. This is a finite-domain argument, not evidence that fixed 512 bits suffice for arbitrary ICC or multiprecision work.

The exact evaluator stores binary32 input as integers in units of `2^-149`. Signed integer products and sums retain every cancellation bit; midpoint comparison makes one ties-to-even decision. Binary32 representation support is checked before construction succeeds. The exact path reads input and writes output bits without depending on floating arithmetic rounding modes. Ordinary double assumes the normal nearest-even environment and has no universal ULP promise.

The unavailable-backend test includes the actual PNG decoder and actual loader manager under a private test namespace. It injects only the selected core's unavailability and factory lookup needed to instantiate that decoder. A positive control proves builtin can load the same bytes. The tested chain contains `libpng:chrm_arithmetic=512bit,builtin` without `!`; it must return `SIXEL_NOT_IMPLEMENTED`, keep the callback count zero, and preserve input bytes and resolved settings across a repeated load. No production fault-injection option or new public API is introduced.

## Inventory

Core tests run independently of the optional libpng dependency. Loader/CLI observations skip early when their required component is disabled. The internal source-only, invalid-input, and transactional-build probes are supplementary defensive controls rather than additional public loader options.

<!-- test-plan: tests/loader/libpng/arithmetic/*.t -->

| Test | Observation |
| --- | --- |
| [tests/loader/libpng/arithmetic/0001_area_positive_below.t](../../tests/loader/libpng/arithmetic/0001_area_positive_below.t) | Area positive below. |
| [tests/loader/libpng/arithmetic/0002_area_positive_equal.t](../../tests/loader/libpng/arithmetic/0002_area_positive_equal.t) | Area positive equal. |
| [tests/loader/libpng/arithmetic/0003_area_positive_above.t](../../tests/loader/libpng/arithmetic/0003_area_positive_above.t) | Area positive above. |
| [tests/loader/libpng/arithmetic/0004_area_negative_below.t](../../tests/loader/libpng/arithmetic/0004_area_negative_below.t) | Area negative below. |
| [tests/loader/libpng/arithmetic/0005_area_negative_equal.t](../../tests/loader/libpng/arithmetic/0005_area_negative_equal.t) | Area negative equal. |
| [tests/loader/libpng/arithmetic/0006_area_negative_above.t](../../tests/loader/libpng/arithmetic/0006_area_negative_above.t) | Area negative above. |
| [tests/loader/libpng/arithmetic/0007_cone_negative_below.t](../../tests/loader/libpng/arithmetic/0007_cone_negative_below.t) | Cone negative below. |
| [tests/loader/libpng/arithmetic/0008_cone_negative_equal.t](../../tests/loader/libpng/arithmetic/0008_cone_negative_equal.t) | Cone negative equal. |
| [tests/loader/libpng/arithmetic/0009_cone_positive_equal.t](../../tests/loader/libpng/arithmetic/0009_cone_positive_equal.t) | Cone positive equal. |
| [tests/loader/libpng/arithmetic/0010_combined_small.t](../../tests/loader/libpng/arithmetic/0010_combined_small.t) | Combined small. |
| [tests/loader/libpng/arithmetic/0011_force_double_small.t](../../tests/loader/libpng/arithmetic/0011_force_double_small.t) | Force double small. |
| [tests/loader/libpng/arithmetic/0012_force_512_ordinary.t](../../tests/loader/libpng/arithmetic/0012_force_512_ordinary.t) | Force 512 ordinary. |
| [tests/loader/libpng/arithmetic/0013_exact_p3.t](../../tests/loader/libpng/arithmetic/0013_exact_p3.t) | Exact p3. |
| [tests/loader/libpng/arithmetic/0014_exact_p3_d50.t](../../tests/loader/libpng/arithmetic/0014_exact_p3_d50.t) | Exact p3 d50. |
| [tests/loader/libpng/arithmetic/0015_exact_permutation.t](../../tests/loader/libpng/arithmetic/0015_exact_permutation.t) | Exact permutation. |
| [tests/loader/libpng/arithmetic/0016_exact_near_primary.t](../../tests/loader/libpng/arithmetic/0016_exact_near_primary.t) | Exact near primary. |
| [tests/loader/libpng/arithmetic/0017_exact_combined.t](../../tests/loader/libpng/arithmetic/0017_exact_combined.t) | Exact combined. |
| [tests/loader/libpng/arithmetic/0018_auto_exact_combined.t](../../tests/loader/libpng/arithmetic/0018_auto_exact_combined.t) | Auto exact combined. |
| [tests/loader/libpng/arithmetic/0019_source_only_exact.t](../../tests/loader/libpng/arithmetic/0019_source_only_exact.t) | Source only exact. |
| [tests/loader/libpng/arithmetic/0020_double_budget_p3.t](../../tests/loader/libpng/arithmetic/0020_double_budget_p3.t) | Double budget p3. |
| [tests/loader/libpng/arithmetic/0021_double_budget_p3_d50.t](../../tests/loader/libpng/arithmetic/0021_double_budget_p3_d50.t) | Double budget p3 d50. |
| [tests/loader/libpng/arithmetic/0022_double_budget_permutation.t](../../tests/loader/libpng/arithmetic/0022_double_budget_permutation.t) | Double budget permutation. |
| [tests/loader/libpng/arithmetic/0023_neutral_auto.t](../../tests/loader/libpng/arithmetic/0023_neutral_auto.t) | Neutral auto. |
| [tests/loader/libpng/arithmetic/0024_neutral_double.t](../../tests/loader/libpng/arithmetic/0024_neutral_double.t) | Neutral double. |
| [tests/loader/libpng/arithmetic/0025_neutral_512bit.t](../../tests/loader/libpng/arithmetic/0025_neutral_512bit.t) | Neutral 512bit. |
| [tests/loader/libpng/arithmetic/0026_standard_target_identity.t](../../tests/loader/libpng/arithmetic/0026_standard_target_identity.t) | Standard target identity. |
| [tests/loader/libpng/arithmetic/0027_singular_state.t](../../tests/loader/libpng/arithmetic/0027_singular_state.t) | Singular state. |
| [tests/loader/libpng/arithmetic/0028_invalid_pixels_separate.t](../../tests/loader/libpng/arithmetic/0028_invalid_pixels_separate.t) | Invalid pixels separate. |
| [tests/loader/libpng/arithmetic/0029_invalid_pixels_alias.t](../../tests/loader/libpng/arithmetic/0029_invalid_pixels_alias.t) | Invalid pixels alias. |
| [tests/loader/libpng/arithmetic/0030_loader_double.t](../../tests/loader/libpng/arithmetic/0030_loader_double.t) | Loader double. |
| [tests/loader/libpng/arithmetic/0031_loader_512bit.t](../../tests/loader/libpng/arithmetic/0031_loader_512bit.t) | Loader 512bit. |
| [tests/loader/libpng/arithmetic/0032_loader_auto.t](../../tests/loader/libpng/arithmetic/0032_loader_auto.t) | Loader auto. |
| [tests/loader/libpng/arithmetic/0033_unavailable_terminal.t](../../tests/loader/libpng/arithmetic/0033_unavailable_terminal.t) | Selected 512bit unavailable remains terminal. |
| [tests/loader/libpng/arithmetic/0034_apng_exact.t](../../tests/loader/libpng/arithmetic/0034_apng_exact.t) | Apng exact. |
| [tests/loader/libpng/arithmetic/0035_background_exact.t](../../tests/loader/libpng/arithmetic/0035_background_exact.t) | Background exact. |
| [tests/loader/libpng/arithmetic/0036_env_override.t](../../tests/loader/libpng/arithmetic/0036_env_override.t) | CLI arithmetic overrides the environment. |
| [tests/loader/libpng/arithmetic/0037_env_invalid.t](../../tests/loader/libpng/arithmetic/0037_env_invalid.t) | Invalid environment arithmetic retains auto. |
| [tests/loader/libpng/arithmetic/0038_env_empty.t](../../tests/loader/libpng/arithmetic/0038_env_empty.t) | Empty environment arithmetic retains auto. |
| [tests/loader/libpng/arithmetic/0039_default_auto.t](../../tests/loader/libpng/arithmetic/0039_default_auto.t) | Omitted arithmetic selects auto. |
| [tests/loader/libpng/arithmetic/0040_icc_not_applicable.t](../../tests/loader/libpng/arithmetic/0040_icc_not_applicable.t) | Selected ICC bypasses cHRM arithmetic. |
| [tests/loader/libpng/arithmetic/0041_srgb_not_applicable.t](../../tests/loader/libpng/arithmetic/0041_srgb_not_applicable.t) | Selected sRGB bypasses cHRM arithmetic. |
| [tests/loader/libpng/arithmetic/0042_untagged_not_applicable.t](../../tests/loader/libpng/arithmetic/0042_untagged_not_applicable.t) | Untagged PNG bypasses cHRM arithmetic. |
| [tests/loader/libpng/arithmetic/0043_cms_disabled.t](../../tests/loader/libpng/arithmetic/0043_cms_disabled.t) | Disabled CMS bypasses cHRM arithmetic. |
| [tests/loader/libpng/arithmetic/0044_singular_not_applicable.t](../../tests/loader/libpng/arithmetic/0044_singular_not_applicable.t) | Exactly collinear cHRM remains metadata fallback. |
| [tests/loader/libpng/arithmetic/0045_invalid_cli.t](../../tests/loader/libpng/arithmetic/0045_invalid_cli.t) | Invalid cli. |
| [tests/loader/libpng/arithmetic/0046_wrong_owner.t](../../tests/loader/libpng/arithmetic/0046_wrong_owner.t) | Wrong owner. |
| [tests/loader/libpng/arithmetic/0047_lsqa_arithmetic.t](../../tests/loader/libpng/arithmetic/0047_lsqa_arithmetic.t) | Lsqa forwards its loader arithmetic setting. |
| [tests/loader/libpng/arithmetic/0048_lcms2_arithmetic.t](../../tests/loader/libpng/arithmetic/0048_lcms2_arithmetic.t) | lcms2 selection retains exact libpng cHRM arithmetic. |
| [tests/loader/libpng/arithmetic/0049_colorsync_arithmetic.t](../../tests/loader/libpng/arithmetic/0049_colorsync_arithmetic.t) | colorsync selection retains exact libpng cHRM arithmetic. |
| [tests/loader/libpng/arithmetic/0050_cone_positive_below.t](../../tests/loader/libpng/arithmetic/0050_cone_positive_below.t) | Cone positive below branch and exact value. |
| [tests/loader/libpng/arithmetic/0051_cone_positive_above.t](../../tests/loader/libpng/arithmetic/0051_cone_positive_above.t) | Cone positive above branch and exact value. |
| [tests/loader/libpng/arithmetic/0052_cone_negative_above.t](../../tests/loader/libpng/arithmetic/0052_cone_negative_above.t) | Cone negative above branch and exact value. |
| [tests/loader/libpng/arithmetic/0053_ordinary_auto_p3.t](../../tests/loader/libpng/arithmetic/0053_ordinary_auto_p3.t) | Ordinary auto p3 independent numeric budget. |
| [tests/loader/libpng/arithmetic/0054_ordinary_auto_p3_d50.t](../../tests/loader/libpng/arithmetic/0054_ordinary_auto_p3_d50.t) | Ordinary auto p3_d50 independent numeric budget. |
| [tests/loader/libpng/arithmetic/0055_ordinary_auto_permutation.t](../../tests/loader/libpng/arithmetic/0055_ordinary_auto_permutation.t) | Ordinary auto permutation independent numeric budget. |
| [tests/loader/libpng/arithmetic/0056_tiny_p3_auto.t](../../tests/loader/libpng/arithmetic/0056_tiny_p3_auto.t) | P3 auto nonneutral tiny exact bits. |
| [tests/loader/libpng/arithmetic/0057_tiny_p3_double.t](../../tests/loader/libpng/arithmetic/0057_tiny_p3_double.t) | P3 double nonneutral tiny exact bits. |
| [tests/loader/libpng/arithmetic/0058_tiny_p3_512bit.t](../../tests/loader/libpng/arithmetic/0058_tiny_p3_512bit.t) | P3 512bit nonneutral tiny exact bits. |
| [tests/loader/libpng/arithmetic/0059_tiny_p3_d50_auto.t](../../tests/loader/libpng/arithmetic/0059_tiny_p3_d50_auto.t) | P3_d50 auto nonneutral tiny exact bits. |
| [tests/loader/libpng/arithmetic/0060_tiny_p3_d50_double.t](../../tests/loader/libpng/arithmetic/0060_tiny_p3_d50_double.t) | P3_d50 double nonneutral tiny exact bits. |
| [tests/loader/libpng/arithmetic/0061_tiny_p3_d50_512bit.t](../../tests/loader/libpng/arithmetic/0061_tiny_p3_d50_512bit.t) | P3_d50 512bit nonneutral tiny exact bits. |
| [tests/loader/libpng/arithmetic/0062_loader_tiny_p3_auto.t](../../tests/loader/libpng/arithmetic/0062_loader_tiny_p3_auto.t) | Real loader p3 auto tiny frame bits. |
| [tests/loader/libpng/arithmetic/0063_loader_tiny_p3_double.t](../../tests/loader/libpng/arithmetic/0063_loader_tiny_p3_double.t) | Real loader p3 double tiny frame bits. |
| [tests/loader/libpng/arithmetic/0064_loader_tiny_p3_512bit.t](../../tests/loader/libpng/arithmetic/0064_loader_tiny_p3_512bit.t) | Real loader p3 512bit tiny frame bits. |
| [tests/loader/libpng/arithmetic/0065_loader_tiny_p3_d50_auto.t](../../tests/loader/libpng/arithmetic/0065_loader_tiny_p3_d50_auto.t) | Real loader p3_d50 auto tiny frame bits. |
| [tests/loader/libpng/arithmetic/0066_loader_tiny_p3_d50_double.t](../../tests/loader/libpng/arithmetic/0066_loader_tiny_p3_d50_double.t) | Real loader p3_d50 double tiny frame bits. |
| [tests/loader/libpng/arithmetic/0067_loader_tiny_p3_d50_512bit.t](../../tests/loader/libpng/arithmetic/0067_loader_tiny_p3_d50_512bit.t) | Real loader p3_d50 512bit tiny frame bits. |
| [tests/loader/libpng/arithmetic/0068_loader_tiny_env_double.t](../../tests/loader/libpng/arithmetic/0068_loader_tiny_env_double.t) | Loader shared resolver env_double tiny frame bits. |
| [tests/loader/libpng/arithmetic/0069_loader_tiny_env_512bit.t](../../tests/loader/libpng/arithmetic/0069_loader_tiny_env_512bit.t) | Loader shared resolver env_512bit tiny frame bits. |
| [tests/loader/libpng/arithmetic/0070_loader_tiny_env_auto.t](../../tests/loader/libpng/arithmetic/0070_loader_tiny_env_auto.t) | Loader shared resolver env_auto tiny frame bits. |
| [tests/loader/libpng/arithmetic/0071_loader_tiny_option_over_env.t](../../tests/loader/libpng/arithmetic/0071_loader_tiny_option_over_env.t) | Loader shared resolver option_over_env tiny frame bits. |
| [tests/loader/libpng/arithmetic/0072_loader_tiny_short_over_env.t](../../tests/loader/libpng/arithmetic/0072_loader_tiny_short_over_env.t) | Loader shared resolver short_over_env tiny frame bits. |
| [tests/loader/libpng/arithmetic/0073_loader_tiny_invalid_env.t](../../tests/loader/libpng/arithmetic/0073_loader_tiny_invalid_env.t) | Loader shared resolver invalid_env tiny frame bits. |
| [tests/loader/libpng/arithmetic/0074_loader_tiny_empty_env.t](../../tests/loader/libpng/arithmetic/0074_loader_tiny_empty_env.t) | Loader shared resolver empty_env tiny frame bits. |

<!-- test-plan-end -->
