# Color Spaces and Loader Color Management

## Scope

A three-component pixel value is not a complete color description. The same numeric triplet can represent different light depending on its primaries, white point, transfer function, and any attached device profile. libsixel therefore keeps color interpretation separate from pixel storage and uses color spaces at several different pipeline boundaries.

This document explains the shared color-space model, the roles of `-X`, `-W`, and `-U`, and the color-management behavior of image loaders. The storage types and implicit promotion boundaries are defined in [Pixel-format precision](pixelformat-precision.md); broader quantizer, dither, lookup, speed, and size effects are measured in [Encoder working precision](../functionality/precision.md); palette-construction geometry is measured in [Palette clustering color space](../functionality/clustering-colorspace.md); and palette-application geometry is measured in [Working color space](../functionality/working-colorspace.md).

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
- [`src/frame.c`](../../src/frame.c) keeps pixel-format conversion and frame color-space metadata synchronized.
- [`src/loader-common.c`](../../src/loader-common.c) resolves `cms_target`, `prefer_8bit`, and rendering-intent settings into a loader output representation.
- [`src/encoder.c`](../../src/encoder.c) separates palette-building and main-image views and converts generated palettes from `-X` to `-W`.
- [`src/encoder-core-encode.c`](../../src/encoder-core-encode.c) converts final palette entries from the working source color space to `-U` before SIXEL serialization.
