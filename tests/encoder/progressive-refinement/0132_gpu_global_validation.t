#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Reject malformed global sampling requests before GPU submission.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0183_gpu_global_validation"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu global validation # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu global validation"
    exit 0
}
echo "ok 1 - gpu global validation"
exit 0
