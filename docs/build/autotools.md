# Autotools Configuration and Build Orchestration

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

The phase member targets can use make's parallel scheduler. The phases themselves express ordering constraints; removing them merely because a subdirectory has an `all` target can reintroduce duplicate linking or generated-file races.

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
