#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Preserve global sample positions and populations across filtered batches.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0182_gpu_global_strata"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu global strata # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu global strata"
    exit 0
}
echo "ok 1 - gpu global strata"
exit 0
