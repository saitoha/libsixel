# Command-Line Interface Design Policy

## Scope

This document defines the design policy for libsixel command-line interfaces,
principally `img2sixel` and `sixel2png`. The CLI is a public compatibility
surface, not a thin test wrapper around the C library.

Accepted spellings, defaults, precedence, diagnostics, output streams, and exit
status are part of the interface contract.

## Option structure

The converters expose two levels of controls. Top-level options select major
operations or broadly applicable behavior. Typed suboptions group controls
that belong to a loader, quantizer, diffusion method, lookup method, or another
named subsystem. Choose the level from the user's mental model of the feature,
then apply the naming and compatibility rules below.

A structured option argument has this general form:

```text
BASE[:SUBOPTION...]
```

The long suboption form is `name=value`. Every public suboption also has a
compact form consisting of one uppercase ASCII letter followed immediately by
the value: `Kvalue`. For example, `-Q kmeans:inittype=pca` and
`-Q kmeans:Ipca` select the same setting. The compact form does not use `=`,
and lowercase letters do not enter the compact-key namespace.

Some suboptions apply to every base of the enclosing option, while others are
valid only for particular bases. A compact letter therefore has meaning in
the context of its top-level option and active base; the same letter may be
used by unrelated options or by disjoint bases. List-valued options apply the
same grammar to each list item.

Every public option or suboption that represents a configurable setting must
have a corresponding environment-variable form. Pure actions such as showing
help and positional input or output targets are not configuration settings.
The environment name is part of the public interface and must be listed by
`-H` and the manual; it is not inferred by mechanically capitalizing the CLI
name. Multiple CLI routes to the same setting may intentionally share one
environment variable. `--env NAME=VALUE` supplies that environment form for
one converter invocation; it does not define a separate setting.

## Naming

### Long options

- Use clear domain-oriented names that describe behavior rather than internal
  implementation.
- Reuse established libsixel and `img2sixel` terminology.
- Keep related names structurally consistent.
- Avoid names that become false if the implementation changes.

### Short options

Every public top-level option of `img2sixel` and `sixel2png` must have both a
one-character short form and a long form. Long-only top-level options are not
permitted. Keep every top-level option within the short-option namespace, and
choose a memorable, unambiguous character that is worth preserving
indefinitely. Do not copy a flag from another tool without verifying its
current meaning in this repository.

Requiring a short form is a libsixel compatibility convention, not a
limitation of the current parser. The repository provides a local
`getopt_long()` fallback where the platform does not provide one. The rule is
retained so that the public CLI remains expressible with traditional
single-character option syntax.

The namespace is converter-local because encoder and decoder flags are parsed
by different programs. If no suitable character remains in that converter,
first consider expressing the behavior as a typed suboption of an existing
top-level option. Do not bypass namespace exhaustion by assigning a synthetic
integer flag to a long-only option.

### Suboptions

Use suboptions for policy specific to a named loader, quantizer, diffusion
method, lookup policy, colorspace component, or similar subsystem. A
suboption has a typed domain, one canonical long name, one uppercase compact
name, and an explicit environment representation. Retain additional aliases
only when required by a released compatibility contract.

Every public suboption must define both an uppercase ASCII one-letter short
form and an environment variable in its registered definition. If the short-form
namespace cannot represent another setting, reduce or separate the option axis
instead of introducing a long-only suboption.

Long suboption names require exact `name=value` spelling. Do not accept a
prefix of a long key because it could become ambiguous when another setting is
added. The compact `Kvalue` spelling is the only abbreviated key form. Choice
values within either spelling may use the prefix rules described below.

List-valued options must define the type of every item they accept. Their help
and diagnostics must describe the same base values and suboptions.

When one option exposes suboptions for multiple named bases, `--help` and the
manual must show the exact primary environment mapping as
`scope:suboption=VARIABLE`. This is especially important for list options,
where each item can select a different base and therefore a different
environment namespace.

### Internal test controls

