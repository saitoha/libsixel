# Loader precision, gamut, and fast paths

This policy applies to every loader adapter. It separates the backend's decoded representation from the adapter's color interpretation and from the encoder's later working/output spaces. It defines the contract for new and repaired paths; it does **not** assert that every existing backend and format already meets it. Backend documents and the [loader inventory](README.md#compiled-loader-inventory) identify implemented paths and limitations. See [color management](color-management.md) for engine selection and [encoder precision](../functionality/precision.md) for `-.`.

## Representation is not color meaning

An indexed source need not produce an indexed frame. If a backend has expanded a palette into RGB/RGBA, accept those decoded pixels without reconstructing indices or quantizing them back into a palette. Preserve indexed structure only when the backend provides it and the required color/alpha operations can represent their result that way. Index width is not RGB precision: a one-bit PNG index can select an eight-bit RGB palette entry.

Read layout, sample type/depth, alpha association and adopted color space together. RGBA does not imply sRGB, eight-bit components do not imply a narrow gamut, and float storage does not prove that earlier operations retained precision. An ICC profile describing source coordinates is distinct from the output intent selected by `-U`. Relabeling P3 bytes as sRGB, or treating canonical linear coordinates as device RGB, is not a conversion.

The backend and adapter have separate responsibilities. A backend can legitimately change storage layout without changing colors. The adapter must not introduce an avoidable byte rendering, premultiplication, palette reconstruction or gamut clipping step merely to simplify access. If the backend already rounded, clipped or discarded hidden RGB, a later float cast cannot recover it. Record that limitation for the affected backend/format/layout; do not describe it as lossless preservation or generalize it to all formats handled by that backend.

## Precision and gamut through normalization

| Boundary | Required behavior |
| --- | --- |
| Backend output | Retain available component precision, source color definition and original alpha while validating layout, stride and sizes. |
| Color conversion | Apply the adopted source definition once, using an appropriate high-precision path into the canonical coordinates. Avoid an intermediate bounded sRGB byte bitmap. |
| Canonical frame | Publish samples and matching format/color tags together. Wide-gamut colors expressed in canonical linear sRGB can legitimately contain negative or above-one components. |
| Alpha resolution | Treat alpha as coverage, outside the RGB transfer/profile transform. Composite in the defined linear space with the background's own interpretation. Preserve exact-zero coverage independently of RGB. |
| Later processing | Keep the extended values through precision-requiring transforms and processing. A float32 storage format may require double intermediates. |
| Explicit narrowing | Apply clipping/rounding only at an identified destination boundary that requires bounded/integer values, under that boundary's contract. Do not use narrowing as an internal normalization step. |

See [canonical color spaces](../concepts/colorspace.md), [numeric precision](../concepts/pixelformat-precision.md), [alpha](alpha-policy.md) and [background](background-policy.md) for the detailed contracts. For straight source alpha, hidden RGB remains color data even at alpha zero. Premultiplied backend output requires an explicit association contract; dividing cannot recover colors the backend already erased. SIXEL's binary coverage remains separate from the source's fractional alpha and is resolved according to the alpha policy.

“Preserve precision” means avoid unnecessary narrowing and honor a documented numerical error budget, not bitwise equality across different CMMs. “Preserve gamut” means retain the canonical coordinates needed to represent the source color; it does not promise that an arbitrary destination display or output format can reproduce that color. Unsupported profile classes or missing CMS connections must be reported as implementation limitations rather than hidden by a float output tag.

## Keep the byte path when it is sufficient

Precision selection follows the **work that actually occurs**, not the presence of a profile, an alpha channel, an installed CMM, or a float-capable backend alone.

| Available samples and required operations | Default / `-.8bit` | `-.float32` |
| --- | --- | --- |
| Exact canonical gamma RGB bytes or a compatible byte palette; no color conversion or precision-requiring arithmetic | Retain a byte path. Avoid a full-image float allocation and redundant CMM call. | Promote before the requested float work; an exact byte decode may feed the shared planner. |
| Straight byte RGB with alpha, but no composition or color conversion | Copy RGB and extract zero coverage without rounding RGB or passing alpha through CMS. Alpha-channel presence alone is not a float requirement. | Preserve the same source values and coverage in the float path. |
| A profile/transfer conversion, high-depth source, existing float frame, linear-light partial-alpha composition, or another independent float requirement | Preserve the required precision; the byte preference cannot override it. | Preserve the required precision. |
| Backend-added opaque alpha or a background that contributes to no pixel | These alone do not justify promoting otherwise sufficient byte samples. | Honor the float request. |

