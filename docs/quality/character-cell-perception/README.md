# Character Cell Perception

## Scope

Programs that draw images with text, such as AAlib, libcaca, chafa and the half-block and Braille modes of terminal image viewers, use the character cell as their picture element. A cell can show one glyph in one foreground and one background color, so a character renderer behaves like a display device with a very coarse grid and a strongly restricted set of pixel patterns. Its history and the methods of these programs are described under [pictures made of characters](../../sixel-format/terminal-graphics-history/README.md#pictures-made-of-characters); this document looks at the cell from the side of the viewer.

This document builds on the [visual perception chapter](../visual-perception/README.md), in particular its sections on [contrast sensitivity](../visual-perception/README.md#contrast-sensitivity) and [viewing geometry](../visual-perception/README.md#from-pixels-to-cycles-per-degree).

## Where a character cell falls on the sensitivity curves

With the geometry of [the example of the visual perception chapter](../visual-perception/README.md#from-pixels-to-cycles-per-degree), 100 ppi at 50 cm or about 34 pixels per degree, a cell of 8 × 16 pixels spans about a quarter of a degree in width and half a degree in height. A pattern that alternates from one cell to the next therefore has a frequency of only about 1 to 2 cpd:

| Picture element | Size in pixels | Horizontal cpd | Vertical cpd |
| --- | --- | ---: | ---: |
| Character cell | 8 × 16 | 2.15 | 1.07 |
| Half block (two per cell) | 8 × 8 | 2.15 | 2.15 |
| chafa 8 × 8 sub-cell bitmap | 1 × 2 | 17.18 | 8.59 |

*Table 1. Nyquist frequency of each picture element at 100 ppi and 50 cm. A larger font or a closer view moves every row to lower frequencies.*

The two sensitivity curves of the [contrast sensitivity figure](../visual-perception/README.md#contrast-sensitivity) of the visual perception chapter weight these frequencies very differently. Figure 1 shows the ratio of the two curves over the frequencies of Table 1:

![A single curve on logarithmic axes: the ratio of luminance to chroma sensitivity, 0.21 at 0.5 cycles per degree, 0.45 at 1, 1.6 at 2, 41 at 8 and 137 at 17. A dashed line marks equal sensitivity, crossed near 1.5 cycles per degree. A shaded band from 1 to 2 cycles per degree marks the cell grid and another from about 9 to 17 the shapes inside a cell.](figures/csf-ratio.svg)

*Figure 1. Ratio of the two curves of the [contrast sensitivity figure](../visual-perception/README.md#contrast-sensitivity), the luminance model of Mannos and Sakrison (1974) and the schematic chroma curve after Mullen (1985), each scaled to its own peak. The shaded bands are the frequencies of Table 1 for a character cell and for chafa's sub-cell bitmaps. The ratio compares the shapes of the curves, not absolute thresholds, and is reliable only in its order of magnitude because the chroma curve is schematic; for that reason it is drawn only up to 30 cycles per degree. The values at the marked points are recorded in [`figures/figures.json`](figures/figures.json).*

At the frequency of the cell grid, around 1 to 2 cpd, luminance and chroma are about equally effective, and below 1 cpd chroma is relatively the stronger of the two. At the frequency of the shapes inside a cell, around 8 to 17 cpd, luminance dominates by more than an order of magnitude and chroma detail is close to invisible. Seen this way the usual design of character renderers is well matched to vision: the glyph inside a cell is read mainly as a pattern of luminance coverage, and the colors of the cell are read mainly as area colors. It also follows that the color assigned to each cell carries as much weight as the choice of glyph. What the viewer judges about those colors, however, is not their distance from the original, as the next section explains.

## Research on coarse mosaics

The perception of block-quantized, or pixelated, images has been studied since the 1970s, mostly with faces:

- **Block edges mask the image.** Harmon and Julesz (1973) found that a portrait reduced to large uniform blocks is hard to recognize largely because the sharp block edges add spurious high-frequency energy close to the low frequencies that carry the face, and that removing that energy by blurring restores recognition. Morrone, Burr and Ross (1983) showed that adding noise can likewise restore the recognizability of coarsely quantized images.
- **A small number of elements suffices.** Bachmann (1991) found that faces remain identifiable down to about 16 blocks across the face, about 8 cycles per face. Costen, Parker and Craw (1994) placed the band most important for face recognition at about 8 to 16 cycles per face. Bachmann and Kahusk (1997) studied how the coarseness of quantization interacts with exposure time and attention. Bachmann (2016) reviews the field in a book-length treatment.
- **Size and distance decide between tiles and image.** Pelli (1999) studied the portraits of Chuck Close, which are built from tiles: whether the viewer sees a face or an array of tiles depends on the number of tiles per face and on the angular size of the picture.
- **Color helps when shape is degraded.** Yip and Sinha (2002) found that color contributes to face recognition mainly when shape information is degraded, for example by blur. Oliva and Schyns (2000) found that diagnostic colors speed scene recognition, especially at coarse scales. Schyns and Oliva (1994) and Oliva and Schyns (1997) showed that the spatial scale used for recognition depends on the task, rather than always proceeding from coarse to fine.
- **Glyphs as structure.** Xu, Zhang and Wong (2010) chose glyphs for line drawings by structural similarity rather than by tone, an approach that treats glyphs as small images, as later character renderers do.

## What the viewer compares

A viewer of a terminal image has no original to compare with. The judgment is *no-reference*: the viewer sees a defect when the picture contains structure that a natural image would not, not when a color differs from a reference that is not on the screen. Two consequences follow for character cells.

- **A smooth absolute error is tolerated.** An image that is slightly too warm or too dark everywhere, or whose colors drift slowly across a region, is accepted as the picture's own appearance, helped by [chromatic adaptation](../visual-perception/README.md#chromatic-adaptation). The comparison of Figure 1 says how visible a pattern at a given frequency is; it does not say that the viewer can tell a cell's color from the original's.
- **A fluctuation between neighboring cells is seen.** When adjacent cells of a smooth region, such as skin or sky, change hue, brightness or texture without reason, the change forms a pattern at the frequency of the cell grid, 1 to 2 cpd in Table 1. Both luminance and chroma are seen well there, and natural images vary little in chroma within smooth regions, so a hue that jumps from cell to cell is judged unnatural even when its error against the original is small.

Harmon and Julesz point the same way: what defeated recognition was the spurious structure of the block edges, not the error of the block averages. Measures of blocking artifacts in compressed images likewise work without a reference by measuring discontinuities at block boundaries (Wang, Bovik and Evans 2000; Wang, Sheikh and Bovik 2002).

![Six panels in two rows. Top row, one color per pixel: a photograph of a green and yellow snake; the same photograph dithered with 512 uniformly spaced colors, which looks close to the original; and the same colors without dithering, which shows flat bands. Bottom row, one color per 9 by 9 tile: the exact mean color of each tile; the tiles dithered with the same 512 colors, a mottled checkerboard; and the tiles without dithering, blocky with broad flat areas.](figures/tile-dithering.svg)

*Figure 2. The same 512 colors (3 bits per channel) and the same Floyd–Steinberg dithering on a fine grid and on coarse tiles. Visible error is the estimate described under [color precision for character cells](#color-precision-for-character-cells), for 192 ppi at 50 cm. On the fine grid dithering cuts it from 6.5 to 2.0; on the tiles it changes it only from 8.1 to 7.1, and replaces bands with a mottled pattern of tiles.*

This reverses a usual assumption about dithering. On a fine pixel grid, dithering moves quantization error to frequencies that the eye does not resolve. On a cell grid, error diffused from cell to cell lands at the cell frequency, where it is seen as exactly the neighbor-to-neighbor fluctuation described above. Figure 2 shows both cases with the same palette, and Table 3 below measures the effect for several palettes: the benefit of dithering largely disappears on a grid of tiles. For a character renderer, leaving cells undithered can therefore look better than dithering them, and the answer depends on the palette:

- **24-bit color.** The colors of a cell can match the local mean almost exactly, so the only remaining quantization is the shape of the glyph. There is little reason to dither color.
- **256 colors.** Steps between palette colors are moderate. Without dithering, smooth gradients show a few broad bands; with dithering between cells, they show cell-scale fluctuation of about the same visibility. A dither, if used, is better placed inside cells.
- **16 colors or fewer.** Steps are large, and bands are strengthened by Mach bands into prominent contours, which are themselves a structure a natural image does not have. Dithering between cells still reduces the visible error slightly for such coarse palettes, but much less than on a pixel grid, so a sub-cell grain and a pattern fixed in position, such as ordered or blue-noise dithering, are preferable to error diffusion between cells.

A pattern fixed in position also keeps its decision stable: error diffusion propagates each decision to the following cells, so a small change of input can flip a whole run of cells, which in animation is seen as shimmer.

## How character renderers handle it

The source code of three renderers shows choices that agree with this reasoning in the newest of them, and with the constraints of their time in the older two.

| Renderer | Colors | Default dithering | Where the error goes | Cell choice |
| --- | --- | --- | --- | --- |
| AAlib 1.4 | Brightness only, with text attributes | Floyd–Steinberg | Between cells: the difference between the mean of a cell's 2 × 2 samples and the brightness of the chosen glyph passes to the neighboring cells | Glyph from a table indexed by the 2 × 2 samples; flat cells, whose samples differ from their mean by less than 13 of 256 levels, use a glyph chosen by brightness alone |
| libcaca (`img2txt`) | 16 ANSI colors | Floyd–Steinberg | Between cells: the difference between the cell average and the mixture of the chosen colors and density glyph | Background is the nearest palette color in RGB, foreground the second nearest, and the glyph is chosen from a density ramp assuming linear mixing |
| chafa 1.18, symbols | 24-bit, 256, 240, 16, 8 or 2 colors | None. Ordered, Floyd–Steinberg or blue-noise dithering on request; always off with 24-bit color | Inside the 8 × 8 working image of the cells, in grains of 1/8 to 1 cell, by default half a cell | Glyph and foreground and background colors minimize the error within the cell, each color being the mean of the pixels it covers; palette colors are the nearest match |
| chafa 1.18, sixel | Dynamic palette | Blue noise | Pixels | Not applicable |

*Table 2. Default behavior of character renderers, from their source code: AAlib `src/aarender.c` (Debian packaging repository), libcaca `caca/dither.c` and `src/img2txt.c`, chafa `chafa/chafa-canvas.c`, `chafa/internal/chafa-pixops.c` and `chafa/internal/chafa-symbol-renderer.c`.*

chafa turns dithering off in 24-bit symbol mode with the comment that there is "no reason to dither in truecolor mode", and keeps it off by default in every other symbol mode, while it dithers SIXEL output, which is drawn with real pixels. In 24-bit mode each cell color is the mean of the pixels under the glyph, a continuous function of the image, so neighboring cells of a smooth region receive neighboring colors. In 16-color and fewer modes chafa instead preprocesses the image with a contrast boost, and for 16 colors a saturation boost, which accepts a flatter, stylized result rather than masking the steps with noise. When dithering is requested, its default grain of half a cell puts the pattern at about 2 to 4 cpd in the geometry of Table 1; a grain of one eighth of a cell moves it to the 8 to 17 cpd of the sub-cell row.

AAlib and libcaca both diffuse error between cells by default, which places it at the cell frequency. Both come from the era of monochrome and 16-color terminals, where the steps between available tones are so large that bands would dominate without dithering, which is the last case above. AAlib's special handling of flat cells keeps uniform areas uniform within the tone that diffusion delivers to them.

None of the three has an explicit term for consistency between neighboring cells: each cell is chosen independently, given the image and any diffused error. A nearest-color lookup is discontinuous, so in palette modes two neighboring cells with almost the same content can still fall on different sides of a decision boundary.

An informal check with chafa 1.18.2 agrees. A horizontal gradient, constant in the vertical direction, was rendered as 80 × 20 cells. Any difference between vertically adjacent cells is therefore structure that the image does not contain. Of the 1,520 vertically adjacent pairs, none differed without dithering in the 16-color, 256-color and 24-bit modes, while with dithering in the 16-color mode 721 differed with ordered dithering, 781 with blue noise and 1,285 with Floyd–Steinberg. The count records any change of glyph or color and not its visibility, so it shows where dithering creates cell-scale structure rather than how conspicuous that structure is.

## Color precision for character cells

Comparisons of terminal color modes often set a fixed 256-color palette against 24-bit color and conclude that 24-bit color is needed. Such a comparison changes several things at once: the choice of palette colors, whether and where dithering can hide the steps between them, the precision of each color, and the number of colors that can be shown together. This section separates them.

*Method.* Visible error is estimated in the manner of S-CIELAB (Zhang and Wandell 1997). The original and the rendition are converted to the opponent space of S-CIELAB, using the matrix of its reference implementation, filtered with the sensitivity curves of the [contrast sensitivity figure](../visual-perception/README.md#contrast-sensitivity) of the visual perception chapter for a given viewing geometry, with the luminance curve held flat below its peak so that it acts as a low-pass filter, then converted to CIELAB and compared pixel by pixel. Tables give the mean and, where noted, the 99th percentile of the difference. The geometry is 192 ppi at 50 cm, about 66 pixels per degree, with tiles of 9 × 9 pixels. For tiles, the reference is the 24-bit mean color of each tile, so that the tables measure color quantization rather than the loss of detail inside tiles. Palettes are applied with averages and dithering errors in linear light and nearest colors by DIN99d. The estimate is a model: it accounts for spatial sensitivity but not for the no-reference judgment described above, which penalizes cell-to-cell structure more than a mean difference does.

| Image | Palette | Pixels, not dithered | Pixels, dithered | Gain | Tiles, not dithered | Tiles, dithered | Gain |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Snake | Uniform 3 bits per channel | 6.48 | 2.02 | 3.21 | 8.09 | 7.14 | 1.13 |
| Snake | Uniform 4 bits per channel | 2.29 | 1.00 | 2.29 | 3.14 | 3.26 | 0.96 |
| Snake | Uniform 5 bits per channel | 0.93 | 0.49 | 1.90 | 1.49 | 1.60 | 0.93 |
| Snake | Adaptive 64 | 1.59 | 1.60 | 0.99 | 1.85 | 2.22 | 0.83 |
| Snake | Adaptive 256 | 0.81 | 0.70 | 1.16 | 1.07 | 1.25 | 0.86 |
| Gradient | Uniform 3 bits per channel | 6.33 | 1.37 | 4.62 | 6.32 | 5.39 | 1.17 |
| Gradient | Uniform 4 bits per channel | 2.67 | 0.63 | 4.24 | 2.68 | 2.45 | 1.09 |
| Gradient | Uniform 5 bits per channel | 1.09 | 0.31 | 3.52 | 1.10 | 1.08 | 1.02 |
| Gradient | Adaptive 64 | 1.19 | 0.57 | 2.09 | 1.20 | 1.26 | 0.95 |
| Gradient | Adaptive 256 | 0.58 | 0.27 | 2.15 | 0.60 | 0.63 | 0.95 |

*Table 3. Visible error with and without Floyd–Steinberg dithering, for the same palette on pixels and on tiles. Gain is the error without dithering divided by the error with it.*

On pixels, dithering divides the visible error by 1.9 to 4.6 for uniform palettes. On tiles, the gain ranges from 0.83 to 1.17: coarse uniform palettes gain a little, and finer or image-specific palettes lose slightly. Dithering does not remove error; it moves it to higher spatial frequencies, and on a grid of tiles the highest available frequency is the tile frequency, where sensitivity is still high. A grid of character cells therefore cannot trade spatial resolution for color precision, and each color must be precise enough by itself.

| Colors per tile | Snake | Gradient | Dark gradient |
| --- | --- | --- | --- |
| 5 bits per channel (32,768 colors) | 1.32 / 3.80 | 0.98 / 2.57 | 1.84 / 3.77 |
| 5, 6 and 5 bits (65,536 colors) | 0.87 / 2.42 | 0.64 / 1.81 | 1.21 / 2.27 |
| 6 bits per channel | 0.60 / 1.71 | 0.54 / 1.30 | 0.67 / 1.26 |
| 7 bits per channel | 0.29 / 0.88 | 0.27 / 0.60 | 0.31 / 0.72 |
| 8 bits per channel (24-bit) | 0.15 / 0.43 | 0.12 / 0.29 | 0.17 / 0.39 |
| 256 colors chosen for the image | 0.82 / 4.14 | 0.63 / 1.98 | 0.17 / 0.39 |

*Table 4. Visible error, mean / 99th percentile, of tile colors rounded to a given precision in sRGB-encoded values or mapped to 256 colors chosen for the image, without dithering.*

Rounding to 5 bits per channel, the precision of 15-bit color, leaves errors of 2.6 to 3.8 in the worst one percent of the picture, and 5, 6 and 5 bits leave 1.8 to 2.4. With 7 bits per channel the 99th percentile stays below 1 and with 8 bits below 0.5, so under this model the precision needed per color lies between 6 and 7 bits per channel. A palette of 256 colors chosen for the image comes close on average but leaves a tail of larger errors where small but distinctive colors are merged into their neighbors.

The second requirement is the number of colors shown together, and it is bounded by the screen rather than by the color space:

![Two rows of four panels. Top row: a photograph of a snake drawn as 120 by 45 half-block cells with 24-bit color and with 256 palette colors chosen for the image, nearly indistinguishable at full size, and an enlarged region of the head in which the 256-color version merges bluish and yellowish accents into a flatter green. Bottom row: a synthetic sunset with the same comparison, where the enlarged region shows faint contours near the sun in the 256-color version.](figures/palette-direct.svg)

*Figure 3. 24-bit direct color and 256 palette colors chosen for the image and set with OSC 4, for 120 × 45 half-block cells without dithering. The palette is a median cut refined with k-means in OKLab, the most favorable case for a palette: one image owns all 256 entries.*

| Image | Color units | Distinct colors, 8 bits | Distinct colors, 7 bits | ΔE of 256 colors, mean / 99th pct. | Bytes, direct color | Bytes, indexed + palette |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Snake | 10,800 | 8,072 | 5,427 | 2.01 / 8.62 | 186,961 | 91,146 + 5,522 |
| Sunset gradient | 10,800 | 7,300 | 3,697 | 1.19 / 3.53 | 154,367 | 41,678 + 5,522 |

*Table 5. Requirements of a 120 × 45 half-block picture. Color units are the upper and lower halves of the cells. ΔE is the CIELAB difference per color unit between 24-bit color and the 256-color palette, without spatial filtering. Bytes count SGR sequences emitted only where a color changes, with semicolon separators, plus the half-block characters and line ends; the palette is uploaded with one OSC 4 sequence per entry.*

A screen of this size never needs more colors than it has color units, about ten thousand, and at 7 bits per channel the two pictures use 5,427 and 3,697 distinct colors. The perceptual requirement is therefore a color precision of about 7 bits per channel together with as many simultaneous colors as there are color units. 24-bit direct color meets it by allowing 16.7 million colors at every cell, at a cost of 1.9 to 3.3 times the bytes of indexed colors in Table 5. A palette with entries of that precision and enough of them, scoped to a picture, would meet it as well. Why terminals went the other way is a question of protocol history, described under [palettes and direct color](../../sixel-format/terminal-graphics-history/README.md#palettes-and-direct-color).

## Practical consequences

These results suggest four rules for a character renderer:

- **Preserve coarse luminance structure.** It carries recognition at the few cycles per object that a cell grid can show.
- **Keep neighboring cells consistent.** In smooth regions, differences between adjacent cells should follow the image. A slowly varying absolute error is acceptable; a fluctuation from cell to cell is not.
- **Dither only as much as the palette requires, and inside cells.** With 24-bit color, do not dither color. With fewer colors, prefer patterns fixed in position at a sub-cell grain over error diffusion between cells.
- **Do not make the cell grid conspicuous.** Sharp, regular block edges mask the image.

The reasoning of this document is an argument from the sensitivity curves, the research above and a perceptual model, checked against the implementations; it has not been tested in a controlled viewing experiment.

## Figures and reproduction

Figures 1 to 3 and the numbers of Tables 1 and 3 to 5 are generated by [`tools/plot_character_cell_figures.py`](../../../tools/plot_character_cell_figures.py) with NumPy and Pillow, using the sensitivity models and color conversions of [`tools/plot_visual_perception_figures.py`](../../../tools/plot_visual_perception_figures.py). Run it to regenerate them and with `--check` to verify that the committed files are current; the figures are hand-written SVG with embedded PNG images, so their bytes do not depend on the Matplotlib version. The measurements are recorded in [`figures/figures.json`](figures/figures.json). Figures 2 and 3 use the repository image `images/snake.png` and reproduce processing chosen with the interactive [tile perception lab](../../../tools/docs/tile-lab/index.html), a development tool for comparing fine grids and tiles. Table 2 is compiled from the source code of the programs it names.

## References

- Bachmann, T. "Identification of spatially quantised tachistoscopic images of faces: How many pixels does it take to carry identity?" European Journal of Cognitive Psychology 3(1):87–103, 1991. doi:[10.1080/09541449108406221](https://doi.org/10.1080/09541449108406221)
- Bachmann, T. *Perception of Pixelated Images*. Academic Press, 2016. Reviewed by V. Goffaux in Frontiers in Psychology 7:1151, 2016. doi:[10.3389/fpsyg.2016.01151](https://doi.org/10.3389/fpsyg.2016.01151)
- Bachmann, T., and N. Kahusk. "The effects of coarseness of quantisation, exposure duration, and selective spatial attention on the perception of spatially quantised ('blocked') visual images." Perception 26(9):1181–1196, 1997. doi:[10.1068/p261181](https://doi.org/10.1068/p261181)
- Costen, N. P., D. M. Parker, and I. Craw. "Spatial content and spatial quantisation effects in face recognition." Perception 23(2):129–146, 1994. doi:[10.1068/p230129](https://doi.org/10.1068/p230129)
- Harmon, L. D., and B. Julesz. "Masking in visual recognition: effects of two-dimensional filtered noise." Science 180(4091):1194–1197, 1973. doi:[10.1126/science.180.4091.1194](https://doi.org/10.1126/science.180.4091.1194)
- Mannos, J. L., and D. J. Sakrison. "The effects of a visual fidelity criterion on the encoding of images." IEEE Transactions on Information Theory 20(4), 1974.
- Morrone, M. C., D. C. Burr, and J. Ross. "Added noise restores recognizability of coarse quantized images." Nature 305:226–228, 1983. doi:[10.1038/305226a0](https://doi.org/10.1038/305226a0)
- Mullen, K. T. "The contrast sensitivity of human colour vision to red-green and blue-yellow chromatic gratings." Journal of Physiology 359:381–400, 1985. doi:[10.1113/jphysiol.1985.sp015591](https://doi.org/10.1113/jphysiol.1985.sp015591)
- Oliva, A., and P. G. Schyns. "Coarse blobs or fine edges? Evidence that information diagnosticity changes the perception of complex visual stimuli." Cognitive Psychology 34(1):72–107, 1997. doi:[10.1006/cogp.1997.0667](https://doi.org/10.1006/cogp.1997.0667)
- Oliva, A., and P. G. Schyns. "Diagnostic colors mediate scene recognition." Cognitive Psychology 41(2):176–210, 2000. doi:[10.1006/cogp.1999.0728](https://doi.org/10.1006/cogp.1999.0728)
- Pelli, D. G. "Close encounters: an artist shows that size affects shape." Science 285(5429):844–846, 1999. doi:[10.1126/science.285.5429.844](https://doi.org/10.1126/science.285.5429.844)
- Schyns, P. G., and A. Oliva. "From blobs to boundary edges: evidence for time- and spatial-scale-dependent scene recognition." Psychological Science 5(4):195–200, 1994. doi:[10.1111/j.1467-9280.1994.tb00500.x](https://doi.org/10.1111/j.1467-9280.1994.tb00500.x)
- Wang, Z., A. C. Bovik, and B. L. Evans. "Blind measurement of blocking artifacts in images." Proceedings of the 2000 International Conference on Image Processing, vol. 3, 981–984, 2000. doi:[10.1109/ICIP.2000.899622](https://doi.org/10.1109/ICIP.2000.899622)
- Wang, Z., H. R. Sheikh, and A. C. Bovik. "No-reference perceptual quality assessment of JPEG compressed images." Proceedings of the 2002 International Conference on Image Processing, vol. 1, I-477–I-480, 2002. doi:[10.1109/ICIP.2002.1038064](https://doi.org/10.1109/ICIP.2002.1038064)
- Xu, X., L. Zhang, and T.-T. Wong. "Structure-based ASCII art." ACM Transactions on Graphics 29(4):52, 2010. doi:[10.1145/1778765.1778789](https://doi.org/10.1145/1778765.1778789)
- Yip, A. W., and P. Sinha. "Contribution of color to face recognition." Perception 31(8):995–1003, 2002. doi:[10.1068/p3376](https://doi.org/10.1068/p3376)
- Zhang, X., and B. A. Wandell. "A spatial extension of CIELAB for digital color-image reproduction." Journal of the Society for Information Display 5(1):61–63, 1997. doi:[10.1889/1.1985127](https://doi.org/10.1889/1.1985127)
