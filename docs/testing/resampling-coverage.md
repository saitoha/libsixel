# Resampling regression coverage

This inventory complements the public kernel contracts in the [compact](../functionality/resampling/compact-kernels.md) and [wide-support](../functionality/resampling/wide-kernels.md) method references. Those references own the method-specific exact PPM tests. This document groups cross-policy and measurement-regression probes that are valuable for fault localization but do not define universal byte-identity or benchmark promises.

## Precision, SIMD, and threading intersections

The precision cases encode a two-tone P6 PPM through a fixed 101-entry safe-tone palette and compare direct decoded RGB output with a separately encoded precomputed PPM. Gamma-space `preserve` must produce the safe midpoint 33; the two linear-light policies must produce 46. The SIMD case uses a fixture deliberately stable across scalar and native rounding. The thread case repeats a nontrivial 16-pixel row over 16 rows so the four-thread horizontal pass has independently schedulable work, then compares both paths with the same oracle. Fixed palette input bypasses quantizer construction, final merge, cover, and snap; diffusion, lookup, GPU, CMS, and alternate loaders are explicitly disabled.

<!-- test-plan: tests/processing/resampling-policy/*.t -->

- [tests/processing/resampling-policy/0001_resize_precision_preserve_exact.t](../../tests/processing/resampling-policy/0001_resize_precision_preserve_exact.t) checks the byte-preserving gamma-space midpoint.
- [tests/processing/resampling-policy/0002_resize_precision_linear_exact.t](../../tests/processing/resampling-policy/0002_resize_precision_linear_exact.t) checks transient linear-light promotion and conversion back to bytes.
- [tests/processing/resampling-policy/0003_resize_precision_float_exact.t](../../tests/processing/resampling-policy/0003_resize_precision_float_exact.t) checks linear-light resize with retained float work precision.
- [tests/processing/resampling-policy/0004_simd_auto_matches_scalar_exact.t](../../tests/processing/resampling-policy/0004_simd_auto_matches_scalar_exact.t) checks scalar and automatically selected SIMD output against one stable safe-tone oracle.
- [tests/processing/resampling-policy/0005_parallel_matches_serial_exact.t](../../tests/processing/resampling-policy/0005_parallel_matches_serial_exact.t) checks multi-row four-thread and serial byte resize against one safe-tone oracle.

<!-- test-plan-end -->

The SIMD case is conditional evidence: on a scalar-only build, `auto` legitimately resolves to scalar. Runtime-cap selection has separate option and diagnostic tests. This fixture also does not erase the documented rule that scalar and vector implementations may differ by rounding on other inputs.

## Recorded implementation measurements

The plotting pipeline records far more than file shape and finiteness. The following tests recompute summary metrics from the checked-in CSV samples and compare them with the reviewable tolerances in [`tests/data/expected/resampling-measurement-baselines.csv`](../../tests/data/expected/resampling-measurement-baselines.csv). The measurement checker applies the same gates when new artifacts are reproduced, so regenerating hashes cannot by itself bless a changed discrete operator, alias response, directional spread, or radial-chirp error.

<!-- test-plan: tests/processing/resampling-measurement/*.t -->

- [tests/processing/resampling-measurement/0001_discrete_stencil_numeric_baseline.t](../../tests/processing/resampling-measurement/0001_discrete_stencil_numeric_baseline.t) gates nonzero support, normalized response sum, and absolute response sum for both measured geometries and all methods.
- [tests/processing/resampling-measurement/0002_frequency_numeric_baseline.t](../../tests/processing/resampling-measurement/0002_frequency_numeric_baseline.t) gates gain nearest 0.45 cycles per output pixel and above-Nyquist alias RMS.
- [tests/processing/resampling-measurement/0003_isotropy_numeric_baseline.t](../../tests/processing/resampling-measurement/0003_isotropy_numeric_baseline.t) gates directional range divided by mean gain.
- [tests/processing/resampling-measurement/0004_moire_numeric_baseline.t](../../tests/processing/resampling-measurement/0004_moire_numeric_baseline.t) gates radial-chirp RMS error against the supersampled area comparator.
- [tests/processing/resampling-measurement/0005_quality_size_numeric_baseline.t](../../tests/processing/resampling-measurement/0005_quality_size_numeric_baseline.t) gates MS-SSIM, mean Delta E 00, and encoded SIXEL size with explicit bands.
- [tests/processing/resampling-measurement/0006_speed_ratio_numeric_baseline.t](../../tests/processing/resampling-measurement/0006_speed_ratio_numeric_baseline.t) gates each method's median time relative to nearest-neighbor while deliberately avoiding an absolute host-time promise.

<!-- test-plan-end -->

These are regression bands for the recorded protocol, not universal rankings between kernels. Absolute speed is intentionally not gated because it is host- and load-dependent; the relative ratios use wider bands than deterministic quality measurements. Deliberately changing the scaler or protocol requires reviewing the new images and CSVs, explaining the change, and explicitly updating the baseline and tolerance table.
