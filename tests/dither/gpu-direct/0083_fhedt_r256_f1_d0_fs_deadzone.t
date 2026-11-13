#!/bin/sh
# Verify fhedt r256 f1 d0 fs deadzone.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup fhedt \
    "dither/gpu-direct/0083_fhedt_r256_f1_d0_fs_deadzone" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - fhedt r256 f1 d0 fs deadzone"
    exit 0
}
echo "ok 1 - fhedt r256 f1 d0 fs deadzone"
exit 0
