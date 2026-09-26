# AGENTS.md

## Scope

These instructions apply to documentation under `docs/`. The repository-level `AGENTS.md` continues to apply. User instructions take precedence.

## Purpose of this directory

`docs/` contains durable, project-wide references that users and contributors should be able to discover from the repository. Keep the top level limited to the documentation index, documentation governance, and similarly important entry points. Put detailed material in a subject directory so that each area can grow into multiple documents without crowding the top level.

Start from the [documentation index](README.md). Read the relevant document before changing its subject area:

- [Project history and lineage](project-history.md): origin, development milestones, contribution history, and attribution sources.
- [SIXEL format overview](sixel-format/README.md): wire syntax, control functions, rendering semantics, interoperability, and implementation references.
- [Build, runtime, and platform support](platform-support.md): build and runtime requirements, CI-backed support tiers, and platform matrix.
- [Color spaces and loader color management](concepts/colorspace/README.md): color interpretation, transfer functions, encoder color-space boundaries, and loader ICC/CMS behavior.
- [Loader precision, gamut, and fast paths](loader/precision-policy.md): backend representation, preservation boundaries, byte eligibility, and validation.
- [Pixel-format precision](concepts/pixelformat-precision/README.md): byte and float32 storage, implicit promotion boundaries, resize precision, and focused quality measurements.
- [Pixel formats and alpha representation](concepts/pixelformat.md): input memory layouts, frame storage, transparency metadata, and the SIXEL wire boundary.
- [Functional overview](overview.md): capabilities, components, data flow, and architectural boundaries. Encoder topics live under `encoder/` (preprocessing, palette construction, palette application and output) and decoder topics under `decoder/`.
- [Palette clustering color space](encoder/palette-construction/clustering-colorspace/README.md): `-X` geometry, normalization, interactions, and reproducible measurements.
- [Palette working color space](encoder/palette-application/working-colorspace/README.md): `-W` palette-application geometry, lookup and dither interactions, precision, and reproducible quality, speed, and size measurements.
- [External palette input and output](encoder/palette-construction/external-palettes.md): `-m` and `-M` workflows, format origins, application compatibility, detection rules, and fixed-palette policy interactions.
- [Final palette merge policy](encoder/palette-construction/merge-policy/README.md): `-F` Ward reduction, optional Lloyd polishing, objective tradeoffs, and reproducible quality, speed, size, geometry, and flat-patch measurements.
- [Palette cover policy](encoder/palette-construction/cover-policy/README.md): post-quantizer gamut reachability repair, soft and hard anchors, historical color-model connections, growth, and layered reachability measurement.
- [Palette snap policy](encoder/palette-construction/snap-policy/README.md): enablement, reversible 101-level tone mapping, timing, approach rate, and working-space target selection.
- [Encoder working precision](encoder/precision/README.md): requested and effective `--precision` paths and cross-policy measurement controls.
- [Alpha policy](loader/alpha-policy/README.md): source-alpha treatment, omitted SIXEL pixels, and `P2` selection.
- [Background policy](loader/background-policy.md): background-source priority, colorspace, and OSC 11 interaction.
- [CLI](cli/design-policy.md): option design, compatibility, parsing, diagnostics, and documentation synchronization.
- [Quality](quality/measurement-policy.md): perceptual metrics, fixtures, thresholds, baselines, and performance comparisons.
- [Testing](testing/guide.md): test organization, shell TAP rules, registration, portability, and required checks.
- [Staticcheck](testing/staticcheck.md): the repository-wide invariant-verification target, cross-surface synchronization, generated artifacts, and test meta-checks.
- [Threading](threading/README.md): worker budgets, band encoding and decoding, stage ordering, animation, and reproducible timeline measurements.
- [CI](ci/design.md): workflow responsibilities, matrix design, failure triage, and CI-change validation.
- [Platform compatibility ledger](misc/platforms/README.md): maintained platform, ABI, C-runtime, tool, test-harness, and reciprocal staticcheck boundaries, including OpenVMS, MinGW, Haiku, and Solaris.

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
- Describe experimental terminal extensions through receiver-independent protocol proposals. Do not name private consumers, link to their repositories, or include their launch commands, internal interfaces, or integration records in public documentation or commit messages. Separate proposed behavior from implemented libsixel behavior.

## Policy ownership and references

