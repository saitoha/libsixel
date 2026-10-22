# Documentation Index

This index lists every libsixel document, grouped into parts that run from an overview of the project through concepts, quality, and implementation to maintenance. Each entry says in one sentence what the document covers; the document itself opens with its full scope.

## Start here

| If you want to | Start with |
| --- | --- |
| Find out what libsixel is and what it can do | [Functional overview](overview.md) |
| Understand the SIXEL format | [SIXEL format overview](sixel-format/README.md) |
| Display or convert images with `img2sixel` and `sixel2png` | [Command-line options at a glance](cli/options.md) |
| Improve output quality | [Why quantization quality needs color, display, and vision knowledge](concepts/quality-optimization.md), then [Encoding pipeline](encoder/pipeline/README.md) |
| Use the library from C | [Functional overview](overview.md), then [Encoding pipeline](encoder/pipeline/README.md) and [Decoding pipeline](decoder/pipeline/README.md) |
| Learn how a SIXEL codec works by writing one | [Writing a SIXEL codec, step by step](tutorial/sixel-codec/README.md) |
| Build libsixel on your platform | [Build, runtime, and platform support](platform-support.md) |
| Contribute code or documentation | [Testing guide](testing/guide.md), [Staticcheck](testing/staticcheck.md), and [Documentation workflow](documentation/workflow.md) |

## Introduction

What libsixel is, where it came from, and the SIXEL format it implements.

### Project

- [Functional overview](overview.md) describes libsixel's capabilities, components, data flow, and architectural boundaries.
- [Project history and lineage](project-history.md) explains how libsixel grew from kmiya's `sixel` and how contributions shaped it.

### SIXEL format

- [SIXEL format overview](sixel-format/README.md) is a practical guide to the DEC SIXEL wire format, its control functions, rendering model, and interoperability limits.
- [Terminal graphics history](sixel-format/terminal-graphics-history/README.md) surveys how terminals have drawn pictures, from vector and raster protocols to images made of characters.
- [SIXEL specification status and interpretation layers](sixel-format/specification-layers.md) explains what the standards and DEC manuals define, how implementations interpret the rest, and where libsixel stands.
- [SIXEL patents and royalties](sixel-format/patents-and-royalties.md) summarizes the public evidence that implementing SIXEL requires no license or royalty.
- [SIXEL codec comparison](sixel-format/comparison/README.md) compares SIXEL's quality, speed, and size with JPEG, GIF, PNG, QOI, WebP, AVIF, and HEIC.

### SIXEL codec tutorial

- [Writing a SIXEL codec, step by step](tutorial/sixel-codec/README.md) is an illustrated course in which you write a SIXEL encoder and decoder, from a thirty-line codec to SIMD and threads.

## Using libsixel

Supported platforms and command-line behavior that need no background in color or encoding; encoding options are documented with their pipeline stage under Implementation.

### Platform support

- [Build, runtime, and platform support](platform-support.md) defines build requirements, runtime assumptions, CI-backed support tiers, and the platform matrix.

### Command-line basics

- [Command-line options at a glance](cli/options.md) lists every `img2sixel` and `sixel2png` option with a one-line summary and a link to its explanation.
- [Clipboard input and output](cli/clipboard.md) explains `-y`, its targets and backends, and headless workflows.
- [Choice prefix-matching policy](cli/prefix-matching.md) defines how abbreviated option values are resolved and when they are ambiguous.
- [CLI correction-suggestion policy](cli/correction-suggestions.md) defines the suggestions offered for ambiguous or misspelled values and paths.

### Companion programs and OS extensions

- [SIXEL Quick Look extension](programs/quicklook-extension.md) explains the macOS Quick Look bundles, their builds, registration, and limits.
- [SIXEL WIC codec](programs/wic-codec.md) explains the Windows WIC codec, its MSI deployment, entry points, and COM ownership.

## Concepts and theory

The foundations the rest of the documentation builds on, in reading order from vision and color through pixels and displays to palettes and dithering.

### Why these concepts matter

- [Why quantization quality needs color, display, and vision knowledge](concepts/quality-optimization.md) explains why judging and improving quantization requires knowing how color, displays, and vision work.

### Visual perception

- [Visual perception for image quality](quality/visual-perception/README.md) explains the physiology of vision that image-quality metrics rely on.
- [Character cell perception](quality/character-cell-perception/README.md) treats images drawn with characters as a display device and asks how much color and detail cells need.

### Color spaces

