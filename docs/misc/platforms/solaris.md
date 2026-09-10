# Solaris Compatibility

## Scope

This document owns compatibility with Solaris and illumos-family C runtimes and base tools. Many of these constraints are not represented by a predefined C macro: they concern `awk`, `od`, `sed`, shell, make, header visibility, and `setjmp` semantics. The complete inventory is in the [platform compatibility ledger](README.md).

## C and header contract

The libjpeg loader stores post-`setjmp` failure state in `volatile int jpeg_failed`. A non-volatile automatic modified after `setjmp` can become indeterminate after `longjmp`, and Solaris GCC exposed that bug in practice. Preserve the qualifier and audit any new state read after the libjpeg error jump.

`sys/ttycom.h` is a separate configure and Meson header probe represented by `HAVE_SYS_TTYCOM_H`. The TTY code includes it when present and enables the window-size path only when `TIOCGWINSZ` is actually defined. Header presence and ioctl availability are separate capabilities.

Secure-CRT names are not proof of the MSVC type system. In `assessment/lsqa.c`, declarations such as `errno_t` and uses of `fopen_s` remain guarded by `_MSC_VER` in addition to any function probe so a Solaris declaration or configure result does not route compilation into the Microsoft ABI branch.

## awk, shell, and build-tool contract

Solaris awk variants do not provide every extension accepted by GNU awk. Maintained scripts use POSIX operations such as `index`, `substr`, `sub`, `split`, `RSTART`, and `RLENGTH` instead of `match(string, regexp, capture_array)`. JSON metrics and quoted shell values must be parsed structurally without depending on GNU capture arrays or fragile regex escaping.

Byte checks must ignore the implementation-specific spacing produced by `od`; consume fields through shell splitting or remove `[[:space:]]` rather than comparing the raw line. TAP parsing runs with `LC_ALL=C` where character classes and diagnostics must be stable.

`build-aux/resolve-test-tool-paths.sh.in` deliberately resolves libtool state, quote removal, key trimming, and shell-safe assignment output using POSIX shell string operations. Reintroducing sed/awk into that bootstrap path recreates quoting and implementation differences before tests can start.

Coverage normalization chooses `/usr/xpg4/bin/awk`, then `nawk`, then `awk`. Solaris Autotools jobs use `gmake`, configure with `--disable-dependency-tracking`, build with `V=1`, and run checks without forcing `V=1`; the test recipe is sensitive to the extra verbosity assignment. Meson jobs install GCC, Meson, and Ninja explicitly.

## Maintenance checklist

- Mark every automatic value read after a possible `longjmp` according to the C rule, not only the first observed flag.
- Keep header, declaration, and callable-feature probes separate.
- Restrict repository awk to POSIX syntax unless a script explicitly selects and verifies a stronger implementation.
- Validate shell quoting and `od` normalization with Solaris tools before accepting GNU-only simplifications.
- Keep GNU make and dependency-tracking choices explicit in the Solaris CI row.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| SOL-01 | The libjpeg longjmp flag remains `volatile`, and TTY use remains guarded by both `HAVE_SYS_TTYCOM_H` and `TIOCGWINSZ`. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| SOL-02 | Test-tool resolution remains shell-based, coverage retains the XPG4/nawk fallback, and Solaris CI retains GNU make plus disabled dependency tracking. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

The static check rejects loss of representative portability structures but is not a complete awk or shell language parser. Solaris CI remains authoritative for the actual base-tool implementations, C optimizer, headers, and make behavior.
