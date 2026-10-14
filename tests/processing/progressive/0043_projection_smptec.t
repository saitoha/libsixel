#!/bin/sh
# Verify projection smptec.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0043_projection_smptec" || {
    echo "not ok 1 - projection smptec"
    exit 0
}
echo "ok 1 - projection smptec"
exit 0
