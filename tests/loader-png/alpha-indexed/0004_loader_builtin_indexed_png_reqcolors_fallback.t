#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP wrapper for builtin indexed PNG reqcolors fallback C test.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/0021_loader_builtin_indexed_png_reqcolors_fallback" || {
    printf '\n%s\n' "not ok 1 - loader/0021_loader_builtin_indexed_png_reqcolors_fallback"
    exit 0
}

printf '\n%s\n' "ok 1 - loader/0021_loader_builtin_indexed_png_reqcolors_fallback"
