# Documentation Index

This directory contains the durable libsixel documentation that users and contributors should be able to discover from the repository. The index is arranged in parts that follow a reader from an overview of the project, through its concepts and quality model, to the implementation and its maintenance. Each part groups related subject directories into chapters, so a chapter can collect documents from more than one directory.

## Introduction

What libsixel is, where it came from, and the SIXEL format it implements.

### Project

- [Functional overview](overview.md) describes the product capabilities, major components, data flow, and architectural boundaries.
- [Project history and lineage](project-history.md) explains how libsixel was derived from kmiya's `sixel`, how the project developed, and how community contributions shaped it.

### SIXEL format

- [SIXEL format overview](sixel-format/README.md) is a practical guide to the DEC SIXEL wire format, its control functions, rendering model, and interoperability limits.
- [SIXEL codec comparison](sixel-format/comparison/README.md) compares quality, encoding and decoding latency, 7-bit transport size, thread budgets, and repeated generations with JPEG, GIF, PNG, and WebP.

## Using libsixel

Supported platforms and command-line behavior that needs no background in color or encoding. Options that control encoding, such as quantization, dithering, lookup, and color spaces, are documented with their pipeline stage under Implementation, after the concepts they rely on.

### Platform support

- [Build, runtime, and platform support](platform-support.md) defines the build requirements, runtime assumptions, CI-backed support tiers, and the OS, architecture, compiler, and build-system matrix.

### Command-line basics

- [Command-line options at a glance](cli/options.md) lists every `img2sixel` and `sixel2png` option by purpose with a one-line summary, marks which options need background in color or encoding, and links each one to its detailed explanation.
- [Clipboard input and output](cli/clipboard.md) explains `-y`, pseudo targets, system/file backends, platform payloads, and headless workflows.
- [Choice prefix-matching policy](cli/prefix-matching.md) defines exact and unique-prefix resolution, ambiguity, CLI/environment vocabulary separation, and compatibility constraints.
- [CLI correction-suggestion policy](cli/correction-suggestions.md) defines ambiguous-prefix, fuzzy-name, and nearby-path guidance, including defaults, ranking, diagnostics, performance, and privacy boundaries.

## Concepts and theory

Color, pixel representation, and visual perception that the rest of the documentation builds on.

### Color and pixel representation

- [Color spaces and loader color management](concepts/colorspace/README.md) explains linear RGB/XYZ conversion hubs, ICC PCS and reference whites, linear-light mixing, and when implicit conversions occur, then connects loader output with the `-X`, `-W`, and `-U` encoder boundaries and shows how to inspect the effective path.
- [DIN99d and color difference](concepts/din99d/README.md) illustrates perceptual nearest-color selection, the corrected XYZ transform, common coordinate normalization, and measured palette tradeoffs.
- [Pixel-format precision](concepts/pixelformat-precision/README.md) explains the `RGB888` and typed float32 storage contracts, implicit promotion by resize, CMS, and color-space conversion, memory cost, and measured resize quality.
- [Pixel formats and alpha representation](concepts/pixelformat.md) separates input memory layouts, frame storage, alpha-zero metadata, and what can be represented in a SIXEL stream.

### Visual perception

- [Visual perception for image quality](quality/visual-perception.md) explains the sensory physiology behind the metrics: perceptual quality, photoreceptors and opponent channels, light and chromatic adaptation including display filters and dark rooms, contrast sensitivity, viewing geometry, and the visibility of noise and chroma detail.

## Quality and measurement

How output quality is defined, measured, and accepted.

### Metrics and color evaluation

- [Image-quality metrics](quality/metrics.md) defines each `lsqa` metric with its origin, mathematical definition, and implementation differences, and explains multiscale evaluation, dither noise spectra, structure-preserving dithering, how to combine metrics for dithered output, and the current emphasis on luminance.
- [Color difference and color reproduction](quality/color-difference.md) compares color-difference formulas, explains per-pixel, regional, global, and multiscale measures of color reproduction, why small palettes shift color, and which tolerance grades to target for output that does not look wrong.
- [Color-aware quality evaluation](quality/color-evaluation.md) defines independent input color interpretation, current-coordinate measurement, XYZ D50 color differences, fixed regional means, comparison signals, and the pending PNG integration boundary.

