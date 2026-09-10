# Converter Suboption Architecture

## Scope

This document defines the structured suboption architecture used principally by `img2sixel` and `sixel2png`. It explains how a converter option selects a base policy, how subordinate settings are registered, parsed, validated, loaded from the environment, and applied, and why the interface has both descriptive and compact spellings. The broader reasons for choosing this design over a flat option namespace belong to the [CLI design policy](design-policy.md).

`lsqa` does not adopt the converter suboption architecture as its general option model. It does, however, reuse the decoder's `-d` parser for dequantization, so the limited forms `lso_undither:Vfs`, `lso_undither:Vlight`, and `selective_blur:threshold=N` are intentionally accepted there. This is shared decoder policy, not evidence that every `lsqa` option participates in the converter registry.

## Why subordinate settings are structured

A base option such as `-Q` answers a first-order question: which palette construction family is requested? Parameters such as the k-means initialization strategy only make sense after that family is known. Giving every parameter a flat top-level option would repeat the subsystem name, consume the finite one-character namespace, and make it harder to reject a setting used with the wrong algorithm. Nesting keeps the option close to its owner and lets the active base determine the valid domain.

This is a retrospective description of the current system, not a declaration that it is ideal from first principles. Had the eventual option count been foreseeable in 2014, the project could have implemented the missing `getopt_long()` compatibility layer for Interix then, made descriptive long options primary, and limited short aliases to frequent operations. Structured suboptions become reasonably systematic with experience, but their colon grammar and context-dependent compact letters can surprise new and occasional users. They are retained as a released compatibility contract, so help, manuals, completion, exact diagnostics, and environment alternatives must carry more of the discoverability burden.

