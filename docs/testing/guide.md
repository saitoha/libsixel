# Testing Guide

## Test strategy

For discovery, C dispatch, build-system execution, and command-length limits, see [Test Harness and Command-Length Limits](../build/test-harness.md).

Place a test at the narrowest boundary that owns the behavior. Use functional or unit-style tests for local contracts, regression tests for previously broken behavior, quality tests for perceptual output, [staticcheck](staticcheck.md) for repository-wide invariants and test meta-checks, and fuzz tests for parser and state-space exploration.

An end-to-end test is valuable when integration is the contract. It is not a substitute for a smaller test that identifies the failing layer.

## Assertions and evidence

Choose assertions from the contract being tested. Value and state assertions establish numeric results, metadata, representation, branch selection and error behavior. Perceptual metrics establish a stated image-quality floor; they do not substitute for those assertions or for safety checks. The [image-quality measurement policy](../quality/measurement-policy.md) owns metric selection, acceptance thresholds and image-quality comparison conditions.

For a numerical-correctness claim, derive expected values from the documented contract and an independent reference, such as literal source samples or a separately derived formula. Agreement between two paths that share the same converter or expected-value generator establishes consistency, not independent correctness. State which kind of evidence a test provides. A regression test should fail against the faulty behavior it is intended to detect.

Use exact comparisons for properties the contract makes exact, such as copied samples, dimensions, indices, masks and format tags. An integer result derived from floating-point arithmetic also needs the quantization analysis below; its storage type alone does not justify exact comparison. For numerical results, document the reference, error metric and tolerance at the owning numeric contract. Do not relax a tolerance merely to accept current output.

When implementation selection is part of the observation, verify the path that actually executed, not only the requested configuration. An unintended backend, engine or fallback must not satisfy a test for another path. Test an intentional fallback as its own behavior. Likewise, protect a no-conversion or no-allocation contract with an observable trace or counter; correct output alone does not establish that unnecessary work was avoided.

