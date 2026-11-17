#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0110_mpet_float_storage_fallback || {
    echo "not ok 1 - out-of-range RGB_F32 retries the legacy path for the same intent"
    exit 0
}

echo "ok 1 - out-of-range RGB_F32 retries the legacy path for the same intent"
exit 0
