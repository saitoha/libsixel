# Builtin Netpbm Loader

## Format lineage

Netpbm is a family of deliberately simple raster interchange formats. PBM stores black and white, PGM adds grayscale, PPM adds RGB, and PAM later generalizes the model to tuples with an explicit depth. “PNM” is the collective name for PBM, PGM, and PPM rather than another independent encoding. The authoritative format descriptions are the Netpbm manuals for [PBM](https://netpbm.sourceforge.net/doc/pbm.html), [PGM](https://netpbm.sourceforge.net/doc/pgm.html), [PPM](https://netpbm.sourceforge.net/doc/ppm.html), and [PAM](https://netpbm.sourceforge.net/doc/pam.html).

The first three families each have a plain ASCII representation and a compact binary representation. PAM has one binary representation with a line-oriented extensible header. None of these formats performs general-purpose compression; their portability comes from a small grammar, explicit dimensions and maximum sample value, and a raster that can be read without a codec library.

libsixel's first in-tree PNM reader came from kmiya's `sixel` in commit [`b2996ec8d`](https://github.com/saitoha/libsixel/commit/b2996ec8d) in 2014. Commit [`bfda5bcff`](https://github.com/saitoha/libsixel/commit/bfda5bcff) replaced the old byte-oriented boundary with PAM support, 16-bit preservation, and linear-light alpha composition in 2026. Later commits made historically permissive behavior opt-in. `STBI_NO_PNM` prevents an input rejected here from silently entering the obsolete stb PNM parser inside the same component.

## Container and sample model

A Netpbm image is a magic value, a header, and one raster. Comments start with `#` and run to the line ending where the active grammar permits whitespace and comments. Although current Netpbm specifications allow image sequences, builtin decode returns one image and its strict default rejects a second image as trailing data.

| Magic | Conventional name | Raster encoding | Source components |
| --- | --- | --- | --- |
| `P1` | Plain PBM | ASCII `0` or `1`; whitespace and comments separate samples. | One bilevel sample; `0` is white and `1` is black. |
| `P2` | Plain PGM | ASCII decimal samples. | One grayscale sample with `MAXVAL` 1 through 65535. |
| `P3` | Plain PPM | ASCII decimal samples. | Three samples in RGB order with `MAXVAL` 1 through 65535. |
| `P4` | Raw PBM | One bit per pixel, most-significant bit first; each row ends on a byte boundary. | One bilevel sample. |
| `P5` | Raw PGM | One byte per sample when `MAXVAL <= 255`, otherwise two-byte big-endian samples. | One grayscale sample. |
| `P6` | Raw PPM | The same sample-width rule as `P5`, in RGB tuple order. | Three RGB samples. |
| `P7` | PAM | Binary tuples after a line-oriented header terminated by `ENDHDR`. | `DEPTH` samples whose interpretation is selected by `TUPLTYPE` or the fallback below. |

PAM requires exactly one usable `WIDTH`, `HEIGHT`, `DEPTH`, and `MAXVAL` under the strict default. Recognized tuple types are `BLACKANDWHITE`, `BLACKANDWHITE_ALPHA`, `GRAYSCALE`, `GRAYSCALE_ALPHA`, `RGB`, and `RGB_ALPHA`, and each must have its specified depth. An absent or unknown tuple type falls back by depth: 1 is grayscale, 2 is grayscale plus alpha, 3 is RGB, and 4 is RGB plus alpha. Unknown header keys are skipped. Arbitrary tuples and depths above four are not exposed as a generic channel array.

Dimensions are limited to 65536 in either direction, `MAXVAL` is limited to 65535, and every raster allocation is checked independently for multiplication overflow. The default PAM policy also limits the header to 64 KiB and 1024 lines. These are implementation safety limits, not maxima imposed by every Netpbm variant.

## Decode pipeline and frame representation

The builtin dispatcher recognizes `P1` through `P7` only when the third byte is whitespace and routes the complete in-memory chunk to [`frompnm.c`](../../../src/frompnm.c). The parser first fixes the tuple model and sample width, then validates dimensions and allocation sizes, decodes samples while checking `0..MAXVAL`, and finally validates the raster tail.

PBM expands to RGB with its inverse-looking historical convention preserved. Grayscale is replicated into three components. Samples are normalized by `sample / MAXVAL`; this is scaling, not a color transform.

| Decoded source | Frame returned by the format decoder |
| --- | --- |
| Opaque input with `MAXVAL <= 255` | Gamma `RGB888`. |
| Opaque input with `MAXVAL > 255` | Gamma `RGBFLOAT32`, preserving the normalized 9–16-bit values instead of rounding to bytes. |
| Alpha-bearing PAM whose alpha plane is entirely opaque | The corresponding opaque row above; the redundant alpha plane is discarded. |
| Alpha-bearing PAM with any non-opaque sample | `LINEARRGBFLOAT32`, composited in linear light against the resolved background. |

PAM alpha is not returned as RGBA or as a mask. With transparency and no resolved explicit background, this decoder uses black and still produces an opaque linear RGB frame. Gamma samples are decoded to linear sRGB for the composition, the alpha blend is performed there, and the result stays linear for downstream resize and palette work.

Netpbm has no standardized embedded ICC container in these accepted variants. `frompnm` therefore does not open a source profile or infer Exif orientation. The numeric samples are treated as the fallback gamma RGB interpretation until alpha composition requires linearization.

## Options and observable differences

The examples below close the loader chain with the final `!`, so rejection cannot fall through to a framework decoder with different tolerance.

```console
img2sixel -Lbuiltin! image.ppm
img2sixel -Lbuiltin:pnm_trailing_data=1:pam_large_header=1! image.pam
```

| Control | Default | Exact effect on this family |
| --- | --- | --- |
| `builtin:pnm_trailing_data=0|1` (`N0`/`N1`) | `0` | `0` accepts only whitespace/comments after an ASCII raster and no bytes after a binary raster. `1` ignores everything after the first decoded image, including another Netpbm image. |
| `builtin:pnm_truncated_ascii=0|1` (`Y0`/`Y1`) | `0` | `0` rejects an early end in `P1`–`P3`. `1` fills missing `P1` samples with white (`0`) and missing `P2`/`P3` samples with numeric zero. It never relaxes truncated binary input. |
| `builtin:pam_duplicate_keys=0|1` (`D0`/`D1`) | `0` | `0` rejects repeated required PAM keys. `1` accepts them and the later parsed value is effective. |
| `builtin:pam_endhdr_tokens=0|1` (`G0`/`G1`) | `0` | `0` requires `ENDHDR` to be the only token on its line. `1` permits tokens after it. |
| `builtin:pam_large_header=0|1` (`J0`/`J1`) | `0` | `1` removes the 64 KiB/1024-line policy caps; input length and checked arithmetic still bound parsing. |
| `-B COLOR`, `background_colorspace=gamma|linear` | no explicit background; gamma | Affects only alpha-bearing PAM. The selected color is interpreted in the declared background colorspace before linear composition. |
| `-A auto|composite|clear|keep` | `auto` | Does not retain PAM alpha: format decode has already flattened any non-opaque PAM before common alpha policy runs. |
| `cms_engine`, `cms_target`, `cms_intent`, `prefer_8bit` | CMS off | No embedded-profile transform is available, so these do not change ordinary Netpbm decode. |
| `orientation`, `trns_keycolor`, HDR controls, `bmp_info40_mode` | format-specific defaults | No effect on Netpbm. |
| `-S`, `-l`, `-T`, `-g` | normal animation defaults | No effect because this path emits one still frame and has no timing metadata. |

The environment variables and precedence rules corresponding to every suboption are listed in [`img2sixel(1)`](../../../converters/img2sixel.1). Explicit `-Lbuiltin:...` assignments override their environment defaults.

## Unsupported dialects and security boundary

PFM floating-point maps, XV thumbnail files that also begin with `P7`, arbitrary PAM tuple depths, and non-`P1`–`P7` extensions are not supported. Multiple-image Netpbm streams are not emitted as animations. Unknown PAM keys are ignored rather than preserved, and there is no metadata object for comments.

Plain formats are not inherently safe merely because their grammar is simple: decimal token growth, dimensions, header lines, and tuple products are attacker-controlled. The strict defaults intentionally reject ambiguous tails and truncated rasters. Compatibility switches should be enabled for a known producer, not used as a general way to make untrusted files decode.

## Implementation landmarks

- Parser, sample decode, compatibility policy, and alpha composition: [`frompnm.c`](../../../src/frompnm.c)
- Builtin signature routing and frame initialization: [`loader-builtin.c`](../../../src/loader-builtin.c)
- Executable compatibility matrix: [`tests/loader/builtin`](../../../tests/loader/builtin)
- Shared frame meanings: [Pixel Formats and Alpha Representation](../../concepts/pixelformat.md) and [Color Spaces and Loader Color Management](../../concepts/colorspace.md)
