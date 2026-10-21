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

Use ordered `case` patterns such as `*"first event"*"later event"*` for trace order, and `*"event"*"event"*` for duplicate detection. Avoid `${trace#*marker*}`: shortest-prefix removal can repeatedly scan a long trace before the assertion even runs. Its final wildcard is redundant. The static check rejects that exact leading-and-trailing-wildcard form in expansions and assignments, including self-assignment, without relying on variable names or runtime lengths. This syntactic rule does not claim to detect all expensive expansions; removing the final wildcard alone is not a performance fix. Bounded field/path extraction remains permitted.

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

For GPU prerequisites and the distinction between capability skips and execution failures, see [GPU initialization and execution](#gpu-initialization-and-execution).

## Image-quality tests

Every test that measures MS-SSIM must compare images whose width and height are both at least 176 pixels, as required by the [MS-SSIM fixture policy](../quality/measurement-policy.md#standard-inputs). This applies after cropping, resizing, frame selection and any other comparison preparation. Tests specifically asserting rejection or unavailability below that boundary deliberately use smaller inputs and must check that outcome instead of a quality threshold.

Before replacing an undersized fixture, decide what the test must establish:

1. If the contract is a value, layout, mask, dimension, metadata field, option binding or exact output relationship, keep the small fixture and assert that property directly. Use independent expected values and a justified numerical error budget where necessary. A trace or two agreeing code paths alone does not prove independent pixel correctness.
2. If the contract is perceptual image quality, retain MS-SSIM and prepare a meaningful fixture with both dimensions at least 176 pixels. Preserve the feature under test, regenerate the matching independent reference and document how the fixture was produced. Verify the accepted output and a degraded control against the stated threshold. Do not pad, tile or enlarge already encoded output solely to make a measurement available, and do not replace MS-SSIM with another metric solely because its score passes.

Some loader integration fixtures intentionally request a larger encoder output. In that case, specify the output dimensions and resampling method in the test, prepare the independent reference at exactly those dimensions offline, and keep native-sample checks separate. Resampling must occur before palette construction and encoding; enlarging the measured SIXEL result is not permitted. The [`loader/quality` reference manifest](../../tests/data/loader/quality/references.json) records those source references and dimensions.

For small decoded-sample contracts, `test_runner loader/image_values reference actual loader-order tolerance` compares dimensions, finite RGB samples and the maximum absolute component error before palette quantization. The tolerance is expressed in 8-bit RGB levels. The independent PSD PPM cases allow 1.01 levels: at most half a level at each 8-bit rounding boundary, plus floating-point roundoff. The `loader/image_display_values` variant clips actual RGB components to the display range before comparison, for independently clipped color references. The `loader/image_black_values` and `loader/image_white_values` variants explicitly composite source alpha over the named background through the loader before comparing RGB samples; their controls include a wrong-background rejection. RGB sample comparisons do not establish alpha or paint-mask preservation, and clipped display comparisons do not establish out-of-gamut preservation; those require their own numeric assertions. Its controls cover independent known samples, changed samples, mismatched dimensions and an invalid nonfinite budget. An optional final argument selects a zero-based animation frame; observation is bounded to one animation cycle, and a missing frame fails rather than waiting for an infinite playback loop.

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

## Test timing

Keep each test within the recommended 30 ms of wall time. A slower test may use an explicitly approved, exact-name entry in [`tests/test-time-allowlist.tsv`](../../tests/test-time-allowlist.tsv), with its limit, reason and user approval reference. An exception must be above 30 ms and at most 100 ms; wildcards, missing approval and limits above 100 ms are rejected. Do not automatically populate the allowlist from slow results or move necessary work outside the timer to meet a budget. The checker validates the approval field's presence; review must verify the user's actual approval.

### GPU initialization and execution

A small GPU test can be dominated by host-side device discovery, command-queue creation, shader/pipeline preparation, buffer setup and synchronization. Separate these costs from upload, kernel execution, download and completed-result validation. Submission time alone does not measure asynchronous GPU completion. A slow wall-clock result does not establish GPU-core saturation or transfer-bandwidth saturation; those claims need device timings or utilization evidence at the relevant stage.

GPU palette C tests can use `test_runner --require-gpu-palette <test>` to check backend availability and execute the observation in one process. Exit 77 means the backend is unavailable; wrappers translate it to a TAP skip and propagate every other nonzero result as failure. Device and pipeline initialization stay inside the measured test interval. The real test must still dispatch GPU work and check its result; the prerequisite alone is not execution evidence. CLI tests that assert successful GPU execution and its backend trace may defer a separate availability probe until that assertion fails. Probe exit 77 permits a skip; an available backend must preserve the original conversion or trace failure. This keeps unavailable-device classification without initializing a second successful process.

Keep process-first-use initialization in the timed observation. Do not prewarm a device outside the measurement or merge independent test files merely to amortize startup and hide their cost. After redundant initialization is removed, record the remaining required startup or synchronization cost as such; do not label all GPU tests unoptimizable or grant a category-wide exception.

### Measurement boundaries and logs

The TAP driver records elapsed wall time in each `.trs` as `:duration-ms:` and the identity as `:test-name:`. It measures the process including startup and waits. Batch producers additionally emit `# test-time: NUMBER ms RESULT NAME`, retained as `:subtest-time:` records. The report separates individual observations, shared `SETUP` and enclosing `BATCH` duration. These intervals overlap and must not be summed as independent costs. A capability skip before process execution can be recorded as `unmeasured` with `SKIP`; it is not a measured fast test.

Meson keeps its native JSON Lines, duration, returncode, verdict and captured output in `meson-logs/testlog.json`. The common registrations additionally use the build-machine [`lso-timeout`](../../tools/lso-timeout.c) executor when its POSIX supervision and monotonic-clock primitives are available. Its opt-in form is `lso-timeout --record-child-time NAME SECONDS -- COMMAND...`; the existing `[-k DURATION] DURATION COMMAND...` form keeps its legacy behavior. The canonical `NAME` is passed explicitly, without a boundary label or filename inference. `SECONDS` is a nonnegative decimal number with digits before and after any decimal point, at most 86400; it is not an expression, unit-suffixed duration, infinity or NaN. Zero delegates the deadline to the caller. Meson registration uses zero so its configured timeout, command-line/setup multiplier and disabled-timeout setting remain authoritative; positive values supply an internal deadline for standalone callers. Names are nonempty, at most 4096 bytes, and contain no control characters.

The two reported boundaries overlap and must not be added or subtracted to estimate a GC or framework cost:

- `meson-outer` is the unchanged native Meson duration. It includes executor startup, native framework scheduling and TAP parsing, record delivery and executor termination.
- `child-spawn-through-output` starts at a monotonic timestamp before any supervision pipe, fork, spawn or exec. It includes the entire original command, shell and existing language/XFAIL wrappers, assertions, API/IPC, waits, cleanup, command exit/reap, and stdout/stderr EOF and complete forwarding. Backpressure is part of this interval. The final metadata record is emitted after this interval; metadata delivery and guardian/executor termination remain visible in `meson-outer`.
- Existing `recorded` observations from batch producers retain their exact identities, values and results. For a batch, both enclosing boundaries are `BATCH` context and retain the native Meson verdict separately; they do not replace the individual observations or shared `SETUP`.

The executor writes a versioned stderr comment, `# child-time: 1 NUMBER ms EXIT_CODE TIMED_OUT NAME`, only after complete forwarding and reap. `TIMED_OUT` is `0` or `1`, distinguishing an executor timeout from a command that itself exits 124. It does not parse TAP or infer PASS from exit 0. The common reader derives the functional verdict from Meson, validates version, canonical identity, finite nonnegative duration, containment in the outer interval, record cardinality and exit consistency, and never rewrites the raw JSON. The generic registration's `LSO_CHILD_TIME=1` environment metadata requires a record even when startup fails or the outer harness kills the executor. Missing or invalid evidence prints the native verdict and outer time with `child unmeasured/evidence error`; it never supplies zero, a fast pass, or a synthetic child result. Both boundaries' over-budget counts and complete slow lists remain visible, using the same exact-name allowlist independently. An outer interval is setup/batch context when a complete child interval or nested producer observations accompany it. An individual outer-only test remains an individual entry in `Slow tests`, including legacy logs and unsupported platforms.

The [POSIX supervisor](../../tools/lso-child-time.h) uses bounded buffers and nonblocking pipe forwarding. A detached guardian monitors an executor lifeline, forwards termination to the command process group, escalates after 100 ms, and kills/reaps on setup, clock, transfer or outer-death errors. The child alone creates its process group before exec, avoiding competing group creation. Exit is observed with `waitid(WNOWAIT)` while the direct child remains waitable, reserving its PID through the last process-group signal. Only then is the child reaped. If setup fails or the outer executor dies before that group exists, cleanup kills the still-owned direct child, observes its exit without reaping, signals the final group and then reaps. No group signal follows reap; a delayed drain must not target a reused numeric PID/PGID. SIGPIPE cannot bypass cleanup. A permanently blocked or broken output stream may prevent a complete record: the wrapper exits 125 and the reader reports incomplete evidence. Outer SIGKILL also leaves child evidence incomplete while preserving Meson's original outcome. Commands must not intentionally escape their process group with `setsid`/`setpgid`; this supervisor is not a security sandbox. Normal command signal exits use the existing timeout helper's shell convention, `128 + signal` (SIGTERM is 143, rather than direct Meson's raw -15); executor timeouts return 124. Meson's own outer timeout/verdict remains authoritative. No clock failure is replaced by a wall-clock estimate.

With a zero deadline and any standard descriptor attached to a terminal, the executor writes `# child-time: 1 unmeasured terminal NAME` and directly executes the original command, preserving inherited descriptors and session/job-control behavior. This terminal path has no child clock, pipe forwarding or detached guardian; the original outer harness controls termination. A positive standalone deadline continues to use captured supervision. Meson `--interactive` inherits output and reports `IGNORED` without capturing that diagnostic; versions that omit interactive JSON logs leave the native terminal report as the outer evidence. If a harness supplies a registered `IGNORED` JSON row with no captured record, the reader explicitly reports unmeasured/outer-only rather than manufacturing a child duration or TAP verdict. Normal captured execution still requires complete EOF/forwarding evidence; a missing record after ordinary ERROR/TIMEOUT remains an evidence error. Interactive execution without a terminal is not detected by terminal checks; any uncaptured child evidence is still unmeasured in the Meson reader. No new PTY subsystem is introduced.

The executor is compiled with the build-machine compiler and a separate native capability configuration. Cross builds never execute a target `lso-timeout` to measure tests; the original runtime/exe-wrapper requirements still apply to the test commands. If the native compiler or required POSIX primitives are unavailable, including native MinGW/MSVC Windows, Cygwin, and APE/WebAssembly compiler outputs, registration records `LSO_CHILD_TIME=outer-only: ...`, runs the original real test command, and the reader explicitly reports outer-only measurements. This is not a test skip or a timing success. Older uninstrumented logs are also labeled outer-only. Native Windows Job Object/pipe timing is not implemented; Cygwin pipe/termination semantics are also outside the child-timing support boundary. Autotools retains its existing outer driver timing without adding this opt-in wrapper around ordinary tests.

For WIC, shared setup includes script/fixture preparation, MSI/COM queries and native process startup. Each numbered script's stopwatch includes its PowerShell call and IPC, native COM initialization, DLL loading, fixture creation, observation, release and DLL/COM cleanup. The native diagnostic setup/observation/total times are nested breakdowns, not substitutes for the outer wall time; they exclude PowerShell invocation and communication overhead. Do not attribute the difference to DLL loading without measuring those other boundaries. No warmup observation may be hidden outside the report.

| Execution | Recorded evidence and reporting entry |
| --- | --- |
| Autotools `make check` | `.log` retains TAP/diagnostics and `.trs` retains result/timing metadata. `tests/test-suite.log` lists every completed measurement, including passes/skips. The WIC package batch uses `wic/wic-msi.log`, `wic/wic-msi.trs` and `wic/test-suite.log`. Top-level `check` calls `testtimecheck` after recursive test execution. |
| Autotools installed-codec check | `make -C wic installcheck` writes `wic/wic-installed.log`, `wic/wic-installed.trs` and `wic/installcheck-suite.log`; the ordered lifecycle target runs package checks, install and installcheck sequentially. |
| Meson TAP/package tests | `meson test -C builddir` (or a suite/selection) retains native outer seconds, verdict and output in `meson-logs/testlog.json`. The post-run reader reports registered child evidence when supported, explicit outer-only fallback otherwise, and unchanged nested batch/setup records. |
| Meson installed-codec check | `meson compile -C builddir installcheck-wic` writes TAP and per-file/setup times to `wic/installcheck.log`. `check-wic-installation` subsequently passes this log and `meson-logs/testlog.json` to `tools/check_test_times.py`; a direct installcheck run can be checked with the same reader. That standalone log has no separately measured enclosing process duration, so it must not be represented as having one. |

### Post-run interpretation

Normal `make check` remains advisory for timing: it prints the number and list of over-budget tests without changing functional pass/fail status. Slow setup/batch processes have a separate count and remain visible. Host load and parallel execution can increase wall time; use a quiet, comparable environment and a small focused selection when considering an exception. Preserve the first-call cost and report repeated samples, rather than choosing only the fastest run.

The timing meta-check reads completed real-test results and does not register extra runtime cases. `make testtimecheck` alone rechecks existing reports. Subdirectory checks report their own selection; a partial run cannot establish whole-suite coverage. [`build-aux/test-suite-report.awk`](../../build-aux/test-suite-report.awk) reads Automake records/reports; [`tools/check_test_times.py`](../../tools/check_test_times.py) reads reports, batch TAP logs and Meson JSON Lines, using the same allowlist.

Time overruns are advisory. Missing/unreadable reports, missing non-skip durations, invalid durations or malformed allowlist records are evidence errors, not slow-test exceptions. The Python reader also rejects a supplied log with no measured records and nonfinite/negative measurements. Neither reader infers every intended observation from a partial log or proves that reported timing surrounds the required work; source review and comparison with actual TAP results remain necessary. Validation uses completed run logs and their measured selections; it does not rely on a separate boundary-value runtime self-test suite. Changes to the executor also require bounded process-semantic controls for exit-zero TAP failure, abnormal exit, delayed descendant output, early stream closure, timeout/backpressure, broken output, outer termination, setup/clock errors, argv/environment preservation, and the declared platform/fallback behavior. These controls do not replace normal-entry measurements or turn historical slow samples into passes.

## Test coverage

<!-- test-coverage: enforced -->

Each automated contract has a stable ID and an owning test. The reciprocal `Policy:` reference in each check is itself verified by `staticcheck-doc-test-links`.

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
| TG-11 | Shell pattern predicates use `case`; the redundant `${value#*marker*}` scan is rejected, and only exact, current entries with a recorded user approval can exempt a detected form. | [tests/_static/sh/staticcheck-test-pattern-match.sh](../../tests/_static/sh/staticcheck-test-pattern-match.sh) |

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
