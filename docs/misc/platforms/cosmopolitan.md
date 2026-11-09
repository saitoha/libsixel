# Cosmopolitan Compatibility

## Scope

This document owns `__COSMOPOLITAN__` behavior for Cosmopolitan Libc and Actually Portable Executable builds. The complete inventory is in the [platform compatibility ledger](README.md).

## Runtime dispatch contract

A Cosmopolitan binary can run on more than one operating system, so its build-time macro does not identify the runtime OS. Windows path conversion must remain guarded by `IsWindows()` in both the library and converter path adapters. Replacing this runtime test with `_WIN32`, host-triple inference, or an unconditional conversion would make the same APE mis-handle paths on Unix hosts.

On a Windows runtime the adapter may use `cygpath -wa` and then falls back to internal drive and `/cygdrive` mappings. On a non-Windows runtime it returns the original path. The `-` stdin marker, `clipboard:` pseudo-target, UNC paths, allocation ownership, and Wine-sensitive helper behavior must remain aligned with the other path implementations.

The fat-binary checks are part of release confidence because a successful build on the compiler host does not exercise runtime dispatch on each supported OS.

## Maintenance checklist

- Use Cosmopolitan runtime predicates for OS behavior; reserve `__COSMOPOLITAN__` for API availability and compilation.
- Keep library and converter path adapters in lockstep.
- Terminal API behavior also follows the runtime OS. On Darwin, Cosmopolitan `ttyname()` can return `/dev/tty`, whose device identity differs from the output PTY. The CLI transport accepts this alias only when `tcgetsid()` identifies both descriptors as the current session’s controlling terminal; a session has at most one. It rechecks that relationship before I/O, as well as output identity and foreground ownership. The PTY tests select Darwin’s kernel-maintained `PENDIN` expectation with `IsXnu()` rather than compiler-host macros.
- Worker creation requests at least 1 MiB of stack, preserving a larger configured default. Cosmopolitan's 80 KiB default cannot hold nested palette construction and conversion frames; even a small image can overflow it. This uses the shared thread adapter in both build systems. Existing successful CLI conversion tests exercise the palette worker on Cosmopolitan; they do not establish a bound on every possible call stack.
- Autotools links the individually compiled float-lookup objects directly into the library and test runner. Their per-object arithmetic flags remain isolated. A convenience archive would require Libtool to list and extract it, but `cosmoar` lists both architectures and extracts them into the same directory; the resulting duplicates and misplaced companion objects do not satisfy Libtool or `cosmocc`.
- Resolve the SDK installer before configuration with `INSTALL="$(command -v cosmoinstall)"`. A bare `INSTALL=cosmoinstall` is treated as a build-relative filename by Autoconf, producing `../cosmoinstall` in subdirectory Makefiles even when the SDK is on `PATH`. The absolute path survives both recursive installation and staged installation through `DESTDIR`.

- Lookup intrinsics require both a configured header and the active compiler target. `cosmocc` reuses one `config.h` across its CPU builds, so an x86 configure result alone cannot enable `<emmintrin.h>` in the AArch64 compilation. Keep target-qualified aliases shared by the header and SIMD branches; the [source check](../../../tests/_static/sh/staticcheck-lookup-intrinsics-target.sh) audits the SSE2 include boundary without a compiler.
- Python process creation does not supply a shell's `ENOEXEC` fallback for APE files on kernels without a registered loader. Native Python checks use the shared [test harness launch helper](../../build/test-harness.md#shared-discovery-and-c-test-dispatch). Timing probes still skip when complete-command timing is unavailable; successful APE startup alone is not timing evidence.
- Exercise the same APE on Windows and at least one Unix runtime after changing path logic.
- The [CI setup action](../../../.github/actions/ci-steps/action.yml) pins the official GitHub release of `cosmocc` and its SHA-256 digest. The archive path and cache key derive from that selection before download; both restored and downloaded bytes must match the digest. Downloads have bounded connection and transfer times, reject HTTP errors, and become cacheable only after verification. When updating the compiler, update the version and digest together using the [official release assets](https://github.com/jart/cosmopolitan/releases).
- Checksum verification uses `-c -` with both `sha256sum` and the `shasum -a 256` fallback. macOS versions of `sha256sum` can reject GNU long options and require the explicit standard-input operand in check mode. A verified cache avoids downloading again; a downloaded archive is published and unpacked only after verification succeeds. The [cache digest check](../../../tests/_static/sh/staticcheck-cosmocc-cache-digest.sh) executes the CI recipe with mocked tools to enforce these paths without network access.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| COSMO-01 | Both path adapters compile under `__COSMOPOLITAN__`, retain `IsWindows()` runtime dispatch, and agree on their target-runtime conversion result. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh), [tests/platform/path/0001_path_to_libc_runtime.t](../../../tests/platform/path/0001_path_to_libc_runtime.t) |
| COSMO-02 | Lookup SSE2 headers remain behind active compiler-target guards, directly or through guarded aliases, independently of shared configure results. | [tests/_static/sh/staticcheck-lookup-intrinsics-target.sh](../../../tests/_static/sh/staticcheck-lookup-intrinsics-target.sh) |
| COSMO-03 | A default threaded palette conversion completes with the worker stack reservation. | [tests/cli/dither-options/0004_option_matching_diffusion_sierra_default_variant_success.t](../../../tests/cli/dither-options/0004_option_matching_diffusion_sierra_default_variant_success.t) |
| COSMO-04 | The transport opens stdout’s controlling terminal through a runtime alias while rejecting an unrelated controlling terminal, replacement stdout and background ownership. The same APE must exercise these cases on each runtime kernel. | [tests/cli/terminal-color/0113_roundtrip.t](../../../tests/cli/terminal-color/0113_roundtrip.t), [tests/cli/terminal-color/0122_identity.t](../../../tests/cli/terminal-color/0122_identity.t), [tests/cli/terminal-color/0123_foreground.t](../../../tests/cli/terminal-color/0123_foreground.t), [tests/cli/terminal-color/0125_unrelated.t](../../../tests/cli/terminal-color/0125_unrelated.t) |
| COSMO-05 | Native Python probes can start APE build tools and executors through the shell fallback; unsupported child-time supervision remains an explicit capability skip. | [tests/build/install/0001_pkgconfig_headers.py](../../../tests/build/install/0001_pkgconfig_headers.py), [tests/tools/child-time/0001_marker_backpressure.py](../../../tests/tools/child-time/0001_marker_backpressure.py), [tests/tools/child-time/0002_binding_run_identity.py](../../../tests/tools/child-time/0002_binding_run_identity.py) |
| COSMO-06 | CI resolves its installer to a path before configuration, and the staged public-header install works from recursive Makefile directories. | [tests/_static/sh/staticcheck-ci-install-path.sh](../../../tests/_static/sh/staticcheck-ci-install-path.sh), [tests/build/install/0001_pkgconfig_headers.py](../../../tests/build/install/0001_pkgconfig_headers.py) |
| COSMO-07 | CI verifies cached and downloaded archives with portable checksum options, reuses a matching cache, and propagates download or verification failure before publishing an archive. | [tests/_static/sh/staticcheck-cosmocc-cache-digest.sh](../../../tests/_static/sh/staticcheck-cosmocc-cache-digest.sh) |

### Coverage boundary

The static checks cannot run one APE on multiple kernels or compile its CPU slices. The lookup audit follows literal positive target predicates and guarded aliases defined as `1`; it is not a complete C preprocessor. Both CPU compilations remain necessary, and cross-runtime execution remains the proof that build-time and runtime OS identities were not conflated.
