# Meson Adoption and Remaining Differences

Meson adoption has two distinct dates in the libsixel family: the community fork, `libsixel/libsixel`, introduced it in June 2021; the resumed `saitoha/libsixel` line added its Meson build on September 13, 2025. The latter date describes this repository's integration, not the first Meson implementation anywhere in libsixel.

The two repositories adopted Meson for different roles. `libsixel/libsixel` sought a faster, modern replacement for its main Autotools build. `saitoha/libsixel` introduced a supplementary build system for environments where Autotools + Libtool was difficult or impractical to use, while retaining that existing build path. The resumed line's next-release goal was also to carry forward the community fork's broad externally visible feature set, including Meson support. That compatibility goal did not entail adopting the fork's replacement policy.

| Repository | Purpose of Meson adoption | Role of Autotools + Libtool |
| --- | --- | --- |
| `libsixel/libsixel` | Replace the main build with a faster, modern build system. | Main build removed during the 2021 Meson switch. |
| `saitoha/libsixel` | Add a supplementary path where Autotools + Libtool is hard to use, while carrying forward the community fork's user-visible build capability. | Retained and maintained as a complementary path. |

[Why both build systems are maintained](README.md#why-maintain-two-build-systems) explains the distribution, toolchain, and platform requirements behind this complementary design.

## Introduction and scope of the evidence

### The community fork's earlier implementation

Fredrick Brennan (`@ctrlcctrlv`) authored the initial Meson implementation in [`0b535a598`](https://github.com/libsixel/libsixel/commit/0b535a598fc93c27e87d9ec6a12e0fd51dd9068b) on June 13, 2021. The community fork integrated that work through [PR #20](https://github.com/libsixel/libsixel/pull/20), with the resulting commit [`27635ffdf`](https://github.com/libsixel/libsixel/commit/27635ffdffa2f85e4ffe079c3d5081b7fb152781) dated June 15 at 23:51:55 in the author's `-04:00` timezone. GitHub shows the merge as June 16 in UTC. That change added the Meson build and removed the main Autotools build files from the community fork.

Meson was therefore part of the community-maintained line more than four years before its introduction into the resumed original repository. Its later appearance in `saitoha/libsixel` must preserve that earlier implementation's credit and context. [Project history](../project-history.md#2021-2025-work-in-libsixellibsixel) describes the community fork's wider maintenance and release work.

The [PR #20 discussion](https://github.com/libsixel/libsixel/pull/20) presents the switch as a move to a widely used modern build system and reports faster builds as a benefit. The removal of the main Autotools files implements that replacement policy. These are the community fork's stated motivation and reported experience; they do not establish a universal speed advantage across every platform and configuration.

### Selective integration into the resumed original line

`saitoha/libsixel` continued its own development line and chose to cherry-pick the changes it needed from `libsixel/libsixel`, adapting or reimplementing work where appropriate. It did not base the resumed line on the community fork's later development history. The repositories share earlier history from before the fork; selective reuse does not make the source commit an ancestor of the receiving branch.

The Meson commits illustrate this distinction: `27635ffdf` is not an ancestor of `cf813513b`; their common ancestor is [`6a5be8b72`](https://github.com/saitoha/libsixel/commit/6a5be8b72d84037b83a5ea838e17bcf372ab1d5f), from January 2020. The first addition of `meson.build` and `meson_options.txt` along the resumed `saitoha/libsixel` line is [`cf813513b`](https://github.com/saitoha/libsixel/commit/cf813513b8ab0498f246ada93f75d2d55c018f79), authored on September 13, 2025 by Hayaki Saito. It added root, library, converter, public-header, and Python build descriptions alongside the existing Autotools files. The initial root graph did not enter a `tests` subdirectory, even though it already exposed and summarized a tests option.

The maintainer identifies two connected considerations behind this addition. The intended next release should carry forward the broad externally visible feature set of `libsixel/libsixel`, where users and packagers already had Meson support. Within `saitoha/libsixel`, that support would serve as a supplementary path for environments where Autotools + Libtool was hard to use. The retained build system still supplied required distribution and portability properties. The complementary role was part of the adoption purpose itself, rather than a later justification for an unfinished migration.

The September 2025 commit message records the addition but does not state this rationale; the explanation here records the maintainer's account. The immediate follow-up work on MinGW, MSVC, and WIC documents early Windows integration. Carrying forward the broad feature set did not mean every build option, platform behavior, or internal implementation was already identical. The [remaining differences](#remaining-differences-and-gaps) below describe specific implementation and validation boundaries.

### Other candidates considered

Around the 2025 Meson introduction, the `saitoha/libsixel` maintainer also considered adopting CMake and `build2`. Neither met libsixel's requirements, so both were passed over. This evaluation belonged to the resumed original line's search for a suitable build system, separately from the community fork's 2021 choice.

The project's requirements combine source distribution without build-generator dependencies for ordinary builders, portability, library-linking support, customization, and understandable toolchain control. [The build-system policy](README.md#why-maintain-two-build-systems) explains those requirements and the maintainer's dissatisfaction with GNU make dependence and parallel orchestration. Passing over CMake and `build2` was a project-specific assessment at the time of adoption. Meson was accepted for the supplementary role, while Autotools + Libtool continued to supply the properties required of the existing build path.

## Adoption timeline

The dates below are Git author dates. Some older commits have later committer dates, and the resumed line includes another commit with the same Meson-addition subject, [`9ceb2a024`](https://github.com/saitoha/libsixel/commit/9ceb2a024d2c1c2439625aa4dec626b33c7c43bc). Inspect the actual additions and graph rather than treating repeated subjects or committer dates as independent introductions.

| Author date | Repository | Development step | Evidence and significance |
| --- | --- | --- | --- |
| 2021-06-13 | `libsixel/libsixel` | Initial Meson implementation. | [`0b535a598`](https://github.com/libsixel/libsixel/commit/0b535a598fc93c27e87d9ec6a12e0fd51dd9068b), authored by Fredrick Brennan. |
| 2021-06-15 | `libsixel/libsixel` | Integrated the Meson switch through PR #20. | [`27635ffdf`](https://github.com/libsixel/libsixel/commit/27635ffdffa2f85e4ffe079c3d5081b7fb152781) adds Meson and removes the main Autotools build. The merge date is June 16 in UTC. |
| 2025-09-13 | `saitoha/libsixel` | Added Meson as a supplementary build path, also carrying forward a community-fork capability for the next release. | [`cf813513b`](https://github.com/saitoha/libsixel/commit/cf813513b8ab0498f246ada93f75d2d55c018f79): core library, CLI, headers, dependency detection, and Python; Autotools + Libtool retained. |
| 2025-09-13 to 2025-09-15 | `saitoha/libsixel` | Added early Windows corrections and integration. | [`bedc9d2e2`](https://github.com/saitoha/libsixel/commit/bedc9d2e2) fixes MinGW; [`97449f8f9`](https://github.com/saitoha/libsixel/commit/97449f8f9) adds the WIC build; [`5cee1a3a3`](https://github.com/saitoha/libsixel/commit/5cee1a3a3) adds minimum MSVC support. |
| 2025-12-03 to 2025-12-09 | `saitoha/libsixel` | Added the TAP harness and made Meson tests enabled by default. | [`d36892cd4`](https://github.com/saitoha/libsixel/commit/d36892cd4b365d86d65a6fd79e8f96c2f723faaf) introduces the harness and CI, already using Meson's TAP protocol; [`ce389de94`](https://github.com/saitoha/libsixel/commit/ce389de94) changes the `tests` default to true and supplies Meson root paths to tests. |
| 2026-01-18 | `saitoha/libsixel` | Consolidated C test helpers. | [`17a75cd0e`](https://github.com/saitoha/libsixel/commit/17a75cd0e3a3aac412e04f5dff6e3809eacba2d7): a unified runner became the build and dispatch container. |
| 2026-02-13 | `saitoha/libsixel` | Shared runtime test-list metadata. | [`b13d9d747`](https://github.com/saitoha/libsixel/commit/b13d9d747d799c0e9883bffa660ba61e72d29f9d): Autotools and Meson use a common inventory mechanism, subsequently moved into `build-aux`. |
| 2026-03-06 | `saitoha/libsixel` | Separated static checks from runtime tests. | [`67f27862b`](https://github.com/saitoha/libsixel/commit/67f27862b23a7ca6129ef7e3236e7ab5daca81b0): dedicated Meson staticcheck targets and alignment with the Autotools set. |
| 2026-03-16 | `saitoha/libsixel` | Moved the Meson TAP launcher into a script file. | [`3bf727177`](https://github.com/saitoha/libsixel/commit/3bf727177d394a057a1b965e454ddb3efbcf1fc3): avoids embedding the wrapper program in Windows command strings. |
| 2026-04-05 | `saitoha/libsixel` | Aligned expected-failure interpretation. | [`f78f5d200`](https://github.com/saitoha/libsixel/commit/f78f5d200a562d92d9c6a4fe597e7d077d8a8f8a): maps TAP outcomes for Meson's expected-failure handling. |
| 2026-06-11 | `saitoha/libsixel` | Added the Windows Meson installcheck CI phase. | [`46b4d72f1`](https://github.com/saitoha/libsixel/commit/46b4d72f18825bcff6c17434b19aacdbc0a92fc0): extends validation to installed-command execution on that CI path. |

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

The [support-script audit](support-scripts.md#wasm-configuration-execution-and-installation) additionally records Emscripten adapter boundaries: `exe_wrapper` is placed in the cross files' `[properties]` section instead of Meson's documented `[binaries]` section; both cross files currently have identical contents despite the `no-threads` name; and Meson 1.8.3's `.js` executable naming exposes mismatches between the split-install target, launcher, and sidecar lookup. These affect configuration and installation evidence even when the shell tests can execute build-tree programs through the compiler wrapper's extensionless Node entry points.

## Differences that are already implemented

Do not list TAP, the unified C runner, optional bindings, staticcheck, amalgamated library/tools, documentation builds, or installed-command testing as wholly absent from Meson. Their current owners are described in the [Meson build chapter](meson.md). The relevant question is which build and execution path each target actually covers.

Similarly, the custom Automake result collector is not a missing Meson feature. Meson has its own scheduler, TAP interpretation, timeout handling, and logs. The shared contract is the observation reported by each test, with explicit adapters where the harness semantics differ.

## Maintaining the comparison

When a difference is fixed, update this table from the changed implementation and the corresponding validation evidence. Verify an external static consumer for pkg-config closure, installed registration state for WIC cleanup, the actual runner translation units for amalgamation, and the executed-versus-skipped staticcheck set for probe coverage. For platform claims, use a maintained end-to-end CI path rather than one successful configure or compile command.
