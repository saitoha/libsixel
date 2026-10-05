# Palettes

## Scope

This document explains what a palette is, why palettes appeared in computer graphics, how they evolved through display hardware, file formats and terminals, and where they are still used today. It is the first of three chapters on palette images. [Palette clustering](../clustering/README.md) explains how a palette is chosen for an image. [Dithering](../dithering/README.md) explains how the chosen palette is used to draw it.

**Key ideas in one minute:**

- A palette image stores a small **index** for each pixel and a short **table of colors**. The index says which table entry to use.
- Palettes save memory and bandwidth. Changing one table entry also recolors every pixel that uses it at once.
- Palettes were first a **hardware** necessity, then a **file format** feature, and they remain the color model of **SIXEL**.
- A palette can be **fixed** (the same for every image), **adaptive** (chosen for each image), or **local** (chosen for each frame or block).
- The fewer colors a palette has, the more the image depends on how the palette is chosen and how the image is dithered.

## What a palette image is

A full-color image stores three numbers per pixel, such as red, green and blue. A palette image, also called an **indexed-color** image, stores one small number per pixel. That number is an index into a table of colors, the **palette** or **color lookup table** (CLUT). To display the image, each index is replaced by the color found at that position in the table.

![On the left, an eight by six grid of the digits 0 to 3. In the middle, a palette of four entries: 0 dark navy, 1 amber, 2 cream, 3 red. On the right, the same grid drawn in those colors, forming a simple shape. Below, palette entries 1 and 3 are changed to green and blue, and a small copy of the image shows every amber pixel turned green and every red pixel turned blue.](figures/indexed-image.svg)

*Figure 1. An indexed image is a grid of indices plus a palette. Editing a palette entry recolors every pixel that uses it, without touching the pixels.*

Two properties follow from this separation.

**Memory and bandwidth.** With K palette entries, each pixel needs only log₂ K bits: 1 bit for 2 colors, 4 bits for 16, 8 bits for 256. A 320 × 200 image with 256 colors needs 64,000 bytes of indices plus a 768-byte palette, a third of the 192,000 bytes of 24-bit color. When memory cost dollars per kilobyte, this difference decided what a computer could show.

**Indirection.** Because pixels refer to colors instead of containing them, the table can be changed without redrawing the image. Rotating a range of palette entries makes water flow or lights blink while the pixel data stays still, a technique known as **color cycling** or palette animation. Fades to black, flashes and night scenes were made the same way, by rewriting a few dozen bytes instead of a whole frame.

> **Try it.** In Figure 1, find every pixel with index 1 in the left grid. Check that each of them is amber in the middle image and green in the small image below. One table entry controls all of them.

## A short history

![A vertical timeline from 1973 to 2015: SuperPaint frame buffer, 8-bit pixels through a color map; IBM CGA, 4 fixed colors at 320 by 200; IBM EGA, 16 of 64 colors; Amiga, 32 of 4,096 and HAM mode; IBM VGA, 256 of 262,144 colors; DEC VT340, 16 of 4,096 colors; GIF87a, 256-color palettes; GIF89a, animation and transparency; web-safe palette, 216 shared colors; PNG, palettes with alpha per entry; S3TC texture blocks, a 4-color palette per 4 by 4 block; xterm 256 colors, 16 plus 216 plus 24 grays; PICO-8, a fixed 16-color palette.](figures/timeline.svg)

*Figure 2. Palettes in display hardware, file formats, texture compression and terminals.*

### Frame buffers and color maps

Early raster displays were limited by memory, not by the screen. Richard Shoup's SuperPaint system at Xerox PARC, working by 1973, stored 8 bits per pixel and passed each value through a color map to produce one of 256 colors on a video display (Shoup, 2001). Changing the map changed the colors on screen instantly, and SuperPaint used this for animation effects such as color cycling, which made colors appear to flow through a still picture. The same arrangement became standard: a frame buffer of indices, and a small, fast table in front of the video output.

### Personal computers

Personal computers inherited the idea and its limits.

