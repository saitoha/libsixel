# Display Technologies

## Scope

This document looks at the devices that finally turn image data into light: printed paper and photographic film, cathode-ray tubes, liquid crystal, plasma and organic LED panels, projectors, and electronic paper. It follows their history, compares how each sends light to the eye and mixes color, traces how pixel density grew relative to the eye's resolution, and explains why electronic paper makes dithering and small palettes important again. It builds on [Visual perception](../../quality/visual-perception/README.md) and [Color spaces](../colorspace/README.md), and it prepares the [Palettes](../palettes/README.md) and [Dithering](../dithering/README.md) chapters.

**Key ideas in one minute:**

- A display either **emits** light, **transmits** a backlight through a filter, or **reflects** room light. Emissive and backlit displays lose contrast in bright rooms; reflective ones keep a fixed contrast and get brighter.
- Color is mixed **subtractively** by inks and dyes, or **additively** by light, either side by side (**spatially**) or one after another (**temporally**). Every display relies on the eye to average small elements.
- Pixel density, measured in **pixels per degree** of visual angle, grew slowly for decades and even **fell** for many consumer screens in the 2000s, before the "Retina" displays of the 2010s reached the eye's resolution.
- Where pixels are finer than the eye resolves, dither patterns disappear. Where levels are few and updates are slow, as on **electronic paper**, dithering is again the main tool.
- Color electronic paper revives the situation of 1980s terminals such as the VT340: **a few colors at a time, chosen from a limited set**.

## How a display sends light

![Three diagrams. An emissive pixel sends its own light to the eye. A transmissive display filters light from a backlight. A reflective surface returns part of the room light.](figures/light-paths.svg)

*Figure 1. Emissive, transmissive and reflective displays.*

**Emissive** displays make light in each pixel: the phosphors of a cathode-ray tube, the gas cells of a plasma panel, organic and inorganic LEDs. **Transmissive** displays modulate light from a separate source: a liquid crystal panel in front of a backlight, a color slide in front of a projector lamp. **Reflective** displays have no light of their own: printed paper and electronic paper return part of the light that falls on them.

The difference matters most when the room light changes. The figure below compares the three kinds with simple, illustrative values: a 300 cd/m² emissive or backlit display whose glass reflects 2% of the room light, a liquid crystal panel with a native contrast of 1000:1, electronic paper whose white reflects 40% and black 3% of the light, and printed paper with 85% and 3%.

![Two charts against room illuminance from 1 to 100,000 lux. On the left, the luminance of white is constant for an emissive or backlit display and rises in proportion to the room light for electronic and printed paper, overtaking the display between 1,000 and 3,000 lux. On the right, the contrast ratio of OLED and LCD falls as room light grows, while the contrast of electronic and printed paper stays constant at about 13:1 and 28:1; the display curves cross below them in bright daylight.](figures/ambient-light.svg)

*Figure 2. Luminance of white and contrast ratio as the room gets brighter. The optical values are illustrative assumptions, not measurements of particular products.*

- An emissive or backlit display has fixed brightness. Room light reflected from its surface adds to both its white and its black, so its contrast falls as the room gets brighter. In sunlight, the reflection can be brighter than the picture.
- A reflective display reflects a fixed fraction of the light, so its contrast ratio does not depend on the room light at all, and its white becomes brighter as the room does. It is easiest to read in sunlight and impossible to read in the dark without a front light.
- A transmissive display in a dark room, such as a slide projected in a darkened hall, combines a bright source with a dark surround and can show a very wide range of tones.