An identity color interpretation may qualify for the byte path only when established from the adopted color definition. Missing embedded ICC is not proof of sRGB: other metadata or the backend's adopted space can supply the definition. Conversely, merely selecting a CMS engine does not mean a transform was applied.

`-.8bit` is a preference, not permission to discard source precision or gamut. `-.float32` requests extra float work when bytes would otherwise suffice. A format-specific CMS `prefer_8bit` setting is a separate, explicit post-conversion narrowing boundary; it must not be inferred from `-.8bit`. The loader must not weaken precision requirements because palette output is requested or a low color count is selected: those choices belong to later quantization.

Keep existing fast paths when repairing CMS routes. Conversion of a small indexed palette once can avoid per-pixel CMM work; an already-expanded provider should use its actual RGB samples instead. Allocation or conversion failure must not silently retry the same source as unmanaged bytes. Publish a complete frame only after all required buffers, transforms and alpha work succeed. The loader manager's separately documented fallback selection is not permission for an adapter to bypass its color contract.

## Adoption and review

Apply the policy to the loader's actual supported routes. A backend that already performs native color management must preserve that interpretation even if its API does not expose the common CMS selector. A loader with no CMS connection is not automatically brought into scope by a neighboring loader repair. Document that absence at loader level; missing routes inside an otherwise connected loader remain implementation gaps, classified by unreadable/wrong interpretation, missing CMS, or lost precision.

Review every affected fast and managed path together. Backend expansion alone is acceptable; avoidable precision/gamut loss in the adapter is a bug. When an API cannot provide the required samples, document the exact boundary and supported alternatives instead of claiming recovery from a lower-precision intermediate. This policy does not itself extend support to unsupported formats, provider layouts, profiles or platforms.

## Test coverage

<!-- test-coverage: enforced -->

Value tests and MS-SSIM regressions have different purposes. Value tests protect source interpretation, finite extended components, channel order, original alpha ratios, exact masks, frame tags, byte identity and failure publication. Quality tests protect perceptual output and cannot establish those arithmetic or metadata contracts. Confirm the actual loader/CMM used, so an unintended fallback cannot pass a CMS test. Byte controls should also check absence of unnecessary float-image allocation and CMS application. Tiny fixtures keep these checks fast; record native-library tolerance separately from adapter arithmetic tolerance.

