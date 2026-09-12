# Autotools + Libtool Configuration and Build Orchestration

This chapter covers the combined Autoconf/Automake configuration and Makefile machinery and Libtool's library compilation and linking support. Their joint distribution, portability, and customization properties explain [why libsixel maintains this path alongside Meson](README.md#why-maintain-two-build-systems).

## Configuration pipeline

The maintained inputs are [configure.ac](../../configure.ac), macros under `m4/`, and the root and per-directory `Makefile.am` files. Autoconf, Automake, and Libtool generate the checked-in `configure`, `Makefile.in`, and support files. Running `configure` then probes the selected compiler and target dependencies and uses `config.status` to produce `config.h`, configured headers, Makefiles, package metadata, and configured wrappers.

The ordinary sequence is:

```sh
# Start at the source root.
export PATH="$PWD/.local/bin:$PATH"
./configure
make
make staticcheck
make check
```

For an out-of-tree build, run the source tree's `configure` from a separate build directory, preserving the source-root `.local/bin` entry in `PATH`. The complete option reference and platform examples remain in [build.md](../../build.md).

Regeneration is maintenance work, not a prerequisite for every build. After modifying build inputs, use the project-local tools, for example `autoreconf -fi` for a full regeneration or `automake Makefile` for a root Makefile-only change, then inspect the generated diff. Commit each changed `Makefile.am` with its corresponding tracked `Makefile.in`. Re-run configuration before relying on changed test discovery or configured feature flags.

## Why generated files are committed

libsixel deliberately tracks generated build files, including `configure`, `aclocal.m4`, `config.h.in`, and the per-directory `Makefile.in` files, together with support files such as `build-aux/ltmain.sh`. The repository is also a way to distribute buildable source to users who will never modify the build machinery.

The maintainer's objection is explicit: **people who do not need to work on the build system should not be made to install its maintenance tools.** A Git checkout may be used to test an unreleased fix, reproduce a problem, or make a C-only change. Those activities should not require learning how to bootstrap Autoconf, Automake, M4, and Libtool, finding compatible generator versions, or diagnosing missing macros before the first compilation. Keeping the generated files current is work for the people changing the build definitions.

### Relationship to upstream guidance

