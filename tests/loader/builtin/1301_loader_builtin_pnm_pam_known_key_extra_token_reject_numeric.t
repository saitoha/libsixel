#!/bin/sh
# TAP wrapper for builtin PNM PAM known-key extra-token reject checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PNM_NUMERIC_PAM_KNOWN_KEY_EXTRA_TOKEN_REJECT=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam known-key extra-token reject numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pnm pam known-key extra-token reject numeric)"
exit 0