Fault injection and test-only implementation switches are not public options
or public environment contracts. Their names use the `_SIXEL_TEST_*`
namespace and do not appear in public help, manual pages, shell completion, or
language bindings.

## Compatibility

- Do not silently change a default.
- Do not reuse an existing flag for a different meaning.
- Do not remove an accepted name or value without an explicit compatibility or
  deprecation plan.
- Treat unique prefixes and aliases as compatibility behavior once documented
  or covered by intentional tests.
- Keep library and converter compatibility separate. A CLI convenience must not
  silently change defaults for library embedders.

When a behavior must change, document the old behavior, the new behavior, the
migration path, and which releases or interfaces are affected.

## Parsing and validation

- Apply consistent rules for choice matching, numeric syntax, validation, and
  diagnostics across options.
- Reject trailing garbage, overflow, underflow, out-of-range values, invalid
  combinations, and unavailable backend choices deterministically.
- Validate at the layer that has enough context to report the responsible
  option and invalid value.
- Never continue with partially initialized option state after a parse error.

### Choice values and prefix abbreviations

Only arguments whose registered type permits prefix matching may be
abbreviated. This includes structured-option base names and choice-valued
suboption values. It does not include long suboption keys, arbitrary strings,
paths, or numeric values.

Choice matching follows this order:

1. A complete choice name is an exact match and wins immediately.
2. Otherwise, a nonempty prefix is accepted when every matching spelling maps
   to the same semantic value. This permits a unique prefix and also permits
   compatible aliases that share a value.
3. A prefix matching different semantic values is ambiguous and is rejected.
4. A token matching no accepted prefix is unknown and is rejected.

For example, `-s ave` resolves to `average`, while a value such as `-d st` is
rejected when it matches both `stucki` and `stbn`. Similarly,
`scan=ser` may abbreviate `scan=serpentine`, but `sca=serpentine` is not a
valid abbreviation of the `scan` suboption key.

Command-line and environment matching rules are declared separately. A CLI
choice accepting prefixes does not imply that its environment form accepts
the same prefixes or aliases. Environment values normally use the stricter
spellings documented in `-H` and the manual.

## Defaults and precedence

Every configurable value needs one identifiable default and a documented
precedence order. Unless an established interface defines another order, use:

1. command-line value;
2. environment value;
3. built-in default.

An empty public environment value is equivalent to an unset value unless an
older documented contract explicitly requires otherwise. Public boolean
suboptions accept only `0` and `1` in both CLI and environment forms.

An explicit value equal to the default remains distinct from an unset value
when downstream behavior depends on whether the user made a choice.

## Diagnostics and suggestions

libsixel intentionally treats diagnostics as part of CLI design rather than
as incidental parser errors. The intended experience is maximally helpful,
not terse: users should be guided from an invalid command toward the valid
spellings and choices available in its exact context. The matching and
suggestion machinery deliberately accepts the resulting complexity. This
helpfulness must remain deterministic and must never make an invalid token
silently succeed.

- Diagnostics must identify the option and invalid input precisely.
- Suggestions are diagnostics, not parsing. A suggestion must not turn invalid
  input into a successful command.
- Keep prefix, fuzzy-name, and path suggestions independently controllable when
  the existing interface exposes them separately.
- Avoid leaking platform-specific paths, uninitialized memory, or unrelated
  candidate names.
- Send diagnostics to standard error.

Library embedders and standalone converters may use different diagnostic
defaults. Preserve that boundary.

An ambiguous accepted prefix is rejected with an `ambiguous prefix`
diagnostic. Human-readable mode lists the matched spellings when prefix
suggestions are enabled. Unknown base names, suboption keys, and suboption
values are distinguished from one another; the diagnostic lists the valid
keys or values for the active option and base.

Typo suggestions compare the invalid token with prefixes that would be valid
choices. Matching uses case-insensitive normalized Levenshtein similarity.
Candidates must have edit distance at most two and similarity of at least
0.6; candidates of at most three characters require distance one. Results are
ordered by higher similarity, lower edit distance, shorter name, and then
lexical order, with at most five names emitted after `Did you mean:`.

