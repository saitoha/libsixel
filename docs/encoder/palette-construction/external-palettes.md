# External Palette Input and Output

## Purpose

For the compiled palette families and the distinct `-e`/`-i` monochrome output mode, see [built-in palettes and monochrome output](fixed-palettes.md).

`img2sixel` normally constructs a palette from each input image. The external-palette options separate palette design from palette application:

```text
ordinary path:  image -> palette construction -> palette application -> SIXEL
export path:    image -> palette construction -> -M palette file
reuse path:     -m palette file ---------------> palette application -> SIXEL
```

`-m FILE` supplies a completed palette, while `-M FILE` exports the palette prepared by the encoder. This separation is useful when palette quality, consistency, reproducibility, or compatibility matters more than choosing a new palette independently for every image.

Typical uses include:

- constructing a high-quality palette once from a representative image or contact sheet, then reusing it across a related image set;
- editing a generated palette by hand in GIMP, Krita, PaintShop Pro, or another palette editor before applying it again;
- keeping palette indices and colors stable across animation frames, slides, icons, or repeated terminal updates, which can reduce distracting color changes even when a per-frame palette would score slightly better in isolation;
- constraining output to the colors of a target machine, terminal, theme, brand, game, or artwork pipeline;
- separating an expensive offline palette search from a latency-sensitive conversion path; and
- comparing lookup and dithering policies against exactly the same palette.

A palette optimized for one image is not automatically high quality for a collection. Build a reusable palette from representative material, then validate the actual images with [`lsqa`](../../quality/measurement-policy.md) or another suitable quality check. Palette reuse removes palette-construction variability; it does not guarantee that the fixed colors cover every later image well.

## Importing a palette with `-m`

The basic form is:

```sh
img2sixel -m palette.gpl -o output.six input.png
```

The palette is fixed before the input pixels are mapped. Palette-construction controls such as the quantizer do not rebuild it, but palette-application controls still matter. In particular, an explicit [`--lookup-policy`](../palette-application/lookup-policy/README.md), the [`--diffusion`](../palette-application/dithering/README.md) method, and the [`-W` working color space](../palette-application/working-colorspace/README.md) apply when the image is mapped to the supplied colors.

### Image mapfiles

`-m` also accepts an image as the palette source:

```sh
img2sixel -m hardware-palette.png -o output.six input.png
```

PNG is a common choice, but this path is not specific to PNG. A filename that does not select ACT, PAL, or GPL is passed to the configured image loaders, so JPEG, GIF, BMP, PNM, and other enabled input formats can also serve as map images. The image is loaded as a single static palette source; for an animated image, only its first frame contributes colors.