### Measurement practice

- [Quality measurement policy](quality/measurement-policy.md) defines perceptual metrics, fixtures, thresholds, baselines, and performance comparisons.
- [Palette gradient analysis](quality/palette-gradient-analysis.md) provides a local command to select a source-image gradient and inspect its quantized output through RGB scatter plots, exact Voronoi slices, actual pixel profiles and reusable source selections.

## Implementation

The encoder and decoder in data-flow order, from loading an image to writing SIXEL or PNG, followed by cross-cutting execution topics.

### Encoder architecture

- [Encoding pipeline](encoder/pipeline/README.md) connects loading, palette construction, palette application, SIXEL byte generation, and their composed cost model.
- [Encoder execution map](encoder/execution-map/README.md) expands the implementation path through loader fallback, preprocessing, scheduler overlap, quantizer-specific work, lookup and dither application, and palette serialization.
- [Encoder working precision](encoder/precision/README.md) explains the cross-cutting `--precision` axis, effective 8-bit and float32 paths, and the shared quality, speed, and size comparison across processing policies.

### Image loading

- [Image loader architecture](loader/README.md) explains the compiled loader registry, `-L` chain construction, predicate and fallback control flow, typed frame outputs, backend inventory, and quality/security tradeoffs.
- [Loader precision, gamut, and fast paths](loader/precision-policy.md): backend representation, color/alpha preservation, byte eligibility, and validation requirements.
- [Loader Color Management](loader/color-management/README.md) defines the backend integration policy, lists current CMS and precision boundaries by loader/format, and compares engines, unsupported profiles, fallback, rendering intents, quality, and speed/memory tradeoffs.
- [Builtin CMS specification and architecture](loader/builtin-cms.md) explains the implemented ICC subset, profile and transform ownership, numerical behavior, safety boundaries, and coverage limits.
- [Builtin image loader](loader/builtin.md) documents the in-tree format families, precision and colorspace goals, security model, and the extraction history from the original stb_image integration.
- [Builtin format components](loader/builtin/README.md) indexes one implementation reference per builtin format family, with exact variants, metadata and extension handling, output representation, history, and explicit exclusions.
- [Builtin PNG source precision](loader/builtin/png-source-precision.md) describes original samples, selected CMS execution, atomic image/background fallback, and independent numeric tests.
- [libpng image loader](loader/libpng.md) separates desired PNG/APNG behavior from current implementation gaps, then documents format support, precision, CMS, alpha/background handling, animation, and test coverage.
- [libtiff format and color-management support](loader/libtiff.md#format-and-color-management-support) tabulates implemented decode and CMS paths, original Lab/CMYK integration, precision limits, and missing or unverified TIFF variants.
- [PNG color metadata and ColorSync compatibility](loader/png-color-metadata.md): shared grayscale/indexed/RGB reference matrix and source-selection policy.
- [Alpha policy](loader/alpha-policy/README.md) defines loader-side background composition, alpha-zero preservation, SIXEL `P2` requests, terminal-specific rendering, and the loader/encoder ownership boundary.
- [Background policy](loader/background-policy.md) defines how builtin PNG, APNG, and GIF loaders choose between file and external backgrounds, including option precedence, colorspace, and OSC 11 interaction.
- [Network access](loader/network-access.md) explains img2sixel's libcurl, libfetch, WinHTTP, and Emscripten bindings, build-dependent URL schemes and TLS support, backend selection, and the behavior and risks of `-k` certificate-verification bypass.
- Codec adapters: [libjpeg](loader/libjpeg.md), [libwebp](loader/libwebp.md), [libtiff](loader/libtiff.md), and [librsvg](loader/librsvg.md) describe accepted routes, precision, metadata, animation/rasterization boundaries, and backend-specific controls.
- Framework adapters: [WIC](loader/wic.md), [CoreGraphics/ImageIO](loader/coregraphics.md), [GdkPixbuf](loader/gdk-pixbuf2.md), and [GD](loader/gd.md) distinguish host/library capabilities from libsixel integration and fallback behavior.
- Preview adapters: [Quick Look](loader/quicklook.md) and [Freedesktop thumbnailers](loader/gnome-thumbnailer.md) explain provider discovery, raster resolution, external execution, and metadata boundaries.
- [CoreGraphics PNG source precision](loader/coregraphics.md#png-source-precision): original samples, native color definitions, alpha, and failure boundaries.

### Preprocessing

- [Crop and resize](encoder/preprocessing/crop-resize/README.md) defines geometry syntax, crop/resize ordering, resize colorspace and precision policy, its SIMD relationship, measured quality, speed, size, transparency, and public frame helpers.
- [Resampling](encoder/preprocessing/resampling/README.md) details resize coordinate mapping, discrete response, moiré, edge normalization, separable passes, threading, SIMD dispatch, visual comparisons, and measured quality, speed, and size. Its [theory and history](encoder/preprocessing/resampling/theory-history.md), [compact methods](encoder/preprocessing/resampling/compact-kernels.md), and [wide-support methods](encoder/preprocessing/resampling/wide-kernels.md) chapters derive the kernels and document each selectable method individually.

### Palette construction

- [Palette construction pipeline](encoder/palette-construction/pipeline/README.md) defines the sampling, palette-space, binning, and quantization boundaries, joint automatic policy resolution, typed artifacts, and migration waves.
- [Palette quantization](encoder/palette-construction/quantization/README.md) explains `-Q`, palette solver objectives, asymptotic costs, and the boundary between palette generation and use.
- [Palette clustering color space](encoder/palette-construction/clustering-colorspace/README.md) explains `-X`, its coordinate geometry and normalization, option interactions, and measured quality, speed, and size tradeoffs.
- [Final palette merge policy](encoder/palette-construction/merge-policy/README.md) explains `-F`, Ward oversplit-and-reduce processing, optional Lloyd polishing, measured quality, speed, size, palette geometry, and flat-patch reachability.
- [Palette cover policy](encoder/palette-construction/cover-policy/README.md) explains `-a` / `--cover-policy`, post-quantizer gamut reachability repair, soft and hard anchor selection, the links among gamut mapping, Neugebauer printing, and error-diffusion stability, and a three-layer measurement model.
- [Palette snap policy](encoder/palette-construction/snap-policy/README.md) explains how `-_` / `--snap-policy` enables and configures the 101 reversible SIXEL tone levels, working-space target selection, timing, and approach rate.
- [Built-in palettes and monochrome output](encoder/palette-construction/fixed-palettes.md) explains `-b`, `-e`, `-i`, palette families, foreground-mask output, construction bypass, and downstream controls.
- [External palette input and output](encoder/palette-construction/external-palettes.md) explains `-m` and `-M`, palette-generation and reuse workflows, format origins, application compatibility, format detection, and fixed-palette policy interactions.

### Palette application

- [Palette working color space](encoder/palette-application/working-colorspace/README.md) explains `-W`, its palette-application geometry, lookup and dither interactions, precision requirements, and measured quality, speed, and size tradeoffs.
- [Lookup policy](encoder/palette-application/lookup-policy/README.md) defines the `-~` / `--lookup-policy` interface provided by `img2sixel`, its mathematical basis, exactness guarantees, and preparation/query costs. It contains one detailed chapter per policy and a measured Delta E, chroma, MS-SSIM, and runtime comparison across palette sizes.
- [Dithering](encoder/palette-application/dithering/README.md) explains `-d`, error shaping, scan order, temporal behavior, per-pixel cost, and its interaction with lookup. The [FS coefficient perturbation measurements](encoder/palette-application/dithering/perturb/README.md) document the optional texture adjustment with RAPSD, flat-field and photographic comparisons, strength and seed controls, and reproducible measurement conditions.
- [Dither clamp policy](encoder/palette-application/clamp-policy.md) defines `-0` / `--clamp-policy`, extended-source preservation, signed feedback headroom, and quality tradeoffs.

### SIXEL output

- [Encoding policy](encoder/output/encode-policy/README.md) explains `-E`, six-row mask serialization, size-policy overpainting, and measured quality, speed, and stream-size behavior.
- [Output color space](encoder/output/output-colorspace.md) explains `-U`, gamma/linear/SMPTE-C output, the selected CPU float Display P3 path, receiver interpretation, percentage rounding, fixed-palette and `-M` interactions, the high-color gap, and test coverage.
- [Delta encoding (6delta)](encoder/output/delta-encoding.md) explains `--update-policy`, retained display colors, threshold and palette comparisons, partial-update APIs, invalidation, CPU/GPU scope, and test coverage.
- [OR-mode output](encoder/output/or-mode/README.md) explains `-O`, index-bit composition, `P2=5`, palette-zero and transparency semantics, receiver compatibility, and regression coverage. Its [scaling and decoding study](encoder/output/or-mode/scaling.md) compares palette budgets, encoder/decoder thread budgets, and in-process versus PNG-inclusive decoding.
- [High-color output](encoder/output/high-color/README.md) explains `-I`, paint-time palette-register semantics, 15bit keys versus actual RGB counts, current parallelism limits, and the required band reset design. Its [measurement reference](encoder/output/high-color/measurement.md) compares 256-color output, artifacts and banding, histograms, quality, API speed, and stream size.
- [DRCS-SIXEL output](encoder/output/drcs.md) explains `-@` / `--drcs`, separate glyph-definition and display-character streams, mapping revisions, cell geometry, receiver requirements, reuse, and current implementation defects.
- [Terminal macros](encoder/output/terminal-macros.md) explains `-u` and `-n`, DECDMAC definitions and DECINVM playback, preloading, transfer cost, receiver state, and exact static-image and animation regression coverage.

### Decoding

- [Decoding pipeline](decoder/pipeline/README.md) follows SIXEL wire bytes through parser and paint state, raster policy, indexed and direct representations, optional reconstruction and resize, parallel fallback, and PNG output, with a reciprocal coverage audit.
- [Dequantization](decoder/dequantization/README.md) explains Kornelski's palette and gradient inference, libsixel's mixing ratios, reconstruction controls, alpha and palette-history boundaries, CPU/GPU execution, and test coverage.

### Image writing

- [PNG writer](writers/png/README.md) defines the libpng and builtin writer backends, indexed and direct 8-bit output, and the post-SIXEL PNG snapshot path used by `img2sixel`.

### Animation, threading, and runtime execution

- [Animation playback](encoder/animation.md) explains `-l`, `-S`, `-T`, and `-g`, loader/format support, GdkPixbuf loop-count limitations, timing and composition, and interactions with macros, alpha, delta output, and temporal dithering.
- [Threading documentation](threading/README.md) indexes the execution model, band encoding and decoding, encoder and decoder budgets, animation, timeline interpretation, and reproducible measurements.
- [Threading overview](threading/overview.md) defines SIXEL bands, palette-application work bands, decoder byte spans, ordered publication, and the small quality differences that work-band seam policy can introduce.
- [Runtime execution policy](cli/runtime-policy.md) explains `-j`, SIMD ceilings, scheduling thresholds, resize precision, process scope, and encoder/decoder differences.

### Diagnostics

- [Diagnostics and logs](cli/diagnostics.md) explains `-x`, `-J`, `-v`, code reporting, trace topics, and timeline capture and interpretation.
- [CLI abort trace diagnostics](cli/abort-trace/README.md) defines the CLI-layer ownership, current converter consumers, non-consuming `lsqa` boundary, controls, platform behavior, representative output, reliability, performance, output-quality boundary, and test coverage of the `SIGABRT` stack trace.

## Terminal protocol proposals

Receiver-independent proposals for terminal behavior beyond SIXEL. They assume the color concepts and output behavior described above.

### Color management

- [Terminal graphics color management proposal](protocols/graphics-color-management.md) defines receiver-independent color policy, device-input RGB discovery, simple RGB and ICC descriptions, filter-status queries, and the proposed application negotiation flow.

## Development and maintenance

Building, testing, and maintaining libsixel and its documentation.

### Build system

- [Build system architecture](build/README.md) explains why Autotools + Libtool and Meson remain complementary, why generated build files are committed for repository users, configure and build orchestration, `m4/` macros, `build-aux/` toolchain adapters, the custom test harness and command-length limits, and Meson adoption and remaining differences.

### Testing

- [Testing guide](testing/guide.md) owns general assertion and evidence rules, test organization, shell TAP rules, registration, portability, and required validation.
- [Staticcheck](testing/staticcheck.md) defines libsixel's repository-wide invariant-verification target, its broader-than-lint scope, IDL and cross-surface checks, test meta-checks, CLI synchronization role, and extension workflow.
- [Builtin loader coverage inventory](testing/builtin-loader-coverage.md) maps every builtin-loader shell test to its primary format, observation, and traceability role while keeping pipeline landmarks distinct from the exhaustive regression suite.
- [Mapfile parser coverage inventory](testing/mapfile-parser-coverage.md) maps every structured-palette parser and writer test to its exact observation while keeping behavioral contracts distinct from defensive and supplementary coverage.
- [Resampling regression coverage](testing/resampling-coverage.md) inventories exact precision, SIMD, thread, discrete-stencil, frequency, isotropy, and moiré regression probes and their numeric tolerance table.
- [libpng cHRM boundary test plan](testing/png-chrm-boundaries.md) inventories integer-grid, exact-collinear fallback, signed-primary acceptance, original-sample, background, and CMS control observations.
- [PNG cHRM arithmetic test plan](testing/png-chrm-arithmetic.md) separates mode selection, exact rounding, ordinary double budgets, target definition, white adaptation, and terminal availability errors.

### Continuous integration

- [CI architecture and design](ci/design.md) describes GitHub Actions, @saitoha's local desktop CI, matrix design, and failure triage.

### Command-line implementation policy

- [CLI design policy](cli/design-policy.md) defines option design, compatibility, parsing, diagnostics, and documentation synchronization.
- [Converter suboption architecture](cli/suboptions.md) defines structured option grammar, typed registry metadata, consumer scopes, environment-variable parity, precedence, and implementation flow.

### Platform compatibility

- [Platform compatibility ledger](misc/platforms/README.md) indexes the maintained OpenVMS, Windows runtime, Emscripten, Cosmopolitan, macOS, POSIX libc, Haiku, and Solaris compatibility contracts and their reciprocal static checks.

### Documentation

- [Documentation instructions](AGENTS.md) define document scope, organization, policy ownership, reference-based reuse, and document/test traceability.

### Building the HTML site

Run `tools/build-docs` from any directory to build the complete searchable HTML site under `site/`. The command creates and caches a private virtual environment under `.docs-build/`; MkDocs and Material for MkDocs remain optional and are not required for configuring, building, testing, or installing libsixel itself. The first documentation build requires network access to install the versions pinned in `tools/docs/requirements.txt`; subsequent builds reuse the cached environment.

An Autotools build provides the equivalent `make docs` target, and a Meson build provides `meson compile -C BUILDDIR docs`. Both write the generated site to `site/` under their build directory. Pass `--no-bootstrap` to `tools/build-docs` only when the current Python environment already contains the pinned documentation packages.
