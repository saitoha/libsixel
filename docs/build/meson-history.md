# Meson Adoption and Remaining Differences

## Introduction and scope of the evidence

The first addition of `meson.build` and `meson_options.txt` in the current history is [`cf813513b`](https://github.com/saitoha/libsixel/commit/cf813513b8ab0498f246ada93f75d2d55c018f79), authored on September 13, 2025 by Hayaki Saito. It added root, library, converter, public-header, and Python build descriptions alongside the existing Autotools files. The initial root graph did not enter a `tests` subdirectory, even though it already exposed and summarized a tests option.

That commit's message records the addition without a detailed design rationale. The [build guide](../../build.md#building-with-meson) describes Meson as a faster and more portable build path. The immediate follow-up work on MinGW, MSVC, and WIC is evidence that compiler and platform coverage was an early practical concern. This chronology supports that interpretation; it is not a measured claim that every Meson configuration builds faster, nor evidence of a decision to retire Autotools.

The dates below are Git author dates. Some older commits have later committer dates, and the history includes another commit with the same Meson-addition subject, [`9ceb2a024`](https://github.com/saitoha/libsixel/commit/9ceb2a024d2c1c2439625aa4dec626b33c7c43bc). Inspect the actual additions and graph rather than treating repeated subjects or committer dates as independent introductions.

## Adoption timeline

| Author date | Development step | Evidence and significance |
| --- | --- | --- |
| 2025-09-13 | Introduced the Meson build alongside Autotools. | [`cf813513b`](https://github.com/saitoha/libsixel/commit/cf813513b8ab0498f246ada93f75d2d55c018f79): core library, CLI, headers, dependency detection, and Python. |
| 2025-09-13 to 2025-09-15 | Added early Windows corrections and integration. | [`bedc9d2e2`](https://github.com/saitoha/libsixel/commit/bedc9d2e2) fixes MinGW; [`97449f8f9`](https://github.com/saitoha/libsixel/commit/97449f8f9) adds the WIC build; [`5cee1a3a3`](https://github.com/saitoha/libsixel/commit/5cee1a3a3) adds minimum MSVC support. |
| 2025-12-03 to 2025-12-09 | Added the TAP harness and made Meson tests enabled by default. | [`d36892cd4`](https://github.com/saitoha/libsixel/commit/d36892cd4b365d86d65a6fd79e8f96c2f723faaf) introduces the harness and CI, already using Meson's TAP protocol; [`ce389de94`](https://github.com/saitoha/libsixel/commit/ce389de94) changes the `tests` default to true and supplies Meson root paths to tests. |
| 2026-01-18 | Consolidated C test helpers. | [`17a75cd0e`](https://github.com/saitoha/libsixel/commit/17a75cd0e3a3aac412e04f5dff6e3809eacba2d7): a unified runner became the build and dispatch container. |
| 2026-02-13 | Shared runtime test-list metadata. | [`b13d9d747`](https://github.com/saitoha/libsixel/commit/b13d9d747d799c0e9883bffa660ba61e72d29f9d): Autotools and Meson use a common inventory mechanism, subsequently moved into `build-aux`. |
| 2026-03-06 | Separated static checks from runtime tests. | [`67f27862b`](https://github.com/saitoha/libsixel/commit/67f27862b23a7ca6129ef7e3236e7ab5daca81b0): dedicated Meson staticcheck targets and alignment with the Autotools set. |
| 2026-03-16 | Moved the Meson TAP launcher into a script file. | [`3bf727177`](https://github.com/saitoha/libsixel/commit/3bf727177d394a057a1b965e454ddb3efbcf1fc3): avoids embedding the wrapper program in Windows command strings. |
| 2026-04-05 | Aligned expected-failure interpretation. | [`f78f5d200`](https://github.com/saitoha/libsixel/commit/f78f5d200a562d92d9c6a4fe597e7d077d8a8f8a): maps TAP outcomes for Meson's expected-failure handling. |
| 2026-06-11 | Added the Windows Meson installcheck CI phase. | [`46b4d72f1`](https://github.com/saitoha/libsixel/commit/46b4d72f18825bcff6c17434b19aacdbc0a92fc0): extends validation to installed-command execution on that CI path. |

These are integration milestones, not a complete commit inventory. Later fixes continue to synchronize feature probes, source lists, runtime DLL lookup, fixture preparation, and platform-specific execution. Meson evolved beyond a minimal library build, while both build systems remain maintained.

## Remaining differences and gaps

The following observations describe the source audited at `66100975a`. The implementation links identify where to reassess them; this table does not claim that an untested platform fails or that every enabled feature has equal CI coverage.

| Area | Classification | Current evidence and practical consequence |
| --- | --- | --- |
| Static consumer pkg-config metadata | Incomplete implementation | Root [meson.build](../../meson.build) sets `LIBS_PRIVATE` to an empty string with an explicit TODO. `REQUIRES_PRIVATE` is populated, but [libsixel.pc.in](../../libsixel.pc.in) can therefore omit non-pkg-config private linker flags. A successful in-tree static build is not proof that an external consumer gets every dependency through `pkg-config --static --libs libsixel`; completeness depends on its selected dependencies. |
| WIC deregistration during uninstall | Missing automatic integration | [wic/meson.build](../../wic/meson.build) registers the DLL through an install script when requested and outside `DESTDIR`, but does not attach automatic deregistration to ordinary uninstall. Unregister the installed DLL while it still exists, using the matching Windows tool, before removing its files. The build guide also records this limitation. |
| C runner in amalgamated-tools mode | Different coverage path | [tests/Makefile.am](../../tests/Makefile.am) replaces the runner sources with generated `sixel.c` and test defines for this mode. [tests/meson.build](../../tests/meson.build) always builds the scanned ordinary test sources and links `libsixel_dep`. Meson can exercise an amalgamated library or converter, but does not prove the Autotools runner's amalgamated compilation and symbol-collision contract. |
| Compiler probes inside staticcheck | Build-system-specific coverage gap | [staticcheck-suite.sh](../../tests/_static/sh/staticcheck-suite.sh) gates a group of compile probes on the existence of the build root's `src/Makefile`. An ordinary Meson build skips that group. The shared umbrella target is not a guarantee of identical executed checks. |
| OpenVMS/GNV | Not continuously verified with Meson | [OpenVMS compatibility](../misc/platforms/openvms.md) defines the maintained Autotools path and explicitly excludes Meson from continuous verification. There is no basis here for claiming equivalent native linking, RMS inventory handling, or end-to-end tests under Meson. |
| MSVC PGO for WIC | Deliberate instrumentation exclusion | Root [meson.build](../../meson.build) declares WIC before attaching MSVC PGO dependencies to later targets, intentionally leaving the WIC module uninstrumented. A Meson PGO build does not mean every output participates in PGO. |
| Platform/compiler matrix | Evidence boundary | [Platform support](../platform-support.md) records both build systems for many environments but Autotools alone for others, including the documented OpenVMS and OmniOS entries. CI representation is narrower than hypothetical compiler compatibility; consult current workflow definitions before expanding support claims. |

The metadata and deregistration rows are concrete incomplete work. The runner, staticcheck, and PGO rows describe narrower validation or instrumentation paths. The platform rows describe missing continuous evidence. These categories call for different follow-up work and should not be collapsed into a single unsupported-feature list.

## Differences that are already implemented

Do not list TAP, the unified C runner, optional bindings, staticcheck, amalgamated library/tools, documentation builds, or installed-command testing as wholly absent from Meson. Their current owners are described in the [Meson build chapter](meson.md). The relevant question is which build and execution path each target actually covers.

Similarly, the custom Automake result collector is not a missing Meson feature. Meson has its own scheduler, TAP interpretation, timeout handling, and logs. The shared contract is the observation reported by each test, with explicit adapters where the harness semantics differ.

## Maintaining the comparison

When a difference is fixed, update this table from the changed implementation and the corresponding validation evidence. Verify an external static consumer for pkg-config closure, installed registration state for WIC cleanup, the actual runner translation units for amalgamation, and the executed-versus-skipped staticcheck set for probe coverage. For platform claims, use a maintained end-to-end CI path rather than one successful configure or compile command.
