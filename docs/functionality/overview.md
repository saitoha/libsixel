# Functional Overview

## Project role

libsixel is a C implementation of the DEC SIXEL terminal graphics format. It
provides the library and tools needed to load raster images, process their
pixels, encode them as terminal-safe SIXEL escape sequences, and decode SIXEL
data back into raster output.

SIXEL is both an image representation and a terminal protocol payload. This
means that byte-stream correctness, terminal-safe output, image quality,
resource limits, and compatibility all belong to the core product contract.

## User-facing capabilities

### Encoding

The encoder accepts decoded or caller-supplied pixels and produces SIXEL. The
normal fixed-palette path can be summarized as:

```text
loader -> normalized pixels -> palette construction -> palette application
       -> indexed pixels and palette -> SIXEL encoding
```

The detailed [Encoding Pipeline](encoding-pipeline.md) explains the input and output contract of each stage and how these controls cooperate:

- [Palette Quantization](quantization.md), selected by `-Q`, determines which
  palette colors are available and documents each solver's objective and cost;
- [Palette Cover Policy](cover-policy.md), selected by `-a` or `--cover-policy`, repairs reachability after quantization;
- [Palette Snap Policy](snap-policy.md), selected by `-_` or `--snap-policy`, enables and configures reversible safe-tone mapping;
- [Palette Clustering Color Space](clustering-colorspace.md), selected by `-X`,
  determines the coordinate geometry used to construct those palette colors;
- [Working Color Space](working-colorspace.md), selected by `-W`, determines the coordinate geometry used to apply the palette through lookup and dithering;
- [Encoder Working Precision](precision.md), selected by `--precision`,
  determines the base sample representation shared across palette
  construction, palette application, dithering, lookup, and encoding;
- [Dithering](dithering.md), selected by `-d`, determines how representation
  error is distributed and the state required to do so;
- [Lookup Policy](lookup-policy.md), selected by `-~` or `--lookup-policy`,
  determines how each color candidate is mapped to a palette index and whether
  cost is paid during preparation or per-pixel query.

The broader pipeline also includes:

- image and frame normalization;
- resizing and pixel-format conversion;
- colorspace handling;
- palette selection, quantization, lookup, and dithering;
- transparency and background handling;
- animation and inter-frame policy;
- SIXEL command generation and output buffering.

Encoding policy must remain separate from loader-specific details. A loader
describes the input correctly; shared processing layers decide how to represent
that input in SIXEL.

### Decoding

The decoder parses SIXEL control sequences, palette definitions, raster
attributes, repeat commands, and image data into a raster representation. It
must handle malformed and adversarial streams without out-of-bounds access,
integer overflow, unbounded allocation, ownership errors, or partially
initialized output.

Deprecated and raw decoding entry points may expose different output contracts.
Do not assume that a compatibility API has the same palette, alpha, or ownership
semantics as a newer API; verify the public header and its focused tests.

### Image loading

The project supports built-in and optional loaders for multiple raster and
document formats. Availability varies by platform and configured dependency.
Loader responsibilities include:

- format detection and decoding;
- frame enumeration and timing;
- dimensions and pixel-format reporting;
- alpha and background semantics;
- orientation and metadata handling;
- embedded color-profile handling;
- bounded parsing and error propagation.

Loader selection and loader suboptions are public policy surfaces. Shared
behavior should live in the loader registry or common pipeline rather than be
reimplemented independently by each backend.

### Command-line tools

The principal converters are:

- `img2sixel`, which loads source images and emits SIXEL;
- `sixel2png`, which decodes SIXEL and emits raster output.

The command-line tools combine library functionality with CLI-specific policy,
including option parsing, environment precedence, filesystem behavior,
diagnostics, and process exit status. The detailed rules are in
[CLI Design Policy](../cli/design-policy.md).

### Quality assessment

`lsqa` compares a reference image with an output image and reports perceptual
and diagnostic metrics. It is used to detect visual regressions that byte-level
tests cannot describe. Its use and threshold policy are defined in
[Quality Measurement Policy](../quality/measurement-policy.md).

### Threading

Threading is organized as stage-local worker pools with explicit ordered
publication boundaries. The [Threading documentation](../threading/README.md)
defines SIXEL bands, palette-application work bands, decoder byte spans,
encoder and decoder budgets, animation handoff, and reproducible timelines.

### Integration surfaces

libsixel is consumed through several surfaces:

- installed public C headers and libraries;
- the command-line converters;
- language bindings;
- the amalgamated distribution;
- platform integrations and image-loader plugins;
- examples that demonstrate embedding or terminal output.

A change is incomplete if only one exposed surface learns about a new public
constant, option, source file, or lifecycle contract.

## Repository map

- `src/` contains core encoding, decoding, loading, quantization, palette, and
  processing implementation.
- `include/` contains installed public C headers.
- `converters/` contains CLI frontends, manual pages, and shell completion.
- `assessment/` contains `lsqa` and quality metric implementation.
- `tests/` contains functional, regression, quality, static, security, and
  binding tests.
- `fuzz/` contains fuzz targets, dictionaries, and runner support.
- `amalgamation/` contains the single-file distribution path.
- `examples/` and language-specific directories contain integration examples
  and bindings.
- `.github/workflows/` contains CI, experimental, fuzzing, nightly, and triage
  workflows.

## Architectural boundaries

Preserve these boundaries when adding functionality:

1. Public APIs define stable caller-visible types, ownership, and status.
2. CLI code owns process policy and translates options into library operations.
3. Loaders translate input formats into shared frame and pixel contracts.
4. Processing policy chooses colorspaces, palettes, lookup, dithering, and
   inter-frame behavior independently of a particular input loader.
5. Encoder and decoder code own the SIXEL protocol representation.
6. Platform backends implement platform capability without redefining shared
   semantics.
7. Tests and static checks enforce cross-surface synchronization.

Avoid duplicating policy across those layers. Prefer a registry, shared model,
or narrow interface when several consumers need the same definition.

## Build and distribution model

Autotools and Meson are supported build systems. The amalgamated library is an
additional distribution surface, not an unrelated fork of the implementation.
Keep source lists, feature defines, generated inputs intentionally committed to
the repository, public constants, and tests synchronized across applicable
build paths.

Optional dependencies must have explicit enabled, disabled, and unavailable
behavior. A feature-disabled build is a supported configuration when the build
system advertises it as such; it must compile without unused code and tests must
report intentional skips rather than false passes.

## Design priorities

When requirements compete, use these priorities:

1. Memory safety, bounds safety, and ownership correctness.
2. Stable public API, CLI, and byte-stream contracts.
3. Correct image semantics, including frames, alpha, orientation, and color.
4. Cross-platform and cross-build-system behavior.
5. Deterministic, measurable image quality.
6. Performance improvements supported by reproducible measurement.

An optimization must preserve the applicable correctness and quality contracts.
Do not hide a semantic regression behind a better throughput number.
