# Patent Status of Loader Formats

## Scope and status of this document

This document records what the libsixel project has been able to establish about patents affecting the image formats that its loaders decode. It is not legal advice, and the project has not commissioned a freedom-to-operate analysis. Patent status depends on jurisdiction, on the exact claims of each patent, and on facts that public sources may not reveal. Absence of a known patent or claim is not proof that none exists. Readers who need a legal opinion should obtain one from a qualified professional.

The SIXEL format itself is covered by [SIXEL patents and royalties](../sixel-format/patents-and-royalties.md), which also explains how patent terms are calculated. Encoder and decoder algorithms such as quantization, dithering, and dequantization are covered by [patent and license status of processing algorithms](../misc/algorithm-patents.md).

## Summary

| Loader path | Code that decodes | Patent status as far as known |
| --- | --- | --- |
| Builtin GIF | libsixel | The LZW patents expired in 2003–2004. |
| Builtin JPEG | libsixel (stb-derived) | The patents asserted against JPEG and those listed for its arithmetic-coding option have expired. |
| Builtin PNG/APNG, PSD ZIP | libsixel (stb-derived zlib inflate) | Deflate was designed to be implementable without patents; no claim is known. |
| Builtin BMP, TGA, PIC, PNM/PAM, HDR, PSD RLE | libsixel | Simple run-length, Huffman, or uncompressed codings from the 1980s or earlier; no claim is known. |
| WebP in the builtin loader | None: the builtin loader declines WebP | No in-tree WebP decoder exists, because an independent decoder would not be covered by Google's patent grant. See [WebP](#webp). |
| `libpng`, `libjpeg`, `libtiff`, `libwebp`, `librsvg` | The external library | As for the format; libwebp includes Google's patent grant. Optional codecs compiled into the library carry their own status. |
| `wic`, `coregraphics`, `quicklook`, `gdk-pixbuf2`, `gd`, `gnome-thumbnailer` | The host platform, installed modules, or external programs | Determined by the component that decodes; libsixel passes encoded bytes to it and receives pixels. |

For every builtin decoder, the relevant patents that the project could identify have expired or no claim is known. Lossy WebP is the one format for which a residual risk could be identified, and the builtin loader therefore does not decode WebP.

## Builtin decoders

### GIF

The [builtin GIF decoder](builtin/gif.md) implements GIF LZW. Unisys's U.S. Patent 4,558,302 covered LZW and was enforced against GIF software in the 1990s. It expired in the United States on 20 June 2003, and its counterparts in the United Kingdom, France, Germany, Italy, Japan, and Canada expired between 18 June and 7 July 2004 ([Out-Law](https://www.pinsentmasons.com/out-law/news/gif-patent-expires), [GIF](https://en.wikipedia.org/wiki/GIF#Unisys_and_LZW_patent_enforcement)). libjpeg-turbo likewise restored LZW GIF support on the ground that the patent "has long expired" ([libjpeg-turbo change log](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/ChangeLog.md)).

### JPEG

The [builtin JPEG decoder](builtin/jpeg.md) accepts baseline, extended, progressive, lossless, hierarchical, and arithmetic-coded processes of ITU-T T.81 (1992).

- **Arithmetic coding.** The arithmetic-coding option was historically avoided because patents held by IBM, AT&T, and Mitsubishi covered it, and free JPEG software omitted it for that reason. Those patents date from the 1980s and were listed in T.81, so all were filed before 1992 and their terms have ended.
- **Forgent.** Forgent Networks asserted U.S. Patent 4,698,672 against JPEG users. Its term ended on 27 October 2006, and in November 2006 Forgent abandoned enforcement against use of the JPEG standard.
- **Princeton Digital Image.** U.S. Patent 4,813,056, originally assigned to General Electric and asserted against JPEG, expired in December 2007.

