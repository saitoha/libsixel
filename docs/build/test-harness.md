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

## Result semantics

The assertion protocol is TAP. A normal shell test declares one observation with `1..1`, emits `ok` or `not ok`, and exits zero even on an assertion mismatch. An unavailable supported optional capability can emit `1..0 # SKIP`. The driver, rather than the assertion's process exit code, decides the test outcome.

The `.trs` collector counts `PASS`, `SKIP`, `XFAIL`, `FAIL`, `XPASS`, and `ERROR`. It uses the per-file global result and copy flag when assembling diagnostics. `FAIL`, `XPASS`, or `ERROR` makes the suite fail; an expected failure does not. Failure to launch a TAP producer or a timeout is an infrastructure observation, distinct from a normal `not ok` assertion.

Meson normally parses the same TAP producers directly. Its expected-failure adapter and different log format are described in the [Meson chapter](meson.md#test-registration-and-execution).

## Command-length and record-length boundaries

The inventory has grown to thousands of individually named tests, often with descriptive paths. Its flattened size can exceed a platform's command or argument limits before any test starts. There are several separate boundaries:

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
