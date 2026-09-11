# External Palette Input and Output

## Purpose

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

A palette optimized for one image is not automatically high quality for a collection. Build a reusable palette from representative material, then validate the actual images with [`lsqa`](../quality/measurement-policy.md) or another suitable quality check. Palette reuse removes palette-construction variability; it does not guarantee that the fixed colors cover every later image well.

## Importing a palette with `-m`

The basic form is:

```sh
img2sixel -m palette.gpl -o output.six input.png
```

The palette is fixed before the input pixels are mapped. Palette-construction controls such as the quantizer do not rebuild it, but palette-application controls still matter. In particular, an explicit [`--lookup-policy`](lookup-policy.md), the [`--diffusion`](dithering.md) method, and the [`-W` working color space](working-colorspace.md) apply when the image is mapped to the supplied colors.

### Image mapfiles

`-m` also accepts an image as the palette source:

```sh
img2sixel -m hardware-palette.png -o output.six input.png
```

PNG is a common choice, but this path is not specific to PNG. A filename that does not select ACT, PAL, or GPL is passed to the configured image loaders, so JPEG, GIF, BMP, PNM, and other enabled input formats can also serve as map images. The image is loaded as a single static palette source.

An image mapfile describes colors through its decoded pixels. libsixel converts those pixels to the selected working color space and constructs a palette of at most 256 colors from them. An indexed PNG is therefore convenient for a swatch image, but libsixel does not import its palette table as an ordered file-format palette: unused entries can disappear, duplicate entries can collapse, and entry indices are not preserved. If an image contains more than 256 colors, its pixel distribution also influences the reduction. Use ACT, PAL, or GPL when exact entry count and ordering are part of the interchange contract.

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

## Exporting a palette with `-M`

The basic form is:

```sh
img2sixel -p 64 --quantize-model=kmeans:seed=1 -M palette.gpl -o output.six input.png
```

`-M` writes the palette actually prepared for encoding. The export happens after the [`-U` output-color-space conversion](working-colorspace.md), so internal CIELAB, DIN99d, Oklab, or linear working values are not written directly as if they were ordinary RGB bytes. The default `-Ugamma` is the safest interchange choice for these legacy RGB palette formats.

The output format is selected by a prefix or by `.act`, `.pal`, or `.gpl`. A plain `.pal` output name means JASC text; use `pal-riff:PATH` for RIFF PAL. Standard output requires a type prefix, and the SIXEL stream must be directed elsewhere if both would otherwise use standard output:

```sh
img2sixel -M gpl:- -o output.six input.png > palette.gpl
```

Combining `-m` and `-M` can translate the prepared RGB entries into another palette format:

```sh
img2sixel -m pal-riff:legacy.riff -M gpl:editable.gpl \
    -o /dev/null input.png
```

This is not a metadata-preserving file converter. The command still performs an encode, applies the selected working and output color spaces, and recreates format-specific metadata; entry names, RIFF flags, and ACT transparency information are not carried through.

All four palette formats carry at most 256 8-bit RGB entries and no embedded ICC profile. They cannot preserve arbitrary float working-space coordinates, alpha semantics, rendering intent, or a source profile. Choose `-U` deliberately when the receiving program expects something other than the default gamma-encoded RGB convention. The exported entry count is the finalized palette count and can be smaller than the requested `-p` limit after palette optimization.

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
| `-U`, `--output-colorspace` | Converts the prepared palette for SIXEL serialization and for `-M` export. |

The same lookup, cover, and snap rules apply to the other fixed palettes selected by `-b` and `-e`: explicit lookup applies, explicit cover conflicts, and snap is accepted but bypassed. The owning policy documents define the normative details: [Lookup Policy](lookup-policy.md), [Palette Cover Policy](cover-policy.md), [Palette Snap Policy](snap-policy.md), and [Palette Working Color Space](working-colorspace.md).

## Historical use by sayaka on NetBSD/x68k

