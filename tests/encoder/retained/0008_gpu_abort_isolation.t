#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0008_gpu_abort_isolation" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP GPU unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - Aborting a candidate preserves committed RGB'
    exit 0
}
echo 'ok 1 - Aborting a candidate preserves committed RGB'
exit 0
