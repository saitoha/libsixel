# TAP Test Split Backlog

This document tracks remaining TAP files that still bundle multiple test cases and need to be split into single-test files. The general rule is one test per `.t` file, except when test order/looping requires grouping.

## Completed directories
- `tests/resize/t/` (split into individual precision-mode scenarios)
- `tests/palette/t/` (k-means init and fallback cases split, common helper added)
- `tests/pipeline/t/` (baseline/override planner scenarios split, shared helper)
- `tests/network/t/` (curl scenarios split and port conflicts resolved)
- `tests/loader/t/` (pngsuite cases split)
- `tests/mapfile/t/` (palette import/export cases split with shared helper)

## Remaining work
### CLI core
- `tests/cli-core/t/0002_invalid_option_arguments.t`
- `tests/cli-core/t/0003_invalid_option_combinations.t`
- `tests/cli-core/t/0006_basic.t`
- `tests/cli-core/t/0007_option_matching.t`
- `tests/cli-core/t/0008_argument_shift.t`

### Converter options
- `tests/converter-options/t/0001_conversion_options_01.t`
- `tests/converter-options/t/0002_conversion_options_02.t`
- `tests/converter-options/t/0003_conversion_options_03.t`
- `tests/converter-options/t/0004_conversion_options_04.t`

### Packaging
- `tests/packaging/t/0001_python_wheel.t`

## Work pattern
1. Split one directory at a time, keeping numbering contiguous and naming each new file descriptively.
2. Deduplicate shared setup into small helpers within the same directory when multiple new files need the same logic.
3. After each batch, update `tests/Makefile.am` and `tests/meson.build` to list the new `.t` files exactly once.
4. Run `make check` and `meson test` to ensure both autotools and Meson suites pass after each batch.
