# Testing Guide

## Test strategy

Place a test at the narrowest boundary that owns the behavior. Use functional
or unit-style tests for local contracts, regression tests for previously broken
behavior, quality tests for perceptual output, static checks for repository-wide
invariants, and fuzz tests for parser and state-space exploration.

An end-to-end test is valuable when integration is the contract. It is not a
substitute for a smaller test that identifies the failing layer.

## Organization and naming

- Put every test under the most specific suitable category in `tests/`.
- Use the category's contiguous numeric sequence. Renumber registrations and
  neighboring tests when necessary to keep the sequence coherent.
- Name the file after the behavior and observation it tests.
- Rename the file when a change alters its purpose or test perspective.
- Keep one test file to one test observation.
- Prefer the smallest fixture and command that can expose the behavior.

Shell TAP tests normally declare `1..1`. A whole-file skip declares
`1..0 # SKIP`. Tests in other languages must also follow the repository's
single-observation plan policy.

## Shell TAP tests

### Required structure

Every `*.t` shell test must begin with `set -eux`. Print the TAP plan and then
enable `set -v` so CI logs show the executed source. The test must read from top
to bottom; do not define shell functions.

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

The TAP stream, not the process status, reports an assertion failure. A `*.t`
test must exit with status 0 after emitting `not ok`. Build-system or test-runner
infrastructure may treat an inability to execute the TAP producer as a harness
error, but assertion logic in the test must not use `exit 1`.

### Flat control flow

- Avoid `if`, `elif`, `else`, and `case`.
- Express the test as a flat sequence: failure emits `not ok` and exits, a
  satisfied skip condition emits a TAP skip and exits, and the end emits `ok`.
- Use `command || { ...; exit 0; }` for one failure boundary.
- Do not nest `|| { ...; }` blocks.
- Use `test ...`, not `[ ... ]`.
- Prefer shell parameter expansion for simple string inspection.
- Use `case` only when unavoidable and materially simpler than spawning an
  external matcher.

### Process and file cost

Tests run on Windows and other process-expensive environments. Treat every fork
and file operation as a cost.

- Avoid `dirname` and `basename`; use `${0%/*}` and `${0##*/}`.
- Avoid subshells and multi-stage pipelines.
- Do not use Python in shell tests.
- Current static policy rejects `grep`, `awk`, and `sed` in `tests/**/*.t`;
  use shell operations or a focused compiled helper.
- Do not use `cp` or `rm` unless the test specifically observes that file
  operation and no cheaper design exists. Do not create arbitrary directories.
- Do not create a log file only to inspect stderr.
- Capture only output that is part of the assertion.
- Send irrelevant output to `/dev/null`.
- Do not create an output file when its contents will not be verified.
- Keep variables and temporary artifacts to the minimum needed for the single
  observation.

### Artifact directory ownership

The harness exports an isolated `ARTIFACT_LOCAL_DIR` path but does not create
the directory. A test that needs output artifacts creates it exactly once,
after early skip checks and after the TAP plan and `set -v`:

```sh
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
```

This is the narrow exception to the normal rule against directory operations.
Tests that do not create artifacts must not create the directory, and a test
that exits through an early feature skip must do so before this line.

### TAP purity

Only TAP belongs on standard output. Redirect output from tested programs and
helpers when it would contaminate the TAP stream. Diagnostic context for a
failed assertion should be prefixed or sent to standard error without changing
the planned observation count.

Skip output must explain the unavailable capability. A missing optional
dependency is a skip only when that feature-disabled configuration is supported.

## Image-quality tests

Follow [Quality Measurement Policy](../quality/measurement-policy.md). In
particular:

- use `images/snake.png` at approximately 16 by 16 pixels for ordinary `lsqa`
  tests;
- use approximately 64 by 64 pixels for dithering-quality tests;
- prepare alpha/transparency cases with ImageMagick;
- normally omit `-p` and exercise the 256-color default;
- use `MS-SSIM:0.98` as the normal gate;
- never lower a quality threshold below `0.95`;
- assert semantic properties separately from perceptual similarity.

## C tests

- Follow the repository C style: English comments, 80-column source lines, C99
  with K&R braces, and all local variables declared at function scope at the
  beginning of the function.
- Use the project's allocation, I/O, environment, and compatibility wrappers
  when they own cross-platform behavior.
- Avoid direct CRT-sensitive calls such as `getenv()` or `fopen()` where static
  policy requires a libsixel wrapper.
- Keep setup, one observation, and cleanup focused on the contract.
- Register test-runner defines in every applicable normal and amalgamated path.

## Portability

- Quote expansions and use POSIX shell syntax unless a category explicitly
  targets another shell.
- Use harness-provided `TOP_SRCDIR`, `TOP_BUILDDIR`, `ARTIFACT_LOCAL_DIR`,
  executable paths, feature flags, and runtime prefix.
- Do not assume GNU-only flags, `/proc`, a case-sensitive filesystem, executable
  suffixes, or Unix path separators unless the test is platform-specific.
- Keep optional-feature detection explicit and early.
- Avoid timing-sensitive sleeps. Prefer an observable readiness or completion
  condition.

## Registration

Register a test in every build path that supports its feature. Depending on the
category, this may include:

- `tests/Makefile.am`;
- intentionally tracked generated files such as `tests/Makefile.in`;
- `tests/meson.build`;
- test-runner source lists and build defines;
- amalgamation-specific test defines;
- distribution lists and fixture lists.

Keep numbering, filenames, registrations, build defines, and test purpose in
sync. Run the existing static checks that enforce these relationships.

## Required validation

Put project-local Autotools, Meson, and related tools first in `PATH`.

Run ShellCheck without errors, warnings, or informational diagnostics:

```sh
PATH="$PWD/.local/bin:$PATH" \
ARTIFACT_LOCAL_DIR=$PWD TOP_SRCDIR=$PWD \
find tests -type f -name \*.t -exec shellcheck -x -P "$PWD" {} +
```

Run the project static suite, which checks rules generic ShellCheck does not
know:

```sh
PATH="$PWD/.local/bin:$PATH" make staticcheck
```

Run the focused test while developing. Before handing off a completed source or
test change, run the full suite:

```sh
PATH="$PWD/.local/bin:$PATH" make check
```

Run `git diff --check` before committing. Use `make distcheck` when a change
affects distribution contents, generated files, install behavior, or release
packaging.

## Failure interpretation

- An assertion mismatch is a product or test-contract failure.
- A TAP producer that cannot start is a harness/configuration error.
- A missing intentionally optional feature is a skip.
- A timeout, package download error, or unavailable runner may be infrastructure
  failure, but confirm it from the actual log before classifying it.
- A generated test name missing from the current build may indicate stale
  configuration; investigate regeneration before changing the assertion.

Do not weaken assertions, replace expected output, or broaden skips until the
failure's owning boundary is understood.
