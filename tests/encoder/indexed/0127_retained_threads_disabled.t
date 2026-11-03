#!/bin/sh
# Retained commit execution: threads disabled.
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0127_retained_threads_disabled" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP requires a thread-disabled build'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained threads disabled'
    exit 0
}
echo 'ok 1 - retained threads disabled'
exit 0
