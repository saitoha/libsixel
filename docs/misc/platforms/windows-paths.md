# Windows Cross-Runtime and Path Compatibility

## Scope

Windows support is not a single compiler switch. libsixel deliberately treats the environment that drives configure, Meson, make, and the TAP suite as independent from the ABI and C runtime of the executable being produced. A Cygwin or MSYS shell can therefore build and test a native MSVC executable, while an MSYS2 `CLANG64` shell builds a native MinGW-w64/UCRT executable rather than an MSYS executable. This document owns the boundaries where paths, process arguments, executable discovery, and DLL lookup cross those environments.

Windows is a combinatorial platform rather than a boolean support property. The execution environment, including Wine in the broad sense, the build/test driver, compiler and driver mode, CRT, ABI, architecture, and build system can vary independently enough that an unqualified statement such as “Windows is supported” has no precise maintenance meaning. Any support claim or regression report must name the relevant tuple; otherwise it is impossible to know which Windows behavior was proved.

The runtime-specific rules remain in [Windows compatibility](windows.md), [MSVC compatibility](msvc.md), [MinGW compatibility](mingw.md), and [Cygwin and MSYS compatibility](cygwin-msys.md). The public support summary is in [Build, Runtime, and Platform Support](../../platform-support.md).

## Support policy

The long-term Windows policy is to avoid coupling a supported target runtime to one preferred build shell. Subject to tool availability, the project should support every meaningful combination of build/test driver, build system, compiler frontend, target ABI/C runtime, and architecture. “Meaningful” excludes combinations that the toolchain itself cannot produce, but it does not exclude Cygwin-to-MSVC or MSYS-to-MSVC merely because the build environment and produced executable use different path languages.

This is a convergence policy, not a claim that the active matrix is already a complete Cartesian product. An active public CI job is the evidence for a currently exercised combination. A compatibility adapter without an active job is structural support that still needs live coverage. Grouped platform tables must not imply combinations that the workflow does not contain.

## Compatibility status matrix

The matrix uses four evidence levels. **CI-backed** means an active public job builds and runs the applicable tests. **Dormant infrastructure** means a dedicated adapter was started and remains maintained, but no active public row currently invokes it. **Structural only** means source or build-system branches anticipate the combination without an end-to-end Windows run. **Candidate** means the toolchain combination is theoretically possible, but this repository has neither an active job nor a dedicated, proven handoff. These labels describe repository evidence, not whether an external toolchain vendor permits the combination.

### Build and test driver matrix

| Build/test driver | Compiler frontend | Produced ABI and CRT | Autotools | Meson | Notes |
| --- | --- | --- | --- | --- | --- |
| Native Windows command environment | `cl` | MSVC ABI and CRT | Not maintained in this driver | **CI-backed** | x86_64 and AArch64 jobs; binding, analysis, PGO, unity, and distribution variants also use this family |
| Native Windows command environment | `clang-cl` | MSVC ABI and CRT | Not maintained in this driver | **Candidate** | Compiler mode is supported elsewhere, but no active native-command Meson row proves this pairing |
| MSYS2 `MSYS` shell | `cl` | MSVC ABI and CRT | **CI-backed** | **Dormant infrastructure** | The Meson selective-argv wrapper is present in the public action, but no active matrix row invokes it |
| MSYS2 `MSYS` shell | `clang-cl` | MSVC ABI and CRT | **CI-backed** | **Candidate** | The current dormant Meson wrapper resolves `cl.exe`; it is not evidence for a Meson `clang-cl` row |
| Cygwin shell | `cl`, `clang-cl`, or `clang --driver-mode=cl` | MSVC ABI and CRT | **CI-backed** | **Candidate** | Autotools has active build/test and coverage variants; no Meson-to-MSVC Cygwin row is active |
| MSYS2 `MINGW32`, `MINGW64`, or `UCRT64` shell | GCC | MinGW-w64 Windows ABI with the environment's MSVCRT/UCRT family | **CI-backed** | **CI-backed** | Active rows cover 32-bit, 64-bit, UCRT, coverage, unity, and feature variants, but not every variant on every runtime family |
| MSYS2 `CLANG64` shell | GNU-style `clang` | MinGW-w64 Windows ABI with UCRT | **CI-backed** | **CI-backed** | Active build/test, coverage, distribution, and sanitizer variants |
| MSYS2 `MSYS` shell | GCC | MSYS POSIX ABI and runtime | **CI-backed** | **CI-backed** | This is an MSYS executable, not a MinGW executable merely because the host is Windows |
| Cygwin shell | GCC | Cygwin POSIX ABI and runtime | **CI-backed** | **CI-backed** | Active Autotools and Meson build/test variants |
| Cygwin shell | MinGW-w64 GCC or GNU-style Clang | Native MinGW-w64 Windows ABI | **Candidate** | **Candidate** | Cross-toolchainable in principle, but no dedicated public job or maintained handoff proves it |
| Windows native, MSYS2, or Cygwin driver | Emscripten Clang | WebAssembly with Emscripten runtime | **Structural only** | **Structural only** | Emscripten is CI-backed from Linux and macOS; Windows-hosted configure, build, Node launch, and path behavior are not verified end to end |

