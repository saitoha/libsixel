#!/bin/sh
# Verify projection nonfinite.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0045_projection_nonfinite" || {
    echo "not ok 1 - projection nonfinite"
    exit 0
}
echo "ok 1 - projection nonfinite"
exit 0
