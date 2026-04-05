#!/bin/sh
# TAP wrapper for builtin PNM PAM unknown-tuple fallback boundary checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_PAM_UNKNOWN_TUPLTYPE_FALLBACK_BOUNDARY=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam unknown-tuple fallback boundary numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam unknown-tuple fallback boundary numeric)"
exit 0
