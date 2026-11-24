#!/bin/sh
# Verify sierra3 gpu off refresh t0.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/refresh/0046_sierra3_gpu_off_refresh_t0" || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - sierra3 gpu off refresh t0"
    exit 0
}
printf '\n%s\n' "ok 1 - sierra3 gpu off refresh t0"
exit 0
