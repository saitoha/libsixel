# Documentation Index

This directory contains the durable libsixel documentation that users and
contributors should be able to discover from the repository. Detailed topics
are grouped by domain so each area can grow without overloading one document.

## Functionality

- [Functional overview](functionality/overview.md) describes the product
  capabilities, major components, data flow, and architectural boundaries.

## Command-line interface

- [CLI design policy](cli/design-policy.md) defines option design,
  compatibility, parsing, diagnostics, and documentation synchronization.

## Quality

- [Quality measurement policy](quality/measurement-policy.md) defines
  perceptual metrics, fixtures, thresholds, baselines, and performance
  comparisons.

## Testing

- [Testing guide](testing/guide.md) covers test organization, shell TAP rules,
  registration, portability, and required checks.

## Continuous integration

- [CI architecture and design](ci/design.md) describes GitHub Actions,
  @saitoha's local desktop CI, matrix design, and failure triage.
- [CI support matrix](ci/support-matrix.md) is the generated inventory of
  build configurations supported by GitHub Actions and local CI.

## Documentation governance

- [Documentation instructions](AGENTS.md) define the scope, organization, and
  maintenance policy for this directory.

Keep task-specific investigations, temporary measurements, rollout logs, and
private reports outside the tracked `docs/` tree unless they are deliberately
promoted into durable project documentation.