The following CoreGraphics cases are concrete policy enforcement for ImageIO-expanded indexed PNGs, not a claim of universal loader coverage. The existing [CoreGraphics](coregraphics.md#test-coverage), [libpng](libpng.md#test-coverage), [libtiff](libtiff.md#test-coverage), and [precision](../functionality/precision.md#test-coverage) inventories own their additional value and quality coverage. Host-dependent tests skip explicitly when their required provider representation is unavailable.

| ID | Contract | Owning test |
| --- | --- | --- |
| CGE-01 | P3 binary transparency retains extended RGB, hidden RGB and exact zero coverage with the byte preference. | [tests/loader/coregraphics/expanded/0001_p3_binary.t](../../tests/loader/coregraphics/expanded/0001_p3_binary.t) |
| CGE-02 | P3 fractional alpha without a background preserves straight RGB and masks only exact zero. | [tests/loader/coregraphics/expanded/0002_p3_partial.t](../../tests/loader/coregraphics/expanded/0002_p3_partial.t) |
| CGE-03 | P3 opaque Adam7 preserves extended RGB without an unnecessary transparency mask. | [tests/loader/coregraphics/expanded/0003_p3_adam7.t](../../tests/loader/coregraphics/expanded/0003_p3_adam7.t) |
| CGE-04 | An explicit float request retains P3 source values and coverage. | [tests/loader/coregraphics/expanded/0004_p3_float.t](../../tests/loader/coregraphics/expanded/0004_p3_float.t) |
| CGE-05 | Named sRGB binary alpha preserves exact bytes and hidden RGB without CMM apply or image-sized float allocation. | [tests/loader/coregraphics/expanded/0005_srgb_binary_byte.t](../../tests/loader/coregraphics/expanded/0005_srgb_binary_byte.t) |
| CGE-06 | Named sRGB fractional alpha with no background likewise keeps exact byte RGB and zero coverage. | [tests/loader/coregraphics/expanded/0006_srgb_partial_byte.t](../../tests/loader/coregraphics/expanded/0006_srgb_partial_byte.t) |
| CGE-07 | Named sRGB opaque Adam7 stays byte-based even when the backend adds an opaque alpha channel. | [tests/loader/coregraphics/expanded/0007_srgb_adam7_byte.t](../../tests/loader/coregraphics/expanded/0007_srgb_adam7_byte.t) |
| CGE-08 | An explicit float request converts expanded named sRGB to canonical linear coordinates. | [tests/loader/coregraphics/expanded/0008_srgb_float.t](../../tests/loader/coregraphics/expanded/0008_srgb_float.t) |
| CGE-09 | One-bit Adam7 indices select full eight-bit P3 components across odd-width rows. | [tests/loader/coregraphics/expanded/0009_p3_adam7_1.t](../../tests/loader/coregraphics/expanded/0009_p3_adam7_1.t) |
| CGE-10 | Two-bit Adam7 indices select full eight-bit P3 components across odd-width rows. | [tests/loader/coregraphics/expanded/0010_p3_adam7_2.t](../../tests/loader/coregraphics/expanded/0010_p3_adam7_2.t) |
| CGE-11 | Four-bit Adam7 indices select full eight-bit P3 components across odd-width rows. | [tests/loader/coregraphics/expanded/0011_p3_adam7_4.t](../../tests/loader/coregraphics/expanded/0011_p3_adam7_4.t) |
| CGE-12 | P3 partial alpha uses the original ratio over a gamma background under keep, retaining zero coverage. | [tests/loader/coregraphics/expanded/0012_p3_gamma_keep.t](../../tests/loader/coregraphics/expanded/0012_p3_gamma_keep.t) |
| CGE-13 | P3 partial alpha uses the original ratio over a linear background under clear, retaining zero coverage. | [tests/loader/coregraphics/expanded/0013_p3_linear_clear.t](../../tests/loader/coregraphics/expanded/0013_p3_linear_clear.t) |
| CGE-14 | P3 composite over a gamma background includes zero alpha and publishes no mask. | [tests/loader/coregraphics/expanded/0014_p3_gamma_composite.t](../../tests/loader/coregraphics/expanded/0014_p3_gamma_composite.t) |
| CGE-15 | P3 composite over a linear background includes zero alpha and publishes no mask. | [tests/loader/coregraphics/expanded/0015_p3_linear_composite.t](../../tests/loader/coregraphics/expanded/0015_p3_linear_composite.t) |
| CGE-16 | Named sRGB partial-alpha composition requires float despite the byte preference. | [tests/loader/coregraphics/expanded/0016_srgb_partial_composite.t](../../tests/loader/coregraphics/expanded/0016_srgb_partial_composite.t) |
| CGE-17 | A background that contributes to no opaque expanded pixel does not force byte sRGB to float. | [tests/loader/coregraphics/expanded/0017_srgb_opaque_background_byte.t](../../tests/loader/coregraphics/expanded/0017_srgb_opaque_background_byte.t) |
| CGE-18 | Preserved binary zero coverage and an unused background retain the exact sRGB byte path. | [tests/loader/coregraphics/expanded/0018_srgb_binary_background_byte.t](../../tests/loader/coregraphics/expanded/0018_srgb_binary_background_byte.t) |
| CGE-19 | Global engine none does not bypass CoreGraphics native P3 interpretation. | [tests/loader/coregraphics/expanded/0019_p3_engine_none.t](../../tests/loader/coregraphics/expanded/0019_p3_engine_none.t) |
| CGE-20 | Float-frame allocation failure emits no callback; retry succeeds without leaks or byte fallback. | [tests/loader/coregraphics/expanded/0020_float_allocation_retry.t](../../tests/loader/coregraphics/expanded/0020_float_allocation_retry.t) |
| CGE-21 | Mask allocation failure after native conversion emits no callback; retry succeeds without leaks. | [tests/loader/coregraphics/expanded/0021_mask_allocation_retry.t](../../tests/loader/coregraphics/expanded/0021_mask_allocation_retry.t) |
| CGE-22 | Byte-path mask allocation failure emits no callback; retry preserves exact bytes without float allocation. | [tests/loader/coregraphics/expanded/0022_byte_mask_allocation_retry.t](../../tests/loader/coregraphics/expanded/0022_byte_mask_allocation_retry.t) |