Each normative policy has one owning document. Before adding guidance, locate that owner through the documentation index; extend an existing owner when the subject already has one. Guidance that applies unchanged to unrelated subsystems belongs in a shared document, not in each subsystem's page.

| Subject | Owning document |
| --- | --- |
| General test design, assertion evidence, test structure and execution | [Testing guide](testing/guide.md) |
| Image-quality metrics, thresholds, fixtures and comparison conditions | [Image-quality measurement policy](quality/measurement-policy.md) |
| Documentation structure, policy reuse and document/test traceability | This document |
| A subsystem's behavior, supported inputs, numeric contracts and implementation limits | Its subject-specific reference, linked from the [index](README.md) |

Other documents link to the relevant section of the owner. A short, explicitly scoped introduction may explain why the policy applies, but must not create a second normative definition. Do not copy general instructions, threshold lists, defaults or entire contract tables merely to make a topic page self-contained. Topic-specific examples and error budgets belong with the topic; the shared method for choosing or validating them belongs with the general policy.

A topic's `Test coverage` section maps that topic's contracts to evidence and states its coverage limits. It is not a place to restate project-wide testing advice. Keep detailed backend contract tables and exhaustive test inventories at their respective owners, and refer to them from broader policy pages. A test may support distinct contracts at different levels, but that does not justify duplicating the same table or policy text.

When changing a policy, update the owner and any affected references in the same change. When consolidating existing duplicates, preserve the applicable requirements, reconcile differing scopes or conflicting wording in the owner, and replace the other definitions with links. Do not silently choose one version based on its location or recency. An exception must have an explicit scope recorded by the owner rather than forming a competing policy in another page.

Review documentation changes for ownership, duplication, reference accuracy and consistency of scope. These are editorial requirements; link and coverage checks do not establish that two passages have the same meaning or detect policy drift.

## Documentation-to-test traceability

Durable behavioral policy documents must make their automated coverage externally auditable when their normative claims are spread across multiple test categories. Opt such a document into enforcement with the exact marker `<!-- test-coverage: enforced -->` and include a `Test coverage` section.

The coverage inventory must follow these rules:

