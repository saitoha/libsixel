#!/bin/sh
# TAP wrapper for builtin PNM PAM unknown long-key strict regression checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_PAM_UNKNOWN_LONG_KEY_STRICT_REGRESSION=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam unknown long-key strict regression numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam unknown long-key strict regression numeric)"
exit 0
