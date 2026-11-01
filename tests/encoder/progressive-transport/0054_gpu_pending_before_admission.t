#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Suppress a prepared GPU repair when an update precedes admission.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0194_gpu_pending_before_admission"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu pending before admission # SKIP threads or Metal unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu pending before admission"
    exit 0
}
echo "ok 1 - gpu pending before admission"
exit 0
