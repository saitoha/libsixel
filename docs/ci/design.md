# CI Architecture and Design

## Goals

libsixel CI protects portability, build-system parity, public interfaces, image
semantics, memory safety, language bindings, distribution integrity, and
malformed-input handling.

The project uses two complementary CI systems: GitHub Actions and the private
CI operated on @saitoha's local desktop infrastructure. Their combined matrix
is intentionally broad because the project crosses C compilers, ABIs,
operating systems, optional image libraries, build systems, and language
runtimes. A green result in one common Linux configuration is not sufficient.

The generated [CI Support Matrix](support-matrix.md) lists the build
configurations configured in both systems.

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

The private local CI complements GitHub Actions with environments that are not
available as practical GitHub-hosted runners. It includes OpenVMS 9.2-3 with
GNV, Debian GNU/Hurd, the BSD family, Haiku, Solaris and illumos derivatives,
Windows variants, Intel macOS, and Linux Docker toolchain/sanitizer variants.

The local CI implementation and configuration files are versioned in the
companion `libsixel-ci` source repository. Its relevant configuration is:

- `srv/misc/jobs.tsv`: authoritative job catalog;
- `srv/misc/runner-profiles.tsv`: backend, resource, and execution profile for
  each runner class;
- `srv/misc/resource-classes.tsv`: scheduler resource policy;
- `srv/misc/workers.tsv`: dispatch workers and their capabilities;
- `srv/misc/job-definition/`: build and test command flows;
- `srv/setup/`: guest and toolchain provisioning, including OpenVMS and Debian
  GNU/Hurd support.

The local catalog is host-owned configuration, but it is not an unversioned
live-server setting. Change it in the `libsixel-ci` source repository and deploy
through that repository's commit-and-push workflow. Do not edit `/srv` runtime
files directly.

`docs/ci/local-jobs.tsv` is a normalized documentation snapshot of the job name
and runner profile columns from the authoritative catalog. It is deliberately
non-operative: the local scheduler continues to read the `libsixel-ci` catalog.

## Maintained support inventory

[CI Support Matrix](support-matrix.md) is generated rather than maintained
as a hand-written list. The generator expands literal GitHub Actions matrix
entries and combines them with the tracked normalized local-CI snapshot.

After changing GitHub Actions configuration, regenerate the inventory with:

```sh
python3 tests/_static/python/generate_ci_support_matrix.py \
    --root . \
    --local-snapshot docs/ci/local-jobs.tsv \
    --write docs/ci/support-matrix.md
```

After changing the authoritative local CI catalog, refresh both the snapshot
and inventory with:

```sh
python3 tests/_static/python/generate_ci_support_matrix.py \
    --root . \
    --local-snapshot docs/ci/local-jobs.tsv \
    --sync-local-catalog /path/to/libsixel-ci/srv/misc/jobs.tsv \
    --write docs/ci/support-matrix.md
```

`make staticcheck` rejects drift between the GitHub Actions workflow matrices,
the local snapshot, and the generated Markdown. In @saitoha's normal workspace,
it also discovers the companion catalog and compares it with the snapshot. Set
`LIBSIXEL_LOCAL_CI_JOBS` to its path when using another checkout layout.

When adding, removing, or renaming a CI job:

1. change the authoritative Actions workflow or local CI catalog and job
   definition;
2. regenerate the support inventory, refreshing the local snapshot when
   applicable;
3. review the generated list as part of the CI change;
4. run `make staticcheck` before committing;
5. verify the actual replacement job in the corresponding CI system.

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
5. Regenerate the support inventory when a configured job changes.
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