### Produced-artifact execution matrix

| Produced artifact | Execution environment | Status | What is actually proved |
| --- | --- | --- | --- |
| MSVC-ABI executable | Native Windows, launched from the native command environment | **CI-backed** | Native Meson build and test execution |
| MSVC-ABI executable | Native Windows, launched from MSYS2 or Cygwin | **CI-backed** | Autotools configure, build, path handoff, DLL discovery, and test execution across the runtime boundary |
| MSVC-ABI executable | Wine on Linux or macOS | **Candidate** | No active public row; MSVC redistributable and Wine behavior are not part of the current support proof |
| MinGW-w64 executable | Native Windows, launched from MSYS2 | **CI-backed** | GCC and CLANG64 artifacts run on Windows with the selected MinGW/UCRT environment |
| MinGW-w64 executable | Wine on Linux | **CI-backed** | Autotools cross-builds exercise both win32-thread and posix-thread MinGW-w64 variants through `SIXEL_RUNTIME=wine` |
| Cygwin executable | Cygwin runtime on Windows | **CI-backed** | Native Cygwin GCC artifacts run under the runtime that owns `/cygdrive` mounts |
| MSYS executable | MSYS runtime on Windows | **CI-backed** | Native MSYS GCC artifacts run under the runtime that owns `/x` mounts |
| Cygwin or MSYS executable | Wine | Not maintained | No active row or compatibility promise covers these POSIX runtime DLLs under Wine |
| Emscripten WebAssembly artifact | Node.js on Linux or macOS | **CI-backed** | Autotools and Meson build and test the Emscripten target on non-Windows hosts |
| Emscripten WebAssembly artifact | Node.js on Windows | **Structural only** | The `NODERAWFS` path branch anticipates Windows drive paths, but Windows-hosted Emscripten has no active public CI row |

The dormant Meson-under-MSYS2 adapter and Windows-hosted Emscripten branch are the clearest started-but-unfinished combinations. They must not be presented as CI-backed, but deleting them as apparently unused code would also discard deliberate progress toward the all-meaningful-combinations policy.

## Keep the driver and target identities separate

Compile-time macros describe the executable being compiled, not the shell that launched the compiler. A native executable built by `cl` from Cygwin sees `_WIN32` and `_MSC_VER`; it does not become a Cygwin executable and must not select `__CYGWIN__` path or pthread behavior. Conversely, a Cygwin GCC executable selects the Cygwin libc even though both binaries run on the same Windows host.

| Identity | What it controls | Typical path contract |
| --- | --- | --- |
| Build/test driver: native command environment | How commands, quoting, environment variables, and tools are launched | Native drive and UNC paths |
| Build/test driver: Cygwin shell | Configure and TAP shell semantics; paths received from the shell | `/cygdrive/c/...` plus Cygwin mounts |
| Build/test driver: MSYS/MSYS2 shell | Configure, Meson, make, and automatic argv rewriting | `/c/...` plus MSYS mounts |
| Target: MSVC ABI/CRT | `_WIN32`, `_MSC_VER`, Win32 process APIs, native CRT filesystem calls | Native drive paths at the CRT boundary |
| Target: MinGW-w64 ABI | `_WIN32`, `__MINGW32__`, selected MSVCRT/UCRT import libraries | Native drive paths at the CRT boundary |
| Target: Cygwin or MSYS runtime | `__CYGWIN__` or `__MSYS__`, POSIX libc and process model | Runtime-owned POSIX paths at the libc boundary |

