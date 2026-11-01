#!/bin/sh
# Verify keep atkinson using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0018_keep_atkinson" || {
    echo "not ok 1 - keep_atkinson"
    exit 0
}
echo "ok 1 - keep_atkinson"
exit 0
