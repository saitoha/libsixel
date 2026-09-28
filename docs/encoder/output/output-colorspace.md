# Output Color Space

`img2sixel -U` selects the RGB representation of the palette written to a SIXEL stream. Palette export with `-M` has its own sRGB interchange boundary. For normal terminal display, leave `-U` at its default, `gamma`. Use another output space only when the consumer's interpretation of those numbers is known.

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
| `auto[:timeout_ms=N]` | CLI negotiation resolves to gamma sRGB or standard Display P3. | Only images sent directly to stdout as a TTY; other destinations retain gamma sRGB. |

The parser also accepts `smptec` as an alias for `smpte-c`, folds letter case, and uses the shared [choice prefix-matching policy](../../cli/prefix-matching.md). Full names are preferable in durable commands. A repeated `-U` replaces the earlier value. `oklab`, `cielab`, and `din99d` are working/clustering choices, not accepted output choices. `-U` takes a required name, not a numeric gamma exponent or an ICC profile path.

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
                         SIXEL color definitions
```

`-X` controls the geometry used to construct a palette. `-W` controls the geometry used to choose a palette index and propagate dither error. In these legacy paths, `-U` converts the resulting palette for output; it does not rerun either operation. Changing only `-U` can therefore change the displayed colors while retaining the index assignments for a static image. The selected P3 and ordinary sRGB float paths instead finalize the sent palette before lookup, as described below.

Unlike the implicit coupling from an explicit `-W` to an unspecified `-X`, `-U` does not change either earlier choice. For example:

```sh
img2sixel -Xgamma -Woklab -Ugamma image.png
```

This builds the palette in gamma coordinates, applies it in Oklab, and writes gamma RGB output. Keeping `-Ugamma` does not cancel the effect of `-Woklab`. Conversely, `-Ulinear` alone does not make lookup, dithering, resize, or alpha compositing operate in linear light. Those stages have their own controls; see [Working color space](../palette-application/working-colorspace/README.md), [Crop and resize](../preprocessing/crop-resize/README.md), and [Alpha policy](../../loader/alpha-policy/README.md).

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

The [W3C sRGB definition](https://www.w3.org/TR/css-color-4/#predefined-sRGB) describes the primary basis, D65 white point, and transfer function. libsixel's conversion implementation is in [`src/colorspace.c`](../../../src/colorspace.c), with typed float paths and byte lookup-table paths.

### `linear`: linear-light sRGB

`linear` uses the same RGB basis without the sRGB transfer encoding. A gamma channel near `0.5` corresponds to only about `0.214` in linear light. Thus an output palette can change from roughly `50%` to `21%` without intending to change the physical color: the recipient must apply the matching interpretation.

If a receiver instead treats that linear `21%` as an ordinary sRGB code value, the result is much darker. This follows from applying the sRGB decoding curve to a number that was already linear. It is an interpretation mismatch, not evidence that the encoder chose a worse palette.

### `smpte-c`: primary conversion and a power curve

The current implementation first converts the working color to linear sRGB, applies the fixed `sixel_linear_srgb_to_smptec_matrix` in [`src/colorspace.c`](../../../src/colorspace.c), clamps the resulting channels to `[0, 1]`, and encodes each channel with the power `1/2.2`. Its inverse uses the power `2.2` and the inverse primary-conversion matrix.

Consequently, `-Usmpte-c` can change channel balance as well as tone response. It is not an alternative spelling for `gamma`, a generic CRT correction, an HDR mode, or a destination ICC transform. The fixed matrix and curve describe libsixel's implementation; this option does not measure or calibrate a monitor, or establish that a particular DEC terminal expects these values.

## `display-p3`: selected CPU float path

The selected P3 path supports opaque static RGB input, automatic or explicit float precision, linear clustering, linear or gamma working coordinates, CPU K-means with full-frame sampling and no binning, exact `none` lookup, and no dithering or CPU spatial error diffusion (`fs`, `atkinson`, `jajuni`, `stucki`, `burkes`, `sierra:variant=1`, `sierra:variant=2`, `sierra:variant=3`, or `lso2`). `auto` retains the existing palette-count selection of Atkinson or FS. Merge, cover and snap are disabled. It emits normal RGB SIXEL. K-means feedback is disabled. The same selected path accepts fixed palettes through `-m`; their interpretation and image-map construction are specified in [External palette input and output](../palette-construction/external-palettes.md#display-p3-output). Other combinations fail before SIXEL output starts. `-M` exports sRGB8 rather than unmarked P3 values, clipping colors outside sRGB. Other quantizers, positional and interframe dithering, GPU, high-color, HLS, DRCS, macros, delta encoding, animation and alpha-bearing frames are outside this selected path.

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

P3 requires an effective float working format even for `-Wgamma` and an explicit `--precision=8bit` preference. The byte preference does not override precision required by color conversion. Requested precision stays separate from this output requirement, so leaving P3 restores the effective format required by the remaining precision and working-space settings. An explicit float request remains float after leaving P3; automatic gamma working coordinates return to the ordinary byte preference.

Gamma working coordinates use the existing sRGB transfer function with sign-preserving extended values. FS computes its error against the inverse of the actual sent palette. [Dither clamp policy](../palette-application/clamp-policy.md) controls feedback while retaining original extended coordinates; `-0none` propagates the unrestricted residual. For example, errors `(0.125, -0.125, 0.0625)` sent to a following source `(1, -0.03125, 0.125)` with the FS right-neighbor weight `7/16` produce `(1.0546875, -0.0859375, 0.15234375)` before that next lookup. The direct test observes this input to the real lookup. The CLI two-color test checks independently derived source-group means, paint-time wire agreement with a raster FS reference calculation, and actual FS dispatch. Its painted result is also unchanged when error propagation is omitted, so detecting missing propagation or an intermediate clamp belongs to the direct next-lookup test and its code mutations. The other few-color CLI witnesses can optimize FS away because every source color fits in the palette; they establish option acceptance and wire values, not error diffusion. Nonfinite working samples or errors fail instead of entering lookup, and a later valid apply on the same policy recovers. The same feedback policy applies to ordinary output; it is independent of the output primaries.

Repeated `-U` settings replace the output definition. Reusing one encoder and output object for P3, gamma and P3 again restores the corresponding behavior on each job. The private precision opt-in is scoped to one palette-generation call and does not change the installed component request structures.

## Selected ordinary sRGB float path

The same extended-palette/RGB101 preparation is selected for ordinary `-Ugamma` when a static opaque RGB frame already uses float storage or explicit float precision, and the options match the CPU K-means path described above: linear clustering, gamma/linear working space, full-frame sampling, no binning, exact lookup, none or the CPU spatial diffusion methods listed above (including `auto`), and disabled merge/cover/snap. It does not promote byte-eligible input merely to select this algorithm. Other settings retain their existing path.

Palette generation retains extended coordinates. The destination palette is clamped and rounded to RGB101 once, then those codes are decoded back into the working coordinates before lookup and error diffusion. The writer emits the stored codes directly. Source color interpretation, working coordinates and destination RGB definition remain separate decisions. The `dither_contract` trace reports `rgb101=1` for this path and `rgb101=0` for legacy palette finalization.

### Spatial diffusion and the sent palette

Each admitted spatial kernel consumes the authoritative float palette reconstructed from the sent RGB101 codes. Its residual is computed against those coordinates, including canonical negative or above-one values for P3. No byte palette round trip is inserted. This changes palette preparation and its lookup/error reference, not the kernel weights or the clamp policy. The Stucki and Sierra-2 names retain the [documented libsixel coefficient variants](../palette-application/dithering/README.md); LSO2 retains its error-dependent coefficient table. `auto` retains the completed-palette-count rule, with the existing exact-color optimization still able to disable unnecessary diffusion.

The whole-encoder tests independently fix the extended source centroid and final wire values for each method, both output definitions and both working transfers. Separate two-pixel kernel tests fix the next lookup value from a nonzero residual against the sent palette. These tests cover horizontal propagation; existing [clamp tests](../palette-application/clamp-policy.md#test-coverage) cover limiting, FS vertical/serpentine taps and parallel overlap separately. This is not exhaustive stencil, animation, low-color quality or parallel-boundary coverage for every kernel.

## SIXEL syntax and receiver interpretation

With `-t rgb`, a palette definition has this form:

```text
# index ; 2 ; red ; green ; blue
```

Each RGB component is an integer percentage in `0..100`. The `2` selects the RGB form of the color definition. It does not identify sRGB, linear RGB, SMPTE-C, Display P3, a transfer curve, or a display profile. SIXEL provides no ICC payload or output-space tag for the receiver to recover the `-U` setting. See [SIXEL color registers](../../sixel-format/README.md#graphics-color-introducer).

`-t hls` instead serializes the converted output RGB triplet as DEC HLS coordinates. It changes the syntax of the color definition; it does not make `-U` an HLS working-space selector or add color-management metadata.

Display behavior therefore depends on the receiving terminal and display stack. A browser preview or a decoded PNG that treats all three streams as ordinary RGB does not show three correctly color-managed renditions. For a meaningful comparison of alternative output spaces, the consumer must know the chosen space outside the SIXEL stream.

### Negotiation through the proposed color protocol

The [Terminal graphics color management proposal](../../protocols/graphics-color-management.md) defines a receiver-independent agreement for these RGB numbers. It separates an explicit sRGB policy from a reported device-input definition, provides DECRQSS save/restore for each target's policy, and specifies dedicated color-definition queries. Protocol support must be established by the session's replies; a P3-capable display alone does not establish the receiver's input interpretation.

For ordinary SIXEL, a controller using this proposal can display the selected `-Udisplay-p3` output with the following transaction. `CSI`, `DCS`, and `ST` denote the control bytes defined in the proposal, and `M` is the exact saved policy.

| Step | Exchange or action |
| --- | --- |
| Discover target 1 | Send `CSI ?1./r`; require a complete valid reply with `A=1`, `S2=1`, and `C=4`. For example, `DCS 1;1;1;1;4./r ST` also advertises expansion support. |
| Save policy | Send `DCS $q1./p ST`; save `M` from `DCS 1$r1;M./p ST`, preserving `0` versus `1`. |
| Select device input | Send `CSI 1;2./p`; query again and require policy `2`. |
| Display | Send the bytes generated with `-Udisplay-p3`; new RGB definitions are interpreted as standard Display P3. |
| Restore | Send `CSI 1;M./p`; verify the saved value, including on recoverable errors. |

For a color-preserving sRGB comparison, generate `-Ugamma` output and set and verify policy `1`. Feeding gamma sRGB values under a P3 device policy reinterprets those values in the wider basis; it does not establish correct sRGB rendering. Policy `3` explicitly requests the proposal's component expansion and serves a different purpose from source-to-destination color conversion.

Explicit `img2sixel -Udisplay-p3` and `-Ugamma` perform none of these color-protocol exchanges. The CLI-only `-Uauto` manages discovery and the transaction above, accepting the named sRGB and Display P3 definitions. An available target with an unsupported definition uses verified policy `1` and gamma output; P3 requires both `C=4` and `S2=1`. A final automatic request cannot be combined with DRCS (`-@`): the implemented transaction is for ordinary SIXEL (`T=1`), while DRCS uses a separate `T=2` policy. This combination fails before any color query or image output, regardless of the eventual advertised definition. A later explicit `-Ugamma` clears auto and retains the existing DRCS behavior. Neither the library `setopt` API nor bindings accept `auto`.

The reply parser enforces the proposal's [numeric field and simple-RGB rules](../../protocols/graphics-color-management.md#simple-rgb-definition), including rejection of the reserved saturation value `9999`. A simple definition has eight to ten fields: the five common fields, three mandatory base-9999 gamma limbs, and optional primaries/white identifiers. It remains an unsupported transform and therefore uses verified sRGB fallback only after its syntax and reconstructed gamma range pass validation. The five-field named sRGB/P3 replies and their negotiation steps above are unchanged.

The automatic route is limited to images sent to stdout as a TTY, with no `-o` option or with every `-o` argument exactly `-`. Any other outfile argument excludes negotiation, including `-o file -o -`, whose actual output remains a file under the existing outfile semantics. File/pipe destinations keep the interchange default, gamma sRGB, regardless of a terminal attached to stderr. An explicit `-Udisplay-p3` remains a user-selected representation even when saving a file; it requires a consumer that knows how to interpret that file.

During the POSIX query phases, the CLI temporarily disables the terminal's suspend character so a protocol SUB byte reaches the reply parser without suspending the process or flushing input. A keyboard suspend key, normally Ctrl-Z, consequently becomes protocol input during those phases. Each request/reply exchange shares one deadline across its request write and reply reads; a separate control write has its own deadline. Each deadline can be configured up to ten seconds, and a phase containing multiple exchanges has no combined ten-second limit. The original suspend character and other terminal settings are restored before synchronous encoding and during cleanup. Signal handling remains enabled, and SIGINT or SIGTERM can cancel the exchange.

The transport blocks SIGINT and SIGTERM while checking cancellation, then uses `pselect` to restore the caller's signal mask atomically with the I/O wait. A cancellation signal enabled by that mask therefore cannot be lost between the check and the wait, even when output is blocked. Every return restores the caller's mask, and unrelated signals remain enabled according to that mask. Both build systems require this `pselect` surface through the shared terminal-I/O capability probe.

The default remains `-Ugamma`. `SIXEL_OUTPUT_COLORSPACE` supplies a CLI default using the same case folding, unambiguous prefixes and named choices; it takes only a base name, without suboptions. An explicit `-U` overrides that environment value, and the last `-U` replaces the whole earlier request. Empty environment values are unset. `-Uauto:timeout_ms=N` and compact `-Uauto:TN` set a decimal deadline in `1..10000` milliseconds, overriding `SIXEL_OUTPUT_COLORSPACE_AUTO_TIMEOUT_MS`; the default is 200 ms. The timeout environment is relevant only to a final automatic request, and an explicit timeout overrides even an invalid environment timeout. These CLI settings also work through `--env NAME=VALUE` without outranking explicit options.

Only a completely silent first capability exchange, a valid unavailable reply, or unavailable local TTY access allows unconfirmed legacy gamma output. This does not prove that the receiver currently interprets RGB as sRGB. A partial or malformed response fails before sending an image. After a timeout or loss of reply correlation, no later query is issued on that job/TTY; saved policy restoration is write-only and bounded. An available target whose policy cannot be saved or verified receives no image. Normal restoration is queried and verified, and restoration or terminal-settings failure cannot produce a successful exit.

Capability may be reused within one invocation, but each image's current policy is saved and restored separately. The CLI releases its reader and terminal settings before synchronous encoding, allowing the existing library OSC11/CPR handling to retain its own lifecycle. Encoding conditions remain those of the selected P3 path: an incompatible input or option can fail after the policy was selected. The CLI preserves that error and restores the original policy without rereading the input or retrying gamma encoding. When a failed control write or encode may leave SIXEL/control framing unfinished, bounded CAN/ST precedes recovery. SIGINT/SIGTERM observed during negotiation, encoding or restoration stops subsequent inputs.

## Precision, rounding, and cost

The legacy output encoder converts a private palette copy and preserves the reusable dither palette in working coordinates. When a usable float32 palette is available, it converts those typed values before packing bytes. Otherwise it converts the byte palette. The selected P3 and ordinary sRGB float paths commit their RGB101 palette before lookup. Increasing working precision can reduce conversion rounding, but it cannot increase the precision of SIXEL's integer percentage fields.

For RGB definitions, the byte path rounds a channel `b` with integer arithmetic:

```text
percentage = (b * 100 + 127) / 255
```

The float path rounds a clamped normalized channel directly with `floor(channel * 100 + 0.5)`. An exported 8-bit palette and a decoded SIXEL palette can therefore differ through their respective byte and percentage quantization steps. They are not generally byte-identical merely because both use the same `-U` value.

For generated palettes with exact [snap policy](../palette-construction/snap-policy/README.md) enabled at `rate=1`, output finalization follows the `-U` conversion and puts the palette onto the reversible RGB percentage grid. This includes SMPTE-C output. It prevents the conversion from leaving the final palette off that grid; it does not remove clipping or make every earlier color conversion lossless.

For `K` palette entries, this conversion is `O(K)` work and uses `O(K)` palette storage, rather than converting all `N` image pixels again. `-U` alone does not request a float32 image buffer. This cost model applies to the ordinary palette path; it is not a measured whole-program speed comparison. Different numeric color definitions can change serialized byte length even when index assignment is unchanged.

## Fixed palettes and palette export

`-M` always exports sRGB8 in ACT, JASC PAL, RIFF PAL and GPL. This applies to every `-U` choice, including the selected Display P3 path. It converts the prepared working palette before wire-specific conversion and final rounding. See [External palette input and output](../palette-construction/external-palettes.md#exporting-a-palette-with-m) for the interchange contract and its wide-gamut limitation.

The preparation of fixed palettes also matters:

| Palette source | Current preparation and output boundary |
| --- | --- |
| Generated palette | Constructed in `-X` and prepared for `-W`. The selected P3 and ordinary sRGB float paths finalize RGB101 before application; other paths convert to `-U` afterward. |
| Image mapfile, such as `-m swatches.png` | Loaded as an image and prepared in the selected working representation before output conversion. Loader/CMS settings can affect its colors. |
| Structured ACT/PAL/GPL input or built-in `-b` palette | sRGB8 entries are converted to `-W` once, with float coordinates retained for non-gamma workspaces. They are never interpreted as native working-space bytes. |
| Monochrome `-e` | Uses a fixed black/white palette and bypasses palette construction. Black and white alone are poor probes of a nonlinear output transfer. |

Fixed palettes bypass generated-palette snap processing. Structured-file and built-in entries retain their sRGB interpretation under `-Wlinear`; image mapfiles still pass through their separate image-loader and palette-construction route.

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

The exported first entry is `128 64 32`; the black entry stays `0 0 0`. Replacing only `-Ulinear` gives:

| Output option | First entry in the exported PAL | First SIXEL RGB definition |
| --- | --- | --- |
| `-Ugamma` | `128 64 32` | `#0;2;50;25;13` |
| `-Ulinear` | `128 64 32` | `#0;2;22;5;2` |
| `-Usmpte-c` | `128 64 32` | `#0;2;51;25;15` |

