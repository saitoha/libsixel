#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0110_mpet_float_storage_fallback || {
    printf '\n%s\n' "not ok 1 - out-of-range RGB_F32 retries the legacy path for the same intent"
    exit 0
}

printf '\n%s\n' "ok 1 - out-of-range RGB_F32 retries the legacy path for the same intent"
exit 0
