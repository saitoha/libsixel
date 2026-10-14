#!/bin/sh
# Verify prepare rounding stall.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0062_prepare_rounding_stall" || {
    echo "not ok 1 - prepare rounding stall"
    exit 0
}
echo "ok 1 - prepare rounding stall"
exit 0