- [Color spaces](concepts/colorspace/README.md) explains what color numbers mean, how color spaces are defined and converted, and why mixing needs linear light.
- [Transfer functions and gamma](concepts/colorspace/transfer-functions.md) explains why images are gamma-encoded, the common transfer curves, and when arithmetic needs linear light.
- [Primaries and white points](concepts/colorspace/primaries-and-white-points.md) explains chromaticity diagrams, common gamuts and white points, and how they determine RGB-to-XYZ conversion.
- [Color management basics](concepts/color-management/README.md) explains ICC profiles, chromatic adaptation, rendering intents, and where libsixel manages color.
- [Gamut mapping](concepts/gamut-mapping/README.md) illustrates how out-of-gamut colors are brought into range and compares the mapping methods libsixel offers.

### Color difference

- [Color difference and color reproduction](quality/color-difference.md) compares color-difference formulas and measures and explains why small palettes shift color.
- [DIN99d and color difference](concepts/din99d/README.md) illustrates perceptual nearest-color selection in DIN99d and its measured palette trade-offs.

### Pixel representation and precision

- [Pixel formats and alpha representation](concepts/pixelformat.md) separates input memory layouts, frame storage, alpha metadata, and what a SIXEL stream can represent.
- [Pixel-format precision](concepts/pixelformat-precision/README.md) explains byte and float32 storage, when processing promotes precision, and what it costs.

### Display technologies

- [Display technologies](concepts/displays/README.md) follows displays from print and film to OLED and electronic paper and explains what pixel density means for dithering.

### Palettes and dithering

- [Palettes](concepts/palettes/README.md) explains indexed-color images, the history and kinds of palettes, and how palettes work in SIXEL.
- [Palette clustering](concepts/clustering/README.md) explains how a palette is chosen for an image and compares the main clustering methods.
- [Dithering](concepts/dithering/README.md) explains why few-color images need dithering, how the main method families work, and what dithering costs in SIXEL.

## Quality and measurement

How output quality is measured and accepted, building on the concepts above.

### Metrics

- [Image-quality metrics](quality/metrics.md) defines each `lsqa` metric and explains how to combine metrics for dithered output.
- [Video evaluation workflow](quality/video-workflow.md) walks through capturing an animation and interpreting its quality, persistence, size, and playback cost.
- [Video evaluation](quality/video-evaluation.md) describes how frames and clocks are aligned and how video quality, size, and residual persistence are reported.
- [Color-aware quality evaluation](quality/color-evaluation.md) defines how `lsqa` interprets input colors and measures color differences.

### Measurement practice

- [Quality measurement policy](quality/measurement-policy.md) defines the metrics, fixtures, thresholds, baselines, and performance comparisons used to accept changes.
- [Palette gradient analysis](quality/palette-gradient-analysis.md) provides a local tool to inspect how a source gradient is quantized.

## Implementation

The encoder and decoder in data-flow order, from loading an image to writing SIXEL or PNG, followed by cross-cutting execution topics.

### Encoder architecture

- [Encoding pipeline](encoder/pipeline/README.md) connects loading, palette construction, palette application, and SIXEL output, with their composed cost.
- [Encoder execution map](encoder/execution-map/README.md) traces the implementation path through each encoder stage and its scheduling.
- [Color-space handling](encoder/colorspace-handling/README.md) describes every implicit and explicit color conversion in the encoder and how to inspect the effective path.
- [Encoder working precision](encoder/precision/README.md) explains the `--precision-policy` axis and compares quality, speed, and size across processing policies.

### Image loading

