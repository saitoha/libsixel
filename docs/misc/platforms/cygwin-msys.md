# Cygwin and MSYS Compatibility

## Scope

Cygwin and MSYS run on Windows but expose POSIX-oriented libc, path, process, and pthread contracts. This document owns `__CYGWIN__` and `__MSYS__`; native Win32 behavior is described in [Windows compatibility](windows.md). A Cygwin or MSYS shell driving a native MSVC-ABI executable is a cross-runtime build rather than a Cygwin/MSYS target and is documented in [Windows cross-runtime and path compatibility](windows-paths.md). The complete inventory is in the [platform compatibility ledger](README.md).

## Runtime identity

Do not route Cygwin or MSYS through native Win32 synchronization merely because `_WIN32` is visible. The maintained discriminator explicitly excludes `__CYGWIN__` and `__MSYS__`, allowing the pthread and POSIX code paths to own mutex layout, one-time initialization, process behavior, and libc calls.

Cygwin, MSYS, MinGW, and UCRT configuration sites describe different ABI and path environments. A compiler invocation that combines one environment's headers with another environment's startup objects or import libraries is not a supported shortcut.

## Path contract

`cygwin_conv_path` with `CCP_WIN_A_TO_POSIX` is authoritative when the Cygwin/MSYS API is available. The internal `/c/...` and `/cygdrive/c/...` mappings remain fallbacks for configurations where the API or helper cannot be used. Preserve UNC paths and drive-relative distinctions.

The `-` stdin marker and `clipboard:` pseudo-target are protocol values, not filesystem paths, and must never be rewritten. For native Windows or Cosmopolitan-on-Windows, an external `cygpath -wa` may be used when available; under Wine it is skipped because host-side Cygwin conversion can produce paths that the Wine process cannot consume.

The library and converter copies of the path adapter intentionally follow the same ordering. Change both copies together until that duplication is replaced by a shared build-safe component. Their two-phase allocation contract, the test-launch boundary, and mixed Cygwin/MSYS-to-native builds are described in [Windows cross-runtime and path compatibility](windows-paths.md).

## Maintenance checklist

- Preserve the Cygwin/MSYS exclusions wherever native Win32 lock storage is selected.
- Prefer `cygwin_conv_path` over handwritten mappings, but keep fallbacks and pseudo-path exclusions.
- Exercise Cygwin and MSYS independently because their default mount spellings differ.
- Treat config-site files as runtime-family declarations, not generic Windows presets.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| CYG-01 | Cygwin and MSYS remain excluded from the native Win32 thread backend. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| CYG-02 | Both path adapters retain authoritative `cygwin_conv_path` conversion and preserve `clipboard:` as a pseudo-target. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh), [tests/platform/path/0001_path_to_libc_runtime.t](../../../tests/platform/path/0001_path_to_libc_runtime.t) |

### Coverage boundary

The static check verifies dispatch structure and representative calls. It cannot validate mount tables, Wine behavior, or the path returned by a particular Cygwin/MSYS runtime, so live path tests remain required.
