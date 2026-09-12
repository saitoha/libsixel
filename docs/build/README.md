# Build System Architecture

libsixel deliberately maintains **Autotools + Libtool** and **Meson with its selected backend, normally Ninja**, because their strengths cover different environments. Autotools + Libtool provides portable source distribution and adaptable toolchain control; Meson provides a practical build path where the shell-based build's execution cost is too high, especially on Windows. Autotools + Libtool remains necessary where Meson cannot provide a usable build path. Maintaining both is an ongoing portability strategy.

The combined name identifies two responsibilities: Autoconf and Automake provide configuration probes and Makefile generation, while Libtool handles portable library compilation and linking, including shared and static libraries and platform-specific link conventions. References to the shorter "Autotools build" elsewhere in these chapters include the Libtool integration unless they identify a specific generator or make rule.

Both build the C library, converters, optional integrations, and tests. They share source code and several generators and test helpers, but each owns its own configuration probes and dependency graph. Meson does not invoke the Autotools build.

The unusual parts of this system address concrete costs: repeated compiler probes, recursive build ordering, large generated translation units, thousands of test scripts, expensive process creation, and operating systems with different command, path, and file-record semantics.

## Why maintain two build systems?

For libsixel's requirements, there is still no complete replacement for Autotools + Libtool. Replacing that combination would require preserving source distribution without generator dependencies at the receiving end, portability of both configuration and library linking, customization, and understandable control for maintainers who know what the toolchain must do. A build tool can improve particular parts of that experience without satisfying the whole set of requirements.

Retaining this system comes with substantial dissatisfaction. The maintainer has long wanted a successor that preserves those properties while removing the dependence on GNU make encountered in the Automake-based build and improving parallel execution. In the maintainer's assessment, build-tool development has not moved in that desired direction: newer tools solve useful problems, but none has delivered the replacement this project needs. The decision to keep Autotools + Libtool reflects that unmet requirement, alongside its strengths.

libsixel has addressed parallelism weaknesses with its own configuration probes, phased build scheduling, and test orchestration. These are necessary engineering work that the standard machinery did not provide for this project's layout and scale. They improve the retained build path while leaving its GNU make dependency in place. [Limitations and local remedies](autotools.md#limitations-and-local-remedies) separates the maintainer's objections, upstream capabilities, and the concrete adaptations.

### Autotools + Libtool separates maintainer tools from distribution requirements

Autoconf and Automake run on the maintainer's machine to produce `configure` and `Makefile.in`; Libtool support files are prepared as part of that process. Shipping those generated files lets the recipient configure and build the C software without installing Autoconf, Automake, GNU M4, or a separate Libtool installation just to regenerate the build system. It also avoids requiring Meson and its Python runtime at that receiving end.

