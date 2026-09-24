#!/bin/sh
# Verify keep sierra1 using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0022_keep_sierra1" || {
    echo "not ok 1 - keep_sierra1"
    exit 0
}
echo "ok 1 - keep_sierra1"
exit 0