An image mapfile describes colors through its decoded pixels. libsixel converts those pixels to the selected working color space and constructs a palette of at most 256 colors from them. An indexed PNG is therefore convenient for a swatch image, but libsixel does not import its palette table as an ordered file-format palette: unused entries can disappear, duplicate entries can collapse, and entry indices are not preserved. If an image contains more than 256 colors, its pixel distribution also influences the reduction. Selected float image-map jobs honor the construction controls described under [ordinary sRGB output](#selected-ordinary-srgb-output); byte-only jobs retain the legacy reconstruction path. Use ACT, PAL, or GPL when exact entry count and ordering are part of the interchange contract.

Image mapfiles are input-only. `-M` does not write PNG or another map image; it writes ACT, JASC PAL, RIFF PAL, or GPL. The two options can still be combined to inspect or translate the palette constructed from an image:

```sh
img2sixel -m hardware-palette.png -M gpl:hardware-palette.gpl \
    -o /dev/null input.png
```

### Format selection

The explicit `TYPE:PATH` spelling avoids extension ambiguity:

```sh
img2sixel -m act:palette.act input.png
img2sixel -m pal-jasc:palette.psp input.png
img2sixel -m pal-riff:palette.riff input.png
img2sixel -m gpl:palette.gpl input.png
```

The accepted type prefixes are `act:`, `pal:`, `pal-jasc:`, `pal-riff:`, and `gpl:`. `pal:` detects JASC PAL versus RIFF PAL from the contents.

Without a prefix, `.act`, `.pal`, and `.gpl` select palette parsing. A `.pal` file is then distinguished by its JASC or RIFF signature. A path with another extension is treated as an image. An extensionless path is content-sniffed as a palette, but a raw ACT payload is deliberately rejected as ambiguous; use `act:PATH`. These rules mean that conventional `.riff` and `.psp` filenames require `pal-riff:` and `pal-jasc:` respectively even though other applications recognize those extensions.

Standard input also requires an explicit type because there is no filename:

```sh
generate-palette | img2sixel -m gpl:- -o output.six input.png
```

Repeating `-m` is allowed and the last mapfile wins. `-m` is mutually exclusive with `-b`, `-e`, and `-I` because each selects a different palette or output mode.

Native sRGB/gamma [delta output](../output/delta-encoding.md#fixed-table-omission-and-ownership) preserves every supplied entry, including full 256-entry tables, and carries omissions separately from palette indices. Image-map construction does not reserve a color for the image being displayed; its palette is independent of the retained display history. The delta guide owns the applicable workspaces, output formats and mask lifecycle.

## Display P3 output

The [selected CPU float Display P3 path](../output/output-colorspace.md) accepts `-m`. ACT, JASC PAL, RIFF PAL and GPL retain their sRGB interpretation. The encoder converts a separate copy to P3 and rounds it to SIXEL RGB101 values. Lookup and error diffusion retain the unrounded working coordinates. Palette order, duplicates and unused entries are retained. All five workspaces keep an authoritative float palette; ordinary byte-eligible sRGB jobs keep their existing fast path.

Existing image mapfiles keep their image-to-palette meaning. In the P3 path, a map image goes through its selected loader/CMS and the same extended float builder used for generated palettes. Its colors remain unrounded until the fixed-palette output boundary, so CMS-retained values outside sRGB survive all five working coordinate choices. This does not introduce an ordered PNG palette format or a PNG `-M` writer.

The other selected-path restrictions still apply, including the supported lookup policies, [CPU spatial/positional dithering or none](../output/output-colorspace.md#display-p3-selected-cpu-float-path), and static RGB source images, optionally with a complete [separate alpha-zero mask](../output/output-colorspace.md#separate-alpha-zero-masks), including [normalized packed alpha input](../output/output-colorspace.md#packed-alpha-normalization). Explicit cover-policy options conflict with fixed palettes, so omit `-a` (including `-aoff`); fixed palettes bypass cover. For example:

```sh
img2sixel --threading-policy=greedy:global_budget=1 --gpu-policy=off --precision-policy=auto \
  -Qkmeans:seed=1 -Xlinear -Wlinear -Fnone -_none '-~none' \
  --sampling-policy=full-frame --binning-policy=none \
  -d fs -t rgb -U display-p3 -m colors.pal input.png
```

## Selected ordinary sRGB output

The same [selected CPU float path](../output/output-colorspace.md#selected-ordinary-srgb-float-path) accepts `-m` with `-Ugamma`, `-Ulinear` or `-Usmpte-c` and AUTO or explicit Heckbert, K-means, K-center or K-medoids, supported lookup and spatial/positional dithering, none or Ward merge, and disabled snap (including zero approach rate). Cover is bypassed for fixed palettes; omit `-a`. Selection requires that the source, a loaded image map, or encoder processing needs float precision. An image map is an independent precision source: its loader representation and the required `-X` clustering precision are inspected before workspace conversion. Non-gamma clustering requires an authoritative float palette even when both loaded images are bytes and `-Wgamma` is selected. Structured palettes have no clustering stage, so an unused non-gamma `-X` does not promote them. Mapfile selection alone does not promote a job when both inputs and the requested operations are byte-eligible. Packed source alpha follows the [shared normalization boundary](../output/output-colorspace.md#packed-alpha-normalization). Active snap and the other selected-path exclusions retain their existing ordinary-sRGB behavior; fixed-palette animation follows the cache contract below.

Structured ACT, JASC PAL, RIFF PAL and GPL entries remain sRGB8 and retain their order, duplicates and unused entries. For selected float application, import creates an authoritative float working palette even in gamma. Lookup and error diffusion use those unrounded coordinates; only a separate RGB101 output copy is rounded. The supplied entries are not snapped implicitly. `-M` captures the sRGB8 export before wire rounding. Byte-fast-path jobs keep the original allocation and representation behavior.

For a selected float image-map job, the callback preserves loader/CMS precision before conversion to `-W`, then reuses the native palette builder with the selected solver. This extends the P3 image-map behavior to ordinary sRGB output; it does not turn the image into an ordered palette format. The builder uses the configured clustering/binning/merge controls on the loaded map frame, unlike the legacy reconstruction path. Values outside sRGB remain available to palette application in all five working spaces; the final sRGB wire/export boundary necessarily clips them. When only the map requires float, source loading and the initial image plan can remain RGB888; palette application promotes its work buffer to use the authoritative native palette. The base precision request stays unchanged. If that required buffer cannot be allocated, encoding fails instead of falling back to byte lookup or returning unfinished indices. An enabled CMS engine alone does not force float: untagged byte maps, disabled CMS and an explicit post-CMS `prefer_8bit=1` result remain byte-eligible under the existing loader contract when no subsequent operation requires float precision.

Image-map AUTO selection follows the common binning-aware resolver: omitted/auto/none binning selects Heckbert, while exact/hard/soft selects K-means. Explicit solvers keep their own supported binning combinations; an unsupported image-map combination fails before output. The requested quantizer remains unchanged and resolution occurs afresh on each input. Each static input, including a direct `sixel_encoder_encode_frame()` call on a reused encoder, reloads its map under the current loader/precision settings. Animation uses the [independent fixed-source cache](../output/output-colorspace.md#fixed-palettes-in-animation); the first constructed palette remains fixed throughout that animation. Structured palettes execute no quantizer or binning stage, including when those construction settings would be incompatible for an image map. Model-specific selected-path restrictions still apply at admission, as described in [output color space](../output/output-colorspace.md).

## Exporting a palette with `-M`

The basic form is:

```sh
img2sixel -p 64 --quantize-model=kmeans:seed=1 -M palette.gpl -o output.six input.png
```

ACT, JASC PAL, RIFF PAL and GPL entries are always interpreted as sRGB8. Import converts those entries into `-W` before palette application; it preserves a float view for non-gamma working coordinates. Gamma byte jobs need no such conversion. These formats do not inherit an interpretation from `-W`, `-U`, a terminal or the source image.

`-M` converts the prepared working palette to sRGB8, independently of the [`-U` wire output definition](../output/output-colorspace.md). In the selected RGB101 paths it saves that palette before destination projection and rounding; other paths export the retained working palette after palette application. The final wire snap is not applied to the export. Export therefore need not equal the colors sent to SIXEL, and does not promise reversible RGB101 entries. Palette-construction policies still affect which colors are prepared.

The selected Display P3 path supports `-M` with these legacy formats, but sRGB8 export necessarily clips colors outside sRGB and rounds to eight bits. It cannot preserve a wide-gamut palette. A self-contained color-bearing palette representation is still required for lossless interpretation of wider gamuts; an external override does not change the meaning of these legacy formats.

The output format is selected by a prefix or by `.act`, `.pal`, or `.gpl`. A plain `.pal` output name means JASC text; use `pal-riff:PATH` for RIFF PAL. Prefix matching is case-insensitive, and an explicit prefix takes precedence over a mismatched filename extension. An empty value, a prefix without a path, an unknown extension without a prefix, and a file that cannot be opened are errors. Standard output requires a type prefix, and the SIXEL stream must be directed elsewhere if both would otherwise use standard output:

```sh
img2sixel -M gpl:- -o output.six input.png > palette.gpl
```

Repeating `-M` is allowed and the last palette-output destination wins. Export retains only the sRGB8 palette entries and writes them directly; it does not allocate or duplicate an indexed image. Its retained storage is bounded by 256 RGB entries (768 bytes), independent of image dimensions. Reusing an encoder with a smaller palette writes only the current entry count; different encoder objects own independent export buffers.

Combining `-m` and `-M` can translate the prepared RGB entries into another palette format:

```sh
img2sixel -m pal-riff:legacy.riff -M gpl:editable.gpl \
    -o /dev/null input.png
```

This is not a metadata-preserving file converter. The command still performs an encode, converts palette colors through the working representation and back to sRGB8 for export, and recreates format-specific metadata; entry names, RIFF flags, and ACT transparency information are not carried through.

All four palette formats carry at most 256 8-bit RGB entries and no embedded ICC profile. They cannot preserve arbitrary float working-space coordinates, alpha semantics, rendering intent, or a source profile. There is no option to reinterpret their entries as device RGB; use a color-bearing format for that purpose once supported. The exported entry count is the finalized palette count and can be smaller than the requested `-p` limit after palette optimization.

### A quality-tuning and reuse workflow

Construct a deterministic palette from representative content and export it in an editable form:

```sh
img2sixel -p 64 --sampling-policy=full-frame \
    --quantize-model=kmeans:seed=1 --cover-policy=off \
    -M gpl:series.gpl -o representative.six representative.png
```

Inspect or edit `series.gpl`, then reuse it while choosing the lookup and diffusion behavior independently:

```sh
img2sixel -m gpl:series.gpl --lookup-policy=certlut \
    --diffusion=fs -o frame-001.six frame-001.png
```

For a sequence, reuse the same file for every frame or asset. If the palette is tuned outside libsixel, keep an unmodified master and record the `-W`, `-U`, lookup, and diffusion choices used for the final conversion; these choices can change the rendered result without changing the stored palette entries.

## Supported palette-file formats

The formats below are the structured palette files accepted by `-m` and written by `-M`; image mapfiles are described separately above. These formats share an 8-bit RGB model but come from different application ecosystems. “Compatible applications” below means that the named application's current or historical documentation describes importing or using the format; it does not imply preservation of names, flags, transparency, profiles, or every vendor extension.

| Format | Origin and overview | libsixel selector and filename behavior | Common application compatibility |
| --- | --- | --- | --- |
| Adobe Color Table (ACT) | Photoshop color-table format. A binary file contains 256 interleaved RGB triplets; the 768-byte form has no trailer, while the 772-byte form adds a color count and a transparency index. | `act:PATH` or `.act`. `-m` imports the counted colors from entry zero and ignores transparency metadata because source-image alpha is a separate policy. `-M` emits 772 bytes, pads unused RGB slots with zero, records the entry count, and writes a zero transparency index. | Adobe Photoshop color tables; GIMP and Krita import ACT. |
| JASC PAL | Text palette associated with JASC Software's Paint Shop Pro, now continued as Corel PaintShop Pro. It begins with `JASC-PAL`, version `0100`, an entry count, and decimal `R G B` lines. Historical files conventionally use CRLF line endings. | `pal-jasc:PATH`; `pal:PATH` or `.pal` also works when the signature is JASC. A conventional `.psp` filename needs the prefix. libsixel accepts CR, LF, or CRLF and emits LF; `-M palette.pal` defaults to this format. | Paint Shop Pro; GIMP; Krita imports it as a PaintShop Pro `.psp` palette. |
| RIFF PAL | Microsoft's RIFF “Simple PAL” representation of a Windows logical palette. A `RIFF` / `PAL ` container holds a `data` chunk with version `0x0300`, an entry count, and RGB-plus-flags entries. | `pal-riff:PATH`; `pal:PATH` or `.pal` also works when the signature is RIFF. A conventional `.riff` filename needs the prefix. libsixel reads the RGB fields without preserving entry flags and writes every flag as zero. | Windows RIFF palette consumers; GIMP; Krita imports `.riff`. |
| GIMP Palette (GPL) | GIMP's human-readable palette format. It has a `GIMP Palette` header, optional name and column metadata, comments, and decimal RGB rows with optional color names. GPL version 2 defines those entries as 8-bit sRGB. | `gpl:PATH` or `.gpl`. libsixel reads RGB entries but does not preserve names or layout metadata; it exports a new palette name, 16-column hint, comment, and `Index N` labels. | GIMP; Krita; Aseprite and other open-source graphics tools that use GIMP palettes. |

### Shared import limits and text handling

Structured palette input is read completely before parsing and is limited to 16 MiB, whether it comes from a file or standard input. A payload exactly at the limit is accepted if its format is otherwise valid; the next byte is rejected. Empty input, missing files, directory paths, and an explicit type prefix with an empty path are errors.

JASC PAL and GPL are parsed as byte-oriented text. An optional UTF-8 BOM is accepted, UTF-16 and UTF-32 BOMs are rejected, and an embedded NUL byte is rejected. The parser accepts CR, LF, or CRLF line endings. Leading and trailing spaces or tabs are removed from each line before it is interpreted. These are libsixel's accepted-input rules, which are deliberately somewhat broader than the canonical line-ending rules of the originating formats.

All imported structured formats produce an ordered palette of 1 to 256 RGB triplets, except that ACT's historical zero count means 256. Palette order is retained. No format carries an ICC profile in the supported representation, and non-RGB metadata is either rejected or discarded as described below.

### Adobe Color Table (ACT) layout

An ACT file has no magic number or version field. Its layout is fixed:

```text
offset      size       value
0           768        256 consecutive entries: R, G, B, one byte each
768         2          optional used-color count, unsigned big-endian
770         2          optional transparency index, unsigned big-endian
total       768 or 772 bytes
```

On import, a 768-byte file uses all 256 entries. In a 772-byte file, a count from 1 through 256 selects that many entries starting at index zero; count zero is accepted as the historical spelling of 256. Counts above 256, payloads shorter than 768 bytes, and lengths other than 768 or 772 bytes are rejected. The transparency word does not move the first imported entry and does not make any palette entry transparent: `-m` supplies colors, while source-image transparency remains governed by the loader alpha policy.

`-M` always writes the 772-byte form. It writes the finalized `N` RGB entries first, zero-fills the unused part of the 256-entry table, writes `N` as the big-endian count, and writes zero as the transparency index. Consequently an ACT round trip preserves the ordered RGB entries and count but does not preserve an imported transparency index.

### JASC PAL layout

The canonical JASC form is a sequence of text lines:

```text
JASC-PAL
0100
N
R0 G0 B0
...
R(N-1) G(N-1) B(N-1)
```

The first three nonblank, noncomment lines must be the exact header `JASC-PAL`, the exact version `0100`, and a decimal count from 1 through 256. A comment line starts with `#`. Exactly `N` color rows must follow. Every color row contains exactly three decimal components in RGB order, each from 0 through 255; missing components, a fourth token, glued suffixes, negative values, and values above 255 are rejected. Fewer or more rows than declared are also rejected. libsixel accepts CR, LF, and CRLF input even though historical JASC files conventionally use CRLF.

`-M` writes the header, version, decimal count, and exactly `N` unlabelled `R G B` rows with LF endings. It does not emit comments or palette metadata.

### RIFF PAL layout

RIFF PAL uses the generic little-endian RIFF chunk structure. The Simple PAL form accepted by libsixel is:

```text
offset      size       value
0           4          ASCII "RIFF"
4           4          little-endian file size minus 8
8           4          ASCII form type "PAL "
12          variable   RIFF subchunks

required "data" subchunk payload:
0           2          little-endian version 0x0300 (bytes 00 03)
2           2          little-endian entry count N, 1 through 256
4           4 * N      entries: R, G, B, flags, one byte each
```

The RIFF size must describe the complete file. Each subchunk has a four-byte identifier, a little-endian 32-bit payload size, its payload, and a pad byte when that size is odd; the size excludes the eight-byte subchunk header and the pad byte. Unknown well-formed subchunks are skipped, but exactly one `data` subchunk is required. The `data` payload size must equal `4 + 4 * N`. Truncated chunks, missing padding, duplicate or missing `data` chunks, version values other than `0x0300`, counts outside 1 through 256, inconsistent sizes, and bytes left outside the declared chunk traversal are rejected.

Each entry's first three bytes are imported in red, green, blue order. The flags byte is ignored. `-M` writes one `data` subchunk, writes version `0x0300`, writes `N`, preserves the ordered RGB entries, and writes every flags byte as zero. The resulting file is `24 + 4 * N` bytes long.

### GIMP Palette (GPL) layout

The canonical GPL version 2 form begins with `GIMP Palette`, may carry `Name:` and `Columns:` metadata, and then contains comments, blank lines, and color rows. A color row is:

```text
R G B optional color name
```

libsixel requires the `GIMP Palette` header before any color row and requires at least one color. Blank lines and lines beginning with `#` are ignored. `Name:` and `Columns:` lines are accepted but their values are not validated or preserved. Each color row begins with exactly three decimal components in RGB order, each from 0 through 255. A trailing nonnumeric color name is accepted, but a fourth numeric token is rejected. More than 256 color rows, missing components, glued component suffixes, negative values, and values above 255 are rejected.

`-M` writes LF-terminated GPL text with the exact prologue `GIMP Palette`, `Name: libsixel export`, `Columns: 16`, and `# Exported by libsixel`. It then writes exactly `N` RGB rows and labels them `Index 0` through `Index N-1`. Imported names, comments, column settings, and layout are recreated rather than preserved.

### Choosing an interchange format

- Prefer GPL for manual inspection, version control, hand editing, meaningful color labels, and interchange among GIMP-oriented tools.
- Prefer JASC PAL when a simple text format or Paint Shop Pro compatibility is the priority.
- Prefer RIFF PAL for software built around Windows logical palettes or when a binary RIFF container is required.
- Prefer ACT for Photoshop color-table workflows.
- Use a PNG swatch image as an image mapfile when the colors are naturally distributed with an image asset and exact palette-table indices are unnecessary.

Do not choose a format merely from the `.pal` suffix: JASC PAL and RIFF PAL are unrelated layouts that conventionally share it. Use an explicit prefix in scripts and build systems.

## Interaction with palette policies

Supplying a palette removes the construction stage but leaves the application stage. The current behavior is:

| Control | Effect with `-m` |
| --- | --- |
| `-p` | Conflicts because the supplied palette determines its own entry count. |
| `-b`, `-e`, `-I` | Conflicts because these select another fixed palette or output mode. |
| `-Q`, `-4`, `-5`, `-F`, `-X` | The palette-construction consumers are bypassed. |
| `-a`, `--cover-policy` | An explicit value, including `off`, is rejected during option processing because a fixed palette has no cover-repair stage. |
| `-_`, `--snap-policy` | Accepted for compatibility but currently bypassed, preserving the supplied entries. |
| `-W`, `--working-colorspace` | Applies to palette application; both image and palette are represented in the selected lookup and diffusion space. |
| `-~`, `--lookup-policy` | An explicit policy applies to the supplied palette. Without an explicit policy, text palette files retain the historical direct-scan behavior. |
| `-d`, `--diffusion` | Applies while input pixels are mapped to the supplied entries. |
| `-U`, `--output-colorspace` | Selects SIXEL serialization; legacy `-M` output remains sRGB8. |

The selected float image-map builder is the documented exception to the construction bypass above; it uses the native construction controls before the resulting palette becomes fixed. Legacy image-map reconstruction has a [known snap-environment override defect](snap-policy/README.md#known-contract-violations); the structured-palette bypass tests do not cover that reconstruction path.

The same lookup, cover, and snap rules apply to the other fixed palettes selected by `-b` and `-e`: explicit lookup applies, explicit cover conflicts, and snap is accepted but bypassed. The owning policy documents define the normative details: [Lookup Policy](../palette-application/lookup-policy/README.md), [Palette Cover Policy](cover-policy/README.md), [Palette Snap Policy](snap-policy/README.md), and [Palette Working Color Space](../palette-application/working-colorspace/README.md).

## Historical use by sayaka on NetBSD/x68k

libsixel added `-m` in [March 2014](https://github.com/saitoha/libsixel/commit/7771fd9a962bf719d99123de1f52fddf88a5a555), followed three days later by [support for using color-map images](https://github.com/saitoha/libsixel/commit/9674865e09838c48f38d1ef7999444ecfb0164c5). The [initial PHP implementation of isaki68k/sayaka](https://github.com/isaki68k/sayaka/blob/6d3f96b6e8605abb95de548623a0ffa4b46a94ef/sayaka.php#L125-L150), committed in September 2014, invoked `img2sixel -m colormap8.png` or `colormap16.png` for its reduced-color modes. A [September 2015 x68k change](https://github.com/isaki68k/sayaka/commit/b297510e2edb75dd2b64294f0768dca9d019ceec) added `colormapx68k16.png` and passed it through `-m` for the X68000-specific 16-color mode. This is a concrete early example of the external-palette feature being used to fit image output to a restricted old-machine display environment.

The project then moved away from that dependency. A [July 2015 commit](https://github.com/isaki68k/sayaka/commit/f4d43a605f5d701ae6266f86ea506ec593ae43d8) introduced a `SixelConverter` that used GdkPixbuf and emitted SIXEL without `img2sixel`; the [old PHP implementation was retired](https://github.com/isaki68k/sayaka/commit/05f27b3050db69abdd6e42abd6e3565d317ded1e) in 2016. The present sayaka tree contains its own image reduction and SIXEL output code.

`-M` is not part of that early history. It was added in [November 2025](https://github.com/saitoha/libsixel/commit/74a8cda3f441145dc6601989eab9e1f9412f0107), turning the older one-way fixed-palette input into a palette-generation, inspection, editing, and reuse workflow.

## Implementation

- [`src/mapfile.c`](../../../src/mapfile.c) implements format detection, parsing, and writing for ACT, JASC PAL, RIFF PAL, and GPL.
- [`src/encoder.c`](../../../src/encoder.c) decides whether a mapfile is a palette or image, prepares fixed palettes, applies option conflicts, captures the final palette, and emits `-M` output.
- [`tests/palette/mapfile-io/`](../../../tests/palette/mapfile-io/) covers format parsing, detection, malformed inputs, standard input, and format writers.
- [`tests/palette`](../../../tests/palette) covers image mapfiles, lookup and diffusion application, fixed-palette bypasses, color-space conversion, animation, and palette export.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The table contains tests that directly own user-visible contracts stated by this document. A test is not added merely because it exercises the same parser or option.

| ID | Contract | Owning behavior test |
| --- | --- | --- |
| EP-01 | A direct-color PNG can supply colors through `-m`. | [tests/palette/mapfile-application/0103_palette_map_png_rgb.t](../../../tests/palette/mapfile-application/0103_palette_map_png_rgb.t) |
| EP-02 | Indexed PNG map images with 1-, 2-, 4-, and 8-bit indices are accepted. | [tests/palette/mapfile-application/0104_palette_map_png_pal1.t](../../../tests/palette/mapfile-application/0104_palette_map_png_pal1.t), [tests/palette/mapfile-application/0105_palette_map_png_pal2.t](../../../tests/palette/mapfile-application/0105_palette_map_png_pal2.t), [tests/palette/mapfile-application/0106_palette_map_png_pal4.t](../../../tests/palette/mapfile-application/0106_palette_map_png_pal4.t), [tests/palette/mapfile-application/0107_palette_map_png_pal8.t](../../../tests/palette/mapfile-application/0107_palette_map_png_pal8.t) |
| EP-164 | An unmanaged 2-bit grayscale image map retains its four decoded gray values through gamma-byte palette reconstruction; palette order is not prescribed. | [tests/palette/mapfile-application/0109_palette_map_png_g2_values.t](../../../tests/palette/mapfile-application/0109_palette_map_png_g2_values.t) |
| EP-03 | Non-gamma image-map clustering retains original occupied unmanaged colors and builds in float under every precision policy; automatic precision also applies that float palette, and explicit float reconstruction provides a control. | [tests/palette/working-space/0121_mapfile_png_clustering_values.t](../../../tests/palette/working-space/0121_mapfile_png_clustering_values.t), [tests/palette/mapfile-io/0128_mapfile_png_float_values.t](../../../tests/palette/mapfile-io/0128_mapfile_png_float_values.t), [tests/palette/working-space/0126_mapfile_png_clustering_float_output.t](../../../tests/palette/working-space/0126_mapfile_png_clustering_float_output.t) |
| EP-04 | Native linear working conversion preserves the occupied source sRGB8 colors of indexed and RGBA PNG mapfiles at export, with CMS, binning and snap explicitly disabled, including embedded ICC metadata. | [tests/palette/working-space/0136_mapfile_png_working_output_colorspace_deterministic.t](../../../tests/palette/working-space/0136_mapfile_png_working_output_colorspace_deterministic.t), [tests/palette/working-space/0145_mapfile_png_icc_working_output_colorspace_deterministic.t](../../../tests/palette/working-space/0145_mapfile_png_icc_working_output_colorspace_deterministic.t) |
| EP-05 | `-m` and `-M` can translate a prepared JASC PAL palette to GPL. | [tests/palette/mapfile-io/0131_mapfile_pal_mapfile_output_gpl.t](../../../tests/palette/mapfile-io/0131_mapfile_pal_mapfile_output_gpl.t) |
| EP-06 | An explicit lookup policy reaches both byte and float32 palette application with a palette mapfile. | [tests/lookup/dispatch/0026_mapfile_pal_explicit_lookup_policy_applied.t](../../../tests/lookup/dispatch/0026_mapfile_pal_explicit_lookup_policy_applied.t) |
| EP-07 | ACT transparency metadata neither shifts entries nor causes a valid palette to be rejected. | [tests/palette/mapfile-io/0025_mapfile_import_act_accepts_transparency_255_count_2.t](../../../tests/palette/mapfile-io/0025_mapfile_import_act_accepts_transparency_255_count_2.t), [tests/palette/mapfile-io/0045_mapfile_import_act_transparency_does_not_offset_palette.t](../../../tests/palette/mapfile-io/0045_mapfile_import_act_transparency_does_not_offset_palette.t) |
| EP-08 | ACT export writes the exact 772-byte RGB table, zero padding, big-endian entry count, and zero transparency index, including the 256-color boundary and binary standard output. | [tests/palette/mapfile-io/0080_mapfile_export_act_exact_layout.t](../../../tests/palette/mapfile-io/0080_mapfile_export_act_exact_layout.t), [tests/palette/mapfile-io/0109_mapfile_export_act_256_color_count.t](../../../tests/palette/mapfile-io/0109_mapfile_export_act_256_color_count.t), [tests/palette/mapfile-io/0119_mapfile_export_act_stdout_exact.t](../../../tests/palette/mapfile-io/0119_mapfile_export_act_stdout_exact.t) |
| EP-09 | A trailer-free 768-byte ACT imports as 256 ordered entries. | [tests/palette/mapfile-io/0081_mapfile_import_act_768_exact.t](../../../tests/palette/mapfile-io/0081_mapfile_import_act_768_exact.t) |
| EP-10 | JASC PAL export writes the exact header, version, count, and unlabelled RGB rows. | [tests/palette/mapfile-io/0082_mapfile_export_jasc_exact_layout.t](../../../tests/palette/mapfile-io/0082_mapfile_export_jasc_exact_layout.t) |
| EP-11 | RIFF PAL export writes the exact RIFF size, `PAL ` form, `data` payload, version, count, ordered RGB entries, and zero flags, including the 256-color boundary and binary standard output. | [tests/palette/mapfile-io/0086_mapfile_export_riff_exact_layout.t](../../../tests/palette/mapfile-io/0086_mapfile_export_riff_exact_layout.t), [tests/palette/mapfile-io/0107_mapfile_export_riff_256_color_fields.t](../../../tests/palette/mapfile-io/0107_mapfile_export_riff_256_color_fields.t), [tests/palette/mapfile-io/0120_mapfile_export_riff_stdout_exact.t](../../../tests/palette/mapfile-io/0120_mapfile_export_riff_stdout_exact.t) |
| EP-12 | An independently constructed RIFF PAL imports ordered RGB entries and discards nonzero entry flags. | [tests/palette/mapfile-io/0087_mapfile_import_riff_exact_entries.t](../../../tests/palette/mapfile-io/0087_mapfile_import_riff_exact_entries.t) |
| EP-13 | GPL export has the documented prologue and indexed rows, including exact 256-color output, while GPL import retains ordered RGB but discards names and layout metadata. | [tests/palette/mapfile-io/0130_builtin_palette_mapfile_output_gpl.t](../../../tests/palette/mapfile-io/0130_builtin_palette_mapfile_output_gpl.t), [tests/palette/mapfile-io/0088_mapfile_import_gpl_exact_entries.t](../../../tests/palette/mapfile-io/0088_mapfile_import_gpl_exact_entries.t), [tests/palette/mapfile-io/0110_mapfile_export_gpl_256_colors_exact.t](../../../tests/palette/mapfile-io/0110_mapfile_export_gpl_256_colors_exact.t) |
| EP-14 | JASC PAL import accepts LF, CR, and CRLF as three independently checked line-ending forms. | [tests/palette/mapfile-io/0083_mapfile_import_jasc_lf.t](../../../tests/palette/mapfile-io/0083_mapfile_import_jasc_lf.t), [tests/palette/mapfile-io/0084_mapfile_import_jasc_cr.t](../../../tests/palette/mapfile-io/0084_mapfile_import_jasc_cr.t), [tests/palette/mapfile-io/0085_mapfile_import_jasc_crlf.t](../../../tests/palette/mapfile-io/0085_mapfile_import_jasc_crlf.t) |
| EP-15 | GPL import accepts LF, CR, and CRLF as three independently checked line-ending forms. | [tests/palette/mapfile-io/0089_mapfile_import_gpl_lf.t](../../../tests/palette/mapfile-io/0089_mapfile_import_gpl_lf.t), [tests/palette/mapfile-io/0090_mapfile_import_gpl_cr.t](../../../tests/palette/mapfile-io/0090_mapfile_import_gpl_cr.t), [tests/palette/mapfile-io/0091_mapfile_import_gpl_crlf.t](../../../tests/palette/mapfile-io/0091_mapfile_import_gpl_crlf.t) |
| EP-16 | An indexed PNG PLTE entry not referenced by any pixel does not appear in the constructed map palette. | [tests/palette/mapfile-io/0142_mapfile_png_unused_plte_entry_ignored.t](../../../tests/palette/mapfile-io/0142_mapfile_png_unused_plte_entry_ignored.t) |
| EP-17 | Indexed-image source palette indices are not retained; output entries follow the palette reconstructed from decoded pixels. | [tests/palette/mapfile-io/0143_mapfile_png_source_indices_not_preserved.t](../../../tests/palette/mapfile-io/0143_mapfile_png_source_indices_not_preserved.t) |
| EP-18 | Duplicate colors in decoded indexed-image pixels can collapse to one constructed map-palette entry. | [tests/palette/mapfile-io/0144_mapfile_png_duplicate_colors_collapse.t](../../../tests/palette/mapfile-io/0144_mapfile_png_duplicate_colors_collapse.t) |
| EP-19 | For an image mapfile containing more than 256 colors, changing pixel frequencies while retaining the same color set can change the reduced palette. | [tests/palette/mapfile-io/0145_mapfile_image_distribution_affects_reduction.t](../../../tests/palette/mapfile-io/0145_mapfile_image_distribution_affects_reduction.t) |
| EP-20 | Structured palette selection honors explicit prefixes, typed standard input, recognized extensions, case-insensitive prefix spelling, content signatures, and documented ambiguity rejection and prefix precedence. Untyped standard input is rejected. | [tests/palette/mapfile-io/0006_mapfile_import_gpl_prefixed.t](../../../tests/palette/mapfile-io/0006_mapfile_import_gpl_prefixed.t), [tests/palette/mapfile-io/0007_mapfile_import_act_extension.t](../../../tests/palette/mapfile-io/0007_mapfile_import_act_extension.t), [tests/palette/mapfile-io/0008_mapfile_import_riff_explicit.t](../../../tests/palette/mapfile-io/0008_mapfile_import_riff_explicit.t), [tests/palette/mapfile-io/0009_mapfile_import_stdin.t](../../../tests/palette/mapfile-io/0009_mapfile_import_stdin.t), [tests/palette/mapfile-io/0034_mapfile_import_rejects_extensionless_size_only_act.t](../../../tests/palette/mapfile-io/0034_mapfile_import_rejects_extensionless_size_only_act.t), [tests/palette/mapfile-io/0035_mapfile_import_accepts_extensionless_act_with_prefix.t](../../../tests/palette/mapfile-io/0035_mapfile_import_accepts_extensionless_act_with_prefix.t), [tests/palette/mapfile-io/0036_mapfile_import_prefers_jasc_signature_over_act_size.t](../../../tests/palette/mapfile-io/0036_mapfile_import_prefers_jasc_signature_over_act_size.t), [tests/palette/mapfile-io/0044_mapfile_import_rejects_unknown_extensionless_format.t](../../../tests/palette/mapfile-io/0044_mapfile_import_rejects_unknown_extensionless_format.t), [tests/palette/mapfile-io/0046_mapfile_import_rejects_pal_extension_with_act_payload.t](../../../tests/palette/mapfile-io/0046_mapfile_import_rejects_pal_extension_with_act_payload.t), [tests/palette/mapfile-io/0047_mapfile_import_accepts_pal_jasc_prefix_over_extension.t](../../../tests/palette/mapfile-io/0047_mapfile_import_accepts_pal_jasc_prefix_over_extension.t), [tests/palette/mapfile-io/0050_mapfile_import_accepts_uppercase_prefix.t](../../../tests/palette/mapfile-io/0050_mapfile_import_accepts_uppercase_prefix.t), [tests/palette/mapfile-io/0051_mapfile_import_accepts_extensionless_riff_signature.t](../../../tests/palette/mapfile-io/0051_mapfile_import_accepts_extensionless_riff_signature.t), [tests/palette/mapfile-io/0052_mapfile_import_accepts_pal_riff_prefix_over_extension.t](../../../tests/palette/mapfile-io/0052_mapfile_import_accepts_pal_riff_prefix_over_extension.t), [tests/palette/mapfile-io/0092_mapfile_import_stdin_rejects_missing_type.t](../../../tests/palette/mapfile-io/0092_mapfile_import_stdin_rejects_missing_type.t), [tests/palette/mapfile-io/0102_mapfile_import_pal_extension_detects_riff.t](../../../tests/palette/mapfile-io/0102_mapfile_import_pal_extension_detects_riff.t), [tests/palette/mapfile-io/0103_mapfile_import_pal_prefix_detects_riff.t](../../../tests/palette/mapfile-io/0103_mapfile_import_pal_prefix_detects_riff.t), [tests/palette/mapfile-io/0104_mapfile_import_extensionless_gpl_detected.t](../../../tests/palette/mapfile-io/0104_mapfile_import_extensionless_gpl_detected.t) |
| EP-21 | File and standard-input palette streams accept exactly 16 MiB and reject the next byte. | [tests/palette/mapfile-io/0014_mapfile_import_rejects_input_over_16mib.t](../../../tests/palette/mapfile-io/0014_mapfile_import_rejects_input_over_16mib.t), [tests/palette/mapfile-io/0015_mapfile_import_accepts_exactly_16mib.t](../../../tests/palette/mapfile-io/0015_mapfile_import_accepts_exactly_16mib.t), [tests/palette/mapfile-io/0048_mapfile_import_stdin_rejects_input_over_16mib.t](../../../tests/palette/mapfile-io/0048_mapfile_import_stdin_rejects_input_over_16mib.t), [tests/palette/mapfile-io/0049_mapfile_import_stdin_accepts_exactly_16mib.t](../../../tests/palette/mapfile-io/0049_mapfile_import_stdin_accepts_exactly_16mib.t) |
| EP-22 | Missing files, directories, empty option values, prefix-only paths, and empty standard input are rejected. | [tests/palette/mapfile-io/0053_mapfile_import_rejects_missing_path.t](../../../tests/palette/mapfile-io/0053_mapfile_import_rejects_missing_path.t), [tests/palette/mapfile-io/0054_mapfile_import_rejects_directory_path.t](../../../tests/palette/mapfile-io/0054_mapfile_import_rejects_directory_path.t), [tests/palette/mapfile-io/0055_mapfile_import_rejects_empty_option.t](../../../tests/palette/mapfile-io/0055_mapfile_import_rejects_empty_option.t), [tests/palette/mapfile-io/0056_mapfile_import_rejects_prefix_only.t](../../../tests/palette/mapfile-io/0056_mapfile_import_rejects_prefix_only.t), [tests/palette/mapfile-io/0057_mapfile_import_rejects_empty_stdin.t](../../../tests/palette/mapfile-io/0057_mapfile_import_rejects_empty_stdin.t) |
| EP-23 | ACT import implements zero-as-256 count compatibility, accepts valid count/transparency boundaries, and rejects oversized counts, truncation, and lengths other than 768 or 772 bytes. | [tests/palette/mapfile-io/0010_mapfile_import_act_rejects_count_over_256.t](../../../tests/palette/mapfile-io/0010_mapfile_import_act_rejects_count_over_256.t), [tests/palette/mapfile-io/0016_mapfile_import_act_accepts_zero_color_count.t](../../../tests/palette/mapfile-io/0016_mapfile_import_act_accepts_zero_color_count.t), [tests/palette/mapfile-io/0024_mapfile_import_act_accepts_transparency_255_count_1.t](../../../tests/palette/mapfile-io/0024_mapfile_import_act_accepts_transparency_255_count_1.t), [tests/palette/mapfile-io/0026_mapfile_import_act_accepts_transparency_0_count_256.t](../../../tests/palette/mapfile-io/0026_mapfile_import_act_accepts_transparency_0_count_256.t), [tests/palette/mapfile-io/0058_mapfile_import_act_rejects_truncated_palette.t](../../../tests/palette/mapfile-io/0058_mapfile_import_act_rejects_truncated_palette.t), [tests/palette/mapfile-io/0059_mapfile_import_act_rejects_invalid_length.t](../../../tests/palette/mapfile-io/0059_mapfile_import_act_rejects_invalid_length.t) |
| EP-24 | JASC PAL and GPL accept a UTF-8 BOM, reject embedded NUL, and reject representative UTF-16LE, UTF-16BE, UTF-32LE, and UTF-32BE BOM input. | [tests/palette/mapfile-io/0032_mapfile_import_pal_rejects_embedded_nul.t](../../../tests/palette/mapfile-io/0032_mapfile_import_pal_rejects_embedded_nul.t), [tests/palette/mapfile-io/0033_mapfile_import_gpl_rejects_embedded_nul.t](../../../tests/palette/mapfile-io/0033_mapfile_import_gpl_rejects_embedded_nul.t), [tests/palette/mapfile-io/0039_mapfile_import_pal_rejects_utf16_le_bom.t](../../../tests/palette/mapfile-io/0039_mapfile_import_pal_rejects_utf16_le_bom.t), [tests/palette/mapfile-io/0040_mapfile_import_gpl_rejects_utf16_be_bom.t](../../../tests/palette/mapfile-io/0040_mapfile_import_gpl_rejects_utf16_be_bom.t), [tests/palette/mapfile-io/0042_mapfile_import_pal_accepts_utf8_bom.t](../../../tests/palette/mapfile-io/0042_mapfile_import_pal_accepts_utf8_bom.t), [tests/palette/mapfile-io/0043_mapfile_import_gpl_accepts_utf8_bom.t](../../../tests/palette/mapfile-io/0043_mapfile_import_gpl_accepts_utf8_bom.t), [tests/palette/mapfile-io/0093_mapfile_import_pal_rejects_utf32_le_bom.t](../../../tests/palette/mapfile-io/0093_mapfile_import_pal_rejects_utf32_le_bom.t), [tests/palette/mapfile-io/0094_mapfile_import_gpl_rejects_utf32_be_bom.t](../../../tests/palette/mapfile-io/0094_mapfile_import_gpl_rejects_utf32_be_bom.t) |
| EP-25 | JASC PAL requires the exact header and version, a count from 1 through 256, exactly that many rows, and exactly three decimal components from 0 through 255 per row; comments, blank lines, and surrounding spaces or tabs are ignored. | [tests/palette/mapfile-io/0011_mapfile_import_pal_rejects_count_suffix.t](../../../tests/palette/mapfile-io/0011_mapfile_import_pal_rejects_count_suffix.t), [tests/palette/mapfile-io/0012_mapfile_import_pal_rejects_component_suffix.t](../../../tests/palette/mapfile-io/0012_mapfile_import_pal_rejects_component_suffix.t), [tests/palette/mapfile-io/0020_mapfile_import_pal_accepts_version_0100.t](../../../tests/palette/mapfile-io/0020_mapfile_import_pal_accepts_version_0100.t), [tests/palette/mapfile-io/0021_mapfile_import_pal_rejects_version_0099.t](../../../tests/palette/mapfile-io/0021_mapfile_import_pal_rejects_version_0099.t), [tests/palette/mapfile-io/0022_mapfile_import_pal_rejects_version_0200.t](../../../tests/palette/mapfile-io/0022_mapfile_import_pal_rejects_version_0200.t), [tests/palette/mapfile-io/0023_mapfile_import_pal_rejects_version_suffix.t](../../../tests/palette/mapfile-io/0023_mapfile_import_pal_rejects_version_suffix.t), [tests/palette/mapfile-io/0060_mapfile_import_pal_rejects_missing_header.t](../../../tests/palette/mapfile-io/0060_mapfile_import_pal_rejects_missing_header.t), [tests/palette/mapfile-io/0061_mapfile_import_pal_rejects_incomplete_header.t](../../../tests/palette/mapfile-io/0061_mapfile_import_pal_rejects_incomplete_header.t), [tests/palette/mapfile-io/0062_mapfile_import_pal_rejects_count_zero.t](../../../tests/palette/mapfile-io/0062_mapfile_import_pal_rejects_count_zero.t), [tests/palette/mapfile-io/0063_mapfile_import_pal_rejects_count_over_256.t](../../../tests/palette/mapfile-io/0063_mapfile_import_pal_rejects_count_over_256.t), [tests/palette/mapfile-io/0064_mapfile_import_pal_rejects_missing_component.t](../../../tests/palette/mapfile-io/0064_mapfile_import_pal_rejects_missing_component.t), [tests/palette/mapfile-io/0065_mapfile_import_pal_rejects_negative_component.t](../../../tests/palette/mapfile-io/0065_mapfile_import_pal_rejects_negative_component.t), [tests/palette/mapfile-io/0066_mapfile_import_pal_rejects_component_over_255.t](../../../tests/palette/mapfile-io/0066_mapfile_import_pal_rejects_component_over_255.t), [tests/palette/mapfile-io/0067_mapfile_import_pal_rejects_excess_entries.t](../../../tests/palette/mapfile-io/0067_mapfile_import_pal_rejects_excess_entries.t), [tests/palette/mapfile-io/0068_mapfile_import_pal_rejects_color_count_mismatch.t](../../../tests/palette/mapfile-io/0068_mapfile_import_pal_rejects_color_count_mismatch.t), [tests/palette/mapfile-io/0095_mapfile_import_pal_accepts_comments.t](../../../tests/palette/mapfile-io/0095_mapfile_import_pal_accepts_comments.t), [tests/palette/mapfile-io/0096_mapfile_import_pal_accepts_blank_lines.t](../../../tests/palette/mapfile-io/0096_mapfile_import_pal_accepts_blank_lines.t), [tests/palette/mapfile-io/0097_mapfile_import_pal_trims_spaces_and_tabs.t](../../../tests/palette/mapfile-io/0097_mapfile_import_pal_trims_spaces_and_tabs.t), [tests/palette/mapfile-io/0100_mapfile_import_pal_rejects_fourth_numeric_token.t](../../../tests/palette/mapfile-io/0100_mapfile_import_pal_rejects_fourth_numeric_token.t), [tests/palette/mapfile-io/0105_mapfile_import_pal_accepts_256_colors.t](../../../tests/palette/mapfile-io/0105_mapfile_import_pal_accepts_256_colors.t) |
| EP-26 | RIFF PAL validates the container size, chunk traversal and padding, unique required `data` chunk, version `0x0300`, entry count from 1 through 256, and exact payload size, while skipping well-formed unknown chunks. | [tests/palette/mapfile-io/0017_mapfile_import_riff_rejects_chunk_payload_overrun.t](../../../tests/palette/mapfile-io/0017_mapfile_import_riff_rejects_chunk_payload_overrun.t), [tests/palette/mapfile-io/0018_mapfile_import_riff_rejects_missing_chunk_padding.t](../../../tests/palette/mapfile-io/0018_mapfile_import_riff_rejects_missing_chunk_padding.t), [tests/palette/mapfile-io/0027_mapfile_import_riff_rejects_declared_size_mismatch.t](../../../tests/palette/mapfile-io/0027_mapfile_import_riff_rejects_declared_size_mismatch.t), [tests/palette/mapfile-io/0028_mapfile_import_riff_rejects_missing_data_chunk.t](../../../tests/palette/mapfile-io/0028_mapfile_import_riff_rejects_missing_data_chunk.t), [tests/palette/mapfile-io/0029_mapfile_import_riff_rejects_entry_count_zero.t](../../../tests/palette/mapfile-io/0029_mapfile_import_riff_rejects_entry_count_zero.t), [tests/palette/mapfile-io/0030_mapfile_import_riff_rejects_entry_count_over_256.t](../../../tests/palette/mapfile-io/0030_mapfile_import_riff_rejects_entry_count_over_256.t), [tests/palette/mapfile-io/0031_mapfile_import_riff_rejects_duplicate_data_chunk.t](../../../tests/palette/mapfile-io/0031_mapfile_import_riff_rejects_duplicate_data_chunk.t), [tests/palette/mapfile-io/0037_mapfile_import_riff_rejects_invalid_version.t](../../../tests/palette/mapfile-io/0037_mapfile_import_riff_rejects_invalid_version.t), [tests/palette/mapfile-io/0038_mapfile_import_riff_rejects_trailing_bytes.t](../../../tests/palette/mapfile-io/0038_mapfile_import_riff_rejects_trailing_bytes.t), [tests/palette/mapfile-io/0069_mapfile_import_riff_rejects_truncated_palette.t](../../../tests/palette/mapfile-io/0069_mapfile_import_riff_rejects_truncated_palette.t), [tests/palette/mapfile-io/0070_mapfile_import_riff_rejects_missing_header.t](../../../tests/palette/mapfile-io/0070_mapfile_import_riff_rejects_missing_header.t), [tests/palette/mapfile-io/0071_mapfile_import_riff_rejects_data_chunk_too_small.t](../../../tests/palette/mapfile-io/0071_mapfile_import_riff_rejects_data_chunk_too_small.t), [tests/palette/mapfile-io/0072_mapfile_import_riff_rejects_unexpected_chunk_size.t](../../../tests/palette/mapfile-io/0072_mapfile_import_riff_rejects_unexpected_chunk_size.t), [tests/palette/mapfile-io/0101_mapfile_import_riff_skips_padded_unknown_chunk.t](../../../tests/palette/mapfile-io/0101_mapfile_import_riff_skips_padded_unknown_chunk.t), [tests/palette/mapfile-io/0108_mapfile_import_riff_accepts_256_colors.t](../../../tests/palette/mapfile-io/0108_mapfile_import_riff_accepts_256_colors.t) |
| EP-27 | GPL requires its header and at least one but no more than 256 rows, accepts trailing nonnumeric labels, ignores blank lines, trims surrounding spaces or tabs, and rejects invalid RGB components or a fourth numeric token. | [tests/palette/mapfile-io/0013_mapfile_import_gpl_rejects_component_suffix.t](../../../tests/palette/mapfile-io/0013_mapfile_import_gpl_rejects_component_suffix.t), [tests/palette/mapfile-io/0019_mapfile_import_gpl_accepts_label_suffix.t](../../../tests/palette/mapfile-io/0019_mapfile_import_gpl_accepts_label_suffix.t), [tests/palette/mapfile-io/0041_mapfile_import_gpl_rejects_extra_numeric_token.t](../../../tests/palette/mapfile-io/0041_mapfile_import_gpl_rejects_extra_numeric_token.t), [tests/palette/mapfile-io/0073_mapfile_import_gpl_rejects_missing_header.t](../../../tests/palette/mapfile-io/0073_mapfile_import_gpl_rejects_missing_header.t), [tests/palette/mapfile-io/0074_mapfile_import_gpl_rejects_metadata_only_header_missing.t](../../../tests/palette/mapfile-io/0074_mapfile_import_gpl_rejects_metadata_only_header_missing.t), [tests/palette/mapfile-io/0075_mapfile_import_gpl_rejects_no_colors.t](../../../tests/palette/mapfile-io/0075_mapfile_import_gpl_rejects_no_colors.t), [tests/palette/mapfile-io/0076_mapfile_import_gpl_rejects_too_many_colors.t](../../../tests/palette/mapfile-io/0076_mapfile_import_gpl_rejects_too_many_colors.t), [tests/palette/mapfile-io/0077_mapfile_import_gpl_rejects_missing_component.t](../../../tests/palette/mapfile-io/0077_mapfile_import_gpl_rejects_missing_component.t), [tests/palette/mapfile-io/0078_mapfile_import_gpl_rejects_negative_component.t](../../../tests/palette/mapfile-io/0078_mapfile_import_gpl_rejects_negative_component.t), [tests/palette/mapfile-io/0079_mapfile_import_gpl_rejects_component_over_255.t](../../../tests/palette/mapfile-io/0079_mapfile_import_gpl_rejects_component_over_255.t), [tests/palette/mapfile-io/0098_mapfile_import_gpl_accepts_blank_lines.t](../../../tests/palette/mapfile-io/0098_mapfile_import_gpl_accepts_blank_lines.t), [tests/palette/mapfile-io/0099_mapfile_import_gpl_trims_spaces_and_tabs.t](../../../tests/palette/mapfile-io/0099_mapfile_import_gpl_trims_spaces_and_tabs.t), [tests/palette/mapfile-io/0106_mapfile_import_gpl_accepts_256_colors.t](../../../tests/palette/mapfile-io/0106_mapfile_import_gpl_accepts_256_colors.t) |
| EP-28 | Palette output selection honors `.gpl`, case-insensitive type prefixes, and explicit prefix precedence. | [tests/palette/mapfile-io/0111_mapfile_export_gpl_extension_exact.t](../../../tests/palette/mapfile-io/0111_mapfile_export_gpl_extension_exact.t), [tests/palette/mapfile-io/0117_mapfile_export_accepts_uppercase_prefix.t](../../../tests/palette/mapfile-io/0117_mapfile_export_accepts_uppercase_prefix.t), [tests/palette/mapfile-io/0118_mapfile_export_prefix_overrides_extension.t](../../../tests/palette/mapfile-io/0118_mapfile_export_prefix_overrides_extension.t) |
| EP-29 | Palette output rejects untyped standard output, unknown extensions, empty values, prefix-only values, and paths that cannot be opened. | [tests/palette/mapfile-io/0112_mapfile_export_stdout_rejects_missing_type.t](../../../tests/palette/mapfile-io/0112_mapfile_export_stdout_rejects_missing_type.t), [tests/palette/mapfile-io/0113_mapfile_export_rejects_unknown_extension.t](../../../tests/palette/mapfile-io/0113_mapfile_export_rejects_unknown_extension.t), [tests/palette/mapfile-io/0114_mapfile_export_rejects_empty_option.t](../../../tests/palette/mapfile-io/0114_mapfile_export_rejects_empty_option.t), [tests/palette/mapfile-io/0115_mapfile_export_rejects_prefix_only.t](../../../tests/palette/mapfile-io/0115_mapfile_export_rejects_prefix_only.t), [tests/palette/mapfile-io/0116_mapfile_export_rejects_unopenable_path.t](../../../tests/palette/mapfile-io/0116_mapfile_export_rejects_unopenable_path.t) |
| EP-30 | Repeating `-m` or `-M` is allowed and the last value wins. | [tests/cli/option-syntax/0028_option_matching_mapfile_repeated_last_wins.t](../../../tests/cli/option-syntax/0028_option_matching_mapfile_repeated_last_wins.t), [tests/cli/option-syntax/0041_option_matching_mapfile_output_repeated_last_wins.t](../../../tests/cli/option-syntax/0041_option_matching_mapfile_output_repeated_last_wins.t) |
| EP-31 | `-m` conflicts with `-p`, `-b`, `-e`, and `-I` in either option order. | [tests/cli/palette-stage-options/0008_invalid_option_combinations_palette_size_with_mapfile.t](../../../tests/cli/palette-stage-options/0008_invalid_option_combinations_palette_size_with_mapfile.t), [tests/cli/palette-stage-options/0021_invalid_option_combinations_mapfile_then_palette_size.t](../../../tests/cli/palette-stage-options/0021_invalid_option_combinations_mapfile_then_palette_size.t), [tests/cli/palette-stage-options/0009_invalid_option_combinations_builtin_palette_then_mapfile.t](../../../tests/cli/palette-stage-options/0009_invalid_option_combinations_builtin_palette_then_mapfile.t), [tests/cli/palette-stage-options/0010_invalid_option_combinations_mapfile_then_builtin_palette.t](../../../tests/cli/palette-stage-options/0010_invalid_option_combinations_mapfile_then_builtin_palette.t), [tests/cli/option-syntax/0016_invalid_option_combinations_mapfile_then_monochrome.t](../../../tests/cli/option-syntax/0016_invalid_option_combinations_mapfile_then_monochrome.t), [tests/cli/option-syntax/0017_invalid_option_combinations_monochrome_then_mapfile.t](../../../tests/cli/option-syntax/0017_invalid_option_combinations_monochrome_then_mapfile.t), [tests/cli/output-options/0005_invalid_option_combinations_high_color_then_mapfile.t](../../../tests/cli/output-options/0005_invalid_option_combinations_high_color_then_mapfile.t), [tests/cli/output-options/0006_invalid_option_combinations_mapfile_then_high_color.t](../../../tests/cli/output-options/0006_invalid_option_combinations_mapfile_then_high_color.t) |
| EP-32 | Explicit diffusion remains active when source pixels are mapped to a supplied palette. | [tests/palette/mapfile-io/0129_mapfile_palette_explicit_dither_changes_output.t](../../../tests/palette/mapfile-io/0129_mapfile_palette_explicit_dither_changes_output.t) |
| EP-33 | A generated float CIELAB palette exports neutral black and white as exact sRGB8 endpoints through `-M gpl:-`, without a biased byte a/b round trip; solver entry order is not fixed. | [tests/palette/mapfile-io/0141_mapfile_output_converts_cielab_to_gamma.t](../../../tests/palette/mapfile-io/0141_mapfile_output_converts_cielab_to_gamma.t) |
| EP-34 | A non-PNG PNM image can construct a deterministic image-mapfile palette. | [tests/palette/mapfile-io/0121_mapfile_image_pnm_source_exact.t](../../../tests/palette/mapfile-io/0121_mapfile_image_pnm_source_exact.t) |
| EP-35 | An animated image mapfile uses only its first frame as one static palette source. | [tests/palette/mapfile-io/0122_mapfile_image_animated_gif_uses_first_frame.t](../../../tests/palette/mapfile-io/0122_mapfile_image_animated_gif_uses_first_frame.t) |
| EP-36 | Structured sRGB entries select and emit the expected pixel in a linear workspace. | [tests/palette/mapfile-io/0123_mapfile_linear_working_pixels.t](../../../tests/palette/mapfile-io/0123_mapfile_linear_working_pixels.t) |
| EP-37 | Linear wire output leaves the prepared sRGB8 export unchanged. | [tests/palette/mapfile-io/0124_mapfile_export_linear_output_srgb.t](../../../tests/palette/mapfile-io/0124_mapfile_export_linear_output_srgb.t) |
| EP-38 | sRGB and Display P3 RGB101 paths export source colors before destination rounding. | [tests/palette/mapfile-io/0125_mapfile_export_before_srgb_rgb101.t](../../../tests/palette/mapfile-io/0125_mapfile_export_before_srgb_rgb101.t), [tests/palette/snap-destination/0007_p3_palette_export_srgb.t](../../../tests/palette/snap-destination/0007_p3_palette_export_srgb.t) |
| EP-39 | A snapped working palette has the same export with gamma and linear wire output. | [tests/palette/snap-policy/0002_snap_export_output_independent.t](../../../tests/palette/snap-policy/0002_snap_export_output_independent.t) |
| EP-40 | Linear fixed-palette lookup retains unrounded working values while P3 serialization preserves entry identity. | [tests/palette/snap-destination/0006_p3_mapfile_linear.t](../../../tests/palette/snap-destination/0006_p3_mapfile_linear.t) |
| EP-41 | Gamma fixed-palette lookup retains the unrounded float palette. | [tests/palette/snap-destination/0014_p3_mapfile_gamma.t](../../../tests/palette/snap-destination/0014_p3_mapfile_gamma.t) |
| EP-42 | Linear FS uses the unrounded fixed palette for nonzero error feedback. | [tests/palette/snap-destination/0015_p3_mapfile_fs_linear.t](../../../tests/palette/snap-destination/0015_p3_mapfile_fs_linear.t) |
| EP-43 | Gamma FS uses the unrounded fixed palette for nonzero error feedback. | [tests/palette/snap-destination/0016_p3_mapfile_fs_gamma.t](../../../tests/palette/snap-destination/0016_p3_mapfile_fs_gamma.t) |
| EP-44 | ACT entries retain their sRGB meaning through P3 output. | [tests/palette/snap-destination/0017_p3_mapfile_act.t](../../../tests/palette/snap-destination/0017_p3_mapfile_act.t) |
| EP-45 | RIFF PAL entries retain their sRGB meaning through P3 output. | [tests/palette/snap-destination/0018_p3_mapfile_riff.t](../../../tests/palette/snap-destination/0018_p3_mapfile_riff.t) |
| EP-46 | GPL entries retain their sRGB meaning through P3 output. | [tests/palette/snap-destination/0019_p3_mapfile_gpl.t](../../../tests/palette/snap-destination/0019_p3_mapfile_gpl.t) |
| EP-47 | Reusing an encoder across P3/sRGB/P3 does not reuse a projected fixed palette. | [tests/palette/snap-destination/0020_p3_mapfile_reuse.t](../../../tests/palette/snap-destination/0020_p3_mapfile_reuse.t) |
| EP-48 | An ICC-tagged P3 image map preserves out-of-sRGB colors in a linear workspace. | [tests/palette/snap-destination/0021_p3_mapfile_image_linear.t](../../../tests/palette/snap-destination/0021_p3_mapfile_image_linear.t) |
| EP-49 | An ICC-tagged P3 image map preserves out-of-sRGB colors in a gamma workspace. | [tests/palette/snap-destination/0022_p3_mapfile_image_gamma.t](../../../tests/palette/snap-destination/0022_p3_mapfile_image_gamma.t) |
| EP-50 | P3 fixed-palette export preserves original sRGB values, order, duplicates and unused entries before wire rounding. | [tests/palette/snap-destination/0023_p3_mapfile_export_srgb.t](../../../tests/palette/snap-destination/0023_p3_mapfile_export_srgb.t) |

Every test listed above links back to this document through its `Policy` comment. This keeps the behavioral contract discoverable in both directions when either the documentation or a regression test changes.

Perceptual-workspace fixed-palette contracts:

| ID | Contract | Owning test |
| --- | --- | --- |
| EP-51 | P3 oklab structured map. | [tests/palette/working-space/0080_p3_oklab_structured_map.t](../../../tests/palette/working-space/0080_p3_oklab_structured_map.t) |
| EP-52 | P3 cielab structured map. | [tests/palette/working-space/0081_p3_cielab_structured_map.t](../../../tests/palette/working-space/0081_p3_cielab_structured_map.t) |
| EP-53 | P3 din99d structured map. | [tests/palette/working-space/0082_p3_din99d_structured_map.t](../../../tests/palette/working-space/0082_p3_din99d_structured_map.t) |
| EP-54 | P3 oklab image map. | [tests/palette/working-space/0083_p3_oklab_image_map.t](../../../tests/palette/working-space/0083_p3_oklab_image_map.t) |
| EP-55 | P3 cielab image map. | [tests/palette/working-space/0084_p3_cielab_image_map.t](../../../tests/palette/working-space/0084_p3_cielab_image_map.t) |
| EP-56 | P3 din99d image map. | [tests/palette/working-space/0085_p3_din99d_image_map.t](../../../tests/palette/working-space/0085_p3_din99d_image_map.t) |

### Selected ordinary-sRGB mapfile contracts

These value tests check actual application and a separate wire cache, not perceptual similarity. Structured working entries use independently calculated sRGB transfer/opponent equations (the reference equations in `tools/generate_color_fixtures.py`), with component tolerance `2e-6 + abs(expected) * 5e-6` in the normalized working units for coefficient and binary32 storage error. The P3 image-map primary starts from ideal chromaticities and allows `3e-4` per component for the fixture ICC matrix quantization and subsequent conversions; clamping its signed/above-range RGB coordinates is much larger than this budget. Discrete indices, order, wire codes and captured export bytes are exact. Export tests observe the prepared `-M` payload; the format writers have separate coverage above.

| ID | Contract | Owning test |
| --- | --- | --- |
| EP-57 | Retain ordered sRGB entries as independently derived working coordinates and literal wire values (palette gamma). | [tests/palette/mapfile-application/0014_map_palette_gamma.t](../../../tests/palette/mapfile-application/0014_map_palette_gamma.t) |
| EP-58 | Retain ordered sRGB entries as independently derived working coordinates and literal wire values (palette linear). | [tests/palette/mapfile-application/0015_map_palette_linear.t](../../../tests/palette/mapfile-application/0015_map_palette_linear.t) |
| EP-59 | Retain ordered sRGB entries as independently derived working coordinates and literal wire values (palette oklab). | [tests/palette/mapfile-application/0016_map_palette_oklab.t](../../../tests/palette/mapfile-application/0016_map_palette_oklab.t) |
| EP-60 | Retain ordered sRGB entries as independently derived working coordinates and literal wire values (palette cielab). | [tests/palette/mapfile-application/0017_map_palette_cielab.t](../../../tests/palette/mapfile-application/0017_map_palette_cielab.t) |
| EP-61 | Retain ordered sRGB entries as independently derived working coordinates and literal wire values (palette din99d). | [tests/palette/mapfile-application/0018_map_palette_din99d.t](../../../tests/palette/mapfile-application/0018_map_palette_din99d.t) |
| EP-62 | Select unsnapped gray entries with nonzero FS residual feedback (fs gamma). | [tests/palette/mapfile-application/0019_map_fs_gamma.t](../../../tests/palette/mapfile-application/0019_map_fs_gamma.t) |
| EP-63 | Select unsnapped gray entries with nonzero FS residual feedback (fs linear). | [tests/palette/mapfile-application/0020_map_fs_linear.t](../../../tests/palette/mapfile-application/0020_map_fs_linear.t) |
| EP-64 | Select unsnapped gray entries with nonzero FS residual feedback (fs oklab). | [tests/palette/mapfile-application/0021_map_fs_oklab.t](../../../tests/palette/mapfile-application/0021_map_fs_oklab.t) |
| EP-65 | Select unsnapped gray entries with nonzero FS residual feedback (fs cielab). | [tests/palette/mapfile-application/0022_map_fs_cielab.t](../../../tests/palette/mapfile-application/0022_map_fs_cielab.t) |
| EP-66 | Select unsnapped gray entries with nonzero FS residual feedback (fs din99d). | [tests/palette/mapfile-application/0023_map_fs_din99d.t](../../../tests/palette/mapfile-application/0023_map_fs_din99d.t) |
| EP-67 | Retain ordered sRGB entries as independently derived working coordinates and literal wire values (act). | [tests/palette/mapfile-application/0024_map_act.t](../../../tests/palette/mapfile-application/0024_map_act.t) |
| EP-68 | Retain ordered sRGB entries as independently derived working coordinates and literal wire values (riff). | [tests/palette/mapfile-application/0025_map_riff.t](../../../tests/palette/mapfile-application/0025_map_riff.t) |
| EP-69 | Retain ordered sRGB entries as independently derived working coordinates and literal wire values (gpl). | [tests/palette/mapfile-application/0026_map_gpl.t](../../../tests/palette/mapfile-application/0026_map_gpl.t) |
| EP-70 | Retain actual gamma byte application (byte). | [tests/palette/mapfile-application/0027_map_byte.t](../../../tests/palette/mapfile-application/0027_map_byte.t) |
| EP-71 | Honor explicit float promotion of otherwise eligible bytes (promote). | [tests/palette/mapfile-application/0028_map_promote.t](../../../tests/palette/mapfile-application/0028_map_promote.t) |
| EP-72 | Resolve float/byte/float source precision when reusing an encoder (precision reuse). | [tests/palette/mapfile-application/0029_map_precision_reuse.t](../../../tests/palette/mapfile-application/0029_map_precision_reuse.t) |
| EP-73 | Keep unprojected working entries across gamma/P3/gamma output reuse (output reuse). | [tests/palette/mapfile-application/0030_map_output_reuse.t](../../../tests/palette/mapfile-application/0030_map_output_reuse.t) |
| EP-74 | Retain a CMS-loaded image-map P3 primary outside sRGB and apply its selected entry (image gamma). | [tests/palette/mapfile-application/0031_map_image_gamma.t](../../../tests/palette/mapfile-application/0031_map_image_gamma.t) |
| EP-75 | Retain a CMS-loaded image-map P3 primary outside sRGB and apply its selected entry (image linear). | [tests/palette/mapfile-application/0032_map_image_linear.t](../../../tests/palette/mapfile-application/0032_map_image_linear.t) |
| EP-76 | Retain a CMS-loaded image-map P3 primary outside sRGB and apply its selected entry (image oklab). | [tests/palette/mapfile-application/0033_map_image_oklab.t](../../../tests/palette/mapfile-application/0033_map_image_oklab.t) |
| EP-77 | Retain a CMS-loaded image-map P3 primary outside sRGB and apply its selected entry (image cielab). | [tests/palette/mapfile-application/0034_map_image_cielab.t](../../../tests/palette/mapfile-application/0034_map_image_cielab.t) |
| EP-78 | Retain a CMS-loaded image-map P3 primary outside sRGB and apply its selected entry (image din99d). | [tests/palette/mapfile-application/0035_map_image_din99d.t](../../../tests/palette/mapfile-application/0035_map_image_din99d.t) |
| EP-79 | Capture original ordered sRGB8 entries before RGB101 rounding (export gamma). | [tests/palette/mapfile-application/0036_map_export_gamma.t](../../../tests/palette/mapfile-application/0036_map_export_gamma.t) |
| EP-80 | Capture original ordered sRGB8 entries before RGB101 rounding (export cielab). | [tests/palette/mapfile-application/0037_map_export_cielab.t](../../../tests/palette/mapfile-application/0037_map_export_cielab.t) |
| EP-81 | Treat zero-rate snap as inactive (zero snap). | [tests/palette/mapfile-application/0038_map_zero_snap.t](../../../tests/palette/mapfile-application/0038_map_zero_snap.t) |

### Mapfile solver admission

These tests reuse the independent coordinate and wire oracles above and additionally observe the actual image-map solver. None/exact/hard cases require four entries and a unique match for the independently expected primary; soft binning can retain nearly equal representatives, so that test checks the independently expected primary at the applied entry without promising a palette count or a tie index. Fixed-palette cases check nonzero FS feedback and ordered entries while construction is bypassed. Byte cases observe the actual byte lookup; the reuse case changes binning on one encoder without rewriting its AUTO request.

| ID | Contract | Owning test |
| --- | --- | --- |
| EP-82 | Retain a CMS-loaded P3 primary and execute heckbert in gamma. | [tests/palette/mapfile-application/0063_map_heckbert_gamma.t](../../../tests/palette/mapfile-application/0063_map_heckbert_gamma.t) |
| EP-83 | Retain a CMS-loaded P3 primary and execute heckbert in linear. | [tests/palette/mapfile-application/0064_map_heckbert_linear.t](../../../tests/palette/mapfile-application/0064_map_heckbert_linear.t) |
| EP-84 | Retain a CMS-loaded P3 primary and execute heckbert in oklab. | [tests/palette/mapfile-application/0065_map_heckbert_oklab.t](../../../tests/palette/mapfile-application/0065_map_heckbert_oklab.t) |
| EP-85 | Retain a CMS-loaded P3 primary and execute heckbert in cielab. | [tests/palette/mapfile-application/0066_map_heckbert_cielab.t](../../../tests/palette/mapfile-application/0066_map_heckbert_cielab.t) |
| EP-86 | Retain a CMS-loaded P3 primary and execute heckbert in din99d. | [tests/palette/mapfile-application/0067_map_heckbert_din99d.t](../../../tests/palette/mapfile-application/0067_map_heckbert_din99d.t) |
| EP-87 | Retain a CMS-loaded P3 primary and execute center in gamma. | [tests/palette/mapfile-application/0068_map_center_gamma.t](../../../tests/palette/mapfile-application/0068_map_center_gamma.t) |
| EP-88 | Retain a CMS-loaded P3 primary and execute center in linear. | [tests/palette/mapfile-application/0069_map_center_linear.t](../../../tests/palette/mapfile-application/0069_map_center_linear.t) |
| EP-89 | Retain a CMS-loaded P3 primary and execute center in oklab. | [tests/palette/mapfile-application/0070_map_center_oklab.t](../../../tests/palette/mapfile-application/0070_map_center_oklab.t) |
| EP-90 | Retain a CMS-loaded P3 primary and execute center in cielab. | [tests/palette/mapfile-application/0071_map_center_cielab.t](../../../tests/palette/mapfile-application/0071_map_center_cielab.t) |
| EP-91 | Retain a CMS-loaded P3 primary and execute center in din99d. | [tests/palette/mapfile-application/0072_map_center_din99d.t](../../../tests/palette/mapfile-application/0072_map_center_din99d.t) |
| EP-92 | Retain a CMS-loaded P3 primary and execute medoids in gamma. | [tests/palette/mapfile-application/0073_map_medoids_gamma.t](../../../tests/palette/mapfile-application/0073_map_medoids_gamma.t) |
| EP-93 | Retain a CMS-loaded P3 primary and execute medoids in linear. | [tests/palette/mapfile-application/0074_map_medoids_linear.t](../../../tests/palette/mapfile-application/0074_map_medoids_linear.t) |
| EP-94 | Retain a CMS-loaded P3 primary and execute medoids in oklab. | [tests/palette/mapfile-application/0075_map_medoids_oklab.t](../../../tests/palette/mapfile-application/0075_map_medoids_oklab.t) |
| EP-95 | Retain a CMS-loaded P3 primary and execute medoids in cielab. | [tests/palette/mapfile-application/0076_map_medoids_cielab.t](../../../tests/palette/mapfile-application/0076_map_medoids_cielab.t) |
| EP-96 | Retain a CMS-loaded P3 primary and execute medoids in din99d. | [tests/palette/mapfile-application/0077_map_medoids_din99d.t](../../../tests/palette/mapfile-application/0077_map_medoids_din99d.t) |
| EP-97 | Resolve omitted AUTO image-map construction without changing the request. | [tests/palette/mapfile-application/0078_map_auto_omitted.t](../../../tests/palette/mapfile-application/0078_map_auto_omitted.t) |
| EP-98 | Resolve none AUTO image-map construction without changing the request. | [tests/palette/mapfile-application/0079_map_auto_none.t](../../../tests/palette/mapfile-application/0079_map_auto_none.t) |
| EP-99 | Resolve hard AUTO image-map construction without changing the request. | [tests/palette/mapfile-application/0080_map_auto_hard.t](../../../tests/palette/mapfile-application/0080_map_auto_hard.t) |
| EP-100 | Resolve soft AUTO image-map construction without changing the request. | [tests/palette/mapfile-application/0081_map_auto_soft.t](../../../tests/palette/mapfile-application/0081_map_auto_soft.t) |
| EP-101 | Resolve exact AUTO image-map construction without changing the request. | [tests/palette/mapfile-application/0082_map_auto_exact.t](../../../tests/palette/mapfile-application/0082_map_auto_exact.t) |
| EP-102 | Retain native entries with heckbert and Display P3 wire output. | [tests/palette/mapfile-application/0083_map_p3_heckbert.t](../../../tests/palette/mapfile-application/0083_map_p3_heckbert.t) |
| EP-103 | Retain native entries with center and Display P3 wire output. | [tests/palette/mapfile-application/0084_map_p3_center.t](../../../tests/palette/mapfile-application/0084_map_p3_center.t) |
| EP-104 | Retain native entries with medoids and Display P3 wire output. | [tests/palette/mapfile-application/0085_map_p3_medoids.t](../../../tests/palette/mapfile-application/0085_map_p3_medoids.t) |
| EP-105 | Resolve AUTO to Heckbert with Display P3 output. | [tests/palette/mapfile-application/0086_map_p3_auto_none.t](../../../tests/palette/mapfile-application/0086_map_p3_auto_none.t) |
| EP-106 | Resolve AUTO to K-means with Display P3 output. | [tests/palette/mapfile-application/0087_map_p3_auto_hard.t](../../../tests/palette/mapfile-application/0087_map_p3_auto_hard.t) |
| EP-107 | Keep ordered unrounded fixed entries and FS selection with auto; skip construction. | [tests/palette/mapfile-application/0088_map_fixed_auto.t](../../../tests/palette/mapfile-application/0088_map_fixed_auto.t) |
| EP-108 | Keep ordered unrounded fixed entries and FS selection with heckbert; skip construction. | [tests/palette/mapfile-application/0089_map_fixed_heckbert.t](../../../tests/palette/mapfile-application/0089_map_fixed_heckbert.t) |
| EP-109 | Keep ordered unrounded fixed entries and FS selection with center; skip construction. | [tests/palette/mapfile-application/0090_map_fixed_center.t](../../../tests/palette/mapfile-application/0090_map_fixed_center.t) |
| EP-110 | Keep ordered unrounded fixed entries and FS selection with medoids; skip construction. | [tests/palette/mapfile-application/0091_map_fixed_medoids.t](../../../tests/palette/mapfile-application/0091_map_fixed_medoids.t) |
| EP-111 | Keep genuine byte map application with auto. | [tests/palette/mapfile-application/0092_map_byte_auto.t](../../../tests/palette/mapfile-application/0092_map_byte_auto.t) |
| EP-112 | Keep genuine byte map application with heckbert. | [tests/palette/mapfile-application/0093_map_byte_heckbert.t](../../../tests/palette/mapfile-application/0093_map_byte_heckbert.t) |
| EP-113 | Keep genuine byte map application with center. | [tests/palette/mapfile-application/0094_map_byte_center.t](../../../tests/palette/mapfile-application/0094_map_byte_center.t) |
| EP-114 | Keep genuine byte map application with medoids. | [tests/palette/mapfile-application/0095_map_byte_medoids.t](../../../tests/palette/mapfile-application/0095_map_byte_medoids.t) |
| EP-115 | Resolve none/hard/none AUTO image-map construction afresh on one encoder. | [tests/palette/mapfile-application/0096_map_auto_reuse.t](../../../tests/palette/mapfile-application/0096_map_auto_reuse.t) |
| EP-116 | Reject unsupported image-map binning before any output. | [tests/palette/mapfile-application/0097_map_invalid_binning.t](../../../tests/palette/mapfile-application/0097_map_invalid_binning.t) |
| EP-117 | Keep fixed entries and nonzero FS feedback with AUTO and Display P3 output. | [tests/palette/mapfile-application/0098_map_fixed_p3_auto.t](../../../tests/palette/mapfile-application/0098_map_fixed_p3_auto.t) |

### Independent image-map precision

These tests start with an RGB888 source and inspect native map coordinates, the actual lookup consumer, unchanged precision/quantizer requests and exact selected wire pixels. The P3-primary budget is the same `3e-4` ICC/float budget above. Non-gamma workspaces, explicit float and P3 output are existing-path controls; gamma with auto/byte precision exercises map-only promotion. Byte controls establish dispatch, not independent colorimetric accuracy. Reuse changes CMS on the same map without changing the map path. A static-to-multiframe transition protects the admission/cache boundary, not native animation support. Export observes the prepared sRGB8 payload.

The direct FS case uses byte samples 92 then 91 and palette entries 88/94. The first residual is -2, so the next corrected sample is `91 - 2*7/16 = 90.125`, below midpoint 91 by 0.875 byte units, well beyond float rounding error. Allocation-failure tests reject unfinished results with no diffusion or hard FS, release owned index storage, and preserve a borrowed index buffer. These value tests are separate from photographic quality measurements.

| ID | Contract | Owning test |
| --- | --- | --- |
| EP-118 | `--precision-policy=8bit` rounds a managed map primary to bytes and applies it on the byte path. | [tests/palette/mapfile-application/0039_map_cms_gamma.t](../../../tests/palette/mapfile-application/0039_map_cms_gamma.t) |
| EP-119 | Retain a managed map primary with byte source and linear work. | [tests/palette/mapfile-application/0040_map_cms_linear.t](../../../tests/palette/mapfile-application/0040_map_cms_linear.t) |
| EP-120 | Retain a managed map primary with byte source and oklab work. | [tests/palette/mapfile-application/0041_map_cms_oklab.t](../../../tests/palette/mapfile-application/0041_map_cms_oklab.t) |
| EP-121 | Retain a managed map primary with byte source and cielab work. | [tests/palette/mapfile-application/0042_map_cms_cielab.t](../../../tests/palette/mapfile-application/0042_map_cms_cielab.t) |
| EP-122 | Retain a managed map primary with byte source and din99d work. | [tests/palette/mapfile-application/0043_map_cms_din99d.t](../../../tests/palette/mapfile-application/0043_map_cms_din99d.t) |
| EP-123 | Retain managed map precision with automatic base precision. | [tests/palette/mapfile-application/0044_map_cms_auto.t](../../../tests/palette/mapfile-application/0044_map_cms_auto.t) |
| EP-124 | Keep explicit float source and map behavior as a control. | [tests/palette/mapfile-application/0045_map_cms_float.t](../../../tests/palette/mapfile-application/0045_map_cms_float.t) |
| EP-125 | Preserve the map loader gamma-float representation before conversion. | [tests/palette/mapfile-application/0046_map_cms_gamma_target.t](../../../tests/palette/mapfile-application/0046_map_cms_gamma_target.t) |
| EP-126 | Keep byte application when map CMS is explicitly disabled. | [tests/palette/mapfile-application/0047_map_cms_disabled_byte.t](../../../tests/palette/mapfile-application/0047_map_cms_disabled_byte.t) |
| EP-127 | Honor the explicit loader post-CMS byte narrowing boundary. | [tests/palette/mapfile-application/0048_map_cms_explicit_narrowing.t](../../../tests/palette/mapfile-application/0048_map_cms_explicit_narrowing.t) |
| EP-128 | Keep an untagged byte map on the byte path with a CMS engine enabled. | [tests/palette/mapfile-application/0049_map_untagged_byte.t](../../../tests/palette/mapfile-application/0049_map_untagged_byte.t) |
| EP-129 | Retain independent map precision with automatic solver selection. | [tests/palette/mapfile-application/0050_map_cms_auto_solver.t](../../../tests/palette/mapfile-application/0050_map_cms_auto_solver.t) |
| EP-130 | Retain independent map precision with K-center construction. | [tests/palette/mapfile-application/0051_map_cms_center.t](../../../tests/palette/mapfile-application/0051_map_cms_center.t) |
| EP-131 | Retain independent map precision with K-medoids construction. | [tests/palette/mapfile-application/0052_map_cms_medoids.t](../../../tests/palette/mapfile-application/0052_map_cms_medoids.t) |
| EP-132 | Re-evaluate one map across unmanaged/managed/unmanaged static inputs. | [tests/palette/mapfile-application/0053_map_reuse_byte_first.t](../../../tests/palette/mapfile-application/0053_map_reuse_byte_first.t) |
| EP-133 | Re-evaluate one map across managed/unmanaged/managed static inputs. | [tests/palette/mapfile-application/0054_map_reuse_float_first.t](../../../tests/palette/mapfile-application/0054_map_reuse_float_first.t) |
| EP-134 | Export sRGB8 from the native map while preserving working coordinates. | [tests/palette/mapfile-application/0055_map_cms_export.t](../../../tests/palette/mapfile-application/0055_map_cms_export.t) |
| EP-135 | Keep existing Display P3 map output as a precision control. | [tests/palette/mapfile-application/0056_map_cms_p3_control.t](../../../tests/palette/mapfile-application/0056_map_cms_p3_control.t) |
| EP-136 | Preserve native image-map values for multiframe source jobs. | [tests/palette/mapfile-application/0057_map_multiframe_native.t](../../../tests/palette/mapfile-application/0057_map_multiframe_native.t) |
| EP-137 | Promote byte samples for native FS and verify nonzero feedback. | [tests/palette/mapfile-application/0058_map_promote_fs.t](../../../tests/palette/mapfile-application/0058_map_promote_fs.t) |
| EP-138 | Reject native map work-buffer allocation failure with no diffusion. | [tests/palette/mapfile-application/0059_map_allocation_none.t](../../../tests/palette/mapfile-application/0059_map_allocation_none.t) |
| EP-139 | Reject native map work-buffer allocation failure with hard FS. | [tests/palette/mapfile-application/0060_map_allocation_fs.t](../../../tests/palette/mapfile-application/0060_map_allocation_fs.t) |
| EP-140 | Preserve native map precision from static to animated input. | [tests/palette/mapfile-application/0061_map_static_to_animation.t](../../../tests/palette/mapfile-application/0061_map_static_to_animation.t) |
| EP-141 | Keep caller-owned indices intact when native float allocation fails. | [tests/palette/mapfile-application/0062_map_allocation_borrowed.t](../../../tests/palette/mapfile-application/0062_map_allocation_borrowed.t) |

### Unsnapped native export across output definitions

| ID | Contract | Owning test |
| --- | --- | --- |
| EP-142 | Native unsnapped sRGB8 export is unchanged by linear wire output. | [tests/palette/output-color/0174_export_linear.t](../../../tests/palette/output-color/0174_export_linear.t) |
| EP-143 | Native unsnapped sRGB8 export is unchanged by smptec wire output. | [tests/palette/output-color/0175_export_smptec.t](../../../tests/palette/output-color/0175_export_smptec.t) |

### Palette-only export storage and reuse

| ID | Contract | Owning test |
| --- | --- | --- |
| EP-144 | Native float export adds palette storage only and preserves the emitted stream. | [tests/palette/export/0001_native_capture_memory.t](../../../tests/palette/export/0001_native_capture_memory.t) |
| EP-145 | Byte export adds palette storage only and preserves the emitted stream. | [tests/palette/export/0002_byte_capture_memory.t](../../../tests/palette/export/0002_byte_capture_memory.t) |
| EP-146 | Native full-file export avoids image-sized capture and writer copies. | [tests/palette/export/0003_native_writer_memory.t](../../../tests/palette/export/0003_native_writer_memory.t) |
| EP-147 | Byte full-file export avoids image-sized capture and writer copies. | [tests/palette/export/0004_byte_writer_memory.t](../../../tests/palette/export/0004_byte_writer_memory.t) |
| EP-148 | Encoder reuse exports the current count after a palette shrinks. | [tests/palette/export/0005_reuse_count.t](../../../tests/palette/export/0005_reuse_count.t) |
| EP-149 | Two encoders retain independent export counts and values. | [tests/palette/export/0006_encoder_isolation.t](../../../tests/palette/export/0006_encoder_isolation.t) |

### Byte image-map clustering precision

These value tests reduce a two-pixel sRGB red/blue map at the internal one-color capacity boundary (the CLI does not accept `-p` with `-m`). Independent means are calculated in each named `-X` geometry and transformed to signed gamma working coordinates. The `8e-5` absolute budget accounts for rounded inverse matrices and float storage; it is much smaller than byte rounding and the negative CIELAB green witness. The closest RGB101 boundary is more than `2.7e-4` away, so the asserted wire integers are unambiguous within that budget. K-center/K-medoids use two colors and check exact endpoints within `1e-6`. Source RGB888 storage and the base precision request stay unchanged. These are construction/precision contracts, not photographic quality measurements.

| ID | Contract | Owning test |
| --- | --- | --- |
| EP-150 | Retain the independently computed linear mean when reducing a byte image map. | [tests/palette/mapfile-application/0001_map_linear_mean.t](../../../tests/palette/mapfile-application/0001_map_linear_mean.t) |
| EP-151 | Retain the independently computed Oklab mean of a byte image map. | [tests/palette/mapfile-application/0002_map_oklab_mean.t](../../../tests/palette/mapfile-application/0002_map_oklab_mean.t) |
| EP-152 | Keep the negative green component of a CIELAB mean until wire clipping. | [tests/palette/mapfile-application/0003_map_cielab_negative.t](../../../tests/palette/mapfile-application/0003_map_cielab_negative.t) |
| EP-153 | Retain the independently computed DIN99d mean of a byte image map. | [tests/palette/mapfile-application/0004_map_din99d_mean.t](../../../tests/palette/mapfile-application/0004_map_din99d_mean.t) |
| EP-154 | Use the native Heckbert image-map mean. | [tests/palette/mapfile-application/0005_map_heckbert_mean.t](../../../tests/palette/mapfile-application/0005_map_heckbert_mean.t) |
| EP-155 | Keep exact source endpoints through native K-center map construction. | [tests/palette/mapfile-application/0006_map_center_endpoints.t](../../../tests/palette/mapfile-application/0006_map_center_endpoints.t) |
| EP-156 | Keep exact source endpoints through native K-medoids map construction. | [tests/palette/mapfile-application/0007_map_medoids_endpoints.t](../../../tests/palette/mapfile-application/0007_map_medoids_endpoints.t) |
| EP-157 | Resolve AUTO image-map construction through the common native builder. | [tests/palette/mapfile-application/0008_map_auto_mean.t](../../../tests/palette/mapfile-application/0008_map_auto_mean.t) |
| EP-158 | `--precision-policy=8bit` rounds the float map clustering palette to bytes before application. | [tests/palette/mapfile-application/0009_map_byte_request.t](../../../tests/palette/mapfile-application/0009_map_byte_request.t) |
| EP-159 | Keep a gamma-only byte image map on the byte path. | [tests/palette/mapfile-application/0010_map_gamma_byte.t](../../../tests/palette/mapfile-application/0010_map_gamma_byte.t) |
| EP-160 | Keep a structured byte palette byte-eligible despite an unused linear -X. | [tests/palette/mapfile-application/0011_map_structured_byte.t](../../../tests/palette/mapfile-application/0011_map_structured_byte.t) |
| EP-161 | Re-evaluate a static map across linear, gamma and CIELAB clustering on one encoder. | [tests/palette/mapfile-application/0012_map_clustering_reuse.t](../../../tests/palette/mapfile-application/0012_map_clustering_reuse.t) |
| EP-162 | Reuse the native map source and application across animation frames and loop replay. | [tests/palette/mapfile-application/0013_map_animation_cache.t](../../../tests/palette/mapfile-application/0013_map_animation_cache.t) |
| EP-163 | A tRNS PNG image map retains required non-gamma clustering precision under automatic precision. | [tests/palette/working-space/0144_mapfile_png_trns_clustering_float_output.t](../../../tests/palette/working-space/0144_mapfile_png_trns_clustering_float_output.t) |

### Defensive, malformed-input, and supplementary tests

The [complete mapfile input and output coverage inventory](../../testing/mapfile-parser-coverage.md) links every test individually and records the exact observation from its opening comment. Because this document specifies the accepted grammar, size boundary, format-selection rules, and rejection conditions, the corresponding malformed-input tests are grouped into the stable public contracts EP-20 through EP-27, output-option rejections are grouped in EP-29, and both groups carry `Policy:` and `Test-plan:` backlinks.

The older writer tests `0001` through `0005` remain supplementary smoke and localization checks: they assert a size or leading signature already covered more exactly by EP-08, EP-10, EP-11, and EP-13. They appear in the exhaustive test plan but intentionally carry no `Policy:` backlink because treating redundant weaker assertions as additional contract owners would obscure the minimum evidence for each guarantee.

### Coverage audit boundary

EP-08 through EP-15 close the field-layout and line-ending gaps that the earlier smoke tests left open. The ACT and RIFF expected byte streams are constructed explicitly by their representative layout tests rather than emitted by libsixel and read back by libsixel. The positive JASC, RIFF, and GPL import tests compare canonical exported RGB entries against independently supplied input; the 256-entry RIFF round trip relies additionally on EP-11's independent size and count-field assertion. None of these tests treats a nonempty SIXEL stream as proof that the parser interpreted every field correctly. EP-16 through EP-19 separately own the four documented image-mapfile reconstruction effects rather than inferring them from successful PNG loading. EP-24 through EP-29 extend the same exact-oracle approach to BOM handling, grammar whitespace, the 256-color boundary, format selection, standard streams, and output errors. EP-30 through EP-35 cover repeated options, option conflicts, diffusion, output color-space conversion, and representative static and animated image mapfiles.

This table proves libsixel's emitted bytes and its accepted and rejected interpretation of representative input. It does not run Photoshop, PaintShop Pro, GIMP, Krita, Aseprite, or Windows palette consumers, so the application-compatibility column remains evidence from the cited external specifications and application documentation rather than an integration-test claim. The exhaustive test-plan inventory proves that every current mapfile test remains discoverable, but its presence alone does not promote future fault-injection or implementation-specific cases into this public behavioral table.

The fixed-palette snap and cover interactions are owned by the [snap-policy](snap-policy/README.md) and [cover-policy](cover-policy/README.md) documents. Their tests should remain linked there rather than being duplicated here solely because `-m`, `-b`, or `-e` participates in the scenario. The explicit lookup behavior for `-b` and `-e` is tested, but the lookup-policy document does not yet have an enforced coverage inventory, so those two tests currently have no reciprocal policy link.

## External format and compatibility references

- [Adobe Photoshop File Formats Specification: Color Table](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/) defines the 768-byte and 772-byte ACT layouts and the trailer fields.
- [GIMP and Standards: Palette File Formats](https://testing.developer.gimp.org/core/standards/) summarizes ACT, RIFF PAL, JASC PAL, and GPL origins and supported color models.
- [GIMP Palette Format Version 2](https://testing.developer.gimp.org/core/standards/gpl/) specifies the current GPL text structure and 8-bit sRGB interpretation.
- [Krita Palette Docker](https://docs.krita.org/en/reference_manual/dockers/palette_docker.html) lists GPL, Microsoft RIFF, Photoshop ACT, and PaintShop Pro palettes among its imports.
- [Microsoft PALETTEENTRY](https://learn.microsoft.com/en-us/previous-versions/dd162769(v=vs.85)) defines the red, green, blue, and flags fields used by a Windows logical-palette entry.
- [Microsoft Resource Interchange File Format Services](https://learn.microsoft.com/en-us/windows/win32/multimedia/resource-interchange-file-format-services) defines RIFF chunk identifiers, sizes, form types, and traversal.
- [Microsoft WMF Palette Object](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-wmf/3ff2d783-b934-4049-bba3-67fe157fb3d5) documents the `0x0300` logical-palette version, entry count, and palette-entry array used by the Simple PAL payload.
- [Corel's JASC Paint Shop Pro history](https://www.paintshoppro.com/en/pages/old-brands/jasc-paintshop-pro/) records the JASC origin and the 2004 transition to Corel PaintShop Pro.
- [Aseprite palette extensions](https://www.aseprite.org/docs/extensions/palettes/) documents use of GIMP `.gpl` palettes in Aseprite extensions.

The CIELAB export test uses black and white source pixels, retaining the authoritative float working palette. Export must recover `(0,0,0)` and `(255,255,255)` in sRGB8, with either solver entry order. This protects the neutral endpoints against a byte a/b midpoint bias; it does not freeze K-means representatives from a natural image. The [independent byte-projection reference](../../../tests/data/color-numeric/palette-export.json) describes the legacy storage loss and is not the oracle for this float path.

Replacing the typed byte palette boundary requires re-deriving this storage-specific expectation from independent values. The current byte artifacts are not an ideal black/white color contract for a future canonical float palette. Natural-image clustering and decoded-output quality remain covered by the existing working-space quality tests (WC-04 through WC-08 in [Working color space](../palette-application/working-colorspace/README.md#test-coverage)), including CIELAB with the unchanged MS-SSIM floor.
