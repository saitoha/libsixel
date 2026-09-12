# Build Support Scripts and Toolchain Adapters

`build-aux/` contains executable parts of the build system: compiler adapters, test launchers, installation helpers, and copies of upstream support programs. Their presence explains how a build description that looks conventional can accommodate unusual toolchains and thousands of tests. Some helpers translate command conventions; others deliberately bypass a narrow probe and leave real compilation or execution to later validation.

The [Autoconf macro chapter](configure-macros.md) covers the generation side in `m4/` and `configure.ac`. The [test harness chapter](test-harness.md) owns scheduling, TAP results, command-length limits, and the detailed test preparation pipeline. This chapter maps the support files to their callers and explains the compiler, runtime, and installation adapters.

## File inventory and ownership

The table covers the tracked files in `build-aux/`. A configured checkout also contains `lso-tap-driver.sh` and `resolve-test-tool-paths.sh`; their maintained sources are the `.in` files listed below. Upstream-derived support files are intentionally shipped with the source, as explained in [the generated-file policy](autotools.md#why-generated-files-are-committed).

| Files | Origin or role | Caller and purpose |
| --- | --- | --- |
| [cosmocc-meson](../../build-aux/cosmocc-meson) | Project compiler adapter. | Cosmopolitan Meson CI selects it through `CC` and `LD`; bypasses selected sanity-check outputs, delegates other commands to `cosmocc`. |
| [pcc-meson](../../build-aux/pcc-meson) | Project compiler adapter. | PCC Meson CI selects it through `CC`; presents a GCC-compatible identity and delegates normal compilation and linking to PCC. |
| [emcc-meson](../../build-aux/emcc-meson) | Project compiler and output adapter. | Emscripten cross files select it; bypasses selected sanity checks and makes an extensionless Node entry point for `.js` output. |
| [ape-wrapper](../../build-aux/ape-wrapper) | Project test-command adapter. | Cosmopolitan CI passes it to `meson test --wrapper`; substitutes a shell when the test command has the recognized APE header. |
| [meson-cross/emscripten](../../build-aux/meson-cross/emscripten), [meson-cross/emscripten-no-threads](../../build-aux/meson-cross/emscripten-no-threads) | Project cross-machine descriptions. | Meson setup selects Emscripten tools and a little-endian `wasm32` host. See the filename caveat below. |
| [meson-native/msvc.ini](../../build-aux/meson-native/msvc.ini) | Project native-machine description. | Meson setup selects `cl` and `lib` after the Visual Studio environment is initialized. |
| [ar-lib](../../build-aux/ar-lib) | Automake's Microsoft librarian adapter. | Autotools invokes `lib.exe` through an `ar`-style command interface. |
| [compile](../../build-aux/compile) | Automake's compiler adapter. | Generated compiler rules accommodate missing `-c -o` support and translate MSVC-style compiler commands and paths. |
| [ltmain.sh](../../build-aux/ltmain.sh) | Libtool's shell implementation. | `LT_INIT` and generated `libtool` combine it with configure's platform and linker results. |
| [config.guess](../../build-aux/config.guess), [config.sub](../../build-aux/config.sub) | GNU configuration support. | Canonical system detection and normalization of supplied machine names during configuration. |
| [depcomp](../../build-aux/depcomp) | Automake dependency adapter. | Generated dependency-tracking rules translate compiler-specific dependency output. |
| [install-sh](../../build-aux/install-sh), [missing](../../build-aux/missing), [py-compile](../../build-aux/py-compile) | Automake support programs. | Portable installation fallback, missing-maintainer-tool diagnostics, and Python byte-compilation support in generated rules. Their inclusion does not mean every selected build executes them. |
| [test-driver](../../build-aux/test-driver) | Automake's basic exit-status driver. | Default generated test-driver machinery; the project's TAP extension rules select `lso-tap-driver.sh` instead. |
| [lso-tap-driver.sh.in](../../build-aux/lso-tap-driver.sh.in) | Project TAP consumer and launcher template. | Configured by Autotools, called by test log rules and staticcheck; resolves interpreters, captures output, applies timeouts, and emits `.trs` results. |
| [read-check-test-list.sh](../../build-aux/read-check-test-list.sh) | Shared discovery policy. | Autotools configure, Meson setup, and distribution rules obtain the runnable script inventory. |
| [resolve-test-tool-paths.sh.in](../../build-aux/resolve-test-tool-paths.sh.in) | Project configured path resolver. | Autotools `check-TESTS` chooses Libtool wrappers, real executables, or installcheck command paths. |
| [resolve-config-h-exports.sh](../../build-aux/resolve-config-h-exports.sh) | Shared feature exporter. | Both test setups export enabled `config.h` macros without repeating parsing in every test. |
| [resolve-binding-test-common.sh](../../build-aux/resolve-binding-test-common.sh) | Shared binding helper library. | Binding resolvers source shell quoting and platform-specific shared-library lookup functions. |
| [resolve-python-test-venv.sh](../../build-aux/resolve-python-test-venv.sh), [resolve-ruby-test-venv.sh](../../build-aux/resolve-ruby-test-venv.sh), [resolve-perl-test-venv.sh](../../build-aux/resolve-perl-test-venv.sh), [resolve-php-test-venv.sh](../../build-aux/resolve-php-test-venv.sh) | Project binding-environment resolvers. | Both harnesses and language-specific test targets prepare reusable isolated package environments. |
| [meson-test-with-failures.sh](../../build-aux/meson-test-with-failures.sh) | Project reporting adapter. | Invoked with `meson test` arguments to preserve the test status and append a failed-name list from `testlog.json`. |
| [sixel-apply-shebang.sh](../../build-aux/sixel-apply-shebang.sh), [shebang/node](../../build-aux/shebang/node) | Project executable-output adapter and template. | Optional Autotools and Meson post-link rules make selected script outputs directly executable. |
| [install-emscripten-sidecar.sh](../../build-aux/install-emscripten-sidecar.sh) | Project Meson install helper. | Converter install hooks place an external Wasm module next to its JavaScript program. |
| [sanitizer-ignorelist.txt](../../build-aux/sanitizer-ignorelist.txt) | Project sanitizer policy input. | Both build descriptions scope selected experimental integer checks away from bundled STB conversion code. |

The [CI step implementation](../../.github/actions/ci-steps/action.yml) adds the source tree's `build-aux` to `PATH` for relevant jobs. The [CI matrix](../../.github/workflows/ci.yml) selects the compiler wrappers, cross files, and test wrapper. A wrapper's presence in the repository does not cause Meson to discover or enable it automatically. Local invocations must also make the selected wrapper discoverable while keeping the project-local tools first, for example `PATH="$PWD/.local/bin:$PWD/build-aux:$PATH"` from the source root.

## Meson compiler adapters

### Cosmopolitan: bypass one setup probe, then use the real compiler

`cosmocc-meson` examines the argument immediately following `-o`. If that output argument matches `sanitycheck*.exe` or `sanity_check_for_*.exe`, it copies the host's `true` executable to that name and returns success. All other invocations use `exec cosmocc "$@"`.

This is a deliberate bypass of Meson's initial executable sanity check. It accommodates a toolchain whose output and launch conventions do not fit that check; it does not compile the sanity source or establish that a Cosmopolitan executable can run. The two filename patterns accommodate different Meson sanity-check naming conventions. A change to those names can stop the interception from applying.

The real library, converters, subsequent compiler probes, and runtime tests remain necessary evidence. Diagnose setup success and target execution separately: the copied `true` belongs only to the intercepted check, while ordinary outputs must come from `cosmocc`.

### PCC: translate compiler identity and selected preprocessing

`pcc-meson` is a Python program because it needs to dispatch several different compiler-query forms without flattening argument lists. It reports GCC-like answers to Meson while preserving PCC as the compiler for ordinary compile and link commands. The root [meson.build](../../meson.build) separately compiles a `__PCC__` probe to recognize the actual backend and apply PCC-specific workarounds.

| Invocation or setting | Adapter behavior | Interpretation |
| --- | --- | --- |
| Single-argument `--version`, `-dumpmachine`, `-dumpversion` | Query GCC; if it cannot be launched, emit built-in fallback identity strings. | The reported GCC identity is a detection aid, not the identity of the compiler producing ordinary objects. The fallback machine string is `x86_64-linux-gnu`, so it is not a portable target detector. |
| `-print-search-dirs`, `-Wl,--version` | Query GCC's search paths or `ld`'s version when the helper can be launched. | These queries describe the compatibility tools used for detection. |
| Arguments containing `-E`, without `-c` or `-S` | Try GCC preprocessing; fall back to PCC only if GCC cannot be launched. | Avoids PCC preprocessor failures encountered in Meson's header probes. A preprocessing result is therefore not necessarily produced by PCC. |
| Ordinary compilation and linking | Prepend `-D__cold__=` unless explicitly disabled, then execute PCC. | Works around older PCC packages rejecting the attribute in newer libc headers. |
| `SIXEL_REAL_PCC`, `SIXEL_PCC_MESON_GCC`, `SIXEL_PCC_MESON_LD` | Override the executable names, defaulting to `pcc`, `gcc`, and `ld`. | These identify real programs behind the adapter. |
| `SIXEL_PCC_KEEP_ATTRIBUTES=1` | Suppress the injected `__cold__` definition. | Useful when evaluating a PCC/header combination that can handle the attribute itself. |

Fallback means failure to launch a helper, not every nonzero helper result. A GCC command that runs and reports a compilation error keeps that error. Successful Meson setup through this adapter must be interpreted together with actual PCC compilation and the test results.

### Emscripten: sanity adaptation and a Node entry point

`emcc-meson` uses the same two sanity-output patterns as `cosmocc-meson` and substitutes the host's `true` for those checks. Other invocations run `emcc`. After a successful command whose recorded `-o` output ends in `.js`, it creates a sibling file with that suffix removed, prepends `#!/usr/bin/env node`, copies the JavaScript payload, and makes the sibling executable.

For example, an output named `img2sixel.js` gains an executable sibling named `img2sixel`. This lets shell tests invoke a command-shaped path while Node runs the generated JavaScript. The helper does not turn Wasm into a native executable. Its sanity bypass, JavaScript launcher, Wasm payload, and runtime behavior are separate parts of the path.

## APE test-command adaptation

`ape-wrapper` receives a command path followed by its arguments. It reads the first line of that command. If the line is exactly `MZqFpD='`, it discards that command path and executes `sh` with the remaining arguments. Otherwise it executes the original command with those arguments.

The Cosmopolitan jobs select this through `meson test --wrapper`. The ordinary TAP registrations in [tests/meson.build](../../tests/meson.build) launch a shell and pass a script file as its argument, which is the context for this shell substitution. This is not a general-purpose APE loader: invoking it on an arbitrary APE program would discard the program rather than load its machine code. The replacement `sh` is resolved through the current `PATH`, so the environment matters as much as the small wrapper itself.

Keep this distinct from `cosmocc-meson`: the compiler adapter acts during Meson setup and build; `ape-wrapper` acts on a test command during execution.

## Wasm configuration, execution, and installation

### Cross files and feature selection

Both Emscripten cross files declare `emcc-meson`, `emar`, `emranlib`, `emstrip`, and `llvm-nm`, plus a little-endian `wasm32` host with system `emscripten`. Their `[properties]` sections contain an empty executable suffix, `needs_exe_wrapper = true`, and `exe_wrapper = 'node'`.

These declarations have a concrete limitation: Meson's [cross-compilation interface](https://mesonbuild.com/Cross-compilation.html#binaries) places `exe_wrapper` in `[binaries]`, while `needs_exe_wrapper` belongs in `[properties]`. [Meson 1.8.3's environment implementation](https://github.com/mesonbuild/meson/blob/1.8.3/mesonbuild/environment.py) looks up the wrapper as a binary entry, so the files' property does not register Node as Meson's executable wrapper. Its [executable-target implementation](https://github.com/mesonbuild/meson/blob/1.8.3/mesonbuild/build.py) also names Emscripten targets with `.js`; the property `exe_suffix = ''` does not override that target rule. The project's shell tests can use the extensionless entry points created by `emcc-meson`, but that execution path is distinct from Meson's own cross-binary execution support.

The files named `emscripten` and `emscripten-no-threads` currently have identical contents. The latter is selected by the Emscripten UBSan CI entries, but its name alone does not disable threads. Thread selection and Emscripten link behavior also depend on the root build's probes, project options, and CI arguments. Treat the selected file and effective compiler/linker commands as the evidence.

The root build owns options such as `emscripten_noderawfs`, `emscripten_environment`, `emscripten_allow_memory_growth`, `emscripten_single_file`, and `emscripten_wasm_bigint`. The cross file establishes the toolchain and host; it does not contain the complete runtime configuration. [The build guide](../../build.md) owns the option reference.

### Shebang application

`sixel-apply-shebang.sh` accepts a template path and one or more target paths. It verifies that the files exist, checks the first two target bytes, and leaves an existing shebang intact while ensuring executable permission. Otherwise it writes the template and target contents to a temporary sibling, inserts a separating newline when necessary, replaces the target, and makes it executable. Reapplication does not stack another shebang onto an already prefixed target.

Autotools selects this through `--with-shebang-file` and post-link stamp rules in the converter, assessment, and test Makefiles. Meson selects it through `shebang_file` and its custom targets. `shebang/node` supplies the Node template. This optional generic helper and `emcc-meson`'s `.js` sibling creation are different mechanisms: one updates specified targets in place; the other creates an additional entry point from compiler output.

### Split installation and Wasm sidecars

When Emscripten output is split rather than embedded with `SINGLE_FILE=1`, installing only the JavaScript command can leave a program that starts Node but cannot find its Wasm module. The converter build definitions express the following intended split layout; the Meson naming boundary below requires separate attention:

| Installed location | Contents | Owner |
| --- | --- | --- |
| `bindir/img2sixel`, `bindir/sixel2png` | Small shell launchers that execute Node on the configured private program path. | [img2sixel-node-launcher.in](../../converters/img2sixel-node-launcher.in), [sixel2png-node-launcher.in](../../converters/sixel2png-node-launcher.in). |
| `libexecdir/libsixel/` | The generated JavaScript programs. | [converters/Makefile.am](../../converters/Makefile.am), [converters/meson.build](../../converters/meson.build). |
| Beside each private program | The corresponding `.wasm` module. | Autotools install hooks or Meson's `install-emscripten-sidecar.sh`. |

The sidecar helper takes `WRAPPER_PATH INSTALL_DIR MODULE_NAME`, looks for `${WRAPPER_PATH}.wasm`, and installs it as `${MODULE_NAME}.wasm`. If there is no sidecar, it returns successfully, accommodating single-file output. For a relative destination it uses `MESON_INSTALL_DESTDIR_PREFIX`, or the install prefix and `DESTDIR`; for an absolute destination it prepends `DESTDIR`. The launcher embeds the configured installation path, while `DESTDIR` selects the staging location. A staged installation and its eventual runtime location are consequently different concepts.

Under Meson 1.8.3's `.js` target naming, the current Meson hook passes an executable `full_path()` such as `img2sixel.js`, causing the helper to look for `img2sixel.js.wasm`. The installed shell launcher instead refers to the extensionless private program `img2sixel`, while Meson's executable target retains its `.js` name. The compiler wrapper's extra extensionless file is not itself an additional Meson install target. These source-level naming mismatches need resolution and installed-command validation before this Meson layout can be treated as equivalent to the Autotools installation hooks. Successful helper exit when a sidecar is absent can conceal the mismatch; build-tree shell tests do not establish that the installed layout works.

This layout is specifically implemented for the installed converters. It should not be generalized into a claim that every test helper or assessment executable has identical sidecar installation handling. Validate installed-command behavior as well as build-tree execution when changing it.

## MSVC: compiler, archiver, and library linking are separate

The native Meson file simply selects `c = 'cl'` and `ar = 'lib'`. It expects Visual Studio's environment initialization to have already populated `PATH`, `INCLUDE`, and `LIB`. The CI helper can layer a generated `msvc-vcpkg.ini` over it for package metadata. Neither file supplies the compiler installation or eliminates shell requirements in the project's generators and tests.

Autotools needs adapters because its generated rules and Libtool expect Unix-style command interfaces. `configure.ac` detects MSVC intent before generic compiler discovery and `LT_INIT`, then selects the corresponding companion tools. Unless the caller supplied `AR`, it sets `AR` to the configured shell followed by `ar-lib lib`; it also selects `ARFLAGS=cr`, `dumpbin -symbols`, and no-op ranlib/strip defaults where appropriate.

| Layer | Responsibility |
| --- | --- |
| `compile` | Translate compiler command conventions, including MSVC argument and path forms and compilers lacking combined `-c -o` support. |
| `ar-lib` | Translate archive operations into Microsoft librarian commands. For example, `ar-lib lib cr archive.lib object.obj` constructs a `lib -NOLOGO -OUT:...` command. |
| `m4/libtool.m4`, `ltmain.sh`, generated `libtool` | Determine and execute shared/static library linking, naming, export, and build-tree versus installed-library behavior. |
| `configure.ac` and `src/Makefile.am` | Select the toolchain and add libsixel's DLL/link-driver and import-library dependency handling. |

`ar-lib` supports creation/replacement, listing, deletion, extraction, and `@FILE` member lists, translating paths for the detected MSYS/MinGW, Cygwin, or Wine environment. Its index operation requires no separate ranlib because the librarian updates the index implicitly. Some modifiers, including `u` and `v`, are accepted but ignored. It is an interface adapter for the archive operations the build needs, rather than a complete reimplementation of every `ar` behavior.

The checked-in `ar-lib` identifies itself as an Automake-maintained script. The local engineering here includes arranging for the correct adapter and native tools to be used; not every file in this directory is a libsixel-authored hack. [MSVC compatibility](../misc/platforms/msvc.md) owns the wider platform constraints.

## Test and instrumentation support

The [test preparation and driver sections](test-harness.md#support-scripts-before-each-test) describe the configured path resolver, enabled-feature export, binding package caches, timeout selection, streaming, and the difference between the basic Automake driver and the project TAP driver. These helpers reduce repeated shell work and keep each test attached to the selected build's tools and packages.

`meson-test-with-failures.sh` is a smaller convenience wrapper: it forwards its arguments to `meson test`, remembers the status, locates `meson-logs/testlog.json` using `-C`, and extracts failed names with AWK. It recognizes failure, error, timeout, interruption, and unexpected-pass results. It returns Meson's original status even if the log is missing. Its line-oriented JSON extraction is reporting support, not a second result authority or scheduler.

`sanitizer-ignorelist.txt` lists bundled `stb_image.h` and `stb_image_write.h` for selected integer sanitizer categories. Those upstream image routines intentionally narrow or wrap intermediate values; stopping there can prevent experimental checks from reaching project code. The root build descriptions attach this input to relevant sanitizer profiles. The exclusions are scoped to those source patterns and categories, not a global exemption of the loaders from all sanitizers.

## Maintaining an adapter

Identify whether a change affects setup detection, actual compilation, test-command execution, or installation. A successful bypassed sanity check cannot validate a real compiler output, and a passing build-tree test cannot validate sidecar installation. Keep the wrapper, its selected CI command, and the owning build rules in view together.

For upstream-derived files, inspect the header and the tracked diff before replacing them with a tool-installed copy. For configured project helpers, edit the `.in` source and regenerate the local output through configuration. Update the appropriate distribution list when adding support files; the existence of a tracked file alone does not establish that an Autotools source archive contains it.
