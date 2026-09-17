# Output Color Space

`img2sixel -U` selects the RGB representation of the palette written to a SIXEL stream. The legacy output choices also apply to palette export with `-M`; the limited Display P3 path rejects palette interchange. For normal terminal display and reusable palette files, leave it at its default, `gamma`. Use another output space only when the consumer's interpretation of those numbers is known.

```text
-U COLORSPACE
--output-colorspace=COLORSPACE
```

| Value | Output representation | Intended use |
| --- | --- | --- |
| `gamma` | Gamma-encoded sRGB channels. | Default output for consumers expecting ordinary RGB code values. |
| `linear` | Linear-light RGB with the sRGB primary basis. | A consumer explicitly configured to interpret linear-light channel values. |
| `smpte-c` | libsixel's SMPTE-C primary conversion and gamma encoding. | A consumer expecting that specific RGB representation. |
| `display-p3` | Standard Display P3 primaries, D65 white and sRGB transfer encoding. | The [limited CPU float path](#display-p3-selected-cpu-float-path) and a receiver explicitly configured for P3 input. |

The parser also accepts `smptec` as an alias for `smpte-c`, folds letter case, and uses the shared [choice prefix-matching policy](../cli/prefix-matching.md). Full names are preferable in durable commands. A repeated `-U` replaces the earlier value. `oklab`, `cielab`, and `din99d` are working/clustering choices, not accepted output choices. `-U` takes a required name, not a numeric gamma exponent or an ICC profile path.

## Where the conversion happens

For the legacy `gamma`, `linear` and `smpte-c` palette paths, the relevant data flow is:

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

`-X` controls the geometry used to construct a palette. `-W` controls the geometry used to choose a palette index and propagate dither error. In these legacy paths, `-U` converts the resulting palette for output; it does not rerun either operation. Changing only `-U` can therefore change the displayed colors while retaining the index assignments for a static image. The P3 path instead finalizes the sent palette before lookup, as described below.

Unlike the implicit coupling from an explicit `-W` to an unspecified `-X`, `-U` does not change either earlier choice. For example:

```sh
img2sixel -Xgamma -Woklab -Ugamma image.png
```

This builds the palette in gamma coordinates, applies it in Oklab, and writes gamma RGB output. Keeping `-Ugamma` does not cancel the effect of `-Woklab`. Conversely, `-Ulinear` alone does not make lookup, dithering, resize, or alpha compositing operate in linear light. Those stages have their own controls; see [Working color space](working-colorspace.md), [Crop and resize](crop-resize.md), and [Alpha policy](../loader/alpha-policy.md).

## Legacy representations

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

## `display-p3`: selected CPU float path

The selected P3 path supports opaque static RGB input, automatic or explicit float precision, linear clustering, linear or gamma working coordinates, CPU K-means with full-frame sampling and no binning, exact `none` lookup, and either no dithering or CPU Floyd–Steinberg (`-d fs`). Merge, cover and snap are disabled. It emits normal RGB SIXEL. K-means feedback is disabled. Other combinations fail before SIXEL output starts; in particular, `-m` and `-M` cannot read or export unmarked P3 palette values. Other quantizers and dither methods, GPU, high-color, HLS, DRCS, macros, delta encoding, animation and alpha-bearing frames are outside this selected path.

The selected loader integration tests use libpng P3 ICC RGB8/RGB16, an untagged sRGB RGB8 PNG, and a static RGB16 TIFF with orientation 6. They explicitly select builtin CMS, canonical linear output and `prefer_8bit=0`. These tests do not establish wide-gamut support for every loader, source domain, resizing filter or optional backend. The accepted opaque RGB frame storage forms are RGB888, RGBFLOAT32 and LINEARRGBFLOAT32; the selected encoder path converts them to linear float coordinates before palette construction.

```sh
./converters/img2sixel --threads=1 \
  -L 'libpng:cms_engine=builtin:cms_target=linear:prefer_8bit=0!' \
  -j scalar:resize_precision=float --precision=auto \
  -Q kmeans:seed=1 -X linear -W gamma -U display-p3 \
  --sampling-policy=full-frame --binning-policy=none \
  -F none -a off -_ none '-~none' -d fs -t rgb \
  tests/data/color-numeric/png-p3-16-rgb.png > output.six
```

The frame remains in canonical sRGB coordinates, including negative and above-one components needed to represent P3 colors. The output definition is a separate job policy. K-means retains these extended values through seeding, Lloyd updates and float palette export. It cannot silently retry a byte solver when the float path fails.

Before index selection, the palette owner converts the canonical palette to P3, clamps only to the destination gamut, and rounds each channel to an integer in `0..100`. It stores these exact codes. It also converts the stored codes back to canonical float coordinates for exact lookup. The byte palette is only a compatibility projection. Lookup therefore selects among the colors that the stream will actually send. The writer consumes the stored RGB101 codes without another color transform or byte round trip; palette compaction, reordering and cache reuse are disabled in this path.

This distinction is observable even within sRGB: linear gray centers `0.1` and `0.11` become P3 code values `35%` and `37%`. Their decoded midpoint moves above `0.106`, so that input must select the first sent color although it was closer to the second original center. The direct palette test protects this index change and the inverse canonical values. P3 primary and intermediate-color wire tests independently protect the destination conversion; they do not infer correctness from a perceptual similarity score.

With `--precision=auto` or no precision option, P3 requires an effective float working format even for `-Wgamma`. An explicit `--precision=8bit` is rejected before output; a later `--precision=auto` clears that request. Requested precision stays separate from this output requirement, so leaving P3 restores the effective format required by the remaining precision and working-space settings. An explicit float request remains float after leaving P3; automatic gamma working coordinates return to the ordinary byte preference.

Gamma working coordinates use the existing sRGB transfer function with sign-preserving extended values. FS computes its error against the inverse of the actual sent palette and propagates it without an intermediate sRGB gamut clamp. For example, errors `(0.125, -0.125, 0.0625)` sent to a following source `(1, -0.03125, 0.125)` with the FS right-neighbor weight `7/16` produce `(1.0546875, -0.0859375, 0.15234375)` before that next lookup. The direct test observes this input to the real lookup. The CLI two-color test checks independently derived source-group means, paint-time wire agreement with a raster FS reference calculation, and actual FS dispatch. Its painted result is also unchanged when error propagation is omitted, so detecting missing propagation or an intermediate clamp belongs to the direct next-lookup test and its code mutations. The other few-color CLI witnesses can optimize FS away because every source color fits in the palette; they establish option acceptance and wire values, not error diffusion. Nonfinite working samples or errors fail instead of entering lookup, and a later valid apply on the same policy recovers. The ordinary non-P3 FS path retains its existing channel clamp.

Repeated `-U` settings replace the output definition. Reusing one encoder and output object for P3, gamma and P3 again restores the corresponding behavior on each job. The private precision opt-in is scoped to one palette-generation call and does not change the installed component request structures.

## SIXEL syntax and receiver interpretation

With `-t rgb`, a palette definition has this form:

```text
# index ; 2 ; red ; green ; blue
```

Each RGB component is an integer percentage in `0..100`. The `2` selects the RGB form of the color definition. It does not identify sRGB, linear RGB, SMPTE-C, Display P3, a transfer curve, or a display profile. SIXEL provides no ICC payload or output-space tag for the receiver to recover the `-U` setting. See [SIXEL color registers](../sixel-format.md#graphics-color-introducer).

`-t hls` instead serializes the converted output RGB triplet as DEC HLS coordinates. It changes the syntax of the color definition; it does not make `-U` an HLS working-space selector or add color-management metadata.

Display behavior therefore depends on the receiving terminal and display stack. A browser preview or a decoded PNG that treats all three streams as ordinary RGB does not show three correctly color-managed renditions. For a meaningful comparison of alternative output spaces, the consumer must know the chosen space outside the SIXEL stream.

### Negotiation through the proposed color protocol

The [Terminal graphics color management proposal](../protocols/graphics-color-management.md) defines a receiver-independent agreement for these RGB numbers. It separates an explicit sRGB policy from a reported device-input definition, provides DECRQSS save/restore for each target's policy, and specifies dedicated color-definition queries. Protocol support must be established by the session's replies; a P3-capable display alone does not establish the receiver's input interpretation.

For ordinary SIXEL, a controller using this proposal can display the selected `-Udisplay-p3` output with the following transaction. `CSI`, `DCS`, and `ST` denote the control bytes defined in the proposal, and `M` is the exact saved policy.

| Step | Exchange or action |
| --- | --- |
| Discover target 1 | Send `CSI ?1./r`; require a complete valid reply with `A=1`, `S2=1`, and `C=4`. For example, `DCS 1;1;1;1;4./r ST` also advertises expansion support. |
| Save policy | Send `DCS $q1./p ST`; save `M` from `DCS 1$r1;M./p ST`, preserving `0` versus `1`. |
| Select device input | Send `CSI 1;2./p`; query again and require policy `2`. |
| Display | Send the bytes generated with `-Udisplay-p3`; new RGB definitions are interpreted as standard Display P3. |
| Restore | Send `CSI 1;M./p`; verify the saved value, including on recoverable errors. |

For a color-preserving sRGB comparison, generate `-Ugamma` output and set and verify policy `1`. Feeding gamma sRGB values under a P3 device policy reinterprets those values in the wider basis; it does not establish correct sRGB rendering. Policy `3` explicitly requests the proposal's component expansion and serves a different purpose from source-to-destination color conversion.

The current `img2sixel -Udisplay-p3` and `-Ugamma` options perform none of these color-protocol exchanges. A controller must manage the transaction and error cleanup; interactive `printf`/`cat` commands do not provide complete framing, correlation, or restoration. The proposed `-Uauto` CLI flow is described in the proposal and is not an implemented option in this revision. A timeout or malformed response never proves that a receiver is using sRGB.

## Precision, rounding, and cost

The legacy output encoder converts a private palette copy and preserves the reusable dither palette in working coordinates. When a usable float32 palette is available, it converts those typed values before packing bytes. Otherwise it converts the byte palette. The P3 path commits its RGB101 palette before lookup. Increasing working precision can reduce conversion rounding, but it cannot increase the precision of SIXEL's integer percentage fields.

For RGB definitions, the byte path rounds a channel `b` with integer arithmetic:

```text
percentage = (b * 100 + 127) / 255
```

The float path rounds a clamped normalized channel directly with `floor(channel * 100 + 0.5)`. An exported 8-bit palette and a decoded SIXEL palette can therefore differ through their respective byte and percentage quantization steps. They are not generally byte-identical merely because both use the same `-U` value.

For generated palettes with exact [snap policy](snap-policy.md) enabled at `rate=1`, output finalization follows the `-U` conversion and puts the palette onto the reversible RGB percentage grid. This includes SMPTE-C output. It prevents the conversion from leaving the final palette off that grid; it does not remove clipping or make every earlier color conversion lossless.

For `K` palette entries, this conversion is `O(K)` work and uses `O(K)` palette storage, rather than converting all `N` image pixels again. `-U` alone does not request a float32 image buffer. This cost model applies to the ordinary palette path; it is not a measured whole-program speed comparison. Different numeric color definitions can change serialized byte length even when index assignment is unchanged.

## Fixed palettes and palette export

For the legacy output choices, `-M` writes the selected output RGB representation into ACT, PAL, RIFF PAL, or GPL entries. In particular, `-Wcielab -Ugamma -M gpl:palette.gpl` converts the working palette back to gamma RGB instead of exporting internal Lab coordinates. `display-p3` rejects both `-m` and `-M` before output because these formats do not preserve its color definition. File format selection and palette reuse are covered by [External palette input and output](external-palettes.md).

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

The current `-I` [high-color path](high-color.md) dispatches to `sixel_encode_highcolor`, which normalizes pixel format and emits changing palette registers without the legacy `source_colorspace` to output `colorspace` conversion above. Changing only a legacy `-U` choice currently does not provide the requested output-space conversion on that path. This is an implementation gap. The P3 path explicitly rejects high-color output.

Likewise, `-U` does not configure decoder interpretation, embed an output ICC profile, change loader CMS, or replace the [PNG writer's snapshot policy](../writers/png.md). The static-image stage explanation should not be extended to [delta encoding](delta-encoding.md) without accounting for its retained display-color state across frames.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Covered observation | Owning test |
| --- | --- | --- |
| OC-01 | A generated two-entry CIELAB byte palette exports independently derived gamma RGB8 values through `-M gpl:-`; solver entry order is not fixed. | [tests/quant/palette/usage/0197_mapfile_output_converts_cielab_to_gamma.t](../../tests/quant/palette/usage/0197_mapfile_output_converts_cielab_to_gamma.t) |
| OC-02 | A built-in palette preserves its supplied bytes under the current `-Wlinear -Ulinear` preparation/export path. | [tests/quant/palette/usage/0143_builtin_palette_working_output_colorspace_stable.t](../../tests/quant/palette/usage/0143_builtin_palette_working_output_colorspace_stable.t) |
| OC-03 | A structured PAL palette preserves its supplied bytes under the current `-Wlinear -Ulinear` preparation/export path. | [tests/quant/palette/usage/0144_mapfile_pal_working_output_colorspace_stable.t](../../tests/quant/palette/usage/0144_mapfile_pal_working_output_colorspace_stable.t) |
| OC-04 | A PNG image mapfile exports the exact expected converted palette under `-Wlinear -Ulinear` with CMS disabled. | [tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t](../../tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t) |
| OC-05 | The corresponding PNG image-mapfile path remains deterministic when ICC metadata is present but CMS is explicitly disabled. | [tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t](../../tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t) |
| OC-06 | Exact snap after SMPTE-C output conversion gives matching reversible ACT channels and SIXEL RGB definitions. | [tests/quant/palette/0047_snap_smptec_grid.t](../../tests/quant/palette/0047_snap_smptec_grid.t) |
| OC-07 | P3 ICC RGB8 PNG produces independently derived P3 RGB101 values through the actual libpng/builtin CMS path. | [tests/cli/color/0005_libpng_p3_8_wire.t](../../tests/cli/color/0005_libpng_p3_8_wire.t) |
| OC-08 | The corresponding RGB16 PNG retains its original sample precision through P3 RGB101 output. | [tests/cli/color/0006_libpng_p3_16_wire.t](../../tests/cli/color/0006_libpng_p3_16_wire.t) |
| OC-09 | Untagged sRGB PNG colors are converted to P3 rather than reinterpreted as P3 values. | [tests/cli/color/0007_libpng_srgb_to_p3_wire.t](../../tests/cli/color/0007_libpng_srgb_to_p3_wire.t) |
| OC-10 | P3 ICC RGB16 TIFF orientation 6 preserves displayed geometry and independently derived P3 RGB101 values. | [tests/cli/color/0008_libtiff_p3_16_wire.t](../../tests/cli/color/0008_libtiff_p3_16_wire.t) |
| OC-11 | Extended canonical primary values survive K-means and the stored RGB101 inverse reconstructs the expected canonical float palette. | [tests/quant/palette/0051_p3_palette_values.t](../../tests/quant/palette/0051_p3_palette_values.t) |
| OC-12 | Exact lookup selects the inverse of the sent RGB101 palette, including a decision that changes after output quantization. | [tests/quant/palette/0052_p3_sent_palette_lookup.t](../../tests/quant/palette/0052_p3_sent_palette_lookup.t) |
| OC-13 | A float solver allocation failure cannot succeed through a byte retry; ordinary generation and later P3 generation recover without a leaked opt-in or stale wire palette. | [tests/quant/palette/0053_p3_palette_failure_reset.t](../../tests/quant/palette/0053_p3_palette_failure_reset.t) |
| OC-14 | One encoder and output object can encode P3, gamma and P3 again with the correct restored interpretation. | [tests/quant/palette/0054_p3_encoder_reuse.t](../../tests/quant/palette/0054_p3_encoder_reuse.t) |
| OC-15 | P3 mapfile input is rejected before the writer starts. | [tests/quant/palette/0055_p3_mapfile_rejected.t](../../tests/quant/palette/0055_p3_mapfile_rejected.t) |
| OC-16 | P3 palette export is rejected before the writer starts. | [tests/quant/palette/0056_p3_palette_export_rejected.t](../../tests/quant/palette/0056_p3_palette_export_rejected.t) |
| OC-17 | Unsupported Atkinson dithering is rejected before the P3 writer starts. | [tests/quant/palette/0057_p3_unsupported_diffusion_rejected.t](../../tests/quant/palette/0057_p3_unsupported_diffusion_rejected.t) |
| OC-18 | Gamma working coordinates retain the extended canonical P3 red through its final (100,0,0) wire value. | [tests/quant/palette/0058_p3_gamma_working.t](../../tests/quant/palette/0058_p3_gamma_working.t) |
| OC-19 | Automatic P3 precision promotes gamma working coordinates to float without changing the base request. | [tests/quant/palette/0059_p3_precision_auto.t](../../tests/quant/palette/0059_p3_precision_auto.t) |
| OC-20 | An explicit P3 byte request fails before output with an actionable precision diagnostic. | [tests/quant/palette/0060_p3_precision_8bit_rejected.t](../../tests/quant/palette/0060_p3_precision_8bit_rejected.t) |
| OC-21 | Automatic reset, explicit float/byte requests and leaving/reentering P3 preserve requested and effective state on one encoder. | [tests/quant/palette/0061_p3_precision_reuse.t](../../tests/quant/palette/0061_p3_precision_reuse.t) |
| OC-22 | Nonzero FS errors reach the next real lookup with independently derived extended values. | [tests/quant/palette/0062_p3_fs_error.t](../../tests/quant/palette/0062_p3_fs_error.t) |
| OC-23 | The ordinary non-P3 FS path retains its existing bounded channel values. | [tests/quant/palette/0063_p3_fs_legacy_control.t](../../tests/quant/palette/0063_p3_fs_legacy_control.t) |
| OC-24 | A nonfinite P3 sample fails before lookup and the same policy subsequently processes valid samples. | [tests/quant/palette/0064_p3_fs_nonfinite_recovery.t](../../tests/quant/palette/0064_p3_fs_nonfinite_recovery.t) |
| OC-25 | P3 RGB8 PNG retains independent RGB101 values with gamma working coordinates and automatic precision. | [tests/cli/color/0009_libpng_p3_gamma_auto_wire.t](../../tests/cli/color/0009_libpng_p3_gamma_auto_wire.t) |
| OC-26 | P3 RGB16 PNG reduced to two colors matches independent source-group means and the paint-time wire from a raster FS reference calculation, and reports actual FS dispatch; this fixture does not independently detect omitted error propagation. | [tests/cli/color/0010_libpng_p3_gamma_fs_wire.t](../../tests/cli/color/0010_libpng_p3_gamma_fs_wire.t) |
| OC-27 | Untagged sRGB PNG is color-converted to independent P3 RGB101 values with automatic gamma processing and an accepted FS request; the few-color optimization needs no dithering. | [tests/cli/color/0011_libpng_srgb_to_p3_fs_wire.t](../../tests/cli/color/0011_libpng_srgb_to_p3_fs_wire.t) |
| OC-28 | Oriented RGB16 TIFF retains geometry and independent P3 RGB101 with automatic linear processing and an accepted FS request; the few-color optimization needs no dithering. | [tests/cli/color/0012_libtiff_p3_auto_fs_wire.t](../../tests/cli/color/0012_libtiff_p3_auto_fs_wire.t) |

### Coverage audit boundary

The first five tests assert exact exported entries. OC-06 checks the output grid and export/wire agreement; it does not independently validate the SMPTE-C conversion matrix. OC-02 and OC-03 establish byte preservation for the named configurations, not perceptual correctness of all non-gamma fixed-palette combinations. OC-05 is not evidence that a CMS transform ran.

P3 values, lookup and reuse are directly covered by OC-07 through OC-28. General defaults, alias and case handling, invalid values, legacy repeated-option precedence, unchanged legacy static index assignment, the numeric example, and the high-color gap are inspectable in the owning code or reproducible with CLI probes, but are not each covered by an independent `-U` regression in this inventory. Multi-input/animation reuse and cross-frame delta interactions also lack an exhaustive output-space matrix here. The proposed receiver protocol has no established automated end-to-end coverage in this inventory. No image-quality threshold or terminal-compatibility result is inferred from the exact palette checks.

## Implementation references

- [`src/encoder.c`](../../src/encoder.c): option defaults and parsing, palette-source preparation, output format handoff, and the separate `-M` capture conversion.
- [`src/encoder-core-encode.c`](../../src/encoder-core-encode.c): private byte/float palette copies, output conversion, exact-snap finalization, and RGB/HLS definitions.
- [`src/colorspace.c`](../../src/colorspace.c): transfer curves, SMPTE-C matrices, conversion through linear RGB, clipping, and byte/float dispatch.
- [`src/encoder-core-highcolor.c`](../../src/encoder-core-highcolor.c): the alternate high-color implementation and its current output-conversion gap.
- [Color spaces and loader color management](../concepts/colorspace.md): the wider source-profile, working-space, and output-space model.

The CIELAB export test uses black and white source pixels and the existing typed byte palette boundary. [Independent inverse CIELAB/XYZ/sRGB calculations](../../tests/data/color-numeric/palette-export.json) predict `(3,0,0)` and `(255,254,254)` after that boundary because the byte a/b representation has no exact neutral midpoint. This exact storage-conversion assertion does not establish lossless float palette retention or freeze K-means representatives from a natural image.

Replacing the typed byte palette boundary requires re-deriving this storage-specific expectation from independent values. The current byte artifacts are not an ideal black/white color contract for a future canonical float palette. Natural-image clustering and decoded-output quality remain covered by the existing working-space quality tests (WC-04 through WC-08 in [Working color space](working-colorspace.md#test-coverage)), including CIELAB with the unchanged MS-SSIM floor.
