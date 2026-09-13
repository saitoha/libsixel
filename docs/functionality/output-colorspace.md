# Output Color Space

`img2sixel -U` selects the RGB representation of the palette written to a SIXEL stream or exported with `-M`. For normal terminal display and reusable palette files, leave it at its default, `gamma`. Use another output space only when the consumer's interpretation of those numbers is known.

```text
-U COLORSPACE
--output-colorspace=COLORSPACE
```

| Value | Output representation | Intended use |
| --- | --- | --- |
| `gamma` | Gamma-encoded sRGB channels. | Default output for consumers expecting ordinary RGB code values. |
| `linear` | Linear-light RGB with the sRGB primary basis. | A consumer explicitly configured to interpret linear-light channel values. |
| `smpte-c` | libsixel's SMPTE-C primary conversion and gamma encoding. | A consumer expecting that specific RGB representation. |

The parser also accepts `smptec` as an alias for `smpte-c`, folds letter case, and uses the shared [choice prefix-matching policy](../cli/prefix-matching.md). Full names are preferable in durable commands. A repeated `-U` replaces the earlier value. `oklab`, `cielab`, and `din99d` are working/clustering choices, not accepted output choices. `-U` takes a required name, not a numeric gamma exponent or an ICC profile path.

## Where the conversion happens

For the ordinary palette encoder, the relevant data flow is:

```text
Loaded image with a known color interpretation
    |
    +--> Palette construction in -X --> Palette in -W
    |                                       |
    +--> Image pixels in -W -----------------+
                                            |
                                  Lookup and dithering
                                            |
                               Pixel indices + working palette
                                            |
                               Convert a palette copy to -U
                                            |
                               Optional exact-snap finalization
                                            |
                         SIXEL color definitions / -M palette file
```

`-X` controls the geometry used to construct a palette. `-W` controls the geometry used to choose a palette index and propagate dither error. `-U` converts the resulting palette for output; it does not rerun either operation. Changing only `-U` can therefore change the displayed colors while retaining the index assignments for a static image. This is a stage boundary, not a promise that complete SIXEL byte streams or stateful delta sequences remain identical.

Unlike the implicit coupling from an explicit `-W` to an unspecified `-X`, `-U` does not change either earlier choice. For example:

```sh
img2sixel -Xgamma -Woklab -Ugamma image.png
```

This builds the palette in gamma coordinates, applies it in Oklab, and writes gamma RGB output. Keeping `-Ugamma` does not cancel the effect of `-Woklab`. Conversely, `-Ulinear` alone does not make lookup, dithering, resize, or alpha compositing operate in linear light. Those stages have their own controls; see [Working color space](working-colorspace.md), [Crop and resize](crop-resize.md), and [Alpha policy](../loader/alpha-policy.md).

## What the three representations mean

### `gamma`: sRGB encoding

The name `gamma` denotes libsixel's sRGB transfer curve, including its linear segment near black. It is not simply a power of `1/2.2`. For a normalized linear-light channel `l` in `[0, 1]`, the corresponding sRGB channel `g` is:

```text
g = 12.92 * l                       when l <= 0.0031308
g = 1.055 * l^(1 / 2.4) - 0.055    otherwise
```

The inverse is:

```text
l = g / 12.92                      when g <= 0.04045
l = ((g + 0.055) / 1.055)^2.4      otherwise
```

