# PNG cHRM arithmetic tests

This inventory covers the libpng-only arithmetic selector and its fixed-capacity rational evaluator. The [libpng policy](../loader/libpng.md#chrm-arithmetic-selection) defines the user-facing modes. The separate [cHRM boundary plan](png-chrm-boundaries.md) continues to own original-grid validation and metadata selection.

## Independent expectations and distinct changes

[`generate_png_arithmetic_tests.py`](../../tools/generate_png_arithmetic_tests.py) derives RGB-to-XYZ from literal PNG grid coordinates by rational Gaussian elimination, independently inverts the standard sRGB matrix, optionally applies rational Bradford adaptation, and rounds the resulting rational value directly to binary32. `--check` verifies checked-in bytes without changing fixtures; the common color fixture generator also includes these outputs. No production matrix or pixel evaluator supplies expected values or an error tolerance.

Three observations remain separate: mode selection uses threshold cases; target definition uses the exact rational sRGB identity, before the pixel shortcut; and CAT uses D50-adapted vectors plus a source-only control of the same grid. Ordinary double uses a fixed absolute budget of `2e-7` against the independently rounded references. Those component comparisons do not establish perceptual quality. The CLI registry image test uses the existing 16-by-16 snake and checks final encoded quality separately.

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

<!-- test-plan-end -->
