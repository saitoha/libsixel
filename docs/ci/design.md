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

### Experimental configurations

`.github/workflows/experimental.yml` exercises less conventional Windows build
combinations and targeted Clang sanitizer families. These jobs expose undefined
behavior, truncation, sign changes, object-bound errors, control-flow issues,
and portability gaps that ordinary builds may not diagnose.

An experimental failure can identify a real defect. Classify it from the
compiler or runtime evidence rather than dismissing it because the primary
matrix is green.

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

The private local CI complements GitHub Actions only with platform/compiler pairs that are not represented there. Its active catalog covers Debian GNU/Linux Bookworm with GCC, OpenIndiana 2025.10 with GCC 13, and OpenVMS 9.2-3 with the GNV `cc` environment. Autotools and Meson variants may coexist within an owned pair, but they do not create a reason to duplicate that pair across CI systems.

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

Keep `fail-fast` disabled for diagnostic matrices. One early failure should not
hide an unrelated compiler, platform, or sanitizer failure.

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

## Artifact policy

Preserve artifacts needed to reproduce or understand a failure:

- test logs and summaries;
- sanitizer reports;
- fuzz crash inputs and bounded run logs;
- coverage reports;
- intended release packages.

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
