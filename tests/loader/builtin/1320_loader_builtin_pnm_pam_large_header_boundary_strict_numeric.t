#!/bin/sh
# TAP wrapper for builtin PNM PAM large-header boundary strict checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_BOUNDARY_STRICT=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam large-header boundary strict numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam large-header boundary strict numeric)"
exit 0
