# Builtin SIXEL Input Loader

## Identity and implementation

The builtin component can use a SIXEL control string as an input image. SIXEL input entered the generic image-helper path with commit [`98b7aee47`](https://github.com/saitoha/libsixel/commit/98b7aee47) in 2014, and current routing was separated into [`loader-builtin.c`](../../../src/loader-builtin.c) in 2025. Recognition accepts either the 8-bit DCS introducer `0x90` or the 7-bit `ESC P` form and scans the DCS parameters for the SIXEL final byte `q`. The loader calls `sixel_decode_raw()`; the parser and paint state live in [`fromsixel.c`](../../../src/fromsixel.c). The wire language and decoder behavior are documented in detail by the [SIXEL Format](../../sixel-format.md) reference.

The decoder interprets DCS parameters, raster attributes, color-register selection and definition, sixel data bytes, repeat introducers, carriage return, and next-line controls. It produces one indexed raster plus a 256-entry RGB palette. If palette fusion is enabled and the decoded color count fits the request, the frame remains gamma `PAL8`; otherwise indices are expanded to gamma `RGB888`.

SIXEL is already quantized source material. Loading and re-encoding it can preserve the decoded register colors, but it cannot recover colors discarded by the original encoder. Palette register lifetime outside the one decoded DCS is not imported from a live terminal, and surrounding terminal output is not replayed as a terminal session.

## Boundaries

This path decodes one recognized SIXEL DCS as one still image. It does not infer animation from several DCS strings, execute non-SIXEL terminal controls, query terminal state, or preserve the original byte-level paint ordering for round-trip emission. It uses the raw final-palette representation rather than `sixel_decode_direct()`, so redefining a color register after that register has already painted pixels cannot be represented faithfully by this loader path. Transparent or unpainted decoder sentinels can remain in the indexed result and must not be treated as ordinary palette indices; no separate paint mask is exported here. Source precision is limited to the decoded SIXEL palette values, and there is no embedded ICC or Exif metadata.
