#!/bin/sh
# Verify keep fs using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0017_keep_fs" || {
    echo "not ok 1 - keep_fs"
    exit 0
}
echo "ok 1 - keep_fs"
exit 0
