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

| ID | Contract | Regression coverage |
| --- | --- | --- |
| EP-01 | A direct-color PNG can supply colors through `-m`. | [tests/quant/palette/usage/0050_palette_map_png_rgb.t](../../tests/quant/palette/usage/0050_palette_map_png_rgb.t) |
| EP-02 | Indexed PNG map images with 1-, 2-, 4-, and 8-bit indices are accepted. | [tests/quant/palette/usage/0051_palette_map_png_pal1.t](../../tests/quant/palette/usage/0051_palette_map_png_pal1.t), [tests/quant/palette/usage/0052_palette_map_png_pal2.t](../../tests/quant/palette/usage/0052_palette_map_png_pal2.t), [tests/quant/palette/usage/0053_palette_map_png_pal4.t](../../tests/quant/palette/usage/0053_palette_map_png_pal4.t), [tests/quant/palette/usage/0054_palette_map_png_pal8.t](../../tests/quant/palette/usage/0054_palette_map_png_pal8.t) |
| EP-03 | Palette-construction color-space and precision controls do not reinterpret a completed PNG mapfile palette. | [tests/quant/palette/usage/0122_mapfile_png_clustering_colorspace_stable.t](../../tests/quant/palette/usage/0122_mapfile_png_clustering_colorspace_stable.t), [tests/quant/palette/usage/0123_mapfile_png_precision_stable.t](../../tests/quant/palette/usage/0123_mapfile_png_precision_stable.t) |
| EP-04 | Working/output color-space conversion of PNG mapfiles is deterministic, including an embedded ICC profile. | [tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t](../../tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t), [tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t](../../tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t) |
| EP-05 | `-m` and `-M` can translate a prepared JASC PAL palette to GPL. | [tests/quant/palette/usage/0147_mapfile_pal_mapfile_output_gpl.t](../../tests/quant/palette/usage/0147_mapfile_pal_mapfile_output_gpl.t) |
| EP-06 | An explicit lookup policy reaches palette application with a palette mapfile. | [tests/quant/palette/usage/0191_mapfile_pal_explicit_lookup_policy_applied.t](../../tests/quant/palette/usage/0191_mapfile_pal_explicit_lookup_policy_applied.t) |
| EP-07 | ACT transparency metadata neither shifts entries nor causes a valid palette to be rejected. | [tests/quant/mapfile/0025_mapfile_import_act_accepts_transparency_255_count_2.t](../../tests/quant/mapfile/0025_mapfile_import_act_accepts_transparency_255_count_2.t), [tests/quant/mapfile/0045_mapfile_import_act_transparency_does_not_offset_palette.t](../../tests/quant/mapfile/0045_mapfile_import_act_transparency_does_not_offset_palette.t) |

Every test listed above links back to this document through its `Policy` comment. This keeps the behavioral contract discoverable in both directions when either the documentation or a regression test changes.

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
- [Corel's JASC Paint Shop Pro history](https://www.paintshoppro.com/en/pages/old-brands/jasc-paintshop-pro/) records the JASC origin and the 2004 transition to Corel PaintShop Pro.
- [Aseprite palette extensions](https://www.aseprite.org/docs/extensions/palettes/) documents use of GIMP `.gpl` palettes in Aseprite extensions.
