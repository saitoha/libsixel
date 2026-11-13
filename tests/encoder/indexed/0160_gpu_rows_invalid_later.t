#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/gpu_rows_invalid_later || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - A later invalid index cannot invalidate a completed prefix."
    exit 0
}
echo "ok 1 - A later invalid index cannot invalidate a completed prefix."
exit 0
