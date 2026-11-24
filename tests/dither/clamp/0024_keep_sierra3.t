#!/bin/sh
# Verify keep sierra3 using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0024_keep_sierra3" || {
    printf '\n%s\n' "not ok 1 - keep_sierra3"
    exit 0
}
printf '\n%s\n' "ok 1 - keep_sierra3"
exit 0