To inspect the wire definitions, replace `-M pal:- -o /dev/null` with `-o output.six` and inspect that file. The table describes the byte-palette path with snap bypassed; it is not an expected-value table for arbitrary generated or float32 palettes. Its purpose is to show separately the RGB conversion, 8-bit export, and percentage serialization boundaries.

## Scope and current gaps

The conversion described here belongs to `sixel_encode_dither`, including ordinary generated and supplied palette output. An option being accepted does not establish that every alternate encoder consumes it.

The current `-I` [high-color path](high-color/README.md) dispatches to `sixel_encode_highcolor`, which normalizes pixel format and emits changing palette registers without the legacy `source_colorspace` to output `colorspace` conversion above. Changing only a legacy `-U` choice currently does not provide the requested output-space conversion on that path. This is an implementation gap. The P3 path explicitly rejects high-color output.

Likewise, `-U` does not configure decoder interpretation, embed an output ICC profile, change loader CMS, or replace the [PNG writer's snapshot policy](../../writers/png/README.md). The static-image stage explanation should not be extended to [delta encoding](delta-encoding.md) without accounting for its retained display-color state across frames.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Covered observation | Owning test |
| --- | --- | --- |
| OC-01 | A generated two-entry CIELAB byte palette exports independently derived gamma RGB8 values through `-M gpl:-`; solver entry order is not fixed. | [tests/quant/palette/usage/0197_mapfile_output_converts_cielab_to_gamma.t](../../../tests/quant/palette/usage/0197_mapfile_output_converts_cielab_to_gamma.t) |
| OC-02 | A built-in palette preserves its supplied bytes after sRGB-to-linear working conversion and sRGB export, even with `-Ulinear`. | [tests/quant/palette/usage/0143_builtin_palette_working_output_colorspace_stable.t](../../../tests/quant/palette/usage/0143_builtin_palette_working_output_colorspace_stable.t) |
| OC-03 | A structured PAL palette preserves its supplied bytes after sRGB-to-linear working conversion and sRGB export, even with `-Ulinear`. | [tests/quant/palette/usage/0144_mapfile_pal_working_output_colorspace_stable.t](../../../tests/quant/palette/usage/0144_mapfile_pal_working_output_colorspace_stable.t) |
| OC-04 | A PNG image mapfile exports the exact expected converted palette under `-Wlinear -Ulinear` with CMS disabled. | [tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t](../../../tests/quant/palette/usage/0145_mapfile_png_working_output_colorspace_deterministic.t) |
| OC-05 | The corresponding PNG image-mapfile path remains deterministic when ICC metadata is present but CMS is explicitly disabled. | [tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t](../../../tests/quant/palette/usage/0156_mapfile_png_icc_working_output_colorspace_deterministic.t) |
| OC-06 | Exact snap and SMPTE-C output conversion produce independently expected wire values. | [tests/quant/palette/0047_snap_smptec_grid.t](../../../tests/quant/palette/0047_snap_smptec_grid.t) |
| OC-07 | P3 ICC RGB8 PNG produces independently derived P3 RGB101 values through the actual libpng/builtin CMS path. | [tests/cli/color/0005_libpng_p3_8_wire.t](../../../tests/cli/color/0005_libpng_p3_8_wire.t) |
| OC-08 | The corresponding RGB16 PNG retains its original sample precision through P3 RGB101 output. | [tests/cli/color/0006_libpng_p3_16_wire.t](../../../tests/cli/color/0006_libpng_p3_16_wire.t) |
| OC-09 | Untagged sRGB PNG colors are converted to P3 rather than reinterpreted as P3 values. | [tests/cli/color/0007_libpng_srgb_to_p3_wire.t](../../../tests/cli/color/0007_libpng_srgb_to_p3_wire.t) |
| OC-10 | P3 ICC RGB16 TIFF orientation 6 preserves displayed geometry and independently derived P3 RGB101 values. | [tests/cli/color/0008_libtiff_p3_16_wire.t](../../../tests/cli/color/0008_libtiff_p3_16_wire.t) |
| OC-11 | Extended canonical primary values survive K-means and the stored RGB101 inverse reconstructs the expected canonical float palette. | [tests/quant/palette/0051_p3_palette_values.t](../../../tests/quant/palette/0051_p3_palette_values.t) |
| OC-12 | Exact lookup selects the inverse of the sent RGB101 palette, including a decision that changes after output quantization. | [tests/quant/palette/0052_p3_sent_palette_lookup.t](../../../tests/quant/palette/0052_p3_sent_palette_lookup.t) |
| OC-13 | A float solver allocation failure cannot succeed through a byte retry; ordinary generation and later P3 generation recover without a leaked opt-in or stale wire palette. | [tests/quant/palette/0053_p3_palette_failure_reset.t](../../../tests/quant/palette/0053_p3_palette_failure_reset.t) |
| OC-14 | One encoder and output object can encode P3, gamma and P3 again with the correct restored interpretation. | [tests/quant/palette/0054_p3_encoder_reuse.t](../../../tests/quant/palette/0054_p3_encoder_reuse.t) |
| OC-15 | A structured sRGB palette retains all entries and uses inverse-projected P3 RGB101 colors for linear nearest-color lookup. | [tests/quant/palette/0055_p3_mapfile_linear.t](../../../tests/quant/palette/0055_p3_mapfile_linear.t) |
| OC-16 | P3 palette export retains the prepared sRGB values before destination rounding. | [tests/quant/palette/0056_p3_palette_export_srgb.t](../../../tests/quant/palette/0056_p3_palette_export_srgb.t) |
| OC-17 | Unsupported bluenoise dithering is rejected before the P3 writer starts. | [tests/quant/palette/0057_p3_unsupported_diffusion_rejected.t](../../../tests/quant/palette/0057_p3_unsupported_diffusion_rejected.t) |
| OC-18 | Gamma working coordinates retain the extended canonical P3 red through its final (100,0,0) wire value. | [tests/quant/palette/0058_p3_gamma_working.t](../../../tests/quant/palette/0058_p3_gamma_working.t) |
| OC-19 | Automatic P3 precision promotes gamma working coordinates to float without changing the base request. | [tests/quant/palette/0059_p3_precision_auto.t](../../../tests/quant/palette/0059_p3_precision_auto.t) |
| OC-20 | The P3 float requirement overrides an explicit byte preference. | [tests/quant/palette/0060_p3_precision_8bit_promoted.t](../../../tests/quant/palette/0060_p3_precision_8bit_promoted.t) |
| OC-21 | Automatic reset, explicit float/byte requests and leaving/reentering P3 preserve requested and effective state on one encoder. | [tests/quant/palette/0061_p3_precision_reuse.t](../../../tests/quant/palette/0061_p3_precision_reuse.t) |
| OC-22 | Nonzero FS errors reach the next real lookup with independently derived extended values. | [tests/quant/palette/0062_p3_fs_error.t](../../../tests/quant/palette/0062_p3_fs_error.t) |
| OC-23 | Explicit hard feedback preserves the original extended source while bounding added compensation. | [tests/quant/palette/0063_fs_hard_source_bounds.t](../../../tests/quant/palette/0063_fs_hard_source_bounds.t) |
| OC-24 | A nonfinite P3 sample fails before lookup and the same policy subsequently processes valid samples. | [tests/quant/palette/0064_p3_fs_nonfinite_recovery.t](../../../tests/quant/palette/0064_p3_fs_nonfinite_recovery.t) |
| OC-25 | P3 RGB8 PNG retains independent RGB101 values with gamma working coordinates and automatic precision. | [tests/cli/color/0009_libpng_p3_gamma_auto_wire.t](../../../tests/cli/color/0009_libpng_p3_gamma_auto_wire.t) |
| OC-26 | P3 RGB16 PNG reduced to two colors matches independent source-group means and the paint-time wire from a raster FS reference calculation, and reports actual FS dispatch; this fixture does not independently detect omitted error propagation. | [tests/cli/color/0010_libpng_p3_gamma_fs_wire.t](../../../tests/cli/color/0010_libpng_p3_gamma_fs_wire.t) |
| OC-27 | Untagged sRGB PNG is color-converted to independent P3 RGB101 values with automatic gamma processing and an accepted FS request; the few-color optimization needs no dithering. | [tests/cli/color/0011_libpng_srgb_to_p3_fs_wire.t](../../../tests/cli/color/0011_libpng_srgb_to_p3_fs_wire.t) |
| OC-28 | Oriented RGB16 TIFF retains geometry and independent P3 RGB101 with automatic linear processing and an accepted FS request; the few-color optimization needs no dithering. | [tests/cli/color/0012_libtiff_p3_auto_fs_wire.t](../../../tests/cli/color/0012_libtiff_p3_auto_fs_wire.t) |
| OC-29 | Exact original policies 0, 1, 2 and 3 surround P3 output checked against independent RGB101 values. | [tests/cli/color/session/0001_save_0.t](../../../tests/cli/color/session/0001_save_0.t), [tests/cli/color/session/0002_save_1.t](../../../tests/cli/color/session/0002_save_1.t), [tests/cli/color/session/0003_save_2.t](../../../tests/cli/color/session/0003_save_2.t), [tests/cli/color/session/0004_save_3.t](../../../tests/cli/color/session/0004_save_3.t) |
| OC-30 | An available sRGB target selects and verifies policy 1. | [tests/cli/color/session/0005_srgb.t](../../../tests/cli/color/session/0005_srgb.t) |
| OC-31 | Initial complete silence or valid unavailability selects unconfirmed gamma output. | [tests/cli/color/session/0007_silent.t](../../../tests/cli/color/session/0007_silent.t), [tests/cli/color/session/0006_legacy.t](../../../tests/cli/color/session/0006_legacy.t) |
| OC-32 | Unsupported definitions and P3 without policy-2 support use verified sRGB output. | [tests/cli/color/session/0008_unsupported.t](../../../tests/cli/color/session/0008_unsupported.t), [tests/cli/color/session/0049_simple.t](../../../tests/cli/color/session/0049_simple.t), [tests/cli/color/session/0050_icc.t](../../../tests/cli/color/session/0050_icc.t), [tests/cli/color/session/0051_p3_no_s2.t](../../../tests/cli/color/session/0051_p3_no_s2.t) |
| OC-33 | Partial or invalid capabilities fail without an image. | [tests/cli/color/session/0009_partial.t](../../../tests/cli/color/session/0009_partial.t), [tests/cli/color/session/0010_invalid.t](../../../tests/cli/color/session/0010_invalid.t) |
| OC-34 | An unsaved terminal policy is never overwritten and receives no image. | [tests/cli/color/session/0011_save_failure.t](../../../tests/cli/color/session/0011_save_failure.t), [tests/cli/color/session/0012_save_timeout.t](../../../tests/cli/color/session/0012_save_timeout.t) |
| OC-35 | Failed policy confirmation restores the saved value without another query or image. | [tests/cli/color/session/0013_set_mismatch.t](../../../tests/cli/color/session/0013_set_mismatch.t), [tests/cli/color/session/0014_set_timeout.t](../../../tests/cli/color/session/0014_set_timeout.t) |
| OC-36 | Restoration mismatch or timeout cannot report success. | [tests/cli/color/session/0015_restore_mismatch.t](../../../tests/cli/color/session/0015_restore_mismatch.t), [tests/cli/color/session/0016_restore_timeout.t](../../../tests/cli/color/session/0016_restore_timeout.t) |
| OC-37 | SIGINT and SIGTERM cancel capability or setting phases; restoration is write-only after cancellation. | [tests/cli/color/session/0017_sigint.t](../../../tests/cli/color/session/0017_sigint.t), [tests/cli/color/session/0018_sigterm.t](../../../tests/cli/color/session/0018_sigterm.t), [tests/cli/color/session/0053_set_sigint.t](../../../tests/cli/color/session/0053_set_sigint.t), [tests/cli/color/session/0054_set_sigterm.t](../../../tests/cli/color/session/0054_set_sigterm.t) |
| OC-38 | SIGINT during restoration cannot report success. | [tests/cli/color/session/0055_restore_sigint.t](../../../tests/cli/color/session/0055_restore_sigint.t) |
| OC-39 | An incompatible P3 precision preserves the encoder failure and restores policy. | [tests/cli/color/session/0019_encode_error.t](../../../tests/cli/color/session/0019_encode_error.t) |
| OC-40 | An image arriving through a stdin pipe leaves the output TTY available for replies. | [tests/cli/color/session/0020_stdin.t](../../../tests/cli/color/session/0020_stdin.t) |
| OC-41 | Stdout file/pipe output keeps gamma bytes and has no color controls despite stderr being a TTY. | [tests/cli/color/session/0022_stdout_file.t](../../../tests/cli/color/session/0022_stdout_file.t), [tests/cli/color/session/0021_stdout_pipe.t](../../../tests/cli/color/session/0021_stdout_pipe.t) |
| OC-42 | Only stdout outfile spellings permit auto; a prior real filename remains disqualifying. | [tests/cli/color/session/0023_outfile.t](../../../tests/cli/color/session/0023_outfile.t), [tests/cli/color/session/0024_outfile_dash.t](../../../tests/cli/color/session/0024_outfile_dash.t), [tests/cli/color/session/0025_outfile_file_dash.t](../../../tests/cli/color/session/0025_outfile_file_dash.t) |
| OC-43 | Explicit gamma cancels auto; explicit P3 is honored for a saved file. | [tests/cli/color/session/0026_explicit_gamma.t](../../../tests/cli/color/session/0026_explicit_gamma.t), [tests/cli/color/session/0027_explicit_p3_file.t](../../../tests/cli/color/session/0027_explicit_p3_file.t) |
| OC-44 | Long and compact timeout forms and both inclusive bounds are accepted; the 1 ms parsing case uses file output to avoid scheduler-dependent query latency. | [tests/cli/color/session/0028_timeout_long.t](../../../tests/cli/color/session/0028_timeout_long.t), [tests/cli/color/session/0029_timeout_compact.t](../../../tests/cli/color/session/0029_timeout_compact.t), [tests/cli/color/session/0030_timeout_min.t](../../../tests/cli/color/session/0030_timeout_min.t), [tests/cli/color/session/0031_timeout_max.t](../../../tests/cli/color/session/0031_timeout_max.t) |
| OC-45 | The timeout environment is used unless an explicit timeout overrides it, including an invalid environment value. | [tests/cli/color/session/0032_timeout_env.t](../../../tests/cli/color/session/0032_timeout_env.t), [tests/cli/color/session/0033_timeout_cli_over_env.t](../../../tests/cli/color/session/0033_timeout_cli_over_env.t) |
| OC-46 | Auto inherits case folding and unambiguous base-prefix matching. | [tests/cli/color/session/0034_prefix.t](../../../tests/cli/color/session/0034_prefix.t) |
| OC-47 | Invalid active environment values fail before output. | [tests/cli/color/session/0045_bad_env_base.t](../../../tests/cli/color/session/0045_bad_env_base.t), [tests/cli/color/session/0041_bad_env_timeout.t](../../../tests/cli/color/session/0041_bad_env_timeout.t) |
| OC-48 | Output-space environment defaults accept auto/gamma and treat empty as unset. | [tests/cli/color/session/0042_env_auto.t](../../../tests/cli/color/session/0042_env_auto.t), [tests/cli/color/session/0043_env_gamma.t](../../../tests/cli/color/session/0043_env_gamma.t), [tests/cli/color/session/0044_env_empty.t](../../../tests/cli/color/session/0044_env_empty.t) |
| OC-49 | Explicit CLI choices override the environment and the last request replaces earlier values. | [tests/cli/color/session/0046_base_invalid_ignored.t](../../../tests/cli/color/session/0046_base_invalid_ignored.t), [tests/cli/color/session/0047_cli_over_env.t](../../../tests/cli/color/session/0047_cli_over_env.t), [tests/cli/color/session/0048_repeated_auto.t](../../../tests/cli/color/session/0048_repeated_auto.t) |
| OC-50 | Multiple images reuse capability but save and restore each image's actual current policy. | [tests/cli/color/session/0052_multiple.t](../../../tests/cli/color/session/0052_multiple.t) |
| OC-51 | `--env` supplies the same defaults without overriding an explicit CLI choice. | [tests/cli/color/session/0062_env_assignment.t](../../../tests/cli/color/session/0062_env_assignment.t), [tests/cli/color/session/0063_env_assignment_overridden.t](../../../tests/cli/color/session/0063_env_assignment_overridden.t) |
| OC-52 | The installed library option path rejects CLI-only auto. | [tests/cli/color/session/0064_library_auto.t](../../../tests/cli/color/session/0064_library_auto.t) |
| OC-53 | Complete initial silence retires color queries for later images in the same invocation. | [tests/cli/color/session/0065_multiple_silent.t](../../../tests/cli/color/session/0065_multiple_silent.t) |
| OC-54 | The existing library OSC11 reader owns encoding, and CLI policy restoration resumes after its cleanup. | [tests/cli/color/session/0066_encoder_reader.t](../../../tests/cli/color/session/0066_encoder_reader.t) |
| OC-55 | Auto rejects DRCS before color queries or image output; a later gamma choice and explicit choices keep their existing behavior. | [tests/cli/color/session/0067_drcs_auto.t](../../../tests/cli/color/session/0067_drcs_auto.t), [tests/cli/color/session/0068_drcs_env.t](../../../tests/cli/color/session/0068_drcs_env.t), [tests/cli/color/session/0069_drcs_assignment.t](../../../tests/cli/color/session/0069_drcs_assignment.t), [tests/cli/color/session/0070_drcs_overridden.t](../../../tests/cli/color/session/0070_drcs_overridden.t), [tests/cli/color/session/0071_drcs_gamma.t](../../../tests/cli/color/session/0071_drcs_gamma.t), [tests/cli/color/session/0072_drcs_p3.t](../../../tests/cli/color/session/0072_drcs_p3.t) |
| OC-56 | At a simulated interrupted-encoder boundary, actual CLI controller cleanup ends an unfinished DCS before the independent receiver accepts the saved-policy restoration. This is distinct from the real CLI signal cases. | [tests/cli/color/session/0073_partial_encode.t](../../../tests/cli/color/session/0073_partial_encode.t) |
| OC-57 | Gamma sRGB lookup uses the exact transmitted RGB101 palette. | [tests/quant/palette/0065_srgb_gamma_wire_lookup.t](../../../tests/quant/palette/0065_srgb_gamma_wire_lookup.t) |
| OC-58 | Linear sRGB lookup uses the inverse of exact transmitted RGB101 codes. | [tests/quant/palette/0066_srgb_linear_wire_lookup.t](../../../tests/quant/palette/0066_srgb_linear_wire_lookup.t) |
| OC-59 | Extended source values survive clustering and gamma working conversion before sRGB output. | [tests/quant/palette/0067_srgb_gamma_extended_source.t](../../../tests/quant/palette/0067_srgb_gamma_extended_source.t) |
| OC-60 | Extended source values survive clustering and linear working conversion before sRGB output. | [tests/quant/palette/0068_srgb_linear_extended_source.t](../../../tests/quant/palette/0068_srgb_linear_extended_source.t) |
| OC-61 | Byte-eligible sRGB input retains legacy palette construction under matching algorithm options. | [tests/cli/color/0013_srgb_byte_eligibility.t](../../../tests/cli/color/0013_srgb_byte_eligibility.t) |
| OC-62 | Selecting linear output leaves sRGB8 palette export unchanged. | [tests/quant/mapfile/0124_mapfile_export_linear_output_srgb.t](../../../tests/quant/mapfile/0124_mapfile_export_linear_output_srgb.t) |
| OC-63 | Gamma output exports the prepared palette before RGB101 rounding. | [tests/quant/mapfile/0125_mapfile_export_before_srgb_rgb101.t](../../../tests/quant/mapfile/0125_mapfile_export_before_srgb_rgb101.t) |
| OC-64 | Gamma fixed-palette lookup uses transmitted P3 colors. | [tests/quant/palette/0069_p3_mapfile_gamma.t](../../../tests/quant/palette/0069_p3_mapfile_gamma.t) |
| OC-65 | Linear FS uses the transmitted fixed palette for nonzero error feedback. | [tests/quant/palette/0070_p3_mapfile_fs_linear.t](../../../tests/quant/palette/0070_p3_mapfile_fs_linear.t) |
| OC-66 | Gamma FS uses the transmitted fixed palette for nonzero error feedback. | [tests/quant/palette/0071_p3_mapfile_fs_gamma.t](../../../tests/quant/palette/0071_p3_mapfile_fs_gamma.t) |
| OC-67 | ACT entries retain their sRGB meaning through P3 output. | [tests/quant/palette/0072_p3_mapfile_act.t](../../../tests/quant/palette/0072_p3_mapfile_act.t) |
| OC-68 | RIFF PAL entries retain their sRGB meaning through P3 output. | [tests/quant/palette/0073_p3_mapfile_riff.t](../../../tests/quant/palette/0073_p3_mapfile_riff.t) |
| OC-69 | GPL entries retain their sRGB meaning through P3 output. | [tests/quant/palette/0074_p3_mapfile_gpl.t](../../../tests/quant/palette/0074_p3_mapfile_gpl.t) |
| OC-70 | Reusing an encoder across P3/sRGB/P3 does not reuse a projected fixed palette. | [tests/quant/palette/0075_p3_mapfile_reuse.t](../../../tests/quant/palette/0075_p3_mapfile_reuse.t) |
| OC-71 | An ICC-tagged P3 image map preserves out-of-sRGB colors in a linear workspace. | [tests/quant/palette/0076_p3_mapfile_image_linear.t](../../../tests/quant/palette/0076_p3_mapfile_image_linear.t) |
| OC-72 | An ICC-tagged P3 image map preserves out-of-sRGB colors in a gamma workspace. | [tests/quant/palette/0077_p3_mapfile_image_gamma.t](../../../tests/quant/palette/0077_p3_mapfile_image_gamma.t) |
| OC-73 | P3 fixed-palette export preserves original sRGB values, order, duplicates and unused entries before wire rounding. | [tests/quant/palette/0078_p3_mapfile_export_srgb.t](../../../tests/quant/palette/0078_p3_mapfile_export_srgb.t) |
| OC-74 | SIGINT and SIGTERM arriving between the cancellation check and a blocked I/O wait interrupt promptly, without consuming the request timeout, and preserve the original terminal state and signal mask. | [tests/cli/color/tty/0024_sigint_wait_race.t](../../../tests/cli/color/tty/0024_sigint_wait_race.t), [tests/cli/color/tty/0025_sigterm_wait_race.t](../../../tests/cli/color/tty/0025_sigterm_wait_race.t) |
| OC-75 | Capability fields reserve 9999 and reject larger values without accepting them as unknown identifiers. | [tests/cli/color/auto/0007_numeric_grammar.t](../../../tests/cli/color/auto/0007_numeric_grammar.t) |
| OC-76 | Simple RGB accepts three base-9999 gamma limbs, logical bounds and optional P/W defaults as a valid unsupported definition. | [tests/cli/color/auto/0018_simple_definition_unsupported.t](../../../tests/cli/color/auto/0018_simple_definition_unsupported.t) |
| OC-77 | Invalid simple-RGB limbs, reconstructed overflow, missing/extra fields and payloads cannot establish a usable definition. | [tests/cli/color/auto/0019_simple_definition_invalid.t](../../../tests/cli/color/auto/0019_simple_definition_invalid.t) |
| OC-78 | atkinson: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0001_srgb_gamma_atkinson_wire.t](../../../tests/quant/palette/rgb101_diffusion/0001_srgb_gamma_atkinson_wire.t), [tests/quant/palette/rgb101_diffusion/0002_srgb_linear_atkinson_wire.t](../../../tests/quant/palette/rgb101_diffusion/0002_srgb_linear_atkinson_wire.t), [tests/quant/palette/rgb101_diffusion/0003_p3_gamma_atkinson_wire.t](../../../tests/quant/palette/rgb101_diffusion/0003_p3_gamma_atkinson_wire.t), [tests/quant/palette/rgb101_diffusion/0004_p3_linear_atkinson_wire.t](../../../tests/quant/palette/rgb101_diffusion/0004_p3_linear_atkinson_wire.t) |
| OC-79 | jajuni: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0005_srgb_gamma_jajuni_wire.t](../../../tests/quant/palette/rgb101_diffusion/0005_srgb_gamma_jajuni_wire.t), [tests/quant/palette/rgb101_diffusion/0006_srgb_linear_jajuni_wire.t](../../../tests/quant/palette/rgb101_diffusion/0006_srgb_linear_jajuni_wire.t), [tests/quant/palette/rgb101_diffusion/0007_p3_gamma_jajuni_wire.t](../../../tests/quant/palette/rgb101_diffusion/0007_p3_gamma_jajuni_wire.t), [tests/quant/palette/rgb101_diffusion/0008_p3_linear_jajuni_wire.t](../../../tests/quant/palette/rgb101_diffusion/0008_p3_linear_jajuni_wire.t) |
| OC-80 | stucki: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0009_srgb_gamma_stucki_wire.t](../../../tests/quant/palette/rgb101_diffusion/0009_srgb_gamma_stucki_wire.t), [tests/quant/palette/rgb101_diffusion/0010_srgb_linear_stucki_wire.t](../../../tests/quant/palette/rgb101_diffusion/0010_srgb_linear_stucki_wire.t), [tests/quant/palette/rgb101_diffusion/0011_p3_gamma_stucki_wire.t](../../../tests/quant/palette/rgb101_diffusion/0011_p3_gamma_stucki_wire.t), [tests/quant/palette/rgb101_diffusion/0012_p3_linear_stucki_wire.t](../../../tests/quant/palette/rgb101_diffusion/0012_p3_linear_stucki_wire.t) |
| OC-81 | burkes: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0013_srgb_gamma_burkes_wire.t](../../../tests/quant/palette/rgb101_diffusion/0013_srgb_gamma_burkes_wire.t), [tests/quant/palette/rgb101_diffusion/0014_srgb_linear_burkes_wire.t](../../../tests/quant/palette/rgb101_diffusion/0014_srgb_linear_burkes_wire.t), [tests/quant/palette/rgb101_diffusion/0015_p3_gamma_burkes_wire.t](../../../tests/quant/palette/rgb101_diffusion/0015_p3_gamma_burkes_wire.t), [tests/quant/palette/rgb101_diffusion/0016_p3_linear_burkes_wire.t](../../../tests/quant/palette/rgb101_diffusion/0016_p3_linear_burkes_wire.t) |
| OC-82 | sierra1: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0017_srgb_gamma_sierra1_wire.t](../../../tests/quant/palette/rgb101_diffusion/0017_srgb_gamma_sierra1_wire.t), [tests/quant/palette/rgb101_diffusion/0018_srgb_linear_sierra1_wire.t](../../../tests/quant/palette/rgb101_diffusion/0018_srgb_linear_sierra1_wire.t), [tests/quant/palette/rgb101_diffusion/0019_p3_gamma_sierra1_wire.t](../../../tests/quant/palette/rgb101_diffusion/0019_p3_gamma_sierra1_wire.t), [tests/quant/palette/rgb101_diffusion/0020_p3_linear_sierra1_wire.t](../../../tests/quant/palette/rgb101_diffusion/0020_p3_linear_sierra1_wire.t) |
| OC-83 | sierra2: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0021_srgb_gamma_sierra2_wire.t](../../../tests/quant/palette/rgb101_diffusion/0021_srgb_gamma_sierra2_wire.t), [tests/quant/palette/rgb101_diffusion/0022_srgb_linear_sierra2_wire.t](../../../tests/quant/palette/rgb101_diffusion/0022_srgb_linear_sierra2_wire.t), [tests/quant/palette/rgb101_diffusion/0023_p3_gamma_sierra2_wire.t](../../../tests/quant/palette/rgb101_diffusion/0023_p3_gamma_sierra2_wire.t), [tests/quant/palette/rgb101_diffusion/0024_p3_linear_sierra2_wire.t](../../../tests/quant/palette/rgb101_diffusion/0024_p3_linear_sierra2_wire.t) |
| OC-84 | sierra3: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0025_srgb_gamma_sierra3_wire.t](../../../tests/quant/palette/rgb101_diffusion/0025_srgb_gamma_sierra3_wire.t), [tests/quant/palette/rgb101_diffusion/0026_srgb_linear_sierra3_wire.t](../../../tests/quant/palette/rgb101_diffusion/0026_srgb_linear_sierra3_wire.t), [tests/quant/palette/rgb101_diffusion/0027_p3_gamma_sierra3_wire.t](../../../tests/quant/palette/rgb101_diffusion/0027_p3_gamma_sierra3_wire.t), [tests/quant/palette/rgb101_diffusion/0028_p3_linear_sierra3_wire.t](../../../tests/quant/palette/rgb101_diffusion/0028_p3_linear_sierra3_wire.t) |
| OC-85 | lso2: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0029_srgb_gamma_lso2_wire.t](../../../tests/quant/palette/rgb101_diffusion/0029_srgb_gamma_lso2_wire.t), [tests/quant/palette/rgb101_diffusion/0030_srgb_linear_lso2_wire.t](../../../tests/quant/palette/rgb101_diffusion/0030_srgb_linear_lso2_wire.t), [tests/quant/palette/rgb101_diffusion/0031_p3_gamma_lso2_wire.t](../../../tests/quant/palette/rgb101_diffusion/0031_p3_gamma_lso2_wire.t), [tests/quant/palette/rgb101_diffusion/0032_p3_linear_lso2_wire.t](../../../tests/quant/palette/rgb101_diffusion/0032_p3_linear_lso2_wire.t) |
| OC-86 | auto: extended source centroid reaches independent sRGB/P3 RGB101 values in gamma/linear workspaces. | [tests/quant/palette/rgb101_diffusion/0033_srgb_gamma_auto_wire.t](../../../tests/quant/palette/rgb101_diffusion/0033_srgb_gamma_auto_wire.t), [tests/quant/palette/rgb101_diffusion/0034_srgb_linear_auto_wire.t](../../../tests/quant/palette/rgb101_diffusion/0034_srgb_linear_auto_wire.t), [tests/quant/palette/rgb101_diffusion/0035_p3_gamma_auto_wire.t](../../../tests/quant/palette/rgb101_diffusion/0035_p3_gamma_auto_wire.t), [tests/quant/palette/rgb101_diffusion/0036_p3_linear_auto_wire.t](../../../tests/quant/palette/rgb101_diffusion/0036_p3_linear_auto_wire.t) |
| OC-87 | atkinson: the next lookup receives the independent horizontal residual against sent sRGB/P3 palette coordinates, without clipping extended samples or using byte projections. | [tests/quant/palette/rgb101_diffusion/0037_srgb_gamma_atkinson_residual.t](../../../tests/quant/palette/rgb101_diffusion/0037_srgb_gamma_atkinson_residual.t), [tests/quant/palette/rgb101_diffusion/0038_srgb_linear_atkinson_residual.t](../../../tests/quant/palette/rgb101_diffusion/0038_srgb_linear_atkinson_residual.t), [tests/quant/palette/rgb101_diffusion/0039_p3_gamma_atkinson_residual.t](../../../tests/quant/palette/rgb101_diffusion/0039_p3_gamma_atkinson_residual.t), [tests/quant/palette/rgb101_diffusion/0040_p3_linear_atkinson_residual.t](../../../tests/quant/palette/rgb101_diffusion/0040_p3_linear_atkinson_residual.t) |
| OC-88 | jajuni: the next lookup receives the independent horizontal residual against sent sRGB/P3 palette coordinates, without clipping extended samples or using byte projections. | [tests/quant/palette/rgb101_diffusion/0041_srgb_gamma_jajuni_residual.t](../../../tests/quant/palette/rgb101_diffusion/0041_srgb_gamma_jajuni_residual.t), [tests/quant/palette/rgb101_diffusion/0042_srgb_linear_jajuni_residual.t](../../../tests/quant/palette/rgb101_diffusion/0042_srgb_linear_jajuni_residual.t), [tests/quant/palette/rgb101_diffusion/0043_p3_gamma_jajuni_residual.t](../../../tests/quant/palette/rgb101_diffusion/0043_p3_gamma_jajuni_residual.t), [tests/quant/palette/rgb101_diffusion/0044_p3_linear_jajuni_residual.t](../../../tests/quant/palette/rgb101_diffusion/0044_p3_linear_jajuni_residual.t) |
| OC-89 | stucki: the next lookup receives the independent horizontal residual against sent sRGB/P3 palette coordinates, without clipping extended samples or using byte projections. | [tests/quant/palette/rgb101_diffusion/0045_srgb_gamma_stucki_residual.t](../../../tests/quant/palette/rgb101_diffusion/0045_srgb_gamma_stucki_residual.t), [tests/quant/palette/rgb101_diffusion/0046_srgb_linear_stucki_residual.t](../../../tests/quant/palette/rgb101_diffusion/0046_srgb_linear_stucki_residual.t), [tests/quant/palette/rgb101_diffusion/0047_p3_gamma_stucki_residual.t](../../../tests/quant/palette/rgb101_diffusion/0047_p3_gamma_stucki_residual.t), [tests/quant/palette/rgb101_diffusion/0048_p3_linear_stucki_residual.t](../../../tests/quant/palette/rgb101_diffusion/0048_p3_linear_stucki_residual.t) |
| OC-90 | burkes: the next lookup receives the independent horizontal residual against sent sRGB/P3 palette coordinates, without clipping extended samples or using byte projections. | [tests/quant/palette/rgb101_diffusion/0049_srgb_gamma_burkes_residual.t](../../../tests/quant/palette/rgb101_diffusion/0049_srgb_gamma_burkes_residual.t), [tests/quant/palette/rgb101_diffusion/0050_srgb_linear_burkes_residual.t](../../../tests/quant/palette/rgb101_diffusion/0050_srgb_linear_burkes_residual.t), [tests/quant/palette/rgb101_diffusion/0051_p3_gamma_burkes_residual.t](../../../tests/quant/palette/rgb101_diffusion/0051_p3_gamma_burkes_residual.t), [tests/quant/palette/rgb101_diffusion/0052_p3_linear_burkes_residual.t](../../../tests/quant/palette/rgb101_diffusion/0052_p3_linear_burkes_residual.t) |
| OC-91 | sierra1: the next lookup receives the independent horizontal residual against sent sRGB/P3 palette coordinates, without clipping extended samples or using byte projections. | [tests/quant/palette/rgb101_diffusion/0053_srgb_gamma_sierra1_residual.t](../../../tests/quant/palette/rgb101_diffusion/0053_srgb_gamma_sierra1_residual.t), [tests/quant/palette/rgb101_diffusion/0054_srgb_linear_sierra1_residual.t](../../../tests/quant/palette/rgb101_diffusion/0054_srgb_linear_sierra1_residual.t), [tests/quant/palette/rgb101_diffusion/0055_p3_gamma_sierra1_residual.t](../../../tests/quant/palette/rgb101_diffusion/0055_p3_gamma_sierra1_residual.t), [tests/quant/palette/rgb101_diffusion/0056_p3_linear_sierra1_residual.t](../../../tests/quant/palette/rgb101_diffusion/0056_p3_linear_sierra1_residual.t) |
| OC-92 | sierra2: the next lookup receives the independent horizontal residual against sent sRGB/P3 palette coordinates, without clipping extended samples or using byte projections. | [tests/quant/palette/rgb101_diffusion/0057_srgb_gamma_sierra2_residual.t](../../../tests/quant/palette/rgb101_diffusion/0057_srgb_gamma_sierra2_residual.t), [tests/quant/palette/rgb101_diffusion/0058_srgb_linear_sierra2_residual.t](../../../tests/quant/palette/rgb101_diffusion/0058_srgb_linear_sierra2_residual.t), [tests/quant/palette/rgb101_diffusion/0059_p3_gamma_sierra2_residual.t](../../../tests/quant/palette/rgb101_diffusion/0059_p3_gamma_sierra2_residual.t), [tests/quant/palette/rgb101_diffusion/0060_p3_linear_sierra2_residual.t](../../../tests/quant/palette/rgb101_diffusion/0060_p3_linear_sierra2_residual.t) |
| OC-93 | sierra3: the next lookup receives the independent horizontal residual against sent sRGB/P3 palette coordinates, without clipping extended samples or using byte projections. | [tests/quant/palette/rgb101_diffusion/0061_srgb_gamma_sierra3_residual.t](../../../tests/quant/palette/rgb101_diffusion/0061_srgb_gamma_sierra3_residual.t), [tests/quant/palette/rgb101_diffusion/0062_srgb_linear_sierra3_residual.t](../../../tests/quant/palette/rgb101_diffusion/0062_srgb_linear_sierra3_residual.t), [tests/quant/palette/rgb101_diffusion/0063_p3_gamma_sierra3_residual.t](../../../tests/quant/palette/rgb101_diffusion/0063_p3_gamma_sierra3_residual.t), [tests/quant/palette/rgb101_diffusion/0064_p3_linear_sierra3_residual.t](../../../tests/quant/palette/rgb101_diffusion/0064_p3_linear_sierra3_residual.t) |
| OC-94 | lso2: the next lookup receives the independent horizontal residual against sent sRGB/P3 palette coordinates, without clipping extended samples or using byte projections. | [tests/quant/palette/rgb101_diffusion/0065_srgb_gamma_lso2_residual.t](../../../tests/quant/palette/rgb101_diffusion/0065_srgb_gamma_lso2_residual.t), [tests/quant/palette/rgb101_diffusion/0066_srgb_linear_lso2_residual.t](../../../tests/quant/palette/rgb101_diffusion/0066_srgb_linear_lso2_residual.t), [tests/quant/palette/rgb101_diffusion/0067_p3_gamma_lso2_residual.t](../../../tests/quant/palette/rgb101_diffusion/0067_p3_gamma_lso2_residual.t), [tests/quant/palette/rgb101_diffusion/0068_p3_linear_lso2_residual.t](../../../tests/quant/palette/rgb101_diffusion/0068_p3_linear_lso2_residual.t) |

