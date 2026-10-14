# Output gamut mapping

`img2sixel -U SPACE:gamut=POLICY` selects how colors outside the output RGB gamut are handled. The destination comes from `-U`, independently of the source frame's coordinate space. Negative or greater-than-one canonical sRGB coordinates can be valid colors inside a Display P3 or Adobe RGB destination and are retained.

| Policy | Behavior |
| --- | --- |
| `auto` | Default; resolves to `clip`. |
| `clip` | Retains the compatibility pipeline: no source mapping, and destination channels are bounded at output. |
| `edge-seeker` | Maps colors using a precomputed hue boundary table and constant-lightness chroma reduction in Oklab. Already representable colors are unchanged. |
| `local-minde` | Maps source colors and the application palette using binary search with local MINDE in Oklab. Already representable colors are unchanged, within the numerical boundary tolerance below. |

`gamut` has compact key `G` and environment default `SIXEL_OUTPUT_COLORSPACE_GAMUT`. An explicit suboption overrides that environment. Repeating `-U` replaces its base and tuning; an invalid replacement preserves the previous configuration. The gamut environment also applies to default sRGB output when `-U` is omitted. CLI-only `-U auto` retains common gamut tuning when terminal negotiation resolves the destination; its existing timeout, output-TTY and policy-restoration contracts remain at [output color space](output-colorspace.md).

```sh
img2sixel image.png -U display-p3:gamut=local-minde
img2sixel image.png -U gamma:Glocal-minde
img2sixel image.png -U display-p3:gamut=edge-seeker
img2sixel image.png -U icc:path=/path/to/profile.icc:gamut=local-minde
img2sixel image.png -U auto:gamut=local-minde:timeout_ms=200
```

`local-minde` and `edge-seeker` identify specific algorithms, not space selectors. Explicit selection does not permit substitution of another algorithm. A future recommendation change belongs to `auto`. There is no `oklch` alias, because Oklch is a coordinate representation shared by distinct algorithms.

## Numeric contract

### Local MINDE

