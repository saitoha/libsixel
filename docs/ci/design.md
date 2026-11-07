# CI Architecture and Design

## Goals

libsixel CI protects portability, build-system parity, public interfaces, image semantics, memory safety, language bindings, distribution integrity, and malformed-input handling.

The project uses two complementary CI systems: GitHub Actions and the private CI operated on @saitoha's local desktop infrastructure. GitHub Actions owns every platform/compiler pair it can run; the local CI is reserved for pairs that are not already represented there. Within each owning system, the matrix remains broad across build systems, ABIs, optional image libraries, linkage modes, sanitizers, and language runtimes.

The user-facing [Build, Runtime, and Platform Support](../platform-support.md) document defines the support tiers and summarizes the OS, architecture, compiler, and build-system coverage. The GitHub Actions workflows are the authoritative source for public jobs, while the support document is the repository-visible source for local-CI coverage. The repository does not maintain a duplicate job-by-job correspondence table.

## GitHub Actions workflow responsibilities

### Primary build and test

`.github/workflows/ci.yml` is the primary compatibility workflow. It includes
families for:

- smoke builds and compiler variants;
- static checks and static analysis;
- PGO and LTO;
- language bindings and their coverage;
- out-of-tree Autotools builds;
- Autotools and Meson platform matrices;
- distribution checks;
- project and Coveralls coverage;
- sanitizer configurations;
- supported Unix, macOS, Windows, ABI, and architecture combinations.

This workflow is the main evidence that a source change is broadly compatible.
Do not treat one passing job as a substitute for the required job families.

### WIC installer validation

The `extension-wic-msys2` jobs explicitly enable the [WIC codec](../programs/wic-codec.md) for Autotools and Meson on native Windows MinGW. Their existing `check_target` matrix field selects `make -C wic check-installation` or `meson compile -C builddir check-wic-installation`. The build targets own sequential execution even when the outer build is parallel; the shared CI action has no WIC-specific parameter, individual test names or expected test counts. Ordinary jobs retain the default-disabled extension.

| Phase | Responsibility and boundary |
| --- | --- |
| `make check` / Meson extension suite | Inspect the generated MSI and payload; exercise registration capture in a private per-process registry override. Do not install a system codec. MSI table assertions do not establish executed repair, upgrade, downgrade or uninstall behavior. |
| `make install` / `meson install` | Copy and launch the MSI, obtaining elevation or using an already elevated unattended environment. Windows Installer owns deployed files/registration. DESTDIR only stages files and skips system installation. |
| `make installcheck` / `installcheck-wic` | Verify the exact installed MSI product, payload hash, declared registry values, WIC discovery/pixel output and COM unload contracts. Never install or repair; fail when the exact product is absent. DESTDIR or SIXEL_INSTALLCHECK_DESTDIR reports a whole-suite system-codec skip. |

The explicit lifecycle targets reject staging variables, then run package checks, install and installed-codec checks in that order. They deliberately change the machine's installed codec; use disposable runners or preserve and restore pre-existing registrations/files around external validation. They do not automatically uninstall the product at the end. The [companion inventory](../testing/companion-programs-coverage.md) owns the complete numbered observations and limitations. One literal WIC decode is not proof of Explorer display, and unload eligibility is not a general memory-leak test.

