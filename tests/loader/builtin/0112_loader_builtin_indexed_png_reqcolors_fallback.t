#!/bin/sh
# TAP wrapper for builtin indexed PNG reqcolors fallback C test.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/0021_loader_builtin_indexed_png_reqcolors_fallback" || {
    echo "not ok 1 - loader/0021_loader_builtin_indexed_png_reqcolors_fallback"
    exit 0
}

echo "ok 1 - loader/0021_loader_builtin_indexed_png_reqcolors_fallback"
