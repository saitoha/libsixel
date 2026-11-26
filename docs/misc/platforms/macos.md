# macOS Compatibility

## Scope

This document owns the `__APPLE__` and `_DARWIN_C_SOURCE` compatibility boundary. Generic POSIX and BSD libc variants are described in [POSIX runtime compatibility](posix-runtimes.md), and the complete inventory is in the [platform compatibility ledger](README.md).

## SDK and runtime contract

Autotools and Meson define `_DARWIN_C_SOURCE` before platform headers so Darwin SDK declarations and BSD-flavored types remain visible when `_POSIX_C_SOURCE` is also active. `src/threading.c` repeats a guarded definition before including `sys/sysctl.h`; moving it below a system header is ineffective.

Apple Clang can expose `vsnprintf` through a fortified macro. The compatibility implementation undefines that macro before defining its wrapper to avoid macro expansion changing the declaration or function body.

Hardware-concurrency detection uses Darwin `sysctl` with `HW_AVAILCPU` and `HW_NCPU` fallbacks. pthread naming follows Apple's one-argument `pthread_setname_np` signature rather than the two-argument variants found elsewhere.

CoreGraphics color management, Metal output, Quick Look integration, and clipboard support are selected through SDK/framework probes. Host-name checks alone are insufficient: keep Autotools and Meson dependency gates synchronized and retain fallback behavior when a framework or deployment target does not expose the required API.

Abort-trace signal context requires both `__APPLE__` and the `__MACH__` environment. Executable-path discovery uses dyld APIs; neither rule should be generalized to all Unix-like targets.

Metal validates the buffer arguments of a compute kernel even when a dispatch takes a branch that skips the corresponding loads or stores. The SIXEL body span count pass binds a placeholder for each write-pass output until the counts determine its required size. Grouped span ordering requires the compiled pipeline to fit the device’s threadgroup memory and thread-count limits. Shader validation can increase the memory requirement. The serial Metal ordering kernel handles all bands when grouped ordering is unavailable.

The GPU body equivalence test keeps Metal API Validation enabled on macOS and exercises shader validation on ARM64. Its Intel macOS runs disable shader validation because pipeline creation can hang in the virtual GPU's Metal compiler. Both configurations compare the CPU and GPU output and record Metal pipeline creation, command submission, and completion in the test log. If its native `test_runner` process is still running after 60 seconds, the test uses `sample` to capture all thread stacks in `test_runner.sample.txt` under the test's artifact directory. The single-slot playback, Eytzinger wire, and GPU decoder palette map tests enable Metal API Validation for their dispatches and check the resulting bytes or pixels. On failed macOS CI jobs, the artifacts include available Darwin crash reports for `test_runner` under `test-runner-crashes/`.

## Maintenance checklist

- Define Darwin feature-test macros before any affected system header.
- Test the deployment target as well as the current SDK when changing framework gates.
- Preserve Apple-specific pthread and dyld signatures instead of papering over them with casts.
- Keep native framework paths optional unless the public build configuration explicitly requires them.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| MAC-01 | Both build systems and the threading translation unit retain `_DARWIN_C_SOURCE` before Darwin headers. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| MAC-02 | Apple formatting and hardware-concurrency adapters retain their SDK-specific handling. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

The static check does not link against multiple SDK or deployment-target versions and cannot exercise framework availability. macOS CI remains required for those checks.
