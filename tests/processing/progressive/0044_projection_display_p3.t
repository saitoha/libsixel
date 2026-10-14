#!/bin/sh
# Verify projection display p3.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0044_projection_display_p3" || {
    echo "not ok 1 - projection display p3"
    exit 0
}
echo "ok 1 - projection display p3"
exit 0
