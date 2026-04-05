#!/bin/sh
# TAP wrapper for builtin PNM signature-guard decode-path checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_SIGNATURE_GUARD=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm signature guard numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm signature guard numeric)"
exit 0
