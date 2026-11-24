#!/bin/sh
# Verify measurement output identity.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0118_measurement_output_identity" || {
    printf '\n%s\n' "not ok 1 - measurement output identity"
    exit 0
}
printf '\n%s\n' "ok 1 - measurement output identity"
exit 0