Clang must also be classified by driver mode and target runtime. `clang-cl` and `clang --driver-mode=cl` are MSVC-compatible frontends in the current CI rows and produce MSVC-ABI binaries. `CLANG64` uses the GNU-style `clang` driver with the MSYS2 mingw-w64 toolchain and UCRT. Treating every Clang row as either “MSVC” or “MinGW” based only on the compiler brand loses the ABI, linker, option syntax, and path contract that the compatibility code actually needs.

## Path representations that can meet

The compatibility layer can receive native drive paths such as `C:/work/file`, backslash drive paths, MSYS drive paths such as `/c/work/file`, Cygwin drive paths such as `/cygdrive/c/work/file`, UNC paths beginning `//`, and mixed paths such as `/c/cygdrive/c/work/file` produced after more than one environment has attempted conversion. Relative paths remain relative because translating them through a mount table can change which process owns their interpretation.

The mixed form is not theoretical noise: it is a symptom of two conversion layers both assuming they own the boundary. Keep its parser as a recovery path, but prevent new double conversion by assigning each boundary one owner.

`-` and `clipboard:` are command protocols rather than filesystem names. They must bypass filesystem conversion even when `cygwin_conv_path` is available.

## Boundary 1: build-tool arguments

The first boundary is between a POSIX-like build driver and a native compiler, linker, or librarian. MSYS2 may automatically rewrite slash-prefixed argv fields before launching a native program. That is useful for a bare source path such as `/d/a/project/file.c`, but MSVC also uses slash-prefixed options such as `/nologo`, `/Fe...`, `/Fo...`, `/I...`, and `/LIBPATH:...`. Disabling conversion globally preserves the options but leaves Meson-generated POSIX paths unusable by native tools.

The Meson/MSYS2 adapter in [`.github/actions/ci-steps/action.yml`](../../../.github/actions/ci-steps/action.yml) therefore resolves `cl.exe`, `lib.exe`, and `link.exe` from the saved Visual Studio tool path before changing `PATH`. This prevents the native linker name `link.exe` from resolving to an unrelated MSYS/GNU program. Its wrapper then classifies argv fields and converts only path-bearing forms: response files beginning `@/x/`, colon options such as `/LIBPATH:/x/`, `/Fe`/`/Fo`/`/Fd`/`/Fp`/`/Fa`, `/I`, GNU-style `-I` and `-L`, and bare `/x/...` paths. It finally exports `MSYS2_ARG_CONV_EXCL='*'` so MSYS does not convert the already classified argv a second time.

This classifier is intentionally narrow. Adding an MSVC option that contains a path requires updating the wrapper and its static check. Broadening it to every slash option would corrupt flags; disabling it for every slash would corrupt source and output paths.

Autotools MSYS2 jobs append the Visual Studio binary path in POSIX list form and pass an explicit MSVC host triple and companion tools. Cygwin-driven MSVC jobs likewise pass `CC`, `LD`, `AR`, `NM`, and `--host=...-windows-msvc` explicitly. Those settings are what make the target identity native MSVC even though configure itself is running under a POSIX shell.

## Boundary 2: test executable and DLL discovery

The test shell must locate the artifact that matches the target runtime. An Autotools build may expose a libtool shell wrapper at the top level and the real executable under the libtool object directory. [`build-aux/resolve-test-tool-paths.sh.in`](../../../build-aux/resolve-test-tool-paths.sh.in) inspects the wrapper signature and `shlibpath_overrides_runpath`, preserves `EXEEXT`, and chooses either the wrapper or the real program. Tests consume the resulting `IMG2SIXEL_PATH`, `SIXEL2PNG_PATH`, `LSQA_PATH`, and `TEST_RUNNER_PATH` rather than reconstructing Windows paths independently.

