#!/bin/sh
# Verify measurement resumable histogram.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0120_measurement_resumable_histogram" || {
    printf '\n%s\n' "not ok 1 - measurement resumable histogram"
    exit 0
}
printf '\n%s\n' "ok 1 - measurement resumable histogram"
exit 0
