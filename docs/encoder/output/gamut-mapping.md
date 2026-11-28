# Output gamut mapping

For a worked color example, diagrams and comparisons of the actual mapper and encoded images, start with [Gamut mapping: choosing a color the output can reproduce](../../concepts/gamut-mapping/README.md). This reference owns the CLI, numerical and implementation contracts.

`img2sixel -U SPACE:gamut=POLICY` selects how colors outside the output RGB gamut are handled. The destination comes from `-U`, independently of the source frame's coordinate space. Negative or greater-than-one canonical sRGB coordinates can be valid colors inside a Display P3 or Adobe RGB destination and are retained.

| Policy | Behavior |
| --- | --- |
| `auto` | Default; resolves to `clip`. |
| `clip` | Retains the compatibility pipeline: no source mapping, and destination channels are bounded at output. |
| `ray-trace` | Maps colors with up to four linear RGB ray intersections and Oklab lightness/hue corrections. Already representable colors are unchanged. |
| `edge-seeker` | Maps colors using a precomputed hue boundary table and constant-lightness chroma reduction in Oklab. Already representable colors are unchanged. |
| `local-minde` | Maps source colors and the application palette using binary search with local MINDE in Oklab. Already representable colors are unchanged, within the numerical boundary tolerance below. |

`gamut` has compact key `G` and environment default `SIXEL_OUTPUT_COLORSPACE_GAMUT`. An explicit suboption overrides that environment. Repeating `-U` replaces its base and tuning; an invalid replacement preserves the previous configuration. The gamut environment also applies to default sRGB output when `-U` is omitted. CLI-only `-U auto` retains common gamut tuning when terminal negotiation resolves the destination; its existing timeout, output-TTY and policy-restoration contracts remain at [output color space](output-colorspace.md).

```sh
img2sixel image.png -U display-p3:gamut=local-minde
img2sixel image.png -U srgb:Glocal-minde
img2sixel image.png -U display-p3:gamut=edge-seeker
img2sixel image.png -U display-p3:gamut=ray-trace
img2sixel image.png -U icc:path=/path/to/profile.icc:gamut=local-minde
img2sixel image.png -U auto:gamut=local-minde:timeout_ms=200
```

