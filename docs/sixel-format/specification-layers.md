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
- the IANA media-type registry contains no SIXEL type, and neither do the common media-type and file-identification databases (see [media types in use](#media-types-in-use));
- nothing defines how a file containing several DCS strings, surrounding text, or other terminal controls is to be interpreted as an image;
- color register values are percentages of device primaries with no stated color space, so colorimetric meaning is device-dependent (see the [graphics color-management proposal](../protocols/graphics-color-management.md) for one way to supply it outside the stream);
- placement, scrolling, and the result of mixing graphics with text are terminal behaviors rather than properties of the stream.

### Media types in use

No media type for SIXEL has been registered with IANA. In October 2026 there was also no SIXEL entry in freedesktop.org's shared-mime-info database, the jshttp `mime-db` database used by many web servers and libraries, Apache httpd's `mime.types`, or the `file(1)` magic database.

A small number of projects nevertheless label SIXEL data with an unregistered media type. `image/x-sixel` is the more common form:

| Project | Use of `image/x-sixel` |
| --- | --- |
| [Wikidata, item Q7532980](https://www.wikidata.org/wiki/Q7532980) | Lists `image/x-sixel` as the media type of the Sixel format, with the extensions `six` and `sixel`. |
| [chawan](https://git.sr.ht/~bptato/chawan) | This terminal web browser labels the SIXEL data it produces for display as an `image/x-sixel` blob. |
| [clifm](https://github.com/leo-arch/clifm) | The terminal file manager maps `six` and `sixel` to `image/x-sixel` in its `mime.types` and returns that type from its built-in content sniffer when it recognizes a SIXEL introducer. |
| [vv](https://github.com/hackerb9/vv) | This SIXEL image viewer assigns `image/x-sixel` to files named `*.six` or `*.sixel`, with a comment that the magic database does not recognize the format. |
| [dexvert](https://github.com/Sembiance/dexvert) | The file-format identification and conversion tool declares `image/x-sixel` for its Sixel format. |
| [Casciian](https://github.com/crramirez/casciian) | The Java text user interface library registers its SIXEL image decoder for `image/x-sixel`. |

`image/sixel` without the prefix appears less often. The Julia package [TerminalGraphics.jl](https://github.com/m-j-w/TerminalGraphics.jl) documents `display(MIME"image/sixel", image)` for its SIXEL display, and the Neovim Jupyter integration [Jove.nvim](https://github.com/AM-I-Human/Jove.nvim) registers an IPython display formatter under `image/sixel`.

libsixel's own integrations follow the majority practice: its GdkPixbuf loader, Quick Look extension, Freedesktop thumbnailer, and shared-mime-info definition declare only `image/x-sixel`. libsixel uses this name because the implementations listed above use it, not because it is an official type; these labels are conventions among individual projects, not a registration. Both forms are unregistered. [RFC 6838](https://www.rfc-editor.org/rfc/rfc6838) governs registration with IANA; its section 3.4 strongly discourages unregistered types, and [RFC 6648](https://www.rfc-editor.org/rfc/rfc6648) deprecates the `x-` convention. These rules do not prevent local media-type databases from naming unregistered types: freedesktop.org's shared-mime-info alone defines about one hundred `image/x-*` types, and GdkPixbuf's standard loaders declare types such as `image/x-tga` and `image/x-portable-pixmap`. For a widely deployed unregistered type, Appendix A of RFC 6838 says that it should be re-registered under a faceted vendor or personal subtype name, optionally keeping the original name as a deprecated alias, and that only if this is not possible may the unfaceted name be registered, with the approval of both the media types reviewer and the IESG. Every route requires a specification and a party responsible for change control, which would amount to defining SIXEL as an interchange format. libsixel therefore uses `image/x-sixel` as the de facto name and does not register a type itself; `image/sixel` without the prefix is not used because no such registration has been approved. A label also does not resolve the questions in the previous section, such as how a file containing several DCS strings or other terminal controls is to be interpreted.

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

### Incompatible extensions kept for historical preservation

libsixel also emits two extensions that are incompatible with the formal layer. They are not recommended for new applications. libsixel adopts and maintains them because they worked on real platforms and terminals, so that streams written for those receivers can still be produced and examined; their presence in libsixel is not an endorsement of their design.

#### OR mode

The [OR-mode dialect](../encoder/output/or-mode/README.md) encodes palette indices as bit planes that the receiver combines with bitwise OR. It originated with the NetBSD/x68k `ite` console and the `sayaka` client, and it worked on those receivers.

It has two problems that libsixel does not try to repair:

- **Overloaded DCS parameter.** OR mode is selected by `P2=5`. DEC defines `P2` as the background-selection parameter with the values 0, 1, and 2, so `5` reuses a parameter whose meaning is already fixed instead of identifying a new protocol. A receiver that does not know the dialect applies its own handling of an undefined `P2` value and decodes the stream as ordinary SIXEL, producing wrong colors without any indication of failure. The stream itself offers no means of negotiation.
- **Incompatible change to the encoding.** Combining bit planes by OR replaces the overwrite semantics of ordinary SIXEL painting. The same data characters therefore mean different pixels under the two models, so OR-mode output is not a variant that ordinary receivers can approximate.

libsixel enables OR mode only on explicit request and does not use it as a default or fallback.

#### DRCS-SIXEL

[DRCS-SIXEL](../encoder/output/drcs.md) embeds a SIXEL image in a soft-character download so that a terminal registers the image as character-cell glyphs. It originated with RLogin in 2012 and was later specified with mlterm; see the [project history](../project-history.md#drcs-sixel-and-unicode-plane-16). It is an experimental extension, and it is incompatible with DEC's design: it is introduced with the same final byte `{` as DEC's [DECDLD](https://vt100.net/docs/vt510-rm/DECDLD.html) soft-font download, and both the [v2](https://github.com/arakiken/mlterm/blob/master/drcssixel/DRCS-SIXEL-v2) and [v3](https://github.com/arakiken/mlterm/blob/master/drcssixel/DRCS-SIXEL-v3) specifications distinguish it from DECDLD only by fixing the sixth parameter, `Pt`, to `3` (`DCS Pfn; Pcn; Pe; Pcmw; Pss; 3; Pcmh; Pcss { SP Dscs <SIXEL> ST`). DEC defines `Pt` as the text or full-cell selector with the values 0 and 1 for text and 2 for full cell, and states that a terminal ignores a DECDLD string whose `Pcmw`, `Pt`, and `Pcmh` combination is not one of the defined ones. A conforming DECDLD receiver should therefore discard a DRCS-SIXEL definition, while an implementation that does not validate the combination may misinterpret the SIXEL body as soft-font data. The `Pt` value makes the two formats distinguishable to a receiver that knows both, but this does not make the extension a recommended design.

DRCS-SIXEL versions are distinguished separately, outside the DECDLD string. The versions differ mainly in the DRCS mapping method (DRCSMM), which assigns Unicode Plane 16 code points to character-set designations and determines the designation (`Dscs`) used in the definition. A receiver selects the mapping through DEC private modes: mode 8800 enables mappings v1 and v2, and mode 8801 selects v3, as listed under drcsterm in [mlterm's control-sequence reference](https://github.com/arakiken/mlterm/blob/master/doc/en/ControlSequences). The `Pt=3` marker is the same in every version. These mode numbers are not DEC assignments; like the `Pt` value, they are conventions of the implementations that maintain the extension.

The incompatibility went largely unnoticed because, in the early 2010s, almost no terminal emulators in use implemented DECDLD, so there was no practical need to remain compatible with it.

The libsixel project's position is that DRCS-SIXEL should eventually move to its own control-function space, identified by a distinct combination of intermediate bytes and final byte instead of DECDLD's final byte and an undefined parameter value. libsixel continues to emit the existing form for the receivers that implement it, and does not define the replacement unilaterally; such a change belongs to the implementations that maintain the extension.

If applications require terminal graphics whose display and control semantics are unambiguous on modern terminals and devices, the appropriate course is a new protocol designed for those terminals and devices, not a redefinition of SIXEL.

## References

- Ecma International, [*ECMA-48: Control Functions for Coded Character Sets*, 5th edition, June 1991](https://ecma-international.org/wp-content/uploads/ECMA-48_5th_edition_june_1991.pdf), sections 8.3.27 (DCS) and 8.3.65 (IDCS).
- Digital Equipment Corporation, [*VT330/VT340 Programmer Reference Manual, Volume 2: Graphics Programming*, second edition, May 1988](https://bitsavers.trailing-edge.com/pdf/dec/terminal/vt340/EK-VT3XX-GP-002_VT330_VT340_Graphics_Programming_198805.pdf), Chapter 14.
- Chris Chiesa, [*All About SIXELs*](https://shuford.invisible-island.net/all_about_sixels.txt), Usenet posting, 1990-09-29.
- [hackerb9/vt340test](https://github.com/hackerb9/vt340test), tests and observations of a real VT340 compared with modern terminal emulators.
- Richard Vinci, [*ppmtosixel converter and man page*](https://www.tuhs.org/Usenet/alt.sources/1991-May/002739.html), alt.sources, 1991-05-12.
- Richard S. Shuford, [*Sixel Graphics News*](https://shuford.invisible-island.net/sixel_graphics_news.txt), an archive of Usenet discussions from 1992 to 2004.
- [Kermit `sixel` distribution directory](https://sunsite3.icm.edu.pl/pub/kermit/sixel/), mirrored from the Kermit Project archive, files dated 1995–1996.
