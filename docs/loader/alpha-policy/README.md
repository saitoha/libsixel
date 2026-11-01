# Alpha Policy

The alpha policy defines where source alpha is resolved and what an omitted SIXEL pixel asks the terminal to do. It is selected with `img2sixel --alpha-policy=POLICY` (short form `-A POLICY`) or the `SIXEL_ALPHA_POLICY` environment variable. An explicit command-line option takes precedence over the environment.

## Alpha model

SIXEL has no per-pixel alpha channel. A SIXEL data bit either paints the selected palette color or leaves that position unpainted. DCS `P2` tells the terminal what to do with unpainted positions; it does not carry an alpha value and cannot express partial transparency.

libsixel nevertheless accepts alpha-bearing source files and memory formats such as RGBA, ARGB, BGRA, ABGR, gray-alpha, and alpha-gray. Loader paths resolve that source alpha before palette encoding. Semi-transparent pixels are composited when a background is available. Fractional alpha cannot survive in SIXEL:

- an exact alpha-zero sample may remain an unpainted pixel under `clear` or `keep`;
- after normalization, color samples and alpha-zero coverage are represented separately, commonly as RGB or three-channel float pixels plus a binary per-pixel mask, or as indexed pixels plus a transparent palette index.

A frame can temporarily retain an alpha-bearing packed pixel format. This is needed by public frame APIs and by loader backends whose normalization happens at a later boundary. The frame therefore carries transparency metadata as well as a pixel-format tag; the encoder handles either an alpha-bearing buffer or an already separated mask. See [Pixel Formats and Alpha Representation](../../concepts/pixelformat.md) for the full distinction between source, frame, and wire representations.

## Policies

| Policy | Loader treatment | SIXEL treatment |
| --- | --- | --- |
| `auto` | Preserve source alpha without terminal background probing. | Select `clear` normally and `keep` when transparent offset or 6delta output requires retained pixels. |
| `composite` | Composite source alpha over the resolved background. | Emit opaque output when composition succeeds. If no background can be resolved, fall back to `keep`. |
| `clear` | Preserve alpha-zero pixels. | Omit alpha-zero pixels and emit DCS `P2=0`, requesting the terminal to clear omitted pixels. |
| `keep` | Preserve alpha-zero pixels. | Omit alpha-zero pixels and emit DCS `P2=1`, requesting the terminal to keep existing pixels. |

`auto` is the default. These are the only accepted policy names. If no background is available, exact alpha zero can still become an omitted pixel, but nonzero fractional alpha cannot remain fractional in the SIXEL output.

## Visual comparison

The following APNG illustrates the policies with a black terminal background and a translucent window. `composite` produces an opaque image rectangle, `clear` removes omitted pixels on each frame, and `keep` leaves earlier image pixels in place. This is an illustration of the requested operations, not a promise that every terminal will produce the same pixels.

![Animated comparison of composite, clear, and keep](figures/alpha-policy-comparison.png)

