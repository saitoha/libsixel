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
| Numerical expectation data | Stored color certificates and source-value tables agree with an independent mathematical derivation. | Exact rational matrix calculations, binary32 rounding-cell checks, and exhaustive UINT16 normalization and polarity checks. |
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
PATH="$PWD/.local/bin:$PATH" make -j8 staticcheck
```

The Meson build exposes the same umbrella suite through its run target:

```sh
meson compile -C build staticcheck
```

The suite schedules independent named checks through recursive Make and prints complete logs in deterministic registration order, followed by a concise pass, skip, and fail summary. `make staticcheck` runs serially; `make -j8 staticcheck` uses Make's shared eight-job budget, and `make -j1 staticcheck` explicitly selects serial execution. ShellCheck and codespell retain bounded file batches, whose recursive Make jobs share the same jobserver with the suite and other build targets. No independent CPU-count detection or `STATICCHECK_JOBS` override is used. Direct shell and Meson run-target invocations execute the same checks serially because they have no Make jobserver. Every scheduled check is accounted for, including after another check fails; missing worker results fail the suite. Workers share an immutable source-contract inventory, use separate logs, and never reuse results from previous invocations. A failing check makes the umbrella target fail. Some checks require a configured header or built introspection binary; those checks report a skip when the prerequisite is absent, so a run with skips is not evidence for the skipped contract. Optional external lint tools are also reported as skips when unavailable. Use a normal configured build with the documented toolchain when claiming full validation.

For a TAP-producing static check, only standard output is parsed as TAP; standard error is retained separately for diagnostics. The dispatcher requires one leading or trailing plan, consecutive explicit or implicit result numbers, and a result count matching the plan. Missing or malformed results, unknown stdout records, bailout, an unterminated diagnostic block, or a nonzero child status fail the check. A mixture of successful and skipped results passes; only an entirely skipped check is reported as skipped. A failed ordinary result still fails when another result is skipped. TODO does not establish a static invariant and is rejected in this suite. This staticcheck policy does not alter the ordinary behavioral TAP harness.

Failure output retains the original TAP result lines, child standard error, and parser diagnostics. Shell verbose/trace output can contain literal `not ok`, `1..N`, or `Bail out!` strings; because it belongs on standard error, those strings cannot turn a successful TAP stream into a failed or skipped result.

The registry image-coverage check accepts equivalent escaped-pipe shell patterns and complete CMS record assertions without weakening them to partial wildcard checks. It still requires both typed assignments and actual CMS application. Captures must use unique local artifact names and be overwritten by shell `>` or the converter `-o` option, which opens with `O_TRUNC`; append redirection and missing capture destinations are rejected. The converter-owned output path also avoids GNV text-record limits without weakening the image assertions. Short/environment output equivalence can use exact byte comparison or a checked MS-SSIM 0.98 comparison for documented nondeterministic output. Tests whose contract is option routing may instead assert typed values, actual consumers, nonempty output and exact short/environment output equivalence. Time-budget CLI evidence checks the value consumed by presentation under a deterministic pass cap, while injected-clock tests cover deadline behavior. Their explicit `Image assertion: option-values-and-output-equivalence` marker does not claim independent pixel correctness; the checker still rejects missing assignments, consumers, comparisons and nonempty-output checks. Tests that claim pixel correctness additionally need a quality comparison or an independently specified literal pixel reference encoded through the strict loader path. The [test-design rules](guide.md#image-quality-tests) determine which observation is appropriate.

The suite collects source-owned `Staticcheck:`, `Policy:` and `Test-plan:` references in one shared scan. Full files are read so misplaced declarations after line 20 still fail their owning checks. The reference outputs are immutable for the duration of that invocation; focused document checks use the same collector with their own scope. Header/modeline validation reads each tracked source once, retaining the first declaration and last matching line positions instead of launching per-file text pipelines. Git-file worktrees receive the same validation as ordinary checkouts.

During development, an individual Autotools check can be run from the test directory, for example:

```sh
make -C tests staticcheck-docs-bash-vs-zsh-completion
```

The umbrella target remains the completion criterion because a focused check does not expose interactions with other repository invariants.

The color expectation audits also have individual targets:

```sh
make -C tests staticcheck-png-color-oracles
make -C tests staticcheck-tiff-uint16-oracles
meson compile -C build staticcheck-png-color-oracles
meson compile -C build staticcheck-tiff-uint16-oracles
```

Both targets run the [independent oracle auditor](../../tools/verify_color_oracles.py) with the configured host Python interpreter and only its standard library. The PNG check independently derives the stored cHRM exact values, error bounds, and binary32 rounding certificates. The TIFF check covers every UINT16 normalization and MINISWHITE complement, then the separate-plane source tables for orientations 1–4. These checks require neither optional image libraries nor a CMS or built converter; a missing Python interpreter is reported as a skip. A mismatch fails the target and the umbrella suite. They verify stored expectations, while loader value assertions and final-image MS-SSIM regressions remain separate runtime observations.

## Adding or changing a staticcheck

1. State one repository invariant and identify its authoritative and derived representations. Do not start from a preferred parsing trick.
2. Normalize only syntax that is irrelevant to the contract. Retain names, argument shapes, scopes, or ordering whenever the policy treats them as meaningful.
3. Prefer a small deterministic check under `tests/_static/`; use a temporary artifact for regeneration or comparison and avoid mutating tracked files.
4. Give the check a `staticcheck-<subject>` name and add it to [`staticcheck-suite.sh`](../../tests/_static/sh/staticcheck-suite.sh), which is the canonical umbrella ordering used by both build systems.
5. Keep the Autotools, tracked `Makefile.in`, Meson run target, and distribution lists synchronized when the check is exposed individually.
6. Add reciprocal `Policy: docs/...` comments when the check enforces a durable subject policy document, and list the check in that subject document's coverage inventory. Do not add every check to this overview; reserve this document's own coverage inventory for checks that maintain staticcheck itself or this documentation contract.
7. Run the focused check, `staticcheck-doc-test-links` when policy links changed, `staticcheck-test-plan-links` when exhaustive suite inventories changed, the full `make staticcheck`, and behavioral tests appropriate to any product code changed alongside the invariant.

Do not hard-code numbered test filenames in a checker to require behavioral coverage or test existence. Keep coverage in the owning document's stable coverage IDs and reciprocal `Policy:` links, or use a marked `test-plan` inventory when exhaustive membership is the invariant; [documentation-to-test traceability](../AGENTS.md#documentation-to-test-traceability) defines those formats. When a property is mechanically observable, scan test contents for that property instead of selecting a test by filename. Runtime comparisons and verdicts belong to the behavioral test; static checks must not require a particular variable name or assertion spelling merely to duplicate its verdict. A rename updates document links and build registration where applicable, while the contract ID stays stable.

A literal test filename is appropriate only when its identity is meaningful, such as an explicit exception allowlist, a fixture used to test the checker itself, or a platform compatibility ledger. Explain that reason near the beginning of the checker and fail with the missing path when a named target no longer exists. A representative behavioral example alone is not an exception to this rule.

Source-only TAP contracts that also run in the ordinary test harness declare `# Staticcheck: staticcheck-<subject>` within their first 20 lines. The umbrella suite registers the stable subject and discovers exactly one marked owner; a missing or duplicate owner fails. These contracts inspect source representations, so running them under staticcheck does not claim an image-processing observation. The PSD diagnostic mappings and k-center preference source contract use this mechanism. Their `Policy:` links below preserve documentation ownership independently of their filenames.

