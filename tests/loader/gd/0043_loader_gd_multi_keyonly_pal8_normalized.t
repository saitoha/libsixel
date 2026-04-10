#!/bin/sh
# TAP wrapper for gd pixelpolicy case: multi-key pal8 normalization.

set -eux

test "${HAVE_TEST_RUNNER-}" = 1 || {
    printf "1..0 # SKIP test_runner is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0059_loader_gd_pixelpolicy_detail" \
    "multi_keyonly_pal8_normalized" || {
    echo "not ok 1 - gd multi_keyonly_pal8_normalized"
    exit 0
}

echo "ok 1 - gd multi_keyonly_pal8_normalized"
exit 0