- [Image loader architecture](loader/README.md) explains the loader registry, `-L` chains, fallback, typed frame outputs, and backend trade-offs.
- [Patent and license status of processing algorithms](misc/algorithm-patents.md) records what is known about patents and licenses for libsixel's processing methods.
- [Patent status of loader formats](loader/patents.md) records what is known about patents on each decoder and codec path.
- [Loader precision, gamut, and fast paths](loader/precision-policy.md) defines how loaders preserve precision, color, and alpha and when byte fast paths apply.
- [Loader color management](loader/color-management/README.md) defines how loaders apply color management and compares engines, fallbacks, quality, and cost.
- [Builtin CMS specification and architecture](loader/builtin-cms.md) explains the ICC subset the builtin CMS implements and its numerical and safety boundaries.
- [Builtin image loader](loader/builtin.md) documents the in-tree format families, their goals and security model, and their history.
- [Builtin format components](loader/builtin/README.md) indexes one implementation reference per builtin format family.
- [Builtin PNG source precision](loader/builtin/png-source-precision.md) describes how the builtin PNG decoder preserves original samples and falls back.
- [libpng image loader](loader/libpng.md) documents PNG and APNG support through libpng and its current gaps.
- [PNG color metadata and ColorSync compatibility](loader/png-color-metadata.md) defines how PNG color metadata is selected across loaders.
- [Alpha policy](loader/alpha-policy/README.md) defines background composition, alpha-zero preservation, SIXEL `P2` requests, and the loader/encoder boundary.
- [Background policy](loader/background-policy.md) defines how loaders choose between file and external backgrounds.
- [Network access](loader/network-access.md) explains URL input, its backends and TLS support, and the risks of `-k`.
- Codec adapters: [libjpeg](loader/libjpeg.md), [libwebp](loader/libwebp.md), [libtiff](loader/libtiff.md), and [librsvg](loader/librsvg.md) describe each library's routes, precision, metadata, and controls.
- Framework adapters: [WIC](loader/wic.md), [CoreGraphics/ImageIO](loader/coregraphics.md), [GdkPixbuf](loader/gdk-pixbuf2.md), and [GD](loader/gd.md) separate host capabilities from libsixel's integration.
- Preview adapters: [Quick Look](loader/quicklook.md) and [Freedesktop thumbnailers](loader/gnome-thumbnailer.md) explain provider discovery, rasterization, and metadata boundaries.

### Preprocessing

- [Crop and resize](encoder/preprocessing/crop-resize/README.md) defines geometry syntax, crop and resize order, and resize color space and precision.
- [Resampling](encoder/preprocessing/resampling/README.md) explains resize kernels and their quality, speed, and size, with one chapter per selectable method.

### Palette construction

- [Palette construction pipeline](encoder/palette-construction/pipeline/README.md) defines the stages that build a palette and how their automatic policies are resolved.
- [Palette quantization](encoder/palette-construction/quantization/README.md) explains `-Q` and the objectives and costs of each palette solver.
- [GPU K-means](encoder/palette-construction/gpu-kmeans.md) describes the Metal K-means backend, its CPU fallback, and its limits.
- [Palette clustering color space](encoder/palette-construction/clustering-colorspace/README.md) explains `-X` and its measured quality, speed, and size trade-offs.
- [Final palette merge policy](encoder/palette-construction/merge-policy/README.md) explains `-F`, Ward reduction, optional Lloyd polishing, and their measured trade-offs.
- [Palette cover policy](encoder/palette-construction/cover-policy/README.md) explains `-a` and how it repairs palette coverage of the source gamut.
- [Palette snap policy](encoder/palette-construction/snap-policy/README.md) explains `-_` and how it moves palette colors onto SIXEL's 101 reversible tone levels.
- [Built-in palettes and monochrome output](encoder/palette-construction/fixed-palettes.md) explains `-b`, `-e`, and `-i` and the built-in palette families.
- [External palette input and output](encoder/palette-construction/external-palettes.md) explains `-m` and `-M` and the palette file formats they read and write.

### Palette application

- [Palette working color space](encoder/palette-application/working-colorspace/README.md) explains `-W` and how it affects lookup, dithering, and measured trade-offs.
- [Lookup policy](encoder/palette-application/lookup-policy/README.md) explains `-~`, the exactness and cost of each nearest-color policy, and their measured comparison.
- [Delaunay lookup](encoder/palette-application/lookup-policy/delaunay.md) describes exact graph walks, representation boundaries, SIMD arithmetic and their tests.
- [Dithering](encoder/palette-application/dithering/README.md) explains `-d`, each error-diffusion and positional method, and their cost and measured quality.
- [Dither clamp policy](encoder/palette-application/clamp-policy.md) explains `-0` and how extended source values and diffusion feedback are clamped.

### SIXEL output

- [Encoding policy](encoder/output/encode-policy/README.md) explains `-E` and how six-row masks are serialized, with measured trade-offs.
- [Output color space](encoder/output/output-colorspace.md) explains `-U`, the output color spaces, and how receivers interpret palette values.
- [Output gamut mapping](encoder/output/gamut-mapping.md) specifies the `-U` gamut policies and their numeric contracts.
- [Delta encoding (6delta)](encoder/output/delta-encoding.md) explains `--update-policy` and how frames are updated incrementally.
- [OR-mode output](encoder/output/or-mode/README.md) explains `-O`, index-bit composition, receiver compatibility, and the X68030 console it was designed for.
- [High-color output](encoder/output/high-color/README.md) explains `-I`, how high-color streams use palette registers, and their measured cost.
- [DRCS-SIXEL output](encoder/output/drcs.md) explains `-@` and how images are sent as downloadable character glyphs.
- [Terminal macros](encoder/output/terminal-macros.md) explains `-u` and `-n` and how macro definitions and playback reduce transfer.

