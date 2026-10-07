# SIXEL Patents and Royalties

## Scope and status of this document

This document summarizes public evidence about patents, royalties, and related rights for the SIXEL format. It is not legal advice. The libsixel project has not commissioned a freedom-to-operate analysis, and absence of a known claim is not proof that none exists. Readers who need a legal opinion for a particular product or jurisdiction should obtain one from a qualified professional.

The subject is the SIXEL format as documented by DEC and described in the [SIXEL format overview](README.md). libsixel's own license, the image-processing algorithms it implements, and the formats handled by loaders are separate matters. [Patent status of loader formats](../loader/patents.md) covers the loaders, and [patent and license status of processing algorithms](../misc/algorithm-patents.md) covers color, palette, lookup, dithering, and dequantization methods.

## Summary

No license or royalty is known to be required to implement SIXEL. The evidence for that conclusion is:

- the format was publicly documented by DEC no later than 1987–1988, so any patent filed on what those documents describe has expired, and the documents themselves are prior art against later claims to the same subject;
- the comparable GIF/LZW patents, filed in the same period, expired worldwide by July 2004;
- the one DEC patent found that refers to SIXEL is a printer-controller patent that has expired;
- SIXEL has been implemented openly for four decades, by DEC itself, by commercial and noncommercial terminal emulators, and recently by Microsoft, with no known licensing program, royalty demand, or patent assertion.

In practice, implementing the format is therefore about as safe from intellectual-property claims as a technology can be shown to be. Patents on later, specific techniques that happen to involve SIXEL, such as a particular rendering or compression method, remain possible in principle and are discussed below.

## Public disclosure

