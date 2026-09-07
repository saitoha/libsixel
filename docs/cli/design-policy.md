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

Use suboptions for policy specific to a named loader, quantizer, diffusion method,
lookup policy, colorspace component, or similar subsystem. A suboption should
have a typed domain, one canonical name, documented aliases only when needed,
and an explicit environment representation if environment configuration is
supported.

Every public suboption must define both an uppercase ASCII one-letter short
form and an environment variable in its registered definition. If the short-form
namespace cannot represent another setting, reduce or separate the option axis
instead of introducing a long-only suboption.

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

## Input and output contracts

- Keep SIXEL, raster, JSON, or other machine-readable output free of progress
  messages and helper chatter.
- Send status and error diagnostics to standard error.
- Define stdin and stdout behavior explicitly, including conflicts between
  binary input, terminal input, and option arguments.
- Treat file overwriting, output creation, partial-output cleanup, and broken
  pipes as designed behaviors.
- Keep `--help`, `--version`, and error exit statuses stable and scriptable.

## Automated coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and a corresponding static check or
test.
The reciprocal `Policy:` reference in each check or test is enforced by
`staticcheck-doc-test-links`.

| ID | Design contract | Static check or test |
| --- | --- | --- |
| CLI-01 | Every public top-level option has a one-character short form and a long form with the same argument shape. | [tests/_static/sh/staticcheck-suboption-registry.sh](../../tests/_static/sh/staticcheck-suboption-registry.sh) |
| CLI-02 | Public suboptions have typed values, uppercase one-letter forms, environment forms, and image-level coverage. | [tests/_static/sh/staticcheck-suboption-registry.sh](../../tests/_static/sh/staticcheck-suboption-registry.sh) |
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

### Coverage boundary

The structural checks cover the option hierarchy, short and long forms,
argument shape, typed suboptions, environment exposure, help, the manual, and
completion. Behavioral tests cover representative parsing, precedence,
diagnostics, and binary I/O contracts. Decisions about whether a released
name may be removed, whether a new name is clear, and whether a short form is
memorable require design review because they cannot be inferred from the
current source tree alone.
