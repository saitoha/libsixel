# Image Loader Architecture

For engine selection and its limits, read [Loader CMS: builtin and Little CMS](color-management.md). It distinguishes decoder support from CMS support and covers unsupported profiles, best-effort fallback, quality, and performance. For the subsystem design, read [Builtin CMS specification and architecture](builtin-cms.md).

## Scope

Image loading in libsixel is an ordered component pipeline rather than one decoder hidden behind a uniform RGBA buffer. The loader manager builds a candidate chain, gives each candidate an opportunity to recognize and decode the input, and returns the first accepted frame. Each backend owns format recognition, decoding, metadata precedence, animation delivery, orientation, alpha finalization, and any CMS path that it supports.

This document defines that shared architecture and the `-L` policy surface. Backend-specific parsing and format behavior belongs in a separate document, beginning with the [Builtin Image Loader](builtin.md).

## Loader-chain mental model

<picture>
  <source media="(max-width: 640px)" srcset="loader-architecture-figures/loader-chain-mobile.svg">
  <img alt="The loader order parser puts explicit -L entries first, removes duplicates, and unless a final exclamation mark is present appends compiled default-enabled loaders. At load time each candidate is checked by its format predicate, then decodes and performs its own metadata, color-management, orientation, animation, and alpha work. A successful candidate returns a typed frame; an allowed mismatch or decode status advances to the next candidate; allocation, cancellation, argument, overflow, and other terminal errors stop the chain." src="loader-architecture-figures/loader-chain-wide.svg">
</picture>

*Figure 1. Chain construction and candidate selection. Blue is policy and control flow, purple is backend-owned work, teal is the accepted frame contract, and red is a rejected candidate or terminal error. Labels and line shape duplicate the color encoding.*

The default chain is the compiled registry order, not a fixed list shared by every binary. Optional libraries and operating-system frameworks add components at configure time. `builtin` is always registered and default-enabled. `gnome-thumbnailer`, when compiled, is selectable but is deliberately not appended to the default chain.

`img2sixel --help` prints the loaders available in the current binary. That output is more authoritative for a particular installation than a list copied from another build.

## Building the chain with `-L`

`-L LIST` and `--loaders=LIST` accept a comma-separated ordered list. Names are case-insensitive and may be abbreviated only when the prefix is unique among the loaders compiled into the current binary. Repeating a loader does not create a second attempt; the plan keeps its first occurrence.

Explicit entries are placed first. Unless the complete list ends with `!`, the manager then appends every remaining default-enabled compiled loader in registry order. The final `!` therefore means “use exactly this deduplicated list”, not “make the selected decoder strict internally”.

```console
# Prefer libpng, then try the remaining default-enabled loaders.
img2sixel -Llibpng input.png

# Try only libpng and builtin, in that order.
img2sixel -Llibpng,builtin! input.png

# Use only builtin and request a float32 linear CMS result when a transform runs.
img2sixel -Lbuiltin:cms_engine=builtin:cms_target=linear:prefer_8bit=0! input.png
```

Suboptions after `:` are resolved before components are created. Environment defaults are applied first and explicit `-L` assignments override them. Common suboptions include background policy and colorspace, CMS target and preferred output precision, rendering intents, HDR policy, OSC 11 background queries, thumbnail sizing, and PNG transparent-key handling. A backend may add narrower options such as `libwebp:max_output_frames`, `librsvg:relative_resources`, or builtin PNM/BMP compatibility controls. The canonical grammar and complete current vocabulary are generated into [`img2sixel --help`](../../converters/img2sixel.c) and the [`img2sixel(1)` manual](../../converters/img2sixel.1).

`cms_intent=INTENT[+INTENT][!]` has an inner `!` that disables fallback *rendering intents*. If that suboption is last and the loader chain must also be closed, a second `!` is required. The manual owns the exact escaping rule because it is part of the CLI grammar rather than decoder behavior.

## Candidate selection and fallback

