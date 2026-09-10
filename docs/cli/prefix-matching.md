# Choice Prefix-Matching Policy

## Scope

This document defines how libsixel CLI choice values are resolved from exact names and prefixes. The shared matcher is used by `img2sixel`, `sixel2png`, and selected finite-choice arguments in `lsqa`; it is not synonymous with the [suboption grammar](suboptions.md). A suboption key remains exact even when its choice-valued payload permits a prefix.

Prefix matching is a typing convenience with compatibility consequences. It is accepted only where a schema declares a finite choice vocabulary, so the parser can prove whether an abbreviation has one semantic result.

## Resolution algorithm

[`sixel_option_match_choice()`](../../src/options.c) performs case-sensitive matching in this order:

1. Reject a null or empty token as no match.
2. Scan declared spellings whose leading bytes equal the token.
3. If a spelling is an exact full match, return its semantic value immediately.
4. Otherwise accept the prefix only when every matched spelling maps to the same semantic value.
5. Reject matches that map to different semantic values as ambiguous.
6. Reject a token with no matched spelling as unknown; the correction layer may calculate suggestions, but matching has already failed.

Semantic-value comparison matters because multiple compatibility aliases can intentionally represent one value. Such aliases do not manufacture ambiguity merely because more than one spelling shares a prefix. Conversely, one displayed candidate is not enough to prove success if another hidden alias under the same prefix maps to a different value.

## Where prefixes apply

Prefixes apply to declared CLI choice domains, including structured-option base names and choice-valued suboption values. Examples include:

```sh
# "ser" uniquely selects the choice value "serpentine".
img2sixel -d fs:scan=ser input.png

# "st" matches both "stucki" and "stbn" and therefore fails.
img2sixel -d st input.png
```

They do not apply to long suboption keys, arbitrary strings, paths, integers, sizes, or floating-point values. Consequently, `scan=ser` can be valid while `sca=serpentine` is not. A registered compact key such as `Nserpentine` is an independent exact key spelling, not the result of prefix matching.

## CLI and environment vocabularies

CLI and environment matching are separate contracts. The registry can define CLI prefix matching while retaining exact environment names, environment-only aliases, case-insensitive legacy values, numeric legacy forms, or another explicitly recorded compatibility rule. An accepted CLI command such as `scan=ser` therefore does not imply that the corresponding environment variable accepts `ser`.

This separation prevents an ergonomic interactive rule from silently broadening inherited process configuration. Documentation and tests must name the actual environment vocabulary instead of asking users to infer it from CLI behavior.

## Ambiguity diagnostics

An ambiguous prefix is always a parse failure; converters use exit status 2. Human diagnostics may list matched spellings when `prefix_suggestions` is enabled, while code diagnostic mode retains the stable `AMBIGUOUS_PREFIX` category and omits that evolving candidate prose. Disabling the candidate list does not make an ambiguous token valid.

Unknown input is distinct from ambiguity. An unknown token may receive a fuzzy `Did you mean:` message under the [correction-suggestion policy](correction-suggestions.md), but a suggestion never participates in resolution.

## Compatibility consequences

Every accepted unique prefix is observable behavior. Adding a new spelling can turn an existing successful abbreviation into an ambiguity even when no canonical name is removed. Before changing a choice table:

1. enumerate the existing exact names and aliases by semantic value;
2. find the shortest unique prefixes and any prefixes documented or tested directly;
3. check whether the new spelling changes the result of an existing prefix;
4. prefer a clear canonical name over a contrived initial, but provide a compatibility plan if a released prefix must change;
5. update completion and diagnostics so displayed candidates match the active choice domain.

Exact full names remain the reproducible form for scripts because they communicate intent and avoid future ambiguity. Short prefixes are primarily an interactive convenience; a test may intentionally pin one where maintaining that convenience is a project decision, such as the registered lookup-policy initials.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| PFX-01 | A unique choice-value prefix succeeds without an error diagnostic. | [tests/cli/options/matching/0001_option_matching_prefix_unique.t](../../tests/cli/options/matching/0001_option_matching_prefix_unique.t) |
| PFX-02 | A prefix matching different semantic values fails with exit status 2 and an ambiguity diagnostic. | [tests/cli/options/matching/0002_option_matching_prefix_ambiguous.t](../../tests/cli/options/matching/0002_option_matching_prefix_ambiguous.t) |
| PFX-03 | Long suboption keys do not inherit value-prefix matching. | [tests/cli/options/matching/0026_option_matching_loader_suboption_prefix_rejected.t](../../tests/cli/options/matching/0026_option_matching_loader_suboption_prefix_rejected.t) |
| PFX-04 | An environment choice can retain exact lowercase matching even when the CLI offers prefixes. | [tests/cli/options/matching/0239_option_matching_diag_mode_environment_exact.t](../../tests/cli/options/matching/0239_option_matching_diag_mode_environment_exact.t) |
| PFX-05 | Every registered lookup policy retains its accepted single-character initial. | [tests/cli/0035_cli_lookup_policy_initial_prefixes.c](../../tests/cli/0035_cli_lookup_policy_initial_prefixes.c), [tests/cli/options/matching/0278_option_matching_lookup_registry_initials.t](../../tests/cli/options/matching/0278_option_matching_lookup_registry_initials.t) |

### Coverage boundary

The focused tests exercise unique, ambiguous, key/value-boundary, environment, and compatibility-pinned cases. The matcher also permits multiple aliases with one semantic value; registry validation and review must preserve that mapping when aliases change.
