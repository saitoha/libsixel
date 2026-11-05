#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0032_gpu_invalidate_then_move" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP GPU unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - A later GPU rectangle preserves invalidated holes'
    exit 0
}
echo 'ok 1 - A later GPU rectangle preserves invalidated holes'
exit 0