The diagnostics policy controls these additions independently:
`prefix_suggestions` controls candidates for ambiguous prefixes,
`fuzzy_suggestions` controls typo candidates, and `path_suggestions` controls
filesystem suggestions. Human-readable CLI diagnostics enable prefix and
fuzzy guidance by default. Code mode retains the stable error category but
omits human-oriented candidate text.

## Input and output contracts

- Keep SIXEL, raster, JSON, or other machine-readable output free of progress
  messages and helper chatter.
- Send status and error diagnostics to standard error.
- Define stdin and stdout behavior explicitly, including conflicts between
  binary input, terminal input, and option arguments.
- Treat file overwriting, output creation, partial-output cleanup, and broken
  pipes as designed behaviors.
- Keep `--help`, `--version`, and error exit statuses stable and scriptable.

### Pseudo targets

`clipboard:` is a pseudo path accepted wherever a converter accepts a
supported input or output target. As an input it reads a compatible image or
text payload from the selected clipboard backend; as an output it publishes
the generated payload. A documented format prefix can be combined with it,
for example `png:clipboard:`, to select the clipboard representation.

The `clipboard:` marker must terminate the operand and is interpreted as a
pseudo target rather than a filesystem path or remote URL. It is distinct
from the `-` stdin/stdout sentinel. Clipboard backend policy is configured by
`-y`/`--clipboard-policy`, its typed suboptions, and their corresponding
environment variables.

## Automated coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and a corresponding static check or
test.
The reciprocal `Policy:` reference in each check or test is enforced by
`staticcheck-doc-test-links`.