A staticcheck should fail with enough normalized evidence to locate the divergent representation. It must not silently rewrite committed outputs: automatic correction can be a separate developer command, but the verification target remains read-only with respect to tracked repository state.

Both documentation reciprocal checks scan PowerShell `.ps1` comments as well as the existing source languages. The [traceability policy](../AGENTS.md#documentation-to-test-traceability) defines the supported nonrecursive inventory patterns, including `????_*.ps1` for numbered cases beside shared helpers, and the requirements when adding another language. Invalid marker syntax, missing/extra files, duplicate inventory entries and one-sided references are errors. Source descriptions and evidence claims still need semantic review.

The documentation-link check also validates the [generated PNG CMS matrix](../loader/png-color-metadata.md#maintaining-the-tables) against its input chunks, committed reference bytes, test ownership, and recorded quality floors. Adding or remapping a matrix test requires reviewing and regenerating that shared table.

## Test coverage

<!-- test-coverage: enforced -->

This is deliberately not an inventory of every `staticcheck-*` case. Subject policy documents own links to the checks that enforce their contracts. This document links only the meta-checks that maintain the staticcheck mechanism and its documentation representation.

| ID | Staticcheck meta-contract | Owning check |
| --- | --- | --- |
| SC-01 | Every standalone `staticcheck-*.sh` implementation other than the runner itself is registered in the umbrella suite, the suite does not reference missing implementations, source-only TAP roles have exactly one owner and one registration, and both Autotools and Meson invoke that suite. | [tests/_static/sh/staticcheck-staticcheck-suite-sync.sh](../../tests/_static/sh/staticcheck-staticcheck-suite-sync.sh) |
| SC-02 | This document and its owning meta-checks maintain complete reciprocal links. | [tests/_static/sh/staticcheck-doc-test-links.sh](../../tests/_static/sh/staticcheck-doc-test-links.sh) |
| SC-03 | Exhaustive test-plan markers, their scoped test links, and reciprocal test comments remain complete and duplicate-free. | [tests/_static/sh/staticcheck-test-plan-links.sh](../../tests/_static/sh/staticcheck-test-plan-links.sh) |
| SC-04 | The actual TAP dispatcher preserves failure diagnostics and rejects failed, missing, malformed, misnumbered, prematurely terminated, or bailed-out results while distinguishing mixed success/skip from all-skip. | [tests/_static/sh/staticcheck-tap-dispatch.sh](../../tests/_static/sh/staticcheck-tap-dispatch.sh) |
| SC-05 | Registry image-evidence inspection accepts supported assertion forms but detects missing assignments, wrong bindings, missing consumers, missing output comparisons, and missing independent image references. Mutation controls validate whole-corpus completeness once, then inspect each changed body in a fresh process with the real registries; unchanged fixtures must pass focused inspection, and missing, empty or extra inputs must fail. | [tests/_static/sh/staticcheck-suboption-image-coverage.sh](../../tests/_static/sh/staticcheck-suboption-image-coverage.sh) |

| SC-06 | PSD source retains the deferred solid-overlay and split diagnostic mappings. | [tests/loader/builtin/1464_loader_builtin_psd_psdtools_blend_and_clipping_clip_weighted_deferred_solid_overlay_trace.t](../../tests/loader/builtin/1464_loader_builtin_psd_psdtools_blend_and_clipping_clip_weighted_deferred_solid_overlay_trace.t) |
| SC-07 | PSD source retains the deferred miter stroke-adjust diagnostic, adjustment gate, and coverage-join call. | [tests/loader/builtin/1578_loader_builtin_psd_psdtools_effects_stroke_composite_vector_stroke_adjust_deferred_trace.t](../../tests/loader/builtin/1578_loader_builtin_psd_psdtools_effects_stroke_composite_vector_stroke_adjust_deferred_trace.t) |
| SC-08 | The k-center constraint source contains the auto/perceptual/OKLab hybrid-preference comparison and dispatch contract. | [tests/quant/palette/init/0073_kcenter_auto_perceptual_oklab_hybrid_preference.t](../../tests/quant/palette/init/0073_kcenter_auto_perceptual_oklab_hybrid_preference.t) |
| SC-09 | Serial and parallel workers share Make job slots across nested lint batches and preserve argument boundaries, ordered diagnostics, pass/skip/fail accounting, checks after a failure, and rejection of missing worker results. | [tests/_static/sh/staticcheck-suite-workers.sh](../../tests/_static/sh/staticcheck-suite-workers.sh) |

| SC-10 | Shared and focused source-reference scans preserve source identities, full-file placement checks, language-specific comment rules, role-registration scope and records across batches. | [tests/_static/sh/staticcheck-source-reference-controls.sh](../../tests/_static/sh/staticcheck-source-reference-controls.sh) |
| SC-11 | Header/modeline scanning preserves the 120-line SPDX, 160-line license and final 40-line modeline windows, checks Git-file worktrees and rejects failed inventories. | [tests/_static/sh/staticcheck-c-header-modeline-controls.sh](../../tests/_static/sh/staticcheck-c-header-modeline-controls.sh) |

### Coverage boundary

The suite-registration check covers shell implementations named `staticcheck-*.sh`; inline compile probes remain explicit runner code and are not projected into a second generated inventory. The reciprocal policy-link check proves the behavioral documentation relationship, and the test-plan check proves exhaustive suite membership, but neither proves the completeness or accuracy of prose. Whether a new invariant belongs in staticcheck, whether normalization preserves the right distinctions, and whether a passing static contract has sufficient behavioral coverage remain review decisions.
