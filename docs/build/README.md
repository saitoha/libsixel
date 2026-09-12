# Build System Architecture

libsixel maintains two build descriptions: Autotools with Libtool, and Meson with its selected backend, normally Ninja. Both build the C library, converters, optional integrations, and tests. They share source code and several generators and test helpers, but each owns its own configuration probes and dependency graph. Meson does not invoke the Autotools build.

The unusual parts of this system address concrete costs: repeated compiler probes, recursive build ordering, large generated translation units, thousands of test scripts, expensive process creation, and operating systems with different command, path, and file-record semantics.

## Reading guide

- [Autotools configuration and build orchestration](autotools.md) follows generated files, configure-time parallelism, phased recursive builds, Libtool, and amalgamation.
- [Meson configuration and build graph](meson.md) follows feature detection, generated targets, test registration, packaging, and execution wrappers.
- [Test harness and command-length limits](test-harness.md) explains the shared test inventory, unified C runner, custom Automake driver and result collection, and the separate limits encountered during testing, cleanup, and distribution.
- [Meson adoption and remaining differences](meson-history.md) records the introduction and subsequent integration, distinguishes implementation gaps from execution differences and unverified platforms, and points to the owning evidence.

For commands and the complete option inventory, use the repository's [build guide](../../build.md). [Platform support](../platform-support.md) owns requirements and CI-backed support claims; the [platform compatibility ledger](../misc/platforms/README.md) owns individual portability constraints. The [testing guide](../testing/guide.md) describes how to write tests, and [staticcheck](../testing/staticcheck.md) describes repository invariant verification.

## Ownership map

| Concern | Autotools owner | Meson owner | Shared input or helper |
| --- | --- | --- | --- |
| Compiler, platform, and optional dependency detection | `configure.ac` | Root `meson.build`, `meson_options.txt` | C headers, feature names, target libraries and frameworks |
| Configured headers and package metadata | `config.status`, `include/Makefile.am` | `configure_file()`, `include/meson.build` | Public header and pkg-config templates |
| Library and program build graph | Root and per-directory `Makefile.am`, Libtool | Root and per-directory `meson.build` | C and Objective-C sources |
| Embedded completion and amalgamation | Generated-file rules and `BUILT_SOURCES` | `custom_target()` and target dependencies | Completion generator and `tools/gen-amalgamation.sh` |
| Runtime test discovery | Configure-time `SIXEL_CHECK_TESTS`, or an OpenVMS list file | Setup-time `run_command()` | `build-aux/read-check-test-list.sh` |
| Runtime test execution | Custom `check-TESTS`, `.log` rules, `lso-tap-driver.sh` | Meson `test()` with TAP and a shell wrapper | Individual test scripts, C dispatch names, feature exports, binding environment helpers |
| Repository invariant checks | `make staticcheck` | `meson compile -C builddir staticcheck` | `tests/_static/sh/staticcheck-suite.sh` |
| HTML documentation | `make docs` | `meson compile -C builddir docs` | `tools/build-docs`, `docs/README.md`, MkDocs hook |

The shared inputs reduce duplication but do not make the two build descriptions interchangeable. Adding a library source, optional dependency, generated input, or test dispatch entry still requires checking every applicable normal and amalgamated path.

## Generated files and configuration boundaries

The Git checkout intentionally contains generated Autotools files such as `configure` and `Makefile.in`. A normal build can use those files directly. A maintainer changes `configure.ac` or `Makefile.am`, regenerates the corresponding outputs with project-local tools, and commits the source and generated changes together. A configured `Makefile` and `config.h` describe one build directory; they are not substitutes for the maintained build inputs.

Meson creates its configuration, backend files, and introspection metadata inside its build directory. Its configuration probes must agree with the intended Autotools feature contract, but a Meson build does not produce Autotools `Makefile` outputs. Use separate build directories when comparing configurations, and inspect their summaries and `config.h` files rather than assuming that similarly named defaults selected the same dependencies.

## Build and validation entry points

The examples below start at the source root with an existing configured build. Put project-local tools first in `PATH`:

```sh
export PATH="$PWD/.local/bin:$PATH"

# Autotools build and validation.
make
make staticcheck
make check

# Meson build and validation in a separate configured directory.
meson compile -C builddir
meson compile -C builddir staticcheck
meson test -C builddir --suite tap --print-errorlogs
```

`staticcheck` and the runtime suite have different observations. Passing the runtime suite does not prove source-list, generated-file, public-interface, or documentation synchronization. A skip also does not prove the skipped contract; build-system-specific skips are discussed in [remaining differences](meson-history.md#remaining-differences-and-gaps).

## Documentation and source distribution

HTML documentation is optional. `tools/build-docs` creates or reuses a private environment under `.docs-build/venv`, runs a strict MkDocs build, and validates local rendered media references. Normal C configuration, compilation, testing, and installation do not require the documentation packages. The [MkDocs hook](../../tools/docs/mkdocs_hooks.py) derives navigation from `docs/README.md` and linked directory indexes, and rejects Markdown pages that are missing from that navigation.

New documentation must also be included in the Autotools distribution list in the root [Makefile.am](../../Makefile.am), with its tracked `Makefile.in` regenerated. The test tree has additional distribution rules to keep large inventories out of shell command arguments; those rules are explained in the [test harness chapter](test-harness.md#command-length-and-record-length-boundaries).