Three further suboptions choose where the shown result of the destination is used; [gamut aims](#gamut-aims-for-diffusion-and-clustering) owns them.

`local-minde`, `edge-seeker` and `ray-trace` identify specific algorithms, not space selectors. Explicit selection does not permit substitution of another algorithm. A future recommendation change belongs to `auto`. There is no `oklch` alias, because Oklch is a coordinate representation shared by distinct algorithms.

## Numeric contract

### Local MINDE

The reference is [CSS Color 4, 9 October 2026, binary search with local MINDE](https://www.w3.org/TR/2026/CRD-css-color-4-20261009/#binsearch). Chroma is searched along a constant-lightness, constant-hue ray in Oklab: multiply `a` and `b` by the same nonnegative scale. This avoids a polar-coordinate conversion and does not add an Oklch choice to `-X` or `-W`.

The local clipping tolerance is Delta E OK `0.02`; the chroma search termination is `0.0001`. The candidate is clipped to the destination gamut and evaluated back in Oklab, allowing small lightness and hue changes instead of requiring excessive chroma reduction. Lightness at or beyond the SDR endpoints maps to destination black or white. This is not HDR tone mapping, saturation enhancement, or a general ICC perceptual rendering intent.

### EdgeSeeker

The reference is [CSS Color 4, 9 October 2026, EdgeSeeker](https://www.w3.org/TR/2026/CRD-css-color-4-20261009/#GMA-EdgeSeeker). At construction, 400 encoded RGB samples per HSL hue region at lightness 50% and 75% describe each destination's cusp and upper boundary. The destination transfer curves are used when decoding these samples, including the identity transfer for `-U srgb-linear`. Hue folds are removed by replacing the preceding overshoot with a bridge, rather than discarding only backward-moving samples. The sorted table interpolates across the 0/360-degree seam.

For an out-of-gamut color, the mapper looks up the cusp and signed curvature at its Oklab hue. Below the cusp, maximum chroma follows a straight line from black; above it, a fitted circle connects the cusp to white. Chroma is reduced at fixed lightness and hue, followed by destination clipping to contain approximation error. The circle intersection uses a rationalized expression which remains stable as curvature tends to zero. This method does not use local MINDE's Delta E tolerance or iterative chroma search. It shares the SDR black/white endpoint behavior.

Construction rejects a boundary which cannot be represented by this model: invalid hue folds, nonfinite data, cusps outside lightness `(0,1)`, nonpositive cusp chroma, or absolute curvature at least one. Decoded device endpoints must be zero and one within `2e-7`. The model is intended for ordinary display RGB gamuts; it is not a general solution for imaginary primaries such as ROMM RGB. The supported named spaces and the matrix/TRC Display P3 ICC fixture are covered by numerical tests; arbitrary matrix/TRC profiles are admitted only if construction succeeds. A successful geometric check does not certify physical primaries or perceptual accuracy for an arbitrary profile.

The table has at most 2,402 entries, each containing four doubles (about 75 KiB per encoder), plus temporary construction storage of about 176 KiB. Construction is performed when selecting the policy/destination, not per frame. In-gamut pixels take the common identity path; out-of-gamut pixels require table lookup and a geometric evaluation. There is no per-pixel allocation or iterative gamut search. This describes computational structure, not a measured speed advantage on every workload.

### Ray Trace

The reference is [CSS Color 4, 9 October 2026, Ray Trace](https://www.w3.org/TR/2026/CRD-css-color-4-20261009/#GMA-RayTrace). For an out-of-gamut color, a ray starts at the neutral color of the same Oklab lightness and intersects the destination's linear RGB box. Up to four intersections are performed. Before each subsequent intersection, the current intersection is converted to Oklab and corrected to the original lightness and hue while retaining its current chroma. This approximates constant-lightness chroma reduction; final clipping contains numerical error. SDR lightness endpoints map to destination black or white.

The ray is unbounded, not limited to the segment between anchor and candidate. When a corrected candidate lies strictly inside the box, it advances the anchor after the intersection is found; it does not terminate the search. Parallel-axis handling and the strict-interior test use `1e-12` in double-precision linear destination coordinates. If a ray has no forward intersection, the last successful intersection (or the original candidate before any hit) is retained and finally clipped, as specified by the algorithm. This recovery is not substitution of another gamut policy.

Before tracing a corrected point, the implementation checks convergence against the last successful intersection: if every linear destination channel changes by at most `2e-7` times that channel's decoded range, it retains the last intersection and stops. This is a numerical safeguard added to the referenced pseudocode. The tolerance covers float storage and the rounded Oklab matrix pair near the boundary; it is not a perceptual chroma-compression threshold. Without it, a correction can step minutely behind an advanced anchor, reverse the ray, and be magnified into an opposite-face intersection. Rays which have not converged remain unbounded, and an ordinary inward correction still advances the anchor rather than ending the search.

The destination box uses decoded device endpoints, including non-unit ranges of supported matrix/TRC definitions. Unlike EdgeSeeker, Ray Trace constructs no hue table and imposes no single-valued hue-boundary model. All mapping state except the immutable destination matrices/ranges is local to the call. The iteration limit bounds per-color work but does not establish a measured performance or quality advantage.

### Shared destination rules

Gamut membership is evaluated in the destination's linear channel domain before inverse transfer curves can clamp it. Named definitions use their primaries, white adaptation and transfer; supported matrix/TRC ICCs use the matrix and channel ranges between their decoded device endpoints. A tolerance of `2e-7` per linear destination channel covers float storage and matrix boundary error. An in-gamut input is returned directly, without a conversion round trip. Intermediate arithmetic uses double, and invalid/nonfinite conversions fail rather than publishing nonfinite output.

ICC LUT profiles cannot use `local-minde`, `edge-seeker` or `ray-trace`: a bounded inverse LUT is not a gamut-membership oracle. The mapper requires separable, supported, invertible channel curves and an invertible RGB matrix. `clip` continues to admit the existing supported ICC output subset; it does not override a LUT's own rendering behavior. Explicit unsupported mapping never silently falls back to clipping.

## Pipeline and policy boundaries

The mapper belongs to the shared encoder library, not the loader. Its immutable object owns the destination matrices and endpoint ranges. Separate encoders have separate policy objects; palette workers only read them. Changing gamut policy invalidates incompatible palette and delta caches. No environment access or allocation occurs in per-color mapping.

With `local-minde`, `edge-seeker` or `ray-trace`, encoding owns a private float frame. A standalone `gamut` filter is placed after geometry and working-space preparation in the planner graph; it maps visible source pixels before lookup/diffusion. `diffusion=aim` omits this filter and applies the mapper to the [diffusion aim](#gamut-aims-for-diffusion-and-clustering) instead. Palette-construction samples are also mapped before clustering. The existing sampling policy still determines the sample source: full-frame uses the preprocessed frame, while adaptive-grid uses its loaded-frame samples. The mapper does not silently change that policy or move adaptive sampling after resizing. Omitted pixels are excluded from mapping.

Adjacent float colorspace and gamut filters can run as separate tile stages under the [encoder thread budget](../../threading/encoder.md#float-colorspace-transactions). The producer runs ordinary colorspace filters and a consumer runs ordinary gamut filters; neither their loops nor their numerical operations are fused. One-slot, small, byte, alpha-plane and incompatible-binding cases retain sequential execution. Mapping without a preceding conversion remains a standalone filter, including linear input with automatic linear working space. Both paths retain the same successful float bits and restore sequential failure precedence after draining any queued work.

Generated palettes are checked again after clustering, merge, cover and workspace conversion, because those operations can produce new out-of-gamut representatives. Fixed palettes are mapped in their application copy; the original palette definition remains separate. The continuous float palette and its compatibility byte view are published consistently before lookup or serialization. High-color maps its source plane before destination conversion and cell construction.

The [pipeline diagram](../../concepts/gamut-mapping/README.md#where-the-mapper-sits-in-the-implemented-pipeline) distinguishes the sampling and application branches. Mapping a triplet uses canonical linear RGB for destination membership and transient Oklab for replacement selection, then returns to the caller's coordinate space; it does not retag the frame as Oklab. In particular, the clustering copy is converted to `-X` before mapping, and the application frame completes its planned `-W` conversion before mapping. This is not a loader hook or an unconditional preprocessing pass before resize.

| Implementation entry | Responsibility |
| --- | --- |
| [`sixel_gamut_new`, `sixel_gamut_apply`, `sixel_gamut_free`](../../../src/gamut.c) | Construct the immutable destination/policy object; dispatch through its vtable; perform membership, replacement and cleanup. |
| [`sixel_filter_gamut_apply`](../../../src/filter-gamut.c) | Visit visible float pixels, preserve their coordinate tag and reject invalid float results. |
| [`sixel_encode_dag_node_preplan`](../../../src/encoder.c) | Append the independent mapping filter after geometry/colorspace preparation. |
| [`sixel_encoder_prepare_palette`](../../../src/encoder.c) | Map the clustering copy before quantization and recheck the generated application palette before output preparation. |
| [`sixel_palette_map_gamut`](../../../src/palette.c) | Update continuous palette entries and their compatibility byte view consistently, retaining indices and order. |
| [`sixel_palette_prepare_output_definition`](../../../src/palette.c) | Prepare destination encoding and wire values; this is separate from choosing a gamut replacement. |

| Related policy | Boundary |
| --- | --- |
| `-X` / `-W` | Continue to select clustering and application geometry. Mapping does not change either choice. |
| `-0` | Limits diffusion compensation. Error-adjusted candidates and accumulated error are not gamut-mapped. |
| `-_` | Owns wire-lattice snap. Mapping alone never rounds a palette to RGB101. |
| `-a` | Repairs palette reachability. Its representatives must also fit the selected output gamut. |
| `-M` | Exports using the existing sRGB interchange interpretation. Mapping can change the constructed palette, but does not reinterpret a legacy mapfile as device RGB. |
| `sixel2png -T` | Interprets decoded numbers. It does not apply a second gamut mapping. |

The mapper uses the selected CPU float palette/high-color paths and their existing admission rules. The mapping itself runs in float under every precision policy; `--precision-policy=8bit` rounds the mapped result to gamma bytes at palette application. `auto` and `clip` do not introduce a mapper or force float. RGB101 rounding remains at the output boundary unless an explicit snap policy requests earlier projection.

## Gamut aims for diffusion and clustering

The **gamut aim** of a color is what the receiver shows for it: the color itself when its `-U` codes lie inside `[0, 1]` within `2e-7`, otherwise `interpret(clamp01(encode(map(S))))`. `map` is the selected gamut mapper, or the identity under `auto` and `clip`, where the wire-range clamp bounds the color; `encode` and `interpret` are the transforms the palette owner uses for the same `-U`. The aim is computed in double precision and returned to the coordinates of the view it replaces. A representable color keeps its source bits, and wire quantization to RGB101 is not part of the aim. The [conceptual explanation](../../concepts/gamut-mapping/README.md#aiming-at-what-the-receiver-shows) shows why diffusion and clustering may want it.

| Suboption | Compact | Values | Effect |
| --- | --- | --- | --- |
| `diffusion` | `D` | `input` (default), `aim` | `aim` keeps the unmapped working view as lookup input, adds `aim - source` to each diffused residual, and gives out-of-gamut application palette entries their aims. |
| `samples` | `S` | `mapped` (default), `shown` | `shown` replaces each visible clustering sample by its aim instead of its mapper result. |
| `aim_method` | `M` | `exact` (default), `lut`, `auto` | Evaluates aims per pixel or through a verified 3D LUT; `auto` uses the LUT when visible pixels outnumber four times its nodes. |

`SIXEL_OUTPUT_COLORSPACE_DIFFUSION`, `SIXEL_OUTPUT_COLORSPACE_SAMPLES` and `SIXEL_OUTPUT_COLORSPACE_AIM_METHOD` supply defaults; explicit suboptions win, and repeating `-U` replaces them with its base. With the defaults, output is identical to omitting the suboptions. The encoder's dither contract trace reports them as `gamut_diffusion`, `gamut_samples`, `gamut_aim_method` and whether an aim offset plane was published, `gamut_aim_offset`.

```sh
img2sixel image.png -U srgb:diffusion=aim
img2sixel image.png -U display-p3:Daim:Sshown
img2sixel image.png -U srgb:gamut=ray-trace:diffusion=aim:aim_method=lut
```

### Diffusion contract

With `diffusion=aim`, the candidate passed to lookup is the working-view source plus accumulated error; the residual sent onward is that candidate minus the chosen palette color plus the offset `aim - source`. A component offset below `1e-6` in magnitude is round-off and is treated as zero, and an all-zero plane is dropped. The offset plane is computed once per frame, is read-only and is independent of band geometry; transparent pixels receive zero. When `-W` is an ICC profile, aims are computed in linear sRGB and expressed through the same profile as the source, so the profile's own A2B/B2A asymmetry does not enter the offset.

Generated and fixed application palettes are replaced by their aims after point C and before output preparation. Under `clip` a generated representative can lie outside the destination, and its float value is then not what the receiver shows; measuring the residual between a shown aim and an unshown entry would feed the clipped-away difference back as error. Representable entries keep their bits and wire values are unchanged. Explicit ICC output then scores registers through A2B as described in [output color space](output-colorspace.md), which already gives lookup and diffusion the shown color of every in-gamut register.

`diffusion=aim` requires CPU float32 error diffusion with FS, Atkinson, Jarvis--Judice--Ninke, Stucki, Burkes, a Sierra kernel or LSO2. `--precision-policy=8bit` with one of these kernels is an error, and so is `--gpu-policy=force`; `auto` GPU selection keeps the CPU path. Positional and no-dither methods have no residual. Byte gamma sRGB input with an sRGB `-U` is its own aim, so its byte path is kept. The `-0` clamp policy bounds the combined correction before lookup; the offset is added after lookup and is not itself limited.

### Clustering contract

With `samples=shown`, each visible clustering sample is replaced by its aim, expressed in `-X`, before palette construction. Under `auto` and `clip` this is the only point-A treatment and changes which colors the quantizer sees; under a mapper the aim of a sample differs from its mapped value only by the encode/interpret round trip. Byte RGBA alpha-key samples are converted through a temporary float copy. A fixed mapfile palette has no samples and is unchanged. Point C still checks the generated palette.

### LUT method and cost

The LUT covers the bounding box of the visible pixels in the coordinates of the view, with 33 points per axis. A cell is interpolated only when all eight corners and the center lie inside the `-U` gamut and the interpolated center agrees with the exact aim within Delta E OK `0.001`; every other cell, and therefore every gamut boundary, is evaluated exactly per pixel. Because representable colors are their own aims, the LUT acts as a verified membership cache. The center check samples the cell rather than bounding it; use `exact` when a transform may have structure finer than one cell.

| Item | `exact` | `lut` |
| --- | --- | --- |
| Per visible pixel | One `-U` encode; for an out-of-gamut color, a mapper call, a second encode and one interpret | One trilinear interpolation, or the exact path in rejected cells |
| Setup | None | `33^3` node and `32^3` center evaluations |
| Memory | One offset plane, 12 bytes per pixel for RGB float32 | The offset plane plus about 0.5 MB of nodes and cell flags |

Named and ICC destinations evaluate through XYZ and cost more per pixel than builtin ones. The offset plane exists only for one palette application.

### Limits

The aims do not reproduce mapping before lookup: lookup still reads the unmapped color. The quality effect depends on the image, palette size and gamut policy, and the [illustrated measurements](../../concepts/gamut-mapping/README.md#what-the-measurements-show) include cases where each suboption is worse than the default. Evaluate with the [measurement policy](../../quality/measurement-policy.md) rather than assuming a gain. High-color output does not consume aims.

| Implementation entry | Responsibility |
| --- | --- |
| [`src/target-color.c`](../../../src/target-color.c) | Owns the aim evaluator: encode/interpret, LUT build and verification, offsets and in-place aims. |
| [`sixel_dither_apply_palette`](../../../src/dither.c) | Decides whether an offset is needed, rejects byte or forced-GPU diffusion and publishes the frame offset plane. |
| [`sixel_palette_map_aim`](../../../src/palette.c) | Replaces application palette entries by their aims with a consistent byte view. |
| [`sixel_encoder_map_target_samples`](../../../src/encoder.c) | Replaces clustering samples by their aims, including byte alpha-key samples. |

## Evaluation boundary

The algorithm's published reference is not evidence that it improves every photograph. Default selection remains `clip` until photograph, wide-gamut, gradient and saturated highlight/shadow comparisons establish an acceptable quality/performance tradeoff. A future compression policy that changes near-boundary in-gamut colors is a different contract.

The [illustrated comparison](../../concepts/gamut-mapping/README.md#add-palette-quantization-and-sixel) separates continuous mapping from quantization and dithering. Reference-vector agreement and bounded RGB output alone do not establish local continuity: a wrong-face ray intersection can still return valid RGB numbers. The convergence regression therefore compares neighboring magenta-boundary inputs, including float workspace storage, against independently bisected constant-lightness/hue boundary values. Its `3e-5` absolute tolerance in canonical linear-sRGB channels covers the four-intersection approximation and matrix/storage rounding on that near-converged patch; it is not a global accuracy promise for every color or profile.

Use the [quality measurement policy](../../quality/measurement-policy.md) and [numerical assertion rules](../../testing/guide.md#intermediate-floating-point-error). Mapping error relative to the original and quantization error relative to the mapped reference are distinct observations. Inspect hue changes, flattened gradients and localized color errors in addition to aggregate metrics. Mapping may inspect every pixel; the chroma search, boundary lookup or ray iteration is restricted to out-of-gamut colors.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Contract | Owning test |
| --- | --- | --- |
| GM-01 | In-gamut sRGB is retained exactly. | [tests/color/gamut/0001_srgb_identity.t](../../../tests/color/gamut/0001_srgb_identity.t) ([tests/color/gamut/0001_srgb_identity.c](../../../tests/color/gamut/0001_srgb_identity.c)) |
| GM-02 | Canonical sRGB values outside its cube remain unchanged inside P3. | [tests/color/gamut/0002_p3_extended_identity.t](../../../tests/color/gamut/0002_p3_extended_identity.t) ([tests/color/gamut/0002_p3_extended_identity.c](../../../tests/color/gamut/0002_p3_extended_identity.c)) |
| GM-03 | sRGB results agree with independent local-MINDE values. | [tests/color/gamut/0003_srgb_reference.t](../../../tests/color/gamut/0003_srgb_reference.t) ([tests/color/gamut/0003_srgb_reference.c](../../../tests/color/gamut/0003_srgb_reference.c)) |
| GM-04 | P3 results use its own gamut boundary. | [tests/color/gamut/0004_p3_reference.t](../../../tests/color/gamut/0004_p3_reference.t) ([tests/color/gamut/0004_p3_reference.c](../../../tests/color/gamut/0004_p3_reference.c)) |
| GM-05 | Adobe RGB results use its own gamut boundary. | [tests/color/gamut/0005_adobe_reference.t](../../../tests/color/gamut/0005_adobe_reference.t) ([tests/color/gamut/0005_adobe_reference.c](../../../tests/color/gamut/0005_adobe_reference.c)) |
| GM-06 | DCI-P3 results include white adaptation. | [tests/color/gamut/0006_dci_reference.t](../../../tests/color/gamut/0006_dci_reference.t) ([tests/color/gamut/0006_dci_reference.c](../../../tests/color/gamut/0006_dci_reference.c)) |
| GM-07 | Nonfinite input fails without modifying the output triple. | [tests/color/gamut/0007_nonfinite_atomic.t](../../../tests/color/gamut/0007_nonfinite_atomic.t) ([tests/color/gamut/0007_nonfinite_atomic.c](../../../tests/color/gamut/0007_nonfinite_atomic.c)) |
| GM-08 | Invalid replacement preserves the selected policy/definition. | [tests/color/gamut/0008_policy_atomicity.t](../../../tests/color/gamut/0008_policy_atomicity.t) ([tests/color/gamut/0008_policy_atomicity.c](../../../tests/color/gamut/0008_policy_atomicity.c)) |
| GM-09 | Full-frame construction averages mapped samples, preserves borrowed input and keeps unsnapped float representatives. | [tests/color/gamut/0009_full_frame_samples.t](../../../tests/color/gamut/0009_full_frame_samples.t) ([tests/color/gamut/0009_full_frame_samples.c](../../../tests/color/gamut/0009_full_frame_samples.c)) |
| GM-10 | Adaptive construction maps its loaded-frame samples. | [tests/color/gamut/0010_adaptive_samples.t](../../../tests/color/gamut/0010_adaptive_samples.t) ([tests/color/gamut/0010_adaptive_samples.c](../../../tests/color/gamut/0010_adaptive_samples.c)) |
| GM-11 | Application in Oklab retains the same mapped colors. | [tests/color/gamut/0011_oklab_workspace.t](../../../tests/color/gamut/0011_oklab_workspace.t) ([tests/color/gamut/0011_oklab_workspace.c](../../../tests/color/gamut/0011_oklab_workspace.c)) |
| GM-12 | High-color output uses the mapped destination values. | [tests/color/gamut/0012_highcolor.t](../../../tests/color/gamut/0012_highcolor.t) ([tests/color/gamut/0012_highcolor.c](../../../tests/color/gamut/0012_highcolor.c)) |
| GM-13 | Matrix/TRC ICC mapping agrees with the independent P3 reference. | [tests/color/gamut/0013_matrix_icc.t](../../../tests/color/gamut/0013_matrix_icc.t) ([tests/color/gamut/0013_matrix_icc.c](../../../tests/color/gamut/0013_matrix_icc.c)) |
| GM-14 | Unsupported LUT mapping fails without replacing the old configuration. | [tests/color/gamut/0014_lut_rejected.t](../../../tests/color/gamut/0014_lut_rejected.t) ([tests/color/gamut/0014_lut_rejected.c](../../../tests/color/gamut/0014_lut_rejected.c)) |
| GM-15 | Separate encoders retain independent destinations and lifetimes. | [tests/color/gamut/0015_encoder_isolation.t](../../../tests/color/gamut/0015_encoder_isolation.t) ([tests/color/gamut/0015_encoder_isolation.c](../../../tests/color/gamut/0015_encoder_isolation.c)) |
| GM-16 | Environment defaults apply without `-U`; explicit suboptions override them. | [tests/color/gamut/0016_environment_precedence.t](../../../tests/color/gamut/0016_environment_precedence.t) ([tests/color/gamut/0016_environment_precedence.c](../../../tests/color/gamut/0016_environment_precedence.c)) |
| GM-17 | Byte-view reconciliation preserves the continuous mapped float palette. | [tests/color/gamut/0017_palette_continuous.t](../../../tests/color/gamut/0017_palette_continuous.t) ([tests/color/gamut/0017_palette_continuous.c](../../../tests/color/gamut/0017_palette_continuous.c)) |
| GM-18 | Changing policy invalidates the previous delta image. | [tests/color/gamut/0018_policy_cache_reset.t](../../../tests/color/gamut/0018_policy_cache_reset.t) ([tests/color/gamut/0018_policy_cache_reset.c](../../../tests/color/gamut/0018_policy_cache_reset.c)) |
| GM-19 | Negotiated sRGB output retains explicit gamut tuning, using an isolated fake-terminal PTY. | [tests/color/gamut/0019_auto_tty.t](../../../tests/color/gamut/0019_auto_tty.t) |
| GM-20 | sRGB EdgeSeeker agrees with independently derived values, including hue seams and blue folds. | [tests/color/gamut/0020_edge_srgb_reference.t](../../../tests/color/gamut/0020_edge_srgb_reference.t) ([tests/color/gamut/0020_edge_srgb_reference.c](../../../tests/color/gamut/0020_edge_srgb_reference.c)) |
| GM-21 | Display P3 EdgeSeeker uses its own boundary. | [tests/color/gamut/0021_edge_p3_reference.t](../../../tests/color/gamut/0021_edge_p3_reference.t) ([tests/color/gamut/0021_edge_p3_reference.c](../../../tests/color/gamut/0021_edge_p3_reference.c)) |
| GM-22 | Adobe RGB EdgeSeeker uses its own boundary and transfer. | [tests/color/gamut/0022_edge_adobe_reference.t](../../../tests/color/gamut/0022_edge_adobe_reference.t) ([tests/color/gamut/0022_edge_adobe_reference.c](../../../tests/color/gamut/0022_edge_adobe_reference.c)) |
| GM-23 | DCI-P3 EdgeSeeker includes white adaptation and its transfer. | [tests/color/gamut/0023_edge_dci_reference.t](../../../tests/color/gamut/0023_edge_dci_reference.t) ([tests/color/gamut/0023_edge_dci_reference.c](../../../tests/color/gamut/0023_edge_dci_reference.c)) |
| GM-24 | EdgeSeeker leaves in-gamut P3 extended RGB, neutrals and endpoints unchanged. | [tests/color/gamut/0024_edge_identity.t](../../../tests/color/gamut/0024_edge_identity.t) ([tests/color/gamut/0024_edge_identity.c](../../../tests/color/gamut/0024_edge_identity.c)) |
| GM-25 | EdgeSeeker maps out-of-range SDR lightness to destination black or white. | [tests/color/gamut/0025_edge_endpoints.t](../../../tests/color/gamut/0025_edge_endpoints.t) ([tests/color/gamut/0025_edge_endpoints.c](../../../tests/color/gamut/0025_edge_endpoints.c)) |
| GM-26 | Nonfinite EdgeSeeker input fails atomically. | [tests/color/gamut/0026_edge_nonfinite_atomic.t](../../../tests/color/gamut/0026_edge_nonfinite_atomic.t) ([tests/color/gamut/0026_edge_nonfinite_atomic.c](../../../tests/color/gamut/0026_edge_nonfinite_atomic.c)) |
| GM-27 | Matrix/TRC P3 ICC EdgeSeeker agrees with the independent P3 reference. | [tests/color/gamut/0027_edge_matrix_icc.t](../../../tests/color/gamut/0027_edge_matrix_icc.t) ([tests/color/gamut/0027_edge_matrix_icc.c](../../../tests/color/gamut/0027_edge_matrix_icc.c)) |
| GM-28 | An unrepresentable ROMM boundary fails explicitly. | [tests/color/gamut/0028_edge_invalid_boundary.t](../../../tests/color/gamut/0028_edge_invalid_boundary.t) ([tests/color/gamut/0028_edge_invalid_boundary.c](../../../tests/color/gamut/0028_edge_invalid_boundary.c)) |
| GM-29 | An all-hue lightness sweep is bounded and idempotent after mapping. | [tests/color/gamut/0029_edge_bounds.t](../../../tests/color/gamut/0029_edge_bounds.t) ([tests/color/gamut/0029_edge_bounds.c](../../../tests/color/gamut/0029_edge_bounds.c)) |
| GM-30 | EdgeSeeker maps full-frame samples before averaging and preserves borrowed input. | [tests/color/gamut/0030_edge_full_frame_samples.t](../../../tests/color/gamut/0030_edge_full_frame_samples.t) ([tests/color/gamut/0030_edge_full_frame_samples.c](../../../tests/color/gamut/0030_edge_full_frame_samples.c)) |
| GM-31 | EdgeSeeker maps adaptive loaded-frame samples before averaging. | [tests/color/gamut/0031_edge_adaptive_samples.t](../../../tests/color/gamut/0031_edge_adaptive_samples.t) ([tests/color/gamut/0031_edge_adaptive_samples.c](../../../tests/color/gamut/0031_edge_adaptive_samples.c)) |
| GM-32 | EdgeSeeker palette application in Oklab retains the mapped colors. | [tests/color/gamut/0032_edge_oklab_workspace.t](../../../tests/color/gamut/0032_edge_oklab_workspace.t) ([tests/color/gamut/0032_edge_oklab_workspace.c](../../../tests/color/gamut/0032_edge_oklab_workspace.c)) |
| GM-33 | High-color output encodes the EdgeSeeker-mapped source. | [tests/color/gamut/0033_edge_highcolor.t](../../../tests/color/gamut/0033_edge_highcolor.t) ([tests/color/gamut/0033_edge_highcolor.c](../../../tests/color/gamut/0033_edge_highcolor.c)) |
| GM-34 | LUT ICC rejection preserves the previous EdgeSeeker configuration. | [tests/color/gamut/0034_edge_lut_rejected.t](../../../tests/color/gamut/0034_edge_lut_rejected.t) ([tests/color/gamut/0034_edge_lut_rejected.c](../../../tests/color/gamut/0034_edge_lut_rejected.c)) |
| GM-35 | EdgeSeeker environment defaults force float; explicit clip removes the mapper. | [tests/color/gamut/0035_edge_environment_precedence.t](../../../tests/color/gamut/0035_edge_environment_precedence.t) ([tests/color/gamut/0035_edge_environment_precedence.c](../../../tests/color/gamut/0035_edge_environment_precedence.c)) |
| GM-36 | Separate EdgeSeeker encoders retain independent table lifetimes. | [tests/color/gamut/0036_edge_encoder_isolation.t](../../../tests/color/gamut/0036_edge_encoder_isolation.t) ([tests/color/gamut/0036_edge_encoder_isolation.c](../../../tests/color/gamut/0036_edge_encoder_isolation.c)) |
| GM-37 | Negotiated output retains EdgeSeeker tuning through an isolated fake-terminal PTY. | [tests/color/gamut/0037_edge_auto_tty.t](../../../tests/color/gamut/0037_edge_auto_tty.t) |
| GM-38 | Linear output uses the linear destination transfer when constructing its boundary. | [tests/color/gamut/0038_edge_linear_reference.t](../../../tests/color/gamut/0038_edge_linear_reference.t) ([tests/color/gamut/0038_edge_linear_reference.c](../../../tests/color/gamut/0038_edge_linear_reference.c)) |
| GM-39 | Ray Trace sRGB output agrees with independent six-face intersection values. | [tests/color/gamut/0039_ray_srgb_reference.t](../../../tests/color/gamut/0039_ray_srgb_reference.t) ([tests/color/gamut/0039_ray_srgb_reference.c](../../../tests/color/gamut/0039_ray_srgb_reference.c)) |
| GM-40 | Ray Trace uses the P3 gamut boundary. | [tests/color/gamut/0040_ray_p3_reference.t](../../../tests/color/gamut/0040_ray_p3_reference.t) ([tests/color/gamut/0040_ray_p3_reference.c](../../../tests/color/gamut/0040_ray_p3_reference.c)) |
| GM-41 | Ray Trace uses the Adobe RGB gamut boundary. | [tests/color/gamut/0041_ray_adobe_reference.t](../../../tests/color/gamut/0041_ray_adobe_reference.t) ([tests/color/gamut/0041_ray_adobe_reference.c](../../../tests/color/gamut/0041_ray_adobe_reference.c)) |
| GM-42 | Ray Trace includes DCI-P3 white adaptation. | [tests/color/gamut/0042_ray_dci_reference.t](../../../tests/color/gamut/0042_ray_dci_reference.t) ([tests/color/gamut/0042_ray_dci_reference.c](../../../tests/color/gamut/0042_ray_dci_reference.c)) |
| GM-43 | Ray Trace retains in-gamut colors, including extended canonical P3 values, exactly. | [tests/color/gamut/0043_ray_identity.t](../../../tests/color/gamut/0043_ray_identity.t) ([tests/color/gamut/0043_ray_identity.c](../../../tests/color/gamut/0043_ray_identity.c)) |
| GM-44 | Ray Trace maps out-of-range SDR lightness to black or white. | [tests/color/gamut/0044_ray_endpoints.t](../../../tests/color/gamut/0044_ray_endpoints.t) ([tests/color/gamut/0044_ray_endpoints.c](../../../tests/color/gamut/0044_ray_endpoints.c)) |
| GM-45 | Ray Trace rejects nonfinite input without modifying output. | [tests/color/gamut/0045_ray_nonfinite_atomic.t](../../../tests/color/gamut/0045_ray_nonfinite_atomic.t) ([tests/color/gamut/0045_ray_nonfinite_atomic.c](../../../tests/color/gamut/0045_ray_nonfinite_atomic.c)) |
| GM-46 | Ray Trace matrix/TRC ICC values agree with the independent P3 reference. | [tests/color/gamut/0046_ray_matrix_icc.t](../../../tests/color/gamut/0046_ray_matrix_icc.t) ([tests/color/gamut/0046_ray_matrix_icc.c](../../../tests/color/gamut/0046_ray_matrix_icc.c)) |
| GM-47 | Ray Trace remains bounded and idempotent across a hue/lightness sweep. | [tests/color/gamut/0047_ray_bounds.t](../../../tests/color/gamut/0047_ray_bounds.t) ([tests/color/gamut/0047_ray_bounds.c](../../../tests/color/gamut/0047_ray_bounds.c)) |
| GM-48 | Ray Trace maps full-frame samples before clustering and preserves borrowed input. | [tests/color/gamut/0048_ray_full_frame_samples.t](../../../tests/color/gamut/0048_ray_full_frame_samples.t) ([tests/color/gamut/0048_ray_full_frame_samples.c](../../../tests/color/gamut/0048_ray_full_frame_samples.c)) |
| GM-49 | Ray Trace maps adaptive samples before clustering. | [tests/color/gamut/0049_ray_adaptive_samples.t](../../../tests/color/gamut/0049_ray_adaptive_samples.t) ([tests/color/gamut/0049_ray_adaptive_samples.c](../../../tests/color/gamut/0049_ray_adaptive_samples.c)) |
| GM-50 | Ray Trace retains mapped colors in the Oklab workspace. | [tests/color/gamut/0050_ray_oklab_workspace.t](../../../tests/color/gamut/0050_ray_oklab_workspace.t) ([tests/color/gamut/0050_ray_oklab_workspace.c](../../../tests/color/gamut/0050_ray_oklab_workspace.c)) |
| GM-51 | Ray Trace high-color output matches independently derived RGB101 bins. | [tests/color/gamut/0051_ray_highcolor.t](../../../tests/color/gamut/0051_ray_highcolor.t) ([tests/color/gamut/0051_ray_highcolor.c](../../../tests/color/gamut/0051_ray_highcolor.c)) |
| GM-52 | Ray Trace rejects LUT destinations without replacing existing configuration. | [tests/color/gamut/0052_ray_lut_rejected.t](../../../tests/color/gamut/0052_ray_lut_rejected.t) ([tests/color/gamut/0052_ray_lut_rejected.c](../../../tests/color/gamut/0052_ray_lut_rejected.c)) |
| GM-53 | The Ray Trace environment default applies without `-U`; explicit tuning overrides it. | [tests/color/gamut/0053_ray_environment_precedence.t](../../../tests/color/gamut/0053_ray_environment_precedence.t) ([tests/color/gamut/0053_ray_environment_precedence.c](../../../tests/color/gamut/0053_ray_environment_precedence.c)) |
| GM-54 | Separate Ray Trace encoders retain independent destination lifetimes. | [tests/color/gamut/0054_ray_encoder_isolation.t](../../../tests/color/gamut/0054_ray_encoder_isolation.t) ([tests/color/gamut/0054_ray_encoder_isolation.c](../../../tests/color/gamut/0054_ray_encoder_isolation.c)) |
| GM-55 | Negotiated output retains Ray Trace tuning through an isolated fake-terminal PTY. | [tests/color/gamut/0055_ray_auto_tty.t](../../../tests/color/gamut/0055_ray_auto_tty.t) |
| GM-56 | Ray-box geometry handles parallel, zero, outside and unbounded rays; a miss retains and clips the last candidate, including non-unit boxes, while matrix overflow fails atomically. | [tests/color/gamut/0056_ray_geometry.t](../../../tests/color/gamut/0056_ray_geometry.t) ([tests/color/gamut/0056_ray_geometry.c](../../../tests/color/gamut/0056_ray_geometry.c)) |
| GM-57 | Switching from EdgeSeeker to Ray Trace invalidates accumulated output. | [tests/color/gamut/0057_ray_cache_reset.t](../../../tests/color/gamut/0057_ray_cache_reset.t) ([tests/color/gamut/0057_ray_cache_reset.c](../../../tests/color/gamut/0057_ray_cache_reset.c)) |
| GM-58 | Linear output uses the same Ray Trace linear gamut geometry as sRGB. | [tests/color/gamut/0058_ray_linear_reference.t](../../../tests/color/gamut/0058_ray_linear_reference.t) ([tests/color/gamut/0058_ray_linear_reference.c](../../../tests/color/gamut/0058_ray_linear_reference.c)) |
| GM-59 | Near-converged boundary colors retain the same hue-side intersection through linear, gamma and Oklab float workspaces, within the scoped independent-boundary error budget. | [tests/color/gamut/0059_ray_convergence.t](../../../tests/color/gamut/0059_ray_convergence.t) ([tests/color/gamut/0059_ray_convergence.c](../../../tests/color/gamut/0059_ray_convergence.c)) |
| GM-60 | The standalone gamut filter retains hidden nonfinite storage and source coordinates while matching the bulk mapper. | [tests/color/gamut/0062_filter_standalone.t](../../../tests/color/gamut/0062_filter_standalone.t) ([tests/color/gamut/0062_filter_standalone.c](../../../tests/color/gamut/0062_filter_standalone.c)) |
| GM-61 | Independent color/gamut tile stages match sequential float bits for three mapping policies, five workspaces, two/four explicit greedy slots (including single-CPU hosts) and an incomplete final tile. | [tests/color/gamut/0063_pipeline_minde_linear_b2.t](../../../tests/color/gamut/0063_pipeline_minde_linear_b2.t), [tests/color/gamut/0069_pipeline_minde_linear_b4.t](../../../tests/color/gamut/0069_pipeline_minde_linear_b4.t), [tests/color/gamut/0070_pipeline_minde_gamma_b2.t](../../../tests/color/gamut/0070_pipeline_minde_gamma_b2.t), [tests/color/gamut/0071_pipeline_minde_gamma_b4.t](../../../tests/color/gamut/0071_pipeline_minde_gamma_b4.t), [tests/color/gamut/0072_pipeline_minde_oklab_b2.t](../../../tests/color/gamut/0072_pipeline_minde_oklab_b2.t), [tests/color/gamut/0073_pipeline_minde_oklab_b4.t](../../../tests/color/gamut/0073_pipeline_minde_oklab_b4.t), [tests/color/gamut/0074_pipeline_minde_cielab_b2.t](../../../tests/color/gamut/0074_pipeline_minde_cielab_b2.t), [tests/color/gamut/0075_pipeline_minde_cielab_b4.t](../../../tests/color/gamut/0075_pipeline_minde_cielab_b4.t), [tests/color/gamut/0076_pipeline_minde_din99d_b2.t](../../../tests/color/gamut/0076_pipeline_minde_din99d_b2.t), [tests/color/gamut/0077_pipeline_minde_din99d_b4.t](../../../tests/color/gamut/0077_pipeline_minde_din99d_b4.t), [tests/color/gamut/0078_pipeline_edge_linear_b2.t](../../../tests/color/gamut/0078_pipeline_edge_linear_b2.t), [tests/color/gamut/0079_pipeline_edge_linear_b4.t](../../../tests/color/gamut/0079_pipeline_edge_linear_b4.t), [tests/color/gamut/0080_pipeline_edge_gamma_b2.t](../../../tests/color/gamut/0080_pipeline_edge_gamma_b2.t), [tests/color/gamut/0081_pipeline_edge_gamma_b4.t](../../../tests/color/gamut/0081_pipeline_edge_gamma_b4.t), [tests/color/gamut/0082_pipeline_edge_oklab_b2.t](../../../tests/color/gamut/0082_pipeline_edge_oklab_b2.t), [tests/color/gamut/0083_pipeline_edge_oklab_b4.t](../../../tests/color/gamut/0083_pipeline_edge_oklab_b4.t), [tests/color/gamut/0084_pipeline_edge_cielab_b2.t](../../../tests/color/gamut/0084_pipeline_edge_cielab_b2.t), [tests/color/gamut/0085_pipeline_edge_cielab_b4.t](../../../tests/color/gamut/0085_pipeline_edge_cielab_b4.t), [tests/color/gamut/0086_pipeline_edge_din99d_b2.t](../../../tests/color/gamut/0086_pipeline_edge_din99d_b2.t), [tests/color/gamut/0087_pipeline_edge_din99d_b4.t](../../../tests/color/gamut/0087_pipeline_edge_din99d_b4.t), [tests/color/gamut/0088_pipeline_ray_linear_b2.t](../../../tests/color/gamut/0088_pipeline_ray_linear_b2.t), [tests/color/gamut/0089_pipeline_ray_linear_b4.t](../../../tests/color/gamut/0089_pipeline_ray_linear_b4.t), [tests/color/gamut/0090_pipeline_ray_gamma_b2.t](../../../tests/color/gamut/0090_pipeline_ray_gamma_b2.t), [tests/color/gamut/0091_pipeline_ray_gamma_b4.t](../../../tests/color/gamut/0091_pipeline_ray_gamma_b4.t), [tests/color/gamut/0092_pipeline_ray_oklab_b2.t](../../../tests/color/gamut/0092_pipeline_ray_oklab_b2.t), [tests/color/gamut/0093_pipeline_ray_oklab_b4.t](../../../tests/color/gamut/0093_pipeline_ray_oklab_b4.t), [tests/color/gamut/0094_pipeline_ray_cielab_b2.t](../../../tests/color/gamut/0094_pipeline_ray_cielab_b2.t), [tests/color/gamut/0095_pipeline_ray_cielab_b4.t](../../../tests/color/gamut/0095_pipeline_ray_cielab_b4.t), [tests/color/gamut/0096_pipeline_ray_din99d_b2.t](../../../tests/color/gamut/0096_pipeline_ray_din99d_b2.t), [tests/color/gamut/0097_pipeline_ray_din99d_b4.t](../../../tests/color/gamut/0097_pipeline_ray_din99d_b4.t) ([tests/color/gamut/0063_pipeline_bits.c](../../../tests/color/gamut/0063_pipeline_bits.c)) |
| GM-62 | One-slot, small and byte-input cases decline tile admission without changing the frame. | [tests/color/gamut/0066_pipeline_fallback.t](../../../tests/color/gamut/0066_pipeline_fallback.t) ([tests/color/gamut/0066_pipeline_fallback.c](../../../tests/color/gamut/0066_pipeline_fallback.c)) |
| GM-63 | The independent mapping node follows the final working-space conversion and precedes palette join or dithering after the encoder's post-schedule replan, including a one-slot schedule. | [tests/color/gamut/0068_filter_dag.t](../../../tests/color/gamut/0068_filter_dag.t) ([tests/color/gamut/0068_filter_dag.c](../../../tests/color/gamut/0068_filter_dag.c)) |
| GM-64 | The packed float32 mapper matches scalar results bit for bit after float storage across named, SMPTE-C and matrix/TRC ICC destinations, three mapping policies, five workspaces and samples on both sides of gamut edges, including signed zero. The scalar reference rereads the completed float plane in a separate pass so loop vectorization cannot forward an unrounded double from input generation. | [tests/color/gamut/0060_float32_scalar_equivalence.t](../../../tests/color/gamut/0060_float32_scalar_equivalence.t) ([tests/color/gamut/0060_float32_scalar_equivalence.c](../../../tests/color/gamut/0060_float32_scalar_equivalence.c)) |
| GM-65 | Explicit default gamut aim suboptions keep the output of an encode without them. | [tests/cli/dither-options/0044_gamut_target_default_output.t](../../../tests/cli/dither-options/0044_gamut_target_default_output.t) |
| GM-66 | `diffusion=aim` publishes a nonzero offset for out-of-gamut colors and changes diffusion. | [tests/cli/dither-options/0045_gamut_diffusion_aim_offset.t](../../../tests/cli/dither-options/0045_gamut_diffusion_aim_offset.t) |
| GM-67 | A LUT aim produces the same output as exact evaluation. | [tests/cli/dither-options/0046_gamut_lut_matches_exact.t](../../../tests/cli/dither-options/0046_gamut_lut_matches_exact.t) |
| GM-68 | An identity `-U` keeps byte sRGB output unchanged under both aims. | [tests/cli/dither-options/0047_gamut_aim_identity_byte_output.t](../../../tests/cli/dither-options/0047_gamut_aim_identity_byte_output.t) |
| GM-69 | `samples=shown` alone leaves diffusion on the mapped input. | [tests/cli/dither-options/0048_gamut_samples_keep_diffusion.t](../../../tests/cli/dither-options/0048_gamut_samples_keep_diffusion.t) |
| GM-70 | Byte error diffusion rejects `diffusion=aim` with a diagnostic. | [tests/cli/dither-options/0049_gamut_aim_rejects_byte_diffusion.t](../../../tests/cli/dither-options/0049_gamut_aim_rejects_byte_diffusion.t) |
| GM-71 | Byte RGBA alpha-key clustering samples take their aims. | [tests/cli/dither-options/0050_gamut_samples_alpha_key.t](../../../tests/cli/dither-options/0050_gamut_samples_alpha_key.t) |
| GM-72 | Palette aims clamp an entry outside sRGB to its shown color and keep a representable entry bit for bit. | [tests/color/gamut/0098_palette_aim.t](../../../tests/color/gamut/0098_palette_aim.t) ([tests/color/gamut/0098_palette_aim.c](../../../tests/color/gamut/0098_palette_aim.c)) |

Reference comparisons allow absolute error `3e-5` per canonical linear-sRGB channel for independently rounded matrices and binary-search endpoints. The matrix/TRC ICC case allows `3e-4` for its additional 15.16 colorant quantization; the mapped-centroid checks allow `4e-5` after float storage and workspace conversion. These budgets remain separate from the membership and local-MINDE search tolerances above.

The scalar reference generator and stored vectors are [gamut-reference.py](../../../tests/data/color-numeric/gamut-reference.py) and [gamut-reference.h](../../../tests/data/color-numeric/gamut-reference.h). They derive RGB matrices from chromaticities and independently invert Oklab matrices without calling the production implementation. [edge-reference.py](../../../tests/data/color-numeric/edge-reference.py) and [edge-reference.h](../../../tests/data/color-numeric/edge-reference.h) independently fit and intersect circles using their centers to check the production curvature-scaled calculation; they use the same `3e-5` channel budget and `3e-4` ICC budget. The EdgeSeeker vectors include both boundary branches, the hue seam, and blue-region folds.

[ray-reference.py](../../../tests/data/color-numeric/ray-reference.py) and [ray-reference.h](../../../tests/data/color-numeric/ray-reference.h) derive the Ray Trace expectations by testing six faces, independently of the production slab intersection. They cover inward corrections that advance the anchor, saturated highlights/shadows and the hue seam, with the same `3e-5` channel and `3e-4` ICC budgets. Exact comparisons are reserved for retained input, synthetic ray geometry and wire bins away from rounding thresholds.

### Quality regression tests

| ID | Contract | Owning test |
| --- | --- | --- |
| GMQ-01 | Compact and environment suboptions bind the same mapper and emit identical images, with separate sRGB image sanity. | [tests/cli/option-syntax/0083_output_gamut_image_regression.t](../../../tests/cli/option-syntax/0083_output_gamut_image_regression.t) |
| GMQ-02 | Compact and environment `diffusion` values bind the same aim and emit identical images. | [tests/cli/option-syntax/0092_output_gamut_diffusion_image_regression.t](../../../tests/cli/option-syntax/0092_output_gamut_diffusion_image_regression.t) |
| GMQ-03 | Compact and environment `samples` values bind the same aim and emit identical images. | [tests/cli/option-syntax/0093_output_gamut_samples_image_regression.t](../../../tests/cli/option-syntax/0093_output_gamut_samples_image_regression.t) |
| GMQ-04 | Compact and environment `aim_method` values reach the dither and emit identical images. | [tests/cli/option-syntax/0094_output_gamut_aim_method_image_regression.t](../../../tests/cli/option-syntax/0094_output_gamut_aim_method_image_regression.t) |
