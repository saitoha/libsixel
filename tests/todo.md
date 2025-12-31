# TAP Test Split Backlog

This document tracks remaining TAP files that still bundle multiple test cases and need to be split into single-test files. The general rule is one test per `.t` file, except when test order/looping requires grouping.

## Completed directories
- `tests/resize/t/` (split into individual precision-mode scenarios)
- `tests/palette/t/` (k-means init and fallback cases split, common helper added)
- `tests/pipeline/t/` (baseline/override planner scenarios split, shared helper)
- `tests/network/t/` (curl scenarios split and port conflicts resolved)
- `tests/loader/t/` (pngsuite cases split)
- `tests/mapfile/t/` (palette import/export cases split with shared helper)
- `tests/packaging/t/` (python wheel install and roundtrip split, shared helper)
- `tests/cli-core/t/` (basic option handling, invalid argument/combination, option matching, and argument-shift suites split)
- `tests/converter-options/t/` (conversion options split with dedicated helper)

## Remaining work
None. All previously bundled TAP files have been split.

## Work pattern
1. Split one directory at a time, keeping numbering contiguous and naming each new file descriptively.
2. Deduplicate shared setup into small helpers within the same directory when multiple new files need the same logic.
3. After each batch, update `tests/Makefile.am` and `tests/meson.build` to list the new `.t` files exactly once.
4. Run `make check` and `meson test` to ensure both autotools and Meson suites pass after each batch.
