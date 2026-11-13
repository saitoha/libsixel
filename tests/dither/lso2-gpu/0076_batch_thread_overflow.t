#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/batch_thread_overflow || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - batch thread overflow"
    exit 0
}
echo "ok 1 - batch thread overflow"
exit 0