### Defensive and malformed-input tests

The [automatic CLI cases](../../../tests/cli/color/session) also reject malformed timeout numbers and keys. The controller boundary cases in the same category compile the actual controller with isolated dependency calls, deterministically cancelling during restoration writes or final terminal release while preserving original encoder errors and preventing another image. These are control-flow observations; real process-signal tests are identified above. The [TTY transport tests](../../../tests/cli/color/tty) separately cover absolute deadlines, backpressure, framing, stale bytes, identity/foreground changes and exact state restoration.

### Coverage audit boundary

The first five tests assert exact exported entries. OC-06 checks the output grid and export/wire agreement; it does not independently validate the SMPTE-C conversion matrix. OC-02 and OC-03 establish byte preservation for the named configurations, not perceptual correctness of all non-gamma fixed-palette combinations. OC-05 is not evidence that a CMS transform ran.

P3 values, lookup and reuse are directly covered by OC-07 through OC-28. General defaults, alias and case handling, invalid values, legacy repeated-option precedence, unchanged legacy static index assignment, the numeric example, and the high-color gap are inspectable in the owning code or reproducible with CLI probes, but are not each covered by an independent `-U` regression in this inventory. Multi-input/animation reuse and cross-frame delta interactions also lack an exhaustive output-space matrix here. OC-29 through OC-55 exercise the actual CLI against scripted PTY replies for the selected libpng RGB16 route. P3 pixels use the existing independent wire oracle; gamma fallback/routing compares complete SIXEL frame bytes with an explicit gamma control, with terminal cleanup sequences checked separately. These scripted peers do not establish support in arbitrary terminal products or measure a physical display. C1/C2 transforms remain unimplemented. No image-quality threshold or terminal-compatibility result is inferred from the exact palette checks.

