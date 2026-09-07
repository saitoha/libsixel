# Command-Line Interface Design Policy

## Scope

This document defines the design policy for libsixel command-line interfaces,
principally `img2sixel` and `sixel2png`. The CLI is a public compatibility
surface, not a thin test wrapper around the C library.

Accepted spellings, defaults, precedence, diagnostics, output streams, and exit
status are part of the interface contract.

## Ownership and sources of truth

- Extend the existing option and suboption registries instead of creating an
  independent parser or duplicate option table.
- Prefer one typed definition for names, aliases, value domains, defaults, and
  validation. Derive other surfaces from it or add static checks against drift.
- Keep public option constants, converter parsing, help text, manual pages,
  shell completion, environment variables, bindings, and tests synchronized.
- Require every public environment control to have exactly one registry owner.
  Consumers select typed bindings or accessors and do not copy environment
  variable spellings or call a generic name-based resolver.
- Preserve Autotools, Meson, and amalgamation parity when an option needs a new
  source file, public constant, test define, or generated input.

Before adding an option, identify which layer owns the behavior. Process-level
policy belongs in a converter. Reusable image or protocol behavior belongs in a
library abstraction. Loader-specific behavior belongs in a typed loader
suboption rather than a global special case.

Library option scopes describe semantic encoder or decoder consumers only.
They must not name concrete executables such as `img2sixel` or `sixel2png`.
Each converter owns the projection from those semantic options to its getopt,
help, manual, and shell-completion surfaces. This keeps intentional short-name
reuse, such as encoder and decoder `-d`, separate without teaching the library
which executable exposes either option.

## Naming

### Long options

- Use clear domain-oriented names that describe behavior rather than internal
  implementation.
- Reuse established libsixel and `img2sixel` terminology.
- Keep related names structurally consistent.
- Avoid names that become false if the implementation changes.

### Short options

Add a short option only when it is memorable, unambiguous, and worth preserving
indefinitely. The short-option namespace is scarce and compatibility-sensitive.
Do not copy a flag from another tool without verifying its current meaning in
this repository.

### Suboptions

Use suboptions for policy owned by a named loader, quantizer, diffusion method,
lookup policy, colorspace component, or similar subsystem. A suboption should
have a typed domain, one canonical name, documented aliases only when needed,
and an explicit environment representation if environment configuration is
supported.

Every public suboption must define both an uppercase ASCII one-letter short
form and an environment variable in the owning registry row. If the short-form
namespace cannot represent another setting, reduce or separate the option axis
instead of introducing a long-only suboption.

`SIXEL_OPTION_ARGUMENT_LIST` defines argument cardinality. The option value
schema and suboption registry together define the list item type. Consumers
must enumerate that existing schema instead of introducing a parallel list
option table.

When one option exposes suboptions for multiple named bases, `--help` and the
manual must show the exact primary environment mapping as
`scope:suboption=VARIABLE`. Derive the expected pairs from the typed registry;
do not infer environment names from a prefix convention. This is especially
important for list options, where each item can select a different base and
therefore a different environment namespace.

### Internal test controls

Fault injection and test-only implementation switches are not public options
or public environment contracts. Put them in the `_SIXEL_TEST_*` namespace,
declare their value kind in the typed internal environment broker, and expose
only semantic accessors to consumers. Keep these names out of public help,
manual pages, shell completion, and language bindings. Do not add a generic
consumer API that accepts an arbitrary environment variable name.

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

- Reuse the common choice matching, numeric parsing, validation, and diagnostic
  machinery.
- Avoid one-off `strtol`, prefix matching, fuzzy matching, or value-list code
  when a shared parser already owns the contract.
- Reject trailing garbage, overflow, underflow, out-of-range values, invalid
  combinations, and unavailable backend choices deterministically.
- Validate at the layer that has enough context to report the responsible
  option and invalid value.
- Never continue with partially initialized option state after a parse error.

Test exact names, supported aliases, unique-prefix behavior where applicable,
invalid values, ambiguous values, duplicates, and conflicts.

## Defaults and precedence

Every configurable value needs one identifiable default and a documented
precedence order. Unless an established interface defines another order, use:

1. command-line value;
2. environment value;
3. built-in default.

An empty public environment value is equivalent to an unset value unless an
older documented contract explicitly requires otherwise. Public boolean
suboptions accept only `0` and `1` in both CLI and environment forms.

Cover precedence with focused tests. A test should distinguish an explicit
default from an unset value when downstream behavior treats them differently.
Do not distribute the same default among the parser, help text, manual page,
and backend implementation without a synchronization mechanism.

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
- Redirect helper output away from TAP when a CLI is invoked by a test.

## Documentation synchronization

A public CLI change may require updates to:

- the owning option or suboption registry;
- converter parsing and help generation;
- installed public headers;
- `converters/*.1` manual pages;
- `converters/shell-completion/`;
- documented environment variables;
- language-binding constants or wrappers;
- Autotools, Meson, and amalgamation inputs;
- matching, rejection, precedence, and regression tests;
- static checks that compare these surfaces.

Do not assume all surfaces are generated. Inspect the current build and static
checks before deciding which files are authoritative and which are derived.

## Change checklist

1. Identify the owning layer and current source of truth.
2. Define the name, type, domain, default, precedence, and failure behavior.
3. Decide compatibility and migration behavior.
4. Update parsing and the library-facing implementation.
5. Update help, manuals, completion, environment documentation, and bindings.
6. Add focused success, rejection, ambiguity, duplicate, precedence, and
   regression tests as applicable.
7. Synchronize both build systems and tracked generated inputs.
8. Run option/documentation static checks, then the full test suite.
