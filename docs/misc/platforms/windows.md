# Windows Compatibility

## Scope

This document owns compatibility rules shared by native Windows builds. It covers the `_WIN32` and `_WIN32_WINNT` boundary and common Win32/CRT behavior; compiler-runtime differences belong to [MSVC](msvc.md) and [MinGW](mingw.md), while the POSIX runtimes hosted on Windows belong to [Cygwin and MSYS](cygwin-msys.md). The complete inventory is the [platform compatibility ledger](README.md).

## Native Windows discriminator

Do not treat `_WIN32` alone as proof that POSIX facilities are unavailable. Native Windows branches use `_WIN32` while excluding `__CYGWIN__` and `__MSYS__`; the threading layer also lets `WITH_WINPTHREAD` force the pthread backend. This discriminator is repeated where one-time initialization and mutex storage must match the selected ABI, including the encoder, parallel decoder, palette implementations, color management, loaders, atomics, and timeline writer. Changing only one copy can create layout or lock mismatches.

`_WIN32_WINNT` controls API declaration visibility and must remain consistent with the minimum supported Windows API level. `_UNICODE` is defined locally where a Windows callback or API type requires it; it is not permission to change the public narrow-character API. `_WINSOCKAPI_` and `_WINSOCK2API_` are SDK include guards used to avoid incompatible duplicate socket declarations and must not be treated as feature probes.

## CRT and Win32 boundaries

Windows descriptors require `_O_BINARY` for image and SIXEL streams. The compatibility layer maps the `_O_CREAT`, `_O_EXCL`, `_O_RDONLY`, `_O_RDWR`, `_O_TRUNC`, `_O_WRONLY`, `_S_IREAD`, and `_S_IWRITE` families where the source uses POSIX-style open modes. Guard macros `_MODE_T_DEFINED`, `_SSIZE_T_DEFINED`, and `_TIMEVAL_DEFINED` prevent fallback typedefs from colliding with CRT definitions, while `_CRT_DECLARE_NONSTDC_NAMES` and `_USE_MATH_DEFINES` request declarations that some Windows SDK configurations hide.

Environment access remains in the compatibility layer because `GetEnvironmentVariableA` must distinguish an absent value from an empty value and because returned storage follows the project's cache lifetime. Mutation uses `SetEnvironmentVariableA`. Console detection must use `GetConsoleMode`; the CRT can report the `NUL` device as a TTY even though it is not interactive. Wall-clock conversion starts from `FILETIME` and subtracts the Windows-to-Unix epoch offset.

Path conversion must happen before passing paths to the native CRT. Preserve drive-letter, UNC, `/c/...`, and `/cygdrive/c/...` handling as well as the `-` stdin marker and `clipboard:` pseudo-target exceptions. A successful optional `cygpath` conversion is an optimization for interoperability, not the only fallback.

WIC image loading and WinHTTP fetching are Windows facilities selected only when their headers and libraries are available. Their build-system gates must stay synchronized across Autotools and Meson.

## Maintenance checklist

- Verify the native-Windows discriminator in every shared synchronization implementation when changing thread backends.
- Verify binary mode on every newly introduced descriptor carrying image or SIXEL bytes.
- Route new environment, time, console, and filesystem compatibility through the shared adapter rather than scattering CRT assumptions.
- Test both a native Windows runtime and at least one Windows-hosted POSIX runtime before simplifying `_WIN32` conditions.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| WIN-01 | Native Windows selects Win32 threads only after excluding Cygwin and MSYS, while `WITH_WINPTHREAD` retains precedence. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| WIN-02 | Binary descriptor, environment, console, clock, path, and fallback-type boundaries retain their Windows-specific adapters and macro classification. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

The static check protects source selection and representative adapter calls. It cannot validate Windows handle behavior, filesystem spelling, SDK availability, or DLL loading and must be complemented by a native Windows CI build and tests.
