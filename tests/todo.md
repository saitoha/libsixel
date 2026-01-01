# TAP Test Split Backlog

This document tracks remaining TAP files that still bundle multiple test cases and need to be split into single-test files. The general rule is one test per `.t` file, except when test order/looping requires grouping.

## Completed directories
- `tests/geometry/t/` (split into individual precision-mode scenarios and scaled coverage migrated from resize)
- `tests/palette/t/` (k-means init and fallback cases split, common helper added, palette-specific converter flows regrouped)
- `tests/dither/t/`, `tests/format/t/`, `tests/output/t/`, and `tests/cli-options/t/` (former converter-option coverage redistributed by feature with a shared helper)
- `tests/pipeline/t/` (baseline/override planner scenarios split, shared helper)
- `tests/network/t/` (curl scenarios split and port conflicts resolved)
- `tests/loader/t/` (pngsuite cases split)
- `tests/mapfile/t/` (palette import/export cases split with shared helper)
- `tests/packaging/t/` (python wheel install and roundtrip split, shared helper)
- `tests/cli-core/t/` (basic option handling, invalid argument/combination, option matching, and argument-shift suites split)
- `tests/conversion/common.sh` (shared setup extracted for converter-focused TAP cases)

## Remaining work
New test coverage items to strengthen regression detection. Keep the rule of one case per `.t` file, with four-digit sequential prefixes matching each directory's existing numbering.

- **SSIM/PSNR regression checks for pixel conversions and resizing**
  - Use `tests/common/quality.sh` to call `assessment/lsqa` (stb_image loader) for MS-SSIM/PSNR checks; avoid Pillow/libpng dependencies.
  - Keep fixed baseline PNGs in `tests/data/baseline/` (e.g., `resize_source_50.png`) with matching sources under `tests/data/inputs/formats/`.
  - Add single-case TAP files:
    - `tests/conversion/t/0001_ssim_regression.t`: convert sample inputs in several pixel formats and compare to baselines (new directory starts at `0001`).
    - `tests/geometry/t/0015_resize_ssim.t`: resize representative inputs and compare to baselines at set thresholds (next after existing `0014`).
  - Register the two TAP files in `tests/Makefile.am` and `tests/meson.build`; ensure each TAP runs one scenario only.

- **Platform codecs and thumbnailers (WIC/OLE/Quick Look/GNOME)**
  - Create platform-conditional TAPs that SKIP cleanly when the OS or tools are missing; each file holds one roundtrip scenario:
    - `tests/wic/t/0001_wic_roundtrip.t`: call a small helper (if needed under `tests/wic/helper/`) to exercise WIC/OLE encoding/decoding.
    - `tests/quicklook/t/0001_ql_thumbnail.t`: invoke Quick Look to render a thumbnail and compare hash/SSIM after PNG conversion.
    - `tests/gnome-thumbnailer/t/0001_gnome_thumb.t`: run GNOME thumbnailer and verify PNG hash/SSIM.
  - Store shared sample inputs in `tests/platform-data/` and document expected outputs.
  - Note CI needs for Windows/macOS/Linux runners capable of running these TAPs.

- **CLI options and environment variable coverage**
  - Add helper `tests/common/cli_matrix.sh` to expand option matrices deterministically.
  - Add TAPs under `tests/cli-options/t/`, one scenario per file following existing numbering (`0001` already present):
    - `0002_env_options.t`: verify `IMG2SIXEL_OPTIONS`, `SIXEL_PALETTE_PATH`, and environment override precedence.
    - `0003_scale_tonemap.t`: cover rare option mixes (extreme width/height, scaling filters, tone/contrast adjustments) and assert headers/output size.
  - Register new TAPs in both build systems and keep options documented inline.

- **Threading behavior with threads on/off**
  - Create `tests/threading/` with one-case TAPs (new numbering starts at `0001`):
    - `t/0001_threads_consistency.t`: compare outputs for `--threads 0`, `--threads 1`, and a higher count on the same input.
    - `t/0002_threads_disabled.t`: run against a build configured without threads; SKIP with a message if unavailable.
  - Add CI matrix entries (thread-enabled vs. thread-disabled) so both TAPs run in matching environments.

- **SIMD level consistency**
  - Add `tests/simd/data/` with inputs sensitive to quantization/resize/dither differences.
  - Add one-case TAP `tests/simd/t/0001_simd_on_off.t` comparing outputs between SIMD-enabled and fallback builds (SSE2/AVX2/NEON vs. none) using hash or SSIM/PSNR.
  - Document and wire CI jobs for SIMD on/off builds to run the TAP.

- **Cross-platform clipboard roundtrip**
  - Add shared helper `tests/common/clipboard_roundtrip.sh` to drive clipboard commands and compare PNG hashes after `sixel2png`.
  - Add OS/tool-specific one-case TAPs under `tests/clipboard/t/` (existing `0001` present):
    - `0002_xclip_roundtrip.t`, `0003_wl_copy_roundtrip.t`, `0004_pbpaste_roundtrip.t`, and `0005_win_clipboard.t`; each SKIP when the command/OS is missing.
  - Register TAPs in both build systems and keep baseline PNGs alongside existing clipboard fixtures.

## Work pattern
1. Split one directory at a time, keeping numbering contiguous and naming each new file descriptively.
2. Deduplicate shared setup into small helpers within the same directory when multiple new files need the same logic.
3. After each batch, update `tests/Makefile.am` and `tests/meson.build` to list the new `.t` files exactly once.
4. Run `make check` and `meson test` to ensure both autotools and Meson suites pass after each batch.
