#!/bin/sh
# Verify keep jajuni using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0019_keep_jajuni" || {
    echo "not ok 1 - keep_jajuni"
    exit 0
}
echo "ok 1 - keep_jajuni"
exit 0
