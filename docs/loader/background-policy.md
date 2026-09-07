# Background Policy

Background policy chooses the color source used when a loader composites source
alpha. It does not choose the SIXEL DCS `P2` request; that is the independent
[Alpha Policy](alpha-policy.md).

The policy currently applies to file-background handling in the builtin PNG,
APNG, and GIF loaders. Other loaders may accept an explicit background without
having a competing file-background source.

## Policies

| Policy | First choice | Fallback |
| --- | --- | --- |
| `file_first` | PNG `bKGD` or the GIF logical-screen background | External background |
| `explicit_first` | External background | Supported file background |

`file_first` is the default. A missing first-choice source always falls back to
the other group; the policy never invents a background color.

## External background sources

External backgrounds are resolved in this order:

1. `-B COLOR` from the command line;
2. `SIXEL_BGCOLOR` when `-B` is absent;
3. a successful OSC 11 reply when probing is enabled and no explicit color has
   already been resolved.

An OSC 11 reply is a candidate in the external group. Under `file_first`, a
supported file background can therefore still win after the query succeeds.
OSC 11 is queried only for effective `alpha-policy=composite`, only when an
eligible output stream is a terminal, and only when the query controls permit
it. Failure, timeout, or a malformed reply leaves the external source
unresolved and allows normal fallback.

## Configuration and precedence

The same two values are exposed at three levels. Highest precedence comes
first:

1. the loader `background_policy` suboption, including short form
   `Pfile_first|Pexplicit_first`;
2. `--background-policy=file_first|explicit_first`;
3. `SIXEL_BACKGROUND_POLICY`;
4. the built-in default, `file_first`.

The loader suboption is request-specific and therefore overrides the global
dedicated option. The dedicated option is deliberately long-only and
overrides the environment value. Invalid command-line values are rejected.
Invalid or empty environment values fall back to `file_first`. See
`img2sixel -H` for loader-suboption syntax and environment injection.

## Colorspace

Background composition is performed in the loader's selected background
colorspace. `SIXEL_LOADER_BACKGROUND_COLORSPACE` and the corresponding loader
suboption select `gamma` or `linear`; `gamma` is the default. OSC 11 describes
a terminal UI color and is always interpreted as gamma-encoded, regardless of
that setting. File metadata is interpreted according to the owning format and
loader path before composition.

Fully transparent pixels are filled only when the effective alpha policy is
`composite` and a background is resolved. `clear` and `keep` retain alpha-zero
coverage, while partial alpha may still require composition because SIXEL has
no partial-alpha representation. See the alpha policy document for the exact
mask and DCS behavior.

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and an owning test. The reciprocal
`Policy:` reference in each test is checked by `staticcheck-doc-test-links`.

| ID | Contract | Owning test |
| --- | --- | --- |
| BP-01 | The builtin PNG default is `file_first`. | [tests/loader/builtin/0113_loader_builtin_png_bkgd_default_file_first.t](../../tests/loader/builtin/0113_loader_builtin_png_bkgd_default_file_first.t) |
| BP-02 | PNG accepts both values; invalid environment input falls back to `file_first`; `explicit_first` changes source priority. | [tests/loader/builtin/1497_loader_builtin_background_policy_png_priority.t](../../tests/loader/builtin/1497_loader_builtin_background_policy_png_priority.t) |
| BP-03 | APNG falls back to the explicit source when no supported file background exists. | [tests/loader/builtin/1498_loader_builtin_background_policy_apng_explicit_fallback.t](../../tests/loader/builtin/1498_loader_builtin_background_policy_apng_explicit_fallback.t) |
| BP-04 | GIF default, invalid-value fallback, and `explicit_first` selection are stable. | [tests/loader/builtin/1499_loader_builtin_background_policy_gif_priority.t](../../tests/loader/builtin/1499_loader_builtin_background_policy_gif_priority.t) |
| BP-05 | GIF canvas fill chooses between logical-screen and explicit backgrounds according to policy. | [tests/loader/0054_loader_gif_bgcolor_canvas_fill.c](../../tests/loader/0054_loader_gif_bgcolor_canvas_fill.c) |
| BP-06 | Loader suboption and environment forms are equivalent, and an explicit loader suboption wins a conflicting environment value. | [tests/cli/options/regression/0097_loader_background_policy_image_regression.t](../../tests/cli/options/regression/0097_loader_background_policy_image_regression.t) |
| BP-07 | Dedicated `--background-policy` overrides a conflicting environment value. | [tests/cli/options/matching/0275_option_matching_background_policy_cli_precedence.t](../../tests/cli/options/matching/0275_option_matching_background_policy_cli_precedence.t) |
| BP-08 | An explicit loader suboption overrides both the dedicated option and environment. | [tests/quant/palette/usage/0188_background_policy_loader_priority.t](../../tests/quant/palette/usage/0188_background_policy_loader_priority.t) |
| BP-09 | An unknown dedicated option value is rejected and reports the valid values. | [tests/cli/options/matching/0273_option_matching_background_policy_unknown_value.t](../../tests/cli/options/matching/0273_option_matching_background_policy_unknown_value.t) |
| BP-10 | Gamma and linear background colorspace paths produce their documented image result. | [tests/cli/options/regression/0098_loader_background_colorspace_image_regression.t](../../tests/cli/options/regression/0098_loader_background_colorspace_image_regression.t) |
| BP-11 | The loader background-colorspace override applies and resets correctly. | [tests/loader/0053_loader_background_colorspace_override.c](../../tests/loader/0053_loader_background_colorspace_override.c) |
| BP-12 | OSC 11 colorspec syntax accepts supported forms and rejects malformed colors. | [tests/loader/0050_loader_osc11_colorspec_parser.c](../../tests/loader/0050_loader_osc11_colorspec_parser.c) |
| BP-13 | OSC 11 response framing accepts BEL/ST termination and rejects malformed replies. | [tests/loader/0051_loader_osc11_response_parser.c](../../tests/loader/0051_loader_osc11_response_parser.c) |
| BP-14 | OSC 11 probing requires `composite`, an eligible terminal stream, and no pre-resolved explicit background. | [tests/loader/0052_loader_osc11_query_control.c](../../tests/loader/0052_loader_osc11_query_control.c) |
| BP-15 | Loader OSC 11 suboption/environment precedence and the alpha-policy gate reach the integrated query path. | [tests/cli/options/regression/0116_loader_osc11_query_image_regression.t](../../tests/cli/options/regression/0116_loader_osc11_query_image_regression.t) |

### Coverage audit boundary

The automated inventory covers every accepted policy value, both fallback
directions, default and invalid environment behavior, all three configuration
levels, PNG/APNG/GIF source selection, background colorspace, OSC 11 parsing,
and OSC 11 eligibility. A real OSC 11 round trip is not automated because it
requires an interactive terminal that implements the query. Parser, timeout,
stream-eligibility, and alpha-policy gating are tested below that external
boundary.
