#!/bin/sh
# PNG packed rows report allocation failure.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0050_packed_allocation_failure >/dev/null || {
    echo "not ok 1 - PNG packed rows report allocation failure"
    exit 0
}
echo "ok 1 - PNG packed rows report allocation failure"
exit 0
