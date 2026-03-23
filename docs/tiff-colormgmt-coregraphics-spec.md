# TIFF Color Management Spec (CoreGraphics Baseline)

This document fixes libsixel TIFF color-management behavior against
CoreGraphics/ImageIO output for a controlled matrix of metadata tags.

## Scope

- Input formats:
  - TIFF `PhotometricInterpretation=RGB`
  - TIFF `PhotometricInterpretation=CIELab`
- Matrix axes (on/off):
  - ICC profile (`ICCProfile`, tag 34675)
  - `WhitePoint` (tag 318)
  - `PrimaryChromaticities` (tag 319)
  - `TransferFunction` (tag 301)
- CMS mode:
  - loader option `libtiff:cms=1` (same as `libtiff:c=1`)
- Expected outputs:
  - generated with `-Lcoregraphics!` and stored as `.six`.

Fixtures and grouped outputs:

- Inputs: `tests/data/colormgmt/input/tiff/{rgb,lab}`
- References: `tests/data/colormgmt/reference/tiff/{rgb,lab}`
- Group reports:
  - `tests/data/colormgmt/reference/tiff/rgb_groups.tsv`
  - `tests/data/colormgmt/reference/tiff/lab_groups.tsv`

## Observed CoreGraphics Rules

### RGB photometric

- Two output groups only:
  - `icc=0`: one common output
  - `icc=1`: one common output
- `WhitePoint` / `PrimaryChromaticities` / `TransferFunction` do not change output
  in this matrix.

Resulting precedence rule:

1. Embedded ICC affects output when present.
2. Otherwise, output follows ImageIO default RGB handling.
3. `WhitePoint`, `PrimaryChromaticities`, `TransferFunction` are ignored for
   this decode path.

### CIELab photometric

- All 16 combinations collapse to one output group.
- Output is invariant to presence/absence of:
  - ICC
  - WhitePoint
  - PrimaryChromaticities
  - TransferFunction

Resulting precedence rule:

1. ImageIO CoreGraphics conversion for this Lab fixture behaves as if these tags
   are not independently selecting alternate output variants.

## libsixel Mapping

- TIFF decode uses `TIFFReadRGBAImageOriented()` / high-precision TIFF path.
- Embedded ICC application is controlled by `cms` suboption:
  - `cms=1`: apply ICC to sRGB output path.
  - `cms=0`: skip ICC application.
- lcms2 disabled build:
  - fallback ICC conversion is used (RGB/Gray compatible path) to preserve `cms=1`
    behavior close to lcms2 builds.
- CIELab alignment:
  - For `PHOTOMETRIC_CIELAB`, loader normalizes whitepoint to D50-like
    `(x=0.34567, y=0.35850)` before libtiff RGBA decode so output grouping
    matches CoreGraphics expectations.

## `cms` Axis Policy

Tests are split per case and include both `cms=1` and `cms=0`.

- `cms=1`:
  - Compare against same-name CoreGraphics reference
    (`img_tiff_*_iccX_*` -> same `iccX`).
- `cms=0`:
  - Treat embedded ICC as disabled.
  - Compare against `icc0` CoreGraphics reference for the same non-ICC axes:
    - `img_tiff_*_icc0_*` -> `icc0` reference
    - `img_tiff_*_icc1_*` -> corresponding `icc0` reference

Observed result on current fixtures:

- RGB: `cms=0` outputs align with `icc0` reference behavior.
- Lab: all combinations collapse to one group, so `cms=0/1` both match the
  single Lab reference group.

## Not Covered Yet

- EXIF IFD color tags (for example `ColorSpace` 0xA001, `Gamma` 0xA500)
- `ReferenceBlackWhite`, `YCbCrCoefficients`, `GrayResponseCurve`, and other
  colorimetric tags outside this matrix
- Device/OS-version variation beyond the current CoreGraphics baseline

## Regeneration

Use:

```sh
tools/generate_tiff_colormgmt_matrix.py --with-reference
```

This regenerates:

- TIFF matrix inputs
- CoreGraphics `.six` references
- group reports (`*_groups.tsv`)
