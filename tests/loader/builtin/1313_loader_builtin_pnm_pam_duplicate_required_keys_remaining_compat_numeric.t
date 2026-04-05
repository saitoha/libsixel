#!/bin/sh
# TAP wrapper for builtin PNM PAM required-key duplicate compat checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_PAM_DUP_REQUIRED_REMAIN_COMPAT=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam remaining required-key duplicate compat numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam remaining required-key duplicate compat numeric)"
exit 0