- **IBM CGA** (1981) showed 4 colors at 320 × 200, chosen from a few fixed sets, or 2 colors at higher resolution. The palette was mostly fixed by the hardware.
- **IBM EGA** (1984) showed 16 colors, and each of them could be chosen from 64. This was a programmable palette: the image stored 4-bit indices, and the program chose what they meant.
- **The Amiga** (1985) showed up to 32 colors chosen from 4,096. Its Hold-And-Modify (HAM) mode could show all 4,096 at once by letting a pixel modify one channel of its left neighbor instead of indexing the palette.
- **IBM VGA** (1987) showed 256 colors chosen from 262,144, because its color converter had 6 bits per channel. The 320 × 200 × 256 mode became the standard format of PC games for a decade.

Choosing the 256 colors that best represent a full-color photograph became a research problem in this period. Heckbert's median cut (1982) was written for exactly these frame buffers; [Palette clustering](../clustering/README.md) follows this line of work.

### Terminals: the VT340 and SIXEL

Digital Equipment Corporation used SIXEL graphics in its terminals. The VT330 had 4 color registers for gray levels, and the color VT340 (1987) had 16 registers, each chosen from 4,096 colors (Digital Equipment Corporation, 1988). A SIXEL stream defines registers with the `#` command and then paints with them. On the VT340, redefining a register changes the pixels already painted with it, which is exactly the palette indirection of Figure 1.

### File formats

Palettes also became the color model of image files.

- **GIF87a** (CompuServe, 1987) stored up to 256 colors per image with a global color table for the file and optional local tables for individual images.
- **GIF89a** (CompuServe, 1989; specification dated 1990) added delays between images and a transparent palette index. Together with local color tables, these made GIF the animation format of the web: each frame can have its own palette.
- **PNG** (W3C Recommendation, 1996) kept palettes for small images. Its PLTE chunk holds 1 to 256 entries, and the tRNS chunk can give each palette entry its own alpha, so a palette image can have smooth transparent edges.

### The web-safe palette

On the 8-bit displays of the mid-1990s, a browser had to show many images with one shared hardware palette. Browsers such as Netscape Navigator used a fixed **6 × 6 × 6 color cube**, with levels 0, 51, 102, 153, 204 and 255 for each channel, and dithered every image to it. Its 216 colors left the remaining entries of a 256-color palette to the operating system and the application. Web designers chose these "web-safe" colors to keep flat areas from being dithered. The palette lost its purpose once 16- and 24-bit displays became common.

### Texture compression

Graphics processors brought the palette idea back in a new form. S3 Texture Compression (S3TC, 1998), later called DXT1 and BC1, divides a texture into 4 × 4 blocks (Iourcha, Nayak and Hong, 1999). Each block stores two endpoint colors in 16-bit RGB565 and a 2-bit index per pixel. The two other palette colors are interpolated between the endpoints. Every block therefore has its own four-color palette, and a texture takes a fixed 4 bits per pixel, so the hardware can find any block without decoding the others.

![On the left, a four by four grid of indices from 0 to 3. In the middle, a block palette: entry 0 is a blue endpoint, entry 1 a yellow endpoint, entries 2 and 3 are colors two thirds and one third of the way between them. On the right, the decoded block shows blue in one corner fading through gray-brown to yellow in the opposite corner.](figures/block-palette.svg)

*Figure 3. A BC1 texture block. Two stored colors define a four-color palette for 16 pixels.*

### Terminals again: xterm 256 colors

Text terminals set colors with escape sequences, at first from 8 or 16 fixed colors. In 1999, Todd Larason's patch #111 to xterm added a 256-color mode (Dickey, xterm change log). Its palette has the 16 system colors, a 6 × 6 × 6 color cube with levels 0, 95, 135, 175, 215 and 255, and a 24-step gray ramp. Many terminal emulators adopted the same table, and it remains the reference "256-color terminal" palette.

![Three groups of swatches. The xterm 256-color palette: a row of 16 system colors, a block of 216 cube colors in six rows, and a ramp of 24 grays. The web-safe palette: 216 cube colors with evenly spaced levels. The DEC VT340 color map: 16 colors in a four by four grid, including blue, red, green, magenta, cyan, yellow, several grays and muted variants, and black.](figures/standard-palettes.svg)

*Figure 4. Three standard fixed palettes. The xterm and VT340 swatches are read from libsixel's built-in palette tables.*

Compare the two cubes in Figure 4. The web-safe cube spaces its levels evenly in encoded values. The xterm cube jumps from 0 to 95 and then steps by 40, so its dark colors are few and its light colors are close together. Neither spacing is uniform to the eye; the [color spaces](../colorspace/README.md) chapter explains why equal steps in encoded values are not equal steps in perception.

