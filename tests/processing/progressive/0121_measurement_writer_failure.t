#!/bin/sh
# Verify measurement writer failure.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0121_measurement_writer_failure" || {
    echo "not ok 1 - measurement writer failure"
    exit 0
}
echo "ok 1 - measurement writer failure"
exit 0
