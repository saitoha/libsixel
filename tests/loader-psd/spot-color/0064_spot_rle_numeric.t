#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot rle numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0020_spot_rle_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - spot rle numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - spot rle numeric"
exit 0
