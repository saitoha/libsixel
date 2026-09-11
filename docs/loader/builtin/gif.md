# Builtin GIF Loader

## Identity and history

[`fromgif.c`](../../../src/fromgif.c) accepts the `GIF87a` and `GIF89a` signatures. GIF was the first format extracted from the original stb_image integration: commits [`30b80e140`](https://github.com/saitoha/libsixel/commit/30b80e140) and [`1d92429d7`](https://github.com/saitoha/libsixel/commit/1d92429d7) added and selected `fromgif` on 2015-05-05 because the earlier single-image API could not deliver an animation sequence. The implementation retains stb ancestry but owns its frame canvas, timing, disposal, loop, and callback behavior. `STBI_NO_GIF` disables the residual stb decoder.

## Accepted image structure

The decoder supports global and per-image local color tables from 2 through 256 entries, LZW minimum code sizes and codes through 12 bits, interlaced image data, image rectangles within the logical screen, and multiple images in one stream. Every emitted frame covers the logical canvas; a sub-rectangle is composited at its image offset rather than returned as a smaller frame.

The Graphic Control Extension supplies centisecond delay, transparent palette index, and disposal. Disposal values 0 and 1 retain the canvas, 2 restores the affected rectangle to the resolved background, and 3 restores the saved previous canvas. The `NETSCAPE2.0` application extension, sub-block identifier 1, supplies the loop count. Loader animation controls select a start frame, request a static result, or bound looping without changing GIF parsing.

## Extension handling

| Extension | Treatment |
| --- | --- |
| Graphic Control (`0xf9`) | Parsed and applied to the next image. |
| Application (`0xff`) with `NETSCAPE2.0` | Loop sub-block is interpreted. |
| Other Application extensions | Structurally consumed and ignored. |
| Comment (`0xfe`) | Structurally consumed and ignored. |
| Plain Text (`0x01`) | Structurally consumed and ignored; text is not rendered. |
| Unknown extension label | Its sub-block chain is consumed and ignored. |

Ignored does not mean unchecked: malformed or truncated sub-block chains and invalid image/LZW structure still fail decoding. The logical-screen pixel-aspect byte, color-table sort flag, comments, and private application data do not affect rendering.

## Output and limits

When palette fusion is useful and the requested color count permits it, a frame can remain `PAL8` with palette transparency metadata. Otherwise it is expanded to gamma RGB/RGBA for common alpha and background finalization. GIF has no source precision beyond eight-bit palette entries and carries no ICC colorspace profile in the interpreted path.

The decoder does not render Plain Text extensions, execute or expose private application protocols, interpret XMP carried in application blocks, or assign semantic meaning to the pixel-aspect ratio. It does not support nonstandard code streams beyond the validated GIF LZW model. Some vendor-specific animation conventions outside Graphic Control plus `NETSCAPE2.0` are therefore intentionally unsupported.