These events are summarized with sources in the [JPEG article](https://en.wikipedia.org/wiki/JPEG#Patent_controversy). United States law bars recovery for infringement committed more than six years before a suit is filed (35 U.S.C. 286), so infringement before these expiries can no longer be pursued.

### PNG, APNG, and ZIP-compressed PSD

PNG was designed in 1995–1996 as a patent-free replacement for GIF, and its deflate compression was specified so that it could be implemented without patented techniques ([RFC 1951](https://www.rfc-editor.org/rfc/rfc1951), [RFC 2083](https://www.rfc-editor.org/rfc/rfc2083)). APNG adds animation chunks and introduces no new compression. The builtin PSD decoder uses the same inflate code for ZIP-compressed channels. No patent claim against these formats is known.

### BMP, TGA, Softimage PIC, Netpbm, Radiance HDR, and PSD run-length data

These decoders handle uncompressed samples and simple run-length codings: BMP RLE4, RLE8, and OS/2 RLE24; TGA and PIC run-length packets; Radiance RGBE scanline runs; and PackBits-style PSD rows. The OS/2 one-dimensional Huffman mode uses the modified-Huffman coding of ITU-T T.4, standardized in 1980. All of these codings date from the 1980s or earlier, and no patent claim against them is known.

### WebP

The builtin loader does not decode WebP. It recognizes RIFF containers only to decline them with a recoverable status, so that the `libwebp` loader or a platform loader decodes WebP instead. During 2026 the builtin loader briefly contained an independent decoder for the VP8 key-frame codec (lossy WebP) and the VP8L lossless codec, not derived from libwebp; it was removed because of the position described below, which applies to any independent WebP decoder.

- **Google's grant does not reach an independent decoder.** The "Additional IP Rights Grant" distributed with libwebp and libvpx, and reproduced in the VP8 specification [RFC 6386](https://www.rfc-editor.org/rfc/rfc6386) section 20.27, licenses Google's patent claims only as they are infringed by Google's own implementation, and excludes claims infringed only after modifying it. An independent decoder does not receive this license.
- **VP8 cross-license.** In March 2013 Google obtained a license from MPEG LA and eleven patent holders to patents that may be essential to VP8, with the right to sublicense royalty-free to any VP8 implementation, whether or not based on Google's code; MPEG LA then stopped forming a VP8 pool ([LWN](https://lwn.net/Articles/551297/)). Google offers the resulting [VP8 Patent Cross-License Agreement](https://www.webmproject.org/cross-license/) to implementers who sign it. The libsixel project has not signed it, and the agreement does not apply automatically.
- **Nokia.** Nokia declared to the IETF that it held patents essential to VP8 and declined to offer either royalty-free or FRAND licenses. In Nokia's litigation against HTC, the Mannheim Regional Court ruled on 5 August 2013 that VP8 did not infringe EP 1 186 177 B1 ([OSnews](https://www.osnews.com/story/27245/vp8-does-not-infringe-on-nokia-patent/)). No later assertion against VP8 decoders is known to the project, but the other declared patents were not adjudicated in that case.
- **Lossless VP8L.** No third-party patent claim against VP8L is known. Any Google patents reading on it are licensed through the libwebp grant only for Google's implementation.
- **Age.** No single expiry date exists. A valid patent reading on VP8 must have been filed before the technique became public, which for the complete decoder specification was Google's publication of the bitstream in May 2010; for VP8L it was the announcement of WebP lossless in November 2011. Under the 20-year term such patents end by about 2030 and 2031 respectively, but United States patents can be extended by patent term adjustment for examination delays, typically by months and occasionally by several years, so a conservative horizon is the mid-2030s. Many of the techniques involved were already used in codecs of the early 2000s, and patents on them have already expired or will expire sooner; the filing dates of the specific patents Nokia declared have not been reviewed by the project.

Independent VP8 decoders are widely distributed, notably FFmpeg's native VP8 decoder, and no enforcement against VP8 or WebP decoders is known to the project. The residual risk is nevertheless not zero in the way it is for GIF or JPEG, and libsixel does not take it on in its own code. Reintroducing an in-tree WebP decoder would first require resolving this position, for example through the VP8 cross-license or after the relevant patents have expired.

## External codec libraries

The `libpng`, `libjpeg`, `libtiff`, `libwebp`, and `librsvg` loaders hand decoding to the linked library. The format notes above apply, with these additions:

- **libwebp** includes Google's patent grant for its own code, so the WebP concern above does not arise when this loader decodes.
- **libtiff** supports many compression schemes, some of them optional build features. Classic TIFF schemes such as LZW, PackBits, CCITT Group 3 and Group 4, and JPEG fall under the expired patents described above or under 1980s standards. Optional codecs added to libtiff, such as JBIG, LERC, Zstandard, or WebP inside TIFF, carry the status of their own formats and libraries; the project has not examined each one.
- **librsvg** renders SVG, a W3C Recommendation developed under the [W3C Patent Policy](https://www.w3.org/policies/patent-policy/), which requires participants to license essential claims royalty-free.

## Platform frameworks and external programs

The `wic`, `coregraphics`, `quicklook`, `gdk-pixbuf2`, and `gd` loaders, and the explicit `gnome-thumbnailer` loader, delegate decoding to the operating system, to installed modules or libraries, or to external thumbnailer programs. libsixel contains no decoder for the formats that only these paths reach, such as HEIF/HEIC, AVIF, JPEG 2000, or JPEG XL, and it does not distribute those decoders.

Some of these formats are patent-encumbered. HEIF images normally use HEVC, which is licensed through patent pools; AVIF uses AV1, which the Alliance for Open Media licenses royalty-free although third parties have announced competing pool claims. Patent licensing for such formats is a matter between the patent holders and whoever supplies the decoder: the platform vendor for WIC or ImageIO, or the distributor of a GdkPixbuf module, GD build, or thumbnailer. A distributor who bundles such a decoder together with libsixel takes on its terms; libsixel's own code does not change them.

## Maintenance

When a loader gains a format or codec, record its patent status here, including uncertainty, before treating it as a supported path. Evidence that changes any statement above, such as a new assertion or a confirmed expiry, should be recorded with its source.

## References

- [SIXEL patents and royalties](../sixel-format/patents-and-royalties.md), including patent-term rules.
- [RFC 6386](https://www.rfc-editor.org/rfc/rfc6386), *VP8 Data Format and Decoding Guide*, section 20.27, *PATENTS*.
- [libwebp `PATENTS`](https://github.com/webmproject/libwebp/blob/main/PATENTS), Additional IP Rights Grant.
- The WebM Project, [*VP8 Patent Cross-License*](https://www.webmproject.org/cross-license/).
- LWN.net, [*Google releases a draft VP8 patent cross-license*](https://lwn.net/Articles/551297/), 2013.
- OSnews, [*VP8 does not infringe on Nokia patent*](https://www.osnews.com/story/27245/vp8-does-not-infringe-on-nokia-patent/), 2013.
- [RFC 1951](https://www.rfc-editor.org/rfc/rfc1951), *DEFLATE Compressed Data Format Specification*; [RFC 2083](https://www.rfc-editor.org/rfc/rfc2083), *PNG Specification Version 1.0*.
- [JPEG: patent controversy](https://en.wikipedia.org/wiki/JPEG#Patent_controversy) and [GIF: Unisys and LZW patent enforcement](https://en.wikipedia.org/wiki/GIF#Unisys_and_LZW_patent_enforcement), with their cited primary sources.
- [W3C Patent Policy](https://www.w3.org/policies/patent-policy/).
