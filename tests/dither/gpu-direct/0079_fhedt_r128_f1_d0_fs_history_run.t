#!/bin/sh
# Verify fhedt r128 f1 d0 fs history run.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup fhedt \
    "dither/gpu-direct/0079_fhedt_r128_f1_d0_fs_history_run" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - fhedt r128 f1 d0 fs history run"
    exit 0
}
echo "ok 1 - fhedt r128 f1 d0 fs history run"
exit 0
