#!/bin/sh
# Policy: docs/threading/animation.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" encoder/scheduling/0028_ready_fifo || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required backend unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - ready fifo'
    exit 0
}
echo 'ok 1 - ready fifo'
exit 0
