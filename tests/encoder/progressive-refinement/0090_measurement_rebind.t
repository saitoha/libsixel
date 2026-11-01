#!/bin/sh
# Verify measurement rebind.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0122_measurement_rebind" || {
    echo "not ok 1 - measurement rebind"
    exit 0
}
echo "ok 1 - measurement rebind"
exit 0