### A deliberate limit: fantasy consoles

When memory was no longer a reason, some designers kept palettes as a creative constraint. The PICO-8 "fantasy console" (Lexaloffle, 2015) has a 128 × 128 display with a fixed 16-color palette (Lexaloffle, PICO-8 manual). Pixel artists often work the same way: a small, hand-chosen palette gives an image a consistent style and makes every color decision visible.

## Fixed, adaptive and local palettes

Palettes differ in who chooses the colors and how widely they apply.

| Kind | Chosen | Example | Strength | Weakness |
| --- | --- | --- | --- | --- |
| Fixed | Once, for every image | Web-safe, xterm 256, PICO-8 | Images can share one display palette; no palette needs to be sent | Colors the image needs may be missing |
| Adaptive | For each image | GIF and PNG-8 files, SIXEL with `-p` | Colors follow the image | The palette must be computed and sent |
| Local | For each frame, region or block | GIF local tables, BC1 blocks | Small palettes that still follow the content | More palettes to compute and store; seams between regions |

A fixed palette needs no clustering, so its quality depends almost entirely on dithering. An adaptive palette moves the problem to [palette clustering](../clustering/README.md): which colors best represent this image. A local palette repeats that choice for smaller pieces. In an animation, frames with different palettes can make the same object change color slightly from frame to frame.

## How many colors are enough

![A photograph of a green and yellow snake reduced to 2, 4, 16 and 256 colors with error diffusion. At 2 colors, the snake is a pattern of olive dots. At 4 colors, the shapes are clear but the colors are muddy. At 16 colors, the image is recognizable with visible noise in flat areas. At 256 colors, it is close to the original.](figures/palette-sizes.svg)

*Figure 5. The same photograph with palettes of 2, 4, 16 and 256 colors chosen by median cut and applied with error diffusion.*

Each doubling of the palette adds one bit per pixel. The visible improvement is large at first and then shrinks: 2 to 16 colors changes the image completely, while 256 colors is already close to the original for many photographs. Smooth gradients, skin tones and dark areas are the last places where a 256-color palette still shows steps or noise. These are where [dithering](../dithering/README.md) and the choice of palette colors matter most.

> **Try it.** In Figure 5, look at the background in the 16-color image, then at the snake's scales. Dithering noise is easy to see in the smooth background and nearly invisible in the textured scales. Busy texture masks quantization error; smooth areas reveal it.

## Palettes today

Full-color displays are now universal, yet palettes remain in use wherever bandwidth, memory, physical inks or style still limit the colors.

- **Web and messaging.** GIF animations and PNG-8 images still carry palettes. Palette PNG with per-entry alpha is often the smallest format for icons, diagrams and screenshots with few colors.
- **Pixel art and games.** Retro-style games and pixel art use small palettes deliberately; fantasy consoles make the palette part of the platform.
- **GPU textures.** Block compression formats derived from S3TC still use per-block palettes of interpolated colors, with more modes and endpoints in later formats.
- **Electronic paper and printing.** E-paper panels can show only a few physical pigment colors, and printers lay down a small set of inks. Both reduce images to a fixed palette with dithering. Spot colors in printing name exact inks from a palette of standard colors.
- **Data visualization.** A colormap such as a heat map or a perceptually uniform scale is a palette indexed by a data value instead of a pixel index.
- **Terminals.** Text terminals still define 16- and 256-color palettes, and SIXEL graphics in modern terminal emulators use color registers that are a palette.

## Palettes in SIXEL

SIXEL is a palette format. A stream first defines color registers with `#Pc;2;R;G;B`, where each channel is a percentage from 0 to 100, and then paints six-pixel columns with the selected register. This has consequences that the rest of libsixel's documentation develops.

