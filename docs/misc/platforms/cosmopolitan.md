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
- Exercise the same APE on Windows and at least one Unix runtime after changing path logic.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| COSMO-01 | Both path adapters compile under `__COSMOPOLITAN__` and retain `IsWindows()` runtime dispatch. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

The static check cannot run one APE on multiple kernels. Cross-runtime execution remains the only proof that build-time and runtime OS identities were not conflated.
