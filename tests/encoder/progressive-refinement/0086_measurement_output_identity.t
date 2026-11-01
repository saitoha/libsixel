#!/bin/sh
# Verify measurement output identity.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0118_measurement_output_identity" || {
    echo "not ok 1 - measurement output identity"
    exit 0
}
echo "ok 1 - measurement output identity"
exit 0
