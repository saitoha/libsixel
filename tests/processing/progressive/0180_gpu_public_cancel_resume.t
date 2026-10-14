#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Cancel between GPU batches and resume without early output.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0180_gpu_public_cancel_resume"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu public cancel resume # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu public cancel resume"
    exit 0
}
echo "ok 1 - gpu public cancel resume"
exit 0
