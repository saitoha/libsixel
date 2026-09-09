# Documentation Index

This directory contains the durable libsixel documentation that users and
contributors should be able to discover from the repository. Detailed topics
are grouped by domain so each area can grow without overloading one document.

## Project and format

- [Project history and lineage](project-history.md) explains how libsixel was
  derived from kmiya's `sixel`, how the project developed, and how community
  contributions shaped it.
- [SIXEL format](sixel-format.md) is a practical guide to the DEC SIXEL wire
  format, its control functions, rendering model, and interoperability limits.

## Requirements and support

- [Build, runtime, and platform support](platform-support.md) defines the
  build requirements, runtime assumptions, CI-backed support tiers, and the
  OS, architecture, compiler, and build-system matrix.

## Concepts

- [Color spaces and loader color management](concepts/colorspace.md) separates color values, pixel formats, color-space interpretation, and ICC/CMS conversion, then connects loader output with the `-X`, `-W`, and `-U` encoder boundaries.
- [Pixel-format precision](concepts/pixelformat-precision.md) explains the `RGB888` and typed float32 storage contracts, implicit promotion by resize, CMS, and color-space conversion, memory cost, and measured resize quality.
- [Pixel formats and alpha representation](concepts/pixelformat.md) separates
  input memory layouts, frame storage, alpha-zero metadata, and what can be
  represented in a SIXEL stream.

## Functionality

- [Functional overview](functionality/overview.md) describes the product
  capabilities, major components, data flow, and architectural boundaries.
- [Encoding pipeline](functionality/encoding-pipeline.md) connects loading,
  palette construction, palette application, SIXEL byte generation, and their
  composed cost model.
- [Palette construction pipeline](functionality/palette-pipeline.md) defines
  the sampling, palette-space, binning, and quantization boundaries, joint
  automatic policy resolution, typed artifacts, and migration waves.
- [Palette quantization](functionality/quantization.md) explains `-Q`, palette
  solver objectives, asymptotic costs, and the boundary between palette
  generation and use.
- [Final palette merge policy](functionality/merge-policy.md) explains `-F`, Ward oversplit-and-reduce processing, optional Lloyd polishing, objective tradeoffs, and measured quality, speed, and size.
- [Palette cover policy](functionality/cover-policy.md) explains `-a` / `--cover-policy`, post-quantizer gamut reachability repair, soft and hard anchor selection, growth, and cost.
- [Palette snap policy](functionality/snap-policy.md) explains how `-_` / `--snap-policy` enables and configures the 101 reversible SIXEL tone levels, working-space target selection, timing, and approach rate.
- [Palette clustering color space](functionality/clustering-colorspace.md)
  explains `-X`, its coordinate geometry and normalization, option
  interactions, and measured quality, speed, and size tradeoffs.
- [Palette working color space](functionality/working-colorspace.md) explains `-W`, its palette-application geometry, lookup and dither interactions, precision requirements, and measured quality, speed, and size tradeoffs.
- [Encoder working precision](functionality/precision.md) explains the
  cross-cutting `--precision` axis, effective 8-bit and float32 paths, and the
  shared quality, speed, and size comparison across processing policies.
- [Dithering](functionality/dithering.md) explains `-d`, error shaping, scan
  order, temporal behavior, per-pixel cost, and its interaction with lookup.
- [Lookup policy](functionality/lookup-policy.md) defines libsixel's `-~` /
  `--lookup-policy` concept, mathematical basis, exactness guarantees, and
  preparation/query costs. It contains one detailed chapter per policy and a
  measured Delta E, chroma, MS-SSIM, and runtime comparison across palette
  sizes.

## Image loading

- [Alpha policy](loader/alpha-policy.md) defines loader-side background
  composition, alpha-zero preservation, SIXEL `P2` requests, terminal-specific
  rendering, and the loader/encoder ownership boundary.
- [Background policy](loader/background-policy.md) defines how builtin PNG,
  APNG, and GIF loaders choose between file and external backgrounds, including
  option precedence, colorspace, and OSC 11 interaction.

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

## Threading

- [Threading documentation](threading/README.md) indexes the execution model,
  band encoding and decoding, encoder and decoder budgets, animation, timeline
  interpretation, and reproducible measurements.
- [Threading overview](threading/overview.md) defines SIXEL bands,
  palette-application work bands, decoder byte spans, ordered publication, and
  the small quality differences that work-band seam policy can introduce.

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
