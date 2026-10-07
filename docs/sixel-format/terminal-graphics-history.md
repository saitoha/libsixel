# Terminal Graphics History

## Scope

SIXEL is one of many ways that have been used to show pictures through a character terminal or a terminal-like connection. Its strengths and gaps, which the [specification status and interpretation layers](specification-layers.md) describe, are easier to judge against that wider history. This document surveys it from vector storage-tube terminals through videotex and DEC's raster terminals to the image protocols of software terminal emulators.

It is a survey, not an exhaustive catalog. Dates are those of the cited releases, specifications, or first public commits; a product announced earlier or released in stages can have other dates in other sources. The libsixel [project history](../project-history.md) covers the SIXEL implementations closest to libsixel in more detail.

## Timeline

| Year | Event | Kind |
| --- | --- | --- |
| 1972 | Tektronix 4010 storage-tube terminal | Vector terminal |
| 1974 | Tektronix 4014, whose command set became the common target of later Tektronix emulations | Vector terminal |
| 1978 | Telidon trials in Canada | Videotex, alpha-geometric |
| 1979 | Prestel public service in the United Kingdom | Videotex, alpha-mosaic |
| 1980–1981 | CCITT T.4, the Group 3 facsimile coding | Facsimile |
| 1981 | DEC ReGIS on the VT125, then the GIGI | Vector graphics language |
| 1982 | Minitel terminals in France, using the Antiope-based display standard | Videotex, alpha-mosaic |
| 1983 | NAPLPS standardized as ANSI X3.110 and CSA T500; Bildschirmtext service in Germany | Videotex |
| 1984 | NTT's CAPTAIN service in Japan; DEC VT240 programmer reference manual, documenting SIXEL on a terminal screen | Videotex, alpha-photographic; raster terminal |
| 1985–1986 | ESPRIT PICA photovideotex compression project; formation of JPEG | Image coding |
| 1987 | DEC VT330 and VT340 with ReGIS, SIXEL, and Tektronix 4010/4014 emulation | Raster terminal |
| 1988 | JBIG split from JPEG for bi-level images | Image coding |
| 1992 | RIPscrip vector graphics for bulletin board systems; ITU-T T.81 (JPEG) | BBS graphics; image coding |
| 1994 | Tera Term with Tektronix 4010 emulation | Software emulator |
| 1995–1996 | `image/naplps` registered with IANA; UCL's NAPLPS viewer plug-in for Netscape Navigator 2.0 | Videotex on the web |
| 2001 | w3m inline image support through the external `w3mimgdisplay` helper | Terminal browser |
| 2009 | RLogin adds Tektronix 4014/4105 modes | Software emulator |
| 2010 | RLogin 2.9.0 adds SIXEL and ReGIS | SIXEL revival |
| 2011 | GateOne detects image files in the output stream | In-band, no protocol |
| 2012 | tanasinn with Tektronix, SIXEL, and URL-based OSC extensions; Terminology `tycat` passes file paths or URIs; mlterm adds SIXEL | Software emulators |
| 2013 | FinalTerm semantic escape sequences (OSC 133); xterm patch #294 adds SIXEL | Shell integration; SIXEL revival |
| 2014 | iTerm2 inline images (OSC 1337); libsixel; haru's `w3mimg-sixel`; w3m SIXEL support | Image protocol; SIXEL tooling |
| 2015 | DomTerm, later with HTML insertion through OSC 72 | HTML in terminal |
| 2017 | kitty 0.4.0 introduces its graphics protocol | Image protocol |
| 2019 | Terminal working group discussion of a simple image display sequence | Standardization attempt |
| 2020 | mintty 3.3.0 adds Tektronix 4014 emulation | Software emulator |
| 2021 | Nick Black's STEGAP proposal; Windows Terminal adds DECDLD soft fonts; xterm.js image add-on | Proposals and implementations |
| 2024 | tmux 3.4 adds SIXEL; Windows Terminal Preview 1.22 adds SIXEL | SIXEL adoption |

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
- [w3m inline image documentation](https://github.com/tats/w3m/blob/master/doc/README.img) and [SIXEL support](https://github.com/tats/w3m/blob/master/doc/README.sixel).
- haru (uobikiemukot), [sdump](https://github.com/uobikiemukot/sdump), containing `yaimg-sixel`, first added as `w3mimg-sixel` on 2014-09-23.
- [RLogin history](https://github.com/kmiya-culti/RLogin/blob/master/docs/history.html); [mintty changelog](https://github.com/mintty/mintty/blob/master/wiki/Changelog.md); [kitty changelog](https://sw.kovidgoyal.net/kitty/changelog/); [tmux CHANGES](https://github.com/tmux/tmux/blob/master/CHANGES).
- Microsoft, [terminal issue #9164](https://github.com/microsoft/terminal/issues/9164) on DECDLD soft fonts and [Windows Terminal Preview 1.22 release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-22-release/).
