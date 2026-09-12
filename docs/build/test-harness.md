# Test Harness and Command-Length Limits

## Shared discovery and C test dispatch

Runtime test discovery is centralized in [read-check-test-list.sh](../../build-aux/read-check-test-list.sh). The helper scans the test source tree, normalizes relative paths, sorts them deterministically, and selects `.t` scripts and numbered Python, Ruby, Perl, and PHP binding tests. It prunes private test environments and artifacts and excludes selected static-only scripts that run under `staticcheck`.

Autotools invokes discovery during configuration; Meson invokes it during setup. A directory scan is shared policy, not continuous discovery on every test invocation. After adding or renaming tests, refresh the configured inventory.

[test_runner.c](../../tests/test_runner.c) is a separate layer: a C dispatch executable shared by many small TAP wrappers. It reduces the number of independently linked helper programs while retaining independent test reports. A shell test can also call a converter or another helper without using this executable. Sharing one executable does not mean running all observations inside a single long-lived process.

| Registration layer | Autotools | Meson |
| --- | --- | --- |
| Runtime script | Discovered by the shared helper at configure time. | Discovered by the shared helper at setup time. |
| Ordinary C test source | Explicit `test_runner_SOURCES` in `tests/Makefile.am`. | Setup-time scan of C/header files, excluding `*.inc.c`. |
| Dispatchable C observation | Registered in `test_runner.c` with its implementation and wrapper. | The same dispatch implementation and wrapper. |
| Amalgamated runner | `amalgamation/sixel.c` and `TEST_RUNNER_AMALGAMATION_DEFINES` when amalgamated tools are selected. | The ordinary test source list linked to `libsixel_dep`; the runner is not replaced with that Autotools amalgamated translation unit. |

The [testing guide](../testing/guide.md) owns one-observation rules, numbering, shell structure, and validation. This chapter describes the infrastructure around those tests.

## Autotools execution path

The current [tests/Makefile.am](../../tests/Makefile.am) overrides Automake's `check-TESTS` recipe. Its work can be read as five stages:

1. Build `test_runner`, prepare enabled binding packages, and prepare packaged large fixtures.
2. Remove old runtime `.log` and `.trs` outputs without expanding the complete test list into one command.
3. Resolve tool paths, export configured feature macros once, prepare binding environments, and detect the relevant runtime.
4. Invoke the short `sixel-check-test-logs` target, which requests the individual test logs through make dependencies on ordinary platforms.
5. Discover the resulting `.trs` files, read them line by line, aggregate the outcome counts, and write `test-suite.log` and a failed-test list.

On ordinary platforms, the essential dependency relationship is:

```make
check-TESTS:
	$(MAKE) $(AM_MAKEFLAGS) sixel-check-test-logs

sixel-check-test-logs: sixel-check-test-tools $(TEST_LOGS)
	@:
```

This is a simplified excerpt showing the scheduling boundary; the real recipe also performs preparation and result collection. The thousands of log targets stay in make's dependency graph, while the recursive process receives one target name. Automake's per-extension log rules and make's parallel scheduler remain involved.

The configured [lso-tap-driver.sh](../../build-aux/lso-tap-driver.sh.in) is the per-script launcher and TAP consumer. It implements Automake's custom-driver arguments, chooses the test shell or language interpreter, applies runtime library paths, sets the artifact location, enforces a timeout through available helpers or a supported shell fallback, captures diagnostics, and produces `.trs` result fields. It also handles expected failures and infrastructure errors. The source comment on `check-TESTS` separately records avoidance of nested single quotes in Automake helper expansion; command length is not the only reason for customization.

## Support scripts before each test

