# Meson Configuration and Build Graph

## Independent configuration

The root [meson.build](../../meson.build) and [meson_options.txt](../../meson_options.txt) own Meson's feature selection, compiler and function probes, target dependencies, and configuration summary. Per-directory `meson.build` files define the library, converters, assessment tools, extensions, language packages, and tests.

Meson creates `config.h` using `configuration_data()` and `configure_file()`. It configures public headers before consumers such as binding constant generators and builds target relationships directly through library dependencies and generated targets. It does not read the configured Autotools Makefiles or reproduce their recursive phase driver.

```sh
# Start at the source root.
export PATH="$PWD/.local/bin:$PATH"
meson setup builddir
meson compile -C builddir
meson compile -C builddir staticcheck
meson test -C builddir --suite tap --print-errorlogs
```

Feature options use `enabled`, `disabled`, or `auto` where declared as Meson feature values; boolean options such as `tests` and `amalgamated_lib` use `true` or `false`. Do not translate an Autotools spelling mechanically. The complete list is in [build.md](../../build.md#meson-build-options), and `meson configure builddir` shows the selected configuration.

## Generated inputs and dependencies

The root build generates the embedded completion header once, before converters and optional amalgamation consume it. When `amalgamated_lib` or `amalgamated_tools` is enabled, [amalgamation/meson.build](../../amalgamation/meson.build) invokes the shared generator. [src/meson.build](../../src/meson.build) then chooses the normal or amalgamated library inputs, while [converters/meson.build](../../converters/meson.build) chooses the converter inputs.

Meson's dependency graph replaces the need for Autotools' explicit recursive phases, but it still needs complete source and generator input lists. A new C implementation can compile under Autotools and remain absent from the Meson library or amalgamation list. Selected source-list static checks guard known boundaries; source review remains necessary for the full graph.

Compiler and runtime adapters are still part of this build. Examples include compiler identity wrappers for PCC and Cosmopolitan, Emscripten configuration and executable sidecars, Windows DLL lookup, and the MSVC tool and path setup in CI. Native MSVC code generation does not remove the POSIX-shell dependencies of this repository's generators and TAP suite. Read the owning [platform compatibility document](../misc/platforms/README.md) before generalizing a successful compiler probe into an end-to-end platform claim.

## Test registration and execution

[tests/meson.build](../../tests/meson.build) discovers runnable scripts through the same `read-check-test-list.sh` helper used by Autotools. It separately scans test C and header files, excludes `*.inc.c` fragments and private environment/artifact trees, and constructs a single `test_runner` executable. That executable also includes selected converter and extension helper sources and depends on `libsixel_dep`.

Each discovered script becomes a separate Meson `test()` registration in the `tap` suite, with a 90-second per-file timeout. Meson schedules and parses ordinary TAP tests itself. The configured [meson-python-tap-wrapper.sh](../../tests/meson-python-tap-wrapper.sh), despite its historical name, prepares shell tests and multiple binding languages: it selects executable paths, handles runtime environments, and lazily prepares shared binding environments and large fixtures with coordination between workers.

Expected-failure cases add [meson-tap-exitcode-wrapper.sh](../../tests/meson-tap-exitcode-wrapper.sh). The adapter maps `not ok` to failure status, a passing observation to success, and a whole-file skip to status 77. These cases use Meson's `exitcode` protocol with `should_fail`, so an unexpected pass remains a failure of the expectation. The tests themselves retain the repository's TAP assertion contract.

Meson writes its own logs under `meson-logs/`. It does not execute the custom Automake `check-TESTS` collector and does not use its `.trs` summary format. [meson-test-with-failures.sh](../../build-aux/meson-test-with-failures.sh) is a reporting adapter that runs `meson test`, preserves its status, and prints failed names from its JSON log; it is not another scheduler.

## Discovery and regeneration

Both runtime script discovery and C test-source discovery run during Meson setup. A new or renamed file may not appear in an existing configured graph until Meson is reconfigured:

```sh
meson setup --reconfigure builddir
meson compile -C builddir
meson test -C builddir --list
```

Automatic C source discovery does not create a C dispatch-table entry or a TAP wrapper. Those still belong to the test's implementation. It also differs from the explicit Autotools C source list and amalgamated runner defines; see [test registration](test-harness.md#shared-discovery-and-c-test-dispatch).

## Static checks, installed commands, and documentation

The `staticcheck` run target invokes the shared `tests/_static/sh/staticcheck-suite.sh`. The source-level invariant set is shared, but checks that require configured Autotools Makefiles can skip in a Meson build. That is a coverage boundary, not proof that a skipped compiler or linkage contract passed.

After installation, `meson compile -C builddir installcheck` runs the regular TAP suite using the `installcheck` test setup and installed command locations. The root run target prepares the relevant paths, while the test wrapper permits them to override build-tree defaults. The [remaining differences](meson-history.md#remaining-differences-and-gaps) describe why this should not be confused with complete equivalence of packaging behavior.

`meson compile -C builddir docs` delegates to the same optional `tools/build-docs` program as Autotools. Documentation navigation and private Python dependencies are shared; there is no second maintained Meson documentation tree.

## Relationship to Autotools

Meson supports the main library, converter, binding, extension, test, analyzer, sanitizer, and amalgamation surfaces. The maintained build descriptions nevertheless differ in concrete details, including static consumer metadata, WIC deregistration, and the construction of the C runner when amalgamated tools are selected. [Meson adoption and remaining differences](meson-history.md) records those distinctions and their evidence rather than assuming complete parity from successful compilation.
