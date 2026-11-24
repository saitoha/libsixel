#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/gpu_rows_submission_failure || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - Failure creating the second command drains the first."
    exit 0
}
printf '\n%s\n' "ok 1 - Failure creating the second command drains the first."
exit 0
