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
    printf '\n%s\n' 'ok 1 # SKIP preparation overlap unavailable for this build'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - gpu ahead cpu fallback'
    exit 0
}
printf '\n%s\n' 'ok 1 - gpu ahead cpu fallback'
exit 0