The runtime shared-library variable may itself be `PATH`, but the shell that assembles it still uses colon-separated lists. [`configure.ac`](../../../configure.ac) therefore exports `SIXEL_LIBTOOL_PATH_SEPARATOR=':'` even for this Windows-hosted case. The legacy environment variable `SIXEL_TEST_ADDITIOANL_PATH` is retained because existing jobs use that spelling; `SIXEL_TEST_ADDITIONAL_PATH` is the correctly spelled overriding alias. Both Autotools and Meson prepend the built DLL directories and optional package-manager DLL directory before launching tests.

`SIXEL_RUNTIME` is a separate execution prefix. It is empty for directly runnable Windows artifacts and set to `wine` when a cross-built MinGW executable is tested on Linux. Do not fold this process-launch decision into executable path normalization.

The CTRL_BREAK test runner has another native process boundary. [`tests/test_runner.c`](../../../tests/test_runner.c) converts `/cygdrive/c/...` and `/c/...` command tokens before calling `CreateProcessA`, then applies Windows command-line quoting rules. Replacing that helper with ordinary shell quoting would not be equivalent because `CreateProcessA` receives one native command-line string rather than a POSIX argv array.

## Boundary 3: application paths passed to libc

The library implementation in [`src/path.c`](../../../src/path.c) and the standalone converter implementation in [`converters/path.c`](../../../converters/path.c) provide the same two-phase contract:

1. `*_path_to_libc_buffer_size(path)` returns zero when the original pointer is already suitable, or the required converted buffer size otherwise.
2. The caller allocates that size and passes it to `*_path_to_libc(path, buffer, size)`.
3. The conversion function returns the original pointer when no conversion is needed, the caller-owned buffer on conversion, and `NULL` for invalid arguments, insufficient storage, or conversion failure.

The helper never hides allocation ownership. The compatibility adapters in [`src/compat_stub.c`](../../../src/compat_stub.c) and [`converters/compat.c`](../../../converters/compat.c) allocate only when the size phase requires it, pass the normalized path to `fopen`, `open`, `access`, `stat`, `unlink`, rename, and related operations, and free the buffer after the libc call. If an optional conversion cannot be completed, the adapter deliberately falls back to the original spelling; allocation failure remains an error.

Conversion direction is selected by the target runtime:

| Target runtime | Authoritative direction and ordering |
| --- | --- |
| Native MSVC or MinGW | Leave native drive paths unchanged; lexically recover known `/c/...`, `/cygdrive/c/...`, and nested forms as drive paths; use `cygpath -wa --` only for other clearly POSIX-like `/...` or `~...` inputs that depend on a live mount table |
| Cygwin or MSYS | Use `cygwin_conv_path(CCP_WIN_A_TO_POSIX, ...)` first because that runtime owns its mount table; retain explicit drive-path fallbacks producing `/cygdrive/c/...` for Cygwin and `/c/...` for MSYS |
| Wine running a native Windows artifact | Skip the external `cygpath` probe after detecting `wine_get_version` in `ntdll.dll`; a host-side Cygwin conversion can produce a path the Wine process cannot consume |
| Emscripten with `NODERAWFS` | Retain lexical Windows-drive recovery, but do not treat it as live proof of Windows-hosted Emscripten support |

The external `cygpath` command is used only when a mount table is necessary. The command builder single-quotes the input, escapes embedded single quotes, supplies `--` before the operand, and strips the helper's line ending. Known drive forms are handled without a subprocess.

The two path implementations are a deliberate build boundary rather than accidental copy-and-paste. The library owns the `sixel_` symbols. Standalone converter builds cannot rely on the library object and own `img2sixel_` symbols; amalgamated converter builds switch back to `sixel_` symbols. Until the build graph provides one component that all three modes can consume, every semantic path change must update and test both copies.

## Maintenance checklist