The [GNU Automake manual's discussion of version control and generated files](https://www.gnu.org/software/automake/manual/html_node/Version-Control.html) presents both tracking and excluding generated files. Exclusion avoids timestamp and generator-version churn and is defended with the observation that "the job of version control is versioning, not distribution." The same section also recognizes that tracking generated files lets users build a checkout without installing the maintainer tools. It is a discussion of competing tradeoffs, not a blanket prohibition on generated files.

libsixel deliberately declines the separation of version control from source distribution in that argument. Users obtain source from Git as well as release archives, and the project gives both audiences the benefit of prepared build files. A smaller repository or quieter generated diffs does not justify requiring every ordinary source builder to install otherwise unnecessary tools. This choice complements the broader reason for retaining [Autotools + Libtool alongside Meson](README.md#why-maintain-two-build-systems).

### Costs and safeguards

This policy accepts larger diffs, generated-file conflicts, and the obligation to keep outputs synchronized with their inputs. Checkout timestamps can otherwise cause unwanted regeneration or conceal stale outputs, as the upstream manual explains. These costs need explicit handling; committing a generated file alone does not establish that it matches its source.

[configure.ac](../../configure.ac) uses `AM_MAINTAINER_MODE([disable])`, so ordinary builds do not enable the maintainer regeneration rules by default. Build-system contributors can enable maintainer mode or run the appropriate generators explicitly. A user changing compiler flags or selecting existing configure options still runs the shipped `configure`; this does not itself require regenerating that script with Autoconf.

The [generated Makefile pair guard](../../tools/check_makefile_generated_pairs.sh) checks that a staged `Makefile.am` change includes its corresponding `Makefile.in` and that the pair has no remaining unstaged changes. It checks the candidate commit, not timestamps. That guard protects the maintenance workflow; it does not regenerate files or prove semantic equivalence. Contributors must still regenerate with the intended tools and review the output.

The distinction is between generated distribution inputs and local build outputs. The former are intentionally tracked. A configured `Makefile`, `config.h`, object file, or local cache describes one build environment and is not made a repository asset by this policy. The recipient still supplies the compiler, linker, shell, make, and selected feature dependencies described in the [build requirements](../platform-support.md#build-requirements).

## Limitations and local remedies

The maintainer regards GNU make dependence and weak parallel orchestration as significant shortcomings of the Automake-based build. A long-desired replacement would retain the distribution and library-portability properties of Autotools + Libtool while removing those constraints. The available alternatives have not met that combination of requirements, so libsixel maintains this path and implements the improvements it needs locally.

### GNU make dependence

The dependency is concrete in libsixel: the root [Makefile.am](../../Makefile.am) uses GNU make's `filter` and `if` functions to select job flags and recognizes its jobserver arguments. Platform adaptations also account for particular make implementations and versions. Shipping `Makefile.in` removes generator requirements for the recipient; it does not make these recipes work with every system's native make. In this build path, readers should use a compatible GNU make, named `gmake` on systems where that distinguishes it from the native make.

This practical constraint must be distinguished from Automake's general design. Its [manual](https://www.gnu.org/software/automake/manual/html_node/General-Operation.html) describes translating some constructs for other make implementations and limitations on GNU make extensions; it also provides [portability warnings](https://www.gnu.org/software/automake/manual/html_node/automake-Invocation.html). Automake-generated Makefiles are not universally GNU-make-only by definition. The objection here concerns the dependence encountered in maintaining this build and the lack of a satisfactory replacement that removes it without losing the other required properties. The current customizations do not claim to have removed that dependence.

### Parallelism required project-specific work

Parallel compilation and parallel test rules exist in the standard machinery. The weakness is in getting enough independent work scheduled safely across the whole configure, build, and test sequence. Ordinary sequential configure probes do not become concurrent by adding `-j` to a later make invocation. Similarly, Automake's traditional [recursive directory traversal](https://www.gnu.org/s/automake/manual/html_node/Directories.html) visits subdirectories in turn; parallelism inside each child does not by itself schedule independent work across those directory boundaries. Upstream also supports layouts with less or no recursion, but libsixel retains its directory structure and supplies explicit orchestration.

| Constraint | libsixel's response | Scope of the improvement |
| --- | --- | --- |
| Repeated sequential header and function checks | [Parallel configure macros](#configure-time-parallelism) run isolated probes and collect their results. | Selected probe families are accelerated; configuration is not one fully parallel graph. |
| Serial generation of configured outputs | `AC_OUTPUT_PARALLEL` requests `config.status --jobs` when supported. | Capability detection retains a fallback for generated scripts without that option. |
| Recursive directory ordering hides independent work and can allow competing library rebuilds | [Phased build targets](#phased-recursive-builds) prepare shared inputs and the library, then expose independent consumers to make's scheduler. | Dependency barriers protect generation and linking while phase members can run concurrently. |
| Job-control propagation needs to respect the invoking make | `AM_MAKEFLAGS` supplies a default only when no explicit job control is present. | Existing `-j` and jobserver settings take precedence; `MAKEFLAGS` is not rewritten during make execution. |
| A large test inventory overflows command arguments before scheduling can begin | The [custom test harness](test-harness.md#autotools-execution-path) requests one short target and leaves individual log targets in make's dependency graph. | Ordinary platforms retain per-test parallel scheduling; aggregation and cleanup process paths incrementally. |

These changes address both throughput and correctness. Simply launching every directory or probe at once would introduce races over generated files, temporary probe outputs, and library relinking. OpenVMS/GNV still uses conservative serial build and probe paths where its execution semantics require them; its test runner handles concurrency separately. These are build-infrastructure decisions, distinct from libsixel's runtime encoder and decoder threading.

## Configure-time parallelism

Configuration itself has project-specific acceleration:

| Mechanism | Work performed | Boundaries |
| --- | --- | --- |
| `AC_CHECK_HEADERS_PARALLEL` | Runs header probes in separate working directories and collects the results into configure cache variables and definitions. | Uses the detected CPU count; separate probe directories prevent temporary-file collisions. |
| `AC_CHECK_FUNCS_PARALLEL` | Runs function link probes in separate directories, then imports their results. | Preserves relevant compiler flags and cache behavior; this is not a runtime benchmark. |
| `AC_OUTPUT_PARALLEL` | Generates `config.status`, detects whether it supports `--jobs`, and uses that option when available. | Honors `--no-create`; falls back when the generated script lacks parallel support. |

OpenVMS/GNV forces these paths to one worker because its child-status and shell behavior differ from the ordinary Unix assumptions. The OpenVMS branch also adapts generated `config.status` setup and cleanup. These exceptions are owned by [OpenVMS compatibility](../misc/platforms/openvms.md).

The root [Makefile.am](../../Makefile.am) also supplies a detected default job count to recursive makes through `AM_MAKEFLAGS` when the caller has not supplied job control. It deliberately avoids rewriting `MAKEFLAGS` inside a running make. Explicit `-j` and inherited jobserver settings remain relevant, and the OpenVMS path selects serial execution.

## Phased recursive builds

The root Makefile retains Automake's recursive directory structure but adds explicit phases through `all-am: phase2-all`. The dependency rules around `phase1-all`, `phase2-all`, and `phase3-all` establish the following ordering.

```text
configured headers and required generated inputs
    |
phase1-all
    prepare amalgamation when tools alone need it
    build src library objects
    link libsixel.la and prepare the import-library alias
    |
phase2-members-all
    compile converter and assessment objects
    run enabled tool, binding, and extension targets
    |
phase3-members-all
    finish converters, assessment, and other enabled all targets
```

This ordering makes the library available before dependent workers consume it and prevents simultaneous recursive relinks from bindings and tools. `LS_PHASED_ALL=yes` lets binding targets recognize that the library has already been prepared. Direct invocation of those binding targets can still request the prerequisite library build. The later ordinary recursive directory visits remain compatible and generally find already-built outputs.

The phase member targets can use make's parallel scheduler. This is the project's remedy for the limited concurrency of the ordinary recursive traversal: independent consumers become sibling targets after shared prerequisites have completed. The phases themselves express ordering constraints; removing them merely because a subdirectory has an `all` target can reintroduce duplicate linking or generated-file races.

OpenVMS adds the `openvms-all` entry point and one-recursive-make-per-object handling in `src/libobjs`. Those are GNV execution adaptations rather than a different library architecture.

## Library linking and generated translation units

[src/Makefile.am](../../src/Makefile.am) builds `libsixel.la` with Libtool. It owns the normal source list, dependency flags, library version information, and compiler-specific link behavior. For MSVC-ABI builds, it explicitly handles DLL driver mode and the stable import-library alias needed by consumers. The test environment uses Libtool's configured shared-library search metadata to locate build-tree libraries.

The `--enable-amalgamated-lib` and `--enable-amalgamated-tools` options are independent axes. [amalgamation/Makefile.am](../../amalgamation/Makefile.am) calls [gen-amalgamation.sh](../../tools/gen-amalgamation.sh) to produce `amalgamation/sixel.c` from source units, public and private headers, configured features, embedded completion, and registered test units. Dependency lists include nested test sources so a changed test-local symbol invalidates the generated file.

Consumers select the required parts of the generated source through compilation defines. The Autotools amalgamated test runner additionally uses `TEST_RUNNER_AMALGAMATION_DEFINES`; ordinary test-source registration alone is insufficient. In the amalgamated library path, `dither.c` is compiled separately with the split-dither contract to avoid compiler failures on an excessively large translation unit. Amalgamation therefore describes a generated source delivery mechanism, not a promise that every selected build uses exactly one object file.

The tools-only amalgamation mode prepares shared generated inputs before parallel consumer builds. A stale `sixel.c` can retain an old feature selection or test registration, so rebuilding only `tests/test_runner` is insufficient when its generator inputs have changed but the owning generation path was bypassed.

## Test, install, and distribution targets

`make check` enters the custom [test harness](test-harness.md), which builds the unified C runner and executes the discovered scripts. `make staticcheck` delegates to the separately maintained invariant suite.

`make installcheck` runs the regular TAP suite with installed command paths after installation. The root target resolves installation directories and passes them to the test directory; cross-compiling builds skip this installed-command driver. The helper `test_runner` remains test infrastructure, so installcheck should not be interpreted as a test of an installed public test-runner product.

`make distcheck` is the release-packaging validation path. It matters when a change affects distribution contents, generated files, or installation behavior. Fixtures and test scripts use bounded distribution and cleanup operations because normal full-list shell expansion becomes too large; see [command-length boundaries](test-harness.md#command-length-and-record-length-boundaries).

## Source and generated-file maintenance

Before changing this layer, inspect the root orchestration and the owning subdirectory together. Keep normal sources, distributed sources, optional feature branches, and amalgamated defines synchronized. Existing static checks cover selected invariants, including generated Makefile pairs, library source distribution, Meson palette source lists, and test-runner amalgamation defines. They do not prove complete semantic equivalence of the two build systems.
