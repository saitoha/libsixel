# AGENTS.md

## Scope

These instructions apply to documentation under `docs/`. The repository-level
`AGENTS.md` continues to apply. User instructions take precedence.

## Purpose of this directory

`docs/` contains durable, project-wide references that users and contributors
should be able to discover from the repository. Keep the top level limited to
the documentation index, documentation governance, and similarly important
entry points. Put detailed material in a subject directory so that each area
can grow into multiple documents without crowding the top level.

Start from the [documentation index](README.md). Read the relevant document
before changing its subject area:

- [Project history and lineage](project-history.md): origin, development
  milestones, contribution history, and attribution sources.
- [SIXEL format](sixel-format.md): wire syntax, control functions, rendering
  semantics, interoperability, and implementation references.
- [Build, runtime, and platform support](platform-support.md): build and
  runtime requirements, CI-backed support tiers, and platform matrix.
- [Functionality](functionality/overview.md): capabilities, components, data
  flow, and architectural boundaries.
- [CLI](cli/design-policy.md): option design, compatibility, parsing,
  diagnostics, and documentation synchronization.
- [Quality](quality/measurement-policy.md): perceptual metrics, fixtures,
  thresholds, baselines, and performance comparisons.
- [Testing](testing/guide.md): test organization, shell TAP rules,
  registration, portability, and required checks.
- [CI](ci/design.md): workflow responsibilities, matrix design, failure
  triage, and CI-change validation.
- [CI support matrix](ci/support-matrix.md): generated GitHub Actions and
  @saitoha local desktop CI build-configuration inventory.

## Documentation rules

- Write documentation in English.
- Keep project-wide entry documents intended for users, such as project
  history, format, and platform support references, directly under `docs/`.
- Keep one major concern per file and one broad subject per directory. Link
  related documents instead of merging their full contents into one large
  guide.
- Describe current contracts and durable rationale, not a transcript of a
  single debugging session.
- Link to owning code, test categories, or workflows when that makes a claim
  verifiable.
- Update the relevant document in the same change when a long-lived policy or
  architecture boundary changes.
- Keep issue investigations, temporary measurements, rollout logs, and private
  task reports in a local backup or external tracking system, not in `docs/`.

## Adding a document

Add a document only when the topic is durable and materially distinct from the
existing references. Give it a stable subject-oriented name, place it in the
owning subject directory, and add it to `README.md` when users or contributors
should discover it. Add a repository-level link only when contributors must
read it before working in that area.