Tests retain one result per numbered file while sharing PowerShell/COM setup and native process startup. Every lifetime request still measures its own COM setup, DLL load, fixture, observation and cleanup. The [timing policy](../testing/guide.md#test-timing) owns measurement boundaries, exact-name approved exceptions, log locations and post-run advisory reporting. Meson's lifecycle target aggregates completed package JSON and installed TAP timings; Autotools keeps separate package and installcheck reports. These jobs preserve their test logs; a skipped installation path does not validate a system codec.

### Quick Look package lifecycle validation

The `extension-quicklook-macos` matrix in `.github/workflows/ci.yml` explicitly enables the extension for Autotools and Meson on disposable GitHub-hosted macOS runners. Ordinary jobs keep the default disabled. The existing shared action receives only its ordinary matrix and `check_target`: `check-quicklook-installation` for Autotools and `compile check-quicklook-installation` for Meson. No extension-specific action parameter or individual test name is added. [The CI plan adapter](../../tools/quicklook-ci.py) supplies an explicit plan to the existing [native lifecycle](../../quicklook-extension/Sources/SixelLifecycle.h).

Before mutation, the plan requires an unprivileged hosted macOS process, supported native architecture, noninteractive sudo capability, absent project app/support roots, receipt and operation locks, and no existing Host/Bridge LS candidates, Preview/Thumbnail registrations or project default handler (including a stale handler). Unknown state refuses the job rather than being adopted or deleted. It records the caller's UID/session, GUI capability and defaults. This is a **system-only** job: no user activation, election, handler change or GUI observation is performed. The lifecycle's activation/deactivation/restoration commands are explicit no-ops for that empty initial state; they are not user-restoration evidence. A persistent or self-hosted machine is outside this plan's contract.

The build-owned lifecycle runs the registered package TAPs, then the common package install wrapper with explicit `sudo -n`, then the dedicated installed TAP entry. The wrapper invokes the same manager/Installer path as normal `make install` and `meson install`; it does not copy or re-sign apps directly. CI result validation requires the complete registered inventory and every required case to pass. Only an explicitly default-off-only package case or an installed case's explicit no-participating-user skip is allowed in its corresponding phase. Disabled/staging/cross system skips, missing records, failures and an all-SKIP result refuse success. These exclusions do not claim fresh default-off or GUI coverage.

After known successful installation, the existing lifecycle removes the exact owned system payload/receipt and separately verifies their absence and equality of the before/after user snapshots. It never forces a registration/default/cache reset to make equality pass. The empty-user prerequisite and absence of user operations are the basis for its explicit removal declaration; the declaration alone is not proof of deactivation on a general machine. A later state change or unknown Installer outcome remains incomplete. Original failure, cleanup failure and journal-recording failure retain the existing lifecycle's separate outcomes; an install failure does not authorize blind cleanup. Preserve residual state evidence and discard the disposable runner instead of attempting a generic repair.

The shared artifact upload already includes `_artifacts`, TAP `.log`/`.trs` and Meson logs. The private lifecycle record includes command order, status and per-stage elapsed time; native per-test and overlapping Meson child/outer measurements remain available. No automatic timing post-step or new allowance is introduced; use the existing [manual timing reader](../testing/guide.md#post-run-interpretation) after test changes. Workflow definition and offline adapter controls do not establish an executed CI install, successful OS restoration, provider selection or Finder behavior. Actual preview, thumbnail, Bridge open and user restoration still require separately approved environment-specific observations; see the [Quick Look policy](../programs/quicklook-extension.md#explicit-rendering-diagnostics).

### Experimental configurations

`.github/workflows/experimental.yml` exercises less conventional Windows build
combinations and targeted Clang sanitizer families. These jobs expose undefined
behavior, truncation, sign changes, object-bound errors, control-flow issues,
and portability gaps that ordinary builds may not diagnose.

An experimental failure can identify a real defect. Classify it from the
compiler or runtime evidence rather than dismissing it because the primary
matrix is green.

The experimental MSVC/vcpkg matrix has a 240-minute job budget for dependency setup, compilation and the complete codec test inventory. MSYS process startup and serial Autotools test dispatch can exceed 90 minutes while tests continue to complete. Individual test deadlines still detect hangs; the job budget does not relax their assertions or time limits.

### Fuzzing

`.github/workflows/fuzzing.yml` builds and runs structured and component fuzz
targets. It uses bounded corpora, dictionaries, time limits, sanitizers,
coverage or semantic-feature expectations, and uploaded failure artifacts.

Fuzz jobs should:

- identify the build system and target in the job label;
- start from controlled seed corpora;
- preserve a reproducible crashing input;
- bound runtime, memory, and artifact size;
- distinguish a harness/configuration error from a discovered crash;
- replay known regression inputs in ordinary tests where practical.

Embedded CLI fuzz targets compile the same converter sources as the normal `img2sixel` executable. Loader-format targets match the workflow's format matrix in both build systems. Separate [source-only checks](../testing/staticcheck.md#test-coverage) compare these inventories and reject missing-source or missing-format controls without compiling fuzz binaries.

### Nightly builds

`.github/workflows/nightly.yml` performs scheduled pre-release builds across
maintained branches and a platform/architecture matrix. Nightly jobs cover
packaging and combinations that are too costly or release-oriented for every
small change.

### Failure triage and replay

`.github/workflows/triage.yml` collects failed jobs from supported workflows and
replays a bounded subset.

- Normal mode validates the failed branch's current head.
- Forensic mode checks out the exact failed commit.
- Manual inputs can select the source workflow, branch, run, replay reference,
  and maximum number of jobs.

Triage accelerates diagnosis; it does not replace the full source workflow or
prove that a branch is green.

## @saitoha local desktop CI

The private local CI complements GitHub Actions only with platform/compiler pairs that are not represented there. Its active catalog covers Debian GNU/Linux Bookworm with GCC, Debian GNU/Hurd from the 2026-03-14 amd64 image with GCC, OpenIndiana 2025.10 with GCC 13, and OpenVMS 9.2-3 with the GNV `cc` environment. Autotools and Meson variants may coexist within an owned pair, but they do not create a reason to duplicate that pair across CI systems.

The implementation, job catalog, runner configuration, and deployment procedure are private operational details. Repository documentation must not name their repository paths because contributors cannot inspect those paths from this source tree. The Tier 2 table in [Build, Runtime, and Platform Support](../platform-support.md) is the public inventory of local-CI coverage.

## Coverage allocation policy

A platform/compiler pair is identified by the named operating system, distribution, or compatibility environment; target architecture; and compiler or ABI family. A different OS release, build system, sanitizer, optional dependency set, linkage mode, shell, install mode, or test flavor does not by itself make a second local-CI copy of a GitHub Actions pair acceptable.

GitHub Actions is the primary owner. Before adding a local job, inspect the current workflows and record why that platform/compiler pair cannot be maintained there. When GitHub Actions gains the same pair, first obtain a live green Actions result and then remove the pair from the active local schedule. Dormant local runner and job definitions may remain for manual diagnosis, but they must not be scheduled.

The workflows under `.github/workflows/` are the detailed inventory for GitHub Actions. Exact local job definitions remain an operational concern outside this repository; do not mention undiscoverable repositories or paths as though contributors could inspect them. Do not add a generated or manually synchronized combined table to this repository. Update the summary in [Build, Runtime, and Platform Support](../platform-support.md) only when the represented platform, architecture, compiler family, ABI, or build system changes.

When adding, removing, or renaming a CI job:

1. Audit the current GitHub Actions platform/compiler coverage and select exactly one owning CI system.
2. Change the authoritative Actions workflow or use the local CI's private maintenance procedure.
3. Review the owning configuration directly when you have access, including labels and the commands that distinguish each configuration.
4. Update `docs/platform-support.md` when the change adds or removes an OS, architecture, compiler family, ABI, or supported build system.
5. Run `make staticcheck` before committing.
6. Verify the actual replacement job in the corresponding CI system.

## Matrix design principles

### Preserve meaningful dimensions

Keep dimensions that can change behavior:

- Autotools versus Meson;
- compiler family and version;
- C runtime and ABI;
- operating system and architecture;
- enabled and disabled optional loaders;
- shared, static, and amalgamated linkage where supported;
- language runtime and binding;
- sanitizer or analysis policy;
- release and development branch packaging.

Merge jobs only when the resulting job still identifies failures precisely and
does not hide a supported configuration.

### Diagnostic independence

Haiku runs the Meson suite in slices with `--no-rebuild` to bound the lifetime of each test process. Build the non-default `test_runner` target before the first slice; the ordinary default build does not produce it. The step uses `set -eux` so a failed dependency build or slice stops the job. [The source check](../../tests/_static/sh/staticcheck-ci-haiku-test-deps.sh) rejects a missing, late, or failure-ignoring runner build.

Keep `fail-fast` disabled for diagnostic matrices. One early failure should not
hide an unrelated compiler, platform, or sanitizer failure.

The Command Prompt Meson jobs explicitly compile `test_runner` together with Ninja's `all` target, using `--ninja-args="['-k', '0', 'all']"` in both the normal and PGO-use stages. The C case archive is not a default target; compiling only `all` leaves its diagnostics to the test command's automatic rebuild, which stops after the first failing compilation. One combined build attempts every independent compilation, retains warnings as errors, and returns failure before testing if any command failed. This collects remaining diagnostics in one run without adding a second compilation pass to successful jobs. Dependencies blocked by a failed generator or missing header still cannot compile.

MSVC PGO generation also runs the default-built `progressive-study` executable with a small PPM input and a bounded repair budget. Each instrumented executable needs its own profile before the use-phase link; library and test-runner profiles cannot train an unexecuted tool. Training failure propagates before optimization. The [training check](../../tests/_static/sh/staticcheck-ci-pgo-study-training.sh) protects that CI step.

The `dist` job omits the explicit runner in its outer build because distribution validation compiles it in the extracted source tree. This avoids compiling the C inventory twice; ordinary MSVC jobs provide the complete diagnostic pass. Meson's internal distribution rebuild remains a separate execution boundary.

Job labels should expose the build system, platform, compiler or ABI, and the
policy that distinguishes the job. Logs should show the exact configure, build,
and test commands needed for local reproduction.

### Build-system parity

Shared source and public features must be wired into every supported build
system. CI should detect missing source lists, defines, tests, install files,
and optional dependency behavior.

Prefer lightweight static synchronization checks when a repository invariant
can be verified without building a full matrix. Keep an end-to-end build job as
evidence that the synchronized inputs actually work.

## Dependency and infrastructure policy

- Pin external bootstrap inputs that are not safe to float.
- Update a shared pin coherently across every workflow that consumes it.
- Bound package-manager and network retries with clear timeouts.
- Delete or isolate partial bootstrap state before a retry when it can poison
  the next attempt.
- Distinguish infrastructure failure from source regression using actual logs.
- Do not use retries or `continue-on-error` to mask deterministic failures.
- Keep caches accelerative only; a clean build must remain correct.

DragonFly BSD VM attempts run package setup, configure, build and tests through [`tools/ci-vm-result.sh`](../../tools/ci-vm-result.sh). The guest shell stops at the first failed command and writes its exit status to an attempt-specific result file, then lets the VM action copy artifacts back. Only a failed VM action is retried; a recorded build or test failure is validated on the host and fails the job without another build. A missing, incomplete or malformed result cannot become success. The [source-only handoff check](../testing/staticcheck.md#test-coverage) protects both build-system paths and exercises failure propagation without a VM.

NetBSD/SPARC64 separates VM preparation from source execution. Preparation covers startup, source transfer and package installation, with at most two attempts of 25 minutes each within the 180-minute job budget. A failed first attempt is terminated and its SSH aliases removed; the second uses a separate VM data directory with caching disabled. This permits recovery when repeated SSH connections to the same guest time out. After successful preparation, one SSH invocation runs configure, build and smoke tests with stop-on-error; its exit status fails the job directly. Neither a source failure nor a connection failure during that invocation triggers another build. The [SPARC retry check](../../tests/_static/sh/staticcheck-sparc-vm-retry.sh) protects this separation; recovery on a hosted runner remains an integration observation.

VM Autotools jobs export the matrix-selected `MAKE` before running configure, then use that same implementation for build and test. Automake dependency bootstrapping also invokes make during `config.status`; selecting `gmake` only for the later build leaves NetBSD configure using incompatible native make. The source check verifies selection and ordering without booting a VM.

Windows jobs disable Git CRLF conversion before checkout, including Nightly jobs that build a separately selected branch. TAP shell sources must retain LF endings: dash can reject `set -eux` followed by a carriage return before any TAP plan is emitted. The checkout-order check protects this setup; a real Windows run remains necessary to validate the complete test-launch environment.

The Nightly MSVC job builds develop and leaves `AR` and `ARFLAGS` unset. Its configure already detects MSVC, selects `ar-lib` from the configured auxiliary directory, and supplies archive flags. An explicit CI override suppresses those defaults and can retain a stale path after auxiliary files move. The workflow therefore exercises the existing [MSVC toolchain contract](../misc/platforms/msvc.md#toolchain-and-crt-contract) instead of maintaining a second archiver selection.

## Artifact policy

Preserve artifacts needed to reproduce or understand a failure:

- test logs and summaries;
- sanitizer reports;
- fuzz crash inputs and bounded run logs;
- coverage reports;
- intended release packages.

macOS coverage capture uses the selected Xcode toolchain's `llvm-cov gcov` with gcovr 8.8 in an isolated Python environment. The legacy LLVM gcov format can associate executed lines with zero branch counts; lcov consistency checks can reject that input before publishing coverage. The selected reader must successfully produce a nonempty report with source records; capture failure is propagated rather than replaced by an initial-only map or suppressed. The [macOS reader check](../../tests/_static/sh/staticcheck-macos-coverage-reader.sh) executes the production recipe with stub tools to check selection and failure propagation.

DragonFly BSD result files use the `.log` suffix so the shared artifact upload preserves them alongside test diagnostics. Copyback must finish before the host reports a recorded build failure. A transport failure can still prevent artifact recovery; the retry limit does not establish that those missing tests passed.

Do not upload arbitrary complete build trees. Artifact names must identify the
job configuration, and retention or size must remain bounded.

## Changing CI

CI changes are production changes to the project's verification system.

1. Inspect the actual branch, workflow, run, job, and first concrete failure.
2. Determine whether the failure belongs to source, workflow, dependency,
   runner, network, or stale generated configuration.
3. Reproduce the relevant command locally or on a matching platform when
   practical.
4. Keep the fix limited to the failing surface; do not mix unrelated cleanup
   into a CI repair.
5. Recheck that the selected CI system is the sole owner of the platform/compiler pair.
6. Validate workflow syntax and repository policy with `make staticcheck`.
7. Run `make check` when the workflow change can affect compilation, generated
   inputs, tests, packaging, or runtime behavior.
8. Push only the intended files and inspect the replacement run.

A queued or in-progress replacement run is not evidence that the branch is
green.

## Live failure triage

Start from current evidence:

```sh
gh auth status
gh run list --repo saitoha/libsixel --branch BRANCH
gh run view RUN_ID --job JOB_ID --log
```

For a branch push without a pull request, query the branch directly. If normal
job-log retrieval is unavailable, use the GitHub API for that exact job rather
than substituting an older similar failure.

Group failures only after comparing their first concrete errors. Several red
jobs may share one integration defect, or one workflow run may contain several
independent failures.

## Local validation

Use project-local build tools:

```sh
PATH="$PWD/.local/bin:$PATH" make staticcheck
PATH="$PWD/.local/bin:$PATH" make check
```

Use the repository actionlint wrapper through the static suite rather than an
unconfigured standalone invocation. Run `make distcheck` for changes affecting
distribution or release packaging.

If tests or tracked generated inputs appear stale after an Autotools change,
investigate configuration regeneration before changing source assertions:

```sh
PATH="$PWD/.local/bin:$PATH" ./config.status --recheck
```

Finish with `git diff --check` and a selective commit containing only the CI
change and directly required source, test, or generated-file updates.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| CI-01 | CI documentation does not direct contributors to repositories or configuration paths that are unavailable from this source tree. | [tests/_static/sh/staticcheck-ci-doc-public-surface.sh](../../tests/_static/sh/staticcheck-ci-doc-public-surface.sh) |
| CI-02 | Embedded CLI fuzz targets retain the normal converter source inventory. | [tests/_static/sh/staticcheck-fuzz-cli-sources.sh](../../tests/_static/sh/staticcheck-fuzz-cli-sources.sh) |
| CI-03 | Both build systems provide every loader-format fuzz target selected by the workflow. | [tests/_static/sh/staticcheck-fuzz-loader-formats.sh](../../tests/_static/sh/staticcheck-fuzz-loader-formats.sh) |
| CI-04 | DragonFly BSD build failures return through attempt-specific records after artifact copyback, propagate to the host, and do not trigger VM retries. | [tests/_static/sh/staticcheck-ci-vm-result.sh](../../tests/_static/sh/staticcheck-ci-vm-result.sh) |
| CI-05 | VM Autotools configure receives the same matrix-selected make as the subsequent build. | [tests/_static/sh/staticcheck-ci-configure-make.sh](../../tests/_static/sh/staticcheck-ci-configure-make.sh) |
| CI-06 | Nightly disables checkout line-ending conversion before materializing shell tests. | [tests/_static/sh/staticcheck-ci-checkout-eol.sh](../../tests/_static/sh/staticcheck-ci-checkout-eol.sh) |
| CI-07 | Nightly MSVC invokes configure without overriding its archiver or archive flags. | [tests/_static/sh/staticcheck-nightly-msvc-archiver.sh](../../tests/_static/sh/staticcheck-nightly-msvc-archiver.sh) |
| CI-08 | Normal and PGO-use Command Prompt test builds include the C runner and default targets, continue independent compilations after errors, and immediately propagate failure; dist leaves the runner to its extracted-tree build. | [tests/_static/sh/staticcheck-ci-msvc-complete-build.sh](../../tests/_static/sh/staticcheck-ci-msvc-complete-build.sh) |
| CI-09 | Haiku builds the non-default runner before disabling automatic dependency rebuild for test slices. | [tests/_static/sh/staticcheck-ci-haiku-test-deps.sh](../../tests/_static/sh/staticcheck-ci-haiku-test-deps.sh) |
| CI-10 | MSVC PGO trains the default study executable and propagates training failure. | [tests/_static/sh/staticcheck-ci-pgo-study-training.sh](../../tests/_static/sh/staticcheck-ci-pgo-study-training.sh) |
| CI-11 | macOS coverage selects the pinned LLVM-compatible reader and propagates reader failure. | [tests/_static/sh/staticcheck-macos-coverage-reader.sh](../../tests/_static/sh/staticcheck-macos-coverage-reader.sh) |
| CI-12 | SPARC retries only bounded VM preparation, resets the failed guest, and runs the source build once with failure propagation. | [tests/_static/sh/staticcheck-sparc-vm-retry.sh](../../tests/_static/sh/staticcheck-sparc-vm-retry.sh) |
