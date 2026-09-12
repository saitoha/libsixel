# Color Spaces and Loader Color Management

## Scope

A three-component pixel value is not a complete color description. The same numeric triplet can represent different light depending on its primaries, white point, transfer function, and any attached device profile. libsixel therefore keeps color interpretation separate from pixel storage and uses color spaces at several different pipeline boundaries.

This document explains the shared color-space model, linear RGB and XYZ conversion hubs, the ICC profile connection space (PCS), when internal conversions occur, the roles of `-X`, `-W`, and `-U`, and loader color management. The storage types and implicit promotion boundaries are defined in [Pixel-format precision](pixelformat-precision.md); broader quantizer, dither, lookup, speed, and size effects are measured in [Encoder working precision](../functionality/precision.md); palette-construction geometry is measured in [Palette clustering color space](../functionality/clustering-colorspace.md); and palette-application geometry is measured in [Working color space](../functionality/working-colorspace.md).

## Implicit conversions users should expect

**Internal color conversion can occur even when no color-space option is supplied.** Loader CMS, background composition, and the default resize path can require a different representation before palette processing starts. `-Wgamma` selects palette-application coordinates, and `--precision=8bit` requests the byte base path; neither forbids a temporary linear-light float buffer. Likewise, `--cms-engine=none` disables loader CMS but does not disable linear-light resize or composition on loader paths that implement it. These conversions can change intermediate pixel values, memory use, processing time, and the resulting image.

