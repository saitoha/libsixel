#!/bin/sh
# PNG allocation failure after open closes the owned stream.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0057_post_open_allocation_cleanup || {
    printf '\n%s\n' "not ok 1 - PNG allocation failure closes the stream before any write"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG allocation failure closes the stream before any write"
exit 0
