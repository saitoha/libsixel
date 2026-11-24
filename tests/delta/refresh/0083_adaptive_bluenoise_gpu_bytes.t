#!/bin/sh
# Verify adaptive bluenoise gpu bytes.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delta/refresh/0083_adaptive_bluenoise_gpu_bytes || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - adaptive bluenoise gpu bytes"
    exit 0
}
printf '\n%s\n' "ok 1 - adaptive bluenoise gpu bytes"
exit 0
