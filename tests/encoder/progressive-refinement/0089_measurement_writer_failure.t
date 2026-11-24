#!/bin/sh
# Verify measurement writer failure.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0121_measurement_writer_failure" || {
    printf '\n%s\n' "not ok 1 - measurement writer failure"
    exit 0
}
printf '\n%s\n' "ok 1 - measurement writer failure"
exit 0
