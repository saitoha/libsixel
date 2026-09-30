# Built-in palettes and monochrome output

`img2sixel -b NAME` supplies a built-in palette instead of constructing one from the image. `-e` selects a different, monochrome SIXEL output mode; `-i` reverses that mode's assumed foreground/background polarity. Use a fixed palette when repeatable colors or a particular device palette matters more than adapting the palette to each image. For user-supplied palettes, see [external palette input and output](external-palettes.md).

## Choosing a built-in palette

| `-b` value | Entries | Contents and intended use |
| --- | ---: | --- |
| `xterm16` | 16 | The first 16 entries of libsixel's compiled xterm-style table: dark and bright RGB combinations. Useful for a restricted terminal-like color vocabulary. |
| `xterm256` | 256 | Those 16 entries, a 6×6×6 RGB cube with channel levels 0, 95, 135, 175, 215, 255, and 24 grayscale entries. |
| `vt340mono` | 16 | The compiled VT340 monochrome register map. It includes repeated grayscale values and is not the evenly spaced `gray4` ramp. |
| `vt340color` | 16 | The compiled VT340 color register map, including its device-oriented ordering and repeated entries. |
| `gray1` | 2 | Black and white. |
| `gray2` | 4 | Evenly spaced byte grays: 0, 85, 170, 255. |
| `gray4` | 16 | Evenly spaced byte grays in steps of 17. |
| `gray8` | 256 | Every 8-bit grayscale value. SIXEL's percentage-valued palette output can still collapse distinct byte values. |
| `oklch-lattice256` | 256 | A checked-in palette with 32 neutral levels and 16 hues × 7 lightness levels × 2 chroma fractions, designed using an OKLCh lattice. It supplies fixed RGB entries; selecting it does not switch the working color space to Oklab or rebuild a lattice from the input. |

These are compiled tables, not a query of the terminal's current theme. `xterm16` does not inherit a user's customized ANSI colors. The entry count describes the supplied table; it is not a promise that the image uses every entry, that every RGB triple is unique, or that every value remains distinct after SIXEL serialization.

```sh
# Apply a terminal-style palette, with and without error diffusion.
img2sixel -b xterm16 -d none image.png
img2sixel -b xterm16 -d fs image.png

# Make a four-level grayscale image and inspect the supplied palette.
img2sixel -b gray2 -M gpl:gray2.gpl -o image.six image.png
```

Choosing fewer available colors can reduce representation quality. Diffusion can recover an impression of intermediate tones through spatial mixtures, at the cost of texture and potentially more output bytes. A fixed palette is not automatically faster end to end: nearest-color lookup, diffusion, resize, and serialization still run.

## `-e` is a foreground mask, not simply `-b gray1`

`-e` prepares black then white and marks entry zero as the non-painted color. `-e -i` prepares white then black, again omitting entry zero. The monochrome wire path emits no palette-slot definitions or selections. The `-A` policy still chooses whether omitted pixels request clearing (`P2=0`) or preservation (`P2=1`); use `-Akeep` when the foreground mask must overlay existing content. The terminal's monochrome/current foreground behavior therefore matters; this mode does not install a portable black/white palette in the receiver.

| Command | Mapping assumption | Wire behavior |
| --- | --- | --- |
| `-b gray1` | Two available colors, black and white. | Ordinary indexed SIXEL with palette commands; neither entry is made transparent merely by selecting this palette. |
| `-e` | Dark background, light foreground. | Paint the foreground mask and omit the background side. |
| `-e -i` | Light background, dark foreground. | Reverse the mapping polarity and paint the resulting foreground mask. |

`-i` is meaningful for `-e`; it is not a general image-negative operation. It does not invert arbitrary RGB input or a `-b` palette. `-B` and `-A` control image background/alpha processing before encoding, while the monochrome non-painted entry is a separate encoder decision. See [alpha policy](../../loader/alpha-policy/README.md) for the terminal's role in preserving unpainted positions.

```sh
img2sixel -e -Akeep -d fs image.png
img2sixel -e -i -Akeep -d fs image.png
```

## Which pipeline controls still act

