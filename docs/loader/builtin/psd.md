# Builtin PSD and PSB Loader

## Identity and history

The dedicated Photoshop family accepts `8BPS` and the PSB-compatible `8BPB` signature used by the implementation. Version 1 is PSD and version 2 is PSB; `8BPB` is accepted only with version 2. Commit [`2a743d4b2`](https://github.com/saitoha/libsixel/commit/2a743d4b2) extracted the decoder on 2026-03-26, [`e0d479e64`](https://github.com/saitoha/libsixel/commit/e0d479e64) removed the 8-bit-only RGB fallback, and later work added high-depth color modes, ZIP, resources, and layer reconstruction. `STBI_NO_PSD` disables the former stock flattened decoder.

## Composite-image matrix

The header accepts 1 through 56 channels, dimensions 1 through 300000, and raw, PackBits RLE, ZIP, or ZIP-with-prediction compression. PSB uses its wider section and row-length fields. The merged/composite image has the following accepted color/depth model:

| Color mode | Accepted depth and channel condition | Initial interpretation |
| --- | --- | --- |
| Bitmap | 1 bit, at least one channel; ZIP prediction excluded | Expanded monochrome RGB. |
| Grayscale | 8, 16, or 32 bits, at least one channel | Gamma RGB at 8-bit; precision-preserving float for higher depth. |
| Indexed | 8 bits, at least one channel | The 768-byte planar color table is expanded to gamma `RGB888`. |
| RGB | 8, 16, or 32 bits, at least three channels | Gamma RGB or RGB float. |
| CMYK | 8, 16, or 32 bits, at least four channels | ICC/device CMYK conversion; high depth is carried through float work. |
| Multichannel | Exactly three channels treated as RGB or exactly four treated as CMYK; 8, 16, or 32 bits | Same as the corresponding RGB/CMYK path. |
| Duotone | 8, 16, or 32 bits, at least one channel | Decoded through the grayscale-family sample path; duotone ink curves are not rendered. |
| Lab | 8, 16, or 32 bits, at least three channels | `CIELABFLOAT32` before applicable CMS conversion. |

The first channel beyond the minimum color channels can supply composite alpha. Depending on background and alpha policy, it is composited or represented by the frame transparency mask; a caller must not assume a fourth interleaved component.

The ICC image resource (`0x040f`) is interpreted when its domain matches the source path. CMYK profiles are applied while CMYK samples are still available; Lab profiles are restricted to the Lab path; RGB profiles are applied to RGB data. Other image resources are bounded and skipped unless needed by the decoder. Embedded thumbnails, guides, print metadata, XMP, and arbitrary resource payloads are not exposed.

## Missing-composite layer fallback

When the merged image is absent or a guarded comparison selects layer reconstruction, the decoder can rebuild RGB, grayscale/duotone, CMYK, Lab, and corresponding 3/4-channel Multichannel documents at 8, 16, or 32 bits. It parses layer rectangles, channel planes, opacity, visibility, clipping, masks, common blend modes, and selected non-pixel layer data. Recognized additional-layer keys include vector content/stroke (`vscg`, `vstk`), text/fill color extraction (`TySh`), solid/gradient/pattern fills (`SoCo`, `GdFl`, `PtFl`), vector masks (`vmsk`, `vsms`, `vogk`), object and legacy effects (`lfx2`, `lrFX`), fill opacity (`iOpa`), clipped/interior blending flags (`clbl`, `infx`), and knockout (`knko`).

This fallback is deliberately described as selective reconstruction, not Photoshop rendering compatibility. It approximates the visual result for covered fixtures; unsupported descriptor classes and unknown additional-layer keys are skipped after structural validation. It does not rasterize arbitrary smart objects, shape every text run with Photoshop's font engine, execute adjustment layers, reproduce every filter/effect, resolve linked resources, or guarantee pixel identity with Adobe's compositor. A valid merged composite remains the more authoritative representation where the fallback lacks enough semantics.

## Explicit exclusions

Unsupported color modes and depth combinations fail rather than being guessed. Duotone colorants are not applied, Multichannel with channel counts other than exactly three or four is rejected, and Bitmap plus ZIP prediction is rejected. External linked resources are not fetched. The decoder accepts one document image, not a Photoshop timeline/video abstraction. Because layer/resource parsing is a large attacker-controlled graph, PSD/PSB is also the builtin family's broadest parser and should be included deliberately in threat modeling.