The architecture is intentionally not a general command language. Structured arguments select policy for a fixed conversion pipeline; their textual order does not schedule arbitrary processing. The crop/resize exception remains governed by the [CLI design policy](design-policy.md#crop-and-resize-order-exception).

## Grammar

A structured single-value argument has the form:

```text
BASE[:SUBOPTION...]
```

Each suboption entry uses exactly one of two forms:

```text
name=value
Kvalue
```

The long key is its complete canonical name followed by `=` and a nonempty value. The compact key is exactly one registered uppercase ASCII letter followed immediately by a nonempty value. It does not contain `=`, and lowercase letters do not enter the compact-key namespace.

For example, these invocations select the same k-means initialization setting:

```sh
img2sixel -Q kmeans:inittype=pca input.png
img2sixel -Q kmeans:Ipca input.png
```

The corresponding environment form is explicit registry data rather than a name inferred from either spelling:

```sh
SIXEL_PALETTE_KMEANS_INITTYPE=pca img2sixel -Q kmeans input.png
img2sixel --env SIXEL_PALETTE_KMEANS_INITTYPE=pca -Q kmeans input.png
```

Colon-separated entries may be repeated. A schema can select one base or an ordered base list; each list item is parsed against the same declared item schema. The parser preserves a colon that belongs to a Windows drive prefix in a value, so a path such as `C:\path` is not split as another suboption merely because the grammar uses `:`.

## Exact keys and matched values

Long suboption keys require exact spelling. `scan=serpentine` is valid, while `sca=serpentine` is an unknown key. The compact spelling is not an automatically generated abbreviation: `Nserpentine` works for diffusion scan only because `N` is registered for that scoped setting. This separation prevents today's convenient key prefix from blocking a future canonical key.

Choice-valued bases and suboption values may use the separately defined [prefix-matching policy](prefix-matching.md). Thus `-d fs:scan=ser` may accept `ser` as the value `serpentine`, even though `sca` cannot abbreviate the key `scan`. Strings, numbers, paths, and structured values use their own typed parser and do not acquire choice-prefix behavior.

## Registry model

[`src/options-registry.c`](../../src/options-registry.c) is the authoritative table of structured option rows. The public metadata represented by each row includes:

- the owning option schema and optional base or base set;
- the encoder or decoder consumer scope;
- the canonical long key and uppercase compact key;
- the primary environment name and any documented fallback or legacy names;
- the value kind, choices, range, and environment-specific compatibility rules;
- the typed destination binding and diagnostic text.

[`src/options.h`](../../src/options.h) defines the supported value kinds: choice, ordered choice list, boolean, signed and unsigned integers, size, float, double, integer pair, scaled byte, string, and nested structured value. It also separates the consumer scope from the storage target. That distinction allows an option spelling shared by encoder and decoder frontends to expose only the rows valid for the active consumer and to store a validated result in the encoder, decoder, loader, dequantizer, runtime, diagnostics, or clipboard state that owns it.

A row with no base restriction is common to every base in its schema. A base-specific row is visible only when that base is active. Compact letters may therefore be reused by unrelated schemas or disjoint base domains without becoming globally ambiguous.

## Parse and apply lifecycle

[`sixel_option_parse_argument_with_suboptions()`](../../src/options.c) follows a deliberately staged path:

1. Validate the registry and consumer scope before interpreting the argument.
2. Split and match the base in the schema's declared choice domain.
3. Enumerate only the suboption rows visible to that base and consumer.
4. Classify each entry as exact long `name=value` or registered compact `Kvalue` syntax.
5. Parse the value according to the row's type, rejecting empty values, trailing garbage, overflow, underflow, out-of-range values, unknown choices, and invalid combinations.
6. Store typed assignments in an intermediate resolution instead of mutating live converter state during parsing.
7. Load applicable environment defaults into the owning state, then apply explicit CLI assignments so the command line wins.

Keeping parsing and application separate prevents a malformed suffix from leaving partially updated state. The typed resolution also provides a canonical base and value spelling for downstream code without requiring algorithms to reparse user text.

## Environment-variable parity

Every configurable public option and suboption must have an alternative environment representation. This is part of the CLI contract for launchers, CI, applications that cannot conveniently assemble compact command syntax, and users who need process-scoped defaults. Pure actions such as help and version and positional input or output targets are not configurable settings.

The normal precedence is:

1. explicit command-line setting;
2. environment setting;
3. built-in default.

`--env NAME=VALUE` and its short form `-% NAME=VALUE` set the environment channel for one process invocation; they do not outrank an explicit option or suboption. A primary environment name, fallback name, legacy name, accepted aliases, range behavior, or case behavior is registry metadata because released environment contracts sometimes differ from strict new CLI syntax. A present but invalid primary value does not silently fall through to a legacy typo, while a documented shared fallback can still provide a process default.

Help and manuals must publish the primary mapping, including its scope when one structured option has multiple bases. The environment name is not derived mechanically from the CLI spelling. [`staticcheck-suboption-registry.sh`](../../tests/_static/sh/staticcheck-suboption-registry.sh) audits the registry-backed converter mappings and their required image-level coverage, and [`staticcheck-docs-envvars-help-table.sh`](../../tests/_static/sh/staticcheck-docs-envvars-help-table.sh) audits their help exposure. The rule applies to the whole public CLI design even where equivalent structural enforcement has not yet been added.

## Diagnostics and failure behavior

An invalid base, key, and value are distinct failures because each has a different valid domain. Diagnostics identify the owning option and active base where applicable. Unknown keys may list the keys valid in that scope, and the independent [correction-suggestion policy](correction-suggestions.md) may add a close canonical spelling. Neither a candidate list nor a typo suggestion changes failure into success.

Converters report malformed option syntax with exit status 2. Parsing errors occur before conversion consumes partially initialized policy. Stable code diagnostics retain an error category such as `UNKNOWN_SUBOPTION_KEY`; human diagnostics may include explanatory prose.

## Compatibility and extension rules

A base name, long key, compact key, environment name, accepted value, scope, default, and precedence rule are all compatibility surface. When adding or changing a row:

1. Confirm that the setting is subordinate to the owning base or subsystem rather than an independent pipeline stage.
2. Reconsider whether a descriptive long top-level option would be clearer; historical nesting is not sufficient justification by itself.
3. Choose a canonical behavior name that remains true if the implementation changes.
4. Allocate an uppercase compact key that is unambiguous within every scope where the row is visible.
5. Define the type, range, default, repeat behavior, consumer scope, target binding, and primary environment name together.
6. Audit [choice prefixes](prefix-matching.md) if a finite choice domain changes.
7. Update runtime help, manuals, Bash and Zsh completion, registry validation, positive and negative parsing tests, CLI-over-environment precedence tests, and image-level behavior coverage.

Do not add a compatibility alias merely to rescue an unpublished typo. Once released, however, an alias or legacy environment spelling cannot be removed or reused without a migration plan.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| SUB-01 | Registry-backed public options and suboptions have valid forms, types, scopes, environment names, bindings, and required image-level coverage. | [tests/_static/sh/staticcheck-suboption-registry.sh](../../tests/_static/sh/staticcheck-suboption-registry.sh) |
| SUB-02 | Public environment mappings remain represented in converter help. | [tests/_static/sh/staticcheck-docs-envvars-help-table.sh](../../tests/_static/sh/staticcheck-docs-envvars-help-table.sh) |
| SUB-03 | A long-form suboption sequence is accepted in its active base. | [tests/cli/options/matching/0086_option_matching_quantize_kmeans_histogram_long_success.t](../../tests/cli/options/matching/0086_option_matching_quantize_kmeans_histogram_long_success.t) |
| SUB-04 | Registered uppercase compact suboptions are accepted without `=`. | [tests/cli/options/matching/0087_option_matching_quantize_kmeans_histogram_short_success.t](../../tests/cli/options/matching/0087_option_matching_quantize_kmeans_histogram_short_success.t) |
| SUB-05 | A compact key followed by `=` is rejected. | [tests/cli/options/matching/0233_option_matching_quantize_short_suboption_equals_rejected.t](../../tests/cli/options/matching/0233_option_matching_quantize_short_suboption_equals_rejected.t) |
| SUB-06 | A long key requires exact spelling rather than key-prefix matching. | [tests/cli/options/matching/0026_option_matching_loader_suboption_prefix_rejected.t](../../tests/cli/options/matching/0026_option_matching_loader_suboption_prefix_rejected.t) |
| SUB-07 | An unknown key is rejected with the keys valid for the active base. | [tests/cli/options/matching/0112_option_matching_quantize_medoids_unknown_key_lists_candidates.t](../../tests/cli/options/matching/0112_option_matching_quantize_medoids_unknown_key_lists_candidates.t) |
| SUB-08 | Typed numeric parsing rejects a value outside the registered range. | [tests/cli/options/matching/0193_option_matching_quantize_center_seed_overflow_rejected.t](../../tests/cli/options/matching/0193_option_matching_quantize_center_seed_overflow_rejected.t) |
| SUB-09 | Explicit suboption values take precedence over their environment defaults. | [tests/cli/options/matching/0101_option_matching_quantize_medoids_algo_env_cli_precedence.t](../../tests/cli/options/matching/0101_option_matching_quantize_medoids_algo_env_cli_precedence.t) |
| SUB-10 | Registry rows are visible only to their declared encoder or decoder consumer. | [tests/cli/core/0031_suboption_consumer_scope.t](../../tests/cli/core/0031_suboption_consumer_scope.t) |
| SUB-11 | `lsqa` deliberately reuses the decoder suboption parser for selective dequantization. | [tests/cli/core/0027_basic_lsqa_dequantize_selective_blur_compact.t](../../tests/cli/core/0027_basic_lsqa_dequantize_selective_blur_compact.t) |

### Coverage boundary

The tests cover registry structure and representative syntax, type, scope, precedence, environment, converter behavior, and the narrow `lsqa` reuse. They do not decide whether a proposed setting belongs under an existing base or deserves a top-level option; that architectural judgment is reviewed against the [CLI design policy](design-policy.md).
