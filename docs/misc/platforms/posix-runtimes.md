# POSIX Runtime Compatibility

## Scope

This document records libc and kernel-interface exceptions within otherwise POSIX-oriented builds. It owns `__linux__`, `__ANDROID__`, `__GLIBC__`, `__OpenBSD__`, `__NetBSD__`, `__FreeBSD__`, `__DragonFly__`, `_BSD_SOURCE`, `_DEFAULT_SOURCE`, `_DRAGONFLY_SOURCE`, `_GNU_SOURCE`, `_NETBSD_SOURCE`, `_POSIX_C_SOURCE`, `_POSIX_VERSION`, `_SC_NPROCESSORS_ONLN`, and `_XOPEN_SOURCE`. Darwin-specific SDK rules are in [macOS compatibility](macos.md), and the complete inventory is in the [platform compatibility ledger](README.md).

## Feature-test macro contract

Feature-test macros must be set before the first affected system header. Generic build configuration uses `_POSIX_C_SOURCE`, `_XOPEN_SOURCE`, `_DEFAULT_SOURCE`, or `_GNU_SOURCE` according to the APIs being probed. `src/threading.c` locally enables `_BSD_SOURCE`, `_NETBSD_SOURCE`, or `_DRAGONFLY_SOURCE` so `sys/sysctl.h` exposes the legacy kernel types hidden by strict POSIX namespaces.

These macros are declaration controls, not capability proof. Continue to gate calls through configure or Meson results, because a visible prototype can still require a library or differ across libc families.

## Runtime-specific contracts

NetBSD libfetch exposes `fetchIO *` with `fetchIO_read` and `fetchIO_close`; it must not be treated as a `FILE *` consumed by `fread` and `fclose`. FreeBSD and DragonFly retain their `gettimeofday` declaration accommodation and avoid the `posix_spawnp` thumbnailer path because the shared-library link cannot rely on an exported `environ` under the project's no-undefined policy.

OpenBSD needs the BSD namespace for sysctl types and avoids assumptions around generic prototypes and x86 intrinsic headers. DragonFly x86 AVX selection retains the operating-system-state guard before using AVX state. Linux uses `_GNU_SOURCE` where GNU pthread affinity or the GNU `strerror_r` result contract is selected. Android participates in the signal-context branch used by abort tracing but must not be assumed to provide every glibc extension.

`_SC_NPROCESSORS_ONLN` and `_POSIX_VERSION` are runtime-capability markers used for fallback selection. Preserve fallback ordering instead of turning one libc's preferred query into a universal requirement.

## Maintenance checklist

- Place feature-test definitions before system headers and retain separate capability probes.
- Check API types and ownership, not only symbol names, when sharing code across libc implementations.
- Keep skips and fallbacks scoped to the unavailable observation or ABI mismatch.
- Run at least the affected BSD or Android target before removing a branch that appears redundant on Linux.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| POSIX-01 | BSD feature namespaces remain defined before sysctl headers, and platform macros stay classified. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| POSIX-02 | NetBSD libfetch retains its `fetchIO` API, while FreeBSD and DragonFly remain excluded from the affected `posix_spawnp` path. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

The static check confirms type names and branch guards in source. It cannot prove header visibility, symbol export, kernel CPU-state behavior, or signal frame layout for a particular libc release; those require target builds and runtime tests.
