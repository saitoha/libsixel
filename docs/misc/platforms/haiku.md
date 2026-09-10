# Haiku Compatibility

## Scope

Haiku compatibility is expressed through capability probes and test-harness behavior rather than a dedicated C compiler macro. This document owns the Haiku-specific backtrace declaration, locale, signal-test, PSD-test, Meson, package-manager, and artifact-path contracts. The complete inventory is in the [platform compatibility ledger](README.md).

## Runtime and API contract

Haiku can provide the `backtrace` and `backtrace_symbols_fd` symbols while `execinfo.h` is available only through an optional development package. `converters/aborttrace.c` therefore includes the header when `HAVE_EXECINFO_H` is true, but otherwise declares each function independently when `HAVE_BACKTRACE` or `HAVE_BACKTRACE_SYMBOLS_FD` proves that symbol. Do not collapse header and function probes into one condition.

The Python binding cannot assume that locale discovery returns an encoding on Haiku. `_resolve_locale_encoding` preserves an explicit default when the platform result is empty, and filename encoding uses `default="ascii"` for the compatibility path.

## Test and CI contract

`RUNTIME_ENV_BUILD_OS` is exported to tests so skips can identify the build runtime without guessing from the host shell. SIGINT pipeline-trace tests skip Haiku because the signal-timing observation is unstable there. The five PSD TySh trace cases use the narrower `SIXEL_TEST_SKIP_HAIKU_PSD_TYSH_TRACE` control because their VM timeout is not a reason to skip unrelated PSD coverage.

Haiku's Python asyncio subprocess creation can intermittently return `EINVAL` during a long Meson test process. CI retains complete coverage by running `meson test --num-processes 1` in sixteen `--slice` invocations with `--no-rebuild --print-errorlogs`. Replacing the sequence with one parallel invocation reintroduces the failure mode; reducing the slice set would silently reduce coverage.

`pkgman refresh` and `pkgman install` share one three-attempt retry boundary because either half can fail while a mirror is unavailable. The VM workspace is rooted under `/boot/home`; artifact collection must preserve that prefix instead of assuming the guest path matches other BSD-family VMs.

Historical Python TAP initialization also exposed shell differences around null commands, redirection, and `printf`. New shell tests should follow the repository's current portable TAP conventions instead of assuming a GNU or BSD shell behavior observed on another target.

## Maintenance checklist

- Keep header and symbol capability probes independent for optional Haiku development packages.
- Use explicit build-runtime exports for skips and keep every skip narrower than the unavailable observation.
- Preserve all Meson slices, serial execution, and no-rebuild behavior when changing the Haiku CI driver.
- Test guest workspace mapping and package retries on the current Haiku image before simplifying them.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| HAIKU-01 | Abort tracing retains symbol-specific prototypes when `execinfo.h` is absent, and Python filename encoding retains its ASCII fallback. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| HAIKU-02 | Haiku CI retains exact Autotools and Meson matrix tuples, serial complete sixteen-part Meson slicing, the three-attempt package refresh/install retry boundary, both explicit build-OS signal skips, and all five narrow PSD skip controls. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh), [tests/_static/sh/staticcheck-platform-ci-contracts.sh](../../../tests/_static/sh/staticcheck-platform-ci-contracts.sh) |

### Coverage boundary

The static check verifies branch and workflow structure. It cannot reproduce Haiku's subprocess `EINVAL`, signal timing, package-mirror availability, locale state, or optional package layout; the Haiku CI jobs remain required.
