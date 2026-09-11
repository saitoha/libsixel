# Builtin BMP Loader

## Identity and history

BMP is recognized by the `BM` file signature and parsed by [`frombmp.c`](../../../src/frombmp.c). Commit [`379c90cf5`](https://github.com/saitoha/libsixel/commit/379c90cf5) replaced the stock stb BMP path on 2026-04-10. The extraction made Windows and OS/2 DIB variants, compressed payload delegation, CMYK, alpha masks, and color profiles explicit. `STBI_NO_BMP` prevents silent fallback to the removed stock decoder.

## Header, depth, and compression matrix

The accepted DIB header sizes are 12-byte CORE, OS/2 short 16/24/32-byte forms, 40-byte INFO, 52-byte V2, 56-byte V3, 64-byte OS/2 V2, 108-byte V4, and 124-byte V5. The 40-byte header is ambiguous for some OS/2 files; `bmp_info40_mode=auto|windows|os2` controls the heuristic or forces an interpretation.

Uncompressed and palette forms accept 1, 2, 4, 8, 16, 24, and 32 bits per pixel where the selected compression permits them. The decoder supports Windows `BI_RGB`, `BI_RLE8`, `BI_RLE4`, `BI_BITFIELDS`, `BI_ALPHABITFIELDS`, embedded `BI_JPEG`, embedded `BI_PNG`, `BI_CMYK`, `BI_CMYKRLE8`, and `BI_CMYKRLE4`. OS/2 compression values 3 and 4 are interpreted as Huffman 1D at 1 bpp and RLE24 at 24 bpp. Bitfield masks are validated for compatible 16/32-bit layouts. Top-down input is accepted only for `BI_RGB` and `BI_BITFIELDS`.

Embedded JPEG and PNG payloads are not decoded by `frombmp.c` itself: after the BMP wrapper is validated, the payload is routed into the builtin JPEG or PNG family. A 16-bit embedded PNG therefore retains the PNG high-depth behavior instead of being forced through an eight-bit BMP buffer.

## Color, palette, and alpha

Palette rasters and truecolor data are normalized to gamma RGB bytes; an explicit valid alpha mask produces an RGBA intermediate for common alpha finalization. Thirty-two-bit `BI_RGB` data without an explicit alpha contract is not blindly treated as transparent BGRA. CMYK and CMYK-RLE data is converted through an applicable embedded CMYK ICC profile when possible, otherwise through the device conversion.

V4 calibrated RGB endpoints and per-channel gamma are used to synthesize source color interpretation. V5 `PROFILE_EMBEDDED` data is passed to CMS after bounds and applicability checks. `PROFILE_LINKED` is deliberately ignored: builtin decode never resolves an external filesystem or network profile path. Rendering intent and unrelated V5 fields are not exposed as general metadata.

The loader does not accept other DIB header sizes, arbitrary plane counts, unsupported bpp/compression pairings, top-down RLE/CMYK/alpha-bitfield forms, icon-directory semantics, or OS/2 bitmap arrays. ICO/CUR selection belongs to other loader components. Support for `BI_JPEG` and `BI_PNG` is exactly the support of the nested builtin JPEG/PNG decoder, not an independent codec.
