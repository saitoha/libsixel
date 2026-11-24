#!/bin/sh
# ICC export reports its own allocation failure before opening the output.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0059_icc_export_allocation_failure || {
    printf '\n%s\n' "not ok 1 - ICC export reports allocation failure before opening output"
    exit 0
}
printf '\n%s\n' "ok 1 - ICC export reports allocation failure before opening output"
exit 0