SIXEL was in use on DEC printers by the mid-1980s; a 1995 Usenet recollection places LA50 support around 1986 and doubts that it was the first SIXEL device ([Shuford's sixel news archive](https://shuford.invisible-island.net/sixel_graphics_news.txt)). The format was fully described in published product documentation, including:

- the [*LJ250/LJ252 Companion Color Printer Programmer Reference Manual*](https://vt100.net/manx/part/dec/ek-lj250-rm/), first edition August 1987, whose Chapter 6 covers Sixel Graphic Mode;
- the [*VT330/VT340 Programmer Reference Manual, Volume 2: Graphics Programming*](https://bitsavers.trailing-edge.com/pdf/dec/terminal/vt340/EK-VT3XX-GP-002_VT330_VT340_Graphics_Programming_198805.pdf), second edition May 1988, whose Chapter 14 covers sixel graphics.

DEC documented the format so that hosts and third-party software could drive its devices. No DEC licensing terms or royalty program for generating or interpreting SIXEL data is known.

## Patent terms

A patent expires at the end of a fixed term, after which anyone may practice what it claims.

- **United States.** For applications filed before 8 June 1995, the term is the longer of 17 years from grant or 20 years from filing (35 U.S.C. 154(c)). For later applications, it is 20 years from the earliest effective U.S. filing date, which for a continuation is the filing date of its earliest parent.
- **Europe and Japan.** The term is 20 years from filing.

A patent application filed in the 1980s would therefore have expired by around 2010 at the latest under ordinary prosecution. An application filed before June 1995 that remained pending for decades could in principle still issue with a 17-year term. Such cases are exceptional, and none relating to SIXEL is known.

Publication also limits what can be patented later. Once the format was described in manuals distributed from 1987 and 1988, those manuals became prior art. A patent filed afterwards cannot validly claim the format as those manuals describe it, even though a narrower claim to a new technique could still be valid.

### Comparison with GIF

GIF (1987) belongs to the same period. Its LZW compression was covered by Unisys's U.S. Patent 4,558,302, filed on 20 June 1983. That patent expired in the United States on 20 June 2003 ([Out-Law](https://www.pinsentmasons.com/out-law/news/gif-patent-expires)). Its counterparts expired in the United Kingdom, France, Germany, and Italy on 18 June 2004, in Japan on 20 June 2004, and in Canada on 7 July 2004, as Unisys's licensing page stated at the time ([GIF](https://en.wikipedia.org/wiki/GIF#Unisys_and_LZW_patent_enforcement)). GIF demonstrates that even an actively enforced patent from the early 1980s has long expired. SIXEL differs in that no comparable patent covering the format is known at all.

## Where DEC's rights went

DEC's corporate assets passed to Compaq, which completed its acquisition of DEC in June 1998, and then to Hewlett-Packard, which completed its merger with Compaq on 3 May 2002. Not every DEC business followed that path. In August 1995 DEC sold its character-cell terminal business to SunRiver Data Systems, renamed Boundless Technologies in 1996, which continued the VT520 until the company ceased operating in 2003 ([VT520](https://en.wikipedia.org/wiki/VT520)). The libsixel project has not traced which individual rights, if any, accompanied that sale.

One patent illustrates the main path. [U.S. Patent 5,819,014](https://patents.google.com/patent/US5819014A/en), *Parallel distributed printer controller architecture*, was filed by DEC as a continuation of a 1990 application and granted on 6 October 1998. Its description treats "an ASCII or Sixel file" as an input to be translated to PostScript. Its recorded assignments run from DEC and Compaq to Hewlett-Packard Development Company, and Google Patents lists it as expired. It claims a printer architecture rather than the SIXEL encoding; it is cited here because it is the only DEC-assigned patent that a full-text search for "sixel" on Google Patents returned in October 2026, and because it shows that even a DEC patent of the 1990s that mentions SIXEL has expired. Full-text search does not reliably cover older scanned patents, so this search is evidence, not an exhaustive clearance.

## Implementation history as practical evidence

SIXEL has been implemented openly and continuously:

| Period | Implementation |
| --- | --- |
| 1980s– | DEC printers and VT240, VT330, and VT340 terminals |
| late 1980s– | DECterm (`dxterm`), DEC's own DECwindows terminal emulator |
| 1980s–1997 | MS-DOS Kermit from Columbia University, distributed with SIXEL sample files |
| 1990s– | WRQ Reflection, a commercial emulator of VT240 through VT340 graphics |
| 1991– | `ppmtosixel` in PBMPLUS and then [Netpbm](https://netpbm.sourceforge.net/doc/ppmtosixel.html), included in major operating-system distributions |
| 2013– | xterm, from [patch #294](https://invisible-island.net/xterm/xterm.log.html#xterm_294), shipped by major Unix-like distributions |
| 2024– | Microsoft Windows Terminal, from [Preview 1.22](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-22-release/) |

The [project history](../project-history.md#before-libsixel-sixel-in-software-terminal-emulators) gives sources for the earlier emulators. No licensing program, royalty demand, or patent assertion against any of these implementations is known to the libsixel project. A rights holder would have had decades of widely distributed, commercially sold targets, including a terminal emulator from one of the largest software vendors. This is not legal proof, but it is strong practical evidence that no enforceable claim to the format exists.

## Copyright and other rights

Copyright protects the expression in DEC's manuals, not the format they describe. United States law excludes any idea, procedure, process, system, or method of operation from copyright protection (17 U.S.C. 102(b)), and Japan's Copyright Act, Article 10(3), excludes programming languages, rules, and algorithms from the protection of programs. Implementing an encoder or decoder from a description of the format does not reproduce the manuals. libsixel's documentation describes the format in its own words and cites the manuals instead of copying them.

Trademarks are outside the scope of this document.

## libsixel's position

libsixel is distributed under the MIT license in [`LICENSE`](../../LICENSE) and charges no royalty for producing or consuming SIXEL. The project knows of no patent, license requirement, or royalty that applies to the SIXEL format itself, and will record verifiable evidence to the contrary in this document if it appears.

## References

- Digital Equipment Corporation, [*VT330/VT340 Programmer Reference Manual, Volume 2: Graphics Programming*, second edition, May 1988](https://bitsavers.trailing-edge.com/pdf/dec/terminal/vt340/EK-VT3XX-GP-002_VT330_VT340_Graphics_Programming_198805.pdf).
- Digital Equipment Corporation, [*LJ250/LJ252 Companion Color Printer Programmer Reference Manual*](https://vt100.net/manx/part/dec/ek-lj250-rm/), EK-LJ250-RM-001, August 1987.
- [U.S. Patent 5,819,014](https://patents.google.com/patent/US5819014A/en), *Parallel distributed printer controller architecture*, with recorded assignments and legal status.
- [35 U.S.C. 154](https://www.law.cornell.edu/uscode/text/35/154), contents and term of patent.
- [17 U.S.C. 102](https://www.law.cornell.edu/uscode/text/17/102), subject matter of copyright.
- Out-Law, [*GIF patent expires*](https://www.pinsentmasons.com/out-law/news/gif-patent-expires), 2003.
- Richard S. Shuford, [*Sixel Graphics News*](https://shuford.invisible-island.net/sixel_graphics_news.txt).
- Microsoft, [*Windows Terminal Preview 1.22 Release*](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-22-release/), 2024-08-27.
