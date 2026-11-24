#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Restore canonical residuals omitted by a rounded float threshold.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0186_gpu_selection_threshold_guard"
result=$?
set -e
test "$result" != 77 || {
    printf '\n%s\n' "ok 1 - gpu selection threshold guard # SKIP GPU device unavailable"
    exit 0
}
test "$result" = 0 || {
    printf '\n%s\n' "not ok 1 - gpu selection threshold guard"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu selection threshold guard"
exit 0
