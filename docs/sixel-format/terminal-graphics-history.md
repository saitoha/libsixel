# Terminal Graphics History

## Scope

SIXEL is one of many ways that have been used to show pictures through a character terminal or a terminal-like connection. Its strengths and gaps, which the [specification status and interpretation layers](specification-layers.md) describe, are easier to judge against that wider history. Two lineages run through that history. One sends pixels or drawing commands: vector storage-tube terminals, videotex picture coding, DEC's ReGIS and SIXEL, and the image protocols of software terminal emulators. The other builds pictures from characters: the block mosaics of teletext and videotex, the semigraphics of home computers, and the Unicode mosaic and Braille characters used by AAlib, libcaca, chafa, and similar tools. This document surveys both.

It is a survey, not an exhaustive catalog. Dates are those of the cited releases, specifications, or first public commits; a product announced earlier or released in stages can have other dates in other sources. The libsixel [project history](../project-history.md) covers the SIXEL implementations closest to libsixel in more detail.

## Timeline

| Year | Event | Kind |
| --- | --- | --- |
| 1875 | Typewriter art in a newspaper advertisement | Character art |
| 1960s | Teleprinter and RTTY art; line-printer art | Character art |
| 1966 | Knowlton and Harmon's *Studies in Perception I* maps a scanned photograph to typographic symbols | Character rendering |
| 1972 | Tektronix 4010 storage-tube terminal | Vector terminal |
| 1974 | Tektronix 4014, whose command set became the common target of later Tektronix emulations | Vector terminal |
| 1974 | Ceefax teletext with 2-by-3 block mosaic characters | Character mosaic |
| 1977–1979 | Home-computer semigraphics, such as the TRS-80's 2-by-3 blocks and the PC-8001's 2-by-4 patterns | Character mosaic |
| 1978 | Telidon trials in Canada | Videotex, alpha-geometric |
| 1979 | Prestel public service in the United Kingdom | Videotex, alpha-mosaic |
| 1980–1981 | CCITT T.4, the Group 3 facsimile coding | Facsimile |
| 1981 | DEC ReGIS on the VT125, then the GIGI | Vector graphics language |
| 1980s–1990s | VT100 animations, such as Christmas cards, circulate as escape-sequence text files | Character animation |
| 1982 | Minitel terminals in France, using the Antiope-based display standard | Videotex, alpha-mosaic |
| 1983 | NAPLPS standardized as ANSI X3.110 and CSA T500; Bildschirmtext service in Germany | Videotex |
| 1984 | NTT's CAPTAIN service in Japan; DEC VT240 programmer reference manual, documenting SIXEL on a terminal screen | Videotex, alpha-photographic; raster terminal |
| 1985–1986 | ESPRIT PICA photovideotex compression project; formation of JPEG | Image coding |
| 1987 | DEC VT330 and VT340 with ReGIS, SIXEL, and Tektronix 4010/4014 emulation; the Christmas Tree EXEC worm draws a text Christmas tree | Raster terminal; character art |
| 1988 | JBIG split from JPEG for bi-level images | Image coding |
| 1992 | RIPscrip vector graphics for bulletin board systems; ITU-T T.81 (JPEG) | BBS graphics; image coding |
| Early 1990s | ANSI art on bulletin board systems; ASCII art on Usenet, later in alt.ascii-art | Character art |
| 1994 | Tera Term with Tektronix 4010 emulation | Software emulator |
| 1997 | AAlib renders images as ASCII art | Character rendering |
| 1999 | Unicode 3.0 adds Braille Patterns (U+2800–U+28FF) | Unicode |
| 1995–1996 | `image/naplps` registered with IANA; UCL's NAPLPS viewer plug-in for Netscape Navigator 2.0 | Videotex on the web |
| 2001 | w3m inline image support through the external `w3mimgdisplay` helper | Terminal browser |
| 2002 | Unicode 3.2 adds quadrant block elements, proposed as terminal graphics by Frank da Cruz | Unicode |
| 2003 | libcaca 0.1 adds color character rendering with dithering | Character rendering |
| 2009 | RLogin adds Tektronix 4014/4105 modes | Software emulator |
| 2010 | RLogin 2.9.0 adds SIXEL and ReGIS | SIXEL revival |
| 2011 | GateOne detects image files in the output stream | In-band, no protocol |
| 2012 | tanasinn with Tektronix, SIXEL, and URL-based OSC extensions; Terminology `tycat` passes file paths or URIs; mlterm adds SIXEL | Software emulators |
| 2013 | FinalTerm semantic escape sequences (OSC 133); xterm patch #294 adds SIXEL | Shell integration; SIXEL revival |
| 2014 | iTerm2 inline images (OSC 1337); libsixel; haru's `w3mimg-sixel`; w3m SIXEL support; drawille draws with Braille patterns | Image protocol; SIXEL tooling; character rendering |
| 2015 | DomTerm, later with HTML insertion through OSC 72 | HTML in terminal |
| 2017 | kitty 0.4.0 introduces its graphics protocol | Image protocol |
| 2018 | chafa, later combining character mosaics with SIXEL, kitty, and iTerm2 output | Character rendering and image protocols |
| 2019 | Terminal working group discussion of a simple image display sequence | Standardization attempt |
| 2020 | mintty 3.3.0 adds Tektronix 4014 emulation; Unicode 13.0 adds Symbols for Legacy Computing, including 60 sextants from teletext and videotex | Software emulator; Unicode |
| 2021 | Nick Black's STEGAP proposal; Windows Terminal adds DECDLD soft fonts; xterm.js image add-on | Proposals and implementations |
| 2024 | tmux 3.4 adds SIXEL; Windows Terminal Preview 1.22 adds SIXEL; Unicode 16.0 adds 230 octants | SIXEL adoption; Unicode |

