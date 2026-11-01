#!/bin/sh
# Verify keep stucki using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0020_keep_stucki" || {
    echo "not ok 1 - keep_stucki"
    exit 0
}
echo "ok 1 - keep_stucki"
exit 0
