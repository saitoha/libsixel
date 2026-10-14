#!/bin/sh
# Verify projection rgb101.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0040_projection_rgb101" || {
    echo "not ok 1 - projection rgb101"
    exit 0
}
echo "ok 1 - projection rgb101"
exit 0