For each component in the chain, the manager normally asks its predicate whether the byte prefix looks applicable. A false predicate skips that component without invoking its decoder. Broad framework loaders and the builtin component may intentionally use permissive predicates and do finer dispatch after entry.

When a candidate is invoked, three outcomes matter:

| Outcome | Manager action |
| --- | --- |
| `SIXEL_OK` | Accept the component, return its frames, and stop the chain. |
| Recognized mismatch or decoder-family error | Reject only this candidate and continue to the next component. |
| Successful non-`SIXEL_OK` status, or terminal failure | Return immediately without trying another decoder. |

The fallback set is explicit in `sixel_loader_manager_status_allows_fallback()`. It includes `SIXEL_FALSE`, `SIXEL_BAD_INPUT`, and the JPEG, PNG, WebP, TIFF, GDK, GD, stb, COM, and WIC decoder-family errors. Allocation failure, invalid arguments, cancellation, integer overflow, and unrelated runtime or logic failures are terminal. This distinction prevents a resource failure or caller cancellation from being misreported as “perhaps another decoder understands the file”.

A longer chain can expose the same untrusted byte stream to more than one decoder. Closing a chain with `!` narrows that set, but does not sandbox, validate, or otherwise make the chosen backend a security boundary.

Set `SIXEL_TRACE_TOPIC=loader` when diagnosing which candidates were attempted and which status selected or rejected each backend.

## Compiled loader inventory

The registry order below is also the automatic priority order for components present in a build. “Output character” is intentionally representative: source depth, alpha, palette preservation, animation, CMS settings, and backend capabilities can change the concrete frame.

| Loader name | Availability | Primary role | Representative output character |
| --- | --- | --- | --- |
| `libpng` | With libpng | PNG and APNG | Indexed or RGB/RGBA; 16-bit and CMS paths can produce float32. |
| `libjpeg` | With libjpeg | JPEG, including library-supported high-depth variants | RGB float32 on precision-preserving paths; CMS may produce a typed target colorspace. |
| `libwebp` | With libwebp | Static and animated WebP | RGB/RGBA or indexed animation frames; float32 after supported color management. |
| `libtiff` | With libtiff | TIFF | RGB/RGBA or float32, including source- or CMS-derived typed colorspaces. |
| `librsvg` | With librsvg | SVG and permitted SVGZ/resource modes | Rasterized gamma RGB or RGBA frames. |
| [`builtin`](builtin.md) | Always | In-tree SIXEL, PNM/PAM, GIF, WebP, PNG/APNG, JPEG, HDR, PSD, BMP, TGA, and PIC paths | Indexed, byte RGB/RGBA, generic float32, or typed float32 depending on format and policy. |
| `wic` | Windows Imaging Component build | Windows codecs and containers, including ICO selection | Framework-decoded RGBA frames followed by common normalization. |
| `coregraphics` | CoreGraphics/ImageIO build | Apple platform image sources | Indexed, byte RGB, or linear float32 according to source and conversion path. |
| `gdk-pixbuf2` | With GdkPixbuf | Formats supplied by installed GdkPixbuf modules | RGB or linear float32 frames after loader normalization. |
| `gd` | With GD | The formats enabled in the linked GD library | Indexed, RGB, or linear float32 according to source and conversion path. |
| `quicklook` | CoreGraphics plus Quick Look | Apple preview providers for otherwise unsupported documents | Rasterized gamma RGB/RGBA preview frames. |
| `gnome-thumbnailer` | Freedesktop thumbnailing build; explicit only | Desktop thumbnailer definitions and external thumbnailer commands | The returned PNG is decoded through builtin, so its final type follows that path. |

Framework and plugin loaders inherit part of their effective format support from the host. A build-time name therefore identifies a component, not a permanent promise that every installation recognizes the same extensions.

## The output is a typed frame, not universal RGBA

The stable handoff records dimensions, storage kind, `pixelformat`, `colorspace`, animation timing, palette metadata, a possible transparent palette index, alpha-zero interpretation, and an optional transparency mask. Common representations include:

