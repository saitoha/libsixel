#!/bin/sh
# PNG packed rows report allocation failure.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0050_packed_allocation_failure >/dev/null || {
    printf '\n%s\n' "not ok 1 - PNG packed rows report allocation failure"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG packed rows report allocation failure"
exit 0
