#!/bin/sh
# PNG open failure returns the OS error and releases converted pixels.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0056_open_failure_cleanup || {
    echo "not ok 1 - PNG open failure reports the OS error and releases pixels"
    exit 0
}
echo "ok 1 - PNG open failure reports the OS error and releases pixels"
exit 0
