#!/bin/sh
# Verify zero headroom using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0007_zero_headroom" || {
    echo "not ok 1 - zero_headroom"
    exit 0
}
echo "ok 1 - zero_headroom"
exit 0
