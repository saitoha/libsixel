# Builtin WebP Loader

## Identity and history

The dedicated WebP family is recognized from a RIFF container with `WEBP` form type; a valid image begins with `VP8 `, `VP8L`, or `VP8X` structure. Any RIFF-looking input is deliberately routed to this parser so malformed WebP produces a deterministic WebP-family error instead of being guessed as another residual format. The in-tree decoder began with the VP8L path in commit [`fd73c11b5`](https://github.com/saitoha/libsixel/commit/fd73c11b5) on 2026-04-23, followed by VP8, alpha, animation, metadata, and a split into the [`fromwebp-*`](../../../src) parser modules.

## Accepted image coding

Static input supports lossy VP8, lossy VP8 plus `ALPH`, and lossless VP8L version 0. The VP8 path decodes intra/key-frame image payloads; WebP is not used as a general VP8 inter-frame video container. VP8L supports its predictor, color, subtract-green, and color-index transforms, Huffman groups, and color cache. `ALPH` supports raw and VP8L-compressed alpha plus the defined none, horizontal, vertical, and gradient filters. Preprocessing values 0 and 1 are accepted, but the current decoder treats both identically and does not implement a distinct level-reduction reversal stage. Values 2 and 3 and nonzero reserved bits are rejected.

Animated input uses `ANIM` and `ANMF` on a full RGBA canvas. Frame rectangles, duration, source/over blending, background disposal, loop count, start-frame selection, static output, and cancellation are supported. The implementation caps dimensions at 32767, canvas area at 268435456 pixels, and animation length at 1024 frames.

## Container and metadata contract

`VP8X`, `VP8 `, `VP8L`, `ALPH`, `ANIM`, `ANMF`, `ICCP`, `EXIF`, and `XMP ` are recognized. Unknown chunks are skipped after length and padding validation. The parser rejects duplicate singleton chunks, conflicting lossy/lossless payloads, invalid chunk order, nonzero odd-byte padding, and a `VP8X` feature bitmap inconsistent with the chunks actually present.

ICCP is applied to decoded RGBA color channels and converted to eight-bit sRGB when CMS is enabled. EXIF orientation is applied; XMP orientation is a fallback when usable EXIF orientation is absent. XMP color interpretation is intentionally narrow and recognizes compatible names for sRGB/IEC 61966-2-1, Display P3, and Adobe RGB (1998); it is not a general XMP or profile-language implementation. ICCP and EXIF payloads above 1 MiB and XMP payloads above 256 KiB are ignored rather than deeply parsed. ICCP has precedence over the XMP color hint when usable.

The decoder initially produces gamma `RGBA8888`; common finalization then applies background/alpha policy and may replace the alpha channel with the frame's separate transparency representation. WebP's native eight-bit coding is not promoted into extra source precision. Metadata conversion to sRGB also remains an eight-bit RGBA operation in this component.

Not supported are VP8 video interframes, WebP container extensions outside the recognized chunk semantics, arbitrary XMP metadata, arbitrary XMP-described ICC profiles, frame-local ICC/orientation policy, and dimensions or animation counts beyond the stated limits. Unknown chunks are not preserved.
