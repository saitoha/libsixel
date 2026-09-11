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
- [Decoding pipeline](functionality/decoding-pipeline.md) follows SIXEL wire bytes through parser and paint state, raster policy, indexed and direct representations, optional reconstruction and resize, parallel fallback, and PNG output, with a reciprocal coverage audit.
- [Encoder execution map](functionality/encoder-execution-map.md) expands the implementation path through loader fallback, preprocessing, scheduler overlap, quantizer-specific work, lookup and dither application, and palette serialization.
- [Crop and resize](functionality/crop-resize.md) defines geometry syntax, crop/resize ordering, resize colorspace and precision policy, its SIMD relationship, measured quality, speed, size, transparency, and public frame helpers.
- [Resampling](functionality/resampling.md) details resize coordinate mapping, discrete response, moiré, edge normalization, separable passes, threading, SIMD dispatch, visual comparisons, and measured quality, speed, and size. Its [theory and history](functionality/resampling/theory-history.md), [compact methods](functionality/resampling/compact-kernels.md), and [wide-support methods](functionality/resampling/wide-kernels.md) chapters derive the kernels and document each selectable method individually.
- [Encoding policy](functionality/encode-policy.md) explains `-E`, six-row mask serialization, size-policy overpainting, and measured quality, speed, and stream-size behavior.
- [OR-mode output](functionality/or-mode.md) explains `-O`, index-bit composition, `P2=5`, palette-zero and transparency semantics, receiver compatibility, and regression coverage.
- [High-color output](functionality/high-color.md) explains `-I`, paint-time palette-register semantics, repeated 255-slot passes, terminal compatibility, and measured quality, speed, and size tradeoffs.
- [External palette input and output](functionality/external-palettes.md) explains `-m` and `-M`, palette-generation and reuse workflows, format origins, application compatibility, format detection, and fixed-palette policy interactions.
- [Palette construction pipeline](functionality/palette-pipeline.md) defines
  the sampling, palette-space, binning, and quantization boundaries, joint
  automatic policy resolution, typed artifacts, and migration waves.
- [Palette quantization](functionality/quantization.md) explains `-Q`, palette
  solver objectives, asymptotic costs, and the boundary between palette
  generation and use.
- [Final palette merge policy](functionality/merge-policy.md) explains `-F`, Ward oversplit-and-reduce processing, optional Lloyd polishing, measured quality, speed, size, palette geometry, and flat-patch reachability.
- [Palette cover policy](functionality/cover-policy.md) explains `-a` / `--cover-policy`, post-quantizer gamut reachability repair, soft and hard anchor selection, the links among gamut mapping, Neugebauer printing, and error-diffusion stability, and a three-layer measurement model.
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
- [Lookup policy](functionality/lookup-policy.md) defines the `-~` /
  `--lookup-policy` interface provided by `img2sixel`, its mathematical basis, exactness guarantees, and
  preparation/query costs. It contains one detailed chapter per policy and a
  measured Delta E, chroma, MS-SSIM, and runtime comparison across palette
  sizes.

## Image loading

- [Image loader architecture](loader/README.md) explains the compiled loader registry, `-L` chain construction, predicate and fallback control flow, typed frame outputs, backend inventory, and quality/security tradeoffs.
- [Loader CMS: builtin and Little CMS](loader/color-management.md) compares profile support, unsupported cases, fallback, rendering intents, quality, and speed/memory tradeoffs.
- [Builtin CMS specification and architecture](loader/builtin-cms.md) explains the implemented ICC subset, profile and transform ownership, numerical behavior, safety boundaries, and coverage limits.
- [Builtin image loader](loader/builtin.md) documents the in-tree format families, precision and colorspace goals, security model, and the extraction history from the original stb_image integration.
- [Builtin format components](loader/builtin/README.md) indexes one implementation reference per builtin format family, with exact variants, metadata and extension handling, output representation, history, and explicit exclusions.
- [Alpha policy](loader/alpha-policy.md) defines loader-side background
  composition, alpha-zero preservation, SIXEL `P2` requests, terminal-specific
  rendering, and the loader/encoder ownership boundary.
