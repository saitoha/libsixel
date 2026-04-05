#!/bin/sh
# TAP wrapper for builtin PNM PAM ENDHDR trailing-token compat checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_PAM_ENDHDR_TRAILING_TOKEN_COMPAT=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam endhdr trailing-token compat numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam endhdr trailing-token compat numeric)"
exit 0
