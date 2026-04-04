# Test Input Size Guideline

Use the smallest possible fixture that still validates the target behavior.

- `64x64`: Use when LSQA must evaluate quantization or dithering quality and
  at least 256 color samples are required.
- `16x16`: Use when LSQA must verify basic tone/structure reproduction, such
  as checking that a loader works correctly and the image is not corrupted.
- `2x2` to `8x8`: Use when exact tone matching is validated with `cmp`, or
  when LSQA is used to confirm color-management behavior.
- `1x1`, `1x2`: Use as placeholder fixtures for pass/fail checks that are not
  about visual quality.

## Shell TAP Guideline

Write shell tests for speed and deterministic diagnostics.

- Start with `set -eux`.
- Print TAP plan first, then enable command trace with `set -v`.
- Keep one test case per `.t` file.
- Keep control flow flat and top-to-bottom. Avoid helper functions.
- Avoid `if`/`elif`/`else`/`case` when a linear
  `command || { fail ...; exit 0; }` sequence is sufficient.
- Minimize process forks. Prefer shell built-ins and parameter expansion.
  Use `${0%/*}` and `${0##*/}` instead of `dirname` and `basename`.
- Avoid `awk` and `grep` in tests unless absolutely necessary.
  Prefer shell pattern matching and built-ins.
- Do not use Python from shell tests.
- Avoid file operations (`cp`, `mkdir`, `rm`) unless truly required.
- Do not generate extra log files only for diagnostics.
- Redirect output to files only when that output is asserted by the test.
  Otherwise, redirect to `/dev/null`.
