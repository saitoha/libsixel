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

`-i` is meaningful for `-e`; it is not a general image-negative operation. It does not invert arbitrary RGB input or a `-b` palette. `-B` and `-A` control image background/alpha processing before encoding, while the monochrome non-painted entry is a separate encoder decision. See [alpha policy](../loader/alpha-policy.md) for the terminal's role in preserving unpainted positions.

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
| `-W`, `-.`, `-U`, `-M` | Representation, application, serialization, and export are separate boundaries. Do not infer all four from the construction bypass. The current fixed-palette preparation/export behavior and its tests are described in [output color space](output-colorspace.md#fixed-palettes-and-palette-export). |
| Crop, resize, loader CMS, `-A`, `-B` | Still affect the image being mapped. A fixed output palette does not suppress input color management or alpha processing. |

For exact palette interchange, inspect `-M` output rather than assuming the terminal receives the original byte values. For exact terminal behavior, inspect the SIXEL stream as well: monochrome mode and percentage rounding are not represented solely by the exported palette file.

## Implementation

Palette tables and `sixel_dither_get()` are in [`dither.c`](../../src/dither.c), with the lattice in [`oklch-lattice256.h`](../../src/oklch-lattice256.h). [`encoder.c`](../../src/encoder.c) owns selector conflicts, fixed-palette preparation, and option application.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation | Owning test |
| --- | --- | --- |
| FP-01 | Monochrome with the tested `-Acomposite` setting requests `P2=1` and emits no palette commands. | [tests/quant/palette/usage/0165_monochrome_sixel_mode_no_palette_slots.t](../../tests/quant/palette/usage/0165_monochrome_sixel_mode_no_palette_slots.t) |
| FP-02 | Explicit lookup changes the built-in-palette application path. | [tests/quant/palette/usage/0192_builtin_explicit_lookup_policy_applied.t](../../tests/quant/palette/usage/0192_builtin_explicit_lookup_policy_applied.t) |
| FP-03 | Explicit diffusion changes fixed-palette output. | [tests/quant/palette/usage/0141_builtin_palette_explicit_dither_changes_output.t](../../tests/quant/palette/usage/0141_builtin_palette_explicit_dither_changes_output.t) |
| FP-04 | Explicit snap policies are bypassed for a built-in palette. | [tests/quant/palette/usage/0195_builtin_snap_policy_bypassed.t](../../tests/quant/palette/usage/0195_builtin_snap_policy_bypassed.t) |

### Coverage boundary

The [palette usage suite](../../tests/quant/palette/usage/) also contains exported-entry, colorspace, animation, and perceptual-quality checks. The [option-conflict suite](../../tests/cli/options/invalid/) exercises incompatible selectors. These categories do not establish how a particular terminal chooses its monochrome foreground, or that a fixed palette improves quality for every image. The table above records exact output/path observations; quality thresholds elsewhere are separate evidence.
