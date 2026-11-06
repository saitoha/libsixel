#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0044_advance_gpu_partial" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP Metal unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - advance gpu partial'
    exit 0
}
echo 'ok 1 - advance gpu partial'
exit 0
