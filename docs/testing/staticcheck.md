# Staticcheck: Repository Invariant Verification

## Role in libsixel development

`staticcheck` is the libsixel name for the repository-wide invariant-verification target. The term is project-specific: it must not be confused with a particular third-party linter or with the narrower idea of checking one source file without executing it. Linters are one input to `staticcheck`, but they are not its boundary.

As the codebase grows and the test inventory has reached roughly 5,000 cases, successful execution of those tests is not enough to preserve quality. A test can pass while its registration is missing from another build system, a generated header is stale, a CLI option has disappeared from a manual, a binding constant no longer matches the C API, or the test itself violates the repository's one-observation rules. `staticcheck` is the quality control layer that makes those forms of drift fail explicitly. It is therefore a maintenance lifeline for the codebase and test suite, not optional cosmetic lint.

The umbrella target accepts any deterministic check of the repository's static contract. In this context, “static” means that the observation concerns source, metadata, generated or configured artifacts, build structure, interface inventories, or the structure of tests rather than the result quality of a normal image-processing scenario. A check may parse C with an AST, regenerate a file into a temporary location, compile a warning probe, inspect symbols, or execute a built program's `-H` introspection path. The implementation technique does not determine the category; the invariant being checked does.

## Scope

The target deliberately spans more than conventional lint:

| Check family | What it protects | Representative mechanism |
| --- | --- | --- |
| External lint and syntax | Shell, workflow, spelling, and language syntax accepted by maintained tools. | ShellCheck, actionlint, codespell, and Python compile checks. |
| Source and architecture boundaries | Forbidden dependencies, private includes, exported symbols, component ownership, and guarded implementation paths. | Text inspection, C AST queries, symbol inspection, and strict compile probes. |
| IDL, schema, and generated artifacts | A declarative source remains authoritative and every derived representation is current. | Regenerate into a temporary file and compare, or compare registry projections. |
| Cross-surface interface consistency | CLI help, manuals, completion, public headers, bindings, build descriptions, and platform metadata describe the same contract. | Normalize each representation and compare complete inventories. |
| Test meta-checks | Tests themselves follow plan, structure, registration, portability, artifact, and source-list rules. | Inspect the test corpus and its Autotools, Meson, runner, and amalgamation registrations. |
| Documentation traceability | Durable policy claims link to their evidence, while exhaustive suite inventories keep every defensive and supplementary test discoverable without turning it into a public contract. | Compare enforced behavioral inventories with reciprocal `Policy:` comments and marked test-plan inventories with reciprocal `Test-plan:` comments. |

IDL-based verification and tests about how tests are written are both first-class `staticcheck` work. The category is intentionally broad because a growing repository accumulates quality risks between files and systems, while ordinary language linters usually see only one representation at a time.

## Relationship to behavioral tests

`make staticcheck` and `make check` answer different questions. `staticcheck` asks whether the repository is internally well-formed and whether duplicated or generated representations agree. `make check` asks whether built software behaves correctly for concrete inputs, platforms, and feature configurations. Neither subsumes the other.

This distinction is especially important for the test suite itself. Running every behavioral test cannot prove that an unregistered test was included, that a TAP file declares exactly one observation, that the same runner cases exist in normal and amalgamated builds, or that a policy document lists all of its evidence. Those are meta-properties of the suite and belong in `staticcheck`.

Static comparison also cannot prove that synchronized prose is clear, that an option is a good design, or that a C `setopt` implementation produces the intended image. Structural parity prevents accidental divergence; focused behavioral tests and review establish meaning.

## CLI option propagation as a staticcheck contract

