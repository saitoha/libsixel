# How SIXEL Implementations Evolved

This explanation is for anyone who writes a SIXEL encoder or decoder, or wants to know where the techniques in today's codecs came from. It traces the methods of building and parsing SIXEL streams from hand-written files to multithreaded, layered and partially updated output, and names the earliest public example of each technique that could be verified. It does not repeat the wire format, which the [SIXEL format overview](README.md) owns, or the history of the libsixel project itself, which the [project history](../project-history.md) owns. The [SIXEL codec tutorial](../tutorial/sixel-codec/README.md) teaches the techniques by writing them.

**Key ideas in one minute:**

- No single project invented the fast SIXEL codec. Each technique below first appeared in a separate, independent implementation, usually one written for a single application or device.
- The same idea often appeared more than once, decades apart: packing a six-pixel column into one value was used by a monochrome printer driver in 1991 and again by color encoders from 2019.
- New interpretations of the format, such as layering images with a transparent background or redefining palette registers between bands, came from applications that needed more than 256 colors, not from a specification.
- libsixel is one reference implementation among these. It is not an attempt to unify how SIXEL is implemented or to concentrate development in one place.

## How to read the dates

Each date is the earliest public record found for that implementation or technique: a release, a commit, a posting or an issue. An earlier private or unpublished use may exist. Where a record does not say how something was done, this page says that it is not known rather than inferring it.

## Lost tools and hand encoding

Before open-source encoders, SIXEL files were written by hand or by private programs whose code has not survived. Their methods can only be inferred from newsgroup postings and the files themselves.

