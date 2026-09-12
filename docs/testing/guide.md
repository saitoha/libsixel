# Testing Guide

## Test strategy

For discovery, C dispatch, build-system execution, and command-length limits, see [Test Harness and Command-Length Limits](../build/test-harness.md).

Place a test at the narrowest boundary that owns the behavior. Use functional or unit-style tests for local contracts, regression tests for previously broken behavior, quality tests for perceptual output, [staticcheck](staticcheck.md) for repository-wide invariants and test meta-checks, and fuzz tests for parser and state-space exploration.

An end-to-end test is valuable when integration is the contract. It is not a substitute for a smaller test that identifies the failing layer.

## Organization and naming

- Put every test under the most specific suitable category in `tests/`.
- Use the category's contiguous numeric sequence. Renumber registrations and neighboring tests when necessary to keep the sequence coherent.
- Name the file after the behavior and observation it tests.
- Rename the file when a change alters its purpose or test perspective.
- Keep one test file to one test observation.
- Prefer the smallest fixture and command that can expose the behavior.

Shell TAP tests normally declare `1..1`. A whole-file skip declares `1..0 # SKIP`. Tests in other languages must also follow the repository's single-observation plan policy.

## Shell TAP tests

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
- Prefer shell parameter expansion for simple string inspection.
- Use `case` for shell-native pattern matching when it avoids spawning `grep` or another external matcher. Keep it focused on that matching purpose rather than using it to introduce deeply nested control flow.

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

Follow [Quality Measurement Policy](../quality/measurement-policy.md). In particular:

- use `images/snake.png` at approximately 16 by 16 pixels for ordinary `lsqa` tests;
- use approximately 64 by 64 pixels for dithering-quality tests;
- prepare alpha/transparency cases with ImageMagick;
- normally omit `-p` and exercise the 256-color default;
- use `MS-SSIM:0.98` as the normal gate;
- never lower a quality threshold below `0.95`;
- assert semantic properties separately from perceptual similarity.

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

Behavioral policy documents with an enforced coverage inventory also require reciprocal document/test path references. Follow the [documentation-to-test traceability policy](../AGENTS.md#documentation-to-test-traceability) and run `staticcheck-doc-test-links` after changing either side.

Use a reciprocal `Policy:` reference only when the test directly proves a documented user-visible contract. Parser robustness cases, malformed-input probes, resource-limit checks, and implementation-specific failure paths should remain discoverable through a separately described defensive test category unless the policy document promises their exact observable behavior. In that case, promote the behavior to a stable coverage ID and add the reciprocal direct link. A suite-directory link is an index of defensive breadth, not a substitute for contract-level coverage.

When a suite has a marked exhaustive test-plan inventory, add its exact test link to the delimited inventory and add the reciprocal `Test-plan: docs/...` comment near the beginning of the test. `staticcheck-test-plan-links` compares the marker glob, inventory links, and comments as complete sets. Use both `Policy:` and `Test-plan:` when a test is simultaneously public-contract evidence and a member of the exhaustive suite inventory.

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and an owning static check. The reciprocal `Policy:` reference in each check is itself verified by `staticcheck-doc-test-links`.

| ID | Contract | Owning test |
| --- | --- | --- |
| TG-01 | TAP-producing test sources declare one observation, or a whole-file skip. | [tests/_static/sh/staticcheck-test-plan-single.sh](../../tests/_static/sh/staticcheck-test-plan-single.sh) |
| TG-02 | Every shell TAP test passes ShellCheck, and global checks reject top-level `if` and shell function definitions. | [tests/_static/sh/staticcheck-shellcheck.sh](../../tests/_static/sh/staticcheck-shellcheck.sh) |
| TG-03 | Shell TAP tests do not invoke `grep`, `awk`, or `sed`. | [tests/_static/sh/staticcheck-test-no-grep-awk.sh](../../tests/_static/sh/staticcheck-test-no-grep-awk.sh) |
| TG-04 | Test-owned artifact directories are created lazily after TAP setup and early feature skips, while the harness does not create them. | [tests/_static/sh/staticcheck-artifact-local-dir-mkdir.sh](../../tests/_static/sh/staticcheck-artifact-local-dir-mkdir.sh) |
| TG-05 | C test-runner sources remain synchronized with amalgamation build defines. | [tests/_static/sh/staticcheck-test-runner-amalgamation-defines-sync.sh](../../tests/_static/sh/staticcheck-test-runner-amalgamation-defines-sync.sh) |
| TG-06 | Enforced policy documents and their listed tests carry reciprocal repository-relative links, and every coverage row has an owning test. | [tests/_static/sh/staticcheck-doc-test-links.sh](../../tests/_static/sh/staticcheck-doc-test-links.sh) |
| TG-07 | Shell TAP tests do not use `for` or `until` loops; heredoc payloads in other languages are excluded. | [tests/_static/sh/staticcheck-test-no-for-until.sh](../../tests/_static/sh/staticcheck-test-no-for-until.sh) |
| TG-08 | Marked test-plan inventories and their complete glob-scoped test sets carry exact reciprocal links without duplicate entries. | [tests/_static/sh/staticcheck-test-plan-links.sh](../../tests/_static/sh/staticcheck-test-plan-links.sh) |

### Coverage boundary

These checks enforce the repository-wide invariants stated in their contract rows. The test-plan link check proves set completeness and reciprocity, not whether an inventory description accurately characterizes the assertion. Naming quality, fixture minimality, whether separate values provide genuinely distinct observations, and the necessity of file or process work still require review because they cannot be inferred reliably from syntax alone. Other syntactic rules in this guide are requirements even where the current static suite does not yet enforce them; the table must not be read as claiming broader mechanical coverage than its rows state.

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