## Implementation references

- [`converters/terminal-color-job.c`](../../../converters/terminal-color-job.c): CLI requests, stdout routing and per-image save/set/verify/encode/restore.
- [`converters/terminal-color-tty.c`](../../../converters/terminal-color-tty.c): private nonblocking TTY access and bounded exchanges.
- [`src/encoder.c`](../../../src/encoder.c): option defaults and parsing, palette-source preparation, output format handoff, and the separate `-M` capture conversion.
- [`src/encoder-core-encode.c`](../../../src/encoder-core-encode.c): private byte/float palette copies, output conversion, exact-snap finalization, and RGB/HLS definitions.
- [`src/colorspace.c`](../../../src/colorspace.c): transfer curves, SMPTE-C matrices, conversion through linear RGB, clipping, and byte/float dispatch.
- [`src/encoder-core-highcolor.c`](../../../src/encoder-core-highcolor.c): the alternate high-color implementation and its current output-conversion gap.
- [Color spaces and loader color management](../../concepts/colorspace/README.md): the wider source-profile, working-space, and output-space model.

The CIELAB export test uses black and white source pixels and the existing typed byte palette boundary. [Independent inverse CIELAB/XYZ/sRGB calculations](../../../tests/data/color-numeric/palette-export.json) predict `(3,0,0)` and `(255,254,254)` after that boundary because the byte a/b representation has no exact neutral midpoint. This exact storage-conversion assertion does not establish lossless float palette retention or freeze K-means representatives from a natural image.

Replacing the typed byte palette boundary requires re-deriving this storage-specific expectation from independent values. The current byte artifacts are not an ideal black/white color contract for a future canonical float palette. Natural-image clustering and decoded-output quality remain covered by the existing working-space quality tests (WC-04 through WC-08 in [Working color space](../palette-application/working-colorspace/README.md#test-coverage)), including CIELAB with the unchanged MS-SSIM floor.