Fixture size and execution cost follow [organization and naming](#organization-and-naming) and [process and file cost](#process-and-file-cost). Subject documents supply their own numeric contracts, reference values, tolerances and supported execution paths, and link here for these general assertion rules.

### Intermediate floating-point error

An independent reference establishes what is being computed; it does not require bit-identical floating-point results. Unless the contract explicitly requires exact arithmetic or representation preservation, allow justified differences caused by intermediate rounding, excess evaluation precision, fused multiply-add, and math-library approximations across supported builds. Float32 storage does not imply float32 intermediate arithmetic, nor does a double intermediate make the whole calculation exact. Do not disable otherwise supported arithmetic merely to reproduce one host's output bits.

Derive an error budget for the tested input domain and observation boundary. Account for source-sample and coefficient quantization, intermediate arithmetic and cancellation, nonlinear approximation, final storage rounding, and reference uncertainty. Specify whether the reference starts from ideal source values or the exact stored inputs; if it uses ideal values, include the input-representation error. Propagate errors through the intervening operations rather than adding errors expressed in different coordinate units. Separate external-library approximation from adapter arithmetic where both contribute; a generous CMS budget must not conceal a wrong matrix, normalization or rounding operation in the adapter.

For finite numerical results, a common component-wise comparison is:

```text
abs(actual - expected) <= atol + rtol * max(abs(actual), abs(expected))
```

Document the units, input range, chosen absolute and relative terms, and their rationale. A justified absolute-only or relative-only comparison is also valid. A ULP bound must identify the compared representation and its treatment of zero and subnormals; it is not a universal substitute for an error budget. Near zero or after cancellation, relative error or scaling by the largest input can conceal the loss of the entire expected residual. Use a bound at the result's meaningful scale, and check required nonzero values or signs separately when necessary. Neither `FLT_EPSILON` alone nor the largest error observed on one machine is a sufficient pipeline-wide budget.

Check finiteness explicitly before a finite-value comparison: `abs(actual - expected) > tolerance` alone can accept NaN. Test required rejection of NaN, infinity, domain errors and overflow separately from approximation accuracy. Exact byte preservation, including signed zero or NaN representation when the contract requires it, remains an exact assertion; do not replace those tests with approximate equality.

Record relevant floating-point environment assumptions. Tests of ordinary arithmetic do not implicitly promise support for every `fesetround()` mode, reassociation under unsafe compiler options, or flush-to-zero behavior. Tests that change the environment must restore it. If a tie-breaking rule or rounding mode is itself the contract, test it explicitly with controlled representable inputs; do not infer it from a long transform whose intermediates may already have rounded.

### Quantization after floating-point arithmetic

Exact integer output is justified when the propagated error interval, including reference uncertainty, stays inside one quantization bin. Record the distance to the nearest decision boundary in the same units as the error budget. This applies to byte or palette rounding and to threshold-based masks or branch decisions, even if the intermediate value is never exposed by the API.

When the interval crosses a boundary, derive the admissible discrete results from that interval and the specified quantizer, and check the pre-quantization value as well where observable. Do not grant every pixel an unconditional `+/-1` allowance. If the contract instead promises correctly rounded output for every input, boundary cases must enforce that stronger promise; inability to meet it is an implementation issue. Exact ties belong to a separate test of the specified tie-breaking rule.

Keep numerical portability and defect detection together: the accepted interval must still reject the wrong transform, premature clipping, lost residual, or extra low-precision store that the test targets. Independence of the oracle and passage on one architecture do not by themselves establish a portable exact-result assertion. [Pixel-format precision](../concepts/pixelformat-precision/README.md#numerical-validation-follows-the-operation) records the frame-specific tolerances and boundary evidence.

## Organization and naming

- Put every test under the most specific suitable category in `tests/`.
- Use the category's contiguous numeric sequence. Renumber registrations and neighboring tests when necessary to keep the sequence coherent.
- Name the file after the behavior and observation it tests.
- Rename the file when a change alters its purpose or test perspective.
- Keep one test file to one test observation.
- Prefer the smallest fixture and command that can expose the behavior.

Shell TAP tests normally declare `1..1`. A whole-file skip declares `1..0 # SKIP`. Tests in other languages must also follow the repository's single-observation plan policy.

## Shell TAP tests

Shell TAP files under `tests/` must be stored with Git mode `100755` and normally have filesystem mode `0755`. Set that mode when creating or generating a test. Both the Autotools and Meson harnesses explicitly invoke a shell, so a passing test through either harness does not detect missing execute permissions.

`staticcheck-test-executable` checks the current source tree without a legacy exception list. On POSIX it checks the owner execute bit on regular files, including uncommitted changes and allowing normal umask differences; when Git cannot track filesystem modes (`core.filemode=false`), it also checks index mode `100755`. On native Windows the Git index supplies the executable mode: add new tests and use `git update-index --chmod=+x -- <path>` when necessary. Extracted source distributions use POSIX permissions; a Git-free native Windows tree reports a skip because neither source of executable metadata is available. POSIX checks are not skipped merely because `core.filemode` is false.

### Required structure

Every `*.t` shell test must begin with `set -eux`. Print the TAP plan and then enable `set -v` so CI logs show the executed source. The test must read from top to bottom; do not define shell functions.

Use this shape:

```sh
#!/bin/sh
# Verify one observable behavior.

set -eux

echo "1..1"
set -v

some_command >/dev/null || {
    echo "not ok 1 - behavior under test"
    exit 0
}

test "${observed}" = "${expected}" || {
    echo "not ok 1 - behavior under test"
    exit 0
}

echo "ok 1 - behavior under test"
exit 0
```

The TAP stream, not the process status, reports an assertion failure. A `*.t` test must exit with status 0 after emitting `not ok`. Build-system or test-runner infrastructure may treat an inability to execute the TAP producer as a harness error, but assertion logic in the test must not use `exit 1`.

### Flat control flow

- Avoid `if`, `elif`, and `else`.
- Do not use `for` or `until`. Before enumerating values, ask whether every value provides a necessary and distinct observation. A loop body tends to fork external commands once per value, which makes one test increasingly slow, while the copied value list becomes another place that must be updated whenever the accepted values change. If multiple observations are necessary, put them in separate files so each test remains small, fast, and independently reported. Do not mechanically create coverage for values that do not add a distinct test purpose.
- A `while` loop remains available when iteration itself is necessary, such as reading every line from a stream.
- Express the test as a flat sequence: failure emits `not ok` and exits, a satisfied skip condition emits a TAP skip and exits, and the end emits `ok`.
- Use `command || { ...; exit 0; }` for one failure boundary.
- Do not nest `|| { ...; }` blocks.
- Use `test ...`, not `[ ... ]`.
- Use parameter expansion to extract a value, such as `${0%/*}` or `${record##*=}`. Use `test` for exact value comparisons.
- Use `case` for pattern matching. Do not compare a prefix/suffix removal with its unchanged input to infer a match, directly or through a temporary variable. Repeated shortest-prefix probing can become quadratic in the length of a trace even though it does not fork. Bash glob/regex predicates are also prohibited. Keep `case` focused on matching rather than introducing deeply nested control flow.

`staticcheck-test-pattern-match` checks shell TAP sources, including newly added files and source distributions, for these predicate forms. It follows simple removal assignments and copies, ignores comments and heredoc payloads, and permits actual field/path extraction. This is a syntactic check, not a general shell interpreter; it does not prove the cost or correctness of arbitrary shell code. Its lexical and whitelist rejection controls run on every invocation. Python 3 is required for this check; an unavailable interpreter fails rather than silently skipping it.

An unavoidable exception requires explicit user approval **before** adding it to [the pattern-matching whitelist](../../tests/_static/data/test-pattern-match-whitelist.json). Contributors and agents must not approve their own exceptions or grandfather existing tests. Each version-1 `entries` item must identify one `path` under `tests/`, a one-based `line`, the `sha256` printed by the checker, a concrete `reason` why `case` or a focused helper cannot express the necessary observation, and an `approval` recording the user's decision and its reference. The hash covers the complete logical statement as written, including continuations but excluding its final newline. Empty reasons or approvals, wildcard/file-wide exemptions, duplicate entries, and entries that no longer match a reported violation fail the check. The checker validates the record's shape and scope; reviewers must verify that the recorded user approval exists. Changing an approved statement or moving it requires a renewed review of the entry. The whitelist starts empty.

### Process and file cost

Tests run on Windows and other process-expensive environments. Treat every fork and file operation as a cost.

- Avoid `dirname` and `basename`; use `${0%/*}` and `${0##*/}`.
- Avoid subshells and multi-stage pipelines.
- Do not use Python in shell tests.
- Current static policy rejects `grep`, `awk`, and `sed` in `tests/**/*.t`; use shell operations or a focused compiled helper.
- Do not use `cp` or `rm` unless the test specifically observes that file operation and no cheaper design exists. Do not create arbitrary directories.
- Do not create a log file only to inspect stderr.
- Capture only output that is part of the assertion.
- Send irrelevant output to `/dev/null`.
- Do not create an output file when its contents will not be verified.
- Keep variables and temporary artifacts to the minimum needed for the single observation.

### Artifact directory ownership

The harness exports an isolated `ARTIFACT_LOCAL_DIR` path but does not create the directory. A test that needs output artifacts creates it exactly once, after early skip checks and after the TAP plan and `set -v`:

```sh
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
```

This is the narrow exception to the normal rule against directory operations. Tests that do not create artifacts must not create the directory, and a test that exits through an early feature skip must do so before this line.

### TAP purity

Only TAP belongs on standard output. Redirect output from tested programs and helpers when it would contaminate the TAP stream. Diagnostic context for a failed assertion should be prefixed or sent to standard error without changing the planned observation count.

Skip output must explain the unavailable capability. A missing optional dependency is a skip only when that feature-disabled configuration is supported.

## Image-quality tests

Use the [image-quality measurement policy](../quality/measurement-policy.md) for [metrics and thresholds](../quality/measurement-policy.md#primary-and-secondary-metrics), [standard inputs](../quality/measurement-policy.md#standard-inputs) and [controlled comparisons](../quality/measurement-policy.md#controlled-comparisons). The general assertion rules are defined in [Assertions and evidence](#assertions-and-evidence).

## C tests

- Follow the repository C style: English comments, 80-column source lines, C99 with K&R braces, and all local variables declared at function scope at the beginning of the function.
- Use the project's allocation, I/O, environment, and compatibility wrappers when they own cross-platform behavior.
- Avoid direct CRT-sensitive calls such as `getenv()` or `fopen()` where static policy requires a libsixel wrapper.
- Keep setup, one observation, and cleanup focused on the contract.
- The unified `test_runner` executable is only a build and dispatch container. Keep each independently reportable observation in its own C source, runner entry, and TAP wrapper; sharing one executable does not permit unrelated observations to be combined in one C test function or source file.
- Register test-runner defines in every applicable normal and amalgamated path.

## Portability

- Quote expansions and use POSIX shell syntax unless a category explicitly targets another shell.
- Use harness-provided `TOP_SRCDIR`, `TOP_BUILDDIR`, `ARTIFACT_LOCAL_DIR`, executable paths, feature flags, and runtime prefix.
- Do not assume GNU-only flags, `/proc`, a case-sensitive filesystem, executable suffixes, or Unix path separators unless the test is platform-specific.
- Keep optional-feature detection explicit and early.
- Aborttrace TAP tests that invoke the shared abort helper must emit a whole-file skip for both `wine` and `wine64` before the normal TAP plan and before invoking the helper. Wine can leave the combined test runner alive after an aborttrace policy probe, while the converter-level regression suite covers the policy under Wine without that helper path.
- Avoid timing-sensitive sleeps. Prefer an observable readiness or completion condition.

## Registration

Register a test in every build path that supports its feature. Depending on the category, this may include:

- `tests/Makefile.am`;
- intentionally tracked generated files such as `tests/Makefile.in`;
- `tests/meson.build`;
- test-runner source lists and build defines;
- amalgamation-specific test defines;
- distribution lists and fixture lists.

Keep numbering, filenames, registrations, build defines, and test purpose in sync. Run the existing static checks that enforce these relationships.

Documentation links are part of test registration. Follow the [documentation-to-test traceability policy](../AGENTS.md#documentation-to-test-traceability) for coverage IDs, `Policy:` ownership and `Test-plan:` inventories. Update the owning document and test backlinks together, then run the applicable `staticcheck-doc-test-links` and `staticcheck-test-plan-links` checks.

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and an owning static check. The reciprocal `Policy:` reference in each check is itself verified by `staticcheck-doc-test-links`.

| ID | Contract | Owning test |
| --- | --- | --- |
| TG-01 | TAP-producing test sources declare one observation, or a whole-file skip. | [tests/_static/sh/staticcheck-test-plan-single.sh](../../tests/_static/sh/staticcheck-test-plan-single.sh) |
| TG-02 | Every shell TAP test passes ShellCheck, and global checks reject top-level `if` and shell function definitions. | [tests/_static/sh/staticcheck-shellcheck.sh](../../tests/_static/sh/staticcheck-shellcheck.sh) |
| TG-03 | Shell TAP tests do not invoke `grep`, `awk`, or `sed`. | [tests/_static/sh/staticcheck-test-no-grep-awk.sh](../../tests/_static/sh/staticcheck-test-no-grep-awk.sh) |
| TG-04 | Test-owned artifact directories are created lazily after TAP setup and early feature skips, while the harness does not create them. | [tests/_static/sh/staticcheck-artifact-local-dir-mkdir.sh](../../tests/_static/sh/staticcheck-artifact-local-dir-mkdir.sh) |
| TG-05 | Numbered test-runner C sources, shared test support and registered external support retain synchronized amalgamation build defines; include fragments and always-included production units require no standalone test guard. | [tests/_static/sh/staticcheck-test-runner-amalgamation-defines-sync.sh](../../tests/_static/sh/staticcheck-test-runner-amalgamation-defines-sync.sh) |
| TG-06 | Enforced policy documents and their listed tests carry reciprocal repository-relative links, and every coverage row has an owning test. | [tests/_static/sh/staticcheck-doc-test-links.sh](../../tests/_static/sh/staticcheck-doc-test-links.sh) |
| TG-07 | Shell TAP tests do not use `for` or `until` loops; heredoc payloads in other languages are excluded. | [tests/_static/sh/staticcheck-test-no-for-until.sh](../../tests/_static/sh/staticcheck-test-no-for-until.sh) |
| TG-08 | Aborttrace TAP tests that invoke the shared abort helper retain early whole-file skips for both Wine runtime prefixes. | [tests/_static/sh/staticcheck-aborttrace-wine-skip.sh](../../tests/_static/sh/staticcheck-aborttrace-wine-skip.sh) |
| TG-09 | Marked test-plan inventories and their complete glob-scoped test sets carry exact reciprocal links without duplicate entries. | [tests/_static/sh/staticcheck-test-plan-links.sh](../../tests/_static/sh/staticcheck-test-plan-links.sh) |
| TG-10 | Shell TAP sources retain executable filesystem or Git modes on the supported checkout and distribution paths. | [tests/_static/sh/staticcheck-test-executable.sh](../../tests/_static/sh/staticcheck-test-executable.sh) |
| TG-11 | Shell pattern predicates use `case`; only exact, current entries with a recorded user approval can exempt a predicate. | [tests/_static/sh/staticcheck-test-pattern-match.sh](../../tests/_static/sh/staticcheck-test-pattern-match.sh) |

### Coverage boundary

These checks enforce the repository-wide invariants stated in their contract rows. The test-plan link check proves set completeness and reciprocity, not whether an inventory description accurately characterizes the assertion. Assertion choice, reference independence, numerical error budgets, actual-path verification, naming quality, fixture minimality, whether separate values provide distinct observations, and the necessity of file or process work still require review because they cannot be inferred reliably from syntax alone. Other syntactic rules in this guide are requirements even where the current static suite does not yet enforce them; the table must not be read as claiming broader mechanical coverage than its rows state.

## Required validation

Put project-local Autotools, Meson, and related tools first in `PATH`.

Run ShellCheck without errors, warnings, or informational diagnostics:

```sh
PATH="$PWD/.local/bin:$PATH" \
ARTIFACT_LOCAL_DIR=$PWD TOP_SRCDIR=$PWD \
find tests -type f -name \*.t -exec shellcheck -x -P "$PWD" {} +
```

Run the project [staticcheck suite](staticcheck.md), which is libsixel's broader-than-lint target for repository-wide invariants, cross-surface synchronization, IDL and generated artifacts, and test meta-checks:

```sh
PATH="$PWD/.local/bin:$PATH" make staticcheck
```

Run the focused test while developing. Before handing off a completed source or test change, run the full suite:

```sh
PATH="$PWD/.local/bin:$PATH" make check
```

Run `git diff --check` before committing. Use `make distcheck` when a change affects distribution contents, generated files, install behavior, or release packaging.

## Failure interpretation

- An assertion mismatch is a product or test-contract failure.
- A TAP producer that cannot start is a harness/configuration error.
- A missing intentionally optional feature is a skip.
- A timeout, package download error, or unavailable runner may be infrastructure failure, but confirm it from the actual log before classifying it.
- A generated test name missing from the current build may indicate stale configuration; investigate regeneration before changing the assertion.

Do not weaken assertions, replace expected output, or broaden skips until the failure's owning boundary is understood.