## Vector graphics before raster terminals

The Tektronix 4010 (1972) and 4014 (1974) drew vectors on a storage tube, which retained the image without a frame buffer. A host sent coordinates in a compact text encoding, so graphics could travel over the same serial line as text. The 4014's command set became a de facto target for plotting software and, consequently, for emulation.

DEC terminals emulated it as well. The VT330 and VT340 included Tektronix 4010/4014 emulation alongside ReGIS and SIXEL. For the earlier VT100, third-party boards such as Retro-Graphics' upgrade, often called the "VT640", added Tektronix-compatible graphics; DEC's own Advanced Video Option (AVO) for the VT100 extended text attributes and columns and was not a graphics option.

Software emulators kept the mode alive long after the hardware: early xterm versions emulated the Tektronix 4014 beside the VT102, Tera Term (1994) provides a TEK window with 4010 emulation, RLogin 2.6.2 (2009) added Tektronix 4014/4105 modes, tanasinn (2012) included a Tektronix module, and mintty 3.3.0 (2020-08-06) added Tektronix 4014 emulation.

DEC's own vector language, ReGIS, appeared on the VT125 in July 1981 and the GIGI shortly afterwards. SIXEL, DEC's bitmap language, came from its printers and reached a terminal screen with the VT240, whose programmer reference manual is dated August 1984; the VT330 and VT340 followed in 1987.

## Videotex

Videotex delivered pages of text and graphics to home terminals over telephone lines, and teletext broadcast similar pages in television signals. Its national systems, developed from the late 1970s, used three families of picture coding:

- **Alpha-mosaic.** Character cells subdivided into block-graphics patterns, as in the United Kingdom's Prestel (1979), France's Antiope standard used by Minitel (1982), and Germany's Bildschirmtext (1983).
- **Alpha-geometric.** Drawing commands for points, lines, arcs, polygons, and fills, as in Canada's Telidon (1978). Its encoding was standardized as NAPLPS (ANSI X3.110 and CSA T500, 1983) and later used by the Prodigy online service and by bulletin board systems.
- **Alpha-photographic.** Transmission of the page as a picture, as in NTT's CAPTAIN in Japan (1984 to 2002). Because Japanese text needed thousands of characters, CAPTAIN sent pages largely as pre-rendered images, using coding similar to facsimile.