libsixel added `-m` in [March 2014](https://github.com/saitoha/libsixel/commit/7771fd9a962bf719d99123de1f52fddf88a5a555), followed three days later by [support for using color-map images](https://github.com/saitoha/libsixel/commit/9674865e09838c48f38d1ef7999444ecfb0164c5). The [initial PHP implementation of isaki68k/sayaka](https://github.com/isaki68k/sayaka/blob/6d3f96b6e8605abb95de548623a0ffa4b46a94ef/sayaka.php#L125-L150), committed in September 2014, invoked `img2sixel -m colormap8.png` or `colormap16.png` for its reduced-color modes. A [September 2015 x68k change](https://github.com/isaki68k/sayaka/commit/b297510e2edb75dd2b64294f0768dca9d019ceec) added `colormapx68k16.png` and passed it through `-m` for the X68000-specific 16-color mode. This is a concrete early example of the external-palette feature being used to fit image output to a restricted old-machine display environment.

The project then moved away from that dependency. A [July 2015 commit](https://github.com/isaki68k/sayaka/commit/f4d43a605f5d701ae6266f86ea506ec593ae43d8) introduced a `SixelConverter` that used GdkPixbuf and emitted SIXEL without `img2sixel`; the [old PHP implementation was retired](https://github.com/isaki68k/sayaka/commit/05f27b3050db69abdd6e42abd6e3565d317ded1e) in 2016. The present sayaka tree contains its own image reduction and SIXEL output code.

`-M` is not part of that early history. It was added in [November 2025](https://github.com/saitoha/libsixel/commit/74a8cda3f441145dc6601989eab9e1f9412f0107), turning the older one-way fixed-palette input into a palette-generation, inspection, editing, and reuse workflow.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The table contains tests that directly own user-visible contracts stated by this document. A test is not added merely because it exercises the same parser or option.

| ID | Contract | Owning behavior test |
| --- | --- | --- |
| EP-01 | A direct-color PNG can supply colors through `-m`. | [tests/quant/palette/usage/0050_palette_map_png_rgb.t](../../tests/quant/palette/usage/0050_palette_map_png_rgb.t) |
| EP-02 | Indexed PNG map images with 1-, 2-, 4-, and 8-bit indices are accepted. | [tests/quant/palette/usage/0051_palette_map_png_pal1.t](../../tests/quant/palette/usage/0051_palette_map_png_pal1.t), [tests/quant/palette/usage/0052_palette_map_png_pal2.t](../../tests/quant/palette/usage/0052_palette_map_png_pal2.t), [tests/quant/palette/usage/0053_palette_map_png_pal4.t](../../tests/quant/palette/usage/0053_palette_map_png_pal4.t), [tests/quant/palette/usage/0054_palette_map_png_pal8.t](../../tests/quant/palette/usage/0054_palette_map_png_pal8.t) |
| EP-03 | Palette-construction color-space and precision controls do not reinterpret a completed PNG mapfile palette. | [tests/quant/palette/usage/0122_mapfile_png_clustering_colorspace_stable.t](../../tests/quant/palette/usage/0122_mapfile_png_clustering_colorspace_stable.t), [tests/quant/palette/usage/0123_mapfile_png_precision_stable.t](../../tests/quant/palette/usage/0123_mapfile_png_precision_stable.t) |
| EP-04 | Working/output color-space conversion of PNG mapfiles is deterministic, including an embedded ICC profile. | [tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t](../../tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t), [tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t](../../tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t) |
| EP-05 | `-m` and `-M` can translate a prepared JASC PAL palette to GPL. | [tests/quant/palette/usage/0147_mapfile_pal_mapfile_output_gpl.t](../../tests/quant/palette/usage/0147_mapfile_pal_mapfile_output_gpl.t) |
| EP-06 | An explicit lookup policy reaches palette application with a palette mapfile. | [tests/quant/palette/usage/0191_mapfile_pal_explicit_lookup_policy_applied.t](../../tests/quant/palette/usage/0191_mapfile_pal_explicit_lookup_policy_applied.t) |
| EP-07 | ACT transparency metadata neither shifts entries nor causes a valid palette to be rejected. | [tests/quant/mapfile/0025_mapfile_import_act_accepts_transparency_255_count_2.t](../../tests/quant/mapfile/0025_mapfile_import_act_accepts_transparency_255_count_2.t), [tests/quant/mapfile/0045_mapfile_import_act_transparency_does_not_offset_palette.t](../../tests/quant/mapfile/0045_mapfile_import_act_transparency_does_not_offset_palette.t) |
| EP-08 | ACT export writes the exact 772-byte RGB table, zero padding, big-endian entry count, and zero transparency index. | [tests/quant/mapfile/0080_mapfile_export_act_exact_layout.t](../../tests/quant/mapfile/0080_mapfile_export_act_exact_layout.t) |
| EP-09 | A trailer-free 768-byte ACT imports as 256 ordered entries. | [tests/quant/mapfile/0081_mapfile_import_act_768_exact.t](../../tests/quant/mapfile/0081_mapfile_import_act_768_exact.t) |
| EP-10 | JASC PAL export writes the exact header, version, count, and unlabelled RGB rows. | [tests/quant/mapfile/0082_mapfile_export_jasc_exact_layout.t](../../tests/quant/mapfile/0082_mapfile_export_jasc_exact_layout.t) |
| EP-11 | RIFF PAL export writes the exact RIFF size, `PAL ` form, `data` payload, version, count, ordered RGB entries, and zero flags. | [tests/quant/mapfile/0086_mapfile_export_riff_exact_layout.t](../../tests/quant/mapfile/0086_mapfile_export_riff_exact_layout.t) |
| EP-12 | An independently constructed RIFF PAL imports ordered RGB entries and discards nonzero entry flags. | [tests/quant/mapfile/0087_mapfile_import_riff_exact_entries.t](../../tests/quant/mapfile/0087_mapfile_import_riff_exact_entries.t) |
| EP-13 | GPL export has the documented prologue and indexed rows, while GPL import retains ordered RGB but discards names and layout metadata. | [tests/quant/palette/usage/0146_builtin_palette_mapfile_output_gpl.t](../../tests/quant/palette/usage/0146_builtin_palette_mapfile_output_gpl.t), [tests/quant/mapfile/0088_mapfile_import_gpl_exact_entries.t](../../tests/quant/mapfile/0088_mapfile_import_gpl_exact_entries.t) |
| EP-14 | JASC PAL import accepts LF, CR, and CRLF as three independently checked line-ending forms. | [tests/quant/mapfile/0083_mapfile_import_jasc_lf.t](../../tests/quant/mapfile/0083_mapfile_import_jasc_lf.t), [tests/quant/mapfile/0084_mapfile_import_jasc_cr.t](../../tests/quant/mapfile/0084_mapfile_import_jasc_cr.t), [tests/quant/mapfile/0085_mapfile_import_jasc_crlf.t](../../tests/quant/mapfile/0085_mapfile_import_jasc_crlf.t) |
| EP-15 | GPL import accepts LF, CR, and CRLF as three independently checked line-ending forms. | [tests/quant/mapfile/0089_mapfile_import_gpl_lf.t](../../tests/quant/mapfile/0089_mapfile_import_gpl_lf.t), [tests/quant/mapfile/0090_mapfile_import_gpl_cr.t](../../tests/quant/mapfile/0090_mapfile_import_gpl_cr.t), [tests/quant/mapfile/0091_mapfile_import_gpl_crlf.t](../../tests/quant/mapfile/0091_mapfile_import_gpl_crlf.t) |
| EP-16 | An indexed PNG PLTE entry not referenced by any pixel does not appear in the constructed map palette. | [tests/quant/palette/usage/0198_mapfile_png_unused_plte_entry_ignored.t](../../tests/quant/palette/usage/0198_mapfile_png_unused_plte_entry_ignored.t) |
| EP-17 | Indexed-image source palette indices are not retained; output entries follow the palette reconstructed from decoded pixels. | [tests/quant/palette/usage/0199_mapfile_png_source_indices_not_preserved.t](../../tests/quant/palette/usage/0199_mapfile_png_source_indices_not_preserved.t) |
| EP-18 | Duplicate colors in decoded indexed-image pixels can collapse to one constructed map-palette entry. | [tests/quant/palette/usage/0200_mapfile_png_duplicate_colors_collapse.t](../../tests/quant/palette/usage/0200_mapfile_png_duplicate_colors_collapse.t) |
| EP-19 | For an image mapfile containing more than 256 colors, changing pixel frequencies while retaining the same color set can change the reduced palette. | [tests/quant/palette/usage/0201_mapfile_image_distribution_affects_reduction.t](../../tests/quant/palette/usage/0201_mapfile_image_distribution_affects_reduction.t) |
| EP-20 | Structured palette selection honors explicit prefixes, standard input, recognized extensions, case-insensitive prefix spelling, content signatures, and documented ambiguity rejection and prefix precedence. | [tests/quant/mapfile/0006_mapfile_import_gpl_prefixed.t](../../tests/quant/mapfile/0006_mapfile_import_gpl_prefixed.t), [tests/quant/mapfile/0007_mapfile_import_act_extension.t](../../tests/quant/mapfile/0007_mapfile_import_act_extension.t), [tests/quant/mapfile/0008_mapfile_import_riff_explicit.t](../../tests/quant/mapfile/0008_mapfile_import_riff_explicit.t), [tests/quant/mapfile/0009_mapfile_import_stdin.t](../../tests/quant/mapfile/0009_mapfile_import_stdin.t), [tests/quant/mapfile/0034_mapfile_import_rejects_extensionless_size_only_act.t](../../tests/quant/mapfile/0034_mapfile_import_rejects_extensionless_size_only_act.t), [tests/quant/mapfile/0035_mapfile_import_accepts_extensionless_act_with_prefix.t](../../tests/quant/mapfile/0035_mapfile_import_accepts_extensionless_act_with_prefix.t), [tests/quant/mapfile/0036_mapfile_import_prefers_jasc_signature_over_act_size.t](../../tests/quant/mapfile/0036_mapfile_import_prefers_jasc_signature_over_act_size.t), [tests/quant/mapfile/0044_mapfile_import_rejects_unknown_extensionless_format.t](../../tests/quant/mapfile/0044_mapfile_import_rejects_unknown_extensionless_format.t), [tests/quant/mapfile/0046_mapfile_import_rejects_pal_extension_with_act_payload.t](../../tests/quant/mapfile/0046_mapfile_import_rejects_pal_extension_with_act_payload.t), [tests/quant/mapfile/0047_mapfile_import_accepts_pal_jasc_prefix_over_extension.t](../../tests/quant/mapfile/0047_mapfile_import_accepts_pal_jasc_prefix_over_extension.t), [tests/quant/mapfile/0050_mapfile_import_accepts_uppercase_prefix.t](../../tests/quant/mapfile/0050_mapfile_import_accepts_uppercase_prefix.t), [tests/quant/mapfile/0051_mapfile_import_accepts_extensionless_riff_signature.t](../../tests/quant/mapfile/0051_mapfile_import_accepts_extensionless_riff_signature.t), [tests/quant/mapfile/0052_mapfile_import_accepts_pal_riff_prefix_over_extension.t](../../tests/quant/mapfile/0052_mapfile_import_accepts_pal_riff_prefix_over_extension.t) |
| EP-21 | File and standard-input palette streams accept exactly 16 MiB and reject the next byte. | [tests/quant/mapfile/0014_mapfile_import_rejects_input_over_16mib.t](../../tests/quant/mapfile/0014_mapfile_import_rejects_input_over_16mib.t), [tests/quant/mapfile/0015_mapfile_import_accepts_exactly_16mib.t](../../tests/quant/mapfile/0015_mapfile_import_accepts_exactly_16mib.t), [tests/quant/mapfile/0048_mapfile_import_stdin_rejects_input_over_16mib.t](../../tests/quant/mapfile/0048_mapfile_import_stdin_rejects_input_over_16mib.t), [tests/quant/mapfile/0049_mapfile_import_stdin_accepts_exactly_16mib.t](../../tests/quant/mapfile/0049_mapfile_import_stdin_accepts_exactly_16mib.t) |
| EP-22 | Missing files, directories, empty option values, prefix-only paths, and empty standard input are rejected. | [tests/quant/mapfile/0053_mapfile_import_rejects_missing_path.t](../../tests/quant/mapfile/0053_mapfile_import_rejects_missing_path.t), [tests/quant/mapfile/0054_mapfile_import_rejects_directory_path.t](../../tests/quant/mapfile/0054_mapfile_import_rejects_directory_path.t), [tests/quant/mapfile/0055_mapfile_import_rejects_empty_option.t](../../tests/quant/mapfile/0055_mapfile_import_rejects_empty_option.t), [tests/quant/mapfile/0056_mapfile_import_rejects_prefix_only.t](../../tests/quant/mapfile/0056_mapfile_import_rejects_prefix_only.t), [tests/quant/mapfile/0057_mapfile_import_rejects_empty_stdin.t](../../tests/quant/mapfile/0057_mapfile_import_rejects_empty_stdin.t) |
| EP-23 | ACT import implements zero-as-256 count compatibility, accepts valid count/transparency boundaries, and rejects oversized counts, truncation, and lengths other than 768 or 772 bytes. | [tests/quant/mapfile/0010_mapfile_import_act_rejects_count_over_256.t](../../tests/quant/mapfile/0010_mapfile_import_act_rejects_count_over_256.t), [tests/quant/mapfile/0016_mapfile_import_act_accepts_zero_color_count.t](../../tests/quant/mapfile/0016_mapfile_import_act_accepts_zero_color_count.t), [tests/quant/mapfile/0024_mapfile_import_act_accepts_transparency_255_count_1.t](../../tests/quant/mapfile/0024_mapfile_import_act_accepts_transparency_255_count_1.t), [tests/quant/mapfile/0026_mapfile_import_act_accepts_transparency_0_count_256.t](../../tests/quant/mapfile/0026_mapfile_import_act_accepts_transparency_0_count_256.t), [tests/quant/mapfile/0058_mapfile_import_act_rejects_truncated_palette.t](../../tests/quant/mapfile/0058_mapfile_import_act_rejects_truncated_palette.t), [tests/quant/mapfile/0059_mapfile_import_act_rejects_invalid_length.t](../../tests/quant/mapfile/0059_mapfile_import_act_rejects_invalid_length.t) |
| EP-24 | JASC PAL and GPL accept a UTF-8 BOM and reject embedded NUL and UTF-16/UTF-32 BOM input. | [tests/quant/mapfile/0032_mapfile_import_pal_rejects_embedded_nul.t](../../tests/quant/mapfile/0032_mapfile_import_pal_rejects_embedded_nul.t), [tests/quant/mapfile/0033_mapfile_import_gpl_rejects_embedded_nul.t](../../tests/quant/mapfile/0033_mapfile_import_gpl_rejects_embedded_nul.t), [tests/quant/mapfile/0039_mapfile_import_pal_rejects_non_utf8_bom.t](../../tests/quant/mapfile/0039_mapfile_import_pal_rejects_non_utf8_bom.t), [tests/quant/mapfile/0040_mapfile_import_gpl_rejects_non_utf8_bom.t](../../tests/quant/mapfile/0040_mapfile_import_gpl_rejects_non_utf8_bom.t), [tests/quant/mapfile/0042_mapfile_import_pal_accepts_utf8_bom.t](../../tests/quant/mapfile/0042_mapfile_import_pal_accepts_utf8_bom.t), [tests/quant/mapfile/0043_mapfile_import_gpl_accepts_utf8_bom.t](../../tests/quant/mapfile/0043_mapfile_import_gpl_accepts_utf8_bom.t) |
| EP-25 | JASC PAL requires the exact header and version, a count from 1 through 256, exactly that many rows, and exactly three decimal components from 0 through 255 per row. | [tests/quant/mapfile/0011_mapfile_import_pal_rejects_count_suffix.t](../../tests/quant/mapfile/0011_mapfile_import_pal_rejects_count_suffix.t), [tests/quant/mapfile/0012_mapfile_import_pal_rejects_component_suffix.t](../../tests/quant/mapfile/0012_mapfile_import_pal_rejects_component_suffix.t), [tests/quant/mapfile/0020_mapfile_import_pal_accepts_version_0100.t](../../tests/quant/mapfile/0020_mapfile_import_pal_accepts_version_0100.t), [tests/quant/mapfile/0021_mapfile_import_pal_rejects_version_0099.t](../../tests/quant/mapfile/0021_mapfile_import_pal_rejects_version_0099.t), [tests/quant/mapfile/0022_mapfile_import_pal_rejects_version_0200.t](../../tests/quant/mapfile/0022_mapfile_import_pal_rejects_version_0200.t), [tests/quant/mapfile/0023_mapfile_import_pal_rejects_version_suffix.t](../../tests/quant/mapfile/0023_mapfile_import_pal_rejects_version_suffix.t), [tests/quant/mapfile/0060_mapfile_import_pal_rejects_missing_header.t](../../tests/quant/mapfile/0060_mapfile_import_pal_rejects_missing_header.t), [tests/quant/mapfile/0061_mapfile_import_pal_rejects_incomplete_header.t](../../tests/quant/mapfile/0061_mapfile_import_pal_rejects_incomplete_header.t), [tests/quant/mapfile/0062_mapfile_import_pal_rejects_count_zero.t](../../tests/quant/mapfile/0062_mapfile_import_pal_rejects_count_zero.t), [tests/quant/mapfile/0063_mapfile_import_pal_rejects_count_over_256.t](../../tests/quant/mapfile/0063_mapfile_import_pal_rejects_count_over_256.t), [tests/quant/mapfile/0064_mapfile_import_pal_rejects_missing_component.t](../../tests/quant/mapfile/0064_mapfile_import_pal_rejects_missing_component.t), [tests/quant/mapfile/0065_mapfile_import_pal_rejects_negative_component.t](../../tests/quant/mapfile/0065_mapfile_import_pal_rejects_negative_component.t), [tests/quant/mapfile/0066_mapfile_import_pal_rejects_component_over_255.t](../../tests/quant/mapfile/0066_mapfile_import_pal_rejects_component_over_255.t), [tests/quant/mapfile/0067_mapfile_import_pal_rejects_excess_entries.t](../../tests/quant/mapfile/0067_mapfile_import_pal_rejects_excess_entries.t), [tests/quant/mapfile/0068_mapfile_import_pal_rejects_color_count_mismatch.t](../../tests/quant/mapfile/0068_mapfile_import_pal_rejects_color_count_mismatch.t) |
| EP-26 | RIFF PAL validates the container size, chunk traversal and padding, unique required `data` chunk, version `0x0300`, entry count from 1 through 256, and exact payload size. | [tests/quant/mapfile/0017_mapfile_import_riff_rejects_chunk_payload_overrun.t](../../tests/quant/mapfile/0017_mapfile_import_riff_rejects_chunk_payload_overrun.t), [tests/quant/mapfile/0018_mapfile_import_riff_rejects_missing_chunk_padding.t](../../tests/quant/mapfile/0018_mapfile_import_riff_rejects_missing_chunk_padding.t), [tests/quant/mapfile/0027_mapfile_import_riff_rejects_declared_size_mismatch.t](../../tests/quant/mapfile/0027_mapfile_import_riff_rejects_declared_size_mismatch.t), [tests/quant/mapfile/0028_mapfile_import_riff_rejects_missing_data_chunk.t](../../tests/quant/mapfile/0028_mapfile_import_riff_rejects_missing_data_chunk.t), [tests/quant/mapfile/0029_mapfile_import_riff_rejects_entry_count_zero.t](../../tests/quant/mapfile/0029_mapfile_import_riff_rejects_entry_count_zero.t), [tests/quant/mapfile/0030_mapfile_import_riff_rejects_entry_count_over_256.t](../../tests/quant/mapfile/0030_mapfile_import_riff_rejects_entry_count_over_256.t), [tests/quant/mapfile/0031_mapfile_import_riff_rejects_duplicate_data_chunk.t](../../tests/quant/mapfile/0031_mapfile_import_riff_rejects_duplicate_data_chunk.t), [tests/quant/mapfile/0037_mapfile_import_riff_rejects_invalid_version.t](../../tests/quant/mapfile/0037_mapfile_import_riff_rejects_invalid_version.t), [tests/quant/mapfile/0038_mapfile_import_riff_rejects_trailing_bytes.t](../../tests/quant/mapfile/0038_mapfile_import_riff_rejects_trailing_bytes.t), [tests/quant/mapfile/0069_mapfile_import_riff_rejects_truncated_palette.t](../../tests/quant/mapfile/0069_mapfile_import_riff_rejects_truncated_palette.t), [tests/quant/mapfile/0070_mapfile_import_riff_rejects_missing_header.t](../../tests/quant/mapfile/0070_mapfile_import_riff_rejects_missing_header.t), [tests/quant/mapfile/0071_mapfile_import_riff_rejects_data_chunk_too_small.t](../../tests/quant/mapfile/0071_mapfile_import_riff_rejects_data_chunk_too_small.t), [tests/quant/mapfile/0072_mapfile_import_riff_rejects_unexpected_chunk_size.t](../../tests/quant/mapfile/0072_mapfile_import_riff_rejects_unexpected_chunk_size.t) |
| EP-27 | GPL requires its header and at least one but no more than 256 rows, accepts trailing nonnumeric labels, and rejects invalid RGB components or a fourth numeric token. | [tests/quant/mapfile/0013_mapfile_import_gpl_rejects_component_suffix.t](../../tests/quant/mapfile/0013_mapfile_import_gpl_rejects_component_suffix.t), [tests/quant/mapfile/0019_mapfile_import_gpl_accepts_label_suffix.t](../../tests/quant/mapfile/0019_mapfile_import_gpl_accepts_label_suffix.t), [tests/quant/mapfile/0041_mapfile_import_gpl_rejects_extra_numeric_token.t](../../tests/quant/mapfile/0041_mapfile_import_gpl_rejects_extra_numeric_token.t), [tests/quant/mapfile/0073_mapfile_import_gpl_rejects_missing_header.t](../../tests/quant/mapfile/0073_mapfile_import_gpl_rejects_missing_header.t), [tests/quant/mapfile/0074_mapfile_import_gpl_rejects_metadata_only_header_missing.t](../../tests/quant/mapfile/0074_mapfile_import_gpl_rejects_metadata_only_header_missing.t), [tests/quant/mapfile/0075_mapfile_import_gpl_rejects_no_colors.t](../../tests/quant/mapfile/0075_mapfile_import_gpl_rejects_no_colors.t), [tests/quant/mapfile/0076_mapfile_import_gpl_rejects_too_many_colors.t](../../tests/quant/mapfile/0076_mapfile_import_gpl_rejects_too_many_colors.t), [tests/quant/mapfile/0077_mapfile_import_gpl_rejects_missing_component.t](../../tests/quant/mapfile/0077_mapfile_import_gpl_rejects_missing_component.t), [tests/quant/mapfile/0078_mapfile_import_gpl_rejects_negative_component.t](../../tests/quant/mapfile/0078_mapfile_import_gpl_rejects_negative_component.t), [tests/quant/mapfile/0079_mapfile_import_gpl_rejects_component_over_255.t](../../tests/quant/mapfile/0079_mapfile_import_gpl_rejects_component_over_255.t) |

Every test listed above links back to this document through its `Policy` comment. This keeps the behavioral contract discoverable in both directions when either the documentation or a regression test changes.

### Defensive, malformed-input, and supplementary tests

The [complete mapfile parser coverage inventory](../testing/mapfile-parser-coverage.md) links every test individually and records the exact observation from its opening comment. Because this document specifies the accepted grammar, size boundary, format-selection rules, and rejection conditions, the corresponding malformed-input tests are grouped into the stable public contracts EP-20 through EP-27 and carry both `Policy:` and `Test-plan:` backlinks.

The older writer tests `0001` through `0005` remain supplementary smoke and localization checks: they assert a size or leading signature already covered more exactly by EP-08, EP-10, EP-11, and EP-13. They appear in the exhaustive test plan but intentionally carry no `Policy:` backlink because treating redundant weaker assertions as additional contract owners would obscure the minimum evidence for each guarantee.

### Coverage audit boundary

EP-08 through EP-15 close the field-layout and line-ending gaps that the earlier smoke tests left open. The ACT and RIFF expected byte streams are constructed explicitly by their tests rather than emitted by libsixel and read back by libsixel. The positive JASC, RIFF, and GPL import tests likewise compare canonical exported RGB entries against independently supplied input instead of treating a nonempty SIXEL stream as proof that the parser interpreted every field correctly. EP-16 through EP-19 separately own the four documented image-mapfile reconstruction effects rather than inferring them from successful PNG loading.

This table proves libsixel's emitted bytes and its accepted and rejected interpretation of representative input. It does not run Photoshop, PaintShop Pro, GIMP, Krita, Aseprite, or Windows palette consumers, so the application-compatibility column remains evidence from the cited external specifications and application documentation rather than an integration-test claim. The exhaustive test-plan inventory proves that every current mapfile test remains discoverable, but its presence alone does not promote future fault-injection or implementation-specific cases into this public behavioral table.

The fixed-palette snap and cover interactions are owned by the [snap-policy](snap-policy.md) and [cover-policy](cover-policy.md) documents. Their tests should remain linked there rather than being duplicated here solely because `-m`, `-b`, or `-e` participates in the scenario. The explicit lookup behavior for `-b` and `-e` is tested, but the lookup-policy document does not yet have an enforced coverage inventory, so those two tests currently have no reciprocal policy link.

## Implementation

- [`src/mapfile.c`](../../src/mapfile.c) implements format detection, parsing, and writing for ACT, JASC PAL, RIFF PAL, and GPL.
- [`src/encoder.c`](../../src/encoder.c) decides whether a mapfile is a palette or image, prepares fixed palettes, applies option conflicts, captures the final palette, and emits `-M` output.
- [`tests/quant/mapfile/`](../../tests/quant/mapfile/) covers format parsing, detection, malformed inputs, standard input, and format writers.
- [`tests/quant/palette/usage/`](../../tests/quant/palette/usage/) covers image mapfiles, lookup and diffusion application, fixed-palette bypasses, color-space conversion, animation, and palette export.

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