- **Palette size.** DEC defined register numbers 0 to 255, while the VT330 had 4 registers and the VT340 had 16. Modern terminals usually accept 256 or more. See the [SIXEL format](../../sixel-format/README.md) for register limits and lifetimes.
- **Palette precision.** A channel percentage from 0 to 100 gives 101 levels, not 256. A palette color in 8-bit sRGB is therefore rounded on the wire. The [snap policy](../../encoder/palette-construction/snap-policy/README.md) chooses palette colors that the wire can carry exactly.
- **Fixed or adaptive.** libsixel builds an adaptive palette by default and can use fixed ones such as the xterm 256-color or VT340 tables shown in Figure 4.
- **Register reuse.** A stream can redefine a register after painting with it. Whether earlier pixels keep their color depends on the terminal. libsixel's high-color mode relies on terminals that keep the painted color.
- **Animation.** Each SIXEL frame carries its own palette, much like a GIF local color table. Palette changes between frames can make stable areas flicker; [cover policy](../../encoder/palette-construction/cover-policy/README.md#what-a-cover-miss-looks-like-in-animation) shows how this looks. [6delta](../../encoder/output/delta-encoding.md) repaints only the pixels that changed, so unchanged areas keep their earlier colors.

## In libsixel

| Idea | libsixel control | Reference |
| --- | --- | --- |
| Adaptive palette size | `-p` | [Palette quantization](../../encoder/palette-construction/quantization/README.md) |
| How the adaptive palette is chosen | `-Q` | [Palette clustering](../clustering/README.md), [Palette quantization](../../encoder/palette-construction/quantization/README.md) |
| Fixed palettes: xterm, VT340, gray ramps | `-b` | [Built-in palettes](../../encoder/palette-construction/fixed-palettes.md) |
| A palette taken from another image or file | `-m` | [External palettes](../../encoder/palette-construction/external-palettes.md) |
| Palette colors exactly representable in SIXEL | `-_` | [Snap policy](../../encoder/palette-construction/snap-policy/README.md) |
| Reserving extreme colors in the palette | `-a` | [Cover policy](../../encoder/palette-construction/cover-policy/README.md) |
| More colors by redefining registers | `-I` | [High-color output](../../encoder/output/high-color/README.md) |
| Repainting only changed pixels in animation | `-Z delta` | [Delta encoding](../../encoder/output/delta-encoding.md) |

## Figures and reproduction

All figures are generated by [`tools/plot_palette_concept_figures.py`](../../../tools/plot_palette_concept_figures.py) with Matplotlib, NumPy and Pillow. Run it to regenerate them and with `--check` to verify that the committed files are current. The xterm 256-color and VT340 swatches in Figure 4 are read from libsixel's built-in tables in `src/dither.c`. Figure 5 reduces `images/snake.png` with Pillow's median cut and Floyd–Steinberg dithering, so it illustrates palette size and does not measure libsixel's quantizers. The parameters are recorded in [`figures/figures.json`](figures/figures.json).

## References

- CompuServe Incorporated. [*Graphics Interchange Format, Version 89a*](https://www.w3.org/Graphics/GIF/spec-gif89a.txt). Columbus, Ohio, 31 July 1990.
- Dickey, T. E. [*XTerm change log*](https://invisible-island.net/xterm/xterm.log.html), patch #111, 10 July 1999: 256-color support contributed by Todd Larason.
- Digital Equipment Corporation. [*VT330/VT340 Programmer Reference Manual, Volume 2: Graphics Programming*](https://bitsavers.trailing-edge.com/pdf/dec/terminal/vt340/EK-VT3XX-GP-002_VT330_VT340_Graphics_Programming_198805.pdf), second edition, EK-VT3XX-GP-002, May 1988.
- Heckbert, P. "Color image quantization for frame buffer display." Computer Graphics (SIGGRAPH '82 Proceedings) 16(3):297–307, 1982. doi:[10.1145/965145.801294](https://doi.org/10.1145/965145.801294)
- Iourcha, K. I., K. S. Nayak, and Z. Hong. *System and method for fixed-rate block-based image compression with inferred pixel values*. US Patent 5,956,431, 21 September 1999.
- Lexaloffle Games. [*PICO-8 User Manual*](https://www.lexaloffle.com/dl/docs/pico-8_manual.html).
- Shoup, R. "SuperPaint: an early frame buffer graphics system." IEEE Annals of the History of Computing 23(2):32–37, 2001. doi:[10.1109/85.929909](https://doi.org/10.1109/85.929909)
- W3C. [*PNG (Portable Network Graphics) Specification, Version 1.0*](https://www.w3.org/TR/REC-png-961001). W3C Recommendation, 1 October 1996.