This is the relevant meaning of distribution without build-generator dependencies. The recipient still needs a suitable shell, make, ordinary command-line utilities, a compiler and linker, and dependencies for the selected product features. Optional bindings and documentation have their own requirements. Regenerating the build system is a separate maintainer operation, as explained in [the Autotools + Libtool chapter](autotools.md#configuration-pipeline).

That separation matters when bringing up the build tool and its runtime would itself be a significant porting task. The generated configuration script and Makefiles travel with the C source and can be adapted to the tools already available on the receiving platform.

libsixel extends this principle to Git checkouts by committing generated files such as `configure` and `Makefile.in`. The maintainer strongly objects to making people install tools they have no need to work with. Someone who clones the repository to build the library, try a fix, or change ordinary C code has not thereby volunteered to maintain its build system. The project accepts the work of keeping generated files current so those users do not have to bootstrap the build generators. [Why generated files are committed](autotools.md#why-generated-files-are-committed) explains the upstream guidance, the deliberate repository policy, and its maintenance costs.

### Autotools + Libtool exposes the operations that maintainers need to control

Autotools lets the project express compiler and linker probes, cache their results, inspect diagnostics, and add narrowly scoped shell or make rules. This is useful when a platform needs an unusual archiver, a native link wrapper, different process-status handling, or a custom test launcher. The [OpenVMS integration](../misc/platforms/openvms.md) and [test harness](test-harness.md) are concrete examples of that customization in libsixel.

Libtool contributes the library-specific part of this portability: compiler flags for shared-library objects, archive and shared-library construction, platform naming and versioning conventions, and build-tree versus installed-library linking. [src/Makefile.am](../../src/Makefile.am) expresses this through `libsixel.la` and supplements it with targeted platform rules. Replacing configure and Makefile generation alone would leave this library and linking work to be replaced as well.

For a maintainer who understands the compiler, preprocessor, linker, archiver, shell, and make, these operations are also a useful form of clarity. A failed configure probe can be traced through `config.log` to the command and diagnostic; a build recipe can be inspected and reproduced at the toolchain boundary. Familiarity with those operations makes the ability to spell them out valuable. This is a claim about control and comprehensibility for that audience, not a claim that M4 or large generated scripts are easy for every newcomer.

### Meson addresses the cost of Autotools + Libtool on Windows

The shell-oriented execution of Autotools + Libtool can be expensive on Windows. Configuration probes, recursive makes, Libtool wrappers, and small utility invocations create many short-lived processes. POSIX compatibility environments add process and path-handling costs that can make a technically working build impractically slow. Adding parallel jobs does not eliminate the work needed to launch and coordinate those processes.

Meson with Ninja supplies a practical path in that environment: Meson constructs the dependency graph and the backend schedules compiler and linker commands without reproducing the recursive shell-driven orchestration. This is why libsixel needs Meson even though its Autotools + Libtool path supports several Windows toolchains. The benefit concerns build orchestration; it does not make the C compiler intrinsically faster or remove every shell invocation. libsixel's generators and shared TAP tests still use shell helpers, and the [testing guide](../testing/guide.md#process-and-file-cost) separately limits their process cost.

### Autotools + Libtool covers environments that Meson does not adequately serve

The reverse requirement is equally important. A usable Meson path depends on the availability of its runtime and backend, recognition of the compiler and platform, and support for the required linking and execution semantics. Where those pieces are unavailable or do not function adequately, the project still needs the configurable Autotools + Libtool path.

OpenVMS/GNV is the concrete maintained example in libsixel: its Autotools + Libtool path includes explicit adaptations for native linking, file records, and process behavior, while a corresponding Meson path is not continuously verified. That evidence establishes which path the project can rely on today; it does not establish that a future Meson port is impossible. [Platform support](../platform-support.md) and [Meson differences](meson-history.md#remaining-differences-and-gaps) distinguish implemented support from missing validation.

The two systems are therefore complementary. Keeping both costs source-list maintenance, matching feature probes, and validation of both graphs. libsixel accepts that cost to retain usable builds across its target environments. Sharing generators, test discovery, TAP observations, and static checks reduces duplicated work while each build system retains the mechanisms suited to its environments.

## Reading guide

- [Autotools + Libtool configuration and build orchestration](autotools.md) follows generated files, configure-time parallelism, phased recursive builds, library linking, and amalgamation.
- [Meson configuration and build graph](meson.md) follows feature detection, generated targets, test registration, packaging, and execution wrappers.
- [Test harness and command-length limits](test-harness.md) explains the shared test inventory, unified C runner, custom Automake driver and result collection, and the separate limits encountered during testing, cleanup, and distribution.
- [Meson adoption and remaining differences](meson-history.md) records the introduction and subsequent integration, distinguishes implementation gaps from execution differences and unverified platforms, and points to the owning evidence.

For commands and the complete option inventory, use the repository's [build guide](../../build.md). [Platform support](../platform-support.md) owns requirements and CI-backed support claims; the [platform compatibility ledger](../misc/platforms/README.md) owns individual portability constraints. The [testing guide](../testing/guide.md) describes how to write tests, and [staticcheck](../testing/staticcheck.md) describes repository invariant verification.

## Ownership map

| Concern | Autotools + Libtool owner | Meson owner | Shared input or helper |
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

The Git checkout intentionally contains generated Autotools files such as `configure` and `Makefile.in`, applying the same low-dependency distribution principle to repository users as to tarball users. A normal build can use those files directly. A maintainer changes `configure.ac` or `Makefile.am`, regenerates the corresponding outputs with project-local tools, and commits the source and generated changes together. A configured `Makefile` and `config.h` describe one build directory; they are not substitutes for the maintained build inputs.

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
