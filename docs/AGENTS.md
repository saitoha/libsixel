# AGENTS.md

## Scope

These instructions apply to documentation under `docs/`. The repository-level `AGENTS.md` continues to apply. User instructions take precedence.

## Purpose of this directory

`docs/` contains durable, project-wide references that users and contributors should be able to discover from the repository. Keep the top level limited to the documentation index, documentation governance, and similarly important entry points. Put detailed material in a subject directory so that each area can grow into multiple documents without crowding the top level.

Start from the [documentation index](README.md). Read the relevant document before changing its subject area:

- [Project history and lineage](project-history.md): origin, development milestones, contribution history, and attribution sources.
- [SIXEL format](sixel-format.md): wire syntax, control functions, rendering semantics, interoperability, and implementation references.
- [Build, runtime, and platform support](platform-support.md): build and runtime requirements, CI-backed support tiers, and platform matrix.
- [Color spaces and loader color management](concepts/colorspace.md): color interpretation, transfer functions, encoder color-space boundaries, and loader ICC/CMS behavior.
- [Pixel-format precision](concepts/pixelformat-precision.md): byte and float32 storage, implicit promotion boundaries, resize precision, and focused quality measurements.
- [Pixel formats and alpha representation](concepts/pixelformat.md): input memory layouts, frame storage, transparency metadata, and the SIXEL wire boundary.
- [Functionality](functionality/overview.md): capabilities, components, data flow, and architectural boundaries.
- [Palette clustering color space](functionality/clustering-colorspace.md): `-X` geometry, normalization, interactions, and reproducible measurements.
- [Palette working color space](functionality/working-colorspace.md): `-W` palette-application geometry, lookup and dither interactions, precision, and reproducible quality, speed, and size measurements.
- [Final palette merge policy](functionality/merge-policy.md): `-F` Ward reduction, optional Lloyd polishing, objective tradeoffs, and reproducible quality, speed, size, geometry, and flat-patch measurements.
- [Palette cover policy](functionality/cover-policy.md): post-quantizer gamut reachability repair, soft and hard anchors, historical color-model connections, growth, and layered reachability measurement.
- [Palette snap policy](functionality/snap-policy.md): enablement, reversible 101-level tone mapping, timing, approach rate, and working-space target selection.
- [Encoder working precision](functionality/precision.md): requested and effective `--precision` paths and cross-policy measurement controls.
- [Alpha policy](loader/alpha-policy.md): source-alpha treatment, omitted SIXEL pixels, and `P2` selection.
- [Background policy](loader/background-policy.md): background-source priority, colorspace, and OSC 11 interaction.
- [CLI](cli/design-policy.md): option design, compatibility, parsing, diagnostics, and documentation synchronization.
- [Quality](quality/measurement-policy.md): perceptual metrics, fixtures, thresholds, baselines, and performance comparisons.
- [Testing](testing/guide.md): test organization, shell TAP rules, registration, portability, and required checks.
- [Threading](threading/README.md): worker budgets, band encoding and decoding, stage ordering, animation, and reproducible timeline measurements.
- [CI](ci/design.md): workflow responsibilities, matrix design, failure triage, and CI-change validation.
- [OpenVMS compatibility](misc/platforms/openvms.md): maintained GNV and OpenVMS build, status, linker, filesystem, test, and CI boundaries.

## Documentation rules

- Write documentation in English.
- Do not hard-wrap Markdown prose at a fixed column width. Keep each paragraph or list item on one source line unless Markdown syntax requires a line break. Fixed-width wrapping can split content into misleading translation segments and cause avoidable errors for non-English readers.
- Keep TeX commands in GitHub-rendered Markdown math within the conservative [GitHub math allowlist](../tests/_static/data/github-math-macros.txt). GitHub does not publish a stable complete command list, so verify a new command on a committed GitHub.com Markdown page before adding it to the allowlist. Prefer alphabetic commands such as `\lbrace` and `\rbrace`; Markdown can consume backslash-punctuation forms such as `\{` before math rendering.
- Keep project-wide entry documents intended for users, such as project history, format, and platform support references, directly under `docs/`.
- Keep one major concern per file and one broad subject per directory. Link related documents instead of merging their full contents into one large guide.
- Describe current contracts and durable rationale, not a transcript of a single debugging session.
- Link to owning code, test categories, or workflows when that makes a claim verifiable.
- Update the relevant document in the same change when a long-lived policy or architecture boundary changes.
- Keep issue investigations, temporary measurements, rollout logs, and private task reports in a local backup or external tracking system, not in `docs/`.

## Documentation-to-test traceability

Durable behavioral policy documents must make their automated coverage externally auditable when their normative claims are spread across multiple test categories. Opt such a document into enforcement with the exact marker `<!-- test-coverage: enforced -->` and include a `Test coverage` section.

The coverage inventory must follow these rules:

- Give each independently testable contract a stable coverage ID.
- Link each automated contract directly to its owning test with a Markdown link whose label is the repository-relative `tests/...` path.
- Add a reciprocal `Policy: docs/...` repository-relative path in a source comment near the beginning of every listed test. A test owned by multiple policy documents carries one reciprocal line for each document.
- Describe terminal-dependent, platform-dependent, or otherwise manual observations explicitly instead of presenting them as automated coverage.
- Audit defaults, accepted and rejected values, precedence, fallback, cross-option interaction, and repeated or multi-input state where those dimensions are part of the contract.

The [`staticcheck-doc-test-links`](../tests/_static/sh/staticcheck-doc-test-links.sh) check enforces both directions: every document link must name an existing test that links back, and every `Policy: docs/...` test reference must be present in the named document. Update the document and its owning tests in the same change; a one-sided update is a static-check failure.

## Adding a document

Add a document only when the topic is durable and materially distinct from the existing references. Give it a stable subject-oriented name, place it in the owning subject directory, and add it to `README.md` when users or contributors should discover it. Add a repository-level link only when contributors must read it before working in that area.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| DG-01 | Enforced policy documents and their owning tests carry complete reciprocal links, and one-sided references fail the static suite. | [tests/_static/sh/staticcheck-doc-test-links.sh](../tests/_static/sh/staticcheck-doc-test-links.sh) |
| DG-02 | Documentation math uses only TeX commands verified to render on GitHub.com. | [tests/_static/sh/staticcheck-docs-github-math.sh](../tests/_static/sh/staticcheck-docs-github-math.sh) |

### Coverage boundary

The static check validates paths and the completeness of the two link inventories. It does not decide whether a policy contract is decomposed at the correct granularity or whether its owning test proves the stated behavior; those are reviewed through the coverage audit required by this document.
