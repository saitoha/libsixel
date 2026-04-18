#!/bin/sh
# TAP wrapper for builtin PNM ASCII bitmap matrix numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_ASCII_BITMAP_MATRIX=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm ascii bitmap matrix numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm ascii bitmap matrix numeric)"
exit 0