The reference is [CSS Color 4, 9 October 2026, binary search with local MINDE](https://www.w3.org/TR/2026/CRD-css-color-4-20261009/#binsearch). Chroma is searched along a constant-lightness, constant-hue ray in Oklab: multiply `a` and `b` by the same nonnegative scale. This avoids a polar-coordinate conversion and does not add an Oklch choice to `-X` or `-W`.

The local clipping tolerance is Delta E OK `0.02`; the chroma search termination is `0.0001`. The candidate is clipped to the destination gamut and evaluated back in Oklab, allowing small lightness and hue changes instead of requiring excessive chroma reduction. Lightness at or beyond the SDR endpoints maps to destination black or white. This is not HDR tone mapping, saturation enhancement, or a general ICC perceptual rendering intent.

### EdgeSeeker

The reference is [CSS Color 4, 9 October 2026, EdgeSeeker](https://www.w3.org/TR/2026/CRD-css-color-4-20261009/#GMA-EdgeSeeker). At construction, 400 encoded RGB samples per HSL hue region at lightness 50% and 75% describe each destination's cusp and upper boundary. The destination transfer curves are used when decoding these samples, including the identity transfer for `-U linear`. Hue folds are removed by replacing the preceding overshoot with a bridge, rather than discarding only backward-moving samples. The sorted table interpolates across the 0/360-degree seam.

For an out-of-gamut color, the mapper looks up the cusp and signed curvature at its Oklab hue. Below the cusp, maximum chroma follows a straight line from black; above it, a fitted circle connects the cusp to white. Chroma is reduced at fixed lightness and hue, followed by destination clipping to contain approximation error. The circle intersection uses a rationalized expression which remains stable as curvature tends to zero. This method does not use local MINDE's Delta E tolerance or iterative chroma search. It shares the SDR black/white endpoint behavior.

Construction rejects a boundary which cannot be represented by this model: invalid hue folds, nonfinite data, cusps outside lightness `(0,1)`, nonpositive cusp chroma, or absolute curvature at least one. Decoded device endpoints must be zero and one within `2e-7`. The model is intended for ordinary display RGB gamuts; it is not a general solution for imaginary primaries such as ROMM RGB. The supported named spaces and the matrix/TRC Display P3 ICC fixture are covered by numerical tests; arbitrary matrix/TRC profiles are admitted only if construction succeeds. A successful geometric check does not certify physical primaries or perceptual accuracy for an arbitrary profile.

The table has at most 2,402 entries, each containing four doubles (about 75 KiB per encoder), plus temporary construction storage of about 176 KiB. Construction is performed when selecting the policy/destination, not per frame. In-gamut pixels take the common identity path; out-of-gamut pixels require table lookup and a geometric evaluation. There is no per-pixel allocation or iterative gamut search. This describes computational structure, not a measured speed advantage on every workload.

### Shared destination rules

Gamut membership is evaluated in the destination's linear channel domain before inverse transfer curves can clamp it. Named definitions use their primaries, white adaptation and transfer; supported matrix/TRC ICCs use the matrix and channel ranges between their decoded device endpoints. A tolerance of `2e-7` per linear destination channel covers float storage and matrix boundary error. An in-gamut input is returned directly, without a conversion round trip. Intermediate arithmetic uses double, and invalid/nonfinite conversions fail rather than publishing nonfinite output.

ICC LUT profiles cannot use `local-minde` or `edge-seeker`: a bounded inverse LUT is not a gamut-membership oracle. The mapper requires separable, supported, invertible channel curves and an invertible RGB matrix. `clip` continues to admit the existing supported ICC output subset; it does not override a LUT's own rendering behavior. Explicit unsupported mapping never silently falls back to clipping.

## Pipeline and policy boundaries

The mapper belongs to the shared encoder library, not the loader. Its immutable object owns the destination matrices and endpoint ranges. Separate encoders have separate policy objects; palette workers only read them. Changing gamut policy invalidates incompatible palette and delta caches. No environment access or allocation occurs in per-color mapping.

With `local-minde` or `edge-seeker`, encoding owns a private float frame. After geometry preprocessing, visible source pixels are mapped before lookup/diffusion. Palette-construction samples are also mapped before clustering. The existing sampling policy still determines the sample source: full-frame uses the preprocessed frame, while adaptive-grid uses its loaded-frame samples. The mapper does not silently change that policy or move adaptive sampling after resizing. Omitted pixels are excluded from mapping.

Generated palettes are checked again after clustering, merge, cover and workspace conversion, because those operations can produce new out-of-gamut representatives. Fixed palettes are mapped in their application copy; the original palette definition remains separate. The continuous float palette and its compatibility byte view are published consistently before lookup or serialization. High-color maps its source plane before destination conversion and cell construction.

| Related policy | Boundary |
| --- | --- |
| `-X` / `-W` | Continue to select clustering and application geometry. Mapping does not change either choice. |
| `-0` | Limits diffusion compensation. Error-adjusted candidates and accumulated error are not gamut-mapped. |
| `-_` | Owns wire-lattice snap. Mapping alone never rounds a palette to RGB101. |
| `-a` | Repairs palette reachability. Its representatives must also fit the selected output gamut. |
| `-M` | Exports using the existing sRGB interchange interpretation. Mapping can change the constructed palette, but does not reinterpret a legacy mapfile as device RGB. |
| `sixel2png -T` | Interprets decoded numbers. It does not apply a second gamut mapping. |

The mapper uses the selected CPU float palette/high-color paths and their existing admission rules. Effective precision is float even under an explicit byte preference. `auto` and `clip` do not introduce a mapper or force float. RGB101 rounding remains at the output boundary unless an explicit snap policy requests earlier projection.

## Evaluation boundary

The algorithm's published reference is not evidence that it improves every photograph. Default selection remains `clip` until photograph, wide-gamut, gradient and saturated highlight/shadow comparisons establish an acceptable quality/performance tradeoff. A future compression policy that changes near-boundary in-gamut colors is a different contract.

Use the [quality measurement policy](../../quality/measurement-policy.md) and [numerical assertion rules](../../testing/guide.md#intermediate-floating-point-error). Mapping error relative to the original and quantization error relative to the mapped reference are distinct observations. Inspect hue changes, flattened gradients and localized color errors in addition to aggregate metrics. Mapping may inspect every pixel; the chroma search or boundary lookup is restricted to out-of-gamut colors.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| GM-01 | In-gamut sRGB is retained exactly. | [tests/processing/gamut/0001_srgb_identity.t](../../../tests/processing/gamut/0001_srgb_identity.t) ([tests/processing/gamut/0001_srgb_identity.c](../../../tests/processing/gamut/0001_srgb_identity.c)) |
| GM-02 | Canonical sRGB values outside its cube remain unchanged inside P3. | [tests/processing/gamut/0002_p3_extended_identity.t](../../../tests/processing/gamut/0002_p3_extended_identity.t) ([tests/processing/gamut/0002_p3_extended_identity.c](../../../tests/processing/gamut/0002_p3_extended_identity.c)) |
| GM-03 | sRGB results agree with independent local-MINDE values. | [tests/processing/gamut/0003_srgb_reference.t](../../../tests/processing/gamut/0003_srgb_reference.t) ([tests/processing/gamut/0003_srgb_reference.c](../../../tests/processing/gamut/0003_srgb_reference.c)) |
| GM-04 | P3 results use its own gamut boundary. | [tests/processing/gamut/0004_p3_reference.t](../../../tests/processing/gamut/0004_p3_reference.t) ([tests/processing/gamut/0004_p3_reference.c](../../../tests/processing/gamut/0004_p3_reference.c)) |
| GM-05 | Adobe RGB results use its own gamut boundary. | [tests/processing/gamut/0005_adobe_reference.t](../../../tests/processing/gamut/0005_adobe_reference.t) ([tests/processing/gamut/0005_adobe_reference.c](../../../tests/processing/gamut/0005_adobe_reference.c)) |
| GM-06 | DCI-P3 results include white adaptation. | [tests/processing/gamut/0006_dci_reference.t](../../../tests/processing/gamut/0006_dci_reference.t) ([tests/processing/gamut/0006_dci_reference.c](../../../tests/processing/gamut/0006_dci_reference.c)) |
| GM-07 | Nonfinite input fails without modifying the output triple. | [tests/processing/gamut/0007_nonfinite_atomic.t](../../../tests/processing/gamut/0007_nonfinite_atomic.t) ([tests/processing/gamut/0007_nonfinite_atomic.c](../../../tests/processing/gamut/0007_nonfinite_atomic.c)) |
| GM-08 | Invalid replacement preserves the selected policy/definition. | [tests/processing/gamut/0008_policy_atomicity.t](../../../tests/processing/gamut/0008_policy_atomicity.t) ([tests/processing/gamut/0008_policy_atomicity.c](../../../tests/processing/gamut/0008_policy_atomicity.c)) |
| GM-09 | Full-frame construction averages mapped samples, preserves borrowed input and keeps unsnapped float representatives. | [tests/processing/gamut/0009_full_frame_samples.t](../../../tests/processing/gamut/0009_full_frame_samples.t) ([tests/processing/gamut/0009_full_frame_samples.c](../../../tests/processing/gamut/0009_full_frame_samples.c)) |
| GM-10 | Adaptive construction maps its loaded-frame samples. | [tests/processing/gamut/0010_adaptive_samples.t](../../../tests/processing/gamut/0010_adaptive_samples.t) ([tests/processing/gamut/0010_adaptive_samples.c](../../../tests/processing/gamut/0010_adaptive_samples.c)) |
| GM-11 | Application in Oklab retains the same mapped colors. | [tests/processing/gamut/0011_oklab_workspace.t](../../../tests/processing/gamut/0011_oklab_workspace.t) ([tests/processing/gamut/0011_oklab_workspace.c](../../../tests/processing/gamut/0011_oklab_workspace.c)) |
| GM-12 | High-color output uses the mapped destination values. | [tests/processing/gamut/0012_highcolor.t](../../../tests/processing/gamut/0012_highcolor.t) ([tests/processing/gamut/0012_highcolor.c](../../../tests/processing/gamut/0012_highcolor.c)) |
| GM-13 | Matrix/TRC ICC mapping agrees with the independent P3 reference. | [tests/processing/gamut/0013_matrix_icc.t](../../../tests/processing/gamut/0013_matrix_icc.t) ([tests/processing/gamut/0013_matrix_icc.c](../../../tests/processing/gamut/0013_matrix_icc.c)) |
| GM-14 | Unsupported LUT mapping fails without replacing the old configuration. | [tests/processing/gamut/0014_lut_rejected.t](../../../tests/processing/gamut/0014_lut_rejected.t) ([tests/processing/gamut/0014_lut_rejected.c](../../../tests/processing/gamut/0014_lut_rejected.c)) |
| GM-15 | Separate encoders retain independent destinations and lifetimes. | [tests/processing/gamut/0015_encoder_isolation.t](../../../tests/processing/gamut/0015_encoder_isolation.t) ([tests/processing/gamut/0015_encoder_isolation.c](../../../tests/processing/gamut/0015_encoder_isolation.c)) |
| GM-16 | Environment defaults apply without `-U`; explicit suboptions override them. | [tests/processing/gamut/0016_environment_precedence.t](../../../tests/processing/gamut/0016_environment_precedence.t) ([tests/processing/gamut/0016_environment_precedence.c](../../../tests/processing/gamut/0016_environment_precedence.c)) |
| GM-17 | Byte-view reconciliation preserves the continuous mapped float palette. | [tests/processing/gamut/0017_palette_continuous.t](../../../tests/processing/gamut/0017_palette_continuous.t) ([tests/processing/gamut/0017_palette_continuous.c](../../../tests/processing/gamut/0017_palette_continuous.c)) |
| GM-18 | Changing policy invalidates the previous delta image. | [tests/processing/gamut/0018_policy_cache_reset.t](../../../tests/processing/gamut/0018_policy_cache_reset.t) ([tests/processing/gamut/0018_policy_cache_reset.c](../../../tests/processing/gamut/0018_policy_cache_reset.c)) |
| GM-19 | Negotiated sRGB output retains explicit gamut tuning, using an isolated fake-terminal PTY. | [tests/processing/gamut/0019_auto_tty.t](../../../tests/processing/gamut/0019_auto_tty.t) |
| GM-20 | sRGB EdgeSeeker agrees with independently derived values, including hue seams and blue folds. | [tests/processing/gamut/0020_edge_srgb_reference.t](../../../tests/processing/gamut/0020_edge_srgb_reference.t) ([tests/processing/gamut/0020_edge_srgb_reference.c](../../../tests/processing/gamut/0020_edge_srgb_reference.c)) |
| GM-21 | Display P3 EdgeSeeker uses its own boundary. | [tests/processing/gamut/0021_edge_p3_reference.t](../../../tests/processing/gamut/0021_edge_p3_reference.t) ([tests/processing/gamut/0021_edge_p3_reference.c](../../../tests/processing/gamut/0021_edge_p3_reference.c)) |
| GM-22 | Adobe RGB EdgeSeeker uses its own boundary and transfer. | [tests/processing/gamut/0022_edge_adobe_reference.t](../../../tests/processing/gamut/0022_edge_adobe_reference.t) ([tests/processing/gamut/0022_edge_adobe_reference.c](../../../tests/processing/gamut/0022_edge_adobe_reference.c)) |
| GM-23 | DCI-P3 EdgeSeeker includes white adaptation and its transfer. | [tests/processing/gamut/0023_edge_dci_reference.t](../../../tests/processing/gamut/0023_edge_dci_reference.t) ([tests/processing/gamut/0023_edge_dci_reference.c](../../../tests/processing/gamut/0023_edge_dci_reference.c)) |
| GM-24 | EdgeSeeker leaves in-gamut P3 extended RGB, neutrals and endpoints unchanged. | [tests/processing/gamut/0024_edge_identity.t](../../../tests/processing/gamut/0024_edge_identity.t) ([tests/processing/gamut/0024_edge_identity.c](../../../tests/processing/gamut/0024_edge_identity.c)) |
| GM-25 | EdgeSeeker maps out-of-range SDR lightness to destination black or white. | [tests/processing/gamut/0025_edge_endpoints.t](../../../tests/processing/gamut/0025_edge_endpoints.t) ([tests/processing/gamut/0025_edge_endpoints.c](../../../tests/processing/gamut/0025_edge_endpoints.c)) |
| GM-26 | Nonfinite EdgeSeeker input fails atomically. | [tests/processing/gamut/0026_edge_nonfinite_atomic.t](../../../tests/processing/gamut/0026_edge_nonfinite_atomic.t) ([tests/processing/gamut/0026_edge_nonfinite_atomic.c](../../../tests/processing/gamut/0026_edge_nonfinite_atomic.c)) |
| GM-27 | Matrix/TRC P3 ICC EdgeSeeker agrees with the independent P3 reference. | [tests/processing/gamut/0027_edge_matrix_icc.t](../../../tests/processing/gamut/0027_edge_matrix_icc.t) ([tests/processing/gamut/0027_edge_matrix_icc.c](../../../tests/processing/gamut/0027_edge_matrix_icc.c)) |
| GM-28 | An unrepresentable ROMM boundary fails explicitly. | [tests/processing/gamut/0028_edge_invalid_boundary.t](../../../tests/processing/gamut/0028_edge_invalid_boundary.t) ([tests/processing/gamut/0028_edge_invalid_boundary.c](../../../tests/processing/gamut/0028_edge_invalid_boundary.c)) |
| GM-29 | An all-hue lightness sweep is bounded and idempotent after mapping. | [tests/processing/gamut/0029_edge_bounds.t](../../../tests/processing/gamut/0029_edge_bounds.t) ([tests/processing/gamut/0029_edge_bounds.c](../../../tests/processing/gamut/0029_edge_bounds.c)) |
| GM-30 | EdgeSeeker maps full-frame samples before averaging and preserves borrowed input. | [tests/processing/gamut/0030_edge_full_frame_samples.t](../../../tests/processing/gamut/0030_edge_full_frame_samples.t) ([tests/processing/gamut/0030_edge_full_frame_samples.c](../../../tests/processing/gamut/0030_edge_full_frame_samples.c)) |
| GM-31 | EdgeSeeker maps adaptive loaded-frame samples before averaging. | [tests/processing/gamut/0031_edge_adaptive_samples.t](../../../tests/processing/gamut/0031_edge_adaptive_samples.t) ([tests/processing/gamut/0031_edge_adaptive_samples.c](../../../tests/processing/gamut/0031_edge_adaptive_samples.c)) |
| GM-32 | EdgeSeeker palette application in Oklab retains the mapped colors. | [tests/processing/gamut/0032_edge_oklab_workspace.t](../../../tests/processing/gamut/0032_edge_oklab_workspace.t) ([tests/processing/gamut/0032_edge_oklab_workspace.c](../../../tests/processing/gamut/0032_edge_oklab_workspace.c)) |
| GM-33 | High-color output encodes the EdgeSeeker-mapped source. | [tests/processing/gamut/0033_edge_highcolor.t](../../../tests/processing/gamut/0033_edge_highcolor.t) ([tests/processing/gamut/0033_edge_highcolor.c](../../../tests/processing/gamut/0033_edge_highcolor.c)) |
| GM-34 | LUT ICC rejection preserves the previous EdgeSeeker configuration. | [tests/processing/gamut/0034_edge_lut_rejected.t](../../../tests/processing/gamut/0034_edge_lut_rejected.t) ([tests/processing/gamut/0034_edge_lut_rejected.c](../../../tests/processing/gamut/0034_edge_lut_rejected.c)) |
| GM-35 | EdgeSeeker environment defaults force float; explicit clip removes the mapper. | [tests/processing/gamut/0035_edge_environment_precedence.t](../../../tests/processing/gamut/0035_edge_environment_precedence.t) ([tests/processing/gamut/0035_edge_environment_precedence.c](../../../tests/processing/gamut/0035_edge_environment_precedence.c)) |
| GM-36 | Separate EdgeSeeker encoders retain independent table lifetimes. | [tests/processing/gamut/0036_edge_encoder_isolation.t](../../../tests/processing/gamut/0036_edge_encoder_isolation.t) ([tests/processing/gamut/0036_edge_encoder_isolation.c](../../../tests/processing/gamut/0036_edge_encoder_isolation.c)) |
| GM-37 | Negotiated output retains EdgeSeeker tuning through an isolated fake-terminal PTY. | [tests/processing/gamut/0037_edge_auto_tty.t](../../../tests/processing/gamut/0037_edge_auto_tty.t) |
| GM-38 | Linear output uses the linear destination transfer when constructing its boundary. | [tests/processing/gamut/0038_edge_linear_reference.t](../../../tests/processing/gamut/0038_edge_linear_reference.t) ([tests/processing/gamut/0038_edge_linear_reference.c](../../../tests/processing/gamut/0038_edge_linear_reference.c)) |

Reference comparisons allow absolute error `3e-5` per canonical linear-sRGB channel for independently rounded matrices and binary-search endpoints. The matrix/TRC ICC case allows `3e-4` for its additional 15.16 colorant quantization; the mapped-centroid checks allow `4e-5` after float storage and workspace conversion. These budgets remain separate from the membership and local-MINDE search tolerances above.

The scalar reference generator and stored vectors are [gamut-reference.py](../../../tests/data/color-numeric/gamut-reference.py) and [gamut-reference.h](../../../tests/data/color-numeric/gamut-reference.h). They derive RGB matrices from chromaticities and independently invert Oklab matrices without calling the production implementation. [edge-reference.py](../../../tests/data/color-numeric/edge-reference.py) and [edge-reference.h](../../../tests/data/color-numeric/edge-reference.h) independently fit and intersect circles using their centers to check the production curvature-scaled calculation; they use the same `3e-5` channel budget and `3e-4` ICC budget. The EdgeSeeker vectors include both boundary branches, the hue seam, and blue-region folds.

### Quality regression tests

| ID | Contract | Owning test |
| --- | --- | --- |
| GMQ-01 | Compact and environment suboptions bind the same mapper and emit identical images, with separate sRGB image sanity. | [tests/cli/options/regression/0173_output_gamut_image_regression.t](../../../tests/cli/options/regression/0173_output_gamut_image_regression.t) |
