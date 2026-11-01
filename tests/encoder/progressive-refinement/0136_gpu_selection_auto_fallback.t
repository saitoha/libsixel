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
    echo "ok 1 - gpu selection auto fallback # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu selection auto fallback"
    exit 0
}
echo "ok 1 - gpu selection auto fallback"
exit 0
