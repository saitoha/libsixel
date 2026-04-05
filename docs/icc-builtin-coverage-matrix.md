# Builtin ICC Coverage Matrix

This matrix tracks builtin ICC coverage for `cms_engine=builtin` and
`!HAVE_LCMS2` focused behavior.

## A2B/B2A Tag Coverage

| Item | Status | Tests | Notes |
| --- | --- | --- | --- |
| `A2B0` `mft1`/`mft2` (RGB/GRAY) | Implemented | `icc/0001`, `loader/builtin/1199` | RGB/GRAY path in builtin parser/apply. |
| `A2B0` `mft1`/`mft2` (CMYK) | Implemented | `loader/libjpeg/0041`, `loader/libjpeg/0042` | Existing CMYK A2B0 path maintained. |
| `A2B0` `mAB`/`mBA` (RGB/GRAY/CMYK) | Implemented | `icc/0002` | Added parser/apply pipeline for `mAB`/`mBA`. |
| `A2B1` / `A2B2` | Not implemented | - | Next phase. |
| `B2A0` / `B2A1` / `B2A2` | Not implemented | - | Next phase. |
| `D2B*` / `B2D*` | Not implemented | - | Next phase. |

## Curve Coverage

| Item | Status | Tests | Notes |
| --- | --- | --- | --- |
| `curv` | Implemented | `icc/0001`, `icc/0002` | Identity and table paths are covered. |
| `para` | Implemented | `loader/builtin/1199`, `loader/libtiff/0138`/`0139` | Parametric profiles in loader E2E. |
| `segm` | Implemented | `icc/0001` | Segmented TRC parser/apply path is covered. |

## Loader Integration (`cms_engine=builtin`)

| Loader | Status | Tests | Notes |
| --- | --- | --- | --- |
| `libpng` | Implemented | `loader/builtin/1199` | PNG ICC decode path wired to builtin CMS. |
| `libjpeg` | Implemented | `loader/libjpeg/0041`/`0042` | CMYK A2B0 builtin path covered. |
| `libtiff` | Implemented | `loader/libtiff/0138`/`0139` | RGB parametric profile path covered. |
| `libwebp` | Implemented | `loader/libwebp/0165` | RGB mAB fixture path is covered. |

## Behavior Notes

- Priority for RGB/GRAY/CMYK is now `A2B0(mAB/mBA)` -> `A2B0(mft1/mft2)`.
- RGB/GRAY fallback remains matrix/TRC when `A2B0` is invalid.
- Rendering-intent specific path selection in builtin backend is still pending.