The fixed palette bypasses sampling, histogram/binning, palette solving, merge, snap, and cover construction. It still feeds palette application and wire generation. This distinction explains why a construction option can have no effect while changing lookup or diffusion changes the picture.

| Control | Relationship |
| --- | --- |
| `-b`, `-m`, `-e`, `-I` | Mutually exclusive output/palette selectors. Repeated `-b` is allowed and the last palette wins; repeated `-e` is allowed. |
| `-p` | Requests a generated-palette size; it cannot resize a built-in table and conflicts with fixed-palette selection. |
| `-Q`, `-4`, `-5`, `-F`, `-X` | Construction controls are bypassed. A fixed palette is not reclustered in the requested clustering space. |
| `-a` | An explicit cover policy conflicts with a fixed palette, including `-a off`. |
| `-_` | Accepted, but the fixed-palette path bypasses palette-construction snap processing. |
| `-~`, `-d` | Explicit lookup and diffusion continue to act during pixel assignment. Automatic lookup recognizes a black/white palette and can use its specialized monochrome threshold path. |
| `-W`, `-.`, `-U`, `-M` | Representation, application, serialization, and export are separate boundaries. Do not infer all four from the construction bypass. The current fixed-palette preparation/export behavior and its tests are described in [output color space](../output/output-colorspace.md#fixed-palettes-and-palette-export). |
| Crop, resize, loader CMS, `-A`, `-B` | Still affect the image being mapped. A fixed output palette does not suppress input color management or alpha processing. |

For exact palette interchange, inspect `-M` output rather than assuming the terminal receives the original byte values. For exact terminal behavior, inspect the SIXEL stream as well: monochrome mode and percentage rounding are not represented solely by the exported palette file.

## Native float application and Display P3

The [selected opaque static CPU path](../output/output-colorspace.md#selected-ordinary-srgb-float-path) accepts built-in `-b` tables for gamma, linear, SMPTE-C and Display P3 output. The table remains sRGB8: import converts its entries once to the requested gamma, linear, Oklab, CIELAB or DIN99d working coordinates. Lookup and diffusion use these unrounded float values. A separate output copy is converted to the destination definition, clamped and rounded to RGB101. The receiver must interpret that output definition; selecting P3 does not reinterpret the compiled table as P3 primaries.

Table order, duplicate colors and unused entries are retained. `-M` exports the supplied sRGB8 values before wire conversion. A source or operation that needs float selects native application; an otherwise byte-eligible ordinary gamma job keeps its byte path. Changing the output, source precision or built-in table on one encoder recomputes the corresponding working/wire state. Palette construction is still bypassed, and an explicit cover option still conflicts with `-b`.

The other selected-path limits still apply. Snap must be disabled or have zero rate; ordinary output with active snap retains its existing accepted no-op path, and Display P3 with active snap is rejected before output. Alpha/animation, GPU, nonstandard output modes and the distinct `-e` foreground-mask mode are outside this extension. A native gray1 palette emits ordinary black/white color definitions and maps pixels using the configured working geometry; it does not acquire the receiver-dependent foreground-mask semantics of `-e`.

```sh
img2sixel --precision=float32 -b xterm256 -W oklab -d fs image.png
img2sixel -b xterm16 -U display-p3 -d fs image.png
```

## Implementation

Palette tables and `sixel_dither_get()` are in [`dither.c`](../../../src/dither.c), with the lattice in [`oklch-lattice256.h`](../../../src/oklch-lattice256.h). [`encoder.c`](../../../src/encoder.c) owns selector conflicts, fixed-palette preparation, and option application.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation | Owning test |
| --- | --- | --- |
| FP-01 | Monochrome with the tested `-Acomposite` setting requests `P2=1` and emits no palette commands. | [tests/quant/palette/usage/0165_monochrome_sixel_mode_no_palette_slots.t](../../../tests/quant/palette/usage/0165_monochrome_sixel_mode_no_palette_slots.t) |
| FP-02 | Explicit lookup reaches the matching byte or float32 built-in-palette application path. | [tests/quant/palette/usage/0192_builtin_explicit_lookup_policy_applied.t](../../../tests/quant/palette/usage/0192_builtin_explicit_lookup_policy_applied.t) |
| FP-03 | Explicit diffusion changes fixed-palette output. | [tests/quant/palette/usage/0141_builtin_palette_explicit_dither_changes_output.t](../../../tests/quant/palette/usage/0141_builtin_palette_explicit_dither_changes_output.t) |
| FP-04 | Explicit snap policies are bypassed for a built-in palette. | [tests/quant/palette/usage/0195_builtin_snap_policy_bypassed.t](../../../tests/quant/palette/usage/0195_builtin_snap_policy_bypassed.t) |

### Native built-in palette contracts

The typed-entry and wire fixtures come from independent Decimal transfer/matrix equations in `tools/generate_builtin_native_fixtures.py`, starting from the documented sRGB8 tables stored as binary32 fractions. Working entries allow `2e-6 + 5e-6 * abs(expected)` error for intermediate conversion/storage; wire assertions use fixtures separated from rounding ties. The SMPTE-C witness uses gray2 and the existing implementation matrix as its defined contract, not an independently calibrated display. Duplicate VT340 entries retain their slots but may tie during lookup. These value tests establish the named geometry, state and consumer boundaries; they are separate from photographic quality measurements and do not establish native alpha, animation or GPU support.

| ID | Observation | Owning test |
| --- | --- | --- |
| FP-05 | Retain all xterm16 entries in gamma, verify independent sRGB wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0001_srgb_gamma.t](../../../tests/quant/palette/builtin_float/0001_srgb_gamma.t) |
| FP-06 | Retain all xterm16 entries in linear, verify independent sRGB wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0002_srgb_linear.t](../../../tests/quant/palette/builtin_float/0002_srgb_linear.t) |
| FP-07 | Retain all xterm16 entries in Oklab, verify independent sRGB wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0003_srgb_oklab.t](../../../tests/quant/palette/builtin_float/0003_srgb_oklab.t) |
| FP-08 | Retain all xterm16 entries in CIELAB, verify independent sRGB wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0004_srgb_cielab.t](../../../tests/quant/palette/builtin_float/0004_srgb_cielab.t) |
| FP-09 | Retain all xterm16 entries in DIN99d, verify independent sRGB wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0005_srgb_din99d.t](../../../tests/quant/palette/builtin_float/0005_srgb_din99d.t) |
| FP-10 | Retain all xterm16 entries in gamma, verify independent P3 wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0006_p3_gamma.t](../../../tests/quant/palette/builtin_float/0006_p3_gamma.t) |
| FP-11 | Retain all xterm16 entries in linear, verify independent P3 wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0007_p3_linear.t](../../../tests/quant/palette/builtin_float/0007_p3_linear.t) |
| FP-12 | Retain all xterm16 entries in Oklab, verify independent P3 wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0008_p3_oklab.t](../../../tests/quant/palette/builtin_float/0008_p3_oklab.t) |
| FP-13 | Retain all xterm16 entries in CIELAB, verify independent P3 wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0009_p3_cielab.t](../../../tests/quant/palette/builtin_float/0009_p3_cielab.t) |
| FP-14 | Retain all xterm16 entries in DIN99d, verify independent P3 wire values and applied red/gray indices. | [tests/quant/palette/builtin_float/0010_p3_din99d.t](../../../tests/quant/palette/builtin_float/0010_p3_din99d.t) |
| FP-15 | Keep native xterm16 linear coordinates and independently derived linear wire codes. | [tests/quant/palette/builtin_float/0011_linear_output.t](../../../tests/quant/palette/builtin_float/0011_linear_output.t) |
| FP-16 | Keep native gray2 coordinates and wire codes from the existing SMPTE-C matrix/curve. | [tests/quant/palette/builtin_float/0012_smptec_output.t](../../../tests/quant/palette/builtin_float/0012_smptec_output.t) |
| FP-17 | Apply nonzero FS feedback with gray2, changing the second pixel from gray to black. | [tests/quant/palette/builtin_float/0013_gray2_fs.t](../../../tests/quant/palette/builtin_float/0013_gray2_fs.t) |
| FP-18 | Keep the same gray2 FS selection with P3 output. | [tests/quant/palette/builtin_float/0014_gray2_p3_fs.t](../../../tests/quant/palette/builtin_float/0014_gray2_p3_fs.t) |
| FP-19 | Keep all VT340 monochrome entries, including duplicates and unused slots, through linear application. | [tests/quant/palette/builtin_float/0015_vt340_duplicates.t](../../../tests/quant/palette/builtin_float/0015_vt340_duplicates.t) |
| FP-20 | Retain the legacy byte path for an otherwise byte-eligible xterm16 job. | [tests/quant/palette/builtin_float/0016_byte_eligible.t](../../../tests/quant/palette/builtin_float/0016_byte_eligible.t) |
| FP-21 | Honor explicit float application for byte xterm16 input. | [tests/quant/palette/builtin_float/0017_explicit_float.t](../../../tests/quant/palette/builtin_float/0017_explicit_float.t) |
| FP-22 | Treat zero snap rate as inactive for built-in P3 application. | [tests/quant/palette/builtin_float/0018_zero_snap.t](../../../tests/quant/palette/builtin_float/0018_zero_snap.t) |
| FP-23 | Recompute native/byte/native application when reusing an encoder with different source precision. | [tests/quant/palette/builtin_float/0019_precision_reuse.t](../../../tests/quant/palette/builtin_float/0019_precision_reuse.t) |
| FP-24 | Restore byte gamma application between two native P3 jobs on the same encoder. | [tests/quant/palette/builtin_float/0020_output_reuse.t](../../../tests/quant/palette/builtin_float/0020_output_reuse.t) |
| FP-25 | Rebuild working entries and wire values across xterm16, gray2 and VT340 table changes. | [tests/quant/palette/builtin_float/0021_table_reuse.t](../../../tests/quant/palette/builtin_float/0021_table_reuse.t) |
| FP-26 | Reject P3 with active snap before output and recover after disabling snap on the same encoder. | [tests/quant/palette/builtin_float/0022_p3_snap_recovery.t](../../../tests/quant/palette/builtin_float/0022_p3_snap_recovery.t) |
| FP-27 | Keep excluded multiframe jobs on their existing path, then restore native static application. | [tests/quant/palette/builtin_float/0023_multiframe_boundary.t](../../../tests/quant/palette/builtin_float/0023_multiframe_boundary.t) |
| FP-28 | Preserve the exact supplied sRGB8 table in the captured export before P3 wire conversion. | [tests/quant/palette/builtin_float/0024_p3_export.t](../../../tests/quant/palette/builtin_float/0024_p3_export.t) |
| FP-29 | Preserve a source above one and propagate its FS residual across the gray1 decision boundary. | [tests/quant/palette/builtin_float/0025_positive_fs.t](../../../tests/quant/palette/builtin_float/0025_positive_fs.t) |
| FP-30 | Preserve a negative source and propagate its FS residual across the gray1 decision boundary. | [tests/quant/palette/builtin_float/0026_negative_fs.t](../../../tests/quant/palette/builtin_float/0026_negative_fs.t) |
| FP-31 | Define independently expected P3 red/green colors through the CLI with default palette type and no explicit cover option. | [tests/quant/palette/builtin_float/0027_p3_cli.t](../../../tests/quant/palette/builtin_float/0027_p3_cli.t) |
| FP-32 | Export the exact xterm16 sRGB8 table after CIELAB application and P3 wire preparation. | [tests/quant/palette/builtin_float/0028_p3_export_cielab.t](../../../tests/quant/palette/builtin_float/0028_p3_export_cielab.t) |

### Coverage boundary

The [palette usage suite](../../../tests/quant/palette/usage/) also contains exported-entry, colorspace, animation, and perceptual-quality checks. The [option-conflict suite](../../../tests/cli/options/invalid/) exercises incompatible selectors. These categories do not establish how a particular terminal chooses its monochrome foreground, or that a fixed palette improves quality for every image. The table above records exact output/path observations; quality thresholds elsewhere are separate evidence.
