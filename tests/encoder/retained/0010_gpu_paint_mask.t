#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0010_gpu_paint_mask" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP GPU unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - GPU staging records frame-local painted pixels'
    exit 0
}
echo 'ok 1 - GPU staging records frame-local painted pixels'
exit 0
