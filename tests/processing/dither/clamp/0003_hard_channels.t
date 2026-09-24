#!/bin/sh
# Verify hard channels using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0003_hard_channels" || {
    echo "not ok 1 - hard_channels"
    exit 0
}
echo "ok 1 - hard_channels"
exit 0
