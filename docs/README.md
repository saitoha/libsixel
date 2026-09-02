# Documentation Index

This directory contains the durable libsixel documentation that users and
contributors should be able to discover from the repository. Detailed topics
are grouped by domain so each area can grow without overloading one document.

## Project and format

- [Project history and lineage](project-history.md) explains how libsixel was
  derived from KMIYA's `sixel`, how the project developed, and how community
  contributions shaped it.
- [SIXEL format](sixel-format.md) is a practical guide to the DEC SIXEL wire
  format, its control functions, rendering model, and interoperability limits.

## Requirements and support

- [Build, runtime, and platform support](platform-support.md) defines the
  build requirements, runtime assumptions, CI-backed support tiers, and the
  OS, architecture, compiler, and build-system matrix.

## Functionality

- [Functional overview](functionality/overview.md) describes the product
  capabilities, major components, data flow, and architectural boundaries.
- [Encoding pipeline](functionality/encoding-pipeline.md) connects loading,
  palette construction, palette application, and SIXEL byte generation.
- [Palette quantization](functionality/quantization.md) explains `-Q`, palette
  solver objectives, and the boundary between palette generation and use.
- [Dithering](functionality/dithering.md) explains `-d`, error shaping, scan
  order, temporal behavior, and its interaction with lookup.
- [Lookup policy](functionality/lookup-policy.md) defines libsixel's `-~` /
  `--lookup-policy` concept and its correctness and performance tradeoffs.

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
