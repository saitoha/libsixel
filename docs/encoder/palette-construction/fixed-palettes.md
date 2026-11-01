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

`-e` and `-e -i` conflict with `-Z delta`; preserving omitted pixels cannot restore the background over an earlier foreground pixel. For opaque black-and-white delta updates, use `-b gray1`. See [delta interactions](../output/delta-encoding.md#execution-paths-and-interactions) for the rejection, policy-selection and retained-surface contracts.

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

The [selected CPU path](../output/output-colorspace.md#selected-ordinary-srgb-float-path) accepts built-in `-b` tables for gamma, linear, SMPTE-C and Display P3 output. The table remains sRGB8: import converts its entries once to the requested gamma, linear, Oklab, CIELAB or DIN99d working coordinates. Lookup and diffusion use these unrounded float values. A separate output copy is converted to the destination definition, clamped and rounded to RGB101. The receiver must interpret that output definition; selecting P3 does not reinterpret the compiled table as P3 primaries.

Table order, duplicate colors and unused entries are retained. `-M` exports the supplied sRGB8 values before wire conversion. A source or operation that needs float selects native application; an otherwise byte-eligible ordinary gamma job keeps its byte path. Changing the output, source precision or built-in table on one encoder recomputes the corresponding working/wire state. Palette construction is still bypassed, and an explicit cover option still conflicts with `-b`.

The other selected-path limits still apply. Snap settings are accepted and bypassed for fixed palettes, including Display P3 output; they do not alter the supplied table. Complete separate alpha-zero masks are supported under the [shared mask contract](../output/output-colorspace.md#separate-alpha-zero-masks); [packed inputs are normalized first](../output/output-colorspace.md#packed-alpha-normalization). [Animation](../output/output-colorspace.md#fixed-palettes-in-animation) retains the fixed source and application cache. Native sRGB/gamma [delta output](../output/delta-encoding.md#fixed-table-omission-and-ownership) preserves every table entry through a separate omission mask. Other output combinations follow the linked output contracts; GPU retains its separate boundary. The distinct `-e` foreground-mask path is described below. A native gray1 palette emits ordinary black/white color definitions and maps pixels using the configured working geometry; it does not acquire the receiver-dependent foreground-mask semantics of `-e`.

```sh
img2sixel --precision-policy=float32 -b xterm256 -W oklab -d fs image.png
img2sixel -b xterm16 -U display-p3 -d fs image.png
```

## Native monochrome application

The selected CPU float path also prepares `-e` and `-e -i` endpoints in the requested gamma, linear, Oklab, CIELAB or DIN99d working coordinates. Black and white start as sRGB colors; opponent-space black is not an all-zero encoded byte triple, and opponent-space white is not an all-255 triple. Retained float coordinates feed lookup and spatial/positional diffusion without reconstruction through working-space bytes. A matching typed float source retains negative and above-one samples through application, subject to the separately selected [diffusion clamp policy](../palette-application/clamp-policy.md).

The foreground-mask convention is unchanged: entry zero is a mapping candidate and is omitted, entry one paints the receiver's foreground, and neither palette definitions nor color selections are sent. `-i` reverses the assumed black/white endpoints; it does not change which index is omitted. `-A` still controls the DCS clear/keep request. A completely omitted or completely painted image retains the same convention. This differs from an appended alpha key, which is excluded from nearest-color selection.

Gamma, linear, SMPTE-C and Display P3 output definitions are accepted on this selected path, but none can set the actual foreground color of a palette-free mask. Changing only `-U` does not redefine the working black/white endpoints or the sRGB8 `-M` export. Use ordinary indexed output when the receiver must receive explicit RGB colors. This mode remains unsuitable for verifying physical wide-gamut display output by itself.

Genuinely byte-eligible gamma jobs keep their established path. Explicit float requests and already-float sources select native preparation under the same selected-path limits as built-in tables. Reusing an encoder re-evaluates precision, output definition and inversion. Automatic lookup retains its existing selector; explicit lookup remains explicit. Complete separate source masks add their holes to the foreground omission under the [shared mask contract](../output/output-colorspace.md#separate-alpha-zero-masks). [Packed inputs are normalized first](../output/output-colorspace.md#packed-alpha-normalization); [Animation retains an independent fixed source](../output/output-colorspace.md#fixed-palettes-in-animation); GPU and other excluded modes retain their previous boundary. Fixed-palette snap remains an accepted no-op, including active requests with P3 output.

## Implementation

Palette tables and `sixel_dither_get()` are in [`dither.c`](../../../src/dither.c), with the lattice in [`oklch-lattice256.h`](../../../src/oklch-lattice256.h). [`encoder.c`](../../../src/encoder.c) owns selector conflicts, fixed-palette preparation, and option application.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation | Owning test |
| --- | --- | --- |
| FP-01 | Monochrome with the tested `-Acomposite` setting requests `P2=1` and emits no palette commands. | [tests/palette/fixed/0100_monochrome_sixel_mode_no_palette_slots.t](../../../tests/palette/fixed/0100_monochrome_sixel_mode_no_palette_slots.t) |
| FP-02 | Explicit lookup reaches the matching byte or float32 built-in-palette application path. | [tests/lookup/dispatch/0027_builtin_explicit_lookup_policy_applied.t](../../../tests/lookup/dispatch/0027_builtin_explicit_lookup_policy_applied.t) |
| FP-03 | Explicit diffusion changes fixed-palette output. | [tests/palette/fixed/0098_builtin_palette_explicit_dither_changes_output.t](../../../tests/palette/fixed/0098_builtin_palette_explicit_dither_changes_output.t) |
| FP-04 | Explicit snap policies are bypassed for a built-in palette. | [tests/palette/fixed/0101_builtin_snap_policy_bypassed.t](../../../tests/palette/fixed/0101_builtin_snap_policy_bypassed.t) |

### Native built-in palette contracts

The typed-entry and wire fixtures come from independent Decimal transfer/matrix equations in `tools/generate_builtin_native_fixtures.py`, starting from the documented sRGB8 tables stored as binary32 fractions. Working entries allow `2e-6 + 5e-6 * abs(expected)` error for intermediate conversion/storage; wire assertions use fixtures separated from rounding ties. The SMPTE-C witness uses gray2 and the existing implementation matrix as its defined contract, not an independently calibrated display. Duplicate VT340 entries retain their slots but may tie during lookup. These value tests establish the named geometry, state and consumer boundaries; they are separate from photographic quality measurements and do not establish native alpha, animation or GPU support.

| ID | Observation | Owning test |
| --- | --- | --- |
| FP-05 | Retain all xterm16 entries in gamma, verify independent sRGB wire values and applied red/gray indices. | [tests/palette/fixed/0001_quant_palette_builtin_float_srgb_gamma.t](../../../tests/palette/fixed/0001_quant_palette_builtin_float_srgb_gamma.t) |
| FP-06 | Retain all xterm16 entries in linear, verify independent sRGB wire values and applied red/gray indices. | [tests/palette/fixed/0002_quant_palette_builtin_float_srgb_linear.t](../../../tests/palette/fixed/0002_quant_palette_builtin_float_srgb_linear.t) |
| FP-07 | Retain all xterm16 entries in Oklab, verify independent sRGB wire values and applied red/gray indices. | [tests/palette/fixed/0003_quant_palette_builtin_float_srgb_oklab.t](../../../tests/palette/fixed/0003_quant_palette_builtin_float_srgb_oklab.t) |
| FP-08 | Retain all xterm16 entries in CIELAB, verify independent sRGB wire values and applied red/gray indices. | [tests/palette/fixed/0004_quant_palette_builtin_float_srgb_cielab.t](../../../tests/palette/fixed/0004_quant_palette_builtin_float_srgb_cielab.t) |
| FP-09 | Retain all xterm16 entries in DIN99d, verify independent sRGB wire values and applied red/gray indices. | [tests/palette/fixed/0005_quant_palette_builtin_float_srgb_din99d.t](../../../tests/palette/fixed/0005_quant_palette_builtin_float_srgb_din99d.t) |
| FP-10 | Retain all xterm16 entries in gamma, verify independent P3 wire values and applied red/gray indices. | [tests/palette/fixed/0006_quant_palette_builtin_float_p3_gamma.t](../../../tests/palette/fixed/0006_quant_palette_builtin_float_p3_gamma.t) |
| FP-11 | Retain all xterm16 entries in linear, verify independent P3 wire values and applied red/gray indices. | [tests/palette/fixed/0007_quant_palette_builtin_float_p3_linear.t](../../../tests/palette/fixed/0007_quant_palette_builtin_float_p3_linear.t) |
| FP-12 | Retain all xterm16 entries in Oklab, verify independent P3 wire values and applied red/gray indices. | [tests/palette/fixed/0008_quant_palette_builtin_float_p3_oklab.t](../../../tests/palette/fixed/0008_quant_palette_builtin_float_p3_oklab.t) |
| FP-13 | Retain all xterm16 entries in CIELAB, verify independent P3 wire values and applied red/gray indices. | [tests/palette/fixed/0009_quant_palette_builtin_float_p3_cielab.t](../../../tests/palette/fixed/0009_quant_palette_builtin_float_p3_cielab.t) |
| FP-14 | Retain all xterm16 entries in DIN99d, verify independent P3 wire values and applied red/gray indices. | [tests/palette/fixed/0010_quant_palette_builtin_float_p3_din99d.t](../../../tests/palette/fixed/0010_quant_palette_builtin_float_p3_din99d.t) |
| FP-15 | Keep native xterm16 linear coordinates and independently derived linear wire codes. | [tests/palette/fixed/0011_quant_palette_builtin_float_linear_output.t](../../../tests/palette/fixed/0011_quant_palette_builtin_float_linear_output.t) |
| FP-16 | Keep native gray2 coordinates and wire codes from the existing SMPTE-C matrix/curve. | [tests/palette/fixed/0012_quant_palette_builtin_float_smptec_output.t](../../../tests/palette/fixed/0012_quant_palette_builtin_float_smptec_output.t) |
| FP-17 | Apply nonzero FS feedback with gray2, changing the second pixel from gray to black. | [tests/palette/fixed/0013_gray2_fs.t](../../../tests/palette/fixed/0013_gray2_fs.t) |
| FP-18 | Keep the same gray2 FS selection with P3 output. | [tests/palette/fixed/0014_gray2_p3_fs.t](../../../tests/palette/fixed/0014_gray2_p3_fs.t) |
| FP-19 | Keep all VT340 monochrome entries, including duplicates and unused slots, through linear application. | [tests/palette/fixed/0015_vt340_duplicates.t](../../../tests/palette/fixed/0015_vt340_duplicates.t) |
| FP-20 | Retain the legacy byte path for an otherwise byte-eligible xterm16 job. | [tests/palette/fixed/0016_quant_palette_builtin_float_byte_eligible.t](../../../tests/palette/fixed/0016_quant_palette_builtin_float_byte_eligible.t) |
| FP-21 | Honor explicit float application for byte xterm16 input. | [tests/palette/fixed/0017_quant_palette_builtin_float_explicit_float.t](../../../tests/palette/fixed/0017_quant_palette_builtin_float_explicit_float.t) |
| FP-22 | Treat zero snap rate as inactive for built-in P3 application. | [tests/palette/fixed/0018_zero_snap.t](../../../tests/palette/fixed/0018_zero_snap.t) |
| FP-23 | Recompute native/byte/native application when reusing an encoder with different source precision. | [tests/palette/fixed/0019_quant_palette_builtin_float_precision_reuse.t](../../../tests/palette/fixed/0019_quant_palette_builtin_float_precision_reuse.t) |
| FP-24 | Restore byte gamma application between two native P3 jobs on the same encoder. | [tests/palette/fixed/0020_quant_palette_builtin_float_output_reuse.t](../../../tests/palette/fixed/0020_quant_palette_builtin_float_output_reuse.t) |
| FP-25 | Rebuild working entries and wire values across xterm16, gray2 and VT340 table changes. | [tests/palette/fixed/0021_table_reuse.t](../../../tests/palette/fixed/0021_table_reuse.t) |
| FP-26 | Accept P3 snap without changing fixed table values or painted indices, then disable it on the same encoder. | [tests/palette/fixed/0022_quant_palette_builtin_float_p3_snap_bypass.t](../../../tests/palette/fixed/0022_quant_palette_builtin_float_p3_snap_bypass.t) |
| FP-27 | Preserve native entries for multiframe and subsequent static application. | [tests/palette/fixed/0023_quant_palette_builtin_float_multiframe_native.t](../../../tests/palette/fixed/0023_quant_palette_builtin_float_multiframe_native.t) |
| FP-28 | Preserve the exact supplied sRGB8 table in the captured export before P3 wire conversion. | [tests/palette/fixed/0024_p3_export.t](../../../tests/palette/fixed/0024_p3_export.t) |
| FP-29 | Preserve a source above one and propagate its FS residual across the gray1 decision boundary. | [tests/palette/fixed/0025_quant_palette_builtin_float_positive_fs.t](../../../tests/palette/fixed/0025_quant_palette_builtin_float_positive_fs.t) |
| FP-30 | Preserve a negative source and propagate its FS residual across the gray1 decision boundary. | [tests/palette/fixed/0026_quant_palette_builtin_float_negative_fs.t](../../../tests/palette/fixed/0026_quant_palette_builtin_float_negative_fs.t) |
| FP-31 | Define independently expected P3 red/green colors through the CLI with default palette type and no explicit cover option. | [tests/palette/fixed/0027_p3_cli.t](../../../tests/palette/fixed/0027_p3_cli.t) |
| FP-32 | Export the exact xterm16 sRGB8 table after CIELAB application and P3 wire preparation. | [tests/palette/fixed/0028_p3_export_cielab.t](../../../tests/palette/fixed/0028_p3_export_cielab.t) |

### Native monochrome contracts

The tests reuse the independently derived gray1 endpoints from the built-in fixture above, with the same conversion/storage error budget. Interior samples lie at one quarter and three quarters between those endpoints, away from nearest-color ties. FS witnesses state their two-pixel arithmetic in the common probe; omitted feedback changes the next mask bit, and premature source clipping changes the extended-input witnesses. A small independent parser checks painted bits and the DCS request without assuming the decoder's default foreground palette. These assertions establish coordinate, index and wire behavior; they do not establish what color a particular terminal paints.

| ID | Observation | Owning test |
| --- | --- | --- |
| FM-01 | Retain typed black/white endpoints and independently expected foreground bits in gamma. | [tests/palette/fixed/0059_gamma.t](../../../tests/palette/fixed/0059_gamma.t) |
| FM-02 | Retain typed black/white endpoints and independently expected foreground bits in linear. | [tests/palette/fixed/0060_linear.t](../../../tests/palette/fixed/0060_linear.t) |
| FM-03 | Retain typed black/white endpoints and independently expected foreground bits in Oklab. | [tests/palette/fixed/0061_oklab.t](../../../tests/palette/fixed/0061_oklab.t) |
| FM-04 | Retain typed black/white endpoints and independently expected foreground bits in CIELAB. | [tests/palette/fixed/0062_cielab.t](../../../tests/palette/fixed/0062_cielab.t) |
| FM-05 | Retain typed black/white endpoints and independently expected foreground bits in DIN99d. | [tests/palette/fixed/0063_din99d.t](../../../tests/palette/fixed/0063_din99d.t) |
| FM-06 | Reverse endpoint order and foreground bits in gamma. | [tests/palette/fixed/0064_inverted_gamma.t](../../../tests/palette/fixed/0064_inverted_gamma.t) |
| FM-07 | Reverse endpoint order and foreground bits in linear. | [tests/palette/fixed/0065_inverted_linear.t](../../../tests/palette/fixed/0065_inverted_linear.t) |
| FM-08 | Reverse endpoint order and foreground bits in Oklab. | [tests/palette/fixed/0066_inverted_oklab.t](../../../tests/palette/fixed/0066_inverted_oklab.t) |
| FM-09 | Reverse endpoint order and foreground bits in CIELAB. | [tests/palette/fixed/0067_inverted_cielab.t](../../../tests/palette/fixed/0067_inverted_cielab.t) |
| FM-10 | Reverse endpoint order and foreground bits in DIN99d. | [tests/palette/fixed/0068_inverted_din99d.t](../../../tests/palette/fixed/0068_inverted_din99d.t) |
| FM-11 | Propagate nonzero FS feedback into the next foreground decision. | [tests/palette/fixed/0069_quant_palette_mono_float_fs_feedback.t](../../../tests/palette/fixed/0069_quant_palette_mono_float_fs_feedback.t) |
| FM-12 | Propagate FS feedback with inverted mapping polarity. | [tests/palette/fixed/0070_inverted_fs.t](../../../tests/palette/fixed/0070_inverted_fs.t) |
| FM-13 | Retain above-one input and its FS residual across the next decision boundary. | [tests/palette/fixed/0071_quant_palette_mono_float_positive_fs.t](../../../tests/palette/fixed/0071_quant_palette_mono_float_positive_fs.t) |
| FM-14 | Retain negative input and its FS residual across the next decision boundary. | [tests/palette/fixed/0072_quant_palette_mono_float_negative_fs.t](../../../tests/palette/fixed/0072_quant_palette_mono_float_negative_fs.t) |
| FM-15 | Accept P3 while retaining CIELAB endpoints and palette-free mask output. | [tests/palette/fixed/0073_p3_mask.t](../../../tests/palette/fixed/0073_p3_mask.t) |
| FM-16 | Keep CIELAB endpoints and mask semantics with linear output. | [tests/palette/fixed/0074_quant_palette_mono_float_linear_output.t](../../../tests/palette/fixed/0074_quant_palette_mono_float_linear_output.t) |
| FM-17 | Keep CIELAB endpoints and mask semantics with SMPTE-C output. | [tests/palette/fixed/0075_quant_palette_mono_float_smptec_output.t](../../../tests/palette/fixed/0075_quant_palette_mono_float_smptec_output.t) |
| FM-18 | Preserve genuinely byte-eligible monochrome application. | [tests/palette/fixed/0076_quant_palette_mono_float_byte_eligible.t](../../../tests/palette/fixed/0076_quant_palette_mono_float_byte_eligible.t) |
| FM-19 | Honor explicit float preparation with byte input. | [tests/palette/fixed/0077_quant_palette_mono_float_explicit_float.t](../../../tests/palette/fixed/0077_quant_palette_mono_float_explicit_float.t) |
| FM-20 | Re-evaluate native/byte/native preparation on one encoder. | [tests/palette/fixed/0078_quant_palette_mono_float_precision_reuse.t](../../../tests/palette/fixed/0078_quant_palette_mono_float_precision_reuse.t) |
| FM-21 | Re-evaluate byte gamma/native P3/byte gamma preparation on one encoder. | [tests/palette/fixed/0079_quant_palette_mono_float_output_reuse.t](../../../tests/palette/fixed/0079_quant_palette_mono_float_output_reuse.t) |
| FM-22 | Apply an inversion change on the same encoder without stale endpoints. | [tests/palette/fixed/0080_inversion_reuse.t](../../../tests/palette/fixed/0080_inversion_reuse.t) |
| FM-23 | Emit the clear request while preserving the independently expected mask. | [tests/palette/fixed/0081_quant_palette_mono_float_clear_mask.t](../../../tests/palette/fixed/0081_quant_palette_mono_float_clear_mask.t) |
| FM-24 | Accept zero-rate snap as inactive in P3 monochrome output. | [tests/palette/fixed/0082_p3_zero_snap.t](../../../tests/palette/fixed/0082_p3_zero_snap.t) |
| FM-25 | Accept P3 snap without changing fixed monochrome values or the foreground mask, then disable it on the same encoder. | [tests/palette/fixed/0083_quant_palette_mono_float_p3_snap_bypass.t](../../../tests/palette/fixed/0083_quant_palette_mono_float_p3_snap_bypass.t) |
| FM-26 | Preserve native monochrome endpoints for multiframe and subsequent static application. | [tests/palette/fixed/0084_quant_palette_mono_float_multiframe_native.t](../../../tests/palette/fixed/0084_quant_palette_mono_float_multiframe_native.t) |
| FM-27 | Export the exact inverted sRGB8 endpoints after CIELAB application. | [tests/palette/fixed/0085_cielab_export.t](../../../tests/palette/fixed/0085_cielab_export.t) |
| FM-28 | Preserve the automatic gamma lookup selector and expected mask. | [tests/palette/fixed/0086_auto_gamma.t](../../../tests/palette/fixed/0086_auto_gamma.t) |
| FM-29 | Use correctly typed endpoints with automatic CIELAB lookup. | [tests/palette/fixed/0087_auto_cielab.t](../../../tests/palette/fixed/0087_auto_cielab.t) |
| FM-30 | Preserve a fully omitted mask without emitting palette commands. | [tests/palette/fixed/0088_quant_palette_mono_float_empty_mask.t](../../../tests/palette/fixed/0088_quant_palette_mono_float_empty_mask.t) |
| FM-31 | Preserve a fully painted mask without emitting palette commands. | [tests/palette/fixed/0089_full_mask.t](../../../tests/palette/fixed/0089_full_mask.t) |

### Coverage boundary

The [palette usage suite](../../../tests/palette) also contains exported-entry, colorspace, animation, and perceptual-quality checks. The [option-conflict suite](../../../tests/cli) exercises incompatible selectors. These categories do not establish how a particular terminal chooses its monochrome foreground, or that a fixed palette improves quality for every image. The table above records exact output/path observations; quality thresholds elsewhere are separate evidence.
