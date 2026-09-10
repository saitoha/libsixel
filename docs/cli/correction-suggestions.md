# CLI Correction-Suggestion Policy

## Scope and purpose

This document defines the diagnostic assistance that helps a user correct an invalid libsixel command. It covers ambiguous-prefix candidate lists, fuzzy name suggestions, and nearby-path suggestions. These features are suggestions, not automatic correction: accepted syntax is defined by exact and [prefix matching](prefix-matching.md), and an invalid command remains unsuccessful even when the intended spelling appears obvious.

The converter CLIs enable human-friendly name guidance because a dense image-processing vocabulary is error-prone to type and because a precise correction reduces repeated trial and error. The library does not impose that presentation on embedding applications. `lsqa` reuses parts of the shared matcher and diagnostic machinery, but it does not generally expose the converter's `-x MODE[:SUBOPTION...]` control surface.

## Three independent facilities

| Facility | Converter control | Environment form | Default for converter CLI | Cost controlled separately |
| --- | --- | --- | --- | --- |
| Ambiguous-prefix candidates | `-x human:prefix_suggestions=0|1` or `:P0|:P1` | `SIXEL_OPTION_PREFIX_SUGGESTIONS` | enabled | Extra human prose and the compatibility-visible candidate set |
| Fuzzy name suggestions | `-x human:fuzzy_suggestions=0|1` or `:F0|:F1` | `SIXEL_OPTION_FUZZY_SUGGESTIONS` | enabled | CPU and allocation for edit-distance ranking |
| Nearby-path suggestions | `-x human:path_suggestions=0|1` or `:S0|:S1` | `SIXEL_OPTION_PATH_SUGGESTIONS` | disabled | Directory enumeration, filesystem latency, and path disclosure |