The helpers under `build-aux/` move recurring setup out of individual tests. [The support-script inventory](support-scripts.md#file-inventory-and-ownership) lists their maintained sources and callers. The Autotools recipe performs common preparation before requesting the log targets; Meson supplies a base environment at setup and performs selected lazy preparation in its per-test wrapper.

### Executable paths and runtime libraries

[resolve-test-tool-paths.sh.in](../../build-aux/resolve-test-tool-paths.sh.in) becomes a configured script containing the selected executable suffix and Libtool object-directory name. It takes the build root and a file of requested keys, and emits quoted assignments for converter paths, `lsqa`, `test_runner`, and `LIBSIXEL_LIBDIR`. The key list prevents each caller from duplicating path-discovery rules.

A top-level Libtool executable may be a shell wrapper around the real binary under `.libs`. Bypassing it can save a shell launch, but only if the runtime library lookup rules still select the intended library. The resolver reads `shlibpath_overrides_runpath` from the configured `libtool` and prefers the wrapper when the platform does not permit the search environment to override runpath. The TAP driver applies the corresponding runtime library path for eligible direct execution. This is why blindly choosing every `.libs` binary can produce a different test environment.

With `SIXEL_INSTALLCHECK=1`, the resolver instead uses installed-command names or explicit installcheck overrides and the configured installation library directory. `test_runner` remains test infrastructure. Meson's wrapper performs its own default path setup and permits installcheck values to override it; it does not execute the configured Autotools path resolver.

### Enabled-feature export

[resolve-config-h-exports.sh](../../build-aux/resolve-config-h-exports.sh) reads the configured header once and selects definitions of the exact form `#define NAME 1`. Its `exports` mode emits shell assignments; its `pairs` mode emits tab-separated names and values for Meson setup. Undefined features, zero values, strings, and general numeric settings are omitted.

This avoids reparsing the header in thousands of test processes and keeps the inherited environment smaller, particularly on Windows. It is an enabled-boolean feature inventory, not a serialization of all `config.h` contents. Callers must interpret an absent boolean appropriately and obtain non-boolean configuration through its owning interface.

### Shared binding package environments

The `resolve-*-test-venv.sh` names use “venv” broadly. Only the Python resolver creates a Python virtual environment; the others provide language-specific isolated package and library paths. Each emits shell-safe assignments for the harness to import. Empty interpreter assignments indicate unavailable preparation, leaving the caller to apply its skip or failure policy.

| Resolver | Preparation and reuse | Values supplied to tests |
| --- | --- | --- |
| [Python](../../build-aux/resolve-python-test-venv.sh) | Finds packaged wheels, requires `venv` and `ensurepip`, installs the wheel with `pip --no-deps`, verifies it, and caches the selected payload signature. | `SIXEL_TEST_PYTHON`, pointing to the private interpreter. |
| [Ruby](../../build-aux/resolve-ruby-test-venv.sh) | Installs the locally built gem under an isolated gem home and verifies that `libsixel` can be required from the package. | `SIXEL_TEST_RUBY`, `SIXEL_TEST_RUBY_GEM_HOME`, `SIXEL_TEST_RUBYLIB`. |
| [Perl](../../build-aux/resolve-perl-test-venv.sh) | Prepares a contained `local::lib` environment, verifies FFI dependencies and the binding's origin, stages the shared library, and creates an interpreter wrapper. It can use `cpanm` to install missing FFI dependencies. | `SIXEL_TEST_PERL` and the `CPANM`, `PERL5LIB`, local-library, and build-option settings. |
| [PHP](../../build-aux/resolve-php-test-venv.sh) | Prepares interpreter wrappers, checks FFI availability, extracts the packaged binding, resolves its bundled shared library, and handles native Windows path forms. | `SIXEL_TEST_PHP`, optional `SIXEL_TEST_PHPDBG`, binding root, library directory, and library path. |

The shared [resolve-binding-test-common.sh](../../build-aux/resolve-binding-test-common.sh) provides quoting and lookup for the project's `.so`, `.dylib`, and DLL name variants. The resolvers use payload and, where supplied, source/shared-library signatures so a package environment can be reused without silently retaining an earlier build. Callers that omit optional source and library arguments obtain less input-sensitive invalidation; the actual resolver invocation matters.

Autotools prepares the environments before the parallel log targets. [meson-python-tap-wrapper.sh](../../tests/meson-python-tap-wrapper.sh), despite its historical name, coordinates Ruby, Perl, and PHP preparation as well as shell/Python execution. For its shared preparation paths, lock directories and exported-state files coordinate workers so they do not all install or extract the same package. Large packaged fixtures have separate preparation and completion markers. These operations belong to harness setup, which is why the individual TAP tests can remain small and avoid repeated package installation and filesystem work.

## Inside the custom TAP driver

[test-driver](../../build-aux/test-driver) is Automake's shipped basic driver, whose primary observation is process status. The project's configured `lso-tap-driver.sh` instead consumes TAP output. Extension-specific log-driver settings in [tests/Makefile.am](../../tests/Makefile.am) select it for the runtime scripts. The custom suite collector then reads its `.trs` records; these two layers have different responsibilities.

| Driver stage | Adaptation | Why it belongs here |
| --- | --- | --- |
| Invocation | Accept Automake's test name, log path, `.trs` path, expected-failure, and hard-error arguments. | Existing per-extension make rules can use a project-specific result consumer. |
| Interpreter selection | Choose the configured test shell or prepared Python, Ruby, Perl, or PHP interpreter. | Test files retain their language and TAP contract while the build selects the environment. |
| Artifact path | Export a category-specific path under `tests/_artifacts`; abbreviate long filename components to 43 characters. | Avoid repeated path discovery and excessive filename components. Directory creation is deferred to tests that need artifacts. |
| Runtime lookup | Apply configured shared-library search metadata and interpreter-specific library paths. | A test must execute against the selected build's library. |
| Deadline | Prefer a host `timeout`/`gtimeout`, then an eligible project `lso-timeout`, then the supported shell watchdog. | Cross-compiled helpers may not be directly runnable; timeout support must respect the runtime. |
| Capture | Normally redirect the test to its `.log`; optionally stream through a FIFO and `tee`. | Long static checks can report progress while preserving the complete diagnostic log. |
| Interpretation | Parse TAP with AWK, or use the `reduce_fork=yes` shell parsing path when selected. | Avoid requiring another language runtime and permit reduced process overhead. |
| Reporting | Emit individual and global results, recheck/copy flags, elapsed time, and the summary line. | The suite collector can aggregate results without rerunning tests. |

The default timeout budget is 90 seconds with a 10-second escalation delay. The shell watchdog accepts an integer budget, records whether it caused the termination, and returns timeout status 124 for that case. A budget of zero disables that shell deadline. If no helper can be used and the budget is not supported by the shell fallback, the current final fallback runs the command directly. Thus deadline behavior must be diagnosed from the selected path, not just the existence of a `--timeout` argument. The native helper is [lso-timeout.c](../../tools/lso-timeout.c); `lso-timer` is built from [timer.c](../../tools/timer.c).

Streaming defaults to enabled for `staticcheck-*` names and disabled for ordinary runtime tests, with `--stream-output` and `LSO_TAP_STREAM_LOG` overrides. The driver keeps the test's status separately from `tee`'s status and cleans up background processes and the FIFO on interruption. When FIFO creation fails, it falls back to ordinary file capture.

OpenVMS adds a mapped-error-status boundary and a generated `cmp` path adapter because GNV utilities do not all accept the same absolute path forms. These are per-process execution adaptations; the line-oriented inventory and optional test shards described below solve different scheduling and file-record constraints.

The driver normally returns success after recording a failed observation so make can finish the suite and the collector can report every failure. `--enable-hard-errors=yes` changes that driver behavior. This is separate from the rule that an assertion mismatch in a normal TAP test is communicated by `not ok` while the test script exits zero. Both producers and consumers must preserve the distinction between assertion outcome and infrastructure status.

## Result semantics

The assertion protocol is TAP. A normal shell test declares one observation with `1..1`, emits `ok` or `not ok`, and exits zero even on an assertion mismatch. An unavailable supported optional capability can emit `1..0 # SKIP`. The driver, rather than the assertion's process exit code, decides the test outcome.

The `.trs` collector counts `PASS`, `SKIP`, `XFAIL`, `FAIL`, `XPASS`, and `ERROR`. It uses the per-file global result and copy flag when assembling diagnostics. `FAIL`, `XPASS`, or `ERROR` makes the suite fail; an expected failure does not. Failure to launch a TAP producer or a timeout is an infrastructure observation, distinct from a normal `not ok` assertion.

Meson normally parses the same TAP producers directly. Its expected-failure adapter and different log format are described in the [Meson chapter](meson.md#test-registration-and-execution).

## What happened when the suite passed 2,000 tests?

The 2,000-test milestone did coincide with command-length failures. On March 26, 2026, [a CI failure](https://github.com/saitoha/libsixel/actions/runs/23599461022/job/68725092300) was reported with `make[3]: /bin/bash: Argument list too long`. Recounting the tracked scripts immediately before [`ce7a8a488`](https://github.com/saitoha/libsixel/commit/ce7a8a488), using that revision's discovery rules with Ruby included, gives **2,047 runnable scripts**. Their relative paths occupy **133,960 bytes** with one separator per entry. This count includes numbered language-binding tests and excludes the static-only script excluded by that helper; it is not simply the number of `.t` files or assertions that passed.

Related limits had already appeared at a smaller inventory size. They affected several operations and were removed in stages:

| Author date | Affected operation | Local adaptation and evidence |
| --- | --- | --- |
| 2026-03-13 | Result aggregation, reported as `/bin/sh: Bad address` at `test-suite.log` on DragonFlyBSD. | [`e1ca48f3b`](https://github.com/saitoha/libsixel/commit/e1ca48f3b) introduces the short `sixel-check-test-logs` target and `.trs` manifest aggregation. The [failure report](https://github.com/saitoha/libsixel/actions/runs/23030473982/job/66887601664) locates the error in summary generation. |
| 2026-03-22 | Fixture distribution expanded all data filenames. | [`e9a2ef5b8`](https://github.com/saitoha/libsixel/commit/e9a2ef5b8) distributes the fixture directory as a tree. |
| 2026-03-26 | Test setup expanded the entire recheck-log list for cleanup. | The build-rule part of [`b8a840a58`](https://github.com/saitoha/libsixel/commit/b8a840a58) replaces that expansion with filesystem discovery and bounded removal. |
| 2026-03-27 | `distdir` and `clean-local` still expanded whole script inventories. | [`ce7a8a488`](https://github.com/saitoha/libsixel/commit/ce7a8a488) moves script copying into `dist-hook` and processes copied test paths incrementally during cleanup. This is the revision whose parent has the 2,047-script inventory above. |
| 2026-03-27 | `mostlyclean-generic` still expanded the full `TEST_LOGS` list. | [`cc213e7a5`](https://github.com/saitoha/libsixel/commit/cc213e7a5) replaces that remaining expansion with bounded filesystem cleanup. Fixing launch or distribution alone had not fixed every cleanup path. |

These are author dates; the commits shown were subsequently replayed, so their committer dates can be later. The inventory measurement describes a historical snapshot, not a universal failure threshold. Path lengths, repeated expansion inside a recipe, selected tests, environment strings, and platform limits determine which command fails. A make dependency list that is valid in memory can become invalid when flattened into an argument to another process.

This experience is part of the project's reason for retaining Autotools + Libtool. Mature tools still have assumptions that break under unusually large workloads. Here, the useful escape route was already available: override the affected Automake rules, introduce a make target with the large list as prerequisites, use a manifest for collection, and use `dist-hook` for bounded copying. The changes stayed in the project's `Makefile.am` files and their generated outputs; they did not require patching the installed Autotools tools or migrating the whole build. For a maintainer familiar with make and shell, that was a straightforward way to work past the limitation.

The lesson includes both sides: the standard machinery did not automatically handle this scale, and its customization mechanisms made the repairs practical. Preserving small, independently reported tests remained possible while changing how the infrastructure carried their names between processes.

## Command-length and record-length boundaries

The inventory has grown to thousands of individually named tests, often with descriptive paths. Its flattened size can exceed a platform's command or argument limits before a launch, cleanup, distribution, or aggregation step can execute. There are several separate boundaries:

Before the manifest-based collector, the project's `check-TESTS` recipe expanded `TEST_LOGS`, assigned the result to a shell variable, and passed it to recursive make as `TEST_LOGS="$$log_list"` while requesting `test-suite.log`. This made the complete inventory one process argument. Commit `e1ca48f3b` replaced that path with the short log target and file-based result collection; the fix changes how the inventory crosses the process boundary, while retaining per-test make rules.

| Boundary | Problematic full-list operation | Current mitigation and owner |
| --- | --- | --- |
| Recursive test launch | Passing every `.log` target on the recursive make command line. | `sixel-check-test-logs` is a single command argument; the full list remains make prerequisites on ordinary platforms. |
| Suite aggregation | Expanding every result path into a generated shell recipe or utility argument list. | `find` plus a sorted `.trs` manifest, then shell `read` loops and small per-file operations in `check-TESTS`. |
| Cleanup | One `rm` command containing the complete `TEST_LOGS` expansion. | `mostlyclean-generic` and suite setup discover logs and process paths incrementally. |
| Test-script distribution | Expanding `$(TESTS)` through `EXTRA_DIST` into a very large `distdir` shell command. | A `dist-hook` streams discovered paths and copies scripts individually. |
| Fixture distribution | Listing every fixture separately in `DISTFILES`. | `TEST_DATA_DIRS = data` distributes the fixture tree as a directory. |
| OpenVMS Makefile generation | Substituting the complete inventory as one large RMS file record. | Configure writes `tests/check-tests.list` with one path per line; the OpenVMS runner consumes that file directly. |
| Meson wrapper launch on Windows | Embedding a substantial shell program in `sh -c` introduces command-string and quoting hazards. | A configured wrapper file is passed to `sh` instead of carrying its program text in each command. |

The relevant limit may be the combined argument/environment budget, a single argument or command-string limit, or an OpenVMS record-length limit. These are not interchangeable. The ordinary Autotools path still substitutes `TESTS` into a configured Makefile; it avoids the dangerous external-command expansions rather than eliminating every large make variable. The discovery helper's optional `make` output emits bounded `TESTS +=` lines, but the current normal configure path uses its space-separated mode and OpenVMS uses newline mode.

The history uses `ARG_MAX` as shorthand for this family of process-launch limits. For example, Linux also imposes a per-string limit separate from the combined argument/environment budget, as documented in [execve(2)](https://man7.org/linux/man-pages/man2/execve.2.html). A large `sh -c` program or a single `TEST_LOGS=...` argument can hit that boundary even if the combined budget is larger. Moving the same complete inventory into one environment variable therefore does not generally solve the problem.

To inspect the inventory scale without running tests or putting its contents in command arguments:

```sh
sh build-aux/read-check-test-list.sh tests tests include newline |
    awk '{ count++; bytes += length($0) + 1 }
         END { printf "tests=%d path_bytes=%d\n", count, bytes }'
```

`path_bytes` is the flattened path-list size with one separator per entry. It is not a measurement of a particular failing command, whose flags, suffixes, environment, and platform representation can differ. Keep this measurement separate from the number of assertions reported by a completed suite.

## OpenVMS execution

On OpenVMS, `TESTS` is empty in the Makefile and `OPENVMS_CHECK_TESTS_LIST` points to the line-oriented inventory. `sixel-check-test-openvms-shard` reads it and launches each script through the same custom TAP driver. `OPENVMS_TEST_JOBS` defaults to one. The experimental two-worker path splits the inventory, runs its designated serial cases first, and collects worker status files. The source comments record that extra GNV process and file work made the two-worker mode slower on the measured runner.

This is different from normal make-driven log scheduling. It addresses both RMS record semantics and GNV process behavior. Native executable linking has its own [gnv-link-program.sh](../../openvms/gnv-link-program.sh) adapter, which produces an OpenVMS linker option file; it is not the mechanism used to solve ordinary test-list argument growth. [OpenVMS compatibility](../misc/platforms/openvms.md) owns the complete platform contract.

## Runtime and process costs

The harness exports `TOP_SRCDIR`, `TOP_BUILDDIR`, resolved converter and runner paths, configured feature toggles, and an isolated `ARTIFACT_LOCAL_DIR`. Tests use those values instead of repeatedly parsing `config.h` or guessing which installed binary is on `PATH`. Tests create their own artifact directory only when needed, after early skips. Binding package environments and large fixtures have preparation paths shared across tests.

These choices complement the command-length fixes: combining C helpers reduces link work, shared feature exports reduce per-test probes, and avoiding unnecessary shell forks and file creation matters especially on Windows and GNV. They do not remove the need for separate, independently reported observations.

## Diagnosing the correct layer

| Symptom | First boundary to inspect |
| --- | --- |
| New script absent from the suite | Configure/setup-time discovery and exclusions in `read-check-test-list.sh`. |
| `unknown test` from the C runner | Dispatch registration and whether the executable was rebuilt. |
| Amalgamated runner fails after a C test change | Generated `sixel.c`, normal registration, and amalgamation defines. |
| Argument-list error before execution | Recursive make arguments, generated recipes, cleanup, or distribution expansion; identify the actual failing command. |
| OpenVMS rejects a generated Makefile | RMS record format and use of the line-oriented test inventory. |
| Test passes alone but fails under a build system | Exported paths, selected feature set, runtime launcher, shared fixtures, and parallelism. |
| Many skipped tests | Optional dependencies and runtime prerequisites in that exact build, not the overall health of another configuration. |

Historical changes are inspectable in commits [`e1ca48f3b`](https://github.com/saitoha/libsixel/commit/e1ca48f3b) for the short launch target and manifest aggregation, [`ce7a8a488`](https://github.com/saitoha/libsixel/commit/ce7a8a488) for distribution and cleanup limits, and [`3bf727177`](https://github.com/saitoha/libsixel/commit/3bf727177) for the file-based Meson wrapper. Current source rules remain authoritative after later changes.