- indexed pixels plus a palette and optional transparent index;
- three-component byte pixels such as `RGB888` plus separate transparency metadata;
- three-component float pixels such as `RGBFLOAT32`, `LINEARRGBFLOAT32`, `CIELABFLOAT32`, `OKLABFLOAT32`, or `DIN99DFLOAT32` plus separate transparency metadata;
- an alpha-bearing packed layout such as `RGBA8888` while a decode or normalization path still needs explicit component alpha.

Consequently, “loaded frame (RGBA)” and “get pixel (RGBA)” are incorrect general contracts. Consumers must inspect the frame metadata instead of deriving component count, precision, or color interpretation from the loader name.

Loader output is also not controlled solely by the encoder's `--precision` option. A decoder can preserve high-depth source values as float32 before encoder preprocessing begins. Conversely, a requested float-capable CMS target does not manufacture extra source precision when no transform runs. With CMS enabled, `prefer_8bit=0` permits float32 target output; `prefer_8bit=1` requests an 8-bit gamma RGB boundary. See [Pixel-format Precision](../concepts/pixelformat-precision.md) and [Color Spaces and Loader Color Management](../concepts/colorspace.md) for the later resize and encoder conversions.

## Why several loaders exist

Optional mature libraries and platform frameworks provide broad compatibility, native integration, and their own maintenance and security programs. The builtin loader provides a dependency-free baseline, but dependency removal is only one of its design goals. Keeping selected decode paths in-tree also lets libsixel preserve source precision, expose explicit colorspace interpretation, retain indexed structure, control animation and alpha semantics, and delay lossy conversion until the encoder actually needs it. These capabilities can improve the final palette and resize quality.

Those benefits do not make builtin inherently safer. Every in-tree parser, decompressor, metadata interpreter, and animation state machine increases the amount of libsixel code that processes attacker-controlled bytes. It adds memory-safety, denial-of-service, integer-overflow, and semantic-confusion risk, as well as an ongoing review and fuzzing burden. External decoders have attack surfaces too, and may introduce dynamic dependencies or host-provided plugins. Loader choice is therefore a compatibility, quality, operational, and security policy decision rather than a linear “builtin good” or “external good” ranking.

## Backend documentation boundaries

The shared manager contract stays here. Each detailed backend document should separately specify its recognized formats, depth and colorspace behavior, metadata precedence, animation model, alpha representation, suboptions, fallback statuses, host dependencies, security considerations, and implementation/test landmarks. The current detailed builtin references are:

- [Builtin Image Loader](builtin.md), including its stb_image lineage and format-specific extraction history.
- [Builtin Format Components](builtin/README.md), indexing the exact accepted variants, metadata semantics, output precision/colorspace, history, and unsupported features of each builtin decoder family.

Cross-backend alpha and background behavior is defined by [Alpha Policy](alpha-policy.md) and [Background Policy](background-policy.md).

## Implementation landmarks

| Concern | Primary symbols and files |
| --- | --- |
| Registry order, deduplication, default append, candidate loop, and fallback set | `g_sixel_loader_entries`, `loader_manager_build_plan_from_resolution`, and `sixel_loader_manager_load_impl` in [`loader-manager.c`](../../src/loader-manager.c) |
| `-L` parsing and suboption schema | [`loader-order-schema.c`](../../src/loader-order-schema.c) and the loader rows in [`options-registry.c`](../../src/options-registry.c) |
| Manager construction from the public loader helper | `sixel_helper_load_image_file` in [`loader.c`](../../src/loader.c) |
| Component interface and frame callback | `loader_component` and `loader_manager` in [`6cells.idl`](../../include/6cells.idl), plus [`frame-private.h`](../../src/frame-private.h) |
| Loader-order structural fuzzing | [`fuzz-loader-builtin-struct-loader-order-libfuzzer.c`](../../fuzz/fuzz-loader-builtin-struct-loader-order-libfuzzer.c) |
