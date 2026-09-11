# Builtin Netpbm Loader

## Identity and history

The Netpbm family is recognized from the `P1` through `P7` magic values and decoded by [`frompnm.c`](../../../src/frompnm.c). The first in-tree PNM loader was imported from kmiya's `sixel` in commit [`b2996ec8d`](https://github.com/saitoha/libsixel/commit/b2996ec8d) on 2014-05-25. The current implementation is no longer the stock stb PNM path: commit [`bfda5bcff`](https://github.com/saitoha/libsixel/commit/bfda5bcff) added PAM, 16-bit preservation, and linear-light alpha composition in 2026, followed by strict header and compatibility controls. `STBI_NO_PNM` prevents fallback to the old stb implementation.

## Accepted variants

| Magic | Raster | Accepted sample model |
| --- | --- | --- |
| `P1` | ASCII bitmap | One bit, with PBM's `0 = white`, `1 = black` convention. |
| `P2` | ASCII graymap | One component, `MAXVAL` 1 through 65535. |
| `P3` | ASCII pixmap | Three RGB components, `MAXVAL` 1 through 65535. |
| `P4` | Packed binary bitmap | One bit, most-significant bit first, padded at each row. |
| `P5` | Binary graymap | One component; one byte per sample through 255, otherwise two-byte big-endian samples. |
| `P6` | Binary pixmap | Three components with the same byte-width rule as `P5`. |
| `P7` | PAM | Required `WIDTH`, `HEIGHT`, `DEPTH`, `MAXVAL`, and `ENDHDR` fields. Known tuples are `BLACKANDWHITE`, `BLACKANDWHITE_ALPHA`, `GRAYSCALE`, `GRAYSCALE_ALPHA`, `RGB`, and `RGB_ALPHA`. |

PAM requires the known tuple's exact depth. An absent or unknown `TUPLTYPE` is accepted only through the depth fallback: 1 means gray, 2 gray plus alpha, 3 RGB, and 4 RGB plus alpha. Arbitrary depths above four are not accepted. Comments and Netpbm whitespace are recognized. Dimensions are limited to 65536 in either direction and checked again for allocation overflow.

The strict default rejects extra raster bytes, truncated ASCII rasters, duplicate required PAM keys, tokens after `ENDHDR`, and overlarge PAM headers. The builtin loader suboptions `pnm_trailing_data`, `pnm_truncated_ascii`, `pam_duplicate_keys`, `pam_endhdr_tokens`, and `pam_large_header` selectively enable legacy compatibility; the exact grammar and environment defaults are in [`img2sixel(1)`](../../../converters/img2sixel.1).

## Output and color interpretation

Opaque input with `MAXVAL <= 255` becomes gamma `RGB888`; higher-depth input becomes gamma `RGBFLOAT32` so 9–16-bit source precision is not collapsed at the loader boundary. If an alpha-bearing PAM has any non-opaque sample, it is always flattened in linear light against the resolved background and becomes `LINEARRGBFLOAT32`; a missing explicit background resolves to linear black. This decoder does not retain PAM alpha as an interleaved channel or transparency mask. A fully opaque alpha plane is discarded and follows the corresponding RGB output path.

Netpbm carries no standardized embedded ICC profile in these variants. Samples are therefore interpreted by the loader's fallback RGB policy. PBM/PGM are expanded to three color components at the frame boundary.

## Not supported or not interpreted

- PFM floating-point maps, XV thumbnails, and other non-`P1`–`P7` Netpbm extensions are not decoded.
- PAM depths above four and arbitrary multi-channel tuples are not exposed as generic component arrays.
- Unknown PAM header keys are ignored, not preserved as metadata.
- There is no animation, Exif orientation, ICC profile, or file-background semantic.
