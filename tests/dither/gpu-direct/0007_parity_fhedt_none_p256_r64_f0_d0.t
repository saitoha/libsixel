#!/bin/sh
# Verify parity fhedt none p256 r64 f0 d0.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup fhedt \
    "dither/gpu-direct/0007_parity_fhedt_none_p256_r64_f0_d0" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - parity fhedt none p256 r64 f0 d0"
    exit 0
}
echo "ok 1 - parity fhedt none p256 r64 f0 d0"
exit 0
