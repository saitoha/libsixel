# AGENTS.md

## Scope

These instructions apply to documentation under `docs/`. The repository-level
`AGENTS.md` continues to apply. User instructions take precedence.

## Purpose of this directory

`docs/` contains durable, project-wide engineering references. Each major topic
has its own document so that its contract, rationale, and maintenance history
can evolve independently.

Read the relevant document before changing its subject area:

- [Functional overview](functional-overview.md): capabilities, components,
  data flow, and architectural boundaries.
- [CLI design policy](cli-design.md): option design, compatibility, parsing,
  diagnostics, and documentation synchronization.
- [Quality measurement policy](quality-measurement.md): perceptual metrics,
  fixtures, thresholds, baselines, and performance comparisons.
- [Testing guide](testing.md): test organization, shell TAP rules,
  registration, portability, and required checks.
- [CI architecture and design](ci-design.md): workflow responsibilities,
  matrix design, failure triage, and CI-change validation.

## Documentation rules

- Write documentation in English.
- Keep one major concern per file. Link related documents instead of merging
  their full contents into one large guide.
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
existing references. Give it a stable subject-oriented name, add it to the
index above, and add a repository-level link when contributors must discover it
before working in that area.