The controls use the same [suboption and environment precedence](suboptions.md#environment-variable-parity) as other converter settings: an explicit `-x` suboption overrides its environment form. Prefix and fuzzy suggestions receive converter-only enabled defaults through `sixel_option_apply_cli_suggestion_defaults()`; path lookup remains opt-in. This keeps interactive help useful without making every failed path probe enumerate a directory.

## Ambiguous-prefix candidates

The [prefix matcher](prefix-matching.md) already knows the spellings matched by an ambiguous token. When prefix suggestions are enabled, human diagnostics append that candidate set to the `ambiguous prefix` error. For example, `-d st` can explain that it matches both `stucki` and `stbn`. When disabled, the ambiguity is still reported and the command still fails, but the candidate list is omitted.

Code diagnostic mode suppresses the human-oriented matched-name list while retaining the stable `AMBIGUOUS_PREFIX` category and detail. Automation should branch on the category rather than parse candidate prose whose contents can evolve with the registry.

## Fuzzy name suggestions

Fuzzy correction is consulted only after declared choice matching has found no accepted prefix. The implementation compares the invalid token case-insensitively with every prefix that the normal matcher would accept, then projects a winning prefix back to its canonical choice name. This matters for names whose shortest safe interactive spelling is shorter than the canonical name and ensures the suggestion engine does not recommend an ambiguous abbreviation.

[`sixel_option_collect_choice_suggestions()`](../../src/options.c) applies deterministic bounds:

- normalized Levenshtein similarity must be at least `0.6`;
- edit distance must be at most `2`;
- an accepted prefix of three characters or fewer must have distance at most `1`;
- duplicate canonical names are collapsed to their best-scoring occurrence;
- candidates sort by higher similarity, smaller edit distance, shorter canonical name, then lexical order;
- at most five canonical names are emitted.

The same bounded comparison is used for close suboption-key errors, so `-Q kmeans:inittpye=pca` can report `Did you mean: inittype?`. The typo remains an unknown key and the converter exits with status 2. Code diagnostic mode suppresses fuzzy candidate prose while retaining the error category.

The time cost is proportional to the finite candidate vocabulary and the edit-distance matrices for the compared strings. Choice tables and keys are small, candidates are capped, and the work runs only on invalid input, so valid conversion throughput and output quality are unaffected. Allocation failure merely removes optional guidance; it must not turn invalid syntax into valid syntax or destabilize the error path.

## Nearby-path suggestions

Path correction is separate because it observes external state. When enabled after a local path cannot be resolved, the implementation enumerates the containing directory, normalizes platform path syntax, scores nearby entries, and emits up to five paths. Remote-looking targets and unavailable or missing directories follow dedicated bypass or fallback paths rather than being interpreted as local candidates.

Ranking uses a weighted score:

```text
0.55 * filename_similarity + 0.25 * extension_match + 0.20 * recency
```

Filename similarity is case-insensitive normalized Levenshtein similarity, extension equality provides a format-oriented tiebreaker, and modification time favors recent entries. The separate Windows and POSIX directory walkers produce the same ranked contract while respecting their native path separators and enumeration APIs.

Path suggestions are disabled by default because directory enumeration can add noticeable latency on network or very large directories and can disclose filenames that were not present in the command. Enabling them is an explicit user or launcher choice. Unlike ambiguous and fuzzy name candidates, path suggestions can accompany a stable code diagnostic because the opt-in itself requests filesystem guidance; automation that needs a minimal and non-probing error path should leave `SIXEL_OPTION_PATH_SUGGESTIONS=0`.

## User experience and safety

Suggestion text is written to standard error so it cannot corrupt SIXEL, PNG, JSON, or other machine-readable standard output. Candidate generation must use only initialized registry data or deliberately enumerated paths, must be deterministically bounded, and must never include unrelated memory. A correction must name the domain that failed—base, suboption key, suboption value, or path—because a plausible word from the wrong domain is more confusing than no suggestion.

No facility changes image pixels, palette selection, encoded bytes, or successful-command performance. Its quality effect is limited to error recovery: users reach the intended valid command with fewer attempts, while scripts continue to receive failure status for malformed input. Automatically executing a guess is intentionally excluded because it could select a materially different conversion and turn a diagnostic heuristic into an undocumented alias.

## Implementation map

- [`src/options.c`](../../src/options.c) owns choice matching, fuzzy ranking, ambiguity formatting, normalized Levenshtein scoring, and platform path candidate collection.
- [`src/options-registry.c`](../../src/options-registry.c) owns the three diagnostic controls, their environment names, their scopes, and converter default resolution.
- [`converters/img2sixel.c`](../../converters/img2sixel.c) and [`converters/sixel2png.c`](../../converters/sixel2png.c) enable converter suggestion defaults and translate parse failures into CLI diagnostics.
- The converter help tables, manual pages, and shell completion expose the controls; the [suboption architecture](suboptions.md) defines how their long, compact, and environment forms remain equivalent.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| SUG-01 | Ambiguous-prefix candidate lists can be disabled without changing failure status or the ambiguity error. | [tests/cli/options/matching/0008_option_matching_prefix_ambiguous_suggestions_off.t](../../tests/cli/options/matching/0008_option_matching_prefix_ambiguous_suggestions_off.t) |
| SUG-02 | Fuzzy suggestions are enabled by default for converter CLI use and do not make a distance-two typo succeed. | [tests/cli/options/matching/0014_option_matching_fuzzy_suggestions_default_enabled.t](../../tests/cli/options/matching/0014_option_matching_fuzzy_suggestions_default_enabled.t) |
| SUG-03 | Fuzzy suggestions can be disabled without changing rejection behavior. | [tests/cli/options/matching/0009_option_matching_distance2_fuzzy_off.t](../../tests/cli/options/matching/0009_option_matching_distance2_fuzzy_off.t) |
| SUG-04 | A close suboption-key typo is rejected and suggests its canonical key. | [tests/cli/options/matching/0276_option_matching_suboption_key_typo_suggestion.t](../../tests/cli/options/matching/0276_option_matching_suboption_key_typo_suggestion.t) |
| SUG-05 | Path suggestions remain opt-in and emit ranked nearby paths after a local lookup failure. | [tests/cli/options/matching/0010_option_matching_path_suggestions_enabled.t](../../tests/cli/options/matching/0010_option_matching_path_suggestions_enabled.t) |
| SUG-06 | Code diagnostics retain the ambiguity category while suppressing the human candidate list. | [tests/cli/options/matching/0236_option_matching_diag_mode_code.t](../../tests/cli/options/matching/0236_option_matching_diag_mode_code.t) |
| SUG-07 | Explicit diagnostic suboptions override their environment defaults. | [tests/cli/options/regression/0147_diagnostics_prefix_suggestions_image_regression.t](../../tests/cli/options/regression/0147_diagnostics_prefix_suggestions_image_regression.t), [tests/cli/options/regression/0148_diagnostics_fuzzy_suggestions_image_regression.t](../../tests/cli/options/regression/0148_diagnostics_fuzzy_suggestions_image_regression.t), [tests/cli/options/regression/0149_diagnostics_path_suggestions_image_regression.t](../../tests/cli/options/regression/0149_diagnostics_path_suggestions_image_regression.t) |

### Coverage boundary

The tests cover defaults, explicit disabling, failure status, key correction, code-mode output, path opt-in, and environment precedence. Filesystem timing and the sensitivity of candidate names are platform and directory dependent; the policy controls those risks through opt-in behavior and bounded output rather than a universal latency threshold.
