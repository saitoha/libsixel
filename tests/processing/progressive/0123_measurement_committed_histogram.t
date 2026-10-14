#!/bin/sh
# Verify measurement committed histogram.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0123_measurement_committed_histogram" || {
    echo "not ok 1 - measurement committed histogram"
    exit 0
}
echo "ok 1 - measurement committed histogram"
exit 0
