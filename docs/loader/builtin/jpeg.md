# Builtin JPEG Loader

## Identity and history

JPEG is recognized by the SOI marker and decoded by the heavily adapted stb-derived JPEG core in [`stb_image.h`](../../../src/stb_image.h). Metadata extraction, precision routing, and frame initialization are in [`loader-builtin.c`](../../../src/loader-builtin.c). The source entered the project with stb_image 1.33 in commit [`81375f0bd`](https://github.com/saitoha/libsixel/commit/81375f0bd) on 2014-03-19. Commit [`158bb61ea`](https://github.com/saitoha/libsixel/commit/158bb61ea) introduced float decoding in 2026, and [`cb7fb59bb`](https://github.com/saitoha/libsixel/commit/cb7fb59bb) plus [`81184726a`](https://github.com/saitoha/libsixel/commit/81184726a) added and hardened lossless high-depth decoding.

## Coding processes and precision

| SOF | Process | Accepted sample precision |
| --- | --- | --- |
| `SOF0` | Baseline sequential DCT | 8 bits. |
| `SOF1` | Extended sequential DCT | 8 through 12 bits. |
| `SOF2` | Progressive DCT | 8 through 12 bits. |
| `SOF3` | Huffman lossless | 2 through 16 bits, predictors 1 through 7, with a point transform below the sample precision. |

The decoder supports one, three, or four frame components, Huffman tables, 8- and 16-bit quantization tables, restart intervals, and multiple scans. Horizontal and vertical sampling factors are each 1 through 4 and must form integer ratios with the maximum factor. A DNL marker is accepted only when it repeats the already-known height; a frame whose SOF height is initially zero is rejected.

Three-component data is treated as RGB when component IDs are `R`, `G`, `B`, or when applicable Adobe/JFIF signaling selects direct RGB; otherwise it is converted from YCbCr. Four-component Adobe transform 0 is interpreted as CMYK and transform 2 as YCCK. A four-component stream without those semantics is decoded from its first three components; the fourth component is not exposed as alpha.

## Metadata and output

APP0 JFIF and APP14 Adobe markers affect component interpretation. APP2 `ICC_PROFILE` segments are reassembled only when their count, sequence numbers, and uniqueness are consistent. APP1 Exif is inspected for orientation. Other APP segments and COM data are skipped after structural validation; XMP and arbitrary Exif tags are not exposed.

Ordinary eight-bit DCT data can produce gamma `RGB888`. Lossless JPEG, precision above eight bits, an enabled precision-preserving path, or required color work produces gamma `RGBFLOAT32`. The embedded-profile path accepts an RGB-domain ICC profile after JPEG component conversion. CMYK-domain profiles are not applied directly to the original CMYK samples because the decoder has already converted those samples to RGB.

Arithmetic-coded JPEG, differential and hierarchical processes, JPEG-LS, JPEG 2000, and delayed-height streams are not supported. JPEG alpha is not supported. The optional `libjpeg` component is a separate decoder and may have capabilities determined by the linked library; this page describes only `-Lbuiltin!`.