The [W3C sRGB definition](https://www.w3.org/TR/css-color-4/#predefined-sRGB) describes the primary basis, D65 white point, and transfer function. libsixel's conversion implementation is in [`src/colorspace.c`](../../src/colorspace.c), with typed float paths and byte lookup-table paths.

### `linear`: linear-light sRGB

`linear` uses the same RGB basis without the sRGB transfer encoding. A gamma channel near `0.5` corresponds to only about `0.214` in linear light. Thus an output palette can change from roughly `50%` to `21%` without intending to change the physical color: the recipient must apply the matching interpretation.

If a receiver instead treats that linear `21%` as an ordinary sRGB code value, the result is much darker. This follows from applying the sRGB decoding curve to a number that was already linear. It is an interpretation mismatch, not evidence that the encoder chose a worse palette.

### `smpte-c`: primary conversion and a power curve

The current implementation first converts the working color to linear sRGB, applies the fixed `sixel_linear_srgb_to_smptec_matrix` in [`src/colorspace.c`](../../src/colorspace.c), clamps the resulting channels to `[0, 1]`, and encodes each channel with the power `1/2.2`. Its inverse uses the power `2.2` and the inverse primary-conversion matrix.

Consequently, `-Usmpte-c` can change channel balance as well as tone response. It is not an alternative spelling for `gamma`, a generic CRT correction, an HDR mode, or a destination ICC transform. The fixed matrix and curve describe libsixel's implementation; this option does not measure or calibrate a monitor, or establish that a particular DEC terminal expects these values.

## SIXEL syntax and receiver interpretation

With `-t rgb`, a palette definition has this form:

```text
# index ; 2 ; red ; green ; blue
```

Each RGB component is an integer percentage in `0..100`. The `2` selects the RGB form of the color definition. It does not identify sRGB, linear RGB, SMPTE-C, a transfer curve, or a display profile. SIXEL provides no ICC payload or output-space tag for the receiver to recover the `-U` setting. See [SIXEL color registers](../sixel-format.md#graphics-color-introducer).

`-t hls` instead serializes the converted output RGB triplet as DEC HLS coordinates. It changes the syntax of the color definition; it does not make `-U` an HLS working-space selector or add color-management metadata.

Display behavior therefore depends on the receiving terminal and display stack. A browser preview or a decoded PNG that treats all three streams as ordinary RGB does not show three correctly color-managed renditions. For a meaningful comparison of alternative output spaces, the consumer must know the chosen space outside the SIXEL stream.

## Precision, rounding, and cost

The output encoder converts a private palette copy and preserves the reusable dither palette in working coordinates. When a usable float32 palette is available, it converts those typed values before packing bytes. Otherwise it converts the byte palette. Increasing working precision can reduce conversion rounding, but it cannot increase the precision of SIXEL's integer percentage fields.

For RGB definitions, the byte path rounds a channel `b` with integer arithmetic:

```text
percentage = (b * 100 + 127) / 255
```

The float path rounds a clamped normalized channel directly with `floor(channel * 100 + 0.5)`. An exported 8-bit palette and a decoded SIXEL palette can therefore differ through their respective byte and percentage quantization steps. They are not generally byte-identical merely because both use the same `-U` value.

For generated palettes with exact [snap policy](snap-policy.md) enabled at `rate=1`, output finalization follows the `-U` conversion and puts the palette onto the reversible RGB percentage grid. This includes SMPTE-C output. It prevents the conversion from leaving the final palette off that grid; it does not remove clipping or make every earlier color conversion lossless.

For `K` palette entries, this conversion is `O(K)` work and uses `O(K)` palette storage, rather than converting all `N` image pixels again. `-U` alone does not request a float32 image buffer. This cost model applies to the ordinary palette path; it is not a measured whole-program speed comparison. Different numeric color definitions can change serialized byte length even when index assignment is unchanged.

## Fixed palettes and palette export

`-M` writes the selected output RGB representation into ACT, PAL, RIFF PAL, or GPL entries. In particular, `-Wcielab -Ugamma -M gpl:palette.gpl` converts the working palette back to gamma RGB instead of exporting internal Lab coordinates. File format selection and palette reuse are covered by [External palette input and output](external-palettes.md).

These exports do not record a `-U` selection for later automatic recovery. Keep `-Ugamma` for normal interchange. Exporting linear or SMPTE-C values and importing the file later with default settings does not automatically invert the earlier transform.

The preparation of fixed palettes also matters:

| Palette source | Current preparation and output boundary |
| --- | --- |
| Generated palette | Constructed in `-X`, converted to `-W` for application, then converted to `-U` for output. |
| Image mapfile, such as `-m swatches.png` | Loaded as an image and prepared in the selected working representation before output conversion. Loader/CMS settings can affect its colors. |
| Structured ACT/PAL/GPL input or built-in `-b` palette | Supplied byte entries are installed directly; this path does not perform the same image-loader conversion into `-W`. Output conversion uses the prepared palette and the frame's working-space setting. |
| Monochrome `-e` | Uses a fixed black/white palette and bypasses palette construction. Black and white alone are poor probes of a nonlinear output transfer. |

In particular, the existing `-Wlinear -Ulinear` built-in and PAL-file tests preserve the supplied byte entries, while the PNG-mapfile tests export converted entries. Matching the two option names does not prove that those input routes normalized their palettes in the same way. For an ordinary external RGB palette, `-Wgamma -Ugamma` avoids that ambiguity. Fixed palettes also bypass generated-palette snap processing. These are current preparation boundaries, not a claim that every fixed-palette/non-gamma combination is colorimetrically equivalent to a generated palette.

## Reproducible numeric example

Run the following from the repository root, using the converter built in that checkout. It supplies a two-entry byte palette with `-Wgamma`, so quantizer choices do not obscure the output conversion:

```sh
./converters/img2sixel --threads=1 --gpu-policy=off --diffusion=none \
  -Wgamma -Ulinear -t rgb -m pal:- -M pal:- -o /dev/null \
  tests/data/inputs/snake_16.png <<'PAL'
JASC-PAL
0100
2
128 64 32
0 0 0
PAL
```

The exported first entry is `55 13 4`; the black entry stays `0 0 0`. Replacing only `-Ulinear` gives:

| Output option | First entry in the exported PAL | First SIXEL RGB definition |
| --- | --- | --- |
| `-Ugamma` | `128 64 32` | `#0;2;50;25;13` |
| `-Ulinear` | `55 13 4` | `#0;2;22;5;2` |
| `-Usmpte-c` | `130 64 39` | `#0;2;51;25;15` |

To inspect the wire definitions, replace `-M pal:- -o /dev/null` with `-o output.six` and inspect that file. The table describes the byte-palette path with snap bypassed; it is not an expected-value table for arbitrary generated or float32 palettes. Its purpose is to show separately the RGB conversion, 8-bit export, and percentage serialization boundaries.

## Scope and current gaps

The conversion described here belongs to `sixel_encode_dither`, including ordinary generated and supplied palette output. An option being accepted does not establish that every alternate encoder consumes it.

The current `-I` [high-color path](high-color.md) dispatches to `sixel_encode_highcolor`, which normalizes pixel format and emits changing palette registers without the `source_colorspace` to output `colorspace` conversion above. Changing only `-U` currently does not provide the requested output-space conversion on that path. This is an implementation gap, not a compatibility guarantee that `-U` should remain ineffective there.

Likewise, `-U` does not configure decoder interpretation, embed an output ICC profile, change loader CMS, or replace the [PNG writer's snapshot policy](../writers/png.md). The static-image stage explanation should not be extended to [delta encoding](delta-encoding.md) without accounting for its retained display-color state across frames.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Covered observation | Owning test |
| --- | --- | --- |
| OC-01 | A generated CIELAB working palette exports the exact expected gamma RGB values through `-Ugamma -M gpl:-`. | [tests/quant/palette/usage/0197_mapfile_output_converts_cielab_to_gamma.t](../../tests/quant/palette/usage/0197_mapfile_output_converts_cielab_to_gamma.t) |
| OC-02 | A built-in palette preserves its supplied bytes under the current `-Wlinear -Ulinear` preparation/export path. | [tests/quant/palette/usage/0143_builtin_palette_working_output_colorspace_stable.t](../../tests/quant/palette/usage/0143_builtin_palette_working_output_colorspace_stable.t) |
| OC-03 | A structured PAL palette preserves its supplied bytes under the current `-Wlinear -Ulinear` preparation/export path. | [tests/quant/palette/usage/0144_mapfile_pal_working_output_colorspace_stable.t](../../tests/quant/palette/usage/0144_mapfile_pal_working_output_colorspace_stable.t) |
| OC-04 | A PNG image mapfile exports the exact expected converted palette under `-Wlinear -Ulinear` with CMS disabled. | [tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t](../../tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t) |
| OC-05 | The corresponding PNG image-mapfile path remains deterministic when ICC metadata is present but CMS is explicitly disabled. | [tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t](../../tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t) |
| OC-06 | Exact snap after SMPTE-C output conversion gives matching reversible ACT channels and SIXEL RGB definitions. | [tests/quant/palette/0047_snap_smptec_grid.t](../../tests/quant/palette/0047_snap_smptec_grid.t) |

### Coverage audit boundary

The first five tests assert exact exported entries. OC-06 checks the output grid and export/wire agreement; it does not independently validate the SMPTE-C conversion matrix. OC-02 and OC-03 establish byte preservation for the named configurations, not perceptual correctness of all non-gamma fixed-palette combinations. OC-05 is not evidence that a CMS transform ran.

Defaults, alias and case handling, invalid values, repeated-option precedence, unchanged static index assignment, the numeric example, and the high-color gap are inspectable in the owning code or reproducible with CLI probes, but are not each covered by an independent `-U` regression in this inventory. Multi-input/animation reuse and cross-frame delta interactions also lack an exhaustive output-space matrix here. Receiver color management remains a manual integration observation. No image-quality threshold or terminal-compatibility result is inferred from the exact palette checks.

## Implementation references

- [`src/encoder.c`](../../src/encoder.c): option defaults and parsing, palette-source preparation, output format handoff, and the separate `-M` capture conversion.
- [`src/encoder-core-encode.c`](../../src/encoder-core-encode.c): private byte/float palette copies, output conversion, exact-snap finalization, and RGB/HLS definitions.
- [`src/colorspace.c`](../../src/colorspace.c): transfer curves, SMPTE-C matrices, conversion through linear RGB, clipping, and byte/float dispatch.
- [`src/encoder-core-highcolor.c`](../../src/encoder-core-highcolor.c): the alternate high-color implementation and its current output-conversion gap.
- [Color spaces and loader color management](../concepts/colorspace.md): the wider source-profile, working-space, and output-space model.
