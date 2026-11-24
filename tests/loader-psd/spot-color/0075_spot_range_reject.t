#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot range reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0035_spot_range_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - spot range reject"
    exit 0
}
printf '\n%s\n' "ok 1 - spot range reject"
exit 0
