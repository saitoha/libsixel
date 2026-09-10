# Build, Runtime, and Platform Support

## Scope

This document defines the externally visible build and runtime requirements
for libsixel and summarizes the environments continuously represented in CI.
It describes support for the source tree, library, and command-line tools; it
does not promise that the project publishes a prebuilt package for every row.

The tables below are deliberately coarse summaries of the configured CI systems. The workflows under `.github/workflows/` are the authoritative sources for individual public jobs. The Tier 2 table is the repository-visible inventory of private local-CI coverage; this repository does not duplicate either system in a combined job-by-job correspondence table.

## Support tiers

Support is evidence-based:

- **Tier 1** environments have an active build or test configuration in
  GitHub Actions. Failures are visible to all contributors and are expected to
  block or qualify changes according to the owning workflow.
- **Tier 2** environments have a build or test configuration in @saitoha's personal local CI because their platform/compiler pair is not represented in GitHub Actions. Results depend on privately operated runners, and this document records only the externally relevant coverage rather than inaccessible implementation details.

The tiers identify the source of CI evidence and are mutually exclusive at the platform/compiler-pair level. A configured job is evidence of intended support; the result of its latest run determines whether that support is currently healthy.

Only the OS releases, target architectures, compiler or ABI families, and
build systems named by a configured job are covered. Similar releases and
unlisted cross-compilation targets may work, but are not assigned a tier until
CI represents them.

## Build requirements

### Core library and tools

Every supported build needs:

- an ISO C99 compiler and linker for the target environment;
- the target platform's C runtime and standard headers;
- either an Autotools build environment or Meson 0.59.0 or later;
- a build executor: `make` for Autotools, or Ninja or another Meson backend;
- a shell environment appropriate to the selected build path.

The Git checkout contains generated Autotools files. A normal Autotools build
therefore uses `./configure` and `make`. Regenerating those files requires
Autoconf 2.60 or later, Automake, Libtool, GNU M4, and the other programs used
by `autoreconf`. Meson builds require Python as part of the Meson runtime.

Native Visual Studio builds use the MSVC command environment. Autotools on
Windows runs through MSYS2 or Cygwin, including configurations that invoke an
MSVC-ABI compiler from that shell. Cross builds must also provide the target
compiler, sysroot or SDK, and an execution mechanism when the tests run target
binaries.

See [`build.md`](../build.md) for build commands and the complete option list.

### Optional dependencies

The core library does not require every optional image or network library.
Feature detection can add support for:

- libpng, libjpeg, libtiff, and libwebp;
- lcms2 for ICC color management;
- librsvg and Cairo for SVG rendering;
- libcurl or libfetch for network input;
- GD and gdk-pixbuf integration;
- platform frameworks such as CoreGraphics, AppKit, Metal, Quick Look,
  Windows Imaging Component, and WinHTTP;
- POSIX threads or `libwinpthread` for threaded configurations;
- language runtimes when building an optional binding.

Requesting an optional feature explicitly makes its dependency mandatory.
Leaving it on `auto` permits the build to omit the feature when the dependency
or platform API is unavailable. Use the generated configure summary or Meson
configuration summary to determine the actual feature set of a build.

## Runtime requirements

`libsixel` encodes pixels into SIXEL and decodes SIXEL into pixel data. The
library itself does not require an interactive terminal. Applications provide
input and consume the encoded or decoded data through the public API.

The command-line runtime depends on the operation:

- `img2sixel` can write a SIXEL stream to a file or pipe. Displaying that
  stream requires a terminal, terminal emulator, or other consumer that
  implements SIXEL DCS sequences. Every transport between the tool and the
  display must preserve the control sequence unchanged.
- `sixel2png` decodes SIXEL without needing a SIXEL-capable terminal.
- Image formats, color management, network URLs, clipboard access, and desktop
  integration are available only when the corresponding built-in backend,
  optional library, or platform service was enabled at build time.
- Dynamically linked builds require the selected C runtime and optional shared
  libraries to remain available at runtime. Static builds carry a different
  deployment contract determined by their toolchain and linked dependencies.

The project does not specify one fixed memory or storage minimum. Resource use
depends on image dimensions, palette size, frame count, input format, and
enabled safety limits. Callers processing untrusted data should retain the
decoder limits and error handling described by the public API.