- Keep behavioral-contract, quality-regression and defensive coverage inventories distinct. When a document discusses all three, use separate `Behavioral contract tests`, `Quality regression tests`, and `Defensive and malformed-input tests` subsections. The [testing guide](testing/guide.md#assertions-and-evidence) defines what their assertions can establish.
- Give each independently testable contract a stable coverage ID.
- Link each automated contract directly to its owning test with a Markdown link whose label is the repository-relative `tests/...` path.
- Add a reciprocal `Policy: docs/...` repository-relative path in a source comment near the beginning of every listed test. A test owned by multiple policy documents carries one reciprocal line for each document.
- A `Policy:` backlink means that the test directly owns a documented behavioral contract; do not add it merely because a defensive test belongs to the same subsystem. Summarize defensive coverage by category and link to its test directory or inventory with a descriptive Markdown label that is not presented as an owning-test link.
- Invalid specimens derived from a documented accepted grammar do not each become independent behavioral contracts; they can remain in the defensive summary. If the document elevates a particular rejection outcome, diagnostic, resource limit, recovery rule, or other failure behavior to a stable user-visible guarantee, give it a coverage ID and a direct reciprocal test link instead of leaving it only in that summary.
- A directory or suite link describes the breadth of defensive coverage but is not evidence that every normative claim in the document has an owning test.
- Describe terminal-dependent, platform-dependent, or otherwise manual observations explicitly instead of presenting them as automated coverage.
- Audit defaults, accepted and rejected values, precedence, fallback, cross-option interaction, and repeated or multi-input state where those dimensions are part of the contract.

The [`staticcheck-doc-test-links`](../tests/_static/sh/staticcheck-doc-test-links.sh) check enforces both directions: every document link must name an existing test that links back, and every `Policy: docs/...` test reference must be present in the named document. Update the document and its owning tests in the same change; a one-sided update is a static-check failure.

An exhaustive test-plan inventory serves a different purpose from a behavioral coverage table. Use it when every test in a suite should remain discoverable from documentation, including malformed-input variants, resource boundaries, fault-localization probes, and supplementary smoke tests that do not each define a public guarantee.

- Put the exhaustive inventory in a dedicated testing document instead of expanding a user-facing policy document into a test ledger.
- Delimit the inventory with `<!-- test-plan: tests/path/*.t -->` and `<!-- test-plan-end -->`. The glob names the complete, nonrecursive set owned by that inventory.
- Link every matched test exactly once inside the delimited inventory, using its repository-relative `tests/...` path as the Markdown label.
- Add one reciprocal `Test-plan: docs/...` source comment within the first 20 lines of every matched test. A test that directly owns a public behavioral contract carries both `Policy:` and `Test-plan:`; the latter does not replace the former.
- Keep the observation in the test filename and opening comment aligned with the inventory description. The inventory should explain why the cases are grouped, while the test remains the authoritative detail for its fixture and assertion.
- Do not interpret `Test-plan:` as a public compatibility or stability promise. Promote a rejection, diagnostic, resource limit, or recovery rule to the behavioral coverage table and add `Policy:` only when the policy document deliberately guarantees it.

The [`staticcheck-test-plan-links`](../tests/_static/sh/staticcheck-test-plan-links.sh) check expands every marked glob and enforces exact, duplicate-free document links and reciprocal `Test-plan:` comments. It also runs the builtin-loader inventory generator in `--check` mode, so generated assertion-mechanism counts and rows cannot drift from test sources. A suite addition, deletion, rename, moved link, one-sided comment change, or stale generated summary therefore fails the static suite until the inventory is regenerated.

## Document-type inventory

[`tools/docs/doc-types.tsv`](../tools/docs/doc-types.tsv) records one row per Markdown document under `docs/`: `path` is repository-relative; `type` is the primary [Diátaxis](https://diataxis.fr/start-here/) purpose (`tutorial`, `how-to`, `reference` or `explanation`); `audience` is `user`, `contributor` or `both`; `note` identifies mixed purposes or audiences and is otherwise empty. Classify from the opening sections and headings, not the directory name or the largest section alone. `user` includes CLI users and library integrators; `contributor` means repository development and maintenance; a short implementation/evidence link alone does not make a page `both`. Update the inventory in the same change that adds, removes, renames or materially changes a document.

The [`staticcheck-doc-types-inventory`](../tests/_static/sh/staticcheck-doc-types-inventory.sh) check requires exactly one row per document and only the listed type and audience values. It does not judge whether a classification is correct; that remains an editorial review.

## Adding a document

Add a document only when the topic is durable and materially distinct from the existing references. Give it a stable subject-oriented name, place it in the owning subject directory, and add it to `README.md` when users or contributors should discover it. Add a repository-level link only when contributors must read it before working in that area.

Keep the directory layout aligned with the index classification. A document without figures or data is a single `topic.md` file in its category directory. A document with figures, measurements or other assets is a directory `topic/` whose `README.md` is the document, with assets in `topic/figures/`, `topic/measurements/` or a similarly named subdirectory, and with its subpages beside `README.md`. Do not place asset directories such as `topic-figures/` next to documents. Assets shared by several documents of one directory belong in that directory's own `figures/` or `measurements/`; a document outside it links to the owner's assets instead of copying them. Category directories such as `encoder/palette-construction/` do not have a `README.md`, because the index already provides their grouping.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| DG-01 | Enforced policy documents and their owning tests carry complete reciprocal links, and one-sided references fail the static suite. | [tests/_static/sh/staticcheck-doc-test-links.sh](../tests/_static/sh/staticcheck-doc-test-links.sh) |
| DG-02 | Documentation math uses only TeX commands verified to render on GitHub.com. | [tests/_static/sh/staticcheck-docs-github-math.sh](../tests/_static/sh/staticcheck-docs-github-math.sh) |
| DG-03 | Marked test-plan inventories contain every scoped test exactly once and every listed test links back without using `Policy:` for non-contract coverage. | [tests/_static/sh/staticcheck-test-plan-links.sh](../tests/_static/sh/staticcheck-test-plan-links.sh) |
| DG-04 | The document-type inventory lists every Markdown document under `docs/` exactly once with a known type and audience. | [tests/_static/sh/staticcheck-doc-types-inventory.sh](../tests/_static/sh/staticcheck-doc-types-inventory.sh) |

### Coverage boundary

The static checks validate paths, the completeness of reciprocal behavioral-contract and test-plan inventories, and document-type inventory coverage. They do not infer missing contracts from prose, treat a defensive inventory as contract coverage, decide whether a policy contract is decomposed at the correct granularity, or decide whether its owning test proves the stated behavior; those are reviewed through the coverage audit required by this document.
