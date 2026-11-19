# MINIX Compatibility

## Scope

This page records the compiler and runtime capabilities relevant to MINIX 3.3.0 on i386. The [platform support matrix](../../platform-support.md) records the configurations exercised by maintained CI jobs.

## Runtime and API contract

The MINIX 3.3.0 pkgsrc Clang 3.4 toolchain accepts TLS declarations and links an executable that faults on TLS access. Configuration uses a program that reads and writes an initialized TLS variable to select the existing storage fallbacks. A MINIX cross build without target execution also selects the fallbacks. The native TLS check and the pthread link check measure separate capabilities.

The default threadpool setting uses a serial backend when the pthread probe cannot build and link an application. The serial backend uses internal stub types, so compiling it requires neither the pthread header nor a third-party pthread implementation. An explicit request for threading requires the native library and declarations.

The optional x86 XSAVE probe needs a linkable `_xgetbv` intrinsic. Clang 3.4 can compile an implicit call and then fail to link it. The i386 SSE2 check compiles the header with the SSE2 flag in place. Clang 3.4 rejects the header without that flag and defaults to x87 extended evaluation in that case. AVX2 implementations also require a compile check for per-function targeting and the intrinsic types, even if the compiler ships `immintrin.h`. MINIX's libfetch provides the `fetchIO` API. The build selects the appropriate stream operations from the installed header.

The R=256 FHEDT distance grid needs 128 MiB of temporary double storage. MINIX can provide the working set in smaller heap allocations. Both build systems probe the single allocation and select two slabs when it fails. The distance representation, allocator hooks and result are preserved.

## Build and test requirements

The source and build directories must reside on a filesystem that preserves long filenames, such as ext2. The default MINIX filesystem (MFS) truncates filenames at 60 bytes. The test inventory includes source files and image fixtures with longer names.

The Autotools build uses GNU make and a C compiler such as pkgsrc Clang. Its test runner can forward an object response file directly to the linker. That preserves the list when the compiler driver would expand it past the system's argument limit.

The Meson build requires Python 3 and a compatible Ninja implementation. The MINIX 3.3.0 i386 package archive contains Python 2.7.6 and Ninja 1.5.8.1, so these tools require separate provisioning. Native Meson validation uses locally ported CPython 3.6.15, Meson 0.61.5 and samurai 1.2. These tools need working subprocess pipes and regular GNU archives on this system. Both builds use Bash to run the native TAP tests. The system shell can retain an unset command prefix as an empty argument. It can also stall the test inventory pipeline when Meson captures both streams.

The POSIX test runner uses `getpgid` when it can link the function. On MINIX, it retains the existing direct-child signal fallback when it cannot query the child's process group.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| MINIX-01 | Both build systems check executable TLS access and the native pthread declarations and library. The serial threading header can omit pthread. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| MINIX-02 | Both build systems check per-function AVX2 targeting before compiling the spatial refresh implementation. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| MINIX-03 | Both builds select split FHEDT distance storage when the native heap cannot reserve the full grid. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

A local CI smoke job exercises a PNG → SIXEL → PNG conversion on MINIX 3.3.0. Native validation with pkgsrc Clang 3.4 exercises the conversion, `make check`, and `meson test` under the default feature selections. Optional dependencies and unavailable hardware retain their usual capability-based skips.