## Tier 1: GitHub Actions

The following summary includes active GitHub Actions build configurations. "System C compiler" means the default compiler selected by that CI image or guest; the exact package version is intentionally owned by the workflow or VM image rather than repeated here. Grouped compiler, architecture, and build-system cells are not Cartesian products; consult the owning workflows for the exact combinations.

| OS or target | CI release/profile | Architecture | Compiler or ABI | Build systems |
| --- | --- | --- | --- | --- |
| Ubuntu Linux | 24.04 and `ubuntu-latest` | x86_64, i686, AArch64 | GCC, Clang, TCC, PCC | Autotools, Meson |
| Alpine Linux | 3.20 | x86_64 | GCC with musl | Autotools, Meson |
| macOS | `macos-15`, `macos-latest` | x86_64, AArch64 | Apple Clang, GNU GCC | Autotools, Meson |
| Windows and MSYS2 | `windows-latest` | i686, x86_64, AArch64 | MSVC, Clang, clang-cl, MinGW GCC | Autotools, Meson |
| Cygwin on Windows | current CI image | x86_64 | Cygwin GCC; MSVC and Clang MSVC ABI | Autotools, Meson |
| FreeBSD | 14.3 | x86_64 | system C compiler | Autotools, Meson |
| OpenBSD | 7.8 | x86_64 | system C compiler | Autotools, Meson |
| NetBSD | 10.1 | x86_64 | system C compiler | Autotools, Meson |
| DragonFly BSD | 6.4.2 | x86_64 | system C compiler | Autotools, Meson |
| Haiku | R1/beta5 | x86_64 | system C compiler | Autotools, Meson |
| Solaris | 11.4 | x86_64 | GCC | Autotools, Meson |
| OmniOS | r151056 | x86_64 | GCC 13 | Autotools |
| WebAssembly | Emscripten runtime profiles | wasm32 | Emscripten/Clang | Autotools, Meson |
| Cosmopolitan APE | current toolchain profile | portable APE; x86_64 and AArch64 hosts | `cosmocc` | Autotools, Meson |

The Windows rows include native MSVC-ABI builds, MinGW/UCRT variants, and cross-built Win64 binaries exercised under Wine. Their grouped cells are unpacked in [Windows Cross-Runtime and Path Compatibility](misc/platforms/windows-paths.md), including the distinct MSVC-ABI and MinGW-w64/UCRT Clang modes and combinations that do not yet have an active public job. The WebAssembly and Cosmopolitan rows describe target formats; their host OS is recorded by each individual workflow job. Emscripten is currently exercised from Linux and macOS, not from a Windows host.

## Tier 2: @saitoha local CI

The local desktop CI catalog contains only the following platform/compiler groups. Grouped cells are summaries rather than claims that every listed dimension is combined with every other dimension.

| OS or environment | CI release/profile | Architecture | Compiler or ABI | Build systems |
| --- | --- | --- | --- | --- |
| Debian GNU/Linux | Bookworm | x86_64 | GCC | Autotools, Meson |
| OpenIndiana | 2025.10 image | x86_64 | GCC 13 | Autotools, Meson |
| OpenVMS with GNV | 9.2-3 | x86_64 | GNV `cc` environment | Autotools |

The local jobs run on @saitoha-operated desktop infrastructure using containers and virtual machines. Source, runtime, and tool exceptions are indexed in the [Platform Compatibility Ledger](misc/platforms/README.md), including the detailed [OpenVMS Compatibility](misc/platforms/openvms.md) contract.

## Matrix maintenance

The summary tables deliberately avoid enumerating sanitizer, linkage, dependency, binding, packaging, and other job-level permutations. Those exact configurations belong in the owning GitHub Actions workflows or the private local-CI configuration.

When a CI change adds or removes an OS, architecture, compiler family, ABI, or
build system:

1. inspect the current GitHub Actions workflows and assign the platform/compiler pair to exactly one CI system;
2. update the owning GitHub workflow or use the local CI's private maintenance procedure;
3. update the summary table in this document;
4. run `make staticcheck` and validate the changed job in its owning CI system.

Do not add a platform to these tables from an ad hoc successful build. Add a
maintained CI job first, then assign the tier from the system that owns it.