The purpose is to perform each operation in suitable coordinates while preserving the interpretation of the color. The [conversion hubs](#linear-rgb-and-xyz-as-conversion-hubs) explain the mathematics; [lazy conversion at operation boundaries](#lazy-conversion-at-operation-boundaries) identifies the triggers and how to inspect the effective path. Loader behavior is path-specific, so the explanation does not imply that every backend composites in linear light.

## Four concepts that must remain separate

| Concept | Question it answers | libsixel example |
| --- | --- | --- |
| Channel values | What numbers are stored? | `(0.5, 0.2, 0.1)` or `(128, 51, 26)` |
| Pixel format | How are those numbers laid out and represented in memory? | `RGB888`, `RGBA8888`, `RGBFLOAT32`, or `OKLABFLOAT32` |
| Color space | What light or perceptual coordinates do the numbers mean? | gamma-encoded sRGB, linear-light sRGB, Oklab, CIELAB, or DIN99d |
| Color profile and CMS | How are colors described by an input device or file transformed into a known destination space? | an embedded ICC profile interpreted by the builtin, lcms2, or ColorSync engine |

The [pixel-format guide](pixelformat.md) describes memory layout. A pixel-format tag can imply a normal internal color space, but layout and interpretation are still different contracts: copying three bytes does not perform a color conversion, and changing only color-space metadata does not change the pixels.

## Spaces used by libsixel

| CLI name | Meaning in libsixel | Available at |
| --- | --- | --- |
| `gamma` | sRGB primaries and D65 white with the piecewise sRGB transfer function | loader CMS target, `-X`, `-W`, and `-U` |
| `linear` | linear-light sRGB with the same primaries and white point as sRGB | loader CMS target, `-X`, `-W`, and `-U` |
| `oklab` | Oklab opponent coordinates derived from linear sRGB | loader CMS target, `-X`, and `-W` |
| `cielab` | libsixel's normalized CIE 1976 L*a*b* coordinates, derived through D65 XYZ | loader CMS target, `-X`, and `-W` |
| `din99d` | libsixel's normalized DIN99d coordinates derived from CIELAB | loader CMS target, `-X`, and `-W` |
| `smpte-c` | SMPTE-C primaries with the output transfer used by libsixel | `-U` only |

Within this project, `gamma` is not shorthand for an arbitrary power-law curve. It specifically means the sRGB encoded representation. SMPTE-C output uses a different primary conversion and a nominal 2.2 power transfer.

Gamma, linear RGB, and SMPTE-C are device-oriented RGB spaces: their coordinates ultimately describe amounts or encodings of red, green, and blue primaries. Oklab, CIELAB, and DIN99d are opponent or perceptual working spaces designed so that geometric distance is more useful for color decisions. They are internal calculation spaces, not SIXEL palette syntaxes.

The CMS chapter introduces [primaries, white point, and transfer function](#primaries-white-point-and-transfer-function), including what D50 and D65 mean. Those properties explain why two RGB spaces can assign different colors to the same numbers.

## Gamma encoding and linear light

For a normalized sRGB component `C` in `[0, 1]`, libsixel decodes the channel to linear light as:

```text
C_linear = C / 12.92                         when C <= 0.04045
           ((C + 0.055) / 1.055) ^ 2.4       otherwise
```

The inverse transformation is:

```text
C = 12.92 * C_linear                         when C_linear <= 0.0031308
    1.055 * C_linear ^ (1 / 2.4) - 0.055     otherwise
```

The distinction matters whenever values are averaged or mixed. Resizing, alpha composition, and error diffusion are arithmetic operations on colors; performing their additions in gamma-encoded coordinates does not in general produce the same light as performing them in linear RGB. Linear processing is physically meaningful for additive light, while a perceptual space can be more appropriate when the objective is to make numerical errors resemble visible differences.

The sRGB transfer and `srgb-linear` definitions are described by [CSS Color Module Level 4](https://www.w3.org/TR/css-color-4/#predefined-sRGB). Oklab's matrices and design motivation are described in [A perceptual color space for image processing](https://bottosson.github.io/posts/oklab/). libsixel's exact normalizations and conversion constants are defined by [`src/colorspace.c`](../../src/colorspace.c).

## Linear RGB and XYZ as conversion hubs

Color spaces form a connected conversion network. Linear sRGB is libsixel's common internal RGB basis: the general converter first obtains linear sRGB from the source coordinates, then constructs the destination coordinates. XYZ supplies a device-independent connection between RGB primaries and other color descriptions. Its three tristimulus components describe color without choosing a monitor's RGB primaries; `Y` represents luminance on the chosen scale. The white point still matters, so XYZ D50 and XYZ D65 must be distinguished.

For sRGB to Oklab, the conceptual forward route is:

```text
gamma-encoded sRGB
        |
        | decode the sRGB transfer function
        v
linear sRGB ---- RGB-to-XYZ matrix ----> XYZ D65
        |                                 |
        |                                 | XYZ-to-LMS matrix
        |                                 v
        +---- combined RGB-to-LMS ----> LMS cone-response coordinates
                                          |
                                          | component-wise cube roots
                                          v
                                         LMS'
                                          |
                                          | opponent-coordinate matrix
                                          v
                                        Oklab
```

The two routes into LMS are alternatives. In `sixel_linear_to_oklab()`, libsixel uses the combined linear-sRGB-to-LMS matrix directly, followed by cube-root lookup and the opponent transform. It does not allocate an XYZ image or run a separate XYZ pass for every Oklab conversion. Adjacent linear matrices can be combined; the nonlinear sRGB transfer and cube-root stages must retain their order. This is the distinction between a mathematical intermediate and a stored image representation. The [Oklab author's implementation](https://bottosson.github.io/posts/oklab/#converting-from-linear-srgb-to-oklab) provides the direct matrix form.

Other destinations share parts of the route. libsixel's CIELAB conversion explicitly computes XYZ D65 and applies its reference-white-dependent nonlinear transform; DIN99d continues from CIELAB. Reverse transforms recover linear RGB before applying a destination transfer function. Converting to Oklab therefore needs linear-light intermediate values even when no image mixing is requested.

XYZ has another role in CMS: it can serve as the [ICC profile connection space](#pcs-the-connection-between-color-profiles). That profile boundary uses D50 conventions, while the internal sRGB/Oklab route above uses D65. The word "hub" describes reuse of a common basis; it does not mean every stage stores pixels in XYZ or uses the same reference white.

## Why mixing and background composition use linear RGB

For additive-light mixing, the channel values must be proportional to light. For a straight-alpha foreground `F` over an opaque background `B`, each channel is computed as:

```text
C_linear = alpha * F_linear + (1 - alpha) * B_linear
```

Foreground and background must first be interpreted in the same linear RGB basis. Their profiles or transfer functions can differ, so applying the same arithmetic to the original sample numbers is insufficient. Alpha is a coverage/opacity weight and is not sRGB-transfer-decoded. If the next stage needs gamma sRGB, the result is encoded afterward. This is also the order specified by [PNG alpha channel processing](https://www.w3.org/TR/png-3/#13Alpha-channel-processing).

Consider equal contributions of white and black, such as 50% white over opaque black or an equal-area resize average. The linear result is `0.5`, which becomes approximately `0.73536` in encoded sRGB, or byte tone `188`. Averaging encoded values produces `0.5` in sRGB, approximately byte tone `128`, which decodes to only `0.21404` linear light. The darker result is a change in the computed mixture, not merely a storage-rounding difference. The [resize precision experiment](pixelformat-precision.md#measured-resize-quality) demonstrates this distinction with an analytical reference.

XYZ is also linear in light and can express the same additive mixture when the basis, white point, and scaling agree. Linear RGB is convenient here because source samples, resize buffers, and eventual RGB output already use RGB components. Oklab serves a different objective: interpolation in it can produce a perceptually smooth gradient, and distances or centroids can guide palette optimization. Such operations need not reproduce an additive-light mixture. In libsixel, palette clustering follows `-X`, and lookup and error diffusion follow `-W`; their arithmetic is not automatically redirected into linear RGB.

The [background policy](../loader/background-policy.md#colorspace) distinguishes the background's interpretation from the blend arithmetic. Static non-indexed PNG float paths composite in linear light, whereas retained indexed-palette paths and APNG inter-frame blending have different behavior. The [PNG loader comparison](../loader/builtin/png.md#background-source-interpretation-and-blend-arithmetic) records those differences. The equation above explains linear-light composition, not a guarantee that all existing loader paths implement it.

## End-to-end pipeline

The useful mental model is a sequence of explicit interpretation and conversion boundaries:

```text
encoded image bytes
  + format metadata or an embedded ICC profile
                    |
                    v
              image loader
                    |
                    v
       optional loader CMS conversion
       source description -> cms_target
                    |
                    v
       frame {pixels, pixel format, color space}
                    |
       optional resize: enter linear RGB,
       resample, then enter the work format
                    |
          +---------+--------------------------+
          |                                    |
          v                                    v
 palette-building view                    main image view
          |                               converted to -W
       sampling                                 |
          |                                    |
 converted to -X                               |
          |                                    |
 binning -> quantizer                           |
          |                                    |
 generated palette in -X                       |
          +----------- convert -X -> -W -------+
                                               |
                                    lookup and dithering
                                               |
                                  palette conversion -W -> -U
                                               |
                                    SIXEL palette percentages
                                      and palette indices
```

The implementation may reuse storage, clone only the palette-building view, or skip a conversion when source and destination already match. Those optimizations do not change the conceptual contracts. The frame carries its effective pixel format and color space so later stages can choose the correct conversion instead of inferring it from whether resize or another option was requested.

## Lazy conversion at operation boundaries

Here, "lazy" means converting when a consuming operation needs different coordinates or storage. It is implemented through loader branches, frame metadata, and the encoder planner; it is not a cache holding a copy of every image in every color space. A linear intermediate inside one pixel's conversion also does not imply a separate full-frame linear buffer.

| Trigger | Why conversion occurs | Where it happens |
| --- | --- | --- |
| A loader applies a recognized source profile | Source device values must acquire a known interpretation. | Loader CMS connects through the profile transform and produces the effective `cms_target` representation, subject to loader precision/path rules. |
| A loader takes a linear-light background-composition path | Foreground and background must share a linear basis before weighted addition. | Composition happens during loading; the returned frame can already be `LINEARRGBFLOAT32`. |
| Default resize processing is requested | Resampling combines contributions from multiple source pixels. | The planner requests `LINEARRGBFLOAT32` before resize and the effective working format afterward. |
| `-X` or `-W` requires different coordinates | Palette decisions need the selected geometry, even without composition or resize. | Conversion affects the palette-building view or main work view respectively. |
| Final palette coordinates differ from `-U` | The serialized RGB numbers must have the requested output interpretation. | The final palette entries are converted before writing; this does not require converting all image pixels back to output RGB. |

For resize, `sixel_encoding_planner_analyze()` compares the actual frame format with the required resize input and work formats. The encoder inserts pre-resize and post-resize conversion steps only where those formats differ. An already linear float frame needs no pre-resize transfer decoding; an Oklab frame must return to linear RGB for the default resize path and can re-enter Oklab afterward. If resize is absent, there is no resize buffer, although CMS or `-X`/`-W` may independently require conversion. The general color converter likewise skips matching source and destination color spaces.

For example, an opaque `RGB888` source with no applied CMS transform, no resize, and gamma palette policies can stay on the gamma byte path. Adding a resize with `--precision=8bit -Wgamma` introduces a temporary linear float stage:

```text
RGB888 gamma -> LINEARRGBFLOAT32 -> resize -> RGB888 gamma
```

If a loader's background-composition path instead returns linear float and `-Woklab` is selected, default resize processing follows this route:

```text
linear float loader result -> resize in linear RGB -> Oklab work
```

No extra sRGB decoding is needed before that resize. The planner retains loader-originated float precision for subsequent work. These are examples of stage-dependent conversion, not a promise of globally minimal transforms: individual loaders can have additional CMS-target round trips, and alpha/high-depth paths can produce float frames even without actual background blending.

### Inspecting and controlling the effective path

Use verbose planner output to see the source, work, and resize representations together:

```sh
img2sixel --cms-engine=none --precision=8bit -Wgamma \
  -w50% -v -o /dev/null image.png
```

For an opaque byte source that reaches the planner as `RGB888`, the relevant fields are:

```text
formats: source=rgb888 work=rgb888 scale_out=linear-f32
resize: mode=2 input=linear-f32
```

Reading only `work=rgb888` misses the temporary conversion. To investigate a loader-produced float source, use the [loader contract trace](pixelformat-precision.md#observing-the-resolved-path) and check CMS, source alpha, and the resolved background. The planner report begins at the returned frame; it does not enumerate internal PCS calculations or every loader conversion.

The explicit `-j auto:resize_precision=preserve` policy can keep a byte resize in gamma RGB, reducing buffer memory at the cost of different mixing results. It does not disable CMS or loader background processing. Conversely, `--precision=float32` selects float storage but does not by itself make gamma coordinates linear. See [Pixel-format precision](pixelformat-precision.md#operations-that-promote-to-float32) for the exact promotion and preservation rules.

## The three encoder color-space options

| Option | Boundary controlled | Default | Accepted values |
| --- | --- | --- | --- |
| `-X`, `--clustering-colorspace` | coordinates used by binning and the palette solver after sample selection | `gamma`, unless an explicit `-W` supplies the implicit `-X` value | `gamma`, `linear`, `oklab`, `cielab`, `din99d` |
| `-W`, `--working-colorspace` | coordinates used when applying the palette, including lookup and dithering | `gamma` | `gamma`, `linear`, `oklab`, `cielab`, `din99d` |
| `-U`, `--output-colorspace` | RGB coordinate conversion applied to palette entries before serialization | `gamma` | `gamma`, `linear`, `smpte-c` |

### `-X`: palette-construction geometry

`-X` changes the distances, ranges, centroids, and partitions seen by a generated-palette algorithm. It operates on a palette-building view; specifying it does not by itself convert the main image path. After palette construction, the `K` generated entries are converted from `-X` to `-W` before palette lookup and dithering.

An explicit `-X` is authoritative regardless of whether it appears before or after `-W`. When `-W` is specified and `-X` has never been specified, the encoder makes `-X` follow `-W`; this convenience keeps the two geometries matched without preventing an intentional mixed-space configuration. `-X` is ignored by paths that use a fixed palette through `-b`, `-m`, or `-e`, because those paths do not construct a new palette.

### `-W`: palette-application geometry

`-W` selects the shared working coordinates for the main image and the generated palette. Lookup policies compare pixels with palette entries in this space, and dithering propagates its error in this space. Changing `-W` can therefore change selected indices even when the palette itself was generated in another space.

`-Wgamma` can use either the 8-bit or float32 precision path. `linear`, `oklab`, `cielab`, and `din99d` require float32 working storage. This is why a non-gamma `-W` can change memory use and runtime as well as color geometry.

The mathematical role of each accepted space and the controlled quality, runtime, and stream-size comparison are documented in [Working color space](../functionality/working-colorspace.md).

### `-U`: serialized palette coordinates

`-U` does not rerun palette generation or palette lookup. It converts the final palette entries from the frame's working color space to the requested output RGB space before the encoder writes SIXEL color definitions.

The same boundary applies when `-M` writes ACT, PAL, RIFF PAL, or GPL output. These formats receive the `-U` representation, not the internal `-W` coordinates. In particular, the default `-Ugamma` converts a perceptual working palette back to gamma-encoded RGB before it is stored in a reusable palette file.

SIXEL carries numeric palette components but does not embed an ICC profile or an explicit color-space tag. A receiving terminal therefore cannot discover from the stream whether values were prepared as sRGB, linear RGB, or SMPTE-C. `-Ugamma` is the interoperable default; alternate output spaces are meaningful only when the receiving environment is known to interpret the numeric values accordingly.

## Loader color management

For the detailed engine comparison, supported ICC structures, unsupported builtin cases, and quality/performance tradeoffs, see [Loader CMS: builtin and Little CMS](../loader/color-management.md). Loader and CMS engine selection are independent; successful decoding alone does not prove that the source profile was applied.

An embedded ICC profile describes how source sample values relate to a profile connection space. A color-management system uses that description to transform source colors into a known destination. This is different from merely asking the encoder to measure distances in Oklab or linear RGB: `-X` and `-W` assume the frame already has a valid color interpretation, while loader CMS establishes that interpretation from file metadata.

### Primaries, white point, and transfer function

An RGB value gives amounts of three basis colors, but the letters R, G, and B do not specify which red, green, and blue those are. A color-space definition supplies the missing information:

| Property | Meaning | Why conversion needs it |
| --- | --- | --- |
| Primaries | The chromaticities of the red, green, and blue basis colors. Chromaticity describes a color's hue/chromatic character independently of its luminance. | Different primaries span different RGB gamuts and give different colors to the same triplet. |
| White point | The reference chromaticity considered neutral white. For a normalized RGB space, equal full linear channels combine to this white. | Together with the primaries, it fixes their relative scaling for the RGB-to-XYZ matrix and supplies the reference for chromatic adaptation. |
| Transfer function | The mapping between a stored channel number and linear light. | It must be decoded before applying a linear primary-conversion matrix or additive-light mixing. |

For example, sRGB and Display P3 share the sRGB transfer function and D65 white, but use different primaries. Decoding both triplets into linear light therefore does not make their channel values interchangeable: a primary conversion is still required. Conversely, gamma sRGB and linear sRGB share both primaries and white point; their difference is the transfer encoding. [CSS Color 4's predefined RGB spaces](https://www.w3.org/TR/css-color-4/#predefined) specifies these properties separately.

D50 and D65 are standardized daylight references, with correlated color temperatures of approximately 5000 K and 6500 K respectively. D50 is warmer; D65 is bluer. The names specify reference whites, not an instruction to change the terminal display's color temperature. A white point is also not simply the brightest pixel found in an image. ICC PCS uses a D50 reference, while sRGB and Oklab use D65; a profile transform must account for that difference when connecting them.

For matrix-based RGB conversion, the primaries and white point determine a matrix from linear RGB to XYZ. Transfer decoding, this matrix conversion, and any required white-point adaptation answer different questions. Changing only a frame tag performs none of them. More general device profiles can require curves and multidimensional lookup tables rather than a single RGB matrix; the [loader CMS comparison](../loader/color-management.md#supported-profile-structures-and-practical-differences) describes the supported profile structures.

### PCS: the connection between color profiles

PCS means **Profile Connection Space**. It gives profiles a common connection: the source profile describes device values relative to the PCS, and a destination profile describes how to obtain destination device values from that connection. A CMS can pair profiles without requiring a separately authored transform for every source/destination combination.

```text
source device values          PCS                 destination values
(RGB, gray, CMYK, ...)   XYZ or Lab, D50           (for example, sRGB)
          |                    |                          ^
          +-- source profile ->+-- destination profile ---+
```

ICC v2/v4 color profiles use XYZ or Lab PCS representations under D50 reference conditions. PCS names the connecting role; it is not a third coordinate system in addition to XYZ and Lab. Rendering intent also governs the connection, so interpreting PCS requires more than a three-number tuple. See the [ICC introduction](https://www.color.org/getting-started/) and [ICC.1:2022](https://www.color.org/specifications/ICC.1-2022-05.pdf).

For libsixel's builtin CMS, [`src/icc-apply.c`](../../src/icc-apply.c) connects supported profile paths through XYZ D50. A Lab PCS is decoded to that basis. The source-to-sRGB route then applies D50-to-D65 chromatic adaptation and the XYZ-to-linear-sRGB matrix before producing the required RGB representation. Chromatic adaptation relates colors under different reference whites; it is distinct from decoding a transfer curve and cannot be replaced by relabeling D50 numbers as D65. These adjacent matrix operations may be combined without requiring an image buffer for each conceptual step.

| Boundary | Coordinates and reference white | Purpose |
| --- | --- | --- |
| ICC PCS | XYZ or Lab, D50 | Connect source and destination profile interpretations. |
| XYZ in the internal sRGB/Oklab route | XYZ D65 | Relate the sRGB primary basis to other color coordinates. |
| libsixel `linear` | Linear sRGB, D65 | Common internal RGB conversion basis and light-mixing coordinates. |
| libsixel `cielab` | Project-normalized CIELAB derived using D65 | Palette/CMS-target working coordinates; distinct from ICC Lab PCS encoding and white point. |

An intermediate PCS is therefore neither the loader's requested `cms_target` nor the encoder's `-W`. In particular, `cms_target=cielab` does not request that the loader expose its raw ICC Lab PCS. The [builtin CMS architecture](../loader/builtin-cms.md#pixel-execution-and-numerical-behavior) describes supported transforms and precision; its [rendering-intent policy](../loader/builtin-cms.md#rendering-paths-and-fallback) also limits which profile semantics builtin implements. lcms2 and ColorSync own their backend-specific profile connection behavior.

### Loader CMS defaults and engine selection

Loader CMS serves primarily as input normalization: it prevents images authored for different primaries or transfer functions from being misinterpreted as sRGB. The default `-Ugamma` output remains gamma-encoded sRGB; CMS does not expand the output gamut. Mapping that result to a physical display, or deliberately enhancing it beyond sRGB, belongs to the receiving terminal and display stack. See [Why loader CMS matters](../loader/color-management.md#why-loader-cms-matters) for the limits of this division of responsibility.

For participating `img2sixel` and `lsqa` loaders, CMS defaults to `auto`. Library API loader defaults remain disabled. Use `-# ENGINE` or `--cms-engine=ENGINE` to supply a process-wide loader default:

| Engine | Behavior |
| --- | --- |
| `none` | disable libsixel loader CMS |
| `auto` | prefer lcms2, then ColorSync on macOS, then the builtin engine |
| `builtin` | use libsixel's internal ICC implementation |
| `lcms2` | request Little CMS; if unavailable, resolve through the automatic fallback chain |
| `colorsync` | request ColorSync; if unavailable, resolve through the automatic fallback chain |

The global selection applies to the builtin, libpng, libjpeg, libwebp, and libtiff loader components. A loader-specific `cms_engine` suboption or its backend-specific environment variable overrides the process-wide default. Other loader backends can have native framework color behavior outside this switch, so `--cms-engine=none` should be read as “disable the libsixel CMS path,” not as a universal promise that every operating-system decoder returns untouched device samples.

### Source interpretation and internal target

When an enabled loader recognizes usable color metadata, it first resolves the file's source interpretation and converts profile-managed samples to the project's known RGB basis. It then converts the frame to the requested internal CMS target. The exact metadata precedence, supported source profile classes, and malformed-profile fallback are format- and loader-specific; they must not be inferred from a different loader's behavior.

The common loader suboption is:

```text
cms_target=gamma|linear|cielab|oklab|din99d
```

Its short suboption form is `Tvalue`, its environment fallback is `SIXEL_LOADER_CMS_TARGET_COLORSPACE`, and its default is `linear`. This target controls the frame representation produced after a successful CMS conversion. It does not set `-X`, `-W`, or `-U`; the encoder still performs any conversions required by those later boundaries.

For example, this command asks the builtin loader to apply an available CMS backend and leave the managed frame in linear-light float32 before the encoder applies its default gamma working and output policies:

```sh
img2sixel --cms-engine=auto --loaders='builtin:cms_target=linear!' image.png
```

This command keeps palette construction and palette application in Oklab after the same loader-side normalization, then converts the palette to gamma-encoded sRGB for SIXEL output:

```sh
img2sixel --cms-engine=auto --loaders='builtin:cms_target=linear!' -Xoklab -Woklab -Ugamma image.png
```

### Loader precision preference

`prefer_8bit=0|1` (`V0` or `V1`, environment fallback `SIXEL_LOADER_PREFER_8BIT`) controls the representation requested after CMS conversion. The default `0` preserves the selected target in float32. The current `prefer_8bit=1` contract requests `RGB888`; because `RGB888` is gamma-encoded sRGB storage, it trades the selected float target for an 8-bit gamma representation. It can reduce memory and conversion cost, but it is not a byte-precision encoding of Oklab, CIELAB, DIN99d, or linear RGB.

### Rendering intent

`cms_intent=INTENT[+INTENT][!]` (`Rvalue`, environment fallback `SIXEL_LOADER_CMS_RENDERING_INTENT`) selects the order in which the CMS tries `perceptual`, `relative`, `saturation`, and `absolute` rendering intents. Without the exclusive `!`, omitted defaults remain available as fallbacks. Rendering intent governs profile gamut mapping; it does not replace `cms_target`, `-X`, `-W`, or `-U`.

## Common configurations

### Profile-aware input with default encoder geometry

```sh
img2sixel --cms-engine=auto image.png
```

The loader may normalize a recognized source profile, while palette generation, lookup, dithering, and emitted palette values retain their default gamma policy. A linear float loader result can still be converted later because its frame metadata records the actual representation.

### Perceptual palette construction with gamma lookup

```sh
img2sixel --cms-engine=auto -Xoklab -Wgamma -Ugamma image.png
```

Only the palette-building view enters Oklab. The generated palette returns to gamma sRGB before lookup and dithering, and the emitted SIXEL palette remains gamma sRGB.

### Perceptual construction and application

```sh
img2sixel --cms-engine=auto -Woklab -Ugamma image.png
```

Because no explicit `-X` is present, `-X` follows `-Woklab`. Palette construction, lookup, and dithering use Oklab float32, then the palette is converted to gamma sRGB for output.

### Fixed palette

```sh
img2sixel -m palette.map -Xoklab -Wlinear -Ugamma image.png
```

The map supplies the palette, so `-Xoklab` has no palette-construction work to control. `-Wlinear` can still affect palette application, and `-Ugamma` still governs final palette serialization.

## Gamut, clamping, and precision

Changing coordinates does not make the destination RGB gamut larger. Conversion from a perceptual space or from SMPTE-C to the output RGB basis can produce values outside the representable cube; libsixel clamps at conversion boundaries where a bounded RGB or encoded channel is required. Float32 preserves substantially more intermediate precision than byte storage, but it cannot preserve a color that the destination gamut cannot represent.

This distinction is especially important when evaluating `-X` or `-W`: a perceptual space can improve the geometry of an optimization while a later RGB conversion still clips some results. Quality measurements must state the loader CMS engine and target, effective precision, `-X`, `-W`, and `-U`, because changing any one of them can change the comparison.

## Common mistakes

- Treating `RGB888` as a complete source profile. It describes three-byte storage and implies libsixel's gamma representation after normalization, but it does not explain how unconverted file samples were authored.
- Treating `cms_target` as an alias for `-W`. The former is a loader output contract; the latter is an encoder processing contract.
- Treating `-X` as a global conversion. It changes generated-palette geometry and leaves the main image path alone until another stage requires conversion.
- Treating `-W` as the wire color space. The final palette conversion is controlled by `-U`.
- Assuming `-U` embeds color management metadata. SIXEL has no ICC-profile field, so the terminal receives only numeric palette components.
- Comparing spaces without controlling precision and loader behavior. A float32 promotion or a profile conversion can otherwise be mistaken for an effect of the named palette space.

## Implementation references

- [`src/colorspace.c`](../../src/colorspace.c) owns component transfer functions, RGB primary conversions, perceptual transforms, normalization, clamping, lookup tables, and accelerated conversion paths.
- [`src/cms.c`](../../src/cms.c) owns CMS engine selection, ICC profile handling, rendering-intent order, and the builtin/lcms2/ColorSync abstraction.
- [`src/icc-apply.c`](../../src/icc-apply.c) evaluates builtin profile transforms, connects XYZ/Lab PCS through XYZ D50, and adapts toward the internal sRGB basis.
- [`src/frame.c`](../../src/frame.c) keeps pixel-format conversion and frame color-space metadata synchronized.
- [`src/planner.c`](../../src/planner.c) selects resize and work formats and gates conversion before and after resize according to the actual source representation.
- [`src/frompng.c`](../../src/frompng.c) implements the builtin static PNG float composition paths, including source/background interpretation before linear-light blending.
- [`src/loader-common.c`](../../src/loader-common.c) resolves `cms_target`, `prefer_8bit`, and rendering-intent settings into a loader output representation.
- [`src/encoder.c`](../../src/encoder.c) separates palette-building and main-image views and converts generated palettes from `-X` to `-W`.
- [`src/encoder-core-encode.c`](../../src/encoder-core-encode.c) converts final palette entries from the working source color space to `-U` before SIXEL serialization.