| Date | Record | What it shows |
| --- | --- | --- |
| 1980s | Kermit graphics demonstration files, preserved in [hackerb9/vt340test](https://github.com/hackerb9/vt340test) | SIXEL pictures distributed as files and replayed to a terminal. |
| 1990-09 | Chris F. Chiesa, [All About SIXELs](https://vt100.net/shuford/terminal/all_about_sixels.txt) | A tutorial condensed from three years of notes, with a VAX FORTRAN dumper that writes one sixel per character without repeat counts, and an Amiga BASIC viewer. The author mentions other generators that are not described. |
| 1994–1995 | `GIFSIXEL.ZIP`, recommended on comp.terminals ([Shuford's archive](https://shuford.invisible-island.net/sixel_graphics_news.txt)) | A GIF-to-SIXEL converter for VMS distributed by anonymous FTP; its method is not known. |
| 1996-11 | `PCX2SIX` in the [Kermit `sixel` directory](https://sunsite3.icm.edu.pl/pub/kermit/sixel/) (mirror) | A DOS converter from PCX images distributed with sample pictures; its method is not known. |

The [specification layers](specification-layers.md) page lists further postings that show how such files were exchanged.

## Early open-source encoders

The first published encoders served printers and plotting libraries. They wrote one band of six rows at a time and compressed horizontal repeats with `!`, but they differed in how they turned rows into sixel characters.

| Date | Implementation | Technique |
| --- | --- | --- |
| 1988-12 | PGPLOT [`LADRIV`](https://sites.astro.caltech.edu/~tjp/pgplot/ladriv.html), B. H. Toby | Monochrome LA50 output written line by line; runs longer than four characters use `!`. |
| 1991-02 | Ghostscript [`gdevln03.c`](https://sources.debian.org/src/gs/3.33-7/gdevln03.c), Ulrich Mueller (CERN) | Monochrome LN03, later LA50, LA70 and LA75 output. Each column's six bits are built by transposing a six-by-eight block of the bitmap; runs longer than three use `!`. |
| 1991-05 | [`ppmtosix`](https://www.tuhs.org/Usenet/alt.sources/1991-May/002739.html), Richard Vinci; later Netpbm's [`ppmtosixel`](https://netpbm.sourceforge.net/doc/ppmtosixel.html) | Color output that visits one source row at a time and sets one bit per character, so a band takes six passes. The [format overview](README.md#a-row-at-a-time-baseline-netpbm-ppmtosixel) shows its output. |
| 2013-11 | gnuplot's native sixel terminal in `tek.trm`, Erik Olofsen ([patch #647](https://sourceforge.net/p/gnuplot/patches/647)) | Replaced piping plots through `ppmtosixel`. One pass per color over each band, with `$` between colors and `!` for runs longer than three. |

## kmiya's `sixel` and the node method

kmiya's `sixel` encoder, from the author of RLogin, which added SIXEL in 2010, organized output for terminals instead of print heads. For each band it builds one map per color, cuts each map into spans called nodes, and packs nodes that do not overlap onto the same graphics line, so a band needs few carriage returns. The [format overview](README.md#nodes-and-greedy-non-overlapping-sweeps) explains the method. Its first release date is not recorded; the earliest preserved version is dated 2014-03-02, and the [project history](../project-history.md#kmiyas-sixel) describes its later true-color line. gnuplot's `sixelgd` terminal ([patch #742](https://sourceforge.net/p/gnuplot/patches/742), 2016-11) is a modified version of kmiya's code.

## libsixel: the node method as a library

libsixel imported kmiya's encoder in March 2014 and turned it into a library and command-line tools. It applied the encoder to animation, multiplexer passthrough and many terminal environments, and added color quantization in front of it. The [project history](../project-history.md#formation-of-libsixel) owns the details. In 2014-11 Araki Ken added the size-oriented [encoding policy](../encoder/output/encode-policy/README.md) (`-E size`), which fills the first graphics line of a band and lets later lines repaint the other colors, so the same pixels need fewer bytes.

## Decoder-side transposition in `ite`

Tetsuya Isaki's SIXEL support for the NetBSD/x68k `ite` console collected one band before writing video memory from its first version in 2014-10. In 2016-11 it replaced six row arrays with one 32-bit word per column holding the band's six pixels, updated with one table lookup, one multiplication and one OR. That is the decoder-side counterpart of packing a column into one value. The [OR-mode history](../encoder/output/or-mode/x68k.md#history) gives the patch history and the measured effect.

## chafa: column words and parallel batches

Hans Petter Jansson's [chafa](https://github.com/hpjansson/chafa) added a SIXEL encoder in 2019-10. Its first version already stored the six pixels of a column in the low six bytes of one 64-bit word, and kept a bitmap per color that lets a pass skip a group of columns in which that color does not appear. It divides the image into batches of rows, builds them in a thread pool, and appends their output in order. Whether it was the first color encoder to pack columns this way is not known.

## Column-oriented encoders: notcurses and jjrv's gist

| Date | Implementation | Technique |
| --- | --- | --- |
| 2021-02 | [notcurses](https://github.com/dankamongmen/notcurses), nick black | notcurses gains its own SIXEL encoder. |
| 2022-01 | notcurses | Each band keeps one encoded vector per color, so cells covered by text can be wiped and restored without re-encoding the picture. The current encoder scans each band column by column, recording which of the six rows each color occupies; when that order was adopted is not known. |
| 2022-02 | notcurses | Bands become the work unit of a small pool of resident worker threads. |
| 2025-03 | [jjrv's `sixel.ts`](https://gist.github.com/jjrv/69fbf5f1c8b49be070063bd2a1c1288f), Juha Järvi | Transposes six rows of indices into column words and finds pending colors in a seven-column lookahead with SWAR zero-byte tests, in TypeScript. |

## Encoders in other languages

| Date | Implementation | Technique |
| --- | --- | --- |
| 2024-12 | [icy_sixel](https://github.com/mkrueger/icy_sixel) 0.4.0, Mike Krüger | Rust. Earlier versions were based on libsixel; 0.4.0 rewrote the encoder on the quantette quantizer and added a decoder written from the specification, with SSE2 span filling. The dates of earlier versions are not recorded. |
| 2025-05 | [a-sixel](https://github.com/Jesterhearts/a-sixel), Jesterhearts | Rust. Encodes bands in parallel with rayon, one pass per color present in the band, and offers many palette-construction methods. |

## More than 256 colors: layers and per-band palettes

Two independent techniques show more than 256 colors in one picture. Both came from applications.

| Date | Record | Technique |
| --- | --- | --- |
| 2026-03 | [microsoft/terminal#20020](https://github.com/microsoft/terminal/issues/20020), Tyler Young | Several SIXEL images, each with its own palette and a transparent background (`P2=1`), drawn over one another for each frame of a private video player; 8192 colors in 32 palettes are reported. The player exercised a CUDA, Vulkan and OpenCL compute library; which stages ran on a GPU is not known, and whether it updated only changed regions is not known. |
| 2026-08 | [microsoft/terminal#20588](https://github.com/microsoft/terminal/issues/20588), unxed | Redefining palette registers before each band within one image. The issue reports that WezTerm and foot show each band with the colors defined before it, while Windows Terminal resolves colors when the image is complete; the application uses layering there instead. |

The [multi-pass decoding](../decoder/multi-pass.md) reference defines how libsixel's decoder composes layered captures.

## libsixel: parallel bands, delta frames and GPUs

| Date | Change |
| --- | --- |
| 2025-11 | Bands are dispatched to encode workers in parallel and published in order. The decoder splits a stream into byte ranges that are decoded in parallel, with a serial fallback. |
| 2026-06 | [Delta encoding](../encoder/output/delta-encoding.md) sends only the pixels that changed from the previous frame on a retained canvas. |
| 2026-07 | Palette application, and later band composition, run on Metal and Vulkan GPUs; [GPU host memory and frame sharing](../gpu/host-memory.md) explains how frames move between the processors. |

Whether earlier implementations sent only changed regions of video frames, or ran SIXEL encoding stages on a GPU, is not known from the public record.

## Techniques at a glance

| Technique | Earliest public example found |
| --- | --- |
| Repeat compression with `!` | PGPLOT `LADRIV`, 1988 |
| One pass per color over each band | gnuplot `tek.trm`, 2013 |
| Six pixels of a column in one value (encoder) | Ghostscript `gdevln03.c`, 1991, monochrome; chafa, 2019, color |
| One map per color per band, nodes packed onto lines | kmiya's `sixel`, by 2014 |
| Six pixels of a column in one value (decoder) | `ite`, 2016 |
| Bands built in parallel and joined in order | chafa, 2019 |
| Overpainting the first line of a band to save bytes | libsixel `-E size`, Araki Ken, 2014 |
| Bands decoded in parallel | libsixel, 2025 |
| Resident band workers | notcurses, 2022 |
| SWAR search over column words | jjrv's `sixel.ts`, 2025 |
| SIMD span filling in a decoder | icy_sixel, 2024 |
| Layered images with `P2=1` for more colors | Tyler Young, 2026 |
| Palette redefinition per band | unxed, 2026 |
| Delta frames on a retained canvas | libsixel, 2026 |

No earliest example was found for several techniques that the [codec tutorial](../tutorial/sixel-codec/README.md) teaches: filling repeats a row at a time in a decoder, an inner loop over data characters, branch-free masked stores, a bitmap of node start columns, finding the distinct colors of a column with shifted comparisons, and SIMD classification or transposition of column words.

## In libsixel

libsixel's encoder descends from kmiya's node method. Its OR-mode decoder adopted column words in 2026-10, after the `ite` principle, and the project added parallel bands, layered decoding, delta frames and GPU stages as described in the sections above. The [format overview](README.md#libsixel-implementation-map) maps these to source files. New techniques are expected to keep appearing in application-local implementations first; the [tutorial](../tutorial/sixel-codec/README.md) exists so that writing such an implementation does not require libsixel.

## References

- Chris F. Chiesa, [All About SIXELs](https://vt100.net/shuford/terminal/all_about_sixels.txt), 1990.
- Richard Shuford, [SIXEL graphics news archive](https://shuford.invisible-island.net/sixel_graphics_news.txt).
- Tetsuya Isaki, [x68k `ite` SIXEL patch history](https://github.com/isaki68k/misc/commits/master/NetBSD/patch/x68k-ite-sixel.diff).
- Juha Järvi, [Fast DEC sixel encoding with bit twiddling](https://gist.github.com/jjrv/69fbf5f1c8b49be070063bd2a1c1288f), 2025.
