# Builtin PNG and APNG Loader

## Identity and history

PNG is recognized by its eight-byte signature and an `IHDR` first chunk. The path is hybrid: [`frompng.c`](../../../src/frompng.c) owns high-depth decoding, color metadata, background policy, and reusable PNG helpers; [`loader-builtin.c`](../../../src/loader-builtin.c) owns APNG sequencing and selected indexed paths; adapted inflate and PNG machinery remains in [`stb_image.h`](../../../src/stb_image.h). Commit [`51bfc7f69`](https://github.com/saitoha/libsixel/commit/51bfc7f69) added the builtin APNG path on 2026-02-20. Commit [`ca38e5e4a`](https://github.com/saitoha/libsixel/commit/ca38e5e4a) then introduced precision-preserving 16-bit decode, and [`44045c67b`](https://github.com/saitoha/libsixel/commit/44045c67b) extracted `frompng` on 2026-03-05.

## Accepted PNG image forms

The decoder accepts the standard PNG color types and depth combinations: grayscale at 1, 2, 4, 8, or 16 bits; truecolor at 8 or 16; indexed color at 1, 2, 4, or 8; grayscale plus alpha at 8 or 16; and truecolor plus alpha at 8 or 16. Compression method 0, filter method 0, all five row filters, non-interlaced images, and Adam7 interlace are supported. `PLTE`, `IDAT`, `IEND`, and `tRNS` are interpreted according to the active color type. `bKGD` participates in the documented background-source policy.

The adapted decoder also recognizes Apple's `CgBI` marker and the associated channel-order/unpremultiplication path. This is compatibility behavior for CgBI-style payloads, not a promise to preserve Apple-private chunks or reproduce every Apple toolchain quirk.

## Color and metadata

The CMS path recognizes `iCCP`, `sRGB`, `cHRM`, and `gAMA`. An applicable embedded ICC profile has priority; otherwise explicit sRGB is authoritative, followed by chromaticity/gamma interpretation. `eXIf` is scanned for Exif orientation. These chunks affect decoded pixels or geometry but are not exposed as a general metadata object.

Opaque eight-bit input can remain indexed or gamma `RGB888`. Sixteen-bit input is promoted to float32 and preserves its decoded sample precision. Alpha composition is performed in linear light where required, producing a linear float representation before common finalization. A successful CMS conversion can set the frame to the configured typed target colorspace.

Text chunks, timestamps, physical-pixel dimensions, unknown ancillary chunks, and modern HDR signaling such as `cICP`, `mDCv`, and `cLLi` have no semantic effect in the current loader. Unknown critical chunks are rejected. Existing chunk CRC fields are skipped rather than verified by these in-memory decode paths; callers must not treat successful decode as checksum authentication.

## APNG

APNG support interprets `acTL`, `fcTL`, `fdAT`, and `IDAT`, validates sequence numbers and frame bounds, and applies source (`blend=0`) or over (`blend=1`) composition plus none/background/previous disposal. Delay numerator and denominator are converted to frame timing, with denominator zero treated as 100. The `num_plays` value becomes loop control. Shared non-animation chunks are copied into a synthetic per-frame PNG, so each frame uses the same PNG depth, alpha, background, and CMS paths as static input.

The decoder rejects inconsistent frame counts, invalid chunk order, sequence gaps, and rectangles outside the canvas. MNG and JNG are not APNG and are not supported. APNG does not create a distinct per-frame colorspace or expose frame-local arbitrary metadata.
