# TAP Test Split Backlog

This document tracks remaining TAP files that still bundle multiple test cases and need to be split into single-test files. The general rule is one test per `.t` file, except when test order/looping requires grouping.

## Completed directories
- `tests/processing/geometry/` (split into individual precision-mode scenarios and scaled coverage migrated from resize)
- `tests/quant/palette/` (k-means init and fallback cases split, common helper added, palette-specific converter flows regrouped)
- `tests/processing/dither/`, `tests/codec/format/`, `tests/codec/output/`, and `tests/cli/options/general/` (former converter-option coverage redistributed by feature with a shared helper)
- `tests/planner/pipeline/` (baseline/override planner scenarios split, shared helper)
- `tests/io/network/` (curl scenarios split and port conflicts resolved)
- `tests/io/loader/` (pngsuite cases split)
- `tests/quant/mapfile/` (palette import/export cases split with shared helper)
- `tests/packaging/t/` (python wheel install and roundtrip split, shared helper)
- `tests/bindings/ruby/` (bundled gem build and roundtrip split)
- `tests/cli/core/` (basic option handling, invalid argument/combination, option matching, and argument-shift suites split)
- `tests/lib/sh/conversion/common.sh` (shared setup extracted for converter-focused TAP cases)

## Remaining work
None. All previously bundled TAP files have been split.

## Work pattern
1. Split one directory at a time, keeping numbering contiguous and naming each new file descriptively.
2. Deduplicate shared setup into small helpers within the same directory when multiple new files need the same logic.
3. After each batch, update `tests/Makefile.am` and `tests/meson.build` to list the new `.t` files exactly once.
4. Run `make check` and `meson test` to ensure both autotools and Meson suites pass after each batch.
