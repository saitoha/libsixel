#!/bin/sh
# Verify fhedt r64 f1 d1 fs deadzone.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup fhedt \
    "dither/gpu-direct/0073_fhedt_r64_f1_d1_fs_deadzone" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - fhedt r64 f1 d1 fs deadzone"
    exit 0
}
echo "ok 1 - fhedt r64 f1 d1 fs deadzone"
exit 0
