# MinGW Compatibility

## Scope

MinGW is a distinct platform boundary: it combines GCC or Clang language behavior with the Windows ABI and either an MSVCRT or UCRT family runtime. This document owns `__MINGW32__`, `__MINGW64__`, `__MINGW_PRINTF_FORMAT`, and `_CRTIMP`; shared Win32 rules are in [Windows compatibility](windows.md). The distinction between GNU-style Clang in MSYS2 `CLANG64` and MSVC-ABI Clang drivers is documented in [Windows cross-runtime and path compatibility](windows-paths.md). The overall inventory is in the [platform compatibility ledger](README.md).

## Runtime and declaration contract

`__MINGW32__` is defined by both 32-bit and 64-bit MinGW toolchains, so it must not be used as an architecture test. Use `__MINGW64__` only for a genuinely MinGW-w64-specific distinction and use architecture macros for generated-code selection.

Format checking must use `__MINGW_PRINTF_FORMAT`. Selecting GCC's generic `__printf__` archetype can reject valid `%zu` uses under `-Wformat=2` because the linked Windows runtime follows a different printf dialect.

POSIX feature-test macros can hide `_setmode` even though the symbol is present in the Windows CRT. The compatibility layer therefore retains the matching `_CRTIMP int __cdecl _setmode` declaration and sets binary mode before byte-oriented I/O. Do not replace this with an implicit declaration or move binary-mode responsibility to callers.

The compatibility API retains `__declspec(dllexport)` on `_WIN32` so helper symbols needed by the library and tests enter the MinGW DLL and import library. The WIC codec must link the UUID library in addition to COM and codec libraries; both Autotools and Meson probes must retain that dependency.

Keep MSVCRT and UCRT configuration sites distinct. Headers, startup objects, and import libraries from MinGW, MSYS, Cygwin, or another runtime family are not interchangeable even when the compiler accepts them.

## Maintenance checklist

- Run format warnings with the same printf archetype and runtime family used for the final link.
- Test both MinGW32-style and MinGW-w64/UCRT environments before interpreting `__MINGW32__` as bitness.
- Confirm DLL exports and import-library consumers when moving compatibility helpers.
- Keep WIC UUID linkage and binary-mode declarations synchronized across build systems.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| MW-01 | MinGW format attributes use `__MINGW_PRINTF_FORMAT`, and `_setmode` retains its `_CRTIMP` declaration. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| MW-02 | Windows compatibility helpers retain DLL export visibility and Meson retains the WIC UUID dependency. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

The static check protects declarations and link-input selection but cannot detect a mixed MSVCRT/UCRT installation or validate the produced import library. Those require MinGW builds in clean runtime-specific environments.
