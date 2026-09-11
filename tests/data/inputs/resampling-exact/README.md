# Exact resampling fixtures

These binary P6 PPM fixtures exercise the builtin PNM loader and avoid decoder-dependent color conversion. Each expected image contains only SIXEL safe-tone RGB values, even though the low-level tests compare the loaded bytes before SIXEL encoding.

Regenerate them from the repository root with:

```sh
python3 tools/generate_resampling_exact_fixtures.py
```

[`fixtures.json`](fixtures.json) records every source row, expected row, and pre-floor scalar result. [`tools/generate_resampling_exact_fixtures.py`](../../../../tools/generate_resampling_exact_fixtures.py) independently implements the documented coordinate mapping, clipped edge stencil normalization, kernel equations, per-pass floor, and clamp. It does not call libsixel, ImageMagick, or another scaler.

The enlargement cases use a non-integer 3-to-4 phase map. The reduction cases use a strong 16-to-4 reduction, which dilates kernel support over source samples. Filtered fixtures are rejected when they collapse to nearest-neighbor output, and transcendental-kernel results are kept away from unstable integer boundaries.
