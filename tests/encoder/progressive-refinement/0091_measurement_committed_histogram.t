#!/bin/sh
# Verify measurement committed histogram.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0123_measurement_committed_histogram" || {
    printf '\n%s\n' "not ok 1 - measurement committed histogram"
    exit 0
}
printf '\n%s\n' "ok 1 - measurement committed histogram"
exit 0