- [Background policy](loader/background-policy.md) defines how builtin PNG,
  APNG, and GIF loaders choose between file and external backgrounds, including
  option precedence, colorspace, and OSC 11 interaction.

## Image writing

- [PNG writer](writers/png.md) defines the libpng and builtin writer backends, indexed and direct 8-bit output, and the post-SIXEL PNG snapshot path used by `img2sixel`.

## Command-line interface

- [CLI design policy](cli/design-policy.md) defines option design,
  compatibility, parsing, diagnostics, and documentation synchronization.
- [Converter suboption architecture](cli/suboptions.md) defines structured option grammar, typed registry metadata, consumer scopes, environment-variable parity, precedence, and implementation flow.
- [Choice prefix-matching policy](cli/prefix-matching.md) defines exact and unique-prefix resolution, ambiguity, CLI/environment vocabulary separation, and compatibility constraints.
- [CLI correction-suggestion policy](cli/correction-suggestions.md) defines ambiguous-prefix, fuzzy-name, and nearby-path guidance, including defaults, ranking, diagnostics, performance, and privacy boundaries.
- [CLI abort trace diagnostics](cli/abort-trace.md) defines the CLI-layer ownership, current converter consumers, non-consuming `lsqa` boundary, controls, platform behavior, representative output, reliability, performance, output-quality boundary, and test coverage of the `SIGABRT` stack trace.

## Quality

- [Quality measurement policy](quality/measurement-policy.md) defines
  perceptual metrics, fixtures, thresholds, baselines, and performance
  comparisons.

## Testing

- [Testing guide](testing/guide.md) covers test organization, shell TAP rules,
  registration, portability, and required checks.
- [Builtin loader coverage inventory](testing/builtin-loader-coverage.md) maps every builtin-loader shell test to its primary format, observation, and traceability role while keeping pipeline landmarks distinct from the exhaustive regression suite.
- [Mapfile parser coverage inventory](testing/mapfile-parser-coverage.md) maps every structured-palette parser and writer test to its exact observation while keeping behavioral contracts distinct from defensive and supplementary coverage.
- [Resampling regression coverage](testing/resampling-coverage.md) inventories exact precision, SIMD, thread, discrete-stencil, frequency, isotropy, and moiré regression probes and their numeric tolerance table.
- [Staticcheck](testing/staticcheck.md) defines libsixel's repository-wide invariant-verification target, its broader-than-lint scope, IDL and cross-surface checks, test meta-checks, CLI synchronization role, and extension workflow.

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

## Platform compatibility

- [Platform compatibility ledger](misc/platforms/README.md) indexes the maintained OpenVMS, Windows runtime, Emscripten, Cosmopolitan, macOS, POSIX libc, Haiku, and Solaris compatibility contracts and their reciprocal static checks.

## Documentation governance

- [Documentation instructions](AGENTS.md) define the scope, organization, and
  maintenance policy for this directory.

## Building the HTML site

Run `tools/build-docs` from any directory to build the complete searchable HTML site under `site/`. The command creates and caches a private virtual environment under `.docs-build/`; MkDocs and Material for MkDocs remain optional and are not required for configuring, building, testing, or installing libsixel itself. The first documentation build requires network access to install the versions pinned in `tools/docs/requirements.txt`; subsequent builds reuse the cached environment.

An Autotools build provides the equivalent `make docs` target, and a Meson build provides `meson compile -C BUILDDIR docs`. Both write the generated site to `site/` under their build directory. Pass `--no-bootstrap` to `tools/build-docs` only when the current Python environment already contains the pinned documentation packages.

Keep task-specific investigations, temporary measurements, rollout logs, and
private reports outside the tracked `docs/` tree unless they are deliberately
promoted into durable project documentation.