Videotex outlived its services in several ways. NAPLPS reached the web: `image/naplps` was registered with IANA in 1995, an IETF working group addressed NAPLPS as a MIME content type, and the Japanese company Universal Computer Laboratory (UCL) offered a NAPLPS viewer plug-in for Netscape Navigator 2.0 and later. Bulletin board systems also developed their own vector graphics, notably TeleGrafix's RIPscrip (1992).

### Videotex and modern image coding

The picture-coding work of the videotex era is a direct ancestor of image formats in use today:

- **JPEG.** According to the [presentation](https://www.ecma-international.org/wp-content/uploads/IS-presentation-to-JTC1SC29WG1-regarding-JPEG25-celebration.pdf) given at the 25th anniversary of the JPEG standard, three projects triggered JPEG, and the first was the CCITT and ISO *Videotex Photographic Mode* project. The European ESPRIT 563 PICA project on photovideotex image compression for ISDN developed candidate algorithms, test images, and the selection infrastructure, and the ISO group was convened by Hiroshi Yasuda of NTT ([ITU Journal, 2020](https://www.itu.int/dms_pub/itu-s/opb/journal/S-JOURNAL-ICTS.V3I1-2020-13-PDF-E.pdf)). JPEG was formed in 1986, with videotex and color facsimile among its intended applications, and published as ITU-T T.81 in 1992 and ISO/IEC 10918-1 in 1993.
- **Facsimile.** CCITT T.4 (Group 3 facsimile, 1980–1981) provided the bi-level coding that alpha-photographic videotex resembled. JBIG split from JPEG in 1988 to standardize bi-level coding for facsimile and similar images, and color facsimile later adopted JPEG.
- **Royalty-free policy.** The same presentation attributes JPEG's original royalty-free baseline to the practice of earlier CCITT picture coding, including facsimile.

Videotex therefore connects terminal graphics with mainstream image coding: the photographic mode that terminals of the 1980s could not afford became the JPEG that is still the most common photographic format.

## Pictures made of characters

A second lineage does not send pixels at all. It draws with characters whose glyphs are blocks or dots, so that any terminal able to display the characters can show the picture.

### Typewriters, teleprinters, and printers

Pictures made of characters are older than terminals. Typewriter art appeared in print as early as an 1875 advertisement in the Brooklyn Daily Eagle. Teleprinter and radio-teletype (RTTY) operators exchanged pictures built from characters in the mid-twentieth century, and 1960s computer centers printed pictures on line printers such as the IBM 1403.

The first known algorithmic conversion of a photograph into characters came from Bell Labs. In 1966 Kenneth Knowlton and Leon Harmon made *Studies in Perception I* (also known as *Computer Nude*): they scanned a photograph with a camera, converted the voltages to numbers, and assigned typographic symbols according to halftone density. The New York Times printed it on 11 October 1967. The idea of choosing a symbol for each region by its density, and later by its shape, is the core of every character renderer that followed.

### Mosaics in teletext and videotex

Teletext, starting with the BBC's Ceefax in 1974, divided a character cell into a 2-by-3 grid and defined mosaic characters for its 64 on/off combinations, with contiguous and separated forms selected by control codes. The alpha-mosaic videotex systems built on the same idea: Prestel, the Antiope standard behind Minitel, and other CEPT-profile terminals mixed text with these block mosaics. Home computers of the same period had comparable semigraphics, such as the TRS-80's 2-by-3 blocks, PETSCII's 2-by-2 blocks, and the PC-8001's 2-by-4 patterns.

### Character art on networks

Networks spread character art among users. On terminals, artists combined characters with cursor-movement escape sequences to make *VT100 animations*: text files that play as moving pictures when sent raw to a VT100 or compatible terminal, originally at serial speeds such as 9600 baud. The collection preserved at [textfiles.com](http://artscene.textfiles.com/vt100/) includes many Christmas cards, such as `xmas.vt` and its variants, one dated 1992. Seasonal pictures also showed the risk of running received text as a program: in December 1987 the Christmas Tree EXEC drew a text Christmas tree on IBM VM/CMS systems and then mailed itself to the user's contacts, paralyzing the EARN, BITNET, and VNET networks as the first widely disruptive computer worm.

Bulletin board systems developed ANSI art in the IBM code page 437 character set, with color escape sequences, and Atari and Commodore users made ATASCII and PETSCII art with their machines' graphic characters. On Usenet, ASCII art circulated in signatures and postings and later gathered in the alt.ascii-art newsgroup, where artists such as Joan Stark were active in the mid-1990s. These traditions were made by hand; the software described next automated the conversion of images.

### Software that renders images as text

Software then automated the conversion on general-purpose terminals, rendering images with whatever characters were available. The technique spread from stand-alone viewers into the major media frameworks, so that video and images can be shown on any text terminal:

| Software | Character output |
| --- | --- |
| [FFmpeg](https://ffmpeg.org/ffmpeg-devices.html#caca) | The `caca` output device renders video through libcaca. |
| [mpv](https://mpv.io/manual/stable/#video-output-drivers) | `--vo=caca` uses libcaca, and `--vo=tct` draws true-color half blocks; `--vo=sixel` and `--vo=kitty` use pixel protocols instead. |
| MPlayer | The `aa` and `caca` video outputs use AAlib and libcaca. |
| VLC | ASCII-art and color ASCII-art video outputs based on AAlib and libcaca. |
| GStreamer | The `aasink` and `cacasink` elements. |
| libcaca tools | `img2txt`, `cacaview`, and `cacaplay` convert and view images and animations. |
| Image viewers | [timg](https://github.com/hzeller/timg) and [viu](https://github.com/atanunq/viu) use half or quarter blocks when no pixel protocol is available, and [TerminalImageViewer](https://github.com/stefanhaustein/TerminalImageViewer) matches 4-by-8 pixel blocks to block characters. |
| Libraries | [Notcurses](https://github.com/dankamongmen/notcurses) offers half-block, quadrant, sextant, octant, Braille, and pixel blitters, and [chafa](https://github.com/hpjansson/chafa) provides a library as well as its command-line tool. |

AAlib also enabled curiosities such as a text-mode port of Quake II.

### From AAlib to chafa

The quality of character rendering improved through better models of what a character cell can show and through dithering methods that are no less sophisticated than those used for raster output.

- **AAlib (1997).** Jan Hubička's library renders every 2-by-2 block of image pixels into one character. It derives an approximation table from the bitmap of the actual font, so that each glyph is chosen by the brightness distribution over its cell rather than by a fixed character ramp, and treats text attributes such as dim, bold, bold font, and reverse as additional brightness levels. It offers Floyd–Steinberg, error-distribution, and random dithering, together with gamma, brightness, and contrast controls.
- **libcaca (2003).** Sam Hocevar and Jean-Yves Lamoureux's library added color. Combining 16 foreground and background colors with glyphs of different densities from ASCII, shade, or block character sets yields a much larger effective palette than the colors alone. It provides Floyd–Steinberg, 2-by-2, 4-by-4, and 8-by-8 ordered, and random dithering with prefilter antialiasing. Its [study](http://caca.zoy.org/study/) of the underlying science works through thresholding, halftoning patterns, ordered dithering and the void-and-cluster method, Floyd–Steinberg and its derivatives, variable-coefficient error diffusion, block and sub-block error diffusion, model-based dithering with a Gaussian model of human vision and direct binary search, gamma, color dithering, and photographic mosaics.
- **Half and quarter blocks.** With true-color terminals, tools such as timg, viu, and mpv's `tct` driver treat the upper half block as two pixels with independent foreground and background colors, which reproduces color exactly at half the vertical cell resolution. TerminalImageViewer refines this by splitting a 4-by-8 pixel block into two colors and choosing the block character whose shape best matches the split.
- **chafa (2018).** Hans Petter Jansson's chafa generalizes the search. Each symbol is an 8-by-8 coverage bitmap, taken from its built-in sets or loaded from a font file through FreeType. For each cell it chooses foreground and background colors, shortlists symbols by comparing 64-bit coverage masks with population counts, and scores the candidates by color error using SIMD code. It can work in the DIN99d color space, applies ordered, diffusion, or noise dithering with a configurable grain smaller than a cell, preprocesses low-color output, trades accuracy for speed through a work factor, and emits SIXEL, kitty, and iTerm2 images as well as symbols.

The common problem is harder than mapping pixels to a palette: each cell must be represented by one glyph shape and at most two colors, so shape, foreground, and background are chosen jointly under that constraint, and the dithering must distribute error between cells as well as within them.

### Glyph brightness and dot gain

A rendered glyph does not emit light in proportion to its geometric coverage. Antialiasing, stroke weight, font hinting, subpixel rendering, and attributes such as bold or dim change how bright a cell actually appears, much as dot gain makes printed halftone dots darker than their nominal area. Character renderers therefore need a model of the effective brightness of each glyph and attribute. AAlib builds its table from the real font bitmap and lets users calibrate the dim and bold multipliers (`dimmul` and `boldmul`) to their monitor, and chafa can load its symbol bitmaps from a font file, such as the one the terminal uses, instead of relying on idealized shapes.

libsixel does not currently have a model of this kind. Its quantization and dithering assume that every output pixel shows exactly its palette color, which holds for a pixel-addressed terminal but ignores effects such as the dot gain of the SIXEL printers for which the format was first designed. The calculation methods of character rendering are therefore a body of technique that libsixel could learn from.

### Unicode adopts the mosaics

These techniques depend on Unicode supplying suitable characters, and Unicode added them in stages:

- **Block elements.** Half blocks and shades from PC code pages were present from early versions. Frank da Cruz of the Kermit Project proposed *Supplemental Terminal Graphics for Unicode* in 2000, which led to the quadrant characters U+2596–U+259F in Unicode 3.2 (2002).
- **Braille Patterns.** Unicode 3.0 (1999) encoded all 256 eight-dot Braille cells at U+2800–U+28FF for Braille text. Their regular 2-by-4 dot grid made them a high-resolution substitute for pixels on terminals, an application the encoding was not designed for.
- **Symbols for Legacy Computing.** Unicode 13.0 (2020) added a block of characters from home computers and teletext, proposed by Doug Ewell, Rebecca Bettencourt, and others, including 60 *sextants*: the 2-by-3 block mosaics of teletext and videotex, also called alpha-mosaic characters.
- **Symbols for Legacy Computing Supplement.** Unicode 16.0 (2024) added 230 *octants* in a 2-by-4 grid, together with other characters from legacy computers and terminals.

With sextants and octants, teletext's 1974 cell grid became part of the universal character set, and terminal programs can now draw block mosaics with ordinary text. This lineage complements pixel protocols such as SIXEL: it works on any terminal with suitable fonts and copies as text, but its resolution and color are limited to what one or two colors per cell can express.

## Images in terminal browsers

Text-mode web browsers needed images long before terminals had a common way to show them. w3m's inline image support, first released in 2001 by Hironori Sakamoto, used an external helper, `w3mimgdisplay`, which drew directly into the X11 window or Linux framebuffer behind the terminal rather than sending pictures through the terminal's data stream. It therefore worked only where the helper could reach the display.

SIXEL removed that restriction. In 2014 haru (the author of the yaft framebuffer terminal) wrote `w3mimg-sixel`, later renamed `yaimg-sixel`, a `w3mimgdisplay`-compatible helper that emitted SIXEL, together with a framebuffer variant. Araki Ken's SIXEL support for w3m itself, dated 2014-11-05, uses libsixel's `img2sixel`. tanasinn took another route in 2012 and accepted w3m image requests through its own OSC 99 sequence.

## Experiments of the 2010s

As software terminals gained modern rendering engines, several projects tried their own ways to show images or rich content:

| Project | Year | Mechanism |
| --- | --- | --- |
| [GateOne](https://github.com/liftoff/GateOne) | 2011 | An HTML5 terminal that recognizes PNG, JPEG, and other file signatures in the output stream and displays the captured file, without an escape sequence. |
| [tanasinn](https://github.com/saitoha/tanasinn) | 2012 | A Firefox-based terminal with SIXEL and Tektronix support and private OSC sequences, including one that opens a URL given in the sequence in an embedded browser and others for overlay images and video. |
| [Terminology](https://github.com/borisfaure/terminology) | 2012 | `tycat` sends `ESC } ic#width;height;path`, passing a file path or URI that the terminal opens itself. |
| [FinalTerm](https://github.com/p-e-w/finalterm) | 2013 | Semantic escape sequences that mark prompts and commands rather than images; its OSC 133 marks survive in shell integration. |
| [iTerm2](https://iterm2.com/documentation-images.html) | 2014 | `OSC 1337 ; File = … : base64 data`, carrying the encoded file itself. |
| [DomTerm](https://domterm.org/Wire-byte-protocol.html) | 2015 | `OSC 72` inserts sanitized HTML fragments, including images. |

Some of these designs let output name a resource that the terminal then loads: a local path, a URI, or a web page. Output from a remote host or an untrusted file can thereby make the terminal read local files or perform network requests on the user's side, a different trust boundary from in-band data. The general hazards of terminals acting on escape sequences were already known, for example in Terminology's [CVE-2015-8971](https://nvd.nist.gov/vuln/detail/CVE-2015-8971) concerning title reports. Self-contained encodings such as SIXEL and iTerm2's base64 file transfer avoid that indirection, although they still require the parser hardening described in the [format overview](README.md#robustness-and-security).

During the same years SIXEL itself was revived: RLogin in 2010, mlterm in 2012, xterm in 2013, and libsixel from 2014, as the [project history](../project-history.md#before-libsixel-sixel-in-software-terminal-emulators) describes.

## Protocols designed for modern terminals

Later proposals addressed what SIXEL and the inline-file protocols lack: image identity, placement, layering with text, and efficient updates.

- **kitty graphics protocol.** Introduced in kitty 0.4.0 on 2017-10-22. It transmits images by escape sequence, file, or shared memory, gives them identifiers, and places them with explicit position and z-order relative to text. WezTerm, Ghostty, and others implement it.
- **Terminal working group.** In March 2019 Per Bothner opened a [discussion](https://gitlab.freedesktop.org/terminal-wg/specifications/issues/12) of a simple image display sequence in the freedesktop.org terminal working group, citing iTerm2 as prior art. No common specification resulted.
- **Jexer.** Autumn Lamonte's Jexer text user interface library defined its own image sequence, listed among prior work by later proposals.
- **STEGAP.** In June 2021 Nick Black, the author of the Notcurses library, wrote the [Spriteful TErminal GrAphics Protocol](https://nick-black.com/dankwiki/index.php/Spriteful_TErminal_GrAphics_Protocol) proposal for bitmapped sprites combined with glyphs and styling. He describes it as a learning exercise whose result resembled the kitty protocol, and recommends that terminal developers implement kitty's protocol.
- **Contour.** Christian Parpart's Contour terminal added an experimental *Good Image Protocol* alongside kitty, iTerm2, SIXEL, and ReGIS support.
- **Implementations of several protocols.** Libraries and terminals increasingly support more than one protocol: the xterm.js image add-on, begun in 2021, handles SIXEL and iTerm2 images; WezTerm supports SIXEL, iTerm2, and kitty images; tmux 3.4 (2024-02-13) passes SIXEL; and Windows Terminal added DECDLD soft fonts in 2021 and SIXEL in Preview 1.22 in 2024.

No single protocol has displaced the others. SIXEL remains the most widely implemented, owing to its DEC heritage and its self-contained encoding; the newer protocols specify display and control semantics that SIXEL leaves to interpretation. The [libsixel position](specification-layers.md#libsixels-position) on that difference is that unambiguous modern semantics belong in a protocol designed for modern terminals, not in a redefinition of SIXEL.

## References

- Tektronix 4010 series, VT320 series, ReGIS, Videotex, NAPLPS, Captain, Prestel, Minitel, Telidon, Bildschirmtext, RIPscrip, and Tera Term articles on Wikipedia, with their cited sources.
- Digital Equipment Corporation, [*VT330/VT340 Programmer Reference Manual, Volume 2*](https://vt100.net/docs/vt3xx-gp/), Chapter 14 (sixel graphics).
- István Sebestyén, [*JPEG 25 years celebration presentation*](https://www.ecma-international.org/wp-content/uploads/IS-presentation-to-JTC1SC29WG1-regarding-JPEG25-celebration.pdf), Ecma International.
- [*ISO/IEC 10918-1 (1986–1993)*](https://www.itu.int/dms_pub/itu-s/opb/journal/S-JOURNAL-ICTS.V3I1-2020-13-PDF-E.pdf), ITU Journal: ICT Discoveries, 2020.
- [IANA registration of `image/naplps`](https://www.iana.org/assignments/media-types/image/naplps), 1995; Universal Computer Laboratory, [NAPLPS technical information and viewer plug-in](http://uclsun.ucl.co.jp/naplps/naptech-e.html).
- ASCII art, ANSI art, Christmas Tree EXEC, Ken Knowlton, and Leon Harmon articles on Wikipedia; [textfiles.com VT100 animation collection](http://artscene.textfiles.com/vt100/).
- Unicode Consortium proposals [L2/19-025](https://www.unicode.org/L2/L2019/19025-terminals-prop.pdf) (Symbols for Legacy Computing), [L2/21-235](https://www.unicode.org/L2/L2021/21235-terminals-supplement.pdf) (its supplement), and Frank da Cruz, [L2/00-159](https://www.unicode.org/L2/L2000/00159-ucsterminal.txt) (*Supplemental Terminal Graphics for Unicode*); Semigraphics, Teletext character set, Braille Patterns, AAlib, and libcaca articles on Wikipedia.
- [chafa](https://github.com/hpjansson/chafa) and its [manual](https://hpjansson.org/chafa/man/); [drawille](https://github.com/asciimoo/drawille); [AAlib documentation](https://aa-project.sourceforge.net/aalib/) and [tuning guide](https://aa-project.sourceforge.net/tune/); Sam Hocevar, [*Libcaca study: the science behind colour ASCII art*](http://caca.zoy.org/study/).
- [FFmpeg devices](https://ffmpeg.org/ffmpeg-devices.html#caca) and [mpv video output drivers](https://mpv.io/manual/stable/#video-output-drivers).
- [w3m inline image documentation](https://github.com/tats/w3m/blob/master/doc/README.img) and [SIXEL support](https://github.com/tats/w3m/blob/master/doc/README.sixel).
- haru (uobikiemukot), [sdump](https://github.com/uobikiemukot/sdump), containing `yaimg-sixel`, first added as `w3mimg-sixel` on 2014-09-23.
- [RLogin history](https://github.com/kmiya-culti/RLogin/blob/master/docs/history.html); [mintty changelog](https://github.com/mintty/mintty/blob/master/wiki/Changelog.md); [kitty changelog](https://sw.kovidgoyal.net/kitty/changelog/); [tmux CHANGES](https://github.com/tmux/tmux/blob/master/CHANGES).
- Microsoft, [terminal issue #9164](https://github.com/microsoft/terminal/issues/9164) on DECDLD soft fonts and [Windows Terminal Preview 1.22 release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-22-release/).
