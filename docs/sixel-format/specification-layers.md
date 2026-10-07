# SIXEL Specification Status and Interpretation Layers

## Scope

This document explains what is, and what is not, formally specified about SIXEL, and how an implementation's behavior is therefore assembled from several layers of decisions rather than read from a single specification. It also states libsixel's position on those layers. The [SIXEL format overview](README.md) describes the wire syntax itself; this document describes the standing of that syntax.

## What is formally specified

### The control-string envelope

SIXEL is transmitted as an ECMA-48 control string. [ECMA-48](https://ecma-international.org/wp-content/uploads/ECMA-48_5th_edition_june_1991.pdf) (5th edition, 1991; equivalent to ISO/IEC 6429) defines DEVICE CONTROL STRING (DCS, section 8.3.27) as the opening delimiter of a control string for device control use, closed by STRING TERMINATOR (ST). The command string between them may contain only the bit combinations `0x08..0x0d` and `0x20..0x7e`, which is why every SIXEL body byte is a printable 7-bit character.

ECMA-48 deliberately does not define the content of a device control string. Its purpose and format are specified by the most recent IDENTIFY DEVICE CONTROL STRING (IDCS), if any, and otherwise depend on the sending and receiving devices. SIXEL streams do not use IDCS. By the standard's own terms, therefore, a SIXEL stream is a device-dependent command string whose meaning is bound to the receiving device, not a device-independent data format.

The header `P1;P2;P3 q` that selects SIXEL follows DEC's convention of reusing control-sequence parameter and final-byte classes at the start of a DCS. That convention is DEC's, not part of ECMA-48's definition of DCS.

### DEC device documentation

The only primary description of the SIXEL command string is DEC's documentation for the devices that accepted it: printer manuals such as those for the LA50, LN03, and LJ250, and the programmer references for graphics terminals such as the VT240 and VT330/VT340, most completely Chapter 14 of the [*VT330/VT340 Programmer Reference Manual, Volume 2*](https://bitsavers.trailing-edge.com/pdf/dec/terminal/vt340/EK-VT3XX-GP-002_VT330_VT340_Graphics_Programming_198805.pdf). No ANSI, ISO, or ECMA standard defines SIXEL, and no publicly available, device-independent DEC specification consolidates these descriptions.

Each manual describes one device or product family. The descriptions agree on the core encoding but differ in device limits and in parameters that a device ignores: the VT300 series ignores the `P3` grid size, the VT330 implements four color registers and the VT340 sixteen, and printers and terminals treat the page or screen differently. The formal layer is therefore a set of device-specific documents with a shared core, not one normative text. Contemporary practitioners described it the same way; in a 1995 comp.os.vms thread reproduced in [Shuford's sixel news archive](https://shuford.invisible-island.net/sixel_graphics_news.txt), Jerry Leichter wrote that the format is "fully explained in the documentation of the printers that support it."

The same thread notes that "sixel" names three related things: a column of six vertically arranged pixels, the encoding of six bits as a character offset from `0x3f`, and the graphics data format carried in the SIXEL DCS. Soft-character-set loading (DECDLD) shares the first two but not the third. In this documentation, "SIXEL" means the DCS graphics format.

### What is not specified

SIXEL has no definition as an image or interchange format. In particular:

- there is no file signature, container, directory, or checksum; `file(1)` reports a `.six` file as ASCII text with escape sequences;
- the IANA media-type registry contains no SIXEL type;
- nothing defines how a file containing several DCS strings, surrounding text, or other terminal controls is to be interpreted as an image;
- color register values are percentages of device primaries with no stated color space, so colorimetric meaning is device-dependent (see the [graphics color-management proposal](../protocols/graphics-color-management.md) for one way to supply it outside the stream);
- placement, scrolling, and the result of mixing graphics with text are terminal behaviors rather than properties of the stream.

## De facto use as an image file

Although SIXEL was never defined as a file format, recorded SIXEL streams were stored, converted, and exchanged as files with the `.six` suffix from at least the late 1980s. Viewing such a file meant replaying it to a capable device, for example with VMS `TYPE`, Unix `cat`, or an unfiltered print queue. The following examples show that usage:

| Date | Source | Usage |
| --- | --- | --- |
| 1980s | [hackerb9/vt340test](https://github.com/hackerb9/vt340test) | Preserves graphics demonstration files distributed with Kermit in the 1980s, including SIXEL files, together with VMS-era VT340 support files. |
| 1991-05 | [`ppmtosix` posting to alt.sources](https://www.tuhs.org/Usenet/alt.sources/1991-May/002739.html) | Richard Vinci's converter, written for the DEC LJ250 color inkjet printer, describes its output as a "SIX file" and shows `xwdtopnm` output piped into `ppmtosix > dot.six` as its example. It became Netpbm's [`ppmtosixel`](https://netpbm.sourceforge.net/doc/ppmtosixel.html). |
| 1992-10 | comp.os.vms, "Solution: Displaying GIFs on a VT330" (in [Shuford's archive](https://shuford.invisible-island.net/sixel_graphics_news.txt)) | A user converts GIF images with a PBMPLUS pipeline that reduces them to four colors and ends in `ppmtosixel >OUTFILE`, then displays the file on a VT330 with `TYPE`. |
| 1994–1995 | comp.terminals, "Q: ReGIS Graphics on VT ?" (same archive) | A GIF-to-SIXEL converter for VMS, `GIFSIXEL.ZIP`, is distributed by anonymous FTP and recommended for viewing GIF files on VT-series terminals with SIXEL emulation. |
| 1995-05 | comp.os.vms, "What is SIXEL?" (same archive) | DECwindows Motif's screen-capture utility saves a SIXEL file, which the poster displays on DECterm by typing it, and asks whether SIXEL is a DEC format or an ANSI standard. |
| 1996-02 | comp.protocols.kermit.misc, "Is sixel vesa 16 colors?" (same archive) | A user converts GIF images to "sixel files" with the portable bitmap utilities and finds that MS-DOS Kermit maps their up to 256 color definitions onto 16 display colors. |
| 1996-11 | [Kermit `sixel` distribution directory](https://sunsite3.icm.edu.pl/pub/kermit/sixel/) (mirror) | Sample pictures such as `biplane.six`, `chess.six`, and an 800 by 600 `time.six` are distributed with `PCX2SIX`, a DOS converter from PCX images. |
| 2004 | Shuford's sixel news archive | Recommends a Mac OS X image editor as a way to convert old SIXEL images to more portable formats. |

These files circulated and were converted like images, but each remained a recording of commands for some device. `ppmtosix` output carried a printer header and footer for the LJ250, and the same file rendered with 16 colors under MS-DOS Kermit and with a different palette capacity elsewhere. Their history establishes SIXEL as a de facto image format in practice; it does not supply the missing definition of what a `.six` file means independently of the device that replays it.

## Three layers of implementation decisions

Because the formal layer is limited to the envelope and device manuals, the behavior of a SIXEL encoder, decoder, or terminal is determined by decisions in three layers:

| Layer | Sources | Typical subjects | Standing |
| --- | --- | --- | --- |
| 1. Formal specification | ECMA-48 control strings; DEC device manuals | DCS/ST framing, header parameters, data characters, `!`, `"`, `#`, `$`, `-` | Implementations should follow it. |
| 2. Emulation | Practitioner references such as Chris Chiesa's [*All About SIXELs*](https://shuford.invisible-island.net/all_about_sixels.txt); hardware research such as [hackerb9/vt340test](https://github.com/hackerb9/vt340test) | Observed behavior of real DEC devices where the manuals are silent or ambiguous | Often followed, sometimes deliberately not. |
| 3. Modern interpretation | Each implementation's own policy | Features and devices that the original specification did not contemplate | Depends on the implementation. |

### Layer 1: formal specification

The formal layer covers how a stream is delimited and how its commands are encoded: the DCS envelope and its 7-bit and 8-bit forms, the `P1`, `P2`, and `P3` header parameters, the sixel data characters, and the repeat, raster-attribute, color, carriage-return, and new-line commands. The [SIXEL format overview](README.md#wire-format) documents this layer. Its transmission rules are comparatively clear. Even within it, device limits such as the number of color registers or the maximum repeat count come from the particular device documented, not from a common definition.

### Layer 2: emulation

The second layer records what DEC hardware actually did. *All About SIXELs* (1990) gathered generation techniques and device behavior from manuals and experiments. The vt340test project compares a real VT340 with modern terminals using shared test streams. Subjects in this layer include how raster attributes and `P2` establish the background area, where the text cursor is left after an image, the lifetime of color registers across images, and the effect of redefining a register after it has been used for painting on a device whose frame buffer stores palette indices.

Emulation evidence answers how a VT340 behaved, not how every SIXEL receiver must behave. A modern terminal may adopt a VT340 behavior for compatibility or decline it because it conflicts with other goals, and both choices are defensible.

### Layer 3: modern interpretation

The third layer covers questions that the original devices never posed:

- **Scrollback and resizing.** Whether an image scrolls into history, how it is clipped or reflowed when the window changes size, and whether it survives a screen clear.
- **Display resolution.** How SIXEL pixels relate to device pixels and character cells on high-density displays, and whether images are scaled.
- **Pixel aspect ratio.** The `P1` macro parameter and the default of `P1=0` describe vertical:horizontal ratios such as 2:1 for devices with non-square pixels. On square-pixel displays an implementation must decide whether to honor them, treat them as 1:1, or rely on raster attributes.
- **Palette capacity and scope.** Register numbers beyond 255, palettes larger than 256 entries, and whether each image receives private registers or shares them with other images.
- **Color meaning.** Which color space register percentages denote, how precisely they are stored, and whether earlier pixels change when a register is later redefined.
- **Compositing.** How `P2=1` transparency, text drawn over images, and terminal multiplexers interact with graphics.

Different terminals reach different conclusions on these questions. Two receivers can both follow layer 1 exactly and still render or place the same stream differently.

### How the layers combine

Layer 1 should be followed, layer 2 is followed selectively, and layer 3 is a matter of each implementation's direction. An application that needs predictable behavior must therefore determine the relevant layer-2 and layer-3 choices of the receiving terminal, by capability reporting where it exists, instead of assuming that SIXEL support implies one behavior.

## libsixel's position

libsixel is a library that first appeared in 2014. It is not DEC's successor, a standards body, or the owner of SIXEL. It does not redefine SIXEL for modern terminals: for one project to choose layer-3 semantics and present them as the definition of SIXEL would exceed the authority of any single implementation and would effectively appropriate a format that many independent implementations share.

Within that position, libsixel:

- emits streams that use only layer-1 syntax unless an explicitly requested option says otherwise; ordinary output declares raster attributes with a 1:1 pixel aspect ratio (`"1;1;Ph;Pv`) instead of relying on the `P1` macro parameter;
- documents the layer-2 or layer-3 assumptions on which particular output depends, such as the paint-time color resolution required by high-color mode or the receiver support required by the [OR-mode dialect](../encoder/output/or-mode/README.md) and [terminal macros](../encoder/output/terminal-macros.md), and enables those features only on explicit request;
- describes its decoder's choices for undefined cases as libsixel decoder behavior, not as the meaning of SIXEL;
- presents protocol additions, such as the [graphics color-management proposal](../protocols/graphics-color-management.md), as separately negotiated, receiver-independent proposals rather than as changes to ordinary SIXEL.

### Interpretations libsixel adopts

Some libsixel behavior necessarily rests on layer-3 interpretations. They are stated here so that they are not mistaken for properties of SIXEL itself. Each is libsixel's choice; another implementation can reasonably choose differently. They differ in reach. Interpretation 1 is a default that applies to all ordinary output. Interpretations 2 and 3 apply only when a specific encoder option is selected, so a user can avoid them entirely when they do not suit the receiving device.

#### Interpretation 1: register RGB values default to sRGB

SIXEL color registers carry RGB or HLS coordinates as percentages, but the DEC manuals do not tie them to a colorimetric definition. Chapter 2 of the VT330/VT340 graphics manual describes RGB and HLS as color coordinate systems and lists default maps without stating primaries, a white point, or a transfer function. By default, libsixel writes register values as gamma-encoded sRGB (`img2sixel -U gamma`; see [output color space](../encoder/output/output-colorspace.md)).

This matches the practice of current software terminals, which effectively treat register values as ordinary sRGB code values. The convention is implicit rather than specified, and where it first appeared has not been established. It is also anachronistic: sRGB was proposed in 1996 and standardized as IEC 61966-2-1 in 1999, after the DEC devices that defined SIXEL. For faithful emulation, the interpretation would instead follow the NTSC video practice of the DEC era or measurements of the devices' output. "NTSC" is itself ambiguous here: the 1953 FCC NTSC primaries were not what monitors of the 1980s actually used, and SMPTE-C (SMPTE RP 145, 1987) standardized the phosphor primaries then in practical use in NTSC-region displays. `img2sixel -U smpte-c` was added as a candidate answer to what NTSC meant for these devices. It remains a candidate: confirming or replacing it requires device research that is not yet available.

The approximation is probably not far off. sRGB was designed around typical CRT displays of the 1990s, with ITU-R BT.709 primaries close to the SMPTE-C phosphors then common in North American monitors and an effective transfer characteristic near a 2.2 power law, so the CRTs of the DEC era likely responded similarly. This remains an inference, not a measurement.

One observation keeps the question open. Read as sRGB, the VT340 default color map is unevenly bright. Its six saturated hues all have HLS lightness 50 and saturation 60, but blue (`20;20;80` percent RGB) has a CIELAB lightness of about 33, while red has about 44, green about 72, and yellow about 80. Much of this follows from HLS lightness not being a perceptual lightness: equal HLS lightness gives unequal luminance on any display with conventional primaries. Whether DEC also adjusted for its monitors cannot be determined without measurements of the hardware.

An equally defensible reading is that ordinary SIXEL specifies no color space at all, so that colorimetric meaning requires agreement with the receiver. libsixel's default does not exclude that reading. Applications that manage color for a particular device can choose a [fixed palette](../encoder/palette-construction/fixed-palettes.md) such as `vt340color` and an explicit output representation with `-U`, including the emulation-oriented `-U smpte-c`, or negotiate one through the [graphics color-management proposal](../protocols/graphics-color-management.md).

#### Interpretation 2: a later paint replaces an earlier one

libsixel treats painting a pixel that has already been painted as replacing its color. The `-E size` encoder policy depends on this: it fills a span with one color and lets later colors repaint their own pixels, which can shorten the stream (see [encoding policy](../encoder/output/encode-policy/README.md#why-size-can-save-bytes)). libsixel's decoder implements the same replacement.

This holds for a display whose frame buffer stores one value per pixel, but not in general. On a color printer, overprinting a position deposits more ink and mixes the colors, and DEC's color printing was described at the time as successive overstruck passes, one per color plane. Repainting is therefore a device-dependent technique rather than a SIXEL guarantee. The interpretation is confined to `-E size`. The default `-E auto` and the explicit `-E fast` paths never paint a pixel more than once and do not depend on it, so output for a receiver where repainting would mix colors simply does not select `-E size`. The [OR-mode dialect](../encoder/output/or-mode/README.md) deliberately defines a different combining rule and must be requested from a receiver that supports it.

#### Interpretation 3: a color register can be redefined during an image

`img2sixel -I` (high-color mode) increases the number of colors in one SIXEL image by redefining color registers after they have been used for painting. It requires paint-time color resolution, in which each pixel keeps the RGB value its register held when it was painted. The [high-color guide](../encoder/output/high-color/README.md#the-terminal-decides-whether-it-works) describes the two possible receiver models.

This interpretation clearly conflicts with DEC terminals. On a VT340 the frame buffer stores register indices, so redefining a register recolors every pixel already painted with it. Software terminals differ in which model they implement. Whether DEC printers could support the technique has not been verified. The interpretation is confined to `-I`: ordinary output defines each register before use and never redefines a register it has already painted with, so it does not depend on either receiver model. High-color output is enabled only on explicit request and should not be selected for a receiver that follows the live-register model. For decoding, libsixel's direct decoder (`sixel2png --direct`) applies paint-time resolution, while an indexed decode can retain only the final palette.

If applications require terminal graphics whose display and control semantics are unambiguous on modern terminals and devices, the appropriate course is a new protocol designed for those terminals and devices, not a redefinition of SIXEL.

## References

- Ecma International, [*ECMA-48: Control Functions for Coded Character Sets*, 5th edition, June 1991](https://ecma-international.org/wp-content/uploads/ECMA-48_5th_edition_june_1991.pdf), sections 8.3.27 (DCS) and 8.3.65 (IDCS).
- Digital Equipment Corporation, [*VT330/VT340 Programmer Reference Manual, Volume 2: Graphics Programming*, second edition, May 1988](https://bitsavers.trailing-edge.com/pdf/dec/terminal/vt340/EK-VT3XX-GP-002_VT330_VT340_Graphics_Programming_198805.pdf), Chapter 14.
- Chris Chiesa, [*All About SIXELs*](https://shuford.invisible-island.net/all_about_sixels.txt), Usenet posting, 1990-09-29.
- [hackerb9/vt340test](https://github.com/hackerb9/vt340test), tests and observations of a real VT340 compared with modern terminal emulators.
- Richard Vinci, [*ppmtosixel converter and man page*](https://www.tuhs.org/Usenet/alt.sources/1991-May/002739.html), alt.sources, 1991-05-12.
- Richard S. Shuford, [*Sixel Graphics News*](https://shuford.invisible-island.net/sixel_graphics_news.txt), an archive of Usenet discussions from 1992 to 2004.
- [Kermit `sixel` distribution directory](https://sunsite3.icm.edu.pl/pub/kermit/sixel/), mirrored from the Kermit Project archive, files dated 1995–1996.
