#!/bin/sh
# Verify measurement rebind.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0122_measurement_rebind" || {
    printf '\n%s\n' "not ok 1 - measurement rebind"
    exit 0
}
printf '\n%s\n' "ok 1 - measurement rebind"
exit 0
