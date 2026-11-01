#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot plane short reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0036_spot_plane_short_reject 1>&2 || {
    echo "not ok 1 - spot plane short reject"
    exit 0
}
echo "ok 1 - spot plane short reject"
exit 0
