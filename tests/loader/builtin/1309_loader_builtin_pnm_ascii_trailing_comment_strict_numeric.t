#!/bin/sh
# TAP wrapper for builtin PNM ASCII trailing-comment strict checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_ASCII_TRAILING_COMMENT_STRICT=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm ascii trailing-comment strict numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm ascii trailing-comment strict numeric)"
exit 0
