#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/gpu_rows_window_bound || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - Eight chunks reuse a window of at most three commands."
    exit 0
}
echo "ok 1 - Eight chunks reuse a window of at most three commands."
exit 0