### Decoding

- [Decoding pipeline](decoder/pipeline/README.md) follows SIXEL bytes through parsing, painting, optional reconstruction, and PNG output.
- [Multi-pass still-image decoding](decoder/multi-pass.md) documents how progressive multi-pass streams are composed into one image.
- [Dequantization](decoder/dequantization/README.md) explains how palette and gradient inference reconstructs smoother images from SIXEL.

### Image writing

- [PNG writer](writers/png/README.md) defines the PNG writer backends, their output formats, and the PNG snapshot path in `img2sixel`.

### Animation, threading, and runtime execution

- [Animation playback](encoder/animation.md) explains `-l`, `-S`, `-T`, and `-g` and how animation interacts with other output features.
- [Threading documentation](threading/README.md) indexes the threading model, band encoding and decoding, budgets, and measurements.
- [Threading overview](threading/overview.md) defines bands, work bands, ordered publication, and their effect on output.
- [Runtime execution policy](cli/runtime-policy.md) explains `-j`, SIMD ceilings, scheduling thresholds, and encoder and decoder differences.

### Diagnostics

- [Diagnostics and logs](cli/diagnostics.md) explains `-x`, `-J`, and `-v`, trace topics, and timeline capture.
- [CLI abort trace diagnostics](cli/abort-trace/README.md) defines the stack trace printed when a converter aborts.

## Terminal protocol proposals

Receiver-independent proposals for terminal behavior beyond SIXEL, building on the color concepts and output behavior above.

### Color management

- [Terminal graphics color management proposal](protocols/graphics-color-management.md) proposes how applications and terminals can agree on graphics color interpretation.

## Development and maintenance

Building, testing, and maintaining libsixel and its documentation.

### Build system

- [Build system architecture](build/README.md) explains why Autotools and Meson are both maintained and how configuration, macros, and the test harness are organized.

### Testing

- [Testing guide](testing/guide.md) owns the general rules for test evidence, organization, registration, portability, and validation.
- [Staticcheck](testing/staticcheck.md) defines the repository-wide invariant checks and how to extend them.
- [Companion-program test inventory](testing/companion-programs-coverage.md) maps every numbered WIC test to its assertions and evidence limits.
- [Builtin loader coverage inventory](testing/builtin-loader-coverage.md) maps every builtin-loader test to its format and observation.
- [Mapfile parser coverage inventory](testing/mapfile-parser-coverage.md) maps every palette file parser and writer test to its observation.
- [Resampling regression coverage](testing/resampling-coverage.md) inventories the resampling regression probes and their numeric tolerances.
- [libpng cHRM boundary test plan](testing/png-chrm-boundaries.md) inventories the libpng cHRM boundary observations.
- [PNG cHRM arithmetic test plan](testing/png-chrm-arithmetic.md) separates the cHRM arithmetic tests by what each one verifies.

### Continuous integration

- [CI architecture and design](ci/design.md) describes the GitHub Actions and local CI, matrix design, and failure triage.

### Command-line implementation policy

- [CLI design policy](cli/design-policy.md) defines option design, compatibility, parsing, diagnostics, and documentation synchronization.
- [Converter suboption architecture](cli/suboptions.md) defines structured option grammar, the option registry, precedence, and environment-variable parity.

### Platform compatibility

- [Platform compatibility ledger](misc/platforms/README.md) indexes the per-platform compatibility contracts and their static checks.

### Documentation

- [Documentation manifesto](documentation/manifesto.md) states the design principles that keep the documentation one coherent work.
- [Documentation workflow](documentation/workflow.md) defines how a documentation change is classified, proposed, reviewed, and accepted.
- [Document kinds](documentation/document-kinds.md) defines the skeleton and canonical section names of each kind of document.
- [Documentation instructions](AGENTS.md) define document scope, organization, policy ownership, length limits, and document/test traceability.
- [Building the documentation site](documentation/site-build.md) explains how to build the searchable HTML site locally.