The eye adapts to all of this, as [Visual perception](../../quality/visual-perception/README.md#light-level-and-adaptation) explains. A reflective display is always seen at the adaptation level of the room around it; an emissive display can be much brighter or darker than its surroundings.

## How colors are mixed

![Four panels. Subtractive: cyan, magenta and yellow disks on white paper overlap to blue, green, red and black. Additive: red, green and blue light disks on black overlap to yellow, cyan, magenta and white. Spatial additive: thin red and green stripes on black, which look yellow from a distance. Temporal additive: red, green and blue fields shown one after another, which look white when fast enough.](figures/color-mixing.svg)

*Figure 3. Four ways to mix color.*

**Subtractive mixing.** Inks and dyes absorb part of the light that passes through them. Cyan absorbs red, magenta absorbs green and yellow absorbs blue, so layers of them remove more and more light, toward black. Color photographic film and slide film use layers of subtractive dyes; printing presses use cyan, magenta, yellow and black (CMYK) inks.

**Additive mixing.** Lights add up: red and green light together look yellow, and all three primaries together look white. Displays mix additively, but they rarely put two lights on the same spot. Instead they use one of two tricks that rely on the eye's averaging.

- **Spatial (partitive) mixing.** Small red, green and blue elements sit side by side. When they are smaller than the eye can resolve, the eye averages their light. Every color CRT, LCD and OLED panel works this way, and so did the Autochrome plates of 1907, whose dyed starch grains formed a random color mosaic.
- **Temporal mixing.** Colors are shown one after another, faster than the eye can follow. Single-chip DLP projectors show red, green and blue fields in turn through a rotating color wheel. A viewer who moves their eyes quickly can see the fields separate as colored fringes, the "rainbow effect". Plasma panels similarly build each gray level from short pulses of light within a frame.

Printing combines both kinds. A halftone screen prints dots of each ink; where dots overlap, the inks mix subtractively, and the uncovered paper and dots of different colors mix spatially in the eye. The resulting colors are mixtures of a few "Neugebauer primaries", which the [cover policy](../../encoder/palette-construction/cover-policy/README.md#printing-supplies-the-convex-mixture-model) uses as a model for palette coverage.

![Five schematic tiles: CRT shadow-mask phosphor triads, CRT aperture-grille stripes, LCD RGB stripes, OLED RGBG layout with shared subpixels, and a color filter array over gray electronic ink.](figures/subpixels.svg)

*Figure 4. Subpixel layouts of different display families. The patterns are schematic.*

The arrangement of the colored elements differs between technologies. A shadow-mask CRT places round red, green and blue phosphor dots in triangles; an aperture-grille CRT uses continuous vertical phosphor stripes; an LCD divides each pixel into three vertical stripes; many OLED panels share red and blue subpixels between neighboring pixels and give every pixel its own green, because the eye's resolution depends mostly on the luminance that green carries. Color electronic paper can place a color filter array over a monochrome ink layer, so that several gray ink pixels form one color pixel.

> **Try it.** Look at a white area of your screen through a magnifying glass, or take a close-up photograph with a phone. You will see the subpixel pattern of Figure 4 that your eye normally averages into white.

## A timeline

![A timeline from halftone printing in 1881 to color electronic paper in 2023, with each event colored by technology family: print and film, cathode-ray tubes, liquid crystal, plasma, OLED and LED, projection, electronic paper, and pixel-density milestones.](figures/timeline.svg)

*Figure 5. Display technologies from 1881 to 2023.*

### Print and photography

Printing presses can only put ink or no ink on paper. The **halftone** process, made commercially practical by Frederic Ives in 1881 and patented by Georg Meisenbach in Germany in 1882, broke photographs into dots of varying size, so that a press could print gray tones: the first widely used dithering. Color printing extended it to several inks, each with its own screen.

Photography brought color through additive and subtractive processes. The **Autochrome** plate of the Lumière brothers, marketed from 1907, was an additive mosaic of microscopic starch grains dyed red-orange, green and blue-violet over a black-and-white emulsion. **Kodachrome**, introduced in 1935, was a subtractive color reversal film whose slides were viewed by projection. Slide film is transmissive, like a later LCD, and projected in a dark room it shows deep blacks and bright highlights.

### Cathode-ray tubes

Ferdinand Braun built the first cathode-ray tube display in 1897. In a CRT, an electron beam scans the screen and makes a phosphor glow. Color tubes need three phosphors and a way to keep each beam on its own color. RCA's **CT-100** of 1954, the first mass-produced color television, used a **shadow mask**: a metal sheet with holes that let each of three beams reach only its phosphor dots. Sony's **Trinitron** of 1968 replaced the holes with a grille of vertical wires and continuous phosphor stripes, giving a brighter picture.

A CRT has no fixed pixel grid. The video signal sets how many pixels are drawn per line, and the same tube can show 640 × 480 or 1600 × 1200. The sharpness, however, is limited by the spot size of the beam and the pitch of the phosphor pattern, typically around 0.24 to 0.28 mm on later computer monitors. A resolution setting above what the phosphor pitch supports draws pixels that the tube cannot actually separate.

The CRT was the computer display for half a century: the DEC VT340 and other SIXEL terminals, and the PC graphics adapters of the [palette chapter](../palettes/README.md#personal-computers), all drew on CRTs. Flat displays that kept the CRT's principle, such as the surface-conduction electron-emitter display (SED) developed by Canon and Toshiba, did not reach the market: Canon ended SED development in 2010.

### Flat panels

**Plasma.** The plasma display panel was invented in 1964 at the University of Illinois by Donald Bitzer, Gene Slottow and Robert Willson for the PLATO computer-based education system (Bitzer and Slottow, 1966). Each cell is a tiny fluorescent lamp. Color plasma televisions arrived in the late 1990s; Fujitsu's 42-inch panel of 1997 showed 852 × 480 pixels. Plasma's emissive picture had wide viewing angles, but the technology lost the television market to LCDs.

**Liquid crystal.** George Heilmeier's group at RCA demonstrated the first liquid crystal displays in 1968, using dynamic scattering (Heilmeier, Zanoni and Barton, 1968). The **twisted nematic** (TN) cell of Schadt and Helfrich (1971) needed little power and became the basis of watches, calculators and later of computer screens. Liquid crystal does not emit light: it rotates the polarization of light from a backlight, and polarizers turn this into brightness. Active-matrix panels with a thin-film transistor per subpixel made fast, high-resolution screens possible, and in-plane switching (IPS), commercialized by Hitachi in 1996, widened the viewing angle. LCD desktop monitors outshipped CRTs worldwide from the third quarter of 2004, and LCD televisions outsold CRT televisions from the fourth quarter of 2007.

An LCD has a fixed **native resolution**: one transistor per subpixel. Any other resolution must be resampled, which blurs the picture. This simple fact shaped the pixel-density history below.

**Organic LED.** Ching Tang and Steven VanSlyke at Kodak reported efficient organic light-emitting diodes in 1987 (Tang and VanSlyke, 1987). OLED subpixels emit their own light and can switch off completely, so an OLED panel shows true black in a dark room. Sony's XEL-1 of 2007, with an 11-inch 960 × 540 screen, was the first OLED television; OLED panels later became common in phones.

**MicroLED.** Arrays of microscopic inorganic LEDs were first demonstrated by Hongxing Jiang and Jingyu Lin in 2000 (Jiang et al., 2001). Sony showed a 55-inch "Crystal LED" prototype with one LED per subpixel in 2012. MicroLED combines emissive black with the brightness and lifetime of inorganic LEDs, but remains expensive to manufacture.

### Projectors

Projectors enlarge a small image onto a screen, so they mix color in the projector, not on the screen. Epson's VPJ-700 of 1989 was the first **3LCD** projector, with one transmissive LCD panel for each primary and optics that combine the three images. Larry Hornbeck at Texas Instruments invented the **digital micromirror device** (DMD) in 1987: an array of tiny mirrors that tilt toward or away from the lens thousands of times per second. A DLP projector forms gray levels by how long each mirror points at the screen and, with a single chip, forms color by temporal mixing through a color wheel. The first DLP projector came to market in 1997.

### Electronic paper

Nick Sheridon's **Gyricon** at Xerox PARC, from 1974, used tiny beads that were black on one side and white on the other, rotated by an electric field. The electrophoretic ink of E Ink, a company founded in 1997 by MIT Media Lab researchers and students, uses microcapsules of charged white and black pigment particles that move toward or away from the surface (Comiskey et al., 1998). The image stays without power; only changes cost energy.

Sony's Librie of 2004 was the first e-reader with E Ink, followed by Amazon's Kindle in 2007, which showed four gray levels; later generations show 16. Color electronic paper took three routes:

- **Color filters.** E Ink Kaleido places a color filter array over a monochrome ink layer. E Ink describes Kaleido 3 (2022) as showing 16 gray levels and 4,096 colors. The filters reduce the color resolution and the saturation of the reflected light.
- **Several colored pigments per pixel.** E Ink Gallery 3 (2022) moves cyan, magenta, yellow and white particles in each pixel, with color updates of about half a second.
- **A few vivid pigments.** E Ink Spectra 6 (2023) shows six display colors, black, white, red, yellow, blue and green, for signage and labels.

## Pixel density and the eye

### Pixels per degree

How sharp a display looks depends on how large its pixels appear to the eye, which depends on both pixel pitch and viewing distance. [Visual perception](../../quality/visual-perception/README.md#from-pixels-to-cycles-per-degree) defines the measure used here:

```text
pixels per degree = viewing distance × tan(1°) / pixel pitch
finest pattern    = pixels per degree / 2    cycles per degree (cpd)
```

A one-pixel checkerboard on a display with 60 pixels per degree is a 30 cpd pattern. That is the detail of a letter that a person with normal 20/20 acuity can just resolve; the eye's absolute resolution limit lies near 50 to 60 cpd, or 100 to 120 pixels per degree.

![A scatter chart of pixels per degree against year from 1987 to 2017. Cathode-ray tubes rise from 25 to 41 pixels per degree. Desktop and notebook LCDs stay between 34 and 46 in the 2000s, and two arrows mark falls: from a 17-inch to a 19-inch 1280 by 1024 LCD, and from a 15-inch 1600 by 1200 notebook to a 15.6-inch 1366 by 768 notebook. The IBM T221 reaches 84 in 2001. From 2010, phones, a Retina notebook, a 27-inch 5K display and an e-reader reach 68 to 95, above the 60 pixels per degree line that marks 20/20 acuity.](figures/pixel-density.svg)

*Figure 6. Pixels per degree of representative displays, computed from their resolution and diagonal at typical viewing distances: 60 cm for desktop monitors, 50 cm for notebooks, 35 cm for e-readers and 30 cm for phones. CRTs are drawn hollow because their addressable resolution exceeds what their phosphor pitch can show.*

### The long plateau and the 2000s step back

For two decades, desktop displays stayed between about 25 and 45 pixels per degree. CRT monitors grew larger and supported more lines, and a 21-inch CRT at 1600 × 1200 reached about 100 pixels per inch, or 41 pixels per degree at 60 cm. Professional 21- and 22-inch CRTs accepted 2048 × 1536, although their 0.24 mm phosphor pitch could not fully resolve it.

The move to LCDs in the 2000s brought sharp, geometrically perfect pixels, but for many consumers it did not bring more of them, and in some common cases fewer:

- **Fixed native resolutions.** A CRT could be run at any resolution its electronics allowed. An LCD looks good only at its native resolution. Popular 17- and 19-inch LCDs both had 1280 × 1024 pixels, so the larger 19-inch panel had larger pixels: 86 instead of 96 pixels per inch.
- **Fewer lines on wider screens.** Around 2009 notebook and monitor makers moved from 16:10 and 4:3 panels to the 16:9 shape of televisions, which let them cut more panels from each sheet of glass. Many 15-inch notebooks that had offered 1400 × 1050, 1600 × 1200 or 1920 × 1200 were replaced by 15.6-inch models with 1366 × 768 pixels, and desktop monitors moved from 1920 × 1200 to 1920 × 1080.

Figure 6 marks two such steps. A 15-inch 1600 × 1200 notebook of 2006 offered 133 pixels per inch, or 46 pixels per degree at 50 cm; a typical 15.6-inch 1366 × 768 notebook of 2009 offered 100 pixels per inch, or 35 pixels per degree. Professional displays had already gone much further: IBM's T220 and T221 of 2001 put 3840 × 2400 pixels on a 22.2-inch LCD, 204 pixels per inch, but remained a niche product.

### Retina: reaching the eye's resolution

In 2010 Apple introduced the iPhone 4 with a 960 × 640 display at 326 pixels per inch and called it a "Retina" display (Apple, 2010). Presenting it, Steve Jobs argued that about 300 pixels per inch, held 10 to 12 inches from the eye, was the limit of what the eye could resolve. At 30 cm this is about 68 pixels per degree, above the 60 pixels per degree of 20/20 acuity. The MacBook Pro with Retina display followed in 2012 with 2880 × 1800 pixels at 220 pixels per inch, and 27-inch 5K monitors reached about 218 pixels per inch in 2014. Phones of the late 2010s exceed 450 pixels per inch, about 95 pixels per degree at 30 cm. E-readers followed the same path, from about 167 pixels per inch for the first Kindle to 300 pixels per inch in later models.

At these densities, single pixels are no longer visible at normal distance. Fine details, such as the steps of a slightly slanted edge, can still be noticed beyond the resolution limit, because the eye compares positions more precisely than it resolves patterns. Operating systems therefore draw text and images at two or three device pixels per logical pixel instead of making everything smaller.

> **Try it.** Compute your own pixels per degree. Divide your screen's horizontal resolution by its width in centimeters to get pixels per centimeter, then multiply by your viewing distance in centimeters and by 0.01745. If the result is above 60, a one-pixel checkerboard should look like flat gray to you; below about 35, you will see its texture.

## Displays and dithering

### Fine pixels hide dither patterns

Dithering trades spatial resolution for tones, as the [dithering chapter](../dithering/README.md#the-idea-trade-resolution-for-tone) explains. Its noise is least visible when it sits at spatial frequencies the eye barely resolves. On a 35 pixels per degree display, a one-pixel dither pattern is a 17 cpd pattern: attenuated, but visible in smooth areas. On a 70 pixels per degree display, the same pattern is 35 cpd, beyond 20/20 acuity, and the eye sees mostly its average. High-density displays have made dithering both less visible and, for full-color content, rarely necessary.

Displays themselves still dither. Many LCD panels with 6-bit drivers show 8-bit input by alternating between neighboring levels from frame to frame, a temporal dithering known as frame rate control, and plasma and DLP displays build their gray levels from pulses in time.

### Electronic paper brings dithering back

Electronic paper reverses these conditions. A monochrome panel shows 16 gray levels at most; color panels show a fixed set of pigment colors or a coarse color filter mosaic. Updates take hundreds of milliseconds and cost energy, so frame-by-frame temporal dithering, which LCDs use invisibly, is not an option. A photograph on electronic paper is therefore drawn with **spatial dithering** to a small palette, exactly as on a 1980s computer or in a printed newspaper.

![A photograph of a green and yellow snake in four versions: the original; dithered to 16 gray levels; dithered to an idealized six-color palette of black, white, red, yellow, blue and green, which shows strong colored noise; and dithered to an adaptive palette of 16 colors chosen for the image and rounded to 4 bits per channel, which keeps the natural colors with mild noise.](figures/epaper-palettes.svg)

*Figure 7. One photograph prepared for different electronic paper constraints, with Floyd–Steinberg error diffusion. The six-color palette uses idealized saturated colors; real pigments are less saturated.*

Figure 7 shows how much the choice of palette matters. A fixed palette of six vivid pigments must build the muted greens and browns of the photograph from widely separated colors, so the dither noise is strong. An adaptive palette of 16 colors chosen for this image keeps the dither noise low, even though each color is restricted to 16 levels per channel.

### A return to "16 of 4,096"

That last panel follows the design of DEC's color terminals. The VT241 showed 4 colors at a time from a set of 64 (Terminals Wiki); the VT340 showed 16 at a time from 4,096, that is, 16 levels for each of red, green and blue, and SIXEL streams chose the 16 colors for each image. Color electronic paper of the 2020s has a similar shape: a color filter panel advertised as able to show 4,096 colors, or a pigment panel with a handful of display colors, driven by a controller with little memory, and slow to update.

A picture pipeline for such devices may therefore return to small adaptive palettes: choose a few colors per image from the colors the panel can actually produce, and dither with them. This is what SIXEL was designed for. A stream defines up to as many color registers as the device has and then paints with them, and libsixel's palette construction and dithering apply directly. The [palette](../palettes/README.md), [clustering](../clustering/README.md) and [dithering](../dithering/README.md) chapters describe the tools.

> **Try it.** Prepare an image the way a small-palette device would show it: `img2sixel -b gray4 image.png` uses 16 evenly spaced grays, like a monochrome e-reader, and `img2sixel -p 16 image.png` chooses 16 colors for the image, like a VT340. To match a particular panel, write its colors to a GPL palette file and use `img2sixel -m panel.gpl image.png`.

## In libsixel

| Situation | libsixel control | Reference |
| --- | --- | --- |
| Few colors per image, chosen for the image | `-p` | [Palette quantization](../../encoder/palette-construction/quantization/README.md) |
| Grayscale panels, 2 to 256 levels | `-b gray1` … `-b gray8` | [Built-in palettes](../../encoder/palette-construction/fixed-palettes.md) |
| The fixed colors of a particular panel | `-m` | [External palettes](../../encoder/palette-construction/external-palettes.md) |
| Dithering method for coarse palettes | `-d` | [Dithering](../../encoder/palette-application/dithering/README.md) |
| Reserving vivid extreme colors in a small palette | `-a` | [Cover policy](../../encoder/palette-construction/cover-policy/README.md) |
| Updating only changed pixels on a slow display | `-Z delta` | [Delta encoding](../../encoder/output/delta-encoding.md) |

## Figures and reproduction

All figures are generated by [`tools/plot_display_concept_figures.py`](../../../tools/plot_display_concept_figures.py) with Matplotlib, NumPy and Pillow. Run it to regenerate them and with `--check` to verify that the committed files are current. Pixel densities in Figure 6 are computed from the nominal resolution and diagonal of each display, so a value can differ slightly from a manufacturer's figure; the viewing distances are typical assumptions. The optical values of Figure 2 are illustrative assumptions. Figure 7 dithers `images/snake.png` with Pillow, so it illustrates palette constraints and does not measure libsixel's quantizers. All parameters and computed values are recorded in [`figures/figures.json`](figures/figures.json).

## References

- Apple Inc. ["Apple Presents iPhone 4."](https://www.apple.com/newsroom/2010/06/07Apple-Presents-iPhone-4/) Press release, 7 June 2010.
- Bitzer, D. L., and H. G. Slottow. "The plasma display panel: a digitally addressable display with inherent memory." AFIPS Conference Proceedings 29 (Fall Joint Computer Conference), 541–547, 1966. doi:[10.1145/1464291.1464349](https://doi.org/10.1145/1464291.1464349)
- Comiskey, B., J. D. Albert, H. Yoshizawa, and J. Jacobson. "An electrophoretic ink for all-printed reflective electronic displays." Nature 394:253–255, 1998. doi:[10.1038/28349](https://doi.org/10.1038/28349)
- E Ink Holdings. "E Ink Launches E Ink Kaleido 3, the Next Generation of Print Color ePaper Technology." Press release, 7 April 2022.
- E Ink Holdings. ["E Ink Launches E Ink Spectra 6, a Revolutionary New Color ePaper for Paper Sign Replacement."](https://www.eink.com/news/detail/E-Ink-Launches-E-Ink-Spectra-6-a-Revolutionary-New-Color-ePaper-for-Paper-Sign-Replacement) Press release, 6 April 2023.
- Heilmeier, G. H., L. A. Zanoni, and L. A. Barton. "Dynamic scattering: a new electrooptic effect in certain classes of nematic liquid crystals." Proceedings of the IEEE 56(7):1162–1171, 1968. doi:[10.1109/PROC.1968.6513](https://doi.org/10.1109/PROC.1968.6513)
- Jiang, H. X., S. X. Jin, J. Li, J. Shakya, and J. Y. Lin. "III-nitride blue microdisplays." Applied Physics Letters 78:1303, 2001. doi:[10.1063/1.1351521](https://doi.org/10.1063/1.1351521)
- Schadt, M., and W. Helfrich. "Voltage-dependent optical activity of a twisted nematic liquid crystal." Applied Physics Letters 18(4):127–128, 1971. doi:[10.1063/1.1653593](https://doi.org/10.1063/1.1653593)
- [DEC VT240](https://terminals-wiki.org/wiki/index.php/DEC_VT240). Terminals Wiki, for the VT240 and VT241 specifications.
- Tang, C. W., and S. A. VanSlyke. "Organic electroluminescent diodes." Applied Physics Letters 51(12):913–915, 1987. doi:[10.1063/1.98799](https://doi.org/10.1063/1.98799)
- Digital Equipment Corporation. [*VT330/VT340 Programmer Reference Manual, Volume 2: Graphics Programming*](https://bitsavers.trailing-edge.com/pdf/dec/terminal/vt340/EK-VT3XX-GP-002_VT330_VT340_Graphics_Programming_198805.pdf), second edition, May 1988.
