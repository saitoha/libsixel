#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Invalidate retained history after a GPU repair writer failure.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0179_gpu_public_write_failure"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu public write failure # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu public write failure"
    exit 0
}
echo "ok 1 - gpu public write failure"
exit 0