| ID | Design contract | Static check or test |
| --- | --- | --- |
| CLI-01 | Every public top-level option has a one-character short form and a long form with the same argument shape. | [tests/_static/sh/staticcheck-suboption-registry.sh](../../tests/_static/sh/staticcheck-suboption-registry.sh) |
| CLI-02 | Public suboptions have typed values, uppercase one-letter compact forms, environment forms, and image-level coverage. | [tests/_static/sh/staticcheck-suboption-registry.sh](../../tests/_static/sh/staticcheck-suboption-registry.sh) |
| CLI-03 | `img2sixel -H` and the manual expose the same top-level option declarations. | [tests/_static/sh/staticcheck-docs-help-vs-man.sh](../../tests/_static/sh/staticcheck-docs-help-vs-man.sh) |
| CLI-04 | The manual and Bash completion expose the same top-level option declarations. | [tests/_static/sh/staticcheck-docs-man-vs-bash-completion.sh](../../tests/_static/sh/staticcheck-docs-man-vs-bash-completion.sh) |
| CLI-05 | Public environment controls are represented in the generated help inventory. | [tests/_static/sh/staticcheck-docs-envvars-help-table.sh](../../tests/_static/sh/staticcheck-docs-envvars-help-table.sh) |
| CLI-06 | Internal test environment controls remain behind the internal environment interface. | [tests/_static/sh/staticcheck-src-no-direct-getenv.sh](../../tests/_static/sh/staticcheck-src-no-direct-getenv.sh) |
| CLI-07 | A unique accepted value prefix succeeds without an error diagnostic. | [tests/cli/options/matching/0001_option_matching_prefix_unique.t](../../tests/cli/options/matching/0001_option_matching_prefix_unique.t) |
| CLI-08 | An ambiguous value prefix is rejected with exit status 2 and a precise diagnostic. | [tests/cli/options/matching/0002_option_matching_prefix_ambiguous.t](../../tests/cli/options/matching/0002_option_matching_prefix_ambiguous.t) |
| CLI-09 | A numeric value outside its declared range is rejected. | [tests/cli/options/matching/0193_option_matching_quantize_center_seed_overflow_rejected.t](../../tests/cli/options/matching/0193_option_matching_quantize_center_seed_overflow_rejected.t) |
| CLI-10 | An explicit command-line value takes precedence over its environment default. | [tests/cli/options/matching/0246_option_matching_sampling_policy_env_cli_precedence.t](../../tests/cli/options/matching/0246_option_matching_sampling_policy_env_cli_precedence.t) |
| CLI-11 | Missing required arguments are detected before option dispatch. | [tests/cli/argument-shift/0007_cli_guard_missing_argument.t](../../tests/cli/argument-shift/0007_cli_guard_missing_argument.t) |
| CLI-12 | Fuzzy suggestions are diagnostic output and do not make invalid input succeed. | [tests/cli/options/matching/0014_option_matching_fuzzy_suggestions_default_enabled.t](../../tests/cli/options/matching/0014_option_matching_fuzzy_suggestions_default_enabled.t) |
| CLI-13 | Explicit standard input and standard output form a working binary conversion path. | [tests/cli/core/0009_basic_stdin_stdout_map64.t](../../tests/cli/core/0009_basic_stdin_stdout_map64.t) |
| CLI-14 | `png:-` writes PNG data to standard output. | [tests/cli/core/0014_basic_png_stdout.t](../../tests/cli/core/0014_basic_png_stdout.t) |
| CLI-15 | The help command remains available. | [tests/cli/core/0001_help.t](../../tests/cli/core/0001_help.t) |
| CLI-16 | The version command remains available. | [tests/cli/core/0002_version.t](../../tests/cli/core/0002_version.t) |
| CLI-17 | Uppercase compact suboption keys are accepted without `=`. | [tests/cli/options/matching/0087_option_matching_quantize_kmeans_histogram_short_success.t](../../tests/cli/options/matching/0087_option_matching_quantize_kmeans_histogram_short_success.t) |
| CLI-18 | Long suboption keys require exact spelling rather than prefix abbreviation. | [tests/cli/options/matching/0026_option_matching_loader_suboption_prefix_rejected.t](../../tests/cli/options/matching/0026_option_matching_loader_suboption_prefix_rejected.t) |
| CLI-19 | A compact suboption key followed by `=` is rejected. | [tests/cli/options/matching/0233_option_matching_quantize_short_suboption_equals_rejected.t](../../tests/cli/options/matching/0233_option_matching_quantize_short_suboption_equals_rejected.t) |
| CLI-20 | An unknown suboption key is rejected with the valid keys for the active base. | [tests/cli/options/matching/0112_option_matching_quantize_medoids_unknown_key_lists_candidates.t](../../tests/cli/options/matching/0112_option_matching_quantize_medoids_unknown_key_lists_candidates.t) |
| CLI-21 | Fuzzy typo suggestions can be disabled without changing rejection behavior. | [tests/cli/options/matching/0009_option_matching_distance2_fuzzy_off.t](../../tests/cli/options/matching/0009_option_matching_distance2_fuzzy_off.t) |
| CLI-22 | `clipboard:` and `png:clipboard:` work as input and output pseudo targets. | [tests/io/clipboard/0002_clipboard_file_backend.t](../../tests/io/clipboard/0002_clipboard_file_backend.t) |
| CLI-23 | Typed scalar options and suboptions register environment names, and public environment controls remain synchronized with help. | [tests/_static/sh/staticcheck-suboption-registry.sh](../../tests/_static/sh/staticcheck-suboption-registry.sh), [tests/_static/sh/staticcheck-docs-envvars-help-table.sh](../../tests/_static/sh/staticcheck-docs-envvars-help-table.sh) |
| CLI-24 | A close suboption-key typo is rejected with its canonical key as a `Did you mean:` suggestion. | [tests/cli/options/matching/0276_option_matching_suboption_key_typo_suggestion.t](../../tests/cli/options/matching/0276_option_matching_suboption_key_typo_suggestion.t) |

### Coverage boundary

The structural checks cover the option hierarchy, short and long forms,
argument shape, typed suboptions, environment exposure, help, the manual, and
completion. Behavioral tests cover representative parsing, precedence,
diagnostics, and binary I/O contracts. Decisions about whether a released
name may be removed, whether a new name is clear, and whether a short form is
memorable require design review because they cannot be inferred from the
current source tree alone.