A [GIF fallback](figures/alpha-policy-comparison.gif) is available for viewers without APNG animation support. The bouncing-ball source is by Holger Will and is [released into the public domain](https://commons.wikimedia.org/wiki/File:Animated_PNG_example_bouncing_beach_ball.png).

`clear` and `keep` name the requested operation rather than claiming portable transparency. SIXEL renderers differ in how graphics interact with the text plane, earlier graphics, and a translucent terminal window. In xterm and mlterm, `P2=1` holes commonly keep the existing graphics content. Other terminals can produce a different visible result: P2 rendering is terminal-dependent. `P2=0` and `P2=1` never perform source alpha blending; blending belongs to the loader.

## Background resolution

The loader may resolve a background from these sources:

- `-B COLOR` or `SIXEL_BGCOLOR`;
- an OSC 11 terminal background reply when probing is enabled and succeeds;
- a file background such as PNG `bKGD` or the GIF logical-screen background.

Background selection is a separate policy axis. See [Background Policy](../background-policy.md) for source priority, option precedence, colorspace, and OSC 11 controls. OSC 11 is queried only for an effective `composite` alpha policy.

When `composite` cannot resolve any source, the loader must not expose hidden RGB from fully transparent pixels as visible output. It retains the alpha-zero mask, and the encoder falls back to the `keep` request with `P2=1`.

## Loader and encoder boundary

The selected native float encoder path preserves RGB precision alongside a separate alpha-zero mask. Its generated-palette eligibility, reserved-slot handling and exact output contract are documented under [Separate alpha-zero masks](../../encoder/output/output-colorspace.md#separate-alpha-zero-masks); this does not add a per-pixel alpha channel to SIXEL.

Unresolved packed frames supplied to the selected encoder path use its [packed alpha normalization boundary](../../encoder/output/output-colorspace.md#packed-alpha-normalization), which preserves the existing low-level gamma/background convention. This does not reinterpret or recompose loader-resolved RGB.

The loader owns alpha normalization. If it composites onto a background, the resulting frame is opaque and must not carry an alpha-zero mask. If it preserves alpha zero, it records that state in the frame's transparent index or per-pixel mask.

The encoder trusts that frame state. The ordinary generated-palette path reserves a key color only when the frame still contains transparency, then selects `P2` from the requested policy. [High-color output](../../encoder/output/high-color/README.md#separated-source-masks) uses its existing skip value for separated masks instead of reserving another paint color. It must not discard a loader-provided mask merely because the policy is `composite`: such a mask means that background resolution failed and the loader intentionally used the `keep` fallback.

The builtin RGBA and non-indexed PNG paths preserve source RGB for alpha-zero pixels under `clear` and `keep`, while still compositing partial alpha over an available background. Under `composite`, they composite all alpha, including alpha zero, and clear the mask after successful composition.

Eligible RGB/Gray8/16 TIFF alpha scanlines retain original alpha while reversing source association and applying CMS. Background composition follows canonical linear conversion, with the background interpreted independently. Exact-zero masks and partial-alpha behavior follow the policies above; unmanaged 8-bit all-255 input stays exact byte RGB, while UINT16 alpha retains float RGB even when all alpha words are 65535. Original 1/65535 alpha remains nonzero. See the [TIFF source and publication contract](../libtiff.md#original-integer-alpha-and-composition) for eligibility, fallback representation and remaining layout and sample-format limitations.

## Related options

`--transparent-offset` and 6delta output require `keep` because their retained pixels rely on the `P2=1` image-plane request. `auto` selects `keep` for either feature. Explicit `clear` and `composite` are rejected when either feature is active.

[Delta encoding (6delta)](../../encoder/output/delta-encoding.md) explains its retained RGB plane, per-pixel keep decisions, partial-update coordinates, and invalidation. Plain `keep` requests terminal-side preservation without enabling that history or comparing successive frame colors.

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and an owning test. The reciprocal `Policy:` reference in each test is checked by `staticcheck-doc-test-links`.

| ID | Contract | Owning test |
| --- | --- | --- |
| AP-01 | `auto` is the default, explicit `auto` matches it, and normal output resolves to `clear`. | [tests/palette/selection/0009_auto_policy_defaults_clear.t](../../../tests/palette/selection/0009_auto_policy_defaults_clear.t) |
| AP-02 | `composite` fills alpha-zero pixels from a resolved explicit background. | [tests/loader/alpha/0001_loader_builtin_composite_policy_fills_transparency.t](../../../tests/loader/alpha/0001_loader_builtin_composite_policy_fills_transparency.t) |
| AP-03 | `clear` omits alpha-zero pixels from the SIXEL body and emits DCS `P2=0`. | [tests/palette/alpha/0076_clear_policy_omits_alpha_zero.t](../../../tests/palette/alpha/0076_clear_policy_omits_alpha_zero.t) |
| AP-04 | `keep` omits alpha-zero pixels from the SIXEL body and emits DCS `P2=1`, even when `-B` exists. | [tests/palette/alpha/0070_keep_policy_omits_alpha_zero.t](../../../tests/palette/alpha/0070_keep_policy_omits_alpha_zero.t) |
| AP-05 | Unresolved `composite` falls back to mask retention and DCS `P2=1`. | [tests/loader/alpha/0011_composite_policy_without_color_falls_back_keep.t](../../../tests/loader/alpha/0011_composite_policy_without_color_falls_back_keep.t) |
| AP-06 | The unreleased legacy names `background`, `transparent`, `p2-0`, and `--transparent-policy` remain rejected. | [tests/palette/alpha/0077_alpha_policy_rejects_unreleased_names.t](../../../tests/palette/alpha/0077_alpha_policy_rejects_unreleased_names.t) |
| AP-07 | Environment parsing accepts all policies and invalid values fall back to `auto`/`clear`. | [tests/loader/alpha/0004_loader_builtin_alpha_policy_strict_fallback.t](../../../tests/loader/alpha/0004_loader_builtin_alpha_policy_strict_fallback.t) |
| AP-08 | `SIXEL_ALPHA_POLICY` and `-A` produce equivalent results for the same value. | [tests/cli/loader-options/0083_alpha_policy_environment_cli_equivalence.t](../../../tests/cli/loader-options/0083_alpha_policy_environment_cli_equivalence.t) |
| AP-09 | Explicit `-A` takes precedence over `SIXEL_ALPHA_POLICY`. | [tests/cli/loader-options/0078_option_matching_alpha_policy_cli_precedence.t](../../../tests/cli/loader-options/0078_option_matching_alpha_policy_cli_precedence.t) |
| AP-10 | Repeated environment assignments use the last value and remain stable across multiple inputs. | [tests/loader-png/alpha-indexed/0016_loader_builtin_trns_keycolor_multi_input_sequence.t](../../../tests/loader-png/alpha-indexed/0016_loader_builtin_trns_keycolor_multi_input_sequence.t) |
| AP-11 | Transparent offset resolves default `auto` to `keep` and emits retained-plane geometry. | [tests/palette/alpha/0071_transparent_offset_header_geometry.t](../../../tests/palette/alpha/0071_transparent_offset_header_geometry.t) |
| AP-12 | Transparent offset rejects an explicitly incompatible alpha policy. | [tests/palette/alpha/0072_transparent_offset_policy_conflict.t](../../../tests/palette/alpha/0072_transparent_offset_policy_conflict.t) |
| AP-13 | 6delta resolves default `auto` to `keep`. | [tests/delta/comparison/0019_6delta_auto_policy_selects_keep.t](../../../tests/delta/comparison/0019_6delta_auto_policy_selects_keep.t) |
| AP-14 | 6delta rejects an explicitly incompatible alpha policy. | [tests/delta/comparison/0020_6delta_explicit_alpha_policy_conflict.t](../../../tests/delta/comparison/0020_6delta_explicit_alpha_policy_conflict.t) |
| AP-15 | OSC 11 probing is gated to `composite` and requires an eligible terminal stream. | [tests/_support/loader/0052_loader_osc11_query_control.c](../../../tests/_support/loader/0052_loader_osc11_query_control.c), [tests/cli/terminal-color/0143_loader_osc11_query_gate.t](../../../tests/cli/terminal-color/0143_loader_osc11_query_gate.t) |
| AP-16 | The default `auto` path suppresses OSC 11 while explicit `composite` reaches the query gate. | [tests/cli/loader-options/0115_loader_osc11_query_image_regression.t](../../../tests/cli/loader-options/0115_loader_osc11_query_image_regression.t) |
| AP-17 | Partial-alpha composition uses the selected background colorspace. | [tests/cli/loader-options/0097_loader_background_colorspace_image_regression.t](../../../tests/cli/loader-options/0097_loader_background_colorspace_image_regression.t) |
| AP-18 | Transparent offset rejects an explicit `clear` policy. | [tests/palette/alpha/0078_transparent_offset_clear_policy_conflict.t](../../../tests/palette/alpha/0078_transparent_offset_clear_policy_conflict.t) |
| AP-19 | 6delta rejects an explicit `composite` policy. | [tests/delta/comparison/0021_6delta_composite_policy_conflict.t](../../../tests/delta/comparison/0021_6delta_composite_policy_conflict.t) |
| AP-20 | Eligible TIFF byte alpha preserves zero coverage and composes partial alpha under default/auto/clear/keep/composite, with absent or resolved background. | [tests/loader-tiff/alpha/0014_alpha_policy.t](../../../tests/loader-tiff/alpha/0014_alpha_policy.t) |
| AP-21 | Eligible TIFF UINT16 alpha preserves original nonzero coverage and composes partial alpha under default/auto/clear/keep/composite, with absent or resolved background. | [tests/loader-tiff/alpha/0044_alpha16_policy.t](../../../tests/loader-tiff/alpha/0044_alpha16_policy.t) |

### Coverage audit boundary

The automated inventory covers policy parsing, defaults, precedence, all four effective behaviors, unresolved-background fallback, multi-input state, OSC 11 gating, and the transparent-offset and 6delta interactions. Actual rendering of DCS `P2=0` and `P2=1` by xterm, mlterm, and other terminals is deliberately not classified as automated library coverage: that result belongs to the external terminal renderer. The tests verify the exact DCS request emitted by libsixel; the visual comparison above documents representative renderer behavior without claiming portability.
