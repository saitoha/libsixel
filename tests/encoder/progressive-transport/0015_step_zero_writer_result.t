#!/bin/sh
# Verify step zero writer result.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0083_step_zero_writer_result" || {
    printf '\n%s\n' "not ok 1 - step zero writer result"
    exit 0
}
printf '\n%s\n' "ok 1 - step zero writer result"
exit 0