The [CLI design policy](../cli/design-policy.md#cli-driven-contract-propagation) treats a public converter option as the change origin for several representations. `staticcheck` turns the mechanically observable portion of that policy into a chain of invariants:

```text
option registry and public optflag
              |
              v
parser -> -H output -> manual -> Bash completion -> Zsh completion
   |                                                   |
   +---------------- C and binding surfaces -----------+
```

The checks compare the complete top-level option inventory of `img2sixel`, including punctuation short forms, across runtime `-H`, its manual, and the Bash and Zsh completion files that the repository ships. `sixel2png` has a corresponding `-H`-to-manual check; it has no completion edge because the repository does not ship completion files for that program. Registry checks cover structured option metadata and its applicable help, manual, environment, and public-header projections. Binding checks compare configured `SIXEL_OPTFLAG_*` names with Ruby, Perl, Python, and PHP constants and keep binding test coverage for loader `setopt` cases aligned.

This chain does not turn one file into the generated source for every surface. It establishes an equivalence class with explicit owners and catches drift at review time. A new representation must either join the chain through a static check or document why its semantics cannot be compared mechanically.

## Running the target

Put repository-local tools first in `PATH`, build the configured tree, and run the umbrella target:

```sh
PATH="$PWD/.local/bin:$PATH" make staticcheck
```

The Meson build exposes the same umbrella suite through its run target:

```sh
meson compile -C build staticcheck
```

The suite runs named checks in a deterministic order and prints a concise pass, skip, and fail summary. A failing check makes the umbrella target fail. Some checks require a configured header or built introspection binary; those checks report a skip when the prerequisite is absent, so a run with skips is not evidence for the skipped contract. Optional external lint tools are also reported as skips when unavailable. Use a normal configured build with the documented toolchain when claiming full validation.

During development, an individual Autotools check can be run from the test directory, for example:

```sh
make -C tests staticcheck-docs-bash-vs-zsh-completion
```

The umbrella target remains the completion criterion because a focused check does not expose interactions with other repository invariants.

## Adding or changing a staticcheck

1. State one repository invariant and identify its authoritative and derived representations. Do not start from a preferred parsing trick.
2. Normalize only syntax that is irrelevant to the contract. Retain names, argument shapes, scopes, or ordering whenever the policy treats them as meaningful.
3. Prefer a small deterministic check under `tests/_static/`; use a temporary artifact for regeneration or comparison and avoid mutating tracked files.
4. Give the check a `staticcheck-<subject>` name and add it to [`staticcheck-suite.sh`](../../tests/_static/sh/staticcheck-suite.sh), which is the canonical umbrella ordering used by both build systems.
5. Keep the Autotools, tracked `Makefile.in`, Meson run target, and distribution lists synchronized when the check is exposed individually.
6. Add reciprocal `Policy: docs/...` comments when the check enforces a durable subject policy document, and list the check in that subject document's coverage inventory. Do not add every check to this overview; reserve this document's own coverage inventory for checks that maintain staticcheck itself or this documentation contract.
7. Run the focused check, `staticcheck-doc-test-links` when policy links changed, `staticcheck-test-plan-links` when exhaustive suite inventories changed, the full `make staticcheck`, and behavioral tests appropriate to any product code changed alongside the invariant.

A staticcheck should fail with enough normalized evidence to locate the divergent representation. It must not silently rewrite committed outputs: automatic correction can be a separate developer command, but the verification target remains read-only with respect to tracked repository state.

## Test coverage

<!-- test-coverage: enforced -->

This is deliberately not an inventory of every `staticcheck-*` case. Subject policy documents own links to the checks that enforce their contracts. This document links only the meta-checks that maintain the staticcheck mechanism and its documentation representation.

| ID | Staticcheck meta-contract | Owning check |
| --- | --- | --- |
| SC-01 | Every standalone `staticcheck-*.sh` implementation other than the runner itself is registered in the umbrella suite, the suite does not reference missing implementations, and both Autotools and Meson invoke that suite. | [tests/_static/sh/staticcheck-staticcheck-suite-sync.sh](../../tests/_static/sh/staticcheck-staticcheck-suite-sync.sh) |
| SC-02 | This document and its owning meta-checks maintain complete reciprocal links. | [tests/_static/sh/staticcheck-doc-test-links.sh](../../tests/_static/sh/staticcheck-doc-test-links.sh) |
| SC-03 | Exhaustive test-plan markers, their scoped test links, and reciprocal test comments remain complete and duplicate-free. | [tests/_static/sh/staticcheck-test-plan-links.sh](../../tests/_static/sh/staticcheck-test-plan-links.sh) |

### Coverage boundary

The suite-registration check covers shell implementations named `staticcheck-*.sh`; inline compile probes remain explicit runner code and are not projected into a second generated inventory. The reciprocal policy-link check proves the behavioral documentation relationship, and the test-plan check proves exhaustive suite membership, but neither proves the completeness or accuracy of prose. Whether a new invariant belongs in staticcheck, whether normalization preserves the right distinctions, and whether a passing static contract has sufficient behavioral coverage remain review decisions.
