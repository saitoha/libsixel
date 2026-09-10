# MSVC Compatibility

## Scope

This document owns `_MSC_VER`, `_MT`, and `_USE_32BIT_TIME_T` behavior for MSVC and clang-cl builds. Shared Win32 behavior belongs to [Windows compatibility](windows.md), and the overall mapping is in the [platform compatibility ledger](README.md).

## Toolchain and CRT contract

Autotools detects an MSVC-style compiler before generic compiler discovery, infers the Windows-MSVC host when the user did not supply one, and chooses the matching `link`, `lib` through `ar-lib`, `dumpbin`, strip, and ranlib behavior. Reordering that inference behind `AC_PROG_CC` can make configure select Unix companion tools or misclassify compiler diagnostics.

The secure CRT formatting adapter is a deliberate two-pass implementation: `_vscprintf` determines the required size and `_vsnprintf_s` performs the bounded write. File, temporary-name, local-time, and open wrappers use the secure CRT signatures instead of relying on POSIX prototypes that MSVC does not expose.

MSVC maps `struct stat` to different concrete layouts depending on `_USE_32BIT_TIME_T`. The compatibility call must use `_stat32` for the 32-bit-time mapping and `_stat64i32` otherwise so the callee writes the same layout seen by the caller. `_MT` identifies the multithreaded CRT assumptions used by affected declarations.

Native thread creation uses `_beginthreadex`, not raw `CreateThread`, so CRT thread-local state is initialized. CPU feature detection may use MSVC intrinsics selected by `_M_AMD64` and `_M_X64`; these architecture macros are inventoried separately from platform selectors because they describe generated code, not a C runtime.

Analyzer-driven initialization and casts should be retained when they make ownership or narrowing explicit. Do not turn an `/analyze` suppression into a semantic branch merely to silence a warning.

## Maintenance checklist

- Exercise both MSVC and clang-cl discovery when changing configure ordering or companion-tool selection.
- Keep secure CRT adapters centralized and preserve their exact destination-size and error contracts.
- Check both `_USE_32BIT_TIME_T` layouts when touching `stat` wrappers.
- Use CRT-aware thread startup for code that may call libc or use thread-local state.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| MSVC-01 | MSVC intent is inferred before generic compiler discovery and retains its companion-tool defaults. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| MSVC-02 | Secure formatting, `stat` layout selection, and CRT-aware thread startup retain their MSVC-specific calls. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

The static check confirms source and configure structure but does not compile against a particular Universal CRT or inspect `/analyze` output. Windows CI remains authoritative for header, ABI, and linker behavior.
