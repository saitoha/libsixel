#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Account failed GPU selection work before AUTO resumes on CPU.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0188_gpu_selection_auto_fallback"
result=$?
set -e
test "$result" != 77 || {
    printf '\n%s\n' "ok 1 - gpu selection auto fallback # SKIP GPU device unavailable"
    exit 0
}
test "$result" = 0 || {
    printf '\n%s\n' "not ok 1 - gpu selection auto fallback"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu selection auto fallback"
exit 0