- Classify every Windows change by build/test driver and produced runtime before looking at `_WIN32`; the host OS alone is insufficient.
- Record Clang driver mode and target ABI. Keep `CLANG64` distinct from `clang-cl` and `clang --driver-mode=cl`.
- When adding a matrix row, state whether it is active evidence for a new combination or another variant of an existing family. Update this document when a currently explicit gap gains live coverage.
- Assign path conversion to exactly one boundary. Do not let shell argv rewriting, the test launcher, and the application libc adapter all normalize the same token speculatively.
- Extend the MSYS2/MSVC argv classifier only for an identified path-bearing option and retain the `link.exe` provenance check.
- Preserve libtool wrapper-versus-real-executable resolution, `EXEEXT`, colon-separated shell path lists, external DLL injection, and `SIXEL_RUNTIME` as separate decisions.
- Preserve pseudo-paths, relative paths, UNC detection, mixed-form recovery, Wine behavior, and buffer ownership when changing the application path layer.
- Update `src/path.c` and `converters/path.c` together and exercise split-library, standalone-converter, and amalgamated builds where the change can affect symbol ownership.
- Do not claim Windows-hosted Emscripten support until a public Windows job builds and runs the relevant test suite.

## History entry points

The path layer was established and narrowed through `e388ab2dc` (`cygwin_conv_path` and `cygpath`), `5dc0d86d9` (limit `cygpath` to POSIX-like inputs), `389e12947` (preserve stdin and clipboard protocols), `e9e6ffe38` (skip `cygpath` under Wine), and `2507a107e` (make the Wine probe compiler-safe). The native CTRL_BREAK launcher path was repaired in `3b044f034` and `5591a233c`. The MSYS2/Meson native-tool boundary is represented by the public CI action history around `8d1b0c822`, `9ef188e9e`, and `7344d811c`. Use these commits as archaeology entry points, but treat the current contracts above and the reciprocal static check as normative.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| WPATH-01 | Public CI retains representative native, MSYS2, Cygwin, MinGW, UCRT, CLANG64, MSVC-Clang, and Wine-driven combinations as complete matrix tuples while Windows-hosted Emscripten remains an explicit uncovered combination. | [tests/_static/sh/staticcheck-windows-path-compat.sh](../../../tests/_static/sh/staticcheck-windows-path-compat.sh), [tests/_static/sh/staticcheck-platform-ci-contracts.sh](../../../tests/_static/sh/staticcheck-platform-ci-contracts.sh) |
| WPATH-02 | The MSYS2-to-MSVC build boundary retains Visual Studio tool provenance, selective conversion of path-bearing argv forms, and suppression of a second automatic conversion. | [tests/_static/sh/staticcheck-windows-path-compat.sh](../../../tests/_static/sh/staticcheck-windows-path-compat.sh) |
| WPATH-03 | Autotools and Meson retain executable, libtool-wrapper, DLL-search, path-separator, executable-suffix, and runtime-prefix handoff points needed by mixed-runtime tests. | [tests/_static/sh/staticcheck-windows-path-compat.sh](../../../tests/_static/sh/staticcheck-windows-path-compat.sh) |
| WPATH-04 | Both application path implementations retain the two-phase ownership API, target-directed conversion, mixed-path recovery, pseudo-path preservation, Wine exclusion, and converter/amalgamation symbol split. | [tests/_static/sh/staticcheck-windows-path-compat.sh](../../../tests/_static/sh/staticcheck-windows-path-compat.sh), [tests/platform/path/0001_path_to_libc_runtime.t](../../../tests/platform/path/0001_path_to_libc_runtime.t) |
| WPATH-05 | The native CTRL_BREAK launcher retains POSIX-drive normalization before `CreateProcessA`. | [tests/_static/sh/staticcheck-windows-path-compat.sh](../../../tests/_static/sh/staticcheck-windows-path-compat.sh) |

### Coverage boundary

The static check protects public workflow rows and structural conversion seams on non-Windows hosts. It cannot validate a particular Cygwin/MSYS mount table, MSVC installation, Windows command-line parser, DLL loader, Wine prefix, or unexercised driver/target pairing. Active CI remains the evidence for live combinations, and the explicit gaps above must not be promoted to verified support on structural checks alone.
