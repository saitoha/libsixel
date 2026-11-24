#!/bin/sh
# Verify K-center accepts a normal image whose flat pixel count exceeds 1M.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0042_kcenter_large_buffer" || {
    printf '\n%s\n' "not ok 1 - K-center large-buffer regression"
    exit 0
}

printf '\n%s\n' "ok 1 - K-center large-buffer regression"
exit 0
