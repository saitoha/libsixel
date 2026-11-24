#!/bin/sh
# Verify parity fhedt x p256 r256 f1 d0.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup fhedt \
    "dither/gpu-direct/0006_parity_fhedt_x_p256_r256_f1_d0" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - parity fhedt x p256 r256 f1 d0"
    exit 0
}
printf '\n%s\n' "ok 1 - parity fhedt x p256 r256 f1 d0"
exit 0
