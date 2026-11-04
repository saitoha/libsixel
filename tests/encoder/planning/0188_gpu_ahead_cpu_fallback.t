#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify gpu ahead cpu fallback.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/planning/0188_gpu_ahead_cpu_fallback >/dev/null || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required thread or GPU support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - gpu ahead cpu fallback'
    exit 0
}
echo 'ok 1 - gpu ahead cpu fallback'
exit 0
